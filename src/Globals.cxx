/*-------------------------------------------------------------------------
  Globals.cxx

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

#include <algorithm>
#include <limits>

#include "Globals.hxx"
#include "Bucket.hxx"

Globals globals;

Globals::Globals(): 
    regularFont(NULL), boldFont(NULL), magnetic(true), 
    _track(NULL), _currentTrackNo(FlightTrack::npos), _palette(NULL)
{
}

Globals::~Globals()
{
    delete overlays;
    delete regularFont;
    delete boldFont;
}

FlightData *Globals::currentPoint()
{
    if (_track && !_track->empty()) {
	return _track->current();
    }

    return (FlightData *)NULL;
}

FlightTrack *Globals::track(size_t i)
{
    if (i >= _tracks.size()) {
	return NULL;
    }

    return _tracks[i];
}

// A sort operator for tracks.
struct TrackLessThan {
    bool operator()(FlightTrack *a, FlightTrack *b) 
    {
	return strcmp(a->niceName(), b->niceName()) < 0;
    }
};

// Adds the track to the _tracks vector, and returns its index.  If
// select is true, or if this is the only track, also sets '_track'
// and '_currentTrackNo' to the new track.
size_t Globals::addTrack(FlightTrack *t, bool select)
{
    // EYE - check if t is already in the list?
    _tracks.push_back(t);
    if (select || (_tracks.size() == 1)) {
	_track = t;
    }

    // Sort things alphabetically by the tracks' nice names.
    sort(_tracks.begin(), _tracks.end(), TrackLessThan());

    // Since things may have moved around, _currentTrackNo could be
    // invalid.
    for (size_t i = 0; i < _tracks.size(); i++) {
	if (_tracks[i] == _track) {
	    _currentTrackNo = i;
	    break;
	}
    }

    return _currentTrackNo;
}

// Removes the track at the given index .  If out of range, does
// nothing.  Returns the track removed, NULL if nothing is removed.
FlightTrack *Globals::removeTrack(size_t i)
{
    FlightTrack *result = NULL;
    if (i >= _tracks.size()) {
	// If i is out of range, then don't do anything.
	return result;
    }

    // Remove the track.
    result = _tracks[i];
    _tracks.erase(_tracks.begin() + i);

    // Update currentTrackNo and track.
    if (result == _track) {
	// We removed the current track, so make a new one current.
	if (_tracks.empty()) {
	    _currentTrackNo = FlightTrack::npos;
	    _track = NULL;
	} else if (_currentTrackNo >= _tracks.size()) {
	    _currentTrackNo = _tracks.size() - 1;
	    _track = _tracks[_currentTrackNo];
	} else {
	    _track = _tracks[_currentTrackNo];
	}
    }

    return result;
}

// Sets the current track to the one at the given index.  If nothing
// changes, it returns NULL, otherwise it returns the track at that
// index.
FlightTrack *Globals::setCurrent(size_t i)
{
    if ((i >= _tracks.size()) || (i == _currentTrackNo)) {
	return NULL;
    }

    _currentTrackNo = i;
    _track = _tracks[_currentTrackNo];

    return _track;
}

// If we already have a network track listening to the given port,
// return the track in question, otherwise return NULL.  If 'select'
// is true, it also makes the given track current.
FlightTrack *Globals::exists(int port, bool select)
{
    for (size_t i = 0; i < _tracks.size(); i++) {
	FlightTrack *t = _tracks[i];
	if (t->isNetwork() && (port == t->port())) {
	    if (select) {
		return setCurrent(i);
	    } else {
		return track(i);
	    }
	}
    }

    return NULL;
}

// Checks if the serial connection described by the device and baud
// rate already exists in _tracks.  Note that the baud rate is ignored
// in determining equivalence.  Why include it?  So that this method
// has a different call signature than the following one.
FlightTrack *Globals::exists(const char *device, int baud, bool select)
{
    for (size_t i = 0; i < _tracks.size(); i++) {
	FlightTrack *t = _tracks[i];
	if (t->isSerial() && (strcmp(device, t->device()) == 0)) {
	    if (select) {
		return setCurrent(i);
	    } else {
		return track(i);
	    }
	}
    }

    return NULL;
}

// Checking if two files are the same is actually quite tricky, so we
// solve this problem by just ignoring it.  Let the user beware!
FlightTrack *Globals::exists(const char *path, bool select)
{
    for (size_t i = 0; i < _tracks.size(); i++) {
	FlightTrack *t = _tracks[i];
	if (t->hasFile() && (strcmp(path, t->filePath()) == 0)) {
	    if (select) {
		return setCurrent(i);
	    } else {
		return track(i);
	    }
	}
    }

    return NULL;
}

void Globals::setPalette(Palette *p)
{
    _palette = p;
    // The Bucket class tracks the current palette as well.
    Bucket::palette = p;
}
