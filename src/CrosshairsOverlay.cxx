/*-------------------------------------------------------------------------
  CrosshairsOverlay.cxx

  Written by Brian Schack

  Copyright (C) 2009 - 2018 Brian Schack

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

// Our include file
#include "CrosshairsOverlay.hxx"

// Other libraries' include files
#if defined( __APPLE__)
#  include <OpenGL/glu.h>	// Needed for gluOrtho2D(), ...
#else
#  ifdef WIN32
#    include <windows.h>
#  endif
#  include <GL/glu.h>
#endif

// Our project's include files.
#include "Overlays.hxx"

using namespace std;

CrosshairsOverlay::CrosshairsOverlay(Overlays& overlays):
    _overlays(overlays)
{
    // Subscribe to overlay notifications.
    subscribe(Notification::OverlayToggled);
}

CrosshairsOverlay::~CrosshairsOverlay()
{
}

void CrosshairsOverlay::draw()
{
    if (!_visible) {
	return;
    }

    GLfloat viewport[4];
    float x, y;
    glGetFloatv(GL_VIEWPORT, viewport);
    x = viewport[2] / 2.0;
    y = viewport[3] / 2.0;

    // Crosshairs are drawn in red.
    glColor4f(1.0, 0.0, 0.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix(); {
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix(); {
	    glLoadIdentity();
	    gluOrtho2D(0.0, viewport[2], 0.0, viewport[3]);

	    glBegin(GL_LINES); {
		glVertex2f(x - 20.0, y);
		glVertex2f(x + 20.0, y);
		glVertex2f(x, y - 20.0);
		glVertex2f(x, y + 20.0);
	    }
	    glEnd(); 
	}
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
    }
    glPopMatrix();
}

void CrosshairsOverlay::notification(Notification::type n)
{
    if (n == Notification::OverlayToggled) {
	_visible = _overlays.isVisible(Overlays::CROSSHAIRS);
    }
}
