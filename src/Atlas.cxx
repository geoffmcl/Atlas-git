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
#include SG_GLUT_H
#include <plib/fnt.h>
#include <plib/pu.h>
#include <string>
#include <simgear/io/sg_socket.hxx>
#include <simgear/io/sg_serial.hxx>

#include <map>

#include "MapBrowser.hxx"
#include "Overlays.hxx"
#include "FlightTrack.hxx"
#include "Tile.hxx"
#include "TileManager.hxx"
#include "Search.hxx"
#include "Preferences.hxx"

#define SCALECHANGEFACTOR 1.3f

// User preferences (including command-line arguments).
Preferences prefs;

SGIOChannel *input_channel;

bool dragmode = false;
int drag_x, drag_y;
float scalefactor = 1.0f, mapsize, width, height;

float latitude, copy_lat;
float longitude, copy_lon;

float heading = 0.0f, speed, altitude;

int  sock;
char save_buf[ 2 * 2048 ];
int save_len = 0;

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

// Synchronization and map generation interface.
puPopup *sync_interface;
puFrame *sync_frame;
puText *txt_sync_name, *txt_sync_phase, *txt_sync_files, *txt_sync_bytes;
puDial *dial_sync_progress;

// Search interface.
Search *search_interface;

// bool softcursor = false;
char lat_str[80], lon_str[80], alt_str[80], hdg_str[80], spd_str[80];

SGPath lowrespath;
int lowres_avlble;

MapBrowser *map_object;
FlightTrack *track = NULL;

// The tile manager keeps track of tiles that need to be updated.
TileManager *tileManager;

// Keeps track of which downloading tile is being displayed.  0
// represents the first tile.
unsigned int nthTile = 0;

// SGIOChannel *ai_aircraft;
FlightTrack *ai_track = NULL;

bool parse_nmea(char *buf) {
  //  cout << "parsing nmea message = " << buf << endl;

    string msg = buf;
    //msg = msg.substr( 0, length );

    string::size_type begin_line, end_line, begin, end;
    begin_line = begin = 0;

    // extract out each line
    end_line = msg.find("\n", begin_line);
    while ( end_line != string::npos ) {
	string line = msg.substr(begin_line, end_line - begin_line);
	begin_line = end_line + 1;

	// leading character
	string start = msg.substr(begin, 1);
	++begin;

	// sentence
	end = msg.find(",", begin);
	if ( end == string::npos ) {
	    return false;
	}
    
	string sentence = msg.substr(begin, end - begin);
	begin = end + 1;

	double lon_deg, lon_min, lat_deg, lat_min;

	if ( sentence == "GPRMC" ) {
	    // time
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string utc = msg.substr(begin, end - begin);
	    begin = end + 1;

	    // junk
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }

	    string junk = msg.substr(begin, end - begin);
	    begin = end + 1;

	    // latitude val
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string lat_str = msg.substr(begin, end - begin);
	    begin = end + 1;

	    lat_deg = atof( lat_str.substr(0, 2).c_str() );
	    lat_min = atof( lat_str.substr(2).c_str() );

	    // latitude dir
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string lat_dir = msg.substr(begin, end - begin);
	    begin = end + 1;

	    latitude = lat_deg + ( lat_min / 60.0 );
	    if ( lat_dir == "S" ) {
		latitude *= -1;
	    }
	    latitude *= SG_DEGREES_TO_RADIANS;  // convert to radians

	    // longitude val
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string lon_str = msg.substr(begin, end - begin);
	    begin = end + 1;

	    lon_deg = atof( lon_str.substr(0, 3).c_str() );
	    lon_min = atof( lon_str.substr(3).c_str() );

	    // longitude dir
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string lon_dir = msg.substr(begin, end - begin);
	    begin = end + 1;

	    longitude = lon_deg + ( lon_min / 60.0 );
	    if ( lon_dir == "W" ) {
		longitude *= -1;
	    }
	    longitude *= SG_DEGREES_TO_RADIANS;  // convert to radians

	    // speed
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string speed_str = msg.substr(begin, end - begin);
	    begin = end + 1;
	    speed = atof( speed_str.c_str() );

	    // heading
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string hdg_str = msg.substr(begin, end - begin);
	    begin = end + 1;
	    heading = atof( hdg_str.c_str() );
	} else if ( sentence == "GPGGA" ) {
	    // time
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string utc = msg.substr(begin, end - begin);
	    begin = end + 1;

	    // latitude val
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string lat_str = msg.substr(begin, end - begin);
	    begin = end + 1;

	    lat_deg = atof( lat_str.substr(0, 2).c_str() );
	    lat_min = atof( lat_str.substr(2).c_str() );

	    // latitude dir
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string lat_dir = msg.substr(begin, end - begin);
	    begin = end + 1;

	    latitude = lat_deg + ( lat_min / 60.0 );
	    if ( lat_dir == "S" ) {
		latitude *= -1;
	    }
	    latitude *= SG_DEGREES_TO_RADIANS;  // convert to radians

	    // cur_fdm_state->set_Latitude( latitude * SG_DEGREES_TO_RADIANS );

	    // longitude val
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string lon_str = msg.substr(begin, end - begin);
	    begin = end + 1;

	    lon_deg = atof( lon_str.substr(0, 3).c_str() );
	    lon_min = atof( lon_str.substr(3).c_str() );

	    // longitude dir
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string lon_dir = msg.substr(begin, end - begin);
	    begin = end + 1;

	    longitude = lon_deg + ( lon_min / 60.0 );
	    if ( lon_dir == "W" ) {
		longitude *= -1;
	    }
	    longitude *= SG_DEGREES_TO_RADIANS;  // convert to radians

	    // cur_fdm_state->set_Longitude( longitude * SG_DEGREES_TO_RADIANS );

	    // junk
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string junk = msg.substr(begin, end - begin);
	    begin = end + 1;

	    // junk
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    junk = msg.substr(begin, end - begin);
	    begin = end + 1;

	    // junk
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    junk = msg.substr(begin, end - begin);
	    begin = end + 1;

	    // altitude
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string alt_str = msg.substr(begin, end - begin);
	    altitude = atof( alt_str.c_str() );
	    begin = end + 1;
	    
	    // altitude units
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string alt_units = msg.substr(begin, end - begin);
	    begin = end + 1;

	    if ( alt_units != string("F") ) {
		altitude *= 3.28;
	    }
	} else if ( sentence == "PATLA" ) {
	    // nav1 freq
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string nav1_freq_str = msg.substr(begin, end - begin);
	    begin = end + 1;

	    // nav1 selected radial
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string nav1_rad_str = msg.substr(begin, end - begin);
	    begin = end + 1;

	    // nav2 freq
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string nav2_freq_str = msg.substr(begin, end - begin);
	    begin = end + 1;

	    // nav2 selected radial
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string nav2_rad_str = msg.substr(begin, end - begin);
	    begin = end + 1;

	    // adf freq
	    end = msg.find("*", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string adf_freq_str = msg.substr(begin, end - begin);
	    begin = end + 1;

	    nav1_freq = atof( nav1_freq_str.c_str() );
	    nav1_rad =  atof( nav1_rad_str.c_str() ) * 
	      SGD_DEGREES_TO_RADIANS;
	    nav2_freq = atof( nav2_freq_str.c_str() );
	    nav2_rad =  atof( nav2_rad_str.c_str() ) * 
	      SGD_DEGREES_TO_RADIANS;
	    adf_freq =  atof( adf_freq_str.c_str() );
	}

	begin = begin_line;
	end_line = msg.find("\n", begin_line);

  }
  
  return true;
}

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
	// Show the new items.
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
			 n->id, cleanString(n->name), n->freq);
		break;
	    case Overlays::NAV_DME:
		asprintf(&result, "DME: %s %s (%.2f)", 
			 n->id, cleanString(n->name), n->freq);
		break;
	    case Overlays::NAV_NDB:
		asprintf(&result, "NDB: %s %s (%.0f)", 
			 n->id, cleanString(n->name), n->freq);
		break;
	    case Overlays::NAV_ILS:
		asprintf(&result, "ILS: %s %s (%.2f)", 
			 n->id, cleanString(n->name), n->freq);
		break;
	    case Overlays::NAV_FIX:
		// Fixes have no separate id and name (they are identical).
		asprintf(&result, "FIX: %s", n->id);
		break;
	    default:
		assert(true);
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

    // Used to drive the progress meter.
    static int progress = 0;

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
  if (prefs.slaved) {
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

  if (prefs.slaved) {
    info_interface = new puPopup(260, 20);
    info_frame = new puFrame(0, 0, 210, 100);
    txt_info_spd = new puText(10, 10);
    txt_info_hdg = new puText(10, 25);
    txt_info_alt = new puText(10, 40);
    txt_info_lon = new puText(10, 55);
    txt_info_lat = new puText(10, 70);
    info_interface->close();
    info_interface->reveal();
  }

  // Create sync interface (initially hidden).
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

  // Create search interface (initially hidden).  The initial location
  // doesn't matter, as we'll later ensure it appears in the upper
  // right corner.
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

void redrawMap() {
  char buf[256];
  
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

  // BJS - We should add a slaved toggle to the interface, and draw
  // the airplane/crosshairs based on that.
  if (!prefs.slaved) {
  // Draw Crosshair if slaved==false
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.0f, 20.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.0f, -20.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(20.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(-20.0f, 0.0f);
  }
  glEnd(); 
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

  if (prefs.slaved) {
    sprintf( hdg_str, "HDG: %.0f*", heading < 0.0 ? heading + 360.0 : heading);    
    sprintf( alt_str, "ALT: %.0f ft MSL", altitude);
    sprintf( spd_str, "SPD: %.0f KIAS", speed);
    txt_info_lat->setLabel(lat_str);
    txt_info_lon->setLabel(lon_str);
    txt_info_alt->setLabel(alt_str);
    txt_info_hdg->setLabel(hdg_str);
    txt_info_spd->setLabel(spd_str);
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

void timer(int value) {
  char buffer[512];

  int length, totalLength = 0;
  while ( (length = input_channel->readline( buffer, 512 )) > 0 ) {
      parse_nmea(buffer);
      totalLength += length;
  }

  // If we managed to read data, then we'll assume we need to add some
  // flight data.
  if (totalLength > 0) {
      // If we're in terrasync mode, we need to check for unloaded
      // tiles as the aircraft moves.
      if (prefs.terrasync_mode) {
	  terrasyncUpdate();
      }

      // record flight
      FlightData *d = new FlightData;
      d->lat = latitude;
      d->lon = longitude;
      d->alt = altitude;
      d->hdg = heading;
      d->spd = speed;
      track->addPoint(d);

      // EYE - or perhaps Atlas shouldn't follow the aircraft?  Or
      // make it a toggle?  We really need a preferences pane.
//       map_object->setLocation( latitude, longitude );
  
      glutPostRedisplay();
  }

  glutTimerFunc( (int)(prefs.update * 1000.0f), timer, value );
}

// Called to monitor syncing tiles.  It monitors the currently
// downloading tile(s), and updates the interface.
// EYE - mark syncing tiles somehow
// EYE - it would be nice to use a smaller font size, and characters
//       get clipped in font
void tileTimer(int value) {
    int i;
    int concurrency;
    Tile *t;

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

// Get information about "other" aircraft.
// void otherAircraftTimer(int value) {
//     char buf[256];
//     printf("otherAircraftTimer: %d\n", value);

//     int length = 0;
//     while ((length = ai_aircraft->readline(buf, 256)) > 0) {
// 	float lat, lon, alt, hdg, spd;
// 	FlightData *d = new FlightData;

// 	sscanf(buf, "%f,%f,%f,%f,%f", &lat, &lon, &alt, &hdg, &spd);
// 	printf("ai aircraft: %.1f, %.1f, %.1f, %.1f, %.1f\n",
// 	       lat, lon, alt, hdg, spd);

// 	d->lat = lat * SG_DEGREES_TO_RADIANS;
// 	d->lon = lon * SG_DEGREES_TO_RADIANS;
// 	d->alt = alt;
// 	d->hdg = hdg;
// 	d->spd = spd;
// 	ai_track->addPoint(d);
// // 	map_object->setLocation(lat, lon);
// 	glutPostRedisplay();
//     }

//     glutTimerFunc(1000, otherAircraftTimer, 0);
// }

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
    } else
      dragmode = false;
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
    } else {
	puMouse(x, y);
    }

    glutPostRedisplay();
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
    case 'D':
    case 'd':
      if (prefs.slaved) {
	if (!info_interface->isVisible()) {
	  info_interface->reveal();
	} else {
	  info_interface->hide();
	}
	glutPostRedisplay();
      }
      break;
    case 'A':
    case 'a':
      show_arp->setValue(!show_arp->getValue());
      show_cb(show_arp);
      break;
    case 'C':
    case 'c':
      // Center the map on the last position of the aircraft (if it
      // has a non-empty track).
      if (track && !track->empty()) {
	latitude = track->getLastPoint()->lat;
	longitude = track->getLastPoint()->lon;
	map_object->setLocation( latitude, longitude );
	glutPostRedisplay();
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
    case 'T':
    case 't':
      map_object->setTextured( !map_object->getTextured() );
      glutPostRedisplay();
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
  glutCreateWindow( "Atlas" );

  glutReshapeFunc( reshapeMap );
  glutDisplayFunc( redrawMap );

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


  if (prefs.slaved) {
    glutTimerFunc( (int)(prefs.update*1000.0f), timer, 0 );

    track = new FlightTrack(prefs.max_track);
    map_object->setFlightTrack(track);

    if ( prefs.network ) {
	input_channel = new SGSocket( "", prefs.port, "udp" );
    } else if ( prefs.serial ) {
	input_channel = new SGSerial( prefs.device, prefs.baud );
    } else {
	printf("unknown input, defaulting to network on port 5500\n");
	input_channel = new SGSocket( "", "5500", "udp" );
    }
    input_channel->open( SG_IO_IN );
  }

  glutMotionFunc       ( mouseMotion );
  glutPassiveMotionFunc( mouseMotion );
  glutMouseFunc        ( mouseClick  );
  glutKeyboardFunc     ( keyPressed  );
  glutSpecialFunc      ( specPressed );

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
  
  map_object->setLocation( latitude, longitude );
  
  init_gui(prefs.textureFonts);

  // Create a tile manager.  In its creator it will see which scenery
  // directories we have, and whether there are maps generated for
  // those directories.
  printf("Please wait while checking existing scenery ... "); fflush(stdout);
  tileManager = new TileManager(prefs);
  glutTimerFunc(0, tileManagerTimer, 0);
  printf("done.\n");

  // Listen for any AI aircraft, updating once per second.
//   ai_aircraft = new SGSocket("", "5400", "udp");
//   ai_aircraft->open(SG_IO_IN);
//   ai_track = new FlightTrack(100);
//   map_object->setFlightTrack(ai_track);
//   glutTimerFunc(1000, otherAircraftTimer, 0);

  glutMainLoop();
 
  if (prefs.slaved)
      input_channel->close();

  return 0;
}
