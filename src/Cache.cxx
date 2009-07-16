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

// Static map between Cache instance ID's and their addresses.  This
// is used to figure out which one to call when the GLUT timer fires.
map<int, Cache *> Cache::__map;

CacheObject::CacheObject()
{
}

CacheObject::~CacheObject()
{
}

// More C++ hoops to jump through.
struct CacheObjectLessThan {
    bool operator()(CacheObject *a, CacheObject *b) 
    {
	return a->dist() < b->dist();
    }
};

Cache::Cache(int window,
	     unsigned int cacheSize, 
	     unsigned int workTime, 
	     unsigned int interval):
    _window(window), _cacheSize(cacheSize), _objectsSize(0), 
    _workTime(workTime), _interval(interval), _callbackPending(false), 
    _running(false)
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

// Reset the cache in preparation for a new set of objects.  We update
// all loaded objects and we forget everything we knew about what
// needs to be loaded and what needs to be unloaded.  We also stop all
// cache processing.  We do *not* unload anything - all loaded objects
// will remain loaded for the time-being.
void Cache::reset(sgdVec3 centre)
{
    sgdCopyVec3(_centre, centre);

    set<CacheObject *>::const_iterator i;
    for (i = _all.begin(); i != _all.end(); i++) {
	(*i)->_toBeLoaded = false;
	(*i)->_toBeUnloaded = true;
	(*i)->calcDist(_centre);
    }

    _toBeLoaded.clear();
    _toBeUnloaded.clear();

    _running = false;
}

// Add an object for loading.  This should only be done between a
// reset() and a go().  If this is done after go(), nothing is done.
// When we add an object, we call its shouldLoad() method, which
// should return true if the object wants its load() method to be
// called.
void Cache::add(CacheObject* c)
{
    if (_running) {
	// EYE - print warning?
	return;
    }

    c->_toBeUnloaded = false;
    if (c->shouldLoad()) {
	c->calcDist(_centre);
	c->_toBeLoaded = true;
	_toBeLoaded.push_back(c);
    }
}

// Prepares the cache for loading to commence.  We create the
// _toBeLoaded and _toBeUnloaded deques (they probably don't need to
// be deques, but it makes my life a bit easier), sort them by
// distances, then start the timer.
void Cache::go()
{
    set<CacheObject *>::const_iterator i;
    for (i = _all.begin(); i != _all.end(); i++) {
	CacheObject *c = *i;
	if (c->_toBeUnloaded) {
	    _toBeUnloaded.push_back(c);
	}
    }

    // Sort things by decreasing distance from centre.  We load
    // central objects first.
    sort(_toBeLoaded.begin(), _toBeLoaded.end(), CacheObjectLessThan());
    sort(_toBeUnloaded.begin(), _toBeUnloaded.end(), CacheObjectLessThan());

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

// The routine that does the real work.  It is called periodically by
// the timer callback (_cacheTimer).  Each time it is called, it
// unloads as many objects as necessary (and possible) to bring us
// under our limit (_cacheSize), then loads as many objects as it can
// within our time limit (_workTime).
void Cache::_load()
{
    _callbackPending = false;

    // Don't do anything if _running is false (that means we're in the
    // midst of adding new objects).
    if (!_running) {
	return;
    }

    // Make sure that we're 'in' the right window.
    int oldWindow = glutGetWindow();
    glutSetWindow(_window);

    // Unload what we can and should unload.  That means: unload stuff
    // if we have a cache limit AND we are over the limit AND there is
    // stuff to unload.
    if (_cacheSize > 0) {
	while ((_objectsSize > _cacheSize) && !_toBeUnloaded.empty()) {
	    // Unload farthest object.
	    CacheObject *c = _toBeUnloaded.back();

	    _objectsSize -= c->size();
	    if (c->unload()) {
		_toBeUnloaded.pop_back();
		_all.erase(c);
	    }
	    _objectsSize += c->size();
	}
    }

    // If there's nothing left to load, we're done.
    if (_toBeLoaded.empty()) {
	glutSetWindow(oldWindow);
	return;
    }

    // Do some work.
    SGTimeStamp t1, t2;
    long microSeconds = _workTime * 1000;
    t1.stamp();
    do {
	// Load nearest object.
	CacheObject *c = _toBeLoaded.front();

	// Note the slight difference from the logic in the unload
	// section above.  If something is in _all, that means it is
	// at least partially loaded and will need to be unloaded in
	// the future.  So, we add it as soon as we can here, whereas
	// in the unload section, we only remove it if it has been
	// completely unloaded.
	_all.insert(c);
	_objectsSize -= c->size();
	if (c->load()) {
	    _toBeLoaded.pop_front();
	}
	_objectsSize += c->size();

	t2.stamp();
    } while ((_toBeLoaded.size() > 0) && ((t2 - t1) < microSeconds));

    // Return to the old window.
    glutPostWindowRedisplay(_window);
    glutSetWindow(oldWindow);

    // Set up the timer for another callback.  Note that we do this
    // even if there's nothing to be loaded, as there may still be
    // stuff to unload.
    glutTimerFunc(_interval, _cacheTimer, _id);
    _callbackPending = true;
}
