/*-------------------------------------------------------------------------
  Bucket.hxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

  A bucket object can load and draw a bucket, which is a basic unit of
  FlightGear scenery.  Buckets are 1/8 degree high, and anywhere from
  1/8 degree to 360 degrees wide (the latter extreme case occurs at
  the poles).  Buckets themselves can be composed of several binary
  scenery files (usually one for general scenery, then zero or more
  for airports), which we call subbuckets.  A bucket object can
  calculate intersections of a ray with the bucket.

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

#ifndef _BUCKET_H_
#define _BUCKET_H_

#include <plib/sg.h>
#include <simgear/misc/sg_path.hxx>
#include <simgear/io/sg_binobj.hxx>

#include "misc.hxx"
#include "Palette.hxx"

class Subbucket;

class Bucket {
public:
    // These variables are shared amongst all buckets and subbuckets
    // and influence how subbuckets are to be drawn.  Palette
    // determines material and contour colouring; discreteContours
    // controls whether contour levels should blend into each other;
    // contourLines toggles the drawing of contour lines; polygonEdges
    // toggles the drawing the outline of raw scenery polygons (this
    // is intended to be used for debugging only).
    //
    // If you change palette or discreteContours, you should notify
    // all buckets via paletteChanged() and discreteContoursChanged(),
    // and then redraw.  It's not necessary to notify buckets about
    // changes to contourLines or polygonEdges (because it doesn't
    // require any recalculations), but you still need to redraw.
    static Palette *palette;
    static bool discreteContours, contourLines, polygonEdges;

    Bucket(const SGPath &p, long int index);
    ~Bucket();

    enum Projection {CARTESIAN, RECTANGULAR};

    long int index() const { return _index; }
    const atlasSphere& bounds() const { return _bounds; }
    double centreLat() const { return _lat; }
    double centreLon() const { return _lon; }

    void load(Projection p = CARTESIAN);
    bool loaded() const { return _loaded; }
    void unload();
    unsigned int size() { return _size; }

    double maximumElevation() const { return _maxElevation; }

    void paletteChanged();
    void discreteContoursChanged();

    void draw();

    bool intersection(SGVec3<double> near, SGVec3<double> far, 
		      SGVec3<double> *c);

  protected:
    const SGPath& _p;		// Our scenery directory.
    long int _index;		// Bucket index.

    double _lat, _lon;		// The centre of this bucket.

    // EYE - we might want to move this out of here
    atlasSphere _bounds;

    // A bucket is divided into a bunch of pieces, which we call
    // subbuckets.
    vector<Subbucket *> _subbuckets;
    double _maxElevation;

    bool _loaded;
    unsigned int _size;		// Size of bucket (approximately) in bytes.
};

#endif // _BUCKET_H_
