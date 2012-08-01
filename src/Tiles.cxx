/*-------------------------------------------------------------------------
  Tiles.cxx

  Written by Brian Schack

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

// Our include file(s)
#include "Tiles.hxx"
#include "tiles.h"		// Contains the __scenery array.

// C++ system files
#include <stdexcept>
#include <limits>

// Our libraries' include files
#include <plib/ul.h>
#include <simgear/constants.h>

using namespace std;

////////////////////////////////////////////////////////////////////////////////
// __scenery access
////////////////////////////////////////////////////////////////////////////////

// Returns true if there's a tile at the given location in the
// __scenery array.
//
// The __scenery array consists of 180 rows, each corresponding to a
// latitude.  The first row corresponds to 89 degrees north, the last
// row to 90 degrees south.  Each row consists of 45 bytes (360 bits).
// The first byte corresponds to 180 to 173 degrees west (inclusive);
// the last byte corresponds to 172 to 179 degrees east.  The most
// significant bit of a byte is the westmost longitude.  A bit is set
// if there is FlightGear scenery at that <lat, lon>.
static bool __exists(const GeoLocation &loc)
{
    if (!loc.valid()) {
	return false;
    }
    int row = (179 - loc.lat());
    int lon = (loc.lon() + 180) % 360;
    int byte = lon / 8;
    int bit = 1 << (7 - (lon % 8));
    return __scenery[row][byte] & bit;
}

////////////////////////////////////////////////////////////////////////////////
// GeoLocation
////////////////////////////////////////////////////////////////////////////////

// Creates a GeoLocation from the given latitude and longitude.  They
// are assumed to be GeoLocation latitudes and longitudes - ie, 0 <=
// lat < 180, 0 <= lon < 360.  However, if 'standard' is true, then we
// expect standard latitudes - ie, -90 <= lat <= 90 (note the '<=
// 90'), and standard longitudes (which actually can be anything).
GeoLocation::GeoLocation(float lat, float lon, bool standard)
{
    setLoc(lat, lon, standard);
}

// Creates a GeoLocation from a FlightGear scenery name (eg,
// "w123n37").
GeoLocation::GeoLocation(const char *name)
{
    int lat, lon;
    char ew, ns;
    if (sscanf(name, "%1c%3d%1c%2d", &ew, &lon, &ns, &lat) == 4) {
	// The string seems to be well-formed, so try to extract a
	// latitude and longitude from it.

	if (ew == 'w') {
	    lon = 360 - lon;
	}
	if (ns == 's') {
	    lat = -lat;
	}
	lat += 90;

	setLoc(lat, lon);
    } else {
	invalidate();
    }
}

// Creates an invalid GeoLocation.
GeoLocation::GeoLocation()
{
    invalidate();
}

GeoLocation::~GeoLocation()
{
}

bool GeoLocation::valid() const
{
    int lat = _loc.first;
    int lon = _loc.second;
    return ((lat >= 0) && (lat < 180) && (lon >= 0) && (lon < 360));
}

void GeoLocation::invalidate()
{
    _loc.first = _loc.second = -1;
}

// Return our latitude (0 <= lat < 180, -1 if we're invalid).  If
// 'standard' is true, it returns a standard latitude (-90 <= lat <
// 90, -91 if invalid).
int GeoLocation::lat(bool standard) const
{
    int lat = _loc.first;
    if (standard) {
	lat -= 90;
    }
    return lat;
}

// Return our longitude (0 <= lon < 360, -1 if we're invalid).  If
// 'standard' is true, it returns a standard longitude (-180 <= lon <
// 180, -361 if invalid).
int GeoLocation::lon(bool standard) const
{
    int lon = _loc.second;
    if (standard) {
	lon = (lon + 180) % 360 - 180;
    }
    return lon;
}

const char *GeoLocation::name() const
{
    static char name[8];
    if (!valid()) {
	name[0] = '\0';
    } else {
	char ew = 'e', ns = 'n';
	int lat = this->lat(true);
	if (lat < 0) {
	    lat = -lat;
	    ns = 's';
	}
	int lon = this->lon(true);
	if (lon < 0) {
	    lon = -lon;
	    ew = 'w';
	}

	snprintf(name, 8, "%c%03d%c%02d", ew, lon, ns, lat);
    }

    return name;
}

// Sets a latitude and longitude.  By default, we expect GeoLocation
// latitudes and longitudes - ie, 0 <= lat < 180, 0 <= lon < 360.  If
// either is out of range, we become invalid.  If 'standard' is true,
// then we expect standard latitudes - ie, -90 <= lat <= 90 (note the
// '<= 90'), and standard longitudes (which actually can be anything).
void GeoLocation::setLoc(float latf, float lonf, bool standard)
{
    int lat = floor(latf);
    int lon = floor(lonf);
    if (standard) {
	if (lat == 90) {
	    // This is a special case.  In FlightGear, a standard
	    // scenery tile covers a rectangular area up to but *not*
	    // including the northern and eastern boundaries of that
	    // area - these are covered by the tiles to the north and
	    // east respectively.  However, an exception is made at
	    // the north pole - that tile includes the northern
	    // boundary (which is just the north pole).
	    lat = 89;
	}
	lat += 90;

	// It would be nice just to be able to say lon % 360, but C's
	// modulus operator doesn't behave well for negative numbers.
	// We need the extra bit to handle those.
	lon = ((lon % 360) + 360) % 360;
    }

    if ((0 <= lat) && (lat < 180) && (0 <= lon) && (lon < 360)) {
	_loc.first = lat;
	_loc.second = lon;
    } else {
	invalidate();
    }
}

bool GeoLocation::operator==(const GeoLocation& right) const
{
    return (lat() == right.lat()) && (lon() == right.lon());
}

bool GeoLocation::operator!=(const GeoLocation& right) const
{
    return !(*this == right);
}

int GeoLocation::operator<(const GeoLocation& right) const
{
    if (lat() < right.lat()) {
	return true;
    }
    if ((lat() == right.lat()) && (lon() < right.lon())) {
	return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// TileManager
////////////////////////////////////////////////////////////////////////////////

const unsigned char TileManager::NaPI = numeric_limits<unsigned char>::max();

TileManager::TileManager(const SGPath& scenery, const SGPath& maps): _maps(maps)
{
    ////////// Chunks and Tiles //////////

    // Create chunks and tiles.  We create a tile/chunk for each
    // *potential* one, regardless of whether one has actually been
    // downloaded.  This information we get from the __scenery array.

    // EYE - have constants: min_lat, max_lat, min_lon, max_lon,
    // perhaps as part of GeoLocation?
    for (int lat = 0; lat < 180; lat++) {
	int width = Tile::width(lat);
	for (int lon = 0; lon < 360; lon += width) {
	    // This location is guaranteed to by tile-canonical
	    // because we take tile widths into account.
	    GeoLocation loc(lat, lon);
	    if (__exists(loc)) {
		// There's a tile here.
		Tile *t = new Tile(loc, this);
		_tiles[loc] = t;

		// Now create a chunk for this tile if one hasn't been
		// created already.
		loc = Chunk::canonicalize(loc);
		Chunk *c = _chunks[loc];
		if (c == NULL) {
		    c = new Chunk(loc, this);
		    _chunks[loc] = c;
		}

		// Tell the chunk about this tile.
		c->_addTile(t);
	    }
	}
    }

    ////////// Scenery //////////

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

	// FlightGear allows for two types of scenery directory
	// structures.  The most common is for a scenery directory to
	// have a "Terrain" (and an "Objects") subdirectory.  In that
	// case, the "Terrain" subdirectory will contain scenery.
	// Also possible is for the path to point directly at scenery
	// (ie, there is no "Terrain" or "Objects" subdirectory).
        SGPath terrain(paths[i]);
        terrain.append("Terrain");

        ulDir *td = ulOpenDir(terrain.c_str());

        if (td == NULL) {
	    // No "Terrain" subdirectory, so just use the given path.
            _sceneryPaths.push_back(paths[i]);
        } else {
	    _sceneryPaths.push_back(terrain);
	    ulCloseDir(td);
        }
    }

    // Find out what scenery has been downloaded and which maps have
    // been rendered.
    scanScenery();
}

TileManager::~TileManager()
{
    map<GeoLocation, Chunk *>::const_iterator ci = _chunks.begin();
    for (; ci != _chunks.end(); ci++) {
	Chunk *c = ci->second;
	delete c;
    }
    map<GeoLocation, Tile *>::const_iterator ti = _tiles.begin();
    for (; ti != _tiles.end(); ti++) {
	Tile *t = ti->second;
	delete t;
    }
}

// Scans the map directory to see what map levels exist.  Assumes that
// _maps points to the map directory.  A maps directory should have
// some directories which are just small numbers (eg, 4, 7, 10).
// These directories hold rendered maps of the size 2^d (in this
// example, 16x16, 128x128, and 1024x1024).
void TileManager::_scanMapLevels()
{
    _mapLevels.reset();
    ulDir *mapDir = ulOpenDir(_maps.c_str());
    ulDirEnt *entity;
    while (mapDir && (entity = ulReadDir(mapDir))) {
	// We only look at a limited range of map levels: 0 (1x1) to
	// MAX_MAP_LEVEL - 1.
	unsigned int level;
	if (entity->d_isdir &&
	    sscanf(entity->d_name, "%u", &level) == 1) {
	    if ((level >= 0) && (level < MAX_MAP_LEVEL)) {
		_mapLevels[level] = true;
	    }
	}
    }
    ulCloseDir(mapDir);
}

Chunk *TileManager::_chunk(const GeoLocation& loc) const
{
    map<GeoLocation, Chunk *>::const_iterator i = _chunks.find(loc);
    if (i == _chunks.end()) {
	return NULL;
    } else {
	return i->second;
    }
}

// Find out what scenery and maps we have.  Note that if we have a
// piece of scenery, we look for maps for that scenery.  The opposite
// is not true - if there are maps with no corresponding scenery, we
// ignore them.
void TileManager::scanScenery()
{
    ////////// Scenery //////////

    // Tell our existing chunks to reset themselves, which means they
    // tell their tiles to reset themselves (which means they forget
    // about where their scenery is and what maps exist or should
    // exist).
    map<GeoLocation, Chunk *>::const_iterator i = _chunks.begin();
    for (; i != _chunks.end(); i++) {
	Chunk *c = i->second;
	c->_reset();
    }

    // To set up, we scan through each scenery Terrain directory,
    // seeing what we have.  We assume that we have the scenery for a
    // tile if the directory exists.  This is hardly foolproof, but
    // it's a good approximation.  
    //
    // Note that we go through the paths in reverse order.  This is
    // because of the behaviour of Tile::_setSceneryIndex - it just
    // overwrites whatever scenery index it has with the one it is
    // given.  With such a behaviour, we want the last one given to it
    // to be the correct one.  Going in reverse order ensures this.
    for (int i = _sceneryPaths.size() - 1; i >= 0; i--) {
	SGPath &scenery = _sceneryPaths[i];
	ulDir *dir;
	ulDirEnt *ent;

	dir = ulOpenDir(scenery.c_str());
	while (dir && (ent = ulReadDir(dir))) {
	    // At the top level, we'll be getting the 10-degree
	    // scenery directories (chunks).  We need to go down to
	    // the next level to get the actual 1-degree scenery
	    // directories (tiles).  The 10-degree directories should
	    // be of the form [ew]dd0[ns]d0, where 'd' is a decimal
	    // digit, and the 1-degree directories [ew]ddd[ns]dd.
	    int lat, lon;
	    if (ent->d_isdir && 
		(sscanf(ent->d_name, "%*1c%2d0%*1c%1d0", &lon, &lat) == 2)) {
		// We've found an appropriately named chunk directory.
		// We should have a matching chunk in our _chunks map.
		// If we don't, something is very wrong.
		Chunk *c = chunk(ent->d_name);
		if (!c) {
		    fprintf(stderr, "TileManager::scanScenery: unexpected chunk directory '%s' - ignoring\n", ent->d_name);
		} else {
		    // Pass the rest of the work on to the chunk.
		    c->_scanScenery(i);
		}
	    }
	}
	ulCloseDir(dir);
    }

    ////////// Maps //////////

    // Check which map subdirectories exist.  This will tell us what
    // resolutions the user expects.
    _scanMapLevels();

    // Now go through the rendered maps and tell the tiles which ones
    // exist.  Logically this would be better done in the tiles
    // themselves, but this is much faster (less opening and closing
    // of files).
    for (unsigned int i = 0; i < MAX_MAP_LEVEL; i++) {
	if (!_mapLevels[i]) {
	    continue;
	}

	// Open it the map subdirectory and see what maps we've got.
	SGPath maps = mapPath(i);
	ulDir *d = ulOpenDir(maps.c_str());
	ulDirEnt *e;
	while (d && (e = ulReadDir(d))) {
	    if (e->d_isdir) {
		continue;
	    }

	    // We've found a file.  See if it has a "tilish" name
	    // followed by a suffix.
	    int lat, lon;
	    if (sscanf(e->d_name, "%*1c%3d%*1c%2d.%*s", &lon, &lat) == 2) {
		// Get the tile (if one exists) and tell it that it
		// has a map at this level.
		char loc[8];
		strncpy(loc, e->d_name, sizeof(loc) - 1);
		loc[sizeof(loc) - 1] = '\0';
		Tile *t = tile(loc);
		if (!t) {
		    fprintf(stderr, "TileManager::scanScenery: unexpected map '%s' - ignoring\n", e->d_name);
		} else {
		    t->setMapExists(i, true);
		}
	    }
	}
	ulCloseDir(d);
    }
}

// Given a map level, returns a (static) SGPath for maps at that
// level.
const SGPath& TileManager::mapPath(unsigned int level)
{
    static SGPath result;
    result.set(_maps.str());

    char str[3];
    snprintf(str, 3, "%d", level);
    result.append(str);

    return result;
}

// A convenience routine to delete a possibly non-empty directory (and
// all of its subdirectories - be careful using this!).

// EYE - return a success/failure flag?  Only delete one level of a
// hierarchy for safety?  Only delete image files?
static void __rmdir(const char *str)
{
    ulDir *mapDir = ulOpenDir(str);
    ulDirEnt *entity;
    while (mapDir && (entity = ulReadDir(mapDir))) {
    	// Delete all the directories and files in the directory.
    	if (entity->d_isdir) {
	    if (strcmp(entity->d_name, ".") && 
		strcmp(entity->d_name, "..")) {
		SGPath dir(str);
		dir.append(entity->d_name);
		__rmdir(dir.str().c_str());
	    }
    	} else {
	    SGPath file(str);
	    file.append(entity->d_name);
    	    unlink(file.str().c_str());
    	}
    }
    rmdir(str);
}

// Sets our map levels to the given bitset.  In addition to changing
// our local variable _mapLevels, it may create the maps directory and
// individual map directories in that directory.  It may also delete
// existing map directories (and whatever was in those directories, so
// be very careful when using this!).
void TileManager::setMapLevels(std::bitset<MAX_MAP_LEVEL>& levels)
{
    // If the maps directory doesn't exist, create it.
    if (!_maps.exists()) {
	// SGPath has a weird idea about paths - it figures that the
	// last thing in a path like "/Foo/Bar" is *always* a file.
	// So, if you ask it to create directory with that path, it
	// won't create the directory "/Foo/Bar" as you might expect -
	// it will create the directory "/Foo".  And you can't fake it
	// out by creating the path "/Foo/Bar/", because it doesn't
	// allow paths that end with a path separator.  So, to satisfy
	// its strange desires, we add a junk name at the end to act
	// as a pseudo file name.
	SGPath dir(_maps);
	dir.append("junk");
	if (dir.create_dir(0755) < 0) {
	    // If we can't create it, throw an error.
	    throw runtime_error("couldn't create maps directory");
	}
    }

    for (unsigned int level = 0; level < MAX_MAP_LEVEL; level++) {
	if (levels[level] && !_mapLevels[level]) {
	    // Create a new directory.
	    SGPath mapDir = mapPath(level);
	    mapDir.append("junk"); // See comment above.
	    if (mapDir.create_dir(0755) < 0) {
	    	// If we can't create it, throw an error.
	    	throw runtime_error("couldn't create map subdirectory");
	    }
	    _mapLevels[level] = true;
	    // Tell all tiles with scenery that there's a new
	    // directory, which really means telling them that they
	    // don't have a map at that level.
	    TileIterator ti(this, TileManager::DOWNLOADED);
	    for (Tile *t = ti.first(); t; t = ti++) {
		t->setMapExists(level, false);
	    }
	} else if (!levels[level] && _mapLevels[level]) {
	    // Delete a directory.
	    const SGPath& mapDir = mapPath(level);
	    __rmdir(mapDir.str().c_str());
	    _mapLevels[level] = false;
	    // Tell all tiles with scenery that the directory was
	    // deleted, which really means telling them that they
	    // don't have a map at that level.
	    TileIterator ti(this, TileManager::DOWNLOADED);
	    for (Tile *t = ti.first(); t; t = ti++) {
		t->setMapExists(level, false);
	    }
	} else {
	    assert(_mapLevels[level] == levels[level]);
	}
    }
}

// Returns the number of tiles of the given type.
int TileManager::tileCount(SceneryType type)
{
    int result = 0;
    map<GeoLocation, Chunk *>::const_iterator i;
    for (i = _chunks.begin(); i != _chunks.end(); i++) {
	Chunk *c = i->second;
	result += c->tileCount(type);
    }
    return result;
}

// Return the chunk covering the given latitude and longitude.
Chunk *TileManager::chunk(const GeoLocation &loc) const
{
    return _chunk(Chunk::canonicalize(loc));
}

// Return the chunk with the given name, NULL if there is none.
Chunk *TileManager::chunk(const char *name)
{
    return _chunk(GeoLocation(name));
}

// Return the tile covering the given latitude and longitude.
Tile *TileManager::tile(const GeoLocation &loc)
{
    Tile *result = NULL;

    Chunk *c = chunk(loc);
    if (c) {
	result = c->tile(loc);
    }
    return result;
}

// Return the tile of the given name.
Tile *TileManager::tile(const char *name)
{
    Tile *result = NULL;

    Chunk *c = chunk(GeoLocation(name));
    if (c) {
	result = c->tile(name);
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Chunk
////////////////////////////////////////////////////////////////////////////////

// Returns the (canonical) name of the chunk covering the given
// latitude and longitude.
const char *Chunk::name(const GeoLocation &loc)
{
    return Chunk::canonicalize(loc).name();
}

// Returns the canonical name of the chunk containing the given
// latitude and longitude.  This is usually, but not always, the SW
// corner of the containing chunk.
GeoLocation Chunk::canonicalize(const GeoLocation &loc)
{
    GeoLocation result;
    if (loc.valid()) {
	int width = Tile::width(loc.lat());
	int lat = loc.lat() / 10 * 10;
	int lon = loc.lon() - (loc.lon() % width);
	lon = lon / 10 * 10;
	result.setLoc(lat, lon);
    }
    return result;
}

Chunk::Chunk(const GeoLocation &loc, TileManager *tm): 
    _tm(tm), _loc(loc), _downloadedTiles(0), _unmappedTiles(0)
{
}

Chunk::~Chunk()
{
}

const char *Chunk::name() const
{
    return _loc.name();
}

// Returns the number of tiles in this chunk of the given type.
int Chunk::tileCount(TileManager::SceneryType type)
{
    int result = 0;
    if (type == TileManager::ALL) {
	result = _tiles.size();
    } else if (type == TileManager::DOWNLOADED) {
	return _downloadedTiles;
    } else if (type == TileManager::UNMAPPED) {
	return _unmappedTiles;
    } else {
	// TileManager::MAPPED
	return _downloadedTiles - _unmappedTiles;
    }
    return result;
}

// Return the tile covering the given location, NULL otherwise.
Tile *Chunk::tile(const GeoLocation &loc) const
{
    return _tile(Tile::canonicalize(loc));
}

// Return the tile of the given name, NULL otherwise.
Tile *Chunk::tile(const char *name)
{
    return _tile(GeoLocation(name));
}

void Chunk::_addTile(Tile *t)
{
    _tiles[t->loc()] = t; 
}

void Chunk::_reset()
{
    _downloadedTiles = _unmappedTiles = 0;
    map<GeoLocation, Tile *>::const_iterator i = _tiles.begin();
    for (; i != _tiles.end(); i++) {
	Tile *t = i->second;
	t->_resetExists();
	t->_setSceneryIndex(TileManager::NaPI);
    }
}

// Scan a scenery directory for tile directories under this chunk.
// For those that exist, set their scenery path indices to 'i'.  Note
// that we may get called several times, potentially once per
// directory in the scenery search path, so it's important that our
// counters (_downloadedTiles, _unmappedTiles) are reset before
// calling this routine for the first time.
void Chunk::_scanScenery(unsigned char i)
{
    // Go through the chunk subdirectory.
    SGPath directory = _tm->sceneryPaths()[i];
    directory.append(name());
    ulDir *dir = ulOpenDir(directory.c_str());
    ulDirEnt *ent;
    while (dir && (ent = ulReadDir(dir))) {
	int lat, lon;
	if (ent->d_isdir &&
	    (sscanf(ent->d_name, "%*1c%3d%*1c%2d", &lon, &lat) == 2)) {
	    // Looks like we've got ourselves a scenery directory.
	    // Set its path index (it should already have been
	    // created).
	    Tile *t = tile(ent->d_name);
	    if (!t) {
		fprintf(stderr, "Chunk::_scanScenery: unexpected tile directory '%s' - ignoring\n", ent->d_name);
	    } else {
		if (!t->hasScenery()) {
		    // This is the first time we've seen scenery for this
		    // tile, so update our _downloadedTiles and
		    // _unmappedTiles counters.
		    _downloadedTiles++;
		    _unmappedTiles++;
		}
		t->_setSceneryIndex(i);
	    }
	}
    }
    ulCloseDir(dir);
}

void Chunk::_tileBecameMapped()
{
    assert(_unmappedTiles > 0);
    _unmappedTiles--;
}

void Chunk::_tileBecameUnmapped()
{
    _unmappedTiles++;
}

Tile *Chunk::_tile(const GeoLocation &loc) const
{
    map<GeoLocation, Tile *>::const_iterator i = _tiles.find(loc);
    if (i == _tiles.end()) {
	return NULL;
    } else {
	return i->second;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Tile
////////////////////////////////////////////////////////////////////////////////

// Returns the standard tile width at the given latitude (which must
// be a canonical latitude, varying between 0 and 179 inclusive, and
// which refers to the latitude of the bottom of the tile).
int Tile::width(int lat)
{
    // EYE - clip?
    assert((0 <= lat) && (lat < 180));
    lat -= 90;

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

const char *Tile::name(const GeoLocation &loc)
{
    // Convert to a canonical form.
    return canonicalize(loc).name();
}

// Returns the canonical latitude and longitude for a given latitude
// and longitude, where "canonical" means the SW corner of the scenery
// tile that contains the given location.
GeoLocation Tile::canonicalize(const GeoLocation &loc)
{
    GeoLocation result;
    if (loc.valid()) {
	int width = Tile::width(loc.lat());
	result.setLoc(loc.lat(), loc.lon() - (loc.lon() % width));
    }
    return result;
}

Tile::Tile(const GeoLocation &loc, TileManager *tm):
    _tm(tm), _loc(loc), _sceneryIndex(TileManager::NaPI)
{
}

// Tiles use this when returning their scenery directories in the
// sceneryDir() call.
SGPath Tile::__sceneryPath;

Tile::~Tile()
{
}

// Return our scenery directory.  Note that we calculate it on each
// call, and that we use the shared __sceneryPath instance variable.
const SGPath& Tile::sceneryDir()
{
    if (_sceneryIndex == TileManager::NaPI) {
	__sceneryPath = "";
    } else {
	__sceneryPath = _tm->sceneryPaths()[_sceneryIndex];
	__sceneryPath.append(chunk()->name());
	__sceneryPath.append(name(_loc));
    }

    return __sceneryPath;
}

// EYE - use this in more places
bool Tile::isType(TileManager::SceneryType t)
{
    if (t == TileManager::ALL) {
	return true;
    } else if ((t == TileManager::DOWNLOADED) && 
	       hasScenery()) {
	return true;
    } else if ((t == TileManager::UNMAPPED) && 
	       hasScenery() && 
	       missingMaps().any()) {
	return true;
    } else if ((t == TileManager::MAPPED) &&
	       hasScenery() && 
	       !missingMaps().any()) {
	return true;
    }
    return false;
}

void Tile::bucketIndices(vector<long int>& indices)
{
    indices.clear();
    if (hasScenery()) {
	// Scan the scenery directory for the given tile for its buckets.
	// We deem that each .stg file represents one bucket.
	ulDir *dir = ulOpenDir(sceneryDir().str().c_str());
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
	    indices.push_back(index);
	}

	ulCloseDir(dir);
    }
}

// Tell the tile that a map at the given level exists or not.  The
// tile will then tell its owning chunk if its mapped status has
// changed (a tile is mapped when there is a rendered map for all
// desired map levels).
void Tile::setMapExists(unsigned int i, bool exists)
{
    // If we don't have scenery, or the map level is out of bounds,
    // just return.
    if (!hasScenery() || 
	(i >= TileManager::MAX_MAP_LEVEL)) {
	return;
    }

    // Figure if our state has changed.  First, record our current
    // mapped status.  Then set our _maps bitset as appropriate and
    // recheck our mapped status.  If it has changed, tell our owning
    // chunk.
    bool wasUnmapped = missingMaps().any();
    _maps[i] = exists;
    bool isUnmapped = missingMaps().any();
    if (wasUnmapped && !isUnmapped) {
	chunk()->_tileBecameMapped();
    } else if (!wasUnmapped && isUnmapped) {
	chunk()->_tileBecameUnmapped();
    }
}

// Calculates the width and height (in pixels) of the map, given a
// resolution.
void Tile::mapSize(unsigned int level, int *width, int *height) const
{
    // The height is is the same for all tiles at a given resolution
    // (a degree of latitude is pretty much the same wherever you go).
    // The width is set to be as 'natural' as possible, with the
    // constraint that it be a power of 2.  By 'natural' I mean that
    // pixels in different maps should cover approximately the same
    // area.  Since a 1-degree tile at 30 degrees latitude is about
    // half the size of a 1-degree tile at the equator, it should have
    // about half the width.
    *height = 1 << level;

    // The width is proportional to the height, scaled by the latitude
    // (the real latitude, not our special one) and the width of tiles
    // at that latitude.
    double w = *height * cos(lat() * SGD_DEGREES_TO_RADIANS) * Tile::width();

    // Oh yeah, and it has to be a power of 2, and non-zero.
    *width = pow(2.0, round(log2f(w)));
    if (*width == 0) {
    	*width = 1;
    }
}

TileIterator::TileIterator(TileManager *tm, TileManager::SceneryType type):
    _tm(tm), _c(NULL), _type(type)
{
}

TileIterator::TileIterator(Chunk *c, TileManager::SceneryType type):
    _tm(NULL), _c(c), _type(type)
{
}

TileIterator::~TileIterator()
{
}

Tile *TileIterator::operator++(int)
{
    // It seems that constantly calling foo->tiles().end() in this
    // routine is a bit expensive, so we call it once and save it in a
    // variable to speed things up a bit.
    map<GeoLocation, Tile *>::const_iterator tend = _end();

    // If we're already done, just return NULL.
    if (_ti == tend) {
	return NULL;
    }

    // Find the next tile with the correct type.
    _ti++;
    while (true) {
	// Look for the next tile of the given type.
	while (_ti != tend) {
	    Tile *t = _ti->second;
	    if (t->isType(_type)) {
		// Found the right type in this chunk, so return.
		return t;
	    }
	    _ti++;
	}

	// Nothing left.
	return NULL;
    }

    return NULL;
}

Tile *TileIterator::first()
{
    _ti = _begin();
    if (_ti == _end()) {
	return NULL;
    }
    Tile *t = _ti->second;
    if (!t->isType(_type)) {
	return this->operator++(0);
    } else {
	return t;
    }
}

map<GeoLocation, Tile *>::const_iterator TileIterator::_begin()
{
    assert(_tm || _c);
    assert(!(_tm && _c));
    if (_tm) {
	return _tm->tiles().begin();
    } else {
	return _c->tiles().begin();
    }
}

map<GeoLocation, Tile *>::const_iterator TileIterator::_end()
{
    assert(_tm || _c);
    assert(!(_tm && _c));
    if (_tm) {
	return _tm->tiles().end();
    } else {
	return _c->tiles().end();
    }
}
