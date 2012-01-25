/*-------------------------------------------------------------------------
  Cache.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2012 Brian Schack

  A cache manages the loading and unloading of cache objects, which
  exist in 3D space.  The cache guarantees to load all the objects you
  ask for.  It may, if it feels it has enough space, also maintain
  "stale" objects.  Objects are loaded asynchronously (using a timer)
  so as not to slow the caller down unduly.  The cache knows about the
  centre of interest, and tries to load objects closest to that point
  first, and when it removes stale objects, removes those farthest
  away first.

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

#ifndef _CACHE_H_
#define _CACHE_H_

#include <set>
#include <deque>
#include <map>

#include <plib/sg.h>

// An object that can be placed in a cache.  It must be able to load
// and unload itself.  It has a bounds, and can save a distance (this
// is used when determining which objects to load and unload - nearer
// ones are loaded before farther ones, and farther ones are unloaded
// before nearer ones).
//
// A CacheObject must implement 5 routines:
//
// void calcDist() - set _dist to the distance between the CacheObject
//   and the given point.
//
// bool shouldLoad() - this is called when a CacheObject is added to a
//   Cache, but before loading starts.  Return true if you want your
//   load() method to be called.
//
// bool load() - this is called when the Cache determines that the
//   CacheObject should be loaded.  Return false if you want to be
//   called again (eg, you've divided loading into multiple steps).
//   Return true when loading is complete.
// 
// bool unload() - similar to load(), but for unloading.  Like load(),
//   it should return true if it's done unloading.  It's probably
//   silly to give it the same return-value semantics as load()
//   (unloading should always be fast and never need to be broken down
//   into multiple steps), but I didn't want to destroy the symmetry.
//
// unsigned int size() - size of CacheObject, in bytes.  Note that it
//   isn't necessary to be completely accurate.  This is just used by
//   the Cache to get a rough idea of how much it has loaded into
//   memory.
//
class CacheObject {
  public:
    friend class Cache;

    CacheObject();
    virtual ~CacheObject() = 0;

    virtual void calcDist(sgdVec3 centre) = 0;
    virtual bool shouldLoad() = 0;
    virtual bool load() = 0;
    virtual bool unload() = 0;
    // EYE - unsigned long?
    virtual unsigned int size() = 0;

    float dist() const { return _dist; }

  protected:
    // The distance from the centre of the Cache 'region' to the
    // CacheObject.  This should be set in calcDist().
    float _dist;

    // These are meant to be touched only by the Cache object - they
    // help it keep track of who needs to be loaded and unloaded.
    bool _toBeLoaded, _toBeUnloaded;
};

// Manages the loading and unloading of a set of objects of class
// CacheObject.
class Cache {
  public:
    // The window for which we want objects loaded is given in window.
    // The maximum size of the cache (in bytes) is cacheSize, although
    // it will exceed that limit if all objects are visible.  If
    // cacheSize is 0, there is no limit.  On each call to _load(), it
    // works workTime milliseconds.  If workTime is 0, it will load
    // everything in one go.  The number of milliseconds between calls
    // to _load() is given by interval.
    Cache(int window,
	  unsigned int cacheSize = 50 * 1024 * 1024, // 50 MB
	  unsigned int workTime = 10,		     // 10 ms
	  unsigned int interval = 0);		     // 0 ms
    ~Cache();

    // These are used, together, to tell the cache which objects are
    // visible, and should be used in the order given.  
    //
    // The reset() call tells the cache the new centre of the
    // displayed area (the cache needs to know this because it uses
    // distance from centre to decide which tiles to load first).
    // When called, the cache will stop all processing, and assume
    // that no objects are visible.  It will *not* unload any objects
    // however.
    // 
    // The add() is used to add a single object - call this once for
    // each object you want to load (or, in other words, are visible).
    //
    // After adding all visible objects, call go() to tell the cache
    // to begin loading.
    //
    void reset(sgdVec3 centre);
    void add(CacheObject* c);
    void go();

  protected:
    // The GLUT window we're loading objects for.  The cache was
    // written for loading textures and buckets asynchronously
    // (although I suppose it could be used for other things).  As I
    // understand it, we need to make sure we've got the right OpenGL
    // context current before we load a texture.  Because texture
    // loading is asynchronous, any window could be current when
    // _load() is called.  We need to ensure it's the right one.
    // (Note that I'm hypothesizing a bit here; there may be other
    // explanations or better solutions to this problem).
    int _window;				

    // Called periodically to load 1 or more tiles
    void _load();
    static void _cacheTimer(int id);

    // All loaded objects (or, to put it another way, all objects
    // which have not been completely unloaded).
    std::set<CacheObject *> _all;
    // Objects which must still be loaded because they're visible
    // (_toBeLoaded) or unloaded because they're not visible, and
    // we've exceeded the cache size (_toBeUnloaded).  Objects in
    // _toBeUnloaded *must* be in _all.
    std::deque<CacheObject *> _toBeLoaded, _toBeUnloaded;

    // The centre of the area to be displayed.  We use this value to
    // decide which objects to load first.
    sgdVec3 _centre;

    // _cacheSize is the maximum desired cache size; _objectsSize is
    // the actual size of the objects we're managing.  We guarantee to
    // load all visible objects, and then non-visible up to
    // _cacheSize.  Set _cacheSize to 0 if cache size is unlimited.
    unsigned int _cacheSize, _objectsSize;
    // How much time to spend in one call to _load() (in ms).
    unsigned int _workTime;
    // Time (in ms) between calls to _load().
    unsigned int _interval;

    // True if we have scheduled a call to _load().
    bool _callbackPending;
    // True if we are active.
    bool _running;

    // Used to map between Cache instances and their addresses.
    static std::map<int, Cache *> __map;
    int _id;
};

#endif // _CACHE_H_
