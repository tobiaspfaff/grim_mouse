/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

// Since FreeType2 includes files, which contain forbidden symbols, we need to
// allow all symbols here.
#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "common/scummsys.h"
#ifdef USE_FREETYPE2

#include "graphics/fonts/ttf.h"
#include "graphics/font.h"
#include "graphics/surface.h"

#include "common/file.h"
#include "common/singleton.h"
#include "common/stream.h"
#include "common/memstream.h"
#include "common/hashmap.h"
#include "common/ptr.h"
#include "common/unzip.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H

// ResidualVM specific
#if FREETYPE_MAJOR > 2 || ( FREETYPE_MAJOR == 2 &&  FREETYPE_MINOR >= 9)
#include FT_TRUETYPE_DRIVER_H
#endif

namespace Graphics {

namespace {

inline int ftCeil26_6(FT_Pos x) {
	return (x + 63) / 64;
}

inline int divRoundToNearest(int dividend, int divisor) {
	return (dividend + (divisor / 2)) / divisor;
}

} // End of anonymous namespace

class TTFLibrary : public Common::Singleton<TTFLibrary> {
public:
	TTFLibrary();
	~TTFLibrary();

	/**
	 * Check whether FreeType2 is initialized properly.
	 */
	bool isInitialized() const { return _initialized; }

	bool loadFont(const uint8 *file, const uint32 size, FT_Face &face);
	void closeFont(FT_Face &face);
private:
	FT_Library _library;
	bool _initialized;
};

void shutdownTTF() {
	TTFLibrary::destroy();
}

#define g_ttf ::Graphics::TTFLibrary::instance()

TTFLibrary::TTFLibrary() : _library(), _initialized(false) {
	if (!FT_Init_FreeType(&_library))
		_initialized = true;
}

TTFLibrary::~TTFLibrary() {
	if (_initialized) {
		FT_Done_FreeType(_library);
		_initialized = false;
	}
}

bool TTFLibrary::loadFont(const uint8 *file, const uint32 size, FT_Face &face) {
	assert(_initialized);

	return (FT_New_Memory_Face(_library, file, size, 0, &face) == 0);
}

void TTFLibrary::closeFont(FT_Face &face) {
	assert(_initialized);

	FT_Done_Face(face);
}

class TTFFont : public Font {
public:
	TTFFont();
	virtual ~TTFFont();

	bool load(Common::SeekableReadStream &stream, int size, TTFSizeMode sizeMode, uint dpi, TTFRenderMode renderMode, bool stemDarkening, const uint32 *mapping);

	virtual int getFontHeight() const;

	virtual int getMaxCharWidth() const;

	virtual int getCharWidth(uint32 chr) const;

	virtual int getKerningOffset(uint32 left, uint32 right) const;

	virtual Common::Rect getBoundingBox(uint32 chr) const;

	virtual void drawChar(Surface *dst, uint32 chr, int x, int y, uint32 color) const;
private:
	bool _initialized;
	FT_Face _face;

	uint8 *_ttfFile;
	uint32 _size;

	int _width, _height;
	int _ascent, _descent;

	struct Glyph {
		Surface image;
		int xOffset, yOffset;
		int advance;
		FT_UInt slot;
	};

	bool cacheGlyph(Glyph &glyph, uint32 chr) const;
	typedef Common::HashMap<uint32, Glyph> GlyphCache;
	mutable GlyphCache _glyphs;
	bool _allowLateCaching;
	void assureCached(uint32 chr) const;

	Common::SeekableReadStream *readTTFTable(FT_ULong tag) const;

	int computePointSize(int size, TTFSizeMode sizeMode) const;
	int readPointSizeFromVDMXTable(int height) const;
	int computePointSizeFromHeaders(int height) const;

	FT_Int32 _loadFlags;
	FT_Render_Mode _renderMode;
	bool _hasKerning;
};

TTFFont::TTFFont()
    : _initialized(false), _face(), _ttfFile(0), _size(0), _width(0), _height(0), _ascent(0),
      _descent(0), _glyphs(), _loadFlags(FT_LOAD_TARGET_NORMAL), _renderMode(FT_RENDER_MODE_NORMAL),
      _hasKerning(false), _allowLateCaching(false) {
}

TTFFont::~TTFFont() {
	if (_initialized) {
		g_ttf.closeFont(_face);

		delete[] _ttfFile;
		_ttfFile = 0;

		for (GlyphCache::iterator i = _glyphs.begin(), end = _glyphs.end(); i != end; ++i)
			i->_value.image.free();

		_initialized = false;
	}
}

bool TTFFont::load(Common::SeekableReadStream &stream, int size, TTFSizeMode sizeMode, uint dpi, TTFRenderMode renderMode, bool stemDarkening, const uint32 *mapping) {
	if (!g_ttf.isInitialized())
		return false;

	_size = stream.size();
	if (!_size)
		return false;

	_ttfFile = new uint8[_size];
	assert(_ttfFile);

	if (stream.read(_ttfFile, _size) != _size) {
		delete[] _ttfFile;
		_ttfFile = 0;

		return false;
	}

	if (!g_ttf.loadFont(_ttfFile, _size, _face)) {
		delete[] _ttfFile;
		_ttfFile = 0;

		return false;
	}

	// We only support scalable fonts.
	if (!FT_IS_SCALABLE(_face)) {
		delete[] _ttfFile;
		_ttfFile = 0;

		g_ttf.closeFont(_face);

		return false;
	}

	// RedisualVM specific
	if (stemDarkening) {
#if FREETYPE_MAJOR > 2 || ( FREETYPE_MAJOR == 2 &&  FREETYPE_MINOR >= 9)
		FT_Parameter param;
		param.tag = FT_PARAM_TAG_STEM_DARKENING;
		param.data = &stemDarkening;
		FT_Face_Properties(_face, 1, &param);
#else
		warning("Stem darkening is not available with this version of FreeType");
#endif
	}

	// Check whether we have kerning support
	_hasKerning = (FT_HAS_KERNING(_face) != 0);

	if (FT_Set_Char_Size(_face, 0, computePointSize(size, sizeMode) * 64, dpi, dpi)) {
		delete[] _ttfFile;
		_ttfFile = 0;

		return false;
	}

	switch (renderMode) {
	case kTTFRenderModeNormal:
		_loadFlags = FT_LOAD_TARGET_NORMAL;
		_renderMode = FT_RENDER_MODE_NORMAL;
		break;

	case kTTFRenderModeLight:
		_loadFlags = FT_LOAD_TARGET_LIGHT;
		_renderMode = FT_RENDER_MODE_LIGHT;
		break;

	case kTTFRenderModeMonochrome:
		_loadFlags = FT_LOAD_TARGET_MONO;
		_renderMode = FT_RENDER_MODE_MONO;
		break;
	}

	FT_Fixed yScale = _face->size->metrics.y_scale;
	_ascent = ftCeil26_6(FT_MulFix(_face->ascender, yScale));
	_descent = ftCeil26_6(FT_MulFix(_face->descender, yScale));

	_width = ftCeil26_6(FT_MulFix(_face->max_advance_width, _face->size->metrics.x_scale));
	_height = _ascent - _descent + 1;

	if (!mapping) {
		// Allow loading of all unicode characters.
		_allowLateCaching = true;

		// Load all ISO-8859-1 characters.
		for (uint i = 0; i < 256; ++i) {
			if (!cacheGlyph(_glyphs[i], i)) {
				_glyphs.erase(i);
			}
		}
	} else {
		// We have a fixed map of characters do not load more later.
		_allowLateCaching = false;

		for (uint i = 0; i < 256; ++i) {
			const uint32 unicode = mapping[i] & 0x7FFFFFFF;
			const bool isRequired = (mapping[i] & 0x80000000) != 0;
			// Check whether loading an important glyph fails and error out if
			// that is the case.
			if (!cacheGlyph(_glyphs[i], unicode)) {
				_glyphs.erase(i);
				if (isRequired)
					return false;
			}
		}
	}

	_initialized = (_glyphs.size() != 0);
	return _initialized;
}

int TTFFont::computePointSize(int size, TTFSizeMode sizeMode) const {
	int ptSize = 0;
	switch (sizeMode) {
	case kTTFSizeModeCell: {
		ptSize = readPointSizeFromVDMXTable(size);

		if (ptSize == 0) {
			ptSize = computePointSizeFromHeaders(size);
		}

		if (ptSize == 0) {
			warning("Unable to compute point size for font '%s'", _face->family_name);
			ptSize = 1;
		}
		break;
	}
	case kTTFSizeModeCharacter:
		ptSize = size;
		break;
	}

	return ptSize;
}

Common::SeekableReadStream *TTFFont::readTTFTable(FT_ULong tag) const {
	// Find the required buffer size by calling the load function with nullptr
	FT_ULong size = 0;
	FT_Error err = FT_Load_Sfnt_Table(_face, tag, 0, nullptr, &size);
	if (err) {
		return nullptr;
	}

	byte *buf = (byte *)malloc(size);
	if (!buf) {
		return nullptr;
	}

	err = FT_Load_Sfnt_Table(_face, tag, 0, buf, &size);
	if (err) {
		free(buf);
		return nullptr;
	}

	return new Common::MemoryReadStream(buf, size, DisposeAfterUse::YES);
}

int TTFFont::readPointSizeFromVDMXTable(int height) const {
	// The Vertical Device Metrics table matches font heights with point sizes.
	// FreeType does not expose it, we have to parse it ourselves.
	// See https://www.microsoft.com/typography/otspec/vdmx.htm

	Common::ScopedPtr<Common::SeekableReadStream> vdmxBuf(readTTFTable(TTAG_VDMX));
	if (!vdmxBuf) {
		return 0;
	}

	// Read the main header
	vdmxBuf->skip(4); // Skip the version
	uint16 numRatios = vdmxBuf->readUint16BE();

	// Compute the starting position for the group table positions table
	int32 offsetTableStart = vdmxBuf->pos() + 4 * numRatios;

	// Search the ratio table for the 1:1 ratio, or the default record (0, 0, 0)
	int32 selectedRatio = -1;
	for (uint16 i = 0; i < numRatios; i++) {
		vdmxBuf->skip(1); // Skip the charset subset
		uint8 xRatio = vdmxBuf->readByte();
		uint8 yRatio1 = vdmxBuf->readByte();
		uint8 yRatio2 = vdmxBuf->readByte();

		if ((xRatio == 1 && yRatio1 <= 1 && yRatio2 >= 1)
		    || (xRatio == 0 && yRatio1 == 0 && yRatio2 == 0)) {
			selectedRatio = i;
			break;
		}
	}
	if (selectedRatio < 0) {
		return 0;
	}

	// Read from group table positions table to get the group table offset
	vdmxBuf->seek(offsetTableStart + sizeof(uint16) * selectedRatio);
	uint16 groupOffset = vdmxBuf->readUint16BE();

	// Read the group table header
	vdmxBuf->seek(groupOffset);
	uint16 numRecords = vdmxBuf->readUint16BE();
	vdmxBuf->skip(2); // Skip the table bounds

	// Search a record matching the required height
	for (uint16 i = 0; i < numRecords; i++) {
		uint16 pointSize = vdmxBuf->readUint16BE();
		int16 yMax = vdmxBuf->readSint16BE();
		int16 yMin = vdmxBuf->readSint16BE();

		if (yMax + -yMin > height) {
			return 0;
		}
		if (yMax + -yMin == height) {
			return pointSize;
		}
	}

	return 0;
}

int TTFFont::computePointSizeFromHeaders(int height) const {
	TT_OS2 *os2Header = (TT_OS2 *)FT_Get_Sfnt_Table(_face, ft_sfnt_os2);
	TT_HoriHeader *horiHeader = (TT_HoriHeader *)FT_Get_Sfnt_Table(_face, ft_sfnt_hhea);

	if (os2Header && (os2Header->usWinAscent + os2Header->usWinDescent != 0)) {
		return divRoundToNearest(_face->units_per_EM * height, os2Header->usWinAscent + os2Header->usWinDescent);
	} else if (horiHeader && (horiHeader->Ascender + horiHeader->Descender != 0)) {
		return divRoundToNearest(_face->units_per_EM * height, horiHeader->Ascender + horiHeader->Descender);
	}

	return 0;
}

int TTFFont::getFontHeight() const {
	return _height;
}

int TTFFont::getMaxCharWidth() const {
	return _width;
}

int TTFFont::getCharWidth(uint32 chr) const {
	assureCached(chr);
	GlyphCache::const_iterator glyphEntry = _glyphs.find(chr);
	if (glyphEntry == _glyphs.end())
		return 0;
	else
		return glyphEntry->_value.advance;
}

int TTFFont::getKerningOffset(uint32 left, uint32 right) const {
	if (!_hasKerning)
		return 0;

	assureCached(left);
	assureCached(right);

	FT_UInt leftGlyph, rightGlyph;
	GlyphCache::const_iterator glyphEntry;

	glyphEntry = _glyphs.find(left);
	if (glyphEntry != _glyphs.end()) {
		leftGlyph = glyphEntry->_value.slot;
	} else {
		return 0;
	}

	glyphEntry = _glyphs.find(right);
	if (glyphEntry != _glyphs.end()) {
		rightGlyph = glyphEntry->_value.slot;
	} else {
		return 0;
	}

	if (!leftGlyph || !rightGlyph)
		return 0;

	FT_Vector kerningVector;
	FT_Get_Kerning(_face, leftGlyph, rightGlyph, FT_KERNING_DEFAULT, &kerningVector);
	return (kerningVector.x / 64);
}

Common::Rect TTFFont::getBoundingBox(uint32 chr) const {
	assureCached(chr);
	GlyphCache::const_iterator glyphEntry = _glyphs.find(chr);
	if (glyphEntry == _glyphs.end()) {
		return Common::Rect();
	} else {
		const int xOffset = glyphEntry->_value.xOffset;
		const int yOffset = glyphEntry->_value.yOffset;
		const Graphics::Surface &image = glyphEntry->_value.image;
		return Common::Rect(xOffset, yOffset, xOffset + image.w, yOffset + image.h);
	}
}

namespace {

template<typename ColorType>
void renderGlyph(uint8 *dstPos, const int dstPitch, const uint8 *srcPos, const int srcPitch, const int w, const int h, ColorType color, const PixelFormat &dstFormat) {
	uint8 sR, sG, sB;
	dstFormat.colorToRGB(color, sR, sG, sB);

	for (int y = 0; y < h; ++y) {
		ColorType *rDst = (ColorType *)dstPos;
		const uint8 *src = srcPos;

		for (int x = 0; x < w; ++x) {
			if (*src == 255) {
				*rDst = color;
			} else if (*src) {
				const uint8 a = *src;

				uint8 dR, dG, dB;
				dstFormat.colorToRGB(*rDst, dR, dG, dB);

				dR = ((255 - a) * dR + a * sR) / 255;
				dG = ((255 - a) * dG + a * sG) / 255;
				dB = ((255 - a) * dB + a * sB) / 255;

				*rDst = dstFormat.RGBToColor(dR, dG, dB);
			}

			++rDst;
			++src;
		}

		dstPos += dstPitch;
		srcPos += srcPitch;
	}
}

} // End of anonymous namespace

void TTFFont::drawChar(Surface *dst, uint32 chr, int x, int y, uint32 color) const {
	assureCached(chr);
	GlyphCache::const_iterator glyphEntry = _glyphs.find(chr);
	if (glyphEntry == _glyphs.end())
		return;

	const Glyph &glyph = glyphEntry->_value;

	x += glyph.xOffset;
	y += glyph.yOffset;

	if (x > dst->w)
		return;
	if (y > dst->h)
		return;

	int w = glyph.image.w;
	int h = glyph.image.h;

	const uint8 *srcPos = (const uint8 *)glyph.image.getPixels();

	// Make sure we are not drawing outside the screen bounds
	if (x < 0) {
		srcPos -= x;
		w += x;
		x = 0;
	}

	if (x + w > dst->w)
		w = dst->w - x;

	if (w <= 0)
		return;

	if (y < 0) {
		srcPos -= y * glyph.image.pitch;
		h += y;
		y = 0;
	}

	if (y + h > dst->h)
		h = dst->h - y;

	if (h <= 0)
		return;

	uint8 *dstPos = (uint8 *)dst->getBasePtr(x, y);

	if (dst->format.bytesPerPixel == 1) {
		for (int cy = 0; cy < h; ++cy) {
			uint8 *rDst = dstPos;
			const uint8 *src = srcPos;

			for (int cx = 0; cx < w; ++cx) {
				// We assume a 1Bpp mode is a color indexed mode, thus we can
				// not take advantage of anti-aliasing here.
				if (*src >= 0x80)
					*rDst = color;

				++rDst;
				++src;
			}

			dstPos += dst->pitch;
			srcPos += glyph.image.pitch;
		}
	} else if (dst->format.bytesPerPixel == 2) {
		renderGlyph<uint16>(dstPos, dst->pitch, srcPos, glyph.image.pitch, w, h, color, dst->format);
	} else if (dst->format.bytesPerPixel == 4) {
		renderGlyph<uint32>(dstPos, dst->pitch, srcPos, glyph.image.pitch, w, h, color, dst->format);
	}
}

bool TTFFont::cacheGlyph(Glyph &glyph, uint32 chr) const {
	FT_UInt slot = FT_Get_Char_Index(_face, chr);
	if (!slot)
		return false;

	glyph.slot = slot;

	// We use the light target and render mode to improve the looks of the
	// glyphs. It is most noticable in FreeSansBold.ttf, where otherwise the
	// 't' glyph looks like it is cut off on the right side.
	if (FT_Load_Glyph(_face, slot, _loadFlags))
		return false;

	if (FT_Render_Glyph(_face->glyph, _renderMode))
		return false;

	if (_face->glyph->format != FT_GLYPH_FORMAT_BITMAP)
		return false;

	glyph.xOffset = _face->glyph->bitmap_left;
	glyph.yOffset = _ascent - _face->glyph->bitmap_top;

	glyph.advance = ftCeil26_6(_face->glyph->advance.x);

	const FT_Bitmap &bitmap = _face->glyph->bitmap;
	glyph.image.create(bitmap.width, bitmap.rows, PixelFormat::createFormatCLUT8());

	const uint8 *src = bitmap.buffer;
	int srcPitch = bitmap.pitch;
	if (srcPitch < 0) {
		src += (bitmap.rows - 1) * srcPitch;
		srcPitch = -srcPitch;
	}

	uint8 *dst = (uint8 *)glyph.image.getPixels();
	memset(dst, 0, glyph.image.h * glyph.image.pitch);

	switch (bitmap.pixel_mode) {
	case FT_PIXEL_MODE_MONO:
		for (int y = 0; y < (int)bitmap.rows; ++y) {
			const uint8 *curSrc = src;
			uint8 mask = 0;

			for (int x = 0; x < (int)bitmap.width; ++x) {
				if ((x % 8) == 0)
					mask = *curSrc++;

				if (mask & 0x80)
					*dst = 255;

				mask <<= 1;
				++dst;
			}

			src += srcPitch;
		}
		break;

	case FT_PIXEL_MODE_GRAY:
		for (int y = 0; y < (int)bitmap.rows; ++y) {
			memcpy(dst, src, bitmap.width);
			dst += glyph.image.pitch;
			src += srcPitch;
		}
		break;

	default:
		warning("TTFFont::cacheGlyph: Unsupported pixel mode %d", bitmap.pixel_mode);
		glyph.image.free();
		return false;
	}

	return true;
}

void TTFFont::assureCached(uint32 chr) const {
	if (!chr || !_allowLateCaching || _glyphs.contains(chr)) {
		return;
	}

	Glyph newGlyph;
	if (cacheGlyph(newGlyph, chr)) {
		_glyphs[chr] = newGlyph;
	}
}

Font *loadTTFFont(Common::SeekableReadStream &stream, int size, TTFSizeMode sizeMode, uint dpi, TTFRenderMode renderMode, bool stemDarkening, const uint32 *mapping) {
	TTFFont *font = new TTFFont();

	if (!font->load(stream, size, sizeMode, dpi, renderMode, stemDarkening, mapping)) {
		delete font;
		return 0;
	}

	return font;
}

Font *loadTTFFontFromArchive(const Common::String &filename, int size, TTFSizeMode sizeMode, uint dpi, TTFRenderMode renderMode, const uint32 *mapping) {
	Common::Archive *archive;
	if (!Common::File::exists("fonts.dat") || (archive = Common::makeZipArchive("fonts.dat")) == nullptr) {
		return 0;
	}

	Common::File f;
	if (!f.open(filename, *archive)) {
		return 0;
	}

	Font *font = loadTTFFont(f, size, sizeMode, dpi, renderMode, false, mapping);

	delete archive;
	return font;
}

} // End of namespace Graphics

namespace Common {
DECLARE_SINGLETON(Graphics::TTFLibrary);
} // End of namespace Common

#endif

