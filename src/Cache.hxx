/*-------------------------------------------------------------------------
  Cache.hxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

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

#include <plib/sg.h>

// An object that can be placed in a cache.  It must be able to load
// and unload itself.  It has a bounds, and can save a distance (this
// is used when determining which objects to load and unload - nearer
// ones are loaded before farther ones, and farther ones are unloaded
// before nearer ones).
class CacheObject {
  public:
    CacheObject();
    virtual ~CacheObject() = 0;

    virtual void load() = 0;
    virtual void unload() = 0;

    void setCentre(const sgdVec3 centre);
    float dist() const { return _dist; }
    void calcDist(sgdVec3 from);

  protected:
    sgdVec3 _centre;
    float _dist;
};

// Manages the loading and unloading of a set of objects of class
// CacheObject.
class Cache {
  public:
    // Cache is unlimited if cacheSize is 0, all work is done in one
    // go if workTime is 0, and interval is in milliseconds.
    Cache(unsigned int cacheSize = 10, 
	  unsigned int workTime = 10,
	  unsigned int interval = 0);
    ~Cache();

    // These are used, together, to tell the cache which objects are
    // visible, and should be used in the order given.  The reset()
    // call tells the cache the new centre of the displayed area (the
    // cache needs to know this because it uses distance from centre
    // to decide which tiles to load first).  The add() is used to add
    // a single object - call this once for each object you want to
    // load.  After adding all visible objects, call go() to tell the
    // cache to begin loading.
    void reset(sgdVec3 centre);
    void add(CacheObject* c);
    void go();

  protected:
    // Called periodically to load 1 or more tiles
    void _load();
    friend void _cacheTimer(int value);

    // All loaded objects.
    std::set<CacheObject *> _all;
    // Objects which are visible (and which, therefore, must not be
    // unloaded).  These objects will either be in _all or in
    // _toBeLoaded.
    std::set<CacheObject *> _visible;
    // Objects which must still be loaded because they're visible
    // (_toBeLoaded) or unloaded because they're not visible, and
    // we've exceeded the cache size (_toBeUnloaded).  Objects in
    // _toBeLoaded *must not* be in _all, and *must* be in _visible.
    // Objects in _toBeUnloaded *must* be in _all, and *must not* be
    // in _visible.
    std::deque<CacheObject *> _toBeLoaded, _toBeUnloaded;

    // The centre of the area to be displayed.  We use this value to
    // decide which objects to load first.
    sgdVec3 _centre;

    // _all will be the maximum of _cacheSize and _visible.size() (ie,
    // we guarantee to load all visible objects, and then non-visible
    // up to _cacheSize).  Set to 0 if cache size is unlimited.
    unsigned int _cacheSize;
    // How much time to spend in one call to _load() (in ms).
    unsigned int _workTime;
    // Time (in ms) between calls to _load().
    unsigned int _interval;

    // True if we have scheduled a call to _load().
    bool _callbackPending;
    // True if we are active.
    bool _running;
};

#endif _CACHE_H_
