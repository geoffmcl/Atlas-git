/*-------------------------------------------------------------------------
  Preferences.hxx

  Written by Brian Schack, started August 2007.

  Copyright (C) 2007 Brian Schack

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

#ifndef __PREFERENCES_H__
#define __PREFERENCES_H__

#include <simgear/misc/sg_path.hxx>

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
// (5) Add code to process the option in Preferences::loadPreferences
//
// Easy!

// Note: We use getopt to make our option-parsing life easier.  A
// nicer, but non-standard, solution would be argtable.  It can be
// found at:
//
//   http://argtable.sourceforge.net/

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

    // Our preferences.
    float latitude;
    float longitude;
    char *icao;
    SGPath path;
    SGPath fg_root;
    bool textureFonts;
    int width;
    int height;
    bool softcursor;
    char *port;
    char *device;
    char *baud;
    float update;
    int mode;
    SGPath scenery_root;
    char *rsync_server;
    SGPath map_executable;
    int map_size;
    int lowres_map_size;
    int max_track;
    bool terrasync_mode;
    int concurrency;

    // These are derived from the preferences we load, but don't have
    // command-line options of their own.
    bool slaved, network, serial;

protected:
    bool _loadPreferences(int argc, char *argv[]);
    void _superChomp(char **line, size_t *length);
};

#endif        // __PREFERENCES_H__
