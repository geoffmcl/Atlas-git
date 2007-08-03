/*-------------------------------------------------------------------------
  TileManager.cxx

  Written by Brian Schack, started July 2007.

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

#include <math.h>
#include <plib/ul.h>

#include <simgear/misc/sg_path.hxx>

#include "TileManager.hxx"

// TileManager constructor.  Command-line parameters given to Atlas
// are passed in via Preferences object.
TileManager::TileManager(Preferences &prefs) :
    _prefs(prefs)
{
}

// Scans the scenery directories, seeing if there's any scenery we
// need to generate maps for.
void TileManager::checkScenery()
{
    // To set up, we scan through the scenery Terrain directories,
    // seeing what we have.  We assume that we "have" the scenery for
    // a tile if the directory exists.  This is hardly foolproof, but
    // it's a good approximation.
    SGPath scenery(_prefs.scenery_root);
    ulDir *dir10;
    ulDirEnt *ent10;

    // We only look at Terrain subdirectories, since Atlas isn't
    // concerned with Objects.
    scenery.append("Terrain");
    dir10 = ulOpenDir(scenery.c_str());
    while (dir10 && (ent10 = ulReadDir(dir10))) {
	// At the top level, we'll be getting the 10-degree scenery
	// directories.  We need to go down to the next level to get
	// the actual 1-degree scenery directories.  The directories
	// should be of the form [ew]dd0[ns]d0.
	int lat, lon;
	SGPath scenery10 = scenery;
	ulDir *dir1;
	ulDirEnt *ent1;

	if (ent10->d_isdir && 
	    (sscanf(ent10->d_name, "%*1c%2d0%*1c%1d0", &lat, &lon) == 2)) {
	    // Go through the subdirectory.
	    scenery10.append(ent10->d_name);
	    dir1 = ulOpenDir(scenery10.c_str());
	    while (dir1 && (ent1 = ulReadDir(dir1))) {
		if (ent1->d_isdir &&
		    (sscanf(ent1->d_name, "%*1c%3d%*1c%2d", &lat, &lon) == 2)) {
		    // Whew!  Looks like we've got ourselves a scenery
		    // directory!  Add the tile to our "database" if
		    // it has pending work (ie, no hires or lowres
		    // map).
		    Tile *t = new Tile(ent1->d_name, _prefs);
		    unsigned int tasks = 0;

		    if (!t->hasHiresMap()) {
			tasks |= Tile::GENERATE_HIRES_MAP;
		    }
		    // Generate a lowres map when there isn't one
		    // and the user wants them.
		    if (!t->hasLowresMap() &&
			(_prefs.lowres_map_size != 0)) {
			tasks |= Tile::GENERATE_LOWRES_MAP;
		    }
		    if (tasks != Tile::NO_TASK) {
			t->setTasks(tasks);
			addTile(t);
		    } else {
			delete t;
		    }
		}
	    }
	    ulCloseDir(dir1);
	}
    }
    ulCloseDir(dir10);
}

// Adds the tile covering the given latitude and longitude.  The tile
// manager owns the tile.  The tile will be updated when its turn
// comes.  If the tile is already in the list, then nothing is done.
void TileManager::addTile(Tile *t)
{
    Tile *s = tileWithName(t->name());
    if (s) {
	// We already have a tile by that name.  Just delete t.
	delete t;
    } else {
	// New.  Add t to the queue.
	_tiles.push_back(t);
    }
}

// Removes the given tile and deletes it.  Note that the tile must be
// the *same* tile as the one in the list, not just an equivalent
// tile.
void TileManager::removeTile(Tile *t)
{
    _tiles.remove(t);
    delete t;
}

// Returns the number of tiles we're keeping track of.
int TileManager::noOfTiles()
{
    return _tiles.size();
}

// Returns the nth tile, where 0 is the first tile.  If n is out of
// bounds, we return NULL.
Tile *TileManager::nthTile(int n)
{
    std::list<Tile *>::iterator i;
    int j;

    if (n < 0) {
	return NULL;
    }

    // Not particularly efficient, but C++ lists don't seem to allow a
    // better way to do this.
    for (i = _tiles.begin(), j = 0; i != _tiles.end(); i++, j++) {
	if (j == n) {
	    return *i;
	}
    }

    return NULL;
}

// Returns the tile covering the given latitude and longitude, NULL if
// there is none.
Tile *TileManager::tileAtLatLon(float lat, float lon)
{
    char name[8], dir[8];
    float a, b;

    latLonToTileInfo(lat, lon, name, dir, &a, &b);

    return tileWithName(name);
}

// Returns the tile with the given name, NULL if there is none.
Tile *TileManager::tileWithName(char *name)
{
    std::list<Tile *>::iterator i;

    for (i = _tiles.begin(); i != _tiles.end(); i++) {
	if (strcmp((*i)->name(), name) == 0) {
	    return *i;
	}
    }

    return NULL;
}

// EYE - Can FlightGear's scenery scheme represent the north pole, or
// just get arbitrarily close?

// Canonicalizes an arbitrary latitude and longitude, which both must
// be expressed in degrees.  Southern latitudes and western longitudes
// are negative.  Out-of-range latitudes (> 89, < -90) and longitudes
// (> 179, < -180) are clipped.  Sets name, dir, centerLat, and
// centerLon.
//
// For example, given 36.7 and -120.2, it sets name to "w121n36", dir
// to "w130n30", centerLat to 36.5, and centerLon to -120.5.
//
// The caller must have allocated a string big enough to hold name and
// dir (ie, at least 8 bytes).
void TileManager::latLonToTileInfo(float latitude, float longitude,
				   char *name, char *dir,
				   float *centerLat, float *centerLon)
{
    char ns, ew;
    int lat, lon;
    int lat10, lon10;

    // All tiles are 1x1.  The lower-left (SW) corner is the floor of
    // the latitude and longitude.  We convert to integers as soon as
    // possible so that we don't get any rounding errors later.
    lat = (int)floor(latitude);
    lon = (int)floor(longitude);

    // Clip out-of-range values.  Note that we stop one degree short
    // of the north pole (90 degrees north), and the western side of
    // the international date line (180 degrees east).
    if (lat > 89) {
	lat = 89;
    } else if (lat < -90) {
	lat = -90;
    }
    if (lon > 179) {
	lon = 179;
    } else if (lon < -180) {
	lon = -180;
    }

    // Scenery tiles are grouped into chunks of 10 degrees by 10
    // degrees, also named by the lower-left corner.
    lat10 = (int)floor(lat / 10.0) * 10;
    lon10 = (int)floor(lon / 10.0) * 10;

    // Set our tile center.
    *centerLat = lat + 0.5;
    *centerLon = lon + 0.5;

    // Now we need to replace signs by n/s or e/w.
    if (lat >= 0) {
	ns = 'n';
    } else {
	ns = 's';
    }

    if (lon >= 0) {
	ew = 'e';
    } else {
	ew = 'w';
    }

    lat = abs(lat);
    lon = abs(lon);
    lat10 = abs(lat10);
    lon10 = abs(lon10);

    sprintf(name, "%c%03d%c%02d", ew, lon, ns, lat);
    sprintf(dir, "%c%03d%c%02d", ew, lon10, ns, lat10);
}

// Given a tile name, sets the latitude and longitude of the center of
// the tile.
void TileManager::nameToLatLon(char *name, float *latitude, float *longitude)
{
    char ns, ew;
    int lat, lon;

    sscanf(name, "%1c%3d%1c%2d", &ew, &lon, &ns, &lat);

    if (ew == 'w') {
	lon = -lon;
    }
    if (ns == 's') {
	lat = -lat;
    }
    *latitude = lat + 0.5;
    *longitude = lon + 0.5;
}
