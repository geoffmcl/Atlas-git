/*-------------------------------------------------------------------------
  RangeRingsOverlay.cxx

  Written by Brian Schack

  Copyright (C) 2011 - 2012 Brian Schack

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "RangeRingsOverlay.hxx"
#include "LayoutManager.hxx"
#include "Globals.hxx"
#include "AtlasWindow.hxx"

using namespace std;

static const float __ringColour[4] = { 0.85, 0.35, 0.25, 1.0 };
static const float __crosshairSize = 20.0;
static const float __pointSize = 10.0;

RangeRingsOverlay::RangeRingsOverlay(Overlays& overlays):
    _dirty(true), _overlays(overlays)
{
    // Create all the display list indices.  Note that we must have a
    // valid OpenGL context for this to work.
    _crosshairsDL = glGenLists(1);
    _circleDL = glGenLists(1);
    _roseDL = glGenLists(1);
    _rangeRingsDL = glGenLists(1);

    _createCrosshairs();
    _createCircle();
    _createRose();

    // Subscribe to zoomed notifications.
    subscribe(Notification::Zoomed);
}

RangeRingsOverlay::~RangeRingsOverlay()
{
    glDeleteLists(_crosshairsDL, 1);
    glDeleteLists(_circleDL, 1);
    glDeleteLists(_roseDL, 1);
    glDeleteLists(_rangeRingsDL, 1);
}

// Draw a set of range rings on the map.
void RangeRingsOverlay::draw()
{
    if (_dirty) {
	assert(_rangeRingsDL != 0);
	glNewList(_rangeRingsDL, GL_COMPILE); {
	    // Get our centre.
	    GLfloat viewport[4];
	    glGetFloatv(GL_VIEWPORT, viewport);
	    float x = viewport[2] / 2.0;
	    float y = viewport[3] / 2.0;

	    // Set up some unsullied viewing and projection matrices.
	    _pushView(); {
		// We want an orthographic view.
		gluOrtho2D(0.0, viewport[2], 0.0, viewport[3]);

		// RangeRings are drawn in a reddish colour.
		glColor4fv(__ringColour);

		glPushMatrix(); {
		    glTranslatef(x, y, 0.0);
		    glScalef(__crosshairSize, __crosshairSize, __crosshairSize);
		    glCallList(_crosshairsDL);
		}
		glPopMatrix();

		// We want to find nice values for our range ranges, where
		// "nice" means the numbers 2, 5, and 10.  (This code was
		// lifted from Graphs.cxx, by the way.)
		float w = viewport[2] / 2.0;
		float h = viewport[3] / 2.0;
		float size = sqrt(w * w + h * h);

		// Convert size from pixels to nautical miles.
		double metresPerPixel = _overlays.aw()->scale();
		size *= metresPerPixel / SG_NM_TO_METER;

		// Break it down into an exponent (base-10) and mantissa
		// (1.0 <= mantissa < 10.0).
		int exponent = floor(log10(size));
		float mantissa = size / pow(10.0, exponent);

		// We want four range rings.
		float big, medium, small, tiny;
		float base = pow(10.0, exponent);
		if (mantissa < 2) {
		    big = 1 * base;
		    medium = 0.5 * base;
		    small = 0.2 * base;
		    tiny = 0.1 * base;
		} else if (mantissa < 5) {
		    big = 2 * base;
		    medium = 1 * base;
		    small = 0.5 * base;
		    tiny = 0.2 * base;
		} else {
		    big = 5 * base;
		    medium = 2 * base;
		    small = 1 * base;
		    tiny = 0.5 * base;
		}
		// Convert back from nautical miles to pixels.
		big *= SG_NM_TO_METER / metresPerPixel;
		medium *= SG_NM_TO_METER / metresPerPixel;
		small *= SG_NM_TO_METER / metresPerPixel;
		tiny *= SG_NM_TO_METER / metresPerPixel;

		_drawCircle(x, y, big);
		_drawCircle(x, y, medium);
		_drawCircle(x, y, small);
		_drawCircle(x, y, tiny);

		// // Draw a compass rose aligned with magnetic north.
		// float smallest = w > h ? h : w;
		// glPushMatrix(); {
		// 	glTranslatef(x, y, 0.0);
		// 	glScalef(smallest, smallest, smallest);
		// 	if (globals.magnetic) {
		// 	    // EYE - how do we get our current position?  It
		// 	    // should really be part of Globals.
		// 	    double var = magneticVariation(37.5, -122.5);
		// 	    glRotatef(-var, 0.0, 0.0, 1.0);
		// 	}
		// 	glCallList(_roseDL);
		// }
		// glPopMatrix();
	    }
	    _popView();
	}
	glEndList();
	_dirty = false;
    }

    glCallList(_rangeRingsDL);
}

void RangeRingsOverlay::notification(Notification::type n)
{
    if (n == Notification::Zoomed) {
	// Record ourselves as dirty.
	_dirty = true;
    } else {
	assert(false);
    }
}

// Draws a circle of the given radius (given in pixels).
void RangeRingsOverlay::_drawCircle(float x, float y, float radius)
{
    glPushMatrix(); {
    	glTranslatef(x, y, 0.0);
    	glScalef(radius, radius, radius);
    	glCallList(_circleDL);
    }
    glPopMatrix();
    
    // Label the top of the segment.
    float nm = radius * _overlays.aw()->scale() / SG_NM_TO_METER;
    AtlasString str;
    if (nm >= 1) {
    	str.printf("%.0f nm\n", nm);
    } else {
    	str.printf("%.0g nm\n", nm);
    }
    // EYE - magic number
    LayoutManager lm(str.str(), _overlays.regularFont(), __pointSize);
    lm.setBoxed(true);
    lm.moveTo(x, y + radius);
    lm.drawText();
}

// Creates a crosshair in the current colour and line width, with arms
// 1 unit long (ie, it's 2 units high and 2 units wide).
void RangeRingsOverlay::_createCrosshairs()
{
    assert(_crosshairsDL != 0);
    glNewList(_crosshairsDL, GL_COMPILE); {
	glBegin(GL_LINES); {
	    glVertex2f(-1.0, 0.0);
	    glVertex2f(1.0, 0.0);
	    glVertex2f(0.0, -1.0);
	    glVertex2f(0.0, 1.0);
	}
	glEnd(); 
    }
    glEndList();
}

// Creates a circle of radius 1.0, centred at 0, 0.  It is drawn in
// the XY plane, with north in the positive Y direction, and east in
// the positive X direction, using the current colour and line width.
void RangeRingsOverlay::_createCircle()
{
    assert(_circleDL != 0);
    glNewList(_circleDL, GL_COMPILE); {
	glBegin(GL_LINE_LOOP); {
	    const float two_pi = 360.0 * SG_DEGREES_TO_RADIANS;
	    // Draw the circle as 72 short line segments.
	    int segments = 72;
	    for (int i = 0; i < segments; i++) {
		float theta = two_pi * i / segments;
		glVertex2f(sin(theta), cos(theta));
	    }
	}
	glEnd(); 
    }
    glEndList();
}

// Creates a compass rose of radius 1.0, centred at 0, 0.  It is drawn
// in the XY plane, with north in the positive Y direction, and east
// in the positive X direction, using the current colour and line
// width.  Note that the compass rose doesn't include the actual
// circle of the rose, just the tick marks and a north arrow.
void RangeRingsOverlay::_createRose()
{
    assert(_roseDL != 0);
    glNewList(_roseDL, GL_COMPILE); {
	// Now draw the ticks.
	// EYE - magic numbers
	const float thirtiesTickLength = 0.1;
	const float tensTickLength = thirtiesTickLength * 0.8;
	const float fivesTickLength = thirtiesTickLength * 0.5;
	const float onesTickLength = thirtiesTickLength * 0.25;
	const float pointSize = 0.05;
	const float tickEnd = 1.0 - pointSize;
	for (int i = 0; i < 360; i ++) {
	    glPushMatrix(); {
		glRotatef(-i, 0.0, 0.0, 1.0);
		glBegin(GL_LINES); {
		    glVertex2f(0.0, tickEnd);
		    if (i % 30 == 0) {
			// 30 degree tick.
			glVertex2f(0.0, tickEnd - thirtiesTickLength);
		    } else if (i % 10 == 0) {
			// 10 degree tick.
			glVertex2f(0.0, tickEnd - tensTickLength);
		    } else if (i % 5 == 0) {
			// 5 degree tick.
			glVertex2f(0.0, tickEnd - fivesTickLength);
		    } else {
			// 1 degree tick.
			glVertex2f(0.0, tickEnd - onesTickLength);
		    }
		}
		glEnd();
	    }
	    glPopMatrix();
	}

	// Draw a line due north.
	glBegin(GL_LINES); {
	    glVertex2f(0.0, 0.0);
	    glVertex2f(0.0, tickEnd);
	}
	glEnd();

	// Label the rose.
	for (int i = 0; i < 360; i += 30) {
	    glPushMatrix(); {
		glRotatef(-i, 0.0, 0.0, 1.0);
		glTranslatef(0.0, tickEnd, 0.0);

		AtlasString label;
		label.printf("%d", i);

		LayoutManager lm(label.str(), _overlays.regularFont(), 
				 pointSize);
		lm.setAnchor(LayoutManager::LC);
		lm.drawText();
	    }
	    glPopMatrix();
	}
	glEndList();
    }
}

// Convenience routines to set up and tear down a new set projection
// and model-view matrices.  They must be called in matching pairs.
void RangeRingsOverlay::_pushView()
{
    // Remember the previous matrix mode.
    glPushAttrib(GL_TRANSFORM_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
}

void RangeRingsOverlay::_popView()
{
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // Restore the matrix mode that existed when _pushView() was called.
    glPopAttrib();
}
