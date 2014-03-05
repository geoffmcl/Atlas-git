/*-------------------------------------------------------------------------
  NavData.hxx

  Written by Brian Schack

  Copyright (C) 2012 - 2014 Brian Schack

  This contains all the classes used to read in and access our navaid
  and airport data.  The various classes are gathered together in the
  NavData object, which will load them all in and manage searching for
  them.

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

#ifndef _NAVDATA_H
#define _NAVDATA_H

#include <string>
#include <vector>
#include <map>

#include "Culler.hxx"
#include "misc.hxx"
#include "Searcher.hxx"

// Forward class declarations
class FlightData;

// EYE - change to VOR, DME, NDB, ...?
enum NavType {NAV_VOR, NAV_DME, NAV_NDB, NAV_ILS, NAV_GS, 
	      NAV_OM, NAV_MM, NAV_IM};
enum NavSubType {DME, DME_ILS, GS, IGS, ILS_cat_I, ILS_cat_II, ILS_cat_III, 
		 IM, LDA, LDA_GS, LOC, LOM, MM, NDB, NDB_DME, OM, SDF, TACAN, 
		 VOR, VOR_DME, VORTAC, UNKNOWN};

struct NAV: public Searchable, Cullable {
  public:
    // Searchable interface.
    const double *location() const { return _bounds.center; }
    virtual double distanceSquared(const sgdVec3 from) const;
    virtual const std::vector<std::string>& tokens();
    virtual const std::string& asString();

    // Cullable interface.
    const atlasSphere& bounds() { return _bounds; }
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
    // freq2 - the DME frequency of an NDB-DME (freq is the NDB's frequency)
    int elev, freq, range, freq2;
    // magvar means different things for different navaids:
    //   NDB - magnetic variation from true north (degrees)
    //   VOR/VORTAC - slaved variation from true north (degrees)
    //   ILS - true heading (degrees)
    //   GS - ssshhh.hhh - slope * 100,000 and true heading (degrees)
    //   DME - bias (metres)
    float magvar;
    atlasSphere _bounds;

  protected:
    std::vector<std::string> _tokens;
    std::string _str;
};

struct FIX: public Searchable, Cullable {
  public:
    // Searchable interface.
    const double *location() const { return _bounds.center; }
    virtual double distanceSquared(const sgdVec3 from) const;
    virtual const std::vector<std::string>& tokens();
    virtual const std::string& asString();

    // Cullable interface.
    const atlasSphere& bounds() { return _bounds; }
    double latitude() { return lat; }
    double longitude() { return lon; }

    char name[6];		// Fixes are always 5 characters or less.
    double lat, lon;
    bool low, high;
    atlasSphere _bounds;

  protected:
    std::vector<std::string> _tokens;
    std::string _str;
};

// AwyLabel - one end of an airway.  It has a name, a location, and a
// pointer to a NAV struct (if isNavaid is true) or a FIX struct (if
// isNavaid is false).
struct AwyLabel {
    std::string id;
    double lat, lon;
    bool isNavaid;
    void *n;
};

// AWY - a single airway segment, perhaps shared by several airways.
struct AWY: public Cullable {
    // Cullable interface.
    const atlasSphere& bounds() { return _bounds; }
    // EYE - an approximation - use centre?
    double latitude() { return start.lat; }
    double longitude() { return start.lon; }

    std::string name;
    struct AwyLabel start, end;
    bool isLow;			// True if a low airway, false if high
    int base, top;		// Base and top of airway in 100's of feet
    atlasSphere _bounds;
    double length;		// Length of segment in metres
};

struct RWY {
    std::string id;
    double lat, lon;		// centre of runway
    float hdg;			// true heading, in degrees
    float length, width;	// metres

    atlasSphere _bounds;
    sgdVec3 ahead, aside, above;
};

enum ATCCodeType {WEATHER = 50, UNICOM, DEL, GND, TWR, APP, DEP};

// A mapping from strings (like "ATLANTA APP") to a set of frequencies
// associated with them.
typedef std::map<std::string, std::set<int> > FrequencyMap;

struct ARP: public Searchable, Cullable {
  public:
    // Searchable interface.
    const double *location() const { return _bounds.center; }
    virtual double distanceSquared(const sgdVec3 from) const;
    virtual const std::vector<std::string>& tokens();
    virtual const std::string& asString();

    // Cullable interface.
    const atlasSphere& bounds() { return _bounds; }
    double latitude() { return lat; }
    double longitude() { return lon; }

    std::string name, id;
    double lat, lon;
    float elev;
    bool controlled;
    bool lighting;		// True if any runway has any kind of
				// runway lighting.
    bool beacon;
    // EYE - change this lat, lon stuff to a structure.  Use SGGeod?
    double beaconLat, beaconLon;
    std::vector<RWY *> rwys;
    std::map<ATCCodeType, FrequencyMap> freqs;

    atlasSphere _bounds;

  protected:
    std::vector<std::string> _tokens;
    std::string _str;
};

class NavData {
  public:
    NavData(const char *fgRoot, Searcher *searcher);
    ~NavData();

    // NavData's hits() method returns the objects within our current
    // view.  Whenever the view changes, call these methods.
    void move(const sgdMat4 modelViewMatrix);
    void zoom(const sgdFrustum& frustum);

    // Returns all objects of the specified type within the view
    // specified by move() and zoom().
    enum NavDataType { NAVAIDS = 0, FIXES, AIRWAYS, AIRPORTS, _COUNT };
    std::vector<Cullable *> hits(NavDataType t) 
      { return _frustumCullers.at(t)->intersections(); }

    // Returns navaids within radio range.
    const std::vector<Cullable *>& getNavaids(sgdVec3 p);
    // Returns navaids within radio range and which are tuned in (as
    // given by 'p').
    const std::vector<Cullable *>& getNavaids(FlightData *p);

    // A vector of all individual airway segments.
    const std::vector<AWY *>& segments() const { return _segments; }

  protected:
    // These load the given data, throwing an error if they fail.
    void _loadNavaids(const char *fgRoot);
    void _loadFixes(const char *fgRoot);
    void _loadAirways(const char *fgRoot);
    void _loadAirports(const char *fgRoot);

    // These are subsidiary methods used by the above.
    void _loadNavaids810(float cycle, const gzFile& arp);
    void _loadFixes600(const gzFile& arp);
    void _loadAirways640(const gzFile& arp);
    void _checkEnd(AwyLabel &end, bool isLow);
    void _loadAirports810(const gzFile& arp);

    Searcher *_searcher;

    std::vector<Culler *> _cullers;
    std::vector<Culler::FrustumSearch *> _frustumCullers;
    Culler::PointSearch *_navaidsPointCuller;

    // All of our various navaid/fix/airway/airport objects.
    std::vector<NAV *> _navaids;
    std::vector<FIX *> _fixes;
    std::vector<AWY *> _segments;
    std::vector<ARP *> _airports;

    // The _navPoints map is used when generating airways - we want to
    // match the id given in the airways file with the actual navaid
    // or fix it refers to.
    struct _NAVPOINT {
	bool isNavaid;		// True if navaid, false if fix.
	void *n;		// Pointer to the navaid or fix.
    };
    std::multimap<std::string, _NAVPOINT> _navPoints;
};

#endif
