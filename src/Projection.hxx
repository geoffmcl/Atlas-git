/*-------------------------------------------------------------------------
  Projection.hxx
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
#ifndef __PROJECTION_HXX
#define __PROJECTION_HXX

#include <plib/sg.h>

class Projection {
   
 public:
   //Projection systems
#define MAX_NUM_PROJECTIONS 3
   static const int SANSON_FLAMSTEED=0;
   static const int CYL_EQUIDISTANT_EQ=1;
   static const int CYL_EQUIDISTANT_LOCAL=2;
   static const int NUM_PROJECTIONS=MAX_NUM_PROJECTIONS;
   
   Projection();
   ~Projection();
   void setSystem(int id);
   const char *getSystemName(int id);
   void ab_lat( float lat, float lon, float lat_r, float lon_r, sgVec3 dst );
   void nice_angle_pair( float xaim, float yaim, float *pxnice, float *pynice );
 
 private:
   //System classes
   static const int PSEUDOCYLINDRICAL = 0;
   static const int CYLINDRICAL = 1;
   static const int AZIMUTHAL = 2;
   static const int CONIC = 3;
   
   int sysid;
   
   //Projection classes
   int systemclass;
 
};
#endif /*__PROJECTION_HXX*/
