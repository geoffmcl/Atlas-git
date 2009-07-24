/*-------------------------------------------------------------------------
  NavaidsOverlay.hxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

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

#include <vector>
#include <string>
#include <map>

#include <zlib.h>

#include <plib/sg.h>

#include "Overlays.hxx"
#include "Culler.hxx"
#include "LayoutManager.hxx"
#include "FlightTrack.hxx"
#include "Searcher.hxx"
#include "Notifications.hxx"

// EYE - change to VOR, DME, NDB, ...?
enum NavType {NAV_VOR, NAV_DME, NAV_NDB, NAV_ILS, NAV_GS, 
	      NAV_OM, NAV_MM, NAV_IM};
enum NavSubType {DME, DME_ILS, GS, IGS, ILS_cat_I, ILS_cat_II, ILS_cat_III, 
		 IM, LDA, LDA_GS, LOC, LOM, MM, NDB, NDB_DME, OM, SDF, TACAN, 
		 VOR, VOR_DME, VORTAC, UNKNOWN};

struct NAV: public Searchable, Cullable {
  public:
    // Searchable interface.
    const double *location() const { return bounds.center; }
    virtual double distanceSquared(const sgdVec3 from) const;
    virtual const std::vector<std::string>& tokens();
    virtual const std::string& asString();

    // Cullable interface.
    const atlasSphere& Bounds() { return bounds; }
    double latitude() { return lat; }
    double longitude() { return lon; }

    std::string name, id;
    NavType navtype;
    NavSubType navsubtype;
    // lat, lon - location (degrees)
    double lat, lon;
    // elev - elevation (metres)
    // freq - frequency (kHz)
    // range - range (metres)
    int elev, freq, range, freq2;
    // magvar means different things for different navaids:
    //   NDB - magnetic variation from true north (degrees)
    //   VOR/VORTAC - slaved variation from true north (degrees)
    //   ILS - true heading (degrees)
    //   GS - ssshhh.hhh - slope * 100,000 and true heading (degrees)
    //   DME - bias (metres)
    float magvar;
    atlasSphere bounds;

  protected:
    std::vector<std::string> _tokens;
    std::string _str;
};

// Determines how navaids are drawn.
struct NavaidPolicy {
};

// Used for drawing labels on navaids.
struct Label {
    float x, y, width, height;
    float colour[4];
    bool box;
    // EYE - barf!
    int morseChunk;
    std::string id;
    LayoutManager lm;
};

class Overlays;
class NavaidsOverlay: public Subscriber {
  public:
    NavaidsOverlay(Overlays& overlays);
    ~NavaidsOverlay();

    bool load(const std::string& fgDir);

    void setDirty();

    void drawVORs();
    void drawNDBs();
    void drawILSs();
    void drawDMEs();

    // EYE - Navaids::Policy instead?
    void setPolicy(const NavaidPolicy& p);
    NavaidPolicy policy();

    const vector<Cullable *>& getNavaids(sgdVec3 p);
    const vector<Cullable *>& getNavaids(FlightData *p);

    // Subscriber interface.
    bool notification(Notification::type n);

  protected:
    void _createVORRose();
    void _createVORSymbols();
    void _createNDBSymbols();
    void _createMarkerSymbols();
    void _createDMESymbols();
    void _createILSSymbols();
    void _createILSSymbol(GLuint dl, const float *colour);
    bool _load810(float cycle, const gzFile& arp);

    void _renderVOR(const NAV *n);
    void _renderNDB(const NAV *n);
    void _renderMarker(const NAV *n);
    void _renderILS(const NAV *n);
    void _renderDME(const NAV *n);
    void _renderDMEILS(const NAV *n);

    void _drawLabel(const char *fmt, const NAV *n,
		    float labelPointSize,
		    float x, float y);
    Label *_makeLabel(const char *fmt, const NAV *n,
		      float labelPointSize,
		      float x, float y);
    void _drawLabel(Label *l);

    float _morseWidth(const std::string& id, float height);
    float _renderMorse(const std::string& id, float height,
		       float x, float y, bool render = true);

    Overlays& _overlays;
    Culler *_culler;
    Culler::FrustumSearch *_frustum;
    Culler::PointSearch *_point;
    double _metresPerPixel;

    vector<NAV *> _navaids;

    GLuint _VORRoseDL, _VORSymbolDL, _VORTACSymbolDL, _VORDMESymbolDL;
    GLuint _NDBSymbolDL, _NDBDMESymbolDL;
    GLuint _ILSSymbolDL, _LOCSymbolDL, _ILSMarkerDLs[3];
    GLuint _TACANSymbolDL, _DMESymbolDL;
    GLuint _VORDisplayList, _NDBDisplayList, _ILSDisplayList, _DMEDisplayList;
    bool _VORDirty, _NDBDirty, _ILSDirty, _DMEDirty;

    NavaidPolicy _policy;

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
