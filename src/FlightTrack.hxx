/*-------------------------------------------------------------------------
  FlightTrack.hxx

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

#ifndef __FLIGHTTRACK_H__
#define __FLIGHTTRACK_H__

#include <list>

struct FlightData {
  float lat, lon, alt, hdg, spd;
};

class FlightTrack {
public:
  FlightTrack( unsigned int max_buffer = 2000 );
  ~FlightTrack();

  void clear();

  // The data point supplied is added to the flight track
  // NOTE: This pointer is considered FlightTrack's property
  // after this call, i.e. it's responsible for freeing the memory.
  void addPoint( FlightData *data );

  void firstPoint();
  FlightData *getNextPoint();

protected:
  unsigned int max_buffer;

  list<FlightData*> track;
  list<FlightData*>::iterator track_pos;
};


#endif

