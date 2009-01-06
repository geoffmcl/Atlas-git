/*-------------------------------------------------------------------------
  Atlas.cxx
  Map browsing utility

  Written by Per Liedman, started February 2000.
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

#ifdef _MSC_VER
   // For access
#  include <io.h>
#  define F_OK      00
#endif

#include <memory.h>
#include <stdio.h>

#include <simgear/compiler.h>
//#include SG_GLUT_H
#include <plib/fnt.h>
#include <plib/pu.h>
#include <plib/puAux.h>
#include <string>

#include <map>
#include <stdexcept>

#include "MapBrowser.hxx"
#include "Overlays.hxx"
#include "FlightTrack.hxx"
#include "Tile.hxx"
#include "TileManager.hxx"
#include "Search.hxx"
#include "Preferences.hxx"
#include "Graphs.hxx"

#define SCALECHANGEFACTOR 1.3f

using std::runtime_error;

// User preferences (including command-line arguments).
Preferences prefs;

bool dragmode = false;
int drag_x, drag_y;
float scalefactor = 1.0f, mapsize, width, height;

// EYE - replace this stuff with current point?
float latitude, copy_lat;
float longitude, copy_lon;

// float heading = 0.0f, speed, altitude;

fntTexFont *texfont;
puFont *font;
puPopup *main_interface, *minimized, *info_interface;
puFrame *frame, *info_frame;
puOneShot *zoomin, *zoomout, *minimize_button, *minimized_button;
puOneShot *clear_ftrack, *choose_projection_button;
puButton *show_arp, *show_nav, *show_name, *show_id;
puButton *show_vor, *show_ndb, *show_fix, *show_ils;
puButton *show_ftrack, *follow;
puText *labeling, *txt_lat, *txt_lon;
puText *txt_info_lat, *txt_info_lon, *txt_info_alt;
puText *txt_info_hdg, *txt_info_spd;
puInput *inp_lat, *inp_lon;
puPopupMenu *choose_projection_menu;
puObject *proj_item[MAX_NUM_PROJECTIONS];

puButtonBox *button_box_info;
// EYE - constant alert!  We should get '3' from Graphs.hxx somehow.
const char *button_box_labels[3];
puSlider *smoother;

// Synchronization and map generation interface.
puPopup *sync_interface;
puFrame *sync_frame;
puText *txt_sync_name, *txt_sync_phase, *txt_sync_files, *txt_sync_bytes;
puDial *dial_sync_progress;

// Search interface.
Search *search_interface;

// Altitude/Speed interface.
int graphs_window;
int main_window;
Graphs *graphs;

char lat_str[80], lon_str[80], alt_str[80], hdg_str[80], spd_str[80];

// File dialog.
puaFileSelector *fileDialog = NULL;

// By default, when we show a flight track, we display the information
// window and graphs window.  If this is false (this can be toggled by
// the user), then we don't show them.
bool showGraphWindow = true;

SGPath lowrespath;
int lowres_avlble;

MapBrowser *map_object;
FlightTrack *track = NULL;
vector<FlightTrack *> tracks;
int currentFlightTrack;

// The tile manager keeps track of tiles that need to be updated.
TileManager *tileManager;

// Keeps track of which downloading tile is being displayed.  0
// represents the first tile.
unsigned int nthTile = 0;

/*****************************************************************************/
/* Convert degrees to dd mm'ss.s" (DMS-Format)                               */
/*****************************************************************************/
static char *dmshh_format(float degrees, char *buf)
{
 int deg_part;
 int min_part;
 float sec_part;

 if (degrees < 0)
         degrees = -degrees;

 deg_part = (int)degrees;
 min_part = (int)(60.0f * (degrees - deg_part));
 sec_part = 3600.0f * (degrees - deg_part - min_part / 60.0f);

 /* Round off hundredths */
 if (sec_part + 0.005f >= 60.0f)
         sec_part -= 60.0f, min_part += 1;
 if (min_part >= 60)
         min_part -= 60, deg_part += 1;

 sprintf(buf,"%02d*%02d %05.2f",deg_part,min_part,sec_part);

 return buf;
}

#if 0    // Currently not used.
static char *coord_format_latlon(float latitude, float longitude, char *buf)
{
 char buf1[16], buf2[16];

 sprintf(buf,"%c %s   %c %s",
                 latitude > 0 ? 'N' : 'S',
                 dmshh_format(latitude * SG_RADIANS_TO_DEGREES, buf1),
                 longitude > 0 ? 'E' : 'W',
                 dmshh_format(longitude * SG_RADIANS_TO_DEGREES, buf2)       );
 return buf;
}
#endif

/*****************************************************************************/
/* Center map at aircraft's current position.
/*****************************************************************************/
void centerMapOnAircraft()
{
    if (track && !track->empty()) {
	FlightData *pos = track->getCurrentPoint();

	latitude = pos->lat;
	longitude = pos->lon;
	map_object->setLocation(latitude, longitude);

	glutPostRedisplay();
    }
}

/******************************************************************************
Search helper functions.
******************************************************************************/
// Called to initiate a new search.  If the search is big, then it
// will only do a portion of it, then reschedule itself to continue
// the search.  To prevent multiple search threads beginning, we use
// the variable 'searching' to keep track of current activity.
bool searching = false;
void searchTimer(int value)
{
    char *str;
    static int maxMatches = 100;

    str = search_interface->searchString();
    if (map_object->getOverlays()->findMatches(str, maxMatches)) {
	// Show the new items.  Ensure that the main window is the
	// current window.
	glutSetWindow(main_window);

	search_interface->reloadData();
	glutPostRedisplay();

	// Continue the search in 100ms.
	searching = true;
	glutTimerFunc(100, searchTimer, 1);
    } else {
	// Search is finished.
	searching = false;
    }
}

// Called when the user selects an item in the list.
void searchItemSelected(Search *s, int i)
{
    if (i != -1) {
	Overlays::TOKEN t = map_object->getOverlays()->getToken(i);
	if (t.t == Overlays::AIRPORT) {
	    Overlays::ARP *ap = (Overlays::ARP *)t.locAddr;
	    latitude = ap->lat;
	    longitude = ap->lon;
	    map_object->setLocation(latitude, longitude);
	} else if (t.t == Overlays::NAVAID) {
	    Overlays::NAV *n = (Overlays::NAV *)t.locAddr;
	    latitude = n->lat;
	    longitude = n->lon;
	    map_object->setLocation(latitude, longitude);
	}
	glutPostRedisplay();
    }
}

// Called when the user changes the search string.  We just call
// searchTimer() if it's not running already, which will incrementally
// search for str.
void searchStringChanged(Search *s, char *str)
{
    if (!searching) {
	searchTimer(0);
    }
}

// Called by the search interface to find out how many matches there
// are.
int noOfMatches(Search *s)
{
    return map_object->getOverlays()->noOfMatches();
}

// Removes any trailing whitespace, and replaces multiple spaces and
// tabs by single spaces.  Returns a pointer to the cleaned up string.
// If you want to use the string, you might want to copy it, as it
// will be overwritten on the next call.
char *cleanString(char *str)
{
    static char *buf = NULL;
    static int length = 0;
    int i, j;
    
    if ((buf == NULL) || (length < strlen(str))) {
	length = strlen(str);
	buf = (char *)realloc(buf, sizeof(char) * (length + 1));
	if (buf == NULL) {
	    fprintf(stderr, "cleanString: Out of memory!\n");
	    exit(1);
	}
    }

    i = j = 0;
    while (i <= strlen(str)) {
	int skip = strspn(str + i, " \t\n");
	i += skip;

	// Replaced skipped whitespace with a single space, unless
	// we're at the end of the string.
	if ((skip > 0) && (i < strlen(str))) {
	    buf[j++] = ' ';
	}

	// Copy the next character over.  This will copy the final
	// '\0' as well.
	buf[j++] = str[i++];
    }

    return buf;
}

// Called by the search interface to get the data for index i.
char *matchAtIndex(Search *s, int i)
{
    Overlays::TOKEN t = map_object->getOverlays()->getToken(i);
    char *result;

    switch (t.t) {
    case Overlays::AIRPORT: 
	{
	    Overlays::ARP *ap = (Overlays::ARP *)t.locAddr;
	    // The names given to us often have extra whitespace,
	    // including linefeeds at the end, so we call
	    // cleanString() to clean things up.
	    asprintf(&result, "AIR: %s %s", ap->id, cleanString(ap->name));
	}
	break;
    case Overlays::NAVAID: 
	{
	    Overlays::NAV *n = (Overlays::NAV *)t.locAddr;
	    switch (n->navtype) {
	    case Overlays::NAV_VOR:
		asprintf(&result, "VOR: %s %s (%.2f)", 
			 n->id, cleanString(n->name), n->freq / 100.0);
		break;
	    case Overlays::NAV_DME:
		asprintf(&result, "DME: %s %s (%.2f)", 
			 n->id, cleanString(n->name), n->freq / 100.0);
		break;
	    case Overlays::NAV_NDB:
		asprintf(&result, "NDB: %s %s (%d)", 
			 n->id, cleanString(n->name), n->freq);
		break;
	    case Overlays::NAV_ILS:
		asprintf(&result, "ILS: %s %s (%.2f)", 
			 n->id, cleanString(n->name), n->freq / 100.0);
		break;
	    case Overlays::NAV_FIX:
		// Fixes have no separate id and name (they are identical).
		asprintf(&result, "FIX: %s", n->id);
		break;
	    default:
		fprintf(stderr,
			"Token points to unknown record type: %d (%s)\n", 
			t.t, cleanString(n->name));
		assert(false);
		break;
	    }
	}
	break;
    default:
	assert(true);
    }

    return result;
}

/******************************************************************************
Smoothing callback.
******************************************************************************/
void smoother_cb(puObject *dial) 
{
    // This must be static, as PUI will access it later.
    static char buf[50];
    sprintf(buf, "%d", smoother->getIntegerValue());
    smoother->setLegend(buf);

    // Update the graphs.
    graphs->setSmoothing(smoother->getIntegerValue());
    glutSetWindow(graphs_window);
    glutPostRedisplay();

    // Update the interface.
    glutSetWindow(main_window);
    glutPostRedisplay();
}

void redrawGraphs();

// Sets the current flight track to the given flight track (which is
// an index in the tracks vector.  It also deals with propagating the
// change to the graphs object, and the graphs window.  It trackNo is
// out of range, then it sets things to no flight track at all.
void setFlightTrack(int trackNo)
{
    if ((trackNo < 0) || (trackNo >= tracks.size())) {
	currentFlightTrack = -1;
	track = NULL;
    } else {
	currentFlightTrack = trackNo;
	track = tracks[currentFlightTrack];
    }

    graphs->setFlightTrack(track);
    map_object->setFlightTrack(track);

    if (!track) {
	// No tracks, so hide everything.
	glutSetWindow(graphs_window);
	glutHideWindow();

	glutSetWindow(main_window);
	info_interface->hide();

	glutPostRedisplay();
    } else {
	glutSetWindow(graphs_window);
	if (showGraphWindow) {
	    glutShowWindow();
	}
	// EYE - Sometimes I have to force the call to redrawGraphs(),
	// even though a glutPostRedisplay() should be enough.
	// There's something funny going on here (or somewhere else).
	// The case where it's necessary is: start Atlas clean (no
	// tracks), then load in a track.  Without redrawGraphs(), the
	// graphs window will pop up empty.  If I then move the graphs
	// window around, the graphs will be drawn (but the title,
	// curiously, will not).  If I click in the Atlas window, the
	// title will then appear.
	redrawGraphs();
// 	glutPostRedisplay();	// This should be enough.

	glutSetWindow(main_window);
	if (showGraphWindow) {
	    info_interface->reveal();
	}
	centerMapOnAircraft();
	smoother_cb(smoother);
	glutPostRedisplay();
    }
}

/******************************************************************************
Tile loading helper functions.
******************************************************************************/
void tileTimer(int value);

// Schedules the 1x1 tile containing the given latitude/longitude
// (which must be given in degrees) for updating.
void loadTile(float latitude, float longitude)
{
    Tile *tile;
    std::ostringstream buf;
    unsigned int tasks;

    tile = new Tile(latitude, longitude, prefs);

    // We always ask for scenery and a hires map.
    tasks = Tile::SYNC_SCENERY | Tile::GENERATE_HIRES_MAP;
    // Ask for a lowres map only if the user wants them.
    if (prefs.lowres_map_size != 0) {
	tasks |= Tile::GENERATE_LOWRES_MAP;
    }
    tile->setTasks(tasks);
    tileManager->addTile(tile);

    // If there's only one tile, start downloading it right away.
    if (tileManager->noOfTiles() == 1) {
	glutTimerFunc(0, tileTimer, 1);
	glutPostRedisplay();
    }
}

// Centers the map on the 'nth' updating tile.
void nextTile()
{
    float lat, lon;
    Tile *t;

    // If we have no tiles, don't do anything.
    if (tileManager->noOfTiles() == 0) {
	nthTile = 0;
	return;
    }

    // Advance to the next tile.
    nthTile = (nthTile + 1) % tileManager->noOfTiles();
    t = tileManager->nthTile(nthTile);
    latitude = t->lat() * SG_DEGREES_TO_RADIANS;
    longitude = t->lon() * SG_DEGREES_TO_RADIANS;
    map_object->setLocation(latitude, longitude);
    glutPostRedisplay();
}

// Schedules tile at the given latitude and longitude (specified in
// degrees) to be downloaded if it isn't scheduled to be downloaded
// already, removes it from the schedule if it's already scheduled to
// be downloaded.
void toggleTile(float latitude, float longitude)
{
    Tile *tile;

    tile = tileManager->tileAtLatLon(latitude, longitude);
    if (tile) {
	// Toggle it off.
	tileManager->removeTile(tile);
    } else {
	// New tile.  Add it to the list of syncing tiles.
	loadTile(latitude, longitude);
    }
}

// Checks to see if we need to download new tiles as our aircraft
// moves.  This routine expects that the latitude and longitude global
// variables contain the aircraft's latest position, and should be
// called periodically as the aircraft's position changes.
void terrasyncUpdate()
{
    bool doSomething;
    float lat, lon;
    static float oldLat, oldLon;
    char oldName[8], name[8], dir[8];
    float centerLat, centerLon;

    // Convert to degrees.
    lat = latitude * SG_RADIANS_TO_DEGREES;
    lon = longitude * SG_RADIANS_TO_DEGREES;

    // First, check to see if we need to do anything.  We only do
    // something in 2 cases:
    // (1) We've just started
    // (2) We've crossed a tile boundary
    if ((abs(lat) < 0.01) && (abs(lon) < 0.01)) {
	// This is a very special case.  When FlightGear starts a
	// flight, it first moves the aircraft down to <0.0, 0.0>
	// before moving it to its start point.  We don't want to
	// continually have to load tiles at <0.0, 0.0> whenever
	// FlightGear starts up (there isn't any scenery there
	// anyway), so we make a special check in this case.
	doSomething = false;
    } else if (track->empty()) {
	// (1) We've just started
	doSomething = true;
    } else {
	TileManager::latLonToTileInfo(oldLat, oldLon, oldName, dir, 
				      &centerLat, &centerLon);
	TileManager::latLonToTileInfo(lat, lon, name, dir, 
				      &centerLat, &centerLon);
	if (strcmp(oldName, name) != 0) {
	    // (2) We've crossed a tile boundary
	    doSomething = true;
	} else {
	    doSomething = false;
	}
    }
    oldLat = lat;
    oldLon = lon;

    if (doSomething) {
	// Ask the tile manager to check up on the 9 tiles centered on
	// our position.  The tile manager is smart enough to ignore
	// extra requests for tiles it's already dealing with.  It's
	// also smart enough to avoid unnecessary work, so if we
	// already have scenery and maps for these tiles, we won't
	// incur much of a performance hit.
	int i, j;

	// Get the lat,lon of the center of the current tile.  We do
	// this to be safe when doing floating point calculations.
	// Perhaps this is overly paranoid.
	TileManager::latLonToTileInfo(lat, lon, name, dir, 
				      &centerLat, &centerLon);
	for (i = -1; i <= 1; i++) {
	    for (j = -1; j <= 1; j++) {
		loadTile(centerLat + i, centerLon + j);
	    }
	}
    }
}

// Updates the sync interface.
void updateSyncInterface() {
    // These are used to supply strings to labels on the interface.
    // The puText object requires a pointer to a string, and does not
    // copy the string.  Therefore, the buffer must not change
    // location!  If it does, we must call puObject::setLabel() to
    // point the puText object to the new buffer.  
    //
    // Using ostringstreams and strings are a bit ugly, but then we
    // don't have to worry about buffer overflows.
    static std::string tile_name_str, files_str, bytes_str;
    std::ostringstream nbuf, fbuf, bbuf;

    // Ensure that the main window is the current window.
    glutSetWindow(main_window);

    // Used to drive the progress meter.
    static int progress = 0;

    // nthTile points to the currently displayed tile.  However, it
    // may become invalid.  Check and see.
    if (nthTile >= tileManager->noOfTiles()) {
	nthTile = tileManager->noOfTiles() - 1;
    }

    Tile *t = tileManager->nthTile(nthTile);
    if (t == NULL) {
	sync_interface->hide();
	return;
    }

    sync_interface->reveal();

    nbuf << t->name() << " (" << nthTile + 1 << "/" 
	 << tileManager->noOfTiles() << ")";
    tile_name_str = nbuf.str();
    txt_sync_name->setLabel(tile_name_str.c_str());

    // Update the progress meter.  It should spin about once every
    // ten calls.
    dial_sync_progress->setValue((float)(progress / 10.0));
    progress = (progress + 1) % 10;

    if ((t->taskState() == Tile::CHECKING_OBJECTS) ||
	(t->taskState() == Tile::CHECKING_TERRAIN)) {
	fbuf << t->toBeSyncedFiles() << " files";
	files_str = fbuf.str();
	bbuf << t->toBeSyncedSize() << " bytes";
	bytes_str = bbuf.str();

	if (t->taskState() == Tile::CHECKING_OBJECTS) {
	    txt_sync_phase->setLabel("Checking objects");
	} else {
	    txt_sync_phase->setLabel("Checking terrain");
	}
	txt_sync_files->setLabel(files_str.c_str());
	txt_sync_bytes->setLabel(bytes_str.c_str());
    } else if ((t->taskState() == Tile::SYNCING_OBJECTS) ||
	       (t->taskState() == Tile::SYNCING_TERRAIN)) {
	int filesPercent = 0, sizePercent = 0;

	if (t->toBeSyncedFiles() > 0) {
	    filesPercent = 100 * t->syncedFiles() / t->toBeSyncedFiles();
	}
	fbuf << t->syncedFiles() << "/" << t->toBeSyncedFiles() << " files ("
	     << filesPercent << "%)";
	files_str = fbuf.str();

	if (t->toBeSyncedSize() > 0) {
	    sizePercent = 100 * t->syncedSize() / t->toBeSyncedSize();
	}
	bbuf << t->syncedSize() << "/" << t->toBeSyncedSize() << " bytes ("
	     << sizePercent << "%)";
	bytes_str = bbuf.str();

	if (t->taskState() == Tile::SYNCING_OBJECTS) {
	    txt_sync_phase->setLabel("Syncing objects");
	} else {
	    txt_sync_phase->setLabel("Syncing terrain");
	}
	txt_sync_files->setLabel(files_str.c_str());
	txt_sync_bytes->setLabel(bytes_str.c_str());
    } else if (t->taskState() == Tile::MAPPING) {
	if (t->currentTask() == Tile::GENERATE_HIRES_MAP) {
	    txt_sync_phase->setLabel("Mapping");
	} else {
	    txt_sync_phase->setLabel("Mapping lowres");
	}
	txt_sync_files->setLabel("");
	txt_sync_bytes->setLabel("");
    } else {
	txt_sync_phase->setLabel("Waiting");
	txt_sync_files->setLabel("");
	txt_sync_bytes->setLabel("");
    }
}

/******************************************************************************
 PUI code (HANDLERS)
******************************************************************************/
void zoom_cb ( puObject *cb )
{ float new_scale, prev_scale;
   
  prev_scale=map_object->getScale();
  
  if (cb == zoomin) { 
    map_object->setScale( map_object->getScale() / SCALECHANGEFACTOR );
    scalefactor /= SCALECHANGEFACTOR;
  } else {
    map_object->setScale( map_object->getScale() * SCALECHANGEFACTOR );
    scalefactor *= SCALECHANGEFACTOR;
  }
  new_scale=map_object->getScale();
   
  //set map math depending on resolution
  if (lowres_avlble) {
     if (new_scale > 1000000 && prev_scale <=1000000) {
	puts("Switching to low resolution maps");
	map_object->changeResolution(lowrespath.c_str());
     } else if (new_scale <= 1000000 && prev_scale > 1000000) {
	puts("Switching to default resolution maps");
	map_object->changeResolution(prefs.path.c_str());
     }
  }
  glutPostRedisplay();
}

void show_cb ( puObject *cb )
{
  int feature = 0;
  if (cb == show_arp) { 
    feature = Overlays::OVERLAY_AIRPORTS;
  } else if (cb == show_vor) {
    feature = Overlays::OVERLAY_NAVAIDS_VOR;
  } else if (cb == show_ndb) {
    feature = Overlays::OVERLAY_NAVAIDS_NDB;
  } else if (cb == show_fix) {
    feature = Overlays::OVERLAY_NAVAIDS_FIX;
  } else if (cb == show_ils) {
    feature = Overlays::OVERLAY_NAVAIDS_ILS;
  } else if (cb == show_nav) {
    feature = Overlays::OVERLAY_NAVAIDS;
  } else if (cb == show_name) {
    feature = Overlays::OVERLAY_NAMES;
  } else if (cb == show_id) {
    feature = Overlays::OVERLAY_IDS;
  } else if (cb == show_ftrack) {
    feature = Overlays::OVERLAY_FLIGHTTRACK;
  } else {
	  printf("Warning: show_cb called with unknown callback\n");
	  return;
  }
  if (cb->getValue()) {
    map_object->setFeatures( map_object->getFeatures() | feature );
  } else {
    map_object->setFeatures( map_object->getFeatures() & ~feature );
  }
  glutPostRedisplay();
}

void position_cb ( puObject *cb ) {
  char *buffer;
  cb->getValue(&buffer);

  char ns, deg_ch, min_ch = ' ', sec_ch = ' ';
  float degrees = 0, minutes = 0, seconds = 0;

  // Free-format entry: "N51", "N50.99*", "N50*59 24.1", etc.
  int n_items = sscanf(buffer, " %c %f%c %f%c %f%c",
    &ns, &degrees, &deg_ch, &minutes, &min_ch, &seconds, &sec_ch);
  if (n_items < 2) return;
  // if (!strchr(" m'", min_ch) || !strchr(" s\"", sec_ch)) return;
  float angle = (degrees + minutes / 60 + seconds / 3600) *
    SG_DEGREES_TO_RADIANS;
  if (cb == inp_lat) {
    latitude = ((ns=='S'||ns=='s')?-1.0f:1.0f) * angle;
  } else {
    longitude = ((ns=='W'||ns=='w')?-1.0f:1.0f) * angle;
  }
  map_object->setLocation(latitude, longitude);
  glutPostRedisplay();
}

void clear_ftrack_cb ( puObject * ) {
  if (track != NULL) {
    track->clear();
  }

  glutPostWindowRedisplay(graphs_window);
  glutPostRedisplay();
}

void minimize_cb ( puObject * ) {
  main_interface->hide();
  minimized->reveal();
}

void restore_cb ( puObject * ) {
  minimized->hide();
  main_interface->reveal();
}

void projection_cb (puObject *cb) {
   if (cb == choose_projection_button) {
      choose_projection_menu->reveal();
   }
   else {
      int i;
      for (i=0;i<map_object->getNumProjections();i++) {
	 if (cb==proj_item[i])
	   break;
      }
      map_object->setProjectionByID(i);
      choose_projection_menu->hide();
   }
   glutPostRedisplay();
}
      
// EYE - this counts on the Graphs type values being same as the
// corresponding entries in the button box.
void graph_type_cb (puObject *cb) 
{
    glutSetWindow(graphs_window);
    if (button_box_info->getValue() == 0) {
	glutHideWindow();
    } else {
	graphs->setGraphTypes(button_box_info->getValue());
	glutShowWindow();
	// EYE - calling glutPostRedisplay() doesn't seem to work
	// here, so I call redrawGraphs() directly.
	redrawGraphs();
    }

    glutSetWindow(main_window);
}

// Called when the user presses OK or Cancel on the save file dialog.
void save_file_cb(puObject *cb)
{
    // If the user hit "Ok", then the string value of the save dialog
    // will be non-empty.
    char *file = fileDialog->getStringValue();
    if (strcmp(file, "") != 0) {
	// EYE - we should warn the user if they're overwriting an
	// existing file.

	// Note: it's important that we don't let 'track' change while
	// the save dialog is active.
	track->setFilePath(file);
	track->save();

	// Force the graphs window to update itself (in particular,
	// its title).
	glutPostWindowRedisplay(graphs_window);
    }

    // Unfortunately, being a subclass of puDialogBox, a hidden
    // puaFileSelector will continue to grab all mouse events.  So, it
    // must be deleted, not hidden when we're finished.  This is
    // unfortunate because we can't "start up from where we left off"
    // - each time it's created, it's created anew.
    puDeleteObject(fileDialog);
    fileDialog = NULL;
}

// Called when the user presses OK or Cancel on the load file dialog.
void load_file_cb(puObject *cb)
{
    // If the user hit "Ok", then the string value of the save dialog
    // will be non-empty.
    char *file = fileDialog->getStringValue();
    if (strcmp(file, "") != 0) {
	FlightTrack *aTrack;
	try {
	    // EYE - if we've already opened this file before, we
	    // shouldn't open it again, should we?
	    aTrack = new FlightTrack(file);
	    // Set the mark aircraft to the beginning of the track.
	    aTrack->setMark(0);
	    // Add track to end of tracks vector and display it.
	    tracks.push_back(aTrack);
	    setFlightTrack(tracks.size() - 1);
	} catch (runtime_error e) {
	    // EYE - beep? dialog box? console message?
	    printf("Failed to read flight file '%s'\n", file);
	}
    }

    puDeleteObject(fileDialog);
    fileDialog = NULL;
}

/*****************************************************************************
 PUI Code (WIDGETS)
*****************************************************************************/
void init_gui(bool textureFonts) {
  puInit();

  int curx,cury;

  int puxoff=20,puyoff=20,puxsiz=205,puysiz=420;

  if (textureFonts) {
    SGPath font_name(prefs.fg_root.str());
    font_name.append("Fonts/helvetica_medium.txf");

    texfont = new fntTexFont( font_name.c_str() );
    font = new puFont( texfont, 16.0f );
//     font = new puFont( texfont, 12.0f );
  } else {
    font = new puFont();
  }
  puSetDefaultFonts(*font, *font);
  puSetDefaultColourScheme(0.4f, 0.4f, 0.8f, 0.6f);

  main_interface = new puPopup(puxoff,puyoff);
  frame = new puFrame(0, 0, puxsiz, puysiz);

  curx = cury = 10;

  zoomin = new puOneShot(curx, cury, "Zoom In");
  zoomin->setCallback(zoom_cb);
  zoomin->setSize(90, 24);
  zoomout = new puOneShot(curx+95, cury, "Zoom Out");
  zoomout->setCallback(zoom_cb);
  zoomout->setSize(90, 24);

  cury+=35;
  
  show_vor = new puButton(curx, cury, "VOR");
  show_vor->setSize(44,24);
  show_vor->setCallback(show_cb);
  show_vor->setValue(1);
  show_ndb = new puButton(curx+46, cury, "NDB");
  show_ndb->setSize(44,24);
  show_ndb->setCallback(show_cb);
  show_ndb->setValue(1);
  show_ils = new puButton(curx+92, cury, "ILS");
  show_ils->setSize(44,24);
  show_ils->setCallback(show_cb);
  show_ils->setValue(1);
  show_fix = new puButton(curx+138, cury, "FIX");
  show_fix->setSize(44,24);
  show_fix->setCallback(show_cb);
  show_fix->setValue(0);

  cury+=25;

  show_nav = new puButton(curx, cury, "Show Navaids");
  show_nav->setSize(185, 24);
  show_nav->setCallback(show_cb);
  show_nav->setValue(1);

  cury+=25;

  show_arp = new puButton(curx, cury, "Show Airports");
  show_arp->setSize(185, 24);
  show_arp->setCallback(show_cb);
  show_arp->setValue(1);

  cury+=35;
  
  labeling = new puText(curx,cury+20);
  labeling->setLabel("Labeling:");
  show_name = new puButton(curx, cury, "Name");
  show_id   = new puButton(curx+95, cury, "Id");
  show_name ->setSize(90, 24);
  show_id   ->setSize(90, 24);
  show_name ->setCallback(show_cb);
  show_id   ->setCallback(show_cb);
  show_name->setValue(1);

  cury+=55;

  txt_lat = new puText(curx, cury+70);
  txt_lat->setLabel("Latitude:");
  txt_lon = new puText(curx, cury+20);
  txt_lon->setLabel("Longitude:");
  inp_lat = new puInput(curx, cury+50, curx+185, cury+74);
  inp_lon = new puInput(curx, cury, curx+185, cury+24);
  inp_lat->setValue(lat_str);
  inp_lon->setValue(lon_str);
  inp_lat->setCallback(position_cb);
  inp_lon->setCallback(position_cb);
  inp_lat->setStyle(PUSTYLE_BEVELLED);
  inp_lon->setStyle(PUSTYLE_BEVELLED);

  cury+=104;
  if (tracks.size() > 0) {
    show_ftrack  = new puButton(curx, cury, "Show Flight Track");
    clear_ftrack = new puOneShot(curx, cury+25, "Clear Flight Track");
    show_ftrack  -> setSize(185, 24);
    clear_ftrack -> setSize(185, 24);
    show_ftrack  -> setValue(1);
    show_ftrack  -> setCallback( show_cb );
    clear_ftrack -> setCallback( clear_ftrack_cb );
  }

  cury+=60;
  choose_projection_button = new puOneShot(curx, cury, "Change Projection");
  choose_projection_button->setSize(182,24);
  choose_projection_button->setCallback(projection_cb);
   
  cury = puysiz - 10;

  minimize_button = new puOneShot(curx+185-20, cury-24, "X");
  minimize_button->setSize(20, 24);
  minimize_button->setCallback(minimize_cb);

  main_interface->close();
  main_interface->reveal();

  minimized = new puPopup(20, 20);
  minimized_button = new puOneShot(0, 0, "X");
  minimized_button->setCallback(restore_cb);
  minimized->close();

  //////////////////////////////////////////////////////////////////////
  //
  // Info Interface
  //
  // This gives information about the current aircraft position, and
  // is also used to manage aircraft flight tracks.
  info_interface = new puPopup(260, 20);
  info_frame = new puFrame(0, 0, 500, 90);
  txt_info_spd = new puText(5, 0);
  txt_info_hdg = new puText(5, 15);
  txt_info_alt = new puText(5, 30);
  txt_info_lon = new puText(5, 45);
  txt_info_lat = new puText(5, 60);

  // EYE - Perhaps we should get these from Graphs.hxx (if not the
  // actual text, at least the number).
  button_box_labels[0] = "Altitude";
  button_box_labels[1] = "Speed";
  button_box_labels[2] = "Rate of Climb";
  button_box_info = 
      new puButtonBox(180, 0, 355, 90, (char **)button_box_labels, FALSE);
  button_box_info->setCallback(graph_type_cb);

  smoother = new puSlider(360, 10, 130);
  smoother->setLabelPlace(PUPLACE_TOP_CENTERED);
  smoother->setLegendFont(*font);
  smoother->setLabelFont(*font);
  smoother->setLabel("Smoothing (s)");
  smoother->setMinValue(0.0);	// 0.0 = no smoothing
  smoother->setMaxValue(60.0);	// 60.0 = smooth over a 60s interval
  smoother->setStepSize(1.0);
  smoother->setCallback(smoother_cb);

  info_interface->close();

  //////////////////////////////////////////////////////////////////////
  //
  // Tile Synchronization Interface
  //
  // This interface is displayed when downloading tiles and generating
  // maps.  It shows the progress of the downloading and generation
  // processes.
  sync_interface = new puPopup(20, 450);
  sync_frame = new puFrame(0, 0, 300, 80);
  // Stick a progress meter in the upper right corner.
  dial_sync_progress = new puDial(265, 45, 30);
  dial_sync_progress->greyOut();
  // Frame text is: name, phase, x/y files, x/y bytes
  txt_sync_name = new puText(5, 50);
  txt_sync_phase = new puText(5, 35);
  txt_sync_files = new puText(5, 20);
  txt_sync_bytes = new puText(5, 5);
  sync_interface->close();

  //////////////////////////////////////////////////////////////////////
  //
  // Search Interface
  //
  // The search interface is used to search for airports and navaids.

  // The initial location doesn't matter, as we'll later ensure it
  // appears in the upper right corner.
  search_interface = new Search(0, 0, 300, 300);
  search_interface->setCallback(searchItemSelected);
  search_interface->setSelectCallback(searchItemSelected);
  search_interface->setInputCallback(searchStringChanged);
  search_interface->setSizeCallback(noOfMatches);
  search_interface->setDataCallback(matchAtIndex);
  // If we're using texture-based fonts, use a smaller font for the
  // list so that we can see more results.
  if (textureFonts) {
      // EYE - Is it bad to overwrite the old 'font' variable?
      font = new puFont(texfont, 12.0f);
      search_interface->setFont(*font);
  }
  search_interface->hide();

  if (prefs.softcursor) {
    puShowCursor();
  }

  choose_projection_menu = new puPopupMenu(260, 150);
  
  for (int i=0; i<map_object->getNumProjections(); i++) {
	proj_item[i]=choose_projection_menu->add_item(map_object->getProjectionNameByID(i), projection_cb);
  }	
  choose_projection_menu->close();
	
}
/******************************************************************************
 GLUT event handlers
******************************************************************************/
void reshapeMap( int _width, int _height ) {
  width  = (float)_width  ;
  height = (float)_height ;
  mapsize = (width > height) ? width : height;

  // Ensure that the 'jump to location' widget stays in the upper
  // right corner.
  int w, h;
  search_interface->getSize(&w, &h);
  search_interface->setPosition(width - w, height - h);

  map_object->setSize( mapsize );
}

// EYE - BJS - when an object's centre moves off the map, we don't
// draw the object, in spite of the fact that parts of the object
// should still be visible.  Another manifestation of this is that
// when you zoom in on an airport, it will suddenly disappear at high
// enough zoom levels.  This should be fixed.
void redrawMap() {
  char buf[256];
  
  if (track) {
      sprintf(buf, "Atlas - %s", graphs->name());
      glutSetWindowTitle(buf);
  } else {
      glutSetWindowTitle("Atlas");
  }

  glClearColor( 0.643f, 0.714f, 0.722f, 0.0f );
  glClear( GL_COLOR_BUFFER_BIT );

  /* Fix so that center of map is actually the center of the window.
     This should probably be taken care of in OutputGL... */
  glPushMatrix();
  if (width > height) {
    glTranslatef( 0.0f, -(width - height) / 2.0f, 0.0f );
  } else {
    glTranslatef( -(height - width) / 2.0f, 0.0f, 0.0f );
  }

  map_object->draw();

  glPushMatrix();
  glTranslatef( mapsize/2, mapsize/2, 0.0f );
  glColor3f( 1.0f, 0.0f, 0.0f );

  if (!track) {
    // If there is no current track, then draw a crosshair.
    glBegin(GL_LINES);
    glVertex2f(0.0f, 20.0f);
    glVertex2f(0.0f, -20.0f);
    glVertex2f(20.0f, 0.0f);
    glVertex2f(-20.0f, 0.0f);
    glEnd(); 
  }
  glPopMatrix();
   
  if (!inp_lat->isAcceptingInput()) {
    sprintf( lat_str, "%c%s", 
	     (latitude<0)?'S':'N', 
	     dmshh_format(latitude * SG_RADIANS_TO_DEGREES, buf) );
    inp_lat->setValue(lat_str);
  }

  if (!inp_lon->isAcceptingInput()) {
    sprintf( lon_str, "%c%s", 
	     (longitude<0)?'W':'E', 
	     dmshh_format(longitude * SG_RADIANS_TO_DEGREES, buf) );
    inp_lon->setValue(lon_str);
  }

  // EYE - is track always valid?
  if (track) {
      FlightData *p = track->getCurrentPoint();

      if (p) {
	  if (track->isAtlas()) {
	      sprintf(hdg_str, "HDG: %.0f* true", 
		      p->hdg < 0.0 ? p->hdg + 360.0 : p->hdg);    
	      sprintf(spd_str, "SPD: %.0f kt EAS", p->spd);
	  } else {
	      sprintf(hdg_str, "TRK: %.0f* true", 
		      p->hdg < 0.0 ? p->hdg + 360.0 : p->hdg);    
	      sprintf(spd_str, "SPD: %.0f kt GS", p->spd);
	  }
	  sprintf(alt_str, "ALT: %.0f ft MSL", p->alt);
	  sprintf(lat_str, "%c%s", 
		  (p->lat < 0) ? 'S':'N',
		  dmshh_format(p->lat * SG_RADIANS_TO_DEGREES, buf));
	  sprintf(lon_str, "%c%s", 
		  (p->lon < 0) ? 'W':'E', 
		  dmshh_format(p->lon * SG_RADIANS_TO_DEGREES, buf));

	  txt_info_lat->setLabel(lat_str);
	  txt_info_lon->setLabel(lon_str);
	  txt_info_alt->setLabel(alt_str);
	  txt_info_hdg->setLabel(hdg_str);
	  txt_info_spd->setLabel(spd_str);
      }
  }

  // Remove our translation
  glPopMatrix();

  puDisplay();
  /* I have no idea why I suddenly need to set the viewport here -
     I think this might be a pui bug, since I didn't have to do this
     before some plib update. Commenting puDisplay out makes it unnessecary. */
  glViewport(0, 0, (int)mapsize, (int)mapsize);
  glutSwapBuffers();
}

// Display function for the graphs window.
void redrawGraphs() {
    if (track) {
	graphs->draw();
	glutSwapBuffers();
    }
}

// Called when the graphs window is resized.
void reshapeGraphs(int w, int h)
{
    glViewport(0, 0, (GLsizei) w, (GLsizei) h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, (GLdouble) w, 0.0, (GLdouble) h);
}

// Called for mouse motion events (when a mouse button is depressed)
// in the graphs window.
void motionGraphs(int x, int y) 
{
    // Marks are only honoured for non-live tracks.
    if (!track->live()) {
	graphs->setMark(x);

	glutSetWindow(main_window);
	if (prefs.autocenter_mode) {
	    centerMapOnAircraft();
	}
	glutPostRedisplay();

	glutSetWindow(graphs_window);
	glutPostRedisplay();
    }
}

// Called for mouse button events in the graphs window.
void mouseGraphs(int button, int state, int x, int y)
{
    // EYE - should we look at the button and state eventually?
    motionGraphs(x, y);
}

void keyPressed( unsigned char key, int x, int y );

// Called when the user presses a key in the graphs window.  We just
// pass the key on to the handler for the main window.
void keyboardGraphs(unsigned char key, int x, int y)
{
    // EYE - keyPressed does a call to puKeyboard.  Is this okay
    // (especially if we do the same in the future)?
    glutSetWindow(main_window);
    keyPressed(key, x, y);
    glutSetWindow(graphs_window);
}

// Called when the user presses a "special" key in the graphs window,
// where "special" includes directional keys.
void specialGraphs(int key, int x, int y) 
{
    // The special keys all adjust the mark.  However, live tracks
    // have no mark.
    if (track->live()) {
	return;
    }

    int offset = 1;
    if (glutGetModifiers() & GLUT_ACTIVE_SHIFT) {
	// If the user presses the shift key, right and left arrow
	// clicks move 10 times as far.
	offset *= 10;
    }

    switch (key + PU_KEY_GLUT_SPECIAL_OFFSET) {
    case PU_KEY_LEFT:
	if (track->mark() >= offset) {
	    track->setMark(track->mark() - offset);
	} else {
	    track->setMark(0);
	}
	break;
    case PU_KEY_RIGHT:
	if (track->mark() < (track->size() - offset)) {
	    track->setMark(track->mark() + offset);
	} else {
	    track->setMark(track->size() - 1);
	}
	break;
    case PU_KEY_HOME:
	track->setMark(0);
	break;
    case PU_KEY_END:
	track->setMark(track->size() - 1);
	break;
    default:
	return;
    }

    glutSetWindow(main_window);
    if (prefs.autocenter_mode) {
	centerMapOnAircraft();
    }
    glutPostRedisplay();

    glutSetWindow(graphs_window);
    glutPostRedisplay();
}

// Called periodically to check for input on network and serial ports.
void timer(int value) {
    // Ensure that the main window is the current window.
    glutSetWindow(main_window);

    // Check for input on all live tracks.
    for (int i = 0; i < tracks.size(); i++) {
	if (tracks[i]->live() && tracks[i]->checkForInput()) {
	    // If we're in terrasync mode, we need to check for
	    // unloaded tiles as the aircraft moves.
	    if (prefs.terrasync_mode) {
		terrasyncUpdate();
	    }

	    // If we're in auto-center mode, and this track is the
	    // currently displayed track, then recenter the map.
	    if (prefs.autocenter_mode && (tracks[i] == track)) {
		centerMapOnAircraft();
	    }

	    // EYE - be consistent! i == currentFlightTrack or
	    // tracks[i] == track?
	    // And update our graphs if we're displaying this track.
	    if (i == currentFlightTrack) {
		glutSetWindow(graphs_window);
		glutPostRedisplay();
	    }

	    // Just in case the main window has changed.
	    glutSetWindow(main_window);
	    glutPostRedisplay();
	}
    }

    // Check again later.
    glutTimerFunc( (int)(prefs.update * 1000.0f), timer, value );
}

// Called to monitor syncing tiles.  It monitors the currently
// downloading tile(s), and updates the interface.
// EYE - mark syncing tiles somehow
// EYE - it would be nice to use a smaller font size.  Also, characters
//       get clipped with our current font and spacing.
void tileTimer(int value) {
    int i;
    int concurrency;
    Tile *t;

    // Ensure that the main window is the current window.
    glutSetWindow(main_window);

    // Get our concurrency level.  0 indicates maximum concurrency.
    concurrency = prefs.concurrency;
    if (concurrency == 0) {
	concurrency = tileManager->noOfTiles();
    }

    // Do some work on each tile were concurrently updating.
    for (i = 0; i < concurrency; i++) {
	t = tileManager->nthTile(i);
	if (t) {
	    t->doSomeWork();
	}
    }

    // Remove any completed tiles.
    for (i = concurrency - 1; i >= 0; i--) {
	t = tileManager->nthTile(i);
	if (t && (t->currentTask() == Tile::NO_TASK)) {
	    tileManager->removeTile(t);

	    // As we delete tiles, the one we're currently displaying
	    // information on (given by nthTile) may shift its
	    // position, or even disappear.
	    if (nthTile > i) {
		nthTile--;
	    }
	    if ((nthTile >= tileManager->noOfTiles()) && (nthTile > 0)) {
		nthTile--;
	    }
	}
    }

    // We might have more work to do.
    if (tileManager->noOfTiles() > 0) {
	// Update the interface, and schedule another bit of work.
 	updateSyncInterface();
	glutTimerFunc(100, tileTimer, value);
    } else {
	// Done.  Hide the interface.  Don't schedule more updates.
	sync_interface->hide();
    }

    // This is necessary so that Atlas draws any newly-created maps.

    // EYE - Atlas will not redraw a map if it already has one in its
    // cache.  Thus, for example, a new, higher-definition map will
    // not be displayed immediately.
    map_object->setLocation(latitude, longitude);
    glutPostRedisplay();
}

// Called periodically so the tile manager can scan our scenery
// directories and see if there's any scenery that needs a map or two
// generated.
void tileManagerTimer(int value) {
    // If we're not currently doing anything, check if there's scenery
    // that needs to be rendered (ie, if the user installed some
    // scenery through some non-Atlas means).
    if (tileManager->noOfTiles() == 0) {
	tileManager->checkScenery();

	// If there are maps to be made, schedule them to be made.
	if (tileManager->noOfTiles() > 0) {
	    glutTimerFunc(0, tileTimer, value);
	}
    }

    // Check again in 60 seconds.
    glutTimerFunc(1000 * 60, tileManagerTimer, value);
}

void mouseClick( int button, int state, int x, int y ) {
    if ( !puMouse( button, state, x, y ) ) {
	// PUI didn't consume this event
	if (button == GLUT_LEFT_BUTTON) {
	    switch (state) {
	    case GLUT_DOWN:
		dragmode = true;
		drag_x = x;
		drag_y = y;
		copy_lat = latitude;
		copy_lon = longitude;

		// EYE - huh?
		// If we don't do this and some widget is currently active,
		// subsequent mouse moves will be swallowed by PUI.
		// 	puSetActiveWidget(NULL, 0, 0);
		break;
	    default:
		dragmode = false;
	    }
	} else {
	    dragmode = false;
	}
    } else {
	glutPostRedisplay();
    }
}


// BJS - Here's what we need to rewrite (I think) to get rid of the
// "mouse move mistranslation at large scales and extreme latitudes"
// bug.
void mouseMotion( int x, int y ) {
    // While in drag mode, we take complete control.  Only if we're
    // not dragging do we let PUI take a look at the event.
    if (dragmode) {
	latitude  = (copy_lat + (float)(y - drag_y)*scalefactor / 
		     (float)mapsize * SG_DEGREES_TO_RADIANS);
	longitude = (copy_lon + (float)(drag_x - x)*scalefactor / 
		     (float)mapsize * SG_DEGREES_TO_RADIANS);
	while ( longitude > 180.0f * SG_DEGREES_TO_RADIANS )
	    longitude -= (360.0f * SG_DEGREES_TO_RADIANS);
	while ( longitude < -180.0f * SG_DEGREES_TO_RADIANS )
	    longitude += (360.0f * SG_DEGREES_TO_RADIANS);
	map_object->setLocation( latitude, longitude );

	glutPostRedisplay();
    } else {
	// EYE - which policy is correct?
// 	puMouse(x, y);
	if (puMouse(x, y)) {
	    puDisplay();
	}
    }

    // EYE - This call meant the map was redrawn even when the user
    // just moved the mouse.  Presumably it's safe to move it into the
    // "if (dragmode)" section above.  Right?
//     glutPostRedisplay();
}

void keyPressed( unsigned char key, int x, int y ) {
    if (!puKeyboard(key, PU_DOWN)) {
	switch (key) {
	case '+':
	    zoom_cb(zoomin);
	    break;
	case '-':
	    zoom_cb(zoomout);
	    break;
	case 'A':
	case 'a':
	    show_arp->setValue(!show_arp->getValue());
	    show_cb(show_arp);
	    break;
	case 'C':
	    // Toggle auto-centering.
	    prefs.autocenter_mode = !prefs.autocenter_mode;
	    if (prefs.autocenter_mode) {
		centerMapOnAircraft();
	    }
	    break;
	case 'c':
	    centerMapOnAircraft();
	    break;
	case 'D':
	case 'd':
	    // Hide/show the info interface and the graphs window.
	    if (tracks.size() > 0) {
		if (!info_interface->isVisible()) {
		    glutSetWindow(graphs_window);
		    glutShowWindow();

		    glutSetWindow(main_window);
		    info_interface->reveal();

		    showGraphWindow = true;
		} else {
		    glutSetWindow(graphs_window);
		    glutHideWindow();

		    glutSetWindow(main_window);
		    info_interface->hide();

		    showGraphWindow = false;
		}
		glutPostRedisplay();
	    }
	    break;
	case 'F':
	case 'f':
	    // Select the next ('f') or previous ('F') flight track.

	    // If there's an active track dialog, then don't do anything.
	    // EYE - beep?
	    if (fileDialog != NULL) {
		return;
	    }

	    // If there are no tracks, don't do anything.
	    if (tracks.size() == 0) {
		return;
	    }

	    if (key == 'f') {
		setFlightTrack((currentFlightTrack + 1) % tracks.size());
	    } else {
		setFlightTrack((currentFlightTrack + tracks.size() - 1) 
			       % tracks.size());
	    }
	    break;
	case 'J':
	case 'j':
	    // Toggle the search interface.
	    if (search_interface->isVisible()) {
		search_interface->hide();
	    } else {
		search_interface->reveal();
	    }
	    glutPostRedisplay();
	    break;
	case 'L':
	    // Show the next downloading tile.
	    nextTile();
	    break;
	case 'l':
	    // Schedule or deschedule the 1x1 tile at our current lat/lon
	    // for updating.
	    toggleTile(latitude * SG_RADIANS_TO_DEGREES,
		       longitude * SG_RADIANS_TO_DEGREES);
	    break;
	case 'N':
	case 'n':
	    show_nav->setValue(!show_nav->getValue());
	    show_cb(show_nav);
	    break;    
	case 'O':
	case 'o':
	    // Open a flight file (unless the file dialog is already
	    // active doing something else).
	    if (fileDialog == NULL) {
		fileDialog = new puaFileSelector(250, 150, 500, 400, "",
						 "Open Flight Track");
		fileDialog->setCallback(load_file_cb);
		glutPostRedisplay();
	    }
	    break;
	case 'S':
	case 's':
	    // We should warn the user if Atlas quits with unsaved tracks.
	    // However, I don't think GLUT gives us a way to catch program
	    // exits.  Let the user beware!
	    // EYE - maybe we should add a 'q' command which will
	    // check for unsaved tracks.
	    if (!track) {
		break;
	    }
	    if (track->hasFile()) {
		track->save();
		glutPostWindowRedisplay(graphs_window);
	    } else if (fileDialog == NULL) {
		fileDialog = 
		    new puaFileSelector(250, 150, 500, 400, "",
					"Save Flight Track");
		fileDialog->setCallback(save_file_cb);
		glutPostRedisplay();
	    }
	    break;
	case 'T':
	case 't':
	    map_object->setTextured( !map_object->getTextured() );
	    glutPostRedisplay();
	    break;
	case 'W':
	case 'w':
	    // EYE - we should warn the user if the track is unsaved.
	    // Close the current track.
	    if (track) {
		tracks.erase(tracks.begin() + currentFlightTrack);
		delete track;

		// If we still have some tracks, make the next track the
		// current track.
		if (tracks.size() > 0) {
		    setFlightTrack(currentFlightTrack % tracks.size());
		} else {
		    setFlightTrack(-1);
		}
	    }
	    break;
	case 'U':
	case 'u':
	    // 'u'nattach (ie, detach)
	    if (track && track->live()) {
		// If we detach a track, we replace it by a new track
		// listening to the same I/O channel.
		if (track->isNetwork()) {
		    int port = track->port();
		    unsigned int maxSize = track->maxBufferSize();

		    // Detach the old track.
		    track->detach();
		    track->setMark(0);

		    // Create a replacement.
		    FlightTrack* newTrack = new FlightTrack(port, maxSize);
		    tracks.push_back(newTrack);
		} else if (track->isSerial()) {
		    const char *device = track->device();
		    int baud = track->baud();
		    unsigned int maxSize = track->maxBufferSize();

		    // Detach the old track.
		    track->detach();
		    track->setMark(0);

		    // Create a replacement.
		    FlightTrack* newTrack = new FlightTrack(device, baud, maxSize);
		    tracks.push_back(newTrack);
		} else {
		    assert(false);
		}

		glutPostWindowRedisplay(main_window);
		glutPostWindowRedisplay(graphs_window);
	    }
	    break;
	case 'V':
	case 'v':
	    show_name->setValue(!show_name->getValue());
	    show_cb(show_name);
	    break;
	case ' ':
	    if (!main_interface->isVisible()) {
		main_interface->reveal();
		minimized->hide();
	    } else {
		main_interface->hide();
		minimized->hide();
	    }
	    glutPostRedisplay();
	}
    } else {
	// EYE - really?
	glutPostRedisplay();
    }
}

void specPressed(int key, int x, int y) {
  if (puKeyboard(key + PU_KEY_GLUT_SPECIAL_OFFSET, PU_DOWN )) {
    glutPostRedisplay();
  }
}

int main(int argc, char **argv) {
  glutInit( &argc, argv );

  // Load our preferences.  If there's a problem with any of the
  // arguments, it will print some errors to stderr, and return false.
  if (!prefs.loadPreferences(argc, argv)) {
      exit(1);
  }

  // A bit of post-preference processing.
  if (access(prefs.path.c_str(), F_OK)==-1) {
      printf("\nWarning: path %s doesn't exist. Maps won't be loaded!\n", 
	     prefs.path.c_str());
  } else {
      lowrespath.set(prefs.path.str());
      lowrespath.append("/lowres");
      if (access(lowrespath.c_str(), F_OK) == -1) {
	  printf("\nWarning: path %s doesn't exist. Low resolution maps won't be loaded\n", lowrespath.c_str());
	  lowres_avlble = 0;

	  // Since there's no lowres directory, tell the tile manager
	  // that we don't want lowres maps (regardless of what the
	  // user actually asked for).
	  prefs.lowres_map_size = 0;
      } else {
	  lowres_avlble = 1;
      }
  }

  latitude  = prefs.latitude * SG_DEGREES_TO_RADIANS;
  longitude = prefs.longitude * SG_DEGREES_TO_RADIANS;

  glutInitDisplayMode( GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE );
  glutInitWindowSize( prefs.width, prefs.height );
  main_window = glutCreateWindow( "Atlas" );

  glutReshapeFunc( reshapeMap );
  glutDisplayFunc( redrawMap );
  glutMotionFunc ( mouseMotion );
  glutPassiveMotionFunc( mouseMotion );
  glutMouseFunc  ( mouseClick  );
  glutKeyboardFunc( keyPressed  );
  glutSpecialFunc( specPressed );

  mapsize = (float)( (prefs.width>prefs.height)?prefs.width:prefs.height );
  map_object = new MapBrowser( 0.0f, 0.0f, mapsize, 
                               Overlays::OVERLAY_AIRPORTS  | 
                               Overlays::OVERLAY_NAVAIDS   |
                               Overlays::OVERLAY_NAVAIDS_VOR |
                               Overlays::OVERLAY_NAVAIDS_NDB |
			       Overlays::OVERLAY_NAVAIDS_ILS |
                               //Overlays::OVERLAY_NAVAIDS_FIX |
                               //Overlays::OVERLAY_FIXES     |
                               Overlays::OVERLAY_GRIDLINES | 
                               Overlays::OVERLAY_NAMES     |
			       Overlays::OVERLAY_FLIGHTTRACK,
			       prefs.fg_root.str().length() == 0 ? NULL : prefs.fg_root.c_str(), 
                               prefs.mode,
			       prefs.textureFonts );
  map_object->setTextured(true);
  map_object->setMapPath(prefs.path.c_str());


  // Read in files.
  for (int i = 0; i < prefs.flightFiles.size(); i++) {
      // Read in the whole file in one swack.  Files that couldn't be
      // opened are added to a list, then reported.
      FlightTrack *aTrack;
      try {
	  aTrack = new FlightTrack(prefs.flightFiles[i].c_str());
	  // Set the mark aircraft to the beginning of the track.
	  aTrack->setMark(0);
	  tracks.push_back(aTrack);
      } catch (runtime_error e) {
	  printf("Failed to read flight file '%s'\n", 
		 prefs.flightFiles[i].c_str());
      }
  }

  // Make network connections.
  for (int i = 0; i < prefs.networkConnections.size(); i++) {
      tracks.push_back(new FlightTrack(prefs.networkConnections[i],
				       prefs.max_track));
  }
  // Make serial connections.
  for (int i = 0; i < prefs.serialConnections.size(); i++) {
      FlightTrack *f = 
	  new FlightTrack(prefs.serialConnections[i].device,
			  prefs.serialConnections[i].baud,
			  prefs.max_track);
      tracks.push_back(f);
  }

  // Check network connections and serial connections periodically (as
  // specified by the "update" user preference).
  if ((prefs.networkConnections.size() + prefs.serialConnections.size()) > 0) {
      glutTimerFunc((int)(prefs.update * 1000.0f), timer, 0);
  }

  printf("Please wait while loading databases ... "); fflush(stdout);
  map_object->loadDb();
  printf("done.\n");
  
  if(strlen(prefs.icao) != 0) {
    Overlays::ARP* apt = map_object->getOverlays()->findAirportByCode(prefs.icao);
    if(apt) {
      latitude = apt->lat;
      longitude = apt->lon;
    } else {
      printf("Unable to find airport %s.\n", prefs.icao);
    }
  }
  
  init_gui(prefs.textureFonts);

  // Create a tile manager.  In its creator it will see which scenery
  // directories we have, and whether there are maps generated for
  // those directories.
  printf("Please wait while checking existing scenery ... "); fflush(stdout);
  tileManager = new TileManager(prefs);
  glutTimerFunc(0, tileManagerTimer, 0);
  printf("done.\n");

  // Create the graphs window, placed below the main.  First, get the
  // position of the first window.  We must do this now, because the
  // glutGet call works on the current window.
  int x, y, h;
  x = glutGet(GLUT_WINDOW_X);
  y = glutGet(GLUT_WINDOW_Y);
  h = glutGet(GLUT_WINDOW_HEIGHT);

  graphs_window = glutCreateWindow("-- graphs --");
  glutDisplayFunc(redrawGraphs);
  glutReshapeFunc(reshapeGraphs);
  glutMotionFunc(motionGraphs);
  glutMouseFunc(mouseGraphs);
  glutKeyboardFunc(keyboardGraphs);
  glutSpecialFunc(specialGraphs);

  // EYE - add keyboard function: space (play in real time, pause)

  glutReshapeWindow(800, 200);
  // EYE - this kind of works, but neglects the border OS X adds
  // around the window (and perhaps other effects too), so there
  // is some overlap.
  glutPositionWindow(x, y + h);

  graphs = new Graphs(graphs_window);
  graphs->setAircraftColor(map_object->getOverlays()->aircraftColor());
  graphs->setMarkColor(map_object->getOverlays()->aircraftMarkColor());

  // EYE - this counts on the Graphs type values being same as the
  // corresponding entries in the button box.  As well, is there a
  // cleaner way to do this?  Is there a way to force the call to the
  // callback without calling it explicitly?
  button_box_info->setValue(Graphs::ALTITUDE | 
			    Graphs::SPEED | 
			    Graphs::CLIMB_RATE);
  graphs->setGraphTypes(button_box_info->getValue());
  smoother->setValue((int)graphs->smoothing());
      
  // EYE - I wonder if all this setup stuff should be done in a
  // visibility callback?  (see glutVisibilityFunc()).
  glutSetWindow(main_window);
  if (tracks.size() > 0) {
      // If we've loaded some tracks, display the first one.
      setFlightTrack(0);
  } else {
      // Otherwise, display nothing and set our latitude and longitude
      // to the default values.
      setFlightTrack(-1);
      map_object->setLocation(latitude, longitude);
  }

  glutMainLoop();
 
  return 0;
}
