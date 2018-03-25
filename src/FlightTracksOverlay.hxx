/*-------------------------------------------------------------------------
  FlightTracksOverlay.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2018 Brian Schack

  The flight tracks overlay manages the display of flight tracks.

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

#ifndef _FLIGHTTRACKS_OVERLAY_H
#define _FLIGHTTRACKS_OVERLAY_H

// #include <map>

#include "Notifications.hxx"	// Subscriber
#include "OOGL.hxx"		// DisplayList
#include "Scenery.hxx"		// Texture

// struct TRACK_INFO {
//     sgVec4 trackColour;
//     sgVec4 planeColour;
//     GLuint DL;
// };

// Forward class declarations
class Overlays;
class FlightData;

class FlightTracksOverlay: public Subscriber {
  public:
    FlightTracksOverlay(Overlays& overlays);
    ~FlightTracksOverlay();

    // // Adds the given track, which it will display with trackColour,
    // // and the plane with planeColour.
    // void addTrack(FlightTrack *t, sgVec4 trackColour, sgVec4 planeColour);
    // // Removes the given track.  If t is NULL, it removes all tracks.
    // void removeTrack(FlightTrack *t = NULL);

    void draw();

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    Overlays& _overlays;
    bool _visible;

    // std::map<FlightTrack*, TRACK_INFO> _tracks;

    DisplayList _dl;

    // Aircraft icon.
    void _drawAirplane(FlightData *d, const sgVec4 colour);
    bool _haveImage;
    Texture _airplaneTexture;
};

#endif
