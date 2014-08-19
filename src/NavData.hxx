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
    double distanceSquared(const sgdVec3 from) const;
    const std::vector<std::string> &tokens();
    const char *asString();

    // Cullable interface.
    const atlasSphere &bounds() { return _bounds; }
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
};

struct FIX: public Searchable, Cullable {
  public:
    // Searchable interface.
    const double *location() const { return _bounds.center; }
    double distanceSquared(const sgdVec3 from) const;
    const std::vector<std::string> &tokens();
    const char *asString();

    // Cullable interface.
    const atlasSphere &bounds() { return _bounds; }
    double latitude() { return lat; }
    double longitude() { return lon; }

    char name[6];		// Fixes are always 5 characters or less.
    double lat, lon;
    bool low, high;
    atlasSphere _bounds;

  protected:
    std::vector<std::string> _tokens;
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
    const atlasSphere &bounds() { return _bounds; }
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

// RWY - a runway.  Runways are represented by a centre point, a
// heading, a length, and a width.  Note that this can't deal with
// crooked runways (yes, they do exist).  Should they ever be
// introduced, we'll have to change it.
class ARP;
class RWY {
  public:
    // Initialize a runway given its id, centre, heading, length,
    // widht, and owning airport.  The id should be for the end
    // corresponding to the heading.  For example, if the heading is
    // 52 degrees, then the id should be "05" (or "05R", "05L", ...).
    RWY(char *id, double lat, double lon, float hdg, float len, float wid, 
    	ARP *ap);
    // Initialize a runway given the two runway ends, its width, and
    // its owning airport.
    RWY(char *id1, double lat1, double lon1, 
	char *id2, double lat2, double lon2, 
	float width, ARP *ap);

    // The label of the runway (eg, "09", "12R", ...).  This is the
    // end that is aligned with the runway heading given by hdg().
    const char *label() { return _label; }
    // The label at the other end of the runway (opposite hdg()).
    const char *otherLabel();
    float lat() { return _lat; }
    float lon() { return _lon; }
    float hdg() { return _hdg; }
    float length() { return _length; }
    float width() { return _width; }

    const atlasSphere &bounds() { return _bounds; }
    const sgdVec3 &centre() { return _bounds.center; }

  protected:
    // Set the bounding sphere of the runway.  This method assumes
    // that _length, and _width are already initialized.  Since runway
    // data doesn't include elevation, we need to have an elevation
    // explicitly passed in (generally we use the airport elevation).
    // Note as well that we don't use _lat and _lon - they are floats
    // and we need doubles for sufficient accuracy.
    void _setBounds(double lat, double lon, float elev);

    // Runway end labels - we assume they are 3 chars or less.
    char _label[4], _otherLabel[4];
    float _lat, _lon;		// Centre of runway
    float _hdg;			// True heading, in degrees
    float _length, _width;	// Metres

    // Our bounding sphere.  Importantly, it includes the exact
    // cartesian coordinates of the runway centre.
    atlasSphere _bounds;
};

enum ATCCodeType {WEATHER = 50, UNICOM, DEL, GND, TWR, APP, DEP};

// A mapping from strings (like "ATLANTA APP") to a set of frequencies
// associated with them.
typedef std::map<std::string, std::set<int> > FrequencyMap;

class ARP: public Searchable, public Cullable {
  public:
    ARP(char *name, char *code, float elev);
    ~ARP();

    const char *name() { return _name; }
    const char *code() { return _code; }
    float elevation() { return _elev; }

    void setControlled(bool b) { _controlled = b; }
    bool controlled() { return _controlled; }
    void setLighting(bool b) { _lighting = b; }
    bool lighting() { return _lighting; }
    void setBeaconLoc(double lat, double lon);
    // EYE - beaconLatitude(), beaconLongitude() to be consistent with
    // latitude(), longitude()?  Or vice-versa?
    double beaconLat() { return _beaconLat; }
    double beaconLon() { return _beaconLon; }
    bool beacon();

    const std::vector<RWY *> &rwys() { return _rwys; }
    void addRwy(RWY *rwy) { _rwys.push_back(rwy); }
    const std::map<ATCCodeType, FrequencyMap> &freqs() { return _freqs; }
    // Note: this assumes that 'freq' will be in the format found in
    // the apt.dat file (eg, 12192 for 121.925 MHz).  It will be
    // converted here to our own standard representation (121925 kHz),
    // suitable for use in the formatFrequency() function.
    void addFreq(ATCCodeType t, int freq, char *label);

    // Searchable interface.
    // const double *location() const { return _bounds.center; }
    const double *location() const;
    double distanceSquared(const sgdVec3 from) const;
    const std::vector<std::string> &tokens();
    const char *asString();

    // Cullable interface.
    // const atlasSphere& bounds() { return _bounds; }
    const atlasSphere &bounds();
    double latitude();
    double longitude();

    // Extend our bounding sphere by the given sphere.

    // EYE - note that this is only called when loading.  All this
    // loading stuff should really be made an ARP method ('load()'),
    // or maybe a series of methods ('parseAirport()',
    // 'parseRunway()', ..., or perhaps 'ARP(float, int, char *)',
    // 'addRunway()', 'addFrequency()', ...).
    void extend(const sgdSphere &s) { _bounds.extend(&s); }

  protected:
    // Calculates the airport's center in lat, lon from its bounds.
    void _calcLatLon();

    char *_name;
    char _code[5];		// We assume airport codes are 4
				// characters or less.  This is
				// explicitly guaranteed in version
				// 1000.
    float _elev;
    bool _controlled;
    bool _lighting;		// True if any runway has any kind of
				// runway lighting.

    std::vector<std::string> _tokens;

    atlasSphere _bounds;
    // EYE - change this lat, lon stuff to a structure.  Use SGGeod?
    // sgdVec2?  AtlasCoord?
    double _lat, _lon;
    double _beaconLat, _beaconLon;

    std::vector<RWY *> _rwys;
    std::map<ATCCodeType, FrequencyMap> _freqs;
};

class NavData {
  public:
    NavData(const char *fgRoot, Searcher *searcher);
    ~NavData();

    // NavData's hits() method returns the objects within our current
    // view.  Whenever the view changes, call these methods.
    void move(const sgdMat4 modelViewMatrix);
    void zoom(const sgdFrustum &frustum);

    // Returns all objects of the specified type within the view
    // specified by move() and zoom().
    enum NavDataType { NAVAIDS = 0, FIXES, AIRWAYS, AIRPORTS, _COUNT };
    std::vector<Cullable *> hits(NavDataType t) 
      { return _frustumCullers.at(t)->intersections(); }

    // Returns navaids within radio range.
    const std::vector<Cullable *> &getNavaids(sgdVec3 p);
    // Returns navaids within radio range and which are tuned in (as
    // given by 'p').
    const std::vector<Cullable *> &getNavaids(FlightData *p);

    // A vector of all individual airway segments.
    const std::vector<AWY *> &segments() const { return _segments; }

  protected:
    // These load the given data, throwing an error if they fail.
    void _loadNavaids(const char *fgRoot);
    void _loadFixes(const char *fgRoot);
    void _loadAirways(const char *fgRoot);
    void _loadAirports(const char *fgRoot);

    // These are subsidiary methods used by the above.
    void _loadNavaids810(float cycle, const gzFile &arp);
    void _loadFixes600(const gzFile &arp);
    void _loadAirways640(const gzFile &arp);
    void _checkEnd(AwyLabel &end, bool isLow);
    void _loadAirports810(const gzFile &arp);
    void _loadAirports1000(const gzFile &arp);

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
