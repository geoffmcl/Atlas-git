/*-------------------------------------------------------------------------
  Tiles.cxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

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

#include <cassert>
#include <stdexcept>
#include <cmath>

#include <simgear/misc/sg_path.hxx>
#include <plib/ul.h>
#include <plib/sg.h>

#include "Tiles.hxx"

#ifdef _MSC_VER
#include <simgear/math/SGMisc.hxx>
#define ROUND(a) SGMisc<double>::round(a)
#else
#define ROUND(d)    round(d)
#endif

using namespace std;

////////////////////////////////////////////////////////////////////////////////
// TileManager
////////////////////////////////////////////////////////////////////////////////

int TileManager::tileWidth(int lat)
{
    assert((-90 <= lat) && (lat <= 89));

    if (lat < 0) {
	lat = abs(lat + 1);
    }

    if (lat < 83) {
	return 1;
    } else if (lat < 86) {
	return 2;
    } else if (lat < 88) {
	return 4;
    } else if (lat < 89) {
	return 8;
    } else {
	return 360;
    }
}

const char *TileManager::tileName(float latitude, float longitude)
{
    // Names are always 7 characters long.
    static char name[8];

    int lat = floor(latitude);
    int lon = floor(longitude);
    char ew = 'e', ns = 'n';
    if (lat < 0) {
	lat = -lat;
	ns = 's';
    }
    if (lon < 0) {
	lon = -lon;
	ew = 'w';
    }

    snprintf(name, 8, "%c%03d%c%02d", ew, lon, ns, lat);

    return name;
}

TileManager::TileManager(const SGPath& scenery, const SGPath& maps, 
			 bool createDirs): _maps(maps)
{
    // The scenery search path may contain more than one directory.
    // Check and see which ones exist.  If they exist, check if they
    // have a "Terrain" subdirectory too.
    vector<string> paths = sgPathSplit(scenery.str());
    for (unsigned int i = 0; i < paths.size(); i++) {
        ulDir *d = ulOpenDir(paths[i].c_str());
        if (d == NULL) {
	    // Doesn't exist - not interesting.
            continue;
	}
        ulCloseDir(d);

	// Now check if it has a "Terrain" subdirectory.  We don't
	// bother looking at the "Objects" directory, since Atlas
	// isn't interested in objects.
        SGPath terrain(paths[i]);
        terrain.append("Terrain");

        ulDir *td = ulOpenDir(terrain.c_str());

        if (td == NULL) {
	    // No "Terrain" subdirectory, so just use the given path.
            _sceneryPaths.push_back(paths[i]);
        } else {
	    _sceneryPaths.push_back(terrain.str());
	    ulCloseDir(td);
        }
    }

    // SGPath has a weird idea about paths - it figures that the last
    // thing in a path like "/Foo/Bar" is a file.  However, we want to
    // be certain it treats _maps as a directory.  So, if SGPath
    // thinks _maps has a file at the end, we add an empty item, thus
    // changing the path to "/Foo/Bar/", which will convince it that
    // the last thing is in fact a directory.  Furthermore, in Windows
    // the empty() test just plain doesn't work, and the exists() test
    // doesn't work on directories (we're getting close to writing our
    // own SGPath here ...).
#ifdef _MSC_VER
    size_t len = _maps.str().length();
    if (( len != 0 ) && (_maps.str().rfind("/") != (len - 1))) {
#else
   if (!_maps.file().empty()) {
#endif
	_maps.append("");
    }

    // If the maps directory doesn't exist, try creating it if requested.
#ifdef _MSC_VER
    if (!is_valid_path(_maps.str()) && createDirs) {
#else
    if (!_maps.exists() && createDirs) {
#endif
	if (_maps.create_dir(0755) < 0) {
	    // If we can't create it, throw an error.
	    throw runtime_error("couldn't create maps directory");
	}
    }

    // A maps directory should have some directories which are just
    // small numbers (eg, 4, 7, 10).  These directories hold rendered
    // maps of the size 2^d (ie, 16x16, 128x128, and 1024x1024).  We
    // want to find out what those numbers are.
    ulDir *mapDir = ulOpenDir(_maps.c_str());
    ulDirEnt *entity;
    while (mapDir && (entity = ulReadDir(mapDir))) {
	// We only look at a limited range of map levels: 0 (1x1) to
	// MAX_MAP_LEVEL - 1.
	unsigned int level;
	if (entity->d_isdir &&
	    sscanf(entity->d_name, "%d", &level) == 1) {
	    if ((level >= 0) && (level < MAX_MAP_LEVEL)) {
		_mapLevels[level] = true;
	    }
	}
    }
    ulCloseDir(mapDir);

    // If we didn't find any directories, try creating a default set
    // if requested.
    if (_mapLevels.none() && createDirs) {
	// EYE - magic numbers.  Should we pass these in?
	_mapLevels[4] = true;
	_mapLevels[6] = true;
	_mapLevels[8] = true;
	_mapLevels[9] = true;
	_mapLevels[10] = true;
	for (unsigned int level = 0; level < MAX_MAP_LEVEL; level++) {
	    if (!_mapLevels[level]) {
		continue;
	    }

	    SGPath map(_maps);
	    char str[3];
	    snprintf(str, 3, "%d", level);
	    map.append(str);
	    map.append("");
	    if (map.create_dir(0755) < 0) {
		// If we can't create it, throw an error.
		throw runtime_error("couldn't create map subdirectories");
	    }
	}
    }

    scanScenery();
}

TileManager::~TileManager()
{
    map<string, TileInfo *>::iterator i = _tiles.begin();
    for (; i != _tiles.end(); i++) {
	TileInfo *t = i->second;
	delete t;
    }
}

// Scans the scenery directories, seeing if there's any scenery we
// need to generate maps for.
void TileManager::scanScenery()
{
    // First, if this is not the first time we've run scanScenery(),
    // tell our existing tiles to reset themselves.
    SGPath empty("");
    map<string, TileInfo *>::iterator i = _tiles.begin();
    for (; i != _tiles.end(); i++) {
	TileInfo *t = i->second;
	t->_resetExists(_mapLevels);
	t->_setScenery(empty);
    }

    // To set up, we scan through each scenery Terrain directory,
    // seeing what we have.  We assume that we have the scenery for a
    // tile if the directory exists.  This is hardly foolproof, but
    // it's a good approximation.
    for (unsigned int path = 0; path < _sceneryPaths.size(); path++) {
	SGPath scenery(_sceneryPaths[path]);
	ulDir *dir10;
	ulDirEnt *ent10;

	dir10 = ulOpenDir(scenery.c_str());
	while (dir10 && (ent10 = ulReadDir(dir10))) {
	    // At the top level, we'll be getting the 10-degree scenery
	    // directories.  We need to go down to the next level to get
	    // the actual 1-degree scenery directories.  The 10-degree
	    // directories should be of the form [ew]dd0[ns]d0, where 'd'
	    // is a decimal digit, and the 1-degree directories
	    // [ew]ddd[ns]dd.
	    int lat, lon;
	    SGPath scenery10 = scenery;
	    ulDir *dir1;
	    ulDirEnt *ent1;

	    if (ent10->d_isdir && 
		(sscanf(ent10->d_name, "%*1c%2d0%*1c%1d0", &lon, &lat) == 2)) {
		// Go through the subdirectory.
		scenery10.append(ent10->d_name);
		dir1 = ulOpenDir(scenery10.c_str());
		while (dir1 && (ent1 = ulReadDir(dir1))) {
		    if (ent1->d_isdir &&
			(sscanf(ent1->d_name, "%*1c%3d%*1c%2d", &lon, &lat) 
			 == 2)) {
			// Whew!  Looks like we've got ourselves a scenery
			// directory!  Add the tile to our "database".
			SGPath scenery1 = scenery10;
			scenery1.append(ent1->d_name);
			TileInfo *t = _getTile(ent1->d_name);
			t->_setScenery(scenery1);
		    }
		}
		ulCloseDir(dir1);
	    }
	}
	ulCloseDir(dir10);
    }

    // Now go through the rendered maps and tell the tiles which ones
    // exist.  Logically this would be better done in the tiles
    // themselves, but this is much faster (less opening and closing
    // of files).
    for (unsigned int i = 0; i < MAX_MAP_LEVEL; i++) {
	if (!_mapLevels[i]) {
	    continue;
	}

	SGPath maps(_maps.c_str());
	char level[3];
	snprintf(level, 3, "%d", i);
	maps.concat(level);

	// Open it up and see if we've got a map for this tile.
	ulDir *d = ulOpenDir(maps.c_str());
	ulDirEnt *e;
	while (d && (e = ulReadDir(d))) {
	    if (e->d_isdir) {
		continue;
	    }

	    // See if it has a "tilish" name followed by a suffix.
	    int lat, lon;
	    if (sscanf(e->d_name, "%*1c%3d%*1c%2d.%*s", &lon, &lat) == 2) {
		// Get the name without the suffix.
		char root[8];
		strncpy(root, e->d_name, 7);
		root[7] = '\0';

		// Get the tile and tell it that it has a map at this
		// level.
		TileInfo *t = _getTile(root);
		t->_setExists(i);
	    }
	}
	ulCloseDir(d);
    }
}

TileInfo* TileManager::tile(const std::string& name)
{
    map<string, TileInfo *>::iterator i = _tiles.find(name);
    if (i != _tiles.end()) {
	return i->second;
    } else {
	return NULL;
    }
}

TileInfo* TileManager::tile(float latitude, float longitude)
{
    return tile(tileName(latitude, longitude));
}

// Returns the tile with the given name, creating a new one if it
// doesn't exist.
TileInfo *TileManager::_getTile(const char *name)
{
    TileInfo *t = tile(name);
    if (!t) {
	t = new TileInfo(name, _maps, _mapLevels);
	_tiles[name] = t;
    }

    return t;
}

// Removes the given tile.
void TileManager::_removeTile(TileInfo *t)
{
    _tiles.erase(t->name());
    delete t;
}

////////////////////////////////////////////////////////////////////////////////
// TileInfo
////////////////////////////////////////////////////////////////////////////////

// Creates a structure representing a tile of the given name, with
// maps in the given map directory.  The mapLevels bitset tells us
// which map levels the user wants.
TileInfo::TileInfo(const char *name, const SGPath& maps,
		   bitset<TileManager::MAX_MAP_LEVEL>& mapLevels)
    : _name(name), _maps(maps), _buckets(NULL), _mapLevels(mapLevels)
{
    assert(name);

    // Initially, assume we are missing all desired maps.  We expect
    // the TileManager to tell us via _setExists() what's really
    // there.  It would be more logical to check that information
    // here, but it turns out to be very slow, as each TileInfo object
    // opens, searches, and closes the same directories.
    _missingMaps = mapLevels;

    // Get the SW corner of our tile.
    char ns = 'n', ew = 'e';
    assert(sscanf(name, "%c%3d%c%2d", &ew, &_lon, &ns, &_lat) == 4);
    if (ew == 'w') {
	_lon = -_lon;
    }
    if (ns == 's') {
	_lat = -_lat;
    }

    _width = TileManager::tileWidth(_lat);
}

TileInfo::~TileInfo()
{
    if (_buckets) {
	delete _buckets;
    }
}

const vector<long int>* TileInfo::bucketIndices()
{
    // We hold off on scanning our scenery for buckets, doing so only
    // on demand.  This is done under the assumption that callers will
    // rarely ask for buckets, and do so infrequently.
    if (_buckets != NULL) {
	return _buckets;
    }

    _buckets = new vector<long int>;

    if (!_scenery.str().empty()) {
	// Scan the scenery directory for the given tile for its buckets.
	// We deem that each .stg file represents one bucket.
	ulDir *dir = ulOpenDir(_scenery.str().c_str());
	if (dir == NULL) {
	    // EYE - what's a good system for error names?
	    throw runtime_error("scenery directory");
	}

	ulDirEnt *ent;
	while ((ent = ulReadDir(dir)) != NULL) {
	    if (ent->d_isdir) {
		continue;
	    }
	    long int index = 0;
	    unsigned int length = 0;
	    if (sscanf(ent->d_name, "%ld.stg%n", &index, &length) != 1) {
		continue;
	    }
	    // EYE - hack?  Do .stg files end with .gz sometimes?
	    if (length != strlen(ent->d_name)) {
		continue;
	    }

	    // This is a .stg file, which is what we want.
	    _buckets->push_back(index);
	}

	ulCloseDir(dir);
    }

    return _buckets;
}

// Calculates the width and height (in pixels) of the map, given a
// resolution.
void TileInfo::mapSize(unsigned int level, int *width, int *height) const
{
    // The height is is the same for all tiles at a given resolution
    // (a degree of latitude is pretty much the same wherever you go).
    // The width is set to be as 'natural' as possible, with the
    // constraint that it be a power of 2.  By 'natural' I mean that
    // pixels in different maps should be at approximately the same
    // scale - a 1-degree tile at 30 degrees latitude should be half
    // the size of a 1-degree tile at the equator.
    *height = 1 << level;

    // The width is proportional to the height, scaled by the latitude
    // and the width of tiles at that latitude.
    double w = *height * cos(centreLat() * SGD_DEGREES_TO_RADIANS) * _width;

    // Oh yeah, and it has to be a power of 2, and non-zero.
    *width = pow(2.0, ROUND(log2f(w)));
    if (*width == 0) {
    	*width = 1;
    }
}

void TileInfo::_setExists(unsigned int i)
{
    if (i < TileManager::MAX_MAP_LEVEL) {
	_missingMaps[i] = false;
    }
}

void TileInfo::_resetExists(bitset<TileManager::MAX_MAP_LEVEL>& levels)
{
    _missingMaps = levels;
}
