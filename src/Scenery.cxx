/*-------------------------------------------------------------------------
  Scenery.cxx

  Written by Brian Schack

  Copyright (C) 2008 Brian Schack

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

#include <OpenGL/gl.h>
#include <GLUT/glut.h>

#include <simgear/math/sg_geodesy.hxx>
#include <simgear/math/SGMisc.hxx>

#include <cassert>

using namespace std;

#include "Scenery.hxx"
#include "Bucket.hxx"
#include "Globals.hxx"
#include "Image.hxx"
#include "LayoutManager.hxx"

// A nonsensical elevation value.  This is used to represent an unset
// maximum elevation value for a scenery tile or bucket.
static const float NO_ELEVATION = -1e6;

// EYE - for debugging only
extern int main_window;

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

// EYE - put this in a class?
extern sgdVec3 eye;

// A cache object containing a texture.
class TextureCO: public CacheObject {
  public:
    TextureCO(const SGPath &f, int lat, int lon);
    ~TextureCO();

    // Load the texture, extracting its maximum elevation (embedded in
    // the file) if it has one.
    void load();
    void unload();
    bool loaded() const;

    float maximumElevation() { return _maxElevation; }

    void draw();

  protected:
    Texture _t;
    SGPath _f;
    int _lat, _lon;
    GLuint _dlist;		// Display list id to draw this texture.
    float _maxElevation;	// Maximum elevation of the map.
};

// A cache object containing a bucket.  It doesn't do much except act
// as a wrapper for a Bucket object, but adds CacheObject
// capabilities.
class BucketCO: public CacheObject {
  public:
    BucketCO(const SGPath& f, long int index);
    ~BucketCO();

    void load() { _b->load(); }
    void unload() { _b->unload(); }
    void setDirty() { _b->setDirty(); }

    Bucket *bucket() const { return _b; }

  protected:
    Bucket *_b;
};

// class SceneryTile: public Cullable {
class SceneryTile: public Cullable, Subscriber {
  public:
    SceneryTile(TileInfo *t);
    ~SceneryTile();

    void addTexture(unsigned int level, Cache *cache);
    void addBuckets(Cache *cache, Culler::FrustumSearch& frustum);

    // Draws a texture appropriate to the given level.
    void drawTexture(unsigned int level);
    // Draws bucket(s) within the culler's frustum.
    void drawBuckets(Culler::FrustumSearch& frustum);
    // Labels the scenery tile or its buckets with MEFs (minimum
    // elevation figures).
    void label(Culler::FrustumSearch& frustum, double metresPerPixel, 
	       bool live);

    void setCentre(const sgdVec3 centre);

    bool intersection(SGVec3<double> a, SGVec3<double> b,
		      SGVec3<double> *c);
	
    // Cullable interface.
    void setBounds(atlasSphere& bounds) { _bounds = bounds; }
    const atlasSphere& Bounds() { return _bounds; }
    double latitude() { return _ti->centreLat(); }
    double longitude() { return _ti->centreLon(); }

    // This will get called when we receive a notification.
    bool notification(Notification::type n);

  protected:
    void _findBuckets();

    TileInfo *_ti;
    const bitset<TileManager::MAX_MAP_LEVEL> &_levels;

    double _maxElevation;

    map<int, TextureCO *> _textures; // Maps at various resolutions.
    vector<BucketCO *> *_buckets;    // Buckets in this tile.

    atlasSphere _bounds;
};

Texture::Texture(): _name(0)
{
    assert(glutGetWindow() == main_window);
}

Texture::~Texture()
{
    assert(glutGetWindow() == main_window);
    unload();
}

void Texture::load(SGPath f, float *maximumElevation)
{
    GLubyte *data;
    int width, height;

    int oldWindow = glutGetWindow();
    if (oldWindow != main_window) {
	glutSetWindow(main_window);
    }
    assert(glutGetWindow() == main_window);

    // Clear any existing data.
    unload();
    assert(_name == 0);

    // Load the data.

    // EYE - we should not have to guess this - the full pathname
    // should be passed in (as a const char *, I might add).
    f.concat(".jpg");
    data = (GLubyte *)loadJPEG(f.c_str(), &width, &height, maximumElevation);
    if (!data) {
	f.set(f.base());
	f.concat(".png");
	data = (GLubyte *)loadPNG(f.c_str(), &width, &height, maximumElevation);
    }
    assert(data != NULL);

    // Create the texture.
    glGenTextures(1, &_name);
    assert(_name > 0);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

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
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height,
		 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    // This looks nicer, although it slows things down a bit.
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, width, height,
		      GL_RGB, GL_UNSIGNED_BYTE, data);

    delete []data;

    if (oldWindow != main_window) {
	glutSetWindow(oldWindow);
    }
}

void Texture::unload()
{
    assert(glutGetWindow() == main_window);
    if (loaded()) {
	glDeleteTextures(1, &_name);
	_name = 0;
    }
}

// Creates a texture cache object.  It's redundant to give the path
// and the lat and lon, because the lat and lon can be extracted from
// the path name, but it makes our life easier.
TextureCO::TextureCO(const SGPath &f, int lat, int lon): 
    _f(f), _lat(lat), _lon(lon), _dlist(0), _maxElevation(NO_ELEVATION)
{
}

TextureCO::~TextureCO()
{
    unload();
}

// This website:
//
// http://mysite.du.edu/~jcalvert/math/ellipse.htm
//
// has a good explanation of ellipses, including the statement "The
// ellipse is just this auxiliary circle with its ordinates shrunk in the
// ratio b/a", where a is the major axis (equatorial plane), and b is
// the minor axis (axis of rotation).

// EYE - put this in misc.cxx?

// Draw a vertex at the given lat/lon, given in degrees.
void geodVertex3f(float lat, float lon)
{
    sgdVec3 cart;

    atlasGeodToCart(lat, lon, 0.0, cart);
    // Since we don't use lighting for displaying textures, we don't
    // need to calculate a normal.
    glVertex3f(cart[0], cart[1], cart[2]);
}

void TextureCO::draw()
{
    // Don't draw ourselves if the texture hasn't been loaded yet.
    if (!_t.loaded()) {
	return;
    }

    if (_dlist == 0) {
	_dlist = glGenLists(1);
	assert(_dlist != 0);

	glNewList(_dlist, GL_COMPILE);

	// EYE - I added this glColor() to get rid of some strange
	// shading that appeared after I added TACANs.  This needs to
	// be checked.  Perhaps all this should be wrapped in state
	// 'push'.
	glColor3f(1.0, 1.0, 1.0);
	glEnable(GL_TEXTURE_2D);
// #ifndef LIGHTING
// 	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
// #else
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
// #endif

	glBindTexture(GL_TEXTURE_2D, _t.name());

	glBegin(GL_QUAD_STRIP); {
	    int width = tileWidth(_lat);
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

void TextureCO::load()
{
    // Load the file.  Map files can have the map's maximum elevation
    // embedded in them as a text comment, so extract it if it exists.
    _t.load(_f, &_maxElevation);
}

void TextureCO::unload()
{
    _t.unload();
    if (_dlist > 0) {
	glDeleteLists(_dlist, 1);
	_dlist = 0;
    }
}

bool TextureCO::loaded() const
{
    return _t.loaded();
}

BucketCO::BucketCO(const SGPath& f, long int index)
{
    _b = new Bucket(f, index);
    // Set our centre.
    setCentre(_b->bounds().getCenter());
}

BucketCO::~BucketCO()
{
    if (_b) {
	delete _b;
    }
}

SceneryTile::SceneryTile(TileInfo *ti): 
    _ti(ti), _levels(ti->mapLevels()), _maxElevation(NO_ELEVATION),
    _buckets(NULL)
{
    // Create a texture object for each level at which we have maps.
    // EYE - actually, this does levels for which we *want* maps.
    for (unsigned int i = 0; i < _levels.size(); i++) {
	if (_levels[i]) {
	    char str[3];
	    sprintf(str, "%d", i);

	    SGPath f = _ti->mapsDir();
	    f.append(str);
	    f.append(_ti->sceneryDir().file());
	    _textures[i] = new TextureCO(f, _ti->lat(), _ti->lon());
	}
    }

    // Subscribe to the discrete/smooth contour change notification.
    // When we get it, we'll tell our buckets that they're dirty.
    subscribe(Notification::DiscreteContours);
    subscribe(Notification::NewPalette);
}

SceneryTile::~SceneryTile()
{
    map<int, TextureCO *>::const_iterator c;
    for (c = _textures.begin(); c != _textures.end(); c++) {
	delete c->second;
    }

    if (_buckets != NULL) {
	for (unsigned int i = 0; i < _buckets->size(); i++) {
	    delete (*_buckets)[i];
	}
	delete _buckets;
    }
}

// Adds the texture at the given level to the given cache.  This tells
// the cache that the texture is visible and needs to be loaded.
void SceneryTile::addTexture(unsigned int level, Cache *cache)
{
    assert(_textures.find(level) != _textures.end());
    cache->add(_textures[level]);
}

// Adds any visible buckets to the given cache.  This tells the cache
// that each bucket is visible and needs to be loaded.  We use the
// culler to tell us if a bucket is visible or not.
void SceneryTile::addBuckets(Cache *cache, Culler::FrustumSearch& frustum)
{
    // Make sure we know what our buckets are.
    _findBuckets();

    for (unsigned int i = 0; i < _buckets->size(); i++) {
	Bucket *b = (*_buckets)[i]->bucket();
	if ((b != NULL) && 
	    frustum.intersects(b->bounds())) {
	    cache->add((*_buckets)[i]);
	}
    }
}

// Draws the texture that best matches the given level, where "best"
// means the first texture we find at this level or below.  Failing
// that, we choose the first one we find above this level.
void SceneryTile::drawTexture(unsigned int level)
{
    // Note the seemingly backwards termination test.  Why?  The loop
    // variable 'l' is an unsigned int.  We want to test it for all
    // values down to 0.  However, if it's 0 and we subtract 1, it
    // will become very large.  Thus the test.  Tricky (and scary).
    for (unsigned int l = level; l < _levels.size(); l--) {
	if (_levels[l] && _textures[l]->loaded()) {
	    _textures[l]->draw();
	    return;
	}
    }

    // No textures at this level or below?  Okay then, how about
    // above this level?
    for (unsigned int l = level + 1; l < _levels.size(); l++) {
	if (_levels[l] && _textures[l]->loaded()) {
	    _textures[l]->draw();
	    return;
	}
    }
}

// Draw the buckets in the tile that are within the culler's frustum.
void SceneryTile::drawBuckets(Culler::FrustumSearch& frustum)
{
    assert(glutGetWindow() == main_window);
    assert(_buckets != NULL);

    for (unsigned int i = 0; i < _buckets->size(); i++) {
	Bucket *b = (*_buckets)[i]->bucket();
	if ((b != NULL) && 
	    (b->loaded()) && 
	    frustum.intersects(b->bounds())) {
	    b->draw(globals.palette, globals.discreteContours);
	}
    }
}

// Called when the lighting changes.
bool SceneryTile::notification(Notification::type n)
{
    if ((n == Notification::DiscreteContours) ||
	(n == Notification::NewPalette)) {
	// Switched between smooth and discrete contours, or got a new
	// palette.  We need to force re-rendering of our live
	// scenery.  Tell each bucket it's dirty.
	if (_buckets) {
	    for (unsigned int i = 0; i < _buckets->size(); i++) {
		_buckets->at(i)->setDirty();
	    }
	}
    } else {
	assert(false);
    }

    return true;
}

// Draw the maximum elevation figure (MEF) at the given location and
// scale.
static void _label(int mef, double lat, double lon, double metresPerPixel)
{
    int thousands, hundreds;
    thousands = mef / 1000;
    hundreds = mef % 1000 / 100;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // EYE - magic "number"
    glColor4f(0.0, 0.0, 0.5, 0.5);

    glPushMatrix(); {
	// Most of the time we could just use the bucket's bounding
	// sphere centre, but at high latitudes this doesn't work -
	// the centre of the bounding sphere may not even lie within
	// the bucket.  So we use its latitude and longitude instead.
	sgdVec3 v;
	sgGeodToCart(lat * SGD_DEGREES_TO_RADIANS, lon * SGD_DEGREES_TO_RADIANS,
		     0, v);
	glTranslated(v[0], v[1], v[2]);
	glRotatef(lon + 90.0, 0.0, 0.0, 1.0);
	glRotatef(90.0 - lat, 1.0, 0.0, 0.0);

	LayoutManager lm;
	lm.begin();

	// EYE - magic font sizes
	lm.setFont(globals.fontRenderer, 36.0 * metresPerPixel);
	globalString.printf("%d", thousands);
	lm.addText(globalString.str());

	// EYE - need a way to do superscripts
	lm.setFont(globals.fontRenderer, 24.0 * metresPerPixel);
	globalString.printf(" %d", hundreds);
	lm.addText(globalString.str());

	lm.end();

	// EYE - centre it somehow?
	lm.drawText();
    }
    glPopMatrix();

    glDisable(GL_BLEND);
}

// Labels the tiles or buckets with their maximum elevation figure.

// EYE - this should be generalized - label any region of a certain
// size (in pixels).  When we zoom in, we should go down to
// sub-buckets, when we zoom out we should go to tile clusters.
// Because of this, the label() facility doesn't really belong
// (exclusively) at the Tile level.
void SceneryTile::label(Culler::FrustumSearch& frustum, double metresPerPixel,
			bool live)
{
    assert(glutGetWindow() == main_window);

    if (live) {
	// If we're live, we label each bucket individually.
	for (unsigned int i = 0; i < _buckets->size(); i++) {
	    Bucket *b = (*_buckets)[i]->bucket();
	    // Label this bucket if it's loaded and visible.
	    if ((b != NULL) && 
		(b->loaded()) && 
		frustum.intersects(b->bounds())) {
		int mef = MEF(b->maximumElevation());
		_label(mef, b->centreLat(), b->centreLon(), metresPerPixel);
	    }
	}
    } else {
	// We're not live, so label the tile as a whole.  The maximum
	// elevation of this tile is unknown initially, and we don't
	// find out what it is until we load a map which has the
	// maximum elevation tag.  Unfortunately, we aren't told when
	// maps are loaded (we merely request them), so we need to
	// explicitly check.
	if (_maxElevation == NO_ELEVATION) {
	    // Hasn't been set, so check.  We look through our maps
	    // until we find one with a valid maximum elevation
	    // figure, then quit.
	    for (unsigned int i = 0; i < _levels.size(); i++) {
		if (_levels[i] && _textures[i]->loaded()) {
		    double m = _textures[i]->maximumElevation();
		    if (m != NO_ELEVATION) {
			_maxElevation = m;
			break;
		    }
		}
	    }
	}

	if (_maxElevation != NO_ELEVATION) {
	    int mef = MEF(_maxElevation);
	    double lat = _ti->lat() + 0.5;
	    double lon = _ti->lon() + _ti->width() / 2.0;
	    _label(mef,  lat, lon, metresPerPixel);
	}
    }
}

// Tells the tile what the centre of our view is.  The tile tells all
// its textures, which calculate their distance from this point.  This
// distance is used by the corresponding cache to decide which ones to
// load first.
void SceneryTile::setCentre(const sgdVec3 centre)
{
    map<int, TextureCO *>::const_iterator c;
    for (c = _textures.begin(); c != _textures.end(); c++) {
	c->second->setCentre(centre);
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
	Bucket *bucket = (*_buckets)[i]->bucket();
	if (bucket->intersection(a, b, c)) {
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

    _buckets = new vector<BucketCO *>;

    const vector<long int>* buckets = _ti->bucketIndices();
    for (unsigned int i = 0; i < buckets->size(); i++) {
	long int index = buckets->at(i);
	_buckets->push_back(new BucketCO(_ti->sceneryDir(), index));
    }
}

Scenery::Scenery(TileManager *tm): 
    _dirty(true), _level(TileManager::MAX_MAP_LEVEL), _live(false), 
    _levels(tm->mapLevels()), _tm(tm), _backgroundWorld(0)
{
    // Create a culler and a frustum searcher for it.
    _culler = new Culler();
    _frustum = new Culler::FrustumSearch(*_culler);

    // Create scenery tiles.
    const map<string, TileInfo *>& tiles = _tm->tiles();
    map<string, TileInfo *>::const_iterator i = tiles.begin();
    for (; i != tiles.end(); i++) {
	TileInfo *ti = i->second;

	// Create a tile.
	SceneryTile *tile = new SceneryTile(ti);

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
	tile->setCentre(bounds.getCenter());

	tile->setBounds(bounds);

	_tiles.push_back(tile);
// 	_culler->addObject(tile, _tileType, bounds);
	_culler->addObject(tile);
    }

    // Create caches for the texture directories.
    for (unsigned int i = 0; i < _levels.size(); i++) {
	if (_levels[i]) {
	    // Make cache size inversely proportional to the size of
	    // the textures.
	    _textureCaches[i] = new Cache(exp2(15 - i));
	}
    }

    // Subscribe to notifications of moves and zooms.
    subscribe(Notification::Moved);
    subscribe(Notification::Zoomed);
}

Scenery::~Scenery()
{
    map<int, Cache *>::const_iterator c;
    for (c = _textureCaches.begin(); c != _textureCaches.end(); c++) {
	delete c->second;
    }

    for (unsigned int i = 0; i < _tiles.size(); i++) {
	delete _tiles[i];
    }

    delete _frustum;
    delete _culler;
}

// Loads the image to be used as the "background world".  The
// background world is a single texture draped over the globe, used as
// a background where there is no FlightGear scenery.
void Scenery::setBackgroundImage(const SGPath& f)
{
    // Load the file into a Texture object.
    _world.load(f);

    // Create display list for background world.  
    _backgroundWorld = glGenLists(1);
    assert(_backgroundWorld != 0);

    glNewList(_backgroundWorld, GL_COMPILE);
    // Move the background world back slightly.  This is so that the
    // scenery, when draped over the world, is not obscured by the
    // world texture map.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0, 1.0);

    // Now stretch the texture over a world.
    glColor3f(1.0, 1.0, 1.0);

    glEnable(GL_TEXTURE_2D);
// #ifndef LIGHTING
//     glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
// #else
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
// #endif
    glBindTexture(GL_TEXTURE_2D, _world.name());

    // Stitch the texture to the globe in a series of EW strips.
    for (int lat = 0; lat < 180; lat++) {
	glBegin(GL_QUAD_STRIP); {
	    double s, n;
	    s = lat - 90;
	    n = lat - 90 + 1;
	    // "Pin" the texture at 1 degree intervals (for most,
	    // this just means the 4 corners, but at extreme
	    // latitudes, it means points along the upper and
	    // lower edges as well).
	    for (int lon = 0; lon <= 360; lon++) {
		double w = lon - 180.0;

		// Note that the texture is loaded "upside-down" (the
		// first row of the image is y = 0.0 of the texture).
		glTexCoord2f(lon / 360.0, (180 - lat - 1) / 180.0); 
		geodVertex3f(n, w);
		glTexCoord2f(lon / 360.0, (180 - lat) / 180.0); 
		geodVertex3f(s, w);
	    }
	}
	glEnd();
    }
    glDisable(GL_TEXTURE_2D);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glEndList();
}

void Scenery::move(const sgdMat4 modelViewMatrix)
{
    _frustum->move(modelViewMatrix);
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
    // base-2 log to get the "level".
    unsigned int idealLevel = 
	ceil(log2(SGGeodesy::EQURAD * SGD_PI / 180.0 / metresPerPixel));

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

void Scenery::draw(bool elevationLabels)
{
    // We assume that when called, the depth test is on, lighting
    // is off, and that we have smooth shading.
    assert(glIsEnabled(GL_DEPTH_TEST) && !glIsEnabled(GL_LIGHTING));

    // Has our view of the world changed?
    if (_dirty) {
	// Yes.  Update our idea of what to display, ask the culler
	// for visible tiles, and tell the cache(s).

	// Tell all caches to reset themselves (not strictly
	// necessary, as most will be idle anyway, but it's easier
	// than checking).
	map<int, Cache *>::const_iterator c;
	for (c = _textureCaches.begin(); c != _textureCaches.end(); c++) {
	    c->second->reset(eye);
	}
	_bucketCache.reset(eye);

	// Now ask the culler for all visible tiles, and tell them to
	// add themselves to the appropriate cache for loading.
	const vector<Cullable *>& intersections = _frustum->intersections();
	for (unsigned int i = 0; i < intersections.size(); i++) {
	    SceneryTile *t = dynamic_cast<SceneryTile *>(intersections[i]);
	    if (!t) {
		continue;
	    }
	    // Tell the tile to schedule a texture for loading.  Note
	    // that if we have no maps, then there will be no texture
	    // to be loaded.
	    if (_level < _levels.size()) {
		t->addTexture(_level, _textureCaches[_level]);
	    }
	    // If we're live, schedule the appropriate buckets for
	    // loading too.
	    if (_live) {
		t->addBuckets(&_bucketCache, *_frustum);
	    }
	}

	// Now start the cache(s).
	for (c = _textureCaches.begin(); c != _textureCaches.end(); c++) {
	    c->second->go();
	}
	_bucketCache.go();

	_dirty = false;
    }

    // Our strategy is:
    //
    // (a) We display a "background world", consisting of a single
    //     texture wrapped around the world.
    //
    // (b) Draw tile textures at the appropriate resolution.  We use
    //     the culler to decide what tiles to draw.
    //
    // (c) Draw scenery tiles on top of the textures if we're very
    //     close.  Note that scenery tiles do not replace textures,
    //     for 2 reasons: (a) we may not have loaded all the desired
    //     scenery yet, and (b) a tile covers a complete 1x1 (or
    //     bigger) area, but the scenery of which it is composed may
    //     only cover part of that area (because part of it may be
    //     open ocean, or otherwise have no scenery).

    // Render the background world if it exists.
    if (_backgroundWorld != 0) {
	glCallList(_backgroundWorld);
    }

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
	glShadeModel(globals.smoothShading ? GL_SMOOTH : GL_FLAT);
	if (globals.lightingOn) {
	    float BRIGHTNESS = 0.8;
	    GLfloat diffuse[] = {BRIGHTNESS, BRIGHTNESS, BRIGHTNESS, 1.0};
	    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	    
	    // Set the light position (in eye coordinates, not world
	    // coordinates).
	    glMatrixMode(GL_MODELVIEW);
	    glPushMatrix(); {
		glLoadIdentity();
		glLightfv(GL_LIGHT0, GL_POSITION, globals.lightPosition);
	    }
	    glPopMatrix();

	    glEnable(GL_LIGHT0);
	    glEnable(GL_LIGHTING);

	    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	    glEnable(GL_COLOR_MATERIAL);
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
	    t->drawBuckets(*_frustum);
	}

	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);
    }

    if (elevationLabels) {
	_label(_live);
    }
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
    glPushAttrib(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    // Draw elevation figures.
//     vector<void *> intersections = _frustum->intersections(_tileType);
    const vector<Cullable *>& intersections = _frustum->intersections();
    for (unsigned int i = 0; i < intersections.size(); i++) {
// 	SceneryTile *t = (SceneryTile *)intersections[i];
	SceneryTile *t = dynamic_cast<SceneryTile *>(intersections[i]);
	if (!t) {
	    continue;
	}
	t->label(*_frustum, _metresPerPixel, live);
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
// EYE - I'm not sure what x and y should represent - the centre of a
// pixel, the lower-left corner, ...?
bool Scenery::intersection(double x, double y, 
			   SGVec3<double> *c, bool *validElevation)
{
    GLint viewport[4];
    GLdouble mvmatrix[16], projmatrix[16];
    GLdouble wx, wy, wz;	// World x, y, z coords.

    // True if we intersect with the live scenery.
    bool liveIntersection = false;

    // Our line is given by two points: the intersection of our
    // viewing "ray" with the near depth plane and far depth planes.
    // This assumes that we're using an orthogonal projection - in a
    // perspective projection, we'd use our eyepoint as one of the
    // points and the near depth plane as the other.
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projmatrix);
    // EYE - I seem to have to do this correction, but I'm not sure
    // why.  A bug/feature of OS X?  Pixel centre vs pixel corner?  Or
    // am I just confused?
    x -= 1.0;
    y -= 1.0;
    // viewport[3] is height of window in pixels.
    y = viewport[3] - y - 1;

    // Near depth plane intersection.
    gluUnProject (x, y, 0.0, mvmatrix, projmatrix, viewport, &wx, &wy, &wz);
    SGVec3<double> near(wx, wy, wz);

    // Far depth plane intersection.
    gluUnProject (x, y, 1.0, mvmatrix, projmatrix, viewport, &wx, &wy, &wz);
    SGVec3<double> far(wx, wy, wz);
    
    // Later, we'll ask candidate buckets will redraw themselves in
    // select mode to see if there are any intersections.  We define a
    // new projection matrix containing the pick region, which we
    // define to be 1 pixel square.  When the buckets draw themselves,
    // they will only get results for this small region.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix(); {
	glLoadIdentity();
	gluPickMatrix(x, y, 1.0, 1.0, viewport);
	glMultMatrixd(projmatrix);

	// Now that we have our viewing ray (although, technically
	// speaking, it should actually be a very narrow viewing
	// frustum, but a ray is good enough), get the intersection.
	// We first ask each visible tile in turn if the ray
	// intersects their live scenery, and, if it does, the
	// elevation at that point.
// 	vector<void *> intersections = _frustum->intersections(_tileType);
	const vector<Cullable *>& intersections = _frustum->intersections();
	for (unsigned int i = 0; i < intersections.size(); i++) {
// 	    SceneryTile *t = (SceneryTile *)(intersections[i]);
	    SceneryTile *t = dynamic_cast<SceneryTile *>(intersections[i]);
	    if (!t) {
		continue;
	    }
	    if (t->intersection(near, far, c)) {
		// Found one!  Since the tile found the intersection
		// using one of its buckets (ie, live scenery), we
		// know that the elevation value is valid.
		liveIntersection = true;
		break;
	    }
	}
	glMatrixMode(GL_PROJECTION);
    }
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    // Set the validElevation variable if it's not NULL.
    if (validElevation) {
	*validElevation = liveIntersection;
    }

    // If we found a valid intersection, we can return right now.
    if (liveIntersection) {
	return true;
    }
    
    // If we got here, that means no tiles intersected or the user
    // isn't intersted in an elevation.  So, we'll just use a
    // simple earth/ray intersection to find out the lat/lon.  We
    // stretch the universe along the earth's axis so that the
    // earth is a sphere.  This code assumes that the earth is
    // centred at the origin, and that the earth's axis is aligned
    // with the z axis, north positive.
    SGVec3<double> centre(0.0, 0.0, 0.0);
    double mu1, mu2;
    near[2] *= SGGeodesy::STRETCH;
    far[2] *= SGGeodesy::STRETCH;
    if (RaySphere(near, far, centre, SGGeodesy::EQURAD, &mu1, &mu2)) {
	SGVec3<double> s1, s2;
	s1 = near + mu1 * (far - near);
	s2 = near + mu2 * (far - near);

	// Take the nearest intersection (the other is on the other
	// side of the world).
	if (dist(near, s1) < dist(near, s2)) {
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

// Called when somebody issues a Moved or Zoomed notification.  We
// examine the current OpenGL model-view (Moved) or projection
// (zoomed) matrices to update our view of the world.
bool Scenery::notification(Notification::type n)
{
    if (n == Notification::Moved) {
	sgdMat4 modelViewMatrix;
	glGetDoublev(GL_MODELVIEW_MATRIX, (GLdouble *)modelViewMatrix);
	move(modelViewMatrix);
    } else if (n == Notification::Zoomed) {
	// We need to express the current OpenGL projection matrix in
	// the form of the clipping planes.  To do that, we need to
	// dissect it.
	sgdMat4 projectionMatrix;
	glGetDoublev(GL_PROJECTION_MATRIX, (GLdouble *)projectionMatrix);
	
	// Note that we assume the matrix is orthogonal.
	double planes[6];	// left, right, top, bottom, -far, -near
	for (int i = 0; i < 3; i++) {
	    double a = projectionMatrix[i][i];
	    double t = projectionMatrix[3][i];
	    planes[i * 2] = -(t + 1) / a;
	    planes[i * 2 + 1] = -(t - 1) / a;
	}

	sgdFrustum f;
	f.setFrustum(planes[0], planes[1],
		     planes[2], planes[3],
		     -planes[4], -planes[5]);

	// The zoom depends on the size of the viewing window.  We
	// assume the scale is the same vertically as horizontally, so
	// we just look at width (viewport[2]).
	GLdouble viewport[4];
	glGetDoublev(GL_VIEWPORT, viewport);
	double metresPerPixel = (planes[1] - planes[0]) / viewport[2];

	zoom(f, metresPerPixel);
    } else {
	assert(false);
    }

    return true;
}
