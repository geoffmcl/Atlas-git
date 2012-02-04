/*-------------------------------------------------------------------------
  AirwaysOverlay.cxx

  Written by Brian Schack

  Copyright (C) 2008 - 2012 Brian Schack

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
#include "AirwaysOverlay.hxx"

// C++ system files
#include <cassert>
#include <sstream>

// Other project's include files
#include <simgear/misc/sg_path.hxx>
#include <simgear/math/sg_geodesy.hxx>

// Our project's include files
#include "AtlasWindow.hxx"
#include "Globals.hxx"
#include "LayoutManager.hxx"
#include "NavData.hxx"

using namespace std;

// EYE - magic numbers and policies
// const float maxHighAirway = 10000.0;
// const float minHighAirway = 250.0;
// const float maxLowAirway = 500.0;
// const float minLowAirway = 50.0;
const float maxHighAirway = 1250.0;
const float minHighAirway = 50.0;
const float maxLowAirway = 1250.0;
const float minLowAirway = 50.0;

// Colouring airways is a bit difficult.  At first I wanted to try to
// colour each one a different colour.  However, that's nearly
// impossible, as airways often overlap.  As well, it seems to be more
// important to use colour to denote function, rather than identity,
// so I scrapped the idea of individual colouring.

// faa-h-8083-15-2.pdf, page 8-5, has a sample airways chart

// From VFR_Chart_Symbols.pdf
//
// Low altitude:
//   VOR Airway {0.576, 0.788, 0.839}
//   LF / MF Airway {0.831, 0.627, 0.753}

const float awy_low_colour[4] = {0.576, 0.788, 0.839, 0.7};
// const float awy_low_colour[4] = {0.831, 0.627, 0.753, 0.7};

// From IFR chart symbols.pdf
//
// Low / High altitude:
//   Many different kinds!
//     VOR Airway / Jet Route {0.078, 0.078, 0.078}
//     LF / MF AIrway {0.878, 0.812, 0.753}, (text {0.624, 0.439, 0.122})
// High Altitude
//   RNAV Route {0.176, 0.435, 0.667}
//   Joint Jet / RNAV Route {0.078, 0.078, 0.078} and {0.176, 0.435, 0.667}
const float awy_high_colour[4] = {0.176, 0.435, 0.667, 0.7};
// const float awy_high_colour[4] = {0.878, 0.812, 0.753, 0.7};
// const float awy_high_colour[4] = {0.624, 0.439, 0.122, 0.7};

// From Canadian high and low altitude charts:
//
// VOR route {0.5, 0.5, 0.5}
// NDB route {0.2, 0.8, 0.2}

// Different airways have different names, depending on their role and
// country.  There's some information in
//
//   http://en.wikipedia.org/wiki/Airway_%28aviation%29
//
// as well as
//
//   faa-h-8083-15-2.pdf, page 8-4
//
// To summarize ({l} = letter, {d} = digit):
//
// V{d}+ (US) - victor airway, low altitude, 1200' to 18,000', 8nm
//   wide, or 9 degrees, whichever is larger
// J{d}+ (US) - jet route, high altitude, above FL180
// {l}{d}+ (Eur) - air route (low altitude), 10nm wide, up to FL195
// U{l}{d}+ (Eur) - upper air route (high altitude), above FL195

// Airway label colours
sgVec4 nameColour = {1.0, 0.0, 0.0, 1.0};
sgVec4 elevationColour = {0.0, 1.0, 0.0, 1.0};
sgVec4 distanceColour = {0.0, 0.0, 1.0, 1.0};

//////////////////////////////////////////////////////////////////////
// AirwaysOverlay
//////////////////////////////////////////////////////////////////////

AirwaysOverlay::AirwaysOverlay(Overlays& overlays):
    _overlays(overlays), _highDL(0), _lowDL(0)
{
    // Subscribe to zoomed notifications.
    subscribe(Notification::Zoomed);
}

AirwaysOverlay::~AirwaysOverlay()
{
    glDeleteLists(_highDL, 1);
    glDeleteLists(_lowDL, 1);
}

// EYE - we need to be very careful about OpenGL state changes.  Here,
// it turns out that changing line width is extremely expensive.
// Therefore, we need to do all the low-altitude airways first
// (they'll all have the same line width), and all the high-alitude
// airways last.
void AirwaysOverlay::draw(bool drawHigh, bool drawLow, bool label, 
			  NavData *navData)
{
    // I used to add individual airway segments to the culler and draw
    // them based on whether they were visible.  This turned out to be
    // too much work.  It's much easier just to create one display
    // list for low airways and one display list for high airways, and
    // turn them on and off as required.  If, for some reasons, we
    // wanted to render airways differently depending on our zoom, for
    // example, then we couldn't do this.
    const vector<AWY *>& segments = navData->segments();
    if (drawLow) {
	if (_lowDL == 0) {
	    _lowDL = glGenLists(1);
	    assert(_lowDL != 0);
	    glNewList(_lowDL, GL_COMPILE); {
		glColor4fv(awy_low_colour);
		glPushAttrib(GL_LINE_BIT); {
		    glLineWidth(2.0);
		    for (unsigned int i = 0; i < segments.size(); i++) {
			AWY *a = segments[i];
			if (a->isLow) {
			    _render(a);
			}
		    }
		}
		glPopAttrib();
	    }
	    glEndList();
	}

	glCallList(_lowDL);
    }

    if (drawHigh) {
	if (_highDL == 0) {
	    _highDL = glGenLists(1);
	    assert(_highDL != 0);
	    glNewList(_highDL, GL_COMPILE); {
		glColor4fv(awy_high_colour);
		for (unsigned int i = 0; i < segments.size(); i++) {
		    AWY *a = segments[i];
		    if (!a->isLow) {
			_render(a);
		    }
		}
	    }
	    glEndList();
	}

	glCallList(_highDL);
    }

    // Now label them.
    // EYE - we should create a display list, combine this with the
    // previous bit, blah blah blah
    if (label) {
	const vector<Cullable *>& intersections = 
	    navData->hits(NavData::AIRWAYS);
	for (unsigned int i = 0; i < intersections.size(); i++) {
	    AWY *a = dynamic_cast<AWY *>(intersections[i]);
	    assert(a);
	    if (a->isLow && drawLow) {
		_label(a);
	    } 
	    if (!a->isLow && drawHigh) {
		_label(a);
	    }
	}
    }
}

// Renders the given airway.

// EYE - to do: 
//
// (1) airway labels (and don't label each individual segment)
// (2) don't draw labels multiple times
// (3) draw airways/labels in different styles depending on type (eg,
//     LF, high vs low, ...)
// (4) add directions at edge of VOR roses
void AirwaysOverlay::_render(const AWY *a) const
{
//     bool isVOR = false, isNDB = false;
//     if (a->start.isNavaid) {
// 	NAV *n = (NAV *)a->start.n;
// 	if (n->navtype == NAV_VOR) {
// 	    isVOR = true;
// 	} else if (n->navtype == NAV_NDB) {
// 	    isNDB = true;
// 	}
//     }
//     if (a->end.isNavaid) {
// 	NAV *n = (NAV *)a->end.n;
// 	if (n->navtype == NAV_VOR) {
// 	    isVOR = true;
// 	} else if (n->navtype == NAV_NDB) {
// 	    isNDB = true;
// 	}
//     }
//     if (isVOR) {
// // 	glColor4f(0.176, 0.435, 0.667, 0.7); // American high RNAV
// // 	glColor4f(0.5, 0.5, 0.5, 0.7); // Canadian
// 	glColor4f(0.000, 0.420, 0.624, 0.7); // VOR
//     } else if (isNDB) {
// // 	glColor4f(0.624, 0.439, 0.122, 0.7); // American low LF / MF
// // 	glColor4f(0.2, 0.8, 0.2, 0.7); // Canadian
// 	glColor4f(0.525, 0.294, 0.498, 0.7); // NDB
//     } else if (a->isLow) {
// // 	glColor4fv(awy_low_colour);
// 	glColor4f(0.5, 0.5, 0.5, 0.7);
//     } else {
// // 	glColor4fv(awy_high_colour);
// 	glColor4f(0.5, 0.5, 0.5, 0.7);
//     }
    glBegin(GL_LINES); {
	sgdVec3 point;
	atlasGeodToCart(a->start.lat, a->start.lon, 0.0, point);
	glVertex3dv(point);
	atlasGeodToCart(a->end.lat, a->end.lon, 0.0, point);
	glVertex3dv(point);
    }
    glEnd();
}

// Labels the given airway.  Does nothing if there isn't enough room
// to label the segment.
bool AirwaysOverlay::_label(const AWY *a) const
{
    // We need at least minDist pixels of space to write any labels.
    const double minDist = 50.0;
    if ((a->length / _metresPerPixel) < minDist) {
	return false;
    }

    // We have space, so calculate the middle of the segment.  The
    // labels are placed in the middle of the segment, oriented along
    // the segment.
    sgdVec3 start, end, middle;
    atlasGeodToCart(a->start.lat, a->start.lon, 0.0, start);
    atlasGeodToCart(a->end.lat, a->end.lon, 0.0, end);
    sgdAddVec3(middle, start, end);
    sgdScaleVec3(middle, 0.5);

    // EYE - magic numbers
    const float maxPointSize = 12.0;
    const float minPointSize = 1.0;
    float pointSize;
    // EYE - magic number
    const float space = 4.0 * _metresPerPixel; // 4 pixel space between boxes

    // Strategy - point size ranges from a minimum of minPointSize to
    // a maximum of maxPointSize.  We set it based on scale.
    if (a->isLow) {
	// Start labelling at maxLowAirway, stop labelling at
	// minLowAirway.  Maximum point size is reached at 2 *
	// minLowAirway.  Cheesy, yes.
	pointSize = (maxLowAirway - _metresPerPixel) / 
	    (maxLowAirway - 2.0 * minLowAirway) *
	    (maxPointSize - minPointSize) + minPointSize;
    } else {
	pointSize = (maxHighAirway - _metresPerPixel) / 
	    (maxHighAirway - 2.0 * minHighAirway) *
	    (maxPointSize - minPointSize) + minPointSize;
    }
    if (pointSize < minPointSize) {
	return false;
    } 
    if (pointSize > maxPointSize) {
	pointSize = maxPointSize;
    }
    pointSize *= _metresPerPixel;

    // Airway name label
    LayoutManager lmName(a->name, _overlays.regularFont(), pointSize);
    lmName.setBoxed(true);
    if (lmName.width() + (space * 2.0) > a->length) {
	return false;
    }

    // Airway top and base label

    // EYE - start adding these to high routes at around 1000 m/pixel.
    // The lows can be added immediately (500 or less).  Or maybe just
    // add them to the sides of the name, and only do so when there's
    // room?
    LayoutManager lmElev;
    lmElev.setBoxed(true);
    lmElev.setFont(_overlays.regularFont(), pointSize * 0.75);

    lmElev.begin(); {
	globals.str.printf("%d", a->isLow ? a->top * 100 : a->top);
	lmElev.addText(globals.str.str());
	lmElev.newline();
	globals.str.printf("%d", a->isLow ? a->base * 100 : a->base);
	lmElev.addText(globals.str.str());
    }
    lmElev.end();

    // Airway length label
    globals.str.printf("%.0f", a->length * SG_METER_TO_NM);
    // EYE - magic number
    LayoutManager lmDist(globals.str.str(), _overlays.regularFont(), 
			 pointSize * 0.75);
    lmDist.setBoxed(true);

    // EYE - draw NDB segments different than VOR segments?  This is
    // done on Canadian air charts.  We'd have to check what navaids
    // lie on the ends, and deal with mixed cases (a VOR on one end
    // and an NDB on the other).  Another issue is labelling segments
    // versus "legs" (my term - the set of segments between navaids,
    // as opposed to a single segment).  And what about airways with
    // no navaids? (I presume they exist).  Should we draw little
    // markers at segment joints?

    // EYE - doing the ends is tough, but useful.  We want to label
    // the heading for an airway segment leaving a navaid.  For a VOR,
    // that means using its bias, whereas for an NDB we want to use a
    // magnetic heading.  We need space for the label (taking into
    // account the airway label calculated above).  Also, it would be
    // nice if it took into account the navaid's rendered size (eg, it
    // should be positioned just outside of a VOR rose).

//     // Ends.
//     if (a->start->isNavaid) {
// 	NAV *n = (NAV *)a->start->n;
// 	if ((n->navtype == NAV_VOR) || (n->navtype == NAV_NDB)) {
// 	    lmStart.begin();
// 	    lmStart.setFont(f, pointSize * 0.75);
// 	    buf.str("");
// 	    if (n->navType == NAV_VOR) {
// 		buf << "VOR";
// 	    } else {
// 		buf << "NDB";
// 	    }
// 	    lmStart.addText(buf.str());
// 	    lmStart.end();

// 	    lmStart.size(&startWidth, startEndHeight);
// 	    startWidth += border;
// 	    startHeight += border;

// 	    // EYE - we need to know if the start is on the left or
// 	    // the right, and take into account either the length
// 	    // label or the base label.  Yuck!  Also, we should clean
// 	    // up the calculation of this stuff.  Can we use a layout
// 	    // manager for this too?
// 	    if ((startWidth + nameWidth / 2.0 + (border * 2.0)) > 
// 		(sgdDistanceVec3(start.sg(), end.sg()) / 2.0)) {
// 		startWidth = 0.0;
// 	    }
// 	}
//     }

    // The labels will be drawn in the centre of the airway, and we
    // need to make sure the text isn't upside-down.
    double lat, lon, elev, hdg, junk;
    atlasCartToGeod(middle, &lat, &lon, &elev);

    // EYE - is this overkill?  Can we use a simpler calculation
    // to get the heading?  Just use geodDrawText()?
    geo_inverse_wgs_84(lat, lon, a->end.lat, a->end.lon, 
		       &hdg, &junk, &junk);
    if (hdg > 180.0) {
	hdg -= 180.0;
    }
    hdg -= 90.0;

    // Now that we've calculated everything, draw the labels.
    geodPushMatrix(middle, lat, lon, hdg); {
	// Draw the name in the centre of the segment.
	glColor4fv(nameColour);
	lmName.drawText();

	// Draw the base and top elevations (if we have room) on the
	// left.
	if ((lmElev.width() + lmName.width() / 2.0 + (space * 2.0)) <
	    (a->length / 2.0)) {
	    float xOffset = lmElev.width() / 2.0 + space + lmName.width() / 2.0;
	    glColor4fv(elevationColour);
	    lmElev.moveTo(-xOffset, 0.0);
	    lmElev.drawText();
	}

	// Draw the length (if we have room) on the right.
	if ((lmDist.width() + lmName.width() / 2.0 + (space * 2.0)) <
	    (a->length / 2.0)) {
	    float xOffset = lmDist.width() / 2.0 + space + lmName.width() / 2.0;
	    glColor4fv(distanceColour);
	    lmDist.moveTo(xOffset, 0.0);
	    lmDist.drawText();
	}
    }
    geodPopMatrix();

    return true;
}

// Called when somebody posts a notification that we've subscribed to.
void AirwaysOverlay::notification(Notification::type n)
{
    if (n == Notification::Zoomed) {
	_metresPerPixel = _overlays.aw()->scale();
    } else {
	assert(false);
    }
}
