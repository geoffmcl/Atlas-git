/*-------------------------------------------------------------------------
  Graphs.cxx

  Written by Brian Schack, started August 2007.

  Copyright (C) 2007 Brian Schack

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  ---------------------------------------------------------------------------*/

#include <simgear/compiler.h>
//#include SG_GLUT_H
#include<GL/glut.h>

#include "Graphs.hxx"

Graphs::Graphs(int window) : _window(window)
{
    _track = NULL;
    _graphTypes = 0;
    _smoothing = 10;

    // Default mark and live aircraft colours are black.
    GLfloat black[4] = {0.0, 0.0, 0.0, 1.0};
    setAircraftColor(black);
    setMarkColor(black);
}

// Draws all of our graphs (as given by graphTypes) into our window.
// Graphs span the window horizontally, and each graph gets equal
// space vertically.  Assumes that _track is valid.
void Graphs::draw()
{
    glutSetWindow(_window);

    // Set our title.
    glutSetWindowTitle(name());

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

    // Get viewport size.
    GLint viewport[4];
    GLint x, y;
    GLsizei w, h;
    glGetIntegerv(GL_VIEWPORT, viewport);
    x = viewport[0];
    y = viewport[1];
    w = viewport[2];
    h = viewport[3];

    // Reload our data if it has changed.
    if (_lastVersion < _track->version()) {
	// The track has changed, so reload the data.  Loading the
	// data will set 'data', 'min', and 'max' in the Values
	// records.
	_loadData();
	_lastVersion = _track->version();
    }

    // Now draw the graphs.  x and y designates the lower-left corner
    // of the actual graph, ignoring labels.
    y = _header;
    x += _margin;
    _times.pixels = w - (_margin * 2);
    _calcNiceIntervals(_times);
    if (graphTypes() & Graphs::CLIMB_RATE) {
	_rateOfClimb.pixels = (h / graphCount) - (_header * 2);
	_calcNiceIntervals(_rateOfClimb);
	_drawGraph(_rateOfClimb, x, y, "Climb Rate (ft/min)");
	y += _rateOfClimb.pixels + (2 * _header);
    }
    if (graphTypes() & Graphs::SPEED) {
	_speed.pixels = (h / graphCount) - (_header * 2);
	_calcNiceIntervals(_speed);
	_drawGraph(_speed, x, y, "Speed (kt)");
	y += _speed.pixels + (2 * _header);
    }
    if (graphTypes() & Graphs::ALTITUDE) {
	_altitude.pixels = (h / graphCount) - (_header * 2);
	_calcNiceIntervals(_altitude);
	_drawGraph(_altitude, x, y, "Altitude (ft)");
    }

    // Draw current mark and, if it's live, current aircraft position
    // (which is always at the end of the track, but it's drawn just
    // to reinforce the fact that it's live).
    if ((_track->mark() >= 0) || _track->live()) {
	// Draw the mark using time coordinates in the x-axis, pixel
	// coordinates in the y-axis.  Note to self - the
	// transformations happen in reverse of the way they're given.
	// In other words, the raw time is first translated by
	// -times.first, then scaled, then translated by x.
	// EYE - is that really right?
	glPushMatrix();

	glTranslatef(x, 0.0, 0.0);
	glScalef(_times.pixels / (_times.last - _times.first), 1.0, 0.0);
	glTranslatef(-_times.first, 0.0, 0.0);

	if (_track->mark() >= 0) {
	    glBegin(GL_LINES);
	    glColor4fv(_markColour);
	    glVertex2f(_times.data[_track->mark()], 0.0);
	    glVertex2f(_times.data[_track->mark()], (float)h);
	    glEnd();
	}
	if (_track->live()) {
	    glBegin(GL_LINES);
	    glColor4fv(_aircraftColour);
	    glVertex2f(_times.max, 0.0);
	    glVertex2f(_times.max, (float)h);
	    glEnd();
	}

	glPopMatrix();
    }
}

void Graphs::setAircraftColor(const float *color)
{
    memcpy(_aircraftColour, color, sizeof(float) * 4);
}

void Graphs::setMarkColor(const float *color)
{
    memcpy(_markColour, color, sizeof(float) * 4);
}

// Sets the mark (the current record in the flight track) based on an
// x coordinate in the window.  Because time is irregular, finding the
// corresponding point in the flight track requires a search.  We're
// stupid and just do a linear search from the beginning - we really
// should be a bit smarter about it.
void Graphs::setMark(int x)
{
    // EYE - should we do this?  (Also in draw())
    glutSetWindow(_window);

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
    for (int i = 0; i < _times.data.size(); i++) {
	if (_times.data[i] > time) {
	    // We've passed the cutoff.  Check if the point we're at
	    // is better than the point we just passed.
	    if (i == 0) {
		_track->setMark(i);
		return;
	    }
	    // Note that this bit of code assumes that time increases,
	    // which isn't always true.  On the other hand, we don't
	    // have any god way to handle non-increasing time values,
	    // so we'll just close our eyes and hope nothing happens.
	    float time0 = _times.data[i] - time;
	    float time1 = time - _times.data[i - 1];
	    if (time0 < time1) {
		_track->setMark(i);
		return;
	    } else {
		_track->setMark(i - 1);
		return;
	    }
	}
    }
    _track->setMark(_track->size() - 1);
}

int Graphs::graphTypes()
{
    return _graphTypes;
}

void Graphs::setGraphTypes(int types)
{
    _graphTypes = types;
}

FlightTrack *Graphs::flightTrack()
{
    return _track;
}

void Graphs::setFlightTrack(FlightTrack *t)
{
    _track = t;
    if (_track) {
	_lastVersion = -1;
    }
}

unsigned int Graphs::smoothing()
{
    return _smoothing;
}

// EYE - make smoothing a float?
void Graphs::setSmoothing(unsigned int s)
{
    _smoothing = s;
    _loadData();
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
    // Print the graph axes.
    glColor3f(0.0, 0.0, 0.0);
    glBegin(GL_LINE_STRIP);
    glVertex2i(x, y + values.pixels);
    glVertex2i(x, y);
    glVertex2i(x + _times.pixels, y);
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

	glBegin(GL_LINES);
	// To avoid problems with increasing round-off errors, we use
	// an integer to control the loop.
	int intervals = rint((values.last - values.first) / values.d);
	int majorTicks = 0;
	for (int i = 1; i <= intervals; i++) {
	    float tick = values.first + (i * values.d);
	    glColor3f(0.0, 0.0, 0.0);
	    glVertex2f(x, tick);
	    if (fabs(remainderf(tick, values.D)) < 0.00001) {
		// Major tick.
		glVertex2f(x + 10, tick);

		glColor3f(0.5, 0.5, 0.5);
		glVertex2f(x + 10, tick);
		glVertex2f(x + _times.pixels, tick);
		
		majorTicks++;
	    } else {
		// Minor tick.
		glVertex2f(x + 5, tick);

		glColor3f(0.75, 0.75, 0.75);
		glVertex2f(x + 5, tick);
		glVertex2f(x + _times.pixels, tick);
	    }
	}
	glEnd();
    
	// Now label the y axis.
	char format[10];
	sprintf(format, "%%.%df", values.decimals);
	for (int i = 0, tickCount = 0; i <= intervals; i++) {
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
		glColor3f(0.0, 0.0, 0.0);
		char *buf;
		asprintf(&buf, format, tick);
		_drawString(buf, x - _margin, tick);
		free(buf);
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

	glBegin(GL_LINES);
	intervals = rint((_times.last - _times.first) / _times.d);
	for (int i = 1; i <= intervals; i++) {
	    float tick = _times.first + (i * _times.d);
	    glColor3f(0.0, 0.0, 0.0);
	    glVertex2f(tick, y);
	    if (fabs(remainderf(tick, _times.D)) < 0.00001) {
		// Major tick.
		glVertex2f(tick, y + 10);

		glColor3f(0.5, 0.5, 0.5);
		glVertex2f(tick, y + 10);
		glVertex2f(tick, y + values.pixels);
	    } else {
		// Minor tick.
		glVertex2f(tick, y + 5);

		glColor3f(0.75, 0.75, 0.75);
		glVertex2f(tick, y + 5);
		glVertex2f(tick, y + values.pixels);
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
		glColor3f(0.0, 0.0, 0.0);
		char *buf;
		asprintf(&buf, format, tick);
		_drawString(buf, tick, y - _header);
		free(buf);
	    }
	}

	glPopMatrix();

	// Plot the graph.  This time we transform both x and y values.
	glPushMatrix();
	glTranslatef(x, y, 0.0);
	glScalef(_times.pixels / (_times.last - _times.first), 
		 values.pixels / (values.last - values.first), 0.0);
	glTranslatef(-_times.first, -values.first, 0.0);

	int point = 0;
	glColor3f(1.0, 0.0, 0.0);
	glBegin(GL_LINE_STRIP);
	for (int i = 0; i < values.data.size(); i++) {
	    glVertex2f(_times.data[i], values.data[i]);
	    point++;
	}
	glEnd();

	glPopMatrix();
    }

    // Print a header (actually a "center", since we print it in the
    // middle).
    glColor3f(0.0, 0.0, 0.0);
    _drawString(label, x + _times.pixels / 2, y + values.pixels / 2);
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
// void Graphs::_drawString(char *str, int x, int y)
void Graphs::_drawString(const char *str, float x, float y)
{
    glRasterPos2f(x, y);
    for (int i = 0; i < strlen(str); i++) {
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

// network (5500) - live network, no file
// network (5500, <name>) - live network, file
// network (5500, <name>*) - live network, file, unsaved
// serial (/dev/foo, 9600) - live serial, no file
// serial (/dev/foo, 9600, <name>) - live serial, file, saved
// serial (/dev/foo, 9600, <name>*) - live serial, file, unsaved
// <name> - file, saved
// <name>* - file, unsaved
// detached, no file - detached, no file (duh!)
const char *Graphs::name()
{
    // EYE - don't use fixed-length buffer?
    static char buf[512];
    if (_track->isNetwork()) {
	if (_track->hasFile()) {
	    if (_track->modified()) {
		sprintf(buf, "network (%d, %s*)", 
			_track->port(), _track->fileName());
	    } else {
		sprintf(buf, "network (%d, %s)",
			_track->port(), _track->fileName());
	    }
	} else {
	    sprintf(buf, "network (%d)", _track->port());
	}
    } else if (_track->isSerial()) {
	if (_track->hasFile()) {
	    if (_track->modified()) {
		sprintf(buf, "serial (%s, %d, %s*)", 
			_track->device(), _track->baud(), _track->fileName());
	    } else {
		sprintf(buf, "serial (%s, %d, %s)",
			_track->device(), _track->baud(), _track->fileName());
	    }
	} else {
	    sprintf(buf, "serial (%s, %d)", _track->device(), _track->baud());
	}
    } else if (_track->hasFile()) {
	if (_track->modified()) {
	    sprintf(buf, "%s*", _track->fileName());
	} else {
	    sprintf(buf, "%s", _track->fileName());
	}
    } else {
	sprintf(buf, "detached, no file");
    }

    return buf;
}
