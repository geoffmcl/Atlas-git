/*-------------------------------------------------------------------------
  Preferences.hxx

  Written by Brian Schack, started August 2007.

  Copyright (C) 2007 Brian Schack

  Handles command-line options, as well as the Atlas preferences file.

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

#ifndef _PREFERENCES_H_
#define _PREFERENCES_H_

#include <vector>
#include <simgear/misc/sg_path.hxx>
#include <plib/sg.h>

// This class (of which there should only be one instantiated), keeps
// Atlas' preferences.  Preferences come from 3 places:
// 
// (1) Defaults (set in the constructor)
//
// (2) The preferences file (~/.atlasrc)
//
// (3) Command-line options

// Options given on the command line should override all others, while
// options given in the preferences file should override defaults.

// To add a new option, several changes must be made:
//
// (1) Add a public member to the class (of the appropriate type)
//
// (2) Add a new member to the enumeration in Preferences.cxx (but
//     always keep LAST_OPTION last)
//
// (3) Add a new entry to long_options (this is used by getopt_long())
//
// (4) Modify print_short_help() and print_help_for()
//
// (5) Add code to process the option in Preferences::_loadPreferences
//
// (6) Give it a default value in Preferences::Preferences
//
// (7) Add an entry for it in savePreferences (this is not very
//     important, used for debugging at the moment)
//
// Easy!

// Note: We use getopt to make our option-parsing life easier.  A
// nicer, but non-standard, solution would be argtable.  It can be
// found at:
//
//   http://argtable.sourceforge.net/

struct SerialConnection {
    char *device;
    int baud;
};

class Preferences {
public:
    Preferences();

    // Sets the preferences, first from defaults, then the preferences
    // file, and finally from the given command line arguments.
    // Returns true if there are no problems, false otherwise (it will
    // print out error messages to stderr if there are problems).
    bool loadPreferences(int argc, char *argv[]);

    // Right now this just prints out our preferences.  In the future,
    // it will do as it says: save them to the preferences file.
    void savePreferences();

    //////////////////////////////////////////////////////////////////////
    // Our preferences.
    //////////////////////////////////////////////////////////////////////

    // Startup location
    float latitude;
    float longitude;
    float zoom;
    char *icao;

    // Paths
    SGPath path;		// Path to maps
    SGPath fg_root;		// Root of FlightGear stuff
    SGPath palette;		// Path to palettes
    SGPath scenery_root;	// Path to FlightGear scenery

    // Visual stuff - fonts, window size, ...
    bool textureFonts;
    int width, height;
    bool softcursor;

    // Flight paths
    std::vector<SGPath> flightFiles;
    bool autocenter_mode;
    float lineWidth;
    SGPath airplaneImage;
    float airplaneImageSize;

    // FlightGear connection(s)
    float update;
    int max_track;
    std::vector<int> networkConnections;
    std::vector<SerialConnection> serialConnections;

    // Lighting
    bool discreteContours, contourLines, lightingOn, smoothShading;
    float azimuth, elevation;
    sgVec4 lightPosition;

    // Factory defaults
    static const float defaultLatitude, defaultLongitude, defaultZoom;
    static const int defaultWidth = 800, defaultHeight = 600;
    static const bool defaultAutocenterMode = false;
    static const float defaultLineWidth;
    static const float defaultAirplaneSize;
    static const int defaultMaxTrack = 0;
    static const float defaultUpdate;
    static const unsigned int defaultPort = 5500;
    static const char *defaultSerialDevice;
    static const int defaultBaudRate = 4800;
    static const float defaultAzimuth, defaultElevation;

protected:
    bool _loadPreferences(int argc, char *argv[]);
    unsigned int _port;
    SerialConnection _serial;
};

#endif	// _PREFERENCES_H_
