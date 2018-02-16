/*-------------------------------------------------------------------------
  Overlays.cxx

  Written by Brian Schack

  Copyright (C) 2009 - 2017 Brian Schack

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

// Colours from VFR_Chart_Symbols.pdf:
//
// VOR - <0.000, 0.420, 0.624> (same as controlled airport)
// NDB - <0.525, 0.294, 0.498>
//
// city - <1.000, 0.973, 0.459>
// lake - <0.443, 0.745, 0.827>
// open water - <0.859, 0.929, 0.945>

// EYE - the choice of colours here was arbitrary.  Are there
// "official" colours?  Use an official symbol instead?  A: See
// VFR_Chart_Symbols.pdf, page 28.  It looks like they're blue, and
// drawn as a symbol.  There's a distinction (in symbol size) made
// between helipads at airports and stand-alone heliports.
// const float heli_colour1[4] = {0.271, 0.439, 0.420, 0.7}; 
// const float heli_colour2[4] = {0.863, 0.824, 0.824, 0.7};

#include "AtlasWindow.hxx"
Overlays::Overlays(AtlasWindow *aw): _aw(aw)
{
    // Load data.
    _airports = new AirportsOverlay(*this);
    _navaids = new NavaidsOverlay();
    _airways = new AirwaysOverlay(*this);
    _tracks = new FlightTracksOverlay(*this);
    _crosshairs = new CrosshairsOverlay(*this);
    _rangeRings = new RangeRingsOverlay(*this);
}

Overlays::~Overlays()
{
    delete _airports;
    delete _navaids;
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
	_navaids->draw(navData, ILS, _overlays[LABELS]);
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
	_navaids->draw(navData, ILS, _overlays[LABELS]);
    }
    if (_overlays[AIRWAYS]) {
	_airways->draw(_overlays[HIGH], _overlays[LOW], navData);
    }
    if (_overlays[NAVAIDS]) {
	if (_overlays[VOR]) {
	    _navaids->draw(navData, VOR, _overlays[LABELS]);
	}
	if (_overlays[NDB]) {
	    _navaids->draw(navData, NDB, _overlays[LABELS]);
	}
	if (_overlays[DME]) {
	    _navaids->draw(navData, DME, _overlays[LABELS]);
	}
	if (_overlays[FIXES]) {
	    _navaids->draw(navData, FIXES, _overlays[LABELS]);
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

void Overlays::setVisibility(OverlayType type, bool value)
{
    _overlays[type] = value;

    // A special case - toggling an overlay doesn't really dirty it -
    // we just turn it on and off.  However, toggling the labels
    // usually does, because the labels are usually drawn with the
    // overlay contents.
    if (type == LABELS) {
	_airports->setDirty();
	_tracks->setDirty();
    }
}

bool Overlays::isVisible(OverlayType type)
{
    return _overlays[type];
}
