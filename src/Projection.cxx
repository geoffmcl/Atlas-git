/*-------------------------------------------------------------------------
  Projection.cxx
  Implementation of a class for support of multiple projection systems

  Written by Per Liedman, started February 2000.
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
#include <vector>
#include <list>

#include <math.h>

#include "Projection.hxx"
#include "Geodesy.hxx"

static const char *names[]={"Sanson-Flamsteed",
                            "Equidistant Cylindrical (Equatorial)"};

Projection::Projection() {
   setSystem(SANSON_FLAMSTEED);
}

Projection::~Projection() {
}

void Projection::setSystem(int id) {
   sysid=id;
   
   switch (sysid) {
    case CYL_EQUIDISTANT_EQ:
      systemclass=CYLINDRICAL;
      break;
    default:
      systemclass=PSEUDOCYLINDRICAL;
      break;

   }
}

const char *Projection::getSystemName(int id) {
   return names[id];
}

void Projection::ab_lat( float lat, float lon, float lat_r, float lon_r, sgVec3 dst ) {
   switch (sysid) {
    case SANSON_FLAMSTEED:
        dst[2] = earth_radius_lat( lat_r );
        dst[0] = dst[2] * cos(lat)*(lon-lon_r);
        dst[1] = dst[2] * (lat-lat_r);
        break;
    case CYL_EQUIDISTANT_EQ:
        dst[2] = rec;
        dst[0] = dst[2] * (lon-lon_r);
        dst[1] = dst[2] * (lat-lat_r);
        break;
   }
}

// Pick nice angles close to xaim and yaim. Prefer a pair with similar
// relationships to their respective aims, to make the grid fairly square.
void Projection::nice_angle_pair( float xaim, float yaim, float *pxnice, float *pynice ) {
  // If each spacing is an integer multiple of the previous, new lines
  // appear but none disappear as you zoom in.
  const float nice_angles[] = { 180, 90, 45, 15, 5, 1, .5, .25, 5.0/60, 1.0/60 };

  int ix, iy;

  for (ix = 1; ix < sizeof(nice_angles)/sizeof(nice_angles[0]) - 1; ix++) {
    if (nice_angles[ix] < xaim) break;
  }

  for (iy = 1; iy < sizeof(nice_angles)/sizeof(nice_angles[0]) - 1; iy++) {
    if (nice_angles[iy] < yaim) break;
  }

  // ix and iy each index the second of two possible angles.

  float xbest, ybest;
  float prev_lerr = 999;
  for (int x = ix-1; x <= ix; x++) {
    for (int y = iy-1; y <= iy; y++) {
      float xlerr = log(nice_angles[x]) - log(xaim);
      float ylerr = log(nice_angles[y]) - log(yaim);
      float lerr = max(fabs(xlerr - ylerr), max(fabs(xlerr), fabs(ylerr)));
      if (lerr < prev_lerr) {
        prev_lerr = lerr;
        xbest = nice_angles[x];
        ybest = nice_angles[y];
      }
    }
  }
  *pxnice = xbest;
  *pynice = ybest;
}
