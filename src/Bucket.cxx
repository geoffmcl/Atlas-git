/*-------------------------------------------------------------------------
  Bucket.cxx

  Written by Brian Schack

  Copyright (C) 2009 - 2014 Brian Schack

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
#include "Bucket.hxx"

// C++ system files
#include <sstream>
#include <fstream>

// Other libraries' include files
#include <simgear/bucket/newbucket.hxx>

// Our project's include files
#include "Palette.hxx"
#include "Subbucket.hxx"

using namespace std;

Palette *Bucket::palette = NULL;
bool Bucket::discreteContours = true;
bool Bucket::contourLines = false;
bool Bucket::polygonEdges = false;

// EYE - should we make this nan()/nanl()/nanf() and have an isNanE()
// function (which is just isnan())?  Note that we can't do simple
// comparisons with NaNs - the results are always false.  Note as well
// that instead of the C++ constants, we can use C constants (FLT_MAX,
// DBL_MAX, ....  See 'man float' for a list.  See as well 'man 3
// math' for a bunch of useful functions and constants).
const float Bucket::NanE = -numeric_limits<float>::max();

// EYE - chunk? tile? bucket?  In newbucket.hxx, it seems that a 1x1
// square is a chunk, and the parts are called both tiles and buckets.
// In the HTML documentation, it seems that a chunk is just a vague
// term for a part, and a tile is the 1/8 x 1/8 degree (or whatever)
// bit corresponding to a single scenery file.
Bucket::Bucket(const SGPath &p, long int index): 
    _p(p), _index(index), _loaded(false), _size(0)
{
    // Calculate bounds.

    // EYE - SGBucket is buggy at high latitudes, except when we use
    // the index constructor
    SGBucket b(_index);
    double width, height;

    _lat = b.get_center_lat();
    _lon = b.get_center_lon();
    width = b.get_width() / 2.0;
    height = b.get_height() / 2.0;

    // Most buckets are more or less rectangular, so using the 4
    // corners to determine the bounding sphere is justified.
    // However, near the poles this becomes less and less true.  The
    // extreme is from 89 degrees to the pole, where buckets are
    // doughnuts, with the two top corners and two bottom corners
    // coincident.  For that reason we also use the points midway
    // along the top and bottom edges to define the bounds.
    _bounds.extendBy(_lat - height, _lon - width); // west corners
    _bounds.extendBy(_lat + height, _lon - width);
    _bounds.extendBy(_lat - height, _lon + width); // east corners
    _bounds.extendBy(_lat + height, _lon + width);
    _bounds.extendBy(_lat - height, _lon); // middle
    _bounds.extendBy(_lat + height, _lon);
}

Bucket::~Bucket()
{
    unload();
}

void Bucket::load(Projection projection)
{
    // EYE - all guesswork!  Are there docs?

    // A bucket may actually consist of several parts.  The parts are
    // described in a <index>.stg file, where <index> is the bucket
    // index.  The .stg file has one line per part, where each line
    // starts with the type of the object: OBJECT_BASE for the basic
    // scenery, OBJECT for other scenery in the same directory
    // (usually airports), and OBJECT_SHARED for something in the
    // Models directory.  We care about OBJECT_BASE and OBJECT types.

    SGPath stg(_p);
    AtlasString str;
    str.printf("%d.stg", _index);
    stg.append(str.str());
    assert(_size == 0);

    ifstream in(stg.c_str());
    string buf;
    while (getline(in, buf)) {
	// EYE - use this paradigm for other file reading?
	istringstream str(buf);
	string type, data;
	// EYE - checking?
	str >> type >> data;
	if ((type == "OBJECT_BASE") || (type == "OBJECT")) {
	    SGPath object(_p);
	    object.append(data);
	    object.concat(".gz"); // EYE - always?

	    Subbucket *sb = new Subbucket(object);
	    if (sb->load(projection)) {
		_size += sb->size();
		_subbuckets.push_back(sb);
	    } else {
		fprintf(stderr, "'%s': object file '%s' not found\n", 
			stg.c_str(), object.c_str());
	    }
	}
    }

    // Find the highest point in the bucket and set _maxElevation.
    _maxElevation = Bucket::NanE;
    for (unsigned int i = 0; i < _subbuckets.size(); i++) {
	if (_subbuckets[i]->maximumElevation() > _maxElevation) {
	    _maxElevation = _subbuckets[i]->maximumElevation();
	}
    }

    _loaded = true;
}

void Bucket::unload()
{
    for (unsigned int i = 0; i < _subbuckets.size(); i++) {
	delete _subbuckets[i];
    }
    _subbuckets.clear();
    _size = 0;

    _loaded = false;
}

void Bucket::paletteChanged()
{
    for (size_t i = 0; i < _subbuckets.size(); i++) {
	_subbuckets[i]->paletteChanged();
    }
}

void Bucket::draw()
{
    if (!_loaded) {
	return;
    }

    // EYE - Assumes that GL_COLOR_MATERIAL has been set and enabled.
    for (unsigned int i = 0; i < _subbuckets.size(); i++) {
	_subbuckets[i]->draw();
    }
}
