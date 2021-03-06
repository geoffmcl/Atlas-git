/*-------------------------------------------------------------------------
  Overlays.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2018 Brian Schack

  The overlays object manages a bunch of other overlays.  It creates
  them, and knows which ones should be displayed.

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

#ifndef _OVERLAYS_H
#define _OVERLAYS_H

#include <bitset>

// Forward class declarations.

// EYE - should we make an Overlay base class that these derive from?
class AirportsOverlay;
class AirwaysOverlay;
class CrosshairsOverlay;
class FlightTracksOverlay;
class VOROverlay;
class NDBOverlay;
class DMEOverlay;
class FixOverlay;
class ILSOverlay;
class RangeRingsOverlay;

class AtlasController;
class atlasFntTexFont;
class AtlasWindow;
class NavData;

class Overlays {
  public:
    Overlays(AtlasWindow *aw);
    ~Overlays();

    // These are shortcuts to some useful classes that our various
    // overlays need.
    atlasFntTexFont *regularFont();
    atlasFntTexFont *boldFont();
    AtlasWindow *aw();
    AtlasController *ac();

    void draw(NavData *navData);

    enum OverlayType {NAVAIDS = 0, VOR, NDB, ILS, DME, FIXES, FIXES_ENROUTE, 
		      FIXES_TERMINAL, AIRPORTS, AWYS, AWYS_HIGH, AWYS_LOW, 
		      LABELS, TRACKS, CROSSHAIRS, RANGE_RINGS, _LAST};

    // Toggle the overlay on or off.
    void toggle(OverlayType type, bool value);
    // Toggle the state of the overlay.
    void toggle(OverlayType type);
    // Returns true if the overlay is visible.
    bool isVisible(OverlayType type);

    AirportsOverlay* airportsOverlay() { return _airports; }
    VOROverlay* vorOverlay() { return _VORs; }
    NDBOverlay* ndbOverlay() { return _NDBs; }
    DMEOverlay* dmeOverlay() { return _DMEs; }
    FixOverlay* fixOverlay() { return _Fixes; }
    ILSOverlay* ilsOverlay() { return _ILSs; }
    AirwaysOverlay* airwaysOverlay() { return _airways; }
    FlightTracksOverlay* flightTracksOverlay() { return _tracks; }
    CrosshairsOverlay* crosshairsOverlay() { return _crosshairs; }
    RangeRingsOverlay* rangeRingsOverlay() { return _rangeRings; }

  protected:
    AtlasWindow *_aw;
    std::bitset<_LAST> _overlays;

    AirportsOverlay *_airports;
    VOROverlay *_VORs;
    NDBOverlay *_NDBs;
    DMEOverlay *_DMEs;
    FixOverlay *_Fixes;
    ILSOverlay *_ILSs;
    AirwaysOverlay *_airways;
    FlightTracksOverlay *_tracks;
    CrosshairsOverlay *_crosshairs;
    RangeRingsOverlay *_rangeRings;
};

#endif
