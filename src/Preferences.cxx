/*-------------------------------------------------------------------------
  Preferences.cxx

  Written by Brian Schack, started August 2007.

  Copyright (C) 2007 - 2011 Brian Schack

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <getopt.h>
#ifndef _MSC_VER
#include "libgen.h"
#endif
#include <stdarg.h>

#include <cassert>
#include <fstream>

using namespace std;

// This is a space-saving macro used when parsing command-line
// arguments.  The function 'f' is assumed to be sscanf().
#define OPTION_CHECK(f,n,t) if ((f) != (n)) {\
	fprintf(stderr, "%s: bad option argument\n", basename(argv[0]));\
	print_help_for(t, "   ");					\
	return false;	  \
    }

#include "Preferences.hxx"
#include "misc.hxx"

// Preferences file.
const char *atlasrc = ".atlasrc";

// These are used to tag options.  They *cannot* begin at 0, they
// *must* be consecutive, FIRST_OPTION *must* come first, LAST_OPTION
// *must* come last, and the option *cannot* have the value 63 (ASCII
// '?').  Other than that, you can do anything you want. :-)
enum {FIRST_OPTION, 
      FG_ROOT_OPTION, 
      FG_SCENERY_OPTION,
      PATH_OPTION, 
      PALETTE_OPTION, 
      LAT_OPTION, 
      LON_OPTION, 
      AIRPORT_OPTION, 
      ZOOM_OPTION, 
      GLUTFONTS_OPTION,
      GEOMETRY_OPTION,
      SOFTCURSOR_OPTION,
      UDP_OPTION,
      SERIAL_OPTION,
      BAUD_OPTION,
      MAX_TRACK_OPTION,
      UPDATE_OPTION,
      AUTO_CENTER_MODE_OPTION,
      LINE_WIDTH_OPTION,
      AIRPLANE_IMAGE_OPTION,
      AIRPLANE_SIZE_OPTION,
      DISCRETE_CONTOURS_OPTION,
      SMOOTH_CONTOURS_OPTION,
      CONTOUR_LINES_OPTION,
      NO_CONTOUR_LINES_OPTION,
      LIGHTING_ON_OPTION,
      LIGHTING_OFF_OPTION,
      SMOOTH_SHADING_OPTION,
      FLAT_SHADING_OPTION,
      LIGHT_POSITION_OPTION,
      VERSION_OPTION,
      HELP_OPTION,
      LAST_OPTION
};

// Used by getopt_long()
static struct option long_options[] = {
    {"fg-root", required_argument, 0, FG_ROOT_OPTION},
    {"fg-scenery", required_argument, 0, FG_SCENERY_OPTION},
    {"atlas", required_argument, 0, PATH_OPTION},
    {"palette", required_argument, 0, PALETTE_OPTION},
    {"lat", required_argument, 0, LAT_OPTION},
    {"lon", required_argument, 0, LON_OPTION},
    {"airport", required_argument, 0, AIRPORT_OPTION},
    {"zoom", required_argument, 0, ZOOM_OPTION},
    {"glutfonts", no_argument, 0, GLUTFONTS_OPTION},
    {"geometry", required_argument, 0, GEOMETRY_OPTION},
    {"softcursor", no_argument, 0, SOFTCURSOR_OPTION},
    {"udp", optional_argument, 0, UDP_OPTION},
    {"serial", optional_argument, 0, SERIAL_OPTION},
    {"baud", required_argument, 0, BAUD_OPTION},
    {"update", required_argument, 0, UPDATE_OPTION},
    {"max-track", required_argument, 0, MAX_TRACK_OPTION},
    {"autocenter-mode", no_argument, 0, AUTO_CENTER_MODE_OPTION},
    {"line-width", required_argument, 0, LINE_WIDTH_OPTION},
    {"airplane", required_argument, 0, AIRPLANE_IMAGE_OPTION},
    {"airplane-size", required_argument, 0, AIRPLANE_SIZE_OPTION},
    {"discrete-contours", no_argument, 0, DISCRETE_CONTOURS_OPTION},
    {"smooth-contours", no_argument, 0, SMOOTH_CONTOURS_OPTION},
    {"contour-lines", no_argument, 0, CONTOUR_LINES_OPTION},
    {"no-contour-lines", no_argument, 0, NO_CONTOUR_LINES_OPTION},
    {"lighting", no_argument, 0, LIGHTING_ON_OPTION},
    {"no-lighting", no_argument, 0, LIGHTING_OFF_OPTION},
    {"smooth-shading", no_argument, 0, SMOOTH_SHADING_OPTION},
    {"flat-shading", no_argument, 0, FLAT_SHADING_OPTION},
    {"light", required_argument, 0, LIGHT_POSITION_OPTION},
    {"version", no_argument, 0, VERSION_OPTION},
    {"help", no_argument, 0, HELP_OPTION},
    {0, 0, 0, 0}
};

static void print_short_help(char *name) 
{
    printf("usage: %s [--atlas=<path>] [--fg-root=<path>] [--fg-scenery=<path>]\n", 
	   name);
    printf("\t[--palette=<path>] [--lat=<x>] [--lon=<x>] [--zoom=<m/pixel>]\n");
    printf("\t[--airport=<icao>] [--glutfonts] [--geometry=<w>x<h>]\n");
    printf("\t[--softcursor] [--udp[=<port>]] [--serial=[<dev>]] [--baud=<rate>]\n");
    printf("\t[--autocenter-mode] [--discrete-contour] [--smooth-contour]\n");
    printf("\t[--contour-lines] [--no-contour-lines] [--lighting]\n");
    printf("\t[--no-lighting] [--light=azim,elev] [--smooth-shading]\n");
    printf("\t[--flat-shading] [--line-width=<w>] [--airplane=<path>]\n");
    printf("\t[--airplane-size=<size>] [--version]\n");
    printf("\t[--help] [<flight file>] ...\n");
}

// Formats the given strings as follows:
//
// (a) Each line starts with 'indent'.
//
// (b) The first line has 'option', left-justified, occupying 20
//     spaces, then 'str'.
//
// (c) Subsequent lines have 20 spaces, then subsequent strings from
//     the varargs list.
//
// (d) The end of the list (ie, the last argument) must be NULL.
static void printOne(const char *indent, const char *option, 
		     const char *str, ...)
{
    const int width = 20;
    globalString.printf("%%s%%-%ds%%s\n", width);
    printf(globalString.str(), indent, option, str);

    va_list ap;
    char *s;
    va_start(ap, str);
    while ((s = va_arg(ap, char *)) != NULL) {
	printf(globalString.str(), indent, "", s);
    }
    va_end(ap);
}

// Prints a long entry for the given option.
static void print_help_for(int option, const char *indent)
{
    static AtlasString defaultStr;
    switch(option) {
      case FG_ROOT_OPTION:
	printOne(indent, "--fg-root=<path>", 
		 "Overrides FG_ROOT environment variable", NULL);
	break;
      case FG_SCENERY_OPTION:
	printOne(indent, "--fg-scenery=<path>", 
		 "Overrides FG_SCENERY environment variable", NULL);
	break;
      case PATH_OPTION:
	printOne(indent, "--atlas=<path>", "Set path for map images", NULL);
	break;
      case PALETTE_OPTION:
	printOne(indent, "--palette=<palette>", 
		 "Specify Atlas palette", NULL);
	break;
      case LAT_OPTION:
	defaultStr.printf("Default: %.2f", Preferences::defaultLatitude);
	printOne(indent, "--lat=<x>", 
		 "Start browsing at latitude x (south is negative)", 
		 defaultStr.str(), NULL);
	break;
      case LON_OPTION:
	defaultStr.printf("Default %.2f", Preferences::defaultLongitude);
	printOne(indent, "--lon=<x>",
		 "Start browsing at longitude x (west is negative)", 
		 defaultStr.str(), NULL);
	break;
      case AIRPORT_OPTION:
	printOne(indent, "--airport=<str>", 
		 "Start browsing at an airport specified by ICAO code", 
		 "or airport name", NULL);
	break;
      case ZOOM_OPTION:
	defaultStr.printf("Default %.2f", Preferences::defaultZoom);
	printOne(indent, "--zoom=<x>", "Set zoom level to x metres/pixel", 
		 defaultStr.str(), NULL);
	break;
      case GLUTFONTS_OPTION:
	printOne(indent, "--glutfonts", 
		 "Use GLUT bitmap fonts (fast for software rendering)", NULL);
	break;
      case GEOMETRY_OPTION:
	defaultStr.printf("Default: %dx%d", 
		       Preferences::defaultWidth, Preferences::defaultHeight);
	printOne(indent, "--geometry=<w>x<h>", "Set initial window size", 
		 defaultStr.str(), NULL);
	break;
      case SOFTCURSOR_OPTION:
	printOne(indent, "--softcursor", 
		 "Draw mouse cursor using OpenGL (for fullscreen Voodoo",
		 "cards)", NULL);
	break;
      case UDP_OPTION:
	defaultStr.printf("Default: %d", Preferences::defaultPort);
	printOne(indent, "--udp[=<port>]",
		 "Input read from UDP socket at specified port", 
		 defaultStr.str(), NULL);
	break;
      case SERIAL_OPTION:
	defaultStr.printf("Default: %s", Preferences::defaultSerialDevice);
	printOne(indent, "--serial[=<dev>]", 
		 "Input read from serial port with specified device",
		 defaultStr.str(), NULL);
	break;
      case BAUD_OPTION:
	defaultStr.printf("Default: %d", Preferences::defaultBaudRate);
	printOne(indent, "--baud=<rate>",
		 "Set serial port baud rate", defaultStr.str(), NULL);
	break;
      case UPDATE_OPTION:
	defaultStr.printf("Default: %.2f", Preferences::defaultUpdate);
	printOne(indent, "--update=<s>", 
		 "Check for position updates every s seconds",
		 defaultStr.str(), NULL);
	break;
      case MAX_TRACK_OPTION:
	defaultStr.printf("Default: %d", Preferences::defaultMaxTrack);
	printOne(indent, "--max-track=<x>",
		 "Maximum number of points to record while tracking a",
		 "flight (0 = unlimited)", defaultStr.str(), NULL);
	break;
      case AUTO_CENTER_MODE_OPTION:
	defaultStr.printf("Default: %s", Preferences::defaultAutocenterMode ? 
			  "true" : "false");
	printOne(indent, "--autocenter-mode",
		 "Automatically center map on aircraft",
		 defaultStr.str(), NULL);
	break;
      case LINE_WIDTH_OPTION:
	defaultStr.printf("Default: %.2f", Preferences::defaultLineWidth);
 	printOne(indent, "--line-width=<w>",
		 "Set line width of flight track overlays (in pixels)",
		 defaultStr.str(), NULL);
 	break;
      case AIRPLANE_IMAGE_OPTION:
 	printOne(indent, "--airplane=<path>",
		 "Specify image to be used as airplane symbol in flight",
		 "tracks.  If not present, a default image is used.", NULL);
	break;
      case AIRPLANE_SIZE_OPTION:
	defaultStr.printf("Default: %.2f", Preferences::defaultAirplaneSize);
 	printOne(indent, "--airplane-size=<x>",
		 "Set the size of the airplane image in pixels",
		 defaultStr.str(), NULL);
	break;
      case DISCRETE_CONTOURS_OPTION:
	printOne(indent, "--discrete-contours",
		 "Don't blend contour colours on live maps (default)", NULL);
	break;
      case SMOOTH_CONTOURS_OPTION:
	printOne(indent, "--smooth-contours",
		 "Blend contour colours on live maps", NULL);
	break;
      case CONTOUR_LINES_OPTION:
	printOne(indent, "--contour-lines",
		 "Draw contour lines at contour boundaries", NULL);
	break;
      case NO_CONTOUR_LINES_OPTION:
	printOne(indent, "--no-contour-lines",
		 "Don't draw contour lines at contour boundaries (default)", 
		 NULL);
	break;
      case LIGHTING_ON_OPTION:
	printOne(indent, "--lighting",
		 "Light the terrain on live maps (default)", NULL);
	break;
      case LIGHTING_OFF_OPTION:
	printOne(indent, "--no-lighting",
		 "Don't light the terrain on live maps (ie, flat light)", NULL);
	break;
      case SMOOTH_SHADING_OPTION:
	printOne(indent, "--smooth-shading",
		 "Smooth polygons on live maps (default)", NULL);
	break;
      case FLAT_SHADING_OPTION:
	printOne(indent, "--flat-shading",
		 "Don't smooth polygons on live maps", NULL);
	break;
      case LIGHT_POSITION_OPTION:
	defaultStr.printf("Default: %.1f,%.1f", Preferences::defaultAzimuth, 
			  Preferences::defaultElevation);
	printOne(indent, "--light=azim,elev",
		 "Set light position for live maps (all units in degrees)", 
		 "Azimuth is light direction (0 = north, 90 = east, ...)",
		 "Elevation is height above horizon (90 = overhead)",
		 defaultStr.str(), NULL);
	break;
      case VERSION_OPTION:
	printOne(indent, "--version", "Print version number", NULL);
	break;
      case HELP_OPTION:
	printOne(indent, "--help", "Print this help", NULL);
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
      print_help_for(i, "   ");
  }
}

const float Preferences::defaultLatitude = 37.5;
const float Preferences::defaultLongitude = -122.25;
const float Preferences::defaultZoom = 125.0;
const float Preferences::defaultLineWidth = 1.0;
const float Preferences::defaultAirplaneSize = 25.0;
const float Preferences::defaultUpdate = 1.0;
const char *Preferences::defaultSerialDevice = "/dev/ttyS0";
const float Preferences::defaultAzimuth = 315.0;
const float Preferences::defaultElevation = 55.0;

// All preferences should be given default values here.
Preferences::Preferences()
{
    latitude = defaultLatitude;
    longitude = defaultLongitude;
    zoom = defaultZoom;		// metres/pixel
    icao = strdup("");

    char *env = getenv("FG_ROOT");
    if (env == NULL) {
	// EYE - can this not be defined?  Should we just get rid of
	// FGBASE_DIR altogether?
	fg_root.set(FGBASE_DIR);
    } else {
	fg_root.set(env);
    }

    env = getenv("FG_SCENERY");
    if (env == NULL) {
	scenery_root.set(fg_root.str());
    } else {
	scenery_root.set(env);
    }

    if (fg_root.str().length() != 0) {
	path.set(fg_root.str());
    } else {
	path.set(FGBASE_DIR);
    }
    path.append("Atlas");

    textureFonts = true;
    width = defaultWidth;
    height = defaultHeight;
    softcursor = false;
    _port = defaultPort;
    _serial.device = strdup(defaultSerialDevice);
    _serial.baud = defaultBaudRate;
    update = defaultUpdate;
    max_track = defaultMaxTrack;
    autocenter_mode = defaultAutocenterMode;
    lineWidth = defaultLineWidth;
    airplaneImage.set(path.str());
    airplaneImage.append("airplane_image.png");
    airplaneImageSize = defaultAirplaneSize;

    // Lighting and rendering stuff.
    discreteContours = true;
    contourLines = false;
    lightingOn = true;
    smoothShading = true;
    azimuth = defaultAzimuth;
    elevation = defaultElevation;
    palette = strdup("default.ap");
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

    ifstream rc(rcpath.c_str());
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
    printf("%.2f\n", zoom);
    printf("%s\n", icao);
    printf("%s\n", path.c_str());
    printf("%s\n", fg_root.c_str());
    printf("%d\n", textureFonts);
    printf("%d\n", width);
    printf("%d\n", height);
    printf("%d\n", softcursor);
    for (unsigned int i = 0; i < networkConnections.size(); i++) {
	printf("net: %u\n", networkConnections[i]);
    }
    for (unsigned int i = 0; i < serialConnections.size(); i++) {
	printf("serial: %s@%u\n", 
	       serialConnections[i].device, serialConnections[i].baud);
    }
    printf("%.1f\n", update);
    printf("%s\n", scenery_root.c_str());
    printf("%d\n", max_track);
    printf("%d\n", autocenter_mode);
    printf("%f\n", lineWidth);
    printf("%s\n", airplaneImage.c_str());

    printf("%d\n", discreteContours);
    printf("%d\n", contourLines);
    printf("%d\n", lightingOn);
    printf("%d\n", smoothShading);
    printf("<%.1f, %.1f>", azimuth, elevation);
    printf("%s\n", palette);

    for (unsigned int i = 0; i < flightFiles.size(); i++) {
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
	  case ZOOM_OPTION:
	    OPTION_CHECK(sscanf(optarg, "%f", &zoom), 1, ZOOM_OPTION);
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
	  case PALETTE_OPTION:
	    free(palette);
	    palette = strdup(optarg);
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
	  case FG_SCENERY_OPTION:
	    scenery_root.set(optarg);
	    break;
	  case MAX_TRACK_OPTION:
	    OPTION_CHECK(sscanf(optarg, "%d", &max_track), 1, MAX_TRACK_OPTION);
	    break;
	  case DISCRETE_CONTOURS_OPTION:
	    discreteContours = true;
	    break;
	  case SMOOTH_CONTOURS_OPTION:
	    discreteContours = false;
	    break;
	  case CONTOUR_LINES_OPTION:
	    contourLines = true;
	    break;
	  case NO_CONTOUR_LINES_OPTION:
	    contourLines = false;
	    break;
	  case LIGHTING_ON_OPTION:
	    lightingOn = true;
	    break;
	  case LIGHTING_OFF_OPTION:
	    lightingOn = false;
	    break;
	  case SMOOTH_SHADING_OPTION:
	    smoothShading = true;
	    break;
	  case FLAT_SHADING_OPTION:
	    smoothShading = false;
	    break;
	  case LIGHT_POSITION_OPTION:
	    OPTION_CHECK(sscanf(optarg, "%f, %f", &azimuth, &elevation), 
			 2, LIGHT_POSITION_OPTION);
	    // Azimuths are normalized to 0 <= azimuth < 360.
	    azimuth = normalizeHeading(azimuth);
	    // Elevation values are clamped to 0 <= elevation <= 90.
	    if (elevation < 0.0) {
		elevation = 0.0;
	    }
	    if (elevation > 90.0) {
		elevation = 90.0;
	    }
	    break;
	  case AUTO_CENTER_MODE_OPTION:
	    autocenter_mode = true;
	    break;
 	  case LINE_WIDTH_OPTION:
 	    OPTION_CHECK(sscanf(optarg, "%f", &lineWidth), 1, 
			 LINE_WIDTH_OPTION);
 	    break;
 	  case AIRPLANE_IMAGE_OPTION:
	    airplaneImage.set(optarg); 
 	    break;
 	  case AIRPLANE_SIZE_OPTION:
	    OPTION_CHECK(sscanf(optarg, "%f", &airplaneImageSize), 1,
			 AIRPLANE_SIZE_OPTION);
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
