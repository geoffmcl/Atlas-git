/*-------------------------------------------------------------------------
  Overlays.cxx

  Written by Brian Schack

  Copyright (C) 2009 - 2018 Brian Schack

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
#ifdef _MSC_VER //this needs to be the first!
#include "config.h"
#endif // _MSC_VER

// Our include file
#include "Overlays.hxx"

// Our project's include files
#include "AirportsOverlay.hxx"
#include "AirwaysOverlay.hxx"
#include "CrosshairsOverlay.hxx"
#include "FlightTracksOverlay.hxx"
#include "NavaidsOverlay.hxx"
#include "RangeRingsOverlay.hxx"

using namespace std;

#include "AtlasWindow.hxx"
Overlays::Overlays(AtlasWindow *aw): _aw(aw)
{
    // Load data.
    _airports = new AirportsOverlay(*this);
    _VORs = new VOROverlay(*this);
    _NDBs = new NDBOverlay(*this);
    _DMEs = new DMEOverlay(*this);
    _Fixes = new FixOverlay(*this);
    _ILSs = new ILSOverlay(*this);
    _airways = new AirwaysOverlay(*this);
    _tracks = new FlightTracksOverlay(*this);
    _crosshairs = new CrosshairsOverlay(*this);
    _rangeRings = new RangeRingsOverlay(*this);
}

Overlays::~Overlays()
{
    delete _airports;
    delete _VORs;
    delete _NDBs;
    delete _DMEs;
    delete _Fixes;
    delete _ILSs;
    delete _airways;
    delete _tracks;
    delete _crosshairs;
    delete _rangeRings;
}

atlasFntTexFont *Overlays::regularFont() 
{ 
    return _aw->regularFont(); 
}

atlasFntTexFont *Overlays::boldFont() 
{ 
    return _aw->boldFont(); 
}

AtlasWindow *Overlays::aw() 
{ 
    return _aw; 
}

AtlasController *Overlays::ac()
{
    return _aw->ac();
}

// Draws all the overlays.
void Overlays::draw(NavData *navData)
{
    // We assume that when called, the depth test and lighting are
    // off.
    assert(!glIsEnabled(GL_DEPTH_TEST) && !glIsEnabled(GL_LIGHTING));

    // We interleave ILSs and airports.  In particular, we draw the
    // runway backgrounds, then the ILS "beams", then the runway
    // foregrounds, then the other bits of the ILS systems (markers,
    // DMEs, ...).  Note that since we use the WaypointOverlay class
    // to implement the ILS overlay, we have the "feature" that
    // successive calls to draw() render successive passes (and we
    // know that ILSs have 2 passes).
    _airports->drawBackgrounds(navData);
    _ILSs->draw(navData);
    _airports->drawForegrounds(navData);
    _ILSs->draw(navData);

    _airways->draw(navData);

    _VORs->draw(navData);
    _NDBs->draw(navData);
    _DMEs->draw(navData);
    _Fixes->draw(navData);

    _crosshairs->draw();
    _rangeRings->draw();
    _tracks->draw();
}

void Overlays::toggle(OverlayType type, bool value)
{
    // Don't do anything if the overlay is already in the correct
    // state.
    if (_overlays[type] == value) {
	return;
    }

    // Change the state, and let everyone know it.
    _overlays[type] = value;
    Notification::notify(Notification::OverlayToggled);
}

void Overlays::toggle(OverlayType type)
{
    toggle(type, !isVisible(type));
}

bool Overlays::isVisible(OverlayType type)
{
    return _overlays[type];
}
