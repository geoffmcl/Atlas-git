/*-------------------------------------------------------------------------
  FlightTrack.hxx

  Written by Per Liedman, started July 2000.

  Copyright (C) 2000 Per Liedman, liedman@home.se
  Copyright (C) 2009 - 2017 Brian Schack

  A flight track contains the data for a FlightGear session.  It
  includes things like the aircraft's position, speed, altitude, etc.
  Flight tracks can be loaded and saved to files, or imported from
  running sessions of FlightGear over a network.

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

#ifndef _FLIGHTTRACK_H_
#define _FLIGHTTRACK_H_

// C++ system include files
#include <string>
#include <set>
#include <deque>

// Other libraries' include files
#include <simgear/misc/sg_path.hxx> // SGPath

// Our project's include files
#include "misc.hxx"		// AtlasString, ...

// Forward class declarations
class NavData;
class Navaid;
class SGIOChannel;

// EYE - it's wasteful to have the radio stuff in a FlightData struct,
// since they change so infrequently.
class FlightData {
  public:
    FlightData(NavData *navData);
    ~FlightData();

    time_t time;		// Time of record (in integral seconds)
    double lat, lon;		// Latitude, longitude (in degrees)
    // Heading and speed represent different things, depending on
    // whether they come from the atlas or nmea protocols.
    // atlas: hdg = true heading (degrees), spd = KEAS
    // nmea: hdg = true track (degrees), spd = GS
    float alt, hdg, spd;	// alt = altitude (in feet)
    float nav1_rad, nav2_rad;	// VOR radial (in degrees)
    // Frequencies are stored as integer Hz.  This means, for example,
    // that a VOR frequency of 110.90 is stored as the integer
    // 110,900,000.
    unsigned int nav1_freq, nav2_freq, adf_freq;

    // Derived values (ie, not passed explicitly from FlightGear)
    float est_t_offset;		// Estimated time offset from first
				// point in flight track (in seconds)
    sgdVec3 cart;		// Cartesian coordinates of position.
    float dist;			// Cumulative distance from start of
				// flight (in metres)

    // List of tuned-in navaids at this point in the flight.
    const std::set<Navaid *>& navaids();

  protected:
    // EYE - all of this stuff really belongs in the FlightTrack.
    NavData *_navData;
    bool _navaidsLoaded;
    std::set<Navaid *> _navaids;	// In-range tuned navaids.
};

class FlightTrack {
  public:
    static const size_t npos;	// Represents an undefined mark.

    // Flight tracks can be loaded from files, by listening on a
    // socket, or by reading a serial device (I think - I've never
    // tested the latter).
    FlightTrack(NavData *navData, const char *filePath);
    FlightTrack(NavData *navData, int port, unsigned int max_buffer = 2000);
    FlightTrack(NavData *navData, const char *device, int baud, 
		unsigned int max_buffer = 2000);
    ~FlightTrack();

    bool isAtlasProtocol();	// Returns true if this track has
				// atlas-protocol data, false if
				// nmea-protocol.

    bool isNetwork();
    bool isSerial();

    int port();
    const char *device();
    int baud();
    unsigned int maxBufferSize();
    void setMaxBufferSize(unsigned int size);

    void clear();
    bool empty();

    // Returns true if this is a socket- or serial-driven flight
    // track which is currently accepting input.
    bool live() { return _live; }
    // Closes the I/O channel to further input.
    void detach();

    // The "version" of the flight track begins at 0, and is
    // incremented each time the flight track is changed.  This
    // provides a simple way to check for changes.
    int version();

    // Call this periodically to check for input (from a socket or
    // serial device).
    bool checkForInput();

    // Random access to the flight track.
    FlightData *at(size_t i);
    
    // Convenience access.  The first gets the last point, obviously.
    // The second retrieves the last point if we're live, otherwise it
    // returns the marked point.
    FlightData *last();
    FlightData *current() { return at(mark()); }

    size_t size() { return _track.size(); }

    // The mark specifies the position of the aircraft along the
    // track, as selected by the user.
    size_t mark() { return _mark; }
    void setMark(size_t i);

    bool hasFile();		// True if file path is not empty
    const char *fileName();	// File name
    const char *filePath();	// Full path, including file
    const char *niceName();	// Nicely formatted name
    void setFilePath(char *path);
    void save();
    bool modified();

  protected:
    // Flight data points need this to look up in-range navaids.
    NavData *_navData;

    size_t _max_buffer;	// Maximum size of a buffer.
    std::deque<FlightData*> _track; // Our data.
    size_t _mark;

    // True if we are currently accepting input (from a socket or
    // serial port) for this track.
    bool _live;

     // Incremented each time the track changes.  Initially 0.
    int _version;
    // Initially 0, thereafter set to the current version whenever the
    // track is saved.
    int _versionAtLastSave;

    // True if we have an atlas-protocol flight track, false if nmea.
    // Undefined until at least one record is read.
    bool _isAtlasProtocol;

    // For files read from or saved to a file.
    SGPath _file;		

    // For both socket- and serial-based tracks.
    SGIOChannel *_input_channel; 
    int _port;			// For socket-based tracks.
    std::string _device;	// For serial-based tracks.
    int _baud;

    bool _isNetwork, _isSerial;

    AtlasString _name;

    void _adjustOffsetsAround(size_t i);
    void _calcDistancesFrom(size_t i);

    bool _readFlightFile(const char *path);
    bool _parse_message(char *buf, FlightData *d);
    bool _addPoint(FlightData *data, float tolerance = 1.0 / 60.0 / 60.0);

    char _calcChecksum(const char *sentence);
    void _splitAngle(float degrees, const char direction[2], 
		     int *d, float *m, char *c);
};


#endif
