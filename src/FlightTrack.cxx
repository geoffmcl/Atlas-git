/*-------------------------------------------------------------------------
  FlightTrack.cxx

  Written by Per Liedman, started July 2000.

  Copyright (C) 2000 Per Liedman, liedman@home.se
  Copyright (C) 2009 Brian Schack

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

#include <math.h>

#include <cassert>
#include <string>
#include <fstream>
#include <stdexcept>
#include <limits>

#include <simgear/timing/sg_time.hxx>
#include <simgear/math/sg_geodesy.hxx>

#include "FlightTrack.hxx"
#include "Overlays.hxx"
#include "Globals.hxx"

using namespace std;

FlightData::FlightData(): _navaidsLoaded(false)
{
}

FlightData::~FlightData()
{
}

// We force users to access the navaids vector via this method because
// we want to avoid searching for navaids if we can.  The alternative
// is searching for navaids for the entire flight track when we load
// it in, which can be prohibitively slow.
const vector<NAV *>& FlightData::navaids()
{
    if (!_navaidsLoaded) {
	// Look up the navaids we're tuned into.  Note that we must
	// have a valid cartesian location for the call to getNavaids.
	const vector<Cullable *>& results = 
	    globals.overlays->navaidsOverlay()->getNavaids(this);
	for (unsigned int i = 0; i < results.size(); i++) {
	    NAV *n = dynamic_cast<NAV *>(results[i]);
	    assert(n);
	    _navaids.push_back(n);
	}
	
	_navaidsLoaded = true;
    }

    return _navaids;
}

const size_t FlightTrack::npos = numeric_limits<size_t>::max();

// EYE - create a common initializer?
FlightTrack::FlightTrack(const char *filePath) : 
    _max_buffer(0), _mark(npos), _live(false), _input_channel(NULL)
{
    if (!_readFlightFile(filePath)) {
	throw runtime_error("flight file open failure");
    }

    _file.set(filePath);

    _port = -1;
    _device = "";
    _baud = -1;

    _isNetwork = _isSerial = false;

    _versionAtLastSave = _version = 0;
}

FlightTrack::FlightTrack(int port, unsigned int max_buffer) : 
    _max_buffer(max_buffer), _mark(npos), _live(true)
{
    AtlasString portStr;

    portStr.printf("%d", port);
    _input_channel = new SGSocket("", portStr.str(), "udp");
    _input_channel->open(SG_IO_IN);

    _file.set("");

    _port = port;
    _device = "";
    _baud = -1;

    _isNetwork = true;
    _isSerial = false;

    _versionAtLastSave = _version = 0;
}

FlightTrack::FlightTrack(const char *device, int baud, unsigned int max_buffer) : 
    _max_buffer(max_buffer), _mark(npos), _live(true)
{
    AtlasString baudStr;

    baudStr.printf("%d", baud);
    _input_channel = new SGSerial(device, baudStr.str());
    _input_channel->open(SG_IO_IN);

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

bool FlightTrack::isAtlasProtocol()
{
    return _isAtlasProtocol;
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

// Adjusts the maximum size of the flight points buffer.  This may
// result in data being deleted.  If the track is not live, we don't
// do anything.
void FlightTrack::setMaxBufferSize(unsigned int size)
{
    // We can only do this for live tracks.
    if (!live()) {
	return;
    }

    // If the new size is is bigger than the current size, we don't
    // need to delete anything, so we can return right away.
    _max_buffer = size;
    if ((_max_buffer == 0) || (_track.size() <= _max_buffer)) {
	return;
    }

    // We need to delete points.
    while (_track.size() > _max_buffer) {
	delete _track.front();
	_track.pop_front();
    }

    _version++;
    _adjustOffsetsAround(0);
    _calcDistancesFrom(0);
}

void FlightTrack::clear() 
{
    while (!_track.empty()) {
	delete *(_track.begin());
	_track.pop_front();
    }
    _version++;
    _mark = npos;
}

bool FlightTrack::empty() 
{
    return _track.empty();
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
	    // Record point.
	    FlightData *d = new FlightData;
	    *d = tmp;

	    // EYE - I add the point unconditionally (before it was only
	    // added if the change was more than 1 arc second).
	    result = _addPoint(d, -1.0);
	}
    }

    return result;
}

FlightData *FlightTrack::at(size_t i)
{
    if (i >= _track.size()) {
	return NULL;
    } else {
	return _track[i];
    }
}

FlightData *FlightTrack::last() 
{
    if (_track.size() > 0) {
	return _track.back();
    } else {
	return NULL;
    }
}

// Sets mark to given position if we can.
void FlightTrack::setMark(size_t i)
{
    if (_track.size() > i) {
	_mark = i;
    } else {
	_mark = npos;
    }
}

// Returns true if the file path is not the empty string.
bool FlightTrack::hasFile()
{
    return (_file.str() != "");
}

// EYE - return as string?
const char *FlightTrack::fileName()
{
    // SGPath has a bug in the file() method.  If a path has no path
    // separators (eg, "foo.text", as opposed to something like
    // "dir/foo.text"), file() returns "", rather than "foo.text".
    //
    // Also, it returns a copy of the string, but we want to return a
    // char *.  Since it returns a (temporary) copy, we can't just
    // return the c_str() of the resulting string, because it will
    // disappear when we return.  Yeesh.  So, we do the work
    // ourselves.  This code assumes that SGPath always uses '/' as
    // the path separator.
    const char *name = strrchr(_file.c_str(), '/');
    if (name == NULL) {
	name = _file.c_str();
    } else {
	name++;
    }
    return name;
}

const char *FlightTrack::filePath()
{
    return _file.c_str();
}

// Creates a nicely formatted name for the flight track.  The returned
// string should be copied if you want to save it.  Here are examples
// of what it returns:
//
// 'network (<port>)' - live network, no file
// 'network (<port>, <name>)' - live network, with file
// 'network (<port>, <name>*)' - live network, with file, unsaved
// 'serial (<device>, <baud>)' - live serial, no file
// 'serial (<device>, <baud>, <name>)' - live serial, with file, saved
// 'serial (<device>, <baud>, <name>*)' - live serial, with file, unsaved
// '<name>' - file, saved
// '<name>*' - file, unsaved
// 'detached, no file' - no network or serial connection, no file
const char *FlightTrack::niceName()
{
    // We don't save the name because it could change as the status of
    // the flight track changes.  It's easier just to generate it anew
    // each time its requested (presumably this will not happen very
    // often).

    // EYE - do we get re-called when we should?
    if (isNetwork()) {
	if (hasFile()) {
	    if (modified()) {
		_name.printf("network (%d, %s*)", port(), fileName());
	    } else {
		_name.printf("network (%d, %s)", port(), fileName());
	    }
	} else {
	    _name.printf("network (%d)", port());
	}
    } else if (isSerial()) {
	if (hasFile()) {
	    if (modified()) {
		_name.printf("serial (%s, %d, %s*)", 
			   device(), baud(), fileName());
	    } else {
		_name.printf("serial (%s, %d, %s)", 
			   device(), baud(), fileName());
	    }
	} else {
	    _name.printf("serial (%s, %d)", device(), baud());
	}
    } else if (hasFile()) {
	if (modified()) {
	    _name.printf("%s*", fileName());
	} else {
	    _name.printf("%s", fileName());
	}
    } else {
	_name.printf("detached, no file");
    }

    return _name.str();
}

void FlightTrack::setFilePath(char *path)
{
    // EYE - check for existing name?  overwriting?
    // EYE - call this (and other accessors) from constructors?
    _file.set(path);
    // We count this as a change.
    _version++;
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
	    AtlasString time, date;
	    struct tm *t = gmtime(&(d->time));
	    time.printf("%02d%02d%02d", t->tm_hour, t->tm_min, t->tm_sec);
	    if (isAtlasProtocol()) {
		date.printf("%02d%02d%02d", 
			    t->tm_mday, t->tm_mon + 1, t->tm_year);
	    } else {
		// NMEA has a buggy notion of the current year, but we
		// need to record it faithfully.
		date.printf("%02d%02d%02d", 
			    t->tm_mday, t->tm_mon + 1, t->tm_year % 100);
	    }

	    AtlasString lat, lon;
	    int degrees;
	    float minutes;
	    char c;
	    _splitAngle(d->lat, "NS", &degrees, &minutes, &c);
	    lat.printf("%02d%06.3f,%c", degrees, minutes, c);
	    _splitAngle(d->lon, "EW", &degrees, &minutes, &c);
	    lon.printf("%03d%06.3f,%c", degrees, minutes, c);

	    AtlasString buf;
	    char checksum;

	    // $GPRMC
	    buf.printf("GPRMC,%s,A,%s,%s,%05.1f,%05.1f,%s,0.000,E",
		       time.str(), lat.str(), lon.str(), d->spd, d->hdg, 
		       date.str());
	    if (!isAtlasProtocol()) {
		// NMEA adds a little ",A" to the end.
		buf.appendf(",A");
	    }
	    checksum = _calcChecksum(buf.str());
	    fprintf(f, "$%s*%02X\n", buf.str(), checksum);

	    // $GPGGA
	    buf.printf("GPGGA,%s,%s,%s,1,,,%.0f,F,,,,", 
		       time.str(), lat.str(), lon.str(), d->alt);
	    checksum = _calcChecksum(buf.str());
	    fprintf(f, "$%s*%02X\n", buf.str(), checksum);

	    // $PATLA
	    if (isAtlasProtocol()) {
		buf.printf("PATLA,%.2f,%.1f,%.2f,%.1f,%d",
			   d->nav1_freq / 1000.0, 
			   d->nav1_rad, 
			   d->nav2_freq / 1000.0, 
			   d->nav2_rad, 
			   d->adf_freq);
	    } else {
		buf.printf("GPGSA,A,3,01,02,03,,05,,07,,09,,11,12,0.9,0.9,2.0");
	    }
	    checksum = _calcChecksum(buf.str());
	    fprintf(f, "$%s*%02X\n", buf.str(), checksum);
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
void FlightTrack::_adjustOffsetsAround(size_t i)
{
    FlightData *data = at(i);

    if (data == NULL) {
	return;
    }

    // Find the first point with the same absolute time value.
    FlightData *d = at(--i);
    while (d && (fabs(difftime(data->time, d->time)) < 0.5)) {
	d = at(--i);
    }
    i++;

    // Now adjust everything starting at i.
    time_t start = at(0)->time; // Time of very first point.
    time_t t = data->time;		 // Time of first point in
					 // current interval.
    int subPoints = 0;			 // Number of points in
					 // current interval.
    for (; i < size(); i++) {
	subPoints++;

	FlightData *d = at(i);
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
		at(intervalStart + j)->est_t_offset = 
		    difftime(t, start) + (j * subInterval);
	    }

	    t = d->time;
	    subPoints = 0;
	}
    }
}

// Similar to the previous method, this is used to set the 'dist'
// field of the indicated FlightData point and all points after.  All
// points before 'i' are assumed to have the correct cumulative
// distance.
void FlightTrack::_calcDistancesFrom(size_t i)
{
    if (i >= _track.size()) {
	return;
    }

    // If we're being asked to start at the beginning, we need to set
    // the first point's distance to 0.0 explicitly.
    if (i == 0) {
	at(0)->dist = 0.0;
	i++;
    }

    // Now update starting at point i (which depends on the cumulative
    // distance in point i - 1), going to the end.
    FlightData *d0 = at(i - 1), *d1;
    for (; i < _track.size(); i++) {
	d1 = at(i);
	float delta = sgdDistanceVec3(d1->cart, d0->cart);
	d1->dist = d0->dist + delta;
	d0 = d1;
    }
}

// EYE - change to use _file
// Initialize ourselves to the data contained in the given file.
bool FlightTrack::_readFlightFile(const char *path)
{
    // We assume that the file consists of triplets of lines: $GPRMC,
    // $GPGGA, and $PATLA lines.  We read in the file three lines at a
    // time, passing them off to _parse_message.
    ifstream rc(path);
    if (!rc.is_open()) {
	return false;
    }

    string aLine;	// Used for reading the file.
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
    d->est_t_offset = d->dist = 0.0;

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
	    char *date = tokens[9];    // DDMMYYY (YYY = years since 1900)
	    int hours, minutes, seconds;
	    int day, month, year;

	    // EYE - we really should check the return values of the
	    // sscanf() calls.

	    sscanf(utc, "%2d%2d%2d", &hours, &minutes, &seconds);
	    sscanf(date, "%2d%2d%d", &day, &month, &year);
	    if (tokenCount == 13) {
		// The nmea protocol forces all year values to be less
		// than 100, which is wrong (2009, for example, should
		// be 109, not 09).  This hack will correctly for for
		// dates from 1990 to 2089.
		if (year < 90) {
		    year += 100;
		}
	    }
	    d->time = sgTimeGetGMT(year, month - 1, day, 
				   hours, minutes, seconds);

	    // GPRMC also includes the latitude and longitude.
	    // However, since GPGGA also contains that information, as
	    // well as the altitude, we just ignore the latitude and
	    // longitude here.

	    // Speed and heading
	    sscanf(tokens[7], "%f", &d->spd);
	    sscanf(tokens[8], "%f", &d->hdg);

	    // EYE - we should check the checksum (and do what?)
	} else if ((strcmp(tokens[0], "$GPGGA") == 0) && (tokenCount == 15))  {
	    // Latitude
	    char *lat = tokens[2]; // DDMM.MMM
	    char *latDir = tokens[3]; // 'N' or 'S'
	    int deg;
	    float min;
	    
	    sscanf(lat, "%2d%f", &deg, &min);
	    d->lat = deg + (min / 60.0);
	    if (strcmp(latDir, "S") == 0) {
		d->lat = -d->lat;
	    }

	    // Longitude
	    char *lon = tokens[4];    // DDDMM.MMM
	    char *lonDir = tokens[5]; // 'E' or 'W'

	    sscanf(lon, "%3d%f", &deg, &min);
	    d->lon = deg + (min / 60.0);
	    if (strcmp(lonDir, "W") == 0) {
		d->lon = -d->lon;
	    }

	    // Altitude
	    sscanf(tokens[9], "%f", &d->alt);
	    char *units = tokens[10];	// 'F' or 'M'
	    // If units are metres, convert them to feet.
	    if (strcmp(units, "M") == 0) {
		d->alt *= SG_METER_TO_FEET;
	    }

	    // Since we have a lat, lon, and altitude, calculate the
	    // cartesian coordinates of this point.
	    sgGeodToCart(d->lat * SGD_DEGREES_TO_RADIANS, 
			 d->lon * SGD_DEGREES_TO_RADIANS, 
			 d->alt * SG_FEET_TO_METER, 
			 d->cart);
	} else if ((strcmp(tokens[0], "$PATLA") == 0) && (tokenCount == 6)) {
	    // NAV1, NAV2 and ADF
	    float nav1_freq, nav2_freq;
	    sscanf(tokens[1], "%f", &nav1_freq);
	    sscanf(tokens[2], "%f", &d->nav1_rad);
	    sscanf(tokens[3], "%f", &nav2_freq);
	    sscanf(tokens[4], "%f", &d->nav2_rad);
	    sscanf(tokens[5], "%d", &d->adf_freq);
	    // VOR frequencies are transmitted in the PATLA line as
	    // floats (eg, 112.30), but we store them as ints (112300).
	    d->nav1_freq = (int)(nav1_freq * 1000);
	    d->nav2_freq = (int)(nav2_freq * 1000);

	    // This identifies this record (and track) as atlas-based.
	    _isAtlasProtocol = true;
	} else if ((strcmp(tokens[0], "$GPGSA") == 0) && (tokenCount == 18)) {
	    // This is sent in an nmea protocal message.  It contains
	    // no useful information (except to tell us that it's
	    // nmea).
	    _isAtlasProtocol = false;
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
//
// Returns true if the point actually got added.
bool FlightTrack::_addPoint(FlightData *data, float tolerance)
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
	delete data;
	return false;
    }

    // Only add the point if it's different enough from the last
    // point.
    if (fabs(lastlat - data->lat) < tolerance &&
	fabs(lastlon - data->lon) < tolerance) {
	delete data;
	return false;
    }

    if ((_max_buffer != 0) && (_track.size() >= _max_buffer)) {
	// We're over our buffer limit.  Delete the first point.
	delete _track.front();
	_track.pop_front();

	// Recalculate time offsets from the start onwards.
	_adjustOffsetsAround(0);
	_calcDistancesFrom(0);
    }

    // Add point.
    _track.push_back(data);
    _adjustOffsetsAround(_track.size() - 1);
    _calcDistancesFrom(_track.size() - 1);

    // Mark us as changed.
    _version++;

    return true;
}

// Lifted bodily from FlightGear's atlas.cxx.
char FlightTrack::_calcChecksum(const char *sentence) 
{
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
// an angle in degrees to a string of the form "37 28.308 N" (37
// degrees and 28.308 minutes north).  Note that it doesn't create the
// final string - it just gives us the pieces.
//
// Given a number representing an angle (in degrees), and a
// "direction" (where direction[0] is a label to be applied when the
// angle is positive, and direction[1] when angle is negative), sets
// 'd' to the value of the angle (in integer degrees), 'm' to the
// value of the angle in minutes, and 'c' to the label.
void FlightTrack::_splitAngle(float angle, const char direction[2],
			      int *d, float *m, char *c)
{
    if (angle < 0) {
	*c = direction[1];
	angle = -angle;
    } else {
	*c = direction[0];
    }
    *d = (int)angle;
    *m = (angle - (float)*d) * 60.0;
}
