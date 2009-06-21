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

using namespace std;

Subbucket::Subbucket(const SGPath &p): _path(p), _loaded(false)
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

    // Get all the points.
    const vector<SGVec3<double> >& wgs84_nodes = _chunk.get_wgs84_nodes();
    for (unsigned int i = 0; i < wgs84_nodes.size(); i++) {
	// Make the point absolute.
	SGVec3<double> node = wgs84_nodes[i] + gbs_p;
	float *nv = new sgVec3;	// EYE - change to double?
	
	// Calculate lat, lon, elevation.
	SGGeod geod = SGGeod::fromCart(node);

	// Now convert the point using the given projection.
	if (projection == Bucket::CARTESIAN) {
	    // This is a true 3D rendering.
	    sgSetVec3(nv, node[0], node[1], node[2]);
	} else if (projection == Bucket::RECTANGULAR) {
	    // This is a flat projection.  X and Y are determined by
	    // longitude and latitude, respectively.  We don't care
	    // about Z, so set it to 0.0.  The colour will be
	    // determined separately, and the shading will be
	    // determined by the vertex normals.
	    sgSetVec3(nv, geod.getLongitudeDeg(), geod.getLatitudeDeg(), 0.0);
	}
	_vertices.push_back(nv);

	// Save our elevation.  This value is used to do elevation
	// colouring.
	_elevations.push_back(geod.getElevationM());
    }

    // same as above for normals
    const vector<SGVec3<float> >& m_norms = _chunk.get_normals();
    for (unsigned int i = 0; i < m_norms.size(); i++) {
	const SGVec3<float>& normal = m_norms[i];
	// Make a new normal
	float *nn = new sgVec3;
    
	sgSetVec3(nn, normal[0], normal[1], normal[2]);
	_normals.push_back(nn);
    }

//     if (wgs84_nodes.size() != m_norms.size()) {
// 	printf("loadChunk: %d vertices, %d normals\n", 
// 	       wgs84_nodes.size(), m_norms.size());
//     }

    // Find the highest point in the chunk and set _maxElevation.
    // EYE - magic number!
    _maxElevation = -1e6;
    for (unsigned int i = 0; i < _elevations.size(); i++) {
	if (_elevations[i] > _maxElevation) {
	    _maxElevation = _elevations[i];
	}
    }
    _maxElevation *= SG_METER_TO_FEET;

    return true;
}

void Subbucket::unload()
{
    for (unsigned int i = 0; i < _vertices.size(); i++) {
	delete []_vertices[i];
    }
    _vertices.clear();

    for (unsigned int i = 0; i < _normals.size(); i++) {
	delete []_normals[i];
    }
    _normals.clear();

    _elevations.clear();

    _loaded = false;
}

//////////////////////////////////////////////////////////////////////
// Local variables used for gathering statistics
//////////////////////////////////////////////////////////////////////
static int triangles, quads;
static int triCount, fanCount, stripCount;
static int cTriCount, cFanCount, cStripCount;
static int pureTriCount, pureFanCount, pureStripCount;
static int triFanTris, pureTriFanTris;
static int triFanPureLengthCount, triFanPureLengths;

static Palette *_palette;
static bool _discreteContours;

void Subbucket::draw(Palette *palette, bool discreteContours)
{
    // EYE - this is ugly.  We should somehow make palette a part of this.
    _palette = palette;
    _discreteContours = discreteContours;

    const float *material;

    // EYE - we assume that glColorMaterial() has been called.

    const group_list& tris = _chunk.get_tris_v();
    const string_list& tri_mats = _chunk.get_tri_materials();
    const group_list& tris_normals = _chunk.get_tris_n();
    const group_list& fans = _chunk.get_fans_v();
    const string_list& fan_mats = _chunk.get_fan_materials();
    const group_list& fans_normals = _chunk.get_fans_n();
    const group_list& strips = _chunk.get_strips_v();
    const string_list& strip_mats = _chunk.get_strip_materials();
    const group_list& strips_normals = _chunk.get_strips_n();
  
    triCount += tris.size();
    fanCount += fans.size();
    stripCount += strips.size();

    // EYE - do points too?
    // Triangles
    for (unsigned int i = 0; i < tris.size(); i++) {
	material = _palette->colour(tri_mats[i].c_str());

	if (tris_normals[i].size() > 0) {
	    assert(tris[i].size() == tris_normals[i].size());
	    _drawTris(tris[i], tris_normals[i], material);
	} else {
	    _drawTris(tris[i], tris[i], material);
	}
    }

    // Triangle fans
    for (unsigned int i = 0; i < fans.size(); i++) {
	material = _palette->colour(fan_mats[i].c_str());

	if (fans_normals[i].size() > 0) {
	    assert(fans[i].size() == fans_normals[i].size());
	    _drawTrifan(fans[i], fans_normals[i], material);
	} else {
	    _drawTrifan(fans[i], fans[i], material);
	}
    }
	
    // Triangle strips
    for (unsigned int i = 0; i < strips.size(); i++) {
	material = _palette->colour(strip_mats[i].c_str());

	if (strips_normals[i].size() > 0) {
	    assert(strips[i].size() == strips_normals[i].size());
	    _drawTristrip(strips[i], strips_normals[i], material);
	} else {
	    _drawTristrip(strips[i], strips[i], material);
	}
    }
}

//////////////////////////////////////////////////////////////////////
// Helper routines
//////////////////////////////////////////////////////////////////////
static void swap(int &a, int &b) 
{
    int tmp = a;
    a = b;
    b = tmp;
}

static void swap(unsigned int &a, unsigned int &b) 
{
    unsigned int tmp = a;
    a = b;
    b = tmp;
}

static void setColor(const float *rgba)
{
    glColor4fv(rgba);
}

#ifdef ROTATE_NORMALS
// "Fixes" normals when in a flat projection.  A normal perpendicular
// to the earth's surface at a point is transformed into <0, 0, 1>.
//
// to = fixed normal
// from = original normal
// by = vertex (which must be <lon, lat, ignored>)
static void rotate(sgVec3 to, const sgVec3 from, const sgVec3 by)
{
//     static int count = 0;
    float lon = by[0];
    float lat = by[1];

    sgMat4 z, x, mat;
    sgVec3 axis;

    // We want to unrotate by longitude (around the z axis).
    sgSetVec3(axis, 0.0, 0.0, 1.0);
    sgMakeRotMat4(z, -(90.0 + lon), axis);

    // And unrotate by latitude (around the x axis).
    sgSetVec3(axis, 1.0, 0.0, 0.0);
    sgMakeRotMat4(x, lat - 90.0, axis);

    // Create final matrix, and rotate normal.
    sgMultMat4(mat, z, x);
    sgXformVec3(to, from, mat);

//     count++;
//     if (count < 10) {
// 	printf("<%f, %f, %f> => <%f, %f, %f>\n",
// 	       from[0], from[1], from[2], to[0], to[1], to[2]);
//     }
}
#endif // ROTATE_NORMALS

static void drawTriangle(const sgVec3 *p, const sgVec3 *normals)
{
    triangles++;
    glBegin(GL_TRIANGLES);
#ifndef ROTATE_NORMALS
    glNormal3fv(normals[0]); glVertex3fv(p[0]);
    glNormal3fv(normals[1]); glVertex3fv(p[1]);
    glNormal3fv(normals[2]); glVertex3fv(p[2]);
#else
    sgVec3 newNormal;
    for (int i = 0; i < 3; i++) {
	rotate(newNormal, normals[i], p[i]);
	glNormal3fv(newNormal); glVertex3fv(p[i]);
    }
#endif // ROTATE_NORMALS
    glEnd();
}

static void drawTriangle(const sgVec3 *p, const sgVec3 *normals, 
			 const sgVec4 *color) 
{
    triangles++;
    glBegin(GL_TRIANGLES);
#ifndef ROTATE_NORMALS
    glColor4fv(color[0]); glNormal3fv(normals[0]); glVertex3fv(p[0]);
    glColor4fv(color[1]); glNormal3fv(normals[1]); glVertex3fv(p[1]);
    glColor4fv(color[2]); glNormal3fv(normals[2]); glVertex3fv(p[2]);
#else
    sgVec3 newNormal;
    for (int i = 0; i < 3; i++) {
	rotate(newNormal, normals[i], p[i]);
	glColor4fv(color[i]); glNormal3fv(newNormal); glVertex3fv(p[i]);
    }
#endif // ROTATE_NORMALS
    glEnd();
}

static void drawTriangle(const float* p0, const float* p1, const float* p2, 
			 const float* n0, const float* n1, const float* n2)
{
    triangles++;
    glBegin(GL_TRIANGLES);
#ifndef ROTATE_NORMALS
    glNormal3fv(n0); glVertex3fv(p0);
    glNormal3fv(n1); glVertex3fv(p1);
    glNormal3fv(n2); glVertex3fv(p2);
#else
    sgVec3 newNormal;
    rotate(newNormal, n0, p0);
    glNormal3fv(newNormal); glVertex3fv(p0);
    rotate(newNormal, n1, p1);
    glNormal3fv(newNormal); glVertex3fv(p1);
    rotate(newNormal, n2, p2);
    glNormal3fv(newNormal); glVertex3fv(p2);
#endif // ROTATE_NORMALS
    glEnd();
}

static void drawTriangle(const float* p0, const float* p1, const float* p2, 
			 const float* n0, const float* n1, const float* n2,
			 const float* c0, const float* c1, const float* c2) 
{
    triangles++;
    glBegin(GL_TRIANGLES);
#ifndef ROTATE_NORMALS
    glColor4fv(c0); glNormal3fv(n0); glVertex3fv(p0);
    glColor4fv(c1); glNormal3fv(n1); glVertex3fv(p1);
    glColor4fv(c2); glNormal3fv(n2); glVertex3fv(p2);
#else
    sgVec3 newNormal;
    rotate(newNormal, n0, p0);
    glColor4fv(c0); glNormal3fv(newNormal); glVertex3fv(p0);
    rotate(newNormal, n1, p1);
    glColor4fv(c1); glNormal3fv(newNormal); glVertex3fv(p1);
    rotate(newNormal, n2, p2);
    glColor4fv(c2); glNormal3fv(newNormal); glVertex3fv(p2);
#endif // ROTATE_NORMALS
    glEnd();
}

static void drawTriangleFan(const int_list &vertexIndices, 
			    const int_list &normalIndices,
			    vector<float *> &vertices, 
			    vector<float *> &normals)
{
    cFanCount++;
    glBegin(GL_TRIANGLE_FAN); {
	for (unsigned int i = 0; i < vertexIndices.size(); i++) {
#ifndef ROTATE_NORMALS
	    // EYE - note that we only need one set of indices!  Why?
	    // This only seems to be true for fans, but this should be
	    // checked further.
	    assert(normalIndices[i] == vertexIndices[i]);
	    glNormal3fv(normals[normalIndices[i]]);
	    glVertex3fv(vertices[vertexIndices[i]]);
#else
	    sgVec3 newNormal;
	    rotate(newNormal, normals[normalIndices[i]], 
		   vertices[vertexIndices[i]]);
	    glNormal3fv(newNormal);
	    glVertex3fv(vertices[vertexIndices[i]]);
#endif // ROTATE_NORMALS
	}
    }
    glEnd();
}

static void drawQuad(const sgVec3 *p, const sgVec3 *normals)
{
    quads++;
    glBegin(GL_QUADS);
#ifndef ROTATE_NORMALS
    glNormal3fv(normals[0]); glVertex3fv(p[0]);
    glNormal3fv(normals[1]); glVertex3fv(p[1]);
    glNormal3fv(normals[2]); glVertex3fv(p[2]);
    glNormal3fv(normals[3]); glVertex3fv(p[3]);
#else
    sgVec3 newNormal;
    for (int i = 0; i < 4; i++) {
	rotate(newNormal, normals[i], p[i]);
	glNormal3fv(newNormal); glVertex3fv(p[i]);
    }
#endif // ROTATE_NORMALS
    glEnd();
}

static void drawQuad(const sgVec3 *p, const sgVec3 *normals, const sgVec4 *color)
{
    quads++;
    glBegin(GL_QUADS);
#ifndef ROTATE_NORMALS
    glColor4fv(color[0]); glNormal3fv(normals[0]); glVertex3fv(p[0]);
    glColor4fv(color[1]); glNormal3fv(normals[1]); glVertex3fv(p[1]);
    glColor4fv(color[2]); glNormal3fv(normals[2]); glVertex3fv(p[2]);
    glColor4fv(color[3]); glNormal3fv(normals[3]); glVertex3fv(p[3]);
#else
    sgVec3 newNormal;
    for (int i = 0; i < 4; i++) {
	rotate(newNormal, normals[i], p[i]);
	glColor4fv(color[i]); glNormal3fv(newNormal); glVertex3fv(p[i]);
    }
#endif // ROTATE_NORMALS
    glEnd();
}

//////////////////////////////////////////////////////////////////////
// Triangle routines
//////////////////////////////////////////////////////////////////////
void Subbucket::_drawTristrip(const int_list &vertex_indices, 
			      const int_list &normal_indices,
			      const float *col) 
{
    unsigned int i;
    int vert0, vert1, vert2;
    int norm0, norm1, norm2;

    vert0 = vertex_indices[0];
    norm0 = normal_indices[0];
    vert1 = vertex_indices[1];
    norm1 = normal_indices[1];
    for (i = 2; i < vertex_indices.size(); i++) {
	vert2 = vertex_indices[i];
	norm2 = normal_indices[i];

	_drawTri(vert0, vert1, vert2, norm0, norm1, norm2, col);

	vert1 = vert0;
	norm1 = norm0;
	vert0 = vert2;
	norm0 = norm2;
    }
    return;

//     // Not cutting the fan/strip into triangles saves a lot of
//     // drawing.  For example, with w002n51, we have these numbers:
//     //
//     // tris: 190, fans: 17588, strips: 85
//     //
//     // If we subdivide everything we have this many calls to
//     // drawTriangle and drawQuad:
//     // 
//     // triangles: 57843, quads: 8134

//     // If we don't subdivide fans:
//     // 
//     // triangles: 4812, quads: 229
//     //
//     // And if we further don't subdivide strips:
//     //
//     // triangles: 3234, quads: 0
//     //
//     // And, of course, if we do the same for tris:
//     //
//     // triangles: 0, quads: 0

//     // If we look at how many structures are "pure" (don't cross a
//     // contour), and are flat coloured (ie, not an elevation), we get:
//     //
//     // tris: 190 (0 flat coloured, 189 pure, 1 impure)
//     // strips: 85 (0 flat coloured, 72 pure, 13 impure)
//     // fans: 17588 (8841 flat coloured, 6168 pure, 2579 impure)

//     // If we only pass on "impure" structures, we get:
//     //
//     // Pass on fans:
//     //
//     // triangles: 12312, quads: 7905
//     //
//     // Pass on fans and tris:
//     //
//     // triangles: 12348, quads: 7905
//     //
//     // Pass on fans, tris, and strips:
//     //
//     // triangles: 12610, quads: 8134

//     // The 2579 impure fans consist of 12262 individual triangles,
//     // 5054 of which are themselves pure.  The average "run length"
//     // (number of pure triangles in the fan before an impure triangle)
//     // is 2.26.
//     //
//     // Interestingly, these 7208 (12262 - 5054) impure triangles, when
//     // subdivided, create 12312 triangles and 7905 quads.

//     // If we try to draw the pure runs in the impure fans, we get:
//     //
//     // triangles: 7556, quads: 6397,
//     //
//     // nearly cutting the number of triangles in half (although at the
//     // expense of drawing some rather tiny fans).  The performance
//     // gain is difficult to judge, so not huge.

//     // Set colour to the colour of the first vertex.
//     if (col == NULL) {
// 	setColor(_palette->contour(_elevations[vertex_indices[0]]).colour);
// 	// Check to see if they actually all are at the same
// 	// elevation.
// 	unsigned int index = 
// 	    _palette->contourIndex(_elevations[vertex_indices[0]]);
// 	unsigned int i;
// 	for (i = 1; i < vertex_indices.size(); i++) {
// 	    if (_palette->contourIndex(_elevations[vertex_indices[i]]) != 
// 		index) {
// 		break;
// 	    }
// 	}
// 	if (i == vertex_indices.size()) {
// 	    pureStripCount++;
// 	} else {
// 	    unsigned int i;
// 	    int vert0, vert1, vert2;
// 	    int norm0, norm1, norm2;

// 	    vert0 = vertex_indices[0];
// 	    norm0 = normal_indices[0];
// 	    vert1 = vertex_indices[1];
// 	    norm1 = normal_indices[1];
// 	    for (i = 2; i < vertex_indices.size(); i++) {
// 		vert2 = vertex_indices[i];
// 		norm2 = normal_indices[i];

// 		_drawTri(vert0, vert1, vert2, norm0, norm1, norm2, col);

// 		vert1 = vert0;
// 		norm1 = norm0;
// 		vert0 = vert2;
// 		norm0 = norm2;
// 	    }
// 	    return;
// 	}
//     } else {
// 	cStripCount++;
// 	setColor(col);
//     }

//     glBegin(GL_TRIANGLE_STRIP);
//     for (unsigned int i = 0; i < vertex_indices.size(); i++) {
// 	glNormal3fv(_normals[normal_indices[i]]);
// 	glVertex3fv(_vertices[vertex_indices[i]]);
//     }
//     glEnd();
}

void Subbucket::_drawTrifan(const int_list &vertex_indices, 
			    const int_list &normal_indices, 
			    const float *col)
{
    if (col != NULL) {
	// If this is a material-coloured object, just draw it.

	// EYE - we should really impose an ordering on materials.
	// For example, cities should be drawn *under* roads,
	// railways, rivers, etc.  I should check to see if sets of
	// objects cover each other (as opposed to being inlaid into a
	// single grid).  I really should add:
	// (a) point, line, flat-coloured, and smooth-coloured rendering
	// (b) toggle "layers" (materials)
	setColor(col);
	drawTriangleFan(vertex_indices, normal_indices, _vertices, _normals);
    } else {
	// Elevation coloured.
	unsigned int i;
	int cvert, vert1, vert2;
	int cnorm, norm1, norm2;

	cvert = vertex_indices[0];
	cnorm = normal_indices[0];
	vert1 = vertex_indices[1];
	norm1 = normal_indices[1];
	for (i = 2; i < vertex_indices.size(); i++) {
	    vert2 = vertex_indices[i];
	    norm2 = normal_indices[i];

	    _drawTri(cvert, vert1, vert2, cnorm, norm1, norm2, col);

	    vert1 = vert2;
	    norm1 = norm2;
	}
    }

//     // This is an elevation object.  If all the vertices in the object
//     // are in the same elevation band, then we don't have to worry
//     // about splitting it.
//     int index = elev2colour(e[vertex_indices[0]]);
//     int i;

//     for (i = 1; i < vertex_indices.size(); i++) {
// 	if (elev2colour(e[vertex_indices[i]]) != index) {
// 	    break;
// 	}
//     }

//     if (i == vertex_indices.size()) {
// 	// Everything is at the same elevation.
// 	setColor(palette[index]);
// 	// EYE - need to do something else if smoothing is on.
// 	drawTriangleFan(vertex_indices, normal_indices, v, n);

// 	return;
//     }

//     // EYE - and if smoothing is on?
//     // Breaks a contour.  Subdivide it into triangles.
//     int length;
//     int cvert, vert1, vert2;
//     int cnorm, norm1, norm2;

//     length = 0;

//     cvert = vertex_indices[0];
//     cnorm = normal_indices[0];
//     vert1 = vertex_indices[1];
//     norm1 = normal_indices[1];
//     for (i = 2; i < vertex_indices.size(); i++) {
// 	vert2 = vertex_indices[i];
// 	norm2 = normal_indices[i];

// 	triFanTris++;
// 	if ((elev2colour(e[vert1]) == index) && 
// 	    (elev2colour(e[vert2]) == index)) {
// 	    pureTriFanTris++;
// 	    length++;
// 	} else {
// 	    // Did we get a run?
// 	    if (length > 0) {
// 		// Draw section of fan.  

// 		// EYE - the setColor() command must come
// 		// outside of the glBegin()!.  Why?  A: I
// 		// don't know, but changing setColor() fixed
// 		// things.
// 		glBegin(GL_TRIANGLE_FAN);
// 		setColor(palette[index]);
// 		glNormal3fv(n[cnorm]);
// 		glVertex3fv(v[cvert]);

// 		for (int j = i - length - 1; j < i; j++) {
// 		    glNormal3fv(n[normal_indices[j]]);
// 		    glVertex3fv(v[vertex_indices[j]]);
// 		}
// 		glEnd();

// 		triFanPureLengthCount++;
// 		triFanPureLengths += length;
// 		length = 0;
// 	    }
// 	    // And draw the triangle that broke the run.
// 	    draw_a_tri(cvert, vert1, vert2, cnorm, norm1, norm2, 
// 		       v, n, e, col);
// 	}
// 	// 		draw_a_tri(cvert, vert1, vert2, cnorm, norm1, norm2, 
// 	// 			   v, n, e, col);

// 	vert1 = vert2;
// 	norm1 = norm2;
//     }
//     // If we had a run at the end, draw that too.
//     if (length > 0) {
// 	// Draw section of fan.

// 	glBegin(GL_TRIANGLE_FAN);
// 	setColor(palette[index]);
// 	glNormal3fv(n[cnorm]);
// 	glVertex3fv(v[cvert]);

// 	for (int j = i - length - 1; j < i; j++) {
// 	    glNormal3fv(n[normal_indices[j]]);
// 	    glVertex3fv(v[vertex_indices[j]]);
// 	}
// 	glEnd();

// 	triFanPureLengthCount++;
// 	triFanPureLengths += length;
// 	length = 0;
//     }

//     return;
}

void Subbucket::_drawTris(const int_list &vertex_indices, 
			  const int_list &normal_indices, 
			  const float *col) 
{
    unsigned int i;
    int vert0, vert1, vert2;
    int norm0, norm1, norm2;

    // EYE - can we assume indices.size() is divisible by 3?
    assert((vertex_indices.size() % 3) == 0);
    for (i = 0; i < vertex_indices.size(); i += 3) {
	vert0 = vertex_indices[i];
	norm0 = normal_indices[i];
	vert1 = vertex_indices[i + 1];
	norm1 = normal_indices[i + 1];
	vert2 = vertex_indices[i + 2];
	norm2 = normal_indices[i + 2];

	_drawTri(vert0, vert1, vert2, norm0, norm1, norm2, col);
    }
    return;

//     if (col == NULL) {
// 	setColor(_palette->contour(_elevations[vertex_indices[0]]).colour);
// 	// Check to see if they actually all are at the same
// 	// elevation.
// 	unsigned int index = 
// 	    _palette->contourIndex(_elevations[vertex_indices[0]]);
// 	unsigned int i;
// 	for (i = 1; i < vertex_indices.size(); i++) {
// 	    if (_palette->contourIndex(_elevations[vertex_indices[i]]) 
// 		!= index) {
// 		break;
// 	    }
// 	}
// 	if (i == vertex_indices.size()) {
// 	    pureTriCount++;
// 	} else {
// 	    // EYE - and if smoothing is on?
// 	    // Breaks a contour.  Subdivide it into triangles.
// 	    unsigned int i;
// 	    int vert0, vert1, vert2;
// 	    int norm0, norm1, norm2;

// 	    // EYE - can we assume indices.size() is divisible by 3?
// 	    assert((vertex_indices.size() % 3) == 0);
// 	    for (i = 0; i < vertex_indices.size(); i += 3) {
// 		vert0 = vertex_indices[i];
// 		norm0 = normal_indices[i];
// 		vert1 = vertex_indices[i + 1];
// 		norm1 = normal_indices[i + 1];
// 		vert2 = vertex_indices[i + 2];
// 		norm2 = normal_indices[i + 2];

// 		_drawTri(vert0, vert1, vert2, norm0, norm1, norm2, col);
// 	    }
// 	    return;
// 	}
//     } else {
// 	cTriCount++;
// 	setColor(col);
//     }

//     glBegin(GL_TRIANGLES);
//     for (unsigned int i = 0; i < vertex_indices.size(); i++) {
// 	glNormal3fv(_normals[normal_indices[i]]);
// 	glVertex3fv(_vertices[vertex_indices[i]]);
//     }
//     glEnd();
}

// Draws a single triangle defined by the given vertices and normals
// (which are indexes into the _vertices and _normals vectors).  If
// the triangle defines an "elevation triangle" (a triangle that
// should be coloured according to its elevation), then it's passed on
// to _drawElevationTri.
void Subbucket::_drawTri(int vert0, int vert1, int vert2,
			 int norm0, int norm1, int norm2,
			 const float *col)
{
    // Elevation triangles get special treatment.
    if (col == NULL) {
	_drawElevationTri(vert0, vert1, vert2, norm0, norm1, norm2);
	return;
    }

    // Non-elevation triangles are coloured according to col.
    setColor(col);
    drawTriangle(_vertices[vert0], _vertices[vert1], _vertices[vert2], 
		 _normals[norm0], _normals[norm1], _normals[norm2]);
}

// A helper function for _drawElevationTri.  Draws a figure (which may
// be a triangle, quadrilateral, or pentagon).  The number of vertices
// and normals is given by 'vertices'.  The 'ts' array gives the
// vertices, 'nrms' the normals, and 'ps' the scaled map coordinates.
//
// We can assume that the vertices are given from top to bottom, and
// that they move around the perimeter of the figure in the correct
// order.  As well, we only need to guarantee that the last two
// points, which form the bottom of the figure, remain unchanged - the
// calling function doesn't care about the "upper" points.  (This is a
// *very* specialized routine).
static void __drawElevationSlice(int vertices, bool discrete, int k,
				 sgVec3 *nrms, sgVec3 *ps, float *es)
{
    sgVec4 color[5];

    if (!discrete) {
	for (int i = 0; i < vertices; i++) {
	    _palette->smoothColour((int)es[i], color[i]);
	}
    }

//     // EYE - draw it as a polygon, ignore smoothing.  This is simpler,
//     // but doesn't seem to improve performance much.
//     glBegin(GL_POLYGON);
//     setColor(palette[elev_colindex[k]]);
//     for (int i = 0; i < vertices; i++) {
// 	glNormal3fv(nrms[i]); glVertex3fv(ps[i]);
//     }
//     glEnd();
//     return;

    // Draw the figure.
    if (vertices == 3) {
	// Triangle
	if (discrete) {
	    setColor(_palette->contourAtIndex(k).colour);
	    drawTriangle(ps, nrms);
	} else {
	    drawTriangle(ps, nrms, color);
	}
    } else if (vertices == 4) {
	// Quadrilateral.
	if (discrete) {
	    setColor(_palette->contourAtIndex(k).colour);
	    drawQuad(ps, nrms);
	} else {
	    drawQuad(ps, nrms, color);
	}
    } else {
	// Pentagon.  Draw it as a quadrilateral and a triangle.  The
	// quadralateral consists of the first 4 points, and the
	// triangle consists of the remaining two *and* the first one.
	// To draw the triangle, then, we copy the last two points
	// over the second and third points (this is okay, because
	// they won't be used again), and draw a triangle consisting
	// of the first 3 points.
	if (discrete) {
	    setColor(_palette->contourAtIndex(k).colour);
	    drawQuad(ps, nrms);
	} else {
	    drawQuad(ps, nrms, color);
	}

	sgCopyVec3(nrms[1], nrms[3]);
	sgCopyVec3(ps[1], ps[3]);

	sgCopyVec3(nrms[2], nrms[4]);
	sgCopyVec3(ps[2], ps[4]);

	if (discrete) {
	    drawTriangle(ps, nrms);
	} else {
	    sgCopyVec4(color[1], color[3]);
	    sgCopyVec4(color[2], color[4]);
	    drawTriangle(ps, nrms, color);
	}
    }
}

// A helper function for _drawElevationTri.  Given an upper and a
// lower vertex defining an edge, and an elevation at which to slice
// the edge, creates a new point at that slice.  The new point is
// added to the nrms and ps arrays at the index 'dest'.  We also put
// the elevation into es.
//
// EYE - Why pass 'dest' - I should really just pass the vertex,
// normal, and point to be set.
static void __createSubPoint(float *topVert, float *bottomVert,
			     float *topNorm, float *bottomNorm,
			     float topE, float bottomE,
			     int dest, double elevation,
			     sgVec3 *nrms, sgVec3 *ps, float *es)
{
    sgVec3 newPoint, newNorm;
    double scaling = 
	(elevation - bottomE) / (topE - bottomE);
    // EYE - make this more general
    const double epsilon = 1e-5;
    assert((scaling <= 1.0 + epsilon) && (-epsilon <= scaling));

    sgSubVec3(newPoint, topVert, bottomVert);
    sgScaleVec3(newPoint, scaling);
    sgAddVec3(newPoint, bottomVert);
    sgSubVec3(newNorm, topNorm, bottomNorm);
    sgScaleVec3(newNorm, scaling);
    sgAddVec3(newNorm, bottomNorm);

    sgCopyVec3(nrms[dest], newNorm);
    sgCopyVec3(ps[dest], newPoint);
    es[dest] = elevation;
}

// Draws the given triangle, coloured according to its elevation.  If
// the triangle spans more than one elevation level, and so needs more
// than one colour, it is sliced and diced, so that each part occupies
// only one level.  The triangle slices are temporary - no changes are
// made to the _vertices and _normals vectors.
void Subbucket::_drawElevationTri(int vert0, int vert1, int vert2,
				  int norm0, int norm1, int norm2)
{
    unsigned int index[3];
    index[0] = _palette->contourIndex(_elevations[vert0]);
    index[1] = _palette->contourIndex(_elevations[vert1]);
    index[2] = _palette->contourIndex(_elevations[vert2]);

    // Triangle lies within one elevation level.  Draw it in one
    // colour.
    if ((index[0] == index[1]) && (index[1] == index[2])) {
	if (!_discreteContours) {
	    sgVec4 color[3];
	    _palette->smoothColour(_elevations[vert0], color[0]);
	    _palette->smoothColour(_elevations[vert1], color[1]);
	    _palette->smoothColour(_elevations[vert2], color[2]);

	    drawTriangle(_vertices[vert0], _vertices[vert1], _vertices[vert2], 
			 _normals[norm0], _normals[norm1], _normals[norm2],
			 color[0], color[1], color[2]);
	} else {
	    setColor(_palette->contour(_elevations[vert0]).colour);
	    drawTriangle(_vertices[vert0], _vertices[vert1], _vertices[vert2], 
			 _normals[norm0], _normals[norm1], _normals[norm2]);
	}

	return;
    }

    // Triangle spans more than one level.  Drats.  Do a quick sort on
    // the vertices, so that vert0 points to the top vertex, vert1,
    // the middle, and vert2 the bottom.
    if (index[0] < index[1]) {
	swap(vert0, vert1);
	swap(norm0, norm1);
	swap(index[0], index[1]);
    }
    if (index[0] < index[2]) {
	swap(vert0, vert2);
	swap(norm0, norm2);
	swap(index[0], index[2]);
    }
    if (index[1] < index[2]) {
	swap(vert1, vert2);
	swap(norm1, norm2);
	swap(index[1], index[2]);
    }

    assert(index[0] >= index[1]);
    assert(index[1] >= index[2]);

    // Now begin slicing the lines leading away from vert0 to vert1
    // and vert2.  Slicing a triangle creates new triangles, new
    // quadrilaterals, and even new pentagons.  Because the triangle
    // lies in a plane (by definition), we are assured that the new
    // figures are also planar.
    //
    // After each bit is sliced off the top, it is drawn and then
    // discarded.  The process is then repeated on the remaining
    // figure, until there's nothing left.
    //
    // This can be illustrated with the power of ASCII graphics.  If
    // we have to make two cuts of the triangle ABC, we'll create 4
    // new points, D, E, F, and G.  This creates 3 figures: ADE,
    // EDBFG, and GFC.
    //
    //        A	       A	  	 
    //       /|	      /|	  	 
    //      / |	     / |	  	 
    //  -->/  |	    D--E     D--E      D--E
    //    /   |	   /   |    /   |     /   |
    //   B    |	  B    |   B    |    B    |
    //    \   |	   \   |    \   |     \   |
    //     \  |	    \  |     \  |      \  |
    //   -->\ |	  -->\ |   -->\ |       F-G       F-G
    //       \|	      \|       \|        \|        \|
    //        C	       C        C         C         C

    unsigned int k, vertices;
    sgVec3 nrms[5];
    sgVec3 ps[5];
    float es[5];

    // Slicing creates new vertices and normals, so we need to keep
    // track of actual points, not just their indices as before.  The
    // array 'nrms' keeps the norms of the current figure, and 'ps'
    // the scaled points.  At most we can generate a pentagon, so each
    // array has 5 points.  The current number of points is given by
    // 'vertices'.
    //
    // In the example above, the arrays will contain data for ADE,
    // then DEBFG, and finally GFC.  Note that points are given in a
    // counter-clockwise direction (as illustrated here).  This means
    // that that when a bottom line (eg, DE in ADE) becomes a top line
    // (ED in EDBFG), we reverse its order.
    sgCopyVec3(nrms[0], _normals[norm0]);
    sgCopyVec3(ps[0], _vertices[vert0]);
    es[0] = _elevations[vert0];
    vertices = 1;

    for (k = index[0]; k > index[1]; k--) {
	// Make a cut and draw the resulting figure.
	double elevation = _palette->contourAtIndex(k).elevation;

	// Cut along the short line (vert0 to vert1), and put the
	// resulting normal, point, and elevation into nrms, ps, and
	// es.
	__createSubPoint(_vertices[vert0], _vertices[vert1], 
			 _normals[norm0], _normals[norm1], 
			 _elevations[vert0], _elevations[vert1],
			 vertices++, elevation, nrms, ps, es);
	// Ditto for the long line (vert0 to vert2).
	__createSubPoint(_vertices[vert0], _vertices[vert2], 
			 _normals[norm0], _normals[norm2], 
			 _elevations[vert0], _elevations[vert2],
			 vertices++, elevation, nrms, ps, es);

	// Now draw the resulting figure.
	__drawElevationSlice(vertices, _discreteContours, k, nrms, ps, es);

	// We're ready to move down and make the next slice.  The two
	// points we just created will now be the top of the next
	// slice.  We need to reverse the order of the points.
	sgCopyVec3(nrms[0], nrms[vertices - 1]);
	sgCopyVec3(ps[0], ps[vertices - 1]);
	es[0] = es[vertices - 1];

	sgCopyVec3(nrms[1], nrms[vertices - 2]);
	sgCopyVec3(ps[1], ps[vertices - 2]);
	es[1] = es[vertices - 2];

	vertices = 2;
    }

    // Add the middle vertex.
    sgCopyVec3(nrms[vertices], _normals[norm1]);
    sgCopyVec3(ps[vertices], _vertices[vert1]);
    es[vertices] = _elevations[vert1];
    vertices++;
    assert(vertices <= 5);

    for (; k > index[2]; k--) {
	// Make a cut and draw the resulting figure.
	double elevation = _palette->contourAtIndex(k).elevation;

	// Get the point along the short line.
	__createSubPoint(_vertices[vert1], _vertices[vert2], 
			 _normals[norm1], _normals[norm2], 
			 _elevations[vert1], _elevations[vert2],
			 vertices++, elevation, nrms, ps, es);
	// Get the point along the long line.
	__createSubPoint(_vertices[vert0], _vertices[vert2], 
			 _normals[norm0], _normals[norm2], 
			 _elevations[vert0], _elevations[vert2],
			 vertices++, elevation, nrms, ps, es);

	__drawElevationSlice(vertices, _discreteContours, k, nrms, ps, es);

	// The bottom will be the next top.
	sgCopyVec3(nrms[0], nrms[vertices - 1]);
	sgCopyVec3(ps[0], ps[vertices - 1]);
	es[0] = es[vertices - 1];

	sgCopyVec3(nrms[1], nrms[vertices - 2]);
	sgCopyVec3(ps[1], ps[vertices - 2]);
	es[1] = es[vertices - 2];
	
	vertices = 2;
    }

    // Add the final vertex and draw the last figure.
    sgCopyVec3(nrms[vertices], _normals[norm2]);
    sgCopyVec3(ps[vertices], _vertices[vert2]);
    es[vertices] = _elevations[vert2];
    vertices++;
    assert(vertices <= 5);

    __drawElevationSlice(vertices, _discreteContours, k, nrms, ps, es);
}
