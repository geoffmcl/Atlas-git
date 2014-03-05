/*-------------------------------------------------------------------------
  Graphs.cxx

  Written by Brian Schack

  Copyright (C) 2007 - 2014 Brian Schack

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
#include "Graphs.hxx"

// C++ system files
#include <algorithm>		// min(), max()

// Our project's includes
#include "AtlasController.hxx"
#include "AtlasWindow.hxx"
#include "FlightTrack.hxx"
#include "Globals.hxx"
#include "NavData.hxx"

using namespace std;

static float axisColour[4] = {0.0, 0.0, 0.0, 1.0};
static float labelColour[4] = {0.0, 0.0, 0.0, 1.0};
static float majorTickColour[4] = {0.5, 0.5, 0.5, 1.0};
static float minorTickColour[4] = {0.75, 0.75, 0.75, 1.0};
static float graphColour[4] = {1.0, 0.0, 0.0, 1.0};
static float glideslopeOutlineColour[4] = {0.75, 0.75, 0.75, 1.0};

const int majorTickSize = 10;
const int minorTickSize = 5;

// Height (if horizontal) or width (if vertical) of a graph slider.
static int sliderHeight = 10;

void _axis_cb(puObject *cb)
{
    GraphsUI::_AxisUI *me = (GraphsUI::_AxisUI *)cb->getUserData();
    me->_callback(cb);
}

GraphsUI::_AxisUI::_AxisUI(const char *name, const char *units, float scale,
			   GraphsUI& g): _parent(g)
{
    puFont font = puGetDefaultLabelFont();
    const int space = 5, height = font.getStringHeight();
    const int selectWidth = 75, inputWidth = 50;

    _gui = new puGroup(0, 0); {
	int x = space, y = 0;
	puBox *bbox;

	_selectButton = new puButton(x, y, x + height, y + height);
	_selectButton->setLabelPlace(PUPLACE_CENTERED_RIGHT);
	_selectButton->setLabel(name); // EYE - copied?
	_selectButton->setButtonType(PUBUTTON_RADIO);
	_selectButton->setCallback(_axis_cb);
	_selectButton->setUserData((void *)this);
	x += selectWidth;

	_autoscalingButton = new puButton(x, y, x + height, y + height);
	_autoscalingButton->setButtonType(PUBUTTON_VCHECK);
	_autoscalingButton->setCallback(_axis_cb);
	_autoscalingButton->setUserData((void *)this);
	bbox = _autoscalingButton->getBBox();
	x += (bbox->max[0] - bbox->min[0]) + space;

	_scaleInput = new puInput(x, y, x + inputWidth, y + height);
	_scaleInput->setLabelPlace(PUPLACE_CENTERED_RIGHT);
	_scaleInput->setLabel(units);
	_scaleInput->setValue(scale);
	_scaleInput->setCallback(_axis_cb);
	_scaleInput->setUserData((void *)this);
    }
    _gui->close();

    _gui->getSize(&_w, &_h);
}

GraphsUI::_AxisUI::~_AxisUI()
{
    puDeleteObject(_gui);
}

void GraphsUI::_AxisUI::setSize(int y, int w)
{
    _gui->setPosition(0, y);
    _w = w;
}

bool GraphsUI::_AxisUI::selected() const
{
    return (_selectButton->getIntegerValue() == 1);
}

bool GraphsUI::_AxisUI::autoscaling() const
{
    return (_autoscalingButton->getIntegerValue() == 0);
}

float GraphsUI::_AxisUI::scale() const
{
    return _scaleInput->getFloatValue();
}

void GraphsUI::_AxisUI::setSelected(bool b)
{
    _selectButton->setValue(b);
}

void GraphsUI::_AxisUI::setAutoscaling(bool b)
{
    _autoscalingButton->setValue(b);
}

void GraphsUI::_AxisUI::setScale(float f)
{
    _scaleInput->setValue(f);
}

void GraphsUI::_AxisUI::_callback(puObject *cb)
{
    if (cb == _selectButton) {
	_parent._axisSelect(this);
    } else if (cb == _autoscalingButton) {
	_parent._autoscale(this);
    } else if (cb == _scaleInput) {
	_parent._setScale(this);
    } else {
	assert(0);
    }
}

static void _smoother_cb(puObject *cb)
{
    GraphsUI *me = (GraphsUI *)cb->getUserData();
    me->setSmoothing(cb->getIntegerValue());
}

GraphsUI::GraphsUI(GraphsWindow *gw): _gw(gw)
{
    const int vSpace = 4, hSpace = 5;
    int width = 100, height = 100;
    int smootherHeight = 15;

    _mainGroup = new puGroup(0, 0); {
	_mainFrame = new puFrame(0, 0, width, height);

	_xGroup = new puGroup(0, 0); {
	    _xFrame = new puFrame(0, 0, 0, 0);

	    // X-axis controllers.
	    _xAxes[GraphsWindow::TIME] = 
		new GraphsUI::_AxisUI("Time", "s", 
				      gw->scale(GraphsWindow::TIME), 
				      *this);
	    _xAxes[GraphsWindow::DISTANCE] = 
		new GraphsUI::_AxisUI("Distance", "nm", 
				      gw->scale(GraphsWindow::DISTANCE), 
				      *this);

	    width = std::max(_xAxes[GraphsWindow::TIME]->width(), width);
	    width = std::max(_xAxes[GraphsWindow::DISTANCE]->width(), width);
	}
	_xGroup->close();
	_xGroup->reveal();	// EYE - necessary?

	_yGroup = new puGroup(0, 0); {
	    _yFrame = new puFrame(0, 0, 0, 0);

	    // Y-axis controllers.
	    _yAxes[GraphsWindow::ALTITUDE] = 
		new GraphsUI::_AxisUI("Altitude", "ft", 
				      gw->scale(GraphsWindow::ALTITUDE), 
				      *this);
	    _yAxes[GraphsWindow::SPEED] = 
		new GraphsUI::_AxisUI("Speed", "kt", 
				      gw->scale(GraphsWindow::SPEED), 
				      *this);
	    _yAxes[GraphsWindow::CLIMB_RATE] = 
		new GraphsUI::_AxisUI("Climb rate", "ft/min", 
				      gw->scale(GraphsWindow::CLIMB_RATE), 
				      *this);

	    width = std::max(_yAxes[GraphsWindow::ALTITUDE]->width(), width);
	    width = std::max(_yAxes[GraphsWindow::SPEED]->width(), width);
	    width = std::max(_yAxes[GraphsWindow::CLIMB_RATE]->width(), width);

	    // Smoothing slider.
	    _smoother = new puSlider(0, 0, width - 2 * hSpace, FALSE, 
				     smootherHeight);
	    _smoother->setLabelPlace(PUPLACE_TOP_CENTERED);
	    _smoother->setLabel("Smoothing (s)");
	    _smoother->setMinValue(0.0); // 0.0 = no smoothing
	    _smoother->setMaxValue(60.0); // 60.0 = smooth over a 60s interval
	    _smoother->setStepSize(1.0);
	    // Our initial smoothing value is 10s (rates of climb and
	    // descent will be smoothed over a 10s interval).
	    _smoother->setValue(10);    // EYE - magic number?
	    _smoother->setLegend("10"); // EYE - call callback instead?
	    _smoother->setCallback(_smoother_cb);
	    _smoother->setUserData((void *)this);
	}
	_yGroup->close();
	_yGroup->reveal();	// EYE - necessary?

	// EYE - assume all heights are the same.
	int height = _xAxes[GraphsWindow::DISTANCE]->height(), y = vSpace;
	_xAxes[GraphsWindow::DISTANCE]->setSize(y, width);
	y += height + vSpace;
	_xAxes[GraphsWindow::TIME]->setSize(y, width);
	y += height + vSpace;
	_xFrame->setSize(width, y);
	_xGroup->setSize(width, y);

	puBox *bbox =_smoother->getBBox();
	y = vSpace;
	_smoother->setPosition(hSpace, y);
	y += (bbox->max[1] - bbox->min[1]) + vSpace;
	_yAxes[GraphsWindow::CLIMB_RATE]->setSize(y, width);
	y += height + vSpace;
	_yAxes[GraphsWindow::SPEED]->setSize(y, width);
	y += height + vSpace;
	_yAxes[GraphsWindow::ALTITUDE]->setSize(y, width);
	y += height + vSpace;
	_yFrame->setSize(width, y);
	_yGroup->setSize(width, y);
    }
    _mainGroup->close();
    _mainGroup->reveal();

    _w = width;
    _h = height;
}

GraphsUI::~GraphsUI()
{
    puDeleteObject(_mainGroup);
}

void GraphsUI::setSize(int x, int h)
{
    _mainGroup->setPosition(x, 0);
    _h = h;
    _mainFrame->setSize(_w, _h);
    
    int width, height, y;

    _xGroup->getSize(&width, &height);

    _xFrame->setSize(_w, height);
    _xGroup->setSize(_w, height);
    _xGroup->getSize(&width, &height);
    y = _h - height;
    _xGroup->setPosition(0, y);

    _yGroup->getSize(&width, &height);

    _yFrame->setSize(_w, height);
    _yGroup->setSize(_w, height);
    _yGroup->getSize(&width, &height);
    y -= height;
    _yGroup->setPosition(0, y);
}

void GraphsUI::setXAxisType(GraphsWindow::XAxisType t)
{
    if (t == GraphsWindow::TIME) {
	_xAxes[GraphsWindow::TIME]->setSelected(true);
	_xAxes[GraphsWindow::DISTANCE]->setSelected(false);
    } else {
	_xAxes[GraphsWindow::TIME]->setSelected(false);
	_xAxes[GraphsWindow::DISTANCE]->setSelected(true);
    }
}

void GraphsUI::setYAxisType(GraphsWindow::YAxisType t, bool b)
{
    if (t == GraphsWindow::ALTITUDE) {
	_yAxes[GraphsWindow::ALTITUDE]->setSelected(b);
    } else if (t == GraphsWindow::SPEED) {
	_yAxes[GraphsWindow::SPEED]->setSelected(b);
    } else {
	_yAxes[GraphsWindow::CLIMB_RATE]->setSelected(b);
    }
}

void GraphsUI::setSmoothing(unsigned int s)
{
    static AtlasString buf;
    buf.printf("%d", _smoother->getIntegerValue());
    _smoother->setLegend(buf.str());

    _gw->setSmoothing(s);
}

void GraphsUI::_axisSelect(_AxisUI *axis) 
{
    if (axis == _xAxes[GraphsWindow::TIME]) {
	_gw->setXAxisType(GraphsWindow::TIME);
    } else if (axis == _xAxes[GraphsWindow::DISTANCE]) {
	_gw->setXAxisType(GraphsWindow::DISTANCE);
    } else if (axis == _yAxes[GraphsWindow::ALTITUDE]) {
	_gw->setYAxisType(GraphsWindow::ALTITUDE, axis->selected());
    } else if (axis == _yAxes[GraphsWindow::SPEED]) {
	_gw->setYAxisType(GraphsWindow::SPEED, axis->selected());
    } else if (axis == _yAxes[GraphsWindow::CLIMB_RATE]) {
	_gw->setYAxisType(GraphsWindow::CLIMB_RATE, axis->selected());
    }
}

void GraphsUI::_autoscale(_AxisUI *axis) 
{
    if (axis == _xAxes[GraphsWindow::TIME]) {
	_gw->setAutoscale(GraphsWindow::TIME, axis->autoscaling());
    } else if (axis == _xAxes[GraphsWindow::DISTANCE]) {
	_gw->setAutoscale(GraphsWindow::DISTANCE, axis->autoscaling());
    } else if (axis == _yAxes[GraphsWindow::ALTITUDE]) {
	_gw->setAutoscale(GraphsWindow::ALTITUDE, axis->autoscaling());
    } else if (axis == _yAxes[GraphsWindow::SPEED]) {
	_gw->setAutoscale(GraphsWindow::SPEED, axis->autoscaling());
    } else if (axis == _yAxes[GraphsWindow::CLIMB_RATE]) {
	_gw->setAutoscale(GraphsWindow::CLIMB_RATE, axis->autoscaling());
    }
}

void GraphsUI::_setScale(_AxisUI *axis) 
{
    if (axis == _xAxes[GraphsWindow::TIME]) {
	_gw->setScale(GraphsWindow::TIME, axis->scale());
    } else if (axis == _xAxes[GraphsWindow::DISTANCE]) {
	_gw->setScale(GraphsWindow::DISTANCE, axis->scale());
    } else if (axis == _yAxes[GraphsWindow::ALTITUDE]) {
	_gw->setScale(GraphsWindow::ALTITUDE, axis->scale());
    } else if (axis == _yAxes[GraphsWindow::SPEED]) {
	_gw->setScale(GraphsWindow::SPEED, axis->scale());
    } else if (axis == _yAxes[GraphsWindow::CLIMB_RATE]) {
	_gw->setScale(GraphsWindow::CLIMB_RATE, axis->scale());
    }
}

// Slider callback.  It just forwards the event to the Graphs object.

// EYE - I can't declare this static, even though I'd like to, because
// C++ complains that it doesn't match the declaration in Graphs.hxx.
// However, if I change that to static, then it declares that I'm not
// allowed to do that.
void _slider_cb(puObject *cb)
{
    GraphsWindow *me = (GraphsWindow *)cb->getUserData();
    me->_setSlider(cb, cb->getFloatValue());
}

// Draws the given string starting at the given point, in the current
// colour.
static void _drawString(const char *str, float x, float y)
{
    glRasterPos2f(x, y);
    for (unsigned int i = 0; i < strlen(str); i++) {
	// EYE - magic "number"
	glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, str[i]);
    }
}

GraphsWindow::GraphsWindow(const char *name, const char *regularFontFile, 
			   const char *boldFontFile, AtlasController *ac):
    AtlasBaseWindow(name, regularFontFile, boldFontFile), _ac(ac),
    _track(NULL), _graphTypes(0), _smoothing(0),
    _xAxisType(_XAXIS_COUNT), _time("time (s)"), 
    _dist("distance (nm)"), _shouldRerender(false),
    _shouldReload(false), _graphDL(0), _dragging(false)
{
    // Standard OpenGL attributes.
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0);

    // Set up fonts for this window.  By default AtlasBaseWindow fonts
    // are 12.0 points; we want them to be smaller.
    puFont uiFont(_regularFont, 10.0);
    puSetDefaultFonts(uiFont, uiFont);

    // EYE - allocate _time and _dist too?  Add to _values?  Have
    // _xValues and _yValues?  Make strings part of
    // preferences/internationalization stuff?
    _values[ALTITUDE] = new Altitudes("Altitude (ft)");
    _values[SPEED] = new Speeds("Speed (kt)");
    _values[CLIMB_RATE] = new RatesOfClimb("Climb Rate (ft/min)");

    // EYE - these should be made into preferences
    // Default fixed scales.
    _time.setScale(1.0);	 // 1.0 s/pixel
    _dist.setScale(0.005);	 // 0.005 nm/pixel
    _values[SPEED]->setScale(1.0);	 // 1.0 kt/pixel
    _values[ALTITUDE]->setScale(100.0);	 // 100.0 ft/pixel
    _values[CLIMB_RATE]->setScale(10.0); // 10.0 ft/s/pixel

    // Default mark and live aircraft colours are black.
    // EYE - set these from outside?  Get them directly from globals?
    GLfloat black[4] = {0.0, 0.0, 0.0, 1.0};
    setAircraftColour(black);
    setMarkColour(black);

    // Create our sliders, but don't display them.
    _xSlider = new puSlider(0, 0, 100, FALSE, sliderHeight);
    _xSlider->setCallback(_slider_cb);
    _xSlider->setUserData((void *)this);
    _xSlider->hide();
    _dist.setSlider(_xSlider);
    _time.setSlider(_xSlider);
    for (int i = 0; i < _GRAPH_TYPES_COUNT; i++) {
    	_ySliders[i] = new puSlider(0, 0, 100, TRUE, sliderHeight);
    	_ySliders[i]->setCallback(_slider_cb);
    	_ySliders[i]->setUserData((void *)this);
    	_ySliders[i]->hide();
	_values[i]->setSlider(_ySliders[i]);
    }

    // Create the UI.
    _ui = new GraphsUI(this);

    // Set some defaults.
    setFlightTrack(_ac->currentTrack());
    setSmoothing(10);

    // Subscribe to things that interest us.
    subscribe(Notification::AircraftMoved);
    subscribe(Notification::FlightTrackModified);
    subscribe(Notification::NewFlightTrack);
    subscribe(Notification::ShowTrackInfo);
}

GraphsWindow::~GraphsWindow()
{
    for (int t = ALTITUDE; t < _GRAPH_TYPES_COUNT; t++) {
	delete _values[t];
    }

    puDeleteObject(_xSlider);
    for (int i = 0; i < _GRAPH_TYPES_COUNT; i++) {
    	puDeleteObject(_ySliders[i]);
    }

    glDeleteLists(_graphDL, 1);
}

void GraphsWindow::special(int key, int x, int y)
{
    int win = set();
    _special(key, x, y);
    set(win);
}

void GraphsWindow::setAircraftColour(const float *colour)
{
    memcpy(_aircraftColour, colour, sizeof(float) * 4);
}

void GraphsWindow::setMarkColour(const float *colour)
{
    memcpy(_markColour, colour, sizeof(float) * 4);
}

void GraphsWindow::setFlightTrack(FlightTrack *t)
{
    if (t && _ac->showTrackInfo()) {
	reveal();
    } else {
	hide();
    }

    if (_track == t) {
	// If the new flight track is the same as the old, don't do
	// anything.
	return;
    }

    _track = t;

    _shouldRerender = _shouldReload = true;
}

// EYE - make smoothing a float?
void GraphsWindow::setSmoothing(unsigned int s)
{
    if (s == _smoothing) {
	return;
    }

    _smoothing = s;
    RatesOfClimb *roc = dynamic_cast<RatesOfClimb *>(_values[CLIMB_RATE]);
    assert(roc);
    roc->setSmoothing(s);

    // We need to reload the data because the rate of climb graph data
    // depends on the smoothing interval.
    // EYE - make reloading Values-specific?  In other words, just
    // mark _values[CLIMB_RATE]?
    _shouldRerender = _shouldReload = true;
    postRedisplay();
}

void GraphsWindow::setYAxisType(YAxisType t, bool b)
{
    if (_graphTypes[t] == b) {
	return;
    }
    _graphTypes[t] = b;
    _ui->setYAxisType(t, b);

    _shouldRerender = true;
    postRedisplay();
}

void GraphsWindow::setXAxisType(XAxisType t)
{
    if (t == _xAxisType) {
	return;
    }

    _xAxisType = t;
    _ui->setXAxisType(t);

    _shouldRerender = true;
    postRedisplay();
}

void GraphsWindow::toggleXAxisType()
{
    if (_xAxisType == TIME) {
	setXAxisType(DISTANCE);
    } else {
	setXAxisType(TIME);
    }
}

bool GraphsWindow::autoscale(XAxisType t) const 
{
    if (t == TIME) {
	return _time.autoscale();
    } else {
	return _dist.autoscale();
    }
}

bool GraphsWindow::autoscale(YAxisType t) const 
{
    return _values[t]->autoscale();
}

float GraphsWindow::scale(XAxisType t) const 
{
    if (t == TIME) {
	return _time.requestedScale();
    } else {
	return _dist.requestedScale();
    }
}

float GraphsWindow::scale(YAxisType t) const 
{
    return _values[t]->requestedScale();
}

void GraphsWindow::setAutoscale(XAxisType t, bool b)
{
    if (t == TIME) {
	_time.setAutoscale(b);
    } else {
	_dist.setAutoscale(b);
    }

    _shouldRerender = true;
    postRedisplay();
}

void GraphsWindow::setAutoscale(YAxisType t, bool b)
{
    _values[t]->setAutoscale(b);

    _shouldRerender = true;
    postRedisplay();
}

void GraphsWindow::setScale(XAxisType t, float f)
{
    if (t == TIME) {
	_time.setScale(f);
    } else {
	_dist.setScale(f);
    }

    _shouldRerender = true;
    postRedisplay();
}

void GraphsWindow::setScale(YAxisType t, float f)
{
    _values[t]->setScale(f);

    _shouldRerender = true;
    postRedisplay();
}

void GraphsWindow::_setSlider(puObject *slider, float val)
{
    slider->setValue(val);

    _shouldRerender = true;
    postRedisplay();
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
void GraphsWindow::notification(Notification::type n)
{
    if (n == Notification::AircraftMoved) {
	// At the moment we don't do anything when informed of
	// aircraft movement, since we unconditionally draw the
	// aircraft mark.
    } else if (n == Notification::FlightTrackModified) {
	_shouldRerender = true;
	_shouldReload = true;
    } else if ((n == Notification::NewFlightTrack) || 
	       (n == Notification::ShowTrackInfo)) {
	setFlightTrack(_ac->currentTrack());
    } else {
	assert(false);
    }

    postRedisplay();
}

// Draws all of our graphs (as given by graphTypes) into our window.
// Graphs span the window horizontally, and each graph gets equal
// space vertically.  The variables _shouldRerender and _shouldReload
// determine what work is actually performed.
//
// Assumes that _track is valid and that the _graphTypes value is
// correct.
//
// EYE - improvements:

// (1) Several display lists: raw graph, x and y axes of each graph.
//     Update the raw graph only if the flight track has changed; the
//     others if the flight track changes, or if the window changes
//     (enough to change _d).
//
// (2) Perhaps make _update smart enough to report if anything has
//     really changed.  Maybe it should be responsible for deleting
//     the outdated display list?
//
// (3) Have left and right margins, header and footer?
void GraphsWindow::_display()
{
    assert(glutGetWindow() == id());

    // Check errors before...
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
    	printf("GraphsWindow::display (before): %s\n", gluErrorString(error));
    }

    if (!_track) {
	return;
    }

    Values& xVals = _xValues();
    if (_shouldRerender) {
	// Make sure our data is up-to-date.
	_loadData();

	// Set our title.
	setTitle(_track->niceName());

	// Create a new display list.
	glDeleteLists(_graphDL, 1);
	_graphDL = glGenLists(1);
	assert(_graphDL != 0);
	glNewList(_graphDL, GL_COMPILE); {
	    // Clear everything to white.
	    glClearColor(1.0, 1.0, 1.0, 0.0);
	    glClear(GL_COLOR_BUFFER_BIT);

	    // No graphs to plot.  Just return.
	    if (_graphTypes.count() == 0) {
		return;
	    }

	    // EYE - need a routine for _w - (_margin * 2) expression
	    // EYE - check if this changes _showXSlider - if not, do nothing
	    int workingWidth = _w - (_margin * 2);
	    int workingHeight = _h;
	    xVals.setPixels(workingWidth);
	    puSlider *s = xVals.slider();
	    // EYE - combine with yVals code in _drawGraph?
	    if (xVals.showSlider()) {
		workingHeight -= sliderHeight;

		s->setPosition(_margin, workingHeight);
		s->setSize(workingWidth, sliderHeight);
		s->setSliderFraction((float)workingWidth /
				     (float)xVals.pixels());
		s->reveal();
	    } else {
		s->hide();
	    }
	    workingHeight = (_h / _graphTypes.count()) - (_header * 2);

	    // x, y, width, height
	    int viewport[4];
	    viewport[0] = _margin;
	    viewport[1] = _header;
	    viewport[2] = workingWidth;
	    viewport[3] = workingHeight;

	    for (int t = _GRAPH_TYPES_COUNT - 1; t >= ALTITUDE; t--) {
		// EYE - ugly?
		_values[t]->slider()->hide();
		if (_graphTypes[t]) {
		    _values[t]->setPixels(workingHeight);
		    _drawGraph(xVals, *_values[t], viewport);
		    viewport[1] += viewport[3] + (2 * _header);
		}
	    }
	}
	glEndList();

	_shouldRerender = false;
    }

    glCallList(_graphDL);

    // Draw current mark and, if it's live, current aircraft position
    // (which is always at the end of the track, but it's drawn just
    // to reinforce the fact that it's live).  We draw the mark using
    // time or distance coordinates in the x-axis, pixel coordinates
    // in the y-axis.
    glPushAttrib(GL_LINE_BIT);
    glPushMatrix(); {
	// We draw the mark unsmoothed.
	glDisable(GL_LINE_SMOOTH);

	// EYE - just fiddle with the projection matrix instead?
	glLoadIdentity();
	glTranslatef(_margin, 0.0, 0.0);
	glScalef(1.0 / xVals.scale(), 1.0, 0.0);
	glTranslatef(-xVals.first() - _offset(xVals), 0.0, 0.0);

	if (_track->live()) {
	    glBegin(GL_LINES); {
		glColor4fv(_aircraftColour);
		glVertex2f(xVals.max(), 0.0);
		glVertex2f(xVals.max(), (float)_h);
	    }
	    glEnd();
	}
	if (_track->mark() != FlightTrack::npos) {
	    glBegin(GL_LINES); {
		glColor4fv(_markColour);
		glVertex2f(xVals.at(_track->mark()), 0.0);
		glVertex2f(xVals.at(_track->mark()), (float)_h);
	    }
	    glEnd();
	}
    }
    glPopMatrix();
    glPopAttrib();

    puDisplay();

    glutSwapBuffers();

    // ... and check errors at the end.
    error = glGetError();
    if (error != GL_NO_ERROR) {
	printf("GraphsWindow::display (after): %s\n", gluErrorString(error));
    }
}

void GraphsWindow::_reshape(int w, int h)
{
    assert(glutGetWindow() == id());

    // EYE - refuse to resize below a minimum size?
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glViewport(0, 0, w, h);

    _w = w - _ui->width();
    _h = h;

    _ui->setSize(_w, _h);

    _shouldRerender = true;
}

// Called after a mouse click.
void GraphsWindow::_mouse(int button, int state, int x, int y)
{
    assert(glutGetWindow() == id());

    // If we're dragging, we only respond to one mouse event: a mouse
    // up of button 1.
    if (_dragging) {
	if ((button == GLUT_LEFT_BUTTON) && (state == GLUT_UP)) {
	    _dragging = false;
	}
	return;
    }

    // If PUI eats the event, then return right away.
    if (puMouse(button, state, x, y)) {
	return;
    }

    // At this point, we only respond to one event: a mouse down of
    // button 1.
    if ((button != GLUT_LEFT_BUTTON) || (state != GLUT_DOWN)) {
	return;
    }

    // At this point, we know we're starting a drag.
    _dragging = true;

    // Tell the Atlas controller where the aircraft should move to.
    _ac->setMark(_pixelToPoint(x));
}

// Called during a drag.
void GraphsWindow::_motion(int x, int y)
{
    assert(glutGetWindow() == id());

    if (_dragging) {
	_ac->setMark(_pixelToPoint(x));
    } else {
	// No dragging, so give it to PUI, just in case it's interested.
	puMouse(x, y);
    }
}

// Called when the user presses a key in the graphs window.  We just
// pass the key on to the handler for the main window.
void GraphsWindow::_keyboard(unsigned char key, int x, int y)
{
    assert(glutGetWindow() == id());

    if (puKeyboard(key, PU_DOWN)) {
	// EYE - or puDisplay()?
	postRedisplay();
    } else {
	// EYE - keyboard does a call to puKeyboard.  Is this okay
	// (especially if we do the same in the future)?
	globals.aw->keyboard(key, x, y);
    }
}

// Called when the user presses a "special" key in the graphs window,
// where "special" includes directional keys.
void GraphsWindow::_special(int key, int x, int y)
{
    assert(glutGetWindow() == id());

    size_t offset = 1;
    if (glutGetModifiers() & GLUT_ACTIVE_SHIFT) {
	// If the user presses the shift key, right and left arrow
	// clicks move 10 times as far.
	offset *= 10;
    }

    FlightTrack *t = _ac->currentTrack();
    switch (key + PU_KEY_GLUT_SPECIAL_OFFSET) {
      case PU_KEY_LEFT:
	if (t->mark() >= offset) {
	    _ac->setMark(t->mark() - offset);
	} else {
	    _ac->setMark(0);
	}
	break;
      case PU_KEY_RIGHT:
	// Note that since we're using unsigned integers, we have to
	// be very careful with comparisons (in fact, this comparison
	// will still fail if the track length is close to the maximum
	// unsigned int).
	if ((t->mark() + offset) < t->size()) {
	    _ac->setMark(t->mark() + offset);
	} else {
	    _ac->setMark(t->size() - 1);
	}
	break;
      case PU_KEY_HOME:
	_ac->setMark(0);
	break;
      case PU_KEY_END:
	_ac->setMark(t->size() - 1);
	break;
      default:
	return;
    }
}

void GraphsWindow::_visibility(int state)
{
    assert(glutGetWindow() == id());

    if (state == GLUT_VISIBLE) {
	postRedisplay();
    }
}

// Draws a graph as x vs y, in the given viewport (which should
// specify the space for the graph, ignoring labels).  It takes into
// account the position of the x slider.
void GraphsWindow::_drawGraph(Values &xVals, Values& yVals, int viewport[4])
{
    // EYE - should yVals do this itself?
    if (yVals.showSlider()) {
	puSlider *s = yVals.slider();
	s->setPosition(viewport[0] + viewport[2], viewport[1]);
	s->setSize(sliderHeight, viewport[3]);
	s->setSliderFraction((float)viewport[3] / (float)yVals.pixels());
	s->reveal();
    }

    float world[4];		// x, y, width, height
    world[0] = xVals.first();
    world[1] = yVals.first();
    world[2] = viewport[2] * xVals.scale();
    world[3] = viewport[3] * yVals.scale();

    // Adjust for slider.
    world[0] += _offset(xVals);
    world[1] += _offset(yVals);

    // We fiddle with the viewport and matrix mode throughout, so save
    // their states.
    glPushAttrib(GL_VIEWPORT_BIT | GL_TRANSFORM_BIT); {
	// Draw the axes.
	_drawGraphAxis(xVals, viewport, world);
	_drawGraphAxis(yVals, viewport, world, true);

	// Draw the actual graph.
	glMatrixMode(GL_PROJECTION);
	glPushMatrix(); {
	    glLoadIdentity();
	    // The clipping planes are set to the visible parts of the
	    // graph.
	    gluOrtho2D(world[0], world[0] + world[2], 
		       world[1], world[1] + world[3]);

	    // The graph will only occupy one part of the window.
	    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

	    // Draw it.
	    yVals.draw(xVals);
	}
	glPopMatrix();
    }
    glPopAttrib();
    
    // Label the graph.
    glColor4fv(labelColour);
    _drawString(yVals.label(), viewport[0] + viewport[2] / 2, 
		viewport[1] + viewport[3] / 2);
}

// Draws the axis for the given Values object.  This does not draw the
// graph itself, but does draw the axis line, the ticks along the
// line, the labels of the ticks, and the "tick extensions".
void GraphsWindow::_drawGraphAxis(Values &vals, int viewport[4], float world[4],
			    bool vertical)
{
    // This complicated fiddling business is done here so that the
    // drawAxis method can be written generically, not caring about
    // whether the axis is vertical or horizontal, nor the extent of
    // the graph in the graphs window.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix(); {
	// First set up the clipping planes.
	glLoadIdentity();
	if (!vertical) {
	    // The clipping planes are set to the visible parts of the
	    // graph.  We use graph units along the x axis (the first
	    // two parameters), and pixels along the y axis (the last
	    // two).
	    gluOrtho2D(world[0], world[0] + world[2], -_header, viewport[3]);

	    // The graph will only occupy one part of the window.
	    glViewport(viewport[0], viewport[1] - _header, 
		       viewport[2], viewport[3] + _header);
	} else {
	    // Because we're vertical, the x axis is in pixels, and
	    // the y axis is in graph units.
	    gluOrtho2D(-_margin, viewport[2], world[1], world[1] + world[3]);
	    glViewport(viewport[0] - _margin, viewport[1], 
		       viewport[2] + _margin, viewport[3]);
	}
	
	// Now draw the axis.
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix(); {
	    glLoadIdentity();
	    if (!vertical) {
		vals.drawAxis(world[0], world[0] + world[2], 
			      viewport[3], _header);
	    } else {
		// The drawAxis() routine assumes it's drawing along
		// the x axis.  To make it vertical, we rotate 90
		// degrees around the z axis (ie, counter-clockwise),
		// then 180 degrees around the y axis (so that labels
		// are to the left of the axis).
		glRotatef(-90.0, 0.0, 0.0, 1.0);
		glRotatef(180.0, 0.0, 1.0, 0.0);

		vals.drawAxis(world[1], world[1] + world[3], 
			      viewport[2], _margin);
	    }
	}
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
    }
    glPopMatrix();
}

GraphsWindow::Values& GraphsWindow::_xValues()
{
    if (_xAxisType == TIME) {
	return _time;
    } else {
	return _dist;
    }
}

float GraphsWindow::_offset(Values& v)
{
    if (!v.showSlider()) {
	return 0.0;
    } else {
	puSlider *s = v.slider();
	return v.range() * s->getFloatValue() * (1.0 - s->getSliderFraction());
    }
}

bool GraphsWindow::Values::showSlider()
{
    return (!_autoscale && (pixels() > _requestedPixels));
}

// Finds and returns index of the record in the flight track closest
// to the given x coordinate in the window.  Because time and distance
// are irregular (ie, the time/distance between successive records is
// not constant), finding the corresponding point in the flight track
// requires a search.  We're stupid and just do a linear search from
// the beginning - we really should be a bit smarter about it.
size_t GraphsWindow::_pixelToPoint(int x)
{
    assert(glutGetWindow() == id());

    // Reload our data (if necessary).
    _loadData();

    // Figure out how to transform window values to graph values.  Our
    // graph has a regular scale - each pixel corresponds to a fixed
    // amount of time or distance.  The x coordinate therefore
    // corresponds to a certain time/distance.
    Values& xVals = _xValues();
    float offset = _offset(xVals);
    float scale = xVals.scale();
    float xVal = (x - _margin) * scale + offset + xVals.first();

    // Clip to a valid value.
    if ((xVal < xVals.min()) || (xVal > xVals.max())) {
	if (xVal < xVals.min()) {
	    xVal = xVals.min();
	} else {
	    xVal = xVals.max();
	}
	// Adjust x to the new value.
	x = (xVal - offset - xVals.first()) / scale + _margin;
    }

    // If the slider is showing, it's possible our point is not
    // visible.  If so, scroll it into view.
    if (xVals.showSlider() && ((x < _margin) || (x > (_w - _margin)))) {
	// EYE - this is not a very good way of doing things - the
	// user must keep moving the mouse to make the slider keep
	// moving.  However, making it behave better is really a GUI
	// issue, and the solution is to get a better GUI than PUI,
	// rather than hand-rolling our own code.
	if (x < _margin) {
	    // Off the left side - reduce the offset.
	    offset -= ((_margin - x) + _margin) * scale;
	} else {
	    // Off the right side - increase the offset.
	    offset += ((x - (_w - _margin)) + _margin) * scale;
	}
	float sliderVal = offset / xVals.range() / 
	    (1.0 - xVals.slider()->getSliderFraction());
	if (sliderVal < 0.0) {
	    sliderVal = 0.0;
	} else if (sliderVal > 1.0) {
	    sliderVal = 1.0;
	}
	_setSlider(xVals.slider(), sliderVal);
    }

    // Find a point close to our x value, as expressed in x axis units
    // (time or distance).  Note that we may not have an exact match,
    // so we try to find the closest.
    for (size_t i = 0; i < xVals.size(); i++) {
	if (xVals.at(i) > xVal) {
	    // We've passed the cutoff.  Check if the point we're at
	    // is better than the point we just passed.
	    if (i == 0) {
		return i;
	    }
	    // Note that this bit of code assumes that x values
	    // increase.  This is true for distance, but isn't always
	    // true for time.  On the other hand, we don't have any
	    // god way to handle non-increasing time values, so we'll
	    // just close our eyes and hope nothing happens.
	    float xVal0 = xVals.at(i) - xVal;
	    float xVal1 = xVal - xVals.at(i - 1);
	    if (xVal0 < xVal1) {
		return i;
	    } else {
		return (i - 1);
	    }
	}
    }

    // At the end.
    return (_track->size() - 1);
}

// Sets the Values variables, based on the values in _track.  In
// general, this should be called whenever we detect that _track has
// changed.
void GraphsWindow::_loadData()
{
    if (_shouldReload) {
	_time.setFlightTrack(_track);
	_dist.setFlightTrack(_track);
	for (int t = ALTITUDE; t < _GRAPH_TYPES_COUNT; t++) {
	    _values[t]->setFlightTrack(_track);
	}

	_shouldReload = false;
    }
}

//////////////////////////////////////////////////////////////////////
// Values
//////////////////////////////////////////////////////////////////////

GraphsWindow::Values::Values(const char *label): 
    _ft(NULL), _min(0.0), _max(0.0), _autoscale(true), _pixels(0), _scale(0.0), 
    _first(0.0), _last(0.0), _d(0.0), _D(0.0), _decimals(0), _dirty(false),
    _slider(NULL)
{
    _label = strdup(label);
}

GraphsWindow::Values::~Values()
{
    free(_label);
}

void GraphsWindow::Values::setFlightTrack(FlightTrack *ft)
{
    _ft = ft;
    load();
}

size_t GraphsWindow::Values::size()
{
    return _ft->size();
}

// EYE - make private?
void GraphsWindow::Values::load()
{
    if (_ft == NULL) {
	return;
    } else if (_ft->size() > 0) {
	_setMinMax(at(0), true);
    }

    for (size_t i = 1; i < _ft->size(); i++) {
	_setMinMax(at(i));
    }
}

// Return the *calculated* size of the graph, in pixels.
int GraphsWindow::Values::pixels()
{
    _update();

    return _pixels;
}

// Return the *calculated* scale of the graph, in units/pixel.
float GraphsWindow::Values::scale()
{
    _update();

    return _scale;
}

float GraphsWindow::Values::first()
{
    _update();

    return _first;
}

float GraphsWindow::Values::last()
{
    _update();

    return _last;
}

float GraphsWindow::Values::range()
{
    _update();

    return _last - _first;
}

float GraphsWindow::Values::d()
{
    _update();

    return _d;
}

float GraphsWindow::Values::D()
{
    _update();

    return _D;
}

int GraphsWindow::Values::decimals()
{
    _update();

    return _decimals;
}

void GraphsWindow::Values::setAutoscale(bool b)
{
    if (_autoscale == b) {
	// Nothing has changed.
	return;
    }
    _autoscale = b;
    _dirty = true;
}

void GraphsWindow::Values::setPixels(int pixels)
{
    if (_requestedPixels == pixels) {
	// Nothing has changed.
	return;
    }
    _requestedPixels = pixels;
    if (_autoscale) {
	_dirty = true;
    }
}

void GraphsWindow::Values::setScale(float scale)
{
    if (_requestedScale == scale) {
	// Nothing has changed.
	return;
    }
    _requestedScale = scale;
    if (!_autoscale) {
	_dirty = true;
    }
}

void GraphsWindow::Values::drawAxis(float from, float to, int height, int margin)
{
    // numeric_limits::epsilon() is the difference between 1.0 and the
    // smallest number greater than 1.0.  We multiply it by 'to',
    // since, because of the way floating-point numbers are
    // represented, epsilon is larger for larger numbers.  Hopefully
    // the relationship is linear.
    float epsilon = std::numeric_limits<float>::epsilon() * to;

    // Update all our values.  We do this here, then access the
    // derived values directly, just to save a teeny-tiny bit of time.
    _update();

    glPushAttrib(GL_LINE_BIT); {
	// We draw the graph axes and ticks unsmoothed - it looks nicer.
	glDisable(GL_LINE_SMOOTH);

	// Draw the axis.
	glColor4fv(axisColour);
	glBegin(GL_LINES); {
	    glVertex2f(from, 0.0);
	    glVertex2f(to, 0.0);
	}
	glEnd();

	// Adjust from and to so that they have nice values.
	from = floor(from / _d) * _d;
	to = ceil(to / _d) * _d;

	// Draw the "ticks" for axis.  To avoid problems with
	// increasing round-off errors, we use an integer to control
	// the loop.
	int intervals = rint((to - from) / _d);
	int majorTicks = 0;
	glBegin(GL_LINES); {
	    for (int i = 1; i <= intervals; i++) {
		float tick = from + (i * _d);
		glColor4fv(axisColour);
		glVertex2f(tick, 0.0);
		if (fabs(remainderf(tick, _D)) < epsilon) {
		    // Major tick.
		    glVertex2f(tick, majorTickSize);

		    glColor4fv(majorTickColour);
		    glVertex2f(tick, majorTickSize);
		    glVertex2f(tick, height);
		
		    majorTicks++;
		} else {
		    // Minor tick.
		    glVertex2f(tick, minorTickSize);

		    glColor4fv(minorTickColour);
		    glVertex2f(tick, minorTickSize);
		    glVertex2f(tick, height);
		}
	    }
	}
	glEnd();

	// Now label the axis.
	AtlasString format, buf;
	format.printf("%%.%df", _decimals);
	for (int i = 0; i <= intervals; i++) {
	    float tick = from + (i * _d);
	    // In general, we just label major ticks.  However, there
	    // are cases where there is only one major tick.  Just
	    // labelling that one tick means it's impossible for the
	    // user to know the scale.
	    //
	    // So, we label this tick if: (a) it's major, or (b) it's
	    // the first or last one and there are fewer than 2 major
	    // ticks.
	    if ((fabs(remainderf(tick, _D)) < epsilon) ||
		((majorTicks < 2) && (i == 0)) ||
		((majorTicks < 2) && (i == intervals))) {
		glColor4fv(axisColour);
		buf.printf(format.str(), tick);
		// Draw the string just inside the margin.  I note
		// that if I use -margin, then sometimes the strings
		// don't appear, presumably due to rounding issues.
		_drawString(buf.str(), tick, -(margin - 1));
	    }
	}
    }
    glPopAttrib();
}

void GraphsWindow::Values::draw(Values& xVals)
{
    glColor4fv(graphColour);
    glBegin(GL_LINE_STRIP); {
	assert(xVals.size() == size());
	for (size_t i = 0; i < size(); i++) {
	    glVertex2f(xVals.at(i), at(i));
	}
    }
    glEnd();
}


// A convenience routine to set _min and _max.  It makes sure that
// _dirty is updated if necessary.  If you want to force _min and _max
// to be set to the given values, set force to true (default = false).
void GraphsWindow::Values::_setMinMax(float val, bool force)
{
    if (force) {
	_dirty = ((_min != val) || (_max != val));
	_min = _max = val;
    } else {
	if (val < _min) {
	    _min = val;
	    _dirty = true;
	} else if (val > _max) {
	    _max = val;
	    _dirty = true;
	}
    }
}

// Given the minimum and maximum values of our data (_min, _max), and
// either the requested space for the axis (_requestedPixels) in
// autoscale mode, or the requested scale (_requestedScale) otherwise,
// calculates all derived parameters: _pixels, _scale, _first, _last,
// _d, _D, and _decimals.  Sets _dirty to false.  If _dirty is true on
// entry, does nothing.
void GraphsWindow::Values::_update()
{
    const int minimum = 10;	// Minimum small interval, in pixels.
    float actual;		// Actual interval, if min is placed
				// at 0 and max at pixels.
    const float defaultRange = 10.0; // When there's no difference
				     // between _min and _max, we use
				     // this as a default display
				     // range.

    if (!_dirty) {
	return;
    }

    // Get the actual situation: the actual value of an interval of
    // 'minimum' pixels using the requested graph size (in autoscale
    // mode) or requested scale (in fixed scale mode).  In autoscale
    // mode, we honour the graph size, but may adjust the scale; in
    // fixed scale mode, we honour the requested scale but may adjust
    // the graph size.
    if (_autoscale) {
	if (_max != _min) {
	    actual = (_max - _min) / _requestedPixels * minimum;
	} else {
	    // If _min = _max, that means there is no range, so we use
	    // the default of defaultRange.
	    actual = defaultRange / _requestedPixels * minimum;
	}
    } else {
	actual = _requestedScale * minimum;
    }

    // Break it down into an exponent (base-10) and mantissa.
    int exponent = floor(log10(actual));
    float mantissa = actual / pow(10.0, exponent);

    // Our heuristic: good mantissa values are 2, 5, and 10.  We move
    // up from our actual mantissa to the next good mantissa value,
    // which means that our ticks are spaced no less than 'minimum'
    // pixels apart.  We also calculate how many digits should be
    // printed after the decimal place.  This will be used in a printf
    // statement precision string.
    if (mantissa < 2) {
	_d = 2 * pow(10.0, exponent);
	_decimals = -exponent - 1;
    } else if (mantissa < 5) {
	_d = 5 * pow(10.0, exponent);
	_decimals = -exponent;
    } else {
	_d = 10 * pow(10.0, exponent);
	_decimals = -exponent - 1;
    }
    // Some printf's treat negative precision values as 0 (which would
    // work for us), but some just skip over them, so we make sure
    // that _decimals is never less than 0.
    _decimals = std::max(_decimals, 0);

    // Large ticks are always placed every 5 small ticks.
    _D = _d * 5;

    // We want the start and end of the range begin on nice values, so
    // find the largest nice value less than our minimum real value,
    // and the smallest nice value greater than our maximum real
    // value.
    if (_min != _max) {
	_first = floor(_min / _d) * _d;
	_last = ceil(_max / _d) * _d;
    } else {
	// Special case again.  If there's no variation in values, we
	// use the variation given by 'var'.
	_first = floor((_min - defaultRange / 2.0) / _d) * _d;
	_last = ceil((_max + defaultRange / 2.0) / _d) * _d;
    }

    // Calculate the derived values: in autoscale mode, this means the
    // scale; in fixed scale mode, this means the graph size.
    if (_autoscale) {
	_scale = (_last - _first) / _requestedPixels;
	_pixels = _requestedPixels;
    } else {
	_scale = _requestedScale;
	_pixels = (_last - _first) / _requestedScale;
    }

    _dirty = false;
}

//////////////////////////////////////////////////////////////////////
// Times
//////////////////////////////////////////////////////////////////////

GraphsWindow::Times::Times(const char *label): Values(label)
{
}

float GraphsWindow::Times::at(size_t i)
{
    return _ft->at(i)->est_t_offset;
}

//////////////////////////////////////////////////////////////////////
// Distances
//////////////////////////////////////////////////////////////////////

GraphsWindow::Distances::Distances(const char *label): Values(label)
{
}

float GraphsWindow::Distances::at(size_t i)
{
    return _ft->at(i)->dist * SG_METER_TO_NM;
}

//////////////////////////////////////////////////////////////////////
// Speeds
//////////////////////////////////////////////////////////////////////

GraphsWindow::Speeds::Speeds(const char *label): Values(label)
{
}

float GraphsWindow::Speeds::at(size_t i)
{
    return _ft->at(i)->spd;
}

//////////////////////////////////////////////////////////////////////
// Altitudes (including glideslopes)
//////////////////////////////////////////////////////////////////////

GraphsWindow::Altitudes::Altitudes(const char *label): Values(label)
{
}

GraphsWindow::Altitudes::~Altitudes()
{
    for (size_t i = 0; i < _GSs.size(); i++) {
	delete _GSs[i];
    }
}

float GraphsWindow::Altitudes::at(size_t i)
{
    return _ft->at(i)->alt;
}

// Loads altitude data.  The altitude data itself is simple - the
// tough part is the glideslope information, held in the _GSs
// structure.  We want to create 'chunks', where a chunk is a
// contiguous section of glideslope information for a single radio.
// This means that different radios will have different chunks, and
// that a given radio can be composed of several chunks if we go in
// and out of radio range.
//
// EYE - this is inefficient (and not just for glideslopes), because
// load() potentially gets called for each new point added.  We need a
// better way.  Maybe this data should be maintained in the
// FlightTrack (perhaps with the exception of rate of climb, and maybe
// some derived data for glideslopes).  On the other hand, this is
// only an issue for live tracks - file-based tracks will call
// _loadData once only.
void GraphsWindow::Altitudes::load()
{
    if (_ft == NULL) {
	return;
    }

    // "Load" altitude data first (this actually just sets _min and
    // _max).
    Values::load();

    // We plot a glideslope if the aircraft is within 35 degrees
    // either side of its heading.  The figure is fairly arbitrary; I
    // just chose it so that we only show the glideslope when we're
    // reasonably close to the localizer.  The 35 degree figure *does*
    // correspond to the localizer limits at 10nm (FAA AIM).
    const float maxRange = 35.0;

    for (size_t i = 0; i < _GSs.size(); i++) {
	delete _GSs[i];
    }
    _GSs.clear();

    // Only Atlas flight tracks have navaid information, so quit early
    // if it's NMEA.
    if (!_ft->isAtlasProtocol()) {
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
    _GSSection *active[_RADIO_COUNT];
    for (int i = 0; i < _RADIO_COUNT; i++) {
	active[i] = NULL;
    }
    // For each active glideslope, we create 3 planes (the
    // mathematical ones, not the flying ones) representing the
    // bottom, middle, and top of the glideslope beam.  This is what
    // we draw on the graph.
    _Planes planes[_RADIO_COUNT];

    // Now step through the flight track.
    for (size_t i = 0; i < _ft->size(); i++) {
	FlightData *p = _ft->at(i);

	// Check each active navaid to see if: (a) it's a glideslope,
	// (b) it's in our "cone of interest" (as defined by
	// 'maxRange').  If it is, then we need to graph it.
	const vector<NAV *>& navaids = p->navaids();
	for (size_t j = 0; j < navaids.size(); j++) {
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
	    _extractHeadingSlope(n, &gsHeading, &junk);

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
		active[radio] = new _GSSection;
		active[radio]->radio = radio;
		_GSs.push_back(active[radio]);
		_createPlanes(n, &(planes[radio]));
	    }

	    // At this point, 'active[radio]' points to our currently
	    // active section and 'planes[radio]' are the planes
	    // defining the glideslope.  Now fill in the data for our
	    // new glideslope point.
	    _GSValue gs;

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

void GraphsWindow::Altitudes::draw(Values& xVals)
{
    // Draw the glideslopes behind.
    _drawGSs(xVals);

    // Draw the altitude graph on top.
    Values::draw(xVals);
}

void GraphsWindow::Altitudes::_drawGSs(Values& xVals)
{
    if (_GSs.size() == 0) {
	// No glideslope chunks to draw.
	return;
    }

    for (size_t i = 0; i < _GSs.size(); i++) {
	_GSSection *s = _GSs[i];

	float *c;
	if (s->radio == NAV1) {
	    c = globals.vor1Colour;
	} else {
	    assert(s->radio == NAV2);
	    c = globals.vor2Colour;
	}

	glBegin(GL_QUAD_STRIP); {
	    for (size_t j = 0; j < s->vals.size(); j++) {
		_GSValue& v = s->vals[j];

		glColor4f(c[0], c[1], c[2], v.opacity);
		glVertex2d(xVals.at(v.x), v.bottom);
		glVertex2d(xVals.at(v.x), v.top);
	    }
	}
	glEnd();
    }

    // Now draw lines representing the top, centre, and bottom
    // of the glideslope.
    glColor4fv(glideslopeOutlineColour);
    for (size_t i = 0; i < _GSs.size(); i++) {
	_GSSection *s = _GSs[i];

	glBegin(GL_LINE_STRIP); {
	    for (size_t j = 0; j < s->vals.size(); j++) {
		_GSValue& v = s->vals[j];
		glVertex2d(xVals.at(v.x), v.top);
	    }
	}
	glEnd();
	glBegin(GL_LINE_STRIP); {
	    for (size_t j = 0; j < s->vals.size(); j++) {
		_GSValue& v = s->vals[j];
		glVertex2d(xVals.at(v.x), v.middle);
	    }
	}
	glEnd();
	glBegin(GL_LINE_STRIP); {
	    for (size_t j = 0; j < s->vals.size(); j++) {
		_GSValue& v = s->vals[j];
		glVertex2d(xVals.at(v.x), v.bottom);
	    }
	}
	glEnd();
    }
}

// Extracts the heading and slope from a glideslope.
void GraphsWindow::Altitudes::_extractHeadingSlope(NAV *n, double *heading, 
					     double *slope)
{
    // The glideslope's heading is given by the lower 3 digits of the
    // magvar variable.  The thousands and above give slope:
    // ssshhh.hhh.
    *heading = fmod((double)n->magvar, 1000.0);
    *slope = (n->magvar - *heading) / 1e5;
}

// Calculates the planes for the given glideslope.
void GraphsWindow::Altitudes::_createPlanes(NAV *n, _Planes *planes)
{
    // Glideslopes have a vertical angular width of 0.7 degrees above
    // and below the centre of the glideslope (FAA AIM).
    const float glideSlopeWidth = 0.7;

    // The glideslope can be thought of as a plane tilted to the
    // earth's surface.  The top and bottom are given by planes tilted
    // 0.7 degrees more and less than the glideslope, respectively.
    double gsHeading, gsSlope;
    _extractHeadingSlope(n, &gsHeading, &gsSlope);

    // The 'rot' matrix will rotate from a standard orientation (up =
    // positive y-axis, ahead = positive z-axis, and right = negative
    // x-axis) to the navaid's actual orientation.
    sgdMat4 rot, mat;
    sgdMakeRotMat4(rot, n->lon - 90.0, n->lat, -gsHeading + 180.0);
    sgdMakeTransMat4(mat, n->_bounds.center);
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

//////////////////////////////////////////////////////////////////////
// Rates of Climb
//////////////////////////////////////////////////////////////////////

GraphsWindow::RatesOfClimb::RatesOfClimb(const char *label): Values(label)
{
}

float GraphsWindow::RatesOfClimb::at(size_t i)
{
    // EYE - check bounds?  (Also for all other at() methods)
    return _data[i];
}

void GraphsWindow::RatesOfClimb::load()
{
    if (_ft == NULL) {
	return;
    }

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

    // EYE - pass _smoothing in?

    _data.clear();
    for (size_t i = 0, j = 0; i < _ft->size(); i++) {
	float lastAlt, thisAlt;
	float lastTime, thisTime;
	float value;

	thisAlt = _altAt(i);
	thisTime = _timeAt(i);
	// EYE - I really should check to make sure this is correct.
	if (i == 0) {
	    // The first climb rate is 0.0 by default.
	    value = 0.0;

	    lastAlt = thisAlt;
	    lastTime = thisTime;

	    _setMinMax(value, true);
	} else if (_smoothing == 0) {
	    // A smoothing of 0 means we want simultaneous climb
	    // rates.  This is impossible, since we don't have that
	    // information.  The best we can do is use as small an
	    // interval as possible, which means looking back at the
	    // immediately preceding point.
	    value = (thisAlt - lastAlt) / (thisTime - lastTime) * 60.0;

	    lastAlt = thisAlt;
	    lastTime = thisTime;
	} else {
	    if ((_timeAt(i) - _timeAt(j)) > _smoothing) {
		// 'j' has fallen behind.  We need to move it forward
		// until it's back within range.
		j++;
		while ((_timeAt(i) - _timeAt(j)) > _smoothing) {
		    j++;
		}
	    }

	    // EYE - this seems overly complicated - check it (or
	    // document it).
	    if (j == 0) {
		lastAlt = _altAt(j);
		lastTime = _timeAt(j);
	    } else {
		float time0, time1, alt0, alt1;
		time0 = _timeAt(j - 1);
		time1 = _timeAt(j);
		alt0 = _altAt(j - 1);
		alt1 = _altAt(j);
		
		lastTime = _timeAt(i) - _smoothing;
		lastAlt = (lastTime - time0) / (time1 - time0) * (alt1 - alt0) 
		    + alt0;
	    }
	    value = (thisAlt - lastAlt) / (thisTime - lastTime) * 60;
	}

	_data.push_back(value);
	_setMinMax(value);
    }
}

float GraphsWindow::RatesOfClimb::_altAt(size_t i)
{
    return _ft->at(i)->alt;
}

float GraphsWindow::RatesOfClimb::_timeAt(size_t i)
{
    return _ft->at(i)->est_t_offset;
}
