/*-------------------------------------------------------------------------
  MapBrowser.hxx
  Implementation of a map display for maps generated with
  MAP - FlightGear mapping utility

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

#include "LoadPng.hxx"
#include "OutputGL.hxx"
#include "Overlays.hxx"
#include "FlightTrack.hxx"
#include <GL/gl.h>
#include <math.h>
#include <list>
#include <hash_map>

#ifndef M_PI
#define M_PI 3.1415926535
#endif

class MapBrowser {
public:
  static const int CACHE_LIMIT = 2;

  MapBrowser( GLfloat x1, GLfloat y1, GLfloat size, int features, 
	      char *fg_root = NULL, bool texturedFonts = true );
  ~MapBrowser();

  void setLocation( float lat, float lon );
  void setScale( float scale );
  void setSize( GLfloat size );
  void setMapPath( char *path );
  void setFGRoot( char *fg_root );
  void setFeatures( int features );
  void setTextured( bool texture = true );
  void setFlightTrack( FlightTrack *track );

  inline float getLat()   { return clat;  }
  inline float getLon()   { return clon;  }
  inline float getScale() { return scle; }
  inline float getSize()  { return view_size; }
  inline int getFeatures() { return features; }
  inline bool getTextured() { return textured; }
  inline FlightTrack* getFlightTrack() { return track; }

  void loadDb();
  void draw();

protected:
  void update();

  inline float rad( float x ) { return x * M_PI / 180.0f; }
  inline float deg( float x ) { return x / M_PI * 180.0f; }

  inline void scale( float x, float y, GLfloat *cx, GLfloat *cy ) {
    *cx = (view_size / 2 + x * zoom);
    *cy = (view_size / 2 + y * zoom);
  }

  // viewport
  GLfloat view_left, view_top, view_size;

  // geographic location
  float clat, clon, scle, zoom;

  struct Coord {
    int lat, lon;
  };

  struct MapTile {
    Coord   c;
    GLuint  texture_handle;
    GLfloat x, y;
    GLubyte *texbuf;
  };

  struct TileEq {
    bool operator()(const Coord &t1, const Coord &t2) const {
      return t1.lat == t2.lat && t1.lon == t2.lon;
    }
  };

  struct TileHash {
    size_t operator()(const Coord &t) const {
      return (t.lat + t.lon*31);
    }
  };

  typedef hash_map<Coord, MapTile*, TileHash, TileEq> TileTable;

  list<MapTile*> tiles;
  TileTable tiletable;

  char mpath[512];
  int  pathl, features;
  bool textured, texturedFonts;
  OutputGL *output;
  Overlays *overlay;
  FlightTrack *track;
};

