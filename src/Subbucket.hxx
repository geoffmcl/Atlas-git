/*-------------------------------------------------------------------------
  Subbucket.hxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

  A subbucket is a part of a bucket (which is a part of a tile, blah,
  blah, blah).  It contains the polygon information for a single
  SimGear binary scenery (.btg) file.  When we draw scenery, *this* is
  what eventually puts polygons on the screen.

  A note on names: sometimes these are referred to as chunks.  I
  considered calling them chunks, but after a while the names seem so
  arbitrary; what's bigger - a bucket or a chunk or tile?  The names
  don't give much of a clue.  Subbucket seems clearer (and it's more
  fun to say).

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

#ifndef _SUBBUCKET_H_
#define _SUBBUCKET_H_

#include <simgear/misc/sg_path.hxx>
#include <simgear/io/sg_binobj.hxx>

#include "Bucket.hxx"
#include "Palette.hxx"

class Subbucket {
  public:
    Subbucket(const SGPath &p);
    ~Subbucket();

    bool load(Bucket::Projection p = Bucket::CARTESIAN);
    bool loaded() const { return _loaded; }
    void unload();

    double maximumElevation() const { return _maxElevation; }

    void draw(Palette *palette, bool discreteContours = true);
  protected:
    void _drawTristrip(const int_list &vertex_indices, 
		       const int_list &normal_indices,
		       const float *col);
    void _drawTrifan(const int_list &vertex_indices, 
		     const int_list &normal_indices, 
		     const float *col);
    void _drawTris(const int_list &vertex_indices, 
		   const int_list &normal_indices, 
		   const float *col);
    void _drawTri(int vert0, int vert1, int vert2,
		  int norm0, int norm1, int norm2,
		  const float *col);
    void _drawElevationTri(int vert0, int vert1, int vert2,
			   int norm0, int norm1, int norm2);

    SGPath _path;
    bool _loaded;
    double _maxElevation;
    SGBinObject _chunk;
    // EYE - change to SGVec3?
    vector<float *> _vertices, _normals;
    vector<float> _elevations;
};

#endif // _SUBBUCKET_H_
