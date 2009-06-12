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

#include "Globals.hxx"

Globals globals;

Globals::Globals(): 
    palette(NULL), regularFont(NULL), boldFont(NULL), magnetic(true), 
    _track(NULL), _currentTrackNo(0)
{
}

Globals::~Globals()
{
    delete regularFont;
    delete boldFont;
}

FlightData *Globals::currentPoint()
{
    if (_track && !_track->empty()) {
	return _track->getCurrentPoint();
    }

    return (FlightData *)NULL;
}

FlightTrack *Globals::track(unsigned int i)
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
unsigned int Globals::addTrack(FlightTrack *t, bool select)
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
    for (unsigned int i = 0; i < _tracks.size(); i++) {
	if (_tracks[i] == _track) {
	    _currentTrackNo = i;
	    break;
	}
    }

    return (_tracks.size() - 1);
}

// Removes the track at the given index .  If out of range, does
// nothing.  Returns the track removed, NULL if nothing is removed.
FlightTrack *Globals::removeTrack(unsigned int i)
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
	    _currentTrackNo = -1;
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
FlightTrack *Globals::setCurrent(unsigned int i)
{
    if ((i >= _tracks.size()) || (i == _currentTrackNo)) {
	// EYE - what if tracks is empty?
	return NULL;
    }

    _currentTrackNo = i;
    _track = _tracks[_currentTrackNo];

    return _track;
}

// Returns true if we already have a network track listening to the
// given port.
unsigned int Globals::exists(int port)
{
    for (unsigned int i = 0; i < _tracks.size(); i++) {
	FlightTrack *t = _tracks[i];
	if (t->isNetwork() && (port == t->port())) {
	    return i;
	}
    }

    return _tracks.size();
}

// Checks if the serial connection described by the device and baud
// rate already exists in _tracks.  Note that the baud rate is ignored
// in determining equivalence.  Why include it?  So that this method
// has a different call signature than the following one.
unsigned int Globals::exists(const char *device, int baud)
{
    for (unsigned int i = 0; i < _tracks.size(); i++) {
	FlightTrack *t = _tracks[i];
	if (t->isSerial() && (strcmp(device, t->device()) == 0)) {
	    return i;
	}
    }

    return _tracks.size();
}

// Checking if two files are the same is actually quite tricky, so we
// solve this problem by just ignoring it.  Let the user beware!
unsigned int Globals::exists(const char *path)
{
    for (unsigned int i = 0; i < _tracks.size(); i++) {
	FlightTrack *t = _tracks[i];
	if (t->hasFile() && (strcmp(path, t->filePath()) == 0)) {
	    return i;
	}
    }

    return _tracks.size();
}

