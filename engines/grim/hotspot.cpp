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

#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "engines/grim/gfx_opengl.h"

#include "common/foreach.h"
#include "engines/grim/hotspot.h"
#include "engines/grim/primitives.h"
#include "engines/grim/gfx_base.h"
#include "engines/grim/grim.h"
#include "engines/grim/actor.h"
#include "engines/grim/set.h"
#include "engines/grim/inputdialog.h"
#include "engines/grim/cursor.h"
#include "engines/grim/lua.h"
#include "engines/grim/lua/lua.h"
#include "graphics/pixelbuffer.h"
#include "common/array.h"
#include "common/system.h"

namespace Grim {

void Polygon::draw(const Color& col) {
    PrimitiveObject line;
    for (size_t i=0; i<_pnts.size(); i++) {
        line.createLine(_pnts[i],_pnts[(i+1)%_pnts.size()],col);
        line.draw();
    }
}

bool Polygon::contains(const Common::Point& pos) {
    bool result = false;
    for (int i = 0, j = _pnts.size() - 1; i < (int)_pnts.size(); j = i++) {
    if ((_pnts[i].y > pos.y) != (_pnts[j].y > pos.y) &&
        (pos.x < (_pnts[j].x - _pnts[i].x) * (pos.y - _pnts[i].y) / (_pnts[j].y-_pnts[i].y) + _pnts[i].x)) {
        result = !result;
        }
    }
    return result;
}

void Polygon::move(const Common::Point& center) {
    Common::Point p(0,0);
    for (size_t i=0; i<_pnts.size(); i++)
        p += _pnts[i];
    Common::Point offset(center.x - p.x/_pnts.size(), center.y - p.y/_pnts.size());
    for (size_t i=0; i<_pnts.size(); i++)
        _pnts[i] += offset;
}

HotspotMan::HotspotMan() : _selectMode (0), _initialized(false), _lastClick(0), _ctrlMode(0) {
}

HotspotMan::~HotspotMan() {
}

int HotspotMan::addHotspot(const Common::String& name, const Math::Vector3d& pos, const Common::String& scene) {
    Common::String id = name, desc = name;
    for (size_t i=1;i<id.size();i++) {
        if (id[i]=='/') {
            id.erase(i);
            desc.erase(0,i+1);
            break;                
        }            
    }
    
    int num=0;
    for (size_t i=0; i<_hotobject.size(); i++) {
        if (_hotobject[i]._id.hasPrefix(id))
            num++;        
    }
    if (num>0)
        id += Common::String::format("_%d",num);
    
    HotObject hs;
    hs._id = id;
    hs._desc = desc;
    hs._pos = pos;
    hs._active = false;
    _hotobject.push_back(hs);
    
    return _hotobject.size() - 1;
}

void HotspotMan::disableAll() {
    for (size_t i=0; i<_hotobject.size(); i++)
        _hotobject[i]._active = false;
}

void HotspotMan::drawActive(int debug) {
    /*if (_ctrlMode == Dialog) {
        for (int i=0; i<_lines; i++) {
            PrimitiveObject x;
            x.createRectangle(Common::Point(_x0,_y0+i*_h),Common::Point(_x0+_w,_y0+(i+1)*_h),Color(200,200,200),false);
            x.draw();
        }
        //return;
    }*/
    if (_ctrlMode == Inventory) {
        const int dx = 5;//, dx2 = 6;
        int num_rows = (_inventory.size()-1)/_cols+1; 
        int x1 = _x0+_w*_cols, y1 =_y0+_h*num_rows;
        g_driver->blackbox(0,0,640,480,0.3);
        g_driver->blackbox(_x0-dx,_y0-dx,x1+dx,y1+dx,0.7);
        PrimitiveObject rect;
        Color col(195,195,100);
        rect.createRectangle(Common::Point(_x0-dx,_y0-dx),Common::Point(x1+dx,y1+dx), col, false);
        rect.draw();
        //rect.createRectangle(Common::Point(_x0-dx2,_y0-dx2),Common::Point(x1+dx2,y1+dx2), col, false);
        //rect.draw();

        for (int j=0,idx=0; j<_rows; j++) {
            for (int i=0; i<_cols; i++,idx++) {
                if (idx >= _inventory.size()) break;
                _inventory[idx]._bmp->draw(_x0+i*_w,_y0+j*_h);
            }
        }
    }
    if (debug > 0) {
        PrimitiveObject prim;
        for (size_t i=0; i<_hotobject.size(); i++) {
            if (_hotobject[i]._active) {
                Common::Point p1(_hotobject[i]._rect.left, _hotobject[i]._rect.top);
                Common::Point p2(_hotobject[i]._rect.right, _hotobject[i]._rect.bottom);
                prim.createRectangle(p1,p2,Color(200,0,200),false);
                prim.draw();
                
            }
        }
        int setup = g_grim->getCurrSet()->getSetup();             
        for (size_t i=0; i<_hotspot.size(); i++) {
            if (_hotspot[i]._setup == setup)
                _hotspot[i]._region.draw(Color(0,200,200));            
        }
        if (_selectMode > 0)
            _selectPoly.draw(Color(200,200,200));
    }
    if(debug==2) {
        g_grim->getCurrSet()->setupCamera();    
        Set *set = g_grim->getCurrSet();
        Common::Array<PrimitiveObject> pa;
        for (int s=0; s<set->getSectorCount(); s++) {
            Sector* sector = set->getSectorBase(s);
            if ((sector->getType() & Sector::WalkType) != Sector::WalkType &&
                (sector->getType() & Sector::HotType) != Sector::HotType)
                continue;
            int num = sector->getNumVertices();
            for (int j=0; j<num; j++) {
                int d0=j, d1=(j+1)%num;
                int x0,x1,y0,y1;
                Math::Vector3d v0 = sector->getVertices()[d0];
                Math::Vector3d v1 = sector->getVertices()[d1];
                g_driver->worldToScreen(v0, x0, y0);
                g_driver->worldToScreen(v1, x1, y1);
                PrimitiveObject p;
                Color col = Color(0,200,0);
                if ((sector->getType() & Sector::HotType) == Sector::HotType)
                    col = Color(200,0,0);
                p.createLine(Common::Point(x0,y0),Common::Point(x1,y1),col);
                pa.push_back(p);
            }
        }    
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, 640,480, 0, 0, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        for (size_t k=0; k<pa.size(); k++) {
            pa[k].draw();
        }    
    }
}

void HotspotMan::updatePerspective() {
    //warning("Entering region %d", g_grim->getCurrSet()->getSetup());
    g_grim->getCurrSet()->setupCamera();
    for (size_t i=0; i<_hotobject.size(); i++) {
        int x=0,y=0;
        g_driver->worldToScreen(_hotobject[i]._pos,x,y);
        _hotobject[i]._rect = Common::Rect(x-10,y-10,x+10,y+10);        
    }
    reload(false);
}

void HotspotMan::notifyWalkOut() {
    if (_selectMode < 0)
        okKey();
}

void HotspotMan::debug(int num) {
    for (size_t i=0; i<_hotobject.size(); i++) {
        warning("hobj %c %s",_hotobject[i]._active ? 'A':'D',(_hotobject[i]._id + "/" + _hotobject[i]._desc).c_str());
    }    
    for (size_t i=0; i<_hotspot.size(); i++) {
        char c = (_hotspot[i]._objId>=0 && _hotobject[_hotspot[i]._objId]._active) ? 'A' : 'D';
        warning("hspt %c %s",c,(_hotspot[i]._id + "/" + _hotspot[i]._desc).c_str());
    }    
}


void HotspotMan::reload(bool always) {
    _initialized = true;
    Common::String scene = g_grim->getCurrSet()->getName();
    if (_curScene != scene || always) {
        _curScene = scene;
        _hotspot.clear();
        
        char fname[256];
        sprintf(fname, "/Users/tpfaff/code/residualvm/hs/%s.hot",scene.c_str());
        FILE *fp = fopen(fname, "r");
        if (!fp) return;
        
        int setup, type, num, x, y;
        char id[256], name[256];
        float  p[3];
        while(fscanf(fp,"%d %s \"%100[^\"]\" %d %g %g %g %d", &setup, id, name, &type, &p[0], &p[1], &p[2], &num) > 0) {
            Hotspot hs;
            hs._id = id;
            hs._setup = setup;
            hs._desc = name;
            hs._type = type;
            hs._objId = -1;
            hs._pos = Math::Vector3d(p[0],p[1],p[2]);
            for (int i=0; i<num; i++) {
                fscanf(fp,"%d %d", &x, &y);
                hs._region._pnts.push_back(Common::Point(x,y));
            }
            for (size_t j=0; j<_hotobject.size(); j++) {
                if (_hotobject[j]._id == hs._id)
                    hs._objId = j;
            }
            _hotspot.push_back(hs);
        }
        fclose(fp); 
    }
}

inline int min(int a, int b) { return a<b ? a : b; }
inline int max(int a, int b) { return a>b ? a : b; }

void HotspotMan::cancel() {
    _selectMode = 0;
    _selectPoly._pnts.clear();
}

Common::String get_scene_id() {
    Common::String scene = g_grim->getCurrSet()->getName();
    for(int i=0; i<4; i++) scene.deleteLastChar();
    return scene;
}

void split_string(const Common::String& str, char split, Common::String& left, Common::String& right) {
    left = ""; right = "";
    bool useleft = true;
    for (size_t i=0; i<str.size(); i++) {
        if (str[i]==split)
            useleft = false;
        else if (useleft)
            left += str[i];
        else
            right += str[i];
    }
}

void HotspotMan::append_hotspot(const Common::String& id, const Common::String& name, int type, const Math::Vector3d& v) {
    Common::String scene = get_scene_id();
    char fname[256];
    sprintf(fname, "/Users/tpfaff/code/residualvm/hs/%s.set.hot",scene.c_str());
    FILE *fp = fopen(fname,"a");
    fprintf(fp, "%d %s \"%s\" %d %g %g %g %d", _lastSetup, id.c_str(), name.c_str(),
            type, v.x(), v.y(), v.z(), _selectPoly._pnts.size()-1);
    for (size_t i=0; i<_selectPoly._pnts.size()-1; i++)
        fprintf(fp," %d %d",_selectPoly._pnts[i].x, _selectPoly._pnts[i].y);
    fprintf(fp,"\n");
    fclose(fp);                
}

inline Actor* getManny() {
    foreach (Actor *a, g_grim->getActiveActors()) {
        if (a->getName() == "Manny")
            return a;
    }
    return 0;
}

void HotspotMan::okKey() {
    if (_selectMode > 0) {
        Common::String defaultText = "", desc="";
        
        // find possible object defaults
        for (size_t i=0; i<_hotobject.size(); i++) {
            if (!_hotobject[i]._active) continue;
            Common::Point p ((_hotobject[i]._rect.left + _hotobject[i]._rect.right)/2,
                             (_hotobject[i]._rect.top + _hotobject[i]._rect.bottom)/2);
            if (_selectPoly.contains(p)) {
                defaultText = _hotobject[i]._id;
                desc = "[default: " + _hotobject[i]._desc + " ]";
                defaultText.erase(0,3);      
                break;
            }
        }
        
        // object id
        InputDialog dlg("Object ID " +desc, defaultText);
        int res = dlg.runModal();
        Common::String id = dlg.getString();
        
        // The KeyUp event for CTRL has been eat by the gui loop, so we
        // need to reset it manually.
        g_grim->clearEventQueue();  
        if (res && !id.empty()) {
            _lastSetup = g_grim->getCurrSet()->getSetup();
            if (id[0] == 's' && id[1] == 'x') {
                int type = 10;
                Common::String code,name;
                split_string(id,'_',code,name);
                if (name == "star") type = 11; 
                if (name == "go") type = 12; 
                if (name == "right") type = 13; 
                if (name == "left") type = 14; 
                if (name == "up") type = 15; 
                if (name == "down") type = 16; 
                append_hotspot(code, name, type, Math::Vector3d(0,0,0));
            } else if (id[0] == 't' && id[1] == 'x') {
                Common::String name = "_";
                int type = 1;
                id = "/" + get_scene_id() + id;
                for (size_t i=0; i<_hotobject.size(); i++) {
                    if (_hotobject[i]._id == id) 
                        name = _hotobject[i]._desc;
                }
                if (name=="door")
                    type = 2;                                                
                append_hotspot(id, name, type, Math::Vector3d(0,0,0));
            } else {
                _selectMode = -1;
                _lastName = id;
                warning("ok, walk to position and hit ENTER");
                return;
            }
        }
        reload(true);
        _selectMode = 0;
        _selectPoly._pnts.clear();
    } else if (_selectMode < 0) {
        warning("ok, saving position");
        Actor* manny = getManny();
        if (manny) {
            append_hotspot("to",_lastName, 3, manny->getPos());
            reload(true);
            _selectMode = 0;
            _selectPoly._pnts.clear();            
        }        
    }
}

// get min distance s for x0+s*x1, y0+t*y1
double line_line_dist(const Math::Vector3d& x0, const Math::Vector3d& x1, 
                      const Math::Vector3d& y0, const Math::Vector3d& y1) {
    double a = x1.dotProduct(x1), b = -x1.dotProduct(y1), c = y1.dotProduct(y1);
    double d = -x1.dotProduct(x0-y0), e = y1.dotProduct(x0-y0);
    return (c*d-b*e)/(c*a-b*b);
}

void HotspotMan::event(const Common::Point& cursor, const Common::Event& ev, int debug) {
    if (!_initialized)
        reload(true);
    bool climbing = lua_getnumber(LuaBase::instance()->queryVariable("system.currentActor.is_climbing")) != 0;

    int button = 0;
    if (ev.type == Common::EVENT_LBUTTONDOWN)
        button = 1;
    else if (ev.type == Common::EVENT_RBUTTONDOWN)
        button = 2;
    else if (ev.type == Common::EVENT_MBUTTONDOWN)
        button = 3;
    if (ev.kbd.hasFlags(Common::KBD_ALT))
        button = 2;
    if (ev.kbd.hasFlags(Common::KBD_CTRL))
        button = 3;
    update();
    unsigned int currentTime = g_system->getMillis();
    bool doubleClick = (currentTime - _lastClick) < 500;
    _lastClick = currentTime;

    if (_ctrlMode == Dialog && button > 0) {
        // dialog mode
        int dialog = inBox(cursor), click = 1;
        if (button==2) click = 2;
        LuaObjects objects;
        objects.add(dialog);
        objects.add(click);
        LuaBase::instance()->callback("dialogSelect", objects);
    } else if (_ctrlMode == Inventory && button > 0) {
         for (int j=0,idx=0; j<_rows; j++) {
            for (int i=0; i<_cols; i++,idx++) {
                if (idx >= _inventory.size()) break;
                if (cursor.x >= _x0+i*_w && cursor.x <_x0+(i+1)*_w &&
                    cursor.y >= _y0+j*_h && cursor.y <_y0+(j+1)*_h) {
                    LuaObjects objects;
                    objects.add(_inventory[idx]._id.c_str());
                    objects.add(button);
                    LuaBase::instance()->callback("mouseInventory", objects);
                    return;
                }
            }
        }
        LuaObjects objects;
        objects.add("outside");
        objects.add(button);
        LuaBase::instance()->callback("mouseInventory", objects);
        return;
    }

    if (debug > 0 && button == 1 && _selectMode >=0) {
        Common::Point pnt(cursor.x, cursor.y);
        if (_selectMode == 0)
            _selectPoly._pnts.push_back(pnt);
        else 
            _selectPoly._pnts[_selectMode] = pnt;
        _selectPoly._pnts.push_back(pnt);
        _selectMode++;
    } 

    if (debug==0) {
        // ------- click on hot spots ------------        
        int setup = g_grim->getCurrSet()->getSetup();    
        for (size_t i=0; i<_hotspot.size(); i++) {
            Hotspot& hs = _hotspot[i];
            if (hs._setup == setup && hs._region.contains(cursor) && !climbing) {

                // special mode calls
                if ((_ctrlMode == Dialog && hs._type >= 20) ||
                    (_ctrlMode == Special && hs._type >= 10 && hs._type < 20) ||
                    (_ctrlMode == Linear && hs._type >= 10 && hs._type < 20)) { 
                    if (button==0 && _ctrlMode != Special) continue;
                    LuaObjects objects;
                    objects.add(hs._id.c_str());
                    objects.add(button);
                    LuaBase::instance()->callback("mouseSpecial", objects);
                    return;
                }
                // normal mode
                if ((_ctrlMode != Normal && _ctrlMode != NoWalk) || 
                    hs._type >= 10 || button==0) continue;
                if (hs._objId>=0 && !_hotobject[hs._objId]._active) continue;
                if (hs._type == 3) {
                    LuaObjects objects;
                    Math::Vector3d v = hs._pos;
                    objects.add(button);
                    objects.add(v.x());
                    objects.add(v.y());
                    objects.add(v.z());
                    objects.add(doubleClick ? 1 : 0);
                    LuaBase::instance()->callback("mouseWalk", objects);
                    return;
                }

                int gid = hs._objId;
                warning("%d",gid);
                if (gid >= 0) {
                    LuaObjects objects;
                    objects.add(button);
                    objects.add(gid);
                    int walk = doubleClick ? 2 : 1;
                    objects.add(walk);
                    LuaBase::instance()->callback("mouseCommand", objects);
                    return;
                }
            }
        }
        
        if (_ctrlMode == Normal) 
            freeClick(cursor,button,doubleClick,climbing);
        else if (_ctrlMode == NoWalk) {
            LuaObjects objects;
            objects.add(button);
            LuaBase::instance()->callback("mouseNowalk", objects);
        }
    }

    if (_ctrlMode == Linear && button > 0) {
        // linear mode
        Actor* manny = getManny();
        if (!manny) return;
        Math::Vector3d r0, r1;
        g_grim->getCurrSet()->setupCamera();    
        g_driver->raycast(cursor.x, cursor.y, r0, r1);
        Math::Vector3d p0 = manny->getPos(), p1 = _axis;
        r0 -= _axis * _offset;
        r1 -= _axis * _offset;
        float s = line_line_dist(p0,p1,r0,r1);

        LuaObjects objects;
        objects.add(p0.dotProduct(_axis) + s);
        objects.add(s);
        objects.add(button);
        LuaBase::instance()->callback("linearMove", objects);
        return;
    }
}
void HotspotMan::getName(Cursor* cursor) {
    Common::Point pos = cursor->getPosition();
    cursor->setCursor(0);
    for (size_t i=0; i<_hotobject.size(); i++) {
        if (_hotobject[i]._active && _hotobject[i]._rect.contains(pos)) {
            //cursor->setCursor(1);
            warning("current obj %s %s",_hotobject[i]._id.c_str(),_hotobject[i]._desc.c_str());
        }
    }
}

void HotspotMan::hover(Cursor* cursor) {
    if (g_grim->getCurrSet() == NULL) return;
    update();
    Common::Point pos = cursor->getPosition();
    int setup = g_grim->getCurrSet()->getSetup();
    cursor->setCursor(0);

    for (size_t i=0; i<_hotspot.size(); i++) {
        if (_hotspot[i]._setup == setup && _hotspot[i]._region.contains(pos)) {
            if (_hotspot[i]._objId>=0 && !_hotobject[_hotspot[i]._objId]._active)
                continue;
            if (_ctrlMode == Normal || _ctrlMode == NoWalk) {
                if (_hotspot[i]._type>=10) continue;
                cursor->setCursor((_hotspot[i]._type==2 || _hotspot[i]._type==3) ? 2 : 1);    
            } else if (_ctrlMode == Dialog) {
                if (_hotspot[i]._type<20) continue;
                cursor->setCursor(_hotspot[i]._type-20);
            } else if (_ctrlMode == Special || _ctrlMode == Linear) {
                if (_hotspot[i]._type<10 || _hotspot[i]._type >= 20) continue;
                cursor->setCursor(_hotspot[i]._type-10);
            }
        }
    }
    if (_selectMode > 0)
        _selectPoly._pnts[_selectMode] = Common::Point(pos.x, pos.y);
    
    if (_ctrlMode == Dialog) {
        // dialog mode
        int dialog = inBox(pos), click = 0;
        LuaObjects objects;
        objects.add(dialog);
        objects.add(click);
        LuaBase::instance()->callback("dialogSelect", objects);
    } else if (_ctrlMode == Inventory) {
         for (int j=0,idx=0; j<_rows; j++) {
            for (int i=0; i<_cols; i++,idx++) {
                if (idx >= _inventory.size()) break;
                if (pos.x >= _x0+i*_w && pos.x <_x0+(i+1)*_w &&
                    pos.y >= _y0+j*_h && pos.y <_y0+(j+1)*_h) {
                    cursor->setCursor(1);
                }
            }
        }
    }
}

void HotspotMan::freeClick(const Common::Point& cursor, int button, bool doubleClick, bool climbing) {
    Math::Vector3d r0, r1;
    g_grim->getCurrSet()->setupCamera();    
    g_driver->raycast(cursor.x, cursor.y, r0, r1);
    
    // climbing
    if (climbing) {
        Actor* manny = getManny();
        if (!manny) return;
        r0 += Math::Vector3d(0,0,-0.25);
        r1 += Math::Vector3d(0,0,-0.25); // center on mannys body, not feet
        Math::Vector3d p0 = manny->getPos(), p1 = Math::Vector3d(0,0,1);
        float s = line_line_dist(p0,p1,r0,r1);
        
        LuaObjects objects;
        objects.add(p0.z() + s);
        objects.add(s);
        LuaBase::instance()->callback("climbTowards", objects);
        return;
    }
    
    // walking
    for (int i=0; i<50; i++) {
        Set *set = g_grim->getCurrSet();
        for (int s=0; s<set->getSectorCount(); s++) {
            Sector* sector = set->getSectorBase(s);
            if ((sector->getType() & Sector::WalkType) != Sector::WalkType)
                continue;
            Math::Vector3d v = sector->raycast(r0,r1);
            if (sector->isPointInSector(v)) {
                LuaObjects objects;
                objects.add(button);
                objects.add(v.x());
                objects.add(v.y());
                objects.add(v.z());
                objects.add(doubleClick ? 1 : 0);

                LuaBase::instance()->callback("mouseWalk", objects);
                return;
            }
        }
        r0.z() -= 0.03; r1.z() -= 0.03;
    }     
}   

bool HotspotMan::restoreState(SaveGame *savedState) {
    savedState->beginSection('HOTM');
    int num = savedState->readLESint32();
    _hotobject.clear();
    for (int i=0; i<num; i++) {
        HotObject hs;
        hs._id = savedState->readString();
        hs._desc = savedState->readString();
        hs._pos = savedState->readVector3d();
        hs._rect.top = savedState->readLEUint16();
        hs._rect.left = savedState->readLEUint16();
        hs._rect.bottom = savedState->readLEUint16();
        hs._rect.right = savedState->readLEUint16();
        hs._active = savedState->readBool();
        _hotobject.push_back(hs);
    }
    savedState->endSection();
    _icons.clear();

    reload(true);
    return true;
}
void HotspotMan::saveState(SaveGame *savedState) {
    savedState->beginSection('HOTM');
    savedState->writeLESint32(_hotobject.size());
    for (size_t i=0; i<_hotobject.size(); i++) {
        HotObject& hs = _hotobject[i];
        savedState->writeString(hs._id);
        savedState->writeString(hs._desc);
        savedState->writeVector3d(hs._pos);
        savedState->writeLEUint16(hs._rect.top);
        savedState->writeLEUint16(hs._rect.left);
        savedState->writeLEUint16(hs._rect.bottom);
        savedState->writeLEUint16(hs._rect.right);
        savedState->writeBool(hs._active);
    }
    savedState->endSection();
}
int HotspotMan::inBox(const Common::Point& p) {
    for (int j=0; j<_rows; j++) {
        for (int i=0; i<_cols; i++) {
            if (p.x >= _x0+i*_w && p.x < _x0+(i+1)*_w &&
                p.y >= _y0+j*_h && p.y < _y0+(j+1)*_h)
                return j*(_cols)+i+1;
        }
    }
    return 0;
}

void HotspotMan::update() {
    for (size_t i=0; i<_hotspot.size(); i++) {
        if (_hotspot[i]._type != 4) continue;
        foreach(Actor* a,g_grim->getActiveActors()) {
            if (a->getName()==_hotspot[i]._desc) {
                 g_grim->getCurrSet()->setupCamera();
                int x=0,y=0;
                Math::Vector3d trans = _hotspot[i]._pos;
                Math::Quaternion quat = a->getRotationQuat();
                Math::Quaternion quatInv(-quat.x(),-quat.y(),-quat.z(),quat.w());
                quatInv.toMatrix().transform(&trans, true);
                Math::Vector3d pos = a->getPos() + trans;
                g_driver->worldToScreen(pos,x,y);
                Common::Point p(x,y);
                _hotspot[i]._region.move(p);
            }
        }
    }
}

void HotspotMan::updateHotspot(const Common::String& id, const Math::Vector3d& pos, int vis) {
    for (size_t i=0; i<_hotspot.size(); i++) {
        if (_hotspot[i]._id == id) {
            int x=(int)pos.x(), y=(int)pos.y();
            if (vis>0) {
                g_grim->getCurrSet()->setupCamera();
                g_driver->worldToScreen(pos,x,y);
            }
            Common::Point p(x,y);
            _hotspot[i]._region.move(p);
        }
    }
}

void HotspotMan::resetInventory() {
    _inventory.clear();
}

void HotspotMan::addInventory(const Common::String& id, const Common::String& pic) {
    InventoryItem it;
    it._id = id;
    it._bmp = _icons[pic];
    if (!it._bmp) {
        it._bmp = Bitmap::create(pic + ".tga");
        it._bmp->_data->load();
        _icons[pic] = it._bmp;
    }
    _inventory.push_back(it);
}

} /* namespace */
