/*-------------------------------------------------------------------------
  NavaidsOverlay.cxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

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

#include <stdarg.h>

#include <cassert>
#include <sstream>

#include <simgear/misc/sg_path.hxx>
#include <simgear/math/sg_geodesy.hxx>
#include <simgear/magvar/magvar.hxx>
#include <simgear/timing/sg_time.hxx>

#include "Globals.hxx"
#include "misc.hxx"
#include "Geographics.hxx"

#include "NavaidsOverlay.hxx"

using namespace std;

// From Atlas.cxx, used for colouring tuned-in navaids.
// EYE - make part of preferences
extern float vor1Colour[];
extern float vor2Colour[];
extern float adfColour[];
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

//////////////////////////////////////////////////////////////////////
// Searchable interface.
//////////////////////////////////////////////////////////////////////
double NAV::distanceSquared(const sgdVec3 from) const
{
    return sgdDistanceSquaredVec3(bounds.center, from);
}

// Returns our tokens, generating them if they haven't been already.
const std::vector<std::string>& NAV::tokens()
{
    if (_tokens.empty()) {
	bool isNDB = (navtype == NAV_NDB);
	bool isMarker = ((navtype == NAV_OM) ||
			 (navtype == NAV_MM) ||
			 (navtype == NAV_IM));
    
	// The id, if it has one, is a token.
	if (!isMarker) {
	    _tokens.push_back(id);
	}

	// Tokenize the name.
	Searchable::tokenize(name, _tokens);

	// Add a frequency too, if it has one.
	if (!isMarker) {
	    if (isNDB) {
		globalString.printf("%d", freq);
	    } else {
		globalString.printf("%.2f", freq / 1000.0);
	    }
	    _tokens.push_back(globalString.str());
	}

	// Add a navaid type token.
	switch (navtype) {
	  case NAV_VOR:
	    _tokens.push_back("VOR:");
	    break;
	  case NAV_DME:
	    _tokens.push_back("DME:");
	    break;
	  case NAV_NDB:
	    _tokens.push_back("NDB:");
	    break;
	  case NAV_ILS:
	  case NAV_GS:
	    _tokens.push_back("ILS:");
	    break;
	  case NAV_OM:
	    _tokens.push_back("MKR:");
	    _tokens.push_back("OM:");
	    break;
	  case NAV_MM:
	    _tokens.push_back("MKR:");
	    _tokens.push_back("MM:");
	    break;
	  case NAV_IM:
	    _tokens.push_back("MKR:");
	    _tokens.push_back("IM:");
	    break;
	  default:
	    assert(false);
	    break;
	}
    }

    return _tokens;
}

// Returns our pretty string, generating it if it hasn't been already.
const std::string& NAV::asString()
{
    if (_str.empty()) {
	// Initialize our pretty string.
	switch (navtype) {
	  case NAV_VOR:
	    // EYE - cleanString?
	    globalString.printf("VOR: %s %s (%.2f)", 
				id.c_str(), name.c_str(), freq / 1000.0);
	    break;
	  case NAV_DME:
	    globalString.printf("DME: %s %s (%.2f)", 
				id.c_str(), name.c_str(), freq / 1000.0);
	    break;
	  case NAV_NDB:
	    globalString.printf("NDB: %s %s (%d)", 
				id.c_str(), name.c_str(), freq);
	    break;
	  case NAV_ILS:
	  case NAV_GS:
	    globalString.printf("ILS: %s %s (%.2f)", 
				id.c_str(), name.c_str(), freq / 1000.0);
	    break;
	  case NAV_OM:
	    globalString.printf("MKR: OM: %s", name.c_str());
	    break;
	  case NAV_MM:
	    globalString.printf("MKR: MM: %s", name.c_str());
	    break;
	  case NAV_IM:
	    globalString.printf("MKR: IM: %s", name.c_str());
	    break;
	  default:
	    assert(false);
	    break;
	}

	_str = globalString.str();
    }

    return _str;
}

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

    // Create a culler and searchers for it.
    _culler = new Culler();
    _frustum = new Culler::FrustumSearch(*_culler);
    _point = new Culler::PointSearch(*_culler);

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
    for (unsigned int i = 0; i < _navaids.size(); i++) {
	NAV *n = _navaids[i];

	delete n;
    }

    _navaids.clear();

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

bool NavaidsOverlay::load(const string& fgDir)
{
    bool result = false;

    SGPath f(fgDir);
    // EYE - magic name
    f.append("Navaids/nav.dat.gz");

    gzFile arp;
    char *line;

    arp = gzopen(f.c_str(), "rb");
    if (arp == NULL) {
	// EYE - we might want to throw an error instead.
	fprintf(stderr, "_loadNavaids: Couldn't open \"%s\".\n", f.c_str());
	return false;
    } 

    // Check the file version.  We can handle version 810 files.  Note
    // that there was a mysterious (and stupid, in my opinion) change
    // in how DMEs were formatted some time after data cycle 2007.09.
    // So we need to check the data cycle as well.  Unfortunately, the
    // file version line doesn't have a constant format.  We could
    // have the following two:
    //
    // 810 Version - data cycle 2008.05
    //
    // 810 Version - DAFIF data cycle 2007.09
    int version = -1;
    int index;
    float cycle = 0.0;
    gzGetLine(arp, &line);	// Windows/Mac header
    gzGetLine(arp, &line);	// Version
//     sscanf(line, "%d", &version);
    sscanf(line, "%d Version - %n", &version, &index);
    if (strncmp(line + index, "DAFIF ", 6) == 0) {
	index += 6;
    }
    sscanf(line + index, "data cycle %f", &cycle);
    if (version == 810) {
	// It looks like we have a valid file.
	result = _load810(cycle, arp);
    } else {
	// EYE - throw an error?
	fprintf(stderr, "_loadNavaids: \"%s\": unknown version %d.\n", 
		f.c_str(), version);
	result = false;
    }

    gzclose(arp);

    return result;
}

// Creates a standard VOR rose of radius 1.0.  This is a circle with
// ticks and arrows, a line from the centre indicating north, and
// labels at 30-degree intervals around the outside.
//
// The rose is drawn in the current colour, with the current line
// width.
void NavaidsOverlay::_createVORRose()
{
    atlasFntRenderer& f = globals.fontRenderer;

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

	// Label the rose.
	const float pointSize = 10.0;
	f.setPointSize(pointSize);
	for (int i = 0; i < 360; i += 30) {
	    glPushMatrix(); {
		glRotatef(-i, 0.0, 0.0, 1.0);
		glTranslatef(0.0, 1.0, 0.0);
		glScalef(0.01, 0.01, 1.0);

		char label[4];
		sprintf(label, "%d", i / 10);

		float left, right, bottom, top;
		f.getFont()->getBBox(label, pointSize, 0.0,
				     &left, &right, &bottom, &top);
		f.start3f(-(left + right) / 2.0, 0.2 * pointSize, 0.0);
		f.puts(label);
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
		    glVertex2f(-size / 2.0, lobeThickness);
		    glVertex2f(size / 2.0, lobeThickness);
		    glVertex2f(size / 2.0, 0.0);
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

bool NavaidsOverlay::_load810(float cycle, const gzFile& arp)
{
    char *line;

    NAV *n;

    fprintf(stderr, "Loading navaids ...\n");
    while (gzGetLine(arp, &line)) {
	NavType navtype;
	NavSubType navsubtype;
	int lineCode, offset;
	double lat, lon;
	int elev, freq, range;
	float magvar;
	char id[5];

	if (strcmp(line, "") == 0) {
	    // Blank line.
	    continue;
	} 

	if (strcmp(line, "99") == 0) {
	    // Last line.
	    break;
	}

	// A line looks like this:
	//
	// <code> <lat> <lon> <elev> <freq> <range> <magvar> <id> <name>
	//
	// Where name is a string ending with a "type" (eg, a VOR,
	// type code 3, can either be a VOR, VOR-DME, or VORTAC).
	// This type embedded at the end of the name isn't officially
	// in the navaid data file specification, so we can't
	// absolutely count on it.  On the other hand, every file I've
	// checked consistently has it, and it's useful, so we'll use
	// it.
	if (sscanf(line, "%d %lf %lf %d %d %d %f %s %n", &lineCode, 
		   &lat, &lon, &elev, &freq, &range, &magvar, id, &offset)
	    != 8) {
	    continue;
	}
	line += offset;
	assert(lineCode != 99);

	// Find the "type", which is the last space-delimited string.
	char *subType = lastToken(line);
	assert(subType != NULL);

	// We slightly alter the representation of frequencies.  In
	// the navaid database, NDB frequencies are given in kHz,
	// whereas VOR/ILS/DME/... frequencies are given in 10s of
	// kHz.  We adjust the latter so that they are kHz as well.
	if (lineCode != 2) {
	    freq *= 10;
	}

	// EYE - is having navtype and navsubtype a good idea, or
	// should we just stick to one or the other (presumably the
	// latter would be better)?
	switch (lineCode) {
	  case 2: 
	    navtype = NAV_NDB;
	    if (strcmp(subType, "NDB") == 0) {
		navsubtype = NDB;
	    } else if (strcmp(subType, "NDB-DME") == 0) {
		navsubtype = NDB_DME;
	    } else if (strcmp(subType, "LOM") == 0) {
		navsubtype = LOM;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    break; 
	  case 3: 
	    navtype = NAV_VOR; 
	    if (strcmp(subType, "VOR") == 0) {
		navsubtype = VOR;
	    } else if (strcmp(subType, "VOR-DME") == 0) {
		navsubtype = VOR_DME;
	    } else if (strcmp(subType, "VORTAC") == 0) {
		navsubtype = VORTAC;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    break; 
	  case 4: 
	    if (strcmp(subType, "IGS") == 0) {
		navsubtype = IGS;
	    } else if (strcmp(subType, "ILS-cat-I") == 0) {
		navsubtype = ILS_cat_I;
	    } else if (strcmp(subType, "ILS-cat-II") == 0) {
		navsubtype = ILS_cat_II;
	    } else if (strcmp(subType, "ILS-cat-III") == 0) {
		navsubtype = ILS_cat_III;
	    } else if (strcmp(subType, "LDA-GS") == 0) {
		navsubtype = LDA_GS;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    // EYE - have a NAV_ILS and NAV_LOC?
	    navtype = NAV_ILS; 
	    break; 
	  case 5: 
	    if (strcmp(subType, "LDA") == 0) {
		navsubtype = LDA;
	    } else if (strcmp(subType, "LOC") == 0) {
		navsubtype = LOC;
	    } else if (strcmp(subType, "SDF") == 0) {
		navsubtype = SDF;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    navtype = NAV_ILS; 
	    break; 
	  case 6: 
	    // EYE - if we only have one subtype, forget the whole
	    // subtype business?
	    if (strcmp(subType, "GS") == 0) {
		navsubtype = GS;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    navtype = NAV_GS; 
	    break;  
	  case 7: 
	    if (strcmp(subType, "OM") == 0) {
		// Since the navaid database specifies no range for
		// markers, we set our own, such that it is bigger
		// than the marker's rendered size.
		range = markerRange;
		navsubtype = OM;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    navtype = NAV_OM; 
	    break;  
	  case 8: 
	    if (strcmp(subType, "MM") == 0) {
		range = markerRange;
		navsubtype = MM;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    navtype = NAV_MM; 
	    break;  
	  case 9: 
	    if (strcmp(subType, "IM") == 0) {
		range = markerRange;
		navsubtype = IM;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    navtype = NAV_IM; 
	    break;  
	  case 12: 
	  case 13: 
	    // Due to the "great DME shift" of 2007.09, we need to do
	    // extra processing to handle DMEs.  Here's the picture:
	    //
	    // Before:			After:
	    // Foo Bar DME-ILS		Foo Bar DME-ILS
	    // Foo Bar DME		Foo Bar DME
	    // Foo Bar NDB-DME		Foo Bar NDB-DME DME
	    // Foo Bar TACAN		Foo Bar TACAN DME
	    // Foo Bar VORTAC		Foo Bar VORTAC DME
	    // Foo Bar VOR-DME		Foo Bar VOR-DME DME
	    //
	    // The subType is now less useful, only telling us about
	    // DME-ILSs.  To find out the real subtype, we need to
	    // back one more token and look at that.  However, that
	    // doesn't work for "pure" DMEs (ie, "Foo Bar DME").  So,
	    // if the next token isn't NDB-DME, TACAN, VORTAC, or
	    // VOR-DME, then we must be looking at a pure DME.

	    if ((cycle > 2007.09) && (strcmp(subType, "DME-ILS") != 0)) {
		// New format.  Yuck.  We need to find the "real"
		// subType by looking back one token.
		char *subSubType = lastToken(line, subType);
		if ((strncmp(subSubType, "NDB-DME", 7) == 0) ||
		    (strncmp(subSubType, "TACAN", 5) == 0) ||
		    (strncmp(subSubType, "VORTAC", 6) == 0) ||
		    (strncmp(subSubType, "VOR-DME", 7) == 0)) {
		    // The sub-subtype is the real subtype (getting
		    // confused?).  Terminate the string, and make
		    // subType point to subSubType.
		    subType--;
		    *subType = '\0';
		    subType = subSubType;
		}
	    }

	    // Because DMEs are often paired with another navaid, we
	    // tend to ignore them, assuming that we've already
	    // created a navaid for them already.  The ones ignored
	    // are: VOR-DME, VORTAC, and NDB-DME.  We don't ignore
	    // DME-ILSs because, although paired with an ILS, their
	    // location is usually different.
	    if (strcmp(subType, "DME-ILS") == 0) {
		navsubtype = DME_ILS;
	    } else if (strcmp(subType, "TACAN") == 0) {
		// TACANs are drawn like VOR-DMEs, but with the lobes
		// not filled in.  They can provide directional
		// guidance, so they should have a compass rose.
		// Unfortunately, the nav.dat file doesn't tell us the
		// magnetic variation for the TACAN, so it can't be
		// used.
		navsubtype = TACAN;
	    } else if (strcmp(subType, "VOR-DME") == 0) {
		navsubtype = VOR_DME;
		continue;
	    } else if (strcmp(subType, "VORTAC") == 0) {
		navsubtype = VORTAC;
		continue;
	    } else if (strcmp(subType, "DME") == 0) {
		// EYE - For a real stand-alone DME, check lo1.pdf,
		// Bonnyville Y3, 109.8 (N54.31, W110.74, near Cold
		// Lake, east of Edmonton).  It is drawn as a simple
		// DME square (grey, as is standard on Canadian maps
		// it seems, although lo1.pdf is not a VFR map).
		navsubtype = DME;
	    } else if (strcmp(subType, "NDB-DME") == 0) {
		// We ignore NDB-DMEs, in the sense that we don't
		// create a navaid entry for them.  However, we do add
		// their frequency to the corresponding NDB.
		navsubtype = NDB_DME;
		// EYE - very crude
		unsigned int i;
		for (i = 0; i < _navaids.size(); i++) {
		    NAV *o = _navaids[i];
		    // EYE - look at name too
		    if ((o->navtype == NAV_NDB) && 
			(o->navsubtype == NDB_DME) && 
			(o->id == id)) {
			o->freq2 = freq;
			break;
		    }
		}
		if (i == _navaids.size()) {
		    printf("No matching NDB for NDB-DME %s (%s)\n", id, line);
		}
		continue;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    navtype = NAV_DME; 
	    // For DMEs, magvar represents the DME bias, in nautical
	    // miles (which we convert to metres).
	    magvar *= SG_NM_TO_METER;
	    break;
	  default:
	    assert(false);
	    break;
	}
	if (navsubtype == UNKNOWN) {
	    printf("UNKNOWN: %s\n", line);
	}

	if (navtype == NAV_ILS) {
	    // For ILS elements, the name is <airport> <runway>.  I
	    // don't care about the airport, so skip it.
	    // EYE - check return?
	    sscanf(line, "%*s %n", &offset);
	    line += offset;
	}

	// Create a record and fill it in.
	n = new NAV;
	n->navtype = navtype;
	n->navsubtype = navsubtype;

	n->lat = lat;
	n->lon = lon;
	// EYE - in flight tracks, we save elevations (altitudes?) in
	// feet, but here we use metres.  Should we change one?
	n->elev = elev * SG_FEET_TO_METER;
	n->freq = freq;
	n->range = range * SG_NM_TO_METER;
	n->magvar = magvar;

	n->id = id;
	n->name = line;
	// EYE - this seems rather hacky and unreliable.
	n->name.erase(subType - line - 1);

	// Add to the culler.  The navaid bounds are given by its
	// center and range.
	sgdVec3 center;
	atlasGeodToCart(lat, lon, elev * SG_FEET_TO_METER, center);

	n->bounds.setCenter(center);
	n->bounds.setRadius(n->range);

	// Add to our culler.
	_frustum->culler().addObject(n);

	// Add to the navaids vector.
	_navaids.push_back(n);

	// Create search tokens for it.
	globals.searcher.add(n);

	// Add to the navPoints map.
	NAVPOINT foo;
	foo.isNavaid = true;
	foo.n = (void *)n;
	navPoints.insert(pair<string, NAVPOINT>(n->id, foo));
    }

    // EYE - will there ever be a false return?
    return true;
}

void NavaidsOverlay::setDirty()
{
    _VORDirty = true;
    _NDBDirty = true;
    _ILSDirty = true;
    _DMEDirty = true;
}

void NavaidsOverlay::drawVORs()
{
    if (_VORDirty) {
	// Something's changed, so we need to regenerate the VOR
	// display list.
	assert(_VORDisplayList != 0);
	glNewList(_VORDisplayList, GL_COMPILE); {
	    const vector<Cullable *>& intersections = _frustum->intersections();
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

void NavaidsOverlay::drawNDBs()
{
    if (_NDBDirty) {
	// Something's changed, so we need to regenerate the NDB
	// display list.
	assert(_NDBDisplayList != 0);
	glNewList(_NDBDisplayList, GL_COMPILE); {
	    const vector<Cullable *>& intersections = _frustum->intersections();
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

void NavaidsOverlay::drawILSs()
{
    if (_ILSDirty) {
	// Something's changed, so we need to regenerate the ILS
	// display list.
	const vector<Cullable *>& intersections = _frustum->intersections();

	assert(_ILSDisplayList != 0);
	glNewList(_ILSDisplayList, GL_COMPILE); {
	    // We do all markers first, then all the ILS systems.
	    // This ensures that markers don't obscure the localizers.
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

void NavaidsOverlay::drawDMEs()
{
    if (_DMEDirty) {
	// Something's changed, so we need to regenerate the DME
	// display list.
	assert(_DMEDisplayList != 0);
	glNewList(_DMEDisplayList, GL_COMPILE); {
	    const vector<Cullable *>& intersections = _frustum->intersections();
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

// Returns navaids within range.
const vector<Cullable *>& NavaidsOverlay::getNavaids(sgdVec3 p)
{
    static vector<Cullable *> results;

    results.clear();

    // EYE - should we do this?
    _point->move(p);
    results = _point->intersections();

    return results;
}

// Returns navaids within range and which are tuned in (as given by
// 'p').
const vector<Cullable *>& NavaidsOverlay::getNavaids(FlightData *p)
{
    static vector<Cullable *> results;

    results.clear();

    if (p == NULL) {
	return results;
    }

    // We don't do anything if this is from an NMEA track.
    // Unfortunately, there's no explicit marker in a FlightData
    // structure that tells us what kind of track it is.  However,
    // NMEA tracks have their frequencies and radials set to 0, so we
    // just check for that (and in any case, if frequencies are 0, we
    // won't match any navaids anyway).
    if ((p->nav1_freq == 0) && (p->nav2_freq == 0) && (p->adf_freq == 0)) {
	return results;
    }

    const vector<Cullable *>& navaids = getNavaids(p->cart);
	
    for (unsigned int i = 0; i < navaids.size(); i++) {
	NAV *n = dynamic_cast<NAV *>(navaids[i]);
	assert(n);
	if (p->nav1_freq == n->freq) {
	    results.push_back(n);
	} else if (p->nav2_freq == n->freq) {
	    results.push_back(n);
	} else if (p->adf_freq == n->freq) {
	    results.push_back(n);
	}
    }

    return results;
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

    geodPushMatrix(n->bounds.center, n->lat, n->lon); {
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
		    _createTriangle(angularWidth, clearColour, vor1Colour);
		}
		glPopMatrix();
	    }
	    if (n->freq == _p->nav2_freq) {
		rad = _p->nav2_rad + n->magvar;
		glPushMatrix(); {
		    glRotatef(-rad, 0.0, 0.0, 1.0);
		    glScalef(n->range, n->range, n->range);
		    _createTriangle(angularWidth, clearColour, vor2Colour);
		}
		glPopMatrix();
	    }
	}

	////////////////////
	// VOR label
	////////////////////

	// Only add a label if the icon is being drawn full size.
// 	if (radius > iconSize) {
	if (_overlays.isVisible(Overlays::LABELS) && (radius > iconSize)) {
	    // EYE - this DME business is hacky.  Create a
	    // _renderTACAN routine.
	    Label *l;
	    float pointSize = labelPointSize * _metresPerPixel;
	    if (radius > 100.0) {
		if (n->navtype == NAV_VOR) {
		    l = _makeLabel("%N\n%F %I %M", n, pointSize, 0, 0);
		} else {
		    l = _makeLabel("%N\nDME %F %I %M", n, pointSize, 0, 0);
		}
	    } else if (radius > 50.0) {
		if (n->navtype == NAV_VOR) {
		    l = _makeLabel("%F %I", n, pointSize, 0, 0);
		} else {
		    l = _makeLabel("DME %F %I", n, pointSize, 0, 0);
		}
	    } else {
		l = _makeLabel("%I", n, pointSize, 0, 0);
	    }

	    // Place the centre of the label halfway between the VOR
	    // centre and the southern rim, as long as it won't result
	    // in the label overwriting the icon.  For TACANs, place
	    // it just below the icon.
	    float labelOffset;
	    if (n->navtype == NAV_VOR) {
		labelOffset = radius / 2.0;
	    } else {
		labelOffset = 0;
	    }
	    // EYE - without 4.0, this butts the label right against
	    // the icon for the VOR, and slightly overlaps the TACAN,
	    // VOR-DME, and VORTAC icons.
	    float min = iconSize + 4.0 + (l->height / _metresPerPixel / 2.0);
	    if (labelOffset < min) {
		labelOffset = min;
	    }

	    // Convert to metres.
	    l->y -= labelOffset * _metresPerPixel;
	    l->lm.moveTo(l->x, l->y);
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

    geodPushMatrix(n->bounds.center, n->lat, n->lon); {
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
		_createTriangle(angularWidth, adfColour, adfColour, false);
	    }
	    glPopMatrix();
	}

	////////////////////
	// NDB label
	////////////////////
	if (_overlays.isVisible(Overlays::LABELS) && 
	    (radius > iconSize / 5.0)) {
	    Label *l; 
	    float pointSize = labelPointSize * _metresPerPixel;
	    if (fabs(radius - iconSize) < 0.01) {
		if (n->navsubtype != NDB_DME) {
		    l = _makeLabel("%N\n%F %I %M", n, pointSize, 0, 0);
		} else {
		    // EYE - and a different colour?
		    l = _makeLabel("%N\n%F (%f) %I %M", n, pointSize, 0, 0);
		}
	    } else if (radius > iconSize / 2.0) {
		if (n->navsubtype != NDB_DME) {
		    l = _makeLabel("%F %I", n, pointSize, 0, 0);
		} else {
		    // EYE - and a different colour?
		    l = _makeLabel("%F (%f) %I", n, pointSize, 0, 0);
		}
	    } else {
		l = _makeLabel("%I", n, pointSize, 0, 0);
	    }

	    // Place the bottom of the label 2 pixels above the icon.
	    float labelOffset = 
		radius + 2.0 + (l->height / _metresPerPixel / 2.0);
	    // Convert to metres.
	    l->y += labelOffset * _metresPerPixel;
	    l->lm.moveTo(l->x, l->y);
	    _drawLabel(l);

	    delete l;
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
	    ilsColour = vor1Colour;
	    live = true;
	} else if (n->freq == _p->nav2_freq) {
	    ilsLength = n->range;
	    ilsColour = vor2Colour;
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

    geodPushMatrix(n->bounds.center, n->lat, n->lon); {
	glPushMatrix(); {
	    glRotatef(-n->magvar, 0.0, 0.0, 1.0);
	    glScalef(ilsLength, ilsLength, ilsLength);
	    switch (n->navsubtype) {
	      case IGS:
	      case ILS_cat_I:
	      case ILS_cat_II:
	      case ILS_cat_III:
	      case LDA_GS:
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
	    fntRenderer& f = globals.fontRenderer;
	    f.setPointSize(labelPointSize * _metresPerPixel);

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

		float pointSize = f.getPointSize();
		// We draw the ILS in a single style, but it might be
		// better to alter it depending on the scale.
		_drawLabel("RWY %N\n%F %I %M", n, pointSize, offset, 0.0);

		// Now add a heading near the end.
		// EYE - magic number
		offset *= 1.75;
		LayoutManager lm;
		// EYE - magic number
		lm.setFont(globals.regularFont, pointSize * 1.25);
		lm.begin(offset, 0.0);
		// EYE - just record this once, when the navaid is loaded?
		double magvar = 0.0;
		const char *magTrue = "T";
		if (globals.magnetic) {
		    magvar = magneticVariation(n->lat, n->lon, n->elev);
		    magTrue = "";
		}
		int heading = normalizeHeading(rint(n->magvar - magvar), false);

		// EYE - we should add the glideslope too, if it has
		// one (eg, "284@3.00")

		// Degree symbol (EYE - magic number)
		const unsigned char degreeSymbol = 176; 
		globalString.printf("%d%C%s", heading, degreeSymbol, magTrue);
		lm.addText(globalString.str());
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

    geodPushMatrix(n->bounds.center, n->lat, n->lon); {
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

    geodPushMatrix(n->bounds.center, n->lat, n->lon); {
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
	    Label *l;
	    // EYE - need a shorthand for this point size business
	    globals.fontRenderer.setPointSize(labelPointSize * _metresPerPixel);
	    float pointSize = globals.fontRenderer.getPointSize();
	    if (fabs(radius - iconSize) < 0.01) {
		l = _makeLabel("%N\n%F %I %M", n, pointSize, 0.0, 0.0);
	    } else if (radius > iconSize / 2.0) {
		l = _makeLabel("%F %I", n, pointSize, 0.0, 0.0);
	    } else {
		l = _makeLabel("%I", n, pointSize, 0.0, 0.0);
	    }

	    // Place the bottom of the label 2 pixels above the icon.
	    // EYE - this seems to give us a bit more than 2 pixels.  Why?
	    float labelOffset = 
		(radius / sqrt(2.0)) + 2.0 + (l->height / _metresPerPixel / 2.0);
	    // Convert to metres.
	    l->y += labelOffset * _metresPerPixel;
	    l->lm.moveTo(l->x, l->y);
	    _drawLabel(l);

	    delete l;
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

    geodPushMatrix(n->bounds.center, n->lat, n->lon); {
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
	    Label *l = _makeLabel("%I", n, pointSize, 0.0, 0.0);

	    // Place the bottom of the label 2 pixels above the icon.
	    // EYE - this seems to give us a bit more than 2 pixels.  Why?
	    float labelOffset = 
		(radius / sqrt(2.0)) + 2.0 + (l->height / _metresPerPixel / 2.0);
	    // Convert to metres.
	    l->y += labelOffset * _metresPerPixel;
	    l->lm.moveTo(l->x, l->y);
	    _drawLabel(l);

	    delete l;
	}
    }
    geodPopMatrix();
}

// Returns width necessary to render the given string in morse code,
// at the current point size.
float NavaidsOverlay::_morseWidth(const string& id, float height)
{
    return _renderMorse(id, height, 0.0, 0.0, false);
}

// Either draws the given string in morse code at the given location
// (if render is true), OR returns the width necessary to draw it (if
// render is false.  In this case, x and y are ignored).  If drawn, we
// draw the morse stacked on top of each other to fill one line (which
// we assume to be pointSize high).  We assume that the current OpenGL
// units are metres.  The location (x, y) specifies the lower-left
// corner of the rendered morse text.
float NavaidsOverlay::_renderMorse(const string& id, float height,
				   float x, float y, bool render)
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
	glLineWidth(dotWidth / _metresPerPixel);
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
// Each line of text is centered.
Label *NavaidsOverlay::_makeLabel(const char *fmt, const NAV *n,
				  float labelPointSize,
				  float x, float y)
{
    // The label consists of a list of lines.  Each line consists of
    // intermixed text and morse.  The label, each line, and text and
    // morse unit has a width, height, and origin.
    Label *l = new Label;

    // Find out what our ascent is (_morseWidth and _renderMorse need
    // it).
    globals.fontRenderer.setPointSize(labelPointSize);
    atlasFntTexFont *f = (atlasFntTexFont *)globals.fontRenderer.getFont();
    float ascent = f->ascent() * labelPointSize;

    // Go through the format string once, using the layout manager to
    // calculate sizes.
    l->lm.setFont(globals.regularFont, labelPointSize);
    l->lm.begin(x, y);
    bool spec = false;
    l->morseChunk = -1;
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
		l->morseChunk = 
		    l->lm.addBox(_morseWidth(n->id, ascent), 0.0);
		l->id = n->id;
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
		if (n->navtype == NAV_NDB) {
		    line.appendf("%d", n->freq);
		} else {
		    int mhz, khz;
		    splitFrequency(n->freq, &mhz, &khz);
		    line.appendf("%d.%d", mhz, khz);
		}
		break;
	      case 'f':
		// DME frequency in an NDB-DME.
		assert((n->navtype == NAV_NDB) && (n->navsubtype == NDB_DME));
		int mhz, khz;
		splitFrequency(n->freq2, &mhz, &khz);
		line.appendf("%d.%d", mhz, khz);
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
	l->box = true;
	break;
      case NAV_DME:
	memcpy(l->colour, dme_colour, sizeof(float) * 4);
	l->box = true;
	break;
      case NAV_NDB:
	memcpy(l->colour, ndb_colour, sizeof(float) * 4);
	l->box = true;
	break;
      case NAV_ILS:
	memcpy(l->colour, ils_label_colour, sizeof(float) * 4);
	l->box = false;
	break;
      default:
	break;
    }

    // EYE - make this part of the label?
    const float border = labelPointSize / 5.0;

    l->x = x;
    l->y = y;
    l->lm.size(&(l->width), &(l->height));
    if (l->box) {
	l->width += 2.0 * border;
	l->height += 2.0 * border;
    }

    return l;
}

// Draws the label, centered at x, y.  The label will be drawn in the
// current font, at the given point size.  VORs, DMEs and NDBs are
// drawn with a box around the text and a translucent white background
// behind the text.
void NavaidsOverlay::_drawLabel(const char *fmt, const NAV *n,
				float labelPointSize,
				float x, float y)
{
    Label *l;

    l = _makeLabel(fmt, n, labelPointSize, x, y);
    _drawLabel(l);

    delete l;
}

void NavaidsOverlay::_drawLabel(Label *l)
{
    float x = l->x, y = l->y, width = l->width, height = l->height;

    ////////////////////
    // Draw background.
    ////////////////////
    if (l->box) {
	glBegin(GL_QUADS); {
	    glColor4f(1.0, 1.0, 1.0, 0.5);
	    glVertex2f(x - width / 2.0, y - height / 2.0);
	    glVertex2f(x + width / 2.0, y - height / 2.0);
	    glVertex2f(x + width / 2.0, y + height / 2.0);
	    glVertex2f(x - width / 2.0, y + height / 2.0);
	}
	glEnd();
    }

    ////////////////////
    // Render strings.
    ////////////////////
    glColor4fv(l->colour);
    l->lm.drawText();
    if (l->morseChunk >= 0) {
	atlasFntTexFont *f = (atlasFntTexFont *)globals.fontRenderer.getFont();
	float ascent = f->ascent() * globals.fontRenderer.getPointSize();
	float chunkX, chunkY;

	l->lm.nthChunk(l->morseChunk, &chunkX, &chunkY);
	_renderMorse(l->id, ascent, chunkX, chunkY);
    }

    ////////////////////
    // Bounding box.
    ////////////////////
    if (l->box) {
	glBegin(GL_LINE_LOOP); {
	    glVertex2f(x - width / 2.0, y - height / 2.0);
	    glVertex2f(x - width / 2.0, y + height / 2.0);
	    glVertex2f(x + width / 2.0, y + height / 2.0);
	    glVertex2f(x + width / 2.0, y - height / 2.0);
	}
	glEnd();
    }
}

bool NavaidsOverlay::notification(Notification::type n)
{
    if (n == Notification::Moved) {
	// Update our frustum from globals and record ourselves as
	// dirty.
	_frustum->move(globals.modelViewMatrix);
	setDirty();
    } else if (n == Notification::Zoomed) {
	// Update our frustum and scale from globals and record
	// ourselves as dirty.
	_frustum->zoom(globals.frustum.getLeft(),
		       globals.frustum.getRight(),
		       globals.frustum.getBot(),
		       globals.frustum.getTop(),
		       globals.frustum.getNear(),
		       globals.frustum.getFar());
	_metresPerPixel = globals.metresPerPixel;
	setDirty();
    } else if ((n == Notification::AircraftMoved) ||
	       (n == Notification::NewFlightTrack)) {
	_p = globals.currentPoint();

	// The aircraft moved, or we loaded a new flight track.  We
	// may have to update how "live" radios are drawn.
	if (!_p) {
	    // No flight data, so no flight track.  We'll probably need to
	    // redraw any radio "beams".
	    setDirty();
	    return true;
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

    return true;
}
