/*-------------------------------------------------------------------------
  AirwaysOverlay.cxx

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

#include <OpenGL/gl.h>
#include <GLUT/glut.h>

#include <cassert>
#include <sstream>

#include <simgear/misc/sg_path.hxx>
#include <simgear/math/sg_geodesy.hxx>

#include "Globals.hxx"
#include "misc.hxx"

using namespace std;

#include "AirwaysOverlay.hxx"

// EYE - magic numbers and policies
// const float maxHighAirway = 10000.0;
// const float minHighAirway = 250.0;
// const float maxLowAirway = 500.0;
// const float minLowAirway = 50.0;
const float maxHighAirway = 1250.0;
const float minHighAirway = 50.0;
const float maxLowAirway = 1250.0;
const float minLowAirway = 50.0;

// faa-h-8083-15-2.pdf, page 8-5, has a sample airways chart

// From VFR_Chart_Symbols.pdf
//
// Low altitude:
//   Alternate / Enroute Airway {0.576, 0.788, 0.839}
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

const GLubyte airway_colours[36][4] = {
    {255, 204, 102, 191},	// cantaloupe
    {204, 255, 102, 191},	// honeydew
    {102, 255, 204, 191},	// spindrift
    {102, 204, 255, 191},	// sky
    {204, 102, 255, 191},	// lavender
    {255, 111, 207, 191},	// carnation

    {255, 102, 102, 191},	// salmon
    {255, 255, 102, 191},	// banana
    {102, 255, 102, 191},	// flora
    {102, 255, 255, 191},	// ice
    {102, 102, 255, 191},	// orchid
    {255, 102, 255, 191},	// bubblegum
    {255, 102, 0, 191},		// tangerine

    {128, 255, 0, 191},		// lime
    {0, 255, 128, 191},		// sea foam
    {0, 128, 255, 191},		// aqua
    {102, 0, 255, 191},		// grape
    {255, 0, 128, 191},		// strawberry

    {128, 64, 0, 191},		// mocha
    {64, 128, 0, 191},		// fern
    {0, 128, 64, 191},		// moss
    {0, 64, 128, 191},		// ocean
    {64, 0, 128, 191},		// eggplant
    {128, 0, 64, 191},		// maroon

    {128, 0, 0, 191},		// cayenne
    {128, 128, 0, 191},		// asparagus
    {0, 128, 0, 191},		// clover
    {0, 128, 128, 191},		// teal
    {0, 0, 128, 191},		// midnight
    {128, 0, 128, 191},		// plum

    {255, 0, 0, 191},		// maraschino
    {255, 255, 0, 191},		// lemon
    {0, 255, 0, 191},		// spring
    {0, 255, 255, 191},		// turquoise
    {0, 255, 255, 191},		// blueberry
    {255, 0, 255, 191}		// magenta
};

// Cheezy string hash function.
unsigned int hash(const string& foo)
{
    unsigned int result = 0;
    for (unsigned int i = 0; i < foo.size(); i++) {
	if (foo[i] == '-') {
	    break;
	}
	result += foo[i];
    }
    return result;
}

//////////////////////////////////////////////////////////////////////
// AirwaysOverlay
//////////////////////////////////////////////////////////////////////

AirwaysOverlay::AirwaysOverlay(Overlays& overlays):
    _overlays(overlays), _highDL(0), _lowDL(0)
{
    // EYE - Initialize policy here

    // Create a culler and a frustum searcher for it.
    _culler = new Culler();
    _frustum = new Culler::FrustumSearch(*_culler);

    // Subscribe to moved and zoomed notifications.
    subscribe(Notification::Moved);
    subscribe(Notification::Zoomed);
}

AirwaysOverlay::~AirwaysOverlay()
{
    for (unsigned int i = 0; i < _segments.size(); i++) {
	AWY *n = _segments[i];

	delete n;
    }

    _segments.clear();

    delete _frustum;
    delete _culler;
}

void AirwaysOverlay::setPolicy(const AirwayPolicy& p)
{
    _policy = p;
}

AirwayPolicy AirwaysOverlay::policy()
{
    return _policy;
}

bool AirwaysOverlay::load(const string& fgDir)
{
    bool result = false;

    SGPath f(fgDir);
    f.append("Navaids/awy.dat.gz");

    gzFile arp;
    char *line;

    arp = gzopen(f.c_str(), "rb");
    if (arp == NULL) {
	// EYE - we might want to throw an error instead.
	fprintf(stderr, "_loadAirways: Couldn't open \"%s\".\n", f.c_str());
	return false;
    } 

    // Check the file version.  We can handle version 640 files.
    int version = -1;
    gzGetLine(arp, &line);	// Windows/Mac header
    gzGetLine(arp, &line);	// Version
    sscanf(line, "%d", &version);
    if (version == 640) {
	// It looks like we have a valid file.
	result = _load640(arp);
    } else {
	// EYE - throw an error?
	fprintf(stderr, "_loadAirways: \"%s\": unknown version %d.\n", 
		f.c_str(), version);
	result = false;
    }

    gzclose(arp);

    return result;
}

bool AirwaysOverlay::_load640(const gzFile& arp)
{
    char *line;

    AWY *a;

    fprintf(stderr, "Loading airways ...\n");
    while (gzGetLine(arp, &line)) {
	if (strcmp(line, "") == 0) {
	    // Blank line.
	    continue;
	} 

	if (strcmp(line, "99") == 0) {
	    // Last line.
	    break;
	}

	// Create a record and fill it in.
	a = new AWY;
	istringstream str(line);

	// A line looks like this:
	//
	// <id> <lat> <lon> <id> <lat> <lon> <high/low> <base> <top> <name>
	//
	// 
	int lowHigh;
	str >> a->start.id >> a->start.lat >> a->start.lon
	    >> a->end.id >> a->end.lat >> a->end.lon
	    >> lowHigh >> a->base >> a->top >> a->name;
	// EYE - check for errors
	if (lowHigh == 1) {
	    a->isLow = true;
	} else if (lowHigh == 2) {
	    a->isLow = false;
	} else {
	    assert(false);
	}

	// Add to the culler.  The airway bounds are given by its two
	// endpoints.
	// EYE - save these two points
	sgdVec3 point;
	atlasGeodToCart(a->start.lat, a->start.lon, 0.0, point);
	a->bounds.extend(point);
	atlasGeodToCart(a->end.lat, a->end.lon, 0.0, point);
	a->bounds.extend(point);
	double az1, az2, s;
	geo_inverse_wgs_84(0.0, a->start.lat, a->start.lon, 
			   a->end.lat, a->end.lon,
			   &az1, &az2, &s);
	a->length = s;				       

	// Add to our culler.
	_frustum->culler().addObject(a);

	// Add to the airways vector.
	_segments.push_back(a);
	
	// Look for the two endpoints in the navPoints map.  For those
	// that are fixes, update their high/low status.
	_checkEnd(a->start, a->isLow);
	_checkEnd(a->end, a->isLow);
    }

    // EYE - will there ever be a false return?
    return true;
}

void AirwaysOverlay::_checkEnd(AwyLabel &end, bool isLow)
{
    // EYE - clear as mud!
    multimap<string, NAVPOINT>::iterator it;
    pair<multimap<string, NAVPOINT>::iterator, 
	multimap<string, NAVPOINT>::iterator> ret;
    
    // Search for a navaid or fix with the same name and same location
    // as 'end'.
    ret = navPoints.equal_range(end.id);
    for (it = ret.first; it != ret.second; it++) {
	NAVPOINT p = (*it).second;
	FIX *f;
	NAV *n;
	double lat, lon;
	if (p.isNavaid) {
	    n = (NAV *)p.n;
	    lat = n->lat;
	    lon = n->lon;
	} else {
	    f = (FIX *)p.n;
	    lat = f->lat;
	    lon = f->lon;
	}

	if ((lat == end.lat) && (lon == end.lon)) {
	    // Bingo!
	    if (!p.isNavaid) {
		// If the end is a fix, make sure we tag it as high/low.
		if (isLow) {
		    f->low = true;
		} else {
		    f->high = true;
		}
	    }

	    // EYE - put a NAVPOINT structure in AwyLabel?
	    end.isNavaid = p.isNavaid;
	    end.n = p.n;

	    // We've found an exact match, so bail out early.
	    return;
	}
    }

    // Couldn't find an exact match.  Find the closest navaid or fix
    // with the same name.
    double distance = 1e12;
    double latitude, longitude;
    for (it = ret.first; it != ret.second; it++) {
	NAVPOINT p = (*it).second;
	FIX *f;
	NAV *n;
	double lat, lon;
	if (p.isNavaid) {
	    n = (NAV *)p.n;
	    lat = n->lat;
	    lon = n->lon;
	} else {
	    f = (FIX *)p.n;
	    lat = f->lat;
	    lon = f->lon;
	}

	double d, junk;
	geo_inverse_wgs_84(lat, lon, end.lat, end.lon, &junk, &junk, &d);
	if (d < distance) {
	    distance = d;
	    latitude = lat;
	    longitude = lon;
	}
    }

    // EYE - we need some kind of logging facility.
//     if (distance == 1e12) {
// 	fprintf(stderr, "_findEnd: can't find any match for '%s' <%lf, %lf>\n",
// 		end.id.c_str(), end.lat, end.lon);
//     } else {
// 	fprintf(stderr, "_findEnd: closest match for '%s' <%lf, %lf> is\n",
// 		end.id.c_str(), end.lat, end.lon);
// 	fprintf(stderr, "\t%.0f metres away <%lf, %lf>\n",
// 		distance, latitude, longitude);
//     }
}

// EYE - we need to be very careful about OpenGL state changes.  Here,
// it turns out that changing line width is extremely expensive.
// Therefore, we need to do all the low-altitude airways first
// (they'll all have the same line width), and all the high-alitude
// airways last.
// void AirwaysOverlay::draw()
void AirwaysOverlay::draw(bool drawHigh, bool drawLow, bool label)
{
// //     static double scale = 0.0;

//     if (_type < 0) {
// 	// EYE - error?
// 	// Airways weren't loaded.
// 	return;
//     }

// //     if (_overlays.scale() != scale) {
// // 	scale = _overlays.scale();
//     if (_overlays.isDirty()) {
// 	// Something's changed, so we need to regenerate the display
// 	// list.
// 	glDeleteLists(_DL, 1);
// 	_DL = glGenLists(1);
// 	assert(_DL != 0);
// 	glNewList(_DL, GL_COMPILE);

// 	glEnable(GL_LINE_SMOOTH);
// 	glEnable(GL_POINT_SMOOTH);
// 	glEnable(GL_BLEND);
// 	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

// 	vector<void *> intersections = _culler.intersections(_type);
// 	for (int i = 0; i < intersections.size(); i++) {
// 	    AWY *a = (AWY *)intersections[i];

// 	    _render(a);
// 	}

// 	glDisable(GL_BLEND);
// 	glDisable(GL_POINT_SMOOTH);
// 	glDisable(GL_LINE_SMOOTH);

// 	glEndList();
//     }

//     glCallList(_DL);
//     static double scale = 0.0;

    static bool firstTime = true;
    if (firstTime) {
	_lowDL = glGenLists(1);
	assert(_lowDL != 0);
	glNewList(_lowDL, GL_COMPILE);
// 	glColor4fv(awy_low_colour);
	glLineWidth(2.0);
	for (unsigned int i = 0; i < _segments.size(); i++) {
	    AWY *a = _segments[i];
	    if (a->isLow) {
// 	    if (a->isLow && (a->top != 180)) {
		_render(a);
	    }
	}
	glEndList();

	_highDL = glGenLists(1);
	assert(_highDL != 0);
	glNewList(_highDL, GL_COMPILE);
// 	glColor4fv(awy_high_colour);
	glLineWidth(1.0);
	for (unsigned int i = 0; i < _segments.size(); i++) {
	    AWY *a = _segments[i];
	    if (!a->isLow) {
		_render(a);
	    }
	}
	glEndList();

	firstTime = false;
    }

    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//     for (int i = 0; i < _segments.size(); i++) {
// 	AWY *a = _segments[i];
// 	if (a->DL > 0) {
// 	    glCallList(a->DL);
// 	}
//     }
//     if (_overlays.scale() > minHighAirway) {
// 	glCallList(_highDL);
//     }
//     if (_overlays.scale() < maxLowAirway) {
// 	glCallList(_lowDL);
//     }
//     if (_airwayDisplay == low) {
// 	glCallList(_lowDL);
//     } else if (_airwayDisplay == high) {
// 	glCallList(_highDL);
//     }
    if (drawLow) {
	glCallList(_lowDL);
    } 
    if (drawHigh) {
	glCallList(_highDL);
    }
    glDisable(GL_BLEND);
    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_LINE_SMOOTH);

    // Now label them.
    // EYE - we should create a display list, combine this with the
    // previous bit, blah blah blah
    if (label) {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(1.0);
	vector<Cullable *> intersections = _frustum->intersections();
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
	glDisable(GL_BLEND);
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
// void AirwaysOverlay::_render(const AWY *a)
void AirwaysOverlay::_render(AWY *a)
{
    sgdVec3 point;

//     // EYE - magic numbers
//     // Low - < 1000.0 m/pixel, High - > 100.0

//     // Render an airway between a range.  At the edges, the width is
//     // 1, in the middle it's 5.
// //     sgVec2 low = {50.0, 1000.0}, high = {250.0, 10000.0};

//     float width = 0.0;
//     if (a->isLow) {
// 	glColor4fv(awy_low_colour);
// 	if (metresPerPixel < 1000.0) {
// 	    width = 5.0 - metresPerPixel / 200.0;
// 	}
// 	if (metresPerPixel < 100.0) {
// // 	    width = 5.0;
// 	    width = metresPerPixel / 20.0;
// 	}
// // 	if ((metresPerPixel >= low[0]) && (metresPerPixel <= low[1])) {
// // 	    float middle = (low[0] + low[1]) / 2.0;
// // 	    width = (middle - fabs(middle - metresPerPixel)) / middle * 4.0 + 1.0;
// // 	}
//     } else {
// 	glColor4fv(awy_high_colour);
// 	if (metresPerPixel > 200.0) {
// // 	    width = (metresPerPixel - 100.0) / 500.0;
// 	    width = 1.0;
// 	}
// 	if (width > 5.0) {
// 	    width = 5.0;
// 	}
// // 	if ((metresPerPixel >= high[0]) && (metresPerPixel <= high[1])) {
// // 	    float middle = (high[0] + high[1]) / 2.0;
// // 	    width = (middle - fabs(middle - metresPerPixel)) / middle * 4.0 + 1.0;
// // 	}
//     }

//     if (fabs(width) < 0.01) {
// 	return;
//     }
//     a->DL = glGenLists(1);
//     assert(a->DL != 0);
//     glNewList(a->DL, GL_COMPILE);

//     glLineWidth(width);
//     glPointSize(width / 2.0);
// //     if (a->name.find("J121") != string::npos) {
// //     if (a->name == "A632") {
    if (a->isLow) {
	glColor4ubv(airway_colours[hash(a->name) % 24]);
    } else {
	glColor4ubv(airway_colours[(hash(a->name) % 12) + 24]);
    }
    glBegin(GL_LINES); {
	atlasGeodToCart(a->start.lat, a->start.lon, 0.0, point);
	glVertex3dv(point);
	atlasGeodToCart(a->end.lat, a->end.lon, 0.0, point);
	glVertex3dv(point);
    }
    glEnd();
//     glEndList();
//     if (metresPerPixel < 7500.0) {
// 	glBegin(GL_POINTS); {
// 	    glColor4f(0.0, 0.0, 0.0, (7500.0 - metresPerPixel) / 7500.0);
// 	    SGGeodesy::SGGeodToCart(SGGeod::fromDegFt(a->start.lon, a->start.lat, 0.0), point);
// 	    glVertex3dv(point.sg());
// 	    SGGeodesy::SGGeodToCart(SGGeod::fromDegFt(a->end.lon, a->end.lat, 0.0), point);
// 	    glVertex3dv(point.sg());
// 	}
// 	glEnd();
//     }
//     // Label
//     if ((a->isLow && (width > 2.5)) ||
// 	(!a->isLow && (metresPerPixel > 500.0) && (metresPerPixel < 3000.0))) {
// 	// EYE - we really should do the intersections separately from
// 	// the segments.
// 	fntRenderer& f = _overlays.fontRenderer();
// 	if (a->isLow) {
// 	    f.setPointSize(width * 2.0 * metresPerPixel);
// 	} else {
// 	    f.setPointSize(7.0 * metresPerPixel);
// 	}
// 	// EYE - black (from above) looks nicer
// // 	glColor4fv(awy_low_colour);
// 	glPushMatrix(); {
// 	    SGGeodesy::SGGeodToCart(SGGeod::fromDegFt(a->start.lon, a->start.lat, 0.0), point);
// 	    glTranslated(point[0], point[1], point[2]);
// 	    glRotatef(a->start.lon + 90.0, 0.0, 0.0, 1.0);
// 	    glRotatef(90.0 - a->start.lat, 1.0, 0.0, 0.0);

// 	    f.start3f(0.0, 0.0, 0.0);
// 	    f.puts(a->start.id.c_str());
// 	}
// 	glPopMatrix();

// 	glPushMatrix(); {
// 	    SGGeodesy::SGGeodToCart(SGGeod::fromDegFt(a->end.lon, a->end.lat, 0.0), point);
// 	    glTranslated(point[0], point[1], point[2]);
// 	    glRotatef(a->end.lon + 90.0, 0.0, 0.0, 1.0);
// 	    glRotatef(90.0 - a->end.lat, 1.0, 0.0, 0.0);

// 	    f.start3f(0.0, 0.0, 0.0);
// 	    f.puts(a->end.id.c_str());
// 	}
// 	glPopMatrix();
//     }
// //     }
}

// Draws a single text box.  The background is white, the text and a
// box outline are drawn in colour.  The box is drawn with the centre
// at <x, 0>.
static void _drawLabel(LayoutManager &lm, float *colour,
		       float x, float width, float height)
{
    // Background box.
    glBegin(GL_QUADS); {
	glColor4f(1.0, 1.0, 1.0, 1.0);
	glVertex2f(x - width / 2.0, -height / 2.0);
	glVertex2f(x - width / 2.0, height / 2.0);
	glVertex2f(x + width / 2.0, height / 2.0);
	glVertex2f(x + width / 2.0, -height / 2.0);
    }
    glEnd();

    // Text.
    glColor4fv(colour);
    lm.moveTo(x, 0.0); 
    lm.drawText();

    // Box outline.
    glBegin(GL_LINE_LOOP); {
	glVertex2f(x - width / 2.0, -height / 2.0);
	glVertex2f(x - width / 2.0, height / 2.0);
	glVertex2f(x + width / 2.0, height / 2.0);
	glVertex2f(x + width / 2.0, -height / 2.0);
    }
    glEnd();
}

// Labels the given airway.  Does nothing if there isn't enough room
// to label the segment.
bool AirwaysOverlay::_label(AWY *a)
{
    // The labels are placed in the middle of the segment, oriented
    // along the segment.
    sgdVec3 start, end, middle;
    atlasGeodToCart(a->start.lat, a->start.lon, 0.0, start);
    atlasGeodToCart(a->end.lat, a->end.lon, 0.0, end);

    // We need at least minDist pixels of space to write any labels.
    const double minDist = 50.0;
    if ((a->length / _metresPerPixel) < minDist) {
	return false;
    }

    // We have space, so calculate the middle of the segment.
    sgdAddVec3(middle, start, end);
    sgdScaleVec3(middle, 0.5);

    fntRenderer& f = globals.fontRenderer;
    LayoutManager lmName, lmElev, lmDist;
//     LayoutManager lmStart, lmEnd;
    // EYE - magic numbers
    const float maxPointSize = 12.0;
    const float minPointSize = 1.0;
    float pointSize;
    // EYE - magic number
    const float border = 4.0 * _metresPerPixel; // 4 pixel border
    float nameWidth, nameHeight;
    float elevWidth, elevHeight;
    float distWidth, distHeight;
//     float startWidth, startHeight, endWidth, endHeight;

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

    // Name
    lmName.begin();
    lmName.setFont(f, pointSize);
    lmName.addText(a->name);
    lmName.end();

    lmName.size(&nameWidth, &nameHeight);
    nameWidth += border;
    nameHeight += border;

    if (nameWidth + (border * 2.0) > a->length) {
	return false;
    }

    // Top and base of airway

    // EYE - start adding these to high routes at around 1000 m/pixel.
    // The lows can be added immediately (500 or less).  Or maybe just
    // add them to the sides of the name, and only do so when there's
    // room?
    lmElev.begin();
    // EYE - magic number
    lmElev.setFont(f, pointSize * 0.75);
    globalString.printf("%d", a->isLow ? a->top * 100 : a->top);
    lmElev.addText(globalString.str());
    lmElev.newline();

    globalString.printf("%d", a->isLow ? a->base * 100 : a->base);
    lmElev.addText(globalString.str());
    lmElev.end();

    lmElev.size(&elevWidth, &elevHeight);
    elevWidth += border;
    elevHeight += border;

    if ((elevWidth + nameWidth / 2.0 + (border * 2.0)) > (a->length / 2.0)) {
	elevWidth = 0.0;
    }

    // Length
    lmDist.begin();
    // EYE - magic number
    lmDist.setFont(f, pointSize * 0.75);
    globalString.printf("%.0f", a->length * SG_METER_TO_NM);
    lmDist.addText(globalString.str());
    lmDist.end();

    lmDist.size(&distWidth, &distHeight);
    distWidth += border;
    distHeight += border;

    if ((distWidth + nameWidth / 2.0 + (border * 2.0)) > (a->length / 2.0)) {
	distWidth = 0.0;
    }

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

    glPushMatrix(); {
	glTranslated(middle[0], middle[1], middle[2]);

	// Make sure the text isn't upside-down.

	// EYE - is this overkill?  Can we use a simpler calculation
	// to get the heading?
	double lat, lon, elev, hdg, junk;
	sgCartToGeod(middle, &lat, &lon, &elev);
	lat *= SGD_RADIANS_TO_DEGREES;
	lon *= SGD_RADIANS_TO_DEGREES;

	geo_inverse_wgs_84(lat, lon, a->end.lat, a->end.lon, 
			   &hdg, &junk, &junk);
	if (hdg > 180.0) {
	    hdg -= 180.0;
	}
	hdg -= 90.0;

	glRotatef(lon + 90.0, 0.0, 0.0, 1.0);
	glRotatef(90.0 - lat, 1.0, 0.0, 0.0);
	glRotatef(-hdg, 0.0, 0.0, 1.0);

	// Draw the name in the centre of the segment.
	// EYE - magic "number"
	sgVec4 nameColour = {1.0, 0.0, 0.0, 1.0};
	_drawLabel(lmName, nameColour, 0.0, nameWidth, nameHeight);

	// Draw the base and top elevations (if we have room) on the
	// left.
	if (elevWidth > 0.0) {
	    float xOffset = elevWidth / 2.0 + border + nameWidth / 2.0;
	    // EYE - magic "number"
	    sgVec4 colour = {0.0, 1.0, 0.0, 1.0};
	    _drawLabel(lmElev, colour, -xOffset, elevWidth, elevHeight);
	}

	// Draw the length (if we have room) on the right.
	if (distWidth > 0.0) {
	    float xOffset = distWidth / 2.0 + border + nameWidth / 2.0;
	    // EYE - magic "number"
	    sgVec4 colour = {0.0, 0.0, 1.0, 1.0};
	    _drawLabel(lmDist, colour, xOffset, distWidth, distHeight);
	}
    }
    glPopMatrix();

    return true;
}

// Called when somebody posts a notification that we've subscribed to.
bool AirwaysOverlay::notification(Notification::type n)
{
    if (n == Notification::Moved) {
	// Update our frustum from globals.
	_frustum->move(globals.modelViewMatrix);
    } else if (n == Notification::Zoomed) {
	// Update our frustum and scale from globals.
	_frustum->zoom(globals.frustum.getLeft(),
		       globals.frustum.getRight(),
		       globals.frustum.getBot(),
		       globals.frustum.getTop(),
		       globals.frustum.getNear(),
		       globals.frustum.getFar());
	_metresPerPixel = globals.metresPerPixel;
    } else {
	assert(false);
    }

    return true;
}
