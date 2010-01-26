/*-------------------------------------------------------------------------
  Geographics.hxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

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

#include "LayoutManager.hxx"

// Draws the given laid-out text flat on the earth's surface at the
// given lat, lon and heading (all in degrees).  A heading of 0.0
// means that the text is drawn with 'up' corresponding to true north.
//
// If 'alwaysUp' is true, the text will be flipped if upside-down (ie,
// the heading is between 90 and 270).  Uses the current OpenGL
// colour.
//
// The second version is a bit faster, because it can use the
// precomputed Cartesian coordinates.  The first version must
// calculate the coordinates itself.
void geodDrawText(LayoutManager& lm, 
		  double lat, double lon, double hdg = 0.0, 
		  bool alwaysUp = true);
void geodDrawText(LayoutManager& lm, 
		  const sgdVec3 cart, double lat, double lon, double hdg = 0.0,
		  bool alwaysUp = true);

// Draws a vertex at the given lat/lon, given in degrees.  Also
// specifies the normal if asked.
void geodVertex3f(double lat, double lon, bool normals = true);

// Equivalent to the glPushMatrix()/glPopMatrix() pair, except that it
// places us at the correct location and orientation.
//
// Basically it places a sheet of graph paper flat on the earth at the
// given point oriented north, then rotates it clockwise by the
// heading.  Positive Z is away from the earth.
//
// The first version is a bit faster, because it uses a pre-computed
// cartesian location; the second does the calculation internally,
// setting the location to the given <lat, lon, elev>.
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
void geodPopMatrix();

#endif
