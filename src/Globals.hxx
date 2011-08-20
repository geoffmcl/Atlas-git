/*-------------------------------------------------------------------------
  Globals.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2011 Brian Schack

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

#include <vector>

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
    // number will be FlightTrack::npos.
    FlightData *currentPoint();
    const std::vector<FlightTrack *>& tracks() { return _tracks; }
    size_t currentTrackNo() { return _currentTrackNo; }
    FlightTrack *track() { return _track; }
    FlightTrack *track(size_t i);
    size_t addTrack(FlightTrack *t, bool select = true);
    FlightTrack *removeTrack() { return removeTrack(_currentTrackNo); }
    FlightTrack *removeTrack(size_t i);
    FlightTrack *setCurrent(size_t i);
    FlightTrack *exists(int port, bool select = true);
    FlightTrack *exists(const char *device, int baud, bool select = true);
    FlightTrack *exists(const char *path, bool select = true);

    // Our view on the world.
    sgdMat4 modelViewMatrix;
    sgdFrustum frustum;
    double metresPerPixel;

    // Searcher object.
    Searcher searcher;

    // Scenery lighting.
    bool discreteContours, contourLines, lightingOn;

    // Current palette.
    Palette *palette() { return _palette; }
    void setPalette(Palette *p);

    // The fonts we use for the user interface, scenery and overlays.
    atlasFntRenderer fontRenderer;
    atlasFntTexFont *regularFont, *boldFont;
    puFont uiFont;
    // Sets the current font (which is accessed through fontRenderer)
    // to regular or bold.
    void regular() { fontRenderer.setFont(regularFont); }
    void bold() { fontRenderer.setFont(boldFont); }

    // Whether we're displaying true or magnetic headings.
    bool magnetic;

  protected:
    std::vector<FlightTrack *> _tracks;
    FlightTrack *_track;
    size_t _currentTrackNo;

    Palette *_palette;
};

extern Globals globals;

#endif
