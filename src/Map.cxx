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
  2004-12-22        DCL: Now reads FG_SCENERY when present.
  2005-01-16        DCL: Capable of off-screen rendering on modern GLX machines.
---------------------------------------------------------------------------*/

// Needs to be included *before* the UL_GLX check!
#include <plib/ul.h>

#include <GL/gl.h>
#include <GL/glext.h>
#ifdef UL_GLX
  #include <GL/glx.h>
#endif
#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#ifndef _MSC_VER
#  include <unistd.h>
#endif
#include "MapMaker.hxx"
#include "OutputGL.hxx"
#include <simgear/compiler.h>
#include <simgear/misc/sg_path.hxx>
#include <vector>
#include STL_STRING

SG_USING_STD(vector);
SG_USING_STD(string);

typedef vector<string> string_list;

float clat = -100.0f, clon = -100.0f;   // initialize to unreasonable values
float autoscale = 0.0f;                 // 0.0f == no autoscale
char *outp = "map.png";                 // output file name
bool global = false;
bool doublebuffer = true, headless = false;
bool smooth_shade = true, textured_fonts = true;
int features = MapMaker::DO_SHADE;
MapMaker mapobj;

char outname[512], *scenerypath, *palette;
string raw_scenery_path = "";
string_list fg_scenery;
unsigned int scenery_pos;
unsigned int max_path_length = 0;
int opathl, spathl;
ulDir *dir1, *dir2 = NULL;
ulDirEnt *ent;


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
  char font_name[512];

  if (textured_fonts) {
    strcpy( font_name, mapobj.getFGRoot() );
    strcat( font_name, "/Fonts/helvetica_medium.txf");
  }

  if (!global) {
    sprintf(title_buffer, "%c%.1f %c%.1f",
	    (clat<0.0f)?'S':'N', fabs(clat * SG_RADIANS_TO_DEGREES),
	    (clon<0.0f)?'W':'E', fabs(clon * SG_RADIANS_TO_DEGREES));
    if(!headless) glutSetWindowTitle(title_buffer);

    OutputGL output( outp, mapobj.getSize(), smooth_shade, 
		     textured_fonts, font_name );
    mapobj.createMap( &output, clat, clon, scenerypath, autoscale );
    output.closeOutput();
    exit(0);
  } else {
    char ns, ew;
    int lat, lon;
    int s = mapobj.getSize();
  
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
	do {
	  ent = ulReadDir(dir1);
	} while (ent != NULL && ent->d_name[0] == '.');
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
			 &ew, &lon, &ns, &lat) == 4);
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
	    (clat<0.0f)?'S':'N', clat * SG_RADIANS_TO_DEGREES,
	    (clon<0.0f)?'W':'E', clon * SG_RADIANS_TO_DEGREES);
    if(!headless) glutSetWindowTitle(title_buffer);

    OutputGL output(outname, s, smooth_shade, textured_fonts, font_name);
    mapobj.createMap( &output, clat, clon, fg_scenery[scenery_pos], 1.0f );
    output.closeOutput();
    if (doublebuffer && !headless) {
      glutSwapBuffers();
    }

    if(!headless) glutPostRedisplay();
  }
}

/*****************************************************************************/

// Cribbed from FlightGear, with the objects code commented out.
// The supplied path is appended with 'Terrain' if it exists, not if otherwise.
// NOTE: MapMaker::setFGRoot() should be called first.
void set_fg_scenery(const string &scenery) {
    SGPath s;
    
    char* fg_root = mapobj.getFGRoot();
    if (scenery.empty() && fg_root != NULL) {
        s.set( fg_root );
        s.append( "Scenery" );
    } else {
        s.set( scenery );
    }
    
    string_list path_list = sgPathSplit( s.str() );
    fg_scenery.clear();
    
    for (unsigned i = 0; i < path_list.size(); i++) {

        max_path_length = ( path_list[i].size() > max_path_length ? path_list[i].size() : max_path_length );
        ulDir *d = ulOpenDir( path_list[i].c_str() );
        if (d == NULL)
            continue;
        ulCloseDir( d );

        //SGPath pt( path_list[i], po( path_list[i] );
	SGPath pt( path_list[i] );
        pt.append("Terrain");
        //po.append("Objects");

        ulDir *td = ulOpenDir( pt.c_str() );
        //ulDir *od = ulOpenDir( po.c_str() );

        //if (td == NULL && od == NULL)
	if(td == NULL) { // ie. it doen't exist with Terrain appended - push back the original
            fg_scenery.push_back( path_list[i] );
        } else {
            if (td != NULL) {
                fg_scenery.push_back( pt.str() );
                ulCloseDir( td );
            }
	    /*
            if (od != NULL) {
                fg_scenery.push_back( po.str() );
                ulCloseDir( od );
            }
	    */
        }
    }
}

/*****************************************************************************/

void print_help() {
  printf("MAP - FlightGear mapping utility\n\n");
  printf("Usage:\n");
  printf("  --lat=xx.xx             Start at latitude xx.xx (deg., south is neg.)\n");
  printf("  --lon=xx.xx             Start at longitude xx.xx (deg., west is neg.)\n");
  printf("  --size=pixels           Create map of size pixels*pixels (default 256)\n");
  printf("  --scale=x               Kilometers from top to bottom of map (default 100)\n"); 
  printf("  --autoscale             Automatically set scale to 1x1 degree tile\n");
  printf("  --light=x, y, z         Set light vector for shading\n");
  printf("  --airport-filter=string Display only airports with id beginning 'string'\n");
  printf("  --output=name           Write output to given file name (default 'map.png')\n");
  printf("  --fg-root=path          Overrides FG_ROOT environment variable\n");
  printf("  --fg-scenery=path       Overrides FG_SCENERY environment variable\n");
  printf("  --enable-airports       Show airports\n");
  printf("  --enable-navaids        Show navaids\n");
  printf("  --flat-shading          Don't do nice shading of the terrain\n");
  printf("  --atlas=path            Create maps of all scenery, and store them in path\n");
  printf("  --verbose               Display information during processing\n");
  printf("  --singlebuffer          Use single buffered display\n");
#ifdef GLX_VERSION_1_3
  printf("  --headless              Don't display output (render into an off-screen buffer)\n");
#endif
  printf("  --glutfonts             Use GLUT built-in fonts\n");
  printf("  --palette=path          Set the palette file to use\n");
  printf("  --smooth-color          Make smooth color heights\n");
}

bool parse_arg(char* arg) {
  int param;
  char cparam[128];
  float x, y, z;

  if ( sscanf(arg, "--lat=%f", &clat) == 1) {
    // do nothing
  } else if ( sscanf(arg, "--lon=%f", &clon) == 1 ) {
    // do nothing
  } else if ( sscanf(arg, "--size=%d", &param) == 1 ) {
    mapobj.setSize( param );
  } else if ( sscanf(arg, "--scale=%f", &x) == 1 ) {
    mapobj.setScale( (int)(x * 1000.0f) );
  } else if ( sscanf(arg, "--light=%f, %f, %f", &x, &y, &z) == 3 ) {
    mapobj.setLight( x, y, z );
  } else if ( sscanf(arg, "--airport-filter=%s", cparam) == 1 ) {
    mapobj.setAPFilter( strdup(cparam) );
  } else if ( sscanf(arg, "--output=%s", cparam) == 1 ) {
    outp = strdup(cparam);
  } else if ( sscanf(arg, "--fg-root=%s", cparam) == 1 ) {
    mapobj.setFGRoot( cparam );
  } else if ( strncmp(arg, "--fg-scenery=", 13 ) == 0 ) {
    raw_scenery_path = arg + 13;
  } else if ( strcmp(arg, "--enable-airports" ) == 0 ) {
    features |= MapMaker::DO_AIRPORTS;
  } else if ( strcmp(arg, "--enable-navaids" ) == 0 ) {
    features |= MapMaker::DO_NAVAIDS;
  } else if ( strcmp(arg, "--flat-shading" ) == 0 ) {
    smooth_shade = false;
  } else if ( strcmp(arg, "--autoscale") == 0 ) {
    autoscale = 1.0f;
  } else if ( sscanf(arg, "--autoscale=%f", &autoscale) == 1 ) {
    // do nothing
  } else if ( strcmp(arg, "--singlebuffer") == 0 ) {
    doublebuffer = false;
  } else if ( strcmp(arg, "--headless") == 0 ) {
#ifdef GLX_VERSION_1_3
    headless = true;
#else
    cout << "Headless support is only available if compiled and run on a GLX version 1.3 or better machine\n";
    cout << "Exiting...\n";
    exit(-1);
#endif
  } else if ( sscanf(arg, "--atlas=%s", cparam) == 1 ) {
    global = true;
    outp = strdup( cparam );
  } else if ( sscanf(arg, "--palette=%s", cparam) == 1 ) {
    mapobj.setPalette(cparam);
  } else if ( strcmp(arg, "--glutfonts") == 0 ) {
    textured_fonts = false;
  } else if ( strcmp(arg, "--verbose") == 0 ) {
    features |= MapMaker::DO_VERBOSE;
  } else if ( strcmp(arg, "--smooth-color") == 0 ) {
    features |= MapMaker::DO_SMOOTH_COLOR;
  } else if ( strcmp(arg, "--help") == 0 ) {
    print_help();
    exit(0);
  } else if ( strcmp(arg, "--disable-airports" ) == 0 ) {
    // Do nothing - only for backwards compatibility
  } else if ( strcmp(arg, "--disable-navaids" ) == 0 ) {
    // Do nothing - only for backwards compatibility
  } else {
    return false;
  }

  return true;
}

// All the pbuffer stuff might as well be commented out for non-GLX-1.3 or better platforms for now
#ifdef GLX_VERSION_1_3
// Return true if string 1 contains string 2, assuming the required string is whitespace delimited
bool HaveTarget(char* s1, const char* s2) {
  // TODO - parse on whitespace - currently we can return true if a substring of a longer string exists.
  return(strstr(s1, s2) == NULL ? false : true);
}

// Query whether a given extension exists
bool HaveExtension(const char* ext) {
  char* exts = (char*)glGetString(GL_EXTENSIONS);
  return(HaveTarget(exts, ext));
}

// Query whether a given windowing extension exists
// (Very OS specific! - currently only implemented for GLX)
bool HaveXExtension(const char* wext, Display* dpy) {
  char* wexts = (char*)glXQueryExtensionsString (dpy, DefaultScreen(dpy));
  return(HaveTarget(wexts, wext));
}

bool ContinueIfNoHeadless() {
  cout << "Unable to continue in headless mode - revert to doublebuffer mode? [Y/n] ";
  char c;
  cin >> c;
  return((c == 'n' || c == 'N') ? false : true);
}

bool InitPbuffer() {
  Display *dpy = XOpenDisplay(0);

  /*	
  const char *glxext = "";		
  glxext = glXQueryExtensionsString (dpy, DefaultScreen(dpy));
  cout << "GLX extensions:\n" << glxext << endl;
  */

  bool have_glx_sgix_fbconfig = HaveXExtension("GLX_SGIX_fbconfig", dpy);
  bool have_glx_sgix_pbuffer = HaveXExtension("GLX_SGIX_pbuffer", dpy);
  cout << "Seaching for extensions...\n";
  cout << "GLX_SGIX_fbconfig: " << (have_glx_sgix_fbconfig ? "YES\n" : "NO\n");
  cout << "GLX_SGIX_pbuffer: " << (have_glx_sgix_pbuffer ? "YES\n" : "NO\n");
  if(!have_glx_sgix_fbconfig || !have_glx_sgix_pbuffer) {
    cout << "\nOne or more required extension(s) could not be found:\n";
    if(!have_glx_sgix_fbconfig) cout << "GLX_SGIX_fbconfig\n";
    if(!have_glx_sgix_pbuffer) cout << "GLX_SGIX_pbuffer\n";
    bool cont = ContinueIfNoHeadless();
    if(cont) return(false);
    else exit(-1);
  }	    

  int scn=DefaultScreen(dpy);
  int cnt;
  GLXFBConfig *cfg = glXGetFBConfigs(dpy, scn, &cnt);
  printf("glXGetFBConfigs returned %d matches\n", cnt);
  int fbw, fbh;
  bool size_ok = false;
  for(int i=0; i < cnt; ++i) {
	  glXGetFBConfigAttrib(dpy, cfg[i], GLX_MAX_PBUFFER_WIDTH, &fbw);
	  glXGetFBConfigAttrib(dpy, cfg[i], GLX_MAX_PBUFFER_HEIGHT, &fbh);
	  cout << "Checking for fbconfig of size " << mapobj.getSize() << "x" << mapobj.getSize() << " or greater...\n";
	  size_ok = (fbw >= mapobj.getSize() && fbh >= mapobj.getSize());
	  cout << "Buffer " << i << ": " << fbw << "x" << fbh << "... " << (size_ok ? "OK\n" : "insufficient\n");
	  if(size_ok) break;
  }
  if(!size_ok) {
    // TODO - ought to say what the maximum size returned was.
    cout << "Unable to get a fbconfig of sufficient size to hold requested image size :-(\n";
    cout << "Try requesting a smaller image size using the --size= argument\n";
    exit(-1);
  }
  int attrlist[] =
  {
    GLX_PBUFFER_WIDTH, mapobj.getSize(),
    GLX_PBUFFER_HEIGHT, mapobj.getSize(),
    GLX_PRESERVED_CONTENTS, True,
    0
  };
  GLXPbufferSGIX pBuffer = glXCreatePbuffer(dpy, cfg[0], attrlist);
  GLXContext cx = glXCreateNewContext(dpy, cfg[0], GLX_RGBA_TYPE, 0, GL_TRUE);
  bool cur_ok = glXMakeContextCurrent(dpy, pBuffer, pBuffer, cx);
  if(cur_ok) {
    cout << "Sucessfully set pbuffer as rendering context!\n";
  } else {
    bool cont = ContinueIfNoHeadless();
    if(cont) return(false);
    else exit(-1);
  }
  return(cur_ok);
}
#endif  // GLX_VERSION_1_3

int main( int argc, char **argv ) {
  if (argc == 0)
    print_help();
  
  // Read the FG_SCENERY env variable before processing .atlasmaprc and command args,
  // so that we can override it if necessary.
  char* scenedirs = getenv("FG_SCENERY");
  if(scenedirs != NULL) raw_scenery_path = scenedirs;

  // process ~/.atlasmaprc
  char* homedir = getenv("HOME"), *rcpath;
  const char atlasmaprc[] = ".atlasmaprc";
  if (homedir != NULL) {
    rcpath = new char[strlen(homedir) + strlen(atlasmaprc) + 2];
    strcpy(rcpath, homedir);
    strcat(rcpath, "/");
    strcat(rcpath, atlasmaprc);
  } else {
    rcpath = new char[strlen(atlasmaprc) + 1];
    strcpy(rcpath, atlasmaprc);
  }

  FILE* rc = fopen(rcpath, "r");

  if (rc != NULL) {
    char line[256];
    fgets(line, 256, rc);
    while (!feof(rc)) {
      line[strlen(line) - 1] = '\0';
      if (!parse_arg(line)) {
	fprintf(stderr, "%s: unknown argument \"%s\" in file \"%s\"\n",
		argv[0], line, rcpath);
      }
      fgets(line, 256,rc);
    }
    fclose(rc);
  }

  delete[] rcpath;

  // process command line arguments
  for (int arg = 1; arg < argc; arg++) {
    if (!parse_arg(argv[arg])) {
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
  
  set_fg_scenery(raw_scenery_path);
  if(fg_scenery.size()) {
    scenerypath = new char[max_path_length + 256];
    scenery_pos = 0;
  } else {
    cout << "No scenery paths could be found.  You need to set either a valid FG_ROOT and/or FG_SCENERY variable, or specify a valid --fg-root and/or --fg-scenery on the command line.\n";
    exit(-1);
  }
  
  mapobj.setFeatures(features);

  // convert lat & lon to radians
  clat *= SG_DEGREES_TO_RADIANS;
  clon *= SG_DEGREES_TO_RADIANS;

  if (global) {
    int s = mapobj.getSize();

    if ( (s & (s-1)) != 0 ) {           // Thanks for this cutie, Steve
      fprintf(stderr, "%s: WARNING! Size is not a power of two - you will " \
	      "not be able to use\n" \
	      "these maps with the Atlas program!\n", argv[0] );
    }

    dir1 = ulOpenDir(fg_scenery[0].c_str());
    strcpy(scenerypath, fg_scenery[0].c_str());
    NormalisePath(scenerypath);
    spathl = strlen(scenerypath);
    if(dir1 == NULL) {
      printf("Unable to open directory \"%s\"\\n", scenerypath);
    }
   
    strcpy(outname, outp);
    opathl = strlen(outname);
  } else {
    // Need to set the scenery path to point to the required location
    float dlat = clat * SG_RADIANS_TO_DEGREES, dlon = clon * SG_RADIANS_TO_DEGREES;
    int lat2 = (int)floor(dlat), lon2 = (int)floor(dlon);
    int lat1 = (int)(floor((float)lat2 / 10.0f) * 10.0), lon1 = (int)(floor((float)lon2 / 10.0f) * 10.0);
    char* dpath1 = new char[32];
    char* dpath2 = new char[32];
    sprintf(dpath1, "%c%03d%c%02d", (lon1 < 0 ? 'w' : 'e'), abs(lon1), (lat1 < 0 ? 's' : 'n'), abs(lat1));
    sprintf(dpath2, "%c%03d%c%02d", (lon2 < 0 ? 'w' : 'e'), abs(lon2), (lat2 < 0 ? 's' : 'n'), abs(lat2));
    bool path_found = false;
    for(unsigned int i = 0; i < fg_scenery.size(); ++i) {
      strcpy(scenerypath, fg_scenery[i].c_str());
      NormalisePath(scenerypath);
      int sz = strlen(scenerypath);
      strcat(scenerypath, dpath1);
      NormalisePath(scenerypath);
      strcat(scenerypath, dpath2);
      dir1 = ulOpenDir(scenerypath);
      if(dir1 != NULL) {
	path_found = true;
	scenerypath[sz] = '\0';
	//cout << "Scenerypath found,  = " << scenerypath << '\n';
	break;
      }
      //cout << scenerypath << (dir1 == NULL ? " does not exist..." : " exists!") << '\n';
    }
    if(!path_found) {
      cout << "Unable to find required subdirectory " << dpath1 << '/' << dpath2 << " on the available scenery paths:\n";
      for(unsigned int i = 0; i < fg_scenery.size(); ++i) {
	cout << fg_scenery[i] << '\n';
      }
      cout << "... unable to continue - exiting!\n";
      exit(-1);
    }
  }

#ifdef GLX_VERSION_1_3
  if(headless) {
    headless = InitPbuffer();
    if(headless) {
      while(1) {
	redrawMap();
      }
      exit(0);
    }
  }
#endif

  // now initialize GLUT
  glutInit( &argc, argv );

  if (doublebuffer) {
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH );
  } else {
    glutInitDisplayMode( GLUT_SINGLE | GLUT_RGBA | GLUT_DEPTH );
  }
  glutInitWindowSize( mapobj.getSize(), mapobj.getSize() );
  glutCreateWindow( "MAP - Please wait while drawing" );
  
  glutReshapeFunc( reshapeMap );
  glutDisplayFunc( redrawMap );

  glutMainLoop();

  return 0;
}

