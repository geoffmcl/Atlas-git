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

// From the FG materials file
// (this ought to be read from that file in some way)
const char *MapMaker::materials[16] = { "AirportKeep", "Concrete", "Default", 
					"Urban", "Unknown", "Glacier", "Ocean",
					"Lake", "IntermittentLake", "DryLake", 
					"Reservoir", "IntermittentReservoir", 
					"Stream", "Marsh", "Grass", "Cloud" };

const int MapMaker::colours[16] = { 9, 16, -1, 15, 16, -1, 13, 12, 12, -1, 12, 
				    12, 12, -1, -1, 16 };

/* These nice colour settings are (almost?) from the original
   script by Alexei Novikov. */
const float MapMaker::rgb[17][4] = { {0.761, 0.839, 0.847, 1.0}, 
				     {0.647, 0.729, 0.647, 1.0}, 
				     {0.859, 0.906, 0.804, 1.0}, 
				     {0.753, 0.831, 0.682, 1.0}, 
				     {0.949, 0.933, 0.757, 1.0}, 
				     {0.941, 0.871, 0.608, 1.0}, 
				     {0.878, 0.725, 0.486, 1.0}, 
				     {0.816, 0.616, 0.443, 1.0}, 
				     {0.776, 0.529, 0.341, 1.0}, 
				     {0.824, 0.863, 0.824, 1.0}, 
				     {0.682, 0.573, 0.369, 1.0}, 
				     {0.275, 0.275, 0.275, 1.0}, 
				     {0.761, 0.839, 0.847, 1.0}, 
				     {0.761, 0.839, 0.847, 1.0}, 
				     {0.439, 0.271, 0.420, 1.0}, 
				     {0.975, 0.982, 0.484, 1.0}, 
				     {1.000, 0.000, 0.000, 1.0} };

const int MapMaker::elev_height[ELEV_LEVELS] = {-1, 100, 200, 500, 1000, 1500,
						2000, 3000};

MapMaker::MapMaker( char *fg_root, char *ap_filter, int features,
		    int size, int scale ) {
  this->fg_root = fg_root;
  arp_filter = ap_filter;
  this->features = features;
  this->size = size;
  scle = scale;

  modified = false;

  setLight( -1, -1, 2 );       // default lighting
}

int MapMaker::createMap(GfxOutput *output,float theta, float alpha, 
			bool do_square) {
  this->output = output;
  
  modified = false;
  output->clear( rgb[0] );

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
  int min_theta = (int)( (theta - dtheta) * SG_RADIANS_TO_DEGREES ) - sgntheta;
  int max_theta = (int)( (theta + dtheta) * SG_RADIANS_TO_DEGREES ) - sgntheta;
  int min_alpha = (int)( (alpha - dalpha) * SG_RADIANS_TO_DEGREES ) - sgnalpha;
  int max_alpha = (int)( (alpha + dalpha) * SG_RADIANS_TO_DEGREES ) - sgnalpha;

  // Quick & dirty fix from Rainer Emrich (thanks!)
  if(max_alpha == 1) min_alpha = -1;
  if(min_alpha == -2) max_alpha = 0;
  if(max_theta == 1) min_theta = -1;
  if(min_theta == -2) max_theta = 0;
  
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
      process_directory( subdir, slen, k, l, xyz );
    }
  }

  delete[] subdir;

  if (modified) {
    Overlays overlays( fg_root, zoom, size );
    overlays.setOutput( output );
    overlays.setAirportColor( rgb[ARP_LABEL], rgb[9] );
    overlays.setNavaidColor( rgb[ARP_LABEL] );
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
      float levels[ELEV_LEVELS][4];
      sgVec3 normal[ELEV_LEVELS][2];

      for (int k = 0; k < ELEV_LEVELS; k++) { 
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

	      output->setColor(rgb[ecol]);
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
      output->setColor(rgb[col]);
    } else {
      int dcol;
      if (col>=0) {
	dcol = col;
      } else {
	dcol = elev2colour((int)((t[0][2] + t[1][2] + t[2][2]) / 3.0f));
      }
      output->setColor(rgb[dcol]);
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

  char lbuffer[4096], mtl[32];       /* will there be longer lines in
					scenery files? */

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
    // fprintf( stderr, "not a scenery file, line = '%s'\n", lbuffer);
    return 0;
  }

  modified = true;

  while ( (gzgets(tf, lbuffer, 4096) != Z_NULL) ) {      
    if ( sscanf(lbuffer, "# gbs %f %f %f %f", 
		&tmp[0], &tmp[1], &tmp[2], &cr) == 4 ) {
      sgCopyVec3( gbs, tmp );
    } else if (sscanf(lbuffer, "v %f %f %f", 
		      &tmp[0], &tmp[1], &tmp[2]) == 3) {
      // Make a new vertice and translate into map coordinate system
      float *nv = new sgVec3;
      sgAddVec3(tmp, gbs);
      double pr = sgLengthVec3( tmp );
      ab_xy( tmp, xyz, nv );
      // nv[2] contains the sea-level z-coordinate - calculate this vertex'
      // altitude:
      nv[2] = pr - nv[2];
      v.push_back( nv );
      verts++;
    } else if (sscanf(lbuffer, "vn %f %f %f", 
		      &tmp[0], &tmp[1], &tmp[2]) == 3) {
      // Make a new normal
      float *nn = new sgVec3;
      sgCopyVec3( nn, tmp );
      n.push_back( nn );
      normals++;
    } else if (strncmp(lbuffer, "tf ", 3) == 0) {
      // Triangle fan
      list<int> vertex_indices;
      int c = 3;
      while ( lbuffer[c] != 0 ) {
	while (lbuffer[c]==' ') c++;
	sscanf( lbuffer + c, "%d/%d", &i1, &i2 );

	if (i1 >= verts || i1 >= normals) {
	  fprintf(stderr, "Tile \"%s\" contains triangle indices out of " \
		  "bounds.\n", tile_name);
	  break;
	}

	vertex_indices.push_back( i1 );
	while (lbuffer[c] != ' ' && lbuffer[c] != 0) c++;
      }

      draw_trifan( vertex_indices, v, n, material );
      polys++;
    } else if (sscanf(lbuffer, "# usemtl %s", mtl) == 1) {
      int i;
      for (i = 0; i < 16 && strcmp(mtl, materials[i]) != 0; i++);
      if (i<16) {
	material = colours[i];
      } else {
	//printf("unknown material = %s\n", mtl);
	material = colours[2];
      }
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

