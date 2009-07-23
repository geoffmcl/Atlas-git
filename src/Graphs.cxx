/*-------------------------------------------------------------------------
  Graphs.cxx

  Written by Brian Schack

  Copyright (C) 2007 Brian Schack

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

#include <simgear/compiler.h>
#include <simgear/sg_inlines.h>

#include <cassert>

#include "Graphs.hxx"
#include "misc.hxx"
#include "NavaidsOverlay.hxx"
#include "Globals.hxx"

// EYE - eventually these should be placed in preferences
extern float vor1Colour[];
extern float vor2Colour[];

float axisColour[4] = {0.0, 0.0, 0.0, 1.0};
float labelColour[4] = {0.0, 0.0, 0.0, 1.0};
float majorTickColour[4] = {0.5, 0.5, 0.5, 1.0};
float minorTickColour[4] = {0.75, 0.75, 0.75, 1.0};
float graphColour[4] = {1.0, 0.0, 0.0, 1.0};
float glideslopeOutlineColour[4] = {0.75, 0.75, 0.75, 1.0};

Graphs::Graphs(int window): 
    _window(window), _track(NULL), _graphTypes(0), _smoothing(10),
    _shouldRerender(false), _shouldReload(false), _graphDL(0)
{
    // Default mark and live aircraft colours are black.
    GLfloat black[4] = {0.0, 0.0, 0.0, 1.0};
    setAircraftColor(black);
    setMarkColor(black);

    subscribe(Notification::AircraftMoved);
    subscribe(Notification::FlightTrackModified);
    subscribe(Notification::NewFlightTrack);
}

Graphs::~Graphs()
{
    for (unsigned int i = 0; i < _GSs.size(); i++) {
	delete _GSs[i];
    }
}

// Draws all of our graphs (as given by graphTypes) into our window.
// Graphs span the window horizontally, and each graph gets equal
// space vertically.  The variables _shouldRerender and _shouldReload
// determine what work is actually performed.
//
// Assumes that _track is valid and that the _graphTypes value is
// correct.
void Graphs::draw()
{
    assert(glutGetWindow() == _window);

    if (_shouldRerender) {
	if (_shouldReload) {
	    _loadData();

	    // Set our title.
	    glutSetWindowTitle(_track->niceName());
	}

	glDeleteLists(_graphDL, 1);
	_graphDL = glGenLists(1);
	assert(_graphDL != 0);
	glNewList(_graphDL, GL_COMPILE); {
	    // Clear everything to white.
	    glClearColor(1.0, 1.0, 1.0, 0.0);
	    glClear(GL_COLOR_BUFFER_BIT);

	    // What graphs do we need to plot?
	    int graphCount = 0;
	    if (graphTypes() & Graphs::ALTITUDE) {
		graphCount++;
	    }
	    if (graphTypes() & Graphs::SPEED) {
		graphCount++;
	    }
	    if (graphTypes() & Graphs::CLIMB_RATE) {
		graphCount++;
	    }

	    // No graphs to plot.  Just return.
	    if (graphCount == 0) {
		return;
	    }

	    glPushAttrib(GL_ENABLE_BIT); {
		// Now draw the graphs.  x and y designates the
		// lower-left corner of the actual graph, ignoring
		// labels.
		int x = _margin, y = _header;

		_times.pixels = _w - (_margin * 2);
		_calcNiceIntervals(_times);
		if (graphTypes() & Graphs::CLIMB_RATE) {
		    _rateOfClimb.pixels = (_h / graphCount) - (_header * 2);
		    _calcNiceIntervals(_rateOfClimb);
		    _drawGraph(_rateOfClimb, x, y, "Climb Rate (ft/min)");
		    y += _rateOfClimb.pixels + (2 * _header);
		}
		if (graphTypes() & Graphs::SPEED) {
		    _speed.pixels = (_h / graphCount) - (_header * 2);
		    _calcNiceIntervals(_speed);
		    _drawGraph(_speed, x, y, "Speed (kt)");
		    y += _speed.pixels + (2 * _header);
		}
		if (graphTypes() & Graphs::ALTITUDE) {
		    _altitude.pixels = (_h / graphCount) - (_header * 2);
		    _calcNiceIntervals(_altitude);
		    _drawGS(x, y);
		    _drawGraph(_altitude, x, y, "Altitude (ft)");
		}
	    }
	    glPopAttrib();
	}
	glEndList();
    }

    glCallList(_graphDL);

    // EYE - we no longer maintain a live mark!

    // Draw current mark and, if it's live, current aircraft position
    // (which is always at the end of the track, but it's drawn just
    // to reinforce the fact that it's live).  We draw the mark using
    // time coordinates in the x-axis, pixel coordinates in the
    // y-axis.  Note to self - the transformations happen in reverse
    // of the way they're given.  In other words, the raw time is
    // first translated by -times.first, then scaled, then translated
    // by x.  EYE - is that really right?
    glPushAttrib(GL_LINE_BIT); {
	// We draw the mark unsmoothed.
	glDisable(GL_LINE_SMOOTH);

	glPushMatrix(); {
	    glTranslatef(_margin, 0.0, 0.0);
	    glScalef(_times.pixels / (_times.last - _times.first), 1.0, 0.0);
	    glTranslatef(-_times.first, 0.0, 0.0);

	    if (_track->mark() >= 0) {
		glBegin(GL_LINES); {
		    glColor4fv(_markColour);
		    glVertex2f(_times.data[_track->mark()], 0.0);
		    glVertex2f(_times.data[_track->mark()], (float)_h);
		}
		glEnd();
	    }
	    if (_track->live()) {
		glBegin(GL_LINES); {
		    glColor4fv(_aircraftColour);
		    glVertex2f(_times.max, 0.0);
		    glVertex2f(_times.max, (float)_h);
		}
		glEnd();
	    }
	}
	glPopMatrix();
    }
    glPopAttrib();

    _shouldRerender = _shouldReload = false;
}

void Graphs::setAircraftColor(const float *color)
{
    memcpy(_aircraftColour, color, sizeof(float) * 4);
}

void Graphs::setMarkColor(const float *color)
{
    memcpy(_markColour, color, sizeof(float) * 4);
}

// Finds and returns index of the record in the flight track closest
// to the given x coordinate in the window.  Because time is
// irregular, finding the corresponding point in the flight track
// requires a search.  We're stupid and just do a linear search from
// the beginning - we really should be a bit smarter about it.
int Graphs::pixelToPoint(int x)
{
    assert(glutGetWindow() == _window);

    // Reload our data if necessary.
    if (_shouldReload) {
	_loadData();
	_shouldReload = false;
    }

    // Figure out how to transform window values to graph values.  Our
    // graph has a regular scale - each pixel corresponds to a fixed
    // amount of time.  The x coordinate therefore corresponds to a
    // certain time.
    float scale = (_times.last - _times.first) / _times.pixels;
    int left = (_times.min - _times.first) / scale + _margin;
    int right = (_times.max - _times.first) / scale + _margin;
    // Convert out-of-range values to in-range values.
    if (x < left) {
	x = left;
    } else if (x > right) {
	x = right;
    }

    // Find a point close to our x value, this time expressed as time.
    // Note that we may not have an exact match, so we try to find the
    // closest.
    float time = (x - left) * scale + _times.min;
    for (unsigned int i = 0; i < _times.data.size(); i++) {
	if (_times.data[i] > time) {
	    // We've passed the cutoff.  Check if the point we're at
	    // is better than the point we just passed.
	    if (i == 0) {
		return i;
	    }
	    // Note that this bit of code assumes that time increases,
	    // which isn't always true.  On the other hand, we don't
	    // have any god way to handle non-increasing time values,
	    // so we'll just close our eyes and hope nothing happens.
	    float time0 = _times.data[i] - time;
	    float time1 = time - _times.data[i - 1];
	    if (time0 < time1) {
		return i;
	    } else {
		return (i - 1);
	    }
	}
    }

    // At the end.
    return (_track->size() - 1);
}

void Graphs::reshape(int w, int h)
{
    _shouldRerender = true;

    _w = w;
    _h = h;
}

int Graphs::graphTypes()
{
    return _graphTypes;
}

void Graphs::setGraphTypes(int types)
{
    _graphTypes = types;
    _shouldRerender = true;
}

FlightTrack *Graphs::flightTrack()
{
    return _track;
}

void Graphs::setFlightTrack(FlightTrack *t)
{
    if (_track == t) {
	// If the new flight track is the same as the old, don't do
	// anything.
	return;
    }

    _track = t;
    _shouldRerender = _shouldReload = true;
}

unsigned int Graphs::smoothing()
{
    return _smoothing;
}

// EYE - make smoothing a float?
void Graphs::setSmoothing(unsigned int s)
{
    if (s == _smoothing) {
	return;
    }

    _smoothing = s;

    // We need to reload the data because the rate of climb graph data
    // depends on the smoothing interval.
    _shouldRerender = _shouldReload = true;
}

// This routine receives notifications of events that we've subscribed
// to.  Basically it translates from some outside event, like the
// flight track being modified, to some internal action or future
// action (eg, setting _shouldRerender to true).
//
// In general, we try to postpone as much work as possible, merely
// recording the fact that work needs to be done.  Later, in the
// draw() routine, we actually do some real work (which means that
// someone, somewhere has to call draw(), because we never do it
// ourselves).  The reason for this strategy is that we can
// potentially receive many notifications per drawing request.
bool Graphs::notification(Notification::type n)
{
    if (n == Notification::AircraftMoved) {
	// At the moment we don't do anything when informed of
	// aircraft movement, since we unconditionally draw the
	// aircraft mark.
    } else if (n == Notification::FlightTrackModified) {
	_shouldRerender = true;
	_shouldReload = true;
    } else if (n == Notification::NewFlightTrack) {
	setFlightTrack(globals.track());
    } else {
	assert(false);
    }

    return true;
}

// The workhorse routine.  This does the actual plotting of a set of
// values, in the given area of the window, with the given label.  It
// assumes that _track is valid, and that the graph window has been
// cleared.  The values x and y refer the the lower-left corner of the
// actual graph.  _drawGraph assumes that there is space around the
// graph for labels, specifically _header pixels on the top and
// bottom, and _margin pixels on the left and right.
void Graphs::_drawGraph(Values &values, int x, int y, const char *label)
{
    glPushAttrib(GL_LINE_BIT); {
	// We draw the graph axes and ticks unsmoothed - it looks nicer.
	glDisable(GL_LINE_SMOOTH);

	// Print the graph axes.
	glColor4fv(axisColour);
	glBegin(GL_LINE_STRIP); {
	    glVertex2i(x, y + values.pixels);
	    glVertex2i(x, y);
	    glVertex2i(x + _times.pixels, y);
	}
	glEnd();

	// EYE - use vertex arrays?  See
	// http://www.glprogramming.com/red/chapter02.html

	// Don't do anything unless there are at least 2 points.
	if (values.data.size() >= 2) {
	    // Draw the labels and "ticks" for the y (data) axis.  Change
	    // the y transformation.
	    glPushMatrix();
	    glTranslatef(0.0, y, 0.0);
	    glScalef(1.0, values.pixels / (values.last - values.first), 0.0);
	    glTranslatef(0.0, -values.first, 0.0);

	    // To avoid problems with increasing round-off errors, we use
	    // an integer to control the loop.
	    int intervals = rint((values.last - values.first) / values.d);
	    int majorTicks = 0;
	    glBegin(GL_LINES); {
		for (int i = 1; i <= intervals; i++) {
		    float tick = values.first + (i * values.d);
		    glColor4fv(axisColour);
		    glVertex2f(x, tick);
		    if (fabs(remainderf(tick, values.D)) < 0.00001) {
			// Major tick.
			glVertex2f(x + 10, tick);

			glColor4fv(majorTickColour);
			glVertex2f(x + 10, tick);
			glVertex2f(x + _times.pixels, tick);
		
			majorTicks++;
		    } else {
			// Minor tick.
			glVertex2f(x + 5, tick);

			glColor4fv(minorTickColour);
			glVertex2f(x + 5, tick);
			glVertex2f(x + _times.pixels, tick);
		    }
		}
	    }
	    glEnd();
    
	    // Now label the y axis.
	    char format[10];
	    sprintf(format, "%%.%df", values.decimals);
	    AtlasString buf;
	    for (int i = 0; i <= intervals; i++) {
		float tick = values.first + (i * values.d);
		// In general, we just label major ticks.  However, there
		// are cases where there is only one major tick.  Just
		// labelling that one tick means it's impossible for the
		// user to know the scale.
		//
		// So, we label this tick if: (a) it's major, or (b) it's
		// the first or last one and there are fewer than 2 major
		// ticks.
		if ((fabs(remainderf(tick, values.D)) < 0.00001) ||
		    ((majorTicks < 2) && (i == 0)) ||
		    ((majorTicks < 2) && (i == intervals))) {
		    glColor4fv(axisColour);
		    buf.printf(format, tick);
		    _drawString(buf.str(), x - _margin, tick);
		}
	    }

	    glPopMatrix();

	    // Now the time (x) axis.  If you'll notice, this code is very
	    // similar to the preceding code.  Basically, all we do is
	    // flip the x and y axes.  If I was really clever, I'd just
	    // use a wacky transform and use the code above, but I'm not.

	    // Change the transformation.  The y axis uses pixels values,
	    // while the x axis uses times.
	    glPushMatrix();
	    glTranslatef(x, 0.0, 0.0);
	    glScalef(_times.pixels / (_times.last - _times.first), 1.0, 0.0);
	    glTranslatef(-_times.first, 0.0, 0.0);

	    glBegin(GL_LINES); {
		intervals = rint((_times.last - _times.first) / _times.d);
		for (int i = 1; i <= intervals; i++) {
		    float tick = _times.first + (i * _times.d);
		    glColor4fv(axisColour);
		    glVertex2f(tick, y);
		    if (fabs(remainderf(tick, _times.D)) < 0.00001) {
			// Major tick.
			glVertex2f(tick, y + 10);

			glColor4fv(majorTickColour);
			glVertex2f(tick, y + 10);
			glVertex2f(tick, y + values.pixels);
		    } else {
			// Minor tick.
			glVertex2f(tick, y + 5);

			glColor4fv(minorTickColour);
			glVertex2f(tick, y + 5);
			glVertex2f(tick, y + values.pixels);
		    }
		}
	    }
	    glEnd();
    
	    // Now label the major time ticks.
	    sprintf(format, "%%.%df", _times.decimals);
	    for (int i = 0, tickCount = 0; i <= intervals; i++) {
		float tick = _times.first + (i * _times.d);
		if ((fabs(remainderf(tick, _times.D)) < 0.00001) ||
		    ((tickCount < 2) && (i == intervals))) {
		    tickCount++;
		    glColor4fv(axisColour);
		    buf.printf(format, tick);
		    _drawString(buf.str(), tick, y - _header);
		}
	    }

	    glPopMatrix();

	    // Plot the graph (smoothed).  This time we transform both
	    // x and y values.
	    glEnable(GL_LINE_SMOOTH);
	    glPushMatrix(); {
		glTranslatef(x, y, 0.0);
		glScalef(_times.pixels / (_times.last - _times.first), 
			 values.pixels / (values.last - values.first), 0.0);
		glTranslatef(-_times.first, -values.first, 0.0);

		int point = 0;
		glColor4fv(graphColour);
		glBegin(GL_LINE_STRIP); {
		    for (unsigned int i = 0; i < values.data.size(); i++) {
			glVertex2f(_times.data[i], values.data[i]);
			point++;
		    }
		}
		glEnd();
	    }
	    glPopMatrix();
	}
    }
    glPopAttrib();

    // Print a header (actually a "center", since we print it in the
    // middle).
    glColor4fv(labelColour);
    _drawString(label, x + _times.pixels / 2, y + values.pixels / 2);
}

// Draws any glideslope indications on the altitude graph.  We assume
// that _altitude.pixels is correct.
void Graphs::_drawGS(int x, int y)
{
    if (_GSs.size() == 0) {
	// No glideslope chunks to draw.
	return;
    }

    // The glideslope may be far above or below the altitude track,
    // outside of the graphing area.  Rather than testing whether
    // points of the glideslope are outside the graph, we just set a
    // scissor rectangle and let OpenGL do the work for us.
    glScissor(x, y, _times.pixels, _altitude.pixels);
    glEnable(GL_SCISSOR_TEST);

    glPushMatrix(); {
	glTranslatef(x, y, 0.0);
	glScalef(_times.pixels / (_times.last - _times.first), 
		 _altitude.pixels / (_altitude.last - _altitude.first), 
		 0.0);
	glTranslatef(-_times.first, -_altitude.first, 0.0);

	for (unsigned int i = 0; i < _GSs.size(); i++) {
	    GSSection *s = _GSs[i];

	    float *c;
	    if (s->radio == NAV1) {
		c = vor1Colour;
	    } else {
		assert(s->radio == NAV2);
		c = vor2Colour;
	    }

	    glBegin(GL_QUAD_STRIP); {
		for (unsigned int j = 0; j < s->vals.size(); j++) {
		    GSValue& v = s->vals[j];

		    glColor4f(c[0], c[1], c[2], v.opacity);
		    glVertex2d(_times.data[v.x], v.bottom);
		    glVertex2d(_times.data[v.x], v.top);
		}
	    }
	    glEnd();
	}

	// Now draw lines representing the top, centre, and bottom
	// of the glideslope.
	glColor4fv(glideslopeOutlineColour);
	for (unsigned int i = 0; i < _GSs.size(); i++) {
	    GSSection *s = _GSs[i];

	    glBegin(GL_LINE_STRIP); {
		for (unsigned int j = 0; j < s->vals.size(); j++) {
		    GSValue& v = s->vals[j];
		    glVertex2d(_times.data[v.x], v.top);
		}
	    }
	    glEnd();
	    glBegin(GL_LINE_STRIP); {
		for (unsigned int j = 0; j < s->vals.size(); j++) {
		    GSValue& v = s->vals[j];
		    glVertex2d(_times.data[v.x], v.middle);
		}
	    }
	    glEnd();
	    glBegin(GL_LINE_STRIP); {
		for (unsigned int j = 0; j < s->vals.size(); j++) {
		    GSValue& v = s->vals[j];
		    glVertex2d(_times.data[v.x], v.bottom);
		}
	    }
	    glEnd();
	}
    }
    glPopMatrix();
    glDisable(GL_SCISSOR_TEST);
}

// Calculates "nice" intervals for a given set of data values.  The
// members 'pixels' (pixels allocated to the graph), 'max', and 'min'
// must be valid.  Our goal is to have ticks placed on the graph that
// aren't too close together or too far apart, and that can be
// labelled with nice numbers (which are basically 1, 2, and 5,
// multiplied by some power of 10).
//
// In our graphing style, graphs have small ticks and, every 5 small
// ticks, a large tick.
//
// Sets 'first' (value of first tick, assumed to be placed at the
// start of the axis), and 'last' (value of last tick, assumed to be
// placed at the end of the axis), 'd' (value of a small interval),
// 'D' (value of a large interval), 'decimals' (number of significant
// digits to be printed after the decimal point).
void Graphs::_calcNiceIntervals(Values &values)
{
    const int minimum = 10;	// Minimum small interval, in pixels.
    float actual;		// Actual interval, if min is placed
				// at 0 and max at pixels.
    const float var = 10.0;	// Used as an arbitrary variation,
				// when all the data values are
				// identical (ie, no variation or
				// range).

    // First, get the actual situation: the actual value of an
    // interval of 'minimum' pixels, assuming we plot the minimum
    // value at the start of the range, and the maximum value at the
    // end.  
    if (values.max != values.min) {
	actual = (values.max - values.min) / values.pixels * minimum;
    } else {
	// Special case: if actual = 0.0, that means there is no
	// variation in values, and it's impossible to derive a range
	// for them.  So we supply one arbitrarily via 'var'.
	actual = var / values.pixels * minimum;
    }

    // Break it down into an exponent (base-10) and mantissa.
    int exponent = floor(log10(actual));
    float mantissa = actual / pow(10, exponent);

    // Our heuristic: good mantissa values are 2, 5, and 10.  We move
    // up from our actual mantissa to the next good mantissa value,
    // which means that our ticks are spaced no less than 'minimum'
    // pixels apart.
    if (mantissa < 2) {
	values.d = 2 * pow(10, exponent);
    } else if (mantissa < 5) {
	values.d = 5 * pow(10, exponent);
    } else {
	values.d = 10 * pow(10, exponent);
    }
    // Large ticks are always placed every 5 small ticks.
    values.D = values.d * 5;

    // The start and end of the range begin on nice values too, so
    // find the largest nice value less than our minimum real value,
    // and the smallest nice value greater than our maximum real
    // value.
    if (values.min != values.max) {
	values.first = floor(values.min / values.d) * values.d;
	values.last = ceil(values.max / values.d) * values.d;
    } else {
	// Special case again.  If there's no variation in values, we
	// use the variation given by 'var'.
	values.first = floor((values.min - var / 2.0) / values.d) * values.d;
	values.last = ceil((values.max + var / 2.0) / values.d) * values.d;
    }

    // This code is pretty cheezy.  There's probably a cleaner way to
    // do this.  And this code fails on very small intervals (ie, less
    // than around 0.001).  Luckily, we aren't likely to get small
    // numbers like that.
    values.decimals = 0;
    float tmp = values.D;
    while ((tmp - rint(tmp)) > 0.001) {
	values.decimals++;
	tmp = (tmp - (int)tmp) * 10.0;
    }
}

// Draws the given string starting at the given point, in the current
// colour.
void Graphs::_drawString(const char *str, float x, float y)
{
    glRasterPos2f(x, y);
    for (unsigned int i = 0; i < strlen(str); i++) {
	// EYE - magic "number"
	glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, str[i]);
    }
}

// Sets the _times, _speed, _altitude, and _rateOfClimb variables,
// based on the values in _track.  In general, this should be called
// whenever we detect that _track has changed.
void Graphs::_loadData()
{
    // Time, altitude, and speed are easy.  We'll do those first.
    _times.data.clear();
    _speed.data.clear();
    _altitude.data.clear();

    _track->firstPoint();
    while (FlightData *p = _track->getNextPoint()) {
	_addPoint(p->est_t_offset, _times);
	_addPoint(p->spd, _speed);
	_addPoint(p->alt, _altitude);
    }

    // Climb rate is not so easy, so we give it its own routine.
    _loadClimbRate();
    
    // Ditto for glideslopes.

    // EYE - this is inefficient (and not just for glideslopes),
    // because _loadData potentially gets called for each new point
    // added.  We need a better way.  Maybe this data should be
    // maintained in the FlightTrack (perhaps with the exception of
    // rate of climb, and maybe some derived data for glideslopes).
    // On the other hand, this is only an issue for live tracks -
    // file-based tracks will call _loadData once only.
    _loadGSs();
}

// Calculating climb rate values is tough, so we have a special method
// to generate the values.
void Graphs::_loadClimbRate()
{
    // We want to smooth over the last '_smoothing' seconds.
    // Unfortunately, our times are not guaranteed to be regular.  So
    // what we do is have an index, j, which trails behind, as close
    // to '_smoothing' seconds behind our current index, i, as
    // possible.  If, as i increases, j is more than '_smoothing'
    // seconds ago, we move it forward.
    //
    // Note that this assumes that time is strictly ascending.  If you
    // record a flight in which you change the clock, all bets are
    // off.
    _rateOfClimb.data.clear();

    int j = 0;
    for (int i = 0; i < _track->size(); i++) {
	FlightData *p;
	float lastAlt, thisAlt;
	float lastTime, thisTime;
	float value;

	p = _track->dataAtPoint(i);
	thisAlt = p->alt;
	thisTime = _times.data[i];
	// EYE - I really should check to make sure this is correct.
	if (i == 0) {
	    // The first climb rate is 0.0 by default.
	    value = 0.0;

	    lastAlt = thisAlt;
	    lastTime = thisTime;
	} else if (_smoothing == 0) {
	    value = (thisAlt - lastAlt) / (thisTime - lastTime) * 60.0;

	    lastAlt = thisAlt;
	    lastTime = thisTime;
	} else {
	    if ((_times.data[i] - _times.data[j]) > _smoothing) {
		// 'j' has fallen behind.  We need to move it forward
		// until it's back within range.
		j++;
		while ((_times.data[i] - _times.data[j]) > _smoothing) {
		    j++;
		}
	    }

	    if (j == 0) {
		lastAlt = _track->dataAtPoint(j)->alt;
		lastTime = _times.data[j];
	    } else {
		float time0, time1, alt0, alt1;
		time0 = _times.data[j - 1];
		time1 = _times.data[j];
		alt0 = _track->dataAtPoint(j - 1)->alt;
		alt1 = _track->dataAtPoint(j)->alt;
		
		lastTime = _times.data[i] - _smoothing;
		lastAlt = (lastTime - time0) / (time1 - time0) * (alt1 - alt0) 
		    + alt0;
	    }
	    value = (thisAlt - lastAlt) / (thisTime - lastTime) * 60;
	}

	_addPoint(value, _rateOfClimb);
    }
}

// This is used to store some pre-calculated information about a
// glideslope beam - the bottom, middle and top planes comprising the
// glideslope.
struct __Planes {
    sgdVec4 bottom, middle, top;
};

// Extracts the heading and slope from a glideslope.
static void __extractHeadingSlope(NAV *n, double *heading, double *slope)
{
    // The glideslope's heading is given by the lower 3 digits of the
    // magvar variable.  The thousands and above give slope:
    // ssshhh.hhh.
    *heading = fmod(n->magvar, 1000.0);
    *slope = (n->magvar - *heading) / 1e5;
}

// Calculates the planes for the given glideslope.
static void __createPlanes(NAV *n, __Planes *planes)
{
    // Glideslopes have a vertical angular width of 0.7 degrees above
    // and below the centre of the glideslope (FAA AIM).
    const float glideSlopeWidth = 0.7;

    // The glideslope can be thought of as a plane tilted to the
    // earth's surface.  The top and bottom are given by planes tilted
    // 0.7 degrees more and less than the glideslope, respectively.
    double gsHeading, gsSlope;
    __extractHeadingSlope(n, &gsHeading, &gsSlope);

    // The 'rot' matrix will rotate from a standard orientation (up =
    // positive y-axis, ahead = positive z-axis, and right = negative
    // x-axis) to the navaid's actual orientation.
    sgdMat4 rot, mat;
    sgdMakeRotMat4(rot, n->lon - 90.0, n->lat, -gsHeading + 180.0);
    sgdMakeTransMat4(mat, n->bounds.center);
    sgdPreMultMat4(mat, rot);

    ////////////
    // Bottom plane of glideslope.
    sgdVec3 a, b, c;
    double tilt = (gsSlope - glideSlopeWidth) * SG_DEGREES_TO_RADIANS;

    // We create the plane by specifying 3 points, then rotating to
    // the correct orientation.  Note that we only rotate the normal
    // of the plane (the first 3 values in the vector - A, B, C).

    // EYE - make this a routine - sgdXformVec4?
    sgdSetVec3(a, 1.0, 0.0, 0.0);
    sgdSetVec3(b, 0.0, 0.0, 0.0);
    sgdSetVec3(c, 0.0, sin(tilt), cos(tilt));
    sgdXformPnt3(a, mat);
    sgdXformPnt3(b, mat);
    sgdXformPnt3(c, mat);
    sgdMakePlane(planes->bottom, a, b, c);

    ////////////
    // Middle plane of glideslope.
    tilt = gsSlope * SG_DEGREES_TO_RADIANS;
    sgdSetVec3(a, 1.0, 0.0, 0.0);
    sgdSetVec3(b, 0.0, 0.0, 0.0);
    sgdSetVec3(c, 0.0, sin(tilt), cos(tilt));
    sgdXformPnt3(a, mat);
    sgdXformPnt3(b, mat);
    sgdXformPnt3(c, mat);
    sgdMakePlane(planes->middle, a, b, c);

    ////////////
    // Top plane of glideslope.
    tilt = (gsSlope + glideSlopeWidth) * SG_DEGREES_TO_RADIANS;
    sgdSetVec3(a, 1.0, 0.0, 0.0);
    sgdSetVec3(b, 0.0, 0.0, 0.0);
    sgdSetVec3(c, 0.0, sin(tilt), cos(tilt));
    sgdXformPnt3(a, mat);
    sgdXformPnt3(b, mat);
    sgdXformPnt3(c, mat);
    sgdMakePlane(planes->top, a, b, c);
}

// Fills the _GSs structure based on the current flight track.  We
// want to create 'chunks' - continguous sections of glideslope
// information for a single radio.  This means that different radios
// will have different chunks, and that a given radio can be composed
// of different chunks if we go in and out of radio range.
void Graphs::_loadGSs()
{
    // We plot a glideslope if the aircraft is within 35 degrees
    // either side of its heading.  The figure is fairly arbitrary; I
    // just chose it so that we only show the glideslope when we're
    // reasonably close to the localizer.  The 35 degree figure *does*
    // correspond to the localizer limits at 10nm (FAA AIM).
    const float maxRange = 35.0;

    for (unsigned int i = 0; i < _GSs.size(); i++) {
	delete _GSs[i];
    }
    _GSs.clear();

    // Only Atlas flight tracks have navaid information, so quit early
    // if it's NMEA.
    if (!_track->isAtlasProtocol()) {
	return;
    }

    // At each point in the track, we look at the tuned in-range
    // navaids.  If a navaid is a glideslope, and is "ahead" of us,
    // then we want to draw it.

    // Glideslope data is divided into "sections", where a section
    // contains data for a single transmitter, continuously in range
    // during that time.  There can be many sections in the course of
    // a single flight, but at a single moment, there can only be as
    // many sections as there are radios (well, theoretically a radio
    // could be tuned into several navaids, but we're ignoring that).
    // Any new data we get will be added to those active sections.
    // Radio 'r' is inactive if active[r] == NULL.
    GSSection *active[_RADIO_COUNT];
    for (int i = 0; i < _RADIO_COUNT; i++) {
	active[i] = NULL;
    }
    // For each active glideslope, we create 3 planes (the
    // mathematical ones, not the flying ones) representing the
    // bottom, middle, and top of the glideslope beam.  This is what
    // we draw on the graph.
    __Planes planes[_RADIO_COUNT];

    // Now step through the flight track.
    for (int i = 0; i < _track->size(); i++) {
	FlightData *p = _track->dataAtPoint(i);

	// Check each active navaid to see if: (a) it's a glideslope,
	// (b) it's in our "cone of interest" (as defined by
	// 'maxRange').  If it is, then we need to graph it.
	const vector<NAV *>& navaids = p->navaids();
	for (unsigned int j = 0; j < navaids.size(); j++) {
	    NAV *n = navaids[j];

	    //////////////////////////////////////////////////
	    // (a) Is it a glideslope?
	    if (n->navtype != NAV_GS) {
		// Nope.  Go on to the next navaid.
		continue;
	    }

	    //////////////////////////////////////////////////
	    // (b) Is it within the cone of interest?
	    double gsHeading, heading, junk, distance;

	    // First get the navaid's heading.
	    __extractHeadingSlope(n, &gsHeading, &junk);

	    // Calculate the heading from the navaid to the aircraft.
	    geo_inverse_wgs_84(n->lat, n->lon, p->lat, p->lon,
			       &heading, &junk, &distance);
	    heading -= 180.0;

	    // Within the cone of interest?
	    if (fabs(normalizeHeading(heading - gsHeading, true, 180.0)) 
		> maxRange) {
		// Nope.  Go on to the next navaid.
		continue;
	    }

	    //////////////////////////////////////////////////
	    // We need to do something.  What radio is it?
	    Radio radio;
	    if (p->nav1_freq == n->freq) {
		radio = NAV1;
	    } else if (p->nav2_freq == n->freq) {
		radio = NAV2;
	    }

	    // Check to see if the radio is part of an active section.
	    // If not, create a new active section.
	    if (!active[radio]) {
		active[radio] = new GSSection;
		active[radio]->radio = radio;
		_GSs.push_back(active[radio]);
		__createPlanes(n, &(planes[radio]));
	    }

	    // At this point, 'active[radio]' points to our currently
	    // active section and 'planes[radio]' are the planes
	    // defining the glideslope.  Now fill in the data for our
	    // new glideslope point.
	    GSValue gs;

	    // Data point index and glide slope opacity.
	    gs.x = i;
	    gs.opacity = 1.0 - distance / n->range;

	    // Calculate the intersection of the planes with the line
	    // going straight up through the aircraft.  We specify the
	    // line with a point (the aircraft's current position) and
	    // a normal (calculated from the aircraft's latitude and
	    // longitude).
	    sgdVec3 pNorm;
	    double lat = p->lat * SGD_DEGREES_TO_RADIANS;
	    double lon = p->lon * SGD_DEGREES_TO_RADIANS;
	    sgdSetVec3(pNorm, 
		       cos(lon) * cos(lat), sin(lon) * cos(lat), sin(lat));

	    // Bottom plane of glideslope.
	    sgdVec3 intersection;
	    sgdIsectInfLinePlane(intersection, p->cart, pNorm, 
				 planes[radio].bottom);
	    double iLat, iLon, alt;
	    sgCartToGeod(intersection, &iLat, &iLon, &alt);
	    gs.bottom = alt * SG_METER_TO_FEET;

	    // Middle plane of glideslope.
	    sgdIsectInfLinePlane(intersection, p->cart, pNorm, 
				 planes[radio].middle);
	    sgCartToGeod(intersection, &iLat, &iLon, &alt);
	    gs.middle = alt * SG_METER_TO_FEET;

	    // Top plane of glideslope.
	    sgdIsectInfLinePlane(intersection, p->cart, pNorm, 
				 planes[radio].top);
	    sgCartToGeod(intersection, &iLat, &iLon, &alt);
	    gs.top = alt * SG_METER_TO_FEET;
	    
	    // Add the point to our vector.
	    active[radio]->vals.push_back(gs);
	}

	// After processing all the navaids for this point, check to
	// see which sections have become inactive (ie, didn't have
	// any data added in this iteration).
	for (int s = 0; s < _RADIO_COUNT; s++) {
	    // If the index of the last point added is not the current
	    // index, then it's inactive.
	    if (active[s] && (active[s]->vals.back().x != i)) {
		active[s] = NULL;
	    }
	}
    }
}

// Adds a single point to the vector in v, updating its min and max
// variables as well.
void Graphs::_addPoint(float p, Values &v)
{
    if (v.data.size() == 0) {
	v.min = v.max = p;
    } else if (p < v.min) {
	v.min = p;
    } else if (p > v.max) {
	v.max = p;
    }
    v.data.push_back(p);
}
