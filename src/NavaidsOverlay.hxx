/*-------------------------------------------------------------------------
  NavaidsOverlay.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2018 Brian Schack

  Loads and draws navaids (VORs, NDBs, ILS systems, DMEs, and fixes).

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

#include "Notifications.hxx"
#include "OOGL.hxx"
#include "Overlays.hxx"
#include "NavData.hxx"		// Needed for Marker Type enumeration.

// Forward class declarations
class FlightData;

//////////////////////////////////////////////////////////////////////
// WaypointOverlay
//////////////////////////////////////////////////////////////////////

// Most navaid icons are scaled according their range and a few other
// factors.  This structure encapsulates those factors.  If we choose
// a different rendering policy (eg, fixed pixel size, fixed world
// size, ...), then this will have to be changed.  Note that the label
// point size is derived from the other values, and is set in the
// _iconRadius() method.
struct IconScalingPolicy {
    // How big to draw the icon as a proportion of the navaid's range.
    float rangeScaleFactor;
    // The smallest size at which to draw the icon (after scaling).
    // Below this, it is not drawn at all.
    float minSize;		// Pixels
    // An upper limit to the scaled range.  The icon will be drawn no
    // larger than this.
    float maxSize;		// Pixels
    // Font size, in metres.  This scales in proportion to the icon
    // size, and is derived from the other values.
    float labelSize;
};

// A class to render waypoints (or, to be technical, anything derived
// from a Waypoint).  As a subscriber, it keeps track of whether a
// zoom or move event has occurred, and uses that to repopulate a
// vector of waypoints (_waypoints) and update the scale
// (_metresPerPixel) and label size (_labelSize).  It also follows
// FontSize and OverlayToggled notifications to update _labelSize,
// _visible, and _labels.
//
// It has the concepts of "passes" and "layers".  Layers are fairly
// simple - a typical rendering of a waypoint might consist of the
// waypoint icons, labels for the icons, and maybe a radio "beam"
// drawn when a flight track is active.  Each of those is called a
// layer.  When you create an instance of a WaypointOverlay, you tell
// it the number of layers, and it appropriately sizes the _layers
// vector, which contains one display list for each layer.  It is up
// to the subclasser to invalidate those display lists when they need
// to be re-rendered.  Typically this is done in the notification()
// method.
//
// A pass, on the other hand, is basically a kludge, created for ILS
// systems, which need to be drawn in two passes.  The first pass
// draws the markers and localizer beams, then some airport stuff is
// drawn, then the second pass draws any DMEs associated with the ILS.
// If an instance is declared with multiple passes, the draw() method
// needs to be called that many times (ie, twice for a 2-pass
// instance, thrice for a 3-pass instance, ...) to render everything.
// As a caller, if you lose track of which pass you're on, well,
// you're out of luck.  Subclasses know which pass is current via the
// _currentPass variable.  This variable is updated automatically.
//
// To use the class, you need to create a subclass.  Subclasses need
// to implement 2 virtual methods: 
//
// (1) _getWaypoints() - When called, the _waypoints vector is empty.
//     The subclass must get the hits from the NavData class, then add
//     the appropriate waypoints to the vector.
//
// (2) _draw() - This should check the current pass (_currentPass) and
//     call _drawLayer() for the layers that need to be rendered in
//     that pass.  It's up to subclasses to choose which layers to
//     draw - for example, if there's a flight track and the radio is
//     tuned in to a navaid, they can draw a radio beam to indicate
//     the navaid is active.
//
// Subclasses should also implement the notification() method, mostly
// to invalidate layers that need to be rerendered.  Just remember to
// call WaypointOverlay's notification() method first, so that it can
// update _waypointsDirty, _metresPerPixel, ...
//
// Subclasses need to pass to the WaypointOverlay constructor: a
// reference to the Overlays object, their type (in OverlayType
// terms), and the number of passes and layers they require.
class WaypointOverlay: public Subscriber {
  public:
    WaypointOverlay(Overlays& overlays, Overlays::OverlayType t, 
		    int noOfPasses, int noOfLayers);
    virtual ~WaypointOverlay() {}

    void draw(NavData *nd);

    // Subscriber interface.
    virtual void notification(Notification::type n);

  protected:
    // Methods that need to be implemented by subclasses.
    virtual void _getWaypoints(NavData *nd) = 0;
    virtual void _draw() = 0;

    // Called by subclasses when they want to draw a specific layer.
    // It checks if the layer is invalid or not.  If it is, it begins
    // compiling a new display list, and calls the given function to
    // render the layer.  At the end, it calls the display list (ie,
    // draws it).
    template <class T> void _drawLayer(DisplayList& dl, void (T::*fn)());
    // Returns the icon size, scaled according to the scaling policy.
    // As a side-effect, it sets the label point size in isp as well.
    float _iconRadius(Navaid *n, IconScalingPolicy& isp);
    // A shortcut to the font bias maintained by AtlasController.
    int _fontBias();

    // True if we receive a zoom or move notification.  This will
    // result in a call to _getWaypoints().
    bool _waypointsDirty;
    // Useful values updated when we're notified of a zoom event.
    double _metresPerPixel;
    float _labelSize;		// In metres
    Overlays& _overlays;
    // Our type - this is used when we get a OverlayToggled event to
    // decide if we are visible or not.
    Overlays::OverlayType _t;
    // True if the overlay is visible (_visible), and true if we need
    // to draw labels (_labels).  The base class sets both in response
    // to OverlayToggled events, and uses _visible to decide whether
    // to call _draw().  Subclasses should check _labels in their
    // _draw() methods.
    bool _visible, _labels;
    // Keep track of our passes.
    int _currentPass, _noOfPasses;
    // The actual waypoints, and the rendering layers.
    std::vector<Waypoint *> _waypoints;
    std::vector<DisplayList> _layers;
};

class VOROverlay: public WaypointOverlay {
  public:
    VOROverlay(Overlays& overlays);

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    void _createVORRose();
    void _createVORSymbols();

    enum {VORLayer, RadioLayer, LabelLayer, _LayerCount} _layerNames;

    void _getWaypoints(NavData *nd);
    void _draw();

    void _drawVORs();
    void _drawRadios();
    void _drawLabels();

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

class NDBOverlay: public WaypointOverlay {
  public:
    NDBOverlay(Overlays& overlays);

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    void _createNDBSymbols();

    enum {NDBLayer, RadioLayer, LabelLayer, _LayerCount} _layerNames;

    void _getWaypoints(NavData *nd);
    void _draw();

    void _drawNDBs();
    void _drawRadios();
    void _drawLabels();

    static const float _dotScale;
    static const float _angularWidth; // Width of 'radial' (degrees)

    IconScalingPolicy _isp;
    DisplayList _NDBSymbolDL, _NDBDMESymbolDL;
    // True if we're tuned in to an NDB.
    bool _radioactive;
    // Our most recent flight track point (NULL if there's none).
    FlightData *_p;
};

class DMEOverlay: public WaypointOverlay {
  public:
    DMEOverlay(Overlays& overlays);

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    void _createDMESymbols();

    enum {DMELayer, LabelLayer, _LayerCount} _layerNames;

    void _getWaypoints(NavData *nd);
    void _draw();

    void _drawDMEs();
    void _drawLabels();

    IconScalingPolicy _isp;
    DisplayList _DMESymbolDL, _TACANSymbolDL;
};

class FixOverlay: public WaypointOverlay {
  public:
    FixOverlay(Overlays& overlays);

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    enum {EnrouteFixLayer, TerminalFixLayer, 
	  EnrouteLabelLayer, TerminalLabelLayer, _LayerCount} _layerNames;

    void _getWaypoints(NavData *nd);
    void _draw();

    void _drawEnrouteFixes();
    void _drawTerminalFixes();
    void _drawEnrouteLabels();
    void _drawTerminalLabels();

    void _drawFixes(Overlays::OverlayType t, float fullFix);
    void _drawLabels(Overlays::OverlayType t, float fullLabel);
};

class ILSOverlay: public WaypointOverlay {
  public:
    ILSOverlay(Overlays& overlays);

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
	float length;		// metres
	bool live;
	sgVec4 colour;
	float labelSize;	// scaled size of font, in metres
    };

    void _getWaypoints(NavData *nd);
    void _draw();

    bool _ILSVisible(ILS *ils, DrawingParams& p);
    void _drawMarkers();
    void _drawLOCs();
    void _drawLOCLabels();
    void _drawDMEs();
    void _drawDMELabels();

    static const float _DMEScale;

    DisplayList _ILSSymbolDL, _LOCSymbolDL, _ILSMarkerDLs[Marker::_LAST],
	_DMESymbolDL;
    bool _radioactive;
    FlightData *_p;
};

#endif
