/*-------------------------------------------------------------------------
  Graphs.hxx

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

#ifndef __GRAPH_H__
#define __GRAPH_H__

#include <vector>
#include "FlightTrack.hxx"

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

class Graphs {
public:
    static const int ALTITUDE = 1 << 0;
    static const int SPEED = 1 << 1;
    static const int CLIMB_RATE = 1 << 2;

    Graphs(int window);

    void draw();
    void setAircraftColor(const float *color);
    void setMarkColor(const float *color);
    void setMark(int x);

    int graphTypes();
    void setGraphTypes(int types);
    FlightTrack *flightTrack();
    void setFlightTrack(FlightTrack *t);

    unsigned int smoothing();
    void setSmoothing(unsigned int s);
protected:
    int _window;
    FlightTrack *_track;
    int _lastVersion;
    int _graphTypes;
    unsigned int _smoothing;
    GLfloat _aircraftColour[4];
    GLfloat _markColour[4];

    static const int _header = 10; // Space above each graph (in pixels).
    static const int _margin = 10; // Space to the left of each graph.

    // These contain the data (raw and derived) of the track.  Since
    // most tracks never change, we save them here to save ourselves
    // time.
    Values _times;
    Values _speed;
    Values _altitude;
    Values _rateOfClimb;

    void _drawGraph(Values &values, int x, int y, const char *label);

    void _calcNiceIntervals(Values &values);

    void _drawString(const char *str, float x, float y);

    void _loadData();
    void _loadClimbRate();
    void _addPoint(float p, Values &v);

    const char *_generateName();
};

#endif        // __GRAPH_H__
