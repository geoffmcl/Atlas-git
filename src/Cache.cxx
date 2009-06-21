/*-------------------------------------------------------------------------
  Cache.cxx

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

#if defined( __APPLE__)
#  include <GLUT/glut.h>	// Needed for glutTimerFunc().
#else
#  ifdef WIN32
#    include <windows.h>
#  endif
#  include <GL/glut.h>
#endif

#include <cassert>
#include <algorithm>

#include <simgear/timing/timestamp.hxx>

#include "Cache.hxx"

using namespace std;

// EYE - for debugging only
extern int main_window;

// Static map between Cache instance ID's and their addresses.
map<int, Cache *> Cache::__map;

CacheObject::CacheObject(): _dist(0.0)
{
}

CacheObject::~CacheObject()
{
}

void CacheObject::setCentre(const sgdVec3 centre)
{
    sgdCopyVec3(_centre, centre);
}

void CacheObject::calcDist(sgdVec3 from)
{
    _dist = sgdDistanceSquaredVec3(from, _centre);
}

// More C++ hoops to jump through.
struct CacheObjectLessThan {
    bool operator()(CacheObject *a, CacheObject *b) 
    {
	return a->dist() < b->dist();
    }
};

Cache::Cache(unsigned int cacheSize, 
	     unsigned int workTime, 
	     unsigned int interval):
    _cacheSize(cacheSize), _workTime(workTime),
    _interval(interval), _callbackPending(false), _running(false)
{
    // Get a valid id for ourselves and add ourselves to the map.
    // This is a bit inefficient, but caches won't be created very
    // often, nor will there be very many.
    for (_id = 0; __map.find(_id) != __map.end(); _id++) {
    }
    __map[_id] = this;
}

Cache::~Cache()
{
    // Remove ourselves from the map.
    __map.erase(_id);
}

const set<CacheObject *> operator-(const set<CacheObject *>& a, 
				   const set<CacheObject *>& b)
{
    set<CacheObject *> c;
    set_difference(a.begin(), a.end(), b.begin(), b.end(), 
		   inserter(c, c.begin()));

    return c;
}

// Reset the cache.  This means that we forget everything we knew
// about what's visible, what needs to be loaded, and what needs to be
// unloaded.  We also stop all cache processing.  We do *not* unload
// anything - all loaded objects will remain loaded for the
// time-being.
void Cache::reset(sgdVec3 centre)
{
    _visible.clear();
    _toBeLoaded.clear();
    _toBeUnloaded.clear();

    sgdCopyVec3(_centre, centre);

    _running = false;
}

// Add an object.  This should only be done between a reset() and a
// go().  If this is done after go(), nothing is done.
void Cache::add(CacheObject* c)
{
    if (_running) {
	// EYE - print warning?
	return;
    }

    // We set the distance to help us decide which objects to load
    // (and unload) first.
    c->calcDist(_centre);
    _visible.insert(c);

    // Schedule for loading if it hasn't been loaded already.
    if (_all.find(c) == _all.end()) {
	_toBeLoaded.push_back(c);
    }
}

// Prepares the cache for loading to commence.
void Cache::go()
{
    assert(_visible.size() <= (_toBeLoaded.size() + _all.size()));

    // Sort things by decreasing distance from centre.  We load
    // central objects first.
    sort(_toBeLoaded.begin(), _toBeLoaded.end(), CacheObjectLessThan());

    // These new objects might put us over our limit.
    if (_cacheSize > 0) {
	// Check if we're within our limits.  If not, find objects to
	// delete.  First, figure out what we would grow to if we
	// loaded all the new objects.
	unsigned int maxSize = _toBeLoaded.size() + _all.size();
	if (maxSize <= _cacheSize) {
	    // Everything will fit.
	} else if (_visible.size() == maxSize) {
	    // Everything's visible - nothing can be deleted.
	} else {
	    // Find out which objects aren't visible (ie, are
	    // candidates for unloading).
	    set<CacheObject *>notVisible = _all - _visible;

	    // Copy into _toBeUnloaded and sort by distance.
	    copy(notVisible.begin(), notVisible.end(), 
		 inserter(_toBeUnloaded, _toBeUnloaded.begin()));
	    sort(_toBeUnloaded.begin(), _toBeUnloaded.end(), 
		 CacheObjectLessThan());

	    // Find out how much we can shrink.  We'd like to shrink
	    // to _cacheSize, but only if we can keep all visible
	    // objects.
	    int minSize = max(_cacheSize, (unsigned int)_visible.size());
	    for (int overrun = maxSize - minSize; overrun > 0; overrun--) {
		// Unload farthest object.
		CacheObject *c = _toBeUnloaded.back();
		_toBeUnloaded.pop_back();

		c->unload();
		_all.erase(c);
	    }
	}
    }

    // If the timer isn't going already, then start things going.
    if (!_callbackPending) {
	// EYE - use idle timer instead?  (And in search stuff?)
	glutTimerFunc(_interval, _cacheTimer, _id);
	_callbackPending = true;
    }

    _running = true;
}

// Called periodically by the glutTimerFunc().
void Cache::_cacheTimer(int id)
{
    // Look up the id in the id->address map and call _load().
    assert(__map.find(id) != __map.end());
    Cache *c = __map[id];
    c->_load();
}

void Cache::_load()
{
    _callbackPending = false;

    // Don't do anything if _running is false (that means we're in the
    // midst of adding new objects), or if there's nothing to load.
    if ((!_running) || (_toBeLoaded.size() == 0)) {
	return;
    }

    // Do some work.
    SGTimeStamp t1, t2;
    long microSeconds = _workTime * 1000;
    t1.stamp();
    do {
	// Load nearest object.
	CacheObject *c = _toBeLoaded.front();
	_toBeLoaded.pop_front();

	c->load();
	_all.insert(c);

	// EYE - this assumes that we're drawing in main_window
	// (should be parameterized somehow, or perhaps made into a
	// notification).
	glutPostWindowRedisplay(main_window);

	t2.stamp();
    } while ((_toBeLoaded.size() > 0) && ((t2 - t1) < microSeconds));

    glutTimerFunc(_interval, _cacheTimer, _id);
    _callbackPending = true;
}
