/*-------------------------------------------------------------------------
  Tiles.hxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

  A couple of classes (TileManager, TileInfo) for keeping track of
  tiles and their basic information: names, directories, which maps
  exist, size, latitude, longitude, ....

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

#ifndef __TILES_H__
#define __TILES_H__

#include <string>
#include <map>
#include <bitset>

#include <simgear/misc/sg_path.hxx>

class TileInfo;
class TileManager {
public:
    static const char *tileName(float latitude, float longitude);
    static int tileWidth(int lat);

    TileManager(const SGPath& scenery, const SGPath& maps, 
		bool createDirs = false);
    ~TileManager();

    void scanScenery();

    const std::vector<std::string>& sceneryPaths() { return _sceneryPaths; }
    static const unsigned int MAX_MAP_LEVEL = 16;
    const std::bitset<MAX_MAP_LEVEL>& mapLevels() { return _mapLevels; }

    TileInfo* tile(const std::string& name);
    TileInfo* tile(float latitude, float longitude);
    const std::map<std::string, TileInfo *>& tiles() { return _tiles; }

protected:
    TileInfo *_getTile(const char *name);
    void _removeTile(TileInfo *t);

    // Map of tile names -> tiles
    std::map<std::string, TileInfo *> _tiles;

    // Paths
    std::vector<std::string> _sceneryPaths;

    // Map info
    SGPath _maps;
    std::bitset<MAX_MAP_LEVEL> _mapLevels;
};

// A TileInfo object represents the scenery in one of the
// subdirectories of the scenery tree.  Generally this is an area with
// a north-south extent of 1 degree of latitude and an east-west
// extent of 1 degree of longitude, but at extreme latitudes (83
// degrees or higher), one subdirectory can contain scenery for 1x2,
// 1x4, 1x8, or even 1x360 degree areas.
//
// A tile can be represented by FlightGear scenery, maps (of several
// different resolutions), or both.
class TileInfo {
public:
    TileInfo(const char *name, const SGPath& maps, 
	     std::bitset<TileManager::MAX_MAP_LEVEL>& mapLevels);
    ~TileInfo();

    // The "standard" name of the tile (eg, "w128n37").  Always 7
    // characters long.
    const char *name() const { return _name.c_str(); }
    // Where we look for buckets for this tile.
    const SGPath& sceneryDir() const { return _scenery; }
    // Where our maps are.  This is the same for all tiles.
    const SGPath& mapsDir() const { return _maps; }
    // Our buckets.  We return the bucket indices only.  To get the
    // complete path, prepend the scenery directory, and append
    // '.stg'.
    const std::vector<long int>* bucketIndices();
    
    // A bit set of all *desired* maps.  If the bitset is true at
    // index i, then we want a map i^2 pixels high.
    const std::bitset<TileManager::MAX_MAP_LEVEL>& mapLevels() const
    { return _mapLevels; }

    // A bitset of all *missing* maps.
    const std::bitset<TileManager::MAX_MAP_LEVEL>& missingMaps() const
    { return _missingMaps; }

    //////////////////////////////
    // Tile metrics.
    //////////////////////////////
    // SW corner and width of tile.
    int lat() const { return _lat; }
    int lon() const { return _lon; }
    int width() const { return _width; }
    int height() const { return 1; }
    // Centre of tile.
    float centreLat() const { return _lat + 0.5; }
    float centreLon() const { return _lon + _width / 2.0; }
    // Map size, in pixels, at a given resolution.
    void mapSize(unsigned int resolution, int *width, int *height) const;

    friend class TileManager;

protected:
    // Tells us that a rendered map exists.  This is meant to be
    // called by our TileManager as it scans the map directories, and
    // should be done soon after initialization.
    void _setExists(unsigned int i);
    // Tells us that we should assume that none of our desired maps
    // exist.  This will be done by the TileManager before it does a
    // directory scan.
    void _resetExists(std::bitset<TileManager::MAX_MAP_LEVEL>& levels);

    // Sets our scenery.  This is meant to be called by the
    // TileManager only.
    void _setScenery(const SGPath& scenery) { _scenery = scenery; }

    std::string _name;		// Our standard name (eg, "w123n37").

    const SGPath& _maps;	// Where maps are stored.
    SGPath _scenery;		// Where our scenery is.
    std::vector<long int>* _buckets; // Bucket (.stg file) indexes.

    // This keeps track of what maps we should have - _mapLevels[i] is
    // true if we should have a map i^2 pixels high.  Note that this
    // is just a reference to a variable held by the TileManager.
    const std::bitset<TileManager::MAX_MAP_LEVEL>& _mapLevels;

    // This keeps track of what we don't have (but should have) -
    // _missingMaps[i] is true if we don't have a map i^2 pixels high
    std::bitset<TileManager::MAX_MAP_LEVEL> _missingMaps;

    // SW corner and width of tile (degrees).
    int _lat, _lon, _width;
};

#endif        // __TILEMANAGER_H__
