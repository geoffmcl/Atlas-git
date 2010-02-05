/*-------------------------------------------------------------------------
  TileMapper.cxx

  Written by Brian Schack, started March 2009.

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
  ---------------------------------------------------------------------------*/

#include <cassert>
#include <stdexcept>

#include <plib/pu.h>
#include <simgear/misc/sg_path.hxx>

#include "TileMapper.hxx"
#include "Image.hxx"

using namespace std;

const float TileMapper::MIN_ELEVATION;

TileMapper::TileMapper(Palette *p, bool discreteContours, bool contourLines,
		       sgVec4 light, bool lighting, bool smoothShading):
    _palette(p), _discreteContours(discreteContours), 
    _contourLines(contourLines), _lightPosition(light), 
    _lighting(lighting), _smoothShading(smoothShading)
{
    // We must have a palette.
    if (!_palette) {
	throw runtime_error("palette");
    }

    Bucket::palette = _palette;
    Bucket::discreteContours = _discreteContours;
    Bucket::contourLines = _contourLines;
}

TileMapper::~TileMapper()
{
    _unloadBuckets();
}

void TileMapper::set(TileInfo *t)
{
    // Remove any old information we have;
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
}

// Draws the tile at the given size.  Takes responsibility for setting
// up the OpenGL view and drawing parameters.
void TileMapper::draw(unsigned int size)
{
    assert(glGetError() == GL_NO_ERROR);

    // Set up the view.  First, calculate the desired map size in
    // pixels and set our viewport correspondingly.

    // EYE - we also need to see if our current buffer is big enough.
    // This is where we could add the tiling code.
    int width, height;
    _tile->mapSize(size, &width, &height);
    glViewport(0, 0, width, height);

    // When our buckets were loaded, all points were converted to lat,
    // lon.  We need to stretch them to fill the buffer correctly.
    int lat = _tile->lat();
    int lon = _tile->lon();
    int w = _tile->width();
    int h = _tile->height();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(lon, lon + w, lat, lat + h);

    // No model or view transformations.
    glMatrixMode(GL_MODELVIEW);
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
	glDisable(GL_DEPTH_TEST);
	glBegin(GL_QUADS); {
	    glColor4fv(c);
	    glNormal3f(0.0, 0.0, 1.0);

	    glVertex2f(lon, lat);
	    glVertex2f(lon, lat + height);
	    glVertex2f(lon + width, lat + height);
	    glVertex2f(lon + width, lat);
	}
	glEnd();
    }

    // Now clear the depth buffer, enable the depth test, and render
    // the buckets.  Clearing the depth buffer ensures that buckets
    // will be drawn over the ocean we just drew at sea level, even
    // when they lie below sea level (as in the Dead Sea, for
    // example).
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

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
}

// Saves the currently rendered map at the given size.  Note that the
// rendered map doesn't have to have the same size as the saved map.
// This can be useful if, say, we want to do over-sampling.
void TileMapper::save(unsigned int level, ImageType t, unsigned int jpegQuality)
{
    // First, calculate the desired map size in pixels.
    int width, height;
    _tile->mapSize(level, &width, &height);

    // Now get the size of our buffer.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLint bufWidth = viewport[2];
    GLint bufHeight = viewport[3];

    assert(bufWidth >= width);
    assert(bufHeight >= height);

    // Grab the image and resample it if necessary.
    GLubyte *image = new GLubyte[bufWidth * bufHeight * 3];
    assert(image);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, bufWidth, bufHeight, GL_RGB, GL_UNSIGNED_BYTE, image);

    // Are we resampling?
    if (bufWidth > width) {
	assert(bufHeight > height);
	assert((bufWidth % width) == 0);
	assert((bufHeight % height) == 0);
	assert((bufWidth / width) == (bufHeight / height));

	// We do an in-place resampling.  Scary.
	int factor = bufWidth / width;
	int factorSquared = factor * factor;
	unsigned int newIndex = 0;
	for (int y = 0; y < height; y++) {
	    for (int x = 0; x < width; x++) {
		unsigned int rgb[3];
		rgb[0] = rgb[1] = rgb[2] = 0;
		for (int i = 0; i < factor; i++) {
		    unsigned int oldIndex = 
			((y * factor + i) * bufWidth + (x * factor)) * 3;
		    for (int j = 0; j < factor; j++) {
			rgb[0] += image[oldIndex++];
			rgb[1] += image[oldIndex++];
			rgb[2] += image[oldIndex++];
		    }
		}
		image[newIndex++] = rgb[0] / factorSquared;
		image[newIndex++] = rgb[1] / factorSquared;
		image[newIndex++] = rgb[2] / factorSquared;
	    }
	}
    }

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
}

void TileMapper::_unloadBuckets()
{
    for (unsigned int i = 0; i < _buckets.size(); i++) {
	delete _buckets[i];
    }
    _buckets.clear();

    // This might be overkill, but presumably if we're unloading
    // buckets, that means we can no longer trust the maximum
    // elevation figure.
    _maximumElevation = MIN_ELEVATION;
}
