/*-------------------------------------------------------------------------
  NavData.hxx

  Written by Brian Schack

  Copyright (C) 2012 - 2017 Brian Schack

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
#include <deque>

#include "Culler.hxx"
#include "misc.hxx"
#include "Searcher.hxx"

// EYE - use char * or string?  Use char[] when we know we have
// fixed-length strings?

// EYE - a lot of our decisions are based on the X-Plane data.  Should
// we try to be more independent (and complete)?

//////////////////////////////////////////////////////////////////////
// Navaids
//////////////////////////////////////////////////////////////////////

// For lack of a better term, this describes a point that can be used
// for navigation.  It has an ID and a location (but no elevation),
// and nothing else.  The location can be accessed in terms of degrees
// of latitude and longitude, or cartesian coordinates (in metres).
//
// This roughly corresponds to AICM's "SIGNIFICANT_POINT".
class Waypoint: public Searchable, public Cullable {
  public:
    // 'lat' and 'lon' are in degrees
    Waypoint(const char *id, double lat, double lon);
    virtual ~Waypoint();

    const std::string& id() const { return _id; }
    double lat() const { return _lat; } // degrees
    double lon() const { return _lon; } // degrees

    // When a Waypoint (or a subclass of Waypoint) is created, it
    // automatically adds itself to the class variable __waypoints.
    // It removes itself automatically when it's deleted.  We use a
    // multimap because navaid ids are not guaranteed unique.
    static const std::multimap<std::string, Waypoint *>& waypoints() 
        { return __waypoints; }

    friend std::ostream& operator<<(std::ostream& os, const Waypoint& n);

    // Ideally, we could keep this class hierarchy simple, doing only
    // one thing - representing navaids.  However, we want to be able
    // to search with them and cull them in Atlas.  If this were
    // Objective-C, we could add these later, outside of this source
    // file, via a category.  Alas, this is not Objective-C, so we
    // need to "pollute" the basic classes where they are defined.

    // Searchable interface.
    const double *location(const sgdVec3 from);
    double distanceSquared(const sgdVec3 from);
    virtual const std::vector<std::string>& tokens();
    virtual const char *asString();

    // Cullable interface.
    const atlasSphere& bounds();
    double latitude() { return _lat; }
    double longitude() { return _lon; }

  protected:
    // This is called by << and does the real work of output.
    // Subclasses who want custom output should override this.
    virtual std::ostream& _put(std::ostream& os) const;

    // A mapping from a navaid id to the navaid.  Because id's are not
    // unique, we use a multimap.
    static std::multimap<std::string, Waypoint *> __waypoints;

    std::string _id;
    double _lat, _lon;

    // Needed for searchable and cullable interfaces.  The method
    // _calcBounds is called from the Waypoint constructor.  It sets
    // _bounds assuming an elevation and radius of 0.0.  Subclasses
    // that have an elevation or radius should override it.
    virtual void _calcBounds();

    atlasSphere _bounds;
    // EYE - we should use vectors of string pointers/references, and
    // then have a class string for at least the "FIX:", "VOR:",
    // ... string.  Although, given that they are such short strings,
    // the savings wouldn't be that great.  A quick test shows that a
    // string is 8 bytes plus the characters in it (approximately - it
    // really depends on implementation details), while a pointer is 8
    // bytes.
    std::vector<std::string> _tokens;
};

// A fix is almost exactly like a waypoint, with the addition of a
// "type" - terminal or enroute.  Basically these determine when the
// fix is drawn.  By default, a fix is classified as a terminal fix.
//
// At the moment, we have to determine the fix type heuristically,
// because fix.dat does not tell us the official type..  If a fix is a
// member of an airway, we reclassify it as an enroute fix
//
// In the future, I'd like to add information like the fix's
// definition (eg, 15nm from the FOO VOR along the 315 radial).
// However, this information is not currently available in fix.dat.

// EYE - really?
// This corresponds vaguely to AICM's "DESIGNATED_POINT".

// EYE - check if there are more types of fixes.  Version 1100 of
// X-Plane just has terminal and en-route.
class Fix: public Waypoint {
  public:
    Fix(const char *id, double lat, double lon);

    // Get and set the fix's type.
    bool isTerminal() const { return _isTerminal; }
    bool isEnroute() const { return !_isTerminal; }
    void setTerminal() { _isTerminal = true; }
    void setEnRoute() { _isTerminal = false; }

    const std::vector<std::string>& tokens();
    const char *asString();

  private:
    std::ostream& _put(std::ostream& os) const;

    // The fix's "type".
    bool _isTerminal;
};

// A navaid, in addition to an id and a location, also has an
// elevation (in metres), a name (eg, "AARHUS"), a frequency (in Hz),
// and a range (in metres).
//
// A navaid represents a single physical entity on the ground, which
// means that a NDB-DME (which consists of two separate transmitters
// which may be separated by up to 600m according to ICAO Annex 10,
// Vol 1, section 3.5.2.6.1) is represented as two navaids.
class Navaid: public Waypoint {
  public:
    Navaid(const char *id, double lat, double lon, int elev, 
	   const char *name, unsigned int freq, unsigned int range);

    int elev() const { return _elev; } // m
    const std::string& name() const { return _name; }
    unsigned int frequency() const { return _freq; } // Hz
    unsigned int range() const { return _range; }    // m

    // A measure of signal strength, proportional to range squared
    // over distance squared.  That means that at the limit of its
    // range, this method returns 1.0.  At half range it returns 4.0.
    // When distance is 0, we just return the biggest possible double.
    // Past the range limit, it returns values less than 1.0.
    double signalStrength(const sgdVec3 from);

    virtual bool validFrequency() const = 0;

    // Useful constants.
    static const int kHz = 1000;
    static const int MHz = 1000000;

    // Helper methods for checking navaid frequencies.
    static bool validNDBFrequency(unsigned int freq);
    static bool validVHFFrequency(unsigned int freq);
    static bool validVORFrequency(unsigned int freq);
    static bool validILSFrequency(unsigned int freq);

  protected:
    void _calcBounds();

    std::string _name;
    int _elev;
    unsigned int _freq, _range;
};

// An NDB is a navaid, but with a restricted frequency range.  As
// well, it has a type (NDB, NDB-DME, or LOM).  It can be paired
// (optionally) with a DME.

// EYE - according to AICM, NDBs are *not* co-located with DMEs, but
// can be co-located with a marker.
class NDB: public Navaid {
  public:
    NDB(const char *id, double lat, double lon, int elev, 
	const char *name, unsigned int freq, unsigned int range);

    bool validFrequency() const;

    const std::vector<std::string>& tokens();
    const char *asString();

  protected:
    std::ostream& _put(std::ostream& os) const;
};

// A VOR is a navaid, but with a restricted frequency range, and a
// slaved variation (in degrees).  The slaved variation tells us its
// offset from true north (theoretically the offset points it to
// magnetic north, but VORs often aren't adjusted to track magnetic
// north as it changes).  A VOR can be one of 3 types: VOR, VOR-DME,
// and VORTAC.  A VOR can be paired with a DME (VOR-DME) or a TACAN
// (VORTAC).
class VOR: public Navaid {
  public:
    VOR(const char *id, double lat, double lon, int elev,
	const char *name, unsigned int freq, unsigned int range, 
	float slavedVariation);

    float variation() const { return _variation; } // degrees

    bool validFrequency() const;

    const std::vector<std::string>& tokens();
    const char *asString();

  protected:
    std::ostream& _put(std::ostream& os) const;

    float _variation;
};

// A DME is a navaid with a 'bias'.  The bias is an offset from the
// real DME distance, in metres.  As far as I know, this is only used
// for DMEs that are a part of an ILS system, so that the DME reads
// 0.0 at the runway threshold.  DMEs can be of 4 types: DME, NDB-DME,
// DME-ILS, or VOR-DME.  A DME can be optionally paired with an NDB or
// a VOR, or can be a member of an ILS system.
class DME: public Navaid {
  public:
    DME(const char *id, double lat, double lon, int elev,
	const char *name, unsigned int freq, unsigned int range, float bias);

    float bias() const { return _bias; } // m

    bool validFrequency() const;

    const std::vector<std::string>& tokens();
    const char *asString();

  protected:
    std::ostream& _put(std::ostream& os) const;

    float _bias;
};

// A TACAN is like DME with VOR capabilities.  In addition to the
// regular navaid things, it has a slaved variation (like the VOR),
// and a bias (like the DME).  The directional capabilities are only
// accessible to military users, however.  A TACAN can also be paired
// with a VOR, giving a VORTAC.
class TACAN: public DME {
  public:
    TACAN(const char *id, double lat, double lon, int elev,
	  const char *name, unsigned int freq, unsigned int range, 
	  float slavedVariation, float bias);

    float variation() const { return _variation; } // degrees
    float bias() const { return _bias; } // m

    bool validFrequency() const;

    const std::vector<std::string>& tokens();
    const char *asString();

  protected:
    std::ostream& _put(std::ostream& os) const;

    float _variation;
};

// A LOC represents an ILS locator, whether standalone or as part of
// an ILS system.  The heading represents the true (not magnetic)
// heading of the localizer, in degrees.
class LOC: public Navaid {
  public:
    LOC(const char *id, double lat, double lon, int elev,
	const char *name, unsigned int freq, unsigned int range, float heading);

    float heading() const { return _heading; } // degrees

    bool validFrequency() const;

    const std::vector<std::string>& tokens();
    const char *asString();

  protected:
    std::ostream& _put(std::ostream& os) const;

    float _heading;
};

// A GS has both a heading (like a localizer, but not necessarily with
// an identical value) and a glideslope angle (in degrees).
class GS: public Navaid {
  public:
    GS(const char *id, double lat, double lon, int elev,
       const char *name, unsigned int freq, unsigned int range, 
       float heading, float slope);

    float heading() const { return _heading; } // degrees
    float slope() const { return _slope; }     // degrees

    bool validFrequency() const;

    const std::vector<std::string>& tokens();
    const char *asString();

  protected:
    std::ostream& _put(std::ostream& os) const;

    float _heading, _slope;
};

// Markers don't have ids, explicit frequencies, or ranges, but they
// do have names and orientations (and a type).

// EYE - override _calcBounds to take into account non-specfied range?
// Note that Wikipedia says something about marker ranges: "The valid
// signal area is a 2,400 ft (730 m) × 4,200 ft (1,280 m) ellipse (as
// measured 1,000 ft (300 m) above the antenna.)"  In some sense, we
// don't care, as we don't deal with radio reception of markers.  We
// *do* care about culling, but having a range will make little
// difference - it might affect display if the marker is on the edge
// of the screen.
class Marker: public Navaid {
  public:
    enum Type {
	OUTER, MIDDLE, INNER, _LAST
    };

    Marker(double lat, double lon, int elev, const char *name, float heading,
	   Type type);

    Type type() const { return _type; }
    float heading() const { return _heading; } // degrees

    bool validFrequency() const { return true; }

    // EYE - make sure all the classes that have tokens() and
    // asString() declared actually implement them.
    const std::vector<std::string>& tokens();
    const char *asString();

  protected:
    std::ostream& _put(std::ostream& os) const;

    float _heading;
    Type _type;
};

//////////////////////////////////////////////////////////////////////
// NavaidSystem
//////////////////////////////////////////////////////////////////////

// A navaid system is two or more navaids working together.  Often
// they are located together or nearly together - VOR-DMEs, NDB-DMEs,
// and VORTACs.  The elements can be further spaced, in the case of an
// ILS system, which may have markers many miles from the runway end.
//
// Note that waypoints and fixes don't take part in navaid systems (I
// hope), so the basic component is the navaid.

// EYE - LOMs?

// EYE - operator<< for NavaidSystems?

// EYE - cullable?  Probably not.  Searchable?  Probably not.
class NavaidSystem {
  public:
    NavaidSystem();
    virtual ~NavaidSystem();

    // Adds and removes the navaid.  This updates __owners.
    // Subclasses should call these when navaids are added to their
    // system.  Each call to add() must be matched by a call to
    // remove().
    void add(Navaid *n);
    void remove(Navaid *n);
    
    // Returns the navaid system of which n is a member, NULL
    // otherwise.  We assume that a navaid cannot be the member of
    // more than 1 navaid system.
    static NavaidSystem *owner(Navaid *n);

    // Subclasses should return some sort of reasonable value for the
    // system's id and name.
    virtual const std::string& id() const = 0;
    virtual const std::string& name() const = 0;

    // EYE - do we need a map instead, or is a set good enough?
    static const std::set<NavaidSystem *>& systems() { return __systems; }

  protected:
    // All of the navaid systems.  They are added automatically as
    // they are created, and removed automatically as they are
    // deleted.
    static std::set<NavaidSystem *> __systems;

    // This lets us find, for a given navaid, the system of which it
    // is a part (if any).
    static std::map<Navaid *, NavaidSystem *> __owners;
};

// EYE - Although we don't implement it, another possible paired
// navaid is an LOM, a colocated NDB and outer marker.  However,
// FlightGear's navaid database doesn't indicate LOMs (with rare
// exceptions).  In fact, most of the rare LOMs in the database seem
// to be mislabelled NDBs.
class PairedNavaidSystem: public NavaidSystem {
  public:
    PairedNavaidSystem(Navaid *n1, Navaid *n2);
    ~PairedNavaidSystem();

    Navaid *n1() { return _n1; }
    Navaid *n2() { return _n2; }
    
    // Given one member of the pair, returns the other (NULL if n is
    // not in this system).
    Navaid *other(const Navaid *n);

    // Elements of paired navaid systems have identical ids and names,
    // so we use that to represent the id and name of the system.
    const std::string& id() const { return _n1->id(); }
    const std::string& name() const { return _n1->name(); }

  protected:
    Navaid *_n1, *_n2;
};

class VOR_DME: public PairedNavaidSystem {
  public:
    VOR_DME(VOR *vor, DME *dme);
    ~VOR_DME();

    VOR *vor();
    DME *dme();

    VOR *other(DME *dme);
    DME *other(VOR *vor);
};

// A VORTAC is a VOR and a TACAN together.  Civilian users use the VOR
// for bearing information and the TACAN for distance information;
// military users use the TACAN for both.
class VORTAC: public PairedNavaidSystem {
  public:
    VORTAC(VOR *vor, TACAN *tacan);
    ~VORTAC();

    VOR *vor();
    TACAN *tacan();

    VOR *other(TACAN *tacan);
    TACAN *other(VOR *vor);
};

class NDB_DME: public PairedNavaidSystem {
  public:
    NDB_DME(NDB *ndb, DME *dme);
    ~NDB_DME();

    NDB *ndb();
    DME *dme();

    NDB *other(DME *dme);
    DME *other(NDB *vor);
};

//////////////////////////////////////////////////////////////////////
// ILSs
//////////////////////////////////////////////////////////////////////

// An ILS is not a navaid per se, but rather a navaid system.
// According to AICM, an ILS:
//
// - must have a localizer
// - may have a glideslope
// - may have a DME
// - may have one or more markers
//
// An ILS checks to make its components are matched: the glideslope
// and DME must have the same id, name, and frequency as the
// localizer.  Markers must match the localizer name.
// 
// Note that ILS instances do *not* own their components.  Be careful
// about deleting localizers, glideslopes, etc, when there are ILS
// instances about - the safest is to delete the ILS first.

// EYE - change the above policy?

// ILS types:
//
// ILS-CAT-I, -II, -III - always straight-in, with GS (ie, 4)
// LOC_GS - localizer offset, with GS (ie, 4)
// IGS/LDA - localizer offset, GS optional (ie, 4 or 5)
// SDF - straight-in or offset, never has GS (ie, 5)
// LOC - always straight-in (?), without GS (ie, 5).  Can also be
//       called an LLZ.  If offset, should be called an LDA.  X-Plane
//       1000 has a LOC listed with a GS (LOWI 26).  I think this is
//       an error, and should be listed as an LDA (in 1000, everything
//       seems to be doubled, and in 1000 and 810, the LOC and one of
//       the DMEs originate way east of the airport).

class ILS: public NavaidSystem {
  public:
    enum Type {
        ILS_CAT_I, ILS_CAT_II, ILS_CAT_III, LOC_GS, LDA, IGS, Localizer, SDF
    };

    ILS(LOC *loc, Type type);
    ~ILS();

    Type type() const { return _type; }

    void setGS(GS *gs);
    void setDME(DME *dme);
    void addMarker(Marker *m);

    LOC *loc() const { return _loc; }
    GS *gs() const {return _gs; }
    DME *dme() const { return _dme; }
    const std::set<Marker *>& markers() const { return _markers; }

    // A handy shortcut - given a localizer, what is its owning ILS?
    static ILS *ils(LOC *loc);

    const std::string& id() const { return loc()->id(); }
    const std::string& name() const { return loc()->name(); }

    friend std::ostream& operator<<(std::ostream& os, const ILS& ils);

  protected:
    Type _type;
    LOC *_loc;
    GS *_gs;
    DME *_dme;
    std::set<Marker *> _markers;
};

//////////////////////////////////////////////////////////////////////
// Airways
//////////////////////////////////////////////////////////////////////

// A segment connects two points in an airway.
//
// It has two endpoints, a lower and upper elevation (in feet), a
// boolean specifying whether it is a high-altitude or low-altitude
// airway, and a set of pointers to the airway(s) of which it is a
// member.  Between any two points we assume there can be at most *2*
// segments (one high, one low).
//
// Like ILSs, the segment doesn't own its waypoints - if you delete
// them before deleting the segment, bad things may happen.  As well,
// a segment can participate in many airways.  When an airway is
// deleted, it will remove itself from its member segments.  So, the
// best thing to do is: (a) delete all the airways, then (b) delete
// the segments.
//
// Note that "start" and "end" don't imply anything about order within
// an airway.  For example, we could have an airway consisting of two
// segments <BAR, FOO> and <BAR, CHUCK>, BAR being the "start" of both
// segments.  However, the complete airway would be <FOO, BAR, CHUCK>,
// or <CHUCK, BAR, FOO> - BAR is the start of one segment, but the end
// of the other (in that particular airway).
//
// Corresponds to an AICM "RTE_SEG".
class Airway;
class Segment: public Cullable {
  public:
    Segment(const std::string& name, Waypoint *start, Waypoint *end, 
	    int base, int top, bool isLow);
    ~Segment();

    const std::string& name() const { return _name; }
    Waypoint *start() const { return _start; }
    Waypoint *end() const { return _end; }
    // Base and top of airway in 100's of feet
    int base() const { return _base; }
    int top() const { return _top; }
    // True if a low airway, false if high
    bool isLow() const { return _isLow; }

    void addAirway(Airway *awy) { _airways.insert(awy); }
    void removeAirway(Airway *awy) { _airways.erase(awy); }
    const std::set<Airway *>& airways() const { return _airways; }

    static const std::set<Segment *>& segments() { return __segments; }

    friend std::ostream& operator<<(std::ostream& os, const Segment& seg);

    // Cullable interface.  Like waypoints, it would be nice to not
    // pollute the basic class with Atlas-specific stuff.  However,
    // C++ doesn't have a mechanism for doing this.
    const atlasSphere& bounds() { return _bounds; }
    // EYE - an approximation - use the centre of the segment?
    double latitude() { return _start->lat(); }
    double longitude() { return _start->lon(); }

    // Length of segment in metres (used when rendering).
    double length() const { return _length; }

  protected:
    static std::set<Segment *> __segments;

    std::string _name;
    Waypoint *_start, *_end;
    int _base, _top;
    bool _isLow;
    std::set<Airway *> _airways;

    // Used when making rendering decisions.
    double _length;

    // Needed for the cullable interface.
    atlasSphere _bounds;
};

// An airway is a "highway in the sky".  It has a name and a set of
// segments, and is classified as high-altitude or low-altitude.
//
// An important note about prepend() and append(): these methods do
// *no* checking to make sure the segment you're adding belongs there.
// It's up to you to ensure you're building the airway correctly, in
// sequence.
//
// Airways don't own their component segments.  However, be sure to
// delete airways before deleting segments.
//
// Corresponds to an AICM "EN_ROUTE_RTE".

// EYE - add facility to find airway intersections, step through an
// airway from point A to point B, ...
class Airway: public Searchable {
  public:
    Airway(const std::string& name, bool isLow, Segment *segment);
    ~Airway();

    const std::string& name() const { return _name; }
    bool isLow() const { return _isLow; }

    void prepend(Segment *segment);
    void append(Segment *segment);
    const std::deque<Segment *>& segments() const { return _segments; }

    // These are convenience routines if you want to step through the
    // waypoints in order.
    size_t noOfWaypoints() const { return _segments.size() + 1; }
    const Waypoint *nthWaypoint(size_t i) const;

    static const std::set<Airway *>& airways() { return __airways; }

    friend std::ostream& operator<<(std::ostream& os, const Airway& awy);

    // Searchable interface.  Note that segments are cullable, since
    // they are drawn, but airways are searchable, since that's what
    // the user is interested in finding.

    // EYE - one problem - location() should really return that spot
    // on the airway that's closest to the current location.  Do we
    // need to change the Searchable interface slightly?  Also, since
    // airways, or even their segments, can be long, the center might
    // be somewhere below the surface of the earth, which causes
    // rendering problems.
    const double *location(const sgdVec3 from);
    double distanceSquared(const sgdVec3 from);
    const std::vector<std::string>& tokens();
    const char *asString();

  protected:
    static std::set<Airway *> __airways;

    std::string _name;
    // EYE - should we be clever and try to discover which airways are
    // both?
    bool _isLow;
    std::deque<Segment *> _segments;

    // For the searchable interface.
    atlasSphere _bounds;
    std::vector<std::string> _tokens;
};

//////////////////////////////////////////////////////////////////////
// Airports
//////////////////////////////////////////////////////////////////////

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

    const atlasSphere& bounds() { return _bounds; }
    const sgdVec3& centre() { return _bounds.center; }

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

    const std::vector<RWY *>& rwys() { return _rwys; }
    void addRwy(RWY *rwy) { _rwys.push_back(rwy); }
    const std::map<ATCCodeType, FrequencyMap>& freqs() { return _freqs; }
    // Note: this assumes that 'freq' will be in the format found in
    // the apt.dat file (eg, 12192 for 121.925 MHz).  It will be
    // converted here to our own standard representation (121,925,000
    // Hz), suitable for use in the formatFrequency() function.
    void addFreq(ATCCodeType t, int freq, char *label);

    // Searchable interface.
    const double *location(const sgdVec3 from) { return _bounds.center; }
    double distanceSquared(const sgdVec3 from);
    const std::vector<std::string>& tokens();
    const char *asString();

    // Cullable interface.
    const atlasSphere& bounds() { return _bounds; }
    double latitude();
    double longitude();

    // Extend our bounding sphere by the given sphere.

    // EYE - note that this is only called when loading.  All this
    // loading stuff should really be made an ARP method ('load()'),
    // or maybe a series of methods ('parseAirport()',
    // 'parseRunway()', ..., or perhaps 'ARP(float, int, char *)',
    // 'addRunway()', 'addFrequency()', ...).
    void extend(const sgdSphere& s) { _bounds.extend(&s); }

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

//////////////////////////////////////////////////////////////////////
// NavData - The one class to rule them all.
//////////////////////////////////////////////////////////////////////

// EYE - rename to Navaids?
// EYE - have a Navaids class and a separate Airports class?

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
    // const std::vector<Cullable *>& getNavaids(sgdVec3 p);
    void getNavaids(sgdVec3 p, std::vector<Cullable *>& navaids);
    // // Returns navaids within radio range and which are tuned in (as
    // // given by 'p').

    // // EYE - should NavData be aware of FlightData?  Maybe we should
    // // have a lower-level method (eg, navaids within range of *this
    // // radio*, or *this frequency* or *these frequencies*).
    // const std::vector<Cullable *>& getNavaids(FlightData *p);

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
    Waypoint *_findEnd(const std::string& id, double lat, double lon);
    void _loadAirports810(const gzFile& arp);
    void _loadAirports1000(const gzFile& arp);

    Searcher *_searcher;

    std::vector<Culler *> _cullers;
    // EYE - rename these _frustumSearchers and _navaidsPointSearcher?
    std::vector<Culler::FrustumSearch *> _frustumCullers;
    Culler::PointSearch *_navaidsPointCuller;

    // All of our various airport objects.
    std::vector<ARP *> _airports;
};

#endif
