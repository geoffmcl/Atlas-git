/*-------------------------------------------------------------------------
  Geodesy.hxx

  Written by Per Liedman, started February 2000.
  Based on a perl-script written by Alexei Novikov (anovikov@heron.itep.ru)

  Copyright (C) 2000 Per Liedman, liedman@home.se

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
---------------------------------------------------------------------------*/

#include <math.h>

/* equatorial and polar earth radius */
const float rec  = 6378137;          // earth radius, equator (?)
const float rpol = 6356752.314;      // earth radius, polar   (?)

/************************************************************************
  some trigonometric helper functions 
  (translated more or less directly from Alexei Novikovs perl original)
*************************************************************************/

//Returns Earth radius at a given latitude (Ellipsoide equation with two equal axis)
inline float earth_radius_lat( float lat ) {
  return 1.0f / sqrt( cos(lat)/rec  * cos(lat)/rec +
		      sin(lat)/rpol * sin(lat)/rpol );
}

inline void ab_xy( sgVec3 xyz, sgVec3 ref, sgVec3 dst) {
  float xy = sqrt( xyz[0]*xyz[0]   + xyz[1]*xyz[1] );
  float r  = sqrt( xy*xy           + xyz[2]*xyz[2] );
  float s_lat = xyz[2] / r;
  float c_lat = xy / r;

  dst[2] = 1.0f / sqrt( c_lat/rec  * c_lat/rec +
			s_lat/rpol * s_lat/rpol );

  float xyr = sqrt( ref[0]*ref[0] + ref[1]*ref[1] );
  float rr  = sqrt( xyr*xyr + ref[2]*ref[2] );
  dst[0]    = dst[2] / (r * xyr) * (xyz[1]*ref[0] - ref[1]*xyz[0]);
  dst[1]    = dst[2] / (r * rr) * (xyr * xyz[2] - xy * ref[2]);
}

inline void geod_geoc( float ang, float *Sin, float *Cos ) {
  const float rec4  = rec*rec*rec*rec;
  const float rpol4 = rpol*rpol*rpol*rpol;

  float s = sin(ang);
  float d = sqrt( rec4 + s*s*(rpol4 - rec4) );
  *Sin = rpol*rpol * s / d;
  *Cos = rec*rec * cos(ang) / d;
}

inline void xyz_lat( float lat, float lon, 
		     sgVec3 xyz, float *r ) {
  float s, c;

  *r = earth_radius_lat( lat );
  geod_geoc( lat, &s, &c );
  xyz[0] = *r * c * cos( lon );
  xyz[1] = (*r * c * sin( lon ));
  xyz[2] = *r * s;
}

inline void lat_ab( float a, float b, float lat_r, float lon_r,
		    float *lat, float *lon ) {
  float s, c;

  float r = earth_radius_lat( lat_r );
  *lat = lat_r + b / r;
  geod_geoc( lat_r, &s, &c );
  *lon = lon_r + a/(c*r);
}

inline float scale( float x, float size, float zoom ) {
  return (size/2 + x * zoom);
}

inline char ew(int lon) { return (lon < 0) ? 'w' : 'e'; }
inline char ns(int lat) { return (lat < 0) ? 's' : 'n'; }
