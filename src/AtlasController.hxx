/*-------------------------------------------------------------------------
  AtlasController.hxx

  Written by Brian Schack

  Copyright (C) 2012 Brian Schack

  This is the main Atlas controller.  It acts as the intermediary
  between "model" (data) objects, such as the navaids data, and the
  "view" objects, such as the main window and the graphs window.  It
  is the "controller" part of the model-view-controller (MVC)
  paradigm.

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

#ifndef _ATLAS_CONTROLLER_H
#define _ATLAS_CONTROLLER_H

#include "TileMapper.hxx"
#include "Searcher.hxx"
#include "Tiles.hxx"
#include "Globals.hxx"
#include "Geographics.hxx"
#include "Notifications.hxx"
#include "FlightTrack.hxx"

using namespace std;

#include <stdexcept>

// This is a convenience class used by the AtlasController that
// consolidates the management of palettes.  It keeps a vector of all
// palettes in Atlas' "Palettes" directory, and keeps track of a
// single palette that is considered current.  This is a "dumb" class,
// in the sense that it notifies no one about what happens - it is
// merely a container of data.
class Palettes {
  public:
    Palettes(const char *paletteDir);
    ~Palettes();

    // "Not a Palette" - represents an invalid palette index.
    static const size_t NaP;

    // The current palette index (NaP if there is no current palette).
    size_t currentNo() { return _i; }
    // Return the palette at the given index (NULL if i is out of
    // bounds).
    Palette *at(size_t i);

    // Return the current palette (NULL if _i is invalid).
    Palette *current() { return at(currentNo()); }
    // Make the palette at index i current.  If i is out of range, set
    // the current palette index to NaP.
    void setCurrentNo(size_t i);

    // Total number of palettes we have.
    size_t size() { return _palettes.size(); }

    // Looks for a palette with the given name, which should match the
    // file name of a palette (with or without a trailing '.ap'),
    // without the leading path.  Returns the index of the palette if
    // found, NaP otherwise.
    size_t find(const char *name);

    // Vector of all palettes.
    const vector<Palette *>& palettes() { return _palettes; }

  protected:
    size_t _i;
    vector<Palette *> _palettes;
};

// Similar to the Palettes class, but for flight tracks.
class FlightTracks {
  public:
    FlightTracks();
    ~FlightTracks();

    // "Not a FlightTrack" - represents an invalid flight track index.
    static const size_t NaFT;

    // The current flight track index (NaFT if there is no current
    // flight track).
    size_t currentNo() { return _i; }
    // Return the flight track at the given index (NULL if i is out of
    // bounds).
    FlightTrack *at(size_t i);

    // Return the current flight track (NULL if _i is invalid).
    FlightTrack *current() { return at(currentNo()); }
    // Make the flight track at index i current.  If i is out of
    // range, set the current flight track index to NaFT.
    void setCurrentNo(size_t i);

    // Total number of flight tracks we have.
    size_t size() { return _flightTracks.size(); }

    // Looks for a flight track (in the _flightTracks vector) at the
    // given network port, on the given serial device, or with the
    // given file name.  Returns the index of the flight track if
    // found, NaFT otherwise.
    size_t find(int port);
    size_t find(const char *device, int baud);
    size_t find(const char *path);

    // Adds the track and makes it the current flight track.
    void add(FlightTrack *t);
    // Removes the given track.  If it is the current flight track,
    // the one immediately preceding it (or, if it is the first, the
    // one after it) becomes current.
    void remove(size_t i);
    void removeCurrent() { remove(currentNo()); }

    // Vector of all flight tracks.
    const vector<FlightTrack *>& tracks() { return _flightTracks; }

  protected:
    size_t _i;
    vector<FlightTrack *> _flightTracks;
};

class AtlasController
{
  public:
    AtlasController(const char *paletteDir);
    ~AtlasController();

    // Lighting
    bool discreteContours() { return _discreteContours; }
    void setDiscreteContours(bool b);
    bool contourLines() { return _contourLines; }
    void setContourLines(bool b);
    bool lightingOn() { return _lightingOn; }
    void setLightingOn(bool b);
    bool smoothShading() { return _smoothShading; }
    void setSmoothShading(bool b);
    float azimuth() { return _azimuth; }
    void setAzimuth(float f);
    float elevation() { return _elevation; }
    void setElevation(float f);

    // Mapping
    int oversampling() {return _oversampling; }
    void setOversampling(int i);
    TileMapper::ImageType imageType() {return _imageType; }
    void setImageType(TileMapper::ImageType it);
    int JPEGQuality() { return _JPEGQuality; }
    void setJPEGQuality(int i);

    // Scenery
    TileManager *tileManager() { return _tm; }
    void setMapLevels(std::bitset<TileManager::MAX_MAP_LEVEL>& levels);

    // Navaids and airport data.
    NavData *navData() { return _navData; }

    // Palettes
    Palette *currentPalette() { return _palettes->current(); }
    int currentPaletteNo() { return _palettes->currentNo(); }
    // Sets the current palette (does nothing if i is out of bounds).
    void setCurrentPalette(size_t i);
    const vector<Palette *>& palettes() { return _palettes->palettes(); }

    // Sets the base of the current palette to the given elevation.
    void setPaletteBase(double elev);

    bool degMinSec() { return _degMinSec; }
    void setDegMinSec(bool degMinSec);
    bool magTrue() { return _magTrue; }
    void setMagTrue(bool magTrue);
    bool MEFs() { return _MEFs; }
    void setMEFs(bool MEFs);

    // Flight track stuff.  The following two work on the current
    // track, if it exists.  If it doesn't, they are smart enough to
    // do nothing.
    FlightData *currentPoint();
    void setMark(size_t i);

    // The following mostly pass the call through to the FlightTracks
    // object.  However, setCurrentTrack() will also emit a
    // notification if the current flight track changes.
    size_t currentTrackNo() { return _flightTracks->currentNo(); }
    void setCurrentTrack(size_t i);
    FlightTrack *currentTrack() { return _flightTracks->current(); }
    FlightTrack *trackAt(size_t i) { return _flightTracks->at(i); }
    size_t find(int port) { return _flightTracks->find(port); }
    size_t find(const char *device, int baud) 
      { return _flightTracks->find(device, baud); }
    size_t find(const char *path) { return _flightTracks->find(path); }
    const std::vector<FlightTrack *>& tracks() 
      { return _flightTracks->tracks(); }

    // The following also deal with tracks, and correspond more or
    // less to the buttons on the UI.
    bool showTrackInfo() { return _showTrackInfo; }
    void setShowTrackInfo(bool b);
    void setTrackLimit(int limit);
    void addTrack(FlightTrack *track);
    void loadTrack(const char *fileName);
    void clearTrack();
    void saveTrack();
    void saveTrackAs(char *fileName);
    void removeTrack();
    void detachTrack();
    void checkForInput();

    // Searcher object.  It allows one to find objects (navaids,
    // airports, ...) by string.  When we read in the various
    // FlightGear databases, we add entries to this object.
    Searcher *searcher() { return _searcher; }

  protected:
    TileManager *_tm;
    Palettes *_palettes;
    FlightTracks *_flightTracks;
    Searcher *_searcher;
    NavData *_navData;

    // Lighting and mapping variables.
    bool _discreteContours, _contourLines, _lightingOn, _smoothShading;
    float _azimuth, _elevation;
    int _oversampling;
    TileMapper::ImageType _imageType;
    int _JPEGQuality;

    bool _degMinSec, _magTrue, _MEFs;
    bool _showTrackInfo;
};

#endif
