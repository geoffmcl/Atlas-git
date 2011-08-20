/*-------------------------------------------------------------------------
  Palette.cxx

  Written by Brian Schack

  Copyright (C) 2009 - 2011 Brian Schack

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Palette.hxx"

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <fstream>

#include <simgear/constants.h>

using namespace std;

Palette::Palette(const char *path): _metres(true), _base(0.0), _i(1)
{
    // EYE - throw errors?
    ifstream in(path);
    if (in.fail()) {
	throw std::runtime_error("failed to open file");
	return;
    }

    string buf;

    // Map from colour names to colours.
    map<string, _foodle> colourMap;
    string keyword, material, name;
    _foodle colour;

    while (getline(in, buf)) {
	istringstream line(buf);
	if (line.str().size() == 0) {
	    continue;
	}

	if (line.str()[0] == '#') {
	    // Comment
	    continue;
	}

	line >> keyword;

	if (keyword == "Units") {
	    // Units <feet/ft/metres/meters/m>
	    line >> name;
	    if ((name == "feet") || 
		(name == "ft")) {
		_metres = false;
	    } else if ((name == "metres") || 
		       (name == "meters") || 
		       (name == "m")) {
		_metres = true;
	    } else {
		throw std::runtime_error("unknown unit");
	    }
	} else if ((keyword == "Colour") || (keyword == "colour")) {
	    // Colour <name> <R> <G> <B> <A>
	    line >> name;

	    for (int i = 0; i < 4; i++) {
		line >> colour.c[i];
	    }

	    if (colourMap.find(name) != colourMap.end()) {
		throw std::runtime_error("colour multiply defined");
	    } else {
		colourMap.insert(make_pair(name, colour));
	    }
	} else if (keyword == "Material") {
	    // Material <material> <name>
	    line >> material >> name;

	    if (colourMap.find(name) == colourMap.end()) {
		// Non-existent colour.
		continue;
	    }

	    int height;
	    if (sscanf(material.c_str(), "Elevation_%d", &height) == 1) {
		Contour entry;
		if (!_metres) {
		    // Internally we use metres.
		    height *= SG_FEET_TO_METER;
		}
		entry.elevation = height;
		sgCopyVec4(entry.colour, colourMap[name].c);
		_elevations.push_back(entry);
	    } else if (_materials.find(material) == _materials.end()) {
		// EYE - need a list of all materials (to help users
		// create AtlasPalette files).

		// EYE - the earlier version would add Elevation_<x>
		// entries to the materials map as well.
		_materials.insert(make_pair(material, colourMap[name]));
	    } else {
		throw std::runtime_error("material multiply defined");
	    }
	} else {
	    fprintf(stderr, "%s: syntax error in line:\n\t'%s'\n", 
		    path, line.str().c_str());
	}
    }

    if (_elevations.size() == 0) {
	throw std::runtime_error("no elevations specified");
    }

    // Bubble sort the _elevations vector by height.
    bool unsorted = true;
    while (unsorted) {
	unsorted = false;
	// EYE - duplicate elevations?
	for (unsigned int i = 0; i < _elevations.size() - 1; i++) {
	    if (_elevations[i].elevation > _elevations[i + 1].elevation) {
		swap(_elevations[i], _elevations[i + 1]);
		unsorted = true;
	    }
	}
    }
    // Now add Contour "fences" to the beginning and end of the deque.
    // The purpose of these fake entries is the make calculating
    // smoothed colour values in smoothColour() easier - without them,
    // we need to do tricky testing for boundary conditions.
    Contour fake = _elevations[0];
    fake.elevation -= 1.0;
    _elevations.push_front(fake);

    size_t i = _elevations.size() - 1;
    fake = _elevations[i];
    fake.elevation += _elevations[i].elevation - _elevations[i - 1].elevation;
    _elevations.push_back(fake);

    // Copy the raw elevations data in _elevations to ones offset by
    // _base, in _offsetElevations.
    _offsetElevations = _elevations;

    _path = path;
}

Palette::~Palette()
{
}

void Palette::setBase(float f)
{
    if (f == _base) {
	return;
    }

    _base = f;
    for (size_t i = 0; i < _elevations.size(); i++) {
	_offsetElevations[i].elevation = _elevations[i].elevation + _base;
    }
}

const Palette::Contour& Palette::contour(float elevation)
{
    return _offsetElevations[contourIndex(elevation)];
}

const float *Palette::colour(const char *material) const
{
    map<string, _foodle>::const_iterator i = _materials.find(material);
    if (i != _materials.end()) {
	return i->second.c;
    }

    return NULL;
}

// Returns the contour index in the palette for the given elevation.
// An entry:
//
// Material Elevation_<x> <colour>
//
// means "everything at <x> or above is <colour>".
//
// Note that we return an index in the range <0, Palette::size()>, and
// that a returned index i corresponds to _offsetElevations[i + 1].
// This is because the palette has an extra <elevation, contour> pair
// inserted at the beginning (and at the end).
unsigned int Palette::contourIndex(float elevation)
{
    // Search up.  We use a class variable _i, which maintains its
    // value between calls, the logic being that successive calls to
    // this routine will be at similar elevations - by beginning a new
    // search at the end of the last one, we're likely to already be
    // at the right place.  The variable _i points to the *real*
    // entry.
    for (; 
	 (_i < _offsetElevations.size() - 2) && 
	     (elevation >= _offsetElevations[_i + 1].elevation); 
	 _i++)
	;
    // Search down.
    for (; (_i > 1) && (elevation < _offsetElevations[_i].elevation); _i--)
	;

    // Adjust for the "fake" first pair.
    return _i - 1;
}

// Returns the <elevation, colour> pair at the given index, which is
// assumed to have come from a call contourIndex() (ie, it has been
// adjusted for the extra pairs at the start and end of the
// _offsetElevations deque).
const Palette::Contour& Palette::contourAtIndex(unsigned int i) const
{
    // Adjust for the "fake" first pair.
    i++;
    if (i < 1) {
	i = 1;
    } else if (i >= _offsetElevations.size() - 2) {
	i = _offsetElevations.size() - 2;
    }

    return _offsetElevations[i];
}

// Creates a smoothed colour value for the given elevation.  Assume we
// have following palette entries (where the colour index has been
// replaced by a variable for clarity):
//
// Material Elevation_0		c0
// Material Elevation_100	c1
// Material Elevation_200	c2
//
// c0 is used from 0 to 100, c1 from 100 to 200, and c2 above 200.
// Our goal is to use pure colours in the middle of their range, an
// equal mix at the boundaries, and a weighted blend elsewhere.
//
// Therefore:
//
// (1) an elevation of 100 gives a colour of c0 + c1 / 2.0.
//
// (2) an elevation of 125 gives a colour of c0 + 3 * c1 / 4.0
//
// (3) an elevation of 150 gives a colour of c1.
//
// (4) an elevation of 175 gives a colour of 3 * c1 + c2 / 4.0.
//
// (5) an elevation of 200 gives a colour of c1 + c2 / 2.0.
void Palette::smoothColour(float elevation, sgVec4 colour)
{
    // We directly access the _offsetElevations deque in this routine,
    // so we need to add one to the index returned by contourIndex.
    unsigned int i = contourIndex(elevation) + 1;

    // These are used when smoothing colours:
    //
    // range - elevation range for the band the elevation is in
    // delta - difference between the elevation and the bottom of the
    //     band
    // scale - how much to scale the two colours
    float range, delta, scale;
    const float *cu, *cl;		// Upper and lower colours.

    range = _offsetElevations[i + 1].elevation - _offsetElevations[i].elevation;
    delta = elevation - _offsetElevations[i].elevation;
    scale = delta / range;
    if (scale > 0.5) {
	// Blend with upper colour.  The lower colour varies from 1.0
	// (in the middle) to 0.5 (at the border), the upper colour
	// from 0.0 (in the middle) to 0.5 (at the border).
	scale -= 0.5;
	cu = _offsetElevations[i + 1].colour;
	cl = _offsetElevations[i].colour;
    } else {
	// Blend with lower colour.  The upper colour varies from 1.0
	// (in the middle) to 0.5 (at the border), the lower colour
	// from 0.0 (in the middle) to 0.5 (at the border).
	scale += 0.5;
	cu = _offsetElevations[i].colour;
	cl = _offsetElevations[i - 1].colour;
    }

    sgSubVec4(colour, cu, cl);
    sgScaleVec4(colour, scale);
    sgAddVec4(colour, cl);
}
