/*-------------------------------------------------------------------------
  Program for creating maps out of Flight Gear scenery data

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
  2000-02-20        Included some compatibility changes submitted by
                    Christian Mayer.
  2000-02-26        Major code reorganisation, made class MapMaker, etc.
  2000-03-06        Following Norman Vine's advice, map now uses OpenGL
                    for output
  2000-04-29        New, cuter, airports
---------------------------------------------------------------------------*/

#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "MapMaker.hxx"
#include "OutputGL.hxx"

float clat = -100.0f, clon = -100.0f;   // initialize to unreasonable values
char *outp = "map.png";                 // output file name
bool autoscale = false, global = false, doublebuffer = true;
MapMaker map;

char outname[512], *scenerypath;
int opathl, spathl;
DIR *dir1, *dir2 = NULL;
dirent *ent;


/*****************************************************************************/
void reshapeMap( int width, int height ) {
  glViewport( 0, 0, width, height );
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity();
  glOrtho( 0.0f, (GLfloat)width, (GLfloat)height, 0.0f, -1.0f, 1.0f );
  glMatrixMode( GL_MODELVIEW );
  glLoadIdentity();
}

void redrawMap() {
  char title_buffer[256];

  if (!global) {
    sprintf(title_buffer, "%c%.1f %c%.1f",
	    (clat<0.0f)?'S':'N', clat * 180.0f / M_PI,
	    (clon<0.0f)?'W':'E', clon * 180.0f / M_PI);
    glutSetWindowTitle(title_buffer);

    OutputGL output( outp, map.getSize() );
    map.createMap( &output, clat, clon, autoscale );
    output.closeOutput();
    exit(0);
  } else {
    char ns, ew;
    int lat, lon;
    int s = map.getSize();

    do {
      while (dir2 == NULL) {
	do {
	  ent = readdir(dir1);
	} while (ent != NULL && ent->d_name[0] == '.');
	if (ent != NULL) {
	  strcpy( scenerypath+spathl, ent->d_name );
	  dir2 = opendir(scenerypath);
	} else {
	  closedir(dir1);
	  delete scenerypath;
	  exit(0); // done reading top directory
	}
      }
      
      bool exit = false;
      do {
	ent = readdir(dir2);

	if (ent != NULL) {
	  exit = (ent->d_name[0] != '.' &&
		  sscanf(ent->d_name, "%c%d%c%d",
			 &ew, &lon, &ns, &lat) == 4);
	} else {
	  exit = true;
	}
      } while (!exit);

      if (ent == NULL) {
	closedir(dir2);
	dir2 = NULL;
      } else {
	// we have found a scenery directory - let's check if we already
	// have an image for it!
	lat *= (ns=='n')?1:-1;
	lon *= (ew=='e')?1:-1;
	clat = ((float)lat + 0.5f) * M_PI / 180.0f;
	clon = ((float)lon + 0.5f) * M_PI / 180.0f;
	
	sprintf( outname+opathl, "/%c%03d%c%02d.png", 
		 (lon<0)?'w':'e', abs(lon), 
		 (lat<0)?'s':'n', abs(lat) );

	struct stat filestat;
	if ( stat(outname, &filestat) == 0 ) {
	  // the file already exists, so skip it!
	  ent = NULL;
	}
	  
      }

    } while (ent == NULL);

   
    sprintf(title_buffer, "%c%.1f %c%.1f",
	    (clat<0.0f)?'S':'N', clat * 180.0f / M_PI,
	    (clon<0.0f)?'W':'E', clon * 180.0f / M_PI);
    glutSetWindowTitle(title_buffer);

    OutputGL output(outname, s);
    map.createMap( &output, clat, clon, true );
    output.closeOutput();
    if (doublebuffer) {
      glutSwapBuffers();
    }

    glutPostRedisplay();
  }
}

/*****************************************************************************/

void print_help() {
  printf("MAP - FlightGear mapping utility\n\n");
  printf("Usage:\n");
  printf("  --lat=xx.xx             Start at latitude xx.xx (deg., south is neg.)\n");
  printf("  --lon=xx.xx             Start at longitude xx.xx (deg., west is neg.)\n");
  printf("  --size=pixels           Create map of size pixels*pixels (default 512)\n");
  printf("  --scale=x               Kilometers from top to bottom of map (default 100)\n"); 
  printf("  --autoscale             Automatically set scale to 1x1 degree tile\n");
  printf("  --light=x, y, z         Set light vector for shading\n");
  printf("  --airport-filter=string Display only airports with id beginning 'string'\n");
  printf("  --output=name           Write output to given file name (default 'map.png')\n");
  printf("  --fgroot=path           Overrides FG_ROOT environment variable\n");
  printf("  --disable-airports      Don't show airports\n");
  printf("  --disable-navaids       Don't show navaids\n");
  printf("  --disable-shading       Don't do nice shading of the terrain\n");
  printf("  --atlas=path            Create maps of all scenery, and store them in path\n");
  printf("  --verbose               Display information during processing\n");
  printf("  --singlebuffer          Use single buffered display.\n\n");
}

int main( int argc, char **argv ) {
  // variables for command line parsing
  int param, features = map.getFeatures();
  float x, y, z;
  char cparam[128];

  if (argc == 0)
    print_help();

  // process command line arguments
  for (int arg = 1; arg < argc; arg++) {
    if ( sscanf(argv[arg], "--lat=%f", &clat) == 1) {
      // do nothing
    } else if ( sscanf(argv[arg], "--lon=%f", &clon) == 1 ) {
      // do nothing
    } else if ( sscanf(argv[arg], "--size=%d", &param) == 1 ) {
      map.setSize( param );
    } else if ( sscanf(argv[arg], "--scale=%d", &param) == 1 ) {
      map.setScale( param * 1000 );
    } else if ( sscanf(argv[arg], "--light=%f, %f, %f", &x, &y, &z) == 3 ) {
      map.setLight( x, y, z );
    } else if ( sscanf(argv[arg], "--airport-filter=%s", cparam) == 1 ) {
      map.setAPFilter( strdup(cparam) );
    } else if ( sscanf(argv[arg], "--output=%s", cparam) == 1 ) {
      outp = strdup(cparam);
    } else if ( sscanf(argv[arg], "--fgroot=%s", cparam) == 1 ) {
      map.setFGRoot( strdup(cparam) );
    } else if ( strcmp(argv[arg], "--disable-airports" ) == 0 ) {
      features &= ~MapMaker::DO_AIRPORTS;
    } else if ( strcmp(argv[arg], "--disable-navaids" ) == 0 ) {
      features &= ~MapMaker::DO_NAVAIDS;
    } else if ( strcmp(argv[arg], "--disable-shading" ) == 0 ) {
      features &= ~MapMaker::DO_SHADE;
    } else if ( strcmp(argv[arg], "--autoscale") == 0 ) {
      autoscale = true;
    } else if ( strcmp(argv[arg], "--singlebuffer") == 0 ) {
      doublebuffer = false;
    } else if ( sscanf(argv[arg], "--atlas=%s", cparam) == 1 ) {
      global = true;
      outp = strdup( cparam );
    } else if ( strcmp(argv[arg], "--verbose") == 0 ) {
      features |= MapMaker::DO_VERBOSE;
    } else if ( strcmp(argv[arg], "--help") == 0 ) {
      print_help();
      exit(0);
    } else {
      fprintf(stderr, "%s: unknown argument '%s'.\n", argv[0], argv[arg]);
      print_help();
      exit(1);
    }

  }
  
  if (!global && (fabs(clat) > 90.0f || fabs(clon) > 180.0f)) {
    fprintf(stderr, 
	    "%s: Invalid position. Check latitude and longitude.\n", argv[0]);
    print_help();
    exit(1);
  }

  if (autoscale) {
    clat = ((int)clat) + 0.5f;
    clon = ((int)clon) + 0.5f;
  }

  map.setFeatures(features);

  // convert lat & lon to radians
  clat *= M_PI / 180.0f;
  clon *= M_PI / 180.0f;

  if (global) {
    int s = map.getSize();

    if ( (s & (s-1)) != 0 ) {           // Thanks for this cutie, Steve
      fprintf(stderr, "%s: WARNING! Size is not a power of two - you will " \
	      "not be able to use\n" \
	      "these maps with the Atlas program!\n", argv[0] );
    }

    scenerypath = new char[strlen(map.getFGRoot()) + 256];
    strcpy(outname, outp);
    opathl = strlen(outname);
    strcpy( scenerypath, map.getFGRoot() );
    strcat( scenerypath, "/Scenery/" );
    spathl = strlen(scenerypath);
    
    if ( (dir1 = opendir(scenerypath)) == NULL ) {
      fprintf( stderr, "%s: Couldn't open directory \"%s\".\n", 
	       argv[0], scenerypath );
      return 1;
    }
  }

  // now initialize GLUT
  glutInit( &argc, argv );

  if (doublebuffer) {
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH );
  } else {
    glutInitDisplayMode( GLUT_SINGLE | GLUT_RGBA | GLUT_DEPTH );
  }
  glutInitWindowSize( map.getSize(), map.getSize() );
  glutCreateWindow( "MAP - Please wait while drawing" );
  glutReshapeFunc( reshapeMap );
  glutDisplayFunc( redrawMap );

  glutMainLoop();

  return 0;
}


