/*-------------------------------------------------------------------------
  MapMaker.cxx

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

#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <zlib.h>

#include "MapMaker.hxx"
/*#include <simgear/magvar/magvar.hxx>*/

MapMaker::MapMaker( char *fg_root, char *ap_filter, int features,
		    int size, int scale ) {
  this->fg_root = NULL;
  setFGRoot(fg_root);
  arp_filter = ap_filter;
  this->features = features;
  this->size = size;
  scle = scale;

  modified = false;
  palette_loaded = false;

  setLight( -1, -1, 2 );       // default lighting

  // setup indices for the elevation levels colours
  // (these might be overridden by read_materials)
  for (int i = 0; i < MAX_ELEV_LEVELS; i++) {
    elev_colindex[i] = 1+i;
  }

  number_elev_levels = 1;
  elev_height[0] = -1;
}

MapMaker::~MapMaker() {
  for (StrMap::iterator it = materials.begin(); it != materials.end(); it++) {
    delete (*it).first;
  }

  for (int i = 0; i < palette.size(); i++) {
    delete palette[i];
  }

  delete fg_root;
}

void MapMaker::setFGRoot( char *fg_root ) {
  delete this->fg_root;
  if (fg_root == NULL) {
    fg_root = getenv("FG_ROOT");
    if (fg_root == NULL) {
      fg_root = "/usr/local/lib/FlightGear";
    }
  }
   
  this->fg_root = new char[strlen(fg_root)];
  strcpy(this->fg_root, fg_root);
}

void MapMaker::setPalette( char* filename ) {
  if (palette_loaded) {
    for (StrMap::iterator it = materials.begin(); it != materials.end(); 
	 it++) {
      delete (*it).first;
    }
    
    for (int i = 0; i < palette.size(); i++) {
      delete palette[i];
    }
  }

  palette_loaded = false;

  read_materials(filename);
}

int MapMaker::createMap(GfxOutput *output,float theta, float alpha, 
			bool do_square) {
  this->output = output;
  
  modified = false;

  if (!palette_loaded)
    read_materials();

  if (!palette_loaded) {
    fprintf(stderr, "No palette loaded.\n");
    return 0;
  }

  output->clear( palette[0] );

  // set up coordinate space
  sgVec3 xyz;
  float r_e;
  xyz_lat( theta, alpha, xyz, &r_e );
  sgMat4 rotation_matrix;
  sgMakeRotMat4(rotation_matrix, 
		alpha * SG_RADIANS_TO_DEGREES,
		0.0f,
		90.0f - theta * SG_RADIANS_TO_DEGREES);
  sgCopyVec3(light_vector, map_light);
  sgXformVec3(light_vector, rotation_matrix);
  output->setLightVector( light_vector );

  // calculate which tiles we will have to load
  float dtheta, dalpha;

  if (do_square) {
    // set scale to 1 degree in meters at this latitude
    scle = (int)(earth_radius_lat(alpha)*2.0f*SG_PI / 360.0f);   
    dtheta = dalpha = 0.8f * SG_DEGREES_TO_RADIANS;
  } else {
    lat_ab( scle/2, scle/2, theta, alpha, &dtheta, &dalpha );
    dtheta -= theta;
    dalpha -= alpha;
  }

  int sgntheta = (theta < 0.0f) ? 1 : 0, sgnalpha = (alpha < 0.0f) ? 1 : 0;
  // Rainer Emrich's improved code for finding map boundaries
  int mid_theta = (int)( theta * SG_RADIANS_TO_DEGREES ) - sgntheta;
  int mid_alpha = (int)( alpha * SG_RADIANS_TO_DEGREES ) - sgnalpha;
  int int_dtheta = (int)( dtheta * SG_RADIANS_TO_DEGREES + 0.6f);
  int int_dalpha = (int)( dalpha * SG_RADIANS_TO_DEGREES + 0.5f);
  int max_theta = mid_theta + int_dtheta;
  int min_theta = mid_theta - int_dtheta;
  int max_alpha = mid_alpha + int_dalpha;
  int min_alpha = mid_alpha - int_dalpha;
  
  zoom = (float)size / (float)scle;

  // load the tiles and do actual drawing
  char *subdir = new char[strlen(fg_root) + 512];  /* 512 should be *very* 
						      safe (too safe?) */
  strcpy( subdir, fg_root );
  strcat( subdir, "/Scenery/" );
  int slen = strlen( subdir );       // index to where we should append

  if (getVerbose());
    //printf("Map area: lat %c%d / %c%d, lon %c%d / %c%d.\n", 
    //   ns(min_theta), abs(min_theta), ns(max_theta), abs(max_theta), 
    //   ew(min_alpha), abs(min_alpha), ew(max_alpha), abs(max_alpha) );
 
  for (int k = min_theta; k <= max_theta; k++) {
    for (int l = min_alpha; l <= max_alpha; l++) {
      if (l > 179) {
	process_directory( subdir, slen, k, l - 360, xyz );
      } else if (l < -180) {
	process_directory( subdir, slen, k, l + 360, xyz );
      } else {
	process_directory( subdir, slen, k, l, xyz );
      }
    }
  }

  delete[] subdir;

  if (modified) {
    Overlays overlays( fg_root, zoom, size );
    overlays.setOutput( output );
    overlays.setAirportColor( palette[materials["RunwayOutline"]],
			      palette[materials["RunwayFill"]] );
    overlays.setNavaidColor( palette[materials["NavaidLabels"]] );
    overlays.setLocation( theta, alpha );

    int features = Overlays::OVERLAY_NAMES | Overlays::OVERLAY_IDS;
    if (getAirports()) features += Overlays::OVERLAY_AIRPORTS;
    if (getNavaids()) features += Overlays::OVERLAY_NAVAIDS;    
    overlays.setFeatures(features);

    overlays.drawOverlays();
  } else if (getVerbose()) {
    printf("Nothing to draw - no output written.\n");
  }

  output->closeOutput();
  return 1;
}

void MapMaker::sub_trifan( list<int> &indices, vector<float*> &v, 
			   vector<float*> &n ) {
  list<int>::iterator index = indices.begin();
  int cvert = *(index++);      // index of central vertex
  int vert2 = *(index++);      // the first vertex of the circumference

  sgVec3 t[3], nrm[3];
  float length2 = 0.6f * scle;

  // get the two first vertices
  sgCopyVec3( t[0], v[cvert]);
  sgCopyVec3( t[1], v[vert2]);
  sgCopyVec3( nrm[0], n[cvert]);
  sgCopyVec3( nrm[1], n[vert2]);

  // now loop over each triangle in the fan
  while ( index != indices.end() ) {
    vert2 = *(index++);

    // get third vertex
    sgCopyVec3( t[2], v[vert2] );
    sgCopyVec3( nrm[2], n[vert2] );
    
    // check whether this triangle is visible (?)
    if (fabs(t[0][0]) < length2 || fabs(t[0][1]) < length2 ||
	fabs(t[1][0]) < length2 || fabs(t[1][1]) < length2 ||
	fabs(t[2][0]) < length2 || fabs(t[2][1]) < length2) {
      
      int ctimes = 0, max_ctimes = 0;
      int times[3], order[3], oind[3];
      float levels[MAX_ELEV_LEVELS][4];
      sgVec3 normal[MAX_ELEV_LEVELS][2];

      for (int k = 0; k < number_elev_levels; k++) { 
	levels[k][0] = 0.0f; levels[k][1] = 0.0f; 
	levels[k][2] = 0.0f; levels[k][3] = 0.0f;
      }
      
      // check for each line how many elevation levels it passes
      for (int j = 0; j < 3; j++) {
	int next = (j+1) % 3;
	
	ctimes = abs( elev2colour((int)t[j][2]) - 
		      elev2colour((int)t[next][2]) );
	times[j] = ctimes;

	if (ctimes > 0) {
	  float lx, ly;
	  int lh;
	  sgVec3 dummy_normal;
	  level_triangle( (float*)&t[j], (float*)&t[next], nrm[j], nrm[next], 
			  ctimes, &lx, &ly, dummy_normal, &lh );

	  order[j] = elev_height[lh];
	  if (ctimes > max_ctimes) max_ctimes = ctimes;
	} else
	  order[j] = 0;

	oind[j] = j;
      }

      // if some line stretches across more than one elevation level
      if (max_ctimes > 0) {      // we have to subdivide
	for (int j = 0; j < 3; j++) {
	  /* pick the index of the greatest value of order 
	     (that has not already been used) */
	  for (int k = j+1; k < 3; k++) {
	    if (order[k] > order[j]) {
	      int tmp = order[j]; order[j] = order[k]; order[k] = tmp;
	      tmp = oind[j]; oind[j] = oind[k]; oind[k] = tmp;
	    }
	  }

	  int idx = oind[j];      /* idx is the index of the vertex 
				     with highest elevation */
	  int next = (idx + 1) % 3;

	  // split this line times[idx] times 
	  for (int k = times[idx]; k > 0; k--) {
	    float lx, ly;
	    int lh;
	    sgVec3 lnormal;
	    level_triangle( (float*)&t[idx], (float*)&t[next], 
			    nrm[idx], nrm[next], k, &lx, &ly, lnormal, &lh );
	    int pmin = (t[idx][2] < t[next][2]) ? idx : next;
	    
	    if (levels[lh][2] != 0.0f) {
	      // this line can now be transformed into a polygon
	      int ecol = lh + 1;
	      float sh;

	      sgVec2 quadverts[4];
	      sgVec3 quadnorms[4];
	      sgSetVec2( quadverts[0], 
			 scale(levels[lh][2], size, zoom), 
			 scale(levels[lh][3], size, zoom) );
	      sgSetVec2( quadverts[1], 
			 scale(levels[lh][0], size, zoom), 
			 scale(levels[lh][1], size, zoom) );
	      sgSetVec2( quadverts[2],
			 scale(lx, size, zoom), scale(ly, size, zoom) );
	      sgSetVec2( quadverts[3],
			 scale(t[pmin][0], size, zoom), 
			 scale(t[pmin][1], size, zoom) );
	      sgCopyVec3(quadnorms[0], normal[lh][1]);
	      sgCopyVec3(quadnorms[1], normal[lh][0]);
	      sgCopyVec3(quadnorms[2], lnormal);
	      sgCopyVec3(quadnorms[3], nrm[pmin]);

	      output->setColor(palette[ecol]);
	      output->drawQuad(quadverts, quadnorms);
	    } else {
	      levels[lh][0] = lx;
	      levels[lh][1] = ly;
	      levels[lh][2] = t[pmin][0];
	      levels[lh][3] = t[pmin][1];
	      sgCopyVec3(normal[lh][0], lnormal);
	      sgCopyVec3(normal[lh][1], nrm[pmin]);
	    }
	  }
	} 
      }
    }

    sgCopyVec3( t[1], t[2] );
    sgCopyVec3( nrm[1], nrm[2] );
  }
}

void MapMaker::draw_trifan( list<int> &indices, vector<float*> &v, 
			    vector<float*> &n, int col ) {
  list<int>::iterator index = indices.begin();
  int cvert = *(index++), vert2 = *(index++);

  sgVec3 t[3], nrm[3];
  sgVec2 p[3];

  sgCopyVec3( t[0], v[cvert] );
  sgCopyVec3( t[1], v[vert2] );
  sgCopyVec3( nrm[0], n[cvert] );
  sgCopyVec3( nrm[1], n[vert2] );
  sgSetVec2( p[0], scale(t[0][0], size, zoom), scale(t[0][1], size, zoom) );
  sgSetVec2( p[1], scale(t[1][0], size, zoom), scale(t[1][1], size, zoom) );

  while ( index != indices.end() ) {
    vert2 = *(index++);

    sgCopyVec3( t[2], v[vert2] );
    sgCopyVec3( nrm[2], n[vert2] );
    sgSetVec2( p[2], scale(t[2][0], size, zoom), scale(t[2][1], size, zoom) );

    if (col == 12 || col == 13) {
      // do not shade ocean/lake/etc.
      output->setColor(palette[col]);
    } else {
      int dcol;
      if (col>=0) {
	dcol = col;
      } else {
	dcol = elev2colour((int)((t[0][2] + t[1][2] + t[2][2]) / 3.0f));
      }
      output->setColor(palette[dcol]);
    }

    output->drawTriangle( p, nrm );

    sgCopyVec2( p[1], p[2] );
    sgCopyVec3( t[1], t[2] );
    sgCopyVec3( nrm[1], nrm[2] );
  }

  if (col < 0) {
    sub_trifan( indices, v, n );
  }
}

int MapMaker::process_file( char *tile_name, sgVec3 xyz ) {
  float cr;               // reference point (gbs)
  sgVec3 gbs, tmp;
  int scount = 0;
  int i1, i2, material = 16;
  int verts = 0, normals = 0;
  vector<float*> v;                   // vertices
  vector<float*> n;                   // normals

  char lbuffer[4096];       /* will there be longer lines in
					scenery files? */
  char *token, delimiters[] = " \t\n";

  // setup some reasonable sizes for vectors
  v.reserve(1024);
  n.reserve(1024);

  gzFile tf = gzopen( tile_name, "rb" );

  if (tf == NULL) {
    fprintf( stderr, "process_file: Couldn't open \"%s\".\n", tile_name );
    exit(1);
    return 0;
  }

  // read the first line
  gzgets( tf, lbuffer, 4096 );
  if ( strncmp(lbuffer, "# FGFS Scenery", 14) != 0 ) {
    gzclose(tf);
    return 0;
  }

  modified = true;

  while ( (gzgets(tf, lbuffer, 4096) != Z_NULL) ) {
    switch (lbuffer[0]) {
    case '#': {
      token = strtok(lbuffer+1, delimiters);

      if ( strcmp(token, "gbs") == 0 ) {
	for (int i = 0; i < 3; i++) {
	  token = strtok(NULL, delimiters);
	  gbs[i] = atof(token);
	}
      } else if ( strcmp(token, "usemtl") == 0 ) {
	token = strtok(NULL, delimiters);
	StrMap::iterator mat_it = materials.find(token);
	
	if (mat_it == materials.end()) {
	  material = -1;  // unknown material
	} else {
	  material = (*mat_it).second;
	}
      }
    }
    break;

    case 'v': {
      if (lbuffer[1] == 'n') {
	// Make a new normal
	float *nn = new sgVec3;

	token = strtok(lbuffer+2, delimiters);
	for (int i = 0; i < 3; i++) {
	  nn[i] = atof(token);
	  token = strtok(NULL, delimiters);
	}

	n.push_back( nn );
	normals++;
      } else if (lbuffer[1] == ' ') {
	float *nv = new sgVec3;
	token = strtok(lbuffer+2, delimiters);
	for (int i = 0; i < 3; i++) {
	  tmp[i] = atof(token);
	  token = strtok(NULL, delimiters);
	}

	sgAddVec3(tmp, gbs);
	double pr = sgLengthVec3( tmp );
	ab_xy( tmp, xyz, nv );
	// nv[2] contains the sea-level z-coordinate - calculate this vertex'
	// altitude:
	nv[2] = pr - nv[2];
	v.push_back( nv );
	verts++;
      }
    }
    break;

    case 't': {
      if (strncmp(lbuffer, "tf ", 3) == 0) {
	// Triangle fan
	list<int> vertex_indices;

	token = strtok(lbuffer+2, " /");
	while ( token != NULL ) {
	  i1 = atoi(token);
	  token = strtok(NULL, delimiters);
	  i2 = atoi(token);

	  if (i1 >= verts || i1 >= normals) {
	    fprintf(stderr, "Tile \"%s\" contains triangle indices out of " \
		    "bounds.\n", tile_name);
	    vertex_indices.clear();
	    break;
	  }
	  
	  vertex_indices.push_back( i1 );
	  token = strtok(NULL, " /");
	}
	
	if ( !vertex_indices.empty() ) {
	  draw_trifan( vertex_indices, v, n, material );
	  polys++;
	}
      }
    }
    break;
    }	
  }

  gzclose( tf );

  unsigned int i;
  for (i = 0; i < v.size(); i++) {
    delete v[i];
  }
  for (i = 0; i < n.size(); i++) {
    delete n[i];
  }

  return 1;
}

// path must be 'FG_ROOT/Scenery/' - more will be appended
// plen is path length
int MapMaker::process_directory( char *path, int plen, int lat, int lon, 
				 sgVec3 xyz ) {
  int sgnk = (lat < 0) ? 1 : 0, sgnl = (lon < 0) ? 1 : 0;

  int llen = sprintf( path + plen, "%c%03d%c%02d/%c%03d%c%02d", 
		      ew(lon), abs((lon+sgnl) / 10 * 10) + sgnl*10, 
		      ns(lat), abs((lat+sgnk) / 10 * 10) + sgnk*10,
		      ew(lon), abs(lon), ns(lat), abs(lat) );

  DIR *dir;
  dirent *ent;

  if (getVerbose()) 
    printf("%s:  ", path + plen);

  if ((dir = opendir(path)) == NULL) {
    if (getVerbose()) printf("\n");
    return 0;
  }

  path[plen + llen] = '/';  
  while ((ent = readdir(dir)) != NULL) {
    if (ent -> d_name[0] == '.')
      continue;  

    if (getVerbose()) {
      putc( '.', stdout );
      fflush(stdout);
    }                                      

    strcpy( path + plen + llen + 1, ent -> d_name );
    process_file( path, xyz );
  }

  if (getVerbose()) putc('\n', stdout);
  closedir(dir);

  return 1;
}

void MapMaker::read_materials(char *fname /* = NULL */) {
  char *filename;
  if (fname == NULL) {
    char* fgroot = getFGRoot();
    filename = new char[strlen(fgroot) + 32];
    strcpy(filename, fgroot);
    strcat(filename, "/AtlasPalette");
  } else {
    filename = fname;
  }

  FILE *fd = fopen(filename, "r");
  if (fd == NULL) {
    fprintf(stderr, "Could not read palette from file \"%s\".\n", filename);
    return;
  }

  char line[256], *token, delimiters[] = " \t", *material;
  int index, elev_level = 0;
  float* colour;
  sgVec4 black = { 0.0f, 0.0f, 0.0f, 0.0f };
  fgets(line, 256, fd);

  // we might as well reserve space for 32 colours from start
  palette.reserve(32);

  while ( !feof(fd) ) {
    switch (line[0]) {
    case '#':       // comment, ignore
      break;
    case 'C':
    case 'c':
      token = strtok(line, delimiters); // "Colour"
      token = strtok(NULL, delimiters); // index
      index = atoi(token);

      colour = new sgVec4;
      for (int i = 0; i < 4; i++) {
	token = strtok(NULL, delimiters);
	colour[i] = atof(token);
      }

      // make sure we have at least index entries in the palette
      while (palette.size() < index)
	palette.push_back(black);

      if ( index < palette.size() ) {
	palette[index] = colour;
      } else {
	palette.push_back(colour);
      }

      break;

    case 'M':
    case 'm':
      token = strtok(line, delimiters); // "Material"

      token = strtok(NULL, delimiters); // material name
      material = new char[strlen(token)+1];
      strcpy(material, token);

      token = strtok(NULL, delimiters); // index
      index = atoi(token);

      materials[material] = index;

      int height;
      if ( sscanf(material, "Elevation_%d", &height) == 1 ) {
	if (elev_level >= MAX_ELEV_LEVELS) {
	  fprintf(stderr, "Only %d elevation levels allowed.\n", 
		  MAX_ELEV_LEVELS);
	} else {
	  elev_height  [elev_level] = height;
	  elev_colindex[elev_level] = index;
	  elev_level++;
	  if (elev_level > number_elev_levels) number_elev_levels++;
	}
      }

      break;

    default:
      fprintf(stderr, 
	      "Syntax error in file \"%s\". Line:\n\t%s\n", filename, line);
      break;
    }

    fgets(line, 256, fd);
  }

  fclose(fd);

  if (fname == NULL)
    delete filename;

  palette_loaded = true;
}
