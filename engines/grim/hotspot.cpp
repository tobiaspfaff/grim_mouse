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
#include "graphics/pixelbuffer.h"
#include "common/array.h"

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


    
HotspotMan::HotspotMan() : _selectMode (0), _initialized(false) {
}

HotspotMan::~HotspotMan() {
}

int HotspotMan::addHotspot(const Common::String& name, const Math::Vector3d& pos, const Common::String& scene) {
    HotObject hs;
    hs._id = name;
    hs._pos = pos;
    hs._active = false;
    _hotobject.push_back(hs);
    
    return _hotobject.size() - 1;
        
    /*Common::String text = LuaBase::instance()->parseMsgText(name.c_str(), NULL);
    char fn[256];
    sprintf(fn,"hs/%s.txt",scene.c_str());
    printf(fn);
    printf("\n");
    FILE *fp = fopen(fn,"a");
    fprintf(fp, "%s %s\n",name.c_str(),text.c_str());
    fclose(fp);*/
    //return 12;
}

void HotspotMan::disableAll() {
    for (size_t i=0; i<_hotobject.size(); i++)
        _hotobject[i]._active = false;
}

void HotspotMan::drawActive(int mode) {
    if (mode == 1) {
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
    } else if(mode==2) {
        g_grim->getCurrSet()->setupCamera();    
        Set *set = g_grim->getCurrSet();
        Common::Array<PrimitiveObject> pa;
        for (int s=0; s<set->getSectorCount(); s++) {
            Sector* sector = set->getSectorBase(s);
            if ((sector->getType() & Sector::WalkType) != Sector::WalkType)
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
                p.createLine(Common::Point(x0,y0),Common::Point(x1,y1),Color(200,0,0));
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
    warning("Entering region %d", g_grim->getCurrSet()->getSetup());
    g_grim->getCurrSet()->setupCamera();
    for (size_t i=0; i<_hotobject.size(); i++) {
        int x=0,y=0;
        g_driver->worldToScreen(_hotobject[i]._pos,x,y);
        _hotobject[i]._rect = Common::Rect(x-10,y-10,x+10,y+10);        
    }    
    reload();
}

void HotspotMan::reload() {
    Common::String scene = g_grim->getCurrSet()->getName();
    if (_curScene != scene) {
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
            hs._type = type;
            hs._pos = Math::Vector3d(p[0],p[1],p[2]);
            for (int i=0; i<num; i++) {
                fscanf(fp,"%d %d", &x, &y);
                hs._region._pnts.push_back(Common::Point(x,y));
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

void HotspotMan::okKey() {
    if (_selectMode > 0) {
        Common::String defaultText = "";
        
        // find possible object defaults
        for (size_t i=0; i<_hotobject.size(); i++) {
            if (!_hotobject[i]._active) continue;
            Common::Point p ((_hotobject[i]._rect.left + _hotobject[i]._rect.right)/2,
                             (_hotobject[i]._rect.top + _hotobject[i]._rect.bottom)/2);
            if (_selectPoly.contains(p)) {
                defaultText = _hotobject[i]._id;
                defaultText.erase(0,3);                
                defaultText.erase(5);
                break;
            }
        }
        
        // object id
        InputDialog dlg("Object ID", defaultText);
        int res = dlg.runModal();
        Common::String id = dlg.getString();
        
        // The KeyUp event for CTRL has been eat by the gui loop, so we
        // need to reset it manually.
        g_grim->clearEventQueue();  
        if (res && !id.empty()) {
            _lastSetup = g_grim->getCurrSet()->getSetup();
            if (id[0] != 't' || id[1] != 'x') {
                _selectMode = -1;
                _lastName = id;
                warning("ok, walk to position and hit ENTER");
                return;
            }
            
            Common::String name = "_";
            int type = 1;
            id = "/" + get_scene_id() + id;
            for (size_t i=0; i<_hotobject.size(); i++) {
                if (strncmp(_hotobject[i]._id.c_str(), id.c_str(), id.size()) == 0) {
                    name.clear();
                    for (size_t k=id.size()+1; k<_hotobject[i]._id.size(); k++)
                        name += _hotobject[i]._id[k];
                }
            }
            if (name=="door")
                type = 2;                                                
            append_hotspot(id, name, type, Math::Vector3d(0,0,0));
        }
        _curScene = "";
        reload();
        _selectMode = 0;
        _selectPoly._pnts.clear();
    } else if (_selectMode < 0) {
        warning("ok, saving position");
        foreach (Actor *a, g_grim->getActiveActors()) {
            if (a->getName() == "Manny") {
                append_hotspot("to",_lastName, 3, a->getPos());
                _curScene = "";
                reload();
                _selectMode = 0;
                _selectPoly._pnts.clear();
                return;
            }
        }        
    }
}


void HotspotMan::event(const Common::Point& cursor, Common::EventType ev, int mode) {
    if (!_initialized)
        reload();
    if (mode == 1 && ev == Common::EVENT_LBUTTONDOWN && _selectMode >=0) {
        Common::Point pnt(cursor.x, cursor.y);
        if (_selectMode == 0)
            _selectPoly._pnts.push_back(pnt);
        else 
            _selectPoly._pnts[_selectMode] = pnt;
        _selectPoly._pnts.push_back(pnt);
        _selectMode++;
    } else if (mode==0) {
        int setup = g_grim->getCurrSet()->getSetup();    
        for (size_t i=0; i<_hotspot.size(); i++) {
            Common::String id = _hotspot[i]._id;
            if (_hotspot[i]._setup == setup && _hotspot[i]._region.contains(cursor)) {
                
                if (_hotspot[i]._type == 3) {
                    LuaObjects objects;
                    Math::Vector3d v = _hotspot[i]._pos;
                    objects.add(v.x());
                    objects.add(v.y());
                    objects.add(v.z());
                    LuaBase::instance()->callback("mouseWalk", objects);
                    return;
                }
                
                // find objecct
                int gid = -1;
                for (size_t j=0; j<_hotobject.size(); j++) {
                    if (strncmp(_hotobject[j]._id.c_str(), id.c_str(), id.size()) == 0) {
                        gid = j;
                        break;
                    }
                }
                if (gid >= 0) {
                    LuaObjects objects;
                    if (ev == Common::EVENT_LBUTTONDOWN)
                        objects.add(1);
                    else if (ev == Common::EVENT_RBUTTONDOWN)
                        objects.add(2);
                    else if (ev == Common::EVENT_MBUTTONDOWN)
                        objects.add(3);
                    else
                        return;
                
                    objects.add((int)gid);
                    warning("%d",gid);
                    LuaBase::instance()->callback("mouseCommand", objects);
                    return;
                }
            }
        }
        //Bitmap* zbmp = g_grim->getCurrSet()->getCurrSetup()->_bkgndZBm;
        //int index = zbmp->getWidth() * cursor.y + cursor.x;
        //uint32 x = zbmp->getData(zbmp->getActiveImage()-1).getValueAt(index);
        //warning("%d",x);
        Math::Vector3d r0, r1;
        g_grim->getCurrSet()->setupCamera();    
        g_driver->raycast(cursor.x, cursor.y, r0, r1);
        //warning("e0 %g %g %g",r0.x(),r0.y(),r0.z());
        //warning("e1 %g %g %g",r1.x(),r1.y(),r1.z());
        //Math::Vector3d dfl = r0 + (-r0.z()/r1.z())*r1;
        //warning("d0 %g %g %g",dfl.x(), dfl.y(), dfl.z());
        
        for (int i=0; i<50; i++) {
            Set *set = g_grim->getCurrSet();
            for (int s=0; s<set->getSectorCount(); s++) {
                Sector* sector = set->getSectorBase(s);
                if ((sector->getType() & Sector::WalkType) != Sector::WalkType)
                    continue;
                Math::Vector3d v = sector->raycast(r0,r1);
                if (sector->isPointInSector(v)) {
                    LuaObjects objects;
                    objects.add(v.x());
                    objects.add(v.y());
                    objects.add(v.z());
                    LuaBase::instance()->callback("mouseWalk", objects);
                    return;
                }
            }
            r0.z() -= 0.03; r1.z() -= 0.03;
        }        
    }
}
void HotspotMan::getName(Cursor* cursor) {
    Common::Point pos = cursor->getPosition();
    cursor->setCursor(0);
    for (size_t i=0; i<_hotobject.size(); i++) {
        if (_hotobject[i]._active && _hotobject[i]._rect.contains(pos)) {
            //cursor->setCursor(1);
            warning("current obj %s",_hotobject[i]._id.c_str());
        }
    }
}

void HotspotMan::hover(Cursor* cursor) {
    if (g_grim->getCurrSet() == NULL) return;
    Common::Point pos = cursor->getPosition();
    int setup = g_grim->getCurrSet()->getSetup();
    cursor->setCursor(0);
    for (size_t i=0; i<_hotspot.size(); i++) {
        if (_hotspot[i]._setup == setup && _hotspot[i]._region.contains(pos)) {
            cursor->setCursor(_hotspot[i]._type>1 ? 2 : 1);
        }
    }
    if (_selectMode > 0)
        _selectPoly._pnts[_selectMode] = Common::Point(pos.x, pos.y);    
}


} /* namespace */
