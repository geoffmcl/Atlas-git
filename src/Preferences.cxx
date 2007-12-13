/*-------------------------------------------------------------------------
  Preferences.cxx

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

#include <stdlib.h>
#include <getopt.h>
#include "config.h"
#include "libgen.h"
#include <fstream>

// This is a space-saving macro used when parsing command-line
// arguments.  The function 'f' is assumed to be sscanf().
#define OPTION_CHECK(f,n,t) if ((f) != (n)) {\
	fprintf(stderr, "%s: bad option argument\n", basename(argv[0]));\
	print_help_for(t);						\
	return false;	  \
    }

#include "Preferences.hxx"
#include "MapBrowser.hxx"

// Preferences file.
const char *atlasrc = ".atlasrc";

// These are used to tag options.  They *cannot* begin at 0, they
// *must* be consecutive, FIRST_OPTION *must* come first, LAST_OPTION
// *must* come last, and the option *cannot* have the value 63 (ASCII
// '?').  Other than that, you can do anything you want. :-)
enum {FIRST_OPTION, 
      LAT_OPTION, LON_OPTION, AIRPORT_OPTION, PATH_OPTION, 
      FG_ROOT_OPTION, GLUTFONTS_OPTION, GEOMETRY_OPTION, 
      SOFTCURSOR_OPTION, UDP_OPTION, SERIAL_OPTION, BAUD_OPTION, 
      SQUARE_OPTION, FG_SCENERY_OPTION, SERVER_OPTION, MAP_PATH_OPTION, 
      SIZE_OPTION, LOWRES_SIZE_OPTION, MAX_TRACK_OPTION, 
      TERRASYNC_MODE_OPTION, CONCURRENCY_OPTION, UPDATE_OPTION,
      AUTO_CENTER_MODE_OPTION, VERSION_OPTION, HELP_OPTION,
      LAST_OPTION
};

// Used by getopt_long()
static struct option long_options[] = {
    {"lat", required_argument, 0, LAT_OPTION},
    {"lon", required_argument, 0, LON_OPTION},
    {"airport", required_argument, 0, AIRPORT_OPTION},
    {"path", required_argument, 0, PATH_OPTION},
    {"fg-root", required_argument, 0, FG_ROOT_OPTION},
    {"glutfonts", no_argument, 0, GLUTFONTS_OPTION},
    {"geometry", required_argument, 0, GEOMETRY_OPTION},
    {"softcursor", no_argument, 0, SOFTCURSOR_OPTION},
    {"udp", optional_argument, 0, UDP_OPTION},
    {"serial", optional_argument, 0, SERIAL_OPTION},
    {"baud", required_argument, 0, BAUD_OPTION},
    {"update", required_argument, 0, UPDATE_OPTION},
    {"square", no_argument, 0, SQUARE_OPTION},
    {"fg-scenery", required_argument, 0, FG_SCENERY_OPTION},
    {"server", required_argument, 0, SERVER_OPTION},
    {"map-executable", required_argument, 0, MAP_PATH_OPTION},
    {"size", required_argument, 0, SIZE_OPTION},
    {"lowres-size", required_argument, 0, LOWRES_SIZE_OPTION},
    {"max-track", required_argument, 0, MAX_TRACK_OPTION},
    {"terrasync-mode", no_argument, 0, TERRASYNC_MODE_OPTION},
    {"concurrency", required_argument, 0, CONCURRENCY_OPTION},
    {"autocenter-mode", no_argument, 0, AUTO_CENTER_MODE_OPTION},
    {"version", no_argument, 0, VERSION_OPTION},
    {"help", no_argument, 0, HELP_OPTION},
    {0, 0, 0, 0}
};

static void print_short_help(char *name) {
    printf("usage: %s [--lat=<x>] [--lon=<x>] [--airport=<icao>] [--path=<path>]\n", name);
    printf("\t[--fg-root=<path>] [--glutfonts] [--geometry=<w>x<h>]\n");
    printf("\t[--softcursor] [--udp[=<port>]] [--serial=[<dev>]] [--baud=<rate>]\n");
    printf("\t[--square] [--fg-scenery=<path>] [--server=<addr>]\n");
    printf("\t[--map-executable=<path>] [--size=<pixels>] [--lowres-size=<pixels>]\n");
    printf("\t[--max-track=<x>] [--terrasync-mode] [--concurrency=<n>]\n");
    printf("\t[--update=<s>] [--autocenter-mode] [--version]\n");
    printf("\t[--help] [<flight file>] ...\n");
}

// Prints a long entry for the give option.
static void print_help_for(int option) 
{
    switch(option) {
    case LAT_OPTION:
	printf("--lat=<x>\tStart browsing at latitude x (deg. south i neg.)\n");
	break;
    case LON_OPTION:
	printf("--lon=<x>\tStart browsing at longitude x (deg. west i neg.)\n");
	break;
    case AIRPORT_OPTION:
	printf("--airport=<icao>\tStart browsing at an airport specified by ICAO code icao\n");
	break;
    case PATH_OPTION:
	printf("--path=<path>\tSet path for map images\n");
	break;
    case FG_ROOT_OPTION:
	printf("--fg-root=<path>\tOverrides FG_ROOT environment variable\n");
	break;
    case GLUTFONTS_OPTION:
	printf("--glutfonts\tUse GLUT bitmap fonts (fast for software rendering)\n");
	break;
    case GEOMETRY_OPTION:
	printf("--geometry=<w>x<h>\tSet initial window size\n");
	break;
    case SOFTCURSOR_OPTION:
	printf("--softcursor\tDraw mouse cursor using OpenGL (for fullscreen Voodoo cards)\n");
	break;
    case UDP_OPTION:
	printf("--udp[=<port>]\tInput read from UDP socket at specified port\n");
	printf("\t(defaults to 5500)\n");
	break;
    case SERIAL_OPTION:
	printf("--serial[=<dev>]\tInput read from serial port with specified device\n");
	printf("\t(defaults to /dev/ttyS0)\n");
	break;
    case BAUD_OPTION:
	printf("--baud=<rate>\tSet serial port baud rate (defaults to 4800)\n");
	break;
    case UPDATE_OPTION:
	printf("--update=<s>\tCheck for position updates every x seconds (defaults to 1.0)\n");
	break;
    case SQUARE_OPTION:
	printf("--square\tSet square mode (map 1x1 degree area on the whole image)\n");
	printf("\tto be compatible with images retrieved by GetMap\n");
	break;
    case FG_SCENERY_OPTION:
	printf("--fg-scenery=<path>\tLocation of FlightGear scenery (defaults to\n");
	printf("\t<fg-root>/Scenery-Terrasync)\n");
	break;
    case SERVER_OPTION:
	printf("--server=<addr>\tRsync scenery server (defaults to\n");
	printf("\t'scenery.flightgear.org')\n");
	break;
    case MAP_PATH_OPTION:
	printf("--map-executable=<path>\tLocation of Map executable (defaults to 'Map')\n");
	break;
    case SIZE_OPTION:
	printf("--size=<pixels>\tCreate maps of size pixels*pixels (default 256)\n");
	break;
    case LOWRES_SIZE_OPTION:
	printf("--lowres-size=<pixels>\tCreate lowres maps of size pixels*pixels\n");
	printf("\t(defaults to 0, meaning don't generate lowres maps)\n");
	break;
    case MAX_TRACK_OPTION:
	printf("--max-track=<x>\tMaximum number of points to record while tracking a\n");
	printf("\tflight (defaults to 2000, 0 = unlimited)\n");
	break;
    case TERRASYNC_MODE_OPTION:
	printf("--terrasync-mode\tDownload scenery while tracking a flight (default is\n");
	printf("\tto not download)\n");
	break;
    case CONCURRENCY_OPTION:
	printf("--concurrency=<n>\tNumber of tiles to simultaneously update (defaults to\n");
	printf("\t1, 0 = unlimited)\n");
	break;
    case AUTO_CENTER_MODE_OPTION:
	printf("--autocenter-mode\tAutomatically center map on aircraft (default is\n");
	printf("\tto not auto-center)\n");
	break;
    case VERSION_OPTION:
	printf("--version\tPrint version number\n");
	break;
    case HELP_OPTION:
	printf("--help\tPrint this help\n");
	break;
    }
}

// This prints a long help message.
static void print_help() {
  // EYE - use executable name here?
  printf("ATLAS - A map browsing utility for FlightGear\n\nUsage:\n\n");
  // EYE - use executable name here?
  printf("Atlas <options> [<flight file>] ...\n\n");
  for (int i = FIRST_OPTION + 1; i < LAST_OPTION; i++) {
      printf("   ");
      print_help_for(i);
  }
}

// All preferences should be given default values here.
Preferences::Preferences()
{
    latitude = 37.5;
    longitude = -122.25;
    icao = strdup("");

    char *env = getenv("FG_ROOT");
    if (env == NULL) {
	fg_root.set(FGBASE_DIR);
    } else {
	fg_root.set(env);
    }

    if (fg_root.str().length() != 0) {
	path.set(fg_root.str());
    } else {
	path.set(FGBASE_DIR);
    }
    path.append("Atlas");

    textureFonts = true;
    width = 800;
    height = 600;
    softcursor = false;
//     port = strdup("5500");
//     device = strdup("/dev/ttyS0");
//     baud = strdup("4800");
    _port = 5500;
    _serial.device = strdup("/dev/ttyS0");
    _serial.baud = 4800;
    update = 1.0;
    mode = MapBrowser::ATLAS;

    scenery_root.set(fg_root.str());
    scenery_root.append("Scenery-Terrasync");

    rsync_server = strdup("scenery.flightgear.org");
    map_executable.set("Map");
    map_size = 256;
    lowres_map_size = 0;
    max_track = 2000;
    terrasync_mode = false;
    concurrency = 1;
    autocenter_mode = false;
}

// First loads preferences from ~/.atlasrc (if it exists), then checks
// the command line options passed in via argc and argv.
bool Preferences::loadPreferences(int argc, char *argv[])
{
    // Check for a preferences file.
    char* homedir = getenv("HOME");
    SGPath rcpath;
    if (homedir != NULL) {
	rcpath.set(homedir);
	rcpath.append(atlasrc);
    } else {
	rcpath.set(atlasrc);
    }

    std::ifstream rc(rcpath.c_str());
    if (rc.is_open()) {
	char *lines[2];
	string line;

	// By default, getopt_long() (called in _loadPreferences())
	// skips past the first argument, which is usually the
	// executable name.  Theoretically, we should be able to tell
	// it to start processing from the first argument by setting
	// optind to 0, but I can't seem to get it to work, and
	// anyway, our error messages depend on argv[0] being set to
	// the executable.
	lines[0] = argv[0];
	while (!rc.eof()) {
	    getline(rc, line);

	    // Skip comments and emtpy lines.  Our version is given in
	    // a special comment of the format "#ATLASRC Version x".
	    int version;
	    if (line.length() == 0) {
		continue;
	    }
	    if (sscanf(line.c_str(), "#ATLASRC Version %d", &version) == 1) {
		if (version > 1) {
		    fprintf(stderr, "%s: Unknown %s version: %d\n",
			    basename(argv[0]), atlasrc, version);
		    return false;
		}
		continue;
	    }
	    if (line[0] == '#') {
		continue;
	    }

	    // EYE - should we remove leading and trailing whitespace?
	    // I guess it's a real option.
	    lines[1] = (char *)line.c_str();

	    // Try to make sense of it.
	    if (!_loadPreferences(2, lines)) {
		fprintf(stderr, "%s: Error in %s: '%s'.\n",
			basename(argv[0]), atlasrc, lines[1]);
		return false;
	    }
	}

	rc.close();
    }

    // Now parse the real command-line arguments.
    return _loadPreferences(argc, argv);
}

void Preferences::savePreferences()
{
    printf("%.2f\n", latitude);
    printf("%.2f\n", longitude);
    printf("%s\n", icao);
    printf("%s\n", path.c_str());
    printf("%s\n", fg_root.c_str());
    printf("%d\n", textureFonts);
    printf("%d\n", width);
    printf("%d\n", height);
    printf("%d\n", softcursor);
    for (int i = 0; i < networkConnections.size(); i++) {
	printf("net: %u\n", networkConnections[i]);
    }
    for (int i = 0; i < serialConnections.size(); i++) {
	printf("serial: %s@%u\n", 
	       serialConnections[i].device, serialConnections[i].baud);
    }
    printf("%.1f\n", update);
    printf("%d\n", mode);
    printf("%s\n", scenery_root.c_str());
    printf("%s\n", rsync_server);
    printf("%s\n", map_executable.c_str());
    printf("%d\n", map_size);
    printf("%d\n", lowres_map_size);
    printf("%d\n", max_track);
    printf("%d\n", terrasync_mode);
    printf("%d\n", concurrency);
    printf("%d\n", autocenter_mode);

    for (int i = 0; i < flightFiles.size(); i++) {
	printf("%s\n", flightFiles[i].c_str());
    }						
}

// Checks the given set of preferences.  Returns true (and sets the
// appropriate variables in Preferences) if there are no problems.
// Returns false (and prints an error message as appropriate) if
// there's a problem, or if the user asked for --version or --help.
bool Preferences::_loadPreferences(int argc, char *argv[])
{
    int c;
    int option_index = 0;
    SGPath p;

    // The use of optind (or optind and optreset, depending on your
    // system) is necessary because we may call getopt_long() many
    // times.
#ifdef HAVE_OPTRESET
    optreset = 1;
    optind = 1;
#else
    optind = 0;
#endif
    while ((c = getopt_long(argc, argv, "", long_options, &option_index)) 
	   != -1) {
	switch (c) {
	case LAT_OPTION:
	    OPTION_CHECK(sscanf(optarg, "%f", &latitude), 1, LAT_OPTION);
	    break;
	case LON_OPTION:
	    OPTION_CHECK(sscanf(optarg, "%f", &longitude), 1, LON_OPTION);
	    break;
	case AIRPORT_OPTION:
	    free(icao);
	    icao = strdup(optarg);
	    // Make sure it's in uppercase only.
	    for (unsigned int i = 0; i < strlen(icao); ++i) {
		icao[i] = toupper(icao[i]);
	    }
	    break;
	case PATH_OPTION:
	    path.set(optarg);
	    break;
	case FG_ROOT_OPTION:
	    fg_root.set(optarg);
	    break;
	case GLUTFONTS_OPTION:
	    textureFonts = false;
	    break;
	case GEOMETRY_OPTION:
	    OPTION_CHECK(sscanf(optarg, "%dx%d", &width, &height), 2, GEOMETRY_OPTION);
	    break;
	case SOFTCURSOR_OPTION:
	    softcursor = true;
	    break;
	case UDP_OPTION: {
	    // EYE - we need better documentation about how the UDP,
	    // SERIAL, and BAUD options interact.

	    // Whenever a unique --udp appears on the command line, we
	    // create an entry for it in networkConnections.  Whenever
	    // a unique --serial appears, we create an entry for it
	    // (using the current baud rate).  Whenever --baud
	    // appears, we just change the baud variable.  It does not
	    // affect --serial's that appear before it.
	    unsigned int thisPort = _port;
	    if (optarg) {
		OPTION_CHECK(sscanf(optarg, "%u", &thisPort), 1, UDP_OPTION);
	    }
	    networkConnections.push_back(thisPort);
	    break;
	}
	case SERIAL_OPTION: {
	    SerialConnection thisConnection;
	    thisConnection.baud = _serial.baud;
	    if (optarg) {
		thisConnection.device = strdup(optarg);
	    } else {
		thisConnection.device = strdup(_serial.device);
	    }
	    serialConnections.push_back(thisConnection);
	    break;
	}
	case BAUD_OPTION:
	    OPTION_CHECK(sscanf(optarg, "%u", &_serial.baud), 1, BAUD_OPTION);
	    break;
	case UPDATE_OPTION:
	    OPTION_CHECK(sscanf(optarg, "%f", &update), 1, UPDATE_OPTION);
	    break;
	case SQUARE_OPTION:
	    mode = MapBrowser::SQUARE;
	    break;
	case FG_SCENERY_OPTION:
	    scenery_root.set(optarg);
	    break;
	case SERVER_OPTION:
	    free(rsync_server);
	    rsync_server = strdup(optarg);
	    break;
	case MAP_PATH_OPTION:
	    map_executable.set(optarg);
	    break;
	case SIZE_OPTION:
	    OPTION_CHECK(sscanf(optarg, "%d", &map_size), 1, SIZE_OPTION);
	    break;
	case LOWRES_SIZE_OPTION:
	    OPTION_CHECK(sscanf(optarg, "%d", &lowres_map_size), 1, LOWRES_SIZE_OPTION);
	    break;
	case MAX_TRACK_OPTION:
	    OPTION_CHECK(sscanf(optarg, "%d", &max_track), 1, MAX_TRACK_OPTION);
	    break;
	case TERRASYNC_MODE_OPTION:
	    terrasync_mode = true;
	    break;
	case CONCURRENCY_OPTION:
	    OPTION_CHECK(sscanf(optarg, "%d", &concurrency), 1, CONCURRENCY_OPTION);
	    break;
	case AUTO_CENTER_MODE_OPTION:
	    autocenter_mode = true;
	    break;
	case HELP_OPTION:
	    print_help();
	    return false;
	    break;
	case VERSION_OPTION:
	    printf("%s version %s\n", basename(argv[0]), VERSION);
	    return false;
	    break;
	case '?':
	    // Note: We must make sure our _OPTION variables don't
	    // equal '?' (63).
	    print_short_help(basename(argv[0]));
	    return false;
	    break;
	default:
	    // We should never get here.
	    assert(false);
	}
    }
    while (optind < argc) {
	p.set(argv[optind++]);
	flightFiles.push_back(p);
    }

    return true;
}
