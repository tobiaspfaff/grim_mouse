MODULE := zlib

MODULE_OBJS := \
	adler32.o \
	deflate.o \
	gzread.o \
	inffast.o \
	trees.o \
	compress.o \
	gzclose.o \
	gzwrite.o \
	inflate.o \
	uncompr.o \
	crc32.o	\
	gzlib.o	\
	infback.o \
	inftrees.o \
	zutil.o

# Include common rules
include $(srcdir)/rules.mk
