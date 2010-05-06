/*-------------------------------------------------------------------------
  misc.hxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

  As the name suggests, miscellaneous classes an functions that don't
  really belong anywhere else.

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

#ifndef _MISC_H_
#define _MISC_H_

#include <zlib.h>

#include <plib/sg.h>
#include <plib/fnt.h>
#include <simgear/math/sg_geodesy.hxx>
#include <simgear/misc/sg_path.hxx>

// atlasSphere - a small extension to PLIB's sgdSphere
//
// I wrote this for 2 reasons:
//
// (a) sgdSphere doesn't initialize itself to empty (although sgSphere
//     does - presumably this is a bug).
//
// (b) It's convenient to be able to extend a bounding sphere by a
//     point defined by a latitude and longitude.
//
class atlasSphere: public sgdSphere {
  public:
    atlasSphere();

    // It would be nice to call this 'extend', like all the other
    // extend routines, but C++, in its infinite wisdom, won't let us.
    void extendBy(double lat, double lon);
};

// atlasFntTexFont - a small extension to PLIB's fntTexFont
//
// PLIB's fntTexFont uses textures to render fonts.  When strings are
// rendered in a begin()/end() pair, or a single string is rendered in
// a puts() call without begin()/end(), it enables textures, and then
// renders the string.  But, it *doesn't* disable textures, which one
// would expect.
//
// This class disables textures when end() is called, or when a single
// puts() is called.
//
class atlasFntTexFont: public fntTexFont {
  public:
    atlasFntTexFont();

    int load(const char *fname, 
	     GLenum mag = GL_NEAREST, 
	     GLenum min = GL_LINEAR_MIPMAP_LINEAR);

    void getBBox(const char *s, float pointsize, float italic,
                 float *left, float *right,
                 float *bot, float *top);
    float ascent();
    float descent();

    void begin();
    void end();
    void puts(sgVec3 curpos, float pointsize, float italic, const char *s);
    void putch(sgVec3 curpos, float pointsize, float italic, char c);
    
  protected:
    bool _bound;
    float _padding, _ascent, _descent;

    void _calcAscentDescent();
};

// atlasFntRenderer - a small extension to PLIB's fntRenderer
//
// PLIB (1.8.5) renders the string using a triangle strip.
// Unfortunately, it is specified with a clockwise winding, which
// means that applications that use backface culling (eg, us) will see
// nothing unless we reverse our definition of front and back.
class atlasFntRenderer: public fntRenderer {
  public:
    void putch(char c);
    void puts(const char *s);
};

// Atlas' special font has a degree symbol.  It can be put into a
// string via printf("%C", degreeSymbol).
const unsigned char degreeSymbol = 176;

// A slightly modified version of zlib's gzgets().  This one *always*
// gets the entire line, and *never* includes the trailing CR/LF.
//
// Returns true if it got a line, false otherwise (generally this
// means the file was at EOF).
//
// Note: callers are given a pointer to the string.  This string is
// modified on each call to gzGetLine(), so you should copy it if you
// need a permanent copy.
bool gzGetLine(const gzFile& f, char **linePtr);

// Converts a character (which must be alphanumeric), to a morse
// string, consisting of periods ('.') and hyphens ('-').  Upper- and
// lower-case characters convert to the same morse code.  If a
// non-alphanumeric character is given, NULL is returned.
const char *toMorse(char c);

// Scans backward through a string for the last token (something
// delimited by blanks and/or tabs).  By default, it starts from the
// end of the string, but you can tell it to start from some place
// within the string (eg, the result of the last call to lastToken()).
//
// Returns a pointer to the start of the token, or NULL if there are
// none.  This could happen if the string is all white-space, or if
// from == str.
//
// lastToken() doesn't modify the string.  This means that the token
// returned will "contain" the entire remainder of the string (which
// may consist of many tokens).  The caller is responsible for
// chopping the string up if that is desired.
char *lastToken(char *str, char *from = NULL);

// Returns the *current* magnetic variation at the given location.
// Angles are in degrees, elevation in metres.  It should be
// subtracted from a true heading to get the correct magnetic heading.
double magneticVariation(double lat, double lon, double elev = 0.0);

// Calculates the intersection of a ray and a sphere.  The line
// segment is defined from p1 to p2.  The sphere is given by a centre,
// sc, and a radius, r.  There are potentially two points of
// intersection, p, given by
//
//   p = p1 + mu1 (p2 - p1)
//   p = p1 + mu2 (p2 - p1)
//
// Returns false if the ray doesn't intersect the sphere.
//
// Taken from:
//
// http://local.wasp.uwa.edu.au/~pbourke/geometry/sphereline/raysphere.c
bool RaySphere(SGVec3<double> p1, SGVec3<double> p2, 
	       SGVec3<double> sc, double r,
	       double *mu1, double *mu2);

// Normalizes hdg to be 0.0 <= hdg < 360.0.  This is used to make sure
// headings are always in the range [0, 360).  If lowerInclusive is
// false, then it puts headings in the range 0.0 < hdg <= 360.0 (this
// is typically needed when printing headings, since north is written
// '360', not '0').
//
// If lowerInclusive is false and you want to convert results to
// integers, be sure to round the argument *before* passing it to
// normalizeHeading():
//
//   int newHdg = normalizeHeading(rint(oldHdg), false)
//
// For example, if oldHdg is 0.1, then without the round,
// normalizeHeading() would return 0.1, which would then be rounded to
// 0, rather than 360.  Casting first means that we pass 0.0, which
// results in a return value of 360.0, the result we want.
//
// If max is set, then the range becomes [360 - max - 0, max) (or (360
// - max - 0, max] if lowerInclusive is false).  Probably the only
// useful value for max (other than 360.0) is 180.0.  This can be used
// for, say, checking the difference between two headings:
//
//   diff = fabs(normalizeHeading(h1 - h2, true, 180.0));
//
// Now diff will be the absolute difference between the two headings.
double normalizeHeading(double hdg, bool lowerInclusive = true, 
			double max = 360.0);

// AtlasString - a nice safe way to create formatted strings
//
// I seem to spend a lot of time creating short-term strings to format
// output.  C++ ostringstreams are the obvious solution, but I've
// never been satisfied with their format (printf() seems clearer than
// separating stuff with '<<'), the fact that I can only get at the
// internal string via str() (which entails a copy), and the inferior
// (I think) formatting options.  Thus this class.
//
// This class gives printf()-like formatting (in fact, it just calls
// sprintf() internally), but you don't have to worry about allocating
// or overflowing strings.  AtlasString maintains an internal
// character array and, makes sure that it is big enough to handle
// formatting requests.
//
// The printf() method sets the AtlasString to the given formatted
// data.  Any previous string is overwritten.  The appendf() method
// will add the given formatted data to the existing string.

class AtlasString {
 public:
    AtlasString();
    AtlasString(const char *str);
    ~AtlasString();
    
    const char *str() { return _buf; }

    void clear();
    const char *printf(const char *fmt, ...);
    const char *appendf(const char *fmt, ...);

  protected:
    char *_buf;
    size_t _size, _strlen;
    static const size_t _increment = 16;

    const char *_appendf(const char *fmt, va_list ap);
};

// I've declared a single global AtlasString, which can be used for
// temporary work.  Save any results you want to keep!
extern AtlasString globalString;

// I use this for printing frequencies.  VHF frequencies on charts are
// printed without trailing zeroes, except when that would mean
// printing nothing after the decimal, in which case a zero is added
// (ie, 126.0, 126.1, 126.05, 126.775).
//
// This routine takes a frequency in *kHz* (eg, 126,000), and returns
// a properly formatted string.  Note - the string is static and will
// be changed on the next call to this function, so you should copy it
// if you need to save it.
//
// This function can be used for printing LF/MF (ie, NDB) frequencies
// as well.
const char *formatFrequency(int freq);

// Formats angles.  If dms is true, then degrees is formated in
// degrees/minutes/seconds format (dd mm' ss.ss", seconds to 2 decimal
// places).  If false, then it is formatted in decimal degrees
// (dd.dddddddd, to eight decimal places).  Its sign is ignored.
//
// Like formatFrequency, you should copy the string if you need to
// hang on to it.
const char *formatAngle(double degrees, bool dms);

// Calculates the minimum elevation figure (MEF) for some area, given
// the highest elevation (in feet) in that area.
//
// According to "Introduction to VFR Chart Symbols", the MEF is
// calculated thusly:
//
// (1) Determine the highest elevation.
//
// (2) Add the possible vertical error (100' or half the contour
//     interval, whichever is highest).  In our case, I think the
//     elevations are fairly accurate, so we use 100'.
//
// (3) Add a 200' allowance for man-made obstacles.
//
// (4) Round up to the next higher 100 foot level.
//
// So, if the highest elevation is 14,112 feet, the MEF is 14,500.
int MEF(double elevation);

// A wrapper for sgGeodToCart that takes latitude and longitude in
// degrees (alt is in metres, as with sgGeodToCart).
void atlasGeodToCart(double lat, double lon, double alt, double *cart);
// Ditto, but for sgCartToGeod.
void atlasCartToGeod(double *cart, double *lat, double *lon, double *alt);

#endif
