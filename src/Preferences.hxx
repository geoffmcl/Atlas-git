/*-------------------------------------------------------------------------
  Preferences.hxx

  Written by Brian Schack, started August 2007.

  Copyright (C) 2007 - 2012 Brian Schack

  Handles command-line options, as well as the Atlas preferences file.

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

#ifndef _PREFERENCES_H_
#define _PREFERENCES_H_

#include <getopt.h>

#include <vector>
#include <bitset>
#include <iostream>

#include <simgear/misc/sg_path.hxx> // SGPath

#include "TileMapper.hxx"	// ImageType

// The current official .atlasrc file version, used when saving a
// preferences file.  Currently this is unused, as we don't save
// preference files.
#define ATLASRC_VERSION 1

//////////////////////////////////////////////////////////////////////
// Pref
//
// A base class for all preferences.  It has a few basic goals:
//
// (1) Ease the use of Unix's getopt_long functionality.
//
// (2) Allow for multiple soures of a preference value.  A preference
//     can get a value from (in increasing priority) a factory
//     default, a preference file, or from the command-line.
//
// (3) Make it easy to track changes to preferences, and save
//     preferences to a file.  Only preferences with non-factory
//     default values are saved.
//
// A Pref is a virtual base class; subclasses need to implement the
// stuff that deals with actual values: isDefault(), parse(), and
// save().  There are subclasses (currently TypedPrefVector) for which
// isDefault() doesn't make sense.
//
// Note: We use getopt_long to make our option-parsing life easier.  A
// nicer, but non-standard, solution would be argtable.  It can be
// found at:
//
//   http://argtable.sourceforge.net/
class Pref {
  public:
    enum PrefSource {CMD_LINE, PREF_FILE, FACTORY, SOURCE_COUNT};

    // Each time a new Pref is created, it is added to the __options
    // vector.  This is used to initialize getopt_long.
    static const std::vector<Pref *> &options() { return __options; }

    Pref(const char *name, int has_arg, const char *shortHelp, 
	 const char *longHelp);
    virtual ~Pref();

    // True if we need to save this preference to .atlasrc.
    virtual bool isDirty() { return _dirty; }
    // True if our PREF_FILE value is the same as our FACTORY value.
    virtual bool isDefault() = 0;

    // Reset to factory defaults.  Specifically, remove the PREF_FILE
    // and CMD_LINE entries.
    void reset();

    // Parse the argument to an option (str will be NULL if there is
    // no argument).
    virtual bool parse(const char *str, PrefSource s) = 0;
    // If we need to save something (ie, if we don't have our default
    // value), this outputs our long help to the stream and sets dirty
    // to false.  It's up to subclasses to save the actual option to
    // the stream.  Returns true if we actually needed to save
    // something.
    virtual bool save(std::ostream& ostr) = 0;

    // Accessors for our getopt_long parameters.
    const struct option &option() const { return _option; }
    const char *name() const { return _option.name; }
    int tag() const { return _option.val; }

    const char *shortHelp() const { return _shortHelp; }
    const char *longHelp() const { return _longHelp; }

  protected:
    static std::vector<Pref *> __options;
    static int __val;

    std::bitset<SOURCE_COUNT> _defined;

    bool _dirty;

    struct option _option;
    char *_shortHelp;
    char *_longHelp;
};

// A NoArgPref is a Pref with no argument, corresponding to the
// getopt_long parameter type 'no_argument' .  Every time the argument
// is encountered on the command line, a counter ('seen') is
// incremented.
class NoArgPref: public Pref {
  public:
    NoArgPref(const char *name, const char *longHelp);

    // EYE - does this make sense?  What really is its default value?
    // How does this fit in with the _defined array?
    bool isDefault() { return (_seen == 0); }
    bool parse(const char *str, PrefSource s) { _seen++; return true; }
    bool save(std::ostream& ostr);

    // Returns the number of times the argument has been seen.
    int seen() { return _seen; }

  protected:
    int _seen;
};

// A TypedPref is, not surprisingly, a Pref with a type.  Whatever
// type you choose, it must have a copy constructor, and the << and >>
// operators.
#include <sstream>
template <class T>
class TypedPref: public Pref {
  public:
    // If the preference has a required argument, use this
    // constructor.
    TypedPref(const char *name, const char *shortHelp, const char *longHelp);
    // If the preference has an optional argument, use this
    // constructor.  The 'optionalValue' parameter tells TypedPref
    // what value to use when no option value is present (eg,
    // '--autocentre-mode').
    TypedPref(const char *name, const char *optionalValue,
	      const char *shortHelp, const char *longHelp);
    ~TypedPref();


    // Gets the preference's value from the "highest" source with a
    // defined value, starting from the named source.
    T get(PrefSource s = CMD_LINE) const;
    // Sets the preference's value for the given source.  Sets _dirty
    // to true if we've changed the PREF_FILE value.

    // EYE - pass T as a reference ('T& v', or 'const T& v')?
    void set(T v, PrefSource s = PREF_FILE);

    // EYE - a bit of scary C++ hackery.  Given:
    //
    // TypedPref<int> p;
    // int i;
    //
    // This operator allows users of this class to say:
    //
    // i = p;
    //
    // instead of:
    //
    // i = p.get();
    //
    // However, this will only work if PrefSource == CMD_LINE (the
    // default).
    operator const T() { return get(); }
    // EYE - more C++ hackery.  Given the same declarations as above,
    // this operator allows users of this class to say:
    //
    // p = i;
    //
    // instead of:
    //
    // p.set(i);
    // 
    // EYE - check against self-assignment?
    //
    // EYE - I'm not sure if this is a particularly good way to
    // implement this, but it works and I don't get incomprehensible
    // compiler errors.
    T operator=(T right) { set(right); return right; }

    // True if the value we want to save to the preferences file is
    // the same as the factory default.
    bool isDefault() { return get(PREF_FILE) == get(FACTORY); }

    // Parse the string to extract a value, place the result in
    // _values[s].
    bool parse(const char *str, PrefSource s);
    bool save(std::ostream &ostr);

  protected:
    std::vector<T> _values;
    char *_optionalValue;
};

// This is used for preferences that can appear several times on the
// command line, and for which we need to remember all the values
// (examples are --udp and --serial).  The accumulated values are kept
// in the prefs() vector.
//
// Actual parsing is done via a single TypedPref that can be accessed
// via pref().  It's a normal TypedPref - it has a default value, a
// preference file file, and a current value, but the meanings are
// slightly different.  The default value is the value given to an
// option when no argument is specified, it that is allowed (eg,
// --udp, as opposed to --udp=5501).  It should be set once and never
// changed.  It contains the last preference parsed, but there's no
// real need to get() or set() it, except to get the default value.
//
// A TypedPrefVector is dirty if the _prefs vector has changed to a
// non-default value (ie, setPrefs() has been called with a non-empty
// vector).  Its default value is an empty _prefs vector.  As well,
// there is no notion of reset(), so it is not included.
template <class T>
class TypedPrefVector: public TypedPref<T> {
  public:
    // See TypedPref() for the difference between these two
    // constructors.
    TypedPrefVector(const char *name, const char *shortHelp, 
		    const char *longHelp);
    TypedPrefVector(const char *name, 
		    const char *optionalValue,
		    const char *shortHelp, 
		    const char *longHelp);

    const std::vector<T>& prefs() const { return _prefs; }
    void setPrefs(std::vector<T>& v);

    // True if setPrefs() has been called; reset when save() is
    // called.
    bool isDirty() { return _dirty; }
    // True if _prefs has nothing in it.
    bool isDefault() { return _prefs.empty(); }

    bool save(std::ostream& ostr);
  protected:
    std::vector<T> _prefs;
    bool _dirty;
};

// Most preferences can be represented by a basic C++ type (float,
// int, string, ...).  However, some can't, or need some special
// processing.  I use a namespace here because there's too great a
// chance that classes like "Geometry" will be used elsewhere.
namespace Prefs {
    class Bool {
      public:
	Bool();
	Bool(bool b);
	Bool(const Bool& b);

	// EYE - maybe we should choose a better name?
	operator const bool() { return _b; }

	int operator==(const Bool& right) const;
	int operator!=(const Bool& right) const;

      protected:
        // Note: I don't think it really matters if these are
        // protected or public.  The important thing is their
        // declaration within the namespace (see below).
	friend std::ostream& operator<<(std::ostream& ostr, const Bool& b);
	friend std::istream& operator>>(std::istream& istr, Bool& b);

	bool _b;
    };

    class Geometry {
      public:
	Geometry();
	Geometry(int w, int h);
	Geometry(const Geometry& g);

	int width() const { return _w; }
	int height() const { return _h; }

	int operator==(const Geometry& right) const;
	int operator!=(const Geometry& right) const;

      protected:
	friend std::ostream& operator<<(std::ostream& ostr, const Geometry& g);
	friend std::istream& operator>>(std::istream& istr, Geometry& g);

	int _w, _h;
    };

    class LightPosition {
      public:
	LightPosition();
	LightPosition(float azim, float elev);
	LightPosition(const LightPosition& g);

	float azimuth() const { return _azimuth; }
	float elevation() const { return _elevation; }

	int operator==(const LightPosition& right) const;
	int operator!=(const LightPosition& right) const;

      protected:
	friend std::ostream& operator<<(std::ostream& ostr, 
					const LightPosition& lp);
	friend std::istream& operator>>(std::istream& istr, LightPosition& lp);

	float _azimuth, _elevation;
    };

    class SerialConnection {
      public:
	SerialConnection();
	SerialConnection(std::string device, int baud);
	SerialConnection(const SerialConnection& g);

	const std::string& device() const { return _device; }
	int baud() const { return _baud; }

	int operator==(const SerialConnection& right) const;
	int operator!=(const SerialConnection& right) const;

      protected:
	friend std::ostream& operator<<(std::ostream& ostr, 
					const SerialConnection& sc);
	friend std::istream& operator>>(std::istream& istr, 
					SerialConnection& sc);

	std::string _device;
	int _baud;
    };

    // Why do I declare these here in addition to inside the classes?
    // Because some C++ guru said so.  Otherwise, it won't compile (or
    // has warnings).  And why is this so?  I have no idea.
    std::ostream& operator<<(std::ostream& ostr, const Bool& b);
    std::istream& operator>>(std::istream& istr, Bool& b);
    std::ostream& operator<<(std::ostream& ostr, const Geometry& g);
    std::istream& operator>>(std::istream& istr, Geometry& g);
    std::ostream& operator<<(std::ostream& ostr, const LightPosition& lp);
    std::istream& operator>>(std::istream& istr, LightPosition& lp);
    std::ostream& operator<<(std::ostream& ostr, const SerialConnection& sc);
    std::istream& operator>>(std::istream& istr, SerialConnection& sc);
}

std::ostream& operator<<(std::ostream& ostr, const TileMapper::ImageType& i);
std::istream& operator>>(std::istream& istr, TileMapper::ImageType& i);

//////////////////////////////////////////////////////////////////////
//
// Preferences
//
// The big kahuna.  This class contains many instances of our
// Pref/TypedPref/... classes, one for each command line option.  It
// allows for reading from a preferences file and from the command
// line, and saving to a preferences file.
class Preferences {
  public:
    Preferences();
    ~Preferences();

    // Prints out a usage option for the given option (which is an
    // index into _long_options), or for all options (if option is out
    // of range).
    void usage(int i = -1);
    // Similar to usage(), but only using Pref::shortHelp().
    void shortUsage(int i = -1);

    // EYE - track changes, allow undo?
    bool load(int argc, char *argv[]);
    void save(std::ostream& ostr);

    // The version read from the .atlasrc file.  If no .atlasrc file
    // was read, we return noAtlasrcFile.  If an .atlasrc file was
    // read but there was no version number, we return
    // noAtlasrcVersion.  Otherwise, we return the version number,
    // which is a positive number >= 1.
    static const int noAtlasrcFile;
    static const int noAtlasrcVersion;
    int atlasrcVersion() const { return _atlasrcVersion; }

    // True if something has changed.
    bool isDirty();

    TypedPref<float> latitude, longitude, zoom;
    TypedPref<std::string> icao;

    TypedPref<SGPath> path, fg_root, scenery_root;

    TypedPref<Prefs::Geometry> geometry;
    TypedPref<Prefs::Bool> textureFonts, softcursor;

    TypedPref<Prefs::Bool> autocentreMode;
    TypedPref<float> lineWidth;
    TypedPref<SGPath> airplaneImage;
    TypedPref<float> airplaneImageSize;

    TypedPref<float> update;
    TypedPref<int> maxTrack;
    TypedPrefVector<int> networkConnections;
    TypedPrefVector<Prefs::SerialConnection> serialConnections;

    TypedPref<Prefs::Bool> discreteContours;
    TypedPref<Prefs::Bool> contourLines, lightingOn, smoothShading;
    TypedPref<Prefs::LightPosition> lightPosition;
    TypedPref<TileMapper::ImageType> imageType;
    TypedPref<unsigned int> JPEGQuality;
    TypedPref<std::string> palette;

    NoArgPref version, help;

    std::vector<SGPath> flightFiles;

  protected:
    bool _load(int argc, char *argv[], Pref::PrefSource source);

    struct option *_long_options;
    int _atlasrcVersion;
};

#endif	// _PREFERENCES_H_
