/*-------------------------------------------------------------------------
  Subbucket.cxx

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

#include <cassert>

#include "Subbucket.hxx"
#include "Globals.hxx"

using namespace std;

Subbucket::Subbucket(const SGPath &p): 
    _path(p), _loaded(false), _palettized(false),
    _materialsDL(0), _contoursDL(0), _contourLinesDL(0), _size(0)
{
}

Subbucket::~Subbucket()
{
    unload();
}

// Documentation on the BTG file format can be found at:
//
// http://wiki.flightgear.org/index.php/BTG_File_Format
//
bool Subbucket::load(Bucket::Projection projection)
{
    // Clear out whatever was there first.
    unload();

    if (!_chunk.read_bin(_path.c_str())) {
	// EYE - throw an error?
	return false;
    }

    // A chunk contains a bunch of points in 3D cartesian space, where
    // the origin is at the centre of the earth, the X axis goes
    // through 0 degrees latitude, 0 degrees longitude (near Africa),
    // the Y axis goes through 0 degrees latitude, 90 degrees west
    // latitude (in the Indian Ocean), and the Z axis goes through the
    // north pole.  Units are metres.  See:
    //
    // http://www.flightgear.org/Docs/Scenery/CoordinateSystem/CoordinateSystem.html
    //
    // for more.

    // Each chunk has a reference point, given by get_gbs_center().
    // All points within the chunk are relative to the reference
    // point.  Therefore, to place points in absolute 3D space, we
    // need to add the reference point to all points.
    const SGVec3<double>& gbs_p = _chunk.get_gbs_center2();

    // Get all the points, and use them to set our maximum elevation
    // figure.
    _maxElevation = -numeric_limits<double>::max();
    const vector<SGVec3<double> >& wgs84_nodes = _chunk.get_wgs84_nodes();
    for (unsigned int i = 0; i < wgs84_nodes.size(); i++) {
	// Make the point absolute.  Note that the documentation for
	// BTG files says that vertices are stored as floats (and
	// sg_binobj.cxx shows that that is true) - strange that it
	// would serve them up as doubles.
	SGVec3<double> node = wgs84_nodes[i] + gbs_p;
	
	// Calculate lat, lon, elevation.
	SGGeod geod = SGGeod::fromCart(node);

	// Now convert the point using the given projection.
	if (projection == Bucket::CARTESIAN) {
	    // This is a true 3D rendering.
	    _vertices.push_back(node[0]);
	    _vertices.push_back(node[1]);
	    _vertices.push_back(node[2]);
	} else if (projection == Bucket::RECTANGULAR) {
	    // This is a flat projection.  X and Y are determined by
	    // longitude and latitude, respectively.  We don't care
	    // about Z, so set it to 0.0.  The colour will be
	    // determined separately, and the shading will be
	    // determined by the vertex normals.
	    _vertices.push_back(geod.getLongitudeDeg());
	    _vertices.push_back(geod.getLatitudeDeg());
	    _vertices.push_back(0.0);
	}

	// Save our elevation.  This value is used to do elevation
	// colouring, as well as calculate our maximum elevation
	// figure.
	double e = geod.getElevationM();
	_elevations.push_back(e);
	if (e > _maxElevation) {
	    _maxElevation = e;
	}
    }
    _maxElevation *= SG_METER_TO_FEET;

    // Same as above for normals.
    const vector<SGVec3<float> >& m_norms = _chunk.get_normals();
    for (unsigned int i = 0; i < m_norms.size(); i++) {
	const SGVec3<float>& normal = m_norms[i];
	_normals.push_back(normal[0]);
	_normals.push_back(normal[1]);
	_normals.push_back(normal[2]);
    }
    
    assert(_vertices.size() == _elevations.size() * 3);
    // EYE - what about points?  Colours, materials, textures?

    // Some sanity checking.  There seem to be two types of BTG files:
    // terrain and airport.  For reasons I don't understand, terrain
    // files never have triangles and strips, but only fans, whereas
    // airports are the opposite.  Moreover, airport files always have
    // different numbers of actual vertices and normals, and, more
    // strangely, they only ever use the first normal.
    if (wgs84_nodes.size() != m_norms.size()) {
	// Airport file - triangles and triangle strips.
	_airport = true;

	assert(_chunk.get_tris_v().size() > 0);
	assert(_chunk.get_tris_v().size() ==_chunk.get_tris_n().size());
	for (size_t i = 0; i < _chunk.get_tris_v().size(); i++) {
	    // There is a separate specification of normals...
	    assert(_chunk.get_tris_n().at(i).size() > 0);
	    // ... with one normal per vertex.
	    assert(_chunk.get_tris_v().at(i).size() == 
		   _chunk.get_tris_n().at(i).size());
	    for (size_t j = 0; j < _chunk.get_tris_n().at(i).size(); j++) {
		// However, it's always the first normal.
		assert(_chunk.get_tris_n().at(i).at(j) == 0);
	    }
	}

	// Make sure the vertex indices are valid.
	for (size_t i = 0; i < _chunk.get_tris_v().size(); i++) {
	    const int_list& vs = _chunk.get_tris_v()[i];
	    for (size_t j = 0; j < vs.size(); j++) {
		assert(vs[j] < _elevations.size());
	    }
	}

	// Ditto for triangle strips.
	assert(_chunk.get_strips_v().size() > 0);
	assert(_chunk.get_strips_v().size() ==_chunk.get_strips_n().size());
	for (size_t i = 0; i < _chunk.get_strips_n().size(); i++) {
	    assert(_chunk.get_strips_n().at(i).size() > 0);
	    assert(_chunk.get_strips_v().at(i).size() ==
		   _chunk.get_strips_n().at(i).size());
	    for (size_t j = 0; j < _chunk.get_strips_n().at(i).size(); j++) {
		assert(_chunk.get_strips_n().at(i).at(j) == 0);
	    }
	}

	// Make sure the vertex indices are valid.
	for (size_t i = 0; i < _chunk.get_strips_v().size(); i++) {
	    const int_list& vs = _chunk.get_strips_v()[i];
	    for (size_t j = 0; j < vs.size(); j++) {
		assert(vs[j] < _elevations.size());
	    }
	}

	// But no triangle fans.
	assert(_chunk.get_fans_v().size() == 0);
	assert(_chunk.get_fans_n().size() == 0);
    } else {
	// Terrain file - no triangles or triangle strips.
	_airport = false;

	assert(_chunk.get_tris_v().size() == 0);
	assert(_chunk.get_tris_n().size() == 0);
	assert(_chunk.get_strips_v().size() == 0);
	assert(_chunk.get_strips_n().size() == 0);

	// But triangle fans.
	assert(_chunk.get_fans_v().size() > 0);
	assert(_chunk.get_fans_n().size() > 0);
	for (size_t i = 0; i < _chunk.get_fans_n().size(); i++) {
	    // There is no separate set of normal indices; the nth
	    // vertex uses the nth normal.
	    assert(_chunk.get_fans_n().at(i).size() == 0);
	}

	// Make sure the vertex indices are valid.
	for (size_t i = 0; i < _chunk.get_fans_v().size(); i++) {
	    const int_list& vs = _chunk.get_fans_v()[i];
	    for (size_t j = 0; j < vs.size(); j++) {
		assert(vs[j] < _elevations.size());
	    }
	}
    }

    // Estimate the size of the subbucket.  We just count the vertices
    // and normals.  There are probably other bits we should count,
    // but I think it's close enough.
    _size = _vertices.size() * sizeof(sgVec3);
    _size += _normals.size() * sizeof(sgVec3);
    
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

    _loaded = false;
}

// Called to notify us that the palette in Bucket::palette has
// changed.  We need to delete all of our display lists and flag that
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
    _vertices.resize(_chunk.get_wgs84_nodes().size() * 3);
    _normals.resize(_chunk.get_normals().size() * 3);

    // Elevation values remain valid as well (but elevation indices
    // don't - they depend on the palette).
    _elevations.resize(_chunk.get_wgs84_nodes().size());

    // Recalculate elevation indices and colours.
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
    _materialsN.clear();
    _contours.clear();
    _contours.resize(Bucket::palette->size());
    _contourLines.clear();

    // First, triangles.
    const group_list& tris = _chunk.get_tris_v();
    const string_list& tri_mats = _chunk.get_tri_materials();
    for (size_t i = 0; i < tris.size(); i++) {
	const string& material = tri_mats[i];
	if (Bucket::palette->colour(material.c_str()) != NULL) {
	    // Transfer the triangles into our vector.
	    vector<GLuint>& triangles = _materials[material];
	    for (size_t j = 0; j < tris[i].size(); j++) {
		triangles.push_back(tris[i][j]);
	    }
	} else {
	    // Contours.
	    _chopTriangles(tris[i]);
	}
    }

    // Second, triangle strips.
    const group_list& strips = _chunk.get_strips_v();
    const string_list& strip_mats = _chunk.get_strip_materials();
    for (size_t i = 0; i < strips.size(); i++) {
	const string& material = strip_mats[i];
	if (Bucket::palette->colour(material.c_str()) != NULL) {
	    // Make raw triangles out of the strips.
	    vector<GLuint>& triangles = _materials[material];
	    for (size_t j = 0; j < strips[i].size() - 2; j++) {
		if (j % 2 == 0) {
		    triangles.push_back(strips[i][j]);
		    triangles.push_back(strips[i][j + 1]);
		    triangles.push_back(strips[i][j + 2]);
		} else {
		    triangles.push_back(strips[i][j + 1]);
		    triangles.push_back(strips[i][j]);
		    triangles.push_back(strips[i][j + 2]);
		}
	    }
	} else {
	    // Contours.
	    _chopTriangleStrip(strips[i]);
	}
    }

    // Finally, triangle fans.
    const group_list& fans = _chunk.get_fans_v();
    const string_list& fan_mats = _chunk.get_fan_materials();
    for (size_t i = 0; i < fans.size(); i++) {
	const string& material = fan_mats[i];
	if (Bucket::palette->colour(material.c_str()) != NULL) {
	    // Make raw triangles out of the fans.  Triangle fans have
	    // per-vertex normals, so add them to _materialsN, not
	    // _materials.
	    vector<GLuint>& triangles = _materialsN[material];
	    for (size_t j = 1; j < fans[i].size() - 1; j++) {
		    triangles.push_back(fans[i][0]);
		    triangles.push_back(fans[i][j]);
		    triangles.push_back(fans[i][j + 1]);
	    }
	} else {
	    // Contours.
	    _chopTriangleFan(fans[i]);
	}
    }

    int ts = 0;
    for (size_t i = 0; i < _contours.size(); i++) {
    	ts += _contours[i].size();
    }

    _edgeMap.clear();

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
    // EYE - do this inside the display list?
    glVertexPointer(3, GL_FLOAT, 0, &_vertices[0]);
    glNormalPointer(GL_FLOAT, 0, &_normals[0]);
    glColorPointer(4, GL_FLOAT, 0, &_colours[0]);

    // ---------- Materials ----------
    if (_materialsDL == 0) {
	_materialsDL = glGenLists(1);
	assert(_materialsDL != 0);

	glNewList(_materialsDL, GL_COMPILE);
	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT); {
	    glEnableClientState(GL_VERTEX_ARRAY);
	    glDisableClientState(GL_COLOR_ARRAY);

	    // Draw the "material" objects (ie, objects coloured based on
	    // their material, not elevation).  Note that we assume that
	    // glColorMaterial() has been called.  First draw the ones
	    // that don't use the normals array.
	    glDisableClientState(GL_NORMAL_ARRAY);
	    glNormal3f(_normals[0], _normals[1], _normals[2]);
	    map<string, vector<GLuint> >::iterator m;
	    for (m = _materials.begin(); m != _materials.end(); m++) {
		const string& material = m->first;
		vector<GLuint>& triangles = m->second;

		glColor4fv(Bucket::palette->colour(material.c_str()));
		glDrawElements(GL_TRIANGLES, triangles.size(),
			       GL_UNSIGNED_INT, &(triangles[0]));
	    }

	    // Now draw ones that use the normals array.
	    glEnableClientState(GL_NORMAL_ARRAY);
	    for (m = _materialsN.begin(); m != _materialsN.end(); m++) {
		const string& material = m->first;
		vector<GLuint>& triangles = m->second;

		glColor4fv(Bucket::palette->colour(material.c_str()));
		glDrawElements(GL_TRIANGLES, triangles.size(),
			       GL_UNSIGNED_INT, &(triangles[0]));
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

	    // Draw "contours" objects (ie, objects coloured based on
	    // their elevation).
	    if (_airport) {
		glDisableClientState(GL_NORMAL_ARRAY);
		glNormal3f(_normals[0], _normals[1], _normals[2]);
	    } else {
		glEnableClientState(GL_NORMAL_ARRAY);
	    }
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
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 
			       &(indices[0]));
	    }
	}
	glPopClientAttrib();
	glEndList();
    }

    // ---------- Contour lines ----------
    if (_contourLinesDL == 0) {
	_contourLinesDL = glGenLists(1);
	assert(_contourLinesDL != 0);

	glNewList(_contourLinesDL, GL_COMPILE);
	glPushAttrib(GL_LINE_BIT);
	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT); {
	    glEnableClientState(GL_VERTEX_ARRAY);
	    glDisableClientState(GL_NORMAL_ARRAY);
	    glDisableClientState(GL_COLOR_ARRAY);

	    glLineWidth(0.5);
	    glColor4f(0.0, 0.0, 0.0, 1.0);
	    glDrawElements(GL_LINES, _contourLines.size(), GL_UNSIGNED_INT,
	    		   &(_contourLines[0]));
	}
	glPopAttrib();
	glPopClientAttrib();
	glEndList();
    }

    // EYE - Do inside display list generation?
    glPushAttrib(GL_POLYGON_BIT); {
	if (Bucket::contourLines) {
	    // To successfully draw the contour lines on top of the
	    // polygons, we need to push the polygons back a bit.
	    glEnable(GL_POLYGON_OFFSET_FILL);
	    glPolygonOffset(1.0, 1.0);
	}

	// Materials
	glCallList(_materialsDL);

	// Contours
	glCallList(_contoursDL);

	// Contour lines
	if (Bucket::contourLines) {
	    glDisable(GL_POLYGON_OFFSET_FILL);
	    glCallList(_contourLinesDL);
	}
    }
    glPopAttrib();
}

void Subbucket::_chopTriangles(const int_list& triangles)
{
    for (size_t i = 0; i < triangles.size(); i += 3) {
	_chopTriangle(triangles[i], triangles[i + 1], triangles[i + 2]);
    }
}

void Subbucket::_chopTriangleStrip(const int_list& strip)
{
    for (size_t i = 0; i < strip.size() - 2; i++) {
	if (i % 2 == 0) {
	    _chopTriangle(strip[i], strip[i + 1], strip[i + 2]);
	} else {
	    _chopTriangle(strip[i + 1], strip[i], strip[i + 2]);
	}
    }
}

void Subbucket::_chopTriangleFan(const int_list& fan)
{
    for (size_t i = 1; i < fan.size() - 1; i++) {
	_chopTriangle(fan[0], fan[i], fan[i + 1]);
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
    // contour cuts through one of the vertices i1 or i2.

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

    // Now chop the edges.  We find all the points along the edge
    // through which a contour passes and create vertices (and normals
    // and colours) for them.  The vertices are created sequentially,
    // starting at the index returned by _chopEdge.  Therefore, the
    // first contour above i0 in _chopEdge(i0, i1) is at index i0i1,
    // the second at i0i1 + 1, ...
    int i0i1 = _chopEdge(i0, i1),
	i1i2 = _chopEdge(i1, i2),
	i0i2 = _chopEdge(i0, i2);

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
    // This is true if i1 is on a contour.
    bool contourVertex = 
	(_elevations[i1] == Bucket::palette->contourAtIndex(e1).elevation);
    // Deal with the bottom half of the triangle (below i1).
    for (; e < e1; e++) {
	// If the next contour slice passes through i1, use that
	// instead of getting a point along i0 - i1.
	if (contourVertex && (e + 1 == e1)) {
	    vs.push_back(i1);
	} else {
	    vs.push_back(i0i1 + e - e0);
	}
	vs.push_back(i0i2 + e - e0);
	_addElevationSlice(vs, e, cw);
    }
    // If i1 is not a contour index, then the above loop stopped with
    // the last slice below i1.  Add i1 and continue on.  This will
    // give us our EDBFG object (or perhaps ABC, or maybe EDBCE,
    // depending on which contours cut through the triangle).
    if (!contourVertex) {
	vs.push_back(i1);
    }

    // This is true if i2 is on a contour.
    contourVertex = (_elevations[i2] == 
		     Bucket::palette->contourAtIndex(e2).elevation);
    // Deal with the top half of the triangle (above i1).
    for (; e < e2; e++) {
	// Create the next contour slice, but not if it passes through
	// i2.
	if ((e + 1 < e2) || !contourVertex) {
	    vs.push_back(i1i2 + e - e1);
	    vs.push_back(i0i2 + e - e0);
	    _addElevationSlice(vs, e, cw);
	}
    }
    vs.push_back(i2);
    if (contourVertex) {
	e--;
    }
    // Draw the last slice, but make sure the top is not treated as a
    // contour.
    _addElevationSlice(vs, e, cw, true);
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

// Given a single slice through a triangle, as given in vs, creates
// one, two, or three triangles to represent it.  If cw is true, we
// reverse the order of the first two vertices to restore a
// counterclockwise winding.  If top is true, that means that the last
// point represents the apex of the triangle, and so we don't need to
// add a contour line segment to _contourLines.
void Subbucket::_addElevationSlice(deque<int>& vs, int e, bool cw, bool top)
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

    // The remaining two points will be part of a contour line, unless
    // we're drawing the top bit of the triangle.
    if (!top) {
	_contourLines.push_back(vs[0]);
	_contourLines.push_back(vs[1]);
    }
}

// Creates new vertices for each point between i0 and i1 that lies on
// a contour.  We store the index of the first vertex created in
// _edgeMap[(i0, i1)] (-1 if there are no vertices).  Returns the
// index.  Can be safely called multiple times.
//
// Note that this is where we spend most of our time (about 50%), so
// it's imperative that it be as efficient as possible.
int Subbucket::_chopEdge(int i0, int i1)
{
    // For convenience, and to make sure we name edges consistently,
    // we force i0 to be lower (topographically) than i1.  If they're
    // at the same elevation, we sort by index.
    if (_elevations[i0] > _elevations[i1]) {
    	swap(i0, i1);
    } else if ((_elevations[i0] == _elevations[i1]) && (i0 > i1)) {
	swap(i0, i1);
    }

    if (i0 != i1) {
    	// assert(_edgeMap.find(make_pair(i1, i0)) == _edgeMap.end());
    } else {
	// Believe it or not, BTG files sometimes have "edges" where
	// the two endpoints are the same.
	// fprintf(stderr, "%s: triangle has two identical vertices!\n",
	// 	_path.file().c_str());
    }

    // Now we're ready.  First see if the edge has been processed
    // already.  The following code is a bit of a trick, relying on
    // two things: (1) New vertices we create will never have an index
    // of 0, and (2) If the given edge doesn't exist, _edgeMap[]
    // returns 0 (ie, a newly created int is given a default value of
    // 0).
    int& firstVertex = _edgeMap[make_pair(i0, i1)];
    if (firstVertex != 0) {
	return firstVertex;
    }

    int e0 = _elevationIndices[i0], e1 = _elevationIndices[i1];
    float elev0 = _elevations[i0], elev1 = _elevations[i1];
    firstVertex = -1;

    // If i0 and i1 are at the same level, then we can't add any points.
    if (elev0 == elev1) {
	// Check if it forms part of a contour line.  If so, record
	// the contour.
    	if (Bucket::palette->contourAtIndex(e0).elevation == elev0) {
	    _contourLines.push_back(i0);
	    _contourLines.push_back(i1);
	}

	return firstVertex;
    }

    firstVertex = _vertices.size() / 3;
    if (elev1 != Bucket::palette->contourAtIndex(e1).elevation) {
	// If the upper point is not on a contour (ie, it's above it),
	// then we need to create a vertex for that contour too.
	e1++;
    }
    int i = firstVertex;
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

	if (!_airport) {
	    // Interpolate the normal.
	    sgVec3 n;
	    float *n0 = &(_normals[i0 * 3]), *n1 = &(_normals[i1 * 3]);
	    sgSubVec3(n, n1, n0);
	    sgScaleVec3(n, scaling);
	    sgAddVec3(n, n0);
	    _normals.push_back(n[0]);
	    _normals.push_back(n[1]);
	    _normals.push_back(n[2]);
	}
    }

    return firstVertex;
}
