/*-------------------------------------------------------------------------
  Bucket.cxx

  Written by Brian Schack

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

#include "Bucket.hxx"

#if defined( __APPLE__)
#  include <GLUT/glut.h>	// Needed for gluPickMatrix().
#else
#  ifdef WIN32
#    include <windows.h>
#  endif
#  include <GL/glut.h>
#endif

#include <cassert>
#include <iostream>
#include <sstream>
#include <fstream>

#include <simgear/bucket/newbucket.hxx>
#include <simgear/math/sg_geodesy.hxx>

// #include "loadMap.hxx"
#include "Subbucket.hxx"
#include "LayoutManager.hxx"
#include "Globals.hxx"

using namespace std;

// EYE - chunk? tile? bucket?  In newbucket.hxx, it seems that a 1x1
// square is a chunk, and the parts are called both tiles and buckets.
// In the HTML documentation, it seems that a chunk is just a vague
// term for a part, and a tile is the 1/8 x 1/8 degree (or whatever)
// bit corresponding to a single scenery file.
Bucket::Bucket(const SGPath &p, long int index): 
    _p(p), _index(index), _dlist(0), _loaded(false), _dirty(true), _size(0)
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

//     // Get notifications of palette changes.
//     subscribe(Notification::NewPalette);
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
    globalString.printf("%d.stg", _index);
    stg.append(globalString.str());
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
		_chunks.push_back(sb);
	    } else {
		fprintf(stderr, "'%s': object file '%s' not found\n", 
			stg.c_str(), object.c_str());
	    }
	}
    }

    // Find the highest point in the bucket and set _maxElevation.
    // EYE - magic number!
    _maxElevation = -1e6;
    for (unsigned int i = 0; i < _chunks.size(); i++) {
	if (_chunks[i]->maximumElevation() > _maxElevation) {
	    _maxElevation = _chunks[i]->maximumElevation();
	}
    }

    _loaded = true;
}

void Bucket::unload()
{
    for (unsigned int i = 0; i < _chunks.size(); i++) {
	delete _chunks[i];
    }
    _chunks.clear();
    _size = 0;

    _loaded = false;

    glDeleteLists(_dlist, 1);
    _dlist = 0;
    _dirty = true;
}

// void Bucket::draw(bool discreteContours)
void Bucket::draw(Palette *palette, bool discreteContours)
{
    if (!_loaded) {
	return;
    }

    if (_dirty) {
	glDeleteLists(_dlist, 1);
	_dlist = glGenLists(1);
	assert(_dlist != 0);

	glNewList(_dlist, GL_COMPILE);

	// EYE - Assumes that GL_COLOR_MATERIAL has been set and
	// enabled.
	for (unsigned int i = 0; i < _chunks.size(); i++) {
// 	    _chunks[i]->draw(globals.palette, discreteContours);
	    _chunks[i]->draw(palette, discreteContours);
	}

	glEndList();
	_dirty = false;
    }

    glCallList(_dlist);
}

// Checks if the ray defined by points near and far intersect this
// bucket.  If it does, then the intersection is placed into c.

// EYE - still not good enough!  Need to mention the viewing frustum,
// and why we need to pass near and far and how they should be
// defined.  Should the RaySphere test be moved out?  (Because it may
// not be 100% reliable, and/or because it belongs higher up).
bool Bucket::intersection(SGVec3<double> near, SGVec3<double> far,
			  SGVec3<double> *c)
{
    // We don't try to do a "live" intersection unless we've actually
    // loaded the scenery and created a display list for it.
    if (!_loaded || (_dlist == 0)) {
	return false;
    }

    // As a first quick check, we see if it intersects our bounding
    // sphere.  If it does, we need to do a more detailed check.
    double mu1, mu2;
    // EYE - is this doing a cast or a copy?  And check this elsewhere.

    // EYE - is it possible to *not* intersect the bounding sphere but
    // to intersect the bucket?  We should check carefully how the
    // bounding sphere is defined.
    if (!RaySphere(near, far, SGVec3<double>(_bounds.center), _bounds.radius, 
		  &mu1, &mu2)) {
	// Doesn't intersect our bounding sphere, so return false
	// immediately.
	return false;
    }

    // It intersects our bounding sphere - we need to draw the scene
    // in select mode and see if we get any hits.  We don't care about
    // the identities of anything that intersects - just the minimum
    // depth value recorded, which is placed in selectBuf[1].
    GLuint selectBuf[3];
    glSelectBuffer(3, selectBuf);
    glRenderMode(GL_SELECT);

    // Now "draw" the bucket.
    glMatrixMode(GL_MODELVIEW);
    glCallList(_dlist);

    // Any hits?
    int hits = glRenderMode(GL_RENDER);
    assert(hits <= 1);
    if (hits == 0) {
	// Nope.
	return false;
    }

    // We got a hit, meaning our picking frustum intersects some part
    // of our bucket.  We want the highest point in that intersection.
    // To make our life easy, we make two simplifying assumptions: (1)
    // We are looking straight down on the bucket (not exactly true,
    // but close enough), and (2) The bucket is not curved (also not
    // exactly true, but close enough).
    //
    // Given these assumptions, the highest point is the nearest z
    // value, which is given in the hit record at offset 1.  Note that
    // OpenGL applies a scaling factor of 2^32-1 to z values, where z
    // = 0 at the near plane, and 2^32-1 at the far plane.
    const double scale = (double)SGLimits<GLuint>::max();
    double minZ = selectBuf[1] / scale;

    *c = (far - near) * minZ + near;

    return true;
}

// // Called when the palette changes.
// bool Bucket::notification(Notification::type n)
// {
//     if (n == Notification::NewPalette) {
// 	_dirty = true;
//     } else {
// 	assert(false);
//     }

//     return true;
// }
