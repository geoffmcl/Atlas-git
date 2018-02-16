/*-------------------------------------------------------------------------
  NavaidsOverlay.cxx

  Written by Brian Schack

  Copyright (C) 2009 - 2017 Brian Schack

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
#include "LayoutManager.hxx"

using namespace std;

//////////////////////////////////////////////////////////////////////
// DisplayList
//////////////////////////////////////////////////////////////////////

// True if begin() has been called without a corresponding end().
bool DisplayList::_compiling = false;

DisplayList::DisplayList(): _dl(0), _valid(false)
{
}

DisplayList::~DisplayList()
{
    glDeleteLists(_dl, 1);
}

void DisplayList::begin()
{
    assert(!_compiling);

    // Generate the display list if necessary.
    if (_dl == 0) {
	_dl = glGenLists(1);
	assert(_dl);
    }

    // Start compiling the display list.
    _valid = false;
    glNewList(_dl, GL_COMPILE);
    _compiling = true;
}

void DisplayList::end()
{
    assert(_compiling);
    glEndList();
    _valid = true;
    _compiling = false;
}

void DisplayList::call()
{
    // Although it's not illegal to call an undefined display list,
    // it's probably a logic error.
    assert(_dl);
    assert(_valid);
    glCallList(_dl);
}

//////////////////////////////////////////////////////////////////////
// Internals - these are constants and functions used elsewhere in the
// file.
//////////////////////////////////////////////////////////////////////

// EYE - should some of these be moved closer to where they are used?
// For example, ILS stuff really should just reside with the ILS
// renderer class, shouldn't it?

const float __clearColour[4] = {1.0, 1.0, 1.0, 0.0};
// VOR (teal)
const float __vorColour[4] = {0.000, 0.420, 0.624, 1.0};
// NDB (purple)
const float __ndbColour[4] = {0.525, 0.294, 0.498, 1.0};

// TACAN (grey? orange? brown? black?)

// EYE - what colour *should* we use really?  This is from lo2.pdf,
// but (a) this colour is used for many navaids, and (b) it's an IFR
// chart, and (c) it's Canadian
//
const float __dmeColour[4] = {0.498, 0.498, 0.498, 1.0};
//
// EYE - theoretically, we should use the same colour for all DME
// components of navaids - VOR-DME, NDB-DME, VORTAC, TACAN - but it's
// hard to make this look nice unfortunately.
//
// This colour looks okay - not too bright, but enough to show up.
// Still, it's not entirely satisfactory.
// const float __dmeColour[4] = {0.75, 0.5, 0.25, 1.0};
//
// This is the same as VORs.
//
// const float __dmeColour[4] = {0.000, 0.420, 0.624, 1.0};

// Markers.  Note that the order of entries must match the Marker
// class Type enumeration (ie, first OUTER, then MIDDLE, finally
// INNER).
const float __markerColours[3][4] = 
    {{0.0, 0.0, 1.0, 0.5},	// Outer marker (blue)
     {1.0, 0.5, 0.0, 0.5},	// Middle marker (amber)
     {1.0, 1.0, 1.0, 0.5}};	// Inner marker (white)

// ILS localizer (from Canada Air Pilot, CYYZ.pdf)
// - clear on left, solid pink on right, black outline, heavy black
//   line down centre
const float __ilsColour[4] = {1.000, 0.659, 0.855, 0.7};

// This is my own invention - a localizer (no glideslope) is drawn in
// grey.
const float __locColour[4] = {0.5, 0.5, 0.5, 0.7};

// ILS text is slightly translucent.
const float __ilsLabelColour[4] = {0.0, 0.0, 0.0, 0.75};

// Radii, in metres, for outer, middle, and inner markers.  Like
// __markerColours, it must match the order of the Marker Type
// enumeration.
const float __markerRadii[3] = 
    {1.0 * SG_NM_TO_METER,
     0.35 * SG_NM_TO_METER,
     0.25 * SG_NM_TO_METER};

// EYE - make user-adjustable?  Combine these into some kind of label
// policy?

// When to switch label display types.  When the range of the navaid,
// in pixels, is less than __smallLabel, we don't draw labels.  When
// between __smallLabel and __mediumLabel, we draw small labels.  When
// between __mediumLabel and __maximumLabel, we draw medium labels.
// When greater than __maximumLabel, we draw large labels.
const float __smallLabel = 50.0, __mediumLabel = 250.0, __maximumLabel = 600.0;

// Standard label font size, in pixels.
const float __labelPointSize = 10.0;

// Standard icon size, in pixels.  In some ways it should really be
// thought of as a scale factor.  An icon (which is always defined
// with a radius of 1.0) is multiplied by this to get a regular icon
// size.  However, when zoomed out, this will be altered.  Also, some
// icons are naturally bigger (ie, NDBs), so they have an additional
// scaling factor to make their relative sizes correct.
const float __iconSize = 10.0;

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
static void __createTriangle(float width, 
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

// Used for drawing labels on navaids.  It contains all the
// information needed to draw a navaid label - the colour, the scale,
// the layout (which includes the label text and position
// information), and the morse identifier (id).
struct Label {
    float colour[4];
    float metresPerPixel;
    LayoutManager lm;
    string id;
};

// Either draws the given string in morse code at the given location
// (if render is true), OR returns the width necessary to draw it (if
// render is false.  In this case, x and y are ignored).  If drawn, we
// draw the morse stacked on top of each other to fill one line (which
// is height high).  We assume that the current OpenGL units are
// metres.  The location (x, y) specifies the lower-left corner of the
// rendered morse text.
static float __renderMorse(const string& id, float height,
			   float x, float y, float metresPerPixel, 
			   bool render = true)
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

// Returns width necessary to render the given string in morse code,
// at the current point size.
static float __morseWidth(const string& id, float height, float metresPerPixel)
{
    return __renderMorse(id, height, 0.0, 0.0, metresPerPixel, false);
}

// Called from the layout manager when it encounters an addBox() box.
static void __morseCallback(LayoutManager *lm, float x, float y, void *userData)
{
    Label *l = (Label *)userData;
    float ascent = lm->font()->ascent() * lm->pointSize();
    __renderMorse(l->id, ascent, x, y, l->metresPerPixel);
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
static Label *__makeLabel(const char *fmt, Navaid *n,
			  float labelPointSize,
			  float x, float y,
			  LayoutManager::Point lp = LayoutManager::CC)
{
    // The label consists of a list of lines.  Each line consists of
    // intermixed text and morse.  The label, each line, and text and
    // morse unit has a width, height, and origin.
    Label *l = new Label;

    // Set our font and find out what our ascent is (__morseWidth and
    // __renderMorse need it).
    atlasFntTexFont *f = globals.aw->regularFont();
    l->lm.setFont(f, labelPointSize);
    float ascent = l->lm.font()->ascent() * labelPointSize;

    // Go through the format string once, using the layout manager to
    // calculate sizes.
    l->lm.begin(x, y);
    bool spec = false;
    // l->morseChunk = -1;
    AtlasString line;
    LOC *loc;			// Needed in case 'N'
    NDB_DME *ndb_dme;		// Needed in case 'f'
    for (const char *c = fmt; *c; c++) {
	if ((*c == '%') && !spec) {
	    spec = true;
	} else if (spec) {
	    switch (*c) {
	      case 'I':
		line.appendf("%s", n->id().c_str());
		break;
	      case 'M': 
		  {
		      double _metresPerPixel = globals.aw->scale();
		      l->lm.addText(line.str());
		      line.clear();
		      l->lm.addBox(__morseWidth(n->id(), ascent, 
						_metresPerPixel), 
				   0.0, __morseCallback, (void *)l);
		      l->id = n->id();
		      l->metresPerPixel = _metresPerPixel;
		  }
		break;
	      case 'N':
		line.appendf("%s", n->name().c_str());
		// The name of an ILS includes the type of approach.
		loc = dynamic_cast<LOC *>(n);
		if (loc) {
		    ILS *ils = dynamic_cast<ILS *>(NavaidSystem::owner(loc));
		    assert(ils);

		    switch (ils->type()) {
		      case ILS::ILS_CAT_I:
			line.appendf(" ILS-CAT-I");
			break;
		      case ILS::ILS_CAT_II:
			line.appendf(" ILS-CAT-II");
			break;
		      case ILS::ILS_CAT_III:
			line.appendf(" ILS-CAT-III");
			break;
		      case ILS::LDA:
			line.appendf(" LDA");
			break;
		      case ILS::IGS:
			line.appendf(" IGS");
			break;
		      case ILS::Localizer:
			line.appendf(" LOC");
			break;
		      case ILS::SDF:
			line.appendf(" SDF");
			break;
		      default:
			assert(0);
			break;
		    }
		}
		break;
	      case 'F':
		line.appendf("%s", formatFrequency(n->frequency()));
		break;
	      case 'f':
		// DME frequency in an NDB-DME.
		ndb_dme = dynamic_cast<NDB_DME *>(NavaidSystem::owner(n));
		if (ndb_dme) {
		    DME *dme = ndb_dme->dme();
		    line.appendf("%s", formatFrequency(dme->frequency()));
		}
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

    if (dynamic_cast<VOR *>(n)) {
	memcpy(l->colour, __vorColour, sizeof(float) * 4);
	l->lm.setBoxed(true);
    } else if (dynamic_cast<DME *>(n)) {
	memcpy(l->colour, __dmeColour, sizeof(float) * 4);
	l->lm.setBoxed(true);
    } else if (dynamic_cast<NDB *>(n)) {
	memcpy(l->colour, __ndbColour, sizeof(float) * 4);
	l->lm.setBoxed(true);
    } else if (dynamic_cast<LOC *>(n)) {
	memcpy(l->colour, __ilsLabelColour, sizeof(float) * 4);
    } else {
	assert(0);
    }

    l->lm.setAnchor(lp);

    return l;
}

// Draws the given label.  The label (generated by __makeLabel) has
// all the information needed for rendering by a layout manager.
static void __drawLabel(Label *l)
{
    // Draw the text.
    glColor4fv(l->colour);
    l->lm.drawText();
}

// Draws a label for the given navaid in the style given by fmt, with
// point lp on the label placed at x, y.  The label will be drawn in
// the current font, at the given point size.  VORs, DMEs and NDBs are
// drawn with a box around the text and a translucent white background
// behind the text.
void __drawLabel(const char *fmt, Navaid *n,
		float labelPointSize,
		float x, float y,
		LayoutManager::Point lp = LayoutManager::CC)
{
    Label *l;

    l = __makeLabel(fmt, n, labelPointSize, x, y, lp);
    __drawLabel(l);

    delete l;
}

//////////////////////////////////////////////////////////////////////
// WaypointRenderer
//////////////////////////////////////////////////////////////////////

WaypointRenderer::WaypointRenderer(int noOfPasses, int noOfLayers): 
    _waypointsDirty(true), _currentPass(0), _noOfPasses(noOfPasses)
{
    _layers.resize(noOfLayers);

    subscribe(Notification::Moved);
    subscribe(Notification::Zoomed);
}

void WaypointRenderer::draw(NavData *nd, bool labels)
{
    if (_waypointsDirty) {
	_waypoints.clear();
	_getWaypoints(nd);
	_waypointsDirty = false;
    }

    _draw(labels);

    _currentPass = (_currentPass + 1) % _noOfPasses;
}

void WaypointRenderer::notification(Notification::type n)
{
    if (n == Notification::Moved) {
	_waypointsDirty = true;
    } else if (n == Notification::Zoomed) {
	// EYE - what about an initial value for _metresPerPixel?
	_metresPerPixel = globals.aw->scale();
	_labelPointSize = __labelPointSize * _metresPerPixel;
	_waypointsDirty = true;
    }
}

// Called by subclasses when they want to draw a layer.  It does a bit
// of housekeeping by checking if the display list is valid.  If so,
// it just calls it directly and returns.  If not, it begins compiling
// the displaylist, using the method supplied by the subclass to
// render the layer..  It's a bit byzantine, but it saves subclasses
// from implementing the same boilerplate code.
//
// Note: I experimented with a templated WaypointRenderer class.  The
// advantage of the templated class was being able to have an
// arbitrary type for the _waypoints vector.  The disadvantage was the
// syntactic complexity of the template code.  Just so I don't forget,
// here's how I defined the templated method:
//
// template <class T, class S>
// void WaypointRenderer::_drawLayers(DisplayList& dl, void(T::*fn)(S))
//
// I needed two classes - the subclass type (eg, VORRenderer) and the
// waypoint class (eg, VOR *).  And here are the key calls in the
// method:
//
//     T *caller = dynamic_cast<T *>(this);
//     ...
// 		(caller->*fn)(dynamic_cast<S>(_waypoints[i]));
//
// In the subclass _draw() method, here's how I'd call _drawLayers():
//
// _drawLayers<VORRenderer, VOR *>(_layers[VORLayer], &VORRenderer::_drawVOR);
//
// Finally, when the class was templated, subclasses couldn't
// reference the _waypoints vector directly (I don't know why).
// Instead, they'd have to do one of 3 things:
//
// (a) this->_waypoints
// (b) WaypointRenderer<T, S>::_waypoints
// (c) using WaypointRenderer<T, S>::_waypoints; (in the class declaration)
//     _waypoints; (here)
//
// All in all, not pretty, and not worth it in my opinion.
template <class T>
void WaypointRenderer::_drawLayer(DisplayList &dl, void (T::*fn)())
{
    if (!dl.valid()) {
	dl.begin(); {
	    T *caller = dynamic_cast<T *>(this);
	    (caller->*fn)();
	}
	dl.end();
    }
    dl.call();
}

bool WaypointRenderer::_iconVisible(Navaid *n, IconScalingPolicy& isp, 
					float& radius)
{
    bool result = true;

    // Scaled size of navaid, in pixels.
    radius = n->range() * isp.rangeScaleFactor / _metresPerPixel;
    
    // If it's too big, just make it the maximum allowable size.
    if (radius > isp.maxSize) {
	radius = isp.maxSize;
    }

    if (radius < isp.minSize) {
	radius = 0;
	result = false;
    }

    // If the icon is shrinking, shrink the label text
    // proportionately.
    isp.labelPointSize = _labelPointSize * radius / isp.maxSize;

    return result;
}

//////////////////////////////////////////////////////////////////////
// VORRenderer
//////////////////////////////////////////////////////////////////////

// This function is used in calls to count_if() in notification
// methods.  It returns true if the given navaid is of the given type.
template <class T>
static bool _isA(Navaid *n)
{
    return dynamic_cast<T>(n);
}

// Line width of VOR rose as a factor of VOR range.
const float VORRenderer::_lineScale = 0.005;
const float VORRenderer::_maxLineWidth = 5.0;
// How fat to make VOR radials.
const float VORRenderer::_angularWidth = 10.0;

VORRenderer::VORRenderer(): 
    WaypointRenderer(1, _LayerCount), _radioactive(false)
{
    // The scaling policy for the VOR icons.
    _isp.rangeScaleFactor = 0.1;
    _isp.minSize = 1.0;
    _isp.maxSize = __iconSize;

    // The scaling policy for the VOR rose.
    _rsp.rangeScaleFactor = 0.1;
    _rsp.minSize = __iconSize * 4.0;
    _rsp.maxSize = 150.0;

    subscribe(Notification::AircraftMoved);
    subscribe(Notification::NewFlightTrack);
}

void VORRenderer::notification(Notification::type n)
{
    if ((n == Notification::Moved) ||
	(n == Notification::Zoomed)) {
	_layers[VORLayer].invalidate();
	_layers[LabelLayer].invalidate();
    } else if ((n == Notification::AircraftMoved) ||
	       (n == Notification::NewFlightTrack)) {
	// We don't need to redraw our radio layer if, for a given
	// radio (remember, there are 2 VOR tuners in a flight data
	// point):
	//
	// (a) We weren't tuned in before and we aren't now
	// (b) We were tuned in before and we're tuned in now, and our
	//     radial and/or frequency has changed.
	//
	// So what we do is see if our tuning status has changed (we
	// were tuned before but aren't now, or vice-versa).  Then, if
	// we were tuned in both times, we just compare <nav1_rad,
	// nav1_freq> and <nav2_rad, nav2_freq> from the previous and
	// current flight data point.
	FlightData *p = globals.aw->ac()->currentPoint();
	const set<Navaid *>& navaids = p->navaids();
	bool radioactive = 
	    count_if(navaids.begin(), navaids.end(), _isA<VOR *>);

	// Were we tuned in before or now?
	if (_radioactive || radioactive) {
	    // Hmmm, we might need to redraw.  Have any of the
	    // radial/frequency pairs changed?
	    if ((_p->nav1_rad != p->nav1_rad) ||
		(_p->nav1_freq != p->nav1_freq) ||
		(_p->nav2_rad != p->nav2_rad) ||
		(_p->nav2_freq != p->nav2_freq)) {
		// Something changed, so we need to redraw.
		_layers[RadioLayer].invalidate();
	    }
	}

	_radioactive = radioactive;
	_p = p;
    }

    WaypointRenderer::notification(n);
}

// Creates a standard VOR rose of radius 1.0.  This is a circle with
// ticks and arrows, a line from the centre indicating north, and
// labels at 30-degree intervals around the outside.
//
// The rose is drawn in the current colour, with the current line
// width.
void VORRenderer::_createVORRose()
{
    // Draw a standard VOR rose or radius 1.  It is drawn in the XY
    // plane, with north in the positive Y direction, and east in the
    // positive X direction.
    _VORRoseDL.begin(); {
	glColor4fv(__vorColour);
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

		LayoutManager lm(label.str(), globals.aw->regularFont(), 
				 pointSize);
		lm.setAnchor(LayoutManager::LC);
		lm.drawText();
	    }
	    glPopMatrix();
	}
    }
    _VORRoseDL.end();
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
void VORRenderer::_createVORSymbols()
{
    // Radius of VOR symbol.
    const float size = 1.0;

    ////////////////////
    // VOR
    ////////////////////
    _VORSymbolDL.begin(); {
	glColor4fv(__vorColour);
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
    _VORSymbolDL.end();

    ////////////////////
    // VORTAC
    ////////////////////
    const float lobeThickness = size * 0.5;

    _VORTACSymbolDL.begin(); {
	_VORSymbolDL.call();
    
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
    _VORTACSymbolDL.end();

    ////////////////////
    // VOR-DME
    ////////////////////
    // Half the length of the long (top and bottom) side.
    const float longSide = size;
    // Half the length of the short (left and right) side.
    const float shortSide = sqrt(3.0) / 2.0 * size;

    _VORDMESymbolDL.begin(); {
	_VORSymbolDL.call();

	glBegin(GL_LINE_LOOP); {
	    glVertex2f(-longSide, -shortSide);
	    glVertex2f(-longSide, shortSide);
	    glVertex2f(longSide, shortSide);
	    glVertex2f(longSide, -shortSide);
	}
	glEnd();
    }
    _VORDMESymbolDL.end();
}

void VORRenderer::_getWaypoints(NavData *nd)
{
    const vector<Cullable *>& intersections = nd->hits(NavData::NAVAIDS);
    for (unsigned int i = 0; i < intersections.size(); i++) {
	VOR *vor = dynamic_cast<VOR *>(intersections[i]);
	if (vor) {
	    _waypoints.push_back(vor);
	}
    }
}

void VORRenderer::_draw(bool labels)
{
    assert(_currentPass == 0);

    // If we haven't created our basic symbols yet, do it now.  Note
    // that we could do this in the constructor, but only if there's a
    // valid OpenGL context at the time.  This, although less elegant,
    // is safer.
    if (!_VORRoseDL.valid()) {
	_createVORRose();
	_createVORSymbols();
    }

    // VORs (layer 0)
    _drawLayer(_layers[VORLayer], &VORRenderer::_drawVORs);

    // Radio "beams" (layer 1)
    if (_radioactive) {
	_drawLayer(_layers[RadioLayer], &VORRenderer::_drawRadios);
    }

    // Labels (layer 2)
    if (labels) {
	_drawLayer(_layers[LabelLayer], &VORRenderer::_drawLabels);
    }
}

// Drawing strategy:
//
// - draw icon, rose
// - draw icon
// - draw nothing
//
// - label with name / id, frequency and morse
// - label with id and frequency
// - label with id, shrinking as we move away
void VORRenderer::_drawVORs()
{
    for (size_t i = 0; i < _waypoints.size(); i++) {
	VOR *vor = dynamic_cast<VOR *>(_waypoints[i]);

	// Although we don't do this at the moment, we might want to
	// try different approaches to scaling the VOR.  For example,
	// we could give it a fixed screen size (eg, 100 pixels).  Or,
	// we could give it a fixed scale (eg, 10nm).  This is similar
	// to what paper VOR maps do.  Our current method is to have a
	// scale proportional to its range - more powerful VORs are
	// drawn larger.  Whatever the strategy, the end result is the
	// definition of 'radius', a value defined in screen pixels.
	float radius;
	if (!_iconVisible(vor, _isp, radius)) {
	    return;
	}

	geodPushMatrix(vor->bounds().center, vor->lat(), vor->lon()); {
	    ////////////////////
	    // VOR icon
	    ////////////////////
	    glPushMatrix(); {
		// We usually draw the icon at constant size.  However, we
		// never draw it larger than the VOR radius.
		float scale = radius * _metresPerPixel;
		glScalef(scale, scale, scale);

		glPushAttrib(GL_POINT_BIT); {
		    // This is the size of the point in the middle of the
		    // VOR.
		    glPointSize(3.0);

		    // How we draw the VOR depends on if it's a member of
		    // a navaid pair.
		    NavaidSystem *sys = NavaidSystem::owner(vor);
		    if (!sys) {
			// It's a standalone VOR.
			_VORSymbolDL.call();
		    } else if (dynamic_cast<VORTAC *>(sys)) {
			_VORTACSymbolDL.call();
		    } else {
			assert(dynamic_cast<VOR_DME *>(sys));
			_VORDMESymbolDL.call();
		    }
		}
		glPopAttrib();
	    }
	    glPopMatrix();

	    ////////////////////
	    // VOR rose
	    ////////////////////
	    if (_iconVisible(vor, _rsp, radius)) {
		// Calculate the line width for drawing the rose.  We
		// scale the line width because when zooming in, it
		// looks better if the lines become fatter.
		float lineWidth;
		if ((_lineScale * radius) > _maxLineWidth) {
		    lineWidth = _maxLineWidth;
		} else {
		    lineWidth = _lineScale * radius;
		}

		glScalef(radius * _metresPerPixel,
			 radius * _metresPerPixel,
			 radius * _metresPerPixel);
		glRotatef(-vor->variation(), 0.0, 0.0, 1.0);

		glPushAttrib(GL_LINE_BIT); {
		    glLineWidth(lineWidth);
	    
		    // Draw the VOR rose using the VOR colour.
		    glColor4fv(__vorColour);
		    _VORRoseDL.call();
		}
		glPopAttrib();
	    }
	}
	geodPopMatrix();
    }
}

void VORRenderer::_drawRadios()
{
    for (size_t i = 0; i < _waypoints.size(); i++) {
	VOR *vor = dynamic_cast<VOR *>(_waypoints[i]);

	geodPushMatrix(vor->bounds().center, vor->lat(), vor->lon()); {
	    // It's possible for zero, one, or both of the radios to
	    // be tuned in to the navaid.
	    double rad;
	    // NMEA tracks set their frequencies to 0, so these tests
	    // should always fail for NMEA tracks.
	    if (vor->frequency() == _p->nav1_freq) {
		rad = _p->nav1_rad + vor->variation();
		glPushMatrix(); {
		    glRotatef(-rad, 0.0, 0.0, 1.0);
		    glScalef(vor->range(), vor->range(), vor->range());
		    __createTriangle(_angularWidth, __clearColour, 
				     globals.vor1Colour);
		}
		glPopMatrix();
	    }
	    if (vor->frequency() == _p->nav2_freq) {
		rad = _p->nav2_rad + vor->variation();
		glPushMatrix(); {
		    glRotatef(-rad, 0.0, 0.0, 1.0);
		    glScalef(vor->range(), vor->range(), vor->range());
		    __createTriangle(_angularWidth, __clearColour, 
				     globals.vor2Colour);
		}
		glPopMatrix();
	    }
	}
	geodPopMatrix();
    }
}

void VORRenderer::_drawLabels()
{
    for (size_t i = 0; i < _waypoints.size(); i++) {
	VOR *vor = dynamic_cast<VOR *>(_waypoints[i]);

	float iconRadius;
	if (!_iconVisible(vor, _isp, iconRadius)) {
	    return;
	}

	// We ignore the return value because it's valid to draw the
	// VOR without a compass rose.
	float roseRadius;
	_iconVisible(vor, _rsp, roseRadius);

	// Range of VOR, in pixels.
	float range = vor->range() / _metresPerPixel;
	if (range < __smallLabel) {
	    return;
	}

	geodPushMatrix(vor->bounds().center, vor->lat(), vor->lon()); {
	    // Place the centre of the label halfway between the VOR
	    // centre and the southern rim, as long as it won't result
	    // in the label overwriting the icon.
	    float roseCentre = -roseRadius / 2.0 * _metresPerPixel,
		iconEdge = -(iconRadius + 5.0) * _metresPerPixel;
	    Label *l;
	    if (range > __maximumLabel) {
		// The whole kit and caboodle.
		l = __makeLabel("%N\n%F %I %M", vor, _isp.labelPointSize, 0, 
				roseCentre);
	    } else if (range > __mediumLabel) {
		// ID and frequency.
		l = __makeLabel("%F %I", vor, _isp.labelPointSize, 0, roseCentre);
	    } else {
		// Just the ID.
		l = __makeLabel("%I", vor, _isp.labelPointSize, 0, iconEdge, 
				LayoutManager::UC);
	    }

	    if (l->lm.y() > iconEdge) {
		l->lm.moveTo(0.0, iconEdge);
		l->lm.setAnchor(LayoutManager::UC);
	    }
	    __drawLabel(l);

	    delete l;
	}
	geodPopMatrix();
    }
}

//////////////////////////////////////////////////////////////////////
// NDBRenderer
//////////////////////////////////////////////////////////////////////

// Size of dots, relative to size of NDB icon.
const float NDBRenderer::_dotScale = 0.1;
// How fat to make NDB radials (degrees).
const float NDBRenderer::_angularWidth = 2.5;

NDBRenderer::NDBRenderer(): 
    WaypointRenderer(1, _LayerCount), _radioactive(false)
{
    _isp.rangeScaleFactor = 0.1;
    _isp.minSize = 1.0;
    // EYE - on Canadian maps, NDBs aren't quite so much bigger: 2.0
    // or less would be a better relative size.  They also differ in
    // colour, etc.  Maybe have different schemes?  This would give
    // users something to distract them with.
    _isp.maxSize = 2.5 * __iconSize;

    subscribe(Notification::AircraftMoved);
    subscribe(Notification::NewFlightTrack);
}


void NDBRenderer::notification(Notification::type n)
{
    if ((n == Notification::Moved) ||
	(n == Notification::Zoomed)) {
	_layers[NDBLayer].invalidate();
	_layers[LabelLayer].invalidate();
    } else if ((n == Notification::AircraftMoved) ||
	       (n == Notification::NewFlightTrack)) {
	// We don't need to redraw our radio layer if we have no NDBs
	// tuned now and didn't the last time we rendered.  Or, to put
	// it another way, we need to draw if we were tuned in before
	// or are tuned in now.  
	//
	// First, find out if we're tuned in to an NDB in our new
	// position.
	_p = globals.aw->ac()->currentPoint();
	const set<Navaid *>& navaids = _p->navaids();
	bool radioactive = 
	    count_if(navaids.begin(), navaids.end(), _isA<NDB *>);

	// Were we tuned in before or now?
	if (_radioactive || radioactive) {
	    _layers[RadioLayer].invalidate();
	}
	_radioactive = radioactive;
    }

    WaypointRenderer::notification(n);
}

// Create an NDB symbol and and NDB-DME symbol, in the WAC style.
void NDBRenderer::_createNDBSymbols()
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
    _NDBSymbolDL.begin(); {
	glColor4fv(__ndbColour);
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
    _NDBSymbolDL.end();

    ////////////////////
    // NDB-DME
    ////////////////////
    _NDBDMESymbolDL.begin(); {
	_NDBSymbolDL.call();

	// DME square
	glColor4fv(__vorColour);	// On US charts.
	glBegin(GL_LINE_LOOP); {
	    glVertex2f(-0.5, -0.5);
	    glVertex2f(0.5, -0.5);
	    glVertex2f(0.5, 0.5);
	    glVertex2f(-0.5, 0.5);
	}
	glEnd();
    }
    _NDBDMESymbolDL.end();
}

void NDBRenderer::_getWaypoints(NavData *nd)
{
    const vector<Cullable *>& intersections = nd->hits(NavData::NAVAIDS);
    for (unsigned int i = 0; i < intersections.size(); i++) {
	NDB *ndb = dynamic_cast<NDB *>(intersections[i]);
	if (ndb) {
	    _waypoints.push_back(ndb);
	}
    }
}

// Draw the navaids.  Draw their labels if 'labels' is true, and draw
// NDB radio "beams" if _radioactive is true.
void NDBRenderer::_draw(bool labels)
{
    assert(_currentPass == 0);

    // If we haven't created our basic symbols yet, do it now.
    if (!_NDBSymbolDL.valid()) {
	_createNDBSymbols();
    }

    // NDBs (layer 0)
    _drawLayer(_layers[NDBLayer], &NDBRenderer::_drawNDBs);

    // Radio "beams" (layer 1)
    if (_radioactive) {
	_drawLayer(_layers[RadioLayer], &NDBRenderer::_drawRadios);
    }

    // Labels (layer 2)
    if (labels) {
	_drawLayer(_layers[LabelLayer], &NDBRenderer::_drawLabels);
    }
}

// Draw the given NDB.
void NDBRenderer::_drawNDBs()
{
    for (size_t i = 0; i < _waypoints.size(); i++) {
	NDB *ndb = dynamic_cast<NDB *>(_waypoints[i]);
	// Scaled size of NDB icon, in pixels.
	float radius;
	if (!_iconVisible(ndb, _isp, radius)) {
	    return;
	}
    
	geodPushMatrix(ndb->bounds().center, ndb->lat(), ndb->lon()); {
	    ////////////////////
	    // NDB icon
	    ////////////////////
	    glPushAttrib(GL_POINT_BIT); {
		// The dots in the NDB scale with the NDB icon.
		glPointSize(radius * _dotScale);

		float scale = radius * _metresPerPixel;
		glScalef(scale, scale, scale);

		// How we draw and label the NDB depends on if it's a
		// member of a navaid pair.
		NavaidSystem *sys = NavaidSystem::owner(ndb);
		if (!sys) {
		    // Standalone NDB.
		    _NDBSymbolDL.call();
		} else {
		    assert(dynamic_cast<NDB_DME *>(sys));
		    _NDBDMESymbolDL.call();
		}
		// EYE - Do LOMs?  An LOM is just an NDB on top of an
		// outer marker.  However, of the 24 LOMs listed in
		// the 850 file, 16 don't have corresponding outer
		// markers.  What to do?
		//
		// A: check some LOMs on real VFR charts and see how
		// they're rendered.  Curiously, most of them are in
		// Denmark, and they're just rendered as regular NDBs.
		// I suspect the Danish LOMs are actually just NDBs,
		// and mislabelled in nav.dat.
	    }
	    glPopAttrib();
	}
	geodPopMatrix();
    }
}

void NDBRenderer::_drawRadios()
{
    for (size_t i = 0; i < _waypoints.size(); i++) {
	NDB *ndb = dynamic_cast<NDB *>(_waypoints[i]);
	// Are we tuned into this particular NDB?
	if (_p->navaids().count(ndb) > 0) {
	    geodPushMatrix(ndb->bounds().center, ndb->lat(), ndb->lon()); {
		// EYE - this seems like overkill.  Is there a simpler
		// way?  Really, I should be able to use a directly
		// calculated angle.  After all, the NDB doesn't
		// really care about the curvature of the earth.
		double rad, end, l;
		geo_inverse_wgs_84(ndb->lat(), ndb->lon(), 
				   _p->lat, _p->lon, 
				   &rad, &end, &l);
		glRotatef(180.0 - rad, 0.0, 0.0, 1.0);
		glScalef(ndb->range(), ndb->range(), ndb->range());
		__createTriangle(_angularWidth, globals.adfColour, 
				 globals.adfColour, false);
	    }
	    geodPopMatrix();
	}
    }
}

void NDBRenderer::_drawLabels()
{
    for (size_t i = 0; i < _waypoints.size(); i++) {
	NDB *ndb = dynamic_cast<NDB *>(_waypoints[i]);

	// Scaled size of NDB icon, in pixels.
	float radius;
	if (!_iconVisible(ndb, _isp, radius)) {
	    return;
	}

	// Range of NDB, in pixels.
	float range = ndb->range() / _metresPerPixel;
	if (range < __smallLabel) {
	    return;
	}

	// How we draw and label the NDB depends on if it's a member
	// of a navaid pair.
	NavaidSystem *sys = NavaidSystem::owner(ndb);

	geodPushMatrix(ndb->bounds().center, ndb->lat(), ndb->lon()); {
	    float labelOffset = (radius + 5.0) * _metresPerPixel;
	    LayoutManager::Point lp = LayoutManager::LC;
	    if (range > __maximumLabel) {
		if (!sys) {
		    // Standalone NDB.
		    __drawLabel("%N\n%F %I %M", ndb, _isp.labelPointSize, 0, 
				labelOffset, lp);
		} else {
		    assert(dynamic_cast<NDB_DME *>(sys));
		    // NDB-DME

		    // EYE - drawLabel should check for pairs.  Or, we
		    // could have a separate drawLabel for
		    // NavaidSystems.
		    __drawLabel("%N\n%F (%f) %I %M", ndb, _isp.labelPointSize, 
				0, labelOffset, lp);
		}
	    } else if (range > __mediumLabel) {
		if (!sys) {
		    // Standalone NDB.
		    __drawLabel("%F %I", ndb, _isp.labelPointSize, 0, labelOffset, 
				lp);
		} else {
		    // NDB-DME
		    __drawLabel("%F (%f) %I", ndb, _isp.labelPointSize, 0, 
				labelOffset, lp);
		}
	    } else {
		__drawLabel("%I", ndb, _isp.labelPointSize, 0, labelOffset, lp);
	    }
	}
	geodPopMatrix();
    }
}

//////////////////////////////////////////////////////////////////////
// DMERenderer
//////////////////////////////////////////////////////////////////////

DMERenderer::DMERenderer(): WaypointRenderer(1, _LayerCount)
{
    _isp.rangeScaleFactor = 0.1;
    _isp.minSize = 1.0;
    _isp.maxSize = __iconSize;
}

void DMERenderer::notification(Notification::type n)
{
    if ((n == Notification::Moved) ||
	(n == Notification::Zoomed)) {
	_layers[DMELayer].invalidate();
	_layers[LabelLayer].invalidate();
    }

    WaypointRenderer::notification(n);
}

// Creates DME symbols - TACANs and stand-alone DMEs (this includes
// DME and DME-ILS).  The others - VOR-DME, NDB-DME - are handled
// elsewhere.
void DMERenderer::_createDMESymbols()
{
    const float size = 1.0;
    const float lobeThickness = size * 0.5;

    ////////////////////
    // TACAN
    ////////////////////
    _TACANSymbolDL.begin(); {
	glColor4fv(__dmeColour);
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
    _TACANSymbolDL.end();

    ////////////////////
    // DME
    ////////////////////
    _DMESymbolDL.begin(); {
	// DME square
	glColor4fv(__dmeColour);
	glBegin(GL_LINE_LOOP); {
	    glVertex2f(-size, -size);
	    glVertex2f(size, -size);
	    glVertex2f(size, size);
	    glVertex2f(-size, size);
	}
	glEnd();
    }
    _DMESymbolDL.end();
}

void DMERenderer::_getWaypoints(NavData *nd)
{
    const vector<Cullable *>& intersections = nd->hits(NavData::NAVAIDS);
    for (unsigned int i = 0; i < intersections.size(); i++) {
	DME *dme = dynamic_cast<DME *>(intersections[i]);
	// DMEs are special, in that we only want to render ones that
	// are standalone.  If they occur as part of a navaid system
	// (VOR-DME, VORTAC, NDB-DME, or ILS), then they will be
	// rendered elsewhere.
	if (dme && !NavaidSystem::owner(dme)) {
	    _waypoints.push_back(dme);
	}
    }
}

void DMERenderer::_draw(bool labels)
{
    assert(_currentPass == 0);

    // If we haven't created our basic symbols yet, do it now.
    if (!_DMESymbolDL.valid()) {
	_createDMESymbols();
    }

    // DMEs (layer 0)
    _drawLayer(_layers[DMELayer], &DMERenderer::_drawDMEs);

    // Labels (layer 1)
    if (labels) {
	_drawLayer(_layers[LabelLayer], &DMERenderer::_drawLabels);
    }
}

// Renders a stand-alone DME.
void DMERenderer::_drawDMEs()
{
    for (size_t i = 0; i < _waypoints.size(); i++) {
	DME *dme = dynamic_cast<DME *>(_waypoints[i]);

	// Scaled size of DME, in pixels.  We try to draw the DME at
	// this radius, as long as it won't be too small or too big.
	float radius;
	if (!_iconVisible(dme, _isp, radius)) {
	    return;
	}

	// We draw DMEs in 2 different ways here: DMEs and TACANs.
	bool isTACAN;
	isTACAN = (dynamic_cast<TACAN *>(dme) != NULL);

	geodPushMatrix(dme->bounds().center, dme->lat(), dme->lon()); {
	    ////////////////////
	    // DME icon
	    ////////////////////
	    glPushMatrix();
	    glPushAttrib(GL_LINE_BIT); {
		// A line width of 1 makes it too hard to pick out, at
		// least when drawn in grey.
		glLineWidth(2.0);

		float scale = radius * _metresPerPixel;
		glScalef(scale, scale, scale);
	
		if (isTACAN) {
		    _TACANSymbolDL.call();
		} else {
		    _DMESymbolDL.call();
		}
	    }
	    glPopAttrib();
	    glPopMatrix();
	}
	geodPopMatrix();
    }
}

void DMERenderer::_drawLabels()
{
    for (size_t i = 0; i < _waypoints.size(); i++) {
	DME *dme = dynamic_cast<DME *>(_waypoints[i]);
	float radius;
	if (!_iconVisible(dme, _isp, radius)) {
	    return;
	}

	// Range of DME, in pixels.
	float range = dme->range() / _metresPerPixel;
	if (range < __smallLabel) {
	    return;
	}

	geodPushMatrix(dme->bounds().center, dme->lat(), dme->lon()); {
	    // Put the DME name, frequency, id, and morse code in a
	    // box above the icon (separated by a bit of space).  The
	    // box has a translucent white background with a solid
	    // border to make it easier to read.  TACANs have an
	    // explicit "DME" in their labels (unless we've zoomed out
	    // too far).
	    float labelOffset = (radius + 5.0) * _metresPerPixel;
	    LayoutManager::Point lp = LayoutManager::LC;
	    bool isTACAN = (dynamic_cast<TACAN *>(dme) != NULL);

	    if (range > __maximumLabel) {
		if (isTACAN) {
		    __drawLabel("%N\nDME %F %I %M", dme, _isp.labelPointSize, 
				0.0, labelOffset, lp);
		} else {
		    __drawLabel("%N\n%F %I %M", dme, _isp.labelPointSize, 0.0, 
				labelOffset, lp);
		}
	    } else if (range > __mediumLabel) {
		if (isTACAN) {
		    __drawLabel("DME %F %I", dme, _isp.labelPointSize, 0.0, 
				labelOffset, lp);
		} else {
		    __drawLabel("%F %I", dme, _isp.labelPointSize, 0.0, 
				labelOffset, lp);
		}
	    } else {
		__drawLabel("%I", dme, _isp.labelPointSize, 0.0, labelOffset, 
			    lp);
	    }
	}
	geodPopMatrix();
    }
}

//////////////////////////////////////////////////////////////////////
// FixRenderer
//////////////////////////////////////////////////////////////////////

// EYE - make these part of class?  Put above where all the other
// constants are?

// Above this level, no fixes are drawn.
const float noLevel = 1250.0;
// Above this level, but below noLevel, enroute fixes are drawn.
const float enrouteLevel = 250.0;
// EYE - doesn't seem to be true - we draw both
// Below enRouteLevel, only approach fixes are drawn.

// Bright yellow.
const float enroute_fix_colour[4] = {1.0, 1.0, 0.0, 0.7};
const float terminal_fix_colour[4] = {1.0, 0.0, 1.0, 0.7};

// EYE - match __ilsLabelColour?
const float fix_label_colour[4] = {0.2, 0.2, 0.2, 0.7};

FixRenderer::FixRenderer(): WaypointRenderer(1, _LayerCount)
{
}

void FixRenderer::notification(Notification::type n)
{
    if ((n == Notification::Moved) ||
	(n == Notification::Zoomed)) {
	_layers[FixLayer].invalidate();
	_layers[LabelLayer].invalidate();
    }

    WaypointRenderer::notification(n);
}

void FixRenderer::_getWaypoints(NavData *nd)
{
    // If we're zoomed out far, we don't do anything.
    if (_metresPerPixel > noLevel) {
	return;
    }

    const vector<Cullable *>& intersections = nd->hits(NavData::FIXES);
    for (unsigned int i = 0; i < intersections.size(); i++) {
	Fix *fix = dynamic_cast<Fix *>(intersections[i]);
	if (fix) {
	    _waypoints.push_back(fix);
	}
    }
}

void FixRenderer::_draw(bool labels)
{
    assert(_currentPass == 0);

    // If we're zoomed out far, we don't draw anything.
    if (_metresPerPixel >= noLevel) {
	return;
    }

    // Fixes (layer 0)
    _drawLayer(_layers[FixLayer], &FixRenderer::_drawFixes);

    // Labels (layer 1)
    if (labels) {
	_drawLayer(_layers[LabelLayer], &FixRenderer::_drawLabels);
    }
}

// Renders a stand-alone Fix (a dot).
void FixRenderer::_drawFixes()
{
    for (size_t i = 0; i < _waypoints.size(); i++) {
	Fix *fix = dynamic_cast<Fix *>(_waypoints[i]);

	// EYE - another constant
	// Size of point used to represent the fix.
	const float fixSize = 4.0;

	if (!fix->isTerminal() && (_metresPerPixel < noLevel)) {
	    glColor4fv(enroute_fix_colour);
	} else if (fix->isTerminal() && (_metresPerPixel < enrouteLevel)) {
	    glColor4fv(terminal_fix_colour);
	} else {
	    continue;
	}

	glPushAttrib(GL_POINT_BIT); {
	    // We use a non-standard point size, so we need to wrap
	    // this in a glPushAttrib().
	    glPointSize(fixSize);
	    geodPushMatrix(fix->bounds().center, fix->lat(), fix->lon()); {
		glBegin(GL_POINTS); {
		    glVertex2f(0.0, 0.0);
		}
		glEnd();
	    }
	    geodPopMatrix();
	}
	glPopAttrib();
    }
}

void FixRenderer::_drawLabels()
{
    // EYE - magic number
    const float labelOffset = _metresPerPixel * 5.0;

    // EYE - make lm part of class?
    LayoutManager lm;
    glColor4fv(fix_label_colour);
    lm.setFont(globals.aw->regularFont(), _labelPointSize);
    for (size_t i = 0; i < _waypoints.size(); i++) {
	Fix *fix = dynamic_cast<Fix *>(_waypoints[i]);

	lm.setText(fix->id());
	if (!fix->isTerminal() && (_metresPerPixel < noLevel)) {
	    lm.moveTo(labelOffset, 0.0, LayoutManager::CL);
	} else if (fix->isTerminal() && (_metresPerPixel < enrouteLevel)) {
	    lm.moveTo(-labelOffset, 0.0, LayoutManager::CR);
	} else {
	    continue;
	}

	geodDrawText(lm, fix->bounds().center, fix->lat(), fix->lon());
    }
}

//////////////////////////////////////////////////////////////////////
// ILSRenderer
//////////////////////////////////////////////////////////////////////

// How much to scale the ILS DME icon compared to a standard icon.
const float ILSRenderer::_DMEScale = 0.5;

ILSRenderer::ILSRenderer(): 
    WaypointRenderer(2, _LayerCount), _radioactive(false)
{
    subscribe(Notification::AircraftMoved);
    subscribe(Notification::NewFlightTrack);
    subscribe(Notification::MagTrue);
}

void ILSRenderer::notification(Notification::type n)
{
    if ((n == Notification::Moved) ||
	(n == Notification::Zoomed)) {
	_layers[MarkerLayer].invalidate();
	_layers[LOCLayer].invalidate();
	_layers[LOCLabelLayer].invalidate();
	_layers[DMELayer].invalidate();
	_layers[DMELabelLayer].invalidate();
    } else if ((n == Notification::AircraftMoved) ||
	       (n == Notification::NewFlightTrack)) {
	// Note that there's no radio layer for an ILS - when radios
	// change, we need to re-render the ILS and its label.
	_layers[LOCLayer].invalidate();
	_layers[LOCLabelLayer].invalidate();

	_p = globals.aw->ac()->currentPoint();
	const set<Navaid *>& navaids = _p->navaids();
	_radioactive = count_if(navaids.begin(), navaids.end(), _isA<LOC *>);
    } else if (n == Notification::MagTrue) {
	_layers[LOCLabelLayer].invalidate();
    }

    WaypointRenderer::notification(n);
}

// Creates ILS localizer symbol, with a length of 1.  The symbol is
// drawn in the x-y plane, with the pointy end at 0,0, and the other
// end at 0, -1.
void ILSRenderer::_createILSSymbols()
{
    _createILSSymbol(_ILSSymbolDL, __ilsColour);
    _createILSSymbol(_LOCSymbolDL, __locColour);
}

// Creates a single ILS-type symbol, for the given display list
// variable, in the given colour.
void ILSRenderer::_createILSSymbol(DisplayList& dl, const float *colour)
{
    dl.begin(); {
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
    dl.end();
}

// Creates 3 marker symbols, with units in metres.  The symbols are
// drawn in the x-y plane, oriented with the long axis along the y
// axis, and the centre at 0, 0.
void ILSRenderer::_createMarkerSymbols()
{
    // Resolution of our arcs.
    const int segments = 10;

    for (int i = Marker::OUTER; i < Marker::_LAST; i++) {
	_ILSMarkerDLs[i].begin(); {
	    const float offset = 
		cos(30.0 * SG_DEGREES_TO_RADIANS) * __markerRadii[i];

	    glColor4fv(__markerColours[i]);
	    glBegin(GL_POLYGON); {
		// Draw first arc (counterclockwise).
		for (int j = 0; j < segments; j++) {
		    float pHdg = (segments / 2 - j) * (60.0 / segments) 
			* SG_DEGREES_TO_RADIANS;
		    glVertex2f(offset - cos(pHdg) * __markerRadii[i], 
			       sin(pHdg) * __markerRadii[i]);
		}

		// Now the other arc.
		for (int j = 0; j < segments; j++) {
		    float pHdg = (segments / 2 - j) * (60.0 / segments) 
			* SG_DEGREES_TO_RADIANS;
		    glVertex2f(cos(pHdg) * __markerRadii[i] - offset, 
			       -sin(pHdg) * __markerRadii[i]);
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
		    glVertex2f(offset - cos(pHdg) * __markerRadii[i], 
			       sin(pHdg) * __markerRadii[i]);
		}

		// Now the other arc.
		for (int j = 0; j < segments; j++) {
		    float pHdg = (segments / 2 - j) * (60.0 / segments) 
			* SG_DEGREES_TO_RADIANS;
		    glVertex2f(cos(pHdg) * __markerRadii[i] - offset,
			       -sin(pHdg) * __markerRadii[i]);
		}
	    }
	    glEnd();
	}
	_ILSMarkerDLs[i].end();
    }
}

void ILSRenderer::_createDMESymbol()
{
    const float size = 1.0;

    _DMESymbolDL.begin(); {
	// Small circle with a dot in the middle.
	glColor4fv(__dmeColour);
	glBegin(GL_LINE_LOOP); {
	    const int subdivision = 30;	// 30-degree steps

	    // Draw the circle.
	    for (int i = 0; i < 360; i += subdivision) {
		float theta, x, y;

		// Draw circle segment.
		theta = i * SG_DEGREES_TO_RADIANS;
		x = sin(theta) * size;
		y = cos(theta) * size;
		glVertex2f(x, y);
	    }
	}
	glEnd();
	    
	// And the dot.
	glBegin(GL_POINTS); {
	    glVertex2f(0.0, 0.0);
	}
	glEnd();
    }
    _DMESymbolDL.end();
}

void ILSRenderer::_getWaypoints(NavData *nd)
{
    const vector<Cullable *>& intersections = nd->hits(NavData::NAVAIDS);
    for (unsigned int i = 0; i < intersections.size(); i++) {
	if (LOC *loc = dynamic_cast<LOC *>(intersections[i])) {
	    _waypoints.push_back(loc);
	}
    }
}

void ILSRenderer::_draw(bool labels)
{
    // If we haven't created our basic symbols yet, do it now.
    if (!_ILSSymbolDL.valid()) {
	_createILSSymbols();
	_createMarkerSymbols();
	_createDMESymbol();
    }

    if (_currentPass == 0) {
	// 0: Markers
	_drawLayer(_layers[MarkerLayer], &ILSRenderer::_drawMarkers);

	// 1: Localizers (tuned-in and not tuned-in)
	_drawLayer(_layers[LOCLayer], &ILSRenderer::_drawLOCs);

	// 2: Localizer labels
	if (labels) {
	    _drawLayer(_layers[LOCLabelLayer], &ILSRenderer::_drawLOCLabels);
	}
    } else if (_currentPass == 1) {
	// 3: DMEs
	_drawLayer(_layers[DMELayer], &ILSRenderer::_drawDMEs);

	// 4: DME labels
	if (labels) {
	    _drawLayer(_layers[DMELabelLayer], &ILSRenderer::_drawDMELabels);
	}
    } else {
	assert(false);
    }
}

// Returns true if the given ILS should be drawn or not.  If visible,
// 'p' is filled in with the appropriate drawing parameters.
bool ILSRenderer::_ILSVisible(ILS *ils, DrawingParams& p)
{
    // Drawn length of an untuned localizer, in nautical miles.
    const float standardLength = 7.5;

    // Minimum length, in pixels, of a localizer.  If a localizer is
    // scaled to less than this length, it will not be drawn.
    const float minimumLength = 100.0;

    // The point at which we transition from fixed font sizes to
    // scaled fonts.  When the zoom level (_metresPerPixel) is less
    // than this, we use a fixed-size font.  When greater than this,
    // we scale linearly.
    const float fontScalingLevel = 50.0;

    p.loc = ils->loc();
    p.length = standardLength * SG_NM_TO_METER;
    p.live = false;
    if (_radioactive) {
	// NMEA tracks set their frequencies to 0, so these tests
	// should always fail for NMEA tracks.
	if (p.loc->frequency() == _p->nav1_freq) {
	    // When an ILS is tuned in, we draw it differently - it is
	    // drawn to its true length, and we use the radio colour
	    // to colour it.
	    p.length = p.loc->range();
	    p.colour = globals.vor1Colour;
	    p.live = true;
	} else if (p.loc->frequency() == _p->nav2_freq) {
	    p.length = p.loc->range();
	    p.colour = globals.vor2Colour;
	    p.live = true;
	}
    }    

    p.pointSize = _labelPointSize;
    if (_metresPerPixel > fontScalingLevel) {
	p.pointSize *= fontScalingLevel / _metresPerPixel;
    }

    return (p.length / _metresPerPixel > minimumLength);
}

void ILSRenderer::_drawMarkers()
{
    for (size_t i = 0; i < _waypoints.size(); i++) {
	ILS *ils = ILS::ils(dynamic_cast<LOC *>(_waypoints[i]));

	DrawingParams p;
	if (!_ILSVisible(ils, p)) {
	    return;
	}

	const set<Marker *>& markers = ils->markers();
	set<Marker *>::iterator it;
	for (it = markers.begin(); it != markers.end(); it++) {
	    Marker *m = *it;

	    // If we're getting desperate, we could test marker sizes,
	    // and not draw them if they're too small.  But it's
	    // easier just to draw them whenever the ILS as a whole is
	    // rendered.
	    geodPushMatrix(m->bounds().center, m->lat(), m->lon()); {
		glRotatef(-m->heading() + 90.0, 0.0, 0.0, 1.0);
		_ILSMarkerDLs[m->type()].call();
	    }
	    geodPopMatrix();
	}
    }
}

void ILSRenderer::_drawLOCs()
{
    for (size_t i = 0; i < _waypoints.size(); i++) {
	ILS *ils = ILS::ils(dynamic_cast<LOC *>(_waypoints[i]));

	DrawingParams p;
	if (!_ILSVisible(ils, p)) {
	    return;
	}

	// EYE - the ilsWidth value is only used for live localizers.
	// Should we use it when creating the ILS localizer symbols?

	// Localizers are drawn 3 degrees wide.  This is an
	// approximation, as localizer angular widths actually vary.
	// According to the FAA AIM, localizers are adjusted so that
	// they are 700' wide at the runway threshold.  Localizers on
	// long runways then, have a smaller angular width than those
	// on short runways.  We're not going for full physical
	// accuracy, but rather for a symbolic representation of
	// reality, so 3.0 is good enough.
	const float ilsWidth = 3.0;

	geodPushMatrix(p.loc->bounds().center, p.loc->lat(), p.loc->lon()); {
	    glPushMatrix(); {
		glRotatef(-p.loc->heading(), 0.0, 0.0, 1.0);
		glScalef(p.length, p.length, p.length);
		if (p.live) {
		    // // We draw two triangles: one for the front course
		    // // and one for the back course.
		    // __createTriangle(ilsWidth, __clearColour, ilsColour, true);
		    // Don't draw a back course.
		    __createTriangle(ilsWidth, __clearColour, p.colour, false);
		} else if (ils->gs()) {
		    _ILSSymbolDL.call();
		} else {
		    _LOCSymbolDL.call();
		}
	    }
	    glPopMatrix();
	}
	geodPopMatrix();
    }
}

// ILS
//
// - freq, runway
// - freq, runway, heading
// - freq, runway, heading, id, morse
// - freq, full name (w/o airport), heading, id, morse
//
// note: ILS name is: <airport> <runway> <type> (eg, KSFO 19L ILS-CAT-I)
//
// full name | heading
// freq      |
//
// full name	   | heading
// freq, id, morse |
void ILSRenderer::_drawLOCLabels()
{
    for (size_t i = 0; i < _waypoints.size(); i++) {
	ILS *ils = ILS::ils(dynamic_cast<LOC *>(_waypoints[i]));

	DrawingParams p;
	if (!_ILSVisible(ils, p)) {
	    return;
	}

	geodPushMatrix(p.loc->bounds().center, p.loc->lat(), p.loc->lon()); {
	    // Label the ILS.
	    if (p.loc->heading() < 180.0) {
		glRotatef(-(p.loc->heading() + 270.0), 0.0, 0.0, 1.0);
	    } else {
		glRotatef(-(p.loc->heading() + 90.0), 0.0, 0.0, 1.0);
	    }

	    // Slightly translucent colours look better than opaque
	    // ones, and they look better if there's a coloured
	    // background (as we have with the box around VORs and
	    // NDBs).
	    glColor4fv(__ilsLabelColour);
	    float offset;
	    if (p.loc->heading() < 180.0) {
		// EYE - a bit ugly - because we're using
		// __renderMorse(), we have to have GL units as metres
		// (fix this somehow?), so we can't call glScalef(),
		// so we have to scale everything ourselves.
		offset = -0.5 * p.length;
	    } else {
		offset = 0.5 * p.length;
	    }

	    // We draw the ILS in a single style, but it might be
	    // better to alter it depending on the scale.
	    __drawLabel("RWY %N\n%F %I %M", p.loc, p.pointSize, offset, 0.0);

	    // Now add a heading near the end.
	    // EYE - magic number
	    offset *= 1.75;
	    LayoutManager lm;
	    // EYE - magic number
	    lm.setFont(globals.aw->regularFont(), p.pointSize * 1.25);
	    lm.begin(offset, 0.0);
	    // EYE - just record this once, when the navaid is loaded?
	    double magvar = 0.0;
	    const char *magTrue = "T";
	    // EYE - these accessor chains are getting pretty long.
	    if (globals.aw->ac()->magTrue()) {
		magvar = magneticVariation(p.loc->lat(), p.loc->lon(), 
					   p.loc->elev());
		magTrue = "";
	    }
	    int heading = 
		normalizeHeading(rint(p.loc->heading() - magvar), false);

	    // EYE - we should add the glideslope too, if it has one
	    // (eg, "284@3.00")

	    globals.str.printf("%03d%c%s", heading, degreeSymbol, magTrue);
	    lm.addText(globals.str.str());
	    lm.end();

	    glColor4fv(__ilsLabelColour);
	    lm.drawText();
	}
	geodPopMatrix();
    }
}

void ILSRenderer::_drawDMEs()
{
    // EYE - if performance is an issue, it might help to move some of
    // the OpenGL calls to the outside of this loop.  For example, we
    // don't need to set the line width over and over again - it only
    // needs to be done once.  This applies to all the _drawFoos()
    // methods.
    for (size_t i = 0; i < _waypoints.size(); i++) {
	ILS *ils = ILS::ils(dynamic_cast<LOC *>(_waypoints[i]));

	DME *dme = ils->dme();
	if (!dme) {
	    // Not all ILS systems have DMEs.
	    continue;
	}

	DrawingParams p;
	if (!_ILSVisible(ils, p)) {
	    return;
	}

	float scale = _DMEScale * __iconSize * _metresPerPixel;

	geodPushMatrix(dme->bounds().center, dme->lat(), dme->lon()); {
	    glPushMatrix();
	    glPushAttrib(GL_LINE_BIT); {
		// A line width of 1 makes it too hard to pick out, at
		// least when drawn in grey.
		glLineWidth(2.0);

		glScalef(scale, scale, scale);
		_DMESymbolDL.call();
	    }
	    glPopAttrib();
	    glPopMatrix();
	}
	geodPopMatrix();
    }
}

void ILSRenderer::_drawDMELabels()
{
    for (size_t i = 0; i < _waypoints.size(); i++) {
	ILS *ils = ILS::ils(dynamic_cast<LOC *>(_waypoints[i]));

	DME *dme = ils->dme();
	if (!dme) {
	    // Not all ILS systems have DMEs.
	    continue;
	}

	DrawingParams p;
	if (!_ILSVisible(ils, p)) {
	    return;
	}

	// EYE - should we scale the DME as we zoom out?  If so, we
	// need to decide the point at which the scaling should begin.
	// We could adopt the approach taken in _ILSVisible for
	// scaling fonts, where scaling begins at a fixed zoom level.
	float iconSize = _DMEScale * __iconSize;

	geodPushMatrix(dme->bounds().center, dme->lat(), dme->lon()); {
	    // Put the DME name, frequency, id, and morse code in a
	    // box above the icon (separated by a bit of space).  The
	    // box has a translucent white background with a solid
	    // border to make it easier to read.
	    float offset = (iconSize + 5.0) * _metresPerPixel;
	    LayoutManager::Point lp = LayoutManager::LC;

	    // EYE - what about overlapping DMEs?  London Heathrow has
	    // ILL and IBB on top of each other, as well as IAA and
	    // IRR.  Is this common?  It's not a mistake, as the EGLL
	    // airport diagram shows them co-located.  Tricky.

	    // Since ILS DMEs have the same frequency as the
	    // corresponding localizer, we don't display frequencies.
	    __drawLabel("%I", dme, p.pointSize, 0.0, offset, lp);
	}
	geodPopMatrix();
    }
}

//////////////////////////////////////////////////////////////////////
// NavaidsOverlay
//////////////////////////////////////////////////////////////////////

void NavaidsOverlay::draw(NavData *navData, Overlays::OverlayType t, 
			  bool labels)
{
    // EYE - VORs/VORS/VOR?  FIXES/FIX/Fixes/Fix?  Overlays.hxx should
    // be a bit more consistent.

    // EYE - what, in general, should we pass in, and what should be
    // part of the class?  For example, should we maintain a pointer
    // to NavData, or pass it in?  Should the parameters to the
    // drawing routine (labels on/off, default font, ...) be be in the
    // class (directly or indirectly, via a pointer to Globals or
    // Overlays) or passed in?  Is the goal to make access as direct
    // as possible, or as central and consistent as possible?

    if (t == Overlays::VOR) {
	_vr.draw(navData, labels);
    } else if (t == Overlays::NDB) {
	_nr.draw(navData, labels);
    } else if (t == Overlays::DME) {
	_dr.draw(navData, labels);
    } else if (t == Overlays::FIXES) {
	_fr.draw(navData, labels);
    } else if (t == Overlays::ILS) {
	_ir.draw(navData, labels);
    }
}
