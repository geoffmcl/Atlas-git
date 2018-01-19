/*-------------------------------------------------------------------------
  NavaidsOverlay.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2017 Brian Schack

  Loads and draws navaids (VORs, NDBs, ILS systems, and DMEs).

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

#ifndef _NAVAIDS_OVERLAY_H
#define _NAVAIDS_OVERLAY_H

#include "LayoutManager.hxx"
#include "Notifications.hxx"
#include "Overlays.hxx"

// Forward class declarations
class NavData;
class Navaid;
class VOR;
class NDB;
class DME;
class Marker;
class ILS;
class FlightData;

// Used for drawing labels on navaids.
// EYE - make Label a class?
struct Label {
    float colour[4];
    std::string id;
    LayoutManager lm;
    float metresPerPixel;
};

class NavaidsOverlay: public Subscriber {
  public:
    NavaidsOverlay(Overlays& overlays);
    ~NavaidsOverlay();

    void setDirty();

    // Draws the given overlay type, which must be VOR, NDB, or DME.
    // ILSs are dealt with specially in drawILS.
    void draw(NavData *navData, Overlays::OverlayType t);
    // Drawing an ILS is more difficult than regular navaids, because
    // it is sandwiched between airport layers (just because it looks
    // nicer).  So we have this special method, with a 'background'
    // boolean to control behaviour.  If 'background' is true, it
    // draws markers and localizers, otherwise it draws ILS DMEs.
    void drawILS(NavData *navData, bool background);

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    void _createVORRose();
    void _createVORSymbols();
    void _createNDBSymbols();
    void _createDMESymbols();
    void _createMarkerSymbols();
    void _createILSSymbols();
    void _createILSSymbol(GLuint dl, const float *colour);

    void _resetHits(NavData *navData);
    template<class T>
    void _draw(std::vector<T *> navaids, bool& dirty, GLuint dl);
    void _draw(VOR *vor);
    void _draw(NDB *ndb);
    void _draw(DME *dme);
    void _renderMarker(Marker *marker);
    void _renderILS(ILS *ils);

    Label *_makeLabel(const char *fmt, Navaid *n,
		      float labelPointSize,
		      float x, float y, 
		      LayoutManager::Point p = LayoutManager::CC);
    void _drawLabel(const char *fmt, Navaid *n,
		    float labelPointSize,
		    float x, float y,
		    LayoutManager::Point p = LayoutManager::CC);
    void _drawLabel(Label *l);

    Overlays& _overlays;
    double _metresPerPixel;

    std::vector<VOR *> _VORs;
    std::vector<NDB *> _NDBs;
    std::vector<DME *> _DMEs;
    std::vector<ILS *> _ILSs;

    GLuint _VORRoseDL, _VORSymbolDL, _VORTACSymbolDL, _VORDMESymbolDL;
    GLuint _NDBSymbolDL, _NDBDMESymbolDL;
    // EYE - can we make _ILSMarkerDLs indexed by Marker::Type?
    GLuint _ILSSymbolDL, _LOCSymbolDL, _ILSMarkerDLs[3];
    GLuint _TACANSymbolDL, _DMESymbolDL, _DMEILSSymbolDL;

    GLuint _VORsDL, _NDBsDL, _DMEsDL;
    GLuint _ILSBackgroundDL, _ILSForegroundDL;

    bool _hitsDirty, _VORDirty, _NDBDirty, _DMEDirty, _ILSDirty;

    // Radio frequencies for NAV1, NAV2, and the ADF, and radials for
    // NAV1 and NAV2.  These are checked each time we get an
    // AircraftMoved or NewFlightTrack notification, and are used to
    // decide whether we need to redraw navaids.

    // EYE - in the future we should accomodate more radios, and in a
    // more general way (although we're limited by the current Atlas
    // protocol).  And why do FlightTracks store radials as floats?
    // (Or maybe the correct question is, why does the Atlas protocol
    // store radials as floats?)
    unsigned int _radios[3];
    float _radials[2];
    // We assume that if _p is not NULL there's valid flight data.
    FlightData *_p;
};

#endif
