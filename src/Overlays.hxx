/*-------------------------------------------------------------------------
  Overlays.hxx

  Written by Per Liedman, started July 2000.

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

#ifndef __OVERLAYS_H__
#define __OVERLAYS_H__

#include <vector>
#include <list>
#include <stdio.h>
#include <memory.h>

#include <string>

#include <simgear/magvar/magvar.hxx>
#include <simgear/timing/sg_time.hxx>

#include "Output.hxx"
#include "FlightTrack.hxx"
#include "Projection.hxx"

class Overlays {
public:
  static const int OVERLAY_AIRPORTS    = 1 << 0;
  static const int OVERLAY_NAVAIDS     = 1 << 1;
  static const int OVERLAY_FIXES       = 1 << 2;
  static const int OVERLAY_FLIGHTTRACK = 1 << 3;
  static const int OVERLAY_GRIDLINES   = 1 << 4;
  static const int OVERLAY_NAMES       = 1 << 5;
  static const int OVERLAY_IDS         = 1 << 6;
  static const int OVERLAY_NAVAIDS_VOR  = 1 << 7;
  static const int OVERLAY_NAVAIDS_NDB  = 1 << 8;
  static const int OVERLAY_NAVAIDS_FIX  = 1 << 9;
  static const int OVERLAY_NAVAIDS_ILS  = 1 << 10;
  static const int OVERLAY_ANY_LABEL   = OVERLAY_NAMES | OVERLAY_IDS;

  //  Overlays();
  Overlays( const char *fg_root, float scale, float width );

  inline void setScale( float s ) { scale = s; }
  inline void setLocation( float lat, float lon ) {
    this->lat = lat;
    this->lon = lon;
  }
  inline void setOutput( GfxOutput *output ) {
    this->output = output;
    size = (float)output->getSize();
  }
  inline void setFGRoot( char *fg_root ) {
    this->fg_root = fg_root;
  }
  inline void setFlightTrack( FlightTrack *track ) {
    flight_track = track;
  }

  inline void setAirportColor( const float *color1, const float *color2 ) {
    memcpy( arp_color1, color1, sizeof(float)*4 );
    memcpy( arp_color2, color2, sizeof(float)*4 );
  }
  inline void setNavaidColor( const float *color ) {
    memcpy( nav_color, color, sizeof(float)*4 );
  }
  inline void setVorToColor( const float *color ) {
    memcpy( vor_to_color, color, sizeof(float)*4 );
  }
  inline void setVorFromColor( const float *color ) {
    memcpy( vor_from_color, color, sizeof(float)*4 );
  }
  inline void setIlsColor( const float *color ) {
    memcpy( ils_color, color, sizeof(float)*4 );
  }
  inline void setNdbColor( const float *color ) {
    memcpy( ndb_color, color, sizeof(float)*4 );
  }
  inline void setGridColor( const float *color ) {
    memcpy( grd_color, color, sizeof(float)*4 );
  }
  inline void setTrackColor( const float *color ) {
    memcpy( trk_color, color, sizeof(float)*4 );
  }
  inline void setAircraftColor( const float *color ) {
    memcpy( ac_color, color, sizeof(float)*4 );
  }
  inline void setAircraftMarkColor( const float *color ) {
    memcpy( ac_mark_color, color, sizeof(float)*4 );
  }

  inline float *aircraftColor() {
      return ac_color;
  }
  inline float *aircraftMarkColor() {
      return ac_mark_color;
  }

  inline void setFeatures(int features) {
    this->features = features;
  }
  
  inline void setProjection(Projection *pr) {
     projection=pr;
  }

  inline float getScale() { return scale; }
  inline FlightTrack *getFlightTrack() { return flight_track; }
  inline int getFeatures() { return features; }
  void load_airports();
  void load_old_airports();
  void load_very_old_airports();
  void load_navaids();
  void load_new_navaids();
  void load_fixes();
  void load_new_fixes();
  void drawOverlays();

  // Aiport & Navaid databases
  struct RWY {
    float lat, lon, hdg;
    int length, width;
  };

  struct ARP {
    char name[64], id[5];
    float lat, lon;
    list<RWY*> rwys;
  };

  enum NavType { NAV_VOR, NAV_DME, NAV_NDB, NAV_ILS, NAV_FIX };

  struct NAV {
    char name[64], id[6];
    NavType navtype;
    float lat, lon, freq, magvar;
    int range;
  };

  // This is used by the search code (findMatches, ...).  It is used
  // to map from a single search token to a NAV or APR structure.
  // These TOKEN structures are stored, sorted, in the tokens vector.
  // When a search is performed, the matching records are placed in
  // the matches vector.
  enum LocationType { NONE, AIRPORT, NAVAID };
  struct TOKEN {
    string token;
    LocationType t;
    void *locAddr;
  };

  // Find an airport by name
  ARP* findAirport( const char* name );
  // Find an airport by ICAO code
  ARP* findAirportByCode( const char* id );
  NAV* findNav    ( const char* name );
  NAV* findNav    ( float lat, float lon, float freq );

  bool recordMatches(int i, vector<string> completeSearchTokens, 
		     string partialSearchToken);
  bool findMatches(char *str, int maxMatches);
  TOKEN getToken(int i);
  int noOfMatches();

protected:

  static vector<ARP*> airports;
  static vector<NAV*> navaids;
  static bool airports_loaded, navaids_loaded, fixes_loaded;
  static const float dummy_normals[][3];

  void airport_labels(float theta, float alpha, 
		      float dtheta, float dalpha);
  void buildRwyCoords( sgVec2 rwyc, sgVec2 rwyl, sgVec2 rwyw, sgVec2 *points );

  void draw_navaids(float theta, float alpha, 
		    float dtheta, float dalpha);
  void draw_ndb( NAV *n, sgVec2 p );
  void draw_vor( NAV *n, sgVec2 p );
  void draw_ils( NAV *n, sgVec2 p );
  void draw_fix( NAV *n, sgVec2 p );
  void draw_flighttrack();
  void _drawAircraft(FlightData *point, float color[4]);
  void draw_gridlines( float dtheta, float dalpha, float spacing );

  int features;
  float scale, lat, lon, size;
  const char *fg_root;
  GfxOutput *output;
  FlightTrack *flight_track;
  Projection *projection;

  static const float airport_color1[4]; 
  static const float airport_color2[4];
  static const float navaid_color[4];
  static const float static_vor_to_color[4];
  static const float static_vor_from_color[4];
  static const float static_ils_color[4];
  static const float static_ndb_color[4];
  static const float grid_color[4];
  static const float track_color[4];
  static const float aircraft_color[4];
  static const float aircraft_mark_color[4];
  float arp_color1[4]; 
  float arp_color2[4];
  float nav_color[4];
  float vor_to_color[4]; 
  float vor_from_color[4];
  float ils_color[4];
  float ndb_color[4];
  float grd_color[4];
  float trk_color[4];
  float ac_color[4];
  float ac_mark_color[4];

  SGTime *time_params;
  SGMagVar *mag;

  vector<TOKEN> tokens;
  vector<TOKEN> matches;
  void tokenizeLocation(LocationType lType, void *loc, vector<TOKEN> &vec);
};

bool operator< (const Overlays::TOKEN& left, const Overlays::TOKEN& right);

#endif        // __OVERLAYS_H__
