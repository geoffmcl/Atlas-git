/*-------------------------------------------------------------------------
  NavaidsOverlay.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2012 Brian Schack

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

// Forward class declarations
class Overlays;
class NavData;
class NAV;
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

    void drawVORs(NavData *navData);
    void drawNDBs(NavData *navData);
    void drawILSs(NavData *navData);
    void drawDMEs(NavData *navData);

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    void _createVORRose();
    void _createVORSymbols();
    void _createNDBSymbols();
    void _createMarkerSymbols();
    void _createDMESymbols();
    void _createILSSymbols();
    void _createILSSymbol(GLuint dl, const float *colour);

    void _renderVOR(const NAV *n);
    void _renderNDB(const NAV *n);
    void _renderMarker(const NAV *n);
    void _renderILS(const NAV *n);
    void _renderDME(const NAV *n);
    void _renderDMEILS(const NAV *n);

    Label *_makeLabel(const char *fmt, const NAV *n,
		      float labelPointSize,
		      float x, float y, 
		      LayoutManager::Point p = LayoutManager::CC);
    void _drawLabel(const char *fmt, const NAV *n,
		    float labelPointSize,
		    float x, float y,
		    LayoutManager::Point p = LayoutManager::CC);
    void _drawLabel(Label *l);

    Overlays& _overlays;
    double _metresPerPixel;

    GLuint _VORRoseDL, _VORSymbolDL, _VORTACSymbolDL, _VORDMESymbolDL;
    GLuint _NDBSymbolDL, _NDBDMESymbolDL;
    GLuint _ILSSymbolDL, _LOCSymbolDL, _ILSMarkerDLs[3];
    GLuint _TACANSymbolDL, _DMESymbolDL;
    GLuint _VORDisplayList, _NDBDisplayList, _ILSDisplayList, _DMEDisplayList;
    bool _VORDirty, _NDBDirty, _ILSDirty, _DMEDirty;

    // Radio frequencies for NAV1, NAV2, and the ADF, and radials for
    // NAV1 and NAV2.  These are checked each time we get an
    // AircraftMoved or NewFlightTrack notification, and are used to
    // decide whether we need to redraw navaids.

    // EYE - in the future we should accomodate more radios, and in a
    // more general way.
    int _radios[3];
    int _radials[2];
    // We assume that if _p is not NULL there's valid flight data.
    FlightData *_p;
};

#endif
