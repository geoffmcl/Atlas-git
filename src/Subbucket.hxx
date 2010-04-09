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

#include <vector>
#include <deque>
#include <tr1/unordered_set>
#include <tr1/unordered_map>

#include <simgear/misc/sg_path.hxx>
#include <simgear/io/sg_binobj.hxx>

#include "Bucket.hxx"
#include "Palette.hxx"

// Create a hash function for pairs of integers.  This will be used in
// the _edgeMap unordered_map.
struct PairHash {
    size_t operator()(const std::pair<int, int>& x) const
    {
	// Note: the decision to use XOR to combine the two values was
	// made on the basis of pure ignorance.  There are, no doubt,
	// better ways.
	return std::tr1::hash<int>()(x.first ^ x.second);
    }
};

class Subbucket {
  public:
    Subbucket(const SGPath &p);
    ~Subbucket();

    bool load(Bucket::Projection p = Bucket::CARTESIAN);
    bool loaded() const { return _loaded; }
    void unload();
    unsigned int size() { return _size; }
    double maximumElevation() const { return _maxElevation; }

    void paletteChanged();
    void discreteContoursChanged();

    void draw();

  protected:
    void _palettize();
    void _chopTriangles(const int_list& triangles);
    void _chopTriangleStrip(const int_list& strip);
    void _chopTriangleFan(const int_list& fan);
    void _chopTriangle(int i0, int i1, int i2);
    std::pair<int, int> _chopEdge(int i0, int i1);
    void _createTriangle(int i0, int i1, int i2, bool cw, int e);
    void _checkTriangle(int i0, int i1, int i2, int e);
    void _doEdgeContour(int i0, int i1, int e0);
    void _addElevationSlice(std::deque<int>& vs, int e, bool cw);

    SGPath _path;
    bool _loaded, _airport;
    double _maxElevation;
    SGBinObject _chunk;
    // Vertices, normals, and elevations are all calculated directly
    // from the chunk.  We may add to these vectors if contours cut
    // through any chunk objects.  If there are n vertices, then the
    // size of the vertices and normals vectors is n * 3, while the
    // elevations vector is n.
    std::vector<float> _vertices, _normals;
    std::vector<float> _elevations;

    // These depend on the palette that we have loaded.  The elevation
    // indices vector stores contour indexes (as returned by the
    // palette), while the colours has smoothed RGBA colours (also as
    // returned by the palette).  If there are n vertices, then the
    // size of the elevation indices is n, while the colours vectors
    // is n * 4.
    std::vector<int> _elevationIndices;
    std::vector<float> _colours;

    // This is true if we've used the current palette to slice, dice,
    // and colour the subbucket triangles.
    bool _palettized;

    // The net result of drawing are these 4 display lists:
    // _materialsDL draws all triangles coloured by their material
    // (ie, not their elevation), _contoursDL draws all triangles
    // coloured by their elevation, _contourLinesDL draws all contour
    // lines, and _polygonEdgesDL draws outlines of all scenery
    // polygons (before being sliced).
    GLuint _materialsDL, _contoursDL, _contourLinesDL, _polygonEdgesDL;

    unsigned int _size;	// Size of subbucket (approximately) in bytes.

    // These contain references to all objects to be coloured by a
    // single material (eg, "water", "railroad", ...).  Each vector
    // contains a list of vertices, in GL_TRIANGLES format (ie, each
    // set of 3 indices represents one triangle).  The first map,
    // _materials, is for objects with a common normal; the second,
    // _materialsN, are for those objects with per-vertex normals.
    std::map<std::string, std::vector<GLuint> > _materials;
    std::map<std::string, std::vector<GLuint> > _materialsN;

    // To colour "contour" objects (objects coloured by their
    // elevation), we have to chop up triangles, strips, and fans into
    // simple triangles, then slice them if a contour line goes
    // through them (creating more, smaller, triangles).  We store
    // these triangles based on their colour index in the palette.
    // So, for example, _contours[3] has a list of vertex indices for
    // triangles coloured with the fourth contour colour.
    std::vector<int_list> _contours;
    
    // A list of vertex index pairs, each one representing a contour
    // line segment (ie, GL_LINES format).  This is used for drawing
    // contour lines.
    int_list _contourLines;
    // Similar to _contourLines, except for the edges of raw scenery
    // polygons.  This is intended for "debugging" scenery.
    int_list _polygonEdges;
    // A temporary varible used to handle a special case: contours
    // that run along a triangle edge (as opposed to cutting through a
    // triangle).  These may be shared by an adjacent triangle, and we
    // don't want to draw them twice, so we accumulate them in a set,
    // then add the contents of the set to _contourLines at the end.
    std::tr1::unordered_set<std::pair<int, int>, PairHash> _edgeContours;

    // This is used while slicing triangles along contour lines.
    // There is one entry per unique triangle edge (which is
    // identified by the vertex indices of the two endpoints).  For
    // each edge we store the beginning index of its intermediate
    // vertices (one vertex per contour passing through the edge), and
    // the number of intermediate vertices.  These vertices are
    // created sequentially.
    std::tr1::unordered_map<std::pair<int, int>, 
			    std::pair<int, int>, PairHash> _edgeMap;
};

#endif // _SUBBUCKET_H_
