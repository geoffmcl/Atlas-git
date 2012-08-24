/*-------------------------------------------------------------------------
  TileMapper.cxx

  Written by Brian Schack, started March 2009.

  Copyright (C) 2009 - 2012 Brian Schack

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
  ---------------------------------------------------------------------------*/

// This is a special exception to the usual rule of including our
// include file first.  In TileMapper.hxx, pu.h includes gl.h.
// However, glew insists on being loaded first.
#include <GL/glew.h>

// Our include file
#include "TileMapper.hxx"

// C++ system files
#include <stdexcept>

// Other libraries' include files
#include <simgear/misc/sg_path.hxx>

// Our project's include files
#include "Bucket.hxx"
#include "Image.hxx"
#include "Palette.hxx"
#include "Tiles.hxx"

using namespace std;

// We generate maps by rendering to a frame buffer, the generating a
// texture from the results.  Therefore, the biggest map we can create
// is limited to the smaller of the maximum frame buffer size and
// maximum texture size.
unsigned int TileMapper::maxPossibleLevel()
{
    // Check supported texture sizes.  First, get the maximum possible
    // texture size (ignoring texture format).
    GLint maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    
    // Now take into account the texture format.  Note that for this
    // test to be accurate, we need to make the parameters (GL_RGB8,
    // ...) the same as the ones we'll use in Atlas when loading the
    // texture.
    while (true) {
	GLint tmp = 0;
	glTexImage2D(GL_PROXY_TEXTURE_2D, 0, GL_RGB8,
    		     maxTextureSize, maxTextureSize, 0, 
		     GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0,
				 GL_TEXTURE_WIDTH, &tmp);

	if ((tmp != 0) || (maxTextureSize == 1)) {
	    // We found an acceptable size (or we're down to 1x1
	    // textures).
	    break;
	}
	maxTextureSize /= 2;
    };

    // Now check buffer sizes.  Note that multisampling changes the
    // calculation - the problem is I don't know how (except to reduce
    // the maximum buffer size).  Let the user beware!
    GLint maxBufferSize;
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE_EXT, &maxBufferSize);

    // The result is the minimum of our maximum map size, the maximum
    // texture size, and the maximum buffer size.
    return log2(min(0x1 << TileManager::MAX_MAP_LEVEL,
		    min(maxTextureSize, maxBufferSize)));
}

TileMapper::TileMapper(Palette *p, unsigned int maxDesiredLevel, 
		       bool discreteContours, bool contourLines,
		       float azimuth, float elevation, bool lighting, 
		       bool smoothShading, ImageType imageType, 
		       unsigned int JPEGQuality):
    _palette(p), _maxLevel(maxDesiredLevel),
    _discreteContours(discreteContours), _contourLines(contourLines),
    _azimuth(azimuth), _elevation(elevation), _lighting(lighting),
    _smoothShading(smoothShading), _imageType(imageType), 
    _JPEGQuality(JPEGQuality), _tile(NULL), _to(0)
{
    // We must have a palette.
    if (!_palette) {
	throw runtime_error("palette");
    }

    float a = (90.0 - azimuth) * SG_DEGREES_TO_RADIANS;
    float e = elevation * SG_DEGREES_TO_RADIANS;
    _lightPosition[0] = cos(a) * cos(e);
    _lightPosition[1] = sin(a) * cos(e);
    _lightPosition[2] = sin(e);
    _lightPosition[3] = 0.0;

    // EYE - make a class method?  Does C++ have those?
    Bucket::palette = _palette;
    Bucket::discreteContours = _discreteContours;
    Bucket::contourLines = _contourLines;
}

TileMapper::~TileMapper()
{
    _unloadBuckets();

    // Delete the texture object.
    glDeleteTextures(1, &_to);
}

// Tells TileMapper which tile is to be rendered.  We load the tile's
// buckets, and set _maximumElevation.
void TileMapper::set(Tile *t)
{
    // Remove any old information we have.
    _unloadBuckets();

    _tile = t;
    if (!_tile) {
	// EYE - throw an error, like when checking for a palette?
	return;
    }

    vector<long int> indices;
    _tile->bucketIndices(indices);
    for (unsigned int i = 0; i < indices.size(); i++) {
    	Bucket *b = new Bucket(_tile->sceneryDir(), indices[i]);
    	b->load(Bucket::RECTANGULAR);

    	_buckets.push_back(b);

    	if (b->maximumElevation() > _maximumElevation) {
    	    _maximumElevation = b->maximumElevation();
    	}
    }
}

// Draws the tile into a mipmapped texture (_to) via a multisampled
// renderbuffer (fboms/rbo).
void TileMapper::render()
{
    // EYE - remove this eventually
    assert(glGetError() == GL_NO_ERROR);

    if (!_palette || !_tile) {
	// EYE - throw an error?
	return;
    }

    // Calculate the proper width and height for the given level.
    // Note that we don't check if _maxLevel is reasonable - that's up
    // to the caller.
    _tile->mapSize(_maxLevel, &_width, &_height);
    // EYE - What could reasonably be described as a hack.  At the
    // poles, a tile is 4x wider than it is high.  At a map resolution
    // of 10, this results in a 4096x1024 renderbuffer/texture/map.
    // More importantly, with my video card it hangs the machine.  I
    // can find no way to query OpenGL state to predict reliably that
    // this will happen.  However, limiting the width of the tiles to
    // no more than the height works (for a reasonable height of
    // course).  It's a bit scary, and it would be nice to have a
    // reliable way to determine buffer size limits.
    if (_width > _height) {
	_width = _height;
    }

    // Create the main framebuffer object and bind it to our context.
    // We'll attach a multisampled renderbuffer to it.
    GLuint fboms;
    glGenFramebuffersEXT(1, &fboms);
    assert(fboms != 0);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboms);

    // Create a multisampled renderbuffer object with as many samples
    // as we can get.
    GLuint rbo;
    glGenRenderbuffersEXT(1, &rbo);
    glBindRenderbufferEXT(GL_RENDERBUFFER, rbo);
    int samples;
    glGetIntegerv(GL_MAX_SAMPLES, &samples);
    glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, samples, GL_RGB8, 
    					_width, _height);

    // Attach it to our framebuffer, fbmos.
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
				 GL_RENDERBUFFER_EXT, rbo);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == 
    	   GL_FRAMEBUFFER_COMPLETE_EXT);

    // The framebuffer shares its state with the current context, so
    // we push some attributes so we don't step on current OpenGL
    // state.
    glPushAttrib(GL_VIEWPORT_BIT | GL_LIGHTING_BIT | GL_CURRENT_BIT |
		 GL_POLYGON_BIT | GL_LINE_BIT | GL_MULTISAMPLE_BIT |
		 GL_TEXTURE_BIT); {
	// Set up the view.
	glViewport(0, 0, _width, _height);

	// When our buckets were loaded, all points were converted to
	// lat, lon.  We need to stretch them to fill the buffer
	// correctly.
	int lat = _tile->lat();
	int lon = _tile->lon();
	int w = _tile->width();
	int h = _tile->height();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(lon, lon + w, lat, lat + h);

	// No model or view transformations.
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	// Turn on backface culling.  We use the OpenGL standard of
	// counterclockwise winding for front faces.
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	// Ensure that multisampling is on.
	glEnable(GL_MULTISAMPLE);

	// Tie material ambient and diffuse values to the current colour.
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);

	// Set up lighting.  The values used here must be the same as used
	// in Atlas if you want live scenery to match pre-rendered scenery
	// (the same goes for the palette used).
	sgVec4 lightPosition;
	// EYE - make this a global constant
	const float brightness = 0.8;
	GLfloat diffuse[] = {brightness, brightness, brightness, 1.0f};
	if (_lighting) {
	    // We make a copy of the light position because we may rotate
	    // it later.
	    sgCopyVec4(lightPosition, _lightPosition);

	    glEnable(GL_LIGHTING);
	    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
	    glEnable(GL_LIGHT0);
	}

	// Ask for smooth or flat shading.
	glShadeModel(_smoothShading ? GL_SMOOTH : GL_FLAT);

	// Now that we've set everything up, we first draw an
	// ocean-coloured rectangle covering the entire tile.  This
	// ensures that every part of the tile is coloured, even where
	// there is no bucket for it.
	const float *c;
	assert(_palette);
	if ((c = _palette->colour("Ocean"))) {
	    glBegin(GL_QUADS); {
	    	glColor4fv(c);
	    	glNormal3f(0.0, 0.0, 1.0);

	    	glVertex2f(lon, lat);
	    	glVertex2f(lon + w, lat);
	    	glVertex2f(lon + w, lat + h);
	    	glVertex2f(lon, lat + h);
	    }
	    glEnd();
	}

#ifdef ROTATE_NORMALS
	// If ROTATE_NORMALS is defined, then vertex normals will be
	// rotated to the correct orientation in the bucket's draw()
	// routine.
	//     printf("TileNew: rotating normals\n");
#else
	// If ROTATE_NORMALS is not defined, then we just rotate the light
	// source.
	//
	// To visualize this, consider looking in the default camera
	// orientation (along the negative z-axis, with the positive
	// y-axis 'up', and so the positive x-axis is right).  In our
	// world coordinates, this means looking down from the north pole,
	// with our head pointing to 90 degrees east (the Indian Ocean),
	// and our butt pointing to 90 degrees west (Lake Superior).  What
	// do we need to do to rotate to an arbitrary <lat, lon>?
	//
	// Consider moving to <-123, 37> (KSFO).  Our local coordinate
	// system is initially aligned with the world's.  First, we need
	// to rotate our local coordinate system 33 degrees clockwise
	// around our negative z-axis (a pin through our belly button), so
	// our butt is facing KSFO.  Rotations are specified around the
	// positive axis, so that corresponds to rotating it -33 degrees
	// clockwise around the positive z-axis.  This corresponds to 90.0
	// + lon.
	//
	// Then, we need to rotate it 53 degrees clockwise around the
	// positive y-axis (aligned with our right arm).  This corresponds
	// to 90.0 - lat.

	if (_lighting) {
	    // As a compromise, we rotate the light to be correct at the
	    // centre of the tile.  This compromise gets worse as we near
	    // the poles.
	    float cLat = _tile->centreLat();
	    float cLon = _tile->centreLon();
	    sgMat4 rot;
	    sgMakeRotMat4(rot, 90.0 + cLon, 90.0 - cLat, 0.0);
	    //     printf("TileNew: Not rotating normals\n");
	    sgXformVec3(lightPosition, rot);
	    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
	}
#endif

	// After all that work, the drawing is a bit anticlimactic.
	for (unsigned int i = 0; i < _buckets.size(); i++) {
	    _buckets[i]->draw();
	}

	// Clean up our matrices.
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	// Disconnect the framebuffer.
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	// At this point rbo (our multisampled renderbuffer) will have
	// the rendered scene, but not in a format that we can access
	// (eg, with glReadPixels()).  To use it, we need to resolve
	// the multiple samples to a single sample.  This is done via
	// a call to glBlitFramebuffer(), which requires another,
	// single-sampled, framebuffer (fbo), to which we attach our
	// texture _to.  The net result is that _to will contain the
	// full anti-aliased scene.
	GLuint fbo;
	glGenFramebuffersEXT(1, &fbo);
	assert(fbo != 0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);

	// Bind our texture (creating it if we haven't already).
	if (_to == 0) {
	    glGenTextures(1, &_to);
	    assert(_to != 0);

	    // Configure the texture.
	    glBindTexture(GL_TEXTURE_2D, _to);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
			    GL_LINEAR_MIPMAP_LINEAR);
	}
	glBindTexture(GL_TEXTURE_2D, _to);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, _width, _height, 0, GL_RGB, 
		     GL_UNSIGNED_BYTE, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
				  GL_COLOR_ATTACHMENT0_EXT,
				  GL_TEXTURE_2D,
				  _to,
				  0);
	assert(glGetError() == GL_NO_ERROR);
	assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == 
	       GL_FRAMEBUFFER_COMPLETE_EXT);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	// Okay, we're ready to go.  We read from our multisampled
	// framebuffer into our single-sampled framebuffer.
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, fboms);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, fbo);
	glBlitFramebufferEXT(0, 0, _width, _height, 
			     0, 0, _width, _height,
			     GL_COLOR_BUFFER_BIT, GL_LINEAR);
	glGenerateMipmapEXT(GL_TEXTURE_2D);
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);

	// Delete the renderbuffer and framebuffer objects.  In the
	// end we're left with the texture, _to.
	glDeleteRenderbuffersEXT(1, &rbo);
	glDeleteFramebuffersEXT(1, &fboms);
	glDeleteFramebuffersEXT(1, &fbo);
    }
    glPopAttrib();
}

// Saves the currently rendered map at the given size.  We assume that
// render() has been called.  Note that level must be <= maxLevel
// (given in the constructor).
void TileMapper::save(unsigned int level)
{
    if (!_palette || !_tile) {
	return;
    }

    // First, calculate the desired map size in pixels.
    int shrinkage = _maxLevel - level;
    int width = _width / (1 << shrinkage),
	height = _height / (1 << shrinkage);

    // With skinny tiles, the above calculation sometimes results in
    // zero-width tiles, so we correct it manually.
    if (width == 0) {
	width = 1;
    }

    // Grab the image.
    glPushAttrib(GL_TEXTURE_BIT); {
	glBindTexture(GL_TEXTURE_2D, _to);
	GLubyte *image = new GLubyte[width * height * 3];
	assert(image);
	glGetTexImage(GL_TEXTURE_2D, shrinkage, GL_RGB, GL_UNSIGNED_BYTE, 
		      image);

	// Save to a file.  We save it in _atlas/size/_name.<type> (if
	// that makes any sense).
	SGPath file = _tile->mapsDir();
	char str[3];
	snprintf(str, 3, "%d", level);
	file.append(str);
	file.append(_tile->name());
	if (_imageType == PNG) {
	    file.concat(".png");
	    savePNG(file.c_str(), image, width, height, _maximumElevation);
	} else if (_imageType == JPEG) {
	    file.concat(".jpg");
	    saveJPEG(file.c_str(), _JPEGQuality, 
		     image, width, height, _maximumElevation);
	}
    
	delete[] image;
    }
    glPopAttrib();
}

// Cleans things up - unloads buckets, resets the maximum elevation
// figure, and deletes our texture.  This should be called when
// finished with a tile.
void TileMapper::_unloadBuckets()
{
    for (unsigned int i = 0; i < _buckets.size(); i++) {
	delete _buckets[i];
    }
    _buckets.clear();
    _tile = NULL;

    // This might be overkill, but presumably if we're unloading
    // buckets, that means we can no longer trust the maximum
    // elevation figure.
    _maximumElevation = Bucket::NanE;
}
