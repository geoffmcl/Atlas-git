/*-------------------------------------------------------------------------
  Globals.hxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

  This contains some variables that are needed in several different
  parts of Atlas.  Although we could pass them around via function
  parameters, this is clearer (I think).  It also adds a few access
  methods for convenience (and to prevent data corruption).

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

#ifndef _GLOBALS_H
#define _GLOBALS_H

// Global variables, all bundled together into a nice simple class.

#include <plib/sg.h>
#include <plib/fnt.h>
#include <plib/pu.h>

#include "Overlays.hxx"
#include "FlightTrack.hxx"
#include "Searcher.hxx"
#include "Palette.hxx"
#include "misc.hxx"

class Globals {
 public:
    Globals();
    ~Globals();

    // Overlays object.
    Overlays *overlays;

    // Flight tracks.  There will always be a valid current track and
    // a valid current track number, except when the tracks vector is
    // empty, in which case the track will be NULL and the track
    // number will be undefined.
    FlightData *currentPoint();
    const vector<FlightTrack *>& tracks() { return _tracks; }
    unsigned int currentTrackNo() { return _currentTrackNo; }
    FlightTrack *track() { return _track; }
    FlightTrack *track(unsigned int i);
    unsigned int addTrack(FlightTrack *t, bool select = true);
    FlightTrack *removeTrack() { return removeTrack(_currentTrackNo); }
    FlightTrack *removeTrack(unsigned int i);
    FlightTrack *setCurrent(unsigned int i);
    unsigned int exists(int port);
    unsigned int exists(const char *device, int baud);
    unsigned int exists(const char *path);

    // Our view on the world.
    sgdMat4 modelViewMatrix;
    sgdFrustum frustum;
    double metresPerPixel;

    // Searcher object.
    Searcher searcher;

    // Scenery lighting.
    bool discreteContours;
    bool lightingOn;
    bool smoothShading;
    sgVec4 lightPosition;

    // Current palette.
    Palette *palette;

    // The fonts we use for the user interface, scenery and overlays.
    fntRenderer fontRenderer;
    atlasFntTexFont *regularFont, *boldFont;
    puFont uiFont;
    // Sets the current font (which is accessed through fontRenderer)
    // to regular or bold.
    void regular() { fontRenderer.setFont(regularFont); }
    void bold() { fontRenderer.setFont(boldFont); }

    // Whether we're displaying true or magnetic headings.
    bool magnetic;

  protected:
    vector<FlightTrack *> _tracks;
    FlightTrack *_track;
    unsigned int _currentTrackNo;
};

extern Globals globals;

#endif
