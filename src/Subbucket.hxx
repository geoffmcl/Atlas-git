/*-------------------------------------------------------------------------
  Subbucket.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2013 Brian Schack

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

#include <deque>
#include <map>
#include <vector>
#include <set>
// A lot of the operations in Subbucket are very time-critical, so we
// use the TR1 unordered_set and unordered_map, as they are about
// twice as fast.
#if (defined(_MSC_VER) && !defined(HAVE_TRI_UNORDERED))
    #include <boost/tr1/unordered_set.hpp>
    #include <boost/tr1/unordered_map.hpp>
#else
    #include <tr1/unordered_set>
    #include <tr1/unordered_map>
#endif

#include <simgear/misc/sg_path.hxx> // SGPath
#include <simgear/io/sg_binobj.hxx> // SGBinObject

#include "Bucket.hxx"		// Bucket::Projection, ...

// Create a hash function for pairs of integers.  This will be used in
// several of the unordered sets and maps.
struct PairHash {
    size_t operator()(const std::pair<int, int>& x) const
    {
	// We use a simple hash function: the top half of the hash is
	// occupied by the first element in the pair, the bottom half
	// by the second (assuming ints are half the size of size_t,
	// although in the end the exact sizes aren't critical).
	size_t hash = x.first;
	// The sizeof(hash) * 4 expression equates to half the size of
	// the hash, in bits.  Thus we shift the bottom half of the
	// hash to the top half.
	hash = (hash << (sizeof(hash) * 4)) + x.second;

	return hash;
    }
};

class Subbucket {
  public:
    Subbucket(const SGPath &p);
    ~Subbucket();

    bool load(Bucket::Projection p = Bucket::CARTESIAN);
    bool loaded() const { return _loaded; }
    void unload();
    // The (very) approximate size of the subbucket, in bytes.
    unsigned int size();
    double maximumElevation() const { return _maxElevation; }

    void paletteChanged();
    void discreteContoursChanged();

    void draw();

  protected:
    // Normally I avoid typedefs, but this one is just too darned
    // long.  A VNMap maps from the <vertex, normal> pairs in the
    // original BTG file to our corrected indices.  It is used
    // internally in load() and _massageIndices().
    typedef std::tr1::unordered_map<std::pair<int, int>, int, PairHash> VNMap;
    void _massageIndices(const group_list &vertices, const group_list &normals, 
    			 VNMap &map, group_list &indices);

    void _palettize();
    void _chopTriangles(const std::vector<GLuint> &triangles);
    void _chopTriangle(int i0, int i1, int i2);
    std::pair<int, int> _chopEdge(int i0, int i1);
    void _createTriangle(int i0, int i1, int i2, bool cw, int e);
    void _checkTriangle(int i0, int i1, int i2, int e);
    void _doEdgeContour(int i0, int i1, int e0);
    void _addElevationSlice(std::deque<int> &vs, int e, bool cw);

    SGPath _path;
    bool _loaded;
    double _maxElevation;
    // Vertices, normals, and elevations for objects in the scenery
    // file.  The _vertices and _normals arrays are of the same size.
    // The nth vertex is an <x, y, z> triplet, and is at _vertices[n *
    // 3], _vertices[n * 3 + 1], and _vertices[n * 3 + 2].  The
    // corresponding normal for that vertex is at the same place in
    // the _normals array.  The elevation of that vertex is at
    // _elevations[n].
    std::vector<float> _vertices, _normals;
    std::vector<float> _elevations;

    // All of our triangles, indexed by material.  For example, all of
    // the "water" triangles can be found at _triangles["water"].  The
    // triangles are indices into _vertices, and _normals, stored in
    // GL_TRIANGLES format.  For example, _triangles["water"][n * 3]
    // is the index of the first vertex (in _vertices) and normal (in
    // _normals) of the nth triangle.  Note that _elevations[n] gives
    // the elevation of that vertex.
    //
    // Depending on the palette, some of these will be coloured by
    // their material, while others will be coloured by their
    // elevation.
    std::map<std::string, std::vector<GLuint> > _triangles;

    // These depend on the palette that we have loaded.  The elevation
    // indices vector stores contour indices (as returned by the
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

    // The number of vertices, normals, elevations, etc, before
    // contour chopping.
    unsigned int _size;

    // When we load a palette, we see which triangles (in _triangles)
    // are to be coloured by material - those materials are added to
    // this set.  When we actually draw the subbucket, we use this set
    // to index _triangles.
    std::set<std::string> _materials;

    // To colour "contour" objects (objects coloured by their
    // elevation), we have to slice triangles if a contour line goes
    // through them (creating more, smaller, triangles).  We store
    // these triangles based on their colour index in the palette.
    // So, for example, _contours[3] has a list of vertex indices for
    // triangles coloured with the fourth contour colour.
    std::vector<int_list> _contours;
    
    // A list of vertex index pairs, each one representing a contour
    // line segment (ie, GL_LINES format).  This is used for drawing
    // contour lines.
    int_list _contourLines;

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
