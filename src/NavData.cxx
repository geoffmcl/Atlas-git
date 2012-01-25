/*-------------------------------------------------------------------------
  NavData.cxx

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

#include <stdexcept>
#include <sstream>
#include <map>

#include <simgear/misc/sg_path.hxx>

#include "NavData.hxx"
#include "Globals.hxx"

using namespace std;

//////////////////////////////////////////////////////////////////////
// Searchable interfaces.
//////////////////////////////////////////////////////////////////////
double NAV::distanceSquared(const sgdVec3 from) const
{
    return sgdDistanceSquaredVec3(bounds.center, from);
}

// Returns our tokens, generating them if they haven't been already.
const std::vector<std::string>& NAV::tokens()
{
    if (_tokens.empty()) {
	bool isNDB = (navtype == NAV_NDB);
	bool isMarker = ((navtype == NAV_OM) ||
			 (navtype == NAV_MM) ||
			 (navtype == NAV_IM));
    
	// The id, if it has one, is a token.
	if (!isMarker) {
	    _tokens.push_back(id);
	}

	// Tokenize the name.
	Searchable::tokenize(name, _tokens);

	// Add a frequency too, if it has one.
	if (!isMarker) {
	    if (isNDB) {
		globals.str.printf("%d", freq);
	    } else {
		globals.str.printf("%.2f", freq / 1000.0);
	    }
	    _tokens.push_back(globals.str.str());
	}

	// Add a navaid type token.
	switch (navtype) {
	  case NAV_VOR:
	    _tokens.push_back("VOR:");
	    break;
	  case NAV_DME:
	    _tokens.push_back("DME:");
	    break;
	  case NAV_NDB:
	    _tokens.push_back("NDB:");
	    break;
	  case NAV_ILS:
	  case NAV_GS:
	    _tokens.push_back("ILS:");
	    break;
	  case NAV_OM:
	    _tokens.push_back("MKR:");
	    _tokens.push_back("OM:");
	    break;
	  case NAV_MM:
	    _tokens.push_back("MKR:");
	    _tokens.push_back("MM:");
	    break;
	  case NAV_IM:
	    _tokens.push_back("MKR:");
	    _tokens.push_back("IM:");
	    break;
	  default:
	    assert(false);
	    break;
	}
    }

    return _tokens;
}

// Returns our pretty string, generating it if it hasn't been already.
const std::string& NAV::asString()
{
    if (_str.empty()) {
	// Initialize our pretty string.
	switch (navtype) {
	  case NAV_VOR:
	    globals.str.printf("VOR: %s %s (%.2f)", 
				id.c_str(), name.c_str(), freq / 1000.0);
	    break;
	  case NAV_DME:
	    globals.str.printf("DME: %s %s (%.2f)", 
				id.c_str(), name.c_str(), freq / 1000.0);
	    break;
	  case NAV_NDB:
	    globals.str.printf("NDB: %s %s (%d)", 
				id.c_str(), name.c_str(), freq);
	    break;
	  case NAV_ILS:
	  case NAV_GS:
	    globals.str.printf("ILS: %s %s (%.2f)", 
				id.c_str(), name.c_str(), freq / 1000.0);
	    break;
	  case NAV_OM:
	    globals.str.printf("MKR: OM: %s", name.c_str());
	    break;
	  case NAV_MM:
	    globals.str.printf("MKR: MM: %s", name.c_str());
	    break;
	  case NAV_IM:
	    globals.str.printf("MKR: IM: %s", name.c_str());
	    break;
	  default:
	    assert(false);
	    break;
	}

	_str = globals.str.str();
    }

    return _str;
}

double FIX::distanceSquared(const sgdVec3 from) const
{
    return sgdDistanceSquaredVec3(bounds.center, from);
}

// Returns our tokens, generating them if they haven't been already.
const std::vector<std::string>& FIX::tokens()
{
    if (_tokens.empty()) {
	// The name/id is a token.
	_tokens.push_back(name);

	// Add a "FIX:" token.
	_tokens.push_back("FIX:");
    }

    return _tokens;
}

// Returns our pretty string, generating it if it hasn't been already.
const std::string& FIX::asString()
{
    if (_str.empty()) {
	// Initialize our pretty string.
	globals.str.printf("FIX: %s", name);
	_str = globals.str.str();
    }

    return _str;
}

double ARP::distanceSquared(const sgdVec3 from) const
{
    return sgdDistanceSquaredVec3(bounds.center, from);
}

// Returns our tokens, generating them if they haven't been already.
const std::vector<std::string>& ARP::tokens()
{
    if (_tokens.empty()) {
	// The id is a token.
	_tokens.push_back(id);

	// Tokenize the name.
	Searchable::tokenize(name, _tokens);

	// Add an "AIR:" token.
	_tokens.push_back("AIR:");
    }

    return _tokens;
}

// Returns our pretty string, generating it if it hasn't been already.
const std::string& ARP::asString()
{
    if (_str.empty()) {
	// Initialize our pretty string.
	globals.str.printf("AIR: %s %s", id.c_str(), name.c_str());
	_str = globals.str.str();
    }

    return _str;
}

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

    // EYE - they're all very similar - can we abstract most of it
    // out?
    _loadNavaids(fgRoot);
    _loadFixes(fgRoot);
    _loadAirways(fgRoot);
    _loadAirports(fgRoot);
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
    _navPoints.clear();

    for (size_t i = 0; i < _navaids.size(); i++) {
	NAV *n = _navaids[i];
	_searcher->remove(n);
	delete n;
    }
    _navaids.clear();
    for (size_t i = 0; i < _fixes.size(); i++) {
	FIX *f = _fixes[i];
	_searcher->remove(f);
	delete f;
    }
    _fixes.clear();
    for (size_t i = 0; i < _airports.size(); i++) {
	ARP *ap = _airports[i];
	for (size_t j = 0; j < ap->rwys.size(); j++) {
	    delete ap->rwys[j];
	}
	_searcher->remove(ap);
	delete ap;
    }
    _airports.clear();
    for (size_t i = 0; i < _segments.size(); i++) {
	delete _segments[i];
    }
    _segments.clear();
}

const vector<Cullable *>& NavData::getNavaids(sgdVec3 p)
{
    static vector<Cullable *> results;

    results.clear();

    // EYE - should we do this?
    _navaidsPointCuller->move(p);
    results = _navaidsPointCuller->intersections();

    return results;
}

const vector<Cullable *>& NavData::getNavaids(FlightData *p)
{
    static vector<Cullable *> results;

    results.clear();

    if (p == NULL) {
	return results;
    }

    // We don't do anything if this is from an NMEA track.
    // Unfortunately, there's no explicit marker in a FlightData
    // structure that tells us what kind of track it is.  However,
    // NMEA tracks have their frequencies and radials set to 0, so we
    // just check for that (and in any case, if frequencies are 0, we
    // won't match any navaids anyway).
    if ((p->nav1_freq == 0) && (p->nav2_freq == 0) && (p->adf_freq == 0)) {
	return results;
    }

    const vector<Cullable *>& navaids = getNavaids(p->cart);
	
    for (unsigned int i = 0; i < navaids.size(); i++) {
	NAV *n = dynamic_cast<NAV *>(navaids[i]);
	assert(n);
	if (p->nav1_freq == n->freq) {
	    results.push_back(n);
	} else if (p->nav2_freq == n->freq) {
	    results.push_back(n);
	} else if (p->adf_freq == n->freq) {
	    results.push_back(n);
	}
    }

    return results;
}

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

// This denotes the radius of a marker's bounding sphere, in nautical
// miles.  It must be an integer, and no smaller than the maximum
// radius of a rendered marker.  We specify this because the navaid
// database doesn't give a range for markers.

// EYE - coordinate with markerRadii (in NavaidsOverlay.cxx)
const int __markerRange = 1;

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

void NavData::_loadNavaids810(float cycle, const gzFile& arp)
{
    char *line;
    NAV *n;

    while (gzGetLine(arp, &line)) {
	NavType navtype;
	NavSubType navsubtype;
	int lineCode, offset;
	double lat, lon;
	int elev, freq, range;
	float magvar;
	char id[5];

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
	if (sscanf(line, "%d %lf %lf %d %d %d %f %s %n", &lineCode, 
		   &lat, &lon, &elev, &freq, &range, &magvar, id, &offset)
	    != 8) {
	    continue;
	}
	line += offset;
	assert(lineCode != 99);

	// Find the "type", which is the last space-delimited string.
	char *subType = lastToken(line);
	assert(subType != NULL);

	// We slightly alter the representation of frequencies.  In
	// the navaid database, NDB frequencies are given in kHz,
	// whereas VOR/ILS/DME/... frequencies are given in 10s of
	// kHz.  We adjust the latter so that they are kHz as well.
	if (lineCode != 2) {
	    freq *= 10;
	}

	// EYE - is having navtype and navsubtype a good idea, or
	// should we just stick to one or the other (presumably the
	// latter would be better)?
	switch (lineCode) {
	  case 2: 
	    navtype = NAV_NDB;
	    if (strcmp(subType, "NDB") == 0) {
		navsubtype = NDB;
	    } else if (strcmp(subType, "NDB-DME") == 0) {
		navsubtype = NDB_DME;
	    } else if (strcmp(subType, "LOM") == 0) {
		navsubtype = LOM;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    break; 
	  case 3: 
	    navtype = NAV_VOR; 
	    if (strcmp(subType, "VOR") == 0) {
		navsubtype = VOR;
	    } else if (strcmp(subType, "VOR-DME") == 0) {
		navsubtype = VOR_DME;
	    } else if (strcmp(subType, "VORTAC") == 0) {
		navsubtype = VORTAC;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    break; 
	  case 4: 
	    if (strcmp(subType, "IGS") == 0) {
		navsubtype = IGS;
	    } else if (strcmp(subType, "ILS-cat-I") == 0) {
		navsubtype = ILS_cat_I;
	    } else if (strcmp(subType, "ILS-cat-II") == 0) {
		navsubtype = ILS_cat_II;
	    } else if (strcmp(subType, "ILS-cat-III") == 0) {
		navsubtype = ILS_cat_III;
	    } else if (strcmp(subType, "LDA-GS") == 0) {
		navsubtype = LDA_GS;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    // EYE - have a NAV_ILS and NAV_LOC?
	    navtype = NAV_ILS; 
	    break; 
	  case 5: 
	    if (strcmp(subType, "LDA") == 0) {
		navsubtype = LDA;
	    } else if (strcmp(subType, "LOC") == 0) {
		navsubtype = LOC;
	    } else if (strcmp(subType, "SDF") == 0) {
		navsubtype = SDF;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    navtype = NAV_ILS; 
	    break; 
	  case 6: 
	    // EYE - if we only have one subtype, forget the whole
	    // subtype business?
	    if (strcmp(subType, "GS") == 0) {
		navsubtype = GS;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    navtype = NAV_GS; 
	    break;  
	  case 7: 
	    if (strcmp(subType, "OM") == 0) {
		// Since the navaid database specifies no range for
		// markers, we set our own, such that it is bigger
		// than the marker's rendered size.
		range = __markerRange;
		navsubtype = OM;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    navtype = NAV_OM; 
	    break;  
	  case 8: 
	    if (strcmp(subType, "MM") == 0) {
		range = __markerRange;
		navsubtype = MM;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    navtype = NAV_MM; 
	    break;  
	  case 9: 
	    if (strcmp(subType, "IM") == 0) {
		range = __markerRange;
		navsubtype = IM;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    navtype = NAV_IM; 
	    break;  
	  case 12: 
	  case 13: 
	    // Due to the "great DME shift" of 2007.09, we need to do
	    // extra processing to handle DMEs.  Here's the picture:
	    //
	    // Before:			After:
	    // Foo Bar DME-ILS		Foo Bar DME-ILS
	    // Foo Bar DME		Foo Bar DME
	    // Foo Bar NDB-DME		Foo Bar NDB-DME DME
	    // Foo Bar TACAN		Foo Bar TACAN DME
	    // Foo Bar VORTAC		Foo Bar VORTAC DME
	    // Foo Bar VOR-DME		Foo Bar VOR-DME DME
	    //
	    // The subType is now less useful, only telling us about
	    // DME-ILSs.  To find out the real subtype, we need to
	    // back one more token and look at that.  However, that
	    // doesn't work for "pure" DMEs (ie, "Foo Bar DME").  So,
	    // if the next token isn't NDB-DME, TACAN, VORTAC, or
	    // VOR-DME, then we must be looking at a pure DME.

	    if ((cycle > 2007.09) && (strcmp(subType, "DME-ILS") != 0)) {
		// New format.  Yuck.  We need to find the "real"
		// subType by looking back one token.
		char *subSubType = lastToken(line, subType);
		if ((strncmp(subSubType, "NDB-DME", 7) == 0) ||
		    (strncmp(subSubType, "TACAN", 5) == 0) ||
		    (strncmp(subSubType, "VORTAC", 6) == 0) ||
		    (strncmp(subSubType, "VOR-DME", 7) == 0)) {
		    // The sub-subtype is the real subtype (getting
		    // confused?).  Terminate the string, and make
		    // subType point to subSubType.
		    subType--;
		    *subType = '\0';
		    subType = subSubType;
		}
	    }

	    // Because DMEs are often paired with another navaid, we
	    // tend to ignore them, assuming that we've already
	    // created a navaid for them already.  The ones ignored
	    // are: VOR-DME, VORTAC, and NDB-DME.  We don't ignore
	    // DME-ILSs because, although paired with an ILS, their
	    // location is usually different.
	    if (strcmp(subType, "DME-ILS") == 0) {
		navsubtype = DME_ILS;
	    } else if (strcmp(subType, "TACAN") == 0) {
		// TACANs are drawn like VOR-DMEs, but with the lobes
		// not filled in.  They can provide directional
		// guidance, so they should have a compass rose.
		// Unfortunately, the nav.dat file doesn't tell us the
		// magnetic variation for the TACAN, so it can't be
		// used.
		navsubtype = TACAN;
	    } else if (strcmp(subType, "VOR-DME") == 0) {
		navsubtype = VOR_DME;
		continue;
	    } else if (strcmp(subType, "VORTAC") == 0) {
		navsubtype = VORTAC;
		continue;
	    } else if (strcmp(subType, "DME") == 0) {
		// EYE - For a real stand-alone DME, check lo1.pdf,
		// Bonnyville Y3, 109.8 (N54.31, W110.74, near Cold
		// Lake, east of Edmonton).  It is drawn as a simple
		// DME square (grey, as is standard on Canadian maps
		// it seems, although lo1.pdf is not a VFR map).
		navsubtype = DME;
	    } else if (strcmp(subType, "NDB-DME") == 0) {
		// We ignore NDB-DMEs, in the sense that we don't
		// create a navaid entry for them.  However, we do add
		// their frequency to the corresponding NDB.
		navsubtype = NDB_DME;
		// EYE - very crude
		unsigned int i;
		for (i = 0; i < _navaids.size(); i++) {
		    NAV *o = _navaids[i];
		    // EYE - look at name too
		    if ((o->navtype == NAV_NDB) && 
			(o->navsubtype == NDB_DME) && 
			(o->id == id)) {
			o->freq2 = freq;
			break;
		    }
		}
		if (i == _navaids.size()) {
		    printf("No matching NDB for NDB-DME %s (%s)\n", id, line);
		}
		continue;
	    } else {
		navsubtype = UNKNOWN;
	    }
	    navtype = NAV_DME; 
	    // For DMEs, magvar represents the DME bias, in nautical
	    // miles (which we convert to metres).
	    magvar *= SG_NM_TO_METER;
	    break;
	  default:
	    assert(false);
	    break;
	}
	if (navsubtype == UNKNOWN) {
	    printf("UNKNOWN: %s\n", line);
	}

	if (navtype == NAV_ILS) {
	    // For ILS elements, the name is <airport> <runway>.  I
	    // don't care about the airport, so skip it.
	    // EYE - check return?
	    sscanf(line, "%*s %n", &offset);
	    line += offset;
	}

	// Create a record and fill it in.
	n = new NAV;
	n->navtype = navtype;
	n->navsubtype = navsubtype;

	n->lat = lat;
	n->lon = lon;
	// EYE - in flight tracks, we save elevations (altitudes?) in
	// feet, but here we use metres.  Should we change one?
	n->elev = elev * SG_FEET_TO_METER;
	n->freq = freq;
	n->range = range * SG_NM_TO_METER;
	n->magvar = magvar;

	n->id = id;
	n->name = line;
	// EYE - this seems rather hacky and unreliable.
	n->name.erase(subType - line - 1);

	// Add to the culler.  The navaid bounds are given by its
	// center and range.
	sgdVec3 center;
	atlasGeodToCart(lat, lon, elev * SG_FEET_TO_METER, center);

	n->bounds.setCenter(center);
	n->bounds.setRadius(n->range);

	// Add to our culler.
	_frustumCullers[NAVAIDS]->culler().addObject(n);

	// Add to the _navaids vector.
	_navaids.push_back(n);

	// Create search tokens for it.
	_searcher->add(n);

	// Add to the _navPoints map.
	_NAVPOINT foo;
	foo.isNavaid = true;
	foo.n = (void *)n;
	_navPoints.insert(pair<string, _NAVPOINT>(n->id, foo));
    }
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

    FIX *f;

    while (gzGetLine(arp, &line)) {
	if (strcmp(line, "") == 0) {
	    // Blank line.
	    continue;
	} 

	if (strcmp(line, "99") == 0) {
	    // Last line.
	    break;
	}

	// Create a record and fill it in.
	f = new FIX;

	// A line looks like this:
	//
	// <lat> <lon> <name>
	//
	if (sscanf(line, "%lf %lf %s", &f->lat, &f->lon, f->name) != 3) {
	    fprintf(stderr, "FixesOverlay::_load600(): bad line in file:\n");
	    fprintf(stderr, "\t'%s'\n", line);
	    continue;
	}

	// Add to the culler.
	sgdVec3 point;
	atlasGeodToCart(f->lat, f->lon, 0.0, point);

	// We arbitrarily say fixes have a radius of 1000m.
	f->bounds.radius = 1000.0;
	f->bounds.setCenter(point);

	// Until determined otherwise, fixes are not assumed to be
	// part of any low or high altitude airways.
	f->low = f->high = false;

	// Add to our culler.
	_frustumCullers[FIXES]->culler().addObject(f);

	// Add to the fixes vector.
	_fixes.push_back(f);

	// Create search tokens for it.
	_searcher->add(f);

	// Add to the _navPoints map.
	_NAVPOINT foo;
	foo.isNavaid = false;
	foo.n = (void *)f;
	_navPoints.insert(pair<string, _NAVPOINT>(f->name, foo));
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

void NavData::_loadAirways640(const gzFile& arp)
{
    char *line;

    AWY *a;

    while (gzGetLine(arp, &line)) {
	if (strcmp(line, "") == 0) {
	    // Blank line.
	    continue;
	} 

	if (strcmp(line, "99") == 0) {
	    // Last line.
	    break;
	}

	// Create a record and fill it in.
	a = new AWY;
	istringstream str(line);

	// A line looks like this:
	//
	// <id> <lat> <lon> <id> <lat> <lon> <high/low> <base> <top> <name>
	//
	// 
	int lowHigh;
	str >> a->start.id >> a->start.lat >> a->start.lon
	    >> a->end.id >> a->end.lat >> a->end.lon
	    >> lowHigh >> a->base >> a->top >> a->name;
	// EYE - check for errors
	if (lowHigh == 1) {
	    a->isLow = true;
	} else if (lowHigh == 2) {
	    a->isLow = false;
	} else {
	    assert(false);
	}

	// Add to the culler.  The airway bounds are given by its two
	// endpoints.
	// EYE - save these two points
	sgdVec3 point;
	atlasGeodToCart(a->start.lat, a->start.lon, 0.0, point);
	a->bounds.extend(point);
	atlasGeodToCart(a->end.lat, a->end.lon, 0.0, point);
	a->bounds.extend(point);
	double az1, az2, s;
	geo_inverse_wgs_84(0.0, a->start.lat, a->start.lon, 
			   a->end.lat, a->end.lon,
			   &az1, &az2, &s);
	a->length = s;				       

	// Add to our culler.
	_frustumCullers[AIRWAYS]->culler().addObject(a);

	// Add to the segments vector.
	_segments.push_back(a);
	
	// Look for the two endpoints in the _navPoints map.  For
	// those that are fixes, update their high/low status.
	_checkEnd(a->start, a->isLow);
	_checkEnd(a->end, a->isLow);
    }
}

// Each airway segment has two endpoints, which should be fixes and/or
// navaids.  If an endpoint is a fix, we use the airway type as a
// heuristic to decide whether that fix is a high or low fix.  Note
// that the navaid, fix, and airways databases are not perfect, so we
// need to handle cases where no or partial matches are made.
void NavData::_checkEnd(AwyLabel &end, bool isLow)
{
    // EYE - clear as mud!
    multimap<string, _NAVPOINT>::iterator it;
    pair<multimap<string, _NAVPOINT>::iterator, 
	multimap<string, _NAVPOINT>::iterator> ret;
    
    // Search for a navaid or fix with the same name and same location
    // as 'end'.
    ret = _navPoints.equal_range(end.id);
    for (it = ret.first; it != ret.second; it++) {
	_NAVPOINT p = (*it).second;
	double lat, lon;
	if (p.isNavaid) {
	    NAV *n = (NAV *)p.n;
	    lat = n->lat;
	    lon = n->lon;
	} else {
	    FIX *f = (FIX *)p.n;
	    lat = f->lat;
	    lon = f->lon;
	}

	if ((lat == end.lat) && (lon == end.lon)) {
	    // Bingo!
	    if (!p.isNavaid) {
		// If the end is a fix, make sure we tag it as high/low.
		FIX *f = (FIX *)p.n;
		if (isLow) {
		    f->low = true;
		} else {
		    f->high = true;
		}
	    }

	    // EYE - put a _NAVPOINT structure in AwyLabel?
	    end.isNavaid = p.isNavaid;
	    end.n = p.n;

	    // We've found an exact match, so bail out early.
	    return;
	}
    }

    // Couldn't find an exact match.  Find the closest navaid or fix
    // with the same name.
    double distance = 1e12;
    double latitude, longitude;
    for (it = ret.first; it != ret.second; it++) {
	_NAVPOINT p = (*it).second;
	FIX *f;
	NAV *n;
	double lat, lon;
	if (p.isNavaid) {
	    n = (NAV *)p.n;
	    lat = n->lat;
	    lon = n->lon;
	} else {
	    f = (FIX *)p.n;
	    lat = f->lat;
	    lon = f->lon;
	}

	double d, junk;
	geo_inverse_wgs_84(lat, lon, end.lat, end.lon, &junk, &junk, &d);
	if (d < distance) {
	    distance = d;
	    latitude = lat;
	    longitude = lon;
	}
    }

    // EYE - we need some kind of logging facility.
//     if (distance == 1e12) {
// 	fprintf(stderr, "_findEnd: can't find any match for '%s' <%lf, %lf>\n",
// 		end.id.c_str(), end.lat, end.lon);
//     } else {
// 	fprintf(stderr, "_findEnd: closest match for '%s' <%lf, %lf> is\n",
// 		end.id.c_str(), end.lat, end.lon);
// 	fprintf(stderr, "\t%.0f metres away <%lf, %lf>\n",
// 		distance, latitude, longitude);
//     }
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
    if (version == 810) {
	// It looks like we have a valid file.
	_loadAirports810(arp);
    } else {
	// EYE - throw an error?
	fprintf(stderr, "AirportsOverlay::load: \"%s\": unknown version %d.\n", 
		f.c_str(), version);
	throw runtime_error("unknown airports file version");
    }

    gzclose(arp);
    printf("  ... done\n");
}

// Calculates the airport's center in lat, lon from its bounds.
static void __airportLatLon(ARP *ap)
{
    double lat, lon, alt;
    sgdVec3 c;
    sgdSetVec3(c,
	       ap->bounds.center[0], 
	       ap->bounds.center[1], 
	       ap->bounds.center[2]);
    sgCartToGeod(c, &lat, &lon, &alt);
    ap->lat = lat * SGD_RADIANS_TO_DEGREES;
    ap->lon = lon * SGD_RADIANS_TO_DEGREES;
}

// Given a runway with a valid lat, lon, and heading (in degrees), and
// a valid length and width (in metres), sets its bounds, "ahead"
// vector (a normalized vector pointing along the runway in the given
// heading), "aside" vector (a normalized vector pointing across the
// runway, 90 degrees clockwise from the ahead vector), and its
// "above" vector (its normal vector).
void __runwayExtents(RWY *rwy, float elev)
{
    // In PLIB, "up" (the direction of our normal) is along the
    // positive y-axis.  What we call "ahead" (looking along our
    // heading, where the runway points), is along the positive
    // z-axis), and what we call "aside" (looking across the runway,
    // 90 degrees from our heading), is along the negative x-axis.

    // EYE - am I thinking about this right?  Is it what PLIB
    // "thinks", or what I think?
    sgdSetVec3(rwy->ahead, 0.0, 0.0, 1.0);
    sgdSetVec3(rwy->aside, -1.0, 0.0, 0.0);
    sgdSetVec3(rwy->above, 0.0, 1.0, 0.0);

    sgdMat4 rot;
    double heading = rwy->lon - 90.0;
    double pitch = rwy->lat;
    double roll = -rwy->hdg;

    // This version has us in our standard orientation, which means 0
    // lat, 0 lon, and a heading of 0 (north).
    // EYE - untested
//     sgdSetVec3(rwy->ahead, 0.0, 0.0, 1.0);
//     sgdSetVec3(rwy->aside, 0.0, 1.0, 0.0);
//     sgdSetVec3(rwy->above, 1.0, 0.0, 0.0);

//     sgdMat4 rot;
//     double heading = rwy->lon;
//     double pitch = -rwy->hdg;
//     double roll = -rwy->lat;

    // This version is in the standard PLIB orientation, facing out
    // along the y axis, the x axis right, and the z axis up.
    // EYE - untested
//     sgdSetVec3(rwy->ahead, 0.0, 0.0, 1.0);
//     sgdSetVec3(rwy->aside, 1.0, 0.0, 0.0);
//     sgdSetVec3(rwy->above, 0.0, 1.0, 0.0);

//     sgdMat4 rot;
//     double heading = 90.0 - rwy->lon;
//     double pitch = rwy->lat;
//     double roll = rwy->hdg;

    sgdMakeRotMat4(rot, heading, pitch, roll);

    sgdXformVec3(rwy->ahead, rot);
    sgdXformVec3(rwy->aside, rot);
    sgdXformVec3(rwy->above, rot);

    // Calculate our bounding sphere.
    sgdVec3 center;
    atlasGeodToCart(rwy->lat, rwy->lon, elev, center);
    sgdCopyVec3(rwy->bounds.center, center);

    sgdMat4 mat;
    sgdMakeTransMat4(mat, rwy->bounds.center);
    sgdPreMultMat4(mat, rot);

    sgdVec3 ll = {rwy->width / 2,  0.0, -rwy->length / 2};
    sgdVec3 lr = {-rwy->width / 2, 0.0, -rwy->length / 2};
    sgdVec3 ul = {rwy->width / 2, 0.0, rwy->length / 2};
    sgdVec3 ur = {-rwy->width / 2, 0.0, rwy->length / 2};

    sgdXformPnt3(ul, mat);
    sgdXformPnt3(lr, mat);
    sgdXformPnt3(ll, mat);
    sgdXformPnt3(ur, mat);
    rwy->bounds.extend(ul);
    rwy->bounds.extend(ll);
    rwy->bounds.extend(ll);
    rwy->bounds.extend(ur);
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
		    // Calculate the airport's center in lat, lon.
		    __airportLatLon(ap);
		    // Add it to our airports vector.
		    _airports.push_back(ap);
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

		// Create a new airport record.
		ap = new ARP;

		float elevation;
		int controlled;
		char code[5];	// EYE - safe?

		sscanf(line, "%f %d %*d %s %n", 
		       &elevation, &controlled, code, &offset);
		line += offset;

		ap->elev = elevation * SG_FEET_TO_METER;
		ap->controlled = (controlled == 1);
		ap->id = code;
		ap->name = line;
		// This will be set to true if we find a runway with
		// any kind of runway lighting.
		ap->lighting = false;
		// If set to true, then beaconLat and beaconLon
		// contain the location of the beacon.
		ap->beacon = false;
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

		// We ignore taxiways and helipads.
		if (strcmp(rwyid, "xxx") == 0) {
		    break;
		}
		if (strncmp(rwyid, "H", 1) == 0) {
		    break;
		}

		// Strip off trailing x's.
		int firstX = strcspn(rwyid, "x");
		if (firstX > 0) {
		    rwyid[firstX] = '\0';
		}
		assert(strlen(rwyid) <= 3);

		// Runway!
		RWY *rwy = new RWY;

		float heading, length, width;
		char *lighting;

		sscanf(line, "%f %f %*f %*f %f %n", 
		       &heading, &length, &width, &offset);
		lighting = line + offset;

		rwy->lat = lat;
		rwy->lon = lon;
		rwy->hdg = heading;
		rwy->length = length * SG_FEET_TO_METER;
		rwy->width  = width * SG_FEET_TO_METER;
		rwy->id = rwyid;
		ap->rwys.push_back(rwy);

		__runwayExtents(rwy, ap->elev);
		ap->bounds.extend(&(rwy->bounds));

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
		    ap->lighting = true;
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
		    ap->beacon = true;
		    ap->beaconLat = lat;
		    ap->beaconLon = lon;
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

		      FrequencyMap& f = ap->freqs[(ATCCodeType)lineCode];
		      set<int>& freqs = f[line];
		      if ((freq % 10 == 2) || (freq % 10 == 7)) {
			  freqs.insert(freq * 10 + 5);
		      } else {
			  freqs.insert(freq * 10);
		      }
		  }
	      }
	    break;
	}
    }

    if (ap != NULL) {
	// Calculate the airport's center in lat, lon.
	__airportLatLon(ap);
	// Add it to our airports vector.
	_airports.push_back(ap);
	// Add our airport text to the searcher object.
	_searcher->add(ap);
	// Add to our culler.
	_frustumCullers[AIRPORTS]->culler().addObject(ap);
    }
}
