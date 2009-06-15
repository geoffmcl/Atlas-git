/*-------------------------------------------------------------------------
  Graphs.hxx

  Written by Brian Schack

  Copyright (C) 2007 Brian Schack

  The graphs object draws graphs for a flight track in a window.

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

#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <OpenGL/gl.h>
#include <GLUT/glut.h>

#include <vector>

#include "FlightTrack.hxx"
#include "Notifications.hxx"

// This contains a set of data points, which could be along the x-axis
// (time), or the y-axis (altitude, speed, or climb rate).  It also
// contains some bookkeeping information we use to draw the graph.
struct Values {
    // This information concerns the data.
    std::vector<float> data;
    float min, max;		// Minimum and maximum data value.
    
    // This information concerns the way it is graphed.
    int pixels;			// Space for axis (excluding labels).
    float first, last;		// Extreme values of graph.
    float d, D;			// Small and large tick intervals.
    int decimals;		// Significant digits after the decimal.
};

// Contains information about one slice of a glideslope: the data
// point (as an index into the flight track data), the opacity (a
// number from 0.0 - clear - to 1.0 - opaque), and the bottom, middle,
// and top of the glideslope (in feet).
struct GSValue {
    int x;
    float opacity;
    float bottom, middle, top;
};

// Contains information on one unbroken section of glideslope data for
// a single radio.  Sections for different radios can overlap in time.
// We ignore the possibility that a single radio could receive signals
// from more than one transmitter.
enum Radio {NAV1 = 0, NAV2, _RADIO_COUNT};
struct GSSection {
    vector <GSValue> vals;
    Radio radio;
};

class Graphs: public Subscriber {
public:
    static const int ALTITUDE = 1 << 0;
    static const int SPEED = 1 << 1;
    static const int CLIMB_RATE = 1 << 2;

    Graphs(int window);
    ~Graphs();

    void draw();
    void reshape(int w, int h);
    void setAircraftColor(const float *color);
    void setMarkColor(const float *color);
    int pixelToPoint(int x);

    int graphTypes();
    void setGraphTypes(int types);
    FlightTrack *flightTrack();
    void setFlightTrack(FlightTrack *t);

    unsigned int smoothing();
    void setSmoothing(unsigned int s);

    bool notification(Notification::type n);
protected:
    int _window;
    int _w, _h;

    FlightTrack *_track;
    int _graphTypes;
    unsigned int _smoothing;
    GLfloat _aircraftColour[4];
    GLfloat _markColour[4];

    static const int _header = 10; // Space above each graph (in pixels).
    static const int _margin = 10; // Space to the left of each graph.

    // These contain the data (raw and derived) of the track.  Since
    // most tracks never change, we save them here to save ourselves
    // time.
    Values _times, _speed, _altitude, _rateOfClimb;
    std::vector<GSSection *> _GSs;

    // These determine whether the graphs should be redrawn anew or
    // whether we can use the display list stored in _graphDL, and
    // whether we need to reload the track data.
    //
    // _shouldRerender: true when we need to regenerate _graphDL
    // _shouldReload: true when we need to call _loadData()
    //
    // Note that if _shouldReload is true, _shouldRerender is always
    // true.  Another way to look at is is if _shouldRerender is
    // false, then so is _shouldReload.
    bool _shouldRerender, _shouldReload;
    GLuint _graphDL;

    void _drawGraph(Values &values, int x, int y, const char *label);
    void _drawGS(int x, int y);

    void _calcNiceIntervals(Values &values);

    void _drawString(const char *str, float x, float y);

    void _loadData();
    void _loadClimbRate();
    void _loadGSs();
    void _addPoint(float p, Values &v);
};

#endif // _GRAPH_H_
