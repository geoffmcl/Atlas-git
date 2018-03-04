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

//////////////////////////////////////////////////////////////////////
// DisplayList
//////////////////////////////////////////////////////////////////////

// True if begin() has been called without a corresponding end().
bool DisplayList::_compiling = false;

DisplayList::DisplayList(): _dl(0), _valid(false)
{
}

DisplayList::~DisplayList()
{
    glDeleteLists(_dl, 1);
}

void DisplayList::begin()
{
    assert(!_compiling);

    // Generate the display list if necessary.
    if (_dl == 0) {
	_dl = glGenLists(1);
	assert(_dl);
    }

    // Start compiling the display list.
    _valid = false;
    glNewList(_dl, GL_COMPILE);
    _compiling = true;
}

void DisplayList::end()
{
    assert(_compiling);
    glEndList();
    _valid = true;
    _compiling = false;
}

void DisplayList::call()
{
    // Although it's not illegal to call an undefined display list,
    // it's probably a logic error.
    assert(_dl);
    assert(_valid);
    glCallList(_dl);
}

#include "AtlasWindow.hxx"
Overlays::Overlays(AtlasWindow *aw): _aw(aw)
{
    // Load data.
    _airports = new AirportsOverlay(*this);
    _VORs = new VOROverlay();
    _NDBs = new NDBOverlay();
    _DMEs = new DMEOverlay();
    _Fixes = new FixOverlay();
    _ILSs = new ILSOverlay();
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

    if (_overlays[AIRPORTS]) {
	_airports->drawBackgrounds(navData);
    }
    // We sandwich ILSs between runway backgrounds and the runways.
    if (_overlays[NAVAIDS] && _overlays[ILS]) {
	_ILSs->draw(navData, _overlays[LABELS]);
    }
    if (_overlays[AIRPORTS]) {
	_airports->drawForegrounds(navData);
	if (_overlays[LABELS]) {
	    _airports->drawLabels(navData);
	}
    }
    // EYE - we need to be more consistent about who checks overlay
    // visibility.  I wonder if we should just move it all into the
    // various overlay subclasses?  It would certainly make for neater
    // code here.
    if (_overlays[NAVAIDS] && _overlays[ILS]) {
	_ILSs->draw(navData, _overlays[LABELS]);
    }
    if (_overlays[AWYS]) {
	_airways->draw(_overlays[AWYS_HIGH], _overlays[AWYS_LOW], navData);
    }
    if (_overlays[NAVAIDS]) {
	if (_overlays[VOR]) {
	    _VORs->draw(navData, _overlays[LABELS]);
	}
	if (_overlays[NDB]) {
	    _NDBs->draw(navData, _overlays[LABELS]);
	}
	if (_overlays[DME]) {
	    _DMEs->draw(navData, _overlays[LABELS]);
	}
	if (_overlays[FIXES]) {
	    _Fixes->draw(navData, _overlays[LABELS]);
	}
    }
    if (_overlays[CROSSHAIRS]) {
    	_crosshairs->draw();
    }
    if (_overlays[RANGE_RINGS]) {
	_rangeRings->draw();
    }
    if (_overlays[TRACKS]) {
	_tracks->draw();
    }
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
