/*-------------------------------------------------------------------------
  NavData.cxx

  Written by Brian Schack

  Copyright (C) 2012 - 2018 Brian Schack

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

// Our include file
#include "NavData.hxx"

// C++ system files
#include <stdexcept>
#include <sstream>

// Our libraries' include files
#include <simgear/misc/sg_path.hxx>

// Our project's include files
#include "FlightTrack.hxx"
#include "Globals.hxx"

using namespace std;

//////////////////////////////////////////////////////////////////////
// Waypoints, Fixes, ...
//////////////////////////////////////////////////////////////////////

// Our class variable.  Whenever a navaid (ie, a Waypoint or any of
// its subclasses) is created, it adds itself into __waypoints.
// Likewise, they remove themselves from __waypoints when they are
// deleted.
multimap<string, Waypoint *> Waypoint::__waypoints;

// EYE - we should rename waypoint to something else, as waypoints
// have another meaning (an RNAV procedure lat/lon).
Waypoint::Waypoint(const char *id, double lat, double lon):
    _id(id), _lat(lat), _lon(lon)
{
    // Add ourselves to the __waypoints map.
    __waypoints.insert(make_pair(id, this));
}

Waypoint::~Waypoint()
{
    // Remove ourselves from __waypoints.  Since __waypoints is keyed
    // by name, and several navaids can have the same name, we need to
    // check carefully before removing ourselves.  First, find all
    // navaids with the same name.
    pair<multimap<string, Waypoint *>::iterator, 
	multimap<string, Waypoint *>::iterator> ret;
    ret = __waypoints.equal_range(_id);

    // Now iterate through them.  When we find ourselves, remove
    // ourselves.
    multimap<string, Waypoint *>::iterator it;
    for (it = ret.first; it != ret.second; it++) {
	Waypoint *p = (*it).second;
	if (p == this) {
	    __waypoints.erase(it);
	    return;
	}
    }
}

ostream& operator<<(ostream& os, const Waypoint& n)
{
    return n._put(os);
}

const double *Waypoint::location(const sgdVec3 from) 
{
    if (_bounds.isEmpty()) {
	_calcBounds();
	// EYE - remove this?
	assert(!_bounds.isEmpty());
    }

    return _bounds.center; 
}

double Waypoint::distanceSquared(const sgdVec3 from)
{
    if (_bounds.isEmpty()) {
	_calcBounds();
	// EYE - remove this?
	assert(!_bounds.isEmpty());
    }
    return sgdDistanceSquaredVec3(_bounds.center, from);
}

const vector<string>& Waypoint::tokens()
{
    if (_tokens.empty()) {
	_tokens.push_back(_id);
	_tokens.push_back("WPT:");
    }

    return _tokens;
}

const char *Waypoint::asString()
{
    // EYE - use our own AtlasString?
    globals.str.printf("WPT: %s", _id.c_str());

    return globals.str.str();
}

const atlasSphere& Waypoint::bounds() 
{
    if (_bounds.isEmpty()) {
	_calcBounds();
	// EYE - remove this?
	assert(!_bounds.isEmpty());
    }

    return _bounds; 
}

void Waypoint::_calcBounds()
{
    // Calculate our bounds.  For waypoints, we assume that our
    // elevation is 0.0 and that we have no radius.
    assert(_bounds.isEmpty());	// EYE - remove this!
    atlasGeodToCart(_lat, _lon, 0.0, _bounds.center);
    _bounds.setRadius(0.0);
}

// EYE - move this to the front eventually
#include <iomanip>
ostream& Waypoint::_put(ostream& os) const
{
    int p = os.precision();

    os << "Waypoint: " << _id
       << fixed << setprecision(2) 
       << " <" << _lat << ", " << _lon << ">";

    os.precision(p);
    return os;
}

Fix::Fix(const char *id, double lat, double lon):
    Waypoint(id, lat, lon), _isTerminal(true)
{
    // EYE - create a _calcBounds()?  Previously we gave fixes a
    // radius of 1000m.
}

const vector<string>& Fix::tokens()
{
    if (_tokens.empty()) {
	_tokens.push_back(_id);
	_tokens.push_back("FIX:");
    }

    return _tokens;
}

// EYE - find a way to chain these calls together from subclasses?
const char *Fix::asString()
{
    globals.str.printf("FIX: %s", _id.c_str());

    return globals.str.str();
}

ostream& Fix::_put(ostream& os) const
{
    int p = os.precision();

    os << "Fix: " << _id << " (";
    // These aren't especially clear, but they are compact and easy to
    // program.
    if (_isTerminal) {
	os << "T";
    } else {
	os << "E";
    }
    os << ") ";
    os << fixed << setprecision(2) 
       << "<" << _lat << ", " << _lon << ">";

    os.precision(p);
    return os;
}

//////////////////////////////////////////////////////////////////////
// Navaid classes
//////////////////////////////////////////////////////////////////////

Navaid::Navaid(const char *id, double lat, double lon, int elev,
	       const char *name, unsigned int freq, unsigned int range):
    Waypoint(id, lat, lon), _name(name), _elev(elev), _freq(freq), 
    _range(range)
{
}

double Navaid::signalStrength(const sgdVec3 from)
{
    if (_bounds.isEmpty()) {
	_calcBounds();
	// EYE - temporary
	assert(_bounds.isEmpty());
    }
    double d = sgdDistanceSquaredVec3(from, _bounds.center);
    if (d > 0) {
	// EYE - this could overflow the double if d is very very
	// tiny.
	return (_range * _range) / d;
    } else {
	return numeric_limits<double>::max();
    }
}

// Frequency routines
//
// These are used to check navaid frequencies.  Information about
// ranges and intervals is taken from ICAO Annex 10, FAA AIM, and
// Wikipedia.
bool Navaid::validNDBFrequency(unsigned int freq)
{
    // According to Wikipedia, NDBs are allowed to vary between 190
    // and 1750 kHz, although in North America they only operate
    // between 190 and 535 kHz.
    if ((freq < (190 * kHz)) || (freq > (1750 * kHz))) {
	return false;
    }

    // In North America, they use 1 kHz steps, but in Europe they can
    // use 0.5 kHz steps.
    if (freq % 500 != 0) {
	return false;
    }

    return true;
}

// Used by various VOR, ILS, GS and DME classes.
bool Navaid::validVHFFrequency(unsigned int freq)
{
    // VORs and DMEs are between 108.00 and 117.975 MHz.
    if ((freq < (108 * MHz)) || (freq > (117.975 * MHz))) {
	return false;
    }

    // Furthermore, they must be multiples of 50 kHz.
    if (freq % (50 * kHz) != 0) {
	return false;
    }

    return true;
}

// Used by VORs only.
bool Navaid::validVORFrequency(unsigned int freq)
{
    // A VOR must have a valid VHF frequency.
    if (!validVHFFrequency(freq)) {
	return false;
    }

    // VOR frequencies in the ILS range must be even 'tenths' - the
    // hundreds of kHz value must be an even number (eg, 110.20 is
    // okay, but 111.30 is not).
    if (freq < (112 * MHz)) {
	int hundreds = freq % MHz / (100 * kHz);
	if (hundreds & 0x1) {
	    return false;
	}
    }

    return true;
}

// Used for localizers and glideslopes.
bool Navaid::validILSFrequency(unsigned int freq)
{
    // ILS frequencies are like other VHF systems ...
    if (!validVHFFrequency(freq)) {
	return false;
    }

    // ... except that they also must be less than 112 MHz.
    if (freq >= (112 * MHz)) {
	return false;
    }

    // Finally, ILS frequencies must be odd 'tenths' - the hundreds of
    // kHz value must be an odd number (eg, 110.35 is okay, but 110.45
    // is not).
    int hundreds = freq % MHz / (100 * kHz);
    return (hundreds & 0x1);
}

void Navaid::_calcBounds()
{
    // Calculate our bounds.  We override Waypoint's _calcBounds
    // because we have an elevation and range.
    atlasGeodToCart(_lat, _lon, _elev, _bounds.center);
    _bounds.setRadius(_range);
}

NDB::NDB(const char *id, double lat, double lon, int elev,
	 const char *name, unsigned int freq, unsigned int range):
    Navaid(id, lat, lon, elev, name, freq, range)
{
}

bool NDB::validFrequency() const
{
    return validNDBFrequency(_freq);
}

const vector<string>& NDB::tokens()
{
    if (_tokens.empty()) {
	// EYE - here we should say Navaid::tokens(), which would add
	// the _id and the _name?
	_tokens.push_back(_id);
	Searchable::tokenize(_name, _tokens);
	if ((_freq % 1000) == 0) {
	    globals.str.printf("%d", _freq / kHz);
	} else {
	    globals.str.printf("%.1f", _freq / (float)kHz);
	}
	_tokens.push_back(globals.str.str());
	_tokens.push_back("NDB:");
    }

    return _tokens;
}

const char *NDB::asString()
{
    globals.str.printf("NDB: %s %s ", _id.c_str(), _name.c_str());
    if ((_freq % 1000) == 0) {
	globals.str.appendf("(%d)", _freq / kHz);
    } else {
	globals.str.appendf("(%.1f)", _freq / (float)kHz);
    }

    return globals.str.str();
}

ostream& NDB::_put(ostream& os) const
{
    int p = os.precision();

    os << "NDB: ";
    os << fixed << setprecision(2)
       << _id << " (" << _name << "): " << "<" << _lat << ", " << _lon << ", " 
       << _elev << " m>, " << setprecision(1) << _freq / (float)kHz 
       << " kHz, " << _range << " m";

    os.precision(p);
    return os;
}

VOR::VOR(const char *id, double lat, double lon, int elev, 
	 const char *name, unsigned int freq, unsigned int range, 
	 float slavedVariation):
    Navaid(id, lat, lon, elev, name, freq, range), _variation(slavedVariation)
{
}

bool VOR::validFrequency() const
{
    return validVORFrequency(_freq);
}

const vector<string>& VOR::tokens()
{
    if (_tokens.empty()) {
	_tokens.push_back(_id);
	Searchable::tokenize(_name, _tokens);
	globals.str.printf("%.2f", _freq / (float)MHz);
	_tokens.push_back(globals.str.str());
	_tokens.push_back("VOR:");
    }

    return _tokens;
}

const char *VOR::asString()
{
    // EYE - use our own AtlasString?
    globals.str.printf("VOR: %s %s (%.2f)", 
		       _id.c_str(), _name.c_str(), _freq / (float)MHz);

    return globals.str.str();
}

ostream& VOR::_put(ostream& os) const
{
    int p = os.precision();
    os << fixed << setprecision(2);

    os << "VOR: ";
    os << _id << " (" << _name << "): " 
       << "<" << _lat << ", " << _lon << ", " << _elev << " m>, " 
       << _freq / (float)MHz << " MHz, " << _range << " m, " 
       << _variation << " deg variation";

    os.precision(p);
    return os;
}

DME::DME(const char *id, double lat, double lon, int elev, 
	 const char *name, unsigned int freq, unsigned int range, float bias):
    Navaid(id, lat, lon, elev, name, freq, range), _bias(bias)
{
}

bool DME::validFrequency() const
{
    // DME frequencies in reality are in the range 962 MHz to 1213
    // MHz.  However, they are paired with VOR frequencies, and the
    // radios in an airplane automatically choose the correct DME
    // frequency when a VOR frequency is selected.  More importantly,
    // the navaid database only lists the paired VOR frequency.  So,
    // we just make sure the DME frequency is in the VHF range.  Note
    // that DMEs can be paired with VORs and with ILSs, so there's no
    // odd/even check.

    return validVHFFrequency(_freq);
}

const vector<string>& DME::tokens()
{
    if (_tokens.empty()) {
	// EYE - here we should say Navaid::tokens(), which would add
	// the _id and the _name?
	_tokens.push_back(_id);
	Searchable::tokenize(_name, _tokens);
	globals.str.printf("%.2f", _freq / (float)MHz);
	_tokens.push_back(globals.str.str());
	_tokens.push_back("DME:");
    }

    return _tokens;
}

const char *DME::asString()
{
    // EYE - use our own AtlasString?
    globals.str.printf("DME: %s %s (%.2f)", 
		       _id.c_str(), _name.c_str(), _freq / (float)MHz);

    return globals.str.str();
}

ostream& DME::_put(ostream& os) const
{
    int p = os.precision();
    os << fixed << setprecision(2);

    os << "DME: ";
    os << _id << " (" << _name << "): " << "<" << _lat << ", " << _lon 
       << ", " << _elev << " m>, " << _freq / (float)MHz << " MHz, " 
       << _range << " m, " << _bias << " m bias";

    os.precision(p);
    return os;
}

TACAN::TACAN(const char *id, double lat, double lon, int elev, 
	     const char *name, unsigned int freq, unsigned int range, 
	     float slavedVariation, float bias):
    DME(id, lat, lon, elev, name, freq, range, bias), 
    _variation(slavedVariation)
{
    // EYE - look more closely into TACAN channels to specify
    // frequencies
    //
    // From "From the Ground Up":
    //
    // To convert from TACAN "X" channels from 17X to 59X to a VHF
    // frequency:
    //
    // VHF Frequency (MHz) = (channel - 17) / 10.0 + 108.0
    //
    // eg, 38X -> (38 - 17) / 10.0 + 108.0 = 110.1
    // 
    // Channels 70X to 126X:
    //
    // VHF Frequency (MHz) = (channel - 70) / 10.0 + 112.3
    //
    // eg, 109X -> (109 - 70) / 10.0 + 112.3 = 116.2
    // 
    // For "Y" channels, add 0.05 to the result.
    //
    // eg, 38Y = 38X + 0.05 = 110.1 + 0.05 = 110.15
    //     109Y = 109X + 0.05 = 116.2 + 0.05 = 116.25
    //
    // From http://www.casa.gov.au/wcmswr/_assets/main/pilots/download/dme.pdf
    //
    // Channels 1X to 16X:
    //
    // VHF Frequency (MHz) = (channel - 1) / 10.0 + 134.4
    //
    // eg, 10X -> (10 - 1) / 10.0 + 134.4 = 135.3
    // 
    // Channels 60X to 69X:
    //
    // VHF Frequency (MHz) = (channel - 60) / 10.0 + 133.3:
    //
    // eg, 66X -> (66 - 60) / 10.0 + 133.3 = 133.9
    // 
    // This document doesn't mention the mapping from "Y" channels to
    // VHF frequencies.
    //
    // From http://www.flightsim.com/main/howto/tacan.htm
    //
    // It basically restates the From the Ground Up says, and
    // explicitly mentions that 1X - 16Y and 60X - 69Y are not paired
    // up.
}

bool TACAN::validFrequency() const
{
    // This only covers the DME part of the TACAN, but that's okay
    // because that's all we're given in the navaids database.
    return validVHFFrequency(_freq);
}

const vector<string>& TACAN::tokens()
{
    if (_tokens.empty()) {
	// EYE - here we should say Navaid::tokens(), which would add
	// the _id and the _name?
	_tokens.push_back(_id);
	Searchable::tokenize(_name, _tokens);
	globals.str.printf("%.2f", _freq / (float)MHz);
	_tokens.push_back(globals.str.str());
	_tokens.push_back("TACAN:");
    }

    return _tokens;
}

const char *TACAN::asString()
{
    // EYE - use our own AtlasString?
    globals.str.printf("TACAN: %s %s (%.2f)", 
		       _id.c_str(), _name.c_str(), _freq / (float)MHz);

    return globals.str.str();
}

ostream& TACAN::_put(ostream& os) const
{
    int p = os.precision();
    os << fixed << setprecision(2);

    os << "TACAN: ";
    os << _id << " (" << _name << "): " 
       << "<" << _lat << ", " << _lon << ", " << _elev << " m>, " 
       << _freq / (float)MHz << " MHz, " 
       << _range << " m, " 
       << _variation << " deg variation, "
       << _bias << " m bias";

    os.precision(p);
    return os;
}

LOC::LOC(const char *id, double lat, double lon, int elev, 
	 const char *name, unsigned int freq, unsigned int range, 
	 float heading):
    Navaid(id, lat, lon, elev, name, freq, range), _heading(heading)
{
}

bool LOC::validFrequency() const
{
    return validILSFrequency(_freq);
}

const vector<string>& LOC::tokens()
{
    if (_tokens.empty()) {
	_tokens.push_back(_id);
	Searchable::tokenize(_name, _tokens);
	globals.str.printf("%.2f", _freq / (float)MHz);
	_tokens.push_back(globals.str.str());
	_tokens.push_back("LOC:");
    }

    return _tokens;
}

const char *LOC::asString()
{
    // EYE - use our own AtlasString?
    globals.str.printf("LOC: %s %s (%.2f)", 
		       _id.c_str(), _name.c_str(), _freq / (float)MHz);

    return globals.str.str();
}

ostream& LOC::_put(ostream& os) const
{
    int p = os.precision();
    os << fixed << setprecision(2);

    os << "LOC: ";
    os << _id << " (" << _name << "): " << "<" << _lat << ", " << _lon 
       << ", " << _elev << " m>, " << _freq / (float)MHz << " MHz, " 
       << _range << " m, " << _heading << " deg";

    os.precision(p);
    return os;
}

GS::GS(const char *id, double lat, double lon, int elev, 
       const char *name, unsigned int freq, unsigned int range, 
       float heading, float slope):
    Navaid(id, lat, lon, elev, name, freq, range), _heading(heading),
    _slope(slope)
{
}

// Glideslope frequencies follow the same rules as localizer frequencies.
bool GS::validFrequency() const
{
    return validILSFrequency(_freq);
}

const vector<string>& GS::tokens()
{
    if (_tokens.empty()) {
	_tokens.push_back(_id);
	Searchable::tokenize(_name, _tokens);
	globals.str.printf("%.2f", _freq / (float)MHz);
	_tokens.push_back(globals.str.str());
	_tokens.push_back("GS:");
    }

    return _tokens;
}

const char *GS::asString()
{
    // EYE - use our own AtlasString?
    globals.str.printf("GS: %s %s (%.2f)", 
		       _id.c_str(), _name.c_str(), _freq / (float)MHz);

    return globals.str.str();
}

ostream& GS::_put(ostream& os) const
{
    int p = os.precision();
    os << fixed << setprecision(2);

    os << "GS: " << _id << " (" << _name << "): " 
       << "<" << _lat << ", " << _lon << ", " << _elev << " m>, " 
       << _freq / (float)MHz << " MHz, " << _range << " m, " 
       << _heading << "@" << _slope << " deg";

    os.precision(p);
    return os;
}

Marker::Marker(double lat, double lon, int elev, const char *name, 
	       float heading, Type type):
    // Markers transmit at 75 MHz, but because they're all the same,
    // we generally ignore the value.
    Navaid("", lat, lon, elev, name, 75 * MHz, 0), _heading(heading), 
    _type(type)
{
}

const vector<string>& Marker::tokens()
{
    if (_tokens.empty()) {
	Searchable::tokenize(_name, _tokens);
	_tokens.push_back("MKR:");
	if (_type == OUTER) {
	    _tokens.push_back("OM:");
	} else if (_type == MIDDLE) {
	    _tokens.push_back("MM:");
	} else {
	    _tokens.push_back("IM:");
	}
    }

    return _tokens;
}

const char *Marker::asString()
{
    // EYE - use our own AtlasString?
    globals.str.printf("MKR: ");
    if (_type == OUTER) {
	globals.str.appendf("OM: ");
    } else if (_type == MIDDLE) {
	globals.str.appendf("MM: ");
    } else {
	globals.str.appendf("IM: ");
    }
    globals.str.appendf("%s", _name.c_str());

    return globals.str.str();
}

ostream& Marker::_put(ostream& os) const
{
    int p = os.precision();
    os << fixed << setprecision(2);

    if (_type == OUTER) {
	os << "Marker (outer): ";
    } else if (_type == MIDDLE) {
	os << "Marker (middle): ";
    } else {
	os << "Marker (inner): ";
    }

    os << _name << ": " << "<" << _lat << ", " << _lon << ", " 
       << _elev << " m>, " << _heading << " deg";

    os.precision(p);
    return os;
}

//////////////////////////////////////////////////////////////////////
// NavaidSystem
//////////////////////////////////////////////////////////////////////

// Our class variables.
set<NavaidSystem *> NavaidSystem::__systems;
map<Navaid *, NavaidSystem *> NavaidSystem::__owners;

NavaidSystem::NavaidSystem()
{
    __systems.insert(this);
}

NavaidSystem::~NavaidSystem()
{
    __systems.erase(this);
}

void NavaidSystem::add(Navaid *n)
{
    // EYE - check if there's already an entry for n for a different
    // system?
    __owners[n] = this;
}

void NavaidSystem::remove(Navaid *n)
{
    __owners.erase(n);
}

NavaidSystem *NavaidSystem::owner(Navaid *n)
{
    NavaidSystem *result = NULL;

    map<Navaid *, NavaidSystem *>::iterator it = __owners.find(n);
    if (it != __owners.end()) {
	result = it->second;
    }

    return result;
}

PairedNavaidSystem::PairedNavaidSystem(Navaid *n1, Navaid *n2): 
    NavaidSystem(), _n1(n1), _n2(n2)
{
    assert(_n1 != NULL);
    assert(_n2 != NULL);
    add(_n1);
    add(_n2);
}

PairedNavaidSystem::~PairedNavaidSystem()
{
    remove(_n1);
    remove(_n2);
}

Navaid *PairedNavaidSystem::other(const Navaid *n)
{
    if (n == _n1) {
	return _n2;
    } else if (n == _n2) {
	return _n1;
    } else {
	return NULL;
    }
}

VOR_DME::VOR_DME(VOR *vor, DME *dme): PairedNavaidSystem(vor, dme)
{
}

// EYE - needed?
VOR_DME::~VOR_DME()
{
}

VOR *VOR_DME::vor()
{
    return dynamic_cast<VOR *>(n1());
}

DME *VOR_DME::dme()
{
    return dynamic_cast<DME *>(n2());
}

VOR *VOR_DME::other(DME *dme)
{
    if (dme == VOR_DME::dme()) {
	return vor();
    } else {
	return NULL;
    }
}

DME *VOR_DME::other(VOR *vor)
{
    if (vor == VOR_DME::vor()) {
	return dme();
    } else {
	return NULL;
    }
}

VORTAC::VORTAC(VOR *vor, TACAN *tacan): PairedNavaidSystem(vor, tacan)
{
}

// EYE - needed?
VORTAC::~VORTAC()
{
}

VOR *VORTAC::vor()
{
    return dynamic_cast<VOR *>(n1());
}

TACAN *VORTAC::tacan()
{
    return dynamic_cast<TACAN *>(n2());
}

VOR *VORTAC::other(TACAN *tacan)
{
    if (tacan == VORTAC::tacan()) {
	return vor();
    } else {
	return NULL;
    }
}

TACAN *VORTAC::other(VOR *vor)
{
    if (vor == VORTAC::vor()) {
	return tacan();
    } else {
	return NULL;
    }
}

NDB_DME::NDB_DME(NDB *ndb, DME *dme): PairedNavaidSystem(ndb, dme)
{
}

// EYE - needed?
NDB_DME::~NDB_DME()
{
}

NDB *NDB_DME::ndb()
{
    return dynamic_cast<NDB *>(n1());
}

DME *NDB_DME::dme()
{
    return dynamic_cast<DME *>(n2());
}

NDB *NDB_DME::other(DME *dme)
{
    if (dme == NDB_DME::dme()) {
	return ndb();
    } else {
	return NULL;
    }
}

DME *NDB_DME::other(NDB *ndb)
{
    if (ndb == NDB_DME::ndb()) {
	return dme();
    } else {
	return NULL;
    }
}

//////////////////////////////////////////////////////////////////////
// ILS
//////////////////////////////////////////////////////////////////////

ILS::ILS(LOC *loc, Type type): 
    NavaidSystem(), _type(type), _loc(loc), _gs(NULL), _dme(NULL)
{
    add(loc);
}

ILS::~ILS()
{
    remove(_loc);
    remove(_gs);
    remove(_dme);

    set<Marker *>::iterator it;
    for (it = _markers.begin(); it != _markers.end(); it++) {
	remove(*it);
    }
    _markers.clear();
}

void ILS::setGS(GS *gs)
{
    // Make sure GS hasn't been set already.
    // EYE - return silently?  Overwrite the old one? Throw an error?
    // (ditto for setDME)
    assert(_gs == NULL);

    // Make sure its id, name, and frequency match the localizer's.
    // EYE - return silently without setting _gs?  Throw an error?
    assert(gs->id() == loc()->id());
    assert(gs->name() == loc()->name());
    assert(gs->frequency() == loc()->frequency());

    _gs = gs;
    add(_gs);
}

void ILS::setDME(DME *dme)
{
    // Make sure DME hasn't been set already.
    assert(_dme == NULL);

    // Make sure its id, name, and frequency match the localizer's.
    assert(dme->id() == loc()->id());
    assert(dme->name() == loc()->name());
    assert(dme->frequency() == loc()->frequency());

    _dme = dme;
    add(_dme);
}

void ILS::addMarker(Marker *m)
{
    // Make sure its name matches the localizer's.
    assert(m->name() == loc()->name());

    _markers.insert(m);
    add(m);
}

ILS *ILS::ils(LOC *loc)
{
    return dynamic_cast<ILS *>(NavaidSystem::owner(loc));
}

ostream& operator<<(ostream& os, const ILS& ils)
{
    os << "ILS (" ;
    if (ils.type() == ILS::ILS_CAT_I) {
	os << "CAT I";
    } else if (ils.type() == ILS::ILS_CAT_II) {
	os << "CAT II";
    } else if (ils.type() == ILS::ILS_CAT_III) {
	os << "CAT III";
    } else if (ils.type() == ILS::LDA) {
	os << "LDA";
    } else if (ils.type() == ILS::IGS) {
	os << "IGS";
    } else if (ils.type() == ILS::Localizer) {
	os << "LOC";
    } else if (ils.type() == ILS::SDF) {
	os << "SDF";
    }
    os << "): " << ils.id() << " (" << ils.name() << ")" << endl;
    os << "\t" << *(ils._loc) << endl;
    if (ils.gs() != NULL) {
	os << "\t" << *(ils.gs()) << endl;
    }
    if (ils.dme() != NULL) {
	os << "\t" << *(ils.dme()) << endl;
    }
    
    set<Marker *>::const_iterator i = ils._markers.begin();
    for (; i != ils._markers.end(); i++) {
	os << "\t" << *(*i) << endl;
    }
    
    return os;
}

//////////////////////////////////////////////////////////////////////
// Airways
//////////////////////////////////////////////////////////////////////

#include <iostream>

// Our class variable.  Every time we create a new segment, it adds
// itself to this vector.  Similarly, each time the destructor is
// called, it removes itself from this vector.
set<Segment *> Segment::__segments;

Segment::Segment(const string& name, Waypoint *start, Waypoint *end, 
		 int base, int top, bool isLow):
    _name(name), _start(start), _end(end), _base(base), _top(top), _isLow(isLow)
{
    assert(_base <= _top);

    // The airway bounds are given by its two endpoints.
    // EYE - save these two points?
    sgdVec3 point;
    atlasGeodToCart(start->lat(), start->lon(), 0.0, point);
    _bounds.extend(point);
    atlasGeodToCart(end->lat(), end->lon(), 0.0, point);
    _bounds.extend(point);

    // Calculate our length.
    double az1, az2;
    geo_inverse_wgs_84(0.0, start->lat(), start->lon(), 
		       end->lat(), end->lon(),
		       &az1, &az2, &_length);

    __segments.insert(this);
}

// Note that we don't delete the airways in the _airways set.  It's
// best not to delete segments until after you delete the airways of
// which they are members.
Segment::~Segment()
{
    // A segment should not be deleted if it's part of an airway.
    assert(_airways.size() == 0);

    // Now remove ourselves from __segments.

    // EYE - see note with ILS::~ILS
    __segments.erase(this);
}

ostream& operator<<(ostream& os, const Segment& seg)
{
    os << "SEG: " << seg.name() << " <" << seg.start()->id() << " - "
       << seg.base() << ", " << seg.top() << " - " << seg.end()->id() << ">";
    if (seg.isLow()) {
	os << " (low), ";
    } else {
	os << " (high), ";
    }
    os << seg.airways().size() << " airways";

    return os;
}

// This routine checks if the airway name follows the ICAO standard.
//
// According to ICAO Annex 11, Appendix 1, Section 2, an airway name:
//
// - consists of at most 6 uppercase characters and ciphers
//
// - starts with one or two uppercase characters and is followed by a
//   number between 1 and 999 without leading zeros and may end with
//   an additional character
//
// - of the three possible characters the first one is optional and
//   may be 'K', 'U', or 'S'
//
// - of the three possible characters the second one is mandatory and
//   may be 'A', 'B', 'G', 'H', 'J', 'L', 'M', 'N', 'P', 'Q', 'R',
//   'T', 'V', 'W', 'Y', or 'Z'
//
// - of the three possible characters the third one (after the number)
//   is optional and may be 'F', 'G', 'Y', or 'Z'.
//
// - the following characters are not allowed in any position: 'C',
//   'D', 'E', 'I', 'O', and 'X'
//
// Or, as a regular expression:
//
// [KUS]?[ABGHJLMNPQRTVWYZ][1-9][0-9]{0,2}[FGYZ]?

/* Generated by re2c 0.13.5 on Fri Jul 31 10:24:58 2009 */
static bool __awyNameMatch(const char *p)
{
    const char *YYMARKER;
    {
	char yych;

	yych = (const char)*p;
	switch (yych) {
	  case 'A':
	  case 'B':
	  case 'G':
	  case 'H':
	  case 'J':
	  case 'L':
	  case 'M':
	  case 'N':
	  case 'P':
	  case 'Q':
	  case 'R':
	  case 'T':
	  case 'V':
	  case 'W':
	  case 'Y':
	  case 'Z':	goto yy4;
	  case 'K':
	  case 'S':
	  case 'U':	goto yy2;
	  default:	goto yy5;
	}
    yy2:
	yych = (const char)*(YYMARKER = ++p);
	switch (yych) {
	  case 'A':
	  case 'B':
	  case 'G':
	  case 'H':
	  case 'J':
	  case 'L':
	  case 'M':
	  case 'N':
	  case 'P':
	  case 'Q':
	  case 'R':
	  case 'T':
	  case 'V':
	  case 'W':
	  case 'Y':
	  case 'Z':	goto yy11;
	  default:	goto yy3;
	}
    yy3:
	{ return false; }
    yy4:
	yych = (const char)*++p;
	switch (yych) {
	  case '1':
	  case '2':
	  case '3':
	  case '4':
	  case '5':
	  case '6':
	  case '7':
	  case '8':
	  case '9':	goto yy6;
	  default:	goto yy3;
	}
    yy5:
	yych = (const char)*++p;
	goto yy3;
    yy6:
	++p;
	switch ((yych = (const char)*p)) {
	  case '0':
	  case '1':
	  case '2':
	  case '3':
	  case '4':
	  case '5':
	  case '6':
	  case '7':
	  case '8':
	  case '9':	goto yy8;
	  case 'F':
	  case 'G':
	  case 'Y':
	  case 'Z':	goto yy9;
	  default:	goto yy7;
	}
    yy7:
	{ return p; }
    yy8:
	yych = (const char)*++p;
	switch (yych) {
	  case '0':
	  case '1':
	  case '2':
	  case '3':
	  case '4':
	  case '5':
	  case '6':
	  case '7':
	  case '8':
	  case '9':	goto yy10;
	  case 'F':
	  case 'G':
	  case 'Y':
	  case 'Z':	goto yy9;
	  default:	goto yy7;
	}
    yy9:
	yych = (const char)*++p;
	goto yy7;
    yy10:
	yych = (const char)*++p;
	switch (yych) {
	  case 'F':
	  case 'G':
	  case 'Y':
	  case 'Z':	goto yy9;
	  default:	goto yy7;
	}
    yy11:
	yych = (const char)*++p;
	switch (yych) {
	  case '1':
	  case '2':
	  case '3':
	  case '4':
	  case '5':
	  case '6':
	  case '7':
	  case '8':
	  case '9':	goto yy6;
	  default:	goto yy12;
	}
    yy12:
	p = YYMARKER;
	goto yy3;
    }
}

// Our class variable.  When an airway is constructed, it adds itself
// to __airways.  Similarly when its destructor is called, it removes
// itself from __airways.
set<Airway *> Airway::__airways;

Airway::Airway(const string& name, bool isLow, Segment *segment):
    _name(name), _isLow(isLow)
{
    prepend(segment);

    // Issue a warning if the airway name doesn't follow the ICAO
    // standard.
    if (!__awyNameMatch(name.c_str())) {
	// EYE - we need a logging facility
    	// fprintf(stderr, "Invalid airway name: '%s'\n", name.c_str());
    }
    __airways.insert(this);
}

Airway::~Airway()
{
    // Remove ourselves from our component segments first.
    deque<Segment *>::iterator i;
    for (i = _segments.begin(); i != _segments.end(); i++) {
	(*i)->removeAirway(this);
    }

    // Now remove ourselves from the __airways set.

    // EYE - see note with ILS::~ILS
    __airways.erase(this);
}

void Airway::prepend(Segment *segment) 
{
    _segments.push_front(segment);
    segment->addAirway(this);

    // Expand our bounds to encompass the new segment.
    _bounds.extend(segment->start()->bounds().center);
    _bounds.extend(segment->end()->bounds().center);
}

void Airway::append(Segment *segment) 
{
    _segments.push_back(segment);
    segment->addAirway(this);

    // Expand our bounds to encompass the new segment.
    _bounds.extend(segment->start()->bounds().center);
    _bounds.extend(segment->end()->bounds().center);
}

const Waypoint *Airway::nthWaypoint(size_t i) const
{
    // Special cases.
    if (_segments.size() == 0) {
	return NULL;
    }
    if (i > _segments.size()) {
	return NULL;
    }
    if (_segments.size() == 1) {
	if (i == 0) {
	    return _segments[0]->start();
	} else {
	    return _segments[0]->end();
	}
    }

    if (i == 0) {
	// Check which point doesn't match a following point.
	if ((_segments[0]->start() != _segments[1]->start()) &&
	    (_segments[0]->start() != _segments[1]->end())) {
	    return _segments[0]->start();
	} else {
	    return _segments[0]->end();
	}
    } else if (i == _segments.size()) {
	// Check which point doesn't match a preceding point.
	if ((_segments[i - 1]->start() != _segments[i - 2]->start()) &&
	    (_segments[i - 1]->start() != _segments[i - 2]->end())) {
	    return _segments[i - 1]->start();
	} else {
	    return _segments[i - 1]->end();
	}
    } else {
	// Check which point matches a preceding point.
	if ((_segments[i]->start() == _segments[i - 1]->start()) ||
	    (_segments[i]->start() == _segments[i - 1]->end())) {
	    return _segments[i]->start();
	} else {
	    return _segments[i]->end();
	}
    }
}

ostream& operator<<(ostream& os, const Airway& awy)
{
    os << "AWY: " << awy.name();
    if (awy.isLow()) {
	os << " (low)";
    } else {
	os << " (high)";
    }
    os << ": ";
    for (size_t i = 0; i < awy.noOfWaypoints(); i++) {
	os << awy.nthWaypoint(i)->id() << " ";
    }

    return os;
}

// Calculates the nearest point on line AB to point P, placing it in
// 'where'.  It returns the distance squared between 'where' and P.
static double _nearestPoint(const sgdVec3 P, const sgdVec3 A, const sgdVec3 B,
			    sgdVec3& where)
{
    sgdVec3 AtoP, AtoB;

    sgdSubVec3(AtoP, P, A);
    sgdSubVec3(AtoB, B, A);

    double magnitudeAtoB = sgdScalarProductVec3(AtoB, AtoB);
    double ABAproduct = sgdScalarProductVec3(AtoP, AtoB);
    double distance = ABAproduct / magnitudeAtoB;

    if (distance < 0.0) {
	sgdCopyVec3(where, A);
    } else if (distance > 1.0) {
	sgdCopyVec3(where, B);
    } else {
	sgdScaleVec3(AtoB, distance);
	sgdAddVec3(where, A, AtoB);
    }

    return sgdDistanceSquaredVec3(P, where);
}

const double *Airway::location(const sgdVec3 from)
{
    static sgdVec3 result;
    double nearest = numeric_limits<double>::max();
    deque<Segment *>::const_iterator it;
    for (it = _segments.begin(); it != _segments.end(); it++) {
	Segment *s = *it;

	// Create parameters that will make PLIB happy.
	sgdVec3 A, B, t;
	sgdCopyVec3(A, s->start()->bounds().center);
	sgdCopyVec3(B, s->end()->bounds().center);
	double d = _nearestPoint(from, A, B, t);
	if (d < nearest) {
	    nearest = d;
	    sgdCopyVec3(result, t);
	}
    }

    return result;
}

// We return the distance (squared) to the nearest point on the
// airway.
double Airway::distanceSquared(const sgdVec3 from)
{
    double result = numeric_limits<double>::max();
    deque<Segment *>::const_iterator it;
    for (it = _segments.begin(); it != _segments.end(); it++) {
	Segment *s = *it;

	// Create parameters that will make PLIB happy.
	sgdLineSegment3 line;
	sgdCopyVec3(line.a, s->start()->bounds().center);
	sgdCopyVec3(line.b, s->end()->bounds().center);
	double d = sgdDistSquaredToLineSegmentVec3(line, from);
	if (d < result) {
	    result = d;
	}
    }

    return result;
}

const vector<string>& Airway::tokens()
{
    if (_tokens.empty()) {
	_tokens.push_back("AWY:");
	// EYE - change to _id for consistency?
	_tokens.push_back(_name);
    }

    return _tokens;
}

const char *Airway::asString()
{
    // EYE - use our own AtlasString?
    globals.str.printf("AWY: %s", _name.c_str());
    return globals.str.str();
}

//////////////////////////////////////////////////////////////////////
// Airports and runways
//////////////////////////////////////////////////////////////////////

RWY::RWY(char *label, double lat, double lon, float hdg, float len, float wid,
	 ARP *ap):
    _lat(lat), _lon(lon), _hdg(hdg), _length(len), _width(wid)
{
    assert(strlen(label) <= 3);
    snprintf(_label, sizeof(_label), "%s", label);
    // Initialize _otherLabel to an empty string for now.  If
    // otherLabel() is called, it will fill it in with the correct
    // label.
    _otherLabel[0] = '\0';

    // Find out our bounds, and tell our owning airport.
    _setBounds(lat, lon, ap->elevation());
    ap->extend(bounds());
}

RWY::RWY(char *lbl1, double lat1, double lon1, 
	 char *lbl2, double lat2, double lon2, 
	 float width, ARP *ap): _width(width)
{
    assert(strlen(lbl1) <= 3);
    snprintf(_label, sizeof(_label), "%s", lbl1);
    assert(strlen(lbl2) <= 3);
    snprintf(_otherLabel, sizeof(_otherLabel), "%s", lbl2);

    // Convert the two ends to a centre and a heading.  This,
    // unfortunately, takes a bit of effort.  Why not just store the
    // two <lat,lon> pairs?  Two <lat, lon> pairs take more space than
    // a single <lat,lon> pair (two doubles) with a length and a
    // heading (two floats).  Also, the calculations required when
    // drawing the runway are a bit more difficult with two endpoints
    // than a single centre point.

    // EYE - if we cached airport/runway/navaid data, all these
    // calculations would only need to be done once, rather than every
    // time Atlas starts.
    SGGeod p1 = SGGeod::fromDeg(lon1, lat1), p2 = SGGeod::fromDeg(lon2, lat2);

    // First, calculate the distance between the two endpoints, and
    // the heading from p1 to p2 (and p2 to p1, but we don't care
    // about that).
    double hdg12, length, hdg21;
    SGGeodesy::inverse(p1, p2, hdg12, hdg21, length);

    // Now figure out where the centre is.
    SGGeod centre = SGGeodesy::direct(p1, hdg12, length / 2.0);

    // Finally, figure out the heading at the centre (ie, the heading
    // from the centre to the second endpoint).  In most cases, this
    // is the same as hdg12, but in extreme cases (ie, at the poles,
    // or for very very long runways), it can be quite different.
    double hdgc2, hdg2c, tmpLen;
    SGGeodesy::inverse(centre, p2, hdgc2, hdg2c, tmpLen);

    _hdg = hdgc2;
    _length = length;
    _lat = centre.getLatitudeDeg();
    _lon = centre.getLongitudeDeg();

    // Find out our bounds, and tell our owning airport.
    _setBounds(centre.getLatitudeDeg(), centre.getLongitudeDeg(), 
	       ap->elevation());
    ap->extend(bounds());
}

// In airport data files before version 1000, we are only given the
// label of one end of the runway, and need to calculate the name of
// the other end.  This method will do that if _otherLabel is
// uninitialized.
//
// So, if we can calculate the name of one end of a runway from the
// other, why bother saving both?  Because you actually *can't* always
// calculate the name of one end of a runway from the other.  There
// are runways that curve (eg, Elk City Airport, S90), which has a
// runway 14/35.  In old versions of apt.dat, these would have been
// labelled incorrectly (14/32).  In newer versions of apt.dat, we are
// explicitly given the names of both ends, so we can label them
// properly (of course, we still don't *draw* curved runways
// correctly, as we are only given the two endpoints).
const char *RWY::otherLabel()
{
    if (_otherLabel[0] == '\0') {
	int hdg;
	unsigned int length;
	sscanf(_label, "%d%n", &hdg, &length);
	assert((length == strlen(_label)) || (length = strlen(_label) - 1));
	hdg = (hdg + 18) % 36;
	if (hdg == 0) {
	    hdg = 36;
	}

	// Handle trailing character (if it exists).  If the character
	// is 'L' or 'R', swap it for 'R' and 'L' respectively.
	// Otherwise assume that we should leave it alone.  If it
	// doesn't exist, set it to '\0'.
	char lr = '\0';
	if (length < strlen(_label)) {
	    if (_label[length] == 'R') {
		lr = 'L';
	    } else if (_label[length] == 'L') {
		lr = 'R';
	    } else {
		// EYE - warn if not a standard suffix?  'C' is the
		// most common, but many northern airports have 'T'
		// suffixes (eg, CYTE).  There are several with 'S'
		// sufixes (eg, HEBA), and KHOP in 1000 has a runway
		// "23H/05H".
		lr = _label[length];
	    }
	}
	sprintf(_otherLabel, "%02d%c", hdg, lr);
	assert(_otherLabel[0] != '\0');
    }

    return _otherLabel;
}

// EYE - setBounds, along with the RWY constructor, are pretty
// expensive (but necessary), so we should take advantage of the
// chance to pre-calculate some drawing data (eg, the runway quad).
void RWY::_setBounds(double lat, double lon, float elev)
{
    atlasGeodToCart(lat, lon, elev, _bounds.center);
    _bounds.setRadius(sqrt((_width * _width) + (_length * _length)));
}

// ARP::_lat and _beaconLat are initialized to __invalidLat.  If _lat
// or _lon are accessed before being initialized, we calculate them
// from _bounds (this assumes that _bounds has been set to its final
// value, which is true once the entry for the airport - including its
// runways - has been read from apt.dat).
//
// An invalid _beaconLat indicates that this airport has no beacon (a
// fact used in the beacon() method).
const double __invalidLat = 100.0;

ARP::ARP(char *name, char *code, float elev):
    _elev(elev), _controlled(false), _lighting(false), _lat(__invalidLat), 
    _beaconLat(__invalidLat)
{
    _name = strdup(name);
    assert(strlen(code) <= 4);
    // Why use snprintf() instead of strncpy()?  Because strncpy() is
    // very tricky to use correctly.  A better substitute is
    // strlcpy(), but I don't think it's very portable.
    snprintf(_code, sizeof(_code), "%s", code);
}

ARP::~ARP()
{
    free(_name);
    for (size_t i = 0; i < _rwys.size(); i++) {
	delete _rwys[i];
    }
}

void ARP::addFreq(ATCCodeType t, int freq, char *label)
{
    set<int>& freqs = _freqs[t][label];
    if ((freq % 10 == 2) || (freq % 10 == 7)) {
	freqs.insert((freq * 10 + 5) * 1000);
    } else {
	freqs.insert((freq * 10) * 1000);
    }
}

double ARP::distanceSquared(const sgdVec3 from)
{
    return sgdDistanceSquaredVec3(_bounds.center, from);
}

// Returns our tokens, generating them if they haven't been already.
const vector<string>& ARP::tokens()
{
    if (_tokens.empty()) {
	// The id is a token.
	_tokens.push_back(_code);

	// Tokenize the name.
	Searchable::tokenize(_name, _tokens);

	// Add an "AIR:" token.
	_tokens.push_back("AIR:");
    }

    return _tokens;
}

// Returns our pretty string.
const char *ARP::asString()
{
    globals.str.printf("AIR: %s %s", _code, _name);
    return globals.str.str();
}

// We derive the airport's latitude and longitude lazily.
double ARP::latitude()
{
    if (_lat == __invalidLat) {
	_calcLatLon();
    }
    assert(_lat != __invalidLat);
    return _lat;
}

double ARP::longitude()
{
    if (_lat == __invalidLat) {
	_calcLatLon();
    }
    assert(_lat != __invalidLat);
    return _lon;
}

void ARP::setBeaconLoc(double lat, double lon)
{
    _beaconLat = lat;
    _beaconLon = lon;
}

bool ARP::beacon()
{
    return (_beaconLat != __invalidLat);
}

// Calculates the airport's center in lat, lon from its bounds.
void ARP::_calcLatLon()
{
    double alt;
    sgdVec3 c;
    sgdSetVec3(c,
	       _bounds.center[0], 
	       _bounds.center[1], 
	       _bounds.center[2]);
    sgCartToGeod(c, &_lat, &_lon, &alt);
    _lat *= SGD_RADIANS_TO_DEGREES;
    _lon *= SGD_RADIANS_TO_DEGREES;
}

// // EYE - just needed for SGTimeStamp
// #include <simgear/timing/timestamp.hxx>

NavData::NavData(const char *fgRoot, Searcher *searcher): _searcher(searcher)
{
    // Create our cullers first - when we load the files, we'll be
    // adding data to them.
    for (size_t i = 0; i < _COUNT; i++) {
	Culler *c = new Culler();
	_cullers.push_back(c);
	_frustumCullers.push_back(new Culler::FrustumSearch(*c));
	if (i == NAVAIDS) {
	    _navaidsPointCuller = new Culler::PointSearch(*c);
	}
    }

    // Load the data.

    // EYE - the load functions are all very similar - can we abstract
    // most of it out?

    // SGTimeStamp t1, t2;
    // t1.stamp();
    _loadNavaids(fgRoot);
    // t2 = SGTimeStamp::now() - t1;
    // printf("\t%.4f s\n", t2.toSecs());
    // t1.stamp();
    _loadFixes(fgRoot);
    // t2 = SGTimeStamp::now() - t1;
    // printf("\t%.4f s\n", t2.toSecs());
    // t1.stamp();
    _loadAirways(fgRoot);
    // t2 = SGTimeStamp::now() - t1;
    // printf("\t%.4f s\n", t2.toSecs());
    // t1.stamp();
    _loadAirports(fgRoot);
    // t2 = SGTimeStamp::now() - t1;
    // printf("\t%.4f s\n", t2.toSecs());
}

NavData::~NavData()
{
    delete _navaidsPointCuller;
    for (size_t i = 0; i < _frustumCullers.size(); i++) {
	delete _frustumCullers[i];
    }
    _frustumCullers.clear();
    for (size_t i = 0; i < _cullers.size(); i++) {
	delete _cullers[i];
    }
    _cullers.clear();
    for (size_t i = 0; i < _airports.size(); i++) {
	ARP *ap = _airports[i];
	// EYE - need to test ARP destructor (and others) for memory
	// leaks.
	_searcher->remove(ap);
	delete ap;
    }
    _airports.clear();

    // EYE - not particularly efficient.  Should we really have these
    // static class variables at all?  Or should we provide another
    // static method to efficiently delete all objects?  Also, should
    // deleting an airway delete its segments (assuming they aren't
    // shared by any other airways)?
    while (Airway::airways().size() > 0) {
	Airway *awy = *(Airway::airways().begin());
	_searcher->remove(awy);
	delete awy;
    }
    while (Segment::segments().size() > 0) {
	delete *(Segment::segments().begin());
    }
}

// const vector<Cullable *>& NavData::getNavaids(sgdVec3 p)
// {
//     // EYE - either document this, or force the caller to pass in a
//     // vector that we fill.
//     static vector<Cullable *> results;

//     results.clear();

//     // EYE - should we do this?
//     _navaidsPointCuller->move(p);
//     results = _navaidsPointCuller->intersections();

//     return results;
// }
void NavData::getNavaids(sgdVec3 p, vector<Cullable *>& navaids)
{
    navaids.clear();

    // EYE - should we do this?  Will this void other search results?
    // We should at least warn callers in the documentation.
    _navaidsPointCuller->move(p);
    navaids = _navaidsPointCuller->intersections();
}

// // EYE - instead of taking a FlightData structure, pass in the
// // location and frequency/frequencies?  That way we remove our
// // dependence on another class.  Or just provide the previous
// // getNavaids() and get the caller to match with frequencies?
// const vector<Cullable *>& NavData::getNavaids(FlightData *p)
// {
//     // EYE - either document this, or force the caller to pass in a
//     // vector that we fill.
//     static vector<Cullable *> results;

//     results.clear();

//     if (p == NULL) {
// 	return results;
//     }

//     // We don't do anything if this is from an NMEA track.
//     // Unfortunately, there's no explicit marker in a FlightData
//     // structure that tells us what kind of track it is.  However,
//     // NMEA tracks have their frequencies and radials set to 0, so we
//     // just check for that (and in any case, if frequencies are 0, we
//     // won't match any navaids anyway).
//     if ((p->nav1_freq == 0) && (p->nav2_freq == 0) && (p->adf_freq == 0)) {
// 	return results;
//     }

//     const vector<Cullable *>& navaids = getNavaids(p->cart);
	
//     for (unsigned int i = 0; i < navaids.size(); i++) {
// 	Navaid *n = dynamic_cast<Navaid *>(navaids[i]);
// 	if (n) {
// 	    if (p->nav1_freq == (n->frequency() / Navaid::kHz)) {
// 		results.push_back(n);
// 	    } else if (p->nav2_freq == (n->frequency() / Navaid::kHz)) {
// 		results.push_back(n);
// 	    } else if (p->adf_freq == (n->frequency() / Navaid::kHz)) {
// 		results.push_back(n);
// 	    }
// 	}
//     }

//     return results;
// }

void NavData::move(const sgdMat4 modelViewMatrix)
{
    for (int i = 0; i < _COUNT; i++) {
	_frustumCullers[i]->move(modelViewMatrix);
    }
}

void NavData::zoom(const sgdFrustum& frustum)
{
    for (int i = 0; i < _COUNT; i++) {
	_frustumCullers[i]->zoom(frustum.getLeft(),
				 frustum.getRight(),
				 frustum.getBot(),
				 frustum.getTop(),
				 frustum.getNear(),
				 frustum.getFar());
    }
}

void NavData::_loadNavaids(const char *fgRoot)
{
    SGPath f(fgRoot);
    // EYE - magic name
    f.append("Navaids/nav.dat.gz");

    gzFile arp;
    char *line;

    printf("Loading navaids from\n  %s\n", f.c_str());
    arp = gzopen(f.c_str(), "rb");
    if (arp == NULL) {
	fprintf(stderr, "_loadNavaids: Couldn't open \"%s\".\n", f.c_str());
	throw runtime_error("couldn't open navaids file");
    } 

    // Check the file version.  We can handle version 810 files.  Note
    // that there was a mysterious (and stupid, in my opinion) change
    // in how DMEs were formatted some time after data cycle 2007.09.
    // So we need to check the data cycle as well.  Unfortunately, the
    // file version line doesn't have a constant format.  We could
    // have the following two:
    //
    // 810 Version - data cycle 2008.05
    //
    // 810 Version - DAFIF data cycle 2007.09
    int version = -1;
    int index;
    float cycle = 0.0;
    gzGetLine(arp, &line);	// Windows/Mac header
    gzGetLine(arp, &line);	// Version
    sscanf(line, "%d Version - %n", &version, &index);
    if (strncmp(line + index, "DAFIF ", 6) == 0) {
	index += 6;
    }
    sscanf(line + index, "data cycle %f", &cycle);
    if (version == 810) {
	// It looks like we have a valid file.
	_loadNavaids810(cycle, arp);
    } else {
	fprintf(stderr, "_loadNavaids: \"%s\": unknown version %d.\n", 
		f.c_str(), version);
	throw runtime_error("unknown navaids file version");
    }

    gzclose(arp);
    printf("  ... done\n");
}

// Convenience routine.  Checks if the given navaid matches an ILS in
// the given multimap.  The multimap is a mapping from a name (like
// "ZWWW 07") to one or more ILS systems.  Returns the matching ILS if
// found, NULL otherwise.  To match, a glideslope or DME must have the
// same name, id, and frequency as the ILS; a marker must have the
// same name (markers have no ids or frequencies).
ILS *__matches(const multimap<string, ILS *>& lmap, const Navaid *n)
{
    pair<multimap<string, ILS *>::const_iterator,
	multimap<string, ILS *>::const_iterator> range = 
	lmap.equal_range(n->name());
    multimap<string, ILS *>::const_iterator i;
    for (i = range.first; i != range.second; i++) {
	if (dynamic_cast<const Marker *>(n)) {
	    // If a marker matches the name, that's good enough -
	    // markers have no ids or frequencies.
	    return i->second;
	}

	// If it's not a marker, then we check the id and frequency as
	// well (although the frequency check is probably not needed).
	ILS *ils = i->second;
	if (n->id() != ils->loc()->id()) {
	    // This one doesn't match.  See if another with the same
	    // name does.
	    continue;
	}
	if (n->frequency() != ils->loc()->frequency()) {
	    // This one doesn't match.  See if another with the same
	    // name does.
	    continue;
	}

	// Same name, id, and frequency.  It's a match!
	return ils;
    }

    return NULL;
}

// Used to see if paired navaids match.

// EYE - we used to check the type as well, but we don't have types
// any more.  Is this strict enough?  We can't use frequencies, as
// NDBs can be paired with DMEs.  Locations won't be exactly the same,
// although they should be close.
struct __NavaidLessThan {
    bool operator()(Navaid *left, Navaid* right) const {
	if (left->id() != right->id()) {
	    return (left->id() < right->id());
	}
	return (left->name() < right->name());
    };
};

void NavData::_loadNavaids810(float cycle, const gzFile& arp)
{
    // Type codes - these are the numbers at the start of each line in
    // the nav data file.  Most are self-explanatory; 'dme_sub' means
    // 'subsidiary DME', which is a DME where X-Plane suppresses
    // display of frequency information.
    enum {ndb = 2, vor = 3, ils = 4, loc = 5, gs = 6,
	  om = 7, mm = 8, im = 9, dme_sub = 12, dme = 13,
	  last_line = 99};

    // This is used for creating paired navaids.  We use the navaids'
    // ids and names to compare them.
    set<Navaid *, __NavaidLessThan> navaidSet;

    // This is used for constructing ILS systems.  We use the
    // localizer name as a key, so that we can match them with
    // markers, glideslopes, and DMEs.  Markers have no ids or
    // frequencies, so we can only match them with localizers based on
    // name.  Localizer names are not guaranteed to be unique (eg,
    // LOWI 26), so we must use a multimap.
    multimap<string, ILS *> ILSMap;

    // We keep track of the line number for reporting errors.  When
    // we're called, the first two lines have already been read.
    int lineNumber = 2;
    char *line;
    while (gzGetLine(arp, &line)) {
	int lineCode, offset;
	double lat, lon;
	int elev, freq, range;
	double magvar;
	char id[5];

	lineNumber++;
	if (strcmp(line, "") == 0) {
	    // Blank line.
	    continue;
	} 

	if (strcmp(line, "99") == 0) {
	    // Last line.
	    break;
	}

	// A line looks like this:
	//
	// <code> <lat> <lon> <elev> <freq> <range> <magvar> <id> <name>
	//
	// Where name is a string ending with a "type" (eg, a VOR,
	// type code 3, can either be a VOR, VOR-DME, or VORTAC).
	// This type embedded at the end of the name isn't officially
	// in the navaid data file specification, so we can't
	// absolutely count on it.  On the other hand, every file I've
	// checked consistently has it, and it's useful, so we'll use
	// it.
	//
	// Note that 'magvar' really represents many things, depending
	// on the specific navaid: slaved variation for VORs, bearing
	// for localizers and markers, bearing *and* slope for
	// glideslopes, and bias for DMEs.
	if (sscanf(line, "%d %lf %lf %d %d %d %lf %s %n", &lineCode, 
		   &lat, &lon, &elev, &freq, &range, &magvar, id, &offset)
	    != 8) {
	    cerr << lineNumber << ": parse error:" << endl;
	    cerr << line << endl;
	    continue;
	}
	assert(lineCode != last_line);

	// Set 'name'.  Note that it will contain more than the name,
	// so we'll need to insert a few strategically-located nulls
	// to get the correct name.
	char *name = line + offset;

	// Find the "type", which is the last space-delimited string.
	char *type = lastToken(name);
	assert(type != NULL);

	// Convert some of the values to our internal units.  Other
	// conversions need to be made, but they depend on the navaid.
	elev *= SG_FEET_TO_METER; // feet to metres
	range *= SG_NM_TO_METER;  // nautical miles to metres

	// We slightly alter the representation of frequencies.  In
	// the navaid database, NDB frequencies are given in kHz,
	// whereas VOR/ILS/DME/... frequencies are given in 10s of
	// kHz.  We adjust them to Hz.
	if (lineCode == ndb) {
	    freq *= 1000;
	} else {
	    freq *= 10000;
	}

	// Due to the "great DME shift" of 2007.09, we might need to
	// do extra processing to handle DMEs.
	//
	// Here's the picture:
	//
	// Before 2007.09:		After:
	// Foo Bar DME-ILS		Foo Bar DME-ILS
	// Foo Bar DME			Foo Bar DME
	// Foo Bar NDB-DME		Foo Bar NDB-DME DME
	// Foo Bar TACAN		Foo Bar TACAN DME
	// Foo Bar VORTAC		Foo Bar VORTAC DME
	// Foo Bar VOR-DME		Foo Bar VOR-DME DME
	//
	// The type is now less useful, only telling us about
	// DME-ILSs.  To find out the real subtype, we need to back
	// one more token and look at that.  However, that doesn't
	// work for "pure" DMEs (ie, "Foo Bar DME").  So, if the next
	// token isn't NDB-DME, TACAN, VORTAC, or VOR-DME, then we
	// must be looking at a pure DME.
	if (((lineCode == dme_sub) || (lineCode == dme)) && 
	    (cycle > 2007.09) && 
	    (strcmp(type, "DME-ILS") != 0)) {
	    // New format.  Yuck.  We need to find the "real" type by
	    // looking back one token.
	    char *subType = lastToken(name, type);
	    if ((strncmp(subType, "NDB-DME", 7) == 0) ||
		(strncmp(subType, "TACAN", 5) == 0) ||
		(strncmp(subType, "VORTAC", 6) == 0) ||
		(strncmp(subType, "VOR-DME", 7) == 0)) {
		// The subtype is the real type (getting confused?).
		// Terminate the string, and make type point to
		// subType.
		char *tmp = type;
		while (*--tmp == ' ')
		    ;
		*++tmp = '\0';
		type = subType;
	    }
	}

	// Get the real navaid name.  At the moment, 'name' points to
	// the start of the name, but the end also includes the type.
	// So, starting where the type pointer points, we back up past
	// any whitespace, then set the following character to null.
	char *tmp = type;
	while (*--tmp == ' ')
	    ;
	*++tmp = '\0';

	Navaid *n = NULL;
	switch (lineCode) {
	  case ndb:
	    {
		if ((strcmp(type, "NDB") != 0) &&
		    (strcmp(type, "NDB-DME") != 0) &&
		    (strcmp(type, "LOM") != 0)) {
		    fprintf(stderr, "%d: unknown NDB type: '%s'\n",
			    lineNumber, type);
		}
		n = new NDB(id, lat, lon, elev, name, freq, range);

		if (strcmp(type, "NDB-DME") == 0) {
		    // This NDB is paired with a DME.  Record the fact -
		    // we'll do the pairing when we read DMEs.  Note that
		    // this code assumes that we read all NDBs (and VORs)
		    // before we read DMEs
		    navaidSet.insert(n);
		}
	    }
	    break; 

	  case vor:
	    {
		if ((strcmp(type, "VOR") != 0) &&
		    (strcmp(type, "VOR-DME") != 0) &&
		    (strcmp(type, "VORTAC") != 0)) {
		    fprintf(stderr, "%d: unknown VOR type: '%s'\n",
			    lineNumber, type);
		}
		n = new VOR(id, lat, lon, elev, name, freq, range, 
			    magvar);

                if ((strcmp(type, "VOR-DME") == 0) ||
		    (strcmp(type, "VORTAC") == 0)) {
		    // VOR is paired with a DME or TACAN.  We'll pair
		    // it later when loading DMEs.
		    navaidSet.insert(n);
		}
	    }
	    break; 

	  case ils:
	  case loc: 
	    {
		// EYE - use a map (from string to Navaid::Type) to do
		// this?
		ILS::Type t;
		if (strcmp(type, "ILS-cat-I") == 0) {
		    t = ILS::ILS_CAT_I;
		} else if (strcmp(type, "ILS-cat-II") == 0) {
		    t = ILS::ILS_CAT_II;
		} else if (strcmp(type, "ILS-cat-III") == 0) {
		    t = ILS::ILS_CAT_III;
		} else if (strcmp(type, "LOC-GS") == 0) {
		    t = ILS::LOC_GS;
		} else if ((strcmp(type, "LDA-GS") == 0) ||
			   (strcmp(type, "LDA") == 0)) {
		    t = ILS::LDA;
		} else if (strcmp(type, "IGS") == 0) {
		    t = ILS::IGS;
		} else if (strcmp(type, "LOC") == 0) {
		    t = ILS::Localizer;
		} else if (strcmp(type, "SDF") == 0) {
		    t = ILS::SDF;
		} else {
		    fprintf(stderr, "%d: unknown LOC type: '%s'\n",
			    lineNumber, type);
		    // EYE - we should standardize our use of error
		    // handling - it's pretty ad hoc at the moment.
		    throw runtime_error("unknown LOC type");
		}

		LOC *loc = new LOC(id, lat, lon, elev, name, freq, range, 
				   magvar);
		n = loc;

		// Create an ILS consisting of the localizer.
		// Creating it adds it to the NavaidSystem class set.
		ILS *i = new ILS(loc, t);

		// EYE - this is where an internal map (within
		// NavaidSystem?) might help (and with creating
		// VORTACs, VOR/DMEs, and NDB/DMEs).

		// Put the ILS in a map under its name.  We use this
		// map to match it with other components of the ILS
		// Its name might not be unique - however, using the
		// name allows us to match markers.  Note that we
		// assume that we read all the localizers first,
		// before any glideslopes, DMEs, or markers.
		ILSMap.insert(make_pair(i->name(), i));
	    }
	    break; 

	  case gs:
	    {
		if (strcmp(type, "GS") != 0) {
		    fprintf(stderr, "%d: unknown GS type: '%s'\n", 
			    lineNumber, type);
		}

		// In the input file, the field represented here by
		// 'magvar' contains both the heading and the slope of
		// the glideslope.
		float heading, slope;
		heading = fmod(magvar, 1000.0);
		slope = (magvar - heading) / 100000.0;

		GS *gs = new GS(id, lat, lon, elev, name, freq, range, 
				heading, slope);
		n = gs;

		// A glideslope must match a localizer.
		ILS *i = __matches(ILSMap, gs);

		if (i != NULL) {
		    i->setGS(gs);
		} else {
		    // EYE - log this!
		    // fprintf(stderr,
		    // 	    "%d: no localizer found for GS %s (%.2f MHz)\n",
		    // 	    lineNumber, gs->name().c_str(), 
		    // 	    gs->frequency() / 1e6);
		}
	    }
	    break;  

	  case om:
	  case mm:
	  case im:
	    {
		// EYE - there are duplicate marker entries, for markers
		// that serve multiple runways.  We should really only
		// have one marker object, and somehow indicate (maybe)
		// that it is shared.  Its alignment should be an average
		// of the 2 runways.
		Marker::Type t;
		if (lineCode == om) {
		    t = Marker::OUTER;
		} else if (lineCode == mm) {
		    t = Marker::MIDDLE;
		} else {
		    t = Marker::INNER;
		}
		Marker *m = new Marker(lat, lon, elev, name, magvar, t);
		n = m;

		// A marker must be part of an ILS system.  We assume
		// that the ILS map has been primed by this point.
		ILS *i = __matches(ILSMap, m);
		if (i != NULL) {
		    i->addMarker(m);
		} else {
		    // EYE - log this!
		    // fprintf(stderr, "%d: no localizer found for marker %s\n", 
		    // 	    lineNumber, name);
		}
	    }
	    break;  

	  case dme_sub:
	  case dme:
	    {
		// What is called 'magvar' in the input file is, for a
		// DME, a bias in nautical miles.  We represent the
		// bias in metres, so convert it here.
		float bias = magvar * SG_NM_TO_METER;

		// The navaids database lumps TACANs (and VORTACs) in
		// with DMEs, but we distinguish them.
		if ((strcmp(type, "TACAN") == 0) ||
		    (strcmp(type, "VORTAC") == 0)) {
		    TACAN *tacan = new TACAN(id, lat, lon, elev, name, freq, 
					     range, 0.0, bias);
		    n = tacan;

		    if (strcmp(type, "VORTAC") == 0) {
			// A VORTAC is a TACAN paired with a VOR.
			// There should be an matching VOR in the
			// navaid set.  If there isn't, complain.
			set<Navaid *, __NavaidLessThan>::iterator other = 
			    navaidSet.find(tacan);
			if (other == navaidSet.end()) {
			    // No match.
			    // EYE - log this!
			    // cerr << lineNumber << ": no match for " << *tacan 
			    // 	 << endl;
			} else {
			    // Found a match.  Make sure it has the correct
			    // type.
			    VOR *vor = dynamic_cast<VOR *>(*other);
			    // EYE - don't use asserts
			    assert(vor);

			    // Create a navaid system.  It looks odd
			    // to not save the created object, but the
			    // NavaidSystem class keeps track of all
			    // created objects.
			    new VORTAC(vor, tacan);

			    // Remove the matching navaid.
			    navaidSet.erase(other);
			}
		    }
		} else {
		    // A real DME.
		    if ((strcmp(type, "DME") != 0) &&
			(strcmp(type, "NDB-DME") != 0) &&
			(strcmp(type, "DME-ILS") != 0) &&
			(strcmp(type, "VOR-DME") != 0)) {
			fprintf(stderr, "%d: unknown DME type: '%s'\n",
				lineNumber, type);
		    }
		    DME *dme = new DME(id, lat, lon, elev, name, freq, range, 
				       bias);
		    n = dme;

		    if (strcmp(type, "DME-ILS") == 0) {
			// A DME-ILS must match a localizer.
			ILS *i = __matches(ILSMap, dme);
			if (i != NULL) {
			    i->setDME(dme);
			} else {
			    // EYE - log this!
			    // fprintf(stderr, 
			    // 	    "%d: no localizer found for DME-ILS %s '%s' (%.2f MHz)\n",
			    // 	    lineNumber, dme->id().c_str(), 
			    // 	    dme->name().c_str(), 
			    // 	    dme->frequency() / 1e6);
			}
		    } else if ((strcmp(type, "NDB-DME") == 0) ||
			       (strcmp(type, "VOR-DME") == 0)) {
			// DME pair.  This DME is paired with an NDB
			// or VOR.  There should be an entry in the
			// navaid set matching this navaid.  If there
			// isn't, complain.
			set<Navaid *, __NavaidLessThan>::iterator other = 
			    navaidSet.find(dme);
			if (other == navaidSet.end()) {
			    // No match.
			    // EYE - log this!
			    // cerr << lineNumber << ": no match for " << *dme 
			    // 	 << endl;
			} else {
			    // Found a match.  Make sure it has the correct
			    // type.
			    if (strcmp(type, "NDB-DME") == 0) {
				NDB *ndb = dynamic_cast<NDB *>(*other);
				// EYE - don't use asserts
				assert(ndb);
				// Create a navaid system.  It looks
				// odd to not save the created object,
				// but the NavaidSystem class keeps
				// track of all created objects.
				new NDB_DME(ndb, dme);
			    } else { 
				VOR *vor = dynamic_cast<VOR *>(*other);
				// EYE - don't use asserts
				assert(vor);
				// Create a navaid system.  It looks
				// odd to not save the created object,
				// but the NavaidSystem class keeps
				// track of all created objects.
				new VOR_DME(vor, dme);
			    }
			    // Remove the matching navaid.
			    navaidSet.erase(other);
			}
		    }
		}
	    }
	    break;  
	  default:
	    assert(false);
	    break;
	}
	assert(n != NULL);

	// EYE - we need a logging facility
	// if (!n->validFrequency()) {
	//     printf("%d: invalid frequency for %s '%s' ", 
	//     	   lineNumber, n->id().c_str(), n->name().c_str());
	//     if (dynamic_cast<NDB *>(n)) {
	//     	printf("(%.1f kHz)\n", n->frequency() / 1000.0);
	//     } else {
	//     	printf("(%.2f MHz)\n", n->frequency() / 1000000.0);
	//     }
	// }

	// Add to our culler.
	_frustumCullers[NAVAIDS]->culler().addObject(n);

	// Create search tokens for it.
	_searcher->add(n);
    }

    // Check for orphaned paired navaids.
    // EYE - log this!
    // if (!navaidSet.empty()) {
    // 	printf("\nNo siblings found for the following paired navaids:\n");
    // 	set<Navaid *, __NavaidLessThan>::iterator i;
    // 	for (i = navaidSet.begin(); i != navaidSet.end(); i++) {
    // 	    cout << "\t" << *(*i) << "\n";
    // 	}
    // }
}

void NavData::_loadFixes(const char *fgRoot)
{
    SGPath f(fgRoot);
    f.append("Navaids/fix.dat.gz");

    gzFile arp;
    char *line;

    printf("Loading fixes from\n  %s\n", f.c_str());
    arp = gzopen(f.c_str(), "rb");
    if (arp == NULL) {
	fprintf(stderr, "_loadFixes: Couldn't open \"%s\".\n", f.c_str());
	throw runtime_error("couldn't open fixes file");
    } 

    // Check the file version.  We can handle version 600 files.
    int version = -1;
    gzGetLine(arp, &line);	// Windows/Mac header
    gzGetLine(arp, &line);	// Version
    sscanf(line, "%d", &version);
    if (version == 600) {
	// It looks like we have a valid file.
	_loadFixes600(arp);
    } else {
	fprintf(stderr, "_loadFixes: \"%s\": unknown version %d.\n", 
		f.c_str(), version);
	throw runtime_error("unknown fixes file version");
    }

    gzclose(arp);
    printf("  ... done\n");
}

void NavData::_loadFixes600(const gzFile& arp)
{
    char *line;
    while (gzGetLine(arp, &line)) {
	if (strcmp(line, "") == 0) {
	    // Blank line.
	    continue;
	} 

	if (strcmp(line, "99") == 0) {
	    // Last line.
	    break;
	}

	// A line looks like this:
	//
	// <lat> <lon> <name>
	//
	double lat, lon;
	int n;
	char *id;
	assert(sscanf(line, "%lf %lf %n", &lat, &lon, &n) == 2);
	id = line + n;

	// Create a record and fill it in.
	Fix *f = new Fix(id, lat, lon);

	// Add to our culler.
	_frustumCullers[FIXES]->culler().addObject(f);

	// Create search tokens for it.
	_searcher->add(f);

	// // Add to the _navPoints map.
	// _NAVPOINT foo;
	// foo.isNavaid = false;
	// foo.n = (void *)f;
	// _navPoints.insert(make_pair(f->name, foo));
    }
}

void NavData::_loadAirways(const char *fgRoot)
{
    SGPath f(fgRoot);
    f.append("Navaids/awy.dat.gz");

    gzFile arp;
    char *line;

    printf("Loading airways from\n  %s\n", f.c_str());
    arp = gzopen(f.c_str(), "rb");
    if (arp == NULL) {
	fprintf(stderr, "_loadAirways: Couldn't open \"%s\".\n", f.c_str());
	throw runtime_error("couldn't open airways file");
    } 

    // Check the file version.  We can handle version 640 files.
    int version = -1;
    gzGetLine(arp, &line);	// Windows/Mac header
    gzGetLine(arp, &line);	// Version
    sscanf(line, "%d", &version);
    if (version == 640) {
	// It looks like we have a valid file.
	_loadAirways640(arp);
    } else {
	fprintf(stderr, "_loadAirways: \"%s\": unknown version %d.\n", 
		f.c_str(), version);
	throw runtime_error("unknown airways file version");
    }

    gzclose(arp);
    printf("  ... done\n");
}

// This is a temporary type used to construct airways.  It represents
// a segment, but only for one airway, and only in one direction.  For
// example, consider the low-level segment between ARNEB and KIRAS,
// shared by airways P161, UW411, and W411.  We will create 6
// sub-segments: ARNEB:P161, KIRAS:P161, ARNEB:UW411, KIRAS:UW411,
// ARNEB:W411, and KIRAS:W411.
struct Subsegment {
  public:
    Subsegment(const Waypoint *start, const char *airwayName, bool isLow);
    ~Subsegment();

    const Waypoint *start() const { return _start; }
    const string& name() const { return _name; }
    bool isLow() { return _isLow; }
    Segment *segment() const { return _seg; }
    const set<Subsegment>::const_iterator& end() const { return _end; }

    void setStart(const Waypoint *wpt) { _start = wpt; }
    void setName(const string& name) { _name = name; }
    void setSegment(Segment* segment) { _seg = segment; }
    void setEnd(set<Subsegment>::iterator& i) { _end = i; }

    bool operator<(const Subsegment& right) const;

    friend std::ostream& operator<<(std::ostream& os, const Subsegment& seg);
    
  protected:
    const Waypoint *_start;		// eg, ARNEB
    string _name;			// eg, "P161"
    bool _isLow;
    Segment *_seg;
    set<Subsegment>::iterator _end; // eg, KIRAS:P161
};

Subsegment::Subsegment(const Waypoint *start, const char *airwayName, 
		       bool isLow): 
    _start(start), _name(airwayName), _isLow(isLow)
{
}

Subsegment::~Subsegment()
{
}

bool Subsegment::operator<(const Subsegment& right) const
{
    if (_start != right._start) {
	return (_start < right._start);
    }
    if (_name != right._name) {
	return (_name < right._name);
    }

    return (_isLow < right._isLow);
}

// EYE - since we have asString, do we want this?
ostream& operator<<(ostream& os, const Subsegment& seg)
{
    os << seg.name() << ": " << seg.start()->id() << " <" 
       << seg.start()->lat() << ", " << seg.start()->lon() 
       << ">";

    return os;
}

void NavData::_loadAirways640(const gzFile& arp)
{
    // As we read segments, we create and add subsegments to this map.
    // It will be used to construct our airways.
    multiset<Subsegment> subsegments;

    char *line;
    while (gzGetLine(arp, &line)) {
	if (strcmp(line, "") == 0) {
	    // Blank line.
	    continue;
	} 

	if (strcmp(line, "99") == 0) {
	    // Last line.
	    break;
	}

	// A line looks like this:
	//
	// <id> <lat> <lon> <id> <lat> <lon> <high/low> <base> <top> <name>
	//
	// 
	char startID[6], endID[6];
	double startLat, startLon, endLat, endLon;
	int lowHigh, base, top, nameOffset;

	sscanf(line, "%s %lf %lf %s %lf %lf %d %d %d %n", 
	       startID, &startLat, &startLon, endID, &endLat, &endLon,
	       &lowHigh, &base, &top, &nameOffset);
	assert((lowHigh == 1) || (lowHigh == 2));
	// Check that the first id is alphabetically less than the
	// second.  We use this assumption in other parts of the code.
	assert(strcmp(startID, endID) < 0);

	Waypoint *start, *end;
	start = _findEnd(startID, startLat, startLon);
	end = _findEnd(endID, endLat, endLon);

	// Create the segment.  It is automatically added to the
	// Segment class's vector.
	char *name = line + nameOffset;
	Segment *seg = new Segment(name, start, end, base, top, lowHigh == 1);

	// We use the airways to help us guess what our fixes are used
	// for.  If a fix appears in an airway, we tag it as en route
	// (otherwise it is considered a terminal fix).
	Fix *f;
	if ((f = dynamic_cast<Fix *>(start))) {
	    f->setEnRoute();
	}
	if ((f = dynamic_cast<Fix *>(end))) {
	    f->setEnRoute();
	}

	// Add the airway segment to the culler.
	_frustumCullers[AIRWAYS]->culler().addObject(seg);

	// Add to subsegments set.  We add the subsegment twice per
	// airway name.  Note that a segment may be shared among
	// several airways - this is indicated by a hyphenated name
	// (eg, "A240-B143B-W16")
	char *awy;
	for (awy = strtok(name, "-"); awy; awy = strtok(NULL, "-")) {
	    // Each segment is added twice, once per endpoint.
	    Subsegment s1(start, awy, seg->isLow()), s2(end, awy, seg->isLow());
	    s1.setSegment(seg);
	    s2.setSegment(seg);

	    // EYE - check for duplicates?
	    multiset<Subsegment>::iterator i1 = subsegments.insert(s1);
	    s2.setEnd(i1);
	    multiset<Subsegment>::iterator i2 = subsegments.insert(s2);
	    // Sometimes C++ can be such a pain - I want to set the
	    // neighbour element of the subsegment we first inserted.
	    // But C++ won't let us do that, because it figures (I
	    // guess) that changing the object might change the sort
	    // order of the multiset (the compiler says '*i1' is a
	    // 'const subsegment&').  I know that setting the
	    // neighbour won't change the ordering, so I'm ramming
	    // this one through with a cast.
	    Subsegment& n = (Subsegment&)(*i1);
	    n.setEnd(i2);

// 	    cout << s1 << endl << s2 << endl;
// 	    assert(subsegments.find(s1) == i1);
// 	    assert(subsegments.find(s2) == i2);
// 	    assert((*i1).end() == i2);
// 	    assert((*i2).end() == i1);
	}
    }

    // Now construct our airways.
    while (subsegments.size() > 0) {
	// Grab a point and build an airway from it.
	multiset<Subsegment>::iterator s1 = subsegments.begin();
	multiset<Subsegment>::iterator s2 = (*s1).end();

	// Create the airway.  It will be added to the __airways
	// vector.
	Airway *awy = 
	    new Airway((*s1).name(), (*s1).segment()->isLow(), (*s1).segment());
	subsegments.erase(s1);
	subsegments.erase(s2);

	// Add to the front ...
	Subsegment s(awy->nthWaypoint(0), awy->name().c_str(), awy->isLow());
	while ((s1 = subsegments.find(s)) != subsegments.end()) {
	    s2 = (*s1).end();
	    awy->prepend((*s1).segment());

	    subsegments.erase(s1);
	    subsegments.erase(s2);

	    s.setStart(awy->nthWaypoint(0));
	}

	// And add to the back ...
	s.setStart(awy->nthWaypoint(awy->noOfWaypoints() - 1));
	while ((s1 = subsegments.find(s)) != subsegments.end()) {
	    s2 = (*s1).end();
	    awy->append((*s1).segment());

	    subsegments.erase(s1);
	    subsegments.erase(s2);

	    s.setStart(awy->nthWaypoint(awy->noOfWaypoints() - 1));
	}

	// Create search tokens for it.
	_searcher->add(awy);
    }

    // const set<Airway *>& airways = Airway::airways();
    // set<Airway *>::iterator it;
    // Airway *shortest;
    // size_t noOfWaypoints = 1000000;
    // for (it = airways.begin(); it != airways.end(); it++) {
    // 	Airway *awy = *it;
    // 	if (awy->noOfWaypoints() < noOfWaypoints) {
    // 	    noOfWaypoints = awy->noOfWaypoints();
    // 	    shortest = awy;
    // 	}
    // }
    // printf("Shortest airway: %s (%lu)\n", 
    // 	   shortest->name().c_str(), noOfWaypoints);
    // exit(0);

    // const set<Airway *>& airways = Airway::airways();
    // set<Airway *>::iterator it;
    // for (it = airways.begin(); it != airways.end(); it++) {
    // 	Airway *awy = *it;
    // 	printf("%s: %lu points\n", awy->name().c_str(), awy->noOfWaypoints());
    // }
    // exit(0);

    // const set<Segment *>& segments = Segment::segments();
    // set<Segment *>::iterator it;
    // double length = 0.0;
    // Segment *longest = NULL;
    // for (it = segments.begin(); it != segments.end(); it++) {
    // 	Segment *seg = *it;
    // 	double d = sgdDistanceVec3(seg->start()->location(),
    // 				   seg->end()->location());
    // 	if (d > length) {
    // 	    length = d;
    // 	    longest = seg;
    // 	}
    // }
    // printf("%s (%s.%s): %.0f metres\n", 
    // 	   longest->name().c_str(), 
    // 	   longest->start()->id().c_str(),
    // 	   longest->end()->id().c_str(),
    // 	   length);
    // exit(0);
}

// Each airway segment has two endpoints, which should be fixes and/or
// navaids.  If an endpoint is a fix, we use the airway type as a
// heuristic to decide whether that fix is a high or low fix.  Note
// that the navaid, fix, and airways databases are not perfect, so we
// need to handle cases where no or partial matches are made.
Waypoint *NavData::_findEnd(const string& id, double lat, double lon)
{
    // EYE - clear as mud!
    multimap<string, Waypoint *>::const_iterator it;
    pair<multimap<string, Waypoint *>::const_iterator, 
	multimap<string, Waypoint *>::const_iterator> ret;
    
    // Find the closest navaid with the given name.
    const multimap<string, Waypoint *>& waypoints = Waypoint::waypoints();
    ret = waypoints.equal_range(id);
    for (it = ret.first; it != ret.second; it++) {
	Waypoint *p = (*it).second;

	if ((p->lat() == lat) && (p->lon() == lon)) {
	    // We've found an exact match, so bail out early.
	    return p;
	}
    }

    // EYE - we need some kind of logging facility.  We should complain if:
    //
    // (a) There is no match at any distance
    // (b) There is a match, but far away
    // (c) There is a close, but not exact, match
    //
    // Of course, we need to define what "near" is.
    
    // Since we didn't get an exact match, we need to do something.
    // But what?  For the lack of a better alternative, we create a
    // new fix with the same name, at the location where we expected
    // to find it.  This will probably result in lots of fixes with
    // identical names close to each other.  So sue me.
    Fix *fix = new Fix(id.c_str(), lat, lon);
    _frustumCullers[FIXES]->culler().addObject(fix);
    _searcher->add(fix);
    return fix;
}

void NavData::_loadAirports(const char *fgRoot)
{
    SGPath f(fgRoot);
    f.append("Airports/apt.dat.gz");

    gzFile arp;
    char *line;

    printf("Loading airports from\n  %s\n", f.c_str());
    arp = gzopen(f.c_str(), "rb");
    if (arp == NULL) {
	fprintf(stderr, "AirportsOverlay::load: Couldn't open \"%s\".\n", 
		f.c_str());
	throw runtime_error("couldn't open airports file");
    } 

    // Check the file version.  We can handle version 810 files.
    int version = -1;
    gzGetLine(arp, &line);	// Windows/Mac header
    gzGetLine(arp, &line);	// Version
    sscanf(line, "%d", &version);
    // EYE - In 810 airports, we use 85% of the data loaded, while in
    // 1000 airports, we only use 7%.  This seems like another
    // argument for caching..
    if (version == 810) {
	_loadAirports810(arp);
    } else if (version == 1000) {
	_loadAirports1000(arp);
    } else {
	fprintf(stderr, "AirportsOverlay::load: \"%s\": unknown version %d.\n", 
		f.c_str(), version);
	throw runtime_error("unknown airports file version");
    }

    gzclose(arp);
    printf("  ... done\n");
}

void NavData::_loadAirports810(const gzFile& arp)
{
    char *line;
    ARP *ap = NULL;

    while (gzGetLine(arp, &line)) {
	int lineCode, offset;

	if (strcmp(line, "") == 0) {
	    // Blank line.
	    continue;
	} 

	if (strcmp(line, "99") == 0) {
	    // Last line.
	    break;
	}

	sscanf(line, "%d%n", &lineCode, &offset);
	line += offset;
	switch (lineCode) {
	  case 1:
	  case 16:
	  case 17:
	    {
		// The presence of a 1/16/17 means that we're starting a
		// new airport/seaport/heliport, and therefore ending an
		// old one.  Deal with the old airport first.
		if (ap != NULL) {
		    // Add our airport text to the searcher object.
		    _searcher->add(ap);
		    // Add to our culler.
		    _frustumCullers[AIRPORTS]->culler().addObject(ap);

		    ap = NULL;
		}

		// EYE - add seaports and heliports!  (Note: the
		// classification of seaports is iffy - Pearl Harbor
		// is called an airport, even though it's in the
		// ocean, and Courchevel is called a seaport, even
		// though it's on top of a mountain).
		if (lineCode != 1) {
		    // We only handle airports (16 = seaport, 17 = heliport)
		    break;
		}

		float elevation;
		int controlled;
		char code[100];

		sscanf(line, "%f %d %*d %99s %n", 
		       &elevation, &controlled, code, &offset);
		line += offset;
		assert(strlen(code) <= 4);

		// Create a new airport record and add it to our
		// airports vector.
		ap = new ARP(line, code, elevation * SG_FEET_TO_METER);
		_airports.push_back(ap);

		ap->setControlled(controlled == 1);
	    }

	    break;
	  case 10:
	    {
		if (ap == NULL) {
		    // If we're not working on an airport (ie, if this is
		    // a heliport), just continue.
		    break;
		}

		double lat, lon;
		char rwyid[4];	// EYE - safe?

		sscanf(line, "%lf %lf %s %n", &lat, &lon, rwyid, &offset);
		line += offset;

		// We ignore taxiways completely.
		if (strcmp(rwyid, "xxx") == 0) {
		    break;
		}

		// Strip off trailing x's.
		int firstX = strcspn(rwyid, "x");
		if (firstX > 0) {
		    rwyid[firstX] = '\0';
		}
		assert(strlen(rwyid) <= 3);

		float heading, length, width;
		char *lighting;

		sscanf(line, "%f %f %*f %*f %f %n", 
		       &heading, &length, &width, &offset);
		lighting = line + offset;

		// Runway!
		RWY *rwy = new RWY(rwyid, lat, lon, heading, 
				   length * SG_FEET_TO_METER, 
				   width * SG_FEET_TO_METER, ap);

		// Atlas doesn't display helipads.  However, there is
		// at least one airport with only helipads - MO06,
		// "Lamar Barton Co Mem Hospital".  In this case we
		// have to use the helipad to establish the airport
		// bounds.  Once we've done that, though, we can throw
		// it away.
		if (strncmp(rwyid, "H", 1) == 0) {
		    // It's a helipad, so just delete it without
		    // adding it to the airport's runway vector.
		    delete rwy;
		} else {
		    ap->addRwy(rwy);
		}

		// According to the FAA's "VFR Aeronautical Chart
		// Symbols", lighting codes on VFR maps refer to
		// runway lights (not approach lights).
		//
		// In apt.dat, visual approach, runway, and approach
		// lighting is given by a six-digit "number" (which we
		// treat as a string).  We're concerned with digits 2
		// and 5, which concern the runway itself.  If the
		// value is '1', there is no runway lighting.
		//
		// Note that the apt.dat database does not tell us
		// about lighting limitations, nor whether the
		// lighting is pilot-controlled.
		if ((lighting[1] != '1') || (lighting[4] != '1')) {
		    ap->setLighting(true);
		}
	    }

	    break;
	  case 18: 
	    if (ap != NULL) {
		// Beacon
		double lat, lon;
		int beaconType;

		sscanf(line, "%lf %lf %d", &lat, &lon, &beaconType);
		if (beaconType != 0) {
		    ap->setBeaconLoc(lat, lon);
		}
	    }
	    break;
	  case WEATHER:		// AWOS, ASOS, ATIS
	  case UNICOM:		// Unicom/CTAF (US), radio (UK)
	  case DEL:		// Clearance delivery
	  case GND:		// Ground
	  case TWR:		// Tower
	  case APP:		// Approach
	  case DEP:		// Departure
	      {
		  // ATC frequencies.
		  //
		  // Here's a sample, from LFPG (Paris Charles De
		  // Gaulle), which is a rather extreme case:
		  //
		  // 50 12712 DE GAULLE ATIS
		  // 53 11810 DE GAULLE TRAFFIC
		  // 53 11955 DE GAULLE TRAFFIC
		  // 53 12160 DE GAULLE GND
		  // 53 12167 DE GAULLE TRAFFIC
		  // 53 12177 DE GAULLE GND
		  // 53 12177 DE GAULLE GND
		  // 53 12180 DE GAULLE GND
		  // 53 12192 DE GAULLE TRAFFIC
		  // 53 12192 DE GAULLE TRAFFIC
		  // 53 12197 DE GAULLE GND
		  // 53 12197 DE GAULLE GND
		  // 54 11865 DE GAULLE TWR
		  // 54 11925 DE GAULLE TWR
		  // 54 12090 DE GAULLE TWR
		  // 54 12360 DE GAULLE TWR
		  // 54 12532 DE GAULLE TWR
		  //
		  // [...]
		  //
		  // There are several important things to note:
		  //
		  // (1) There many be several entries for a given
		  //     type.  For example, there is only one WEATHER
		  //     entry (type code 50), but 10 GND entries
		  //     (type code 53).
		  //
		  // (2) There may be several frequencies with the
		  //     same name in a given type.  For example,
		  //     there are 4 GND entries labelled "DE GAULLE
		  //     TRAFFIC", and 6 labelled "DE GAULLE GND".
		  //     They are not guaranteed to be grouped
		  //     together.  
		  //
		  //     When rendering these, we only print the label
		  //     once, and all frequencies with that label are
		  //     printed after the label.  This makes for a
		  //     less cluttered display:
		  //
		  //     DE GAULLE TRAFFIC 118.1 119.55 121.675 121.925
		  //
		  // (3) There may be duplicates.  For example, '53
		  //     12192 DE GAULLE TRAFFIC' is given twice.  The
		  //     duplicates should presumably be ignored.
		  //
		  // (4) Frequencies are given as integers, and should
		  //     be divided by 100.0 to give the true
		  //     frequency in MHz.  That is, 11810 is 118.1
		  //     MHz.  In addition, they are missing a
		  //     significant digit: 12192 really means 121.925
		  //     MHz, not 121.92 MHz (communications
		  //     frequencies have a 25 kHz spacing).  So, we
		  //     need to correct frequencies with end in the
		  //     digits '2' and '7'.
		  //
		  //     Internally, we also store the frequencies as
		  //     integers, but multiplied by 1000.0, not
		  //     100.0.  And we add a final '5' when
		  //     necessary.  So, we store 12192 as 121925, and
		  //     11810 as 118100.

	          // EYE - what should I do about multiple frequencies
	          // of one type?  A: Check San Jose (KSJC) - it has 2
	          // CT frequencies, and just lists them.  However,
	          // the VFR_Chart_Symbols.pdf file says that it lists
	          // the "primary frequency."

	          // Note: Unicom frequencies are written in bold
	          // italics, others in bold.  CT seems to be written
	          // slightly larger than the others.

	          // Note: Some airports, like Reid-Hillview, have
	          // CTAF and UNICOM.  CTAF is written with a circled
	          // C in front, the frequency bold and slightly
	          // enlarged (like CT), UNICOM in bold italics.

		  if (ap != NULL) {
		      int freq;

		      sscanf(line, "%d %n", &freq, &offset);
		      line += offset;

		      ap->addFreq((ATCCodeType)lineCode, freq, line);
		  }
	      }
	    break;
	}
    }

    if (ap != NULL) {
	// Add our airport text to the searcher object.
	_searcher->add(ap);
	// Add to our culler.
	_frustumCullers[AIRPORTS]->culler().addObject(ap);
    }
}

// EYE - combine with _loadAirports810 so there's less duplicate code?

// EYE - I think it would be a good idea to do some data verification.
// Items we might want to check and report on:
//
// - valid runway ids
// - in-range latitudes and longitudes
// - runway ids that don't correspond (eg, the other end of runway 05
//   should be 23, although there are a few valid exceptions to this
//   rule).
void NavData::_loadAirports1000(const gzFile& arp)
{
    char *line;
    ARP *ap = NULL;

    while (gzGetLine(arp, &line)) {
	int lineCode, offset;

	if (strcmp(line, "") == 0) {
	    // Blank line.
	    continue;
	} 

	if (strcmp(line, "99") == 0) {
	    // Last line.
	    break;
	}

	sscanf(line, "%d%n", &lineCode, &offset);
	line += offset;
	switch (lineCode) {
	  case 1:
	  case 16:
	  case 17:
	    {
		// The presence of a 1/16/17 means that we're starting a
		// new airport/seaport/heliport, and therefore ending an
		// old one.  Deal with the old airport first.
		if (ap != NULL) {
		    // Add our airport text to the searcher object.
		    _searcher->add(ap);
		    // Add to our culler.
		    _frustumCullers[AIRPORTS]->culler().addObject(ap);

		    ap = NULL;
		}

		// EYE - add seaports and heliports!  (Note: the
		// classification of seaports is iffy - Pearl Harbor
		// is called an airport, even though it's in the
		// ocean, and Courchevel is called a seaport, even
		// though it's on top of a mountain).
		if (lineCode != 1) {
		    // We only handle airports (16 = seaport, 17 = heliport)
		    break;
		}

		float elevation;
		char code[100];

		sscanf(line, "%f %*d %*d %99s %n", &elevation, code, &offset);
		line += offset;

		// Create a new airport record and add it to our
		// airports vector.
		ap = new ARP(line, code, elevation * SG_FEET_TO_METER);
		_airports.push_back(ap);
	    }

	    break;
	  case 14:		// Is controlled (sort of)
	    // Line code 14 actually defines a viewpoint (of which an
	    // airport can only have 1).  Although the specification
	    // doesn't actually say it, I'm taking this to mean that
	    // it's a tower, and therefore that the airport is
	    // controlled.  Perhaps this is a bit of a stretch.
	    if (ap) {
		// EYE - record (and indicate) position too?
		ap->setControlled(true);
	    }
	    break;
	  case 18: 
	    if (ap != NULL) {
		// Beacon
		double lat, lon;
		int beaconType;

		sscanf(line, "%lf %lf %d", &lat, &lon, &beaconType);
		if (beaconType != 0) {
		    ap->setBeaconLoc(lat, lon);
		}
	    }
	    break;
	  case WEATHER:		// AWOS, ASOS, ATIS
	  case UNICOM:		// Unicom/CTAF (US), radio (UK)
	  case DEL:		// Clearance delivery
	  case GND:		// Ground
	  case TWR:		// Tower
	  case APP:		// Approach
	  case DEP:		// Departure
	      {
		  // See _loadAirports810 for extensive documentation
		  // of how we deal with ATC frequences, and issues
		  // that haven't been resolved.
		  if (ap != NULL) {
		      int freq;

		      sscanf(line, "%d %n", &freq, &offset);
		      line += offset;

		      ap->addFreq((ATCCodeType)lineCode, freq, line);
		  }
	      }
	    break;
	  case 100: // Land runway - EYE - add water runways (101) and
		    // helipads (102)
	    {
		if (ap == NULL) {
		    // If we're not working on an airport (ie, if this
		    // is a seaport or heliport), just continue.  Note
		    // that airports, seaports and helipads all have
		    // the potential to have runways, water runways,
		    // and helipads.
		    break;
		}

		// First, deal with the data common to both ends of
		// the runway.
		float width;
		int centre, edge; // Runway lighting
		sscanf(line, "%f %*d %*d %*f %d %d %*d %n", 
		       &width, &centre, &edge, &offset);
		line += offset;

		// According to the FAA's "VFR Aeronautical Chart
		// Symbols", lighting codes on VFR maps refer to
		// runway lights (not approach lights).
		//
		// In apt.dat, we say a runway is lit if it has
		// centre-line lines or edge lighting (although it may
		// be that if it has edge lighting, it automatically
		// has centre-line lighting, but I haven't checked).
		//
		// Note that the apt.dat database does not tell us
		// about lighting limitations, nor whether the
		// lighting is pilot-controlled.
		if ((centre != 0) || (edge != 0)) {
		    ap->setLighting(true);
		}

		// Now deal with each end.  At the moment, we only
		// care about the position and id of the ends.
		double lat1, lon1, lat2, lon2;
		char rwyid1[4], rwyid2[4]; // EYE - safe?

		// EYE - get (and indicate) displaced thresholds?
		// Stopways/overrun/blast pads?  See
		// faa-h-8083-15-2.pdf (pg 26), or 7th_IAP_Symbols.pdf
		// (pg 7) for more info.
		//
		// EYE - show surface (hard vs "other than hard")?
		sscanf(line, "%3s %lf %lf %*f %*f %*d %*d %*d %*d %n", 
		       rwyid1, &lat1, &lon1, &offset);
		line += offset;
		assert(strlen(rwyid1) <= 3);

		// EYE - there's an error in v1000 or apt.dat.gz -
		// NZSP (SOUTH POLE STATION) has a runway with a
		// latitude of -90.000357, which is impossible.  More
		// importantly, it screws up our calculations - we end
		// up with a runway that's 65,622,343 feet long
		// (10,800 nautical miles) long!  This is clearly
		// unsatisfactory, so we just clamp all values less
		// than -90 to -90.
#ifdef _MSC_VER
		lat1 = (lat1 > -90.0) ? lat1 : -90.0;
#else // !_MSC_VER       
		lat1 = max(lat1, -90.0);
#endif // _MSC_VER y/n
		sscanf(line, "%3s %lf %lf %*f %*f %*d %*d %*d %*d %n", 
		       rwyid2, &lat2, &lon2, &offset);
		line += offset;
		assert(strlen(rwyid2) <= 3);
#ifdef _MSC_VER
		lat2 = (lat2 > -90.0) ? lat2 : -90.0;
#else // !_MSC_VER       
		lat2 = max(lat2, -90.0); // EYE - hack (see above)
#endif // _MSC_VER y/n
		// Runway!
		RWY *rwy = new RWY(rwyid1, lat1, lon1, rwyid2, lat2, lon2, 
				   width, ap);

		// EYE - check if it's a helipad or not (see 810 code)?
		ap->addRwy(rwy);
	    }

	    break;
	}
    }

    if (ap != NULL) {
	// Add our airport text to the searcher object.
	_searcher->add(ap);
	// Add to our culler.
	_frustumCullers[AIRPORTS]->culler().addObject(ap);
    }
}

