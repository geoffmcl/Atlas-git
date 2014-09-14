/*-------------------------------------------------------------------------
  NavaidsOverlay.cxx

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
#include "NavaidsOverlay.hxx"

// Our project's include files
#include "AtlasController.hxx"
#include "AtlasWindow.hxx"
#include "FlightTrack.hxx"
#include "Globals.hxx"
#include "NavData.hxx"

using namespace std;

float clearColour[4] = {1.0, 1.0, 1.0, 0.0};

// EYE - change to sgVec4?
// VOR (teal)
const float vor_colour[4] = {0.000, 0.420, 0.624, 1.0};
// NDB (purple)
const float ndb_colour[4] = {0.525, 0.294, 0.498, 1.0};

// TACAN (grey? orange? brown? black?)

// EYE - what colour *should* we use really?  This is from lo2.pdf,
// but (a) this colour is used for many navaids, and (b) it's an IFR
// chart, and (c) it's Canadian
//
const float dme_colour[4] = {0.498, 0.498, 0.498, 1.0};
//
// EYE - theoretically, we should use the same colour for all DME
// components of navaids - VOR-DME, NDB-DME, VORTAC, TACAN - but it's
// hard to make this look nice unfortunately.
//
// This colour looks okay - not too bright, but enough to show up.
// Still, it's not entirely satisfactory.
// const float dme_colour[4] = {0.75, 0.5, 0.25, 1.0};
//
// This is the same as VORs.
//
// const float dme_colour[4] = {0.000, 0.420, 0.624, 1.0};

// Markers
const float marker_colours[3][4] = 
    {{0.0, 0.0, 1.0, 0.5},	// Outer marker (blue)
     {1.0, 0.5, 0.0, 0.5},	// Middle marker (amber)
     {1.0, 1.0, 1.0, 0.5}};	// Inner marker (white)
// ILS localizer (from Canada Air Pilot, CYYZ.pdf)
// - clear on left, solid pink on right, black outline, heavy black
//   line down centre
const float ils_colour[4] = {1.000, 0.659, 0.855, 0.7};
// EYE - This is my own invention - a localizer (no glideslope) is
// drawn in grey.
const float loc_colour[4] = {0.5, 0.5, 0.5, 0.7};

// EYE - not a standard
const float ils_label_colour[4] = {0.0, 0.0, 0.0, 0.75};

// EYE - magic numbers
// Radii, in metres, for outer, middle, and inner markers.
const float markerRadii[3] = 
    {1.0 * SG_NM_TO_METER,
     0.35 * SG_NM_TO_METER,
     0.25 * SG_NM_TO_METER};
// This denotes the radius of a marker's bounding sphere, in nautical
// miles.  It must be an integer, and no smaller than the maximum
// radius in markerRadii.  We specify this because the navaid database
// doesn't give a range for markers.
const int markerRange = 1;

NavaidsOverlay::NavaidsOverlay(Overlays& overlays):
    _overlays(overlays),
    _VORDirty(false), _NDBDirty(false), _ILSDirty(false), _DMEDirty(false),
    _p(NULL)
{
    // Create all the display list indices.  Note that we must have a
    // valid OpenGL context for this to work.
    _VORRoseDL = glGenLists(1);
    _VORSymbolDL = glGenLists(1);
    _VORTACSymbolDL = glGenLists(1);
    _VORDMESymbolDL = glGenLists(1);

    _NDBSymbolDL = glGenLists(1);
    _NDBDMESymbolDL = glGenLists(1);

    _ILSSymbolDL = glGenLists(1);
    _LOCSymbolDL = glGenLists(1);
    _ILSMarkerDLs[0] = glGenLists(1);
    _ILSMarkerDLs[1] = glGenLists(1);
    _ILSMarkerDLs[2] = glGenLists(1);

    _TACANSymbolDL = glGenLists(1);
    _DMESymbolDL = glGenLists(1);

    _VORDisplayList = glGenLists(1);
    _NDBDisplayList = glGenLists(1);
    _ILSDisplayList = glGenLists(1);
    _DMEDisplayList = glGenLists(1);

    _createVORRose();
    _createVORSymbols();
    _createNDBSymbols();
    _createDMESymbols();
    _createILSSymbols();
    _createMarkerSymbols();

    // Subscribe to moved, zoomed, flight track and magnetic/true
    // display notifications.
    subscribe(Notification::Moved);
    subscribe(Notification::Zoomed);
    subscribe(Notification::AircraftMoved);
    subscribe(Notification::NewFlightTrack);
    subscribe(Notification::MagTrue);

    _radios[0] = _radios[1] = _radios[2] = 0;
    _radials[0] = _radials[1] = 0;
}

NavaidsOverlay::~NavaidsOverlay()
{
    glDeleteLists(_VORDisplayList, 1);
    glDeleteLists(_NDBDisplayList, 1);
    glDeleteLists(_ILSDisplayList, 1);
    glDeleteLists(_DMEDisplayList, 1);

    glDeleteLists(_VORRoseDL, 1);
    glDeleteLists(_VORSymbolDL, 1);
    glDeleteLists(_VORTACSymbolDL, 1);
    glDeleteLists(_VORDMESymbolDL, 1);
    glDeleteLists(_NDBSymbolDL, 1);
    glDeleteLists(_NDBDMESymbolDL, 1);
    glDeleteLists(_ILSSymbolDL, 1);
    glDeleteLists(_LOCSymbolDL, 1);
    glDeleteLists(_TACANSymbolDL, 1);
    glDeleteLists(_DMESymbolDL, 1);
    glDeleteLists(_ILSMarkerDLs[0], 1);
    glDeleteLists(_ILSMarkerDLs[1], 1);
    glDeleteLists(_ILSMarkerDLs[2], 1);
}

// Creates a standard VOR rose of radius 1.0.  This is a circle with
// ticks and arrows, a line from the centre indicating north, and
// labels at 30-degree intervals around the outside.
//
// The rose is drawn in the current colour, with the current line
// width.
void NavaidsOverlay::_createVORRose()
{
    // Draw a standard VOR rose or radius 1.  It is drawn in the XY
    // plane, with north in the positive Y direction, and east in the
    // positive X direction.
    assert(_VORRoseDL != 0);
    glNewList(_VORRoseDL, GL_COMPILE); {
	glBegin(GL_LINE_LOOP); {
	    const int subdivision = 5;	// 5-degree steps

	    // Now continue around the circle.
	    for (int i = 0; i < 360; i += subdivision) {
		float theta, x, y;

		// Draw circle segment.
		theta = i * SG_DEGREES_TO_RADIANS;
		x = sin(theta);
		y = cos(theta);
		glVertex2f(x, y);
	    }
	}
	glEnd();

	// Now draw the ticks.
	// EYE - magic numbers
	const float bigTickLength = 0.1;
	const float mediumTickLength = bigTickLength * 0.8;
	const float smallTickLength = bigTickLength * 0.5;
	for (int i = 0; i < 360; i += 30) {
	    for (int j = 0; j < 30; j += 5) {
		glPushMatrix(); {
		    glRotatef(-(i + j), 0.0, 0.0, 1.0);
		    glTranslatef(0.0, 1.0, 0.0);
		    if (j == 0) {
			// Major tick.
			glBegin(GL_LINES); {
			    glVertex2f(0.0, 0.0);
			    glVertex2f(0.0, -bigTickLength);
			}
			glEnd();
			// Major ticks have an arrowhead.
			glBegin(GL_TRIANGLES); {
			    glVertex2f(0.0, 0.0);
			    glVertex2f(-bigTickLength * 0.2, -bigTickLength * 0.7);
			    glVertex2f(bigTickLength * 0.2, -bigTickLength * 0.7);
			}
			glEnd();
		    } else if (j % 2 == 0) {
			// Medium tick.
			glBegin(GL_LINES); {
			    glVertex2f(0.0, 0.0);
			    glVertex2f(0.0, -mediumTickLength);
			}
			glEnd();
		    } else {
			// Minor tick.
			glBegin(GL_LINES); {
			    glVertex2f(0.0, 0.0);
			    glVertex2f(0.0, -smallTickLength);
			}
			glEnd();
		    }
		}
		glPopMatrix();
	    }
	}

	// Draw a line due north.
	glBegin(GL_LINES); {
	    glVertex2f(0.0, 0.0);
	    glVertex2f(0.0, 1.0);
	}
	glEnd();

	// Label the rose.  Make the text about 1/10 the size of the
	// radius.
	const float pointSize = 0.1;
	for (int i = 0; i < 360; i += 30) {
	    glPushMatrix(); {
		glRotatef(-i, 0.0, 0.0, 1.0);
		glTranslatef(0.0, 1.0, 0.0);

		AtlasString label;
		label.printf("%d", i / 10);

		LayoutManager lm(label.str(), _overlays.regularFont(), pointSize);
		lm.setAnchor(LayoutManager::LC);
		lm.drawText();
	    }
	    glPopMatrix();
	}
    }
    glEndList();
}

// Creates display lists for the 3 VOR symbols: VOR (a hexagon with a
// dot in the middle), VORTAC (a VOR with 3 filled "lobes"), and
// VOR-DME (a VOR surrounded by a rectangle).  The VOR hexagon has a
// radius of 1.0.
//
// The icons are drawn using lines, points, and quads.  The styles of
// these objects (eg, line width, point size), are not set here, the
// reasoning being that the caller should be able to vary them if
// necessary.
void NavaidsOverlay::_createVORSymbols()
{
    // Radius of VOR symbol.
    const float size = 1.0;

    ////////////////////
    // VOR
    ////////////////////
    assert(_VORSymbolDL != 0);
    glNewList(_VORSymbolDL, GL_COMPILE); {
	glColor4fv(vor_colour);
	glBegin(GL_LINE_LOOP); {
	    for (int i = 0; i < 360; i += 60) {
		float theta, x, y;

		// Draw hexagon segment.
		theta = (i + 30) * SG_DEGREES_TO_RADIANS;
		x = sin(theta) * size;
		y = cos(theta) * size;
		glVertex2f(x, y);
	    }
	}
	glEnd();

	glBegin(GL_POINTS); {
	    glVertex2f(0.0, 0.0);
	}
	glEnd();
    }
    glEndList();

    ////////////////////
    // VORTAC
    ////////////////////
    const float lobeThickness = size * 0.5;

    assert(_VORTACSymbolDL != 0);
    glNewList(_VORTACSymbolDL, GL_COMPILE); {
	glCallList(_VORSymbolDL);
    
	glColor4fv(dme_colour);
	for (int i = 0; i < 360; i += 120) {
	    glPushMatrix(); {
		glRotatef(-(i + 60.0), 0.0, 0.0, 1.0);
		glTranslatef(0.0, size * sqrt(3.0) / 2.0, 0.0);
		glBegin(GL_QUADS); {
		    glVertex2f(-size / 2.0, 0.0);
		    glVertex2f(size / 2.0, 0.0);
		    glVertex2f(size / 2.0, lobeThickness);
		    glVertex2f(-size / 2.0, lobeThickness);
		}
		glEnd();
	    }
	    glPopMatrix();
	}
    }
    glEndList();

    ////////////////////
    // VOR-DME
    ////////////////////
    // Half the length of the long (top and bottom) side.
    const float longSide = size;
    // Half the length of the short (left and right) side.
    const float shortSide = sqrt(3.0) / 2.0 * size;

    assert(_VORDMESymbolDL != 0);
    glNewList(_VORDMESymbolDL, GL_COMPILE); {
	// EYE - if I make the DME a different colour, I render the VOR
	// symbol on top.
	//     glCallList(_VORSymbolDL);

	glColor4fv(dme_colour);
	glBegin(GL_LINE_LOOP); {
	    glVertex2f(-longSide, -shortSide);
	    glVertex2f(-longSide, shortSide);
	    glVertex2f(longSide, shortSide);
	    glVertex2f(longSide, -shortSide);
	}
	glEnd();

	glCallList(_VORSymbolDL);
    }
    glEndList();
}

// Create an NDB symbol and and NDB-DME symbol, in the WAC style.
void NavaidsOverlay::_createNDBSymbols()
{
    // According to VFR_Chart_Symbols.pdf, there are 10 concentric
    // circles of dots, with 16, 21, 26, 31, 36, 41, 46, 51, 56, and 61
    // dots (yes, I counted).
    //
    // If we define the radius of the entire symbol to be 10.0, then
    // here are the distances from the centre to the circles of dots:
    //
    // dots: 2.57, 3.44, 4.24, 5.03, 5.88, 6.69, 7.53, 8.38, 9.19, 10.0
    //
    // That works out to about 0.825 between each circle.  If we assume
    // that there are 12 steps (a blank, the circle, then the 10 circles
    // of dots), that works out to 0.833, which is pretty close to the
    // measured value.
    //
    // The distance to the centre of the circle near the centre:
    //
    // circle: 1.79
    //
    // (ie, about 2 steps of 0.825).
    //
    // Each dot has a radius of 0.326, and the circle has a width of 0.696.

    // For the WAC charts, there are 5 concentric circles, with 11, 16,
    // 21, 27, and 32 dots.
    //
    // dots: 3.33, 4.94, 6.63, 8.27, 10.00 (1.667 each, equivalent to 6
    //       radii), radius 0.39
    //
    // circle: 2.55, width = 1.10

    // Radius of NDB symbol.
    const float size = 1.0;

    // EYE - I'd like to set the point size here, but this doesn't
    // seem to work with scaling.  If I set a small point size (<
    // 1.0), then it seems to be converted to 1.0.  Later when I draw
    // it scaled, that point size (1.0) is scaled, not the original.

    ////////////////////
    // NDB
    ////////////////////
    assert(_NDBSymbolDL != 0);
    glNewList(_NDBSymbolDL, GL_COMPILE); {
	glColor4fv(ndb_colour);
	glBegin(GL_POINTS); {
	    // Centre dot.
	    glVertex2f(0.0, 0.0);

	    // Draw 5 concentric circles of dots.
	    for (int r = 2; r <= 6; r++) {
		float radius = r / 6.0 * size;

		// The circles have 6, 11, 16, 21, 26, and 31 dots.
		int steps = r * 5 + 1;
		float stepTheta = 360.0 / steps;
		for (int j = 0; j < steps; j++) {
		    float theta, x, y;

		    theta = j * stepTheta * SG_DEGREES_TO_RADIANS;
		    x = sin(theta) * radius;
		    y = cos(theta) * radius;
		    glVertex2f(x, y);
		}
	    }
	}
	glEnd();

	// Inner circle.
	glPushAttrib(GL_LINE_BIT); {
	    glLineWidth(2.0);
	    glBegin(GL_LINE_LOOP); {
		const int subdivision = 20;	// 20-degree steps

		// Now continue around the circle.
		for (int i = 0; i < 360; i += subdivision) {
		    float theta, x, y;

		    // Draw circle segment.
		    theta = i * SG_DEGREES_TO_RADIANS;
		    x = sin(theta) * 0.255;
		    y = cos(theta) * 0.255;
		    glVertex2f(x, y);
		}
	    }
	    glEnd();
	}
	glPopAttrib();
    }
    glEndList();

    ////////////////////
    // NDB-DME
    ////////////////////
    assert(_NDBDMESymbolDL != 0);
    glNewList(_NDBDMESymbolDL, GL_COMPILE); {
	glCallList(_NDBSymbolDL);

	// DME square
	glColor4fv(dme_colour);
	glBegin(GL_LINE_LOOP); {
	    glVertex2f(-0.5, -0.5);
	    glVertex2f(0.5, -0.5);
	    glVertex2f(0.5, 0.5);
	    glVertex2f(-0.5, 0.5);
	}
	glEnd();
    }
    glEndList();
}

// Creates DME symbols - TACANs and stand-alone DMEs (this includes
// DME and DME-ILS).  The others - VOR-DME, NDB-DME - are handled
// elsewhere.
void NavaidsOverlay::_createDMESymbols()
{
    // EYE - use the size constant defined in _createVORSymbols?
    const float size = 1.0;
    const float lobeThickness = size * 0.5;

    ////////////////////
    // TACAN
    ////////////////////
    assert(_TACANSymbolDL != 0);
    glNewList(_TACANSymbolDL, GL_COMPILE); {
	glColor4fv(dme_colour);
	glBegin(GL_LINE_LOOP); {
	    for (int i = 0; i < 360; i += 120) {
		float theta, x, y;

		theta = (i - 30) * SG_DEGREES_TO_RADIANS;	
		x = sin(theta) * size;
		y = cos(theta) * size;
		glVertex2f(x, y);

		theta = (i + 30) * SG_DEGREES_TO_RADIANS;	
		x = sin(theta) * size;
		y = cos(theta) * size;
		glVertex2f(x, y);

		theta = (i + 60) * SG_DEGREES_TO_RADIANS;
		x += sin(theta) * lobeThickness;
		y += cos(theta) * lobeThickness;
		glVertex2f(x, y);
	    
		theta = (i + 150) * SG_DEGREES_TO_RADIANS;
		x += sin(theta) * size;
		y += cos(theta) * size;
		glVertex2f(x, y);
	    }
	}

	glEnd();

	glBegin(GL_POINTS); {
	    glVertex2f(0.0, 0.0);
	}
	glEnd();
    }
    glEndList();    

    // EYE - define this elsewhere, so it can be used in VORs and
    // NDBs?

    ////////////////////
    // DME, DME-ILS
    ////////////////////
    assert(_DMESymbolDL != 0);
    glNewList(_DMESymbolDL, GL_COMPILE); {
	glCallList(_DMESymbolDL);

	// DME square
	glColor4fv(dme_colour);
	glBegin(GL_LINE_LOOP); {
	    glVertex2f(-0.5, -0.5);
	    glVertex2f(0.5, -0.5);
	    glVertex2f(0.5, 0.5);
	    glVertex2f(-0.5, 0.5);
	}
	glEnd();
    }
    glEndList();
}

// Creates ILS localizer symbol, with a length of 1.  The symbol is
// drawn in the x-y plane, with the pointy end at 0,0, and the other
// end at 0, -1.

// EYE - add ILS symbol (a dot with a circle) at the tip?
void NavaidsOverlay::_createILSSymbols()
{
    _createILSSymbol(_ILSSymbolDL, ils_colour);
    _createILSSymbol(_LOCSymbolDL, loc_colour);
}

// Creates a single ILS-type symbol, for the given display list
// variable, in the given colour.
void NavaidsOverlay::_createILSSymbol(GLuint dl, const float *colour)
{
    assert(dl != 0);
    glNewList(dl, GL_COMPILE); {
	glBegin(GL_TRIANGLES); {
	    // The right side is pink.
	    glColor4fv(colour);
	    glVertex2f(0.0, 0.0);
	    glVertex2f(0.0, -1.0);
	    // EYE - this should be calculated based on a certain angular
	    // width, and perhaps the constraint that the notch at the end
	    // be square.
	    glVertex2f(0.01, -1.01);
	}
	glEnd();

	glBegin(GL_TRIANGLES); {
	    // The left side is clear.
	    glColor4f(1.0, 1.0, 1.0, 0.0);
	    glVertex2f(0.0, 0.0);
	    glVertex2f(-0.01, -1.01);
	    glVertex2f(0.0, -1.0);
	}
	glEnd();

	// Draw an outline around it, and a line down the middle.
	glBegin(GL_LINE_STRIP); {
	    glColor4f(0.0, 0.0, 0.0, 0.2);
	    glVertex2f(0.0, 0.0);
	    glVertex2f(-0.01, -1.01);
	    glVertex2f(0.0, -1.0);
	    glVertex2f(0.01, -1.01);
	    glVertex2f(0.0, 0.0);
	    glVertex2f(0.0, -1.0);
	}
	glEnd();
    }
    glEndList();
}

// Creates 3 marker symbols, with units in metres.  The symbols are
// drawn in the x-y plane, oriented with the long axis along the y
// axis, and the centre at 0, 0.
void NavaidsOverlay::_createMarkerSymbols()
{
    // Resolution of our arcs.
    const int segments = 10;

    for (int i = 0; i < 3; i++) {
	assert(_ILSMarkerDLs[i] != 0);
	glNewList(_ILSMarkerDLs[i], GL_COMPILE); {
	    const float offset = cos(30.0 * SG_DEGREES_TO_RADIANS) * markerRadii[i];

	    glColor4fv(marker_colours[i]);
	    // EYE - do here, or in the calling routine?
	    glBegin(GL_POLYGON); {
		// Draw first arc (counterclockwise).
		for (int j = 0; j < segments; j++) {
		    float pHdg = (segments / 2 - j) * (60.0 / segments) 
			* SG_DEGREES_TO_RADIANS;
		    glVertex2f(offset - cos(pHdg) * markerRadii[i], 
			       sin(pHdg) * markerRadii[i]);
		}

		// Now the other arc.
		for (int j = 0; j < segments; j++) {
		    float pHdg = (segments / 2 - j) * (60.0 / segments) 
			* SG_DEGREES_TO_RADIANS;
		    glVertex2f(cos(pHdg) * markerRadii[i] - offset, 
			       -sin(pHdg) * markerRadii[i]);
		}
	    }
	    glEnd();

	    // Draw an outline around the marker.
	    sgVec4 black = {0.0, 0.0, 0.0, 0.5};
	    glColor4fv(black);
	    glBegin(GL_LINE_LOOP); {
		for (int j = 0; j < segments; j++) {
		    float pHdg = (segments / 2 - j) * (60.0 / segments) 
			* SG_DEGREES_TO_RADIANS;
		    glVertex2f(offset - cos(pHdg) * markerRadii[i], 
			       sin(pHdg) * markerRadii[i]);
		}

		// Now the other arc.
		for (int j = 0; j < segments; j++) {
		    float pHdg = (segments / 2 - j) * (60.0 / segments) 
			* SG_DEGREES_TO_RADIANS;
		    glVertex2f(cos(pHdg) * markerRadii[i] - offset,
			       -sin(pHdg) * markerRadii[i]);
		}
	    }
	    glEnd();
	}
	glEndList();
    }
}

// Draws a two-dimentional isocelese triangle with angular width of
// 'width' degrees, and radius 1.0.  The centre of the triangle is at
// <0.0, 0.0>, and it points down in the Y direction.  If 'both' is
// true (the default), a second triangle is drawn pointing up.
//
// The triangle is drawn in two colours, leftColour and rightColour.
// The colours are most intense at the centre, and fade to nothing at
// the ends.  In addition, light grey lines are drawn along the edges
// and down the centre.
//
// This routine is used to create the "radials" emanating from a
// navaid that is tuned-in by the current aircraft.
static void _createTriangle(float width, 
			    const float *leftColour,
			    const float *rightColour,
			    bool both = true)
{
    float deflection = sin(width / 2.0 * SG_DEGREES_TO_RADIANS);

    float fadedLeftColour[4], fadedRightColour[4];
    sgCopyVec4(fadedLeftColour, leftColour);
    fadedLeftColour[3] = 0.0;
    sgCopyVec4(fadedRightColour, rightColour);
    fadedRightColour[3] = 0.0;

    glBegin(GL_TRIANGLES); {
	// Right side
	glColor4fv(rightColour);
	glVertex2f(0.0, 0.0);
	glColor4fv(fadedRightColour);
	glVertex2f(0.0, -1.0);
	glVertex2f(deflection, -1.0);

	if (both) {
	    glColor4fv(rightColour);
	    glVertex2f(0.0, 0.0);
	    glColor4fv(fadedRightColour);
	    glVertex2f(deflection, 1.0);
	    glVertex2f(0.0, 1.0);
	}
    }
    glEnd();

    glBegin(GL_TRIANGLES); {
	// Left side
	glColor4fv(leftColour);
	glVertex2f(0.0, 0.0);
	glColor4fv(fadedLeftColour);
	glVertex2f(-deflection, -1.0);
	glVertex2f(0.0, -1.0);

	if (both) {
	    glColor4fv(leftColour);
	    glVertex2f(0.0, 0.0);
	    glColor4fv(fadedLeftColour);
	    glVertex2f(0.0, 1.0);
	    glVertex2f(-deflection, 1.0);
	}
    }
    glEnd();

    // Draw lines down the left, centre and right.  We don't fade the
    // lines like the triangles above - it looks better.
    glBegin(GL_LINES); {
	glColor4f(0.0, 0.0, 0.0, 0.2);

	glVertex2f(0.0, 0.0);
	glVertex2f(deflection, -1.0);

	glVertex2f(0.0, 0.0);
	glVertex2f(0.0, -1.0);

	glVertex2f(0.0, 0.0);
	glVertex2f(-deflection, -1.0);

	if (both) {
	    glVertex2f(0.0, 0.0);
	    glVertex2f(-deflection, 1.0);

	    glVertex2f(0.0, 0.0);
	    glVertex2f(0.0, 1.0);

	    glVertex2f(0.0, 0.0);
	    glVertex2f(deflection, 1.0);
	}
    }
    glEnd();
}

void NavaidsOverlay::setDirty()
{
    _VORDirty = true;
    _NDBDirty = true;
    _ILSDirty = true;
    _DMEDirty = true;
}

void NavaidsOverlay::drawVORs(NavData *navData)
{
    if (_VORDirty) {
	// Something's changed, so we need to regenerate the VOR
	// display list.
	assert(_VORDisplayList != 0);
	glNewList(_VORDisplayList, GL_COMPILE); {
	    const vector<Cullable *>& intersections = 
		navData->hits(NavData::NAVAIDS);
	    for (unsigned int i = 0; i < intersections.size(); i++) {
		NAV *n = dynamic_cast<NAV *>(intersections[i]);
		if (!n || (n->navtype != NAV_VOR)) {
		    continue;
		}
		assert(n->navtype == NAV_VOR);

		_renderVOR(n);
	    }
	}
	glEndList();
	
	_VORDirty = false;
    }

    glCallList(_VORDisplayList);
}

void NavaidsOverlay::drawNDBs(NavData *navData)
{
    if (_NDBDirty) {
	// Something's changed, so we need to regenerate the NDB
	// display list.
	assert(_NDBDisplayList != 0);
	glNewList(_NDBDisplayList, GL_COMPILE); {
	    const vector<Cullable *>& intersections = 
		navData->hits(NavData::NAVAIDS);
	    for (unsigned int i = 0; i < intersections.size(); i++) {
		NAV *n = dynamic_cast<NAV *>(intersections[i]);
		if (!n || (n->navtype != NAV_NDB)) {
		    continue;
		}
		assert(n->navtype == NAV_NDB);

		_renderNDB(n);
	    }
	}
	glEndList();

	_NDBDirty = false;
    }

    glCallList(_NDBDisplayList);
}

void NavaidsOverlay::drawILSs(NavData *navData)
{
    if (_ILSDirty) {
	// Something's changed, so we need to regenerate the ILS
	// display list.
	assert(_ILSDisplayList != 0);
	glNewList(_ILSDisplayList, GL_COMPILE); {
	    // We do all markers first, then all the ILS systems.
	    // This ensures that markers don't obscure the localizers.
	    const vector<Cullable *>& intersections = 
		navData->hits(NavData::NAVAIDS);
	    for (unsigned int i = 0; i < intersections.size(); i++) {
		NAV *n = dynamic_cast<NAV *>(intersections[i]);
		assert(n);
		switch (n->navtype) {
		  case NAV_OM:
		  case NAV_MM:
		  case NAV_IM:
		    _renderMarker(n);
		    break;
		  default:
		    break;
		}
	    }
	    for (unsigned int i = 0; i < intersections.size(); i++) {
		NAV *n = dynamic_cast<NAV *>(intersections[i]);
		assert(n);
		switch (n->navtype) {
		  case NAV_ILS:
		    _renderILS(n);
		    break;
		  default:
		    break;
		}
	    }
	}
	glEndList();

	_ILSDirty = false;
    }

    glCallList(_ILSDisplayList);
}

void NavaidsOverlay::drawDMEs(NavData *navData)
{
    if (_DMEDirty) {
	// Something's changed, so we need to regenerate the DME
	// display list.
	assert(_DMEDisplayList != 0);
	glNewList(_DMEDisplayList, GL_COMPILE); {
	    const vector<Cullable *>& intersections = 
		navData->hits(NavData::NAVAIDS);
	    for (unsigned int i = 0; i < intersections.size(); i++) {
		NAV *n = dynamic_cast<NAV *>(intersections[i]);
		if (!n || (n->navtype != NAV_DME)) {
		    continue;
		}
		assert(n->navtype == NAV_DME);

		switch (n->navsubtype) {
		  case TACAN:
		    _renderVOR(n);
		    break;
		  case DME:
		    // Stand-alone DME.
		    _renderDME(n);
		    break;
		  case DME_ILS:
		    _renderDMEILS(n);
		    break;
		  default:
		    break;
		}
	    }
	}
	glEndList();

	_DMEDirty = false;
    }

    glCallList(_DMEDisplayList);
}

// Drawing strategy:
//
// - draw nothing
// - draw icon
// - draw icon, rose
// - draw icon, rose, ticks and labels
//
// - label with id if close enough
// - label with id and frequency
// - label with name / id, frequency and morse
//
// Sizing strategy:
//
// - icon fixed, draw if VOR range more than x pixels
// - rose proportional to range, with maximum and minimum sizes (if
//   less than minimum, don't draw)
// - label font proportional to rose size, but within tight limits
// - move label closer to icon as we zoom out?

// EYE - make this part of the class or rendering policy?
enum VORScaling {pixel, nm, range};

// Renders the given navaid, which must be either a VOR *or* a TACAN.

// EYE - should we do TACANs separately?  I initially put it here
// because TACANS provide aziumuthal information, like VORs.  However,
// the TACANs in the nav.dat database don't (at least they have no
// magnetic variation), and so I don't draw them with a rose.
void NavaidsOverlay::_renderVOR(const NAV *n)
{
    // Make its radius 100 pixels.
// 	VORScaling scaleType = pixel;
// 	float scaleFactor = 100.0;
    // Make its radius 10 nm.
// 	VORScaling scaleType = nm;
// 	float scaleFactor = 10.0;
    // Make its radius 1/10 of its range.
    VORScaling scaleType = range;
    float scaleFactor = 0.1;

    const float minVORSize = 2.0, maxVORSize = 150.0;
    const float lineScale = 0.005, maxLineWidth = 5.0;
    const float iconSize = 7.5;	// Icon size, in pixels
    // Only draw the rose if it's substantially bigger than the icon.
    const float minRoseSize = iconSize * 4.0;
    const float labelPointSize = 10.0; // Pixels
    const float angularWidth = 10.0;   // Width of 'radial' (degrees)

    assert((n->navtype == NAV_VOR) ||
	   ((n->navtype == NAV_DME) && (n->navsubtype == TACAN)));

    // Calculate desired VOR rose size.
    float radius;
    if (scaleType == pixel) {
	radius = scaleFactor;
    } else if (scaleType == nm) {
	radius = scaleFactor * SG_NM_TO_METER / _metresPerPixel;
    } else {
	radius = n->range * scaleFactor / _metresPerPixel;
    }

    // Only draw the VOR if it's bigger than the minimum size.
    if (radius < minVORSize) {
	return;
    }

    // Draw it no bigger than the maximum.
    if (radius > maxVORSize) {
	radius = maxVORSize;
    }

    geodPushMatrix(n->_bounds.center, n->lat, n->lon); {
	////////////////////
	// VOR icon
	////////////////////
	glPushMatrix(); {
	    // We usually draw the icon at constant size.  However, we
	    // never draw it larger than the VOR radius.
	    float scale;
	    if (radius > iconSize) {
		scale = iconSize * _metresPerPixel;
	    } else {
		scale = radius * _metresPerPixel;
	    }
	    glScalef(scale, scale, scale);

	    glPushAttrib(GL_POINT_BIT); {
		// EYE - magic number
		glPointSize(3.0);

		if (n->navtype == NAV_VOR) {
		    if (n->navsubtype == VOR) {
			glCallList(_VORSymbolDL);
		    } else if (n->navsubtype == VORTAC) {
			glCallList(_VORTACSymbolDL);
		    } else if (n->navsubtype == VOR_DME) {
			glCallList(_VORDMESymbolDL);
		    }
		} else {
		    glPushAttrib(GL_LINE_BIT); {
			// A line width of 1 makes it too hard to pick
			// out, at least when drawn in grey.
			glLineWidth(2.0);
			glCallList(_TACANSymbolDL);
		    }
		    glPopAttrib();
		}
	    }
	    glPopAttrib();
	}
	glPopMatrix();

	////////////////////
	// VOR radial
	////////////////////
	if (_p) {
	    // It's possible for zero, one, or both of the radios to
	    // be tuned in to the navaid.
	    double rad;
	    // NMEA tracks set their frequencies to 0, so these tests
	    // should always fail for NMEA tracks.
	    if (n->freq == _p->nav1_freq) {
		rad = _p->nav1_rad + n->magvar;
		glPushMatrix(); {
		    glRotatef(-rad, 0.0, 0.0, 1.0);
		    glScalef(n->range, n->range, n->range);
		    _createTriangle(angularWidth, clearColour, 
				    globals.vor1Colour);
		}
		glPopMatrix();
	    }
	    if (n->freq == _p->nav2_freq) {
		rad = _p->nav2_rad + n->magvar;
		glPushMatrix(); {
		    glRotatef(-rad, 0.0, 0.0, 1.0);
		    glScalef(n->range, n->range, n->range);
		    _createTriangle(angularWidth, clearColour, 
				    globals.vor2Colour);
		}
		glPopMatrix();
	    }
	}

	////////////////////
	// VOR label
	////////////////////
	if (_overlays.isVisible(Overlays::LABELS) && (radius > iconSize)) {
	    // EYE - this DME business is hacky.  Create a
	    // _renderTACAN routine?
	    Label *l;
	    float pointSize = labelPointSize * _metresPerPixel;

	    // Place the centre of the label halfway between the VOR
	    // centre and the southern rim, as long as it won't result
	    // in the label overwriting the icon.  For TACANs, place
	    // it just below the icon.
	    float roseCentre = -radius / 2.0 * _metresPerPixel,
		iconEdge = -(iconSize + 4) * _metresPerPixel;
	    if (radius > 100.0) {
		if (n->navtype == NAV_VOR) {
		    l = _makeLabel("%N\n%F %I %M", n, pointSize, 0, roseCentre);
		} else {
		    l = _makeLabel("%N\nDME %F %I %M", n, pointSize, 
				   0, iconEdge, LayoutManager::UC);
		}
	    } else if (radius > 50.0) {
		if (n->navtype == NAV_VOR) {
		    l = _makeLabel("%F %I", n, pointSize, 0, roseCentre);
		} else {
		    l = _makeLabel("DME %F %I", n, pointSize, 
				   0, iconEdge, LayoutManager::UC);
		}
	    } else {
		l = _makeLabel("%I", n, pointSize, 
			       0, iconEdge, LayoutManager::UC);
	    }

	    if ((n->navtype == NAV_VOR) && (l->lm.y() > iconEdge)) {
		l->lm.moveTo(0.0, iconEdge);
		l->lm.setAnchor(LayoutManager::UC);
	    }
	    _drawLabel(l);

	    delete l;
	}

	////////////////////
	// VOR rose
	////////////////////
	if ((n->navtype == NAV_VOR) && (radius > minRoseSize)) {
	    // Calculate the line width for drawing the rose.  We
	    // scale the line width because when zooming in, it looks
	    // better if the lines become fatter.
	    float lineWidth;
	    if ((lineScale * radius) > maxLineWidth) {
		lineWidth = maxLineWidth;
	    } else {
		lineWidth = lineScale * radius;
	    }

	    glScalef(radius * _metresPerPixel,
		     radius * _metresPerPixel,
		     radius * _metresPerPixel);
	    glRotatef(-n->magvar, 0.0, 0.0, 1.0);

	    glPushAttrib(GL_LINE_BIT); {
		glLineWidth(lineWidth);
	    
		// Draw the VOR rose using the VOR colour.
		glColor4fv(vor_colour);
		glCallList(_VORRoseDL);
	    }
	    glPopAttrib();
	}
    }
    geodPopMatrix();
}

void NavaidsOverlay::_renderNDB(const NAV *n)
{
    const float iconSize = 20.0; // Icon size, in pixels.
    float dotSize;		// Size of an individual dot

    // Scaled size of NDB, in pixels.  We try to draw the NDB at this
    // radius, as long as it won't be too small or too big.
    float radius = n->range * 0.1 / _metresPerPixel;
    const float minNDBSize = 1.0;
    const float maxNDBSize = iconSize;
    const float labelPointSize = 10.0; // Pixels
    const float angularWidth = 2.5;    // Width of 'radial' (degrees)
    
    if (radius < minNDBSize) {
	return;
    }

    if (radius > maxNDBSize) {
	radius = maxNDBSize;
    }
    // The size of the dots in the NDB varies as the NDB's radius
    // varies.
    // EYE - yes, another magic number
    dotSize = radius * 0.1;

    // Check if we're tuned into this NDB.
    bool live = false;
    if (_p && (n->freq == _p->adf_freq)) {
	live = true;
    }

    geodPushMatrix(n->_bounds.center, n->lat, n->lon); {
	glPushMatrix(); {
	    ////////////////////
	    // NDB icon
	    ////////////////////
	    glPushAttrib(GL_POINT_BIT); {
		glPointSize(dotSize);

		float scale = radius * _metresPerPixel;
		glScalef(scale, scale, scale);

		if (n->navsubtype == NDB) {
		    glCallList(_NDBSymbolDL);
		} else if (n->navsubtype == NDB_DME) {
		    glCallList(_NDBDMESymbolDL);
		} else if (n->navsubtype == LOM) {
		    // EYE - an LOM is just an NDB on top of an outer marker.
		    // However, of the 24 LOMs listed in the 850 file, 16
		    // don't have corresponding outer markers.  What to do?
		    //
		    // A: check some LOMs on real VFR charts and see how
		    // they're rendered.  Curiously, most of them are in
		    // Denmark.
		    glCallList(_NDBSymbolDL);
		}
	    }
	    glPopAttrib();
	}
	glPopMatrix();

	////////////////////
	// NDB 'radial'
	////////////////////
	if (_p && (n->freq == _p->adf_freq)) {
	    glPushMatrix(); {
		// EYE - this seems like overkill.  Is there a simpler
		// way?  Really, I should be able to use a directly
		// calculated angle.  After all, the NDB doesn't
		// really care about the curvature of the earth.
		double rad, end, l;
		geo_inverse_wgs_84(n->lat, n->lon, 
				   _p->lat, _p->lon, 
				   &rad, &end, &l);
		glRotatef(180.0 - rad, 0.0, 0.0, 1.0);
		glScalef(n->range, n->range, n->range);
		_createTriangle(angularWidth, globals.adfColour, 
				globals.adfColour, false);
	    }
	    glPopMatrix();
	}

	////////////////////
	// NDB label
	////////////////////
	if (_overlays.isVisible(Overlays::LABELS) && 
	    (radius > iconSize / 5.0)) {
	    float pointSize = labelPointSize * _metresPerPixel;
	    float labelOffset = (radius + 2.0) * _metresPerPixel;
	    LayoutManager::Point p = LayoutManager::LC;
	    if (fabs(radius - iconSize) < 0.01) {
		if (n->navsubtype != NDB_DME) {
		    _drawLabel("%N\n%F %I %M", n, pointSize, 0, labelOffset, p);
		} else {
		    // EYE - and a different colour?
		    _drawLabel("%N\n%F (%f) %I %M", n, pointSize, 
			       0, labelOffset, p);
		}
	    } else if (radius > iconSize / 2.0) {
		if (n->navsubtype != NDB_DME) {
		    _drawLabel("%F %I", n, pointSize, 0, labelOffset, p);
		} else {
		    // EYE - and a different colour?
		    _drawLabel("%F (%f) %I", n, pointSize, 0, labelOffset, p);
		}
	    } else {
		_drawLabel("%I", n, pointSize, 0, labelOffset, p);
	    }
	}
    }
    geodPopMatrix();
}

// ILS
//
// - freq, runway
// - freq, runway, heading
// - freq, runway, heading, id, morse
// - freq, full name (w/o airport), heading, id, morse
//
// note: ILS name is: <airport> <runway> <type> (eg, KSFO 19L ILS-cat-I)
//
// full name | heading
// freq      |
//
// full name	   | heading
// freq, id, morse |

void NavaidsOverlay::_renderILS(const NAV *n)
{
    const float minimumScale = 100.0;
    float labelPointSize = 10.0; // Pixels

    // Localizers are drawn 3 degrees wide.  This is an approximation,
    // as localizer angular widths actually vary.  According to the
    // FAA AIM, localizers are adjusted so that they are 700' wide at
    // the runway threshold.  Localizers on long runways then, have a
    // smaller angular width than those on short runways.  We're not
    // going for full physical accuracy, but rather for a symbolic
    // representation of reality, so 3.0 is good enough.
    const float ilsWidth = 3.0;

    float ilsLength = 7.5 * SG_NM_TO_METER; // 7.5 nm
    // EYE - cast!
    float *ilsColour = (float *)ils_colour;
    bool live = false;		// True if the ILS is 'live' (tuned in).
    // NMEA tracks set their frequencies to 0, so these tests should
    // always fail for NMEA tracks.
    if (_p) {
	if (n->freq == _p->nav1_freq) {
	    // When an ILS is tuned in, we draw it differently - it is
	    // drawn to its true length, and we use the radio colour
	    // to colour it.
	    ilsLength = n->range;
	    ilsColour = globals.vor1Colour;
	    live = true;
	} else if (n->freq == _p->nav2_freq) {
	    ilsLength = n->range;
	    ilsColour = globals.vor2Colour;
	    live = true;
	}
    }

    // EYE - magic number - we care more about the width of the ILS
    // than the length anyway.
    if (ilsLength / _metresPerPixel < minimumScale) {
	return;
    }
    // EYE - what about glideslopes (NAV_GS)?  Will they ever exist
    // standalone?

    // We scale the label when the ILS is very small.
    if (_metresPerPixel > 50.0) {
	labelPointSize = labelPointSize * 50.0 / _metresPerPixel;
    }

    geodPushMatrix(n->_bounds.center, n->lat, n->lon); {
	glPushMatrix(); {
	    glRotatef(-n->magvar, 0.0, 0.0, 1.0);
	    glScalef(ilsLength, ilsLength, ilsLength);
	    switch (n->navsubtype) {
	      case IGS:
	      case ILS_cat_I:
	      case ILS_cat_II:
	      case ILS_cat_III:
	      case LDA_GS:
	      case LOC_GS:
		if (live) {
// 		    // We draw two triangles: one for the front course
// 		    // and one for the back course.
// 		    _createTriangle(ilsWidth, clearColour, ilsColour, true);
		    // Don't draw a back course.
		    _createTriangle(ilsWidth, clearColour, ilsColour, false);
		} else {
		    glCallList(_ILSSymbolDL);
		}
		break;
	      case LDA:
	      case LOC:
	      case SDF:
		if (live) {
// 		    // We draw two triangles: one for the front course
// 		    // and one for the back course.
// 		    _createTriangle(ilsWidth, clearColour, ilsColour, true);
		    // Don't draw a back course.
		    _createTriangle(ilsWidth, clearColour, ilsColour, false);
		} else {
		    glCallList(_LOCSymbolDL);
		}
		break;
	      default:
		break;
	    }
	}
	glPopMatrix();

	// Label the ILS.
	if (_overlays.isVisible(Overlays::LABELS) &&
	    (ilsLength / _metresPerPixel > minimumScale)) {
	    // EYE - shrink this as we zoom out?
	    glPushMatrix(); {
		if (n->magvar < 180.0) {
		    glRotatef(-(n->magvar + 270.0), 0.0, 0.0, 1.0);
		} else {
		    glRotatef(-(n->magvar + 90.0), 0.0, 0.0, 1.0);
		}

		// EYE - Slightly translucent colours look better
		// than opaque ones, and they look better if
		// there's a coloured background (as we have with
		// the box around VORs and NDBs).
		glColor4fv(ils_label_colour);
		float offset;
		if (n->magvar < 180.0) {
		    // EYE - a bit ugly - because we're using
		    // _renderMorse(), we have to have GL units as
		    // metres (fix this somehow?), so we can't call
		    // glScalef(), so we have to scale everything
		    // ourselves.
		    offset = -0.5 * ilsLength;
		} else {
		    offset = 0.5 * ilsLength;
		}

		float pointSize = labelPointSize * _metresPerPixel;
		// We draw the ILS in a single style, but it might be
		// better to alter it depending on the scale.
		_drawLabel("RWY %N\n%F %I %M", n, pointSize, offset, 0.0);

		// Now add a heading near the end.
		// EYE - magic number
		offset *= 1.75;
		LayoutManager lm;
		// EYE - magic number
		lm.setFont(_overlays.regularFont(), pointSize * 1.25);
		lm.begin(offset, 0.0);
		// EYE - just record this once, when the navaid is loaded?
		double magvar = 0.0;
		const char *magTrue = "T";
		if (_overlays.ac()->magTrue()) {
		    magvar = magneticVariation(n->lat, n->lon, n->elev);
		    magTrue = "";
		}
		int heading = normalizeHeading(rint(n->magvar - magvar), false);

		// EYE - we should add the glideslope too, if it has
		// one (eg, "284@3.00")

		globals.str.printf("%03d%c%s", heading, degreeSymbol, magTrue);
		lm.addText(globals.str.str());
		lm.end();

		glColor4fv(ils_label_colour);
		lm.drawText();
	    }
	    glPopMatrix();
	}
    }
    geodPopMatrix();
}

void NavaidsOverlay::_renderMarker(const NAV *n)
{
    const double minMarkerSize = 5.0;

    // EYE - ugly construction
    if (n->navtype == NAV_OM) {
	if (markerRadii[0] / _metresPerPixel < minMarkerSize) {
	    return;
	}
    } else if (n->navtype == NAV_MM) {
	if (markerRadii[1] / _metresPerPixel < minMarkerSize) {
	    return;
	}
    } else if (n->navtype == NAV_IM) {
	if (markerRadii[2] / _metresPerPixel < minMarkerSize) {
	    return;
	}
    }

    geodPushMatrix(n->_bounds.center, n->lat, n->lon); {
	glRotatef(-n->magvar + 90.0, 0.0, 0.0, 1.0);
	if (n->navtype == NAV_OM) {
	    glCallList(_ILSMarkerDLs[0]);
	} else if (n->navtype == NAV_MM) {
	    glCallList(_ILSMarkerDLs[1]);
	} else if (n->navtype == NAV_IM) {
	    glCallList(_ILSMarkerDLs[2]);
	}
    }
    geodPopMatrix();
}

// Renders a stand-alone DME.
void NavaidsOverlay::_renderDME(const NAV *n)
{
    const float iconSize = 20.0; // Icon size, in pixels.

    // Scaled size of DME, in pixels.  We try to draw the DME at this
    // radius, as long as it won't be too small or too big.
    float radius = n->range * 0.1 / _metresPerPixel;
    const float minDMESize = 1.0;
    const float maxDMESize = iconSize;
    const float labelPointSize = 10.0; // Pixels
    
    if (radius < minDMESize) {
	return;
    }

    if (radius > maxDMESize) {
	radius = maxDMESize;
    }

    geodPushMatrix(n->_bounds.center, n->lat, n->lon); {
	////////////////////
	// DME icon
	////////////////////
	glPushMatrix(); {
	    float scale = radius * _metresPerPixel;
	    glScalef(scale, scale, scale);
	
	    glCallList(_DMESymbolDL);
	}
	glPopMatrix();

	////////////////////
	// DME label
	////////////////////
	if (_overlays.isVisible(Overlays::LABELS) && 
	    (radius > iconSize / 5.0)) {
	    // Put the DME name, frequency, id, and morse code in a
	    // box above the icon.  The box has a translucent white
	    // background with a solid border to make it easier to
	    // read.
	    float pointSize = labelPointSize * _metresPerPixel;
	    float offset = radius * _metresPerPixel;
	    LayoutManager::Point p = LayoutManager::UC;
	    if (fabs(radius - iconSize) < 0.01) {
		_drawLabel("%N\n%F %I %M", n, pointSize, 0.0, offset, p);
	    } else if (radius > iconSize / 2.0) {
		_drawLabel("%F %I", n, pointSize, 0.0, offset, p);
	    } else {
		_drawLabel("%I", n, pointSize, 0.0, offset, p);
	    }
	}
    }
    geodPopMatrix();
}

// Renders a DME-ILS.

// EYE - do this with ILS stuff!  Make sure that the DME frequency and
// ILS frequency match!  We need to actually pair them so that we know
// when to render the DME, so this requires extra work when loading
// the file - check frequency and id (and location?).
void NavaidsOverlay::_renderDMEILS(const NAV *n)
{
    const float iconSize = 10.0; // Icon size, in pixels.

    // Scaled size of DME, in pixels.  We try to draw the DME at this
    // radius, as long as it won't be too small or too big.  Note that
    // the DME-ILS is scaled much smaller than other navaids, the
    // reasoning being that, being associated with an ILS, it's really
    // only necessary to show the DME when quite close to the airport
    // (even though it can actually be received in the aircraft long
    // before the ILS signal can).
    float radius = n->range * 0.005 / _metresPerPixel;
    const float minDMESize = 1.0;
    const float maxDMESize = iconSize;
    const float labelPointSize = 10.0; // Pixels
    
    if (radius < minDMESize) {
	return;
    }

    if (radius > maxDMESize) {
	radius = maxDMESize;
    }

    geodPushMatrix(n->_bounds.center, n->lat, n->lon); {
	////////////////////
	// DME icon
	////////////////////
	glPushMatrix(); {
	    float scale = radius * _metresPerPixel;
	    glScalef(scale, scale, scale);
	
	    glCallList(_DMESymbolDL);
	}
	glPopMatrix();

	////////////////////
	// DME label
	////////////////////
	// EYE - only draw label (which is just the id) if we draw the
	// ILS id.
	// EYE - this seems like a really screwed-up test.  Does it
	// work (here and elsewhere)?
	if (_overlays.isVisible(Overlays::LABELS) && 
	    (radius > iconSize / 2.0)) {
	    // Put the DME id in a box above the icon.  The box has a
	    // translucent white background with a solid border to
	    // make it easier to read.
	    float pointSize = labelPointSize * _metresPerPixel;
	    float offset = radius * _metresPerPixel;
	    _drawLabel("%I", n, pointSize, 0.0, offset, LayoutManager::LC);
	}
    }
    geodPopMatrix();
}

float _renderMorse(const string& id, float height,
		   float x, float y, float metresPerPixel, bool render = true);
// Returns width necessary to render the given string in morse code,
// at the current point size.
float _morseWidth(const string& id, float height, float metresPerPixel)
{
    return _renderMorse(id, height, 0.0, 0.0, metresPerPixel, false);
}

// Either draws the given string in morse code at the given location
// (if render is true), OR returns the width necessary to draw it (if
// render is false.  In this case, x and y are ignored).  If drawn, we
// draw the morse stacked on top of each other to fill one line (which
// is height high).  We assume that the current OpenGL units are
// metres.  The location (x, y) specifies the lower-left corner of the
// rendered morse text.
float _renderMorse(const string& id, float height,
		   float x, float y, float metresPerPixel, bool render)
{
    float maxWidth = 0.0;

    // EYE - magic numbers
    const float dashWidth = height * 0.8;
    const float dashSpace = height * 1.0;
    // 0.2 looks good in boxes (VORs, NDBs), but too dense in ILSs
//     const float dotWidth = height * 0.2;
    const float dotWidth = height * 0.15;
    const float dotSpace = height * 0.4;

    if (render) {
	// We only do the OpenGL stuff if we're actually rendering.
	glPushAttrib(GL_LINE_BIT);
	glLineWidth(dotWidth / metresPerPixel);
	glBegin(GL_LINES);
    }

    float incY = 0.0;
    if (id.size() > 1) {
	incY = height / (id.size() - 1);
    }
    float curY = y + height;
    if (id.size() <= 1) {
	curY = y + height / 2.0;
    }
    for (unsigned int i = 0; i < id.size(); i++) {
	float curX = x;
	const char *morse = toMorse(id[i]);
	if (morse) {
	    for (unsigned int j = 0; j < strlen(morse); j++) {
		if (morse[j] == '.') {
		    if (render) {
			glVertex2f(curX, curY);
			glVertex2f(curX + dotWidth, curY);
		    }
		    curX += dotSpace;
		} else {
		    if (render) {
			glVertex2f(curX, curY);
			glVertex2f(curX + dashWidth, curY);
		    }
		    curX += dashSpace;
		}
	    }
	}

	if ((curX - x) > maxWidth) {
	    maxWidth = curX - x;
	}

	curY -= incY;
    }

    if (render) {
	glEnd();
	glPopAttrib();
    }

    return maxWidth;
}

// Called from the layout manager when it encounters an addBox() box.
void _morseCallback(LayoutManager *lm, float x, float y, void *userData)
{
    Label *l = (Label *)userData;
    float ascent = lm->font()->ascent() * lm->pointSize();
    _renderMorse(l->id, ascent, x, y, l->metresPerPixel);
}

// Create a navaid label.  We use a printf-style format string to
// specify the style.  The format string can include text (including
// linefeeds, specified with '\n') and conversion specifications, a la
// printf.  Valid specifications are:
//
// %I - id
// %M - morse code version of id
// %N - name
// %F - primary frequency
// %f - second frequency (the DME part of an NDB-DME)
// %% - literal '%'
//
// All of the data for the conversion specifications comes from the
// *single* NAV parameter (unlike printf).
//
// Each line of text is centered, and the point p of the bounding box
// is placed at <x, y>.
Label *NavaidsOverlay::_makeLabel(const char *fmt, const NAV *n,
				  float labelPointSize,
				  float x, float y,
				  LayoutManager::Point p)
{
    // The label consists of a list of lines.  Each line consists of
    // intermixed text and morse.  The label, each line, and text and
    // morse unit has a width, height, and origin.
    Label *l = new Label;

    // Set our font and find out what our ascent is (_morseWidth and
    // _renderMorse need it).
    l->lm.setFont(_overlays.regularFont(), labelPointSize);
    float ascent = l->lm.font()->ascent() * labelPointSize;

    // Go through the format string once, using the layout manager to
    // calculate sizes.
    l->lm.begin(x, y);
    bool spec = false;
    // l->morseChunk = -1;
    AtlasString line;
    for (const char *c = fmt; *c; c++) {
	if ((*c == '%') && !spec) {
	    spec = true;
	} else if (spec) {
	    switch (*c) {
	      case 'I':
		line.appendf("%s", n->id.c_str());
		break;
	      case 'M':
		l->lm.addText(line.str());
		line.clear();
		l->lm.addBox(_morseWidth(n->id, ascent, _metresPerPixel), 0.0,
			     _morseCallback, (void *)l);
		l->id = n->id;
		l->metresPerPixel = _metresPerPixel;
		break;
	      case 'N':
		line.appendf("%s", n->name.c_str());
		// The name of an ILS includes the type of approach.
		switch (n->navsubtype) {
		  case IGS:
		    line.appendf(" IGS");
		    break;
		  case ILS_cat_I:
		    line.appendf(" ILS-CAT-I");
		    break;
		  case ILS_cat_II:
		    line.appendf(" ILS-CAT-II");
		    break;
		  case ILS_cat_III:
		    line.appendf(" ILS-CAT-III");
		    break;
		  case LDA_GS:
		    line.appendf(" LDA-GS");
		    break;
		  case LOC_GS:
		    line.appendf(" LOC-GS");
		    break;
		  case LDA:
		    line.appendf(" LDA");
		    break;
		  case LOC:
		    line.appendf(" LOC");
		    break;
		  case SDF:
		    line.appendf(" SDF");
		    break;
		  default:
		    break;
		}
		break;
	      case 'F':
		line.appendf("%s", formatFrequency(n->freq));
		break;
	      case 'f':
		// DME frequency in an NDB-DME.
		assert((n->navtype == NAV_NDB) && (n->navsubtype == NDB_DME));
		line.appendf("%s", formatFrequency(n->freq2));
		break;
	      case '%':
		line.appendf("%%");
		break;
	      default:
		line.appendf("%%%c", *c);
		break;
	    }
	    spec = false;
	} else if (*c == '\n'){
	    l->lm.addText(line.str());
	    line.clear();
	    l->lm.newline();
	} else {
	    line.appendf("%c", *c);
	}
    }
    l->lm.addText(line.str());
    l->lm.end();

    switch (n->navtype) {
      case NAV_VOR:
	memcpy(l->colour, vor_colour, sizeof(float) * 4);
	l->lm.setBoxed(true);
	break;
      case NAV_DME:
	memcpy(l->colour, dme_colour, sizeof(float) * 4);
	l->lm.setBoxed(true);
	break;
      case NAV_NDB:
	memcpy(l->colour, ndb_colour, sizeof(float) * 4);
	l->lm.setBoxed(true);
	break;
      case NAV_ILS:
	memcpy(l->colour, ils_label_colour, sizeof(float) * 4);
	break;
      default:
	break;
    }

    l->lm.setAnchor(p);

    return l;
}

// Draws the label, with point p on the label placed at x, y.  The
// label will be drawn in the current font, at the given point size.
// VORs, DMEs and NDBs are drawn with a box around the text and a
// translucent white background behind the text.
void NavaidsOverlay::_drawLabel(const char *fmt, const NAV *n,
				float labelPointSize,
				float x, float y,
				LayoutManager::Point p)
{
    Label *l;

    l = _makeLabel(fmt, n, labelPointSize, x, y, p);
    _drawLabel(l);

    delete l;
}

void NavaidsOverlay::_drawLabel(Label *l)
{
    // Draw the text.
    glColor4fv(l->colour);
    l->lm.drawText();
}

void NavaidsOverlay::notification(Notification::type n)
{
    if (n == Notification::Moved) {
	setDirty();
    } else if (n == Notification::Zoomed) {
	_metresPerPixel = _overlays.aw()->scale();
	setDirty();
    } else if ((n == Notification::AircraftMoved) ||
	       (n == Notification::NewFlightTrack)) {
	_p = _overlays.ac()->currentPoint();

	// The aircraft moved, or we loaded a new flight track.  We
	// may have to update how "live" radios are drawn.
	if (!_p) {
	    // No flight data, so no flight track.  We'll probably need to
	    // redraw any radio "beams".
	    setDirty();
	    return;
	}

	// Check if the radios have changed.  If so, set the
	// appropriate navaids overlay dirty as well.
	if ((_radios[0] != _p->nav1_freq) || (_radials[0] != _p->nav1_rad)) {
	    _radios[0] = _p->nav1_freq;
	    _radials[0] = _p->nav1_rad;
	    // NAV1 and NAV2 radios don't tune in NDBs, so we don't
	    // set the NDBs dirty.
	    _VORDirty = true;
	    _NDBDirty = true;
	    _ILSDirty = true;
	}
	if ((_radios[1] != _p->nav2_freq) || (_radials[1] != _p->nav2_rad)) {
	    _radios[1] = _p->nav2_freq;
	    _radials[1] = _p->nav2_rad;
	    // NAV1 and NAV2 radios don't tune in NDBs, so we don't
	    // set the NDBs dirty.
	    _VORDirty = true;
	    _NDBDirty = true;
	    _ILSDirty = true;
	}

	// VORs are drawn with a radial corresponding to the radio
	// setting.  So, only when the radio changes do we need to
	// redraw the radial.  However, live NDBs are drawn with a
	// "radial" that tracks the aircraft, so they must be redrawn
	// whenever the aircraft moves.
	_NDBDirty = true;
    } else if (n == Notification::MagTrue) {
	// This means that we have to switch our display between
	// magnetic and true headings.  This only affects ILSs, so we
	// bypass setDirty() and just set _ILSDirty explicitly.
	_ILSDirty = true;
    } else {
	assert(false);
    }
}
