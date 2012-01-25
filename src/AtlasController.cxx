/*-------------------------------------------------------------------------
  AtlasController.cxx

  Written by Brian Schack

  Copyright (C) 2012 Brian Schack

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

#include "AtlasController.hxx"
#include "NavData.hxx"
#include "Notifications.hxx"
#include "Preferences.hxx"

using namespace std;

//////////////////////////////////////////////////////////////////////
// Palettes
//////////////////////////////////////////////////////////////////////
const size_t Palettes::NaP = std::numeric_limits<size_t>::max();

Palettes::Palettes(const char *paletteDir): _i(NaP)
{
    // Find out what palettes are pre-installed and put them in the
    // 'palettes' vector.
    ulDir *dir = ulOpenDir(paletteDir);
    ulDirEnt *entity;
    while (dir && (entity = ulReadDir(dir))) {
	// All palettes should have the suffix ".ap".
	char *suffix = entity->d_name + strlen(entity->d_name) - strlen(".ap");
	if (!entity->d_isdir && (strcmp(suffix, ".ap") == 0)) {
	    SGPath p(paletteDir);
	    p.append(entity->d_name);

	    try {
		Palette *aPalette = new Palette(p.c_str());
		_palettes.push_back(aPalette);
	    } catch (runtime_error e) {
		// EYE - throw an error instead?
		fprintf(stderr, "Palettes::Palettes: couldn't load '%s'", 
			p.c_str());
	    }
	}
    }
    ulCloseDir(dir);
}

Palettes::~Palettes()
{
    for (size_t i = 0; i < _palettes.size(); i++) {
	delete _palettes[i];
    }
    _palettes.clear();
}

Palette *Palettes::at(size_t i)
{
    if (i < _palettes.size()) {
	return _palettes[i];
    } else {
	return NULL;
    }
}

void Palettes::setCurrentNo(size_t i)
{
    if (i < _palettes.size()) {
	_i = i;
    } else {
	_i = NaP;
    }
}

size_t Palettes::find(const char *name)
{
    AtlasString s(name);
    for (size_t i = 0; i < _palettes.size(); i++) {
	SGPath full(_palettes[i]->path());
	if (strcmp(s.str(), full.file().c_str()) == 0) {
	    return i;
	}
    }

    // We couldn't find a match, so try again with ".ap" tacked onto
    // the end.
    s.appendf(".ap");
    for (size_t i = 0; i < _palettes.size(); i++) {
	SGPath full(_palettes[i]->path());
	if (strcmp(s.str(), full.file().c_str()) == 0) {
	    return i;
	}
    }

    return NaP;
}

//////////////////////////////////////////////////////////////////////
// FlightTracks
//////////////////////////////////////////////////////////////////////
const size_t FlightTracks::NaFT = std::numeric_limits<size_t>::max();

FlightTracks::FlightTracks(): _i(NaFT)
{
}

FlightTracks::~FlightTracks()
{
    for (size_t i = 0; i < _flightTracks.size(); i++) {
	delete _flightTracks[i];
    }
    _flightTracks.clear();
}

FlightTrack *FlightTracks::at(size_t i)
{
    if (i < _flightTracks.size()) {
	return _flightTracks[i];
    } else {
	return NULL;
    }
}

void FlightTracks::setCurrentNo(size_t i)
{
    if (i < _flightTracks.size()) {
	_i = i;
    } else {
	_i = NaFT;
    }
}

size_t FlightTracks::find(int port)
{
    for (size_t i = 0; i < _flightTracks.size(); i++) {
	FlightTrack *t = _flightTracks[i];
	if (t->isNetwork() && (port == t->port())) {
	    return i;
	}
    }

    return NaFT;
}

size_t FlightTracks::find(const char *device, int baud)
{
    for (size_t i = 0; i < _flightTracks.size(); i++) {
	FlightTrack *t = _flightTracks[i];
	if (t->isSerial() && (strcmp(device, t->device()) == 0)) {
	    return i;
	}
    }

    return NaFT;
}

size_t FlightTracks::find(const char *path)
{
    for (size_t i = 0; i < _flightTracks.size(); i++) {
	FlightTrack *t = _flightTracks[i];
	if (t->hasFile() && (strcmp(path, t->filePath()) == 0)) {
	    return i;
	}
    }

    return NaFT;
}

// A sort operator for tracks.
struct __TrackLessThan {
    bool operator()(FlightTrack *a, FlightTrack *b) 
    {
	return strcmp(a->niceName(), b->niceName()) < 0;
    }
};

// Adds the track to the _flightTracks vector.  It becomes the current
// flight track.
void FlightTracks::add(FlightTrack *t)
{
    // EYE - check if t is already in the list?
    _flightTracks.push_back(t);

    // Sort things alphabetically by the tracks' nice names.
    sort(_flightTracks.begin(), _flightTracks.end(), __TrackLessThan());

    // Since things may have moved around, find out where the track
    // has gone and return its index.
    for (size_t i = 0; i < _flightTracks.size(); i++) {
	if (_flightTracks[i] == t) {
	    _i = i;
	    break;
	}
    }

    assert(_i != NaFT);
}

void FlightTracks::remove(size_t i)
{
    if (i >= _flightTracks.size()) {
	// If i is out of range, then don't do anything.
	return;
    }

    // Remove the track.
    _flightTracks.erase(_flightTracks.begin() + i);

    // Update _i if we're removing the current track.
    if (i == _i) {
	// We removed the current track, so make a new one current.
	if (_flightTracks.empty()) {
	    _i = NaFT;
	} else if (_i >= _flightTracks.size()) {
	    _i = _flightTracks.size() - 1;
	}
    }
}

//////////////////////////////////////////////////////////////////////
// AtlasController
//////////////////////////////////////////////////////////////////////
AtlasController::AtlasController(const char *paletteDir)
{
    // Create a tile manager.  In its creator it will see which scenery
    // directories we have, and whether there are maps generated for
    // those directories.
    _tm = new TileManager(globals.prefs.scenery_root, globals.prefs.path);
    if (_tm->mapLevels().none()) {
	// EYE - magic numbers
	bitset<TileManager::MAX_MAP_LEVEL> levels;
	levels[4] = true;
	levels[6] = true;
	levels[8] = true;
	levels[9] = true;
	levels[10] = true;
	_tm->setMapLevels(levels);
    }

    // EYE - put inside a try block (see Atlas.cxx)
    _palettes = new Palettes(paletteDir);
    // EYE - is this notification necessary?  Perhaps the Palettes
    // class shouldn't load anything in its creator.
    Notification::notify(Notification::PaletteList);

    // Create an initially empty list of flight tracks.
    _flightTracks = new FlightTracks;

    // Create a searcher object.  It will contain all strings that we
    // can search on (ie, navaid names, navaid ids, airports, ...)
    _searcher = new Searcher();

    // Load our navaid and airport data (and add strings to the
    // Searcher object).
    _navData = new NavData(globals.prefs.fg_root.c_str(), _searcher);

    // EYE - should this and the previous defaults be command-line
    // options?  Should we also initialize them by passing in the
    // Preferences object?
    setShowTrackInfo(true);
    setDegMinSec(true);
    setMagTrue(true);
    setMEFs(true);
    // Set our current palette.  We try:
    //
    // (1) The palette specified in preferences
    //
    // (2) A palette named "default" (or "default.ap")
    //
    // (3) The first palette

    // EYE - use try?  Does the above search policy belong here?
    globals.str.printf("%s", globals.prefs.palette);
    size_t i = _palettes->find(globals.str.str());
    if (i == Palettes::NaP) {
	fprintf(stderr, "Failed to read palette '%s'\n", globals.prefs.palette);

	// If we can't find the specified palette, we fall back to
	// "default".
	const char *def = "default";
	i = _palettes->find(def);
	if (i == Palettes::NaP) {
	    fprintf(stderr, "Failed to read palette '%s'\n", def);
	    i = 0;
	}
    }
    setCurrentPalette(i);

    setContourLines(globals.prefs.contourLines);
    setDiscreteContours(globals.prefs.discreteContours);
    setLightingOn(globals.prefs.lightingOn);
    setSmoothShading(globals.prefs.smoothShading);
    setAzimuth(globals.prefs.azimuth);
    setElevation(globals.prefs.elevation);
}

AtlasController::~AtlasController()
{
    delete _navData;
    // EYE - check if searcher cleans up after itself completely
    delete _searcher;
    delete _flightTracks;
    delete _palettes;
    delete _tm;
}

void AtlasController::setDiscreteContours(bool b)
{
    if (_discreteContours != b) {
	_discreteContours = b;
	// EYE - I think this is necessary.
	Bucket::discreteContours = _discreteContours;
	// EYE - Scenery tile objects subscribe to this.  Should we
	// inform them directly?
	Notification::notify(Notification::DiscreteContours);
    }
}

void AtlasController::setContourLines(bool b)
{
    if (_contourLines != b) {
	_contourLines = b;
	Bucket::contourLines = _contourLines;
	Notification::notify(Notification::ContourLines);
    }
}

void AtlasController::setLightingOn(bool b)
{
    if (_lightingOn != b) {
	_lightingOn = b;
	Notification::notify(Notification::LightingOn);
    }
}

void AtlasController::setSmoothShading(bool b)
{
    if (_smoothShading != b) {
	_smoothShading = b;
	Notification::notify(Notification::SmoothShading);
    }
}

void AtlasController::setAzimuth(float f)
{
    if (_azimuth != f) {
	_azimuth = f;
	Notification::notify(Notification::Azimuth);
    }
}

void AtlasController::setElevation(float f)
{
    if (_elevation != f) {
	_elevation = f;
	Notification::notify(Notification::Elevation);
    }
}

void AtlasController::setOversampling(int i)
{
    if (_oversampling != i) {
	_oversampling = i;
	Notification::notify(Notification::Oversampling);
    }
}

void AtlasController::setImageType(TileMapper::ImageType it)
{
    if (_imageType != it) {
	_imageType = it;
	Notification::notify(Notification::ImageType);
    }
}

void AtlasController::setJPEGQuality(int i)
{
    if (_JPEGQuality != i) {
	_JPEGQuality = i;
	Notification::notify(Notification::JPEGQuality);
    }
}

void AtlasController::setMapLevels(bitset<TileManager::MAX_MAP_LEVEL>& levels)
{
    _tm->setMapLevels(levels);
    Notification::notify(Notification::SceneryChanged);
}

void AtlasController::setCurrentPalette(size_t i)
{
    if (i >= _palettes->size()) {
	return;
    }
    _palettes->setCurrentNo(i);
    Palette *newPalette = _palettes->current();
    Bucket::palette = newPalette;
    Notification::notify(Notification::Palette);
}

void AtlasController::setPaletteBase(double elev)
{
    if (currentPalette()->base() != elev) {
	currentPalette()->setBase(elev);
	Notification::notify(Notification::Palette);
    }
}

void AtlasController::setDegMinSec(bool degMinSec)
{
    if (degMinSec != _degMinSec) {
	_degMinSec = degMinSec;
	Notification::notify(Notification::DegMinSec);
    }
}

void AtlasController::setMagTrue(bool magTrue)
{
    if (magTrue != _magTrue) {
	_magTrue = magTrue;
	Notification::notify(Notification::MagTrue);
    }
}

void AtlasController::setMEFs(bool MEFs)
{
    if (MEFs != _MEFs) {
	_MEFs = MEFs;
	Notification::notify(Notification::MEFs);
    }
}

FlightData *AtlasController::currentPoint()
{
    FlightData *result = NULL;
    FlightTrack *t = currentTrack();
    if (t) {
	result = t->current();
    } 
    return result;
}

void AtlasController::setMark(size_t i)
{
    FlightTrack *t = currentTrack();
    if (t && (i != t->mark())) {
	t->setMark(i);
	Notification::notify(Notification::AircraftMoved);
    }
}

void AtlasController::setCurrentTrack(size_t i)
{
    // Set the current track to the ith track.  If it's different than
    // the previous current track and in range, then update ourselves.
    if ((i != currentTrackNo()) && (i < tracks().size())) {
	_flightTracks->setCurrentNo(i);
	Notification::notify(Notification::NewFlightTrack);
    }
}

void AtlasController::setShowTrackInfo(bool b)
{
    if (b != _showTrackInfo) {
	_showTrackInfo = b;
	Notification::notify(Notification::ShowTrackInfo);
    }
}

void AtlasController::setTrackLimit(int limit)
{
    assert(currentTrack());
    assert(currentTrack()->live());
    assert(limit >= 0);

    currentTrack()->setMaxBufferSize(limit);
}

void AtlasController::addTrack(FlightTrack *track)
{
    _flightTracks->add(track);

    // Tell everyone the list has changed and that a new flight track
    // has been chosen.
    Notification::notify(Notification::FlightTrackList);
    Notification::notify(Notification::NewFlightTrack);
}

void AtlasController::loadTrack(const char *fileName)
{
    // Look to see if we've already loaded that flight track.
    if (find(fileName) == FlightTracks::NaFT) {
	// We didn't find the track, so load it.
	try {
	    FlightTrack *t = new FlightTrack(_navData, fileName);
	    // Set the mark aircraft to the beginning of the track.
	    t->setMark(0);
	    // Add track (selected) and display it.
	    addTrack(t);
	} catch (runtime_error e) {
	    // EYE - beep? dialog box? console message?  Should this
	    // be caught in AtlasWindow rather than here?
	    printf("Failed to read flight file '%s'\n", fileName);
	}
    }
}

void AtlasController::clearTrack()
{
    // EYE - disallow clears of file-based flight tracks?
    FlightTrack *t = currentTrack();
    if (t != NULL) {
	t->clear();

	Notification::notify(Notification::FlightTrackModified);
    }
}

void AtlasController::saveTrack()
{
    FlightTrack *t = currentTrack();
    if (t && t->hasFile()) {
	t->save();
	
	// Saving a flight track might change its display name (the
	// name returned by niceName()), so we need to tell observers
	// that it has been modified.
	Notification::notify(Notification::FlightTrackModified);
    }
}

void AtlasController::saveTrackAs(char *fileName)
{
    FlightTrack *t = currentTrack();
    if (t) {
	// Set the file name and save the track.
	t->setFilePath(fileName);
	t->save();

	// This is a bit of a hack.  If we do a 'save as', then the
	// name of the track will probably change.  Among other
	// things, this means we should resort the tracks list, as
	// held in globals.  One way to do this is to remove the
	// track, then add it again (addTrack() does a sort).
	_flightTracks->removeCurrent();
	_flightTracks->add(t);

	// EYE - are both of these necessary?  Presumably only
	// FlightTrackList is sufficient (as well as being necessary).
	Notification::notify(Notification::FlightTrackList);
	Notification::notify(Notification::FlightTrackModified);
    }
}

void AtlasController::removeTrack()
{
    FlightTrack *t = currentTrack();
    if (t) {
	_flightTracks->removeCurrent();
	delete t;

	// Tell everyone the list has changed and that we've chosen a
	// new flight track.
	Notification::notify(Notification::FlightTrackList);
	Notification::notify(Notification::NewFlightTrack);
    }
}

// EYE - haven't checked yet
void AtlasController::detachTrack()
{
    // EYE - check out this problem: attach, clear, detach, attach -
    // it gives a warning about not being saved, then when the second
    // attach occurs, the time scale duplicates the old one (probably
    // best to try this when the traffic manager is on and screwing up
    // times).
    FlightTrack *t = currentTrack();
    if (!t || !t->live()) {
	return;
    }
    assert(t->isNetwork() || t->isSerial());

    if (t->size() == 0) {
	removeTrack();
    } else {
	t->detach();
	t->setMark(0);

	// It's not really a new flight track, but we do change its
	// name.
	Notification::notify(Notification::NewFlightTrack);
    }
}

void AtlasController::checkForInput()
{
    // Check for input on all live tracks.
    for (unsigned int i = 0; i < tracks().size(); i++) {
	FlightTrack *t = trackAt(i);
	// We consider a track "synced" (the mark should stay at the
	// end) if the mark is currently at the end.
	bool synced = (t->current() == t->last());
	if (t->live() && t->checkForInput()) {
	    // We got some new data.  Is this track is the currently
	    // displayed track?
	    if (t == currentTrack()) {
		// If we're synced, place the mark at the end.
		if (synced) {
		    t->setMark(t->size() - 1);

		    // Register that the aircraft has moved.
		    Notification::notify(Notification::AircraftMoved);
		}

		// Register that the flight track has been modified.
		Notification::notify(Notification::FlightTrackModified);
	    }
	}
    }
}
