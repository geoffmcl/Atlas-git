/*-------------------------------------------------------------------------
  FixesOverlay.cxx

  Written by Brian Schack

  Copyright (C) 2009 - 2014 Brian Schack

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
#include "FixesOverlay.hxx"

// Our project's include files
#include "AtlasWindow.hxx"
#include "LayoutManager.hxx"
#include "NavData.hxx"

using namespace std;

// Above this level, no fixes are drawn.
const float noLevel = 1250.0;
// Above this level, but below noLevel, high-level fixes are drawn.
const float highLevel = 250.0;
// Above this level, but below highLevel, low-level fixes are drawn.
const float lowLevel = 100.0;
// Below lowLevel, only approach fixes are drawn.

// Bright yellow.
// const float fix_colour[4] = {1.0, 1.0, 0.0, 0.7};
const float high_fix_colour[4] = {1.0, 1.0, 0.0, 0.7};
const float low_fix_colour[4] = {0.0, 1.0, 1.0, 0.7};
const float terminal_fix_colour[4] = {1.0, 0.0, 1.0, 0.7};

const float fix_label_colour[4] = {0.2, 0.2, 0.2, 0.7};

// Note: there are many types of fixes and waypoints, but our database
// doesn't differentiate.  So we just draw them as points (rather than
// triangles, or circles, or any of the other common renderings).
// This actually looks cleaner, is easier to implement, and also
// doesn't mislead the user into believing that they are something
// which they aren't.  Because we just draw them as points, we don't
// create a special display list for them (unlike VORs, for example).
FixesOverlay::FixesOverlay(Overlays& overlays):
    _overlays(overlays), _DL(0), _isDirty(false)
{
    // Subscribe to moved and zoomed notifications.
    subscribe(Notification::Moved);
    subscribe(Notification::Zoomed);
}

FixesOverlay::~FixesOverlay()
{
    glDeleteLists(_DL, 1);
}

void FixesOverlay::setDirty()
{
    _isDirty = true;
}

void FixesOverlay::draw(NavData *navData)
{
    // Size of point used to represent the fix.
    const float fixSize = 4.0;

    if (_metresPerPixel > noLevel) {
	return;
    }

    if (_isDirty) {
	// Something's changed, so we need to regenerate the display
	// list.
	if (_DL == 0) {
	    // Get a display list.  We could put this in the
	    // constructor, but it's a bit safer here, because we can
	    // be pretty sure a display context has been created when
	    // draw() is called.
	    _DL = glGenLists(1);
	    assert(_DL != 0);
	}

	glNewList(_DL, GL_COMPILE); {
	    const vector<Cullable *>& intersections = 
		navData->hits(NavData::FIXES);
	    // Fixes (points)
	    glPushAttrib(GL_POINT_BIT); {
		// We use a non-standard point size, so we need to
		// wrap this in a glPushAttrib().
		glPointSize(fixSize);
		for (unsigned int i = 0; i < intersections.size(); i++) {
		    FIX *f = dynamic_cast<FIX *>(intersections[i]);
		    assert(f);

		    if (f->high && _metresPerPixel < noLevel) {
			glColor4fv(high_fix_colour);
			_render(f);
		    } else if (f->low && (_metresPerPixel < highLevel)) {
			glColor4fv(low_fix_colour);
			_render(f);
		    } else if (!f->high && !f->low && 
			       (_metresPerPixel < lowLevel)) {
			glColor4fv(terminal_fix_colour);
			_render(f);
		    }
		}
	    }
	    glPopAttrib();

	    // Fix labels
	    LayoutManager lm;
	    float pointSize = _metresPerPixel * 10.0;
	    lm.setFont(_overlays.regularFont(), pointSize);

	    if (_overlays.isVisible(Overlays::LABELS)) {
		for (unsigned int i = 0; i < intersections.size(); i++) {
		    FIX *f = dynamic_cast<FIX *>(intersections[i]);
		    assert(f);

		    if (f->high && _metresPerPixel < noLevel) {
			_label(f, lm);
		    } else if (f->low && (_metresPerPixel < highLevel)) {
			_label(f, lm);
		    } else if (!f->high && !f->low && 
			       (_metresPerPixel < lowLevel)) {
			_label(f, lm);
		    }
		}
	    }
	}
	glEndList();
	
	_isDirty = false;
    }

    glCallList(_DL);
}

// Renders the given fix.
void FixesOverlay::_render(const FIX *f)
{
    geodPushMatrix(f->_bounds.center, f->lat, f->lon); {
	glBegin(GL_POINTS); {
	    glVertex2f(0.0, 0.0);
	}
	glEnd();
    }
    geodPopMatrix();
}

// Labels the given fix using the given layout manager (which is
// assumed to have been set up with the desired font and point size).
void FixesOverlay::_label(const FIX *f, LayoutManager& lm)
{
    // EYE - magic number
    const float labelOffset = _metresPerPixel * 5.0;

    // Draw a label labelOffset pixels to the left of the fix.
    glColor4fv(fix_label_colour);
    lm.setText(f->name);
    lm.moveTo(-labelOffset, 0.0, LayoutManager::CR);

    geodDrawText(lm, f->_bounds.center, f->lat, f->lon);
}

// Called when somebody posts a notification that we've subscribed to.
void FixesOverlay::notification(Notification::type n)
{
    if (n == Notification::Moved) {
	setDirty();
    } else if (n == Notification::Zoomed) {
	_metresPerPixel = _overlays.aw()->scale();
	setDirty();
    } else {
	assert(false);
    }
}
