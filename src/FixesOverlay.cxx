/*-------------------------------------------------------------------------
  FixesOverlay.cxx

  Written by Brian Schack

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
#include <sstream>

#include <simgear/misc/sg_path.hxx>
#include <simgear/math/sg_geodesy.hxx>

#include "FixesOverlay.hxx"
#include "Globals.hxx"

using namespace std;

// Above this level, no fixes are drawn.
const float noLevel = 1250.0;
// Above this level, but below noLevel, high-level fixes are drawn.
const float highLevel = 250.0;
// Above this level, but below highLevel, low-level fixes are drawn.
const float lowLevel = 100.0;
// Below lowLevel, only approach fixes are drawn.

// Bright yellow.
// const float fix_colour[4] = {1.0, 1.0, 0.0, 0.7};
const float high_fix_colour[4] = {1.0, 1.0, 0.0, 0.7};
const float low_fix_colour[4] = {0.0, 1.0, 1.0, 0.7};
const float terminal_fix_colour[4] = {1.0, 0.0, 1.0, 0.7};

const float fix_label_colour[4] = {0.2, 0.2, 0.2, 0.7};

//////////////////////////////////////////////////////////////////////
// Searchable interface.
//////////////////////////////////////////////////////////////////////
double FIX::distanceSquared(const sgdVec3 from) const
{
    return sgdDistanceSquaredVec3(bounds.center, from);
}

// Returns our tokens, generating them if they haven't been already.
const std::vector<std::string>& FIX::tokens()
{
    if (_tokens.empty()) {
	// The name/id is a token.
	_tokens.push_back(name);

	// Add a "FIX:" token.
	_tokens.push_back("FIX:");
    }

    return _tokens;
}

// Returns our pretty string, generating it if it hasn't been already.
const std::string& FIX::asString()
{
    if (_str.empty()) {
	// Initialize our pretty string.
	globalString.printf("FIX: %s", name);
	_str = globalString.str();
    }

    return _str;
}

FixesOverlay::FixesOverlay(Overlays& overlays):
    _overlays(overlays), _DL(0), _fixDL(0), _isDirty(false)
{
    // EYE - Initialize policy here
    _createFix();

    // Create a culler and a frustum searcher for it.
    _culler = new Culler();
    _frustum = new Culler::FrustumSearch(*_culler);

    // Subscribe to moved and zoomed notifications.
    subscribe(Notification::Moved);
    subscribe(Notification::Zoomed);
}

// Creates a single predefined fix and saves it in _fixDL.  The fix is
// drawn in the x-y plane.
void FixesOverlay::_createFix()
{
    glDeleteLists(_fixDL, 1);
    _fixDL = glGenLists(1);
    assert(_fixDL != 0);
    glNewList(_fixDL, GL_COMPILE); {
// 	glBegin(GL_LINE_LOOP); {
// 	    for (int i = 0; i < 3; i ++) {
// 		float theta, x, y;

// 		theta = i * 120.0 * SG_DEGREES_TO_RADIANS;
// 		x = sin(theta);
// 		y = cos(theta);
// 		glVertex2f(x, y);
// 	    }
// 	}
// 	glEnd();
	glEnable(GL_POINT_SMOOTH);
	glBegin(GL_POINTS); {
	    glVertex2f(0.0, 0.0);
	}
	glEnd();
	glDisable(GL_POINT_SMOOTH);
    }
    glEndList();
}

FixesOverlay::~FixesOverlay()
{
    for (unsigned int i = 0; i < _fixes.size(); i++) {
	FIX *n = _fixes[i];

	delete n;
    }

    _fixes.clear();

    glDeleteLists(_DL, 1);
    glDeleteLists(_fixDL, 1);

    delete _culler;
    delete _frustum;
}

void FixesOverlay::setPolicy(const FixPolicy& p)
{
    _policy = p;

    setDirty();
}

FixPolicy FixesOverlay::policy()
{
    return _policy;
}

bool FixesOverlay::load(const string& fgDir)
{
    bool result = false;

    SGPath f(fgDir);
    f.append("Navaids/fix.dat.gz");

    gzFile arp;
    char *line;

    arp = gzopen(f.c_str(), "rb");
    if (arp == NULL) {
	// EYE - we might want to throw an error instead.
	fprintf(stderr, "_loadFixes: Couldn't open \"%s\".\n", f.c_str());
	return false;
    } 

    // Check the file version.  We can handle version 600 files.
    int version = -1;
    gzGetLine(arp, &line);	// Windows/Mac header
    gzGetLine(arp, &line);	// Version
    sscanf(line, "%d", &version);
    if (version == 600) {
	// It looks like we have a valid file.
	result = _load600(arp);
    } else {
	// EYE - throw an error?
	fprintf(stderr, "_loadFixes: \"%s\": unknown version %d.\n", 
		f.c_str(), version);
	result = false;
    }

    gzclose(arp);

    return result;
}

bool FixesOverlay::_load600(const gzFile& arp)
{
    char *line;

    FIX *f;

    fprintf(stderr, "Loading fixes ...\n");
    while (gzGetLine(arp, &line)) {
	if (strcmp(line, "") == 0) {
	    // Blank line.
	    continue;
	} 

	if (strcmp(line, "99") == 0) {
	    // Last line.
	    break;
	}

	// Create a record and fill it in.
	f = new FIX;

	// A line looks like this:
	//
	// <lat> <lon> <name>
	//
	// EYE - do a real error check
	assert(sscanf(line, "%lf %lf %s", &f->lat, &f->lon, f->name) == 3);

	// Add to the culler.
	sgdVec3 point;
	atlasGeodToCart(f->lat, f->lon, 0.0, point);

	// We arbitrarily say fixes have a radius of 1000m.
	f->bounds.radius = 1000.0;
	f->bounds.setCenter(point);

	// Until determined otherwise, fixes are not assumed to be
	// part of any low or high altitude airways.
	f->low = f->high = false;

	// Add to our culler.
	_frustum->culler().addObject(f);

	// Add to the fixes vector.
	_fixes.push_back(f);

	// Create search tokens for it.
	globals.searcher.add(f);

	// Add to the navPoints map.
	NAVPOINT foo;
	foo.isNavaid = false;
	foo.n = (void *)f;
	navPoints.insert(pair<string, NAVPOINT>(f->name, foo));
    }

    // EYE - will there ever be a false return?
    return true;
}

void FixesOverlay::setDirty()
{
    _isDirty = true;
}

void FixesOverlay::draw()
{
    if (_metresPerPixel > noLevel) {
	return;
    }

    if (_isDirty) {
	// Something's changed, so we need to regenerate the display
	// list.

	// EYE - do we need to delete it and generate a new one, or
	// can we just redefine it?
	glDeleteLists(_DL, 1);
	_DL = glGenLists(1);
	assert(_DL != 0);
	glNewList(_DL, GL_COMPILE);

// 	glEnable(GL_LINE_SMOOTH);
	glLineWidth(1.0);
// 	glColor4fv(fix_colour);

// 	bool high = false, low = false, terminal = false;
// 	if (_overlays.scale() > highLevel) {
// 	    high = true;
// 	    glColor4fv(high_fix_colour);
// 	} else if (_overlays.scale() > lowLevel) {
// 	    low = true;
// 	    glColor4fv(low_fix_colour);
// 	} else {
// 	    terminal = true;
// 	    glColor4fv(terminal_fix_colour);
// 	}

	vector<Cullable *> intersections = _frustum->intersections();
	for (unsigned int i = 0; i < intersections.size(); i++) {
	    FIX *f = dynamic_cast<FIX *>(intersections[i]);
	    assert(f);

// // 	    _render(f);
// 	    if ((high && f->high) || 
// 		(low && f->low) || 
// 		(terminal && !f->high && !f->low)) {
// 		_render(f);
// 	    }
	    if (f->high && _metresPerPixel < noLevel) {
		glColor4fv(high_fix_colour);
		_render(f);
	    } else if (f->low && (_metresPerPixel < highLevel)) {
		glColor4fv(low_fix_colour);
		_render(f);
	    } else if (!f->high && !f->low && (_metresPerPixel < lowLevel)) {
		glColor4fv(terminal_fix_colour);
		_render(f);
	    }
	}

// 	glDisable(GL_LINE_SMOOTH);

	if (_overlays.isVisible(Overlays::LABELS)) {
	    glEnable(GL_BLEND);
	    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	    for (unsigned int i = 0; i < intersections.size(); i++) {
		FIX *f = dynamic_cast<FIX *>(intersections[i]);
		assert(f);

// // 	    _label(f);
// 	    if ((high && f->high) || 
// 		(low && f->low) || 
// 		(terminal && !f->high && !f->low)) {
// 		_label(f);
// 	    }
		if (f->high && _metresPerPixel < noLevel) {
		    _label(f);
		} else if (f->low && (_metresPerPixel < highLevel)) {
		    _label(f);
		} else if (!f->high && !f->low && 
			   (_metresPerPixel < lowLevel)) {
		    _label(f);
		}
	    }

	    glDisable(GL_BLEND);
	}
	glEndList();
	
	_isDirty = false;
    }

    glCallList(_DL);
}

// Renders the given fix.
void FixesOverlay::_render(const FIX *f)
{
    double metresPerPixel = _metresPerPixel;
    SGVec3<double> point;
    float scale = 5.0 * metresPerPixel;

    glPushMatrix(); {
	glTranslated(f->bounds.center[0],
		     f->bounds.center[1],
		     f->bounds.center[2]);
	glRotatef(f->lon + 90.0, 0.0, 0.0, 1.0);
	glRotatef(90.0 - f->lat, 1.0, 0.0, 0.0);

	// Draw it with a radius of 5 pixels.
	glScalef(scale, scale, scale);

	glCallList(_fixDL);

    }
    glPopMatrix();
}

// Labels the given fix.
void FixesOverlay::_label(const FIX *f)
{
    double metresPerPixel = _metresPerPixel;

    // Put this outside of _render() (since we only do it once)?
    LayoutManager lm;
    fntRenderer& renderer = globals.fontRenderer;
    float pointSize = metresPerPixel * 10.0;
    const float labelOffset = metresPerPixel * 10.0;
    renderer.setPointSize(pointSize);
    lm.setFont(renderer, pointSize);

    glPushMatrix(); {
	glTranslated(f->bounds.center[0],
		     f->bounds.center[1],
		     f->bounds.center[2]);
	glRotatef(f->lon + 90.0, 0.0, 0.0, 1.0);
	glRotatef(90.0 - f->lat, 1.0, 0.0, 0.0);

	// Draw a label labelOffset pixels to the left of the fix.
	glColor4fv(fix_label_colour);
	lm.begin();
	lm.addText(f->name);
	lm.end();

	float width, height;
	lm.size(&width, &height);
	lm.moveTo(-(width / 2.0 + labelOffset), 0.0);
	lm.drawText();
    }
    glPopMatrix();
}

// Called when somebody posts a notification that we've subscribed to.
bool FixesOverlay::notification(Notification::type n)
{
    if (n == Notification::Moved) {
	// Update our frustum from globals and record ourselves as
	// dirty.
	_frustum->move(globals.modelViewMatrix);
	setDirty();
    } else if (n == Notification::Zoomed) {
	// Update our frustum and scale from globals and record
	// ourselves as dirty.
	_frustum->zoom(globals.frustum.getLeft(),
		       globals.frustum.getRight(),
		       globals.frustum.getBot(),
		       globals.frustum.getTop(),
		       globals.frustum.getNear(),
		       globals.frustum.getFar());
	_metresPerPixel = globals.metresPerPixel;
	setDirty();
    } else {
	assert(false);
    }

    return true;
}
