/*-------------------------------------------------------------------------
  Scenery.cxx

  Written by Brian Schack

  Copyright (C) 2008 - 2014 Brian Schack

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
// include file first.  We include gl.h in Scenery.hxx.  However, glew
// insists on being loaded before gl.h.
#include <GL/glew.h>

// Our include file
#include "Scenery.hxx"

// C++ system files
#include <cassert>

// Our project's include files
#include "AtlasWindow.hxx"
#include "AtlasController.hxx"
#include "Bucket.hxx"
#include "Geographics.hxx"
#include "Image.hxx"
#include "LayoutManager.hxx"

using namespace std;

// Drawing scenery is a little bit complex, mostly because of a desire
// to maintain reasonable response and performance.  We try to do only
// the minimum amount of work, and we try not to do too much at one
// time.
//
// During initialization, we do two things:
//
// (a) Find out what scenery we have, at what resolutions, and create
//     SceneryTile objects.
//
// (b) Tell the culler about the SceneryTile objects.
//
// Each time we are asked to draw, we do the following:
//
// (a) Calculate the desired display resolution (including if we want
//     live scenery).
//
// (b) Ask the culler for a list of visible tiles, and tell those
//     tiles they are visible.  The tiles will in turn inform the
//     relevant cache that they are visible.  The cache will begin
//     loading textures/scenery as required.
//
// (c) Tell the tiles to draw themselves.  The tiles will draw
//     themselves using the best possible existing texture, and live
//     scenery if we are live.

// A cache object containing a texture.
class MapTexture {
  public:
    MapTexture(const SGPath &f, int lat, int lon);
    ~MapTexture();

    // Load the texture, extracting its maximum elevation (embedded in
    // the file) if it has one.
    void load();
    void unload();
    bool loaded() const;
    unsigned int size() { return _t.size(); }

    float maximumElevation() { return _maxElevation; }

    void draw();

  protected:
    Texture _t;
    SGPath _f;
    int _lat, _lon;
    GLuint _dlist;		// Display list id to draw this texture.
    float _maxElevation;	// Maximum elevation of the map.
};

class SceneryTile: public Cullable, public CacheObject, Subscriber {
  public:
    SceneryTile(Tile *t, Scenery *s);
    ~SceneryTile();

    // Tells the SceneryTile to check its Tile object to see if maps
    // have been added or deleted.
    void update();

    // Draws a texture appropriate to the given level.
    void drawTexture(unsigned int level);
    // Draws bucket(s).
    void drawBuckets();
    // Labels the scenery tile or its buckets with MEFs (minimum
    // elevation figures).
    void label(double metresPerPixel, bool live);

    bool intersection(SGVec3<double> a, SGVec3<double> b, SGVec3<double> *c);
	
    // Cullable interface.
    void setBounds(atlasSphere& bounds) { _bounds = bounds; }
    const atlasSphere& bounds() { return _bounds; }
    double latitude() { return _ti->centreLat(); }
    double longitude() { return _ti->centreLon(); }

    // CacheObject interface.
    void calcDist(sgdVec3 centre);
    bool shouldLoad();
    bool load();
    bool unload();
    unsigned int size() { return _size; }

    // This will get called when we receive a notification.
    void notification(Notification::type n);

  protected:
    // Allocates and initializes the _buckets array, but doesn't load
    // buckets.  It can be called multiple times without incurring any
    // extra work.
    void _findBuckets();

    Tile *_ti;
    Scenery *_scenery;

    double _maxElevation;
    atlasSphere _bounds;

    // Maps at various resolutions.  Will be set to non-null at level
    // l in the constructor if the TileManager indicates there is a
    // map at that level.
    MapTexture *_textures[TileManager::MAX_MAP_LEVEL];
    vector<Bucket *> *_buckets;    // Buckets in this tile.

    // Returns the level of the best available texture nearest
    // 'level', whether it has been loaded or not.  If 'loaded' is
    // true, it returns the level of the best texture nearest 'level'
    // that has already been loaded.
    unsigned int _calcBest(unsigned int level, bool loaded = false);
    // The following are set in shouldLoad() and used in load().  They
    // record what work we need to do at the behest of the cache.
    unsigned int _mapToBeLoaded;	 // Map to be loaded.
    vector<Bucket *> _bucketsToBeLoaded;  // Current buckets to be loaded.
    // Sets _size, which is our approximation of how big we are in
    // bytes (this figure is used by the cache).
    void _calcSize();
    unsigned int _size;
};

GLubyte Texture::__defaultImage[Texture::__defaultSize][Texture::__defaultSize][3];
GLuint Texture::__defaultTexture = 0;

Texture::Texture(): _name(0), _size(0)
{
}

Texture::~Texture()
{
    unload();
}

// Loads the given file, which is assumed to be a JPEG or PNG file.
// On success, loaded() will return true.
void Texture::load(SGPath f, float *maximumElevation)
{
    GLubyte *data;
    int width, height, depth;

    // Clear any existing data.
    unload();
    assert(_name == 0);

    // Load the data.
    if (f.extension() == "jpg") {
	data = (GLubyte *)loadJPEG(f.c_str(), &width, &height, &depth, 
				   maximumElevation);
    } else if (f.extension() == "png") {
	data = (GLubyte *)loadPNG(f.c_str(), &width, &height, &depth,
				  maximumElevation);
    } else {
	// EYE - we should not have to guess this - the full pathname
	// should be passed in (as a const char *, I might add).
	f.concat(".jpg");
	data = (GLubyte *)loadJPEG(f.c_str(), &width, &height, &depth, 
				   maximumElevation);
	if (!data) {
	    f.set(f.base());
	    f.concat(".png");
	    data = (GLubyte *)loadPNG(f.c_str(), &width, &height, &depth,
				      maximumElevation);
	}
    }
    if (data == NULL) {
	return;
    }
    _size = width * height * depth;

    // Create the texture.
    glGenTextures(1, &_name);
    assert(_name > 0);

    glBindTexture(GL_TEXTURE_2D, _name);

    // Standard pixelized texture.
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//     //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);

    // Smoothed, but with fade-offs at the edges.
    //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // Smoothed, no fade-offs, but a sharp border between tiles.  This
    // is not apparent until zoomed pretty close.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // Note that because we specify a mipmap version of this filter,
    // we *must* do mipmapping.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    if (depth == 3) {
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height,
		     0, GL_RGB, GL_UNSIGNED_BYTE, data);
    } else {
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
		     0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }
    // EYE - glGenerateMipmapEXT() should exist if the
    // GL_EXT_framebuffer_object extension exists.  As of OpenGL 3.0,
    // we can also use glGenerateMipmap().  Note that we may need to
    // add a call to glEnable(GL_TEXTURE_2D) to compensate for a bug
    // in ATI cards (although I didn't experience any problems on
    // mine).  See:
    //
    // www.opengl.org/wiki/Common_Mistakes#Automatic_mipmap_generation
    //
    // For more.
    glGenerateMipmapEXT(GL_TEXTURE_2D);

    delete []data;
}

void Texture::unload()
{
    if (loaded()) {
	glDeleteTextures(1, &_name);
	_name = 0;
	_size = 0;
    }
}

// Returns our texture (name).  If _name is 0 (ie, load() wasn't
// called, or it had an error), we substitute a default texture, which
// is a red and white checkerboard.
GLuint Texture::name() const
{
    if (loaded()) {
    	return _name;
    } else {
	// Has our default texture been initialized?
	if (__defaultTexture == 0) {
	    // Nope.  Create it.
	    for (int i = 0; i < __defaultSize; i ++) {
		for (int j = 0; j < __defaultSize; j++) {
		    bool c = (((i & 0x1) == 0) ^ ((j & 0x1) == 0));
		    if (c) {
			// Red square
			__defaultImage[i][j][0] = 255;
			__defaultImage[i][j][1] = 0;
			__defaultImage[i][j][2] = 0;
		    } else {
			// White square
			__defaultImage[i][j][0] = 255;
			__defaultImage[i][j][1] = 255;
			__defaultImage[i][j][2] = 255;
		    }
		}
	    }

	    glGenTextures(1, &__defaultTexture);
	    assert(__defaultTexture != 0);
	    glBindTexture(GL_TEXTURE_2D, __defaultTexture);

	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 
			 __defaultSize, __defaultSize, 0, 
			 GL_RGB, GL_UNSIGNED_BYTE, __defaultImage);
	    // EYE - generate a mipmap?
	}

	return __defaultTexture;
    }
}

// Creates a texture cache object.  It's redundant to give the path
// and the lat and lon, because the lat and lon can be extracted from
// the path name, but it makes our life easier.
MapTexture::MapTexture(const SGPath &f, int lat, int lon): 
    _f(f), _lat(lat), _lon(lon), _dlist(0), _maxElevation(Bucket::NanE)
{
}

MapTexture::~MapTexture()
{
    unload();
}

void MapTexture::draw()
{
    // Don't draw ourselves if the texture hasn't been loaded yet.
    if (!_t.loaded()) {
    	return;
    }

    if (_dlist == 0) {
	_dlist = glGenLists(1);
	assert(_dlist != 0);

	glNewList(_dlist, GL_COMPILE);

	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

	// A few notes on textures for my benefit: 
	//
	// Textures have state.  Texture state for the *current*
	// texture is set by glTexParameter().  Texture units also
	// have state.  The state for the *current* texture unit is
	// set by glTexEnv().
	//
	// The current texture is set with glBindTexture().
	//
	// glEnable(GL_TEXTURE_2D) works on the current texture
	// *unit*, and just tells OpenGL to grab texels, and that they
	// should be from the 2D texture attached to the current unit
	// (a texture unit can have up to 4 textures attached to it -
	// 1D, 2D, 3D, and 4D.  There is really no good reason for
	// this; it's just part of the spec).
	//
	// The current texture unit is set with glActiveTexture() (by
	// default, it is texture unit 0).
	glBindTexture(GL_TEXTURE_2D, _t.name());

	// It's natural to want to use a GL_QUAD_STRIP, but we can't
	// be sure that the four corners of a 1 degree by 1 degree
	// "square" on the earth are co-planar.
	glBegin(GL_TRIANGLE_STRIP); {
	    int width = Tile::width(_lat + 90);
	    double s, n;
	    s = _lat;
	    n = _lat + 1;
	    // "Pin" the texture at 1 degree intervals (for most,
	    // this just means the 4 corners, but at extreme
	    // latitudes, it means points along the upper and
	    // lower edges as well).
	    for (int i = 0; i <= width; i++) {
		double w = _lon + i;

		// Note that the texture is loaded "upside-down" (the
		// first row of the image is y = 0.0 of the texture).
		glTexCoord2f(i / (float)width, 0.0); geodVertex3f(n, w);
		glTexCoord2f(i / (float)width, 1.0); geodVertex3f(s, w);
	    }
	}
	glEnd();
	glDisable(GL_TEXTURE_2D);

	glEndList();
    }

    glCallList(_dlist);
}

void MapTexture::load()
{
    // Load the file.  Map files can have the map's maximum elevation
    // embedded in them as a text comment, so extract it if it exists.
    _t.load(_f, &_maxElevation);
}

void MapTexture::unload()
{
    _t.unload();
    if (_dlist > 0) {
	glDeleteLists(_dlist, 1);
	_dlist = 0;
    }
}

bool MapTexture::loaded() const
{
    return _t.loaded();
}

SceneryTile::SceneryTile(Tile *ti, Scenery *s): 
    _ti(ti), _scenery(s), _maxElevation(Bucket::NanE), _buckets(NULL)
{
    // Create a texture object for each level at which we have maps.
    // EYE - since we only do this at creation, we won't notice new scenery
    for (unsigned int i = 0; i < _ti->mapLevels().size(); i++) {
	_textures[i] = (MapTexture *)NULL;
    }
    update();

    // Subscribe to the discrete/smooth contour change and palette
    // change notifications.  When we get either, we'll tell our
    // buckets.
    subscribe(Notification::DiscreteContours);
    subscribe(Notification::Palette);
}

SceneryTile::~SceneryTile()
{
    for (unsigned int i = 0; i < TileManager::MAX_MAP_LEVEL; i++) {
	if (_textures[i]) {
	    delete _textures[i];
	}
    }

    if (_buckets != NULL) {
	for (unsigned int i = 0; i < _buckets->size(); i++) {
	    delete (*_buckets)[i];
	}
	delete _buckets;
    }
}

// Update our _textures array to match our tile.
void SceneryTile::update()
{
    const bitset<TileManager::MAX_MAP_LEVEL>& missing = _ti->missingMaps();
    for (unsigned int i = 0; i < _ti->mapLevels().size(); i++) {
	if (_ti->mapLevels()[i] && !missing[i]) {
	    // Remove the old texture if it exists.  This is a bit
	    // wasteful, since it might be perfectly valid, but we
	    // need to guard against the case where a map was
	    // re-rendered in a different style.
	    if (_textures[i]) {
		delete _textures[i];
	    }

	    char str[3];
	    sprintf(str, "%d", i);

	    SGPath f = _ti->mapsDir();
	    f.append(str);
	    f.append(_ti->name());
	    _textures[i] = new MapTexture(f, _ti->lat(), _ti->lon());
	} else if (_textures[i]) {
	    delete _textures[i];
	    _textures[i] = (MapTexture *)NULL;
	}
    }
}

// Called by the cache, asking us to Set _dist, the distance from us
// to centre.
void SceneryTile::calcDist(sgdVec3 centre)
{
    _dist = sgdDistanceSquaredVec3(centre, _bounds.center);
}

// Called by the cache when we are added to it.  We return true if we
// need to load something.  We also prepare ourselves to load buckets
// if required.
bool SceneryTile::shouldLoad()
{
    bool result = false;

    // Calculate what map to load.  If there's nothing to load, set it
    // to MAX_MAP_LEVEL.
    _mapToBeLoaded = _calcBest(_scenery->level());
    if (_mapToBeLoaded != TileManager::MAX_MAP_LEVEL) {
	// We need to load a map if it exists and it hasn't been
	// loaded already.
	if (_textures[_mapToBeLoaded] && !_textures[_mapToBeLoaded]->loaded()) {
	    result = true;
	} else {
	    _mapToBeLoaded = TileManager::MAX_MAP_LEVEL;
	}
    }

    if (_scenery->live()) {
	// Make sure we know what our buckets are.
	_findBuckets();

	// Schedule some buckets for loading.  We load a bucket if:
	// (a) there is one, (b) it hasn't been loaded, and (c) it is
	// within the viewing frustum.
	_bucketsToBeLoaded.clear();

	for (unsigned int i = 0; i < _buckets->size(); i++) {
	    Bucket *b = (*_buckets)[i];
	    if ((b != NULL) && !b->loaded() && 
		_scenery->frustum()->intersects(b->bounds())) {
		_bucketsToBeLoaded.push_back(b);
		result = true;
	    }
	}
    }

    return result;
}

// Load a map and/or some buckets.  This is called from a cache, after
// the call to shouldLoad(), where _mapToBeLoaded and
// _bucketsToBeLoaded were set.  In the interests of responsiveness,
// we only do a bit of work (ie, loading the map or one bucket) per
// call.  We return true when everything has been loaded.
bool SceneryTile::load()
{
    if (_mapToBeLoaded != TileManager::MAX_MAP_LEVEL) {
	_textures[_mapToBeLoaded]->load();
	_calcSize();

	// Set our maximum elevation figure if it hasn't been set
	// already.
	if (_maxElevation == Bucket::NanE) {
	    _maxElevation = _textures[_mapToBeLoaded]->maximumElevation();
	}

	_mapToBeLoaded = TileManager::MAX_MAP_LEVEL;

	// Tell others that new scenery has been loaded.
	Notification::notify(Notification::NewScenery);

	// If we still have buckets to load, tell the cache we're not
	// done.
	return _bucketsToBeLoaded.empty();
    }

    if (!_bucketsToBeLoaded.empty()) {
	Bucket *b = _bucketsToBeLoaded.back();
	_bucketsToBeLoaded.pop_back();
	b->load();
	_calcSize();

	// Tell others that new scenery has been loaded.
	Notification::notify(Notification::NewScenery);

	return _bucketsToBeLoaded.empty();
    }

    // If we get here, we're done.
    return true;
}

// Unload our textures and buckets.  This is called from a cache.  We
// always unload everything in a single call.
bool SceneryTile::unload()
{
    for (unsigned int i = 0; i < TileManager::MAX_MAP_LEVEL; i++) {
	if (_textures[i]) {
	    _textures[i]->unload();
	}
    }
    
    if (_buckets != NULL) {
	for (unsigned int i = 0; i < _buckets->size(); i++) {
	    (*_buckets)[i]->unload();
	}
    }

    // We could just set _size to 0, but this seems cleaner.
    _calcSize();

    return true;
}

void SceneryTile::_calcSize()
{
    _size = 0;
    for (unsigned int i = 0; i < TileManager::MAX_MAP_LEVEL; i++) {
	if (_textures[i]) {
	    _size += _textures[i]->size();
	}
    }
    
    if (_buckets != NULL) {
	for (unsigned int i = 0; i < _buckets->size(); i++) {
	    _size += (*_buckets)[i]->size();
	}
    }
}

// Draws the texture that best matches the given level, where "best"
// means the first texture we find at this level or below.  Failing
// that, we choose the first one we find above this level.
void SceneryTile::drawTexture(unsigned int level)
{
    unsigned int best = _calcBest(level, true);
    if (best != TileManager::MAX_MAP_LEVEL) {
	_textures[best]->draw();
    }
}

// Draw the buckets in the tile that are within the scenery culler's
// frustum.
void SceneryTile::drawBuckets()
{
    if (_buckets == NULL) {
	// If we haven't loaded our buckets yet, just return.
	return;
    }

    for (unsigned int i = 0; i < _buckets->size(); i++) {
	Bucket *b = (*_buckets)[i];
	if ((b != NULL) && 
	    (b->loaded()) && 
	    _scenery->frustum()->intersects(b->bounds())) {
	    b->draw();
	}
    }
}

// Called when the lighting changes.
void SceneryTile::notification(Notification::type n)
{
    if (n == Notification::DiscreteContours) {
	// Switched between smooth and discrete contours.
	if (_buckets) {
	    for (unsigned int i = 0; i < _buckets->size(); i++) {
		_buckets->at(i)->discreteContoursChanged();
	    }
	}
    } else if (n == Notification::Palette) {
	// Got a new palette.
	if (_buckets) {
	    for (unsigned int i = 0; i < _buckets->size(); i++) {
		_buckets->at(i)->paletteChanged();
	    }
	}
    } else {
	assert(false);
    }
}

// Draw the maximum elevation figure (MEF) at the given location and
// scale.
static void _label(int mef, double lat, double lon, double metresPerPixel,
		   atlasFntTexFont *fnt)
{
    // EYE - magic numbers
    const float thousandsSize = 36.0 * metresPerPixel;
    const float hundredsSize = 24.0 * metresPerPixel;

    int thousands, hundreds;
    thousands = mef / 1000;
    hundreds = mef % 1000 / 100;

    // Create the label.
    LayoutManager lm;
    lm.begin(); {
	static AtlasString str;
	str.printf("%d", thousands);
	lm.setFont(fnt, thousandsSize);
	lm.addText(str.str());

	str.printf(" %d", hundreds);
	// This is how we fake a superscript - we use a smaller font,
	// then draw it a bit closer (-1/3) and higher (1/2).
	lm.setPointSize(hundredsSize);
	lm.addText(str.str(), -hundredsSize / 3.0, hundredsSize / 2.0);
    }
    lm.end();
    
    // EYE - magic "number"
    glColor4f(0.0, 0.0, 0.5, 0.5);

    // Most of the time we could just use the bucket's bounding sphere
    // centre, but at high latitudes this doesn't work - the centre of
    // the bounding sphere may not even lie within the bucket.  So we
    // use its latitude and longitude instead.
    geodDrawText(lm, lat, lon);
}

// Labels the tiles or buckets with their maximum elevation figure.

// EYE - this should be generalized - label any region of a certain
// size (in pixels).  When we zoom in, we should go down to
// sub-buckets, when we zoom out we should go to tile clusters.
// Because of this, the label() facility doesn't really belong
// (exclusively) at the Tile level.
void SceneryTile::label(double metresPerPixel, bool live)
{
    if (live) {
	// If we're live, we label each bucket individually.
	if (_buckets == NULL) {
	    // EYE - scary.  Should we label the tile if we have no
	    // buckets, even if we're live?
	    return;
	}
	for (unsigned int i = 0; i < _buckets->size(); i++) {
	    Bucket *b = (*_buckets)[i];
	    // Label this bucket if it's loaded and visible.
	    if ((b != NULL) && 
		(b->loaded()) && 
		_scenery->frustum()->intersects(b->bounds())) {
		int mef = MEF(b->maximumElevation());
		_label(mef, b->centreLat(), b->centreLon(), metresPerPixel,
		       _scenery->win()->regularFont());
	    }
	}
    } else {
	// We're not live, so label the tile as a whole.
	if (_maxElevation != Bucket::NanE) {
	    int mef = MEF(_maxElevation);
	    double lat = _ti->lat() + 0.5;
	    double lon = _ti->lon() + _ti->width() / 2.0;
	    _label(mef,  lat, lon, metresPerPixel, 
		   _scenery->win()->regularFont());
	}
    }
}

// Returns true if the ray given by a and b intersects a bucket in
// this tile.  The point of intersection is given in c.  Note that if
// no buckets are loaded, we just return false.
bool SceneryTile::intersection(SGVec3<double> a, SGVec3<double> b, 
			       SGVec3<double> *c)
{
    if (_buckets == NULL) {
	return false;
    }

    for (unsigned int i = 0; i < _buckets->size(); i++) {
	if ((*_buckets)[i]->intersection(a, b, c)) {
	    return true;
	}
    }

    return false;
}

// This requests that we fill our _buckets array.  If we've done so
// already, we just return immediately.  Otherwise we search the
// scenery directory for the buckets that compose this tile, adding
// them to the _buckets array.  The actual file data is not loaded at
// this point; we merely make a note of each bucket's existence,
// location, and name.
void SceneryTile::_findBuckets()
{
    if (_buckets != NULL) {
	return;
    }

    _buckets = new vector<Bucket *>;

    vector<long int> indices;
    _ti->bucketIndices(indices);
    for (unsigned int i = 0; i < indices.size(); i++) {
	_buckets->push_back(new Bucket(_ti->sceneryDir(), indices[i]));
    }
}

// Returns the map level which best matches the given level, where
// "best matches" is defined as "at this level or higher if we've got
// it, else the closest lower level."  If 'loaded' is false (the
// default), then it doesn't check if the map has been loaded.  If
// 'loaded' is true, it only considers already-loaded maps.
unsigned int SceneryTile::_calcBest(unsigned int level, bool loaded) 
{
    // First look at this level or above (higher resolutions).
    for (unsigned int l = level; l < _ti->mapLevels().size(); l++) {
    	if (_textures[l] && (!loaded || _textures[l]->loaded())) {
    	    return l;
    	}
    }

    // If we found none above this level, then look below (lower
    // resolution).  Note the seemingly backwards termination test.
    // Why?  The loop variable 'l' is an unsigned int.  We want to
    // test it for all values down to 0.  However, if it's 0 and we
    // subtract 1, it will become very large.  Thus the test.  Tricky
    // (and scary).
    for (unsigned int l = level; l < _ti->mapLevels().size(); l--) {
    	if (_textures[l] && (!loaded || _textures[l]->loaded())) {
    	    return l;
    	}
    }
    
    // None found at all.  Return TileManager::MAX_MAP_LEVEL.
    return TileManager::MAX_MAP_LEVEL;
}

// Creates a Scenery object.  We assume that all scenery will be
// displayed in the given window.
Scenery::Scenery(AtlasWindow *aw): 
    _aw(aw), _dirty(true), _level(TileManager::MAX_MAP_LEVEL), _live(false), 
    _tm(_aw->ac()->tileManager()), _levels(_tm->mapLevels()), _cache(_aw->id())
{
    // Create a culler and a frustum searcher for it.
    _culler = new Culler();
    _frustum = new Culler::FrustumSearch(*_culler);

    // Create scenery tiles.  We only care about tiles that have been
    // downloaded, regardless of whether any maps have been generated
    // for them or not.
    TileIterator i(_tm, TileManager::DOWNLOADED);
    for (Tile *ti = i.first(); ti; ti = i++) {
	// Create a scenery tile.
	SceneryTile *tile = new SceneryTile(ti, this);

	// Add bounds information about this tile to our Culler
	// object.
	int lat = ti->lat(), lon = ti->lon(), width = ti->width();
	atlasSphere bounds;
	// We set the bounds using 6 points: the 4 corners, plus
	// the middle of the north and south edges.  The last two
	// are added because tiles at high latitudes are very
	// non-rectangular (becoming doughnuts at the poles);
	// adding the extra two points gives a better bounding
	// sphere.
	bounds.extendBy(lat, lon); // west corners
	bounds.extendBy(lat + 1, lon);
	bounds.extendBy(lat, lon + width); // east corners
	bounds.extendBy(lat + 1, lon + width);
	bounds.extendBy(lat, lon + width / 2.0); // middle
	bounds.extendBy(lat + 1, lon + width / 2.0);

	tile->setBounds(bounds);

	_tiles[ti] = tile;
	_culler->addObject(tile);
    }
}

Scenery::~Scenery()
{
    map<Tile *, SceneryTile *>::const_iterator i;
    for (i = _tiles.begin(); i != _tiles.end(); i++) {
	delete i->second;
    }
    _tiles.clear();

    delete _frustum;
    delete _culler;
}

void Scenery::move(const sgdMat4 modelViewMatrix, const sgdVec3 eye)
{
    _frustum->move(modelViewMatrix);
    sgdCopyVec3(_eye, eye);
    _dirty = true;
}

// Called when the user zooms in or out.  We update the frustum
// searcher, set ourselves dirty, and calculate a new best texture
// level, and set _live to true or false.
void Scenery::zoom(const sgdFrustum& frustum, double metresPerPixel)
{
    _frustum->zoom(frustum.getLeft(), frustum.getRight(), 
		   frustum.getBot(), frustum.getTop(),
		   frustum.getNear(), frustum.getFar());
    _dirty = true;
    _metresPerPixel = metresPerPixel;

    // Calculate the ideal level.  We calculate the height in pixels
    // of a map tile at the current zoom level.  We then take the
    // base-2 log to get the "level".  Because idealLevel is unsigned,
    // we have to make sure it isn't assigned a negative value (which
    // will be cast into a very large positive value).
    unsigned int idealLevel = 
    	max(ceil(log2(SGGeodesy::EQURAD * SGD_PI / 180.0 / metresPerPixel)),
    	    0.0);

    // Find the best matching level.  We define "best" to be the
    // closest level greater than or equal to the ideal level.  If
    // none exists (ie, we have no maps as detailed as the ideal
    // level), then choose the closest level less than the ideal level
    // (ie, our most detailed resolution), and set the _live flag to
    // true.
    _live = false;
    for (_level = idealLevel; _level < _levels.size(); _level++) {
	if (_levels[_level]) {
	    return;
	}
    }

    // We have no maps detailed enough.  We set live to true, but in
    // the interests of performance, we only do so if we're not zoomed
    // out too far.
    // EYE - magic number
    if (metresPerPixel < 125.0) {
	_live = true;
    }

    // EYE - use TileManager::MAX_MAP_LEVEL?
    if (idealLevel >= _levels.size()) {
	idealLevel = _levels.size() - 1;
    }

    // Note the strange termination test.  We can't test an unsigned
    // int for being negative - when it's zero and then decremented,
    // it instead becomes very large.  So, we just test if it's still
    // less than the size of the _levels bitset.
    for (_level = idealLevel - 1; _level < _levels.size(); _level--) {
	if (_levels[_level]) {
	    return;
	}
    }
}

void Scenery::draw(bool lightingOn)
{
    // We assume that when called, the depth test is on and lighting
    // is off.
    assert(glIsEnabled(GL_DEPTH_TEST) && !glIsEnabled(GL_LIGHTING));

    // Has our view of the world changed?
    if (_dirty) {
	// Yes.  Update our idea of what to display, ask the culler
	// for visible tiles, and tell the cache.
	_cache.reset(_eye);

	// Now ask the culler for all visible tiles, and add them to
	// the cache for loading.
	const vector<Cullable *>& intersections = _frustum->intersections();
	for (unsigned int i = 0; i < intersections.size(); i++) {
	    SceneryTile *t = dynamic_cast<SceneryTile *>(intersections[i]);
	    if (!t) {
		continue;
	    }

	    _cache.add(t);
	}

	// Now start the cache.
	_cache.go();

	_dirty = false;
    }

    // Our strategy is:
    //
    // (a) Draw tile textures at the appropriate resolution.  We use
    //     the culler to decide what tiles to draw.
    //
    // (b) Draw scenery tiles on top of the textures if we're very
    //     close.  Note that scenery tiles do not replace textures,
    //     for 2 reasons: (a) we may not have loaded all the desired
    //     scenery yet, and (b) a tile covers a complete 1x1 (or
    //     bigger) area, but the scenery of which it is composed may
    //     only cover part of that area (because part of it may be
    //     open ocean, or otherwise have no scenery).

    // Draw textures.
    const vector<Cullable *>& intersections = _frustum->intersections();
    for (unsigned int i = 0; i < intersections.size(); i++) {
    	SceneryTile *t = dynamic_cast<SceneryTile *>(intersections[i]);
    	if (!t) {
    	    continue;
    	}
    	t->drawTexture(_level);
    }

    // Render "live" scenery too if we're zoomed in close enough.
    if (_live) {
	if (lightingOn) {
	    glEnable(GL_LIGHTING);
	}

	// Clear depth buffer.  Generally scenery tiles will be in
	// front of textures, because textures are drawn at sea level
	// and scenery is above sea level, so this usually isn't
	// necessary.  However, in some places, like the Dead Sea, the
	// scenery is below sea level.  By clearing the depth buffer,
	// we ensure that the scenery will be drawn over anything
	// that's already there.
	glClear(GL_DEPTH_BUFFER_BIT);

	for (unsigned int i = 0; i < intersections.size(); i++) {
	    SceneryTile *t = dynamic_cast<SceneryTile *>(intersections[i]);
	    if (!t) {
		continue;
	    }
	    t->drawBuckets();
	}

	glDisable(GL_LIGHTING);
    }

    if (_MEFs) {
	_label(_live);
    }
}

// Tells us that the tile's status has changed.  We find the
// corresponding SceneryTile and pass the message on and set _dirty to
// true.
void Scenery::update(Tile *t)
{
    // EYE - check to make sure there's an entry for the tile?
    _tiles[t]->update();
    _dirty = true;
}

// Labels the scenery (which means just adding an elevation figure on
// each live scenery bucket).  We assume that draw() has been called
// previously, and don't have to worry about any _dirty business.
void Scenery::_label(bool live)
{
    // EYE - put into a display list, only draw if _dirty?

    // As a heuristic, we only draw them on tiles that are at least
    // 256 pixels high.
    // EYE - magic number!  
    if ((SGGeodesy::EQURAD * SGD_PI / 180.0 / _metresPerPixel) < 256) {
	return;
    }

    // This should have been taken care of in draw().
    assert(!_dirty);

    // We assume that when called, the depth test is on, and lighting
    // is off.
    assert(glIsEnabled(GL_DEPTH_TEST) && !glIsEnabled(GL_LIGHTING));

    // Labels must be written on top of whatever scenery is there, so
    // we ignore depth values.
    glPushAttrib(GL_DEPTH_BUFFER_BIT); {
	glDisable(GL_DEPTH_TEST);

	// Draw elevation figures.
	const vector<Cullable *>& intersections = _frustum->intersections();
	for (unsigned int i = 0; i < intersections.size(); i++) {
	    SceneryTile *t = dynamic_cast<SceneryTile *>(intersections[i]);
	    if (!t) {
		continue;
	    }
	    t->label(_metresPerPixel, live);
	}
    }
    glPopAttrib();
}

// Returns the cartesian coordinates of the world at the screen x, y
// point.  Returns true if there is an intersection (in which case c
// contains the intersection coordinates), false otherwise.  If we
// have an elevation value for the intersection then validElevation
// will be set to true (live scenery provides us with elevation
// information, but scenery textures do not).
//
// We calculate the intersection in two different ways, depending on
// if we have live scenery or not.  With live scenery, we find out
// where the ray intersects the scenery, returning the cartesian
// coordinates of that point.  With textures, we intersect with an
// idealized earth ellipsoid.
//
// x and y are window coordinates which represent a point, not a
// pixel.  For example, if a window is 100 pixels wide and 50 pixels
// high, the lower right *pixel* is (99, 49).  However, the *point*
// (99.0, 49.0) is the top left corner of that pixel.  The lower right
// corner of the entire window is (100.0, 50.0).  If you are calling
// this with a mouse coordinate, you probably should add 0.5 to both
// the x and y coordinates, which is the centre of the pixel the mouse
// is on.
//
// Note as well that if we intersect with live scenery, the elevation
// value will be the maximum value in the 1x1 pixel area centred on
// (x, y).
bool Scenery::intersection(double x, double y, 
			   SGVec3<double> *c, bool *validElevation)
{
    // EYE - we should set the window ourselves (which means saving it
    // in the constructor).  Then this method could be called safely
    // from anywhere.
    GLint viewport[4];
    GLdouble mvmatrix[16], projmatrix[16];
    GLdouble wx, wy, wz;	// World x, y, z coords.

    // Make sure we're the active window.
    int oldWindow = _aw->set();

    // Our line is given by two points: the intersection of our
    // viewing "ray" with the near depth plane and far depth planes.
    // This assumes that we're using an orthogonal projection - in a
    // perspective projection, we'd use our eyepoint as one of the
    // points and the near depth plane as the other.
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projmatrix);
    // viewport[3] is height of window in pixels.  We need to convert
    // from window coordinates, where y increases down, to viewport
    // coordinates, where y increases up.
    y = viewport[3] - y;

    // Near depth plane intersection.
    gluUnProject (x, y, 0.0, mvmatrix, projmatrix, viewport, &wx, &wy, &wz);
    SGVec3<double> nnear(wx, wy, wz);

    // Far depth plane intersection.
    gluUnProject (x, y, 1.0, mvmatrix, projmatrix, viewport, &wx, &wy, &wz);
    SGVec3<double> ffar(wx, wy, wz);
    
    // If validElevation is not NULL, that means the caller is
    // interested in a valid elevation result, which means we must
    // query our live scenery.
    if (validElevation != NULL) {
	// For now, assume that no buckets intersect.
	*validElevation = false;

	// Later, we'll ask candidate buckets will redraw themselves
	// in select mode to see if there are any intersections.  We
	// define a new projection matrix containing the pick region,
	// which we define to be 1 pixel square.  When the buckets
	// draw themselves, they will only get results for this small
	// region.
	glMatrixMode(GL_PROJECTION);
	glPushMatrix(); {
	    glLoadIdentity();
	    gluPickMatrix(x, y, 1.0, 1.0, viewport);
	    glMultMatrixd(projmatrix);

	    // Now that we have our viewing ray (although, technically
	    // speaking, it should actually be a very narrow viewing
	    // frustum, but a ray is good enough), get the
	    // intersection.  We first ask each visible tile in turn
	    // if the ray intersects their live scenery, and, if it
	    // does, the elevation at that point.
	    const vector<Cullable *>& intersections = _frustum->intersections();
	    for (unsigned int i = 0; i < intersections.size(); i++) {
		SceneryTile *t = dynamic_cast<SceneryTile *>(intersections[i]);
		if (!t) {
		    continue;
		}
		if (t->intersection(nnear, ffar, c)) {
		    // Found one!  Since the tile found the
		    // intersection using one of its buckets (ie, live
		    // scenery), we know that the elevation value is
		    // valid.  We can also short-circuit our search.
		    *validElevation = true;
		    break;
		}
	    }
	    glMatrixMode(GL_PROJECTION);
	}
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
    }
    // Beyond this point, we don't do any OpenGL stuff, so we can
    // restore the old active window.
    _aw->set(oldWindow);

    // If the user was interested in getting an elevation and we
    // actually got one, we can return now.
    if ((validElevation != NULL) && *validElevation) {
	return true;
    }

    // If we got here, that means no tiles intersected or the user
    // isn't interested in an elevation.  So, we'll just use a simple
    // earth/ray intersection with a standard earth ellipsoid.  This
    // will give us the lat/lon, but the elevation will always be 0
    // (sea level).
    //
    // We stretch the universe along the earth's axis so that the
    // earth is a sphere.  This code assumes that the earth is centred
    // at the origin, and that the earth's axis is aligned with the z
    // axis, north positive.
    //
    // This website:
    //
    // http://mysite.du.edu/~jcalvert/math/ellipse.htm
    //
    // has a good explanation of ellipses, including the statement "The
    // ellipse is just this auxiliary circle with its ordinates shrunk in the
    // ratio b/a", where a is the major axis (equatorial plane), and b is
    // the minor axis (axis of rotation).
    assert((validElevation == NULL) || (*validElevation == false));
    SGVec3<double> centre(0.0, 0.0, 0.0);
    double mu1, mu2;
    nnear[2] *= SGGeodesy::STRETCH;
    ffar[2] *= SGGeodesy::STRETCH;
    if (RaySphere(nnear, ffar, centre, SGGeodesy::EQURAD, &mu1, &mu2)) {
	SGVec3<double> s1, s2;
	s1 = nnear + mu1 * (ffar - nnear);
	s2 = nnear + mu2 * (ffar - nnear);

	// Take the nearest intersection (the other is on the other
	// side of the world).
	if (dist(nnear, s1) < dist(nnear, s2)) {
	    // Unstretch the world.
	    s1[2] /= SGGeodesy::STRETCH;
	    *c = s1;
	} else {
	    // Unstretch the world.
	    s2[2] /= SGGeodesy::STRETCH;
	    *c = s2;
	}

	return true;
    } else {
	return false;
    }
}
