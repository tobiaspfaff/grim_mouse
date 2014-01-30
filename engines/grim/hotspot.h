/* ResidualVM - A 3D game interpreter
 *
 * ResidualVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the AUTHORS
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef HOTSPOT_H_
#define HOTSPOT_H_

#include "common/rect.h"
#include "common/array.h"
#include "primitives.h"
#include "math/vector3d.h"
#include "common/events.h"

namespace Grim {

class GrimEngine;
class Cursor;
class SaveGame;

struct Polygon {
    Common::Array<Common::Point> _pnts;
    
    void draw(const Color &col);
    bool contains(const Common::Point& pos);
};

struct HotObject {
    Common::String _id, _desc;
    Math::Vector3d _pos;
    Common::Rect _rect;    
    bool _active;
};

struct Hotspot {
	Common::String _id;
    Polygon _region;
    Math::Vector3d _pos;
    int _setup;
    int _type;
    int _objId;
};

class HotspotMan {
public:
    HotspotMan();
    virtual ~HotspotMan();
    
    int addHotspot(const Common::String& name, const Math::Vector3d& pos, const Common::String& scene);    
    void disableAll();
    HotObject& getObject(int idx) { return _hotobject[idx]; }
    
    void okKey();
    void cancel();
    void event(const Common::Point& cursor, Common::EventType ev, int mode);
    void getName(Cursor* cursor);
    void hover(Cursor* cursor);
    void updatePerspective();
    void drawActive(int mode);    
    void reload(bool always);
    bool restoreState(SaveGame *savedState);
    void saveState(SaveGame* savedState);
    void switchMode(int ctrlMode) { _ctrlMode = ctrlMode; }
    void setupDialog(int x0, int y0, int w, int h, int lines) { _x0=x0; _y0=y0; _w=w; _h=h; _lines=lines; }
    void notifyWalkOut();
    void debug(int num);
    inline int getCtrlMode() { return _ctrlMode; }
protected:
    void append_hotspot(const Common::String& id, const Common::String& name, int type, const Math::Vector3d& v);
    int inDialogBox(const Common::Point& p);

    // dialog support
    int _ctrlMode, _lines;
    int _x0,_y0,_w,_h;

    unsigned int _lastClick;
    bool _initialized;
    Common::Array<Hotspot> _hotspot;
    Common::String _curScene;
    Common::Array<HotObject> _hotobject;
    
    // hotspot editing
    int _selectMode, _lastSetup;
    Polygon _selectPoly;
    Common::String _lastName;        
};

} /* namespace  */
#endif /* HOTSPOT_H_ */
