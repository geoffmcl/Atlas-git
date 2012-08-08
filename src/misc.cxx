/*-------------------------------------------------------------------------
  misc.cxx

  Written by Brian Schack

  Copyright (C) 2009 - 2012 Brian Schack

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
#include "misc.hxx"

// C++ system files
#include <cassert>
#include <limits>

// Other libraries' include files
#include <simgear/magvar/magvar.hxx>
#include <simgear/math/SGMath.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/timing/sg_time.hxx>
#ifdef _MSC_VER
#  include <simgear/math/SGMisc.hxx>
#  define LROUND(a) SGMisc<double>::round(a)
#else
#  define LROUND(d) lround(d)
#endif

//////////////////////////////////////////////////////////////////////
// atlasSphere
//////////////////////////////////////////////////////////////////////

atlasSphere::atlasSphere()
{
    empty();
}

void atlasSphere::extendBy(double lat, double lon)
{
    sgdVec3 v;
    sgGeodToCart(lat * SGD_DEGREES_TO_RADIANS, 
		 lon * SGD_DEGREES_TO_RADIANS, 0.0, v);

    sgdSphere::extend(v);
}

//////////////////////////////////////////////////////////////////////
// atlasFntTexFont
//////////////////////////////////////////////////////////////////////

atlasFntTexFont::atlasFntTexFont(): 
    _bound(false), _padding(0.1), _ascent(-1.0), _descent(1.0)
{
}

int atlasFntTexFont::load(const char *fname, GLenum mag, GLenum min)
{
    // EYE - get padding from file name
    int result = fntTexFont::load(fname, mag, min);

    if (result == TRUE) {
	_calcAscentDescent();
    }

    return result;
}

// The height of the tallest character above the baseline, in what I
// call "normalized points", which just means a point size of 1.0.
float atlasFntTexFont::ascent()
{
    return _ascent;
}

// The depth of the deepest character below the baseline, in
// "normalized points".  Note that descent is normally negative.
float atlasFntTexFont::descent()
{
    return _descent;
}

// Get the bounding box for the given text, adjusting for the padding
// added around characters by afm2txf.
void atlasFntTexFont::getBBox(const char *s, float pointsize, float italic,
			      float *left, float *right,
			      float *bot, float *top)
{
    fntTexFont::getBBox(s, pointsize, italic, left, right, bot, top);

    // Callers to getBBox don't necessarily care about all font
    // metrics, so we have to be careful about what we adjust.
    if (left) {
	*left += _padding * pointsize;
    }
    if (right) {
	*right -= _padding * pointsize;
    }
    if (bot) {
	*bot += _padding * pointsize;
    }
    if (top) {
	*top -= _padding * pointsize;
    }
}

void atlasFntTexFont::begin()
{
    fntTexFont::begin();
    // EYE - pushAttrib?
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    _bound = true;
}

void atlasFntTexFont::end()
{
    fntTexFont::end();
    glDisable(GL_TEXTURE_2D);
    _bound = false;
}

void atlasFntTexFont::puts(sgVec3 curpos, float pointsize, float italic, 
			   const char *s)
{
    if (_bound) {
	fntTexFont::puts(curpos, pointsize, italic, s);
    } else {
	begin();
	fntTexFont::puts(curpos, pointsize, italic, s);
	end();
    }
}

void atlasFntTexFont::putch(sgVec3 curpos, float pointsize, float italic, 
			    char c)
{
    if (_bound) {
	fntTexFont::putch(curpos, pointsize, italic, c);
    } else {
	begin();
	fntTexFont::putch(curpos, pointsize, italic, c);
	end();
    }
}

// Figures out the ascent and descent of the font by looking at a
// bunch of characters with big ascenders and big descenders.  We then
// normalize them so that they add to 1.0.  Why?  Well, it's a long
// story.
//
// First, in PLIB, the advance for each line of text (ie, the line
// height) is 1.0 (ie, the font's point size).  
//
// Is this right?  Well, according to FontForge, "The sum of the
// ascent and descent is the size of the font.  The point size of a
// piece of metal type was determined by this value [...] By
// convention in postscript the sum of the ascent and descent is 1000
// units [...]".  Apple's ATSUI documentation agrees, stating "The sum
// of ascent plus descent marks the line height of a font".  This
// seems to mean that the distance between the ascent and descent
// should equal 1.0, and that there is no extra "line gap".  This
// agrees with PLIB behaviour.
//
//   http://edt1023.sayya.org/fontforge/overview.html
//   http://developer.apple.com/documentation/Carbon/Conceptual/ATSUI_Concepts/atsui.pdf
//
// Other sources (FreeType, and Apple's Cocoa Font Handling
// documentation) suggest otherwise, stating that line height is
// ascent minus descent PLUS line gap.
//
//   http://freetype.sourceforge.net/freetype2/docs/glyphs/glyphs-3.html
//   http://developer.apple.com/documentation/Cocoa/Conceptual/FontHandling/FontHandling.pdf
//
// I'm choosing to go with PLIB: There is no line gap, line spacing is
// equal to the distance between ascent and descent, and this distance
// should equal 1.0.
void atlasFntTexFont::_calcAscentDescent()
{
    float left, right, normalize;

    getBBox("AQTUgjpqy", 1.0, 0.0, &left, &right, &_descent, &_ascent);

    // The above string may not contain the tallest ascenders and
    // lowest descenders, so we normalize the difference to equal 1.0.
    // It's an approximation, but should be close enough.
    normalize = 1.0 / (_ascent - _descent);
    _ascent *= normalize;
    _descent *= normalize;
}

//////////////////////////////////////////////////////////////////////
// atlasFntRenderer
//////////////////////////////////////////////////////////////////////

void atlasFntRenderer::putch(char c)
{
    glPushAttrib(GL_POLYGON_BIT); {
	glFrontFace(GL_CW);
	fntRenderer::putch(c);
    }
    glPopAttrib();
}

void atlasFntRenderer::puts(const char *s)
{
    glPushAttrib(GL_POLYGON_BIT); {
	glFrontFace(GL_CW);
	fntRenderer::puts(s);
    }
    glPopAttrib();
}

//////////////////////////////////////////////////////////////////////
// gzGetLine
//////////////////////////////////////////////////////////////////////

// Reads a line from a gzipped file.
bool gzGetLine(const gzFile& f, char **linePtr)
{
    static char *buf = NULL;
    static int length = 0;
    const int chunkSize = 1024;

    if (gzeof(f)) {
	*linePtr = NULL;
	return false;
    }

    if (length == 0) {
	// Allocate the initial buffer.
	length = chunkSize;
	buf = (char *)malloc(length * sizeof(char));
	assert(buf != NULL);
    }

    // This will be true when we've read an entire line.
    bool eoln = false;
    // We ask gzgets() to read to the buffer at this point.  At first
    // this is the same as buf, but if we extend the buffer to do
    // extra reads, readPoint will move along the buffer.
    char *readPoint = buf;
    // This is the maximum gzgets() can read.  At first this is the
    // entire buffer, but if we need to extend the buffer, this will
    // be the size of the extension.
    unsigned int toRead = length;

    // First read as much of the line as we can.
    // EYE - check return code, use gzerror() to report error.
    if (gzgets(f, readPoint, toRead) == Z_NULL) {
	int errnum;
	fprintf(stderr, "gzGetLine error: %s\n", gzerror(f, &errnum));
	return false;
    }
    do {
	// Now see if we've hit the end of the line.
	unsigned int crlf = strcspn(readPoint, "\n\r");
	if (crlf == toRead - 1) {
	    // No CR/LF - increase the buffer size.
	    length += chunkSize;
	    buf = (char *)realloc(buf, length * sizeof(char));
	    assert(buf != NULL);

	    // Read some more onto the end of our current string.
	    // Note that we have to recalculate readPoint because
	    // realloc may have moved buf (in other words, we can't
	    // just add chunkSize to readPoint).
	    readPoint = buf + length - chunkSize;
	    toRead = chunkSize;
	    if (gzgets(f, readPoint, toRead) == Z_NULL) {
		int errnum;
		fprintf(stderr, "gzGetLine error: %s\n", gzerror(f, &errnum));
		return false;
	    }
	} else {
	    readPoint[crlf] = '\0';
	    eoln = true;
	}
    } while (!eoln);

    *linePtr = buf;

    return true;
}

//////////////////////////////////////////////////////////////////////
// toMorse
//////////////////////////////////////////////////////////////////////

// Converts a character to a morse string (of periods and hyphens).
const char *toMorse(char c)
{
    static const char *charMap[] = {
	".-",
	"-...",
	"-.-.",
	"-..",
	".",
	"..-.",
	"--.",
	"....",
	"..",
	".---",
	"-.-",
	".-..",
	"--",
	"-.",
	"---",
	".--.",
	"--.-",
	".-.",
	"...",
	"-",
	"..-",
	"...-",
	".--",
	"-..-",
	"-.--",
	"--.."};

    static const char *digitMap[] = {
	"-----",
	".----",
	"..---",
	"...--",
	"....-",
	".....",
	"-....",
	"--...",
	"---..",
	"----."};

    c = tolower(c);
    if ((c >= 'a') && (c <= 'z')) {
	return charMap[c - 'a'];
    } else if ((c >= '0') && (c <= '9')) {
	return digitMap[c - '0'];
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////
// lastToken
//////////////////////////////////////////////////////////////////////

char *lastToken(char *str, char *from)
{
    if (from == NULL) {
	from = str + strlen(str);
    }

    // Skip past any trailing whitespace.
    from--;
    while ((from != str) && (strspn(from, " \t") > 0)) {
	from--;
    }

    // If we're at the head of the string already, return failure.
    if (from == str) {
	return NULL;
    }

    // Now skip past all non-whitespace.
    while ((from != str) && (strspn(from, " \t") == 0)) {
	from--;
    }
    if (from != str) {
	from++;
    }

    return from;
}

//////////////////////////////////////////////////////////////////////
// magneticVariation
//////////////////////////////////////////////////////////////////////

double magneticVariation(double lat, double lon, double elev)
{
    SGTime t1;

    lon *= SGD_DEGREES_TO_RADIANS;
    lat *= SGD_DEGREES_TO_RADIANS;
    t1.update(lon, lat, 0, 0);

    return sgGetMagVar(lon, lat, elev, t1.getJD()) * SGD_RADIANS_TO_DEGREES;
}

//////////////////////////////////////////////////////////////////////
// RaySphere
//////////////////////////////////////////////////////////////////////

bool RaySphere(SGVec3<double> p1, SGVec3<double> p2, 
	       SGVec3<double> sc, double r,
	       double *mu1, double *mu2)
{
    double a, b, c;
    double bb4ac;
    SGVec3<double> dp;

    dp = p2 - p1;

    a = dot(dp, dp);
    b = 2 * dot(dp, (p1 - sc));
    c = dot(sc, sc) + dot(p1, p1) - 2 * dot(sc, p1) - (r * r);

    bb4ac = b * b - 4 * a * c;
    if ((fabs(a) < std::numeric_limits<double>::epsilon()) || (bb4ac < 0)) {
	*mu1 = 0;
	*mu2 = 0;

	return false;
    }

    *mu1 = (-b + sqrt(bb4ac)) / (2 * a);
    *mu2 = (-b - sqrt(bb4ac)) / (2 * a);

    return true;
}

//////////////////////////////////////////////////////////////////////
// normalizeHeading
//////////////////////////////////////////////////////////////////////

double normalizeHeading(double hdg, bool lowerInclusive, double max)
{
    double result;

    if (hdg >= 0.0) {
	result = fmod(hdg, 360.0);
    } else {
	result = fmod(fmod(hdg, 360.0) + 360.0, 360.0);
    }

    if (!lowerInclusive && (result == 0.0)) {
	result = 360.0;
    }

    if (result > max) {
	result = 360.0 - result;
    }

    return result;
}

//////////////////////////////////////////////////////////////////////
// AtlasString
//////////////////////////////////////////////////////////////////////

AtlasString::AtlasString(): _buf(0), _size(0), _strlen(0)
{
    _size = _increment;
    _buf = (char *)malloc(_size * sizeof(char));
    clear();
}

AtlasString::AtlasString(const char *str): _buf(0), _size(0), _strlen(0)
{
    // EYE - how can I put this common code in one constructor?
    _size = _increment;
    _buf = (char *)malloc(_size * sizeof(char));
    this->printf("%s", str);
}

AtlasString::~AtlasString()
{
    if (_buf) {
	free(_buf);
    }
}

void AtlasString::clear()
{
    _strlen = 0;
    _buf[0] = '\0';
}

const char *AtlasString::printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    clear();
    const char *result = _appendf(fmt, ap);
    va_end(ap);

    return result;
}

const char *AtlasString::appendf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const char *result = _appendf(fmt, ap);
    va_end(ap);

    return result;
}

const char *AtlasString::_appendf(const char *fmt, va_list ap)
{
#ifdef _MSC_VER
    // Window's verion of vsnprintf() returns -1 when the string won't
    // fit, so we need to keep trying with larger buffers until it
    // does.
    int newLen;
    while ((newLen = vsnprintf_s(_buf + _strlen, _size - _strlen,
				 _size - _strlen - 1, fmt, ap)) < 0) {
        _size += _increment;
	_buf = (char *)realloc(_buf, _size);
	assert(_buf != NULL);
    }
#else
    size_t newLen;
    va_list ap_copy;
    va_copy(ap_copy, ap);

    newLen = vsnprintf(_buf + _strlen, _size - _strlen, fmt, ap);
    if ((newLen + 1) > (_size - _strlen)) {
	// This just finds the next multiple of _increment greater
	// than the size we need.  Perhaps nicer would be to find the
	// next power of 2?
	_size = (_strlen + newLen + 1 + _increment) / _increment * _increment;
	_buf = (char *)realloc(_buf, _size);
	assert(_buf != NULL);
	vsnprintf(_buf + _strlen, _size - _strlen, fmt, ap_copy);
    }
    va_end(ap_copy);
#endif
    _strlen += newLen;

    return _buf;
}

// // next-largest power of 2 (nlpo2)
// //
// // From 
// //
// // http://aggregate.org/MAGIC
// //
// // Note: this assumes a 32 bit value.
// unsigned int nlpo2(register unsigned int x)
// {
//     x |= (x >> 1);
//     x |= (x >> 2);
//     x |= (x >> 4);
//     x |= (x >> 8);
//     x |= (x >> 16);
//     return (x + 1);
// }

//////////////////////////////////////////////////////////////////////
// formatFrequency
//////////////////////////////////////////////////////////////////////

const char *formatFrequency(int frequency)
{
    // The maximum allowable NDB frequency is 1750 kHz (according to
    // Wikipedia).
    const int maxNDBFrequency = 1750;
    static AtlasString str;
    if (frequency < maxNDBFrequency) {
	str.printf("%d", frequency);
    } else if ((frequency % 1000) == 0) {
	str.printf("%.1f", frequency / 1000.0);
    } else {
	str.printf("%g", frequency / 1000.0);
    }
    return str.str();
}

//////////////////////////////////////////////////////////////////////
// formatAngle
//////////////////////////////////////////////////////////////////////

const char *formatAngle(double degrees, bool dms)
{
    static AtlasString str;
    degrees = fabs(degrees);

    if (dms) {
	// Round degrees to the nearest hundredth of a second, then
	// chop it up into minutes and seconds.
	double degs = LROUND(degrees * 3600.0 * 100.0) / (3600.0 * 100.0);
	double mins = modf(degs, &degs) * 60.0;
	double secs = modf(mins, &mins) * 60.0;
	str.printf("%02.0f%c %02.0f' %05.2f\"", degs, degreeSymbol, mins, secs);
    } else {
	str.printf("%.8f%c", degrees, degreeSymbol);
    }

    return str.str();
}

//////////////////////////////////////////////////////////////////////
// MEF
//////////////////////////////////////////////////////////////////////

int MEF(double elevation)
{
    return ceil((elevation + 100.0 + 200.0) / 100.0) * 100;
}

//////////////////////////////////////////////////////////////////////
// atlasGeodToCart, atlasCartToGeod
//////////////////////////////////////////////////////////////////////

void atlasGeodToCart(double lat, double lon, double alt, double *cart)
{
    sgGeodToCart(lat * SGD_DEGREES_TO_RADIANS,
		 lon * SGD_DEGREES_TO_RADIANS,
		 alt,
		 cart);
}

void atlasCartToGeod(double *cart, double *lat, double *lon, double *alt)
{
    sgCartToGeod(cart, lat, lon, alt);
    *lat *= SGD_RADIANS_TO_DEGREES;
    *lon *= SGD_RADIANS_TO_DEGREES;
}

//////////////////////////////////////////////////////////////////////
// AtlasDialog
//////////////////////////////////////////////////////////////////////

// Create a generic dialog box, with the given buttons.  The dialog
// will have the given text, and call 'cb' when done.  The left
// button's default integer value is set to LEFT, the middle's to
// MIDDLE, and the right's to RIGHT.  An empty or null string will
// have no button.  The button which was pressed will be passed to the
// callback, so you can check its default integer value to find out
// which one was pressed.  The callback must delete the dialog (using
// puDeleteObject) and set it to NULL.  All of the buttons will be
// assigned the given user data.

// EYE - use a vararg instead?
AtlasDialog::AtlasDialog(const char *msg, const char *leftLabel, 
			 const char *middleLabel, const char *rightLabel, 
			 puCallback cb, void *data):
    puDialogBox(0, 0), _cb(cb)
{
    // EYE - magic numbers (and many others later).
    const int dialogWidth = 300;
    const int dialogHeight = 100;

    // _x is used to place buttons.  When a new button is created, we
    // expect it to be at the left edge of the previously placed
    // button.
    _x = dialogWidth;

    // Place the dialog box in the centre of the window.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLint windowWidth = viewport[2];
    GLint windowHeight = viewport[3];
    setPosition(windowWidth / 2 - dialogWidth / 2,
		windowHeight / 2 - dialogHeight / 2);

    // Create a frame with the message.
    new puFrame(0, 0, dialogWidth, dialogHeight); {
	_label = new puText(10, dialogHeight - 30);

	// I copy the string so that the caller is free to do whatever
	// he/she wants with the string passed in.  We free this copy
	// in our destructor.
	_label->setLabel(strdup(msg));

	// Create the buttons from right to left.  Note that I don't
	// check if the strings are NULL - strcmp() seems to be smart
	// enough to handle that.
	if (strcmp(rightLabel, "") != 0) {
	    puOneShot *right = _makeButton(rightLabel, RIGHT, data);
	}
	if (strcmp(middleLabel, "") != 0) {
	    puOneShot *middle = _makeButton(middleLabel, MIDDLE, data);
	}
	if (strcmp(leftLabel, "") != 0) {
	    puOneShot *left = _makeButton(leftLabel, LEFT, data);
	}
    }
    close();
    reveal();
}

AtlasDialog::~AtlasDialog()
{
    free((void *)_label->getLabel());
}

puOneShot *AtlasDialog::_makeButton(const char *label, CallbackButton pos,
				    void *data)
{
    // We'll adjust its position later.
    puOneShot *button = new puOneShot(0, 0, label);
    button->setCallback(_cb);
    button->setUserData(data);
    button->setDefaultValue(pos);

    // EYE - magic number
    const int spacing = 10;
    int width, height;
    button->getSize(&width, &height);
    _x -= spacing + width;
    button->setPosition(_x, spacing);

    return button;
}
