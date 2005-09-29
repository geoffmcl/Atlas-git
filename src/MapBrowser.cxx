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
#include "LoadJpg.hxx"

const char* MapBrowser::TXF_FONT_NAME = "/Fonts/helvetica_medium.txf";

MapBrowser::MapBrowser(GLfloat left, GLfloat top, GLfloat size, int features,
                       char *fg_root, int m, bool texturedFonts) :
  view_left(left), view_top(top), view_size(size), clat(0.0f), clon(0.0f),
  scle(100000), pathl(0), features(features), texturedFonts(texturedFonts),
  mode(m)
{
  mpath[0] = 0;

  zoom = view_size / scle;

  if (texturedFonts) {
    font_name = new char[strlen(fg_root) + strlen(TXF_FONT_NAME) + 1];
    strcpy(font_name, fg_root);
    strcat(font_name, TXF_FONT_NAME);
  } else {
    font_name = NULL;
  }

  output = new OutputGL( NULL, (int)size, false, texturedFonts, font_name );
  output->setShade(false);

  // setup overlays
  overlay = new Overlays( fg_root, zoom, size );
  overlay->setOutput( output );
  overlay->setFeatures(features);

  projection = new Projection;
  overlay->setProjection(projection);
  textured = true;
  track = NULL;
}

MapBrowser::~MapBrowser() {
  delete font_name;
  delete output;
  delete overlay;
  delete projection;
}

void MapBrowser::setProjectionByID(int id) {
   projection->setSystem(id);
   update();
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
  output = new OutputGL(NULL, (int)size, false, texturedFonts, font_name);
  overlay->setOutput( output );
  overlay->setScale( zoom );
  output->setShade(false);

  update();
}

void MapBrowser::setMapPath( char *path ) {
  strcpy( mpath, path );
  pathl = strlen( mpath );
}

void MapBrowser::changeResolution(char *path) {
  MapTile *tile;
  list<MapTile*>::iterator i = tiles.end(),
                           itmp;
  i--;
  while (tiles.begin() != tiles.end()) {
     tile =*i;
     glDeleteTextures( 1, &tile->texture_handle );
     itmp = i--;
     tiles.erase( itmp );
     tiletable.erase(tile->c);
     delete tile;
  }
  
  setMapPath(path);
  update();
}

/*
void MapBrowser::setFGRoot( char *fg_root ) {
  overlay->setFGRoot( fg_root );
}
*/

void MapBrowser::setFeatures( int features ) {
  this->features = features;
  overlay->setFeatures(features);
}

void MapBrowser::toggleFeaturesAllNavaids() {
  features = features ^ ( Overlays::OVERLAY_NAVAIDS |
                          Overlays::OVERLAY_NAVAIDS_VOR |
                          Overlays::OVERLAY_NAVAIDS_NDB |
			  Overlays::OVERLAY_NAVAIDS_ILS |
                          Overlays::OVERLAY_NAVAIDS_FIX );
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
  overlay->load_new_navaids();
  overlay->load_new_fixes();
}

void MapBrowser::draw() {
  glDisable(GL_BLEND);
  glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

  if (textured) {
    glEnable( GL_TEXTURE_2D );
    GLfloat tilesize = earth_radius_lat(clat) * SG_DEGREES_TO_RADIANS;

    for (list<MapTile*>::iterator i = tiles.begin(); i != tiles.end(); i++) {
      MapTile *tile = *i;
      if ( tile->tex ) {
	GLfloat dxs = 0.5f;
	GLfloat dxn = 0.5f;
        if ( mode == ATLAS ) {
	  dxs = tile->w.rs / tilesize / 2.0f;
	  dxn = tile->w.rn / tilesize / 2.0f;
          if ( abs( tile->c.lat ) >= 83 ) {
            dxs *= 2.0f;
            dxn *= 2.0f;
          }
          if ( abs( tile->c.lat ) >= 86 ) {
            dxs *= 2.0f;
            dxn *= 2.0f;
          }
          if ( abs( tile->c.lat ) >= 88 ) {
            dxs *= 2.0f;
            dxn *= 2.0f;
          }
        }

	glBindTexture(GL_TEXTURE_2D, tile->texture_handle);
    
	glBegin(GL_QUADS);
	glTexCoord2f(0.5f-dxs, 1.0f); 
	glVertex2f(tile->xsw, tile->ysw );
	glTexCoord2f(0.5f+dxs, 1.0f); 
	glVertex2f(tile->xso, tile->yso );
	glTexCoord2f(0.5f+dxn, 0.0f); 
	glVertex2f(tile->xno, tile->yno );
	glTexCoord2f(0.5f-dxn, 0.0f); 
	glVertex2f(tile->xnw, tile->ynw );
	glEnd();
      }
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
  if (dlon > SG_PI/2) dlon = SG_PI/2;

  int sgnlat = (clat < 0.0f) ? 1 : 0;
  int sgnlon = (clon < 0.0f) ? 1 : 0;
  // calculate minimum and maximum latitude/longitude of displayed tiles
  int min_lat = (int)( (clat - dlat) * SG_RADIANS_TO_DEGREES ) - sgnlat;
  int max_lat = (int)( (clat + dlat) * SG_RADIANS_TO_DEGREES ) - sgnlat;
  int min_lon = (int)( (clon - dlon) * SG_RADIANS_TO_DEGREES ) - sgnlon;
  int max_lon = (int)( (clon + dlon) * SG_RADIANS_TO_DEGREES ) - sgnlon;
  if (min_lat < -90) min_lat = -90;
  if (max_lat >  90) max_lat =  90;
  int num_lat = (max_lat - min_lat) + 1, num_lon = (max_lon - min_lon) + 1;

  for (list<MapTile*>::iterator it = tiles.begin(); it != tiles.end(); it++) {
    MapTile *tile = *it;

    // remove old tiles
    if (tile->c.lat < min_lat - CACHE_LIMIT || 
         tile->c.lat > max_lat + CACHE_LIMIT ||
         tile->c.lon < min_lon - CACHE_LIMIT || 
         tile->c.lon > max_lon + CACHE_LIMIT) {
 
      list<MapTile*>::iterator tmp = it; tmp++;
      if ( tile->tex ) {
        glDeleteTextures( 1, &tile->texture_handle );
      }
      tiles.erase( it );
      tiletable.erase( tile->c );
      delete tile;
      it = tmp;
    } else {
      // update tiles position
      sgVec3 xyr;
      float dx;
      if ( abs( tile->c.lat ) < 83 ) {
        dx = 1.0f;
      } else if ( abs( tile->c.lat ) < 86 ) {
        dx = 2.0f;
      } else if ( abs( tile->c.lat ) < 88 ) {
        dx = 4.0f;
      } else {
        dx = 8.0f;
      }
      projection->ab_lat( rad((float) tile->c.lat), rad((float) tile->c.lon), clat, clon, 
            xyr );
      scale( xyr[0], xyr[1], &tile->xsw, &tile->ysw );
      projection->ab_lat( rad(tile->c.lat+1.0f), rad((float) tile->c.lon), clat, clon, 
            xyr );
      scale( xyr[0], xyr[1], &tile->xnw, &tile->ynw );
      projection->ab_lat( rad(tile->c.lat+1.0f), rad(tile->c.lon+dx), clat, clon, 
            xyr );
      scale( xyr[0], xyr[1], &tile->xno, &tile->yno );
      projection->ab_lat( rad((float) tile->c.lat), rad(tile->c.lon+dx), clat, clon, 
            xyr );
      scale( xyr[0], xyr[1], &tile->xso, &tile->yso );
    }
  }
  
  // load needed tiles
  for (int i = 0; i < num_lat; i++) {
    for (int j = 0; j < num_lon; j++) {
      int wid, hei;
      float tlat;
      Coord c;
      c.lat = min_lat + i;
      c.lon = min_lon + j;

      TileTable::iterator t = tiletable.find(c);

      if ( t == tiletable.end() ) {  // check if the tile has been loaded
        // Load a new tile
        MapTile *nt = new MapTile;
        nt->tex = false;
        nt->c = c;
        tlat = (float) c.lat * SG_DEGREES_TO_RADIANS;
        nt->w.rs = earth_radius_lat(tlat) * cos(tlat) * SG_DEGREES_TO_RADIANS;
        tlat = (float) (c.lat + 1) * SG_DEGREES_TO_RADIANS;
        nt->w.rn = earth_radius_lat(tlat) * cos(tlat) * SG_DEGREES_TO_RADIANS;

        sprintf( mpath+pathl, "%c%03d%c%02d.png", 
                 (c.lon < 0)?'w':'e', abs(c.lon),
                 (c.lat < 0)?'s':'n', abs(c.lat) );         

        //printf("Loading tile %s...\n", mpath);

        GLubyte *texbuf;
	if ( (texbuf = (GLubyte*)loadPng( mpath, &wid, &hei )) == NULL ) {
	  sprintf( mpath+pathl, "%c%03d%c%02d.jpg", 
		   (c.lon < 0)?'w':'e', abs(c.lon),
		   (c.lat < 0)?'s':'n', abs(c.lat) );         
	  texbuf = (GLubyte*)loadJpg( mpath, &wid, &hei );
	}
	if ( texbuf != NULL ) {
          glPixelStorei(GL_UNPACK_ALIGNMENT, 3);
          glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
          glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
          glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
          //     glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
          glGenTextures( 1, &nt->texture_handle );
          glBindTexture( GL_TEXTURE_2D, nt->texture_handle );
          glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
          glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
          glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
          glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
          glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
          glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, wid, hei, 0, GL_RGB, 
                         GL_UNSIGNED_BYTE, texbuf );
          delete texbuf;
          nt->tex = true;
        } else {
          // printf("Tile %s couldn't be loaded\n",mpath);
	  // texbuf is NULL; texture_handle is undefined.
        }

	sgVec3 xyr;
        float dx;
        if ( abs( nt->c.lat ) < 83 ) {
          dx = 1.0f;
        } else if ( abs( nt->c.lat ) < 86 ) {
          dx = 2.0f;
        } else if ( abs( nt->c.lat ) < 88 ) {
          dx = 4.0f;
        } else {
          dx = 8.0f;
        }
	projection->ab_lat( rad((float) nt->c.lat), rad((float) nt->c.lon), clat, clon, 
		xyr );
	scale( xyr[0], xyr[1], &nt->xsw, &nt->ysw );
	projection->ab_lat( rad(nt->c.lat+1.0f), rad((float) nt->c.lon), clat, clon, 
		xyr );
	scale( xyr[0], xyr[1], &nt->xnw, &nt->ynw );
	projection->ab_lat( rad(nt->c.lat+1.0f), rad(nt->c.lon+dx), clat, clon, 
		xyr );
	scale( xyr[0], xyr[1], &nt->xno, &nt->yno );
	projection->ab_lat( rad((float) nt->c.lat), rad(nt->c.lon+dx), clat, clon, 
		xyr );
	scale( xyr[0], xyr[1], &nt->xso, &nt->yso );

	tiles.push_back(nt);
	tiletable[nt->c] = nt ;
      }
    }
  }
}
