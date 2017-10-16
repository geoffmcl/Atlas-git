/*-------------------------------------------------------------------------
  Subbucket.cxx

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

// We include glew to get some function definitions that Linux doesn't
// automatically define (eg, glBindBuffer()).  Normally, a library
// include file would come after our own include file (Subbucket.hxx).
// However, glew insists on being included before gl.h, and something
// in Subbucket.hxx includes gl.h.
#include <GL/glew.h>

// Our include file
#include "Subbucket.hxx"

// Our project's include files
#include "Palette.hxx"

using namespace std;
using namespace tr1;

template <class T>
const size_t VBO<T>::NaRS = std::numeric_limits<size_t>::max();

template <class T>
VBO<T>::VBO(): _name(0), _size(0)
{
}

template <class T>
VBO<T>::~VBO()
{
    if (_name != 0) {
	glDeleteBuffers(1, &_name);
	_name = 0;
    }
    _size = 0;
}

template <class T>
// EYE - pass in usage hint?  Make it part of constructor?
void VBO<T>::upload(GLenum target)
{
    _size = vector<T>::size();

    if (_size == 0) {
	// We refuse to create zero-length VBOs.  Note that this
	// renders inaccessible any existing data on the GPU for this
	// VBO (at least using the methods in this class).  If you
	// want to erase the data on the GPU and locally, delete this
	// object.
	return;
    }

    if (_name == 0) {
	glGenBuffers(1, &_name);
    }

    glBindBuffer(target, _name);
    glBufferData(target, sizeof(T) * _size, vector<T>::data(), GL_STATIC_DRAW);

    vector<T>::clear();
}

template <class T>
void VBO<T>::download(GLenum target, size_t rawSize)
{
    if (!uploaded()) {
	// Note that download() will not overwrite whatever's in the
	// local vector if nothing's been uploaded.
	return;
    }

    if ((rawSize == NaRS) || (rawSize > _size)) {
	rawSize = _size;
    }

    T *ptr;
    glBindBuffer(target, _name);
    ptr = (T *)glMapBuffer(target, GL_READ_ONLY);
    assert(ptr);
    for (size_t i = 0; i < rawSize; i++) {
	vector<T>::push_back(ptr[i]);
    }
    // EYE - check return value?
    glUnmapBuffer(target);

    // Indicate that we have been unloaded.
    _size = 0;
}

template <class T>
void VBO<T>::clear(bool deleteVBO)
{
    _size = 0;
    vector<T>::clear();
    if (deleteVBO && (_name != 0)) {
	glDeleteBuffers(1, &_name);
	_name = 0;
    }
}

template <class T>
bool VBO<T>::uploaded()
{
    return (_size > 0);
}

void AttributeVBO::enable(GLenum cap)
{
    glEnableClientState(cap);
}

void AttributeVBO::disable(GLenum cap)
{
    glDisableClientState(cap);
}

void AttributeVBO::stage()
{
    if (!uploaded()) {
	if (size() == 0) {
	    return;
	}
	upload(GL_ARRAY_BUFFER);
    }
    glBindBuffer(GL_ARRAY_BUFFER, _name);
}

void AttributeVBO::download(size_t rawSize)
{
    VBO<GLfloat>::download(GL_ARRAY_BUFFER, rawSize);
}

void VertexVBO::enable()
{
    AttributeVBO::enable(GL_VERTEX_ARRAY);
}

void VertexVBO::disable()
{
    AttributeVBO::disable(GL_VERTEX_ARRAY);
}

void VertexVBO::push_back(sgVec3 &v)
{
    vector<GLfloat>::push_back(v[0]);
    vector<GLfloat>::push_back(v[1]);
    vector<GLfloat>::push_back(v[2]);
}

void VertexVBO::stage()
{
    AttributeVBO::stage();
    glVertexPointer(3, GL_FLOAT, 0, 0);
    enable();
}

void NormalVBO::enable()
{
    AttributeVBO::enable(GL_NORMAL_ARRAY);
}

void NormalVBO::disable()
{
    AttributeVBO::disable(GL_NORMAL_ARRAY);
}

void NormalVBO::push_back(sgVec3 &v)
{
    vector<GLfloat>::push_back(v[0]);
    vector<GLfloat>::push_back(v[1]);
    vector<GLfloat>::push_back(v[2]);
}

void NormalVBO::stage()
{
    AttributeVBO::stage();
    glNormalPointer(GL_FLOAT, 0, 0);
    enable();
}

void ColourVBO::enable()
{
    AttributeVBO::enable(GL_COLOR_ARRAY);
}

void ColourVBO::disable()
{
    AttributeVBO::disable(GL_COLOR_ARRAY);
}

void ColourVBO::push_back(sgVec4 &v)
{
    vector<GLfloat>::push_back(v[0]);
    vector<GLfloat>::push_back(v[1]);
    vector<GLfloat>::push_back(v[2]);
    vector<GLfloat>::push_back(v[3]);
}

void ColourVBO::stage()
{
    AttributeVBO::stage();
    glColorPointer(4, GL_FLOAT, 0, 0);
    enable();
}

void IndexVBO::draw(GLenum mode)
{
    if (!uploaded()) {
	if (size() == 0) {
	    return;
	}
	upload(GL_ELEMENT_ARRAY_BUFFER);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _name);
    glDrawElements(mode, _size, GL_UNSIGNED_INT, 0);
}

void IndexVBO::download(size_t rawSize)
{
    VBO<GLuint>::download(GL_ELEMENT_ARRAY_BUFFER, rawSize);
}

void TrianglesVBO::draw()
{
    IndexVBO::draw(GL_TRIANGLES);
}

void TrianglesVBO::push_back(GLuint i0, GLuint i1, GLuint i2)
{
    vector<GLuint>::push_back(i0);
    vector<GLuint>::push_back(i1);
    vector<GLuint>::push_back(i2);
}

void LinesVBO::draw()
{
    IndexVBO::draw(GL_LINES);
}

void LinesVBO::push_back(GLuint i0, GLuint i1)
{
    vector<GLuint>::push_back(i0);
    vector<GLuint>::push_back(i1);
}

Subbucket::Subbucket(const SGPath &p): 
    _path(p), _loaded(false), _palettized(false), _rawSize(0), _bytes(0)
{
}

Subbucket::~Subbucket()
{
}

bool Subbucket::load(Bucket::Projection projection)
{
    // Start with a clean slate.
    _vertices.clear();
    _normals.clear();
    _elevations.clear();
    _triangles.clear();
    _loaded = false;
    _palettized = false;

    // A BTG file contains a bunch of points in 3D cartesian space,
    // where the origin is at the centre of the earth, the X axis goes
    // through 0 degrees latitude, 0 degrees longitude (near Africa),
    // the Y axis goes through 0 degrees latitude, 90 degrees west
    // latitude (in the Indian Ocean), and the Z axis goes through the
    // north pole.  Units are metres.  See:
    //
    // http://www.flightgear.org/Docs/Scenery/CoordinateSystem/CoordinateSystem.html
    //
    // for more.  In the meantime, here are some useful notes about
    // the organization of BTG files, including a supposition about
    // normals not mentioned in the FlightGear documentation, as far
    // as I can tell:
    //
    // The actual vertex and normal data are in the wgs84_nodes and
    // normals vectors.  Points in wgs84_nodes have to be added to
    // gbs_center to get a true world cartesian coordinate.  We ignore
    // the colors and texcoords vectors.
    //
    // Triangles, strips, and fans are accessed with get_<foo>_v and
    // get_<foo>_n (where <foo> is "tris", "strips", or "fans").  Note
    // that these return a VECTOR OF VECTORS OF INDICES.
    //
    // So, get_fans_v()[i] returns the vertex indices for the ith
    // triangle fan, and get_fans_n()[i] does the same for its normals
    // (get_tris_v()[i] returns vertex indices for 1 or more
    // triangles).
    //
    // To save typing, we'll write get_<foo>_v()[i][j] (the jth vertex
    // index of the ith triangle thingy) as v[i][j].
    //
    // Remember, v[i][j] is a vertex INDEX.  The actual vertex is
    // wgs84_nodes[v[i][j]].
    //
    // And normals?  Although this is not documented as far as I can
    // tell, there seem to be two situations in BTG files:
    //
    // (a) v[i].size() == n[i].size()
    //
    //     If so, then:
    //
    //     wgs84_nodes[v[i][j]] and normals[n[i][j]] are the actual
    //     vertex and normal
    //
    //     and
    //
    //     v[i][j] != n[i][j] (generally, although they may
    //     coincidentally be the same).
    //
    // OR
    //
    // (b) n[i].size() == 0
    //
    //     If so, then:
    //
    //     wgs84_nodes[v[i][j]] and normals[v[i][j]] are the actual
    //     vertex and normal
    //
    // Some documentation on the BTG file format can be found at:
    //
    // http://wiki.flightgear.org/index.php/BTG_File_Format

    SGBinObject btg;
    if (!btg.read_bin(_path)) {
	// EYE - throw an error?
	// EYE - will the cache continue to call load() then?
	return false;
    }

    //////////////////////////////////////////////////////////////////////
    //
    // When we load a BTG file, we take the data we need, then convert
    // it into a format that makes OpenGL happy.  There are two big
    // conversions:
    //
    // (a) Converting BTG <vertex, normal> pairs into single indicees.
    //     In the BTG file, a particular point in a triangle might use
    //     the 7th vertex and the 21st normal.  We use OpenGL's
    //     glDrawElements, and it doesn't like that - it wants to use
    //     the 7th vertex and the 7th normal.
    //
    // (b) The BTG file includes triangles, triangle strips, and
    //     triangle fans (and just points, but we don't use those).
    //     To make programming easier, we convert everything to
    //     triangles (it also turns out the with the Scenery 2.0
    //     release, BTG files seem to exclusively use triangles
    //     anyway).
    //
    //////////////////////////////////////////////////////////////////////

    // Perform step (a) - create a map (vnMap) from the BTG file's
    // <vertex, normal> pairs to unique indices.  
    //
    // Also, since vertex and normal indexes are the same, we no
    // longer need the BTG file's separate vertex and normal indices
    // (ie, tris_v and tris_n, strips_v and strips_n, and fans_v and
    // fans_n).  Instead, we replace them with our own tris, strips,
    // and fans variables, which use the unique indices created in
    // vnMap.
    VNMap vnMap;
    group_list tris, strips, fans;
    _massageIndices(btg.get_tris_v(), btg.get_tris_n(), vnMap, tris);
    _massageIndices(btg.get_strips_v(), btg.get_strips_n(), vnMap, strips);
    _massageIndices(btg.get_fans_v(), btg.get_fans_n(), vnMap, fans);

    // The BTG file's tris_v, tris_n, strips_v, ... lists just contain
    // indices - the real data is in the wgs84_nodes (ie, vertices)
    // and normals lists.  Because our new tris, strips, and fans
    // lists have modified indices, we need modified vertex and normal
    // lists (_vertices and _normals).  As well, we want to record an
    // elevation for each point, so we have an _elevations list.
    // 
    // Because we fill them willy-nilly, first set them to the correct
    // size.
    _vertices.resize(vnMap.size() * 3);
    _normals.resize(vnMap.size() * 3);
    _elevations.resize(vnMap.size());

    const vector<SGVec3<double> > &wgs84_nodes = btg.get_wgs84_nodes();
    const SGVec3<double> &gbs_p = btg.get_gbs_center();
    const vector<SGVec3<float> >& m_norms = btg.get_normals();
    _maxElevation = -numeric_limits<double>::max();
    for (VNMap::const_iterator i = vnMap.begin(); i != vnMap.end(); i++) {
	// vn gives the <vertex, normal> pair in the BTG file; index
	// is our new index.
	pair<int, int> vn = i->first;
	int index = i->second;
	int v = vn.first;
	int n = vn.second;

	// Each BTG file has a reference point, given by
	// get_gbs_center().  All vertices within the BTG file are
	// relative to the reference point.  Therefore, to place
	// vertices in absolute 3D space, we need to add the reference
	// point to all vertices.
	//
	// Note that the documentation for BTG files says that
     	// vertices are stored as floats (and sg_binobj.cxx shows that
     	// that is true) - strange that it would serve them up as
     	// doubles.
	SGVec3<double> node = wgs84_nodes[v] + gbs_p;

	// Calculate lat, lon, and elevation.
	SGGeod geod = SGGeod::fromCart(node);

    	// Save our elevation.  This value is used to do elevation
    	// colouring, as well as calculate our maximum elevation
    	// figure.
	double e = geod.getElevationM();
	_elevations[index] = e;
	if (e > _maxElevation) {
	    _maxElevation = e;
	}

	// Note that the _vertices and _normals arrays contain <x, y,
	// z> triples - the ith vertex is at _vertices[i * 3],
	// _vertices[i * 3 + 1], and _vertices[i * 3 + 2] (ditto for
	// the ith normal).
	index *= 3;

    	// Now convert the point using the given projection.
	if (projection == Bucket::CARTESIAN) {
	    // This is a true 3D rendering.
	    // EYE - we go from doubles back to floats.  Wise?
	    _vertices[index] = node[0];
	    _vertices[index + 1] = node[1];
	    _vertices[index + 2] = node[2];
	} else if (projection == Bucket::RECTANGULAR) {
    	    // This is a flat projection.  X and Y are determined by
    	    // longitude and latitude, respectively.  We don't care
    	    // about Z, so set it to 0.0.  The colour will be
    	    // determined separately, and the shading will be
    	    // determined by the vertex normals.
	    _vertices[index] = geod.getLongitudeDeg();
	    _vertices[index + 1] = geod.getLatitudeDeg();
	    _vertices[index + 2] = 0.0;
	}

	const SGVec3<float>& normals = m_norms[n];
	_normals[index] = normals[0];
	_normals[index + 1] = normals[1];
	_normals[index + 2] = normals[2];
    }
    _maxElevation *= SG_METER_TO_FEET;
    
    // Perform step (b) - convert everything to plain old triangles.
    // These triangles go into the _triangles map based on their
    // material type (eg, all the "water" triangles are put in
    // _triangles["water"]).  
    //
    // Note that the BTG file doesn't use a C++ map as we do - it uses
    // parallel lists instead.  So, for example, if tri_materials[i]
    // is "water", then tris_v[i] contains all the water vertices.
    //
    // Note as well that we check if any two vertices of a triangle
    // are the same.  If they are, we throw out the triangle, since it
    // isn't really a triangle any more.  This may seem an odd check
    // to make, but in my experience, it's typical for 40% of all
    // triangles in Scenery 2.0 files to get tossed in this way.  They
    // don't seem to be limited to any particular kind of feature
    // (water, railroad, etc).  They also occur in Scenery 1.0, but
    // very rarely, and only in airports.

    // Triangles
    const string_list &tri_materials = btg.get_tri_materials();
    for (size_t i = 0; i < btg.get_tris_v().size(); i++) {
    	const int_list &oldTris = tris[i];
    	TrianglesVBO &newTris = _triangles[tri_materials[i]];
    	for (size_t j = 0; j < oldTris.size(); j += 3) {
    	    int i0 = oldTris[j], i1 = oldTris[j + 1], i2 = oldTris[j + 2];
	    // Check for zero-area triangle.
    	    if ((i0 == i1) || (i1 == i2) || (i2 == i0)) {
    		continue;
    	    }
    	    newTris.push_back(i0, i1, i2);
    	}
    }
    // Strips
    const string_list &strip_materials = btg.get_strip_materials();
    for (size_t i = 0; i < btg.get_strips_v().size(); i++) {
    	const int_list &oldStrips = strips[i];
    	TrianglesVBO &newTris = _triangles[strip_materials[i]];
    	for (size_t j = 0; j < oldStrips.size() - 2; j++) {
    	    int i0 = oldStrips[j], i1 = oldStrips[j + 1], i2 = oldStrips[j + 2];
	    // Check for zero-area triangle.
    	    if ((i0 == i1) || (i1 == i2) || (i2 == i0)) {
    		continue;
    	    }
    	    if (j % 2 == 0) {
    		newTris.push_back(i0, i1, i2);
    	    } else {
    		newTris.push_back(i1, i0, i2);
    	    }
    	}
    }
    // Fans
    const string_list &fan_materials = btg.get_fan_materials();
    for (size_t i = 0; i < btg.get_fans_v().size(); i++) {
    	const int_list &oldFans = fans[i];
    	TrianglesVBO &newTris = _triangles[fan_materials[i]];
    	int i0 = oldFans[0];
    	for (size_t j = 1; j < oldFans.size() - 1; j++) {
    	    int i1 = oldFans[j], i2 = oldFans[j + 1];
	    // Check for zero-area triangle.
    	    if ((i0 == i1) || (i1 == i2) || (i2 == i0)) {
    		continue;
    	    }
    	    newTris.push_back(i0, i1, i2);
    	}
    }
    
    // Record the "base" size - the number of raw vertices.
    _rawSize = _vertices.size() / 3;
    
    // Calculate our loaded size.  Note that this ignores the extra
    // stuff created when we contour chop, so is merely an
    // approximation.  First, vertices and normals.
    _bytes = _rawSize * sizeof(sgVec3) * 2;

    // Elevations.
    _bytes += _rawSize * sizeof(float);

    // Triangles (we ignore the string keys).
    map<string, TrianglesVBO>::const_iterator i;
    for (i = _triangles.begin(); i != _triangles.end(); i++) {
	const TrianglesVBO &tris = i->second;
	_bytes += tris.size() * sizeof(GLuint);
    }

    // Elevation indices and colours.
    _bytes += _rawSize * sizeof(int);
    _bytes += _rawSize * sizeof(float) * 4;

    _loaded = true;

    return true;
}

void Subbucket::unload()
{
    if (!_loaded) {
	// Nothing to do;
	assert(!_palettized);
	return;
    } 

    // When we're asked to unload, we need to actually get rid of
    // stuff - we're being asked to release resources.  So we shrink
    // ourselves down as much as possible.  We also need to delete
    // buffers on the GPU, so we clear the VBOs with the deleteVBO
    // flag set to true (VBOs that are deleted when the _triangles map
    // and _contours vector are cleared will also be deleted in their
    // destructors).
    _vertices.clear(true);
    _normals.clear(true);
    _elevations.clear();

    _triangles.clear();

    _elevationIndices.clear();
    _colours.clear(true);

    _materials.clear();

    _contours.clear();
    _contourLines.clear(true);

    _palettized = false;
    _loaded = false;
}

void Subbucket::paletteChanged() 
{
    if (!_palettized) {
	return;
    }

    // If we've palettized the scenery, then we'll have a bunch of
    // vertex buffer objects that need to be deleted.  But some of
    // this data (_vertices, _normals and _triangles) we'll need when
    // we process the new palette, so we need to grab it back from the
    // GPU before we delete it.

    // Get the vertices, ignoring the extra ones creating by contour
    // chopping.
    _vertices.download(_rawSize * 3);

    // Normals.
    _normals.download(_rawSize * 3);

    // Triangles.
    for (map<string, TrianglesVBO>::iterator i = _triangles.begin(); 
	 i != _triangles.end();
	 i++) {
	TrianglesVBO &indices = i->second;
	indices.download();
    }

    // Resize the elevations vector to match the restored vertices and
    // normals vectors.
    _elevations.resize(_rawSize);

    // We are now officially de-palettized.
    _palettized = false;
}

// This is a helper routine for the load() method.  For each unique
// <vertex, normal> pair, we add an entry to map consisting of that
// pair and our new index.  For each point in vertices/normals, we add
// a point to indices that uses that new index.  Note that vertices
// and normals, and therefore indices, are vectors of vectors.
//
// So, if vertices[3][7] = 22, and normals[3][7] = 76, then we check
// if map has an entry for <22, 76>.  If it doesn't, we add a new
// entry to map, using <22, 76> as the key, and a new index for the
// value (indices are generated sequentially from 0; for our example,
// let's say the index is 42).  We also add that index, 42, to
// indices[3].
void Subbucket::_massageIndices(const group_list &vertices, 
				const group_list &normals,
				VNMap &map, group_list &indices)
{
    // EYE - is this the preferred way to set the initial size?
    indices.resize(vertices.size());
    for (size_t i = 0; i < vertices.size(); i++) {
	assert(vertices.size() == normals.size());
	int_list &is = indices[i];
    	for (size_t j = 0; j < vertices[i].size(); j++) {
    	    int v = vertices[i][j];
	    int n = v;
	    if (normals[i].size() > 0) {
		// We have independently specified normals.
		assert(vertices[i].size() == normals[i].size());
		n = normals[i][j];
	    }
	    // Now that we have our <vertex, normal> pair, see if it's
	    // in the map already.  If not, add it.  Use the index of
	    // the pair in our 'is' list.
	    pair<int, int> vn(v, n);
	    int index;
	    VNMap::const_iterator vni = map.find(vn);
	    if (vni != map.end()) {
		index = vni->second;
	    } else {
		index = map.size();
		map[vn] = index;
	    }
	    is.push_back(index);
	}
    }
}


// A palette tells us how to colour a subbucket.  Colouring occurs for
// 2 reasons: (1) a triangle has a certain material type, or (2) a
// triangle has no material, and so is coloured according to its
// elevation.  This routine does that colouring (or rather, does all
// the calculations and sets up the arrays, which are then used by
// draw() to do the actual drawing).  There is one exception - it
// doesn't fill the _colours vector/VBO.  We assume that most people
// won't use smooth colouring; by not calculating colours we save some
// time and a fair bit of space (20%).  We also assume that palette
// changes will be rare.
//
// Note that in the case of contour colouring, we may need to
// subdivide the triangle if it spans a contour line.  Thus,
// "palettizing" may result in extra triangles (and therefore extra
// vertices, normals, and elevations).
//
// Whenever _palettize is called, we assume _vertices, _normals,
// _elevations, and _triangles are correctly initialized with the
// "base" data as loaded from the scenery file (before any
// palettization has taken place).
void Subbucket::_palettize()
{
    // Reset the data structures that palettization modifies.
    _elevationIndices.clear();
    _colours.clear();
    _materials.clear();
    _contours.clear();
    _contourLines.clear();

    // Initialize our elevation indices (more will be added later as
    // we contour chop).
    for (unsigned int i = 0; i < _elevations.size(); i++) {
    	// Note that it isn't strictly necessary to calculate these
    	// for all vertices - we only need to do it for vertices that
    	// are parts of "contour" objects (objects coloured according
    	// to elevation).  But this is easier to write.
    	_elevationIndices.push_back(Bucket::palette->contourIndex(_elevations[i]));
    }

    // Create a contour VBO for each contour interval.
    _contours.resize(Bucket::palette->size());

    // Look at our triangles, material by material.  If the material
    // is listed in the palette (ie, those triangles are to be
    // coloured according to the material colour), then just record
    // the material.  If the triangles are to be coloured by
    // elevation, then we need to chop them up.
    for (map<string, TrianglesVBO>::const_iterator i = _triangles.begin();
	 i != _triangles.end();
	 i++) {
	const string &material = i->first;
	const TrianglesVBO &tris = i->second;
	if (Bucket::palette->colour(material.c_str()) != NULL) {
	    _materials.insert(material);
	} else {
	    _chopTriangles(tris);
	}
    }
    _edgeMap.clear();

    // Add all contours that run along the shared edges of triangles
    // to the _contourLines vector.
    unordered_set<pair<int, int>, PairHash>::const_iterator i;
    for (i = _edgeContours.begin(); i != _edgeContours.end(); i++) {
	_contourLines.push_back(i->first, i->second);
    }
    _edgeContours.clear();

    _palettized = true;
}

void Subbucket::draw()
{
    // We can only draw this subbucket if we've actually loaded it and
    // we have a palette.
    if (!_loaded || (Bucket::palette == NULL)) {
    	return;
    }
    if (!_palettized) {
	_palettize();
    }

    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT); {
	// Tell OpenGL where our data is.
	_vertices.stage();
	_normals.stage();

	// Just in case a colour array is still enabled, disable it
	// explicitly.
	ColourVBO::disable();

	// ---------- Materials ----------
	// Draw the "material" objects (ie, objects coloured based on
	// their material, not elevation).  Note that we assume that
	// glColorMaterial() has been called.
	for (set<string>::const_iterator i = _materials.begin();
	     i != _materials.end();
	     i++) {
	    const string &material = *i;
	    TrianglesVBO &indices = _triangles[material];

	    glColor4fv(Bucket::palette->colour(material.c_str()));
	    indices.draw();
	}

	// ---------- Contours ----------
	if (!Bucket::discreteContours) {
	    // We delay calculating smooth colour information as long
	    // as possible.
	    if (!_colours.uploaded()) {
		for (unsigned int i = 0; i < _elevations.size(); i++) {
		    // EYE - we should be able to do this in a shader
		    sgVec4 colour;
		    Bucket::palette->smoothColour(_elevations[i], colour);
		    _colours.push_back(colour);
		}
	    }
	    _colours.stage();
	}
	for (size_t i = 0; i < _contours.size(); i++) {
	    TrianglesVBO &indices = _contours[i];
	    if (Bucket::discreteContours) {
		glColor4fv(Bucket::palette->contourAtIndex(i).colour);
	    }
	    indices.draw();
	}

	// EYE - To reduce the number of times we turn the depth test
	// on and off (assuming that slows things down), we might want
	// to have separate draw() methods, one for materials and
	// contours (which use the depth buffer), another for contour
	// lines and polygon edges (which don't).

	// ---------- Contour lines ----------
	NormalVBO::disable();
	ColourVBO::disable();
	if (Bucket::contourLines) {
	    glPushAttrib(GL_LINE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT); {
	    	glDisable(GL_DEPTH_TEST);
		glLineWidth(0.5);
		glColor4f(0.0, 0.0, 0.0, 1.0);
		_contourLines.draw();
	    }
	    glPopAttrib();
	}

        // ---------- Polygon edges ----------
	if (Bucket::polygonEdges) {
	    glPushAttrib(GL_LINE_BIT | GL_POLYGON_BIT | GL_CURRENT_BIT | 
	    		 GL_DEPTH_BUFFER_BIT); {
	    	glDisable(GL_DEPTH_TEST);
		glPolygonMode(GL_FRONT, GL_LINE);
		glLineWidth(0.5);
		glColor4f(1.0, 0.0, 0.0, 1.0);
		map<string, TrianglesVBO>::iterator i;
		for (i = _triangles.begin(); i != _triangles.end(); i++) {
		    TrianglesVBO &indices = i->second;
		    indices.draw();
		}
	    }
	    glPopAttrib();
	}
    }
    glPopClientAttrib();
}

// Takes a vector representing a series of triangles (ie, GL_TRIANGLES
// format) and chops each triangle along contour lines.
void Subbucket::_chopTriangles(const vector<GLuint> &triangles)
{
    for (size_t i = 0; i < triangles.size(); i += 3) {
	_chopTriangle(triangles[i], triangles[i + 1], triangles[i + 2]);
    }
}

// Chops up the given triangle along contour lines.  This will result
// in new vertices, normals, elevations, and elevation indices being
// added to _vertices, _normals, _elevations, and _elevationIndices,
// and triangles being added to _contours.  Also, we add contour line
// segments to _contourLines.
void Subbucket::_chopTriangle(int i0, int i1, int i2)
{
    // It's very important to keep the number of new vertices (and
    // triangles) to a minimum.  I initially tried a simple recursive
    // routine, which worked, but it created far too many new vertices
    // (more than 10 times as many, in some cases).  The following
    // code is more complicated, but way more efficient.

    // An idea of the algorithm can be illustrated with the power of
    // ASCII graphics.  If two contours cut through triangle i0-i1-i2,
    // we'll create 4 new points, A, B, C, and D.  This creates 3
    // figures: i0-A-B, B-A-i1-C-D, and D-C-i2.
    //
    //   i2       i2       i2       i2       i2
    //   |\       |\       |\       |\       |\
    //   | \      | \      | \      | \      | \
    //-->|  \  -->|  \  -->|  \     D--C     D--C
    //   |   \    |   \    |   \    |   \
    //   |    i1  |    i1  |    i1  |    i1
    //   |   /    |   /    |   /    |   /
    //   |  /     |  /     |  /     |  /
    //-->| /      B-A      B-A      B-A
    //   |/       |/     
    //   i0       i0
    //
    // Note that other figures are possible depending on where the
    // contours cut through the triangle and the orientation of the
    // triangle.  We also have to be careful about the case when a
    // contour cuts through i1.  Finally, the edges of the triangle
    // itself might lie along contours, so they have to be checked as
    // well.

    // We start from the bottom vertex and work our way upward (doing
    // so makes the logic easier), so we first relabel our triangle so
    // that i0 is the lowest vertex, i1 is in the middle, and i2 is at
    // the top.  We may have to flip the triangle to do this, so we
    // keep track of our winding.
    bool cw = false;
    if (_elevations[i0] > _elevations[i1]) {
	swap(i0, i1);
	cw = !cw;
    }
    if (_elevations[i0] > _elevations[i2]) {
	swap(i0, i2);
	cw = !cw;
    }
    if (_elevations[i1] > _elevations[i2]) {
	swap(i1, i2);
	cw = !cw;
    }
    int e0 = _elevationIndices[i0], 
	e1 = _elevationIndices[i1], 
	e2 = _elevationIndices[i2];
    assert(e0 <= e1);
    assert(_elevations[i0] <= _elevations[i1]);
    assert(e1 <= e2);
    assert(_elevations[i1] <= _elevations[i2]);

    // Now chop the edges.  We call _chopEdge, which finds all the
    // points along an edge through which a contour passes and creates
    // vertices (and normals and colours) for them.  It returns a pair
    // of ints, the first being the index (in _vertices, _normals, and
    // so on) of the first vertex created, the second being the number
    // of vertices created (the vertices are created sequentially).
    pair<int, int> i0i1, i1i2, i0i2;
    i0i1 = _chopEdge(i0, i1);
    i1i2 = _chopEdge(i1, i2);
    i0i2 = _chopEdge(i0, i2);

    // The number of crossing contours on the left (i0i2.second) should
    // equal the number on the right (i0i1.second + i1i2.second).  If i1
    // happens to be lying on a contour line, it must be counted as
    // well.
    assert(((i0i1.second + i1i2.second) == i0i2.second) ||
	   ((i0i1.second + i1i2.second + 1) == i0i2.second));

    // The algorithm works by accumulating vertices in a deque (vs) as
    // we move up the triangle, drawing figures when we've accumulated
    // enough, and then throwing out all but the last two vertices.
    // So, using the example above, we would add i0, then A and B,
    // draw a triangle, toss out i0 (leaving A and B), add i1, add C
    // and D, draw a pentagon (using 3 triangles), toss out A, B, and
    // i1, add i2, and draw the final triangle.
    deque<int> vs;
    vs.push_back(i0);

    int e = e0;
    // Deal with contours passing through the bottom half of the
    // triangle (below i1).
    for (; i0i1.second > 0; i0i1.second--, i0i2.second--) {
	// These two points cut through the triangle, creating a
	// contour line.
	_contourLines.push_back(i0i1.first, i0i2.first);

	// Add the two points to the previously existing ones.  This
	// will create a slice through the triangle at one contour
	// level.
	vs.push_back(i0i1.first++);
	vs.push_back(i0i2.first++);
	_addElevationSlice(vs, e++, cw);
    }
    // Add i1.
    vs.push_back(i1);
    // If i1 is on a contour and has an opposite along i0i2, add the
    // opposite.
    if ((i1i2.second + 1) == i0i2.second) {
	_contourLines.push_back(i1, i0i2.first);

	vs.push_back(i0i2.first++);
	i0i2.second--;
	_addElevationSlice(vs, e++, cw);
    }

    // Deal with the top half of the triangle (above i1).
    for (; i1i2.second > 0; i1i2.second--, i0i2.second--) {
	_contourLines.push_back(i1i2.first, i0i2.first);

	vs.push_back(i1i2.first++);
	vs.push_back(i0i2.first++);
	_addElevationSlice(vs, e++, cw);
    }
    vs.push_back(i2);

    // Draw the last slice.
    _addElevationSlice(vs, e++, cw);

    // Now a special case.  The above code handles contours cutting
    // through the triangle, but not contours that run along the edge
    // of the triangle.  These are special because the edge will be
    // shared by an adjacent triangle, and we don't want to draw it
    // twice.  So we accumulate them in a set, then, when we're done,
    // we add the contents of the set to _contourLines.
    _doEdgeContour(i0, i1, e0);
    _doEdgeContour(i1, i2, e1);
    _doEdgeContour(i0, i2, e0);
}

// Creates a single triangle from the given points.  This means adding
// three points to the _contours vector at the correct contour index
// (as given by e).  If cw ('clockwise') is true, we swap i0 and i1 so
// that the triangle will be rendered counterclockwise.
void Subbucket::_createTriangle(int i0, int i1, int i2, bool cw, int e)
{
    TrianglesVBO &indices = _contours[e];
    if (cw) {
	indices.push_back(i1, i0, i2);
    } else {
	indices.push_back(i0, i1, i2);
    }
}

// Given a single slice of a triangle, as given in vs, creates one,
// two, or three triangles to represent it (if you look at the
// documentation for _chopTriangle, vs would contain a figure like
// B-A-i1-C-D, for example).  If cw is true, we reverse the order of
// the first two vertices to restore a counterclockwise winding.
void Subbucket::_addElevationSlice(deque<int>& vs, int e, bool cw)
{
    assert((vs.size() >= 3) && (vs.size() <= 5));
    if (vs.size() == 3) {
	// Triangle
	_createTriangle(vs[0], vs[1], vs[2], cw, e);
    } else if (vs.size() == 4) {
	// Quadrilateral
	_createTriangle(vs[0], vs[1], vs[2], cw, e);
	_createTriangle(vs[0], vs[2], vs[3], cw, e);
    } else {
	// Pentagon
	_createTriangle(vs[0], vs[1], vs[2], cw, e);
	_createTriangle(vs[0], vs[2], vs[4], cw, e);
	_createTriangle(vs[2], vs[3], vs[4], cw, e);
    }

    // Now clean up.  We want to get rid of all vertices except the
    // last two, which were just added and which will form the base of
    // the next figure.  Also, because we want consistent winding, we
    // reverse their order.
    vs.erase(vs.begin(), vs.end() - 2);
    swap(vs[0], vs[1]);
}

// Checks if the edge connecting i0 and i1 (at elevation index e0)
// runs along a contour.  If it does, it adds it to the _edgeContours
// set.  Note that this is only intended to be called for the edges of
// raw triangles, not the contours created by slicing triangles.
void Subbucket::_doEdgeContour(int i0, int i1, int e0)
{
    if ((_elevations[i0] == _elevations[i1]) &&
    	(_elevations[i0] == Bucket::palette->contourAtIndex(e0).elevation)) {
    	if (i0 <= i1) {
    	    _edgeContours.insert(make_pair(i0, i1));
    	} else {
    	    _edgeContours.insert(make_pair(i1, i0));
    	}
    }
}

// Creates new vertices (and elevations, and colours, and normals,
// ...) for each point between i0 and i1 that lies on a contour.
// Returns a pair giving the starting index of the first vertex
// created, and the number of vertices created.  If no vertices are
// created, it returns <0, 0>.  Can be safely called multiple times on
// the same <i0, i1> pair.
//
// Note that this is where we spend most of our time (about 50%), so
// it's imperative that it be as efficient as possible.
pair<int, int> Subbucket::_chopEdge(int i0, int i1)
{
    pair<int, int> result(0, 0);

    // For convenience, and to make sure we name edges consistently,
    // we force i0 to be lower (topographically) than i1.  If they're
    // at the same elevation, we sort by index.
    if (_elevations[i0] > _elevations[i1]) {
    	swap(i0, i1);
    } else if ((_elevations[i0] == _elevations[i1]) && (i0 > i1)) {
	swap(i0, i1);
    }

    // We should never get an edge where the two endpoints are the
    // same (these are explicitly filtered out in load()).
    assert(i0 != i1);

    // Now we're ready.  First see if the edge has been processed
    // already.  The following code is a bit of a trick, relying on
    // two things: (1) New vertices we create will never have an index
    // of 0, and (2) If the given edge doesn't exist, _edgeMap[]
    // returns <0, 0> (ie, a newly created int is given a default
    // value of 0).
    result = _edgeMap[make_pair(i0, i1)];
    if (result.first != 0) {
	return result;
    }

    int e0 = _elevationIndices[i0], e1 = _elevationIndices[i1];
    float elev0 = _elevations[i0], elev1 = _elevations[i1];

    // If i0 and i1 are at the same level, then we can't add any points.
    // EYE - compare e0 and e1 instead?
    if (elev0 == elev1) {
	// Check if it forms part of a contour line.  If so, record
	// the contour.
    	if (Bucket::palette->contourAtIndex(e0).elevation == elev0) {
	    _contourLines.push_back(i0, i1);
	}

	return result;
    }

    result.first = _vertices.size() / 3;
    if (elev1 != Bucket::palette->contourAtIndex(e1).elevation) {
	// If the upper point is not on a contour (ie, it's above it),
	// then we need to create a vertex for that contour too.
	e1++;
    }
    int i = result.first;
    for (int e = e0 + 1; e < e1; e++, i++) {
	const Palette::Contour& contour = Bucket::palette->contourAtIndex(e);
	_elevations.push_back(contour.elevation);
	_elevationIndices.push_back(e);

	// Calculate a scale factor.  Strictly speaking, linear
	// interpolation is not correct, because the earth is not
	// flat.  If you have an edge between two points, one with an
	// elevation of 150 metres and the other with an elevation of
	// 50 metres, the midpoint probably won't be at an elevation
	// of 100 metres - it will probably be less.  The error
	// increases as the distance between the two points increase.
	// However, for our purposes, it's Close Enough (tm).
	float scaling = (contour.elevation - elev0) / (elev1 - elev0);
	// EYE - really it should be '<' not '<=', but roundoff errors
	// sometimes force it to 0.0 or 1.0.  Perhaps we should intercept
	// this earlier to prevent it from happening.
	assert((0.0 <= scaling) && (scaling <= 1.0));

	// Interpolate the vertex.
	sgVec3 v;
	float *v0 = &(_vertices[i0 * 3]), *v1 = &(_vertices[i1 * 3]);
	sgSubVec3(v, v1, v0);
	sgScaleVec3(v, scaling);
	sgAddVec3(v, v0);
	_vertices.push_back(v);

	// Interpolate the normal.
	sgVec3 n;
	float *n0 = &(_normals[i0 * 3]), *n1 = &(_normals[i1 * 3]);
	sgSubVec3(n, n1, n0);
	sgScaleVec3(n, scaling);
	sgAddVec3(n, n0);
	_normals.push_back(n);

	result.second++;
    }

    _edgeMap[make_pair(i0, i1)] = result;

    return result;
}
