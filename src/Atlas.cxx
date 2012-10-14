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

// C system files
#include <stdio.h>
#ifdef _MSC_VER
  #include <io.h>     // For access()
  #ifndef F_OK
    #define F_OK 0
  #endif
#endif

// C++ system files
#include <stdexcept>

// Our libraries' include files
#include <GL/glew.h>

// Our project's include files
#include "AtlasController.hxx"
#include "AtlasWindow.hxx"
#include "FlightTrack.hxx"
#include "Globals.hxx"
#include "Graphs.hxx"
#include "NavData.hxx"

using namespace std;

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
    Preferences& p = globals.prefs;
    if (!p.load(argc, argv)) {
    	exit(1);
    }

    // A bit of post-preference processing.
    SGPath path = p.path;
    if (access(path.c_str(), F_OK) == -1) {
	printf("\nWarning: path %s doesn't exist. Maps won't be loaded!\n", 
	       path.c_str());
    }

    // Get our palette directory.
    SGPath paletteDir = p.path;
    paletteDir.append("Palettes");

    // One controller to rule them all.
    _ac = new AtlasController(paletteDir.c_str());

    // Read in files.
    for (unsigned int i = 0; i < p.flightFiles.size(); i++) {
    	// First check if we've loaded that file already.
    	const char *file = p.flightFiles[i].c_str();
    	if (_ac->find(file) == FlightTracks::NaFT) {
    	    // Nope - open it.
    	    try {
    		FlightTrack *t =new FlightTrack(_ac->navData(), file);
    		// Set the mark aircraft to the beginning of the track.
    		t->setMark(0);
    		_ac->addTrack(t);
    	    } catch (std::runtime_error e) {
    		printf("Failed to read flight file '%s'\n", file);
    	    }
    	}
    }
    // Make network connections.
    const vector<int>& ncs = p.networkConnections.prefs();
    for (unsigned int i = 0; i < ncs.size(); i++) {
    	// Already loaded?.
    	int port = ncs[i];
    	if (_ac->find(port) == FlightTracks::NaFT) {
    	    FlightTrack *f = 
    		new FlightTrack(_ac->navData(), port, p.maxTrack);
    	    _ac->addTrack(f);
    	}
    }
    // Make serial connections.
    const vector<Prefs::SerialConnection>& scs = p.serialConnections.prefs();
    for (unsigned int i = 0; i < scs.size(); i++) {
    	// Already loaded?.
    	const string& device = scs[i].device();
    	int baud = scs[i].baud();
    	if (_ac->find(device.c_str(), baud) == FlightTracks::NaFT) {
    	    FlightTrack *f = new FlightTrack(_ac->navData(), device.c_str(), 
					     baud, p.maxTrack);
    	    _ac->addTrack(f);
    	}
    }

    // GLUT initialization.
    glutInit(&argc, argv);

    // Find our scenery fonts.
    // EYE - put in preferences
    SGPath fontDir, regularFontFile, boldFontFile;
    fontDir = p.path;
    fontDir.append("Fonts");
    regularFontFile = fontDir;
    regularFontFile.append("Helvetica.100.txf");
    boldFontFile = fontDir;
    boldFontFile.append("Helvetica-Bold.100.txf");

    //////////////////////////////////////////////////////////////////////
    // Create the graphs window.  Why create the graphs window first?
    // It allows us to initialize GLEW with a minimum of fuss.  GLEW
    // can only be initialized after a OpenGL context (ie, window)
    // exists.  Moreover, we can't do anything that depends on GLEW
    // before glewInit() is called.  The graphs window constructor
    // doesn't do anything that requires GLEW, but the main Atlas
    // window constructor does, so it's safe to call glewInit() if the
    // graphs window is created first, but *not* if the main Atlas
    // window is.  Simple!

    // The graphs window doesn't need a depth buffer, etc.
    glutInitDisplayString("rgba double");
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
      
    //////////////////////////////////////////////////////////////////////
    // Now that we have a valid OpenGL context, initialize GLEW.
    // Because of the features we use, we need at least OpenGL 1.5,
    // along with the FBO extension.  One of these years we'll advance
    // into the 21st century, OpenGL-wise.
    GLenum err = glewInit();
    if (err != GLEW_OK) {
	fprintf(stderr, "Failed to initialize GLEW!\n");
	exit(0);
    }
    if (!GLEW_VERSION_1_5) {
    	fprintf(stderr, "OpenGL version 1.5 not supported!\n");
	exit(0);
    }
    // EYE - Really we should just ask for OpenGL 3.0, which
    // incorporated all the following as core functions.  However,
    // some people, namely the developer, are living in the past and
    // don't have OpenGL 3.0.
    if (!GLEW_EXT_framebuffer_object) {
	fprintf(stderr, "EXT_framebuffer_object not supported!\n");
	exit(0);
    }
    if (!GLEW_EXT_framebuffer_multisample) {
	fprintf(stderr, "EXT_framebuffer_multisample not supported!\n");
	exit(0);
    }

    //////////////////////////////////////////////////////////////////////
    // Create our Atlas window.
    // EYE - this seems to give us only 2 samples, which isn't enough
    // glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE | GLUT_MULTISAMPLE);

    // EYE - to see what kind of buffer we were given, start up OpenGL
    // Profiler, and either launch Atlas with it or attach to a
    // running Atlas process.  Open the Pixel Format window to find
    // out the format for each OpenGL context used by the process.
    // Note that the reported results differ depending on whether you
    // launched Atlas with OpenGL Profiler or attached to an already
    // running instance.  Both seem to have their uses.

    // EYE - this gives us a bit more control, and for some reason
    // defaults to 6 samples (perhaps it takes the maximum?)
    glutInitDisplayString("rgba depth samples double");
    globals.aw = new AtlasWindow("Atlas", 
				 regularFontFile.c_str(),
				 boldFontFile.c_str(),
				 _ac);
    const Prefs::Geometry& g = p.geometry;
    globals.aw->reshape(g.width(), g.height());

    // Initial position (this may be changed by the --airport option,
    // or by loading flight tracks).
    double latitude = p.latitude, longitude = p.longitude;
    globals.aw->movePosition(latitude, longitude);
    
    // Handle --airport option.
    SGPath icao(p.icao);
    if (!icao.isNull()) {
	if (_ac->searcher()->findMatches(icao.c_str(), 
					 globals.aw->eye(), -1)) {
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
	    fprintf(stderr, "Unknown airport: '%s'\n", icao.c_str());
	}
    }
  
    // Set our default zoom and position.
    globals.aw->zoomTo(p.zoom);
    if (!_ac->tracks().empty()) {
	// If we've loaded some tracks, display the first one and
	// centre the map on the aircraft.
	_ac->setCurrentTrack(0);
	globals.aw->centreMapOnAircraft();
    }

    glutMainLoop();
 
    return 0;
}
