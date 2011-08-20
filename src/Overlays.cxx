/*-------------------------------------------------------------------------
  Overlays.cxx

  Written by Brian Schack

  Copyright (C) 2009 - 2011 Brian Schack

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cassert>
#include <sstream>

#include "Globals.hxx"

#include "Overlays.hxx"

using namespace std;

multimap<string, NAVPOINT> navPoints;

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

Overlays::Overlays(const string& fgDir)
{
    // Load data.
    _airports = new AirportsOverlay(*this);
    _airports->load(fgDir);
    _navaids = new NavaidsOverlay(*this);
    _navaids->load(fgDir);
    _fixes = new FixesOverlay(*this);
    _fixes->load(fgDir);
    _airways = new AirwaysOverlay(*this);
    _airways->load(fgDir);
    _tracks = new FlightTracksOverlay(*this);
    _crosshairs = new CrosshairsOverlay(*this);
    _rangeRings = new RangeRingsOverlay(*this);
}

Overlays::~Overlays()
{
    delete _airports;
    delete _navaids;
    delete _airways;
    delete _fixes;
    delete _tracks;
    delete _crosshairs;
    delete _rangeRings;
}

// Draws all the overlays.
void Overlays::draw()
{
    // We assume that when called, the depth test is on, and lighting
    // is off.
    assert(glIsEnabled(GL_DEPTH_TEST) && !glIsEnabled(GL_LIGHTING));

    // Overlays must be written on top of whatever scenery is there,
    // so we ignore depth values.
    glPushAttrib(GL_DEPTH_BUFFER_BIT); {
	glDisable(GL_DEPTH_TEST);

	if (_overlays[AIRPORTS]) {
	    _airports->drawBackgrounds();
	}
	// We sandwich ILSs between runway backgrounds and the runways.
	if (_overlays[NAVAIDS] && _overlays[ILS]) {
	    _navaids->drawILSs();
	}
	if (_overlays[AIRPORTS]) {
	    _airports->drawForegrounds();
	    if (_overlays[LABELS]) {
		_airports->drawLabels();
	    }
	}
	if (_overlays[AIRWAYS]) {
	    _airways->draw(_overlays[HIGH], _overlays[LOW], _overlays[LABELS]);
	}
	if (_overlays[NAVAIDS] && _overlays[FIXES]) {
	    _fixes->draw();
	}
	if (_overlays[NAVAIDS] && _overlays[NDB]) {
	    _navaids->drawNDBs();
	}
	if (_overlays[NAVAIDS] && _overlays[VOR]) {
	    _navaids->drawVORs();
	}
	if (_overlays[NAVAIDS] && _overlays[DME]) {
	    _navaids->drawDMEs();
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
    glPopAttrib();
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
	_navaids->setDirty();
	_fixes->setDirty();
	_tracks->setDirty();
    }
}

bool Overlays::isVisible(OverlayType type)
{
    return _overlays[type];
}
