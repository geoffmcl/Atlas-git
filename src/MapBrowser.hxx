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
#include "Projection.hxx"
#include <GL/gl.h>
#include <math.h>
#include <list>
#include <map>

class MapBrowser {
public:
  static const int CACHE_LIMIT = 2;
  
  MapBrowser( GLfloat x1, GLfloat y1, GLfloat size, int features, 
              char *fg_root, bool texturedFonts = true );
  ~MapBrowser();

  void setLocation( float lat, float lon );
  void setScale( float scale );
  void setSize( GLfloat size );
  void setMapPath( char *path );
  //void setFGRoot( char *fg_root );
  void setFeatures( int features );
  void toggleFeaturesAllNavaids();
  void setTextured( bool texture = true );
  void setFlightTrack( FlightTrack *track );
  void setProjectionByID(int id);
   
  inline float getLat()   { return clat;  }
  inline float getLon()   { return clon;  }
  inline float getScale() { return scle; }
  inline float getSize()  { return view_size; }
  inline int getFeatures() { return features; }
  inline bool getTextured() { return textured; }
  inline FlightTrack* getFlightTrack() { return track;   }
  inline Overlays*    getOverlays()    { return overlay; }
  void loadDb();
  void draw();
  inline const char *getProjectionNameByID(int id) { 
     return projection->getSystemName(id);
  }
  inline int getNumProjections() {
     return projection->NUM_PROJECTIONS;
  }
  void changeResolution(char *);
protected:
  void update();

  inline float rad( float x ) { return x * SG_DEGREES_TO_RADIANS; }
  inline float deg( float x ) { return x * SG_RADIANS_TO_DEGREES; }

  inline void scale( float x, float y, GLfloat *cx, GLfloat *cy ) {
    *cx = (view_size / 2 + x * zoom);
    *cy = (view_size / 2 + y * zoom);
  }

  static const char* TXF_FONT_NAME;

  char* font_name;

  // viewport
  GLfloat view_left, view_top, view_size;

  // geographic location
  float clat, clon, scle, zoom;

  struct Coord {
    int lat, lon;
  };

  struct Tilewidth {
    GLfloat rn, rs;
  };

  struct MapTile {
    Coord   c;
    Tilewidth w;
    GLuint  texture_handle;
    GLfloat xsw, ysw, xnw, ynw, xno, yno, xso, yso;
    GLubyte *texbuf;
  };

  struct TileLess {
    size_t operator()(const Coord &v1, const Coord &v2) const {
      return (v1.lat < v2.lat || (v1.lat == v2.lat && v1.lon < v2.lon));
    }
  };

  typedef map<Coord, MapTile*, TileLess> TileTable;

  list<MapTile*> tiles;
  TileTable tiletable;

  char mpath[512];
  int  pathl, features;
  bool textured, texturedFonts;
  OutputGL *output;
  Overlays *overlay;
  FlightTrack *track;
  Projection *projection;
};
