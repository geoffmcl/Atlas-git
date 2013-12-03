/*-------------------------------------------------------------------------
  Subbucket.cxx

  Written by Brian Schack

  Copyright (C) 2009 - 2013 Brian Schack

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
#include "Subbucket.hxx"

// Our project's include files
#include "Palette.hxx"

using namespace std;
using namespace tr1;

Subbucket::Subbucket(const SGPath &p): 
    _path(p), _loaded(false), _palettized(false),
    _materialsDL(0), _contoursDL(0), _contourLinesDL(0), 
    _polygonEdgesDL(0), _size(0)
{
}

Subbucket::~Subbucket()
{
    unload();
}

bool Subbucket::load(Bucket::Projection projection)
{
    // Clear out whatever was there first.
    unload();

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
    if (!btg.read_bin(_path.c_str())) {
	// EYE - throw an error?
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
	v *= 3;
    	// Now convert the point using the given projection.
	if (projection == Bucket::CARTESIAN) {
	    // This is a true 3D rendering.
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
    _triangles.clear();

    // Triangles
    const string_list &tri_materials = btg.get_tri_materials();
    for (size_t i = 0; i < btg.get_tris_v().size(); i++) {
    	const int_list &oldTris = tris[i];
    	vector<GLuint> &newTris = _triangles[tri_materials[i]];
    	for (size_t j = 0; j < oldTris.size(); j += 3) {
    	    int i0 = oldTris[j], i1 = oldTris[j + 1], i2 = oldTris[j + 2];
	    // Check for zero-area triangle.
    	    if ((i0 == i1) || (i1 == i2) || (i2 == i0)) {
    		continue;
    	    }
    	    newTris.push_back(i0);
    	    newTris.push_back(i1);
    	    newTris.push_back(i2);
    	}
    }
    // Strips
    const string_list &strip_materials = btg.get_strip_materials();
    for (size_t i = 0; i < btg.get_strips_v().size(); i++) {
    	const int_list &oldStrips = strips[i];
    	vector<GLuint> &newTris = _triangles[strip_materials[i]];
    	for (size_t j = 0; j < oldStrips.size() - 2; j++) {
    	    int i0 = oldStrips[j], i1 = oldStrips[j + 1], i2 = oldStrips[j + 2];
	    // Check for zero-area triangle.
    	    if ((i0 == i1) || (i1 == i2) || (i2 == i0)) {
    		continue;
    	    }
    	    if (j % 2 == 0) {
    		newTris.push_back(i0);
    		newTris.push_back(i1);
    		newTris.push_back(i2);
    	    } else {
    		newTris.push_back(i1);
    		newTris.push_back(i0);
    		newTris.push_back(i2);
    	    }
    	}
    }
    // Fans
    const string_list &fan_materials = btg.get_fan_materials();
    for (size_t i = 0; i < btg.get_fans_v().size(); i++) {
    	const int_list &oldFans = fans[i];
    	vector<GLuint> &newTris = _triangles[fan_materials[i]];
    	int i0 = oldFans[0];
    	for (size_t j = 1; j < oldFans.size() - 1; j++) {
    	    int i1 = oldFans[j], i2 = oldFans[j + 1];
	    // Check for zero-area triangle.
    	    if ((i0 == i1) || (i1 == i2) || (i2 == i0)) {
    		continue;
    	    }
    	    newTris.push_back(i0);
    	    newTris.push_back(i1);
    	    newTris.push_back(i2);
    	}
    }
    
    // Record the "base" size - the number of raw vertices.
    _size = _vertices.size() / 3;
    
    _loaded = true;

    return true;
}

void Subbucket::unload()
{
    _vertices.clear();
    _normals.clear();
    _elevations.clear();
    _elevationIndices.clear();

    glDeleteLists(_materialsDL, 1);
    _materialsDL = 0;
    glDeleteLists(_contoursDL, 1);
    _contoursDL = 0;
    glDeleteLists(_contourLinesDL, 1);
    _contourLinesDL = 0;
    glDeleteLists(_polygonEdgesDL, 1);
    _polygonEdgesDL = 0;

    _loaded = false;
}

// The (very) approximate size of the subbucket, in bytes.  Basically
// we return the base size of the _vertices, _normals, _elevations,
// _triangles, _elevationIndices, and _colours vectors, *ignoring* the
// extra stuff created by contour chopping.
unsigned int Subbucket::size()
{
    unsigned int result = 0;

    // Vertices and normals.
    result += _size * sizeof(sgVec3) * 2;

    // Elevations.
    result += _size * sizeof(float);

    // Triangles (we ignore the string keys).
    map<string, vector<GLuint> >::const_iterator i;
    for (i = _triangles.begin(); i != _triangles.end(); i++) {
	const vector<GLuint> &tris = i->second;
	result += tris.size() * sizeof(GLuint);
    }

    // Elevation indices and colours.
    result += _size * sizeof(int);
    result += _size * sizeof(float) * 4;

    return result;
}

// Called to notify us that the palette in Bucket::palette has
// changed.  We need to delete the display lists that depend on the
// palette (that's all of them except _polygonEdgesDL) and flag that
// we need to re-palettize.
void Subbucket::paletteChanged() 
{
    glDeleteLists(_materialsDL, 1);
    _materialsDL = 0;
    glDeleteLists(_contoursDL, 1);
    _contoursDL = 0;
    glDeleteLists(_contourLinesDL, 1);
    _contourLinesDL = 0;

    _palettized = false;
}

// Called to notify us that Bucket::discreteContours has changed.  We
// need to delete the contours display list (but not the materials or
// contour lines display lists).
void Subbucket::discreteContoursChanged() 
{
    glDeleteLists(_contoursDL, 1);
    _contoursDL = 0;
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
// draw() to do the actual drawing).
//
// In the case of (2), we may need to subdivide the triangle if it
// spans a contour line.  Thus, "palettizing" may result in extra
// triangles (and therefore extra vertices, normals, and elevations).
void Subbucket::_palettize()
{
    // Because we've loaded a new palette, we have to get rid of all
    // the extra vertices (and their normals, elevations, ...) created
    // by contour slicing.  The "base" data (directly loaded from the
    // file) remains valid however.
    _vertices.resize(_size * 3);
    _normals.resize(_size * 3);
    _elevations.resize(_size);

    // Elevation indices and colours don't remain valid - they depend
    // on the palette.
    _elevationIndices.clear();
    _colours.clear();
    for (unsigned int i = 0; i < _elevations.size(); i++) {
	// Note that it isn't strictly necessary to calculate these
	// for all vertices - we only need to do it for vertices that
	// are parts of "contour" objects (objects coloured according
	// to elevation).  But this is easier to write.
	_elevationIndices.push_back(Bucket::palette->contourIndex(_elevations[i]));
	sgVec4 colour;
	Bucket::palette->smoothColour(_elevations[i], colour);
	_colours.push_back(colour[0]);
	_colours.push_back(colour[1]);
	_colours.push_back(colour[2]);
	_colours.push_back(colour[3]);
    }

    // Now start slicing and dicing.
    _materials.clear();
    _contours.clear();
    _contours.resize(Bucket::palette->size());
    _contourLines.clear();

    // Look at our triangles, material by material.  If the material
    // is listed in the palette (ie, those triangles are to be
    // coloured according to the material colour), then just record
    // the material.  If the triangles are to be coloured by
    // elevation, then we need to chop them up.
    for (map<string, vector<GLuint> >::const_iterator i = _triangles.begin();
	 i != _triangles.end();
	 i++) {
	const string &material = i->first;
	const vector<GLuint> &tris = i->second;
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
	_contourLines.push_back(i->first);
	_contourLines.push_back(i->second);
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

    // Tell OpenGL where our data is.
    glVertexPointer(3, GL_FLOAT, 0, _vertices.data());
    glNormalPointer(GL_FLOAT, 0, _normals.data());
    glColorPointer(4, GL_FLOAT, 0, _colours.data());

    // Now create the display lists.
    // ---------- Materials ----------
    if (_materialsDL == 0) {
	_materialsDL = glGenLists(1);
	assert(_materialsDL != 0);

	glNewList(_materialsDL, GL_COMPILE);
	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT); {
	    glEnableClientState(GL_VERTEX_ARRAY);
	    glEnableClientState(GL_NORMAL_ARRAY);
	    glDisableClientState(GL_COLOR_ARRAY);

	    // Draw the "material" objects (ie, objects coloured based on
	    // their material, not elevation).  Note that we assume that
	    // glColorMaterial() has been called.
	    for (set<string>::const_iterator i = _materials.begin();
		 i != _materials.end();
		 i++) {
		const string &material = *i;
		vector<GLuint> &triangles = _triangles[material];
		if (!triangles.empty()) {
		    glColor4fv(Bucket::palette->colour(material.c_str()));
		    glDrawElements(GL_TRIANGLES, triangles.size(),
				   GL_UNSIGNED_INT, triangles.data());
		}
	    }
	}
	glPopClientAttrib();
	glEndList();
    }

    // ---------- Contours ----------
    if (_contoursDL == 0) {
	_contoursDL = glGenLists(1);
	assert(_contoursDL != 0);

	glNewList(_contoursDL, GL_COMPILE);
	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT); {
	    glEnableClientState(GL_VERTEX_ARRAY);
	    glEnableClientState(GL_NORMAL_ARRAY);

	    // Draw "contours" objects (ie, objects coloured based on
	    // their elevation).

	    // EYE - we could also generate two display lists - one
	    // for discrete contours, the other for smoothed contours,
	    // but this seems like overkill.
	    if (Bucket::discreteContours) {
		glDisableClientState(GL_COLOR_ARRAY);
	    } else {
		glEnableClientState(GL_COLOR_ARRAY);
	    }
	    for (size_t i = 0; i < _contours.size(); i++) {
		if (Bucket::discreteContours) {
		    glColor4fv(Bucket::palette->contourAtIndex(i).colour);
		}
	
		int_list& indices = _contours[i];
		if (!indices.empty()) {
		    glDrawElements(GL_TRIANGLES, indices.size(), 
				   GL_UNSIGNED_INT, indices.data());
		}
	    }
	}
	glPopClientAttrib();
	glEndList();
    }

    // ---------- Contour lines ----------
    // Contour lines and especially polygon edges are probably used
    // rarely, so only create display lists for them if explicitly
    // asked and we actually have some.
    if ((_contourLinesDL == 0) && Bucket::contourLines && 
    	!_contourLines.empty()) {
	_contourLinesDL = glGenLists(1);
	assert(_contourLinesDL != 0);

	glNewList(_contourLinesDL, GL_COMPILE);
	glPushAttrib(GL_LINE_BIT | GL_CURRENT_BIT);
	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT); {
	    glEnableClientState(GL_VERTEX_ARRAY);
	    glDisableClientState(GL_NORMAL_ARRAY);
	    glDisableClientState(GL_COLOR_ARRAY);

	    glLineWidth(0.5);
	    glColor4f(0.0, 0.0, 0.0, 1.0);
	    glDrawElements(GL_LINES, _contourLines.size(), GL_UNSIGNED_INT,
	    		   _contourLines.data());
	}
	glPopClientAttrib();
	glPopAttrib();
	glEndList();
    }

    // ---------- Polygon edges ----------
    if ((_polygonEdgesDL == 0) && Bucket::polygonEdges) {
    	_polygonEdgesDL = glGenLists(1);
    	assert(_polygonEdgesDL != 0);

    	glNewList(_polygonEdgesDL, GL_COMPILE);
    	glPushAttrib(GL_LINE_BIT | GL_POLYGON_BIT | GL_CURRENT_BIT);
    	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT); {
    	    glEnableClientState(GL_VERTEX_ARRAY);
    	    glDisableClientState(GL_NORMAL_ARRAY);
    	    glDisableClientState(GL_COLOR_ARRAY);

    	    glPolygonMode(GL_FRONT, GL_LINE);
    	    glLineWidth(0.5);
    	    glColor4f(1.0, 0.0, 0.0, 1.0);
    	    map<string, vector<GLuint> >::const_iterator i;
    	    for (i = _triangles.begin(); i != _triangles.end(); i++) {
    		const vector<GLuint> &triangles = i->second;
    		if (!triangles.empty()) {
    		    glDrawElements(GL_TRIANGLES, triangles.size(),
    				   GL_UNSIGNED_INT, triangles.data());
    		}
    	    }
    	}
    	glPopClientAttrib();
    	glPopAttrib();
    	glEndList();
    }

    // Materials
    glCallList(_materialsDL);

    // Contours
    glCallList(_contoursDL);

    // Contour lines
    if (Bucket::contourLines && _contourLinesDL) {
	glPushAttrib(GL_DEPTH_BUFFER_BIT); {
	    glDisable(GL_DEPTH_TEST);
	    glCallList(_contourLinesDL);
	}
	glPopAttrib();
    }

    // Polygon edges
    if (Bucket::polygonEdges && _polygonEdgesDL) {
	glPushAttrib(GL_DEPTH_BUFFER_BIT); {
	    glDisable(GL_DEPTH_TEST);
	    glCallList(_polygonEdgesDL);
	}
	glPopAttrib();
    }
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
// in new vertices, normals, colours, elevations, and elevation
// indices being added to _vertices, _normals, _colours, _elevations,
// and _elevationIndices, and triangles being added to _contours.
// Also, we add contour line segments to _contourLines.
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
	_contourLines.push_back(i0i1.first);
	_contourLines.push_back(i0i2.first);

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
	_contourLines.push_back(i1);
	_contourLines.push_back(i0i2.first);

	vs.push_back(i0i2.first++);
	i0i2.second--;
	_addElevationSlice(vs, e++, cw);
    }

    // Deal with the top half of the triangle (above i1).
    for (; i1i2.second > 0; i1i2.second--, i0i2.second--) {
	_contourLines.push_back(i1i2.first);
	_contourLines.push_back(i0i2.first);

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
    int_list& indices = _contours[e];
    if (cw) {
	indices.push_back(i1);
	indices.push_back(i0);
    } else {
	indices.push_back(i0);
	indices.push_back(i1);
    }
    indices.push_back(i2);
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
	    _contourLines.push_back(i0);
	    _contourLines.push_back(i1);
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
	sgVec4 colour;
	Bucket::palette->smoothColour(contour.elevation, colour);
	_colours.push_back(colour[0]);
	_colours.push_back(colour[1]);
	_colours.push_back(colour[2]);
	_colours.push_back(colour[3]);

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
	_vertices.push_back(v[0]);
	_vertices.push_back(v[1]);
	_vertices.push_back(v[2]);

	// Interpolate the normal.
	sgVec3 n;
	float *n0 = &(_normals[i0 * 3]), *n1 = &(_normals[i1 * 3]);
	sgSubVec3(n, n1, n0);
	sgScaleVec3(n, scaling);
	sgAddVec3(n, n0);
	_normals.push_back(n[0]);
	_normals.push_back(n[1]);
	_normals.push_back(n[2]);

	result.second++;
    }

    _edgeMap[make_pair(i0, i1)] = result;

    return result;
}
