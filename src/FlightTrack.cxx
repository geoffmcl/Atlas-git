/*-------------------------------------------------------------------------
  FlightTrack.cxx

  Written by Per Liedman, started July 2000.

  Copyright (C) 2000 Per Liedman, liedman@home.se

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  ---------------------------------------------------------------------------*/

#include <math.h>

#include <string>
#include <fstream>
#include <stdexcept>

#include "FlightTrack.hxx"

// EYE - create a common initializer?
FlightTrack::FlightTrack(const char *filePath) : 
    _max_buffer(0), _mark(-1), _input_channel(NULL), _live(false)
{
    if (!_readFlightFile(filePath)) {
	throw std::runtime_error("flight file open failure");
    }

    _file.set(filePath);

    _port = -1;
    _device = "";
    _baud = -1;

    _isNetwork = _isSerial = false;

    _versionAtLastSave = _version = 0;
}

FlightTrack::FlightTrack(int port, unsigned int max_buffer) : 
    _max_buffer(max_buffer), _mark(-1), _live(true)
{
    char *portStr;

    asprintf(&portStr, "%d", port);
    _input_channel = new SGSocket("", portStr, "udp");
    _input_channel->open(SG_IO_IN);
    free(portStr);

    _file.set("");

    _port = port;
    _device = "";
    _baud = -1;

    _isNetwork = true;
    _isSerial = false;

    _versionAtLastSave = _version = 0;
}

FlightTrack::FlightTrack(const char *device, int baud, unsigned int max_buffer) : 
    _max_buffer(max_buffer), _mark(-1), _live(true)
{
    char *baudStr;

    asprintf(&baudStr, "%d", baud);
    _input_channel = new SGSerial(device, baudStr);
    _input_channel->open(SG_IO_IN);
    free(baudStr);

    _file.set("");

    _port = -1;
    _device = device;
    _baud = baud;

    _isNetwork = false;
    _isSerial = true;

    _versionAtLastSave = _version = 0;
}

FlightTrack::~FlightTrack()
{
    clear();

    if (_input_channel) {
	_input_channel->close();
    }
}

bool FlightTrack::isAtlas()
{
    return _isAtlas;
}

bool FlightTrack::isNetwork()
{
    return _isNetwork;
}

bool FlightTrack::isSerial()
{
    return _isSerial;
}

int FlightTrack::port()
{
    return _port;
}

const char *FlightTrack::device()
{
    return _device.c_str();
}

int FlightTrack::baud()
{
    return _baud;
}

unsigned int FlightTrack::maxBufferSize()
{
    return _max_buffer;
}

void FlightTrack::clear() 
{
    while (!_track.empty()) {
	delete *(_track.begin());
	_track.pop_front();
    }
    _version++;
    _mark = -1;
}

bool FlightTrack::empty() 
{
    return _track.empty();
}

// Returns true if this is a socket- or serial-driven flight track
// which is currently accepting input.
bool FlightTrack::live() 
{
    return _live;
}

// Closes the I/O channel to further input.
void FlightTrack::detach()
{
    if (_input_channel) {
	_input_channel->close();
	_input_channel = NULL;
    }
    _live = false;
    _isNetwork = _isSerial = false;
}

// The "version" of the flight track begins at 0, and is incremented
// each time the flight track is changed.  This provides a simple way
// to check for changes.
int FlightTrack::version()
{
    return _version;
}

// Call this periodically to check for input (from a socket or serial
// device).
// EYE - auto detach upon period of inactivity (if track is non-empty)?
bool FlightTrack::checkForInput() 
{
    bool result = false;
    const int bufferSize = 512;
    char buffer[bufferSize];
    int noOfBytes;

    if (!live()) {
	return false;
    }

    // FlightGear sends us the data as 3 lines of text, all in one
    // message.  Messages are about 165 bytes long, so 512 should be
    // more than enough.  It's possible we may receive many messages
    // in one go (perhaps we sample slower than it's produced, perhaps
    // the network is slow/fast, etc), so we keep reading until
    // there's nothing.
    while ((noOfBytes = _input_channel->read(buffer, bufferSize)) > 0) {
	// If we managed to read data, then we'll assume we need to
	// add some flight data.
	FlightData tmp;
	buffer[noOfBytes] = '\0';
	if (_parse_message(buffer, &tmp)) {
	    result = true;

	    // Record point.
	    FlightData *d = new FlightData;
	    *d = tmp;

	    // EYE - I add the point unconditionally (before it was only
	    // added if the change was more than 1 arc second).
	    _addPoint(d, -1.0);
	}
    }

    return result;
}

void FlightTrack::firstPoint() 
{
    _track_pos = _track.begin();
}

FlightData *FlightTrack::getNextPoint() 
{
    if (_track_pos != _track.end()) {
	return *(_track_pos++);
    } else {
	return NULL;
    }
}

FlightData *FlightTrack::dataAtPoint(int i)
{
    if ((i < 0) || (i >= _track.size())) {
	return NULL;
    } else {
	return _track[i];
    }
}

FlightData *FlightTrack::getLastPoint() 
{
    if (_track.size() > 0) {
	return _track.back();
    } else {
	return NULL;
    }
}

FlightData *FlightTrack::getCurrentPoint() 
{
    if (_track.size() == 0) {
	return NULL;
    }

    if (live()) {
	return getLastPoint();
    } else {
	return dataAtPoint(mark());
    }
}

int FlightTrack::size()
{
    return _track.size();
}

void FlightTrack::setMark(int i)
{
    if ((i >= -1) && (i < _track.size())) {
	_mark = i;
    }
}

int FlightTrack::mark()
{
    return _mark;
}

// Returns true if the file path is not the empty string.
bool FlightTrack::hasFile()
{
    return (_file.str() != "");
}

// EYE - return as string?
const char *FlightTrack::fileName()
{
    // EYE - Work around bug in SGPath.  If a path has no path
    // separators (eg, "foo.text", as opposed to something like
    // "dir/foo.text"), file() returns "", rather than "foo.text".
    const char *name = _file.file().c_str();
    if (strcmp(name, "") == 0) {
	name = _file.c_str();
    }
    return name;

//     return _file.file().c_str();
}

const char *FlightTrack::filePath()
{
    return _file.c_str();
}

void FlightTrack::setFilePath(char *path)
{
    // EYE - check for existing name?  overwriting?
    // EYE - call this (and other accessors) from constructors?
    _file.set(path);
    _versionAtLastSave = 0;
}

// Saves the data in _track to a FlightGear-style flight file.
void FlightTrack::save()
{
    // EYE - if we do this test, we must be sure to keep it up to
    // date.
    if (hasFile() && modified()) {
	FILE *f = fopen(_file.c_str(), "w");
	if (f == NULL) {
	    // EYE - need some kind of error code?
	    return;
	}
	for (unsigned int i = 0; i < _track.size(); i++) {
	    FlightData *d = _track[i];

	    // Do the hard stuff first.
	    char *time, *date;
	    struct tm *t = gmtime(&(d->time));
	    asprintf(&time, "%02d%02d%02d", t->tm_hour, t->tm_min, t->tm_sec);
	    asprintf(&date, "%02d%02d%02d", 
		     t->tm_mday, t->tm_mon + 1, t->tm_year);

	    char *lat, *lon;
	    int degrees;
	    float minutes;
	    char c;
	    _splitAngle(d->lat, "NS", &degrees, &minutes, &c);
	    asprintf(&lat, "%02d%06.3f,%c", degrees, minutes, c);
	    _splitAngle(d->lon, "EW", &degrees, &minutes, &c);
	    asprintf(&lon, "%03d%06.3f,%c", degrees, minutes, c);

	    char *buf;
	    char checksum;

	    // $GPRMC
	    asprintf(&buf, "GPRMC,%s,A,%s,%s,%05.1f,%05.1f,%s,0.000,E",
		     time, lat, lon, d->spd, d->hdg, date);
	    checksum = _calcChecksum(buf);
	    fprintf(f, "$%s*%02X\n", buf, checksum);
	    free(buf);

	    // $GPGGA
	    asprintf(&buf, "GPGGA,%s,%s,%s,1,,,%.0f,F,,,,", 
		     time, lat, lon, d->alt);
	    checksum = _calcChecksum(buf);
	    fprintf(f, "$%s*%02X\n", buf, checksum);
	    free(buf);

	    // $PATLA
	    if (isAtlas()) {
		asprintf(&buf, "PATLA,%.2f,%.1f,%.2f,%.1f,%d",
			 d->nav1_freq / 100.0, 
			 d->nav1_rad * SG_RADIANS_TO_DEGREES, 
			 d->nav2_freq / 100.0, 
			 d->nav2_rad * SG_RADIANS_TO_DEGREES, 
			 d->adf_freq);
	    } else {
		asprintf(&buf,
			 "GPGSA,A,3,01,02,03,,05,,07,,09,,11,12,0.9,0.9,2.0");
	    }
	    checksum = _calcChecksum(buf);
	    fprintf(f, "$%s*%02X\n", buf, checksum);
	    free(buf);

	    free(time);
	    free(date);
	    free(lat);
	    free(lon);
	}

	fclose(f);

	_versionAtLastSave = _version;
    }
}

bool FlightTrack::modified()
{
    return (_versionAtLastSave < _version);
}

// Given an index of a data point, adjust the est_t_offset values for
// all points 'around' it.  This means, possibly, points before it, if
// they have the same absolute time values, as well as all points
// after it.
//
// This routine assumes that offsets before this point with different
// absolute time values are correct.
//
// Why do we need to do all this?  Because FlightGear only records
// integral time values, even though we may get many data points per
// second.  We assume that, if there many data points with identical
// time values, they fall evenly in that interval.  However, if we've
// distributed 2 points in an interval (at x and x + 0.5), then get a
// third point, we have to redistribute them (x, x + 0.33, x + 0.67).
// Ditto for removing points.
void FlightTrack::_adjustOffsetsAround(int i)
{
    FlightData *data = dataAtPoint(i);

    // Find the first point with the same absolute time value.
    FlightData *d = dataAtPoint(--i);
    while (d && (fabs(difftime(data->time, d->time)) < 0.5)) {
	d = dataAtPoint(--i);
    }
    i++;

    // Now adjust everything starting at i.
    time_t start = dataAtPoint(0)->time; // Time of very first point.
    time_t t = data->time;		 // Time of first point in
					 // current interval.
    int subPoints = 0;			 // Number of points in
					 // current interval.
    for (; i < size(); i++) {
	subPoints++;

	FlightData *d = dataAtPoint(i);
	// An interval ends when we get a different time value, or
	// when we reach the end of the data.
	if ((difftime(d->time, t) > 0.5) || (i == (size() - 1))) {
	    // We've completed an interval (a set of points with the
	    // same absolute time values).  Set the offsets of all
	    // points in the interval, assuming that they are spaced
	    // equally within the interval.
	    float subInterval = 1.0 / subPoints;
	    int intervalStart = i - subPoints + 1;
	    for (int j = 0; j < subPoints; j++) {
		dataAtPoint(intervalStart + j)->est_t_offset = 
		    difftime(t, start) + (j * subInterval);
	    }

	    t = d->time;
	    subPoints = 0;
	}
    }
}

// EYE - change to use _file
// Initialize ourselves to the data contained in the given file.
bool FlightTrack::_readFlightFile(const char *path)
{
    // We assume that the file consists of triplets of lines: $GPRMC,
    // $GPGGA, and $PATLA lines.  We read in the file three lines at a
    // time, passing them off to _parse_message.
    std::ifstream rc(path);
    if (!rc.is_open()) {
	return false;
    }

    std::string aLine;	// Used for reading the file.
    char *lines = NULL;	// This is what we pass to _parse_message.
    size_t totalLength = 1;	// Total length of 'lines' (plus space
				// for a terminating '\0').
    int count = 0;		// Counts how many lines we see.

    while (!rc.eof()) {
	// Note that C++ getline() chops off the trailing newline, but
	// _parse_message wants them, so we add it in later.
	getline(rc, aLine);
	size_t length = aLine.length();

	// Allocate extra space for the string (and the newline).  EYE
	// - does realloc() behave when ptr = NULL?
	lines = (char *)realloc((void *)lines, totalLength + length + 1);
	strncpy(lines + totalLength - 1, aLine.c_str(), length);
	totalLength += length + 1;
	// Add the newline.
	lines[totalLength - 2] = '\n';
	count++;

	// If we've read 3 lines, send them off to _parse_message.
	if (count == 3) {
	    lines[totalLength - 1] = '\0';
	    FlightData tmp;
	    if (!_parse_message(lines, &tmp)) {
		// EYE - should we delete all the points we've added?
		return false;
	    }

	    FlightData *d = new FlightData;
	    *d = tmp;

	    // Add point unconditionally (ie, no tolerance
	    // specification).
	    _addPoint(d, -1.0);

	    // Start all over again.
	    free(lines);
	    lines = NULL;
	    count = 0;
	    totalLength = 1;
	}
    }

    rc.close();

    return true;
}

// Parses one *complete* message.  We accept two different protocols:
// atlas and nmea.  In both cases, a message consists of 3 lines,
// separated by linefeeds.  Although lines arrive in a fixed order, we
// don't enforce this.
//
// atlas: The first line should be a "$GPRMC" sentence, the second a
//        "$GPGGA" sentence, and the third a "$PATLA" sentence.
//
// nmea: The first line should be a "$GPRMC" sentence, the second a
//       "$GPGGA" sentence, and the third a "$GPGSA" sentence.
//
// Although both atlas and nmea protocols transmit $GPRMC sentences,
// they differ in several ways:
//
// - atlas sends a 12-field $GPRMC sentence, whereas nmea's is 13
//   fields long (it adds a "mode indicator" field, which it always
//   sets to 'A').
//
// - nmea sends the magnetic variation; atlas leaves the field blank
//
// - nmea calculates heading using the north and east components of
//   the aircraft speed.  If the aircraft isn't moving, the heading is
//   undefined.  atlas uses what seems to be a more reliable method.
bool FlightTrack::_parse_message(char *buf, FlightData *d) 
{
    // Set the flight data to some reasonable initial values.
    d->time = 0;
    d->lat = d->lon = d->alt = d->hdg = d->spd = 0.0;
    d->nav1_rad = d->nav2_rad = 0.0;
    d->nav1_freq = d->nav2_freq = d->adf_freq = 0;

    // The buffer should consist of 3 lines, so first divide it by
    // newlines.
    char *aLine;
    while ((aLine = strsep(&buf, "\n\r")) != NULL) {
	// Tokens in each line are separated by commas.  We can get at
	// most 18 tokens (in $GPGSA).
	char *aToken;
	char *tokens[18];
	int tokenCount = 0;
	while ((aToken = strsep(&aLine, ",")) != NULL) {
	    if (tokenCount >= 18) {
		// We got more than 18 tokens.  Bail.
		return false;
	    }
	    tokens[tokenCount++] = aToken;
	}

	if ((strcmp(tokens[0], "$GPRMC") == 0) && 
	    ((tokenCount == 12) || (tokenCount == 13))) {
	    // Time and date
	    char *utc = tokens[1]; // HHMMSS
	    char *date = tokens[9];    // DDMMYYY (YY = years since 1900)
	    int hours, minutes, seconds;
	    int day, month, year;
	    struct tm t;

	    // EYE - we really should check the return values of the
	    // sscanf() calls.
	    sscanf(utc, "%2d%2d%2d", &hours, &minutes, &seconds);
	    sscanf(date, "%2d%2d%d", &day, &month, &year);
	    t.tm_sec = seconds;
	    t.tm_min = minutes;
	    t.tm_hour = hours;
	    t.tm_mday = day;
	    t.tm_mon = month - 1;
	    t.tm_year = year;
	    t.tm_isdst = 0;
	    d->time = timegm(&t);

	    // Latitude
	    char *lat = tokens[3]; // DDMM.MMM
	    char *latDir = tokens[4]; // 'N' or 'S'
	    int deg;
	    float min;
	    
	    sscanf(lat, "%2d%f", &deg, &min);
	    d->lat = deg + (min / 60.0);
	    if (strcmp(latDir, "S") == 0) {
		d->lat = -d->lat;
	    }
	    d->lat *= SG_DEGREES_TO_RADIANS;

	    // Longitude
	    char *lon = tokens[5];    // DDDMM.MMM
	    char *lonDir = tokens[6]; // 'E' or 'W'

	    sscanf(lon, "%3d%f", &deg, &min);
	    d->lon = deg + (min / 60.0);
	    if (strcmp(lonDir, "W") == 0) {
		d->lon = -d->lon;
	    }
	    d->lon *= SG_DEGREES_TO_RADIANS;

	    // Speed and heading
	    sscanf(tokens[7], "%f", &d->spd);
	    sscanf(tokens[8], "%f", &d->hdg);

	    // EYE - we should check the checksum (and do what?)
	} else if ((strcmp(tokens[0], "$GPGGA") == 0) && (tokenCount == 15))  {
	    // GPGGA also includes the UTC time, latitude, and
	    // longitude.  However, since GPRMC also contains that
	    // information, we just ignore it here.  From our point of
	    // view, the only interesting field is altitude.

	    // Altitude
	    sscanf(tokens[9], "%f", &d->alt);
	    char *units = tokens[10];	// 'F' or 'M'
	    // If units are metres, convert them to feet.
	    if (strcmp(units, "M") == 0) {
		d->alt *= SG_METER_TO_FEET;
	    }
	} else if ((strcmp(tokens[0], "$PATLA") == 0) && (tokenCount == 6)) {
	    // NAV1, NAV2 and ADF
	    float nav1_freq, nav2_freq;
	    sscanf(tokens[1], "%f", &nav1_freq);
	    sscanf(tokens[2], "%f", &d->nav1_rad);
	    sscanf(tokens[3], "%f", &nav2_freq);
	    sscanf(tokens[4], "%f", &d->nav2_rad);
	    sscanf(tokens[5], "%d", &d->adf_freq);
	    // VOR frequencies are transmitted in the PATLA line as
	    // floats (eg, 112.30), but we store them as ints (11230).
	    d->nav1_freq = (int)(nav1_freq * 100);
	    d->nav1_rad *= SG_DEGREES_TO_RADIANS;
	    d->nav2_freq = (int)(nav2_freq * 100);
	    d->nav2_rad *= SG_DEGREES_TO_RADIANS;

	    // This identifies this record (and track) as atlas-based.
	    _isAtlas = true;
	} else if ((strcmp(tokens[0], "$GPGSA") == 0) && (tokenCount == 18)) {
	    // This is sent in an nmea protocal message.  It contains
	    // no useful information (except to tell us that it's
	    // nmea).
	    _isAtlas = false;
	} else if ((strcmp(tokens[0], "") == 0) && (tokenCount == 1)) {
	    // This is what an empty line is parsed as.
	} else {
	    return false;
	}
    }

    return true;
}

// The data point supplied is added to the flight track.  NOTE: This
// pointer is considered FlightTrack's property after this call,
// i.e. it's responsible for freeing the memory.  Points are added
// only if they differ from the previously added point by more than
// `tolerance' degrees (N-S or E-W).  Default tolerance is 1 arc
// second.  To force it to accept all points unconditionally, set
// tolerance to a negative number.
void FlightTrack::_addPoint(FlightData *data, float tolerance)
{
    float lastlat, lastlon;

    if (!_track.empty()) {
	FlightData *last;
	last = _track.back();
	lastlat = last->lat;
	lastlon = last->lon;
    } else {
	lastlat = -99.0f;
	lastlon = -99.0f;
    }

    // Special case: FlightGear likes to start flights at latitude
    // 0.0, longitude 0.0 (somewhere in the Atlantic Ocean).  We don't
    // want to record these points.  If you do happen to find yourself
    // at sea level, stationary, in the middle of the Atlantic, call
    // me.  I'd like to know what the hell you're doing.
    if ((fabs(data->lat) < 0.001) &&
	(fabs(data->lon) < 0.001) &&
	(fabs(data->spd) < 0.001) &&
	(fabs(data->hdg) < 0.001) &&
	(fabs(data->alt) < 0.001)) {
	return;
    }

    // Add the point if it's different enough from the last point.
    if (fabs(lastlat - data->lat) > tolerance || 
	fabs(lastlon - data->lon) > tolerance) {
	if ((_max_buffer != 0) && (_track.size() > _max_buffer)) {
	    // We're over our buffer limit.  Delete the first point.
	    delete _track.front();
	    _track.pop_front();

	    // Recalculate time offsets from the start onwards.
	    _adjustOffsetsAround(0);
	}

	// Add point.
	_track.push_back(data);
	_adjustOffsetsAround(_track.size() - 1);

	// Mark us as changed.
	_version++;
    }
}

// Lifted bodily from FlightGear's atlas.cxx.
char FlightTrack::_calcChecksum(char *sentence) {
    unsigned char sum = 0;
    int i, len;

    len = strlen(sentence);
    sum = sentence[0];
    for (i = 1; i < len; i++) {
        sum ^= sentence[i];
    }

    return sum;
}

// A very, very, very specialized and odd little routine used by
// save() when printing a latitude or longitude.  It helps us convert
// an angle in radians to a string of the form "3728.308,N" (37
// degrees and 28.308 minutes north).  Note that it doesn't create the
// final string - it just gives us the pieces.
//
// Given a number representing an angle (in radians), and a
// "direction" (where direction[0] is a label to be applied when the
// angle is positive, and direction[1] when angle is negative), sets
// 'd' to the value of the angle (in integer degrees), 'm' to the
// value of the angle in minutes, and 'c' to the label.
void FlightTrack::_splitAngle(float angle, const char direction[2],
			      int *d, float *m, char *c)
{
    angle *= SG_RADIANS_TO_DEGREES;
    if (angle < 0) {
	*c = direction[1];
	angle = -angle;
    } else {
	*c = direction[0];
    }
    *d = (int)angle;
    *m = (angle - (float)*d) * 60.0;
}
