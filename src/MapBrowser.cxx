/*-------------------------------------------------------------------------
  MapBrowser.cxx
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

#include <stdio.h>
#include <string.h>
#include "MapBrowser.hxx"
#include "Geodesy.hxx"
#include "LoadPng.hxx"

MapBrowser::MapBrowser(GLfloat left, GLfloat top, GLfloat size, int features,
		       char *fg_root, bool texturedFonts) {
  view_left = left;
  view_top  = top;
  view_size = size;

  clat  = 0.0f;
  clon  = 0.0f;

  mpath[0] = 0;
  pathl = 0;

  scle = 100000;
  zoom = view_size / scle;
  this->features = features;
  this->texturedFonts = texturedFonts;

  output = new OutputGL( NULL, size, false, texturedFonts );
  output->setShade(false);

  // setup overlays
  overlay = new Overlays( fg_root, zoom, size );
  overlay->setOutput( output );
  overlay->setFeatures(features);

  textured = true;
  track = NULL;
}

MapBrowser::~MapBrowser() {
  delete output;
  delete overlay;
}

void MapBrowser::setLocation( float lat, float lon ) {
  clat = lat;
  clon = lon;

  overlay->setLocation(lat, lon);

  update();
}

void MapBrowser::setScale( float scale ) {
  scle = scale;
  zoom = view_size / scle;

  overlay->setScale( zoom );

  update();
}

void MapBrowser::setSize( GLfloat size ) {
  view_size = size;
  zoom = view_size / scle;

  delete output;
  output = new OutputGL(NULL, (int)size, false, texturedFonts);
  overlay->setOutput( output );
  overlay->setScale( zoom );
  output->setShade(false);

  update();
}

void MapBrowser::setMapPath( char *path ) {
  strcpy( mpath, path );
  pathl = strlen( mpath );
}

void MapBrowser::setFGRoot( char *fg_root ) {
  overlay->setFGRoot( fg_root );
}

void MapBrowser::setFeatures( int features ) {
  this->features = features;
  overlay->setFeatures(features);
}

void MapBrowser::setTextured( bool textured ) {
  this->textured = textured;
}

void MapBrowser::setFlightTrack( FlightTrack *track ) {
  this->track = track;
  overlay->setFlightTrack(track);
}

void MapBrowser::loadDb() {
  overlay->load_airports();
  overlay->load_navaids();
}

void MapBrowser::draw() {
  glDisable(GL_BLEND);
  glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

  if (textured) {
    glEnable( GL_TEXTURE_2D );
    GLfloat tilesize = earth_radius_lat(clat) * M_PI / 180.0f * zoom;

    for (list<MapTile*>::iterator i = tiles.begin(); i != tiles.end(); i++) {
      MapTile *tile = *i;

      glBindTexture(GL_TEXTURE_2D, tile->texture_handle);
    
      glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 1.0f); 
      glVertex2f(tile->x - tilesize/2, tile->y - tilesize/2 );
      glTexCoord2f(1.0f, 1.0f); 
      glVertex2f(tile->x + tilesize/2, tile->y - tilesize/2 );
      glTexCoord2f(1.0f, 0.0f); 
      glVertex2f(tile->x + tilesize/2, tile->y + tilesize/2 );
      glTexCoord2f(0.0f, 0.0f); 
      glVertex2f(tile->x - tilesize/2, tile->y + tilesize/2 );
      glEnd();
    }

    glDisable( GL_TEXTURE_2D );
  }

  glEnable(GL_BLEND);
  overlay->drawOverlays();
}

void MapBrowser::update() {
  float dlat, dlon;

  if (!textured)
    return;

  lat_ab( scle/2, scle/2, clat, clon, &dlat, &dlon );
  dlat -= clat;
  dlon -= clon;

  int sgnlat = (clat < 0.0f) ? 1 : 0;
  int sgnlon = (clon < 0.0f) ? 1 : 0;
  // calculate minimum and maximum latitude/longitude of displayed tiles
  int min_lat = (int)( (clat - dlat) * 180.0f / M_PI ) - sgnlat;
  int max_lat = (int)( (clat + dlon) * 180.0f / M_PI ) - sgnlat;
  int min_lon = (int)( (clon - dlon) * 180.0f / M_PI ) - sgnlon;
  int max_lon = (int)( (clon + dlon) * 180.0f / M_PI ) - sgnlon;
  int num_lat = (max_lat - min_lat) + 1, num_lon = (max_lon - min_lon) + 1;

  // remove old tiles
  for (list<MapTile*>::iterator i = tiles.begin(); i != tiles.end(); i++) {
    MapTile *tile = *i;

    if (tile->c.lat < min_lat - CACHE_LIMIT || 
	tile->c.lat > max_lat + CACHE_LIMIT ||
	tile->c.lon < min_lon - CACHE_LIMIT || 
	tile->c.lon > max_lon + CACHE_LIMIT) {
 
      list<MapTile*>::iterator tmp = i; tmp++;
      glDeleteTextures( 1, &tile->texture_handle );
      tiles.erase( i );
      tiletable.erase( tile->c );
      delete tile->texbuf;
      delete tile;
      i = tmp;
    } else {
      // update tiles position
      float x, y, r;
      ab_lat( rad(tile->c.lat+0.5f), rad(tile->c.lon+0.5f), clat, clon, 
	      &x, &y, &r );
      scale( x, y, &tile->x, &tile->y );
    }
  }

  // load needed tiles
  for (int i = 0; i < num_lat; i++) {
    for (int j = 0; j < num_lon; j++) {
      int wid, hei;
      Coord c;
      c.lat = min_lat + i;
      c.lon = min_lon + j;

      TileTable::iterator t = tiletable.find(c);
      MapTile *nt = tiletable[c];

      if (t == tiletable.end()) {  // check if the tile has been loaded
	// Load a new tile
	nt = new MapTile;
	nt->c.lat = c.lat;
	nt->c.lon = c.lon;
	
	sprintf( mpath+pathl, "%c%03d%c%02d.png", 
		 (c.lon < 0)?'w':'e', abs(c.lon),
		 (c.lat < 0)?'s':'n', abs(c.lat) );	     

	if ( (nt->texbuf = (GLubyte*)loadPng( mpath, &wid, &hei )) != NULL ) {
	  glPixelStorei(GL_UNPACK_ALIGNMENT, 3);
	  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	  glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	  //	  glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	  glGenTextures( 1, &nt->texture_handle );
	  glBindTexture( GL_TEXTURE_2D, nt->texture_handle );
	  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
	  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
	  glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, wid, hei, 0, GL_RGB, 
			GL_UNSIGNED_BYTE, nt->texbuf );
	  
	  
	  tiles.push_back(nt);
	  tiletable[nt->c] = nt;
	} else {
	  // Tile couldn't be loaded
	  delete nt;
	}

	float x, y, r;
	ab_lat( rad(nt->c.lat+0.5f), rad(nt->c.lon+0.5f), clat, clon, 
		&x, &y, &r );
	scale( x, y, &nt->x, &nt->y );
      }
    }
  }

}
