/*-------------------------------------------------------------------------
  Geographics.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2012 Brian Schack

  Some useful routines for scribbling on the earth.

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

#ifndef _GEOGRAPHICS_H
#define _GEOGRAPHICS_H

// EYE - why include the layout manager?  Geographics should be more
// basic than that.  Perhaps geodDrawText should be moved elsewhere.
#include "LayoutManager.hxx"

// Draws the given laid-out text flat on the earth's surface at the
// given lat, lon and heading (all in degrees).  A heading of 0.0
// means that the text is drawn with 'up' corresponding to true north.
// It uses the current OpenGL colour.
//
// "Fiddling" refers to changes to text orientation.  When NO_FIDDLING
// is on, no adjustment is made - the text will be oriented as given
// by hdg.
//
// If FIDDLE_TEXT is on (the default), the text bounding box gets
// rotated by hdg, but we flip the text if it is upside-down.
//
// FIDDLE_ALL is like FIDDLE_TEXT, except that when the text is
// flipped, the bounding box offset is also flipped.  What this means
// is that, if the box is above the point (let's assume north is up)
// when the text is upside-right, the box will be moved to still be
// above the point when the text is flipped.  This is useful if, for
// example, you want labels on a line to always be above the line.
//
// The first version is a bit faster, because it can use the
// precomputed coordinates.  The second and third versions must derive
// the missing values.
enum GeodTextFiddling {NO_FIDDLING, FIDDLE_TEXT, FIDDLE_ALL};
void geodDrawText(LayoutManager& lm, 
		  const sgdVec3 cart, double lat, double lon, double hdg = 0.0,
		  GeodTextFiddling fiddling = FIDDLE_TEXT);
void geodDrawText(LayoutManager& lm, 
		  double lat, double lon, double hdg = 0.0, 
		  GeodTextFiddling fiddling = FIDDLE_TEXT);
void geodDrawText(LayoutManager& lm, 
		  const sgdVec3 cart, double hdg = 0.0,
		  GeodTextFiddling fiddling = FIDDLE_TEXT);

// Draws a vertex at the given lat/lon, given in degrees.  Also
// specifies the normal if asked.
void geodVertex3f(double lat, double lon, bool normals = true);

// Equivalent to the glPushMatrix()/glPopMatrix() pair, except that it
// places us at the correct location and orientation.
//
// Basically it places a sheet of graph paper flat on the earth with
// its origin at the given point, oriented north, then rotates it
// clockwise by the heading.  Positive Z is away from the earth.
//
// The first version is a bit faster, because it uses pre-computed
// cartesian and geodesic locations; the second and third need to
// derive the missing information.
//
// Angles are in degrees, elevations in feet.
//
// Every geodPushMatrix() must be matched by a geodPopMatrix(), but we
// do no checking to ensure this (although you should get OpenGL
// errors, because they translate into glPushMatrix() and
// glPopMatrix() calls).
void geodPushMatrix(const sgdVec3 cart, double lat, double lon, 
		    double hdg = 0.0);
void geodPushMatrix(double lat, double lon, double hdg = 0.0, 
		    double elev = 0.0);
void geodPushMatrix(const sgdVec3 cart, double hdg = 0.0);
void geodPopMatrix();

// This class represents a great circle route between two points.  In
// addition to calculating the basics of the great circle, it will
// draw it, trying to do so in an efficient way.
class GreatCircle {
  public:
    GreatCircle(SGGeod& start, SGGeod& end);
    ~GreatCircle();

    double toAzimuth() const { return _toAz; }
    double fromAzimuth() const { return _fromAz; }
    double distance() const { return _distance; }
    const SGGeod& from() const { return _start; }
    const SGGeod& to() const { return _end; }

    // Draws the great circle (but only the bits that appear within
    // the viewing frustum after being rotated by the modelview matrix
    // m), using the current OpenGL colour and line settings.  The
    // circle is chopped up into pieces, the number of which depending
    // on the scale (metresPerPixel).
    void draw(double metresPerPixel, 
	      const sgdFrustum& frustum,
	      const sgdMat4& m);

  protected:
    // A great circle is represented as a bunch of short segments (the
    // number and size depending on the scale at which we're drawing
    // and what's visible).  This class will do the work of
    // subdividing itself into the proper-sized pieces and throwing
    // out the bits which aren't visible.
    class _Segment {
      public:
	_Segment(SGGeod& start, SGGeod& end, double toAz, double fromAz,
		 double distance);
	~_Segment();

	// Subdivides the segment into pieces no larger than
	// minimumLength and which are visible according to the
	// frustum and modelview matrix.
	void subdivide(const sgdFrustum& frustum, 
		       const sgdMat4& m,
		       double minimumLength,
		       std::vector<SGVec3<double> >& points);

      protected:
	void _prune();

	SGGeod _start, _end, _middle;
	atlasSphere _bounds;
	// These are the same as in GreatCircle.
	double _toAz, _fromAz, _distance;
	// This is the azimuth from the middle back to the start.
	double _midAz;
	// A subdivided segment consists of two segments of half our size.
	_Segment *_A, *_B;
    };

    // Start and end points
    SGGeod _start, _end;

    // _toAz: azimuth from _start to _end, in true degrees
    // _fromAz: azimuth from _end to _start, in true degrees
    // _distance: great circle distance from _start to _end, in metres
    double _toAz, _fromAz, _distance;
};

// AtlasCoord - a combination of SGGeod and SGVec3<double>
//
// I forever seem to be converting between geodetic and cartesian
// coordinates, and needing to maintain both representations for a
// given point.  This class makes it easier, by maintaining both a
// geodetic and cartesian representation of a point.  It will convert
// from one to the other, but only if required, as the conversion is
// expensive.
//
// Note that if neither the geodetic or cartesian coordinates have
// been initialized, attempts to access their values will result in a
// runtime error being thrown.  Note as well that, because we're
// maintaining both a geodetic and cartesian representation, this
// class uses twice as much space as either alone.
class AtlasCoord {
  public:
    // Default constructor - both coordinates will be marked as false
    // until a location is set().
    AtlasCoord();

    AtlasCoord(double lat, double lon, double elev = 0.0);
    AtlasCoord(SGGeod& geod);

    AtlasCoord(SGVec3<double>& cart);
    AtlasCoord(sgdVec3 cart);

    // True if either the geodetic or cartesian coordinates are valid.
    bool valid() const;
    // Marks both coordinates as invalid.
    void invalidate();

    // Accessors.  All of these may require a conversion between
    // geodetic and cartesian coordinates.  Latitude and longitude are
    // in degrees, elevation and the cartesian coordinates are in
    // metres.
    const SGGeod& geod();
    double lat();
    double lon();
    double elev();
    const SGVec3<double>& cart();
    const double *data();
    double x();
    double y();
    double z();

    // Setters.
    void set(double lat, double lon, double elev = 0.0);
    void set(const SGGeod& geod);
    void set(const SGVec3<double>& cart);
    void set(const sgdVec3 cart);

  protected:
    // Conversion methods.  These will be called whenever we need to
    // convert from one coordinate system to the other.
    void _cartToGeod();
    void _geodToCart();

    // Our data.
    SGGeod _geod;
    bool _geodValid;
    SGVec3<double> _cart;
    bool _cartValid;
};

#endif
