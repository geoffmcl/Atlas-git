/*-------------------------------------------------------------------------
  AirportsOverlay.cxx

  Written by Brian Schack

  Copyright (C) 2008 - 2014 Brian Schack

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
#include "AirportsOverlay.hxx"

// C++ system include files
#include <cassert>

// Other libraries' include files
#include <simgear/misc/sg_path.hxx>
#include <simgear/math/sg_geodesy.hxx>
#ifdef _MSC_VER
#include <simgear/math/SGMisc.hxx>
#define ROUND(a) SGMisc<double>::round(a)
#else
#define ROUND(d)    round(d)
#endif

// Our project's include files
#include "AtlasWindow.hxx"
#include "Globals.hxx"
#include "LayoutManager.hxx"
#include "NavData.hxx"

using namespace std;

// Draw a runway.  If sideBorder and/or endBorder are > 0.0, then the
// runway will be drawn bigger by that amount (both are in metres).
void drawRunway(RWY *rwy, 
		const float sideBorder = 0.0, 
		const float endBorder = 0.0);

// EYE - move to policy?
// Border (magenta)
const float arp_uncontrolled_colour[4] = {0.439, 0.271, 0.420, 1.0}; 
// Border (teal)
const float arp_controlled_colour[4] = {0.000, 0.420, 0.624, 1.0}; 
// Runway (light grey)
const float arp_runway_colour[4] = {0.824, 0.863, 0.824, 1.0};

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

    // Subscribe to moved and zoomed notifications.
    subscribe(Notification::Moved);
    subscribe(Notification::Zoomed);
}

AirportsOverlay::~AirportsOverlay()
{
    glDeleteLists(_backgroundsDisplayList, 1);
    glDeleteLists(_runwaysDisplayList, 1);
    glDeleteLists(_labelsDisplayList, 1);

    glDeleteLists(_beaconDL, 1);
    glDeleteLists(_airportIconDL, 1);
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

	// Push GL_CURRENT, because we change the colour.
	glPushAttrib(GL_CURRENT_BIT); {
	    // Use the runway colour to colour the interior.  Why?
	    // Pure white just seems too white.
	    glColor4fv(arp_runway_colour);
	    glBegin(GL_POLYGON); {
		// Draw the beacon interior counterclockwise.
		for (int i = 4; i >= 0; i--) {
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
	    // Draw the circle (in a counterclockwise direction).
	    for (int i = 360; i > 0; i -= subdivision) {
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
//     drawIcon(ap->latitude(), ap->longitude(), ap->elevation(), 
// 	        rI / _metresPerPixel);
//     glDisable(GL_BLEND);
//
// (4) Draw the circle.  The hole will remain undrawn.
//
//     glStencilFunc(GL_NOTEQUAL, 0x1, 0x1);
//     glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
//     glColor4fv(arp_uncontrolled_colour);
//     drawIcon(ap->latitude(), ap->longitude(), ap->elev, 
//              rO / _metresPerPixel);
//
// (5) At the end of the draw routine, turn off the stencil test.
//
//     glDisable(GL_STENCIL_TEST);

void AirportsOverlay::drawBackgrounds(NavData *navData)
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

	const vector<Cullable *>& intersections = 
	    navData->hits(NavData::AIRPORTS);

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
	    if (ap->controlled()) {
		colour = arp_controlled_colour;
	    } else {
		colour = arp_uncontrolled_colour;
	    }

	    rA = ap->bounds().radius / _metresPerPixel; // pixels
	    if (rA > rI) {
		glColor4fv(colour);
		for (unsigned int j = 0; j < ap->rwys().size(); j++) {
		    drawRunway(ap->rwys()[j], border, border);
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
void AirportsOverlay::drawForegrounds(NavData *navData)
{
    if (_FGDirty) {
	const int rI = _policy.rI;
	const int rAMin = _policy.rAMin;

	// Airport radius (pixels)
	int rA;

	const vector<Cullable *>& intersections = 
	    navData->hits(NavData::AIRPORTS);

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

	    rA = ap->bounds().radius / _metresPerPixel; // pixels
	    if (rA > rAMin) {

		for (unsigned int j = 0; j < ap->rwys().size(); j++) {
		    RWY *rwy = ap->rwys()[j];

		    // This is to ensure that runways are never too
		    // skinny to show up clearly - runways are never
		    // drawn skinnier than rAMin.
		    float border = 
		    	((rAMin * _metresPerPixel) - rwy->width()) / 2.0;
		    if (border < 0.0) {
		    	border = 0.0;
		    }

		    drawRunway(rwy, border);
		}
	    }
	}

	// Airport beacons.
	for (unsigned int i = 0; i < intersections.size(); i++) {
	    ARP *ap = dynamic_cast<ARP *>(intersections[i]);
	    if (!ap) {
		continue;
	    }
	    rA = ap->bounds().radius / _metresPerPixel; // pixels

	    // Only draw the beacon if it has one and if the airport
	    // is being drawn as an airport (outlined runways).
	    if ((ap->beacon()) && (rA > rI)) {
		if (ap->controlled()) {
		    glColor4fv(arp_controlled_colour);
		} else {
		    glColor4fv(arp_uncontrolled_colour);
		}

		// EYE - precompute this?
		geodPushMatrix(ap->beaconLat(), ap->beaconLon()); {
		    // EYE - magic number.  Probably we should scale
		    // this somewhat (start by drawing it small, then
		    // draw it larger as we zoom in, up to a maximum).
		    glScalef(_metresPerPixel * 10.0,
			     _metresPerPixel * 10.0,
			     _metresPerPixel * 10.0);

		    glCallList(_beaconDL);
		};
		geodPopMatrix();
	    }
	}

	glEndList();

	_FGDirty = false;
    }

    glCallList(_runwaysDisplayList);
}

void AirportsOverlay::drawLabels(NavData *navData)
{
    if (_labelsDirty) {
	const int rI = _policy.rI;
	const int rMin = _policy.rMin;

	// Airport radius (pixels)
	int rA;

	const vector<Cullable *>& intersections = 
	    navData->hits(NavData::AIRPORTS);

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
	// EYE - does removing the begin()/end() pair affect things?
	// globals.fontRenderer.begin();
	for (unsigned int i = 0; i < intersections.size(); i++) {
	    ARP *ap = dynamic_cast<ARP *>(intersections[i]);
	    if (!ap) {
		continue;
	    }

	    rA = ap->bounds().radius / _metresPerPixel; // pixels
	    if (rA > rI) {
		for (unsigned int j = 0; j < ap->rwys().size(); j++) {
		    RWY *rwy = ap->rwys()[j];

		    _labelRunway(rwy);
		}
	    }
	}
	// globals.fontRenderer.end();

	// Label the airports.
	for (unsigned int i = 0; i < intersections.size(); i++) {
	    ARP *ap = dynamic_cast<ARP *>(intersections[i]);
	    if (!ap) {
		continue;
	    }

	    // We don't label airports unless they're at least the
	    // minimum size.
	    rA = ap->bounds().radius / _metresPerPixel;
	    if (rA > rMin) {
		_labelAirport(ap, rA);
	    }
	}	
	glEndList();

	_labelsDirty = false;
    }

    glCallList(_labelsDisplayList);
}

// EYE - makes no allowance for the curvature of the earth; assumes
// that we're ignoring depth buffer; ignores great circleness; assumes
// colour has been set.
void drawRunway(RWY *rwy, const float sideBorder, const float endBorder)
{
    // EYE - create an AtlasCoord version of geodPushMatrix?
    geodPushMatrix(rwy->centre(), rwy->lat(), rwy->lon(), rwy->hdg()); {
	glBegin(GL_QUADS); {
	    // Normal always points straight up.
	    glNormal3f(1.0, 0.0, 0.0);

	    float w = rwy->width() / 2.0 + sideBorder;
	    float l = rwy->length() / 2.0 + endBorder;
	    glVertex2f(-w, -l); // ll
	    glVertex2f(w, -l);	// lr
	    glVertex2f(w, l);	// ur
	    glVertex2f(-w, l);	// ul
	}
	glEnd();
    }
    geodPopMatrix();
}

void AirportsOverlay::_drawIcon(ARP *ap, float radius)
{
    // Radius is passed in in pixels; we convert it to metres
    radius = radius * _metresPerPixel;
    geodPushMatrix(ap->bounds().center, ap->latitude(), ap->longitude()); {
	glScalef(radius, radius, radius);
	glCallList(_airportIconDL);
    }
    geodPopMatrix();
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

    const float maxLabelDist = _policy.maxLabelDist;

    // 		float scale = 2.0 * rA / rO;
    float scale = 1.0 * rA / rO;
    if (scale > 1.0) {
	scale = 1.0;
    }
    // EYE - magic number
//     const float pointSize = _metresPerPixel * 24.0 * scale;
    const float pointSize = _metresPerPixel * 18.0 * scale;
    const float mediumFontSize = pointSize * 0.75;
    const float smallFontSize = pointSize * 0.5;
    const float tinyFontSize = pointSize * 0.4;

    // EYE - magic number (pointSize is in metres)
    if (pointSize / _metresPerPixel < 5.0) {
	// Don't do anything if the label would be too small.
	return;
    }

    const float *colour;
    if (ap->controlled()) {
	colour = arp_controlled_colour;
    } else {
	colour = arp_uncontrolled_colour;
    }
    glColor4fv(colour);

    // Generate label.
    LayoutManager lm;
    lm.begin();
    lm.setFont(_overlays.regularFont(), pointSize);

    globals.str.printf("%s (%s)", ap->name(), ap->code());
    lm.addText(globals.str.str());

    // Frequencies
    lm.newline();

    // Only do frequencies if we're very close.
    // EYE - magic number
    if (_metresPerPixel < 20.0) {
	map<ATCCodeType, FrequencyMap>::const_iterator fMap;
	for (fMap = ap->freqs().begin(); fMap != ap->freqs().end(); fMap++) {
	    const FrequencyMap& bar = fMap->second;

	    // The frequency map is a map from strings (like, "ATLANTA
	    // APP") to a set of frequencies (118350, 126900, 127250,
	    // 127900).  All of the name/frequency set pairs are
	    // members of the same ATCCodeType (eg, APP).
	    FrequencyMap::const_iterator freq;
	    for (freq = bar.begin(); freq != bar.end(); freq++) {
		const set<int>& freqs = freq->second;

		// Separate named groups of frequencies with two spaces.
		if (freq != bar.begin()) {
		    lm.addText("  ");
		}

		// First, the frequency name.
		lm.setFont(_overlays.regularFont(), tinyFontSize);
		lm.addText(freq->first);

		// Now, the frequencies themselves.
		globals.str.clear();
		lm.setFont(_overlays.boldFont(), smallFontSize);
		set<int>::iterator j;
		for (j = freqs.begin(); j != freqs.end(); j++) {
		    globals.str.appendf(" %s", formatFrequency(*j));
		}
		lm.addText(globals.str.str());
	    }

	    lm.newline();
	}

	// The last line contains the airport elevation, lighting
	// indicator, and maximum runway length.
	lm.newline();

	// Airport elevation - set in regular italics.
	lm.setFont(_overlays.regularFont(), mediumFontSize, 0.25);
	// We need to ensure that the number is at least 2 digits
	// long (ie, '6' must be written '06').
	globals.str.printf("%02.0f  ", ap->elevation() * SG_METER_TO_FEET);
	lm.addText(globals.str.str());
	lm.setItalics(0.0);

	// Runway lighting.
	if (ap->lighting()) {
	    lm.addText("L  ");
	} else {
	    lm.addText("-  ");
	}

	// Runway length
	float maxRwy = 0;
	for (unsigned int i = 0; i < ap->rwys().size(); i++) {
	    if (ap->rwys()[i]->length() > maxRwy) {
		maxRwy = ap->rwys()[i]->length();
	    }
	}
	// According to the FAA's "IFR Aeronautical Chart Symbols"
	// document, on IFR low altitude charts, the runway length is
	// given to the nearest 100 feet, with 70 feet as the dividing
	// point.  It's probably different in different countries, and
	// it may be different on VFR charts.  For now, though, we'll
	// use the '70 rule'.
	globals.str.printf("%.0f",
			    ROUND((maxRwy * SG_METER_TO_FEET - 20) / 100));
	lm.addText(globals.str.str());
    }

    lm.end();

    // Place airport label 2 pixels outside of the outer circle, but
    // no more than maxLabelDist pixels from the center.
    double distance = (rA * rO / rI) + 2;
    if ((maxLabelDist != 0) && (distance > maxLabelDist)) {
	distance = maxLabelDist;
    }
    distance *= _metresPerPixel;

    // Add space to compensate for the size of the label.
    float width, height;
    lm.size(&width, &height);
    distance += sqrt((width * width) + (height * height)) / 2.0;

    // Place label at a heading of labelHeading, at the calculated
    // distance, from the airport center.
    float heading = SG_DEGREES_TO_RADIANS * (90.0 - _policy.labelHeading);
    float x = cos(heading) * distance;
    float y = sin(heading) * distance;
    lm.moveTo(x, y);

    // Finally - draw the text.
    geodDrawText(lm, ap->bounds().center, ap->latitude(), ap->longitude());
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
    if ((rwy->width() * multiple) < (maxHeight * _metresPerPixel)) {
	pointSize = rwy->width() * multiple;
    } else if (rwy->width() > (maxHeight * _metresPerPixel)) {
	pointSize = rwy->width();
    } else {
	pointSize = maxHeight * _metresPerPixel;
    }
    // If the text is too small, don't bother drawing anything.
    // EYE - magic number
    if (pointSize / _metresPerPixel < 10.0) {
	return;
    }

    // Label "main" end.
    _labelRunwayEnd(rwy->label(), pointSize, 0.0, rwy);

    // Label "other" end.
    _labelRunwayEnd(rwy->otherLabel(), pointSize, 180.0, rwy);

    // Add runway length and width.  According to Canadian rules,
    // width is indicated only if different than 200', the standard
    // width.  Length is drawn alongside the runway (to the foot).
    // Diagrams also indicate slope, actual magnetic heading, lighting
    // symbols (eg, dots down the runway to indicate centre lighting),
    // runway threshold elevation in feet, magnetic variation.
    //
    // British charts indicate lengths in metres (and always write
    // both, in the form length x width).  They also show the highest
    // spot in the touchdown zone.

    // Calculate point size (in metres) of text.  We make the text fit
    // the runway width until it gets to maxHeight * 0.5.
    pointSize = rwy->width();
    if (pointSize > (maxHeight * 0.5 * _metresPerPixel)) {
	pointSize = maxHeight * 0.5 * _metresPerPixel;
    }

    // Create the label.
    globals.str.printf("%.0f' x %.0f'", 
		       rwy->length() * SG_METER_TO_FEET,
		       rwy->width() * SG_METER_TO_FEET);
    LayoutManager lm(globals.str.str(), _overlays.regularFont(), pointSize);

    // Now draw it.
    geodDrawText(lm, rwy->centre(), rwy->lat(), rwy->lon(), rwy->hdg() - 90.0);
}

// Writes the given label at the end of the given rwy, where 'end' is
// defined by the given heading.  The only reasonable values for hdg
// are 0.0 (the "main" end), and 180.0 (the "other" end).
void AirportsOverlay::_labelRunwayEnd(const char *str, float pointSize,
				      float hdg, RWY *rwy)
{
    LayoutManager lm(str, _overlays.regularFont(), pointSize);
    lm.moveTo(0.0, -rwy->length() / 2.0, LayoutManager::UC);

    geodPushMatrix(rwy->centre(), rwy->lat(), rwy->lon(), rwy->hdg() + hdg); {
    	// EYE - magic number
    	glScalef(0.5, 1.0, 1.0); // Squish characters together
	lm.drawText();
    }
    geodPopMatrix();
}

// Called when somebody posts a notification that we've subscribed to.
void AirportsOverlay::notification(Notification::type n)
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
