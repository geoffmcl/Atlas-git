/*-------------------------------------------------------------------------
  Program for creating maps out of Flight Gear scenery data

  Written by Per Liedman, started February 2000.
  Based on a perl-script written by Alexei Novikov (anovikov@heron.itep.ru)

  Copyright (C) 2000 Per Liedman, liedman@home.se
  Copyright (C) 2009 Brian Schack

  This file is part of Atlas.

  Atlas is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Atlas is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU General Public License
  along with Atlas.  If not, see <http://www.gnu.org/licenses/>.

  CHANGES
  2000-02-20        Included some compatibility changes submitted by
                    Christian Mayer.
  2000-02-26        Major code reorganisation, made class MapMaker, etc.
  2000-03-06        Following Norman Vine's advice, map now uses OpenGL
                    for output
  2000-04-29        New, cuter, airports
  2004-12-22        DCL: Now reads FG_SCENERY when present.
  2005-01-16        DCL: Capable of off-screen rendering on modern GLX machines.
  2005-01-30        Switched to cross-platform render-texture code for 
                    off-screen rendering, plus jpg support (submitted by Fred 
		    Bouvier).
  2005-02-26        FB: Arbitrary size by tiling.
                    Move fg_set_scenery to a specific file.
---------------------------------------------------------------------------*/

// EYE - put in configure.ac somehow?
#define VERSION "0.4.0"

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#ifndef _MSC_VER
#  include <unistd.h>
#endif

#include <stdexcept>

#include <plib/pu.h>
#include <simgear/misc/sg_path.hxx>

#include <simgear/bucket/newbucket.hxx>

#include "Tiles.hxx"
#include "TileMapper.hxx"
#include "Palette.hxx"
#include "misc.hxx"

using namespace std;

char *appName;

// Specifies whether to create JPEGs or PNGs.
static bool createJPEG = true;
static unsigned int jpegQuality = 75;
static bool renderToFramebuffer = true;
// Turn the lights on or off?
static bool lighting = true;
// True if we want discrete elevation colours, false for smoothly
// varying elevation colours.
static bool discreteContours = true;
// True if we want contour lines.
static bool contourLines = false;
// Position of light.  We always set the light at infinity (w = 0.0);
// EYE - this must be shared with Atlas - make defaults global?
static float azimuth = 315.0, elevation = 55.0;
// static sgVec4 lightPosition = {-1.0, 1.0, 2.0, 0.0};
static sgVec4 lightPosition;
// True if we want smooth shading, false if we want flat shading.
static bool smoothShading = true;

// Used to specify over-sampling.  This is given as an exponent, to be
// added to the file resolution.  So, for example, if the desired file
// size is 8 (2^8 = 256), and the rescale factor is 2 (2^2 = 4), then
// the map will be rendered at 1024x1024 (2^10 = 1024), but saved at
// 256x256.
static unsigned int rescaleFactor = 0;
// If true, we just print out what we would do, then exit.
static bool test = false;
// Print extra information while processing.
static bool verbose = false;

static TileManager *tileManager;
static SGPath scenery, fg_scenery, fg_root, atlas, palette;
static Palette *atlasPalette;

// Handles to our framebuffer and renderbuffer objects.
GLuint fbo = 0, rbo = 0;
static TileMapper *mapper;

static int bufferSize;	// Size of rendering buffer.

////////////////////////////////////////////////////////////////////////////////
// Renders a single scenery tile, perhaps at several different sizes,
// storing the results as image files.  It only generates maps if they
// don't already exist.
////////////////////////////////////////////////////////////////////////////////
void renderMap(TileInfo *t)
{
    const bitset<TileManager::MAX_MAP_LEVEL>& maps = t->missingMaps();
    if (maps.none()) {
	return;
    }

    // This tile has missing maps.  Load, render, save, and unload
    // them.
    try {
	bool first = true;
	printf("%s: ", t->name());

	mapper->set(t);
	for (unsigned int i = 0; i < TileManager::MAX_MAP_LEVEL; i++) {
	    if (maps[i]) {
		mapper->draw(i + rescaleFactor);
		if (createJPEG) {
		    mapper->save(i, TileMapper::JPEG, jpegQuality);
		} else {
		    mapper->save(i, TileMapper::PNG);
		}

		if (!first) {
		    printf(", ");
		}
		printf("%u", i);
		first = false;
		if (!renderToFramebuffer) {
		    glutSwapBuffers();
		}
	    }
	}
	printf("\n");
    } catch (runtime_error &e) {
	// EYE - make these strings constants?
	if (strcmp(e.what(), "scenery") == 0) {
	    fprintf(stderr, "%s: Unable to load buckets for '%s' from '%s'\n", 
		    appName, t->name(), t->sceneryDir().str().c_str());
	}
    }
}

void print_help() 
{
    printf("Map - FlightGear mapping utility\n\n");
    printf("Usage:\n");
    printf("  --fg-root=path     Overrides FG_ROOT environment variable\n");
    printf("  --fg-scenery=path  Overrides FG_SCENERY environment variable\n");
    printf("  --atlas=path       Store maps in path\n");
    printf("  --palette=path     Set the palette file to use\n");
    printf("  --png              Create PNG images\n");
    printf("  --jpeg             Create JPEG images with quality %u (default)\n",
	   jpegQuality);
    printf("  --jpeg=integer     Create JPEG images with specified quality\n");
    printf("  --aafactor=integer Antialiasing factor (default = %u)\n",
	   rescaleFactor);
    printf("  --render-offscreen Render offscreen (default)\n");
    printf("  --render-to-window Render to a window\n");
    printf("  --discrete-contour Don't blend contour colours (default)\n");
    printf("  --smooth-contour   Blend contour colours\n");
    printf("  --no-contour-lines Don't draw contour lines (default)\n");
    printf("  --contour-lines    Draw contour lines\n");
    printf("  --light=azim,elev  Set light position (default = <%.0f, %.0f>)\n",
	   azimuth, elevation);
    printf("  --lighting         Light the terrain (default)\n");
    printf("  --no-lighting      Don't light the terrain (flat light)\n");
    printf("  --smooth-shading   Smooth polygons (default)\n");
    printf("  --flat-shading     Don't smooth polygons\n");
    printf("  --test             Do nothing, but report what Map would do\n");
    printf("  --verbose          Display extra information while mapping\n");
    printf("  --version          Print version and exit\n");
    printf("  --help             Print this message\n");
}

bool parse_arg(char* arg) 
{
    if (strncmp(arg, "--fg-root=", 10) == 0) {
	fg_root.set(arg + 10);
    } else if (strncmp(arg, "--fg-scenery=", 13) == 0) {
	scenery.set(arg + 13);
    } else if (strncmp(arg, "--atlas=", 8) == 0) {
	atlas.set(arg + 8);
    } else if (strncmp(arg, "--palette=", 10) == 0) {
	palette.set(arg + 10);
    } else if (strcmp(arg, "--png") == 0) {
	createJPEG = false;
    } else if (strcmp(arg, "--jpeg") == 0) {
	createJPEG = true;
    } else if (sscanf(arg, "--jpeg=%d", &jpegQuality) == 1) {
	createJPEG = true;
    } else if (strcmp(arg, "--discrete-contour") == 0) {
	discreteContours = true;
    } else if (strcmp(arg, "--smooth-contour") == 0) {
	discreteContours = false;
    } else if (strcmp(arg, "--contour-lines") == 0) {
	contourLines = true;
    } else if (strcmp(arg, "--no-contour-lines") == 0) {
	contourLines = false;
    } else if (sscanf(arg, "--aafactor=%d", &rescaleFactor) == 1) {
	;
    } else if (strcmp(arg, "--render-offscreen") == 0) {
	renderToFramebuffer = true;
    } else if (strcmp(arg, "--render-to-window") == 0) {
	renderToFramebuffer = false;
    } else if (sscanf(arg, "--light=%f, %f", &azimuth, &elevation) == 2) {
	// Force them to be in range.
	azimuth = normalizeHeading(azimuth);
	if (elevation < 0.0) {
	    elevation = 0.0;
	}
	if (elevation > 90.0) {
	    elevation = 90.0;
	}
    } else if (strcmp(arg, "--lighting") == 0) {
	lighting = true;
    } else if (strcmp(arg, "--no-lighting") == 0) {
	lighting = false;
    } else if (strcmp(arg, "--smooth-shading") == 0) {
	smoothShading = true;
    } else if (strcmp(arg, "--flat-shading") == 0) {
	smoothShading = false;
    } else if (strcmp(arg, "--test") == 0) {
	test = true;
    } else if (strcmp(arg, "--verbose") == 0) {
	verbose = true;
    } else if (strcmp(arg, "--version") == 0) {
	printf("Map version %s\n", VERSION);
	exit(0);
    } else if (strcmp(arg, "--help") == 0) {
	print_help();
	exit(0);
    } else {
	return false;
    }

    return true;
}

bool getFramebuffer(int textureSize) 
{
    glGenFramebuffersEXT(1, &fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);

    glGenRenderbuffersEXT(1, &rbo);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rbo);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, 
			     GL_RGB, textureSize, textureSize);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
				 GL_COLOR_ATTACHMENT0_EXT,
				 GL_RENDERBUFFER_EXT,
				 rbo);

    return (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == 
	    GL_FRAMEBUFFER_COMPLETE_EXT);
}

// Attempts to load a palette at the given path.  Returns the palette
// if successful, NULL otherwise.
Palette *loadPalette(const char *path)
{
    Palette *result = NULL;
    if (verbose) {
	printf("Trying to read palette file '%s'\n", path);
    }
    try {
    	result = new Palette(path);
    } catch (runtime_error e) {
    }

    return result;
}

// Deletes whatever we've allocated.
void cleanup(int exitCode)
{
    if (atlasPalette) {
	delete atlasPalette;
    }
    if (tileManager) {
	delete tileManager;
    }
    if (rbo != 0) {
	glDeleteRenderbuffersEXT(1, &rbo);
    }
    if (fbo != 0) {
	glDeleteFramebuffersEXT(1, &fbo);
    }
    if (mapper) {
	delete mapper;
    }

    exit(exitCode);
}

////////////////////////////////////////////////////////////////////////////////
// main
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    appName = argv[0];

    // Read the FG_ROOT and FG_SCENERY environment variables before
    // processing .atlasmaprc and command args, so that we can
    // override them if necessary.
    char *env = getenv("FG_ROOT");
    if (env == NULL) {
	// EYE - is it possible for this to not be defined?
	fg_root.set(FGBASE_DIR);
    } else {
	fg_root.set(env);
    }

    env = getenv("FG_SCENERY");
    if (env == NULL) {
	fg_scenery.set(fg_root.str());
    } else {
	fg_scenery.set(env);
    }

    // Set a default palette.
    palette.set("default.ap");

    // Process ~/.atlasmaprc.
    char* homedir = getenv("HOME");
    const char *atlasmaprc = ".atlasmaprc";
    AtlasString rcPath;
    if (homedir != NULL) {
	rcPath.printf("%s/%s", homedir, atlasmaprc);
    } else {
	rcPath.printf("%s", atlasmaprc);
    }

    FILE* rc = fopen(rcPath.str(), "r");

    if (rc != NULL) {
	char line[256];
	fgets(line, 256, rc);
	while (!feof(rc)) {
	    line[strlen(line) - 1] = '\0';
	    if (!parse_arg(line)) {
		fprintf(stderr, "%s: unknown argument '%s' in file '%s'\n",
			appName, line, rcPath.str());
	    }
	    fgets(line, 256, rc);
	}
	fclose(rc);
    }

    // Process command line arguments.
    for (int arg = 1; arg < argc; arg++) {
	if (!parse_arg(argv[arg])) {
	    fprintf(stderr, "%s: unknown argument '%s'.\n", appName, argv[arg]);
	    print_help();
	    exit(1);
	}
    }

    // Set lighting vector.
    // EYE - put this code in misc or somewhere?
    float a = (90.0 - azimuth) * SG_DEGREES_TO_RADIANS;
    float e = elevation * SG_DEGREES_TO_RADIANS;
    lightPosition[0] = cos(a) * cos(e);
    lightPosition[1] = sin(a) * cos(e);
    lightPosition[2] = sin(e);
    lightPosition[3] = 0.0;

    // Figure out which scenery path to use.  We place the final path
    // in the 'scenery' variable.
    if (!scenery.str().empty()) {
	// Specified on the command line and already placed in
	// 'scenery'.  We're done.
    } else if (!fg_scenery.str().empty()) {
	// From FG_SCENERY
	scenery.set(fg_scenery.str());
    } else if (!fg_root.str().empty()) {
	// Default: $FG_ROOT/Scenery
	scenery.set(fg_root.str());
	scenery.append("Scenery");
    } else {
	fprintf(stderr, "%s: No scenery directory specified.", appName);
	fprintf(stderr, "\tUse --fg-scenery= or --fg-root= to specify where to find scenery.\n");
	exit(1);
    }
    if (verbose) {
	printf("Scenery directories: %s\n", scenery.str().c_str());
    }

    // EYE - much of the following has to match the logic used in
    // Atlas - put in some global routine(s)?

    // Figure out where to put the rendered maps.  Put the path in the
    // 'atlas' variable.
    if (!atlas.str().empty()) {
	// Specified on the command line and already in 'atlas'.
    } else if (!fg_root.str().empty()) {
	// EYE - make default $HOME/Atlas?
	// Default: $FG_ROOT/Atlas
	atlas.set(fg_root.str());
	atlas.append("Atlas");
    } else {
	fprintf(stderr, "%s: No map directory specified.", appName);
	fprintf(stderr, "\tUse --atlas= to specify where to place maps.\n");
	exit(1);
    }
    if (verbose) {
	printf("Map directory: %s\n", atlas.str().c_str());
    }

    // Now fire up a TileManager.  It will search the scenery and
    // atlas directories and catalogue what we've got.  We ask it to
    // create directories if they don't exist.
    try {
	tileManager = new TileManager(scenery, atlas, true);
    } catch (runtime_error &e) {
	fprintf(stderr, "%s: Unable to create tile manager: %s\n", 
		appName, e.what());
	cleanup(1);
    }

    // Read the Atlas palette file.  If successful, atlasPalette will
    // be set to the loaded palette.
    SGPath palettePath;
    palettePath.append(palette.str());
    if ((atlasPalette = loadPalette(palettePath.c_str())) == NULL) {
	palettePath.set(atlas.str());
	palettePath.append("Palettes");
	palettePath.append(palette.str());
	if ((atlasPalette = loadPalette(palettePath.c_str())) == NULL) {
	    palettePath.set(fg_root.str());
	    palettePath.append("Atlas");
	    palettePath.append("Palettes");
	    palettePath.append(palette.str());
	    atlasPalette = loadPalette(palettePath.c_str());
	}
    }
    if (!atlasPalette) {
	fprintf(stderr, "%s: Failed to read palette file '%s'\n",
		appName, palettePath.c_str());
    	cleanup(1);
    }
    if (verbose) {
	printf("Palette file: %s\n", palettePath.str().c_str());
    }

    if (verbose) {
	printf("Map sizes: ");
	bitset<TileManager::MAX_MAP_LEVEL> sizes = tileManager->mapLevels();
	bool first = true;
	for (unsigned int i = 0; i < sizes.size(); i++) {
	    if (sizes[i]) {
		int x = 1 << i;
		if (!first) {
		    printf(", ");
		}
		printf("%d (%dx%d)", i, x, x);
		first = false;
	    }
	}
	printf("\n");
	printf("Scenery: %d tiles\n", (int)tileManager->tiles().size());
    }

    // Find out what our maximum desired map and buffer sizes are.
    int mapSize = 0;
    const bitset<TileManager::MAX_MAP_LEVEL>& mapLevels = 
	tileManager->mapLevels();
    for (int i = TileManager::MAX_MAP_LEVEL - 1; i >= 0; i--) {
	if (mapLevels[i]) {
	    mapSize = 1 << i;
	    break;
	}
    }
    bufferSize = mapSize << rescaleFactor;

    // Initialize OpenGL.
    int windowSize = bufferSize;
    if (renderToFramebuffer) {
	// Just pick a small window size - we shouldn't see it anyway.
	windowSize = 256;
    }
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(windowSize, windowSize);
    glutCreateWindow("Map");
  
    if (renderToFramebuffer) {
	// Try to get a framebuffer.  First, check if the requested
	// size is supported.
	GLint max;
	glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE_EXT, &max);
	if (bufferSize > max) {
	    fprintf(stderr, 
		    "%s: Requested buffer size (%d) > maximum supported buffer size (%d)\n", 
		    appName, bufferSize, max);
	    cleanup(1);
	}
	if (!getFramebuffer(bufferSize)) {
	    fprintf(stderr, 
		    "%s: Unable to initialize framebuffer.\n",
		    appName);
	    cleanup(1);
	}
	if (verbose) {
	    printf("Framebuffer size: %dx%d\n", bufferSize, bufferSize);
	}
    }

    // Check if largest desired size will fit into a texture.  In some
    // ways this is immaterial to Map.  However, the user should be
    // warned if she is about to create maps that Atlas will be unable
    // to load.
    GLint textureSize = mapSize;
    while (true) {
	GLint tmp;
	// For this test to be accurate, we need to make the
	// parameters (GL_RGB, ...) the same as the ones we'll use in
	// Atlas when loading the texture.
	glTexImage2D(GL_PROXY_TEXTURE_2D, 0, GL_RGB,
		     textureSize, textureSize, 0, 
		     GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0,
				 GL_TEXTURE_WIDTH, &tmp);
	if ((tmp != 0) || (textureSize == 1)) {
	    break;
	}
	textureSize /= 2;
    };
    if (verbose) {
	printf("Maximum supported texture size <= map size: %dx%d\n", 
	       (int)textureSize, (int)textureSize);
    }

    if (textureSize < mapSize) {
	printf("Warning: you have requested maps of maximum size %dx%d,\n",
	       mapSize, mapSize);
	printf("which is larger than the largest texture size supported by\n");
	printf("this machine's graphics hardware (%dx%d).\n",
	       (int)textureSize, (int)textureSize);
	printf("Although Map can generate the maps, Atlas will probably not\n");
	printf("be able to read them on this machine.\n");
    }

    if (test) {
	// Print out a report, then exit.
	printf("Scenery directory:\n\t%s\n", scenery.c_str());
	printf("Atlas map directory:\n\t%s\n", atlas.c_str());
	printf("Palette file:\n\t%s\n", palettePath.c_str());

	const map<string, TileInfo *>& tiles = tileManager->tiles();
	printf("Scenery:\n\t%d tiles in total\n", (int)tiles.size());
	printf("Map resolutions:\n");
	const bitset<TileManager::MAX_MAP_LEVEL>& mapLevels = 
	    tileManager->mapLevels();
	for (unsigned int i = 0; i < TileManager::MAX_MAP_LEVEL; i++) {
	    if (mapLevels[i]) {
		int size = 1 << i;
		printf("\t%d (%dx%d)\n", i, size, size);
	    }
	}

	int tileCount = 0, mapCount = 0;
	map<string, TileInfo *>::const_iterator i = tiles.begin();
	for (; i != tiles.end(); i++) {
	    TileInfo *t = i->second;
	    const bitset<TileManager::MAX_MAP_LEVEL>& maps = t->missingMaps();
	    if (!maps.none()) {
		if (tileCount == 0) {
		    printf("Missing maps:\n");
		}
		tileCount++;
		printf("\t%s: ", t->name());
		for (unsigned int j = 0; j < TileManager::MAX_MAP_LEVEL; j++) {
		    if (maps[j]) {
			mapCount++;
			printf("%d ", j);
		    }
		}
		printf("\n");
	    }
	}
	printf("\n-------------------- Summary --------------------\n");
	if (tileCount > 0) {
	    printf("%d tile(s) missing %d map(s)\n", 
		   tileCount, mapCount);
	} else {
	    printf("No maps to generate\n");
	}
	exit(1);
    }

    // Now we know where to get the scenery data, where to put the
    // maps, the desired map sizes, the size of the buffers we can
    // use, and the palette to use.  Let's draw!
    mapper = new TileMapper(atlasPalette, discreteContours, contourLines,
			    lightPosition, lighting, smoothShading);

    const map<string, TileInfo *>& tiles = tileManager->tiles();
    map<string, TileInfo *>::const_iterator i = tiles.begin();
    for (; i != tiles.end(); i++) {
	TileInfo *t = i->second;
	renderMap(t);
    }

    cleanup(0);
    
    return 0;
}
