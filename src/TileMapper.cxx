/*-------------------------------------------------------------------------
  TileMapper.cxx

  Written by Brian Schack, started March 2009.

  Copyright (C) 2009 - 2011 Brian Schack

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

#include <cassert>
#include <stdexcept>

#include <plib/pu.h>
#include <simgear/misc/sg_path.hxx>

#include "TileMapper.hxx"
#include "Image.hxx"

using namespace std;

TileMapper::TileMapper(Palette *p, unsigned int maxLevel, 
		       bool discreteContours, bool contourLines,
		       float azimuth, float elevation, bool lighting, 
		       bool smoothShading):
    _palette(p), _maxLevel(maxLevel), _discreteContours(discreteContours), 
    _contourLines(contourLines), _azimuth(azimuth), _elevation(elevation),
    _lighting(lighting), _smoothShading(smoothShading), _fbo(0), _to(0)
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
    glDeleteFramebuffersEXT(1, &_fbo);
}

// Tells TileMapper which tile is to be rendered.
void TileMapper::set(Tile *t)
{
    // Remove any old information we have.
    _unloadBuckets();

    _tile = t;
    if (_tile) {
	const vector<long int>* buckets = _tile->bucketIndices();
	for (unsigned int i = 0; i < buckets->size(); i++) {
	    long int index = buckets->at(i);

	    Bucket *b = new Bucket(_tile->sceneryDir(), index);
	    b->load(Bucket::RECTANGULAR);

	    _buckets.push_back(b);

	    if (b->maximumElevation() > _maximumElevation) {
		_maximumElevation = b->maximumElevation();
	    }
	}
    }

    // Silently reduce (if necessary) _maxLevel to a value we can
    // actually handle.

    // EYE - Does this really belong here?
    GLint textureSize = 0x1 << _maxLevel;
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
	fprintf(stderr, "Max level (%d) too big, reducing\n", _maxLevel);
	textureSize /= 2;
	_maxLevel--;
    };

    // Calculate the proper width and height for the given level.
    _tile->mapSize(_maxLevel, &_width, &_height);
}

// Draws the tile into a texture attached to a framebuffer.
void TileMapper::render()
{
    // EYE - remove this
    assert(glGetError() == GL_NO_ERROR);

    // Set up the framebuffer object, if it hasn't been done already.
    // We delay until now so that a TileMapper can be created without
    // having an OpenGL context.
    if (_fbo == 0) {
	glGenFramebuffersEXT(1, &_fbo);
    }

    // Bind the framebuffer so that all rendering goes to it.
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbo);

    // Create a texture object.
    // EYE - we also need to see if our current buffer is big enough.
    // This is where we could add the tiling code.
    // EYE - necessary to enable?  We don't actually do any texturing.
    // glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &_to);
    glBindTexture(GL_TEXTURE_2D, _to);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _width, _height, 0, GL_RGB, 
		 GL_UNSIGNED_BYTE, 0);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
    			      GL_COLOR_ATTACHMENT0_EXT,
    			      GL_TEXTURE_2D,
    			      _to,
    			      0);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == 
	   GL_FRAMEBUFFER_COMPLETE_EXT);

    // The framebuffer shares its state with the current context, so
    // we push some attributes so we don't step on current OpenGL
    // state.
    glPushAttrib(GL_VIEWPORT_BIT | GL_LIGHTING_BIT | GL_CURRENT_BIT); {
	// Set up the view.
	glViewport(0, 0, _width, _height);

	// When our buckets were loaded, all points were converted to
	// lat, lon.  We need to stretch them to fill the buffer
	// correctly.  Note that tiles use special latitudes (0 - 179)
	// and longitudes (0 - 359), so we need to adjust them to
	// "normal" values.

	// EYE - Do we really?  And should we have special accessors
	// in tiles?
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

	// Set up lighting.  The values used here must be the same as used
	// in Atlas if you want live scenery to match pre-rendered scenery
	// (the same goes for the palette used).
	sgVec4 lightPosition;
	const float BRIGHTNESS = 0.8;
	GLfloat diffuse[] = {BRIGHTNESS, BRIGHTNESS, BRIGHTNESS, 1.0f};
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

	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);

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
		glVertex2f(lon, lat + h);
		glVertex2f(lon + w, lat + h);
		glVertex2f(lon + w, lat);
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

	for (unsigned int i = 0; i < _buckets.size(); i++) {
	    // Buckets are smart enough to save a display list and used
	    // that if asked to be drawn more than once.  Therefore
	    // calling Tile::draw() more than once is relatively cheap, if
	    // the buckets have not been unloaded.
	    _buckets[i]->draw();
	}

	glFinish();

	assert(glGetError() == GL_NO_ERROR);

	// Create mimaps and unbind the framebuffer.  We should now have a
	// map of the appropriate size in the texture object _to.
	glGenerateMipmapEXT(GL_TEXTURE_2D);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	// EYE - before the unbinding or after?
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
    }
    glPopAttrib();
}

// Saves the currently rendered map at the given size.  We assume that
// render() has been called.  Note that level must be <= maxLevel
// (given in the constructor).
void TileMapper::save(unsigned int level, ImageType t, unsigned int jpegQuality)
{
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
    // EYE - should we worry about enabling textures, ...?
    // EYE - pushAttrib - pixelstorei, texture
    glBindTexture(GL_TEXTURE_2D, _to);
    GLubyte *image = new GLubyte[width * height * 3];
    assert(image);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, shrinkage, GL_RGB, GL_UNSIGNED_BYTE, image);

    // Save to a file.  We save it in _atlas/size/_name.<type> (if
    // that makes any sense).
    SGPath file = _tile->mapsDir();
    char str[3];
    snprintf(str, 3, "%d", level);
    file.concat(str);
    file.append(_tile->name());
    if (t == PNG) {
	file.concat(".png");
	savePNG(file.c_str(), image, width, height, _maximumElevation);
    } else if (t == JPEG) {
	file.concat(".jpg");
	saveJPEG(file.c_str(), jpegQuality, 
		 image, width, height, _maximumElevation);
    }
    
    delete[] image;
    glBindTexture(GL_TEXTURE_2D, 0);
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

    // This might be overkill, but presumably if we're unloading
    // buckets, that means we can no longer trust the maximum
    // elevation figure.

    // EYE - should be a constant somewhere - Tile?  Bucket::NanE?
    _maximumElevation = -std::numeric_limits<float>::max();

    // Delete the texture object.  We need to do this because
    // different tiles may have different sizes.
    glDeleteTextures(1, &_to);
}
