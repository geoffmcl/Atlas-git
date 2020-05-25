/*-------------------------------------------------------------------------
  Subbucket.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2018 Brian Schack

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
    //#include <boost/tri/unordered_set.hpp>
    //#include <boost/tri/unordered_map.hpp>
    #include <unordered_set>
    #include <unordered_map>
#else
    #include <tr1/unordered_set>
    #include <tr1/unordered_map>
#endif

#include <simgear/misc/sg_path.hxx> // SGPath
#include <simgear/io/sg_binobj.hxx> // SGBinObject

#include "Bucket.hxx"		// Bucket::Projection, ...

// The following classes (VBO, AttributeVBO, etc) simplify the
// management of OpenGL vertex buffer objects (VBOs).  Hopefully this
// will reduce the chances of falling into the many traps OpenGL sets
// for the unwary programmer.

// The VBO class is not meant to be used by itself - use one of the
// subclasses: VertexVBO, NormalVBO, ColourVBO, TrianglesVBO, or
// LinesVBO.  As hinted at by the name, the VertexVBO implements a
// vertex buffer object containing vertex data.  Ditto for NormalVBO,
// ColourVBO, TrianglesVBO and LinesVBO.  VBO is a subclass of the STL
// vector class, with a few extra bits thrown in to manage the OpenGL
// side of things.
//
// The basic usage is as follows:
//
// (1) Create the data.  Since VBO is a subclass of vector, this can
//     be done using the usual vector calls.  We override a few -
//     clear() and push_back() - for our and your convenience.
//
// (2) When ready to draw, 'stage' the appropriate attribute VBOs.
//     Staging uploads the data to the GPU, identifies its "type"
//     (vertex/normal/colour array) to OpenGL, and enables it (ie,
//     subsequent drawing will use that VBO).  You can explicitly
//     enable and disable VBOs as well.  You'll notice that enabling
//     and disabling are class methods, since they work on the
//     currently staged VBO of that class.  Disabling does not unstage
//     a VBO.
//
// (3) Draw.  This is done with an IndexVBO or one of its subclasses
//     (TrianglesVBO or LinesVBO).  The drawing will be done using
//     whatever attribute VBOs are enabled.  Drawing will upload the
//     indices to the GPU if it hasn't been done already.
//
//     You can go through several iterations of drawing for a set of
//     staged (and enabled) VBOs, and you can stage and draw several
//     times.  Just remember that when drawing, it will use the data
//     from the enabled VBOs, so get them right.
//
//     Note that all VBOs use GL_STATIC_DRAW for the usage hint.  As
//     well, after uploading, local data is deleted (if you do a
//     size() after staging or drawing, you'll see that it is empty).
//     The thinking behind this behaviour is that we don't want copies
//     eating memory locally and on the GPU.
//
// (4) If you need to look at or modify data that has been uploaded to
//     the GPU, download it.  If you don't want it all, you can
//     specify an initial range of the data to download with the
//     rawSize parameter.  Although we don't explicitly erase the data
//     on the GPU, it is no longer accessible through the class
//     methods provided.  Subsequent staging (attribute VBOs) or
//     drawing (index VBOs) will overwrite whatever is there.
//
//     If you don't need to download it, but need to indicate that the
//     uploaded data is no longer valid, call clear().  Note that this
//     will also clear any local data in the vector.
//
// (5) The VBO destructor will nicely clean up after you, so you don't
//     have to worry about buffers lying around on the GPU taking up
//     resources.
//
// In general, requests to VBOs will be silently ignored if they can't
// be fulfilled.  This means, for example, you can safely call draw()
// without checking if there's anything to draw.
template <class T>
class VBO: public std::vector<T> {
  public:
    VBO();
    ~VBO();

    // "Not a Raw Size" - when passed into the download() method,
    // indicates that all of the data (given by _size) should be
    // downloaded.
    static const size_t NaRS;

    // Move the data from the vector to the GPU, clearing the vector
    // after.
    void upload(GLenum target);
    // Download the data from the GPU to the vector.  If rawSize is
    // not NaRS, only the first rawSize objects will be downloaded.
    void download(GLenum target, size_t rawSize = NaRS);
    // Clear the data and mark the VBO as not uploaded.  If deleteVBO
    // is true, the VBO will be deleted as well, freeing all
    // resources.
    void clear(bool deleteVBO = false);

    // Returns true if some data has been uploaded to the GPU, without
    // being subsequently downloaded or cleared.
    bool uploaded();

  protected:
    GLuint _name;
    size_t _size;
};

// A base class for all attribute VBOs.  All attribute VBOs are
// floats, whether you like it or not (this is one of the privileges
// of being the one who writes the code).
class AttributeVBO: public VBO<GLfloat> {
  public:
    static void enable(GLenum cap);
    static void disable(GLenum cap);

    void stage();
    void download(size_t rawSize = NaRS);
};

// VertexVBOs are <x, y, z> triplets.  Although you aren't required
// to, you are suggested to use the supplied push_back() method to add
// elements.  By doing so you can be confident the VBO is in the
// correct format.
class VertexVBO: public AttributeVBO {
 public:
    static void enable();
    static void disable();

    void push_back(sgVec3 &v);
    void stage();
};

// NormalVBOs are <x, y, z> triplets
class NormalVBO: public AttributeVBO {
 public:
    static void enable();
    static void disable();

    void push_back(sgVec3 &v);
    void stage();
};

// ColourVBOs are RGBA quadruplets.
class ColourVBO: public AttributeVBO {
  public:
    static void enable();
    static void disable();

    void push_back(sgVec4 &v);
    void stage();
};

// A base class for all index VBOs.  Like attribute VBOs, you have no
// choice over data type or index VBOs - they are unsigned ints.  It
// would be nice to use unsigned shorts (they take half the space),
// but with the advent of Scenery 2.0, some subbucket/palette
// combinations exceed 65,636 vertices, the limit for unsigned shorts.
class IndexVBO: public VBO<GLuint> {
  public:
    void draw(GLenum mode);
    void download(size_t rawSize = NaRS);
};

// A TrianglesVBO implements a GL_TRIANGLES index array.  For
// convenience, it offers a push_back() method that takes 3 indices at
// a time.
class TrianglesVBO: public IndexVBO {
  public:
    void draw();
    void push_back(GLuint i0, GLuint i1, GLuint i2);
};

// A LinesVBO implements a GL_LINES index array.
class LinesVBO: public IndexVBO {
  public:
    void draw();
    void push_back(GLuint i0, GLuint i1);
};

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
    unsigned int size() const { return _bytes; }
    double maximumElevation() const { return _maxElevation; }

    void paletteChanged();

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
    VertexVBO _vertices;
    NormalVBO _normals;
    std::vector<float> _elevations;

    // All of our triangles, indexed by material.  For example, all of
    // the "water" triangles can be found at _triangles["water"].  The
    // triangles are indices into _vertices, and _normals, stored in
    // GL_TRIANGLES format.  For example, _triangles["water"][n * 3]
    // is the index of the first vertex (in _vertices) and normal (in
    // _normals) of the nth water triangle.  Note that _elevations[n]
    // gives the elevation of that vertex.
    //
    // Depending on the palette, some of these will be coloured by
    // their material, while others will be coloured by their
    // elevation.
    std::map<std::string, TrianglesVBO> _triangles;

    // These depend on the palette that we have loaded.  The elevation
    // indices vector stores contour indices (as returned by the
    // palette), while the colours has smoothed RGBA colours (also as
    // returned by the palette).  If there are n vertices, then the
    // size of the elevation indices is n, while the colours vectors
    // is n * 4.
    std::vector<int> _elevationIndices;
    ColourVBO _colours;

    // This is true if we've used the current palette to slice, dice,
    // and colour the subbucket triangles.
    bool _palettized;

    // The number of vertices, normals, elevations, etc, before
    // contour chopping.
    unsigned int _rawSize;

    // The approximate size of the loaded data, in bytes.
    unsigned int _bytes;

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
    std::vector<TrianglesVBO> _contours;
    
    // A list of vertex index pairs, each one representing a contour
    // line segment (ie, GL_LINES format).  This is used for drawing
    // contour lines.
    LinesVBO _contourLines;

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
