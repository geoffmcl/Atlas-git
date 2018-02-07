/*-------------------------------------------------------------------------
  NavaidsOverlay.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2017 Brian Schack

  Loads and draws navaids (VORs, NDBs, ILS systems, and DMEs).

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

#ifndef _NAVAIDS_OVERLAY_H
#define _NAVAIDS_OVERLAY_H

#include "LayoutManager.hxx"
#include "Notifications.hxx"
#include "Overlays.hxx"
#include "NavData.hxx"		// Needed for Marker Type enumeration.

// Forward class declarations
class NavData;
class FlightData;

//////////////////////////////////////////////////////////////////////
// DisplayList
//////////////////////////////////////////////////////////////////////

// This is a convenience class for managing OpenGL display lists.
// Calling begin() starts the compilation of a display list (and
// generates a display list, if this is the first call to begin()).
// Every call to begin() must be paired with a call to end().  Nested
// begin()/end() pairs are not allowed.  The resulting display list
// can be rendered by calling call().  The display list is deleted
// when the destructor is called.
//
// A display list is valid if a begin()/end() pair has been called for
// it.  Calling call() will fail for an invalid display list.  They
// can be invalidated explicitly by calling invalidate().
//
// This class does a bit of error checking.  Calls to begin() are not
// allowed if a begin() is already in progress.  Calls to end() fail
// if there has been no corresponding begin().  "Failure" for this
// class means a failed assertion, resulting in program termination.
// Maybe one day I'll implement exceptions instead.
//
// All display lists are compiled using GL_COMPILE, rather than
// GL_COMPILE_AND_EXECUTE.
class DisplayList {
  public:
    DisplayList();
    ~DisplayList();

    // Define the display list.  Put OpenGL rendering calls between
    // the calls to begin() and end().
    void begin();
    void end();

    // Render/draw the display list.
    void call();

    void invalidate() { _valid = false; }
    bool valid() { return _valid; }

    // Return the display list "name".
    GLuint dl() { return _dl; }

  protected:
    // True if begin() has been called for some display list (only one
    // can be compiled at a time).  When end() is called, it is reset
    // to false.
    static bool _compiling;

    GLuint _dl;
    bool _valid;
};

//////////////////////////////////////////////////////////////////////
// NavaidRenderer
//////////////////////////////////////////////////////////////////////

// Most navaid icons are scaled according their range and a few other
// factors.  This structure encapsulates those factors.  If we choose
// a different rendering policy (eg, fixed pixel size, fixed world
// size, ...), then this will have to be changed.
struct IconScalingPolicy {
    // How big to draw the icon as a proportion of the navaid's range.
    float rangeScaleFactor;
    // The smallest size at which to draw the icon (after scaling).
    // Below this, it is not drawn at all.
    float minSize;		// Pixels
    // An upper limit to the scaled range.  The icon will be drawn no
    // larger than this.
    float maxSize;		// Pixels
};

// A class to render navaids.  As a subscriber, it keeps track of
// whether a zoom or move event has occurred, and uses that to
// repopulate a vector of navaids (_navaids) and update the scale
// (_metresPerPixel) and label point size (_labelPointSize).
//
// It has the concepts of "passes" and "layers".  Layers are fairly
// simple - a typical rendering of a navaid might consist of the
// navaid icons, labels for the icons, and maybe a radio "beam" drawn
// when a flight track is active.  Each of those is called a layer.
// When you create an instance of a NavaidRenderer, you tell it the
// number of layers, and it appropriately sizes the _layers vector,
// which contains one display list for each layer.  It is up to the
// subclasser to invalidate those display lists when they need to be
// re-rendered.  Typically this is done in the notification() method.
//
// A pass, on the other hand, is basically a kludge, created for ILS
// systems, which need to be drawn in two passes.  The first pass
// draws the markers and localizer beams, while the second pass draws
// any DMEs associated with the ILS.  If an instance is declared with
// multiple passes, the draw() method needs to be called that many
// times (ie, twice for a 2-pass instance, thrice for a 3-pass
// instance, ...) to render everything.  As a caller, if you lose
// track of which pass you're on, well, you're out of luck.
// Subclasses know which pass is current via the _currentPass
// variable.  This variable is updated automatically.
//
// To use the class, you need to create a subclass.  The subclass
// declaration requires 2 template arguments, both of which are
// classes.  The first, T, is just a pointer to the class of the
// navaid ("VOR *", "NDB *", ...).  The second is the name of the
// subclass.  For example, to declare a renderer for VORs called Foo,
// declare thusly:
//
// class Foo: public NavaidRenderer<VOR *, Foo> {
// ...
//
// The reason for passing in the subclass name is to make the
// _drawLayer() method work.  It takes two parameters: the layer to
// draw, and a method to call which will render one instance of the
// navaid for that layer.  C++ needs to know the class that owns the
// method in the declaration (at least I think it does - it's the only
// way I could make it work).
//
// Subclasses need to implement 2 virtual methods: (1) _getNavaids(),
// which gets the hits from the NavData class, then adds the
// appropriate navaids to the _navaids vector, and (2) _draw(), which
// checks the current pass and calls _drawLayer() for the layers that
// need to be rendered in that pass.  It is passed a 'labels' boolean,
// which is true if labels should be rendered.  Subclasses are free to
// choose which layers to draw - for example, if there's a flight
// track and the radio is tuned in to a navaid, they can draw a radio
// beam to indicate the navaid is active.
//
// Subclasses should also implement the notification() method, mostly
// to invalidate layers that need to be rerendered.  They must also
// call NavaidRenderer's notification() method, so that it can update
// _navaidsDirty, _metresPerPixel, ...
template <class T, class S>
class NavaidRenderer: public Subscriber {
  public:
    NavaidRenderer(int noOfPasses, int noOfLayers);
    virtual ~NavaidRenderer();

    void draw(NavData *nd, bool labels);

    // Subscriber interface.
    virtual void notification(Notification::type n);

  protected:
    virtual void _getNavaids(NavData *nd) = 0;
    virtual void _draw(bool labels) = 0;
    virtual void _drawLayer(DisplayList& dl, void (S::*fn)(T));
    // Returns true if, for the navaid and its scaling policy, the
    // navaid icon should be drawn.  As a side effect, it returns the
    // icon size in 'radius'.
    bool _iconVisible(Navaid *n, IconScalingPolicy& isp, float& radius);

    // True if we receive a zoom or move notification.  This will
    // result in a call to _getNavaids().
    bool _navaidsDirty;
    // Useful values updated when we're notified of a zoom event.
    double _metresPerPixel;
    float _labelPointSize;
    // Keep track of our passes.
    int _currentPass, _noOfPasses;
    // The actual navaids, and the rendering layers.
    std::vector<T> _navaids;
    std::vector<DisplayList> _layers;
};

class VORRenderer: public NavaidRenderer<VOR *, VORRenderer> {
  public:
    VORRenderer();

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    void _createVORRose();
    void _createVORSymbols();

    enum {VORLayer, RadioLayer, VORLabelLayer, _LayerCount} _layerNames;

    void _getNavaids(NavData *nd);
    void _draw(bool labels);
    void _drawVOR(VOR *vor);
    void _drawRadio(VOR *vor);
    void _drawVORLabel(VOR *vor);

    static const float _lineScale, _maxLineWidth;
    static const float _angularWidth;   // Width of 'radial' (degrees)

    // Scaling policies for the VOR icon (_isp) and its compass rose
    // (_rsp).
    IconScalingPolicy _isp, _rsp;
    DisplayList _VORRoseDL, _VORSymbolDL, _VORTACSymbolDL, _VORDMESymbolDL;
    // True if we're tuned in to a VOR (ie, our RADIO is ACTIVE.
    // Clever, eh?).
    bool _radioactive;
    // Our most recent flight track point (NULL if there's none).
    // This is updated whenever a flighttrack notification comes.
    FlightData *_p;
};

class NDBRenderer: public NavaidRenderer<NDB *, NDBRenderer> {
  public:
    NDBRenderer();

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    void _createNDBSymbols();

    enum {NDBLayer, RadioLayer, NDBLabelLayer, _LayerCount} _layerNames;

    void _getNavaids(NavData *nd);
    void _draw(bool labels);
    void _drawNDB(NDB *ndb);
    void _drawRadio(NDB *ndb);
    void _drawNDBLabel(NDB *ndb);

    static const float _dotScale;
    static const float _angularWidth; // Width of 'radial' (degrees)

    IconScalingPolicy _isp;
    DisplayList _NDBSymbolDL, _NDBDMESymbolDL;
    // True if we're tuned in to an NDB.
    bool _radioactive;
    // Our most recent flight track point (NULL if there's none).
    FlightData *_p;
};

class DMERenderer: public NavaidRenderer<DME *, DMERenderer> {
  public:
    DMERenderer();

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    void _createDMESymbols();

    enum {DMELayer, DMELabelLayer, _LayerCount} _layerNames;

    void _getNavaids(NavData *nd);
    void _draw(bool labels);
    void _drawDME(DME *dme);
    void _drawDMELabel(DME *dme);

    IconScalingPolicy _isp;
    DisplayList _DMESymbolDL, _TACANSymbolDL;
};

class ILSRenderer: public NavaidRenderer<ILS *, ILSRenderer> {
  public:
    ILSRenderer();

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    void _createILSSymbols();
    void _createILSSymbol(DisplayList& dl, const float *colour);
    void _createMarkerSymbols();
    void _createDMESymbol();

    enum {MarkerLayer, LOCLayer, LOCLabelLayer, 
	  DMELayer, DMELabelLayer, _LayerCount} _layerNames;

    // An encapsulation of parameters needed when drawing the various
    // bits and bobs of an ILS system.  These are set as a side effect
    // to the call to _ILSVisible.
    struct DrawingParams {
	LOC *loc;
	float length;
	bool live;
	float *colour;
	float pointSize;
    };

    void _getNavaids(NavData *nd);
    void _draw(bool labels);
    bool _ILSVisible(ILS *ils, DrawingParams& p);
    void _drawMarkers(ILS *ils);
    void _drawLOC(ILS *ils);
    void _drawLOCLabel(ILS *ils);
    void _drawDME(ILS *ils);
    void _drawDMELabel(ILS *ils);

    static const float _DMEScale;

    DisplayList _ILSSymbolDL, _LOCSymbolDL, _ILSMarkerDLs[Marker::_LAST],
	_DMESymbolDL;
    bool _radioactive;
    FlightData *_p;
};

//////////////////////////////////////////////////////////////////////
// NavaidsOverlay
//////////////////////////////////////////////////////////////////////

class NavaidsOverlay {
  public:
    NavaidsOverlay() {}

    // Draws the given overlay type, which must be VOR, NDB, DME, or
    // ILS.  If an overlay has multiple passes, each call to draw()
    // will render the next pass for that overlay.  Only ILS's have
    // this "feature" - they have 2 passes, so you need to call this
    // twice for ILS's on each rendering cycle.
    void draw(NavData *navData, Overlays::OverlayType t, bool labels);

  protected:
    VORRenderer _vr;
    NDBRenderer _nr;
    DMERenderer _dr;
    ILSRenderer _ir;
};

#endif
