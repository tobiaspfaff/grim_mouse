/* ResidualVM - A 3D game interpreter
 *
 * ResidualVM is the legal property of its developers, whose names
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

#ifndef BACKEND_EVENTS_RESVM_SDL
#define BACKEND_EVENTS_RESVM_SDL

#include "sdl-events.h"

/**
 * Custom event source for ResidualVM with true joystick support.
 */
class ResVmSdlEventSource : public SdlEventSource {
protected:
	bool handleJoyButtonDown(SDL_Event &ev, Common::Event &event) override;
	bool handleJoyButtonUp(SDL_Event &ev, Common::Event &event) override;
	bool handleJoyAxisMotion(SDL_Event &ev, Common::Event &event) override;
	bool handleKbdMouse(Common::Event &event) override;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	bool handleControllerButton(const SDL_Event &ev, Common::Event &event, bool buttonUp) override;
	bool handleControllerAxisMotion(const SDL_Event &ev, Common::Event &event) override;
#endif

	bool shouldGenerateMouseEvents();
};

#endif
