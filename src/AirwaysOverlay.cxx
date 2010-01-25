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

#include <cassert>
#include <sstream>

#include <simgear/misc/sg_path.hxx>
#include <simgear/math/sg_geodesy.hxx>

#include "Globals.hxx"
#include "misc.hxx"

#include "AirwaysOverlay.hxx"

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

//////////////////////////////////////////////////////////////////////
// AirwaysOverlay
//////////////////////////////////////////////////////////////////////

AirwaysOverlay::AirwaysOverlay(Overlays& overlays):
    _overlays(overlays), _highDL(0), _lowDL(0)
{
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

    glDeleteLists(_highDL, 1);
    glDeleteLists(_lowDL, 1);

    delete _frustum;
    delete _culler;
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

	// Check that the first id is alphabetically less than the
	// second.  We use this assumption in other parts of the code.
	assert(a->start.id < a->end.id);

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

	// Add to the segments vector.
	_segments.push_back(a);
	
	// Look for the two endpoints in the navPoints map.  For those
	// that are fixes, update their high/low status.
	_checkEnd(a->start, a->isLow);
	_checkEnd(a->end, a->isLow);
    }

    // EYE - will there ever be a false return?
    return true;
}

// Each airway segment has two endpoints, which should be fixes and/or
// navaids.  If an endpoint is a fix, we use the airway type as a
// heuristic to decide whether that fix is a high or low fix.  Note
// that the navaid, fix, and airways databases are not perfect, so we
// need to handle cases where no or partial matches are made.
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
	double lat, lon;
	if (p.isNavaid) {
	    NAV *n = (NAV *)p.n;
	    lat = n->lat;
	    lon = n->lon;
	} else {
	    FIX *f = (FIX *)p.n;
	    lat = f->lat;
	    lon = f->lon;
	}

	if ((lat == end.lat) && (lon == end.lon)) {
	    // Bingo!
	    if (!p.isNavaid) {
		// If the end is a fix, make sure we tag it as high/low.
		FIX *f = (FIX *)p.n;
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
void AirwaysOverlay::draw(bool drawHigh, bool drawLow, bool label)
{
    // I used to add individual airway segments to the culler and draw
    // them based on whether they were visible.  This turned out to be
    // too much work.  It's much easier just to create one display
    // list for low airways and one display list for high airways, and
    // turn them on and off as required.  If, for some reasons, we
    // wanted to render airways differently depending on our zoom, for
    // example, then we couldn't do this.
    if (drawLow) {
	if (_lowDL == 0) {
	    _lowDL = glGenLists(1);
	    assert(_lowDL != 0);
	    glNewList(_lowDL, GL_COMPILE); {
		glColor4fv(awy_low_colour);
		glPushAttrib(GL_LINE_BIT); {
		    glLineWidth(2.0);
		    for (unsigned int i = 0; i < _segments.size(); i++) {
			AWY *a = _segments[i];
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
		for (unsigned int i = 0; i < _segments.size(); i++) {
		    AWY *a = _segments[i];
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
	glVertex2f(x + width / 2.0, -height / 2.0);
	glVertex2f(x + width / 2.0, height / 2.0);
	glVertex2f(x - width / 2.0, height / 2.0);
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
bool AirwaysOverlay::_label(const AWY *a) const
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

    atlasFntRenderer& f = globals.fontRenderer;
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
