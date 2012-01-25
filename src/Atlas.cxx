/*-------------------------------------------------------------------------
  Atlas.cxx
  Map browsing utility

  Written by Per Liedman, started February 2000.
  Copyright (C) 2000 Per Liedman, liedman@home.se
  Copyright (C) 2009 - 2012 Brian Schack

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

#include <stdio.h>

#include "NavData.hxx"
#include "Graphs.hxx"

#ifdef _MSC_VER
  #include <io.h>     // For access()
  #ifndef F_OK
    #define F_OK 0
  #endif
#endif

// Our main controller.  It acts as the mediator between our "model"
// (all the data) and our views (AtlasWindow, GraphsWindow).
AtlasController *_ac;

void cleanup()
{
    delete globals.aw;
    delete globals.gw;
    delete _ac;
}

int main(int argc, char **argv) 
{
    // Our cleanup routine.
    atexit(cleanup);

    // Load our preferences.  If there's a problem with any of the
    // arguments, it will print some errors to stderr, and return false.
    if (!globals.prefs.loadPreferences(argc, argv)) {
    	exit(1);
    }

    // A bit of post-preference processing.
    if (access(globals.prefs.path.c_str(), F_OK)==-1) {
	printf("\nWarning: path %s doesn't exist. Maps won't be loaded!\n", 
	       globals.prefs.path.c_str());
    }

    // Get our palette directory.
    SGPath paletteDir = globals.prefs.path;
    paletteDir.append("Palettes");

    // One controller to rule them all.
    _ac = new AtlasController(paletteDir.c_str());

    // Read in files.
    for (unsigned int i = 0; i < globals.prefs.flightFiles.size(); i++) {
	// First check if we've loaded that file already.
	const char *file = globals.prefs.flightFiles[i].c_str();
	if (_ac->find(file) == FlightTracks::NaFT) {
	    // Nope - open it.
	    try {
		FlightTrack *t =new FlightTrack(_ac->navData(), file);
		// Set the mark aircraft to the beginning of the track.
		t->setMark(0);
		_ac->addTrack(t);
	    } catch (runtime_error e) {
		printf("Failed to read flight file '%s'\n", file);
	    }
	}
    }
    // Make network connections.
    for (unsigned int i = 0; i < globals.prefs.networkConnections.size(); i++) {
	// Already loaded?.
	int port = globals.prefs.networkConnections[i];
	if (_ac->find(port) == FlightTracks::NaFT) {
	    FlightTrack *f = 
		new FlightTrack(_ac->navData(), port, globals.prefs.max_track);
	    _ac->addTrack(f);
	}
    }
    // Make serial connections.
    for (unsigned int i = 0; i < globals.prefs.serialConnections.size(); i++) {
	// Already loaded?.
	const char *device = globals.prefs.serialConnections[i].device;
	int baud = globals.prefs.serialConnections[i].baud;
	if (_ac->find(device, baud) == FlightTracks::NaFT) {
	    FlightTrack *f = new FlightTrack(_ac->navData(), device, baud, 
					     globals.prefs.max_track);
	    _ac->addTrack(f);
	}
    }

    // GLUT initialization.
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
    // EYE - turn off depth test altogether?  We don't really need it,
    // as the back clip plane will do everything we want for us.
    // glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    // EYE - see glutInitWindowPosition man page - call glutInit() after
    // this and pass argc, and argv to glutInit?
    
    // Find our scenery fonts.
    // EYE - put in preferences
    SGPath fontDir, regularFontFile, boldFontFile;
    fontDir = globals.prefs.path;
    fontDir.append("Fonts");
    regularFontFile = fontDir;
    regularFontFile.append("Helvetica.100.txf");
    boldFontFile = fontDir;
    boldFontFile.append("Helvetica-Bold.100.txf");

    // Create our Atlas window.
    globals.aw = new AtlasWindow("Atlas", 
				 regularFontFile.c_str(),
				 boldFontFile.c_str(),
				 _ac);
    globals.aw->reshape(globals.prefs.width, globals.prefs.height);

    // Create the graphs window.
    globals.gw = new GraphsWindow("-- graphs --", 
				  regularFontFile.c_str(),
				  boldFontFile.c_str(),
				  _ac);

    // EYE - make graphs window size part of prefs?
    globals.gw->reshape(800, 200);

    // Initialize the graphs window.
    globals.gw->setAircraftColour(globals.trackColour);
    globals.gw->setMarkColour(globals.markColour);

    globals.gw->setXAxisType(GraphsWindow::TIME);
    globals.gw->setYAxisType(GraphsWindow::ALTITUDE, true);
    globals.gw->setYAxisType(GraphsWindow::SPEED, true);
    globals.gw->setYAxisType(GraphsWindow::CLIMB_RATE, true);
      
    // Make the AtlasWindow the active window.
    globals.aw->set();

    // Initial position (this may be changed by the --airport option,
    // or by loading flight tracks).
    double latitude = globals.prefs.latitude, 
	longitude = globals.prefs.longitude;
    globals.aw->movePosition(latitude, longitude);
    
    // Handle --airport option.
    if (strlen(globals.prefs.icao) != 0) {
	if (_ac->searcher()->findMatches(globals.prefs.icao, globals.aw->eye(), 
					 -1)) {
	    // We found some matches.  The question is: which one to
	    // use?  Answer: the first one that's actually an airport,
	    // for lack of a better choice.
	    for (unsigned int i = 0; i < _ac->searcher()->noOfMatches(); i++) {
		Searchable *s = _ac->searcher()->getMatch(i);
		ARP *ap;
		if ((ap = dynamic_cast<ARP *>(s))) {
		    globals.aw->movePosition(ap->lat, ap->lon);
		    break;
		}
	    }
	} else {
	    fprintf(stderr, "Unknown airport: '%s'\n", globals.prefs.icao);
	}
    }
  
    // Set our default zoom and position.
    globals.aw->zoomTo(globals.prefs.zoom);
    if (!_ac->tracks().empty()) {
	// If we've loaded some tracks, display the first one and
	// centre the map on the aircraft.
	_ac->setCurrentTrack(0);
	globals.aw->centreMapOnAircraft();
    }
    // // Update all our track information.
    // newFlightTrack();

    glutMainLoop();
 
    return 0;
}
