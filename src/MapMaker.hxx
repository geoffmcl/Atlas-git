/*-------------------------------------------------------------------------
  MapMaker.hxx

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
---------------------------------------------------------------------------
  CHANGES
  2000-02-26      Created
  2000-02-06      MapMaker now draws in an OpenGL context
---------------------------------------------------------------------------*/

#include <plib/sg.h>
#include <vector>
#include <list>
#include "Output.hxx"
#include "Overlays.hxx"
#include "Geodesy.hxx"

class MapMaker {
public:
  static const int DO_SHADE      = 1;
  static const int DO_AIRPORTS   = 2;
  static const int DO_NAVAIDS    = 4;
  static const int DO_VERBOSE    = 8;
  static const int DO_ALL_FEAT   = 1+2+4+8;

  // CONSTRUCTOR
  MapMaker( char *fg_root = "/usr/local/lib/FlightGear", 
	    char *ap_filter = NULL, 
	    int features = DO_SHADE | DO_AIRPORTS | DO_NAVAIDS,
	    int size = 512, int scale = 100000 );

  inline void setFGRoot( char *path ) { fg_root = path; }
  inline void setAPFilter( char *filter ) { arp_filter = filter; }
  inline void setFeatures( int features) { this->features = features; }
  inline void setScale( int scale = 100000 ) { scle = scale; }
  inline void setSize( int size = 512 ) { this->size = size; }
  inline void setLight( sgVec3 light ) { 
    sgCopyVec3( map_light, light ); 
    sgNormaliseVec3( map_light );
  }
  inline void setLight( float x, float y, float z ) {
    sgSetVec3( map_light, x, y, z );
    sgNormaliseVec3( map_light );
  }

  inline char *getFGRoot() { return fg_root; }
  inline char *getAPFilter() { return arp_filter; }
  inline bool getShade() { return features & DO_SHADE; }
  inline bool getAirports() { return features & DO_AIRPORTS; }
  inline bool getVerbose() { return features & DO_VERBOSE; }
  inline bool getNavaids() { return features & DO_NAVAIDS; }
  inline int getScale() { return scle; }
  inline int getSize() { return size; }
  inline float *getLight() { return light_vector; }
  inline int getFeatures() { return features; }

  int createMap(GfxOutput *output, float theta, float alpha, 
		bool do_square = false);
  int drawOverlays(GfxOutput *output, float theta, float alpha, 
		bool do_square = false, bool flipy = false );

protected:
  /* colours and materials */
  static const char *materials[16];
  static const float rgb[17][4];
  
  // some constants for colours
  static const int NUM_COLOURS = 17;
  static const int ARP_LABEL = 14;
  static const int LEVEL_LINES = 10;
  
  // Maps materials to a special colour, -1 means shaded, height dependant
  static const int colours[16];

  // Elevation limits for colours
  static const int ELEV_LEVELS = 8;
  static const int elev_height[ELEV_LEVELS]; 

  /* member variables for the run */
  char *fg_root, *arp_filter;

  int features;
  int  size, scle;
  float zoom;          // size / scale

  bool modified;

  sgVec3 map_light, light_vector;
  GfxOutput *output;

  /* some info variables */
  int polys, verts, normals;

  inline int elev2colour( int elev ) {
    int i;
    for (i = 0; i < ELEV_LEVELS && elev >= elev_height[i]; i++);
  
    return 1 + i;
  }

/****************************************************************************/
  inline void level_triangle( sgVec3 u, sgVec3 v, sgVec3 un, sgVec3 vn, int k, 
			      float *x, float *y, sgVec3 n, int *h ) {
    float min = (u[2] < v[2]) ? u[2] : v[2];

    int j = elev2colour( (int)min )-1;
    *h = j + k - 1;
    float t = ((float)elev_height[*h] - u[2]) / (v[2] - u[2]);
    *x = u[0] + t * (v[0] - u[0]);
    *y = u[1] + t * (v[1] - u[1]);
    // n = un + t*(vn-un)
    sgSubVec3(n, vn, un);
    sgScaleVec3(n, t);
    sgAddVec3(n, un);
  }

  inline float shade( sgVec3 *t ) {
    sgVec3 u, v, n;
  
    sgSubVec3( u, t[0], t[1] );
    sgSubVec3( v, t[0], t[2] );

    sgVectorProductVec3( n, u, v );
    sgNormaliseVec3( n );

    return fabs(sgScalarProductVec3( n, light_vector ));
  }

  void sub_trifan( vector<int> &tri, vector<float*> &v, vector<float*> &n, 
		   int index );
  void draw_trifan( vector<int> &tri, vector<float*> &v, vector<float*> &n,
		    int index );

  int process_directory( char *path, int plen, int lat, int lon, 
			 float x, float y, float z );
  int process_file( char *tile_name, float x, float y, float z );

};

