/*-------------------------------------------------------------------------
  RangeRingsOverlay.cxx

  Written by Brian Schack

  Copyright (C) 2011 - 2018 Brian Schack

  Draws range rings.

  This file is part of Atlas.

  Atlas is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Atlas is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU General Public License
  along with Atlas.  If not, see <http://www.gnu.org/licenses/>.
---------------------------------------------------------------------------*/

#ifndef _RANGERINGS_OVERLAY_H
#define _RANGERINGS_OVERLAY_H

// #if defined( __APPLE__)		// For GLuint
// #  include <OpenGL/gl.h>
// #else
// #  include <GL/gl.h>
// #endif

#include "Notifications.hxx"
#include "OOGL.hxx"
#include "Overlays.hxx"

class RangeRingsOverlay: public Subscriber {
  public:
    RangeRingsOverlay(Overlays& overlays);
    ~RangeRingsOverlay();

    void draw();

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    void _createCrosshairs();
    void _createCircle();
    void _createRose();

    void _drawCircle(float x, float y, float radius);

    void _pushView();
    void _popView();

    Overlays& _overlays;
    DisplayList _crosshairs, _circle, _rose, _rangeRings;
};

#endif
