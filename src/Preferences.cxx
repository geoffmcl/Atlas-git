/*-------------------------------------------------------------------------
  Preferences.cxx

  Written by Brian Schack, started August 2007.

  Copyright (C) 2007 - 2012 Brian Schack

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
#include "Preferences.hxx"

// C system files
#include <getopt.h>
#ifndef _MSC_VER
#include "libgen.h"
#endif

// C++ system files
#include <fstream>

// Our project's include files
#include "config.h"		// For VERSION
#include "misc.hxx"

using namespace std;
using namespace Prefs;

// Preferences file.
const char *atlasrc = ".atlasrc";

//////////////////////////////////////////////////////////////////////
//
// A little routine to chop up a string into pieces of the given
// length or less, respecting word boundaries.  It probably isn't very
// efficient, returning an entire vector, but it won't be called very
// often.
static vector<string> __chop(const char *str, int len = 80)
{
    vector<string> result;
    size_t start = 0, end = 0;
    char ws[] = " \t";
    size_t n = strlen(str);
    while (end < n) {
	start = strspn(str + start, ws) + start;
	end = min(start + len, n);
	if (end < n) {
	    // Move back until we get to a whitespace character.
	    while(!strchr(ws, str[end]) && (end > start)) {
		end--;
	    }
	    // Move back to the first whitespace character in this
	    // group.
	    while(strchr(ws, str[end]) && (end > start)) {
		end--;
	    }
	    end++;
	}
	result.push_back(string(str, start, end - start));
	start = end;
    }

    return result;
}

//////////////////////////////////////////////////////////////////////
// Pref
//////////////////////////////////////////////////////////////////////

vector<Pref *> Pref::__options;
int Pref::__val;

Pref::Pref(const char *name, int has_arg, const char *shortHelp, 
	   const char *longHelp): _dirty(false)
{
    // Note that we don't deal with combinations of deleting and
    // creating preference entries.  We assume that all preferences
    // will be created in one fell swoop, and never deleted (until the
    // program exits).
    _defined.reset();

    _option.name = strdup(name);
    _option.has_arg = has_arg;
    _option.flag = &__val;
    _option.val = __options.size();

    AtlasString str;
    if (has_arg == no_argument) {
	str.printf("--%s", name);
    } else if (has_arg == optional_argument) {
	str.printf("--%s[=%s]", name, shortHelp);
    } else {
	str.printf("--%s=%s", name, shortHelp);
    }
    _shortHelp = strdup(str.str());
    _longHelp = strdup(longHelp);

    __options.push_back(this);
}

Pref::~Pref()
{
    free((void *)_option.name);
    free(_shortHelp);
    free(_longHelp);
}

void Pref::reset() 
{
    // EYE - is this the same as set(get(FACTORY))?
    _defined[CMD_LINE] = false;
    if (_defined[PREF_FILE]) {
	_defined[PREF_FILE] = false;
	_dirty = true;
    }
};

bool Pref::save(ostream& ostr) 
{
    _dirty = false;
    if (isDefault()) {
	return false;
    }
    vector<string> strs = __chop(longHelp(), 78);
    for (size_t i = 0; i < strs.size(); i++) {
	ostr << "# " << strs[i] << "\n";
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// NoArgPref
//////////////////////////////////////////////////////////////////////

NoArgPref::NoArgPref(const char *name, const char *longHelp):
    Pref(name, no_argument, "", longHelp), _seen(0)
{
}

bool NoArgPref::save(std::ostream& ostr)
{
    bool result = false;
    if (Pref::save(ostr)) {
	for (int i = 0; i < _seen; i++) {
	    ostr << "--" << name() << "\n\n";
	}
	result = true;
    }

    return result;
}

//////////////////////////////////////////////////////////////////////
// TypedPref
//////////////////////////////////////////////////////////////////////

// Note that I've placed the template class method implementations in
// the .cxx file, which is unusual for templates.  I did this because
// it's so annoying to have to recompile almost everything whenever I
// change an implementation detail (a lot of code depends on
// Preferences.hxx).  Putting it here means only Preferences.cxx will
// be recompiled.  However, templates don't work well in .cxx files,
// so I need to explicitly instantiate the instances I'll need at the
// end of the file (which see).
template<class T>
TypedPref<T>::TypedPref(const char *name,
			const char *shortHelp, 
			const char *longHelp):
    Pref(name, required_argument, shortHelp, longHelp), _optionalValue(NULL)
{
    _values.resize(SOURCE_COUNT);
}

template<class T>
TypedPref<T>::TypedPref(const char *name, 
			const char *optionalValue,
			const char *shortHelp, 
			const char *longHelp):
    Pref(name, optional_argument, shortHelp, longHelp)
{
    _values.resize(SOURCE_COUNT);
    _optionalValue = strdup(optionalValue);
}

template<class T>
TypedPref<T>::~TypedPref()
{
    free(_optionalValue);
}

template<class T>
inline T TypedPref<T>::get(PrefSource s) const
{
    for (int i = s; i < SOURCE_COUNT; i++) {
	if (_defined.test(i)) {
	    return _values[i];
	}
    }
    return T();
}

// Set our value for the given source, setting ourselves to dirty if
// our PREF_FILE value has changed.
template<class T>
inline void TypedPref<T>::set(T v, PrefSource s) 
{
    assert(s != SOURCE_COUNT);
    _defined[s] = true;
    _values[s] = v;
    if (s == PREF_FILE) {
	_dirty = true;
    }
}

template<class T>
inline bool TypedPref<T>::parse(const char *str, PrefSource s) 
{
    assert(s != SOURCE_COUNT);
    _defined[s] = false;
    bool result = false;
    // If there is no option string, but we accept optional arguments,
    // set 'str' to the predefined optional value.
    if (!str && (option().has_arg == optional_argument)) {
	str = _optionalValue;
    }
    // If there's an argument, parse it.
    if (str) {
	istringstream stream(str);
	stream >> _values[s];
	if (stream) {
	    _defined[s] = true;
	    result = true;
	}
    }

    return result;
}

template<class T>
inline bool TypedPref<T>::save(ostream &ostr) 
{
    bool result = false;
    if (Pref::save(ostr)) {
	ostr << "--" << name() << "=" << get(Pref::PREF_FILE) << "\n\n";
	result = true;
    }

    return result;
}

//////////////////////////////////////////////////////////////////////
// TypedPrefVector
//////////////////////////////////////////////////////////////////////

template<class T>
TypedPrefVector<T>::TypedPrefVector(const char *name,
				    const char *shortHelp, 
				    const char *longHelp):
    TypedPref<T>(name, shortHelp, longHelp), _dirty(false)
{
}
template<class T>
TypedPrefVector<T>::TypedPrefVector(const char *name,
				    const char *optionalValue,
				    const char *shortHelp, 
				    const char *longHelp):
    TypedPref<T>(name, optionalValue, shortHelp, longHelp), _dirty(false)
{
}

template<class T>
inline void TypedPrefVector<T>::setPrefs(vector<T>& v) 
{
    _prefs = v;
    _dirty = true;
}

template<class T>
inline bool TypedPrefVector<T>::save(ostream& ostr)
{
    bool result = false;
    if (Pref::save(ostr)) {
	for (size_t i = 0; i < _prefs.size(); i++) {
	    // We need to use 'this->' here, for complicated reasons.
	    // See 11.8.2 "Name lookup, templates, and accessing
	    // members of base classes" in the GCC manual.
	    ostr << "--" << this->name() << "=" << _prefs[i] << "\n";
	}
	ostr << "\n";
	result = true;
    }

    return result;
}

//////////////////////////////////////////////////////////////////////
// SGPath
//////////////////////////////////////////////////////////////////////

// operator>> for SGPath
istream& operator>> (istream& is, SGPath& p)
{
    string buf;
    is >> buf;
    p.set(buf);

    return is;
}

//////////////////////////////////////////////////////////////////////
// Geometry
//////////////////////////////////////////////////////////////////////

Geometry::Geometry(): _w(0), _h(0)
{
}

Geometry::Geometry(int w, int h): _w(w), _h(h)
{
}

Geometry::Geometry(const Geometry& g)
{
    _w = g._w;
    _h = g._h;
}

int Geometry::operator==(const Geometry& right) const
{
    return ((_w == right._w) && (_h == right._h));
}

int Geometry::operator!=(const Geometry& right) const
{
    return !(*this == right);
}

// EYE - why do I need to add Prefs:: here (and for other friend functions)?
istream& Prefs::operator>>(istream& str, Geometry& g)
{
    char by;
    str >> g._w;
    str >> by;
    if (by != 'x') {
	str.setstate(ios::failbit);
    }
    return str >> g._h;
}

ostream& Prefs::operator<<(ostream& str, const Geometry& g)
{
    return str << g._w << "x" << g._h;
}

//////////////////////////////////////////////////////////////////////
// Bool
//////////////////////////////////////////////////////////////////////

Bool::Bool(): _b(false)
{
}

Bool::Bool(bool b): _b(b)
{
}

Bool::Bool(const Bool& b)
{
    _b = b._b;
}

int Bool::operator==(const Bool& right) const
{
    return (_b == right._b);
}

int Bool::operator!=(const Bool& right) const
{
    return !(*this == right);
}

istream& Prefs::operator>>(istream& istr, Bool& b)
{
    string str;
    istr >> str;
    if ((str == "true") || (str == "t") ||
	(str == "yes") || (str == "y") ||
	(str == "on") || (str == "1")) {
	b._b = true;
    } else {
	b._b = false;
    }
    return istr;
}

ostream& Prefs::operator<<(ostream& str, const Bool& b)
{
    if (b._b) {
	// // return str << "";
	// return str << "on";
	return str << "y";
    } else {
	// return str << "off";
	return str << "n";
    }
}

//////////////////////////////////////////////////////////////////////
// LightPosition
//////////////////////////////////////////////////////////////////////

LightPosition::LightPosition(): _azimuth(0.0), _elevation(0.0)
{
}

LightPosition::LightPosition(float azim, float elev): 
    _azimuth(azim), _elevation(elev)
{
}

LightPosition::LightPosition(const LightPosition& g)
{
    _azimuth = g._azimuth;
    _elevation = g._elevation;
}

int LightPosition::operator==(const LightPosition& right) const
{
    return ((_azimuth == right._azimuth) && (_elevation == right._elevation));
}

int LightPosition::operator!=(const LightPosition& right) const
{
    return !(*this == right);
}

istream& Prefs::operator>>(istream& istr, LightPosition& g)
{
    char comma;
    istr >> g._azimuth;
    istr >> comma;
    if (comma != ',') {
	istr.setstate(ios::failbit);
    }
    return istr >> g._elevation;
}

ostream& Prefs::operator<<(ostream& ostr, const LightPosition& g)
{
    return ostr << g._azimuth << "," << g._elevation;
}

//////////////////////////////////////////////////////////////////////
// ImageType
//////////////////////////////////////////////////////////////////////

istream& operator>>(istream& istr, TileMapper::ImageType& i)
{
    string str;
    istr >> str;
    if (strcasecmp("jpeg", str.c_str()) == 0) {
	i = TileMapper::JPEG;
    } else if (strcasecmp("jpg", str.c_str()) == 0) {
	i = TileMapper::JPEG;
    } else if (strcasecmp("png", str.c_str()) == 0) {
	i = TileMapper::PNG;
    } else {
	istr.setstate(ios::failbit);
    }
    return istr;
}

ostream& operator<<(ostream& ostr, const TileMapper::ImageType& i)
{
    if (i == TileMapper::JPEG) {
	return ostr << "JPEG";
    } else {
	return ostr << "PNG";
    }
}

//////////////////////////////////////////////////////////////////////
// SerialConnection
//////////////////////////////////////////////////////////////////////

SerialConnection::SerialConnection(): _device("/dev/ttyS0"), _baud(4800)
{
}

SerialConnection::SerialConnection(string device, int baud): 
    _device(device), _baud(baud)
{
}

SerialConnection::SerialConnection(const SerialConnection& sc)
{
    _device = sc._device;
    _baud = sc._baud;
}

int SerialConnection::operator==(const SerialConnection& right) const
{
    return ((_device == right._device) && (_baud == right._baud));
}

int SerialConnection::operator!=(const SerialConnection& right) const
{
    return !(*this == right);
}

istream& Prefs::operator>>(istream& istr, SerialConnection& sc)
{
    char comma;
    istr >> sc._baud;
    istr >> comma;
    if (comma != ',') {
	istr.setstate(ios::failbit);
    }
    return istr >> sc._device;
}

ostream& Prefs::operator<<(ostream& ostr, const SerialConnection& sc)
{
    return ostr << sc._baud << "," << sc._device;
}

//////////////////////////////////////////////////////////////////////
// Preferences
//////////////////////////////////////////////////////////////////////

const int Preferences::noAtlasrcFile = -1;
const int Preferences::noAtlasrcVersion = 0;

Preferences::Preferences():
    latitude("lat", "<x>", "Start browsing at latitude x (south is negative)"),
    longitude("lon", "<x>", "Start browsing at longitude x (west is negative)"),
    zoom("zoom", "<x>", "Set zoom level to x metres/pixel"),
    icao("airport", "<airport>", 
	 "Start browsing at an airport specified by ICAO code or airport name"),

    path("atlas", "<path>", "Set path for map images"),
    fg_root("fg-root", "<path>", "Overrides FG_ROOT environment variable"),
    scenery_root("fg-scenery", "<path>", 
		 "Overrides FG_SCENERY environment variable"),

    // EYE - instead of "Set ...", just "..."?  In other words,
    // replace the verb phrase with a noun phrase?
    // EYE - deprecate softcursor, texturefonts, serial, baudrate?
    geometry("geometry", "<w>x<h>", "Set initial window size"),
    textureFonts("glutfonts", "y", "y|n",
		 "Use GLUT bitmap fonts (fast for software rendering)"),
    softcursor("softcursor", "y", "y|n",
	       "Draw mouse cursor using OpenGL (for fullscreen Voodoo cards)"),

    autocentreMode("autocentre-mode", "y", "y|n",
		   "Automatically center map on aircraft"),
    lineWidth("line-width", "<w>", 
	      "Set line width of flight track overlays (in pixels)"),
    airplaneImage("airplane", "<path>", 
		  "Specify image to be used as airplane symbol in flight "
		  "tracks.  If not present, a default image is used."),
    airplaneImageSize("airplane-size", "<size>", 
		      "Set the size of the airplane image (in pixels)"),

    update("update", "<s>", "Check for position updates every s seconds"),
    maxTrack("max-track", "<x>", 
	     "Maximum number of points to record while tracking a "
	     "flight (0 = unlimited)"),
    networkConnections("udp", "5500", "<port>", 
		       "Read input from UDP socket at specified port"),
    serialConnections("serial", "<baud,device>", 
		      "Read input at the specified baud rate from the "
		      "specified device"),

    discreteContours("discrete-contours", "y", "y|n",
		     "Don't blend contour colours on maps"),
    contourLines("contour-lines", "y", "y|n",
		 "Draw contour lines at contour boundaries"),
    lightingOn("lighting", "y", "y|n", "Light the terrain on maps"),
    smoothShading("smooth-shading", "y", "y|n", "Smooth polygons on maps"),
    lightPosition("light", "<azim,elev>",
		  "Set light position for maps (all units in degrees).  "
		  "Azimuth is light direction (0 = north, 90 = east, ...).  "
		  "Elevation is height above horizon (90 = overhead)."),
    imageType("image-type", "{JPEG,PNG}", "Set output map file type"),
    JPEGQuality("jpeg-quality", "<q>", 
		"Set JPEG file quality (0 = lowest, 100 = highest)"),
    palette("palette", "<name>", "Specify Atlas palette"),

    version("version", "Print version number"),
    help("help", "Print this help"),

    _atlasrcVersion(noAtlasrcFile)
{
    latitude.set(37.5, Pref::FACTORY);
    longitude.set(-122.25, Pref::FACTORY);
    zoom.set(125.0, Pref::FACTORY);
    icao.set("", Pref::FACTORY);

    // EYE - we could get all this from a running instance of FlightGear:
    //
    // /sim/fg-root
    // /sim/fg-scenery
    // /sim/fg-scenery[1]
    char *env = getenv("FG_ROOT");
    if (env == NULL) {
	// EYE - can this not be defined?  Should we just get rid of
	// FGBASE_DIR altogether?
	fg_root.set(SGPath(FGBASE_DIR), Pref::FACTORY);
    } else {
	fg_root.set(SGPath(env), Pref::FACTORY);
    }

    env = getenv("FG_SCENERY");
    if (env == NULL) {
	SGPath p(fg_root.get());
	p.append("Scenery");
	scenery_root.set(p, Pref::FACTORY);
    } else {
	scenery_root.set(SGPath(env), Pref::FACTORY);
    }

    // EYE - just get()?
    SGPath p(fg_root.get(Pref::FACTORY));
    if (p.isNull()) {
	p.set(FGBASE_DIR);
    }
    p.append("Atlas");
    path.set(p, Pref::FACTORY);

    Geometry g(1000, 750);
    geometry.set(g, Pref::FACTORY);
    textureFonts.set(true, Pref::FACTORY);
    softcursor.set(false, Pref::FACTORY);

    autocentreMode.set(false, Pref::FACTORY);
    lineWidth.set(1.0, Pref::FACTORY);
    p = path.get();
    p.append("airplane_image.png");
    airplaneImage.set(p, Pref::FACTORY);
    airplaneImageSize.set(25.0, Pref::FACTORY);

    update.set(1.0, Pref::FACTORY);
    maxTrack.set(0, Pref::FACTORY);
    networkConnections.set(5500, Pref::FACTORY);
    SerialConnection sc;
    serialConnections.set(sc, Pref::FACTORY);

    discreteContours.set(true, Pref::FACTORY);
    contourLines.set(false, Pref::FACTORY);
    lightingOn.set(true, Pref::FACTORY);
    smoothShading.set(true, Pref::FACTORY);
    LightPosition l(315.0, 55.0);
    lightPosition.set(l, Pref::FACTORY);
    JPEGQuality.set(75, Pref::FACTORY);
    imageType.set(TileMapper::JPEG, Pref::FACTORY);
    palette.set("default.ap", Pref::FACTORY);

    int n = (int)Pref::options().size();
    _long_options = (struct option *)malloc(sizeof(struct option) * (n + 1));
    for (int i = 0; i < n; i++) {
	_long_options[i] = Pref::options()[i]->option();
    }
    _long_options[n].name = (char *)0;
    _long_options[n].has_arg = 0;
    _long_options[n].flag = (int *)0;
    _long_options[n].val = 0;
}

Preferences::~Preferences()
{
    free(_long_options);
}

void Preferences::usage(int option)
{
    // Usage for an option is printed something like this:
    //
    //   --airport=<airport>       Start browsing at an airport specified by 
    //                             ICAO code or airport name
    //
    // There's some leading indentation, then the short help, then the
    // long help (possibly split over several lines).
    //
    // EYE - we should really indicate default values too

    // Our indentation.
    char indent[] = "  ";

    // Calculate the space needed for the short and long help strings.
    // Yeah, I know we should only do this once, not every time
    // usage() is called, but it won't be called very often.
    size_t shortSize = 0, longSize;
    int n = (int)Pref::options().size();
    for (int i = 0; i < n; i++) {
	Pref *e = Pref::options()[i];
	if (strlen(e->shortHelp()) > shortSize) {
	    shortSize = strlen(e->shortHelp());
	}
    }
    shortSize++;
    longSize = 80 - shortSize - strlen(indent);

    AtlasString fmt;
    fmt.printf("%%s%%-%ds%%s\n", shortSize);
    if ((option < 0) || (option >= n)) {
	for (option = 0; option < n; option++) {
	    Pref *e = Pref::options()[option];
	    vector<string> strs = __chop(e->longHelp(), longSize);
	    printf(fmt.str(), indent, e->shortHelp(), strs[0].c_str());
	    for (size_t i = 1; i < strs.size(); i++) {
		printf(fmt.str(), indent, "", strs[i].c_str());
	    }
	}
	printf("\n");

	const char *str = 
	    "Note: For all boolean options (those with [=y|n] parameters), "
	    "other values can be used.  Specifically, 'true', 't', 'yes', "
	    "'y', 'on', '1' or nothing at all turn the option on.  Anything "
	    "else (eg, 'false', 'off', 'banana', ...) turns it off.";
	vector<string> strs = __chop(str);
	for (size_t i = 0; i < strs.size(); i++) {
	    printf("%s\n", strs[i].c_str());
	}
    } else {
	Pref *e = Pref::options()[option];
	vector<string> strs = __chop(e->longHelp(), longSize);
	printf(fmt.str(), indent, e->shortHelp(), strs[0].c_str());
	for (size_t i = 1; i < strs.size(); i++) {
	    printf(fmt.str(), indent, "", strs[i].c_str());
	}
    }
}

void Preferences::shortUsage(int i)
{
    int n = (int)Pref::options().size();
    if ((i < 0) || (i >= n)) {
	// EYE - split lines?
	for (int i = 0; i < n; i++) {
	    Pref *e = Pref::options()[i];
	    printf("[%s] ", e->shortHelp());
	}
	printf("[<flight file>] ...\n");
    } else {
	Pref *e = Pref::options()[i];
	printf("  %-20s %s\n", e->shortHelp(), e->longHelp());
    }
}

bool Preferences::load(int argc, char *argv[])
{
    // Check for a preferences file.
    char* homedir = getenv("HOME");
    SGPath rcpath;
    if (homedir != NULL) {
	rcpath.set(homedir);
	rcpath.append(atlasrc);
    } else {
	rcpath.set(atlasrc);
    }

    ifstream rc(rcpath.c_str());
    if (rc.is_open()) {
	// We have a file, so change _atlasrcVersion from noAtlasFile
	// to noAtlasVersion.  If we find a version line in the file,
	// we'll put the version number into _atlasrcVersion.
	_atlasrcVersion = noAtlasrcVersion;

	char *lines[2];
	string line;

	// By default, getopt_long() (called in _loadPreferences())
	// skips past the first argument, which is usually the
	// executable name.  Theoretically, we should be able to tell
	// it to start processing from the first argument by setting
	// optind to 0, but I can't seem to get it to work, and
	// anyway, our error messages depend on argv[0] being set to
	// the executable.
	lines[0] = argv[0];
	while (!rc.eof()) {
	    getline(rc, line);

	    // Skip emtpy lines.
	    if (line.length() == 0) {
		continue;
	    }

	    // Check for a version string.  Our version is given in a
	    // special comment of the format "#ATLASRC Version x".
	    int v;
	    if (sscanf(line.c_str(), "#ATLASRC Version %d", &v) == 1) {
		// We have a version string, so copy the version
		// number.
		_atlasrcVersion = v;
	    }

	    // Skip all other comments.
	    if (line[0] == '#') {
		continue;
	    }

	    // I guess it's a real option.  We'll put it in lines[1],
	    // starting with the first non-whitespace character.
	    lines[1] = (char *)(line.c_str() + strspn(line.c_str(), " \t"));

	    // Try to make sense of it.
	    if (!_load(2, lines, Pref::PREF_FILE)) {
		fprintf(stderr, "%s: Error in %s: '%s'.\n",
			basename(argv[0]), atlasrc, lines[1]);
		return false;
	    }
	}

	rc.close();
    }

    // Now parse the real command-line arguments.
    if (!_load(argc, argv, Pref::CMD_LINE)) {
	return false;
    }

    if (version.seen()) {
	printf("%s version %s\n", basename(argv[0]), VERSION);
	return false;
    }
    if (help.seen()) {
	printf("ATLAS - A map browsing utility for FlightGear\n\n"
	       "Usage:\n\n");
	printf("%s <options> [<flight file>] ...\n\n", basename(argv[0]));
	usage();
	return false;
    }

    return true;
}

void Preferences::save(ostream& ostr)
{
    // If nothing has changed, we don't need to save anything.
    if (!isDirty()) {
	return;
    }

    ostr << "#ATLASRC VERSION " << ATLASRC_VERSION << "\n\n";
    int n = (int)Pref::options().size();
    for (int i = 0; i < n; i++) {
	Pref *e = Pref::options()[i];
	e->save(ostr);
    }

    // Save flight files.
    for (size_t i = 0; i < flightFiles.size(); i++) {
	// EYE - get absolute file names.  Unfortunately, SGPath's
	// realpath() is broken (for Windows and Mac).
	printf("%s\n", flightFiles[i].str().c_str());
    }
}

bool Preferences::isDirty()
{
    int n = (int)Pref::options().size();
    for (int i = 0; i < n; i++) {
	Pref *e = Pref::options()[i];
	if (e->isDirty()) {
	    return true;
	}
    }
    return false;
}

bool Preferences::_load(int argc, char *argv[], Pref::PrefSource source)
{
    int c;
    int option_index = 0;

    // In case getopt32 was already called:
    // reset the libc getopt() function, which keeps internal state.
    //
    // BSD-derived getopt() functions require that optind be set to 1
    // in order to reset getopt() state.  This used to be generally
    // accepted way of resetting getopt().  However, glibc's getopt()
    // has additional getopt() state beyond optind, and requires that
    // optind be set to zero to reset its state.  So the unfortunate
    // state of affairs is that BSD-derived versions of getopt()
    // misbehave if optind is set to 0 in order to reset getopt(), and
    // glibc's getopt() will core dump if optind is set 1 in order to
    // reset getopt().
    //
    // More modern versions of BSD require that optreset be set to 1
    // in order to reset getopt().  Sigh.  Standards, anyone?
#ifdef __GLIBC__
    optind = 0;
#else /* BSD style */
    optind = 1;
    optreset = 1; 
#endif

    while ((c = getopt_long(argc, argv, ":", _long_options, &option_index)) 
    	   != -1) {
	if (c == '?') {
	    printf("%s: unknown option '%s'.  Usage:\n",
		   basename(argv[0]), argv[optind - 1]);
	    printf("%s ", basename(argv[0]));
	    shortUsage();
	    // EYE - throw an error?
	    return false;
	} else if (c == ':') {
	    // Missing option argument.
	    //
	    // Unfortunately, getopt_long() doesn't return a valid
	    // option_index for options that are missing arguments
	    // (why?).  Consequently, I can't give an error message
	    // specific to the option in question.  Instead, I just
	    // blurt out a general usage message.
	    printf("%s: option '%s' missing argument.  Options:\n",
		   basename(argv[0]), argv[optind - 1]);
	    usage();
	    return false;
	} else if (c == 0) {
	    Pref *e = Pref::options()[option_index];
	    if (!e->parse(optarg, source)) {
		printf("%s: option '--%s' given bad argument ('%s').  "
		       "Usage:\n", 
		       basename(argv[0]), e->name(), optarg);
		usage(option_index);
		return false;
	    }
	}
    }
    SGPath p;
    while (optind < argc) {
	p.set(argv[optind++]);
	flightFiles.push_back(p);
    }

    return true;
}

// Explicit instantiations of TypedPref and TypedPrefVector, required
// (at least by GCC) so that we can put the implementation of
// TypedPref into a .cxx file, rather than a .hxx file.  This solution
// is not ideal, since we need to know about all the different
// instantiations used in project, but that's not a huge deal since we
// only use these classes internally.
template class TypedPref<int>;
template class TypedPref<unsigned int>;
template class TypedPref<float>;
template class TypedPref<SGPath>;
template class TypedPref<string>;
template class TypedPref<char>;
template class TypedPref<Bool>;
template class TypedPref<Geometry>;
template class TypedPref<LightPosition>;
template class TypedPref<TileMapper::ImageType>;
template class TypedPref<SerialConnection>;
template class TypedPrefVector<int>;
template class TypedPrefVector<SerialConnection>;
