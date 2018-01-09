/*-------------------------------------------------------------------------
  Graphs.hxx

  Written by Brian Schack

  Copyright (C) 2007 - 2017 Brian Schack

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

#include <vector>
#include <bitset>

#include <plib/pu.h>

#include "AtlasBaseWindow.hxx"
#include "Notifications.hxx"	// Subscriber

// Forward class declarations
class AtlasController;
class FlightTrack;
class GS;

class GraphsUI;
class GraphsWindow: public AtlasBaseWindow, Subscriber {
  public:
    GraphsWindow(const char *name,
		 const char *regularFontFile,
		 const char *boldFontFile,
		 AtlasController *ac);
    ~GraphsWindow();

    // Public window callback.  The main window needs to pass on
    // special characters to us, so we expose it (making sure to set
    // our window to be current).
    void special(int key, int x, int y);

    void setAircraftColour(const float *colour);
    void setMarkColour(const float *colour);

    // EYE - make protected?
    void setFlightTrack(FlightTrack *t);
    void setSmoothing(unsigned int s);

    enum YAxisType { ALTITUDE, SPEED, CLIMB_RATE, _GRAPH_TYPES_COUNT };
    void setYAxisType(YAxisType t, bool b);

    enum XAxisType { TIME, DISTANCE, _XAXIS_COUNT };
    void setXAxisType(XAxisType t);
    void toggleXAxisType();
    XAxisType xAxisType() const { return _xAxisType; }

    bool autoscale(XAxisType t) const;
    bool autoscale(YAxisType t) const;
    float scale(XAxisType t) const;
    float scale(YAxisType t) const;

    void setAutoscale(XAxisType t, bool b);
    void setAutoscale(YAxisType t, bool b);
    void setScale(XAxisType t, float f);
    void setScale(YAxisType t, float f);

    void notification(Notification::type n);

  protected:
    // The Values class contains a set of data points, which could be
    // along the x-axis (time, distance), or the y-axis (altitude,
    // speed, or climb rate).  It also contains some bookkeeping
    // information we use to draw the graph, especially the labels and
    // ticks along the axis.  It is a pure virtual class - subclasses
    // have to implement the at() routine to access the appropriate
    // data from the flight track.
    class Values {
      public:
	// The label of y-axis Values will be printed on the graph.
	Values(const char *label);
	virtual ~Values();

	// We get our data from a flight track.
	void setFlightTrack(FlightTrack *ft);
	virtual float at(size_t i) = 0;
	virtual size_t size();
	// This should be called whenever the flight track changes -
	// it tells us to recalculate values (eg, min, max, ...) that
	// are used to determine how to draw and label the graph.
	virtual void load();
	
	const char *label() const { return _label; }

	// If autoscaled, the graph will be drawn to fit the given
	// pixels.  If not, it will be drawn at the given scale (the
	// scale is in graph units per pixel).
	bool autoscale() const { return _autoscale; }
	int requestedPixels() const { return _requestedPixels; }
	float requestedScale() const { return _requestedScale; }

	void setAutoscale(bool b);
	void setPixels(int pixels);
	void setScale(float scale);

	// Draws the axis, nicely labelled, with ticks along the axis
	// at regular intervals.  We draw it along the x axis, from
	// 'from' to 'to' in native units, while ticks are drawn in
	// the y direction, 'height' pixels high (so if you want to
	// draw the axis in the y direction, you need to transform the
	// modelview matrix).  Labels are drawn within 'margin' pixels
	// from the axis.
	void drawAxis(float from, float to, int height, int margin);
	
	// Draws the complete graph (but not the axis, grid, or
	// labels), in native units.  The caller needs to set up
	// scaling and clipping to make sure only the desired part of
	// the graph appears at the right place in the window.  We are
	// given the x axis values - we supply our own values for the
	// y axis (so it doesn't make sense to call this method for
	// time and distance objects, but it does make sense to call
	// it for altitude, speed, and rate of climb objects).
	virtual void draw(Values& xVals);

	// The smallest and largest values in the data set.
	float min() const { return _min; }
	float max() const { return _max; }

	// These all return values derived from the data and either
	// the pixel size of the graph or the scale of the graph.
	// They are calculated on demand.
	int pixels();		// Calculated space for axis (pixels).
	float scale();		// Calculated scale (units/pixel).
	float first();		// First tick (units).
	float last();		// Last tick (units).
	float range();		// last() - first()
	float d();		// Small tick interval (units).
	float D();		// Large tick interval (units).
	int decimals();	        // Significant digits after the decimal.

	// We keep track of a slider, which appears when we can't fit
	// within our given space.  However, we do not own the slider.
	// EYE - is this stupid?
	bool showSlider();
	void setSlider(puSlider *s) { _slider = s; }
	puSlider *slider() { return _slider; }

      protected:
	// This information concerns the data.
	char *_label;
	FlightTrack *_ft;
	float _min, _max;	// Minimum and maximum data value, in
				// data units (seconds, nautical
				// miles, feet, etc)
    
	// This information concerns the way it is graphed.
	bool _autoscale;       // True if _pixels is the requested
			       // value and _scale is a derived value,
			       // false if _scale is the requested
			       // value and _pixels is a derived
			       // value.
	int _requestedPixels;  // Requested space for axis (pixels).
	float _requestedScale; // Requested scale (units/pixel).

	// These are derived values and cannot be set directly.
	int _pixels;
	float _scale;
	float _first, _last;
	float _d, _D;
	int _decimals;
	
	// True if we need to update any of the values immediately
	// above (done by calling _update()).
	bool _dirty;

	// Conditionally sets _min and _max to val.  If force is true,
	// _min and _max will be set unconditionally to val.
	void _setMinMax(float val, bool force = false);
	void _update();		// Updates the derived values and sets
				// _dirty to false.
	
	puSlider *_slider;
    };

    // Derived classes.  Most derived classes are very simple - the
    // at() function merely accesses the appropriate field in the
    // flight data point.  There are two exceptions: Altitudes (which
    // must deal with glideslopes), and RatesOfClimb (which needs to
    // deal with smoothing).
    class Times: public Values {
      public:
	Times(const char *label);
	float at(size_t i);
    };

    class Distances: public Values {
      public:
	Distances(const char *label);
	float at(size_t i);
    };

    class Speeds: public Values {
      public:
	Speeds(const char *label);
	float at(size_t i);
    };

    class Altitudes: public Values {
      public:
	Altitudes(const char *label);
	~Altitudes();
	float at(size_t i);
	void load();
	void draw(Values& yVals);

      protected:
	// Contains information about one slice of a glideslope: the
	// data point (as an index into the flight track data), the
	// opacity (a number from 0.0 - clear - to 1.0 - opaque), and
	// the bottom, middle, and top of the glideslope (in feet).
	struct _GSValue {
	    size_t x;
	    float opacity;
	    float bottom, middle, top;
	};

	// Contains information on one unbroken section of glideslope
	// data for a single radio.  Sections for different radios can
	// overlap in time.  We ignore the possibility that a single
	// radio could receive signals from more than one transmitter.
	enum Radio {NAV1 = 0, NAV2, _RADIO_COUNT};
	struct _GSSection {
	    std::vector <_GSValue> vals;
	    Radio radio;
	};

	// This is used to store some pre-calculated information about
	// a glideslope beam - the bottom, middle and top planes
	// comprising the glideslope.
	struct _Planes {
	    sgdVec4 bottom, middle, top;
	};

	std::vector<_GSSection *> _GSs;

	void _drawGSs(Values& xVals);
	void _createPlanes(GS *gs, _Planes *planes);
    };

    class RatesOfClimb: public Values {
      public:
	RatesOfClimb(const char *label);
	float at(size_t i);
	void load();

	void setSmoothing(unsigned int i) { _smoothing = i; }

      protected:
	std::vector<float> _data;
	unsigned int _smoothing;

	float _altAt(size_t i);
	float _timeAt(size_t i);
    };

    int _w, _h;
    AtlasController *_ac;

    FlightTrack *_track;	// The current track.
    std::bitset<_GRAPH_TYPES_COUNT> _graphTypes;
    unsigned int _smoothing;	// Smoothing range (in seconds).
    XAxisType _xAxisType;	// Either TIME or DISTANCE.
    GLfloat _aircraftColour[4];
    GLfloat _markColour[4];

    static const int _header = 10; // Space above and below each graph
				   // (in pixels).
    static const int _margin = 15; // Space to the left and right of
				   // each graph.

    // These contain the data (raw and derived) of the track.  Since
    // most tracks never change, we save them here to save ourselves
    // time.
    Times _time;
    Distances _dist;
    Values *_values[_GRAPH_TYPES_COUNT];

    // These determine whether the graphs should be redrawn anew or
    // whether we can use the display list stored in _graphDL, and
    // whether we need to reload the track data.
    //
    // _shouldRerender: true when we need to regenerate _graphDL
    // _shouldReload: true when we need to call _loadData()
    //
    // Note that if _shouldReload is true, _shouldRerender is always
    // true.  Equivalently, if _shouldRerender is false, then so is
    // _shouldReload.
    bool _shouldRerender, _shouldReload;
    GLuint _graphDL;

    // UI stuff
    // EYE - put in Values class?
    puSlider *_xSlider, *_ySliders[_GRAPH_TYPES_COUNT];
    bool _dragging;
    GraphsUI *_ui;

    // Window callbacks.
    void _display();
    void _reshape(int w, int h);
    void _mouse(int button, int state, int x, int y);
    void _motion(int x, int y);
    void _keyboard(unsigned char key, int x, int y);
    void _special(int key, int x, int y);
    void _visibility(int state);

    // Draws the entire graph, including labels and such.
    void _drawGraph(Values &xVals, Values& yVals, int viewport[4]);
    // Draws one labelled axis.
    void _drawGraphAxis(Values &vals, int viewport[4], float world[4],
			bool vertical = false);
    // Returns the current x axis type.
    Values& _xValues();
    // Returns the offset (in x axis units) due to the slider attached
    // to the given Values object.
    float _offset(Values& v);
    // Returns the flight track data point closest to the given x
    // pixel coordinate in the graph window.  Adjusts the slider if
    // necessary.
    size_t _pixelToPoint(int x);
    // Sets the slider to the given value.
    void _setSlider(puObject *slider, float val);

    // Conditionally loads the data in all the Values objects.
    void _loadData();

    // EYE - I'd like to declare this static.  However, C++ says
    // "storage class specifiers are invalid in friend function
    // declarations."  Why?
    friend void _slider_cb(puObject *cb);
};

//////////////////////////////////////////////////////////////////////
// GraphsUI.  It contains all the controls in the graphs window (but
// not the graph itself).
//////////////////////////////////////////////////////////////////////
class GraphsUI {
  public:
    GraphsUI(GraphsWindow *gw);
    ~GraphsUI();

    // EYE - make part of GLUTWindow?
    int width() const { return _w; }
    int height() const { return _h; }

    void setSize(int x, int h);

    void setXAxisType(GraphsWindow::XAxisType t);
    void setYAxisType(GraphsWindow::YAxisType t, bool b);
    void setSmoothing(unsigned int s);

  protected:
    friend void _axis_cb(puObject *cb);
    class _AxisUI {
      public:
    	_AxisUI(const char *name, const char *units, float scale, GraphsUI &g);
    	~_AxisUI();
	
    	int width() const { return _w; }
    	int height() const { return _h; }
	void setSize(int y, int w);

    	bool selected() const;
    	bool autoscaling() const;
    	float scale() const;

	void setSelected(bool b);
    	void setAutoscaling(bool b);
    	void setScale(float f);

      protected:
	// Yes, I have to declare this twice.  Only C++, in its
	// infinite wisdom, truly knows why.
	friend void _axis_cb(puObject *cb);
    	void _callback(puObject *cb);

	GraphsUI& _parent;
    	puGroup *_gui;
    	puButton *_selectButton, *_autoscalingButton;
    	puInput *_scaleInput;

    	int _w, _h;
    };

    void _axisSelect(_AxisUI *axis);
    void _autoscale(_AxisUI *axis);
    void _setScale(_AxisUI *axis);

    GraphsWindow *_gw;

    puGroup *_mainGroup, *_xGroup, *_yGroup;
    puFrame *_mainFrame, *_xFrame, *_yFrame;
    _AxisUI *_yAxes[GraphsWindow::_GRAPH_TYPES_COUNT];
    _AxisUI *_xAxes[GraphsWindow::_XAXIS_COUNT];
    puSlider *_smoother;

    int _w, _h;
};

#endif // _GRAPH_H_
