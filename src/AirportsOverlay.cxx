/*-------------------------------------------------------------------------
  AirportsOverlay.cxx

  Written by Brian Schack

  Copyright (C) 2008 Brian Schack

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

#include <cassert>

#include <simgear/misc/sg_path.hxx>
#include <simgear/math/sg_geodesy.hxx>

#include "AirportsOverlay.hxx"
#include "LayoutManager.hxx"
#include "Globals.hxx"
#include "misc.hxx"

using namespace std;

void airportLatLon(ARP *ap);
void runwayExtents(RWY *rwy, float elev);
void drawRunway(RWY *rwy, float border = 0.0);
// void drawIcon(const atlasSphere &bounds, float radius);

// EYE - move to policy?
// Border (magenta)
const float arp_uncontrolled_colour[4] = {0.439, 0.271, 0.420, 1.0}; 
// Border (teal)
const float arp_controlled_colour[4] = {0.000, 0.420, 0.624, 1.0}; 
// Runway (light grey)
const float arp_runway_colour[4] = {0.824, 0.863, 0.824, 1.0};

//////////////////////////////////////////////////////////////////////
// Searchable interface.
//////////////////////////////////////////////////////////////////////
double ARP::distanceSquared(const sgdVec3 from) const
{
    return sgdDistanceSquaredVec3(bounds.center, from);
}

// Returns our tokens, generating them if they haven't been already.
const std::vector<std::string>& ARP::tokens()
{
    if (_tokens.empty()) {
	// The id is a token.
	_tokens.push_back(id);

	// Tokenize the name.
	Searchable::tokenize(name, _tokens);

	// Add an "AIR:" token.
	_tokens.push_back("AIR:");
    }

    return _tokens;
}

// Returns our pretty string, generating it if it hasn't been already.
const std::string& ARP::asString()
{
    if (_str.empty()) {
	// Initialize our pretty string.
	globalString.printf("AIR: %s %s", id.c_str(), name.c_str());
	_str = globalString.str();
    }

    return _str;
}

AirportsOverlay::AirportsOverlay(Overlays& overlays):
    _overlays(overlays),
    _backgroundsDisplayList(0), _runwaysDisplayList(0), _labelsDisplayList(0),
    _beaconDL(0), _airportIconDL(0),
    _FGDirty(false), _BGDirty(false), _labelsDirty(false)
{
    _policy.rO = 15;
    _policy.rI = 11;
    _policy.rMin = 0;	// Airports disappear when zoomed out.
//     _policy.rMin = 1;
    _policy.rAMin = 2;
//     _policy.labelHeading = 0;
    _policy.labelHeading = 90;
    _policy.maxLabelDist = 100;
//     _policy.maxLabelDist = 0;

    _createBeacon();
    _createAirportIcon();

//     GLubyte *data;
//     int width, height, depth;
//     data = (GLubyte *)loadPng("FreqIcons.png", &width, &height, &depth);
//     assert(data != NULL);
//     glGenTextures(1, &_icons);
//     assert(_icons > 0);

//     glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//     glBindTexture(GL_TEXTURE_2D, _icons);

//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
// //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

//     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
// 		 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

//     gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, width, height,
// 		      GL_RGBA, GL_UNSIGNED_BYTE, data);

//     delete []data;

    // Create a culler and a frustum searcher for it.
    _culler = new Culler();
    _frustum = new Culler::FrustumSearch(*_culler);

    // Subscribe to moved and zoomed notifications.
    subscribe(Notification::Moved);
    subscribe(Notification::Zoomed);
}

AirportsOverlay::~AirportsOverlay()
{
    for (unsigned int i = 0; i < _airports.size(); i++) {
	ARP *ap = _airports[i];

	for (unsigned int j = 0; j < ap->rwys.size(); j++) {
	    delete ap->rwys[j];
	}
	ap->rwys.clear();

	ap->freqs.clear();

	delete ap;
    }

    _airports.clear();

    glDeleteLists(_backgroundsDisplayList, 1);
    glDeleteLists(_runwaysDisplayList, 1);
    glDeleteLists(_labelsDisplayList, 1);

    glDeleteLists(_beaconDL, 1);
    glDeleteLists(_airportIconDL, 1);

    delete _frustum;
    delete _culler;
}

void AirportsOverlay::setPolicy(const AirportPolicy& p)
{
    _policy = p;

    setDirty();
}

AirportPolicy AirportsOverlay::policy()
{
    return _policy;
}

bool AirportsOverlay::load(const string& fgDir)
{
    bool result = false;

    SGPath f(fgDir);
    f.append("Airports/apt.dat.gz");

    gzFile arp;
    char *line;

    arp = gzopen(f.c_str(), "rb");
    if (arp == NULL) {
	// EYE - we might want to throw an error instead.
	fprintf(stderr, "_loadAirports: Couldn't open \"%s\".\n", f.c_str());
	return false;
    } 

    // Check the file version.  We can handle version 810 files.
    int version = -1;
    gzGetLine(arp, &line);	// Windows/Mac header
    gzGetLine(arp, &line);	// Version
    sscanf(line, "%d", &version);
    if (version == 810) {
	// It looks like we have a valid file.
	result = _load810(arp);
    } else {
	// EYE - throw an error?
	fprintf(stderr, "_loadAirports: \"%s\": unknown version %d.\n", 
		f.c_str(), version);
	result = false;
    }

    gzclose(arp);

    return result;
}

// Creates a single predefined beacon and saves it in _beaconDL.
//
// A beacon is a five-pointed star of radius 1.0 with a white (well,
// nearly white) centre.
void AirportsOverlay::_createBeacon()
{
    if (_beaconDL == 0) {
	_beaconDL = glGenLists(1);
	assert(_beaconDL != 0);
    }
    glNewList(_beaconDL, GL_COMPILE); {
	glBegin(GL_TRIANGLES); {
	    for (int i = 0; i < 5; i ++) {
		float theta, x, y;

		theta = i * 72.0 * SG_DEGREES_TO_RADIANS;
		x = sin(theta);
		y = cos(theta);
		glVertex2f(x, y);

		theta = (i * 72.0 - 90.0) * SG_DEGREES_TO_RADIANS;
		x = sin(theta) * 0.25;
		y = cos(theta) * 0.25;
		glVertex2f(x, y);

		theta = (i * 72.0 + 90.0) * SG_DEGREES_TO_RADIANS;
		x = sin(theta) * 0.25;
		y = cos(theta) * 0.25;
		glVertex2f(x, y);
	    }
	}
	glEnd();

	glPushAttrib(GL_CURRENT_BIT); { // Because we change the colour
	    // Use the runway colour to colour the interior?  Why?  Pure
	    // white just seems too white.
	    glColor4fv(arp_runway_colour);
	    glBegin(GL_POLYGON); {
		for (int i = 0; i < 5; i ++) {
		    float theta, x, y;

		    theta = (i * 72.0 + 36.0) * SG_DEGREES_TO_RADIANS;
		    x = sin(theta) * 0.2;
		    y = cos(theta) * 0.2;
		    glVertex2f(x, y);
		}
	    }
	    glEnd();
	}
	glPopAttrib();
    }
    glEndList();
}

// Creates the airport icon and saves it in a display list.  The
// airport icon is nothing other than a simple filled circle.  I
// suppose I should just have a general routine to create these
// things, since I seem to do it fairly often.
//
// Note that although drawing a point is probably faster, OpenGL clips
// the point if the centre (but not the entire point) lies outside of
// the view.  This results in airports (when drawn as a filled point)
// "popping out" of view when the centre of the airport moves off the
// edge of the view.
void AirportsOverlay::_createAirportIcon()
{
    if (_airportIconDL == 0) {
	_airportIconDL = glGenLists(1);
	assert(_airportIconDL != 0);
    }
    glNewList(_airportIconDL, GL_COMPILE); {
	glBegin(GL_POLYGON); {
	    const int subdivision = 15; // 15-degree steps
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
    }
    glEndList();
}

bool AirportsOverlay::_load810(const gzFile& arp)
{
    char *line;

    ARP *ap = NULL;

    fprintf(stderr, "Loading airports ...\n");
    while (gzGetLine(arp, &line)) {
	int lineCode, offset;

	if (strcmp(line, "") == 0) {
	    // Blank line.
	    continue;
	} 

	if (strcmp(line, "99") == 0) {
	    // Last line.
	    break;
	}

	sscanf(line, "%d%n", &lineCode, &offset);
	line += offset;
	switch (lineCode) {
	  case 1:
	  case 16:
	  case 17:
	    {
		// The presence of a 1/16/17 means that we're starting a
		// new airport/seaport/heliport, and therefore ending an
		// old one.  Deal with the old airport first.
		if (ap != NULL) {
		    // Calculate the airport's center in lat, lon.
		    airportLatLon(ap);
		    _airports.push_back(ap);
		    // Add our airport text to the searcher object.
		    globals.searcher.add(ap);
		    // Add to our culler.
		    _frustum->culler().addObject(ap);

		    ap = NULL;
		}

		// EYE - add seaports and heliports!  (Note: the
		// classification of seaports is iffy - Pearl Harbor
		// is called an airport, even though it's in the
		// ocean, and Courchevel is called a seaport, even
		// though it's on top of a mountain).
		if (lineCode != 1) {
		    // We only handle airports (16 = seaport, 17 = heliport)
		    break;
		}

		// Create a new airport record.
		ap = new ARP;

		float elevation;
		int controlled;
		char code[5];	// EYE - safe?

		sscanf(line, "%f %d %*d %s %n", 
		       &elevation, &controlled, code, &offset);
		line += offset;

		ap->elev = elevation * SG_FEET_TO_METER;
		ap->controlled = (controlled == 1);
		ap->id = code;
		ap->name = line;
		// This will be set to true if we find a runway with
		// any kind of runway lighting.
		ap->lighting = false;
		// If set to true, then beaconLat and beaconLon
		// contain the location of the beacon.
		ap->beacon = false;
	    }

	    break;
	  case 10:
	    {
		if (ap == NULL) {
		    // If we're not working on an airport (ie, if this is
		    // a heliport), just continue.
		    break;
		}

		double lat, lon;
		char rwyid[4];	// EYE - safe?

		sscanf(line, "%lf %lf %s %n", &lat, &lon, rwyid, &offset);
		line += offset;

		// We ignore taxiways and helipads.
		if (strcmp(rwyid, "xxx") == 0) {
		    break;
		}
		if (strncmp(rwyid, "H", 1) == 0) {
		    break;
		}

		// Strip off trailing x's.
		int firstX = strcspn(rwyid, "x");
		if (firstX > 0) {
		    rwyid[firstX] = '\0';
		}
		assert(strlen(rwyid) <= 3);

		// Runway!
		RWY *rwy = new RWY;

		float heading, length, width;
		char *lighting;

		sscanf(line, "%f %f %*f %*f %f %n", 
		       &heading, &length, &width, &offset);
		lighting = line + offset;

		rwy->lat = lat;
		rwy->lon = lon;
		rwy->hdg = heading;
		rwy->length = length * SG_FEET_TO_METER;
		rwy->width  = width * SG_FEET_TO_METER;
		rwy->id = rwyid;
		ap->rwys.push_back(rwy);

		runwayExtents(rwy, ap->elev);
		ap->bounds.extend(&(rwy->bounds));

		// According to the FAA's "VFR Aeronautical Chart
		// Symbols", lighting codes on VFR maps refer to
		// runway lights (not approach lights).
		//
		// In apt.dat, visual approach, runway, and approach
		// lighting is given by a six-digit "number" (which we
		// treat as a string).  We're concerned with digits 2
		// and 5, which concern the runway itself.  If the
		// value is '1', there is no runway lighting.
		//
		// Note that the apt.dat database does not tell us
		// about lighting limitations, nor whether the
		// lighting is pilot-controlled.
		if ((lighting[1] != '1') || (lighting[4] != '1')) {
		    ap->lighting = true;
		}
	    }

	    break;
	  case 18: 
	    if (ap != NULL) {
		// Beacon
		double lat, lon;
		int beaconType;

		sscanf(line, "%lf %lf %d", &lat, &lon, &beaconType);
		if (beaconType != 0) {
		    ap->beacon = true;
		    ap->beaconLat = lat;
		    ap->beaconLon = lon;
		}
	    }
	    break;
	  case WEATHER:		// AWOS, ASOS, ATIS
	  case UNICOM:		// Unicom/CTAF (US), radio (UK)
	  case DEL:		// Clearance delivery
	  case GND:		// Ground
	  case TWR:		// Tower
	  case APP:		// Approach
	  case DEP:		// Departure
	      {
		  // ATC frequencies.
		  //
		  // Here's a sample, from LFPG (Paris Charles De
		  // Gaulle), which is a rather extreme case:
		  //
		  // 50 12712 DE GAULLE ATIS
		  // 53 11810 DE GAULLE TRAFFIC
		  // 53 11955 DE GAULLE TRAFFIC
		  // 53 12160 DE GAULLE GND
		  // 53 12167 DE GAULLE TRAFFIC
		  // 53 12177 DE GAULLE GND
		  // 53 12177 DE GAULLE GND
		  // 53 12180 DE GAULLE GND
		  // 53 12192 DE GAULLE TRAFFIC
		  // 53 12192 DE GAULLE TRAFFIC
		  // 53 12197 DE GAULLE GND
		  // 53 12197 DE GAULLE GND
		  // 54 11865 DE GAULLE TWR
		  // 54 11925 DE GAULLE TWR
		  // 54 12090 DE GAULLE TWR
		  // 54 12360 DE GAULLE TWR
		  // 54 12532 DE GAULLE TWR
		  //
		  // [...]
		  //
		  // There are several important things to note:
		  //
		  // (1) There many be several entries for a given
		  //     type.  For example, there is only one WEATHER
		  //     entry (type code 50), but 10 GND entries
		  //     (type code 53).
		  //
		  // (2) There may be several frequencies with the
		  //     same name in a given type.  For example,
		  //     there are 4 GND entries labelled "DE GAULLE
		  //     TRAFFIC", and 6 labelled "DE GAULLE GND".
		  //     They are not guaranteed to be grouped
		  //     together.  
		  //
		  //     When rendering these, we only print the label
		  //     once, and all frequencies with that label are
		  //     printed after the label.  This makes for a
		  //     less cluttered display:
		  //
		  //     DE GAULLE TRAFFIC 118.1 119.55 121.675 121.925
		  //
		  // (3) There may be duplicates.  For example, '53
		  //     12192 DE GAULLE TRAFFIC' is given twice.  The
		  //     duplicates should presumably be ignored.
		  //
		  // (4) Frequencies are given as integers, and should
		  //     be divided by 100.0 to give the true
		  //     frequency in MHz.  That is, 11810 is 118.1
		  //     MHz.  In addition, they are missing a
		  //     significant digit: 12192 really means 121.925
		  //     MHz, not 121.92 MHz (communications
		  //     frequencies have a 25 kHz spacing).  So, we
		  //     need to correct frequencies with end in the
		  //     digits '2' and '7'.
		  //
		  //     Internally, we also store the frequencies as
		  //     integers, but multiplied by 1000.0, not
		  //     100.0.  And we add a final '5' when
		  //     necessary.  So, we store 12192 as 121925, and
		  //     11810 as 118100.

	          // EYE - what should I do about multiple frequencies
	          // of one type?  A: Check San Jose (KSJC) - it has 2
	          // CT frequencies, and just lists them.  However,
	          // the VFR_Chart_Symbols.pdf file says that it lists
	          // the "primary frequency."

	          // Note: Unicom frequencies are written in bold
	          // italics, others in bold.  CT seems to be written
	          // slightly larger than the others.

	          // Note: Some airports, like Reid-Hillview, have
	          // CTAF and UNICOM.  CTAF is written with a circled
	          // C in front, the frequency bold and slightly
	          // enlarged (like CT), UNICOM in bold italics.

		  if (ap != NULL) {
		      int freq;

		      sscanf(line, "%d %n", &freq, &offset);
		      line += offset;

		      FrequencyMap& f = ap->freqs[(ATCCodeType)lineCode];
		      set<int>& freqs = f[line];
		      if ((freq % 10 == 2) || (freq % 10 == 7)) {
			  freqs.insert(freq * 10 + 5);
		      } else {
			  freqs.insert(freq * 10);
		      }
		  }
	      }
	    break;
	}
    }

    if (ap != NULL) {
	// Calculate the airport's center in lat, lon.
	airportLatLon(ap);
	_airports.push_back(ap);
	// Add our airport text to the searcher object.
	globals.searcher.add(ap);
	// Add to our culler.
	_frustum->culler().addObject(ap);
    }

    // EYE - will there ever be a false return?
    return true;
}

void AirportsOverlay::setDirty()
{
    _FGDirty = true;
    _BGDirty = true;
    _labelsDirty = true;
}

// Using the stencil buffer.
//
// To use the stencil buffer to "knock out" the center hole of an
// airport icon, do the following:
//
// (1) Ask for the stencil buffer (once, in main.cxx)
//
//     glutInitDisplayMode (... | GLUT_STENCIL);
//
// The following operations are done on each draw.  Note that they
// must be done *after* we ask for the new display list.
//
// (2) Enable and clear the buffer.
//
//     glEnable(GL_STENCIL_TEST);
//     glClearStencil(0x0);
//     glClear(GL_STENCIL_BUFFER_BIT);
//
// (3) If a hole is desired, enable blending, and draw the hole with
//     alpha = 0.0.  
//
//     Blending is necessary because we don't actually want to draw
//     anything to the colour buffer, we just want to affect the
//     stencil buffer.  It's important to remember that drawing with
//     the stencil functions still results in changes to the colour
//     buffer (and, presumably, the stencil buffer as well).  By the
//     time _drawAirports() is called, we've already drawn scenery
//     into the colour buffer, and we don't want to mess that up.
//     Theoretically, we could draw the "holes" before drawing
//     anything to the colour buffer, then clear the colour buffer and
//     draw scenery, but that would mean calling this routine, (but
//     only to draw the holes), long before anything else is done.
//     That would be ugly.
//
//     glEnable(GL_BLEND);
//     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//     glStencilFunc(GL_ALWAYS, 0x1, 0x1);
//     glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
//     glColor4f(0.0, 0.0, 0.0, 0.0);
//     drawIcon(ap->lat, ap->lon, ap->elev, rI / _metresPerPixel);
//     glDisable(GL_BLEND);
//
// (4) Draw the circle.  The hole will remain undrawn.
//
//     glStencilFunc(GL_NOTEQUAL, 0x1, 0x1);
//     glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
//     glColor4fv(arp_uncontrolled_colour);
//     drawIcon(ap->lat, ap->lon, ap->elev, rO / _metresPerPixel);
//
// (5) At the end of the draw routine, turn off the stencil test.
//
//     glDisable(GL_STENCIL_TEST);

void AirportsOverlay::drawBackgrounds()
{
    if (_BGDirty) {
	const int rO = _policy.rO;
	const int rI = _policy.rI;
	const int rMin = _policy.rMin;

	// Width of border (converted to metres) to draw around
	// runways
	const float border = (rO - rI) * _metresPerPixel;
	// Airport radius (pixels)
	int rA;

	const vector<Cullable *>& intersections = _frustum->intersections();

	if (_backgroundsDisplayList == 0) {
	    _backgroundsDisplayList = glGenLists(1);
	    assert(_backgroundsDisplayList != 0);
	}
	glNewList(_backgroundsDisplayList, GL_COMPILE);

	// Background.  We draw the background as an outline if the
	// airport is larger than the outer circle size.  If the
	// airport is greater than a minimum size, we draw a scaled
	// circle.  If the airport is less than the minimum size, and
	// the minimum is greater than 0, we draw circle of the size
	// given by the minimum.

	// EYE - according to 
	//
	//       http://webusers.warwick.net/~u1007204/gs/chartsym.html
	//
	//       airports are circled if the longest runway is less
	//       than 8096 feet.

	for (unsigned int i = 0; i < intersections.size(); i++) {
	    ARP *ap = dynamic_cast<ARP *>(intersections[i]);
	    assert(ap);

	    const float *colour;
	    if (ap->controlled) {
		colour = arp_controlled_colour;
	    } else {
		colour = arp_uncontrolled_colour;
	    }

	    rA = ap->bounds.radius / _metresPerPixel; // pixels
	    if (rA > rI) {
		glColor4fv(colour);
		for (unsigned int j = 0; j < ap->rwys.size(); j++) {
		    drawRunway(ap->rwys[j], border);
		}
	    } else if (rA > rMin) {
		glColor4fv(colour);
		_drawIcon(ap, rA * rO / rI);
	    } else if (rMin > 0) {
		glColor4fv(colour);
		_drawIcon(ap, rMin);
	    }
	}

	glEndList();

	_BGDirty = false;
    }

    glCallList(_backgroundsDisplayList);
}

// Draws the runways and the airport beacon.
void AirportsOverlay::drawForegrounds()
{
    if (_FGDirty) {
	const int rI = _policy.rI;
	const int rAMin = _policy.rAMin;

	// Airport radius (pixels)
	int rA;

	const vector<Cullable *>& intersections = _frustum->intersections();

	if (_runwaysDisplayList == 0) {
	    _runwaysDisplayList = glGenLists(1);
	    assert(_runwaysDisplayList != 0);
	}
	glNewList(_runwaysDisplayList, GL_COMPILE);

	// Foreground (runways).  We draw the runways only if the
	// airport is bigger than the airport minimum.
	glColor4fv(arp_runway_colour);
	for (unsigned int i = 0; i < intersections.size(); i++) {
	    ARP *ap = dynamic_cast<ARP *>(intersections[i]);
	    if (!ap) {
		continue;
	    }

	    rA = ap->bounds.radius / _metresPerPixel; // pixels
	    if (rA > rAMin) {

		for (unsigned int j = 0; j < ap->rwys.size(); j++) {
		    RWY *rwy = ap->rwys[j];

		    // This is a bit of a hack to ensure that runways
		    // are never too skinny to show up clearly.  If
		    // the runway width is less than the airport
		    // minimum, we just set the width (temporarily) to
		    // the airport minimum, draw the runway, then set
		    // it back.
		    float width = rwy->width;
		    if (width < (rAMin * _metresPerPixel)) {
			rwy->width = (rAMin * _metresPerPixel);
		    }

		    drawRunway(rwy);

		    rwy->width = width;
		}
	    }
	}

	// Airport beacons.
	for (unsigned int i = 0; i < intersections.size(); i++) {
	    ARP *ap = dynamic_cast<ARP *>(intersections[i]);
	    if (!ap) {
		continue;
	    }
	    rA = ap->bounds.radius / _metresPerPixel; // pixels

	    // Only draw the beacon if it has one and if the airport
	    // is being drawn as an airport (outlined runways).
	    if ((ap->beacon) && (rA > rI)) {
		if (ap->controlled) {
		    glColor4fv(arp_controlled_colour);
		} else {
		    glColor4fv(arp_uncontrolled_colour);
		}

		// EYE - precompute this?
		sgdVec3 location;
		sgGeodToCart(ap->beaconLat * SGD_DEGREES_TO_RADIANS, 
			     ap->beaconLon * SGD_DEGREES_TO_RADIANS, 
			     0.0, location);
		glPushMatrix(); {
		    glTranslated(location[0], location[1], location[2]);
		    glRotatef(ap->beaconLon + 90.0, 0.0, 0.0, 1.0);
		    glRotatef(90.0 - ap->beaconLat, 1.0, 0.0, 0.0);
		    // EYE - magic number.  Probably we should scale
		    // this somewhat (start by drawing it small, then
		    // draw it larger as we zoom in, up to a maximum).
		    glScalef(_metresPerPixel * 10.0,
			     _metresPerPixel * 10.0,
			     _metresPerPixel * 10.0);

		    glCallList(_beaconDL);
		};
		glPopMatrix();
	    }
	}

	glEndList();

	_FGDirty = false;
    }

    glCallList(_runwaysDisplayList);
}

void AirportsOverlay::drawLabels()
{
    if (_labelsDirty) {
	const int rI = _policy.rI;
	const int rMin = _policy.rMin;

	// Airport radius (pixels)
	int rA;

	const vector<Cullable *>& intersections = _frustum->intersections();

	if (_labelsDisplayList == 0) {
	    _labelsDisplayList = glGenLists(1);
	    assert(_labelsDisplayList != 0);
	}
	glNewList(_labelsDisplayList, GL_COMPILE);

	////////////
	// Labels //
	////////////

	// Label the runways.
	glColor4f(0.0, 0.0, 0.0, 1.0);
	globals.fontRenderer.begin();
	for (unsigned int i = 0; i < intersections.size(); i++) {
	    ARP *ap = dynamic_cast<ARP *>(intersections[i]);
	    if (!ap) {
		continue;
	    }

	    rA = ap->bounds.radius / _metresPerPixel; // pixels
	    if (rA > rI) {
		for (unsigned int j = 0; j < ap->rwys.size(); j++) {
		    RWY *rwy = ap->rwys[j];

		    _labelRunway(rwy);
		}
	    }
	}
	globals.fontRenderer.end();

	// Label the airports.
	for (unsigned int i = 0; i < intersections.size(); i++) {
	    ARP *ap = dynamic_cast<ARP *>(intersections[i]);
	    if (!ap) {
		continue;
	    }

	    // We don't label airports unless they're at least the
	    // minimum size.
	    rA = ap->bounds.radius / _metresPerPixel;
	    if (rA > rMin) {
		_labelAirport(ap, rA);
	    }
	}	
	glEndList();

	_labelsDirty = false;
    }

    glCallList(_labelsDisplayList);
}

// Calculates the airport's center in lat, lon from its bounds.
void airportLatLon(ARP *ap)
{
    double lat, lon, alt;
    sgdVec3 c;
    sgdSetVec3(c,
	       ap->bounds.center[0], 
	       ap->bounds.center[1], 
	       ap->bounds.center[2]);
    sgCartToGeod(c, &lat, &lon, &alt);
    ap->lat = lat * SGD_RADIANS_TO_DEGREES;
    ap->lon = lon * SGD_RADIANS_TO_DEGREES;
}

// Given a runway with a valid lat, lon, and heading (in degrees), and
// a valid length and width (in metres), sets its bounds, "ahead"
// vector (a normalized vector pointing along the runway in the given
// heading), "aside" vector (a normalized vector pointing across the
// runway, 90 degrees clockwise from the ahead vector), and its
// "above" vector (its normal vector).
void runwayExtents(RWY *rwy, float elev)
{
    // In PLIB, "up" (the direction of our normal) is along the
    // positive y-axis.  What we call "ahead" (looking along our
    // heading, where the runway points), is along the positive
    // z-axis), and what we call "aside" (looking across the runway,
    // 90 degrees from our heading), is along the negative x-axis.

    // EYE - am I thinking about this right?  Is it what PLIB
    // "thinks", or what I think?
    sgdSetVec3(rwy->ahead, 0.0, 0.0, 1.0);
    sgdSetVec3(rwy->aside, -1.0, 0.0, 0.0);
    sgdSetVec3(rwy->above, 0.0, 1.0, 0.0);

    sgdMat4 rot;
    double heading = rwy->lon - 90.0;
    double pitch = rwy->lat;
    double roll = -rwy->hdg;

    // This version has us in our standard orientation, which means 0
    // lat, 0 lon, and a heading of 0 (north).
    // EYE - untested
//     sgdSetVec3(rwy->ahead, 0.0, 0.0, 1.0);
//     sgdSetVec3(rwy->aside, 0.0, 1.0, 0.0);
//     sgdSetVec3(rwy->above, 1.0, 0.0, 0.0);

//     sgdMat4 rot;
//     double heading = rwy->lon;
//     double pitch = -rwy->hdg;
//     double roll = -rwy->lat;

    // This version is in the standard PLIB orientation, facing out
    // along the y axis, the x axis right, and the z axis up.
    // EYE - untested
//     sgdSetVec3(rwy->ahead, 0.0, 0.0, 1.0);
//     sgdSetVec3(rwy->aside, 1.0, 0.0, 0.0);
//     sgdSetVec3(rwy->above, 0.0, 1.0, 0.0);

//     sgdMat4 rot;
//     double heading = 90.0 - rwy->lon;
//     double pitch = rwy->lat;
//     double roll = rwy->hdg;

    sgdMakeRotMat4(rot, heading, pitch, roll);

    sgdXformVec3(rwy->ahead, rot);
    sgdXformVec3(rwy->aside, rot);
    sgdXformVec3(rwy->above, rot);

    // Calculate our bounding sphere.
    sgdVec3 center;
    atlasGeodToCart(rwy->lat, rwy->lon, elev, center);
    sgdCopyVec3(rwy->bounds.center, center);

    sgdMat4 mat;
    sgdMakeTransMat4(mat, rwy->bounds.center);
    sgdPreMultMat4(mat, rot);

    sgdVec3 ll = {rwy->width / 2,  0.0, -rwy->length / 2};
    sgdVec3 lr = {-rwy->width / 2, 0.0, -rwy->length / 2};
    sgdVec3 ul = {rwy->width / 2, 0.0, rwy->length / 2};
    sgdVec3 ur = {-rwy->width / 2, 0.0, rwy->length / 2};

    sgdXformPnt3(ul, mat);
    sgdXformPnt3(lr, mat);
    sgdXformPnt3(ll, mat);
    sgdXformPnt3(ur, mat);
    rwy->bounds.extend(ul);
    rwy->bounds.extend(ll);
    rwy->bounds.extend(ll);
    rwy->bounds.extend(ur);
}

// EYE - makes no allowance for the curvature of the earth; assumes
// that we're ignoring depth buffer; ignores great circleness; assumes
// colour has been set.
void drawRunway(RWY *rwy, const float border)
{
    // Here's how to do the same thing via OpenGL calls.
//     SGVec3<double> center;
//     SGGeodesy::SGGeodToCart(SGGeod::fromDegM(lon, lat, elev), center);

//     glPushMatrix(); {
// 	glTranslated(center.x(), center.y(), center.z());
// 	glRotatef(lon, 0.0, 0.0, 1.0);
// 	glRotatef(-lat, 0.0, 1.0, 0.0);
// 	glRotatef(-heading, 1.0, 0.0, 0.0);

// 	// Normal always points straight up.
// 	float normal[3];
// 	normal[0] = 1.0;
// 	normal[1] = 0.0;
// 	normal[2] = 0.0;

// 	glBegin(GL_QUADS); {
// 	    glNormal3fv(normal);
// 	    glVertex3fv(ll);
// 	    glVertex3fv(lr);
// 	    glVertex3fv(ur);
// 	    glVertex3fv(ul);
// 	}
// 	glEnd();
//     }
//     glPopMatrix();

    glBegin(GL_QUADS); {
	sgdVec3 ahead, aside;
	sgdScaleVec3(ahead, rwy->ahead, (rwy->length / 2.0) + border);
	sgdScaleVec3(aside, rwy->aside, (rwy->width / 2.0) + border);

	sgdVec3 location;
	sgGeodToCart(rwy->lat * SGD_DEGREES_TO_RADIANS, 
		     rwy->lon * SGD_DEGREES_TO_RADIANS, 
		     0.0, location);
	sgdVec3 l, u;
	sgdVec3 ll, lr, ur, ul;
	sgdSubVec3(l, location, ahead);
	sgdSubVec3(ll, l, aside);
	sgdAddVec3(lr, l, aside);
	sgdAddVec3(u, location, ahead);
	sgdSubVec3(ul, u, aside);
	sgdAddVec3(ur, u, aside);

	glNormal3dv(rwy->above);
	glVertex3dv(ll);
	glVertex3dv(lr);
	glVertex3dv(ur);
	glVertex3dv(ul);
    }
    glEnd();
}

void AirportsOverlay::_drawIcon(ARP *ap, float radius)
{
    // Radius is passed in in pixels; we convert it to metres
    radius = radius * _metresPerPixel;
    glPushMatrix(); {
	glTranslated(ap->bounds.center[0],
		     ap->bounds.center[1],
		     ap->bounds.center[2]);
	glRotatef(ap->lon + 90.0, 0.0, 0.0, 1.0);
	glRotatef(90.0 - ap->lat, 1.0, 0.0, 0.0);
	glScalef(radius, radius, radius);

	glCallList(_airportIconDL);
    }
    glPopMatrix();
}

// Labels a single airport.  Because we don't label many airports at a
// time (if they're too small, we drop the labels), we don't need to
// be terribly efficient.
//
// An airport label can contain its name, id, elevation, maximum
// runway length, and ATC frequencies.
void AirportsOverlay::_labelAirport(ARP *ap, int rA)
{
    const int rO = _policy.rO;
    const int rI = _policy.rI;
    fntRenderer& f = globals.fontRenderer;

    const int labelHeading = _policy.labelHeading;
    const float maxLabelDist = _policy.maxLabelDist;

    // 		float scale = 2.0 * rA / rO;
    float scale = 1.0 * rA / rO;
    if (scale > 1.0) {
	scale = 1.0;
    }
    // EYE - magic number
//     const float pointSize = _metresPerPixel * 24.0 * scale;
    const float pointSize = _metresPerPixel * 18.0 * scale;
    const float medium = pointSize * 0.75;
    const float small = pointSize * 0.5;
    const float tiny = pointSize * 0.4;

    // EYE - magic number (pointSize is in metres)
    if (pointSize / _metresPerPixel < 5.0) {
	// Don't do anything if the label would be too small.
	return;
    }

    const float *colour;
    if (ap->controlled) {
	colour = arp_controlled_colour;
    } else {
	colour = arp_uncontrolled_colour;
    }
    glColor4fv(colour);

    // Place airport 2 pixels outside of the outer circle,
    // but no more than maxLabelDist pixels from the
    // center.
    double distance = (rA * rO / rI) + 2;
    if ((maxLabelDist != 0) && (distance > maxLabelDist)) {
	distance = maxLabelDist;
    }
    distance *= _metresPerPixel;

    // Place label at a heading of labelHeading, at the
    // calculated distance, from the airport center.
    double lat, lon, az;
    sgdVec3 location;
    geo_direct_wgs_84(ap->lat, ap->lon, 
		      labelHeading, distance, &lat, &lon, &az);
    sgGeodToCart(lat * SGD_DEGREES_TO_RADIANS, 
		 lon * SGD_DEGREES_TO_RADIANS, 0.0, location);

    f.setPointSize(pointSize);

    // EYE - needs float, not double
    glPushMatrix(); {
	glTranslated(location[0], location[1], location[2]);
	glRotatef(lon + 90.0, 0.0, 0.0, 1.0);
	glRotatef(90.0 - lat, 1.0, 0.0, 0.0);

	// Generate label.
	LayoutManager lm;
	lm.begin();
	lm.setFont(f, pointSize);

	globalString.printf("%s (%s)", ap->name.c_str(), ap->id.c_str());
	lm.addText(globalString.str());

	// Frequencies
	lm.newline();

	// Only do frequencies if we're very close.
	// EYE - magic number
	if (_metresPerPixel < 20.0) {
	    map<ATCCodeType, FrequencyMap>::iterator fMap;
	    for (fMap = ap->freqs.begin(); fMap != ap->freqs.end(); fMap++) {
		FrequencyMap& bar = fMap->second;

		// The frequency map is a map from strings (like, "ATLANTA
		// APP") to a set of frequencies (118350, 126900, 127250,
		// 127900).  All of the name/frequency set pairs are
		// members of the same ATCCodeType (eg, APP).
		FrequencyMap::iterator freq;
		for (freq = bar.begin(); freq != bar.end(); freq++) {
		    set<int>& freqs = freq->second;

		    // Separate named groups of frequencies with two spaces.
		    if (freq != bar.begin()) {
			lm.addText("  ");
		    }

		    // First, the frequency name.
		    lm.setFont(f, tiny);
		    lm.addText(freq->first);

		    // Now, the frequencies themselves.
		    globalString.clear();
		    globals.bold();
		    lm.setFont(f, small);
		    set<int>::iterator j;
		    for (j = freqs.begin(); j != freqs.end(); j++) {
			int mhz, khz;
			splitFrequency(*j, &mhz, &khz);
			globalString.appendf(" %d.%d", mhz, khz);
		    }
		    lm.addText(globalString.str());
		}

		lm.newline();
	    }

	    // Airport elevation
	    lm.newline();
	    // Set in italics.
	    lm.setFont(f, medium, 0.25);
	    // We need to ensure that the number is at least 2 digits
	    // long (ie, '6' must be written '06').
	    globalString.printf("%02.0f  ", ap->elev * SG_METER_TO_FEET);
	    lm.addText(globalString.str());

	    // Runway lighting.
	    lm.setFont(f, medium);
	    if (ap->lighting) {
		lm.addText("L  ");
	    } else {
		lm.addText("-  ");
	    }

	    // Runway length
	    float maxRwy = 0;
	    for (unsigned int i = 0; i < ap->rwys.size(); i++) {
		if (ap->rwys[i]->length > maxRwy) {
		    maxRwy = ap->rwys[i]->length;
		}
	    }
	    // According to the FAA's "IFR Aeronautical Chart Symbols"
	    // document, on IFR low altitude charts, the runway length is
	    // given to the nearest 100 feet, with 70 feet as the dividing
	    // point.  It's probably different in different countries, and
	    // it may be different on VFR charts.  For now, though, we'll
	    // use the '70 rule'.
	    globalString.printf("%.0f",
				round((maxRwy * SG_METER_TO_FEET - 20) / 100));
	    lm.addText(globalString.str());
	}

	lm.end();

	float width, height;
	lm.size(&width, &height);
	lm.moveTo(width / 2.0, 0.0);
	lm.drawText();
    }
    glPopMatrix();
}

// Given the label for one end of a runway, generates the label for
// the other end.  The otherEnd variable must be at least 4 characters
// long.
void otherEnd(const char *thisEnd, char *otherEnd)
{
    int hdg;
    unsigned int length;
    sscanf(thisEnd, "%d%n", &hdg, &length);
    assert((length == strlen(thisEnd)) || (length = strlen(thisEnd) - 1));
    hdg = (hdg + 18) % 36;
    if (hdg == 0) {
	hdg = 36;
    }

    // Handle trailing character (if it exists).  If the character is
    // 'L' or 'R', swap it for 'R' and 'L' respectively.  Otherwise
    // leave it alone (presumably it's 'C').  If it doesn't exist, set
    // it to '\0'.
    char lr = '\0';
    if (length < strlen(thisEnd)) {
	if (thisEnd[length] == 'R') {
	    lr = 'L';
	} else if (thisEnd[length] == 'L') {
	    lr = 'R';
	} else {
	    lr = thisEnd[length];
	}
    }
    sprintf(otherEnd, "%02d%c", hdg, lr);
}

// Labels a single runway (at both ends).  Because we only label a
// handful of airports at any one time, we don't need to be especially
// careful about efficiency.
void AirportsOverlay::_labelRunway(RWY *rwy)
{
    // EYE - magic numbers - add to airport policy
    static const float maxHeight = 24.0; // Pixels
    // EYE - 10.0 means it's pretty squished at KSFO
//     static const float multiple = 10.0;
    static const float multiple = 4.0;

    // Calculate size (in metres) of text.
    float pointSize;
    if ((rwy->width * multiple) < (maxHeight * _metresPerPixel)) {
	pointSize = rwy->width * multiple;
    } else if (rwy->width > (maxHeight * _metresPerPixel)) {
	pointSize = rwy->width;
    } else {
	pointSize = maxHeight * _metresPerPixel;
    }
    // If the text is too small, don't bother drawing anything.
    // EYE - magic number
    if (pointSize / _metresPerPixel < 10.0) {
	return;
    }
    globals.fontRenderer.setPointSize(pointSize);

    // Label "main" end.
    _labelRunwayEnd(rwy->id.c_str(), 0.0, rwy);

    // Label "other" end.
    // EYE - precompute this, and precompute it more elegantly!
    // EYE - use a string?
    char label[4];
    otherEnd(rwy->id.c_str(), label);
    _labelRunwayEnd(label, 180.0, rwy);
}

// Writes the given label at the end of the given rwy, where 'end' is
// defined by the given heading.  The only reasonable values are 0.0
// (the "main" end), and 180.0 (the "other" end).
void AirportsOverlay::_labelRunwayEnd(const char *str, float hdg, RWY *rwy)
{
    fntRenderer& f = globals.fontRenderer;
    float pointSize = f.getPointSize();
    float left, right, bottom, top;
    f.getFont()->getBBox(str, pointSize, 0.0, &left, &right, &bottom, &top);
    glPushMatrix(); {
	// EYE - fix this!
	sgdVec3 location;
	sgGeodToCart(rwy->lat * SGD_DEGREES_TO_RADIANS, 
		     rwy->lon * SGD_DEGREES_TO_RADIANS, 
		     0.0, location);
	glTranslated(location[0], location[1], location[2]);
	glRotatef(rwy->lon + 90.0, 0.0, 0.0, 1.0);
	glRotatef(90.0 - rwy->lat, 1.0, 0.0, 0.0);
	glRotatef(-(rwy->hdg + hdg), 0.0, 0.0, 1.0);
	// EYE - magic number
	glScalef(0.5, 1.0, 1.0); // Squish characters together

	f.start3f(0.0 - (left + right) / 2.0,
		  -((top - bottom) + rwy->length / 2.0 + rwy->width), 
		   0.0);
	f.puts(str);
    }
    glPopMatrix();
}

// Called when somebody posts a notification that we've subscribed to.
bool AirportsOverlay::notification(Notification::type n)
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
    } else {
	assert(false);
    }

    return true;
}
