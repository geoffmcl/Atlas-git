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

#include <deque>
#include <plib/sg.h>
#include <simgear/compiler.h>
#include <simgear/io/sg_socket.hxx>
#include <simgear/io/sg_serial.hxx>
#include <simgear/misc/sg_path.hxx>

struct FlightData {
    time_t time;		// Time of record (in integral seconds)
    // EYE - I might want to change these (especially lat and lon) to
    // doubles.  Floats only have about 7 digits of precision.
    float lat, lon, alt, hdg, spd;
    float nav1_freq, nav1_rad;
    float nav2_freq, nav2_rad;
    float adf_freq;
    float est_t_offset;		// Estimated time offset from first
				// point in flight track.  This is a
				// derived value.
};

class FlightTrack {
public:
    // Flight tracks can be loaded from files, by listening on a
    // socket, or by reading a serial device (I think - I've never
    // tested the latter).
    FlightTrack(const char *filePath);
    FlightTrack(int port, unsigned int max_buffer = 2000);
    FlightTrack(char *device, int baud, unsigned int max_buffer = 2000);
    ~FlightTrack();

    int port();
    const char *device();
    int baud();

    void clear();
    bool empty();

    // Returns true if this is a socket- or serial-driven flight
    // track which is currently accepting input.
    bool live();
    // Closes the I/O channel to further input.
    void detach();

    // The "version" of the flight track begins at 0, and is
    // incremented each time the flight track is changed.  This
    // provides a simple way to check for changes.
    int version();

    // Call this periodically to check for input (from a socket or
    // serial device).
    bool checkForInput();

    // Sequential access to the flight track.
    void firstPoint();
    FlightData *getNextPoint();

    // Random access to the flight track.
    FlightData *dataAtPoint(int i);
    
    // Convenience access.  The first gets the last point, obviously.
    // The second retrieves the last point if we're live, otherwise it
    // returns the marked point.
    FlightData *getLastPoint();
    FlightData *getCurrentPoint();

    int size();

    // The mark specifies, for "dead" tracks (ie, file-based tracks),
    // the position of the aircraft along the track.  For live tracks,
    // mark is always -1, and the aircraft is always drawn at the end
    // of the track.
    void setMark(int i);
    int mark();

    bool hasFile();		// True if file path is not empty
    const char *fileName();	// File name
    const char *filePath();	// Full path, including file
    void setFilePath(char *path);
    void save();
    bool modified();

protected:
    unsigned int _max_buffer;

    std::deque<FlightData*> _track;
    std::deque<FlightData*>::iterator _track_pos;

    int _mark;
    // True if we are currently accepting input (from a socket or
    // serial port) for this track.
    bool _live;

     // Incremented each time the track changes.  Initially 0.
    int _version;
    // Initially 0, thereafter set to the current version whenever the
    // track is saved.
    int _versionAtLastSave;

    // For files read from or saved to a file.
    SGPath _file;		

    // For both socket- and serial-based tracks.
    SGIOChannel *_input_channel; 
    int _port;			// For socket-based tracks.
    string _device;		// For serial-based tracks.
    int _baud;

    void _adjustOffsetsAround(int i);

    bool _readFlightFile(const char *path);
    bool _parse_nmea(char *buf, FlightData *d);
    void _addPoint(FlightData *data, 
		   float tolerance = SG_DEGREES_TO_RADIANS / 3600.0f);

    char _calcChecksum(char *sentence);
    void _splitAngle(float degrees, char direction[2], 
		     int *d, float *m, char *c);
};


#endif

