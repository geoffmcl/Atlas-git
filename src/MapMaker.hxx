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

#include <simgear/compiler.h>
#include <simgear/io/sg_binobj.hxx>
#include <plib/sg.h>
#include <vector>
#include <list>
#include <map>
#include STL_STRING

#include "Output.hxx"
#include "Overlays.hxx"
#include "Geodesy.hxx"

SG_USING_STD(vector);
SG_USING_STD(string);

// Utility function that I needed to put somewhere - this probably isn't the best place for it.
// Appends a path separator to a directory path if not present.
// Calling function MUST ENSURE that there is space allocated for the potential strcat.
void NormalisePath(char* dpath);

class MapMaker {
public:
  static const int DO_SHADE        =  1;
  static const int DO_AIRPORTS     =  2;
  static const int DO_NAVAIDS      =  4;
  static const int DO_VERBOSE      =  8;
  static const int DO_SMOOTH_COLOR = 16; 
  static const int DO_NAVAIDS_VOR  = 32;
  static const int DO_NAVAIDS_NDB  = 64;
  static const int DO_NAVAIDS_FIX  =128;
  static const int DO_NAVAIDS_ILS  =256;
  static const int DO_ALL_FEAT     = 1+2+4+8+16+32+64+128+256;

  // CONSTRUCTOR
  MapMaker( char *fg_root = 0, 
	    char *ap_filter = NULL, 
	    int features = DO_SHADE | DO_AIRPORTS | DO_NAVAIDS |
	                   DO_NAVAIDS_VOR | DO_NAVAIDS_NDB | DO_NAVAIDS_FIX | DO_NAVAIDS_ILS,
	    int size = 256, int scale = 100000 );
  ~MapMaker();

  void setFGRoot( char *path );
  void setPalette( char *path );
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
  inline bool getNavaidsVOR() { return features & DO_NAVAIDS_VOR; }
  inline bool getNavaidsNDB() { return features & DO_NAVAIDS_NDB; }
  inline bool getNavaidsFIX() { return features & DO_NAVAIDS_FIX; }
  inline bool getNavaidsILS() { return features & DO_NAVAIDS_ILS; }
  inline int getScale() { return scle; }
  inline int getSize() { return size; }
  inline float *getLight() { return light_vector; }
  inline int getFeatures() { return features; }

  int createMap(GfxOutput *output, float theta, float alpha, 
		string dirpath, float autoscale = 0.0f);
  int drawOverlays(GfxOutput *output, float theta, float alpha, 
		bool do_square = false, bool flipy = false );

protected:
  /* colours and materials */
  struct WordLess {
    bool operator()(const char* v1, const char* v2) const {
      return (strcmp(v1, v2) < 0);
    }
  };
  typedef std::map<const char*, int, WordLess> StrMap;

  vector<float*> palette;
  StrMap materials;
  void read_materials(char* filename = NULL);
  
  // Elevation limits for colours
  static const int MAX_ELEV_LEVELS = 27;
  int number_elev_levels;
  int elev_height[MAX_ELEV_LEVELS]; 
  int elev_colindex[MAX_ELEV_LEVELS];

  /* member variables for the run */
  char *fg_root, *arp_filter;

  int features;
  int  size, scle;
  float zoom;          // size / scale

  bool modified, palette_loaded;

  sgVec3 map_light, light_vector;
  GfxOutput *output;

  /* some info variables */
  int polys;

  inline int elev2colour( int elev ) {
    int i;
    for (i = 0; i < number_elev_levels-1 && elev >= elev_height[i]; i++);
  
    return elev_colindex[i];
  }

# define APPROX(Ca,Cb,X,D) ((Cb-Ca)/D*X+Ca)
  inline void elev2colour_smooth( int elev, float color[4] ) {
    int i,j;
    for (i = 0; i < number_elev_levels-1 && elev > elev_height[i]; i++);
    for (j = number_elev_levels-1; j > 0 && elev <= elev_height[j]; j--);
    if(i==j){
      color[0]=palette[elev_colindex[j]][0];
      color[1]=palette[elev_colindex[j]][1];
      color[2]=palette[elev_colindex[j]][2];
      color[3]=palette[elev_colindex[j]][3];
    } else {
      float diver=(float)(elev_height[i] - elev_height[j]);
      float pom=(float)(elev - elev_height[j]);
      color[0]=APPROX(palette[elev_colindex[j]][0],palette[elev_colindex[i]][0],pom,diver);
      color[1]=APPROX(palette[elev_colindex[j]][1],palette[elev_colindex[i]][1],pom,diver);
      color[2]=APPROX(palette[elev_colindex[j]][2],palette[elev_colindex[i]][2],pom,diver);
      color[3]=APPROX(palette[elev_colindex[j]][3],palette[elev_colindex[i]][3],pom,diver);    
    }
    return;
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

  void sub_trifan( const int_list &indices, vector<float*> &v, 
		   vector<float*> &n );
  void draw_trifan( const int_list &indices, 
		    vector<float*> &v, vector <float*> &n, int col );
  void draw_tri( const int_list &indices, 
		 vector<float*> &v, vector <float*> &n, int col );
  void draw_tristrip( const int_list &indices, 
		      vector<float*> &v, vector <float*> &n, int col );
		    
  int process_directory( char *path, int plen, int lat, int lon, sgVec3 xyz );
  int process_binary_file( char *tile_name, sgVec3 xyz );
  int process_ascii_file( char *tile_name, sgVec3 xyz );
  
/*****************************************************************************/

  static const float simple_normals[][3];
};

