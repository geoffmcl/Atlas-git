/*-------------------------------------------------------------------------
  FlightTrack.cxx

  Written by Per Liedman, started July 2000.

  Copyright (C) 2000 Per Liedman, liedman@home.se

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  ---------------------------------------------------------------------------*/

#include "FlightTrack.hxx"
#include <math.h>

FlightTrack::FlightTrack( unsigned int max_buffer = 2000 ) : 
  max_buffer(max_buffer) {
  // Nothing here yet.
}

FlightTrack::~FlightTrack() {
  for (list<FlightData*>::iterator i = track.begin(); i != track.end(); i++) {
    delete *i;
  }
}

void FlightTrack::clear() {
  while (!track.empty()) {
    delete *(track.begin());
    track.pop_front();
  }
}

void FlightTrack::addPoint( FlightData *data ) {
  // TOLERANCE is set to 1 arc second
  static const float TOLERANCE = SG_DEGREES_TO_RADIANS / 3600.0f;

  float lastlat, lastlon;

  if (!track.empty()) {
    FlightData *last;
    last = track.back();
    lastlat = last->lat;
    lastlon = last->lon;
  } else {
    lastlat = -99.0f;
    lastlon = -99.0f;
  }

  if (fabs(lastlat - data->lat) > TOLERANCE || 
      fabs(lastlon - data->lon) > TOLERANCE) {
    if (track.size() > max_buffer) {
      delete track.front();
      track.pop_front();
    }

    track.push_back(data);
  }
}

void FlightTrack::firstPoint() {
  track_pos = track.begin();
}

FlightData *FlightTrack::getNextPoint() {
  if (track_pos != track.end()) {
    return *(track_pos++);
  } else {
    return NULL;
  }
}
