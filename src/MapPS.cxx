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
  2005-02-26        --fg-scenery option
---------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "MapMaker.hxx"
#include "OutputPS.hxx"
#include <plib/ul.h>
#include "Scenery.hxx"

SG_USING_STD(vector);
SG_USING_STD(string);

typedef vector<string> string_list;

float clat = -100.0f, clon = -100.0f;   // initialize to unreasonable values
char *outp = "map.eps";                 // output file name
bool autoscale = false, global = false;
MapMaker mapobj;

char *scenerypath;
string raw_scenery_path = "";
string_list fg_scenery;
unsigned int scenery_pos;
unsigned int max_path_length = 0;

void print_help() {
  printf("MapPS - FlightGear PostScript mapping utility\n\n");
  printf("Usage:\n");
  printf("  --lat=xx.xx             Start at latitude xx.xx (deg., south is neg.)\n");
  printf("  --lon=xx.xx             Start at longitude xx.xx (deg., west is neg.)\n");
  printf("  --size=pixels           Create map of size pixels*pixels (default 512)\n");
  printf("  --scale=x               Kilometers from top to bottom of map (default 100)\n"); 
  printf("  --autoscale             Automatically set scale to 1x1 degree tile\n");
  printf("  --light=x, y, z         Set light vector for shading\n");
  printf("  --airport-filter=string Display only airports with id beginning 'string'\n");
  printf("  --output=name           Write output to given file name (default 'mapobj.eps')\n");
  printf("  --fg-root=path          Overrides FG_ROOT environment variable\n");
  printf("  --fg-scenery=path       Overrides FG_SCENERY environment variable\n");
  printf("  --disable-airports      Don't show airports\n");
  printf("  --disable-navaids       Don't show navaids\n");
  printf("  --only-navaids=which    Which navaids (vor,ndb,fix) to show, e.g. \"vor,ndb\"\n");                   
  printf("  --disable-shading       Don't do nice shading of the terrain\n");
  printf("  --atlas=path            Create maps of all scenery, and store them in path\n");
  printf("  --verbose               Display information during processing\n\n");
}

int main( int argc, char **argv ) {
  // variables for command line parsing
  int param, features = mapobj.getFeatures();
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
      mapobj.setSize( param );
    } else if ( sscanf(argv[arg], "--scale=%f", &x) == 1 ) {
      mapobj.setScale( (int)(x * 1000.0f) );
    } else if ( sscanf(argv[arg], "--light=%f, %f, %f", &x, &y, &z) == 3 ) {
      mapobj.setLight( x, y, z );
    } else if ( sscanf(argv[arg], "--airport-filter=%s", cparam) == 1 ) {
      mapobj.setAPFilter( strdup(cparam) );
    } else if ( sscanf(argv[arg], "--output=%s", cparam) == 1 ) {
      outp = strdup(cparam);
    } else if ( sscanf(argv[arg], "--fg-root=%s", cparam) == 1 ) {
      mapobj.setFGRoot( strdup(cparam) );
    } else if ( strncmp(argv[arg], "--fg-scenery=", 13 ) == 0 ) {
      raw_scenery_path = argv[arg] + 13;
    } else if ( strcmp(argv[arg], "--disable-airports" ) == 0 ) {
      features &= ~MapMaker::DO_AIRPORTS;
    } else if ( strcmp(argv[arg], "--disable-navaids" ) == 0 ) {
      features &= ~MapMaker::DO_NAVAIDS;
    } else if ( sscanf(argv[arg], "--only-navaids=%s", cparam) ==1 ) {
      if (!strstr(cparam,"vor")) features &= ~MapMaker::DO_NAVAIDS_VOR;
      if (!strstr(cparam,"ndb")) features &= ~MapMaker::DO_NAVAIDS_NDB;
      if (!strstr(cparam,"fix")) features &= ~MapMaker::DO_NAVAIDS_FIX;
      if (!strstr(cparam,"ils")) features &= ~MapMaker::DO_NAVAIDS_ILS;
    } else if ( strcmp(argv[arg], "--disable-shading" ) == 0 ) {
      features &= ~MapMaker::DO_SHADE;
    } else if ( strcmp(argv[arg], "--autoscale") == 0 ) {
      autoscale = true;
    } else if ( sscanf(argv[arg], "--atlas=%s", cparam) == 1 ) {
      global = true;
      outp = strdup( cparam );
    } else if ( strcmp(argv[arg], "--verbose") == 0 ) {
      features |= MapMaker::DO_VERBOSE;
    } else if ( strcmp(argv[arg], "--help") == 0 ) {
      print_help();
      exit(0);
    } else {
      printf("%s: unknown argument '%s'.\n", argv[0], argv[arg]);
      print_help();
      exit(1);
    }
  }
  
  if (!global && (fabs(clat) > 90.0f || fabs(clon) > 180.0f)) {
    printf("%s: Invalid position. Check latitude and longitude.\n", argv[0]);
    print_help();
    exit(1);
  }

  if (autoscale) {
    clat = ((int)clat) + 0.5f;
    clon = ((int)clon) + 0.5f;
  }
  
  set_fg_scenery(raw_scenery_path);
  if(fg_scenery.size()) {
    scenerypath = new char[max_path_length + 256];
    scenery_pos = 0;
  } else {
    cout << "No scenery paths could be found.  You need to set either a valid FG_ROOT and/or FG_SCENERY variable, or specify a valid --fg-root and/or --fg-scenery on the command line.\n";
    exit(-1);
  }

  mapobj.setFeatures(features);
  mapobj.setDeviceSize( mapobj.getSize() );

  // convert lat & lon to radians
  clat *= SG_DEGREES_TO_RADIANS;
  clon *= SG_DEGREES_TO_RADIANS;


  // Hack alert -- set the nav1_freq and nav2_freq variables to things
  //   which will prevent MapPS from thinking a radio is set to these
  //   frequencies

  nav1_freq=nav2_freq=-1000.;
  
  char *scenerypath = new char[strlen(mapobj.getFGRoot()) + 256];
  char *workingpath = new char[strlen(mapobj.getFGRoot()) + 256];
  strcpy( scenerypath, mapobj.getFGRoot() );
  strcat( scenerypath, "/Scenery/Terrain/" );
  strcpy( workingpath, scenerypath );

  if (!global) {
    OutputPS output( outp, mapobj.getSize() );
    mapobj.createMap( &output, clat, clon, scenerypath, autoscale );
    output.closeOutput();
  } else {
    char ns, ew;
    int lat, lon;
    int s = mapobj.getSize();
    ulDir *dir1 = NULL, *dir2 = NULL;
    ulDirEnt *ent = NULL;
    size_t opathl, spathl;
    char outname[512];

    scenery_pos = 0;
    dir1 = ulOpenDir(fg_scenery[0].c_str());
    strcpy(scenerypath, fg_scenery[0].c_str());
    NormalisePath(scenerypath);
    spathl = strlen(scenerypath);
    if(dir1 == NULL) {
      printf("Unable to open directory \"%s\"\\n", scenerypath);
    }
   
    strcpy(outname, outp);
    opathl = strlen(outname);

    while(1) {
      do {
        while (dir2 == NULL) {
	  while(dir1 == NULL) {
	    ++scenery_pos;
	    if(scenery_pos >= fg_scenery.size()) {
	      delete[] scenerypath;
	      exit(0);  // Ought to flag whether at least one path read OK before exiting OK.
	    }
	    dir1 = ulOpenDir(fg_scenery[scenery_pos].c_str());
	    strcpy(scenerypath, fg_scenery[scenery_pos].c_str());
	    NormalisePath(scenerypath);
	    spathl = strlen(scenerypath);
	    if(dir1 == NULL) {
	      printf("Unable to open directory \"%s\"\\n", scenerypath);
	    }
	  }
          bool exit = false;
	  do {
	    ent = ulReadDir(dir1);

	    if (ent != NULL) {
	      exit = (ent->d_name[0] != '.' &&
		      sscanf(ent->d_name, "%c%d%c%d",
			    &ew, &lon, &ns, &lat) == 4 &&
                      strlen(ent->d_name) == 7);
	    } else {
	      exit = true;
	    }
          } while (!exit);
          if (ent != NULL) {
	    strcpy( scenerypath+spathl, ent->d_name );
	    dir2 = ulOpenDir(scenerypath);
	  } else {
	    ulCloseDir(dir1);
	    dir1 = NULL;
	  }
        }
        
        bool exit = false;
        do {
	  ent = ulReadDir(dir2);

	  if (ent != NULL) {
	    exit = (ent->d_name[0] != '.' &&
		    sscanf(ent->d_name, "%c%d%c%d",
			  &ew, &lon, &ns, &lat) == 4 &&
                    strlen(ent->d_name) == 7);
	  } else {
	    exit = true;
	  }
        } while (!exit);

        if (ent == NULL) {
	  ulCloseDir(dir2);
	  dir2 = NULL;
        } else {
	  // we have found a scenery directory - let's check if we already
	  // have an image for it!
	  lat *= (ns=='n')?1:-1;
	  lon *= (ew=='e')?1:-1;
	  clat = ((float)lat + 0.5f) * SG_DEGREES_TO_RADIANS;
	  clon = ((float)lon + 0.5f) * SG_DEGREES_TO_RADIANS;
  	
	  sprintf( outname+opathl, "/%c%03d%c%02d.eps", 
		  (lon<0)?'w':'e', abs(lon), 
		  (lat<0)?'s':'n', abs(lat) );

	  struct stat filestat;
	  if ( stat(outname, &filestat) == 0 ) {
	    // the file already exists, so skip it!
	    ent = NULL;
	  }
        }
      } while (ent == NULL);

      OutputPS output( outname, mapobj.getSize() );
      mapobj.createMap( &output, clat, clon, fg_scenery[scenery_pos], 1.0f );
      output.closeOutput();
    }
  }

  return 0;
}


