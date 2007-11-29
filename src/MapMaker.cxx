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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <zlib.h>
#ifndef _MSC_VER
#  include <unistd.h>
#endif
#include <sys/stat.h>
#include <plib/ul.h>

#include "MapMaker.hxx"
/*#include <simgear/magvar/magvar.hxx>*/

// Utility function that I needed to put somewhere - this probably isn't the best place for it.
// Appends a path separator to a directory path if not present.
// Calling function MUST ENSURE that there is space allocated for the potential strcat.
void NormalisePath(char* dpath) {
  size_t dlen = strlen(dpath);
  #if defined( macintosh )
  if(dpath[dlen-1] != ':') {
    dpath[dlen] = ':';
    dpath[dlen+1] = '\0';
  }
  #elseif (defined(WIN32) && !defined(__CYGWIN__))
  if(dpath[dlen-1] != '\\' && dpath[dlen-1] != '/') {
    dpath[dlen] = '\\';
    dpath[dlen+1] = '\0';
  }
  #else
  if(dpath[dlen-1] != '/') {
    //strcat(dpath, '/');
    dpath[dlen] = '/';
    dpath[dlen+1] = '\0';
  }
  #endif
}

const float MapMaker::simple_normals[][3] = {{0.0f, 0.0f, 1.0f},
					     {0.0f, 0.0f, 1.0f},
					     {0.0f, 0.0f, 1.0f},
					     {0.0f, 0.0f, 1.0f}};

MapMaker::MapMaker( char *fg_root, char *ap_filter, int features,
                    int size, int scale ) {
  this->fg_root = NULL;
  setFGRoot(fg_root);
  arp_filter = ap_filter;
  this->features = features;
  this->size = size;
  this->device_size = size;
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
  for (unsigned int i = 0; i < palette.size(); i++) {
    delete[] palette[i];
  }

  delete[] fg_root;
}

void MapMaker::setFGRoot( char *fg_root ) {
  delete this->fg_root;
  if (fg_root == NULL) {
    fg_root = getenv("FG_ROOT");
    if (fg_root == NULL) {
      fg_root = FGBASE_DIR;
    }
  }
   
  this->fg_root = new char[strlen(fg_root)+1];
  strcpy(this->fg_root, fg_root);
}

void MapMaker::setPalette( char* filename ) {
  if (palette_loaded) {
    for (unsigned int i = 0; i < palette.size(); i++) {
      delete palette[i];
    }
  }

  palette_loaded = false;

  read_materials(filename);
}

int MapMaker::createMap(GfxOutput *output,float theta, float alpha, 
                        string dirpath, float autoscale, bool atlas) {
  this->output = output;
  
  modified = false;

  if (!palette_loaded)
    read_materials();

  if (!palette_loaded) {
    fprintf(stderr, "No palette loaded.\n");
    return 0;
  }

  int nb_frag = size / device_size;
  if ( nb_frag * device_size < size )
    nb_frag += 1;
  nb_frag *= nb_frag;
  if ( nb_frag > 1 ) {
    printf("________________________________\r"); fflush( stdout );
  }

  int i = 0, j = 0;
  for (int x = 0; x < size; x += device_size) {
    for (int y = 0; y < size; y += device_size) {
      output->openFragment(x,y,device_size);
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
      
      // Draw a quad spanning the full area and the same as the clear color
      // to avoid corruption from the pixel read not reading areas we don't
      // explicitly draw to on some platforms.
      bool save_shade = output->getShade();
      output->setShade(false);
      output->setColor(palette[12]);
      sgVec2 baseQuad[4];
      sgSetVec2(baseQuad[0], 0.0, 0.0);
      sgSetVec2(baseQuad[1], 0.0, (float)size);
      sgSetVec2(baseQuad[2], (float)size, (float)size);
      sgSetVec2(baseQuad[3], (float)size, 0.0);
      output->drawQuad(&baseQuad[0], simple_normals);
      output->setShade(save_shade);

      // calculate which tiles we will have to load
      float dtheta, dalpha;

      if (autoscale > 0.0f) {
        // I think this is wrong (PL 010719):
        // set scale to autoscale degrees in meters at this latitude
        //scle = (int)(earth_radius_lat(theta)*2.0f * autoscale *SG_PI / 360.0f);

        // this should be better:
        scle = (int)(rec*2.0f * autoscale *SG_PI / 360.0f);

        // no idea why this magic should be here:
        //dtheta = autoscale * 0.8f * SG_DEGREES_TO_RADIANS;
        //dalpha = (autoscale * 0.8f + fabs(tan(theta)) * 0.35f) * SG_DEGREES_TO_RADIANS;
      } //else {
        lat_ab( scle/2, scle/2, theta, alpha, &dtheta, &dalpha );
        dtheta -= theta;
        dalpha -= alpha;
        //  }

//    int sgntheta = (theta < 0.0f) ? 1 : 0, sgnalpha = (alpha < 0.0f) ? 1 : 0;
//    // Rainer Emrich's improved code for finding map boundaries
//    int mid_theta = (int)( theta * SG_RADIANS_TO_DEGREES + 0.0001) - sgntheta;
//    int mid_alpha = (int)( alpha * SG_RADIANS_TO_DEGREES + 0.0001) - sgnalpha;
//    int int_dtheta = (int)( dtheta * SG_RADIANS_TO_DEGREES + 0.6f);
//    int int_dalpha = (int)( dalpha * SG_RADIANS_TO_DEGREES + 0.6f);
//    int max_theta = mid_theta + int_dtheta;
//    int min_theta = mid_theta - int_dtheta;
//    int max_alpha = mid_alpha + int_dalpha;
//    int min_alpha = mid_alpha - int_dalpha;
      int max_theta = (int)floor((theta + dtheta) * SG_RADIANS_TO_DEGREES);
      int min_theta = (int)floor((theta - dtheta) * SG_RADIANS_TO_DEGREES);
      int max_alpha = (int)floor((alpha + dalpha) * SG_RADIANS_TO_DEGREES);
      int min_alpha = (int)floor((alpha - dalpha) * SG_RADIANS_TO_DEGREES);
      if( max_theta > 90 ) max_alpha=90;
      if( min_theta < -90 ) min_alpha=-90;
      if( max_alpha > 360 ) max_alpha=360;
      if( min_alpha < -360 ) min_alpha=-360;
      
      zoom = (float)size / (float)scle;

      // UG! - process_directory modifies the passed char* and wants the result next time
      char *subdir = new char[strlen(fg_root) + 512];  // 512 should be *very* safe (too safe?)
      strcpy(subdir, dirpath.c_str());
      size_t slen = strlen(subdir);
      // Now we need to make sure that the supplied directory ends in a path separator.
      NormalisePath(subdir);
      
      slen = strlen(subdir);  // Need to update it since process_directory relys on it.
      
      //if (getVerbose())
        //printf("Map area: lat %c%d / %c%d, lon %c%d / %c%d.\n", 
        //   ns(min_theta), abs(min_theta), ns(max_theta), abs(max_theta), 
        //   ew(min_alpha), abs(min_alpha), ew(max_alpha), abs(max_alpha) );
     
      if (!atlas) {
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
      } else {
	  int k = (int)floor(theta * SG_RADIANS_TO_DEGREES);
	  int l = (int)floor(alpha * SG_RADIANS_TO_DEGREES);
	  process_directory(subdir, slen, k, l, xyz);
      }

      delete[] subdir;

      if (modified) {
        Overlays overlays( fg_root, zoom, size );
        overlays.setOutput( output );
        overlays.setAirportColor( palette[materials["RunwayOutline"]],
                                  palette[materials["RunwayFill"]] );
        overlays.setNavaidColor( palette[materials["NavaidLabels"]] );
        overlays.setVorToColor( palette[materials["VorToLine"]] );
        overlays.setVorFromColor( palette[materials["VorFromLine"]] );
        overlays.setIlsColor( palette[materials["IlsLoc"]] );
        overlays.setLocation( theta, alpha );

        int features = Overlays::OVERLAY_NAMES | Overlays::OVERLAY_IDS;
        if (getAirports()) features += Overlays::OVERLAY_AIRPORTS;
        if (getNavaids()) {
          features += Overlays::OVERLAY_NAVAIDS;
          if (getNavaidsVOR()) features += Overlays::OVERLAY_NAVAIDS_VOR;
          if (getNavaidsNDB()) features += Overlays::OVERLAY_NAVAIDS_NDB;
          if (getNavaidsFIX()) features += Overlays::OVERLAY_NAVAIDS_FIX;
          if (getNavaidsILS()) features += Overlays::OVERLAY_NAVAIDS_ILS;
        }
        overlays.setFeatures(features);

        overlays.drawOverlays();
      } else if (getVerbose()) {
        printf("Nothing to draw - no output written.\n");
      }

      output->closeFragment();
      ++i;
      if ( nb_frag > 1 ) {
        while ( j < i * 32 / nb_frag ) {
          printf(".");
          j++;
        }
	fflush( stdout );
      }
    }
  }
  printf("\r");
  return 1;
}

// A helper function for draw_elevation_tri.  Draws a figure (which
// may be a triangle, quadrilateral, or pentagon).  The number of
// vertices and normals is given by 'vertices'.  The 'ts' array gives
// the vertices, 'nrms' the normals, and 'ps' the scaled map
// coordinates.
//
// We can assume that the vertices are given from top to bottom, and
// that they move around the perimeter of the figure in the correct
// order.  As well, we only need to guarantee that the last two
// points, which form the bottom of the figure, remain unchanged - the
// calling function doesn't care about the "upper" points.  (This is a
// *very* specialized routine).
void MapMaker::draw_elevation_slice(int vertices, bool smooth, int k,
				    sgVec3 *ts, sgVec3 *nrms, sgVec2 *ps)
{
    sgVec4 color[5];

    if (smooth) {
	for (int i = 0; i < vertices; i++) {
	    elev2colour_smooth((int)ts[i][2], color[i]);
	}
    }

    // Draw the figure.
    if (vertices == 3) {
	// Triangle
	if (smooth) {
	    output->drawTriangle(ps, nrms, color);
	} else {
	    output->setColor(palette[elev_colindex[k]]);
	    output->drawTriangle(ps, nrms);
	}
    } else if (vertices == 4) {
	// Quadrilateral.
	if (smooth) {
	    output->drawQuad(ps, nrms, color);
	} else {
	    output->setColor(palette[elev_colindex[k]]);
	    output->drawQuad(ps, nrms);
	}
    } else {
	// Pentagon.  Draw it as a quadrilateral and a triangle.  The
	// quadralateral consists of the first 4 points, and the
	// triangle consists of the remaining two *and* the first one.
	// To draw the triangle, then, we copy the last two points
	// over the second and third points (this is okay, because
	// they won't be used again), and draw a triangle consisting
	// of the first 3 points.
	if (smooth) {
	    output->drawQuad(ps, nrms, color);
	} else {
	    output->setColor(palette[elev_colindex[k]]);
	    output->drawQuad(ps, nrms);
	}

	sgCopyVec3(ts[1], ts[3]);
	sgCopyVec3(nrms[1], nrms[3]);
	sgCopyVec2(ps[1], ps[3]);

	sgCopyVec3(ts[2], ts[4]);
	sgCopyVec3(nrms[2], nrms[4]);
	sgCopyVec2(ps[2], ps[4]);

	if (smooth) {
	    sgCopyVec4(color[1], color[3]);
	    sgCopyVec4(color[2], color[4]);
	    output->drawTriangle(ps, nrms, color);
	} else {
	    output->drawTriangle(ps, nrms);
	}
    }
}

// A helper function for draw_elevation_tri.  Given an upper and a
// lower vertex defining an edge, and an elevation at which to slice
// the edge, creates a new point at that slice.  The new point is
// added to the ts, nrms, and ps arrays at the index 'dest'.
//
// EYE - Why pass 'dest' - I should really just pass the vertex,
// normal, and point to be set.
void MapMaker::create_sub_point(float *topVert, float *bottomVert, 
				float *topNorm, float *bottomNorm, 
				int dest, double elevation,
				sgVec3 *ts, sgVec3 *nrms, sgVec2 *ps)
{
    sgVec3 newPoint, newNorm;
    double scaling = 
	(elevation - bottomVert[2]) / (topVert[2] - bottomVert[2]);
    assert((scaling <= 1.0) && (0.0 <= scaling));

    sgSubVec3(newPoint, topVert, bottomVert);
    sgScaleVec3(newPoint, scaling);
    sgAddVec3(newPoint, bottomVert);
    sgSubVec3(newNorm, topNorm, bottomNorm);
    sgScaleVec3(newNorm, scaling);
    sgAddVec3(newNorm, bottomNorm);

    sgCopyVec3(ts[dest], newPoint);
    sgCopyVec3(nrms[dest], newNorm);
    sgSetVec2(ps[dest], 
	      scale(newPoint[0], size, zoom),
	      scale(newPoint[1], size, zoom));
}

// Draws the given triangle, coloured according to its elevation.  If
// the triangle spans more than one elevation level, and so needs more
// than one colour, it is sliced and diced, so that each part occupies
// only one level.  The triangle slices are temporary - no changes are
// made to the v and n vectors.
void MapMaker::draw_elevation_tri(int vert0, int vert1, int vert2,
				  int norm0, int norm1, int norm2,
				  vector<float*> &v, vector<float*> &n, 
				  int col)
{
    bool smooth = features & DO_SMOOTH_COLOR;
    sgVec3 t[3], nrm[3];
    sgVec2 p[3];
    int index[3];

    sgCopyVec3(t[0], v[vert0]);
    sgCopyVec3(t[1], v[vert1]);
    sgCopyVec3(t[2], v[vert2]);
    sgCopyVec3(nrm[0], n[norm0]);
    sgCopyVec3(nrm[1], n[norm1]);
    sgCopyVec3(nrm[2], n[norm2]);
    index[0] = elev2index(t[0][2]);
    index[1] = elev2index(t[1][2]);
    index[2] = elev2index(t[2][2]);

    // Triangle lies within one elevation level.  Draw it in one
    // colour.
    if ((index[0] == index[1]) && (index[1] == index[2])) {
	sgSetVec2(p[0], scale(t[0][0], size, zoom), scale(t[0][1], size, zoom));
	sgSetVec2(p[1], scale(t[1][0], size, zoom), scale(t[1][1], size, zoom));
	sgSetVec2(p[2], scale(t[2][0], size, zoom), scale(t[2][1], size, zoom));
	if (smooth) {
	    sgVec4 color[3];
	    elev2colour_smooth((int)t[0][2], color[0]);
	    elev2colour_smooth((int)t[1][2], color[1]);
	    elev2colour_smooth((int)t[2][2], color[2]);

	    output->drawTriangle( p, nrm, color );
	} else {
	    output->setColor(palette[elev2colour(v[vert0][2])]);
	    output->drawTriangle( p, nrm );
	}

	return;
    }

    // Triangle spans more than one level.  Drats.  Do a quick sort on
    // the vertices, so that vert0 points to the top vertex, vert1,
    // the middle, and vert2 the bottom.
    if (index[0] < index[1]) {
	swap(vert0, vert1);
	swap(norm0, norm1);
	swap(index[0], index[1]);
    }
    if (index[0] < index[2]) {
	swap(vert0, vert2);
	swap(norm0, norm2);
	swap(index[0], index[2]);
    }
    if (index[1] < index[2]) {
	swap(vert1, vert2);
	swap(norm1, norm2);
	swap(index[1], index[2]);
    }

    // Now begin slicing the lines leading away from vert0 to vert1
    // and vert2.  Slicing a triangle create new triangles, new
    // quadrilaterals, and even new pentagons.  Because the triangle
    // lies in a plane (by definition), we are assured that the new
    // figures are also planar.
    //
    // After each bit is sliced off the top, it is drawn and then
    // discarded.  The process is then repeated on the remaining
    // figure, until there's nothing left.
    //
    // This can be illustrated with the power of ASCII graphics.  If
    // we have to make two cuts of the triangle ABC, we'll create 4
    // new points, D, E, F, and G.  This creates 3 figures: ADE,
    // EDBFG, and GFC.
    //
    //        A	       A	  	 
    //       /|	      /|	  	 
    //      / |	     / |	  	 
    //  -->/  |	    D--E     D--E      D--E
    //    /   |	   /   |    /   |     /   |
    //   B    |	  B    |   B    |    B    |
    //    \   |	   \   |    \   |     \   |
    //     \  |	    \  |     \  |      \  |
    //   -->\ |	  -->\ |   -->\ |       F-G       F-G
    //       \|	      \|       \|        \|        \|
    //        C	       C        C         C         C

    int k, vertices;
    sgVec3 ts[5], nrms[5];
    sgVec2 ps[5];

    // Slicing creates new vertices and normals, so we need to keep
    // track of actual points, not just their indices as before.  The
    // array 'ts' keeps the vertices of the current figure, 'nrms' the
    // norms, and 'ps' the scaled points.  At most we can generate a
    // pentagon, so each array has 5 points.  The current number of
    // points is given by 'vertices'.
    //
    // In the example above, the arrays will contain data for ADE,
    // then DEBFG, and finally GFC.  Note that points are given in a
    // counter-clockwise direction (as illustrated here).  This means
    // that that when a bottom line (eg, DE in ADE) becomes a top line
    // (ED in EDBFG), we reverse its order.
    sgCopyVec3(ts[0], v[vert0]);
    sgCopyVec3(nrms[0], n[norm0]);
    sgSetVec2(ps[0], 
	      scale(ts[0][0], size, zoom), 
	      scale(ts[0][1], size, zoom));
    vertices = 1;

    for (k = index[0]; k > index[1]; k--) {
	// Make a cut and draw the resulting figure.
	double elevation = elev_height[k - 1]; // This is where we cut.

	// Cut along the short line (vert0 to vert1), and put the
	// resulting vertex, normal, and point into ts, nrms, and ps.
	create_sub_point(v[vert0], v[vert1], n[norm0], n[norm1], 
			 vertices++, elevation, ts, nrms, ps);
	// Ditto for the long line (vert0 to vert2).
	create_sub_point(v[vert0], v[vert2], n[norm0], n[norm2], 
			 vertices++, elevation, ts, nrms, ps);

	// Now draw the resulting figure.
	draw_elevation_slice(vertices, smooth, k, ts, nrms, ps);

	// We're ready to move down and make the next slice.  The two
	// points we just created will now be the top of the next
	// slice.  We need to reverse the order of the points.
	sgCopyVec3(ts[0], ts[vertices - 1]);
	sgCopyVec3(nrms[0], nrms[vertices - 1]);
	sgCopyVec2(ps[0], ps[vertices - 1]);

	sgCopyVec3(ts[1], ts[vertices - 2]);
	sgCopyVec3(nrms[1], nrms[vertices - 2]);
	sgCopyVec2(ps[1], ps[vertices - 2]);

	vertices = 2;
    }

    // Add the middle vertex.
    sgCopyVec3(ts[vertices], v[vert1]);
    sgCopyVec3(nrms[vertices], n[norm1]);
    sgSetVec2(ps[vertices], 
	      scale(ts[vertices][0], size, zoom), 
	      scale(ts[vertices][1], size, zoom));
    vertices++;
    assert(vertices <= 5);

    for (; k > index[2]; k--) {
	// Make a cut and draw the resulting figure.
	double elevation = elev_height[k - 1]; // This is where we cut.

	// Get the point along the short line.
	create_sub_point(v[vert1], v[vert2], n[norm1], n[norm2], 
			 vertices++, elevation, ts, nrms, ps);
	// Get the point along the long line.
	create_sub_point(v[vert0], v[vert2], n[norm0], n[norm2], 
			 vertices++, elevation, ts, nrms, ps);

	draw_elevation_slice(vertices, smooth, k, ts, nrms, ps);

	// The bottom will be the next top.
	sgCopyVec3(ts[0], ts[vertices - 1]);
	sgCopyVec3(nrms[0], nrms[vertices - 1]);
	sgCopyVec2(ps[0], ps[vertices - 1]);

	sgCopyVec3(ts[1], ts[vertices - 2]);
	sgCopyVec3(nrms[1], nrms[vertices - 2]);
	sgCopyVec2(ps[1], ps[vertices - 2]);
	
	vertices = 2;
    }

    // Add the final vertex and draw the last figure.
    sgCopyVec3(ts[vertices], v[vert2]);
    sgCopyVec3(nrms[vertices], n[norm2]);
    sgSetVec2(ps[vertices], 
	      scale(ts[vertices][0], size, zoom), 
	      scale(ts[vertices][1], size, zoom));
    vertices++;
    assert(vertices <= 5);

    draw_elevation_slice(vertices, smooth, k, ts, nrms, ps);
}

// Draws a single triangle defined by the given vertices and normals
// (which are indexes into the v and n vectors).  If the triangle
// defines an "elevation triangle" (a triangle that should be coloured
// according to its elevation), then it's passed on to
// draw_elevation_tri.
void MapMaker::draw_a_tri(int vert0, int vert1, int vert2,
			  int norm0, int norm1, int norm2,
			  vector<float*> &v, vector<float*> &n, int col)
{
    bool save_shade = output->getShade();
    
    // Elevation triangles get special treatment.
    if (col == -1) {
	draw_elevation_tri(vert0, vert1, vert2, norm0, norm1, norm2, v, n, col);
	return;
    }

    // Non-elevation triangles are coloured according to col.  They
    // are shaded (usually), but not smoothed.
    output->setColor(palette[col]);
    if (col == 12 || col == 13) {
	// do not shade ocean/lake/etc.
	// DCL - and switch lighting off for water for now (see the Jan 2005 mailing list archives)
	output->setShade(false);
    }

    sgVec3 nrm[3];
    sgVec2 p[3];
    
    sgCopyVec3(nrm[0], n[norm0]);
    sgCopyVec3(nrm[1], n[norm1]);
    sgCopyVec3(nrm[2], n[norm2]);
    sgSetVec2(p[0], 
	      scale(v[vert0][0], size, zoom), 
	      scale(v[vert0][1], size, zoom));
    sgSetVec2(p[1], 
	      scale(v[vert1][0], size, zoom), 
	      scale(v[vert1][1], size, zoom));
    sgSetVec2(p[2], 
	      scale(v[vert2][0], size, zoom), 
	      scale(v[vert2][1], size, zoom));

    output->drawTriangle(p, nrm);
    
    // DCL - restore the original lighting in case we turned it off for water
    output->setShade(save_shade);
}

// Really should be draw_tris
void MapMaker::draw_tri(const int_list &vertex_indices, 
			const int_list &normal_indices, 
			vector<float*> &v, vector<float*> &n, int col) 
{
    int i;
    int vert0, vert1, vert2;
    int norm0, norm1, norm2;

    // EYE - can we assume indices.size() is divisible by 3?
    assert((vertex_indices.size() % 3) == 0);
    for (i = 0; i < vertex_indices.size(); i += 3) {
	vert0 = vertex_indices[i];
	norm0 = normal_indices[i];
	vert1 = vertex_indices[i + 1];
	norm1 = normal_indices[i + 1];
	vert2 = vertex_indices[i + 2];
	norm2 = normal_indices[i + 2];

	draw_a_tri(vert0, vert1, vert2, norm0, norm1, norm2, v, n, col);
    }
}

void MapMaker::draw_trifan(const int_list &vertex_indices, 
			   const int_list &normal_indices, 
			   vector<float*> &v, vector<float*> &n, int col ) 
{
    int i;
    int cvert, vert1, vert2;
    int cnorm, norm1, norm2;

    cvert = vertex_indices[0];
    cnorm = normal_indices[0];
    vert1 = vertex_indices[1];
    norm1 = normal_indices[1];
    for (i = 2; i < vertex_indices.size(); i++) {
	vert2 = vertex_indices[i];
	norm2 = normal_indices[i];

	draw_a_tri(cvert, vert1, vert2, cnorm, norm1, norm2, v, n, col);

	vert1 = vert2;
	norm1 = norm2;
    }
}
			    
void MapMaker::draw_tristrip(const int_list &vertex_indices, 
			     const int_list &normal_indices, 
			     vector<float*> &v, vector<float*> &n, 
			     int col) 
{
    int i;
    int vert0, vert1, vert2;
    int norm0, norm1, norm2;

    vert0 = vertex_indices[0];
    norm0 = normal_indices[0];
    vert1 = vertex_indices[1];
    norm1 = normal_indices[1];
    for (i = 2; i < vertex_indices.size(); i++) {
	vert2 = vertex_indices[i];
	norm2 = normal_indices[i];

	draw_a_tri(vert0, vert1, vert2, norm0, norm1, norm2, v, n, col);

	vert1 = vert0;
	norm1 = norm0;
	vert0 = vert2;
	norm0 = norm2;
    }
}

int MapMaker::process_binary_file( char *tile_name, sgVec3 xyz ) {
  sgVec3 gbs, tmp;
//   int material = 16;
  int material;			// EYE - a colour, not a material
  unsigned int i;
  SGBinObject tile;

  vector<float*> v, n;

  if ( !tile.read_bin( tile_name ) ) {
    return 0;
  }

  modified = true;

  /* ugly conversion of GBS as Point3D to sgVec3 (why doesn't SimGear use
     SG from PLIB?) */
  Point3D gbs_p = tile.get_gbs_center();
  for (i = 0; i < 3; i++) {
    gbs[i] = gbs_p[i];
  }

  // Although the method is called get_wgs84_nodes, it doesn't
  // actually return WGS84 points.  It returns points in Cartesian
  // coordinate space, with the origin at the center of the earth, the
  // X axis going through 0 degrees latitude, 0 degrees longitude
  // (near Africa), the Y axis going through 0 degrees latitude, 90
  // degrees west latitude (in the Indian Ocean), and the Z axis going
  // through the north pole.  Units are metres.  See
  //
  // http://www.flightgear.org/Docs/Scenery/CoordinateSystem/CoordinateSystem.html
  //
  // for more.
  const point_list wgs84_nodes = tile.get_wgs84_nodes();
  for ( point_list::const_iterator node = wgs84_nodes . begin(); 
	node != wgs84_nodes . end();
	node++ ) {
    float *nv = new sgVec3;

    for (i = 0; i < 3; i++) {
      tmp[i] = (*node)[i];
    }
    
    // Convert <X, Y, Z> to WGS84 <lon, lat, elevation>.
    sgAddVec3(tmp, gbs);
    double pr = sgLengthVec3( tmp );
    ab_xy( tmp, xyz, nv );
    // nv[2] contains the sea-level z-coordinate - calculate this vertex'
    // altitude:
    nv[2] = pr - nv[2];
    v.push_back( nv );
  }

  // same as above for normals
  const point_list m_norms = tile.get_normals();
  for ( point_list::const_iterator normal = m_norms.begin(); 
	normal != m_norms.end();
	normal++ ) {
    // Make a new normal
    float *nn = new sgVec3;
    
    // BJS - One would think the normals should be transformed as
    // well, but this appears not to be the case.  What seems to be
    // happening is that the lighting is set up for the original,
    // untransformed vertices (see createMap and the call to
    // output->setLightVector).  That being the case, the lighting
    // will work with the original, untransformed normals.  I think.
    // The results certainly look okay.
    //
    // Assuming that my guess is correct, I think this approach could
    // fail if a very large map (covering a significant part of the
    // globe) was rendered.  Lighting that would be correct for the
    // left edge of the map might be completely wrong for the right
    // edge.  To be truly correct, the normals should really be
    // transformed, and the lighting set for the transformed map.
    // However, this situation is highly unlikely to occur, and so the
    // current system is good enough (and simpler and faster).
    for (i = 0; i < 3; i++) {
      nn[i] = (*normal)[i];
    }

    n.push_back( nn );
  }

  const group_list tris = tile.get_tris_v();
  string_list tri_mats = tile.get_tri_materials();
  const group_list tri_normals = tile.get_tris_n();
  const group_list fans = tile.get_fans_v();
  string_list fan_mats = tile.get_fan_materials();
  const group_list fans_normals = tile.get_fans_n();
  const group_list strips = tile.get_strips_v();
  string_list strip_mats = tile.get_strip_materials();
  const group_list strips_normals = tile.get_strips_n();
  
  // tris
  i = 0;
  for ( group_list::const_iterator tri = tris.begin(); 
	tri != tris.end(); 
	tri++) {

    StrMap::const_iterator mat_it = materials.find( tri_mats[i] );
    if ( mat_it == materials.end() ) {
      if (getVerbose()) {
	fprintf( stderr, "Warning: unknown material \"%s\" encountered.\n", 
		 tri_mats[i].c_str() );
      }
      material = -1;
    } else {
      material = (*mat_it).second;
    }

    if (tri_normals[i].size() > 0) {
	draw_tri(*tri, tri_normals[i], v, n, material);
    } else {
	draw_tri(*tri, *tri, v, n, material);
    }

    i++;
  }

  // fans
  i = 0;
  for ( group_list::const_iterator fan = fans.begin(); 
	fan != fans.end(); 
	fan++) {

    StrMap::const_iterator mat_it = materials.find( fan_mats[i] );
    if ( mat_it == materials.end() ) {
      if (getVerbose()) {
	fprintf( stderr, "Warning: unknown material \"%s\" encountered.\n", 
		 fan_mats[i].c_str() );
      }
      material = -1;
    } else {
      material = (*mat_it).second;
    }

    if (fans_normals[i].size() > 0) {
	draw_trifan(*fan, fans_normals[i], v, n, material);
    } else {
	draw_trifan(*fan, *fan, v, n, material);
    }

    i++;
  }
	
  // strips
  i = 0;
  for ( group_list::const_iterator strip = strips.begin(); 
	strip != strips.end(); 
	strip++) {

    StrMap::const_iterator mat_it = materials.find( strip_mats[i] );
    if ( mat_it == materials.end() ) {
      if (getVerbose()) {
	fprintf( stderr, "Warning: unknown material \"%s\" encountered.\n", 
		 strip_mats[i].c_str() );
      }
      material = -1;
    } else {
      material = (*mat_it).second;
    }

    if (strips_normals[i].size() > 0) {
	draw_tristrip( *strip, strips_normals[i], v, n, material );
    } else {
	draw_tristrip( *strip, *strip, v, n, material );
    }

    i++;
  }

	
  if(0) {
    cout << "Node_list sizes are nodes: " << wgs84_nodes.size() << " -- normals: " << m_norms.size() << '\n'; 
    cout << "Group_list sizes are tris: " << tris.size() << " -- fans: " << fans.size() << " -- strips: " << strips.size() << '\n';
  } 

  for (i = 0; i < v.size(); i++) {
    delete[] v[i];
  }
  for (i = 0; i < n.size(); i++) {
    delete[] n[i];
  }
  
  return 1;
}

int MapMaker::process_ascii_file( char *tile_name, sgVec3 xyz ) {
  //float cr;               // reference point (gbs)
  sgVec3 gbs, tmp;
  //int scount = 0;
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
         int_list vertex_indices;

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
	     draw_trifan(vertex_indices, vertex_indices, v, n, material);
          polys++;
         }
      }
    }
    break;

    case 'f': {
      if (strncmp(lbuffer, "f ", 2) == 0) {
         // Triangle
         int_list vertex_indices;

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
          draw_trifan(vertex_indices, vertex_indices, v, n, material);
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

//   printf("process_ascii_file: %s\n", tile_name);
  return 1;
}


// path must be to the base of the 10x10/1x1 tile tree - more will be appended.
// plen is path length
int MapMaker::process_directory( char *path, size_t plen, int lat, int lon, 
                                 sgVec3 xyz ) {
  int sgnk = (lat < 0) ? 1 : 0, sgnl = (lon < 0) ? 1 : 0;
  
  //printf("process_directory: path = %s, lat = %i, lon = %i\n", path, lat, lon);

  int llen = sprintf( path + plen, "%c%03d%c%02d/%c%03d%c%02d", 
                      ew(lon), abs((lon+sgnl) / 10 * 10) + sgnl*10, 
                      ns(lat), abs((lat+sgnk) / 10 * 10) + sgnk*10,
                      ew(lon), abs(lon), ns(lat), abs(lat) );

  ulDir *dir;
  ulDirEnt *ent;

  if (getVerbose()) 
    printf("%s:  ", path + plen);

  if ((dir = ulOpenDir(path)) == NULL) {
    if (getVerbose()) printf("\n");
    return 0;
  }

  path[plen + llen] = '/';  
  while ((ent = ulReadDir(dir)) != NULL) {
    strcpy( path + plen + llen + 1, ent -> d_name );

    /* we now have to check if this is a regular file -- I suspect this isn't
       portable to non-UNIX systems... */
    struct stat stat_buf;
    stat( path, &stat_buf );
    if ( !(stat_buf.st_mode & S_IFREG) )
      continue;  

    if (getVerbose()) {
      putc( '.', stdout );
      fflush(stdout);
    }

    /* first try to load the tile as a binary file -- if this fails, we
       try it as an ascii file */
    if ( !process_binary_file( path, xyz ) ) {
      if ( !process_ascii_file( path, xyz ) ) {
	if ( getVerbose() )
	  fprintf( stderr, "Tile \"%s\" is of unknown format.\n", path );
      }
    }
  }

  if (getVerbose()) putc('\n', stdout);
  ulCloseDir(dir);

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

  string material;
  char line[256], *token, delimiters[] = " \t";
  unsigned int index;
  int elev_level = 0;
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
      material = token;

      token = strtok(NULL, delimiters); // index
      index = atoi(token);

      materials[material] = index;

      int height;
      if ( sscanf(material.c_str(), "Elevation_%d", &height) == 1 ) {
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

    case '\n':
    case '\r':
      // Blank line - ignore
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
    delete[] filename;

  palette_loaded = true;
}
