/*-------------------------------------------------------------------------
  Tiles.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2012 Brian Schack

  A group of classes for keeping track of scenery information: names,
  directories, which maps exist, size, latitude, longitude, ....

  Chunks, tiles, buckets, sub-buckets, and maps
  ---------------------------------------------

  In Atlas, a tile is a 1x1 degree piece of scenery (usually - at the
  poles they become wider - this caveat is true for all scenery
  objects).  A chunk is a 10x10 area containing tiles.  In FlightGear,
  both chunks and tiles are represented by directories.  Chunks are at
  the highest level of the scenery directory structure (and have names
  like "w130n40"), and tiles exist immediately below them (and have
  names like "w123n37").

  Below tiles are buckets, which are 1/8x1/8 degree pieces of scenery,
  which are represented by a file with the suffix ".stg".  At the
  bottom we have sub-buckets, which are single files with the suffix
  ".btg.gz" or just ".btg".  Sub-buckets are the real scenery files -
  they model the terrain of an area, including information about which
  bits are roads, which bits are water, ....  A bucket is just a
  simple text file stating which sub-buckets comprise it.  Generally
  each bucket is represented by a single sub-bucket.  However, in
  areas with airports, each airport has its own subbucket (this is
  done because each airport is essentially carved into the surrounding
  terrain, in order that the runways are properly sloped).

  Finally, maps are rendered images of tiles that Atlas displays.

  By the way, the terms "chunk", "tile", "bucket" and "sub-bucket" are
  sometimes used by FlightGear, and sometimes consistently, but not
  always.  You might find other terms, but usually context makes it
  clear what is what.  In this code, I try to use them consistently.

  Existence
  ---------

  Generally if we talk about a chunk or tile existing, we mean in the
  FlightGear world, not on the user's disk.  In other words, it isn't
  necessary for a tile to have been downloaded for it to exist.
  However, if it exists, that means we *can* download scenery for it
  (ie, it isn't a piece of empty ocean).  If you want to find out if a
  tile has been actually downloaded, call the tile's hasScenery()
  function.  In the case of maps, a map exists if the user has scenery
  for it and it has been rendered.

  Latitudes and longitudes
  ------------------------

  In the TileManager, Tile, and Chunk classes, latitudes and
  longitudes are measured in degrees.  Unless stated otherwise,
  latitudes must be in the range 0 <= lat < 180, and longitudes in the
  range 0 <= lon < 359.  A latitude of 0 corresponds to the south pole
  (-90 degrees on a regular map), and 180 to the north pole (90
  degrees on a regular map).  A longitude of 0 corresponds to the
  prime meridian (0 degrees on a regular map), 180 to the
  international date line (180 degrees east - or west - on a regular
  map), and 359 to 1 degree west on a regular map.  The class
  GeoLocation ensures that this constraints are met, so they are
  exclusively used here.  The only places where you see "normal"
  latitudes and longitudes are in Tile::lat(), Tile::lon(),
  Tile::centreLat(), and Tile::centreLon().

  Why the strange numbers?  Two reasons.  First, managing tiles
  requires a lot of integer arithmetic on latitudes and longitudes, in
  particular modulus operations.  The C/C++ modulus operator (and this
  is shared by almost all programming languages) is not mathematically
  very well-behaved for negative numbers, and so I found myself
  forever treating negative numbers as a special case.  For example,
  we often want to find the western edge of a tile.  If tiles are 8
  degrees wide, this can be done with "lon - (lon % 8)" (assuming
  we're using integers), or "(lon / 8) * 8".  So, if the longitude is
  10, we get a west edge of 8, and if the longitude is 2, we get a
  west edge of 0.  But, if the longitude is -2, we don't get -8, we
  get 0, which is wrong.  By making latitudes and longitudes always
  positive, we avoid this problem.

  Second, it's useful to be able to assume that if lon1 is west of
  lon2, then lon1 < lon2.  In the real world, this isn't true (at
  least for any coordinate system I can imagine).  However, with
  FlightGear scenery, it is true if we number from 0 to 359.  With
  this numbering system, the west edge of a FlightGear scenery tile is
  always less than the east edge of a tile.  If we chose a -180 to 179
  system, there would be tiles near the north pole for which this is
  not true, because they span the international date line.  For
  example, the northmost tile is 360 degrees wide, with its west edge
  at 0 degrees, and its east edge at 0 degrees (after going all the
  way around the world).  Dealing with these special cases causes
  endless grief, enough I think to justify going for a 0 - 359 system.

  Canonical locations
  -------------------

  We often refer to "canonical" locations.  A canonical location is
  just the SW corner of a tile or chunk, which uniquely identifies
  that tile or chunk in the FlightGear world (for example, it is used
  to name chunk and tile directories).

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

#ifndef _TILES_H_
#define _TILES_H_

#include <vector>
#include <map>
#include <bitset>

#include <simgear/misc/sg_path.hxx>

// EYE - put this in Geographics?
// EYE - rename it to make it clear it's just used for tile and chunk
// naming?

// An integer-valued latitude, longitude pair, useful for dealing with
// scenery.  It guarantees that, for valid GeoLocations, all latitudes
// will be between 0 and 179 degrees inclusive, and all longitudes
// between 0 and 359 degrees inclusive.  It can be initialized with a
// latitude and longitude, a string in the form [ew]ddd[ns]dd (used to
// name tiles and chunks in FlightGear scenery), or nothing (in which
// case it is invalid).
//
// If given an invalid latitude and/or longitude, either in the
// constructor or in setLat()/setLon(), it will silently invalidate
// itself.
//
// A GeoLocation is invalid if the empty initializer is used, if given
// invalid latitudes or longitudes, or invalidate() is called.  All
// invalid GeoLocations compare equal.
class GeoLocation {
  public:
    GeoLocation(int lat, int lon);
    GeoLocation(const char *name);
    GeoLocation();
    ~GeoLocation();

    bool valid() const;
    void invalidate();

    // Return our latitude or longitude (-1 if we're invalid).
    int lat() const { return _loc.first; }
    int lon() const { return _loc.second; }

    // Set a latitude and longitude.  If either is out of range, we
    // become invalid (both are set to -1).
    void setLoc(int lat, int lon);

    bool operator==(const GeoLocation& right) const;
    bool operator!=(const GeoLocation& right) const;
    int operator<(const GeoLocation& right) const;

  protected:
    // Latitudes and longitudes are stored as shorts to save a bit of
    // space.
    std::pair<short, short> _loc;
};

class Chunk;
class Tile;
class TileIterator;

// The tile manager (generally you should only need one) keeps track
// of FlightGear scenery.  It is given the scenery path (which can
// consist of several directories) and the Atlas map directory.
//
// Maintainer note: The tile manager "knows" about all FlightGear
// scenery, whether loaded or not, via the __scenery static data
// structure (from tiles.h).  Thus you can query the tile manager (and
// chunks and tiles) about some aspects of scenery (names, widths,
// ...)  without having actually downloaded the scenery in question.
// The static data structure should be checked occasionally to make
// sure it correctly represents the state of FlightGear scenery.
// However, I suspect this doesn't need to be done very often - land
// is not likely to appear or disappear in the short term.
class TileManager {
  public:
    // Initialize a tile manager, telling it where to look for scenery
    // and maps.
    TileManager(const SGPath& scenery, const SGPath& maps);
    ~TileManager();

    // Scans the scenery and map directories to find out the current
    // state of local scenery and maps.  This is called in the
    // TileManager constructor, but you can call it if you think
    // things have changed on disk.  If scanMapLevels is true, we scan
    // the maps directory for maps subdirectories, telling us what map
    // levels to expect.  If false, we assume that our internal
    // _mapLevels bitset is correct.
    void scanScenery(bool scanMapLevels = true);

    const std::vector<SGPath>& sceneryPaths() { return _sceneryPaths; }
    const SGPath& mapPath() { return _maps; }
    
    // Returns the map subdirectory for a particular level.  Note that
    // we return a static variable, so if you need to keep using it,
    // make a copy.
    const SGPath& mapPath(unsigned int level);

    // This constant represents "not a path index" - an invalid
    // scenery path index.
    static const int NaPI;

    // Map levels determine the resolutions of the maps generated.
    // Maps can range in size from 0 (1x1) to MAX_MAP_LEVEL - 1
    // (currently 15, or 32768x32768).  Map levels correspond to
    // numbered directories in the map directory.  For example, if a
    // directory named "10" exists in the map directory, the tile
    // manager expects to find maps of size 10 (1024x1024) in that
    // directory.

    // EYE - make this just a regular signed value?
    static const unsigned int MAX_MAP_LEVEL = 16;
    const std::bitset<MAX_MAP_LEVEL>& mapLevels() { return _mapLevels; }

    // Set our map levels.  This could mean creating directories,
    // and/or deleting directories and all of their contents, so
    // be careful when calling it.
    void setMapLevels(std::bitset<MAX_MAP_LEVEL>& levels);

    // Simple accessors for tiles and chunks of various types.  The
    // fooCount() routines tell you how many exist, the foo() routines
    // access them.  You can decide what type you're interested in:
    // ALL means all scenery, whether downloaded or not; DOWNLOADED
    // means only downloaded scenery (for a chunk, DOWNLOADED means it
    // contains at least one downloaded tile), whether mapped or not;
    // UNMAPPED means downloaded but unmapped, or at least not
    // completely mapped (for a chunk, UNMAPPED means at least one of
    // its downloaded tiles has missing maps).
    //
    // These accessors are good for occasional random access of tiles
    // and chunks.  If you need to iterate through all tiles, the
    // TileIterator class is a better bet.
    enum SceneryType {ALL, DOWNLOADED, UNMAPPED, MAPPED};
    int chunkCount(SceneryType type);
    Chunk *chunk(SceneryType type, int i);
    int tileCount(SceneryType type);
    Tile *tile(SceneryType type, int i);

    // Returns the chunk containing the given location, NULL if none
    // exists.  The location does not have to be chunk-canonical.
    Chunk *chunk(const GeoLocation &loc) const;
    // Returns the chunk with the given name (which must be
    // chunk-canonical), NULL otherwise.
    Chunk *chunk(const char *name);
    // Returns our map of chunks.
    std::map<GeoLocation, Chunk *>& chunks() { return _chunks; }

    // Returns the tile containing the given location, NULL if none
    // exists.  The location does not have to be tile-canonical.
    Tile *tile(const GeoLocation &loc);
    // Returns the tile with the given name (which must be
    // tile-canonical), NULL otherwise.
    Tile *tile(const char *name);

  protected:
    // This scans the maps directory to see what levels we have.
    void _scanMapLevels();

    // Returns the chunk mapped at the given location, NULL if none
    // exists.  This is just a convenience routine that wraps STL's
    // find() method.
    Chunk *_chunk(const GeoLocation &loc) const;

    // Paths
    std::vector<SGPath> _sceneryPaths;

    // Map info
    SGPath _maps;
    std::bitset<MAX_MAP_LEVEL> _mapLevels;

    // Chunks
    std::map<GeoLocation, Chunk *> _chunks;
};

// A Chunk object represents a directory of tiles, generally a 10x10
// area.
class Chunk {
  public:
    // The "standard" name of a chunk (eg, "w130n30") containing the
    // given location, which must be valid.  Always 7 characters long.
    // You should copy it if you need to keep it - it comes from
    // static storage and may be overwritten on subsequent calls.
    static const char *name(const GeoLocation &loc);
    // Converts an arbitrary location into a canonical chunk location,
    // which is the SW corner of the chunk containing the given
    // location.  If loc is invalid, so too is the return value.
    static GeoLocation canonicalize(const GeoLocation &loc);
    // Returns true if the chunk specified by the chunk-canonical
    // location exists in FlightGear.  It's not necessary for the
    // chunk to have been downloaded.  A chunk exists if there is any
    // scenery in the area it covers (again, not necessarily scenery
    // that we've downloaded, but scenery that exists and we could
    // download).  Returns false if loc is invalid.
    static bool exists(const GeoLocation &loc);
    // Returns the longitude of the west and east edges of the chunk
    // specified by loc (which must be chunk-canonical), at the
    // latitude specified by lat.
    static void edges(const GeoLocation &loc, int lat, int *west, int *east);

    // Creates a chunk for the given chunk-canonical location.
    Chunk(const GeoLocation &loc, TileManager *tm);
    ~Chunk();

    // The canonical name of this tile.
    const char *name();	      // const?
    // Our canonical latitude and longitude.
    const GeoLocation &loc() const { return _loc; }
    // EYE - latitude, or an offset between 0 and 10?
    void edges(int latitude, int *westEdge, int *eastEdge);

    TileManager *tileManager() { return _tm; }

    // Similar to TileManager's fooCount() and foo() accessors, but
    // for this chunk only.
    int tileCount(TileManager::SceneryType type);
    Tile *tile(TileManager::SceneryType type, int i);

    // Returns the tile of the given latitude and longitude or name,
    // NULL if none exists within this chunk.
    Tile *tile(const GeoLocation &loc) const;
    Tile *tile(const char *name);
    // Tiles in this chunk, referenced by location.
    const std::map<GeoLocation, Tile *>& tiles() const { return _tiles; }

    friend class TileManager;
    friend class Tile;

  protected:
    // Resets all of our tiles.  This is meant to be called from our
    // tile manager.
    void _reset();

    // Scans the given directory for tile directories.  Like _reset(0,
    // this is meant to be called from our tile manager.
    void _scanScenery(SGPath& directory, int pathIndex);

    // Informs us that a tile's mapped state changed.  A tile is
    // unmapped if it has at least one missing map, mapped if all maps
    // have been rendered.  These are meant to be called from one of
    // our tiles, and should only be called from a tile that actually
    // has downloaded scenery (the idea of being mapped or unmapped
    // doesn't make sense without downloaded scenery to render).
    void _tileBecameMapped();
    void _tileBecameUnmapped();

    // Returns the tile mapped at the given location, NULL if none
    // exists.  This is just a convenience routine that wraps STL's
    // find() method.
    Tile *_tile(const GeoLocation &loc) const;

    TileManager *_tm;
    std::map<GeoLocation, Tile *> _tiles;

    // Our canonical latitude and longitude.
    GeoLocation _loc;

    // Pre-calculated values used to speed up tileCount() and tile()
    // operations.  A tile is considered unmapped if at least one map
    // is missing.  For these to be correct, tiles must call us when
    // their mapped status changes.  We use a char because we want to
    // keep the size of a Chunk object as small as possible (and even
    // more so for Tile objects), and we know we'll never have more
    // than 100 tiles per chunk.
    char _downloadedTiles, _unmappedTiles;
};

// A Tile object represents the scenery in one of the subdirectories
// of the scenery tree.  Generally this is an area with a north-south
// extent of 1 degree of latitude and an east-west extent of 1 degree
// of longitude, but at extreme latitudes (83 degrees or higher), one
// subdirectory can contain scenery for 1x2, 1x4, 1x8, or even 1x360
// degree areas.
//
// A tile may have downloaded FlightGear scenery.  If it has scenery,
// it may also have maps (of several different resolutions).
class Tile {
  public:
    // Returns the standard tile width at the given latitude (which
    // must be a canonical latitude, varying between 0 and 179
    // inclusive).
    static int width(int lat);
    // The "standard" name of the tile (eg, "w128n37").  Always 7
    // characters long.  You should copy it if you need to keep it -
    // it comes from static storage and may be overwritten on
    // subsequent calls.  The location is converted to a
    // tile-canonical form.
    static const char *name(const GeoLocation &loc);
    // Converts an arbitrary location into a canonical tile location,
    // which is the SW corner of the tile containing the given
    // location.  If loc is invalid, so too is the return value.
    static GeoLocation canonicalize(const GeoLocation &loc);
    // True if there is a tile specified by the tile-canonical
    // location.  Note that, as for chunks, we don't care if the tile
    // has been downloaded yet or not, only that it *can* be
    // downloaded.  Returns false if loc is invalid.
    static bool exists(const GeoLocation &loc);
    // EYE - do we need a Tile::edges() method?

    // Creates a chunk for the given tile-canonical location.
    Tile(const GeoLocation &loc, TileManager *tm, Chunk *c);
    ~Tile();

    // The canonical name for this tile.  Copy the string if you need
    // to save it.
    const char *name() { return Tile::name(_loc); }
    // True if the tile is of type t.  Note that a tile can have many
    // types - it can exist, it can be downloaded, and it can be
    // mapped (or unmapped).  It is guaranteed to at least exist.
    bool isType(TileManager::SceneryType t);
    // True if there is downloaded FlightGear scenery for this tile.
    bool hasScenery() const { return _sceneryIndex != TileManager::NaPI; }
    // Where we look for buckets for this tile.  If sceneryDir == "",
    // that means we have none.  The returned SGPath is shared amongst
    // all tiles, so copy it if you need to keep it.
    const SGPath& sceneryDir();
    // Where our maps are.  This is the same for all tiles.
    const SGPath& mapsDir() const { return _tm->mapPath(); }
    // Our buckets.  We return the bucket indices only.  To get the
    // complete path, prepend the scenery directory, and append
    // ".stg" or ".stg.gz".
    const std::vector<long int>* bucketIndices();
    
    // A bit set of all *desired* maps.  If the bitset is true at
    // index i, then we want a map i^2 pixels high.
    const std::bitset<TileManager::MAX_MAP_LEVEL>& mapLevels() const
    { return _tm->mapLevels(); }

    // A bitset of all *rendered* maps.
    const std::bitset<TileManager::MAX_MAP_LEVEL>& maps() const 
    { return _maps; }
    // Returns a bitset of all *unrendered* maps.
     const std::bitset<TileManager::MAX_MAP_LEVEL> missingMaps() const 
    { return maps() ^ mapLevels(); }

    // Informs us that a rendered map exists or not.  We trust what we
    // are told, so we don't check the maps directory to see if it's
    // true.  It will tell its owning chunk if its mapped/unmapped
    // status has changed.
    void setMapExists(unsigned int i, bool exists);

    //////////////////////////////
    // Tile metrics.
    //////////////////////////////

    // The canonical location for the tile.  Note that this uses our
    // internal latitude and longitude system (0 <= lat < 180, 0 <=
    // lon < 360).
    const GeoLocation &loc() const { return _loc; }

    // SW corner, width, and height of tile (all units in degrees).
    // Note that these are conventional latitudes and longitudes (-90
    // <= lat < 90, -180 <= lon < 180).
    int lat() const { return _loc.lat() - 90; }
    int lon() const { return (_loc.lon() + 180) % 360 - 180; }
    int width() const { return Tile::width(_loc.lat()); }
    int height() const { return 1; }
    // Centre of tile (also conventional latitudes and longitudes).
    float centreLat() const { return lat() + 0.5; }
    float centreLon() const { return lon() + (width() / 2.0); }
    // Map size, in pixels, at a given resolution.
    void mapSize(unsigned int resolution, int *width, int *height) const;

    friend class Chunk;

  protected:
    // A shared SGPath used by tiles to represent their scenery
    // directory.
    static SGPath __scenery;

    // Tells us that we should assume that none of our desired maps
    // exist (ie, we're missing them all).  This will be called by our
    // chunk before it does a scenery scan.  Later, at the end of the
    // tile manager's scenery scan, it will check the maps directories
    // and tell us which ones we have (with calls to setMapExists()).
    void _resetExists() { _maps.reset(); }

    // Sets our scenery.  This is meant to be called by the
    // TileManager only.
    void _setScenery(int i) { _sceneryIndex = i; }

    TileManager *_tm;		// Our tile manager.
    Chunk *_chunk;		// Our owning chunk.

    int _sceneryIndex;	      // Index into FileManager's paths array.
    std::vector<long int> *_buckets; // Bucket indexes.

    // This keeps track of what maps we have.
    std::bitset<TileManager::MAX_MAP_LEVEL> _maps;

    // True if we have some maps still to be rendered.
    bool _unmapped;

    // SW corner of tile.
    GeoLocation _loc;
};

// A non-STL iterator that allows quick iteration through tiles of a
// specified type.  To use, do something like this:
//
// TileIterator ti(tileManager, TileManager::MAPPED);
// for (Tile *t = ti.first(); t; t = ti++) {
//     // Do something here with t.
// }
//
// The iterator returns NULL from first() and the '++' operator if
// there are no tiles left of that type.  A given iterator can be
// reset by calling first().
// 
// This could probably have been done with STL iterators, but I value
// my sanity and decided not to go there.
class TileIterator {
  public:
    TileIterator(TileManager *tm, TileManager::SceneryType type);
    ~TileIterator();
    Tile *first();
    Tile *operator++(int);
  protected:
    TileManager *_tm;
    TileManager::SceneryType _type;

    // These keep track of our current position as we iterate through
    // all the chunks and their tiles.
    std::map<GeoLocation, Chunk *>::const_iterator _ci;
    Chunk *_c;
    std::map<GeoLocation, Tile *>::const_iterator _ti;
};

#endif	// _TILES_H_
