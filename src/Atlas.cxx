/*-------------------------------------------------------------------------
  Atlas.cxx
  Map browsing utility

  Written by Per Liedman, started February 2000.
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
---------------------------------------------------------------------------*/

#include <memory.h>
#include <stdio.h>

#include <simgear/compiler.h>
#include <simgear/math/sg_geodesy.hxx> // geo_inverse_wgs_84
#include <simgear/sg_inlines.h>	       // min()
#include <plib/fnt.h>
#include <plib/pu.h>
#include <plib/puAux.h>
#include <string>

#include <cassert>
#include <map>
#include <stdexcept>

#include "config.h"		// For our version number

#include "FlightTrack.hxx"
#include "Tiles.hxx"
#include "Search.hxx"
#include "Preferences.hxx"
#include "Graphs.hxx"
#include "misc.hxx"
#include "Culler.hxx"
#include "Scenery.hxx"
#include "Overlays.hxx"
#include "Globals.hxx"
#include "Notifications.hxx"
#include "Palette.hxx"

using namespace std;

// User preferences (including command-line arguments).
Preferences prefs;

// Our windows.
int main_window, graphs_window;

class MainUI {
  public:
    MainUI(int x, int y);

    void setTrackListDirty() { _dirty = true; }
    void setTrackList();
    void update();
    void setTrackSize(int i);
    void setCurrentItem(int i);

    void setDMS(bool magnetic);
    void setMagnetic(bool magnetic);

    puGroup *gui;
    puFrame *preferencesFrame, *locationFrame, *navaidsFrame, 
	*flightTracksFrame;

    // Preferences frame widgets.
    puButtonBox *degMinSecBox, *magTrueBox;
    char *degMinSecBoxLabels[3], *magTrueBoxLabels[3];
    bool degMinSec;

    // Location frame widgets.
    puInput *latInput, *lonInput, *zoomInput;
    puText *elevText, *mouseText;
    puOneShot *zoomInButton, *zoomOutButton;

    // Navaids frame widgets.
    puButton *navaidsToggle;
    puButton *airportsToggle, *airwaysToggle, *labelsToggle, *tracksToggle;
    puButton *MEFToggle;
    puButton *navVOR, *navNDB, *navILS, *navDME, *navFIX;
    puButton *awyHigh, *awyLow;

    // Flight tracks frame widgets.
    puOneShot *loadButton, *attachButton;

    puaComboBox *tracksComboBox;
    puArrowButton *prevTrackButton, *nextTrackButton;
    puOneShot *unloadButton, *detachButton;
    puOneShot *saveButton, *saveAsButton, *clearButton;
    puButton *trackAircraftToggle;
    puOneShot *jumpToButton;
    puText *trackSizeText;
    AtlasString trackSizeLabel;
    puInput *trackLimitInput;

    // True if the mouse coordinates should be shown, false if the
    // centre coordinates should be shown.
    bool showMouse;

  protected:
    // True if the tracks list has changed.
    bool _dirty;
};

//////////////////////////////////////////////////////////////////////
//
// Info Interface
//
// This gives information about the current aircraft position and
// velocities, and navigation receivers.
class InfoUI {
  public:
    InfoUI(int x, int y);
    // EYE - destructor?

    void reveal() { gui->reveal(); }
    void hide() { gui->hide(); }
    bool isVisible() { return gui->isVisible(); }

    void setText();

    puPopup *gui;
    puFrame *infoFrame;
    puButton *follow;
    puText *latText, *lonText, *altText, *hdgText, *spdText, *hmsText, *dstText;
    puFrame *VOR1Colour, *VOR2Colour, *ADFColour;
    puText *VOR1Text, *VOR2Text, *ADFText;
    puButtonBox *graphsBox;
    // EYE - constant alert!  We should get '3' from Graphs.hxx somehow.
    const char *graphsBoxLabels[4];
    puSlider *smoother;
    puButtonBox *xAxisBox;
    const char *xAxisBoxLabels[3];
};

//////////////////////////////////////////////////////////////////////
//
// Lighting Interface
//
// This allows the user to adjust lighting parameters: discrete/smooth
// contours, lighting on/off, smooth/flat polygon shading, and
// current palette.
class LightingUI {
  public:
    LightingUI(int x, int y);
    // EYE - destructor?

    static void azimElevToXYZW(float azimuth, float elevation, float *xyzw);

    void reveal() { gui->reveal(); }
    void hide() { gui->hide(); }
    bool isVisible() { return gui->isVisible(); }

    void getSize(int *w, int *h);
    void setPosition(int x, int y);

    void setLightPosition();
    void previous();
    void next();
    void setPalette(unsigned int i);
    int currentItem() { return paletteComboBox->getCurrentItem(); }
    void updatePalettes();

    puPopup *gui;
    puFrame *frame;

    // Lighting toggles
    puButtonBox *contours, *lighting, *polygons;
    puText *contoursLabel, *lightingLabel, *polygonsLabel;
    char *contoursLabels[3], *lightingLabels[3], *polygonsLabels[3];

    // Lighting direction
    puFrame *directionFrame;
    puText *directionLabel;
    puInput *azimuth, *elevation;
    puDial *azimuthDial;
    puSlider *elevationSlider;

    // Palettes
    puFrame *paletteFrame;
    puText *paletteLabel;
    puaComboBox *paletteComboBox;
    puArrowButton *prevPalette, *nextPalette;

  protected:
    vector<SGPath> _palettes;
};

//////////////////////////////////////////////////////////////////////
//
// Network Popup
//
// This popup lets the user specify the parameters to a network or
// serial connection.
class NetworkPopup {
  public:
    NetworkPopup(int x, int y);
    ~NetworkPopup();

    puDialogBox *dialogBox;
    puButton *networkButton, *serialButton;
    puInput *portInput, *deviceInput, *baudInput;
    puOneShot *cancelButton, *okButton;
};

//////////////////////////////////////////////////////////////////////
//
// Help Interface
//
// Gives the user information on Atlas' startup parameters and
// keyboard shortcuts.
class HelpUI {
  public:
    HelpUI(int x, int y, Preferences& prefs, TileManager& tm);
    ~HelpUI();

    void reveal() { gui->reveal(); }
    void hide() { gui->hide(); }
    bool isVisible() { return gui->isVisible(); }

    void setText(puObject *obj);

    puPopup *gui;
    puText *labelText;
    puaLargeInput *text;
    puButton *generalButton, *keyboardButton;

  protected:
    char *_label, *_generalText, *_keyboardText;
};

MainUI *mainUI = NULL;
InfoUI *infoUI = NULL;
LightingUI *lightingUI = NULL;
NetworkPopup *networkPopup = NULL;
HelpUI *helpUI = NULL;

// Search interface.
Search *search_interface;
// Records where we were when we started a search.  If the search is
// cancelled, then we'll return to that point.
sgdVec3 searchFrom;

// Altitude/Speed interface.
Graphs *graphs;

// File dialog.
puaFileSelector *fileDialog = NULL;

// By default, when we show a flight track, we display the information
// window and graphs window.  If this is false (this can be toggled by
// the user), then we don't show them.
bool showGraphWindow = true;

// Some default colours.
float flightTrackColour[4] = {0.0, 0.0, 1.0, 1.0};
float planeColour[4] = {1.0, 1.0, 0.0, 1.0};
// EYE - eventually these should be placed in preferences.  Also,
// we'll eventually need to deal with more than 3 radios.
float vor1Colour[4] = {0.000, 0.420, 0.624, 0.7};
// float vor2Colour[4] = {0.420, 0.000, 0.624, 0.7};
float vor2Colour[4] = {0.420, 0.624, 0.0, 0.7};
float adfColour[4] = {0.525, 0.294, 0.498, 0.7};

// The tile manager keeps track of tiles.
TileManager *tileManager;

//////////////////////////////////////////////////////////////////////
// Live scenery stuff
//////////////////////////////////////////////////////////////////////

// The Scenery object manages loading and displaying all of our
// scenery.
Scenery *scenery;
// EYE - put in preferences?  MainUI?
// True if we want to label scenery tiles with a MEF.
bool elevationLabels = true;

// We keep a list of all palettes.  Initially this is set to the
// installed palettes in Atlas' Palettes directory, but it can expand
// if the user loads custom palettes of her own.
class Palettes {
  public:
    Palettes(const char *paletteDir);
    ~Palettes();

    Palette *currentPalette();
    unsigned int currentPaletteNo() { return _i; }
    unsigned int size() { return _palettes.size(); }
    const Palette *setPalette(unsigned int i);
    const vector<Palette *>& palettes() { return _palettes; }

    // Adds the given path to the list (unless it's there already),
    // loads the given palette (unless it's loaded already), and
    // returns a pointer to it (NULL if it couldn't be loaded, in
    // which case the vector and current palette are unchanged).
    Palette *load(const char *path);
    // Unloads the current palette and replaces it with another.
    const Palette *unload();

  protected:
    unsigned int _i;
    vector<Palette *> _palettes;
};
Palettes *palettes;

Palettes::Palettes(const char *paletteDir): _i(0)
{
    // Find out what palettes are pre-installed and put them in the
    // 'palettes' vector.
    ulDir *dir = ulOpenDir(paletteDir);
    ulDirEnt *entity;
    while (dir && (entity = ulReadDir(dir))) {
	// All palettes should have the suffix ".ap".
	char *suffix = entity->d_name + strlen(entity->d_name) - strlen(".ap");
	if (!entity->d_isdir && (strcmp(suffix, ".ap") == 0)) {
	    SGPath p(paletteDir);
	    p.append(entity->d_name);

	    try {
		Palette *aPalette = new Palette(p.c_str());
		_palettes.push_back(aPalette);
	    } catch (runtime_error e) {
		// EYE - throw an error instead?
		fprintf(stderr, "Palettes::Palettes: couldn't load '%s'", 
			p.c_str());
	    }
	}
    }
}

Palette *Palettes::currentPalette()
{
    if (_i < _palettes.size()) {
	return _palettes[_i];
    } else {
	return NULL;
    }
}

const Palette *Palettes::setPalette(unsigned int i)
{
    if (i < _palettes.size()) {
	_i = i;
    }

    if (_i < _palettes.size()) {
	return _palettes[_i];
    } else {
	return NULL;
    }
}

Palette *Palettes::load(const char *path)
{
    // First check and see if we have it already.  We just compare the
    // paths we're given, which are not guaranteed to be canonical, so
    // this is not a fail-safe test - it's possible to load the same
    // file twice.
    unsigned int i;
    for (i = 0; i < _palettes.size(); i++) {
	if (strcmp(path, _palettes[i]->path()) == 0) {
	    _i = i;
	    break;
	}
    }

    if (i == _palettes.size()) {
	// It's new, so try loading it.
	try {
	    Palette *aPalette = new Palette(path);
	    _palettes.push_back(aPalette);
	    _i = i;
	} catch (runtime_error e) {
	    fprintf(stderr,
		    "Palettes::load: error loading palette file '%s': %s\n", 
		    path, e.what());
	    return NULL;
	}
    }

    return _palettes[_i];
}

const Palette *Palettes::unload()
{
    if (_i < _palettes.size()) {
	Palette *p = _palettes[_i];
	_palettes.erase(_palettes.begin() + _i);
	delete p;
	if (_i < _palettes.size()) {
	    return _palettes[_i];
	} else if (_palettes.size() > 0) {
	    _i = _palettes.size() - 1;
	    return _palettes[_i];
	} else {
	    _i = 0;
	    return NULL;
	}
    } else {
	return NULL;
    }
}

// Current cursor location.
struct Cursor {
    int x, y;
    SGVec3<double> cart;
    SGGeod geod;
    bool validLocation, validElevation;
} cursor;

// View variables.
// EYE - put in globals?  ('eye' is used in Scenery.cxx)
sgdVec3 eye;
sgdVec3 eyeUp;
sgdVec3 mark;

// Take 10 steps (0.1) to zoom by a factor of 10.
const double zoomFactor = pow(10.0, 0.1);

//////////////////////////////////////////////////////////////////////
// Forward declarations of all callbacks.
//////////////////////////////////////////////////////////////////////
void smoother_cb(puObject *smoother);
void graph_axis_cb(puObject *xAxisBox);
void lighting_cb(puObject *lightingUIObject);
void close_ok_cb(puObject *widget);
void zoom_cb(puObject *cb);
void show_cb(puObject *cb);
void position_cb (puObject *cb);
void clear_ftrack_cb(puObject *);
void degMinSec_cb(puObject *cb);
void magTrue_cb(puObject *cb);
void graph_type_cb (puObject *graphsBox);
void save_as_file_cb(puObject *cb);;
void save_as_cb(puObject *cb);
void save_as_file_cb(puObject *cb);
void save_cb(puObject *cb);
void load_file_cb(puObject *cb);
void load_cb(puObject *cb);
void unload_cb(puObject *cb);
void detach_cb(puObject *cb);
void track_select_cb(puObject *cb);
void track_aircraft_cb(puObject *cb);
void jump_to_cb(puObject *cb);
void track_limit_cb(puObject *cb);
void attach_cb(puObject *);
void network_serial_toggle_cb(puObject *o);
void network_serial_cb(puObject *obj);
void help_cb(puObject *obj);

// Call this when the cursor or the scene moves.  It updates the
// variables that depend on the cursor location.  It assumes that
// 'cursor' has the current cursor x, y position.
void cursorUpdate()
{
    // EYE - check for any out-of-window-bounds conditions.
    // Don't do anything if the cursor x, y is negative.
    if ((cursor.x < 0) || (cursor.y < 0)) {
	return;
    }

    // Find out what point on the earth lies below the cursor.
    cursor.validLocation = 
	scenery->intersection(cursor.x, cursor.y, 
			      &cursor.cart, &cursor.validElevation);
    if (cursor.validLocation) {
	SGGeod start;
	SGGeodesy::SGCartToGeod(cursor.cart, cursor.geod);
    }
}

// All of these routines alter OpenGL state (movePosition() and
// rotatePosition() alter the camera location and rotation via
// gluLookAt(), while zoomTo() alters the clip planes via glOrtho()).
// As well, they inform the culler about changes to the current view
// bounds.
//
// The "underscore" routines (_rotate, _move) are convenience routines
// that are meant to be used internally.

// Sets the up vector to point directly north from the current eye
// vector.  If the current eye vector is at the north or south pole,
// then we arbitrarily align it with lon = 0.  Changes eyeUp.
void _rotate(double hdg)
{
    // North or south pole?
    if ((eye[0] == 0.0) && (eye[1] == 0.0)) {
	sgdSetVec3(eyeUp, 1.0, 0.0, 0.0);
	return;
    }

    // There are probably more efficient ways to accomplish this, but
    // this approach has the advantage that I understand it.
    // Basically what we do is rotate a unit vector that initially
    // points up (north).  We roll it by the heading passed in, and
    // then rotate by the longitude of the current eye point, then
    // pitch it up by the latitude.  Note that the eye point uses
    // geocentric, not geodetic, coordinates.
    SGVec3<double> c(eye[0], eye[1], eye[2]);
    SGGeoc g = SGGeoc::fromCart(c);

    // PLIB's idea of "straight ahead" is looking out along the
    // y-axis, which in our world is 90 degrees east longitude, with
    // "up" in the positive z direction.  PLIB's "heading" corresponds
    // to our longitude (adjusted by 90 degrees), "pitch" to latitude,
    // and "roll" to the heading passed in (negated).
    double heading = g.getLongitudeDeg() - 90.0;
    double pitch = g.getLatitudeDeg();
    double roll = -hdg;

    sgdMat4 rot;
    sgdMakeRotMat4(rot, heading, pitch, roll);

    sgdVec3 up = {0.0, 0.0, 1.0};
    sgdXformVec3(eyeUp, up, rot);
}

// Called after a change to the eye position or up vector.  Sets the
// OpenGL eye point (via gluLookAt), updates the location string, and
// asks GLUT to redisplay.  Assumes that eye and/or eyeUp has been
// correctly set.  In general, this routine shouldn't be called
// directly - use movePosition() or rotatePosition() instead.
void _move()
{
    // Note that we always look at the origin.  This means that our
    // views will not quite be perpendicular to the earth's surface,
    // since the earth is not perfectly spherical.
    glLoadIdentity();
    gluLookAt(eye[0], eye[1], eye[2], 
	      0.0, 0.0, 0.0, 
	      eyeUp[0], eyeUp[1], eyeUp[2]);

    // Update our globals.
    glGetDoublev(GL_MODELVIEW_MATRIX, (GLdouble *)globals.modelViewMatrix);

    // Notify subscribers that we've moved.
    Notification::notify(Notification::Moved);

    // Update cursor-dependent variables.
    cursorUpdate();

    // Update interface.
    glutPostRedisplay();
}

// Sets the up vector to point to the given heading (default is north)
// from the current eye vector.
void rotateEye(double heading = 0.0)
{
    // Adjust eye and eyeUp.
    _rotate(heading);

    // This doesn't really seem like a move, since we're just rotating
    // about the eye point, but it may result in new scenery rotating
    // into view, so we need to tell the culler, the scenery, the
    // overlays, and the cursor that we've shifted things.
    _move();
}

// Moves eye point to the given cartesian location, setting up vector
// to north.
void movePosition(const sgdVec3 dest)
{
    sgdCopyVec3(eye, dest);
    _rotate(0.0);

    _move();			// This will inform the culler.
}

// Moves eye point to the given lat, lon, setting up vector to north.
void movePosition(double lat, double lon)
{
    // Convert from lat, lon to x, y, z.
    sgdVec3 cart;
    atlasGeodToCart(lat, lon, 0.0, cart);

    movePosition(cart);
}

// Rotates the eye point and up vector using the given rotation
// matrix.
void rotatePosition(sgdMat4 rot)
{
    // Rotate the eye and eye up vectors.
    sgdXformVec3(eye, rot);
    sgdXformVec3(eyeUp, rot);

    // The eye point is always assumed to lie on the surface of the
    // earth (at sea level).  Rotating it might leave it above or
    // below (because he earth is not perfectly spherical), so we need
    // to normalize it.
    eye[2] *= SGGeodesy::STRETCH;
    sgdScaleVec3(eye, SGGeodesy::EQURAD / sgdLengthVec3(eye));
    eye[2] /= SGGeodesy::STRETCH;

    _move();			// This will inform the culler.
}

// Sets zoom level.
void zoomTo(double scale)
{
    globals.metresPerPixel = scale;

    // Adjust clip planes.
    GLfloat viewport[4];
    glGetFloatv(GL_VIEWPORT, viewport);
    GLfloat width = viewport[2];
    GLfloat height = viewport[3];

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    double left, right, bottom, top, near, far;
    right = width * globals.metresPerPixel / 2.0;
    left = -right;
    top = height * globals.metresPerPixel / 2.0;
    bottom = -top;

    double l = sgdLengthVec3(eye);
//     near = l - SGGeodesy::EQURAD - 10000;
    near = -10000.0;		// EYE - magic number
    far = l;			// EYE - need better magic number (use
				// bounds?)
    glOrtho(left, right, bottom, top, near, far);

    glMatrixMode(GL_MODELVIEW);

    // Set our global frustum.  This is used by various subsystems to
    // find out what's going on.
    globals.frustum.setFrustum(left, right, bottom, top, near, far);
    
    // Note: If you want to get this and the scale from OpenGL, here's
    // the code:
//     sgdMat4 projectionMatrix;
//     glGetDoublev(GL_PROJECTION_MATRIX, (GLdouble *)projectionMatrix);
	
//     // Note that we assume the matrix is orthogonal.
//     double planes[6];	// left, right, top, bottom, -far, -near
//     for (int i = 0; i < 3; i++) {
// 	double a = projectionMatrix[i][i];
// 	double t = projectionMatrix[3][i];
// 	planes[i * 2] = -(t + 1) / a;
// 	planes[i * 2 + 1] = -(t - 1) / a;
//     }

//     sgdFrustum f;
//     f.setFrustum(planes[0], planes[1],
// 		 planes[2], planes[3],
// 		 -planes[4], -planes[5]);

//     // The zoom depends on the size of the viewing window.  We
//     // assume the scale is the same vertically as horizontally, so
//     // we just look at width (viewport[2]).
//     GLdouble viewport[4];
//     glGetDoublev(GL_VIEWPORT, viewport);
//     double metresPerPixel = (planes[1] - planes[0]) / viewport[2];

    // Tell all interested parties that we've zoomed.
    Notification::notify(Notification::Zoomed);

    // Although the cursor hasn't moved, the scenery has, so we
    // probably need to update cursor-dependent variables.
    cursorUpdate();

    glutPostRedisplay();
}

void zoomBy(double factor)
{
    zoomTo(globals.metresPerPixel * factor);
}

////////////////////////////////////////////////////////////////////////
// Format angle as dd mm'ss.s" (DMS-Format) or dd.dddd (decimal degrees)
////////////////////////////////////////////////////////////////////////
// EYE - put this in misc.hxx?
const unsigned char degreeSymbol = 176;
static const char *formatAngle(double degrees)
{
    degrees = fabs(degrees);

    if (mainUI->degMinSec) {
	int deg_part;
	int min_part;
	float sec_part;

	deg_part = (int)degrees;
	min_part = (int)(60.0f * (degrees - deg_part));
	sec_part = 3600.0f * (degrees - deg_part - min_part / 60.0f);

	/* Round off hundredths */
	if (sec_part + 0.005f >= 60.0f)
	    sec_part -= 60.0f, min_part += 1;
	if (min_part >= 60)
	    min_part -= 60, deg_part += 1;

	// EYE - need to check for texture fonts if we do this
	globalString.printf("%02d%C %02d' %05.2f\"",
			    deg_part, degreeSymbol, min_part, sec_part);
    } else {
	globalString.printf("%.8f%C", degrees, degreeSymbol);
    }

    return globalString.str();
}

///////////////////////////////////////////////////////////////////////////////
// Center map at aircraft's current position.
///////////////////////////////////////////////////////////////////////////////
void centerMapOnAircraft()
{
    FlightData *pos = globals.currentPoint();
    if (pos != (FlightData *)NULL) {
	movePosition(pos->lat, pos->lon);
    }
}

// Called if the aircraft is moved (including if the track changes).
// It will center the map on the aircraft if we're in autocentre mode,
// ensure that the overlays know that the aircraft has moved, and
// update the information interface.
void aircraftMoved()
{
    if (prefs.autocenter_mode) {
	centerMapOnAircraft();
    }
    Notification::notify(Notification::AircraftMoved);
    infoUI->setText();
}

// This doesn't really do much that's useful, but it makes our calls
// consistent.
void flightTrackModified()
{
    Notification::notify(Notification::FlightTrackModified);
}

// Called when we have a new flight track and we want to update our
// interfaces.  Also notifies everyone that we have a new flight
// track.
void newFlightTrack()
{
    // Tell the flight tracks overlay to remove all tracks and replace
    // it with the current one.
    FlightTracksOverlay *fto = globals.overlays->flightTracksOverlay();
    fto->removeTrack();
    fto->addTrack(globals.track(), flightTrackColour, planeColour);

    // Tell everyone what has happened.  Note that we need to do this
    // before some operations below, because they depend on other
    // parts of Atlas updating themselves.
    Notification::notify(Notification::NewFlightTrack);

    // Set our windows appropriately.
    if (!globals.track()) {
	// No tracks, so hide everything.
	glutSetWindow(graphs_window);
	glutHideWindow();

	glutSetWindow(main_window);
	glutSetWindowTitle("Atlas");
	infoUI->hide();
    } else {
	glutSetWindow(graphs_window);
	if (showGraphWindow) {
	    glutShowWindow();
	}
	glutPostRedisplay();

	glutSetWindow(main_window);

	globalString.printf("Atlas - %s", globals.track()->niceName());
	glutSetWindowTitle(globalString.str());

	if (showGraphWindow) {
	    infoUI->reveal();
	}
	centerMapOnAircraft();
	infoUI->setText();
	smoother_cb(infoUI->smoother);
    }

    // Set status of the flight track buttons.
    mainUI->update();
    // EYE - combine with update()?
    // EYE - make protected and force callers to use setTrackListDirty()?
    mainUI->setTrackList();
    // EYE - ditto?
    mainUI->setCurrentItem(globals.currentTrackNo());

    glutPostRedisplay();
}

///////////////////////////////////////////////////////////////////////////////
// Search helper functions.
///////////////////////////////////////////////////////////////////////////////

// Called to initiate a new search or continue an active search.  If
// the search is big, it will only do a portion of it, then reschedule
// itself to continue the search.  To prevent multiple search threads
// beginning, we use the variable 'searchTimerScheduled' to record if
// there are pending calls to searchTimer().
bool searchTimerScheduled = false;
void searchTimer(int value)
{
    // If the search interface is hidden, we take that as a signal
    // that the search has ended.
    if (!search_interface->isVisible()) {
	searchTimerScheduled = false;
	return;
    }

    // Check if we have any more hits.
    char *str;
    static int maxMatches = 100;

    str = search_interface->searchString();
    if (globals.searcher.findMatches(str, eye, maxMatches)) {
	// We have new matches, so we need to show them.  Ensure that
	// the main window is the current window.
	glutSetWindow(main_window);

	search_interface->reloadData();
	glutPostRedisplay();

	// Continue the search in 100ms.
	assert(searchTimerScheduled == true);
	glutTimerFunc(100, searchTimer, 1);
    } else {
	// No new matches, so our search is finished.
	searchTimerScheduled = false;
    }
}

// Called when the user hits return or escape.
void searchFinished(Search *s, int i)
{
    if (i != -1) {
	// User hit return.  Jump to the selected point.
	const Searchable *s = globals.searcher.getMatch(i);
	movePosition(s->location());
    } else {
	// User hit escape, so return to our original point.
	// EYE - restore original orientation too
	movePosition(searchFrom);
    }

    glutPostRedisplay();
}

// Called when the user selects an item in the list.
void searchItemSelected(Search *s, int i)
{
    if (i != -1) {
	const Searchable *match = globals.searcher.getMatch(i);
	movePosition(match->location());
    }

    glutPostRedisplay();
}

// Called when the user changes the search string.  We just call
// searchTimer() if it's not running already, which will incrementally
// search for str.
void searchStringChanged(Search *s, char *str)
{
    if (!searchTimerScheduled) {
	searchTimerScheduled = true;
	searchTimer(0);
    }
}

// Called by the search interface to find out how many matches there
// are.
int noOfMatches(Search *s)
{
    return globals.searcher.noOfMatches();
}

// // Removes any trailing whitespace, and replaces multiple spaces and
// // tabs by single spaces.  Returns a pointer to the cleaned up string.
// // If you want to use the string, you might want to copy it, as it
// // will be overwritten on the next call.
// char *cleanString(const char *str)
// {
//     static char *buf = NULL;
//     static unsigned int length = 0;
//     unsigned int i, j;
    
//     if ((buf == NULL) || (length < strlen(str))) {
// 	length = strlen(str);
// 	buf = (char *)realloc(buf, sizeof(char) * (length + 1));
// 	if (buf == NULL) {
// 	    fprintf(stderr, "cleanString: Out of memory!\n");
// 	    exit(1);
// 	}
//     }

//     i = j = 0;
//     while (i <= strlen(str)) {
// 	int skip = strspn(str + i, " \t\n");
// 	i += skip;

// 	// Replaced skipped whitespace with a single space, unless
// 	// we're at the end of the string.
// 	if ((skip > 0) && (i < strlen(str))) {
// 	    buf[j++] = ' ';
// 	}

// 	// Copy the next character over.  This will copy the final
// 	// '\0' as well.
// 	buf[j++] = str[i++];
//     }

//     return buf;
// }

// Called by the search interface to get the data for index i.
char *matchAtIndex(Search *s, int i)
{
    Searchable *searchable = globals.searcher.getMatch(i);

    // The search interface owns the strings we give it, so make a
    // copy.
    return strdup(searchable->asString().c_str());
}

//////////////////////////////////////////////////////////////////////
// Generic dialog
//////////////////////////////////////////////////////////////////////
puDialogBox *dialogBox = NULL;

// Called if the user hits a button on the exit dialog box.
void exit_ok_cb(puObject *widget)
{
    bool okay = widget->getDefaultIntegerValue();
    puDeleteObject(dialogBox);
    dialogBox = NULL;
    if (okay) {
	exit(0);
    }
}

// Called if the hits a button on the close track dialog box.
void close_ok_cb(puObject *widget)
{
    bool okay = widget->getDefaultIntegerValue();
    puDeleteObject(dialogBox);
    dialogBox = NULL;
    if (okay) {
	// Unload the track with extreme prejudice.
	unload_cb(NULL);
    }
}

// Create a generic dialog box, with "OK" and "Cancel" buttons.  The
// dialog will have the given text, and call 'cb' when done.  The okay
// button's default integer value is set to 1 (true), while the cancel
// button's is set to 0 (false).  The button which was pressed will be
// passed to the callback, so you can check its default integer value
// to find out which one was pressed.  The callback must delete the
// dialog (using puDeleteObject) and set it to NULL.
void makeDialog(const char *str, puCallback cb)
{
    // Do we already have a dialog box?
    if (dialogBox != NULL) {
	return;
    }

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLint windowWidth = viewport[2];
    GLint windowHeight = viewport[3];
    // EYE - magic numbers (and many others later).
    const int dialogWidth = 300;
    const int dialogHeight = 100;

    // Place the dialog box in the centre of the window.
    dialogBox = new puDialogBox(windowWidth / 2 - dialogWidth / 2, 
				windowHeight / 2 - dialogHeight / 2); {
	int x = dialogWidth / 2, y = 10;
	new puFrame(0, 0, dialogWidth, dialogHeight);
	puText *label = new puText(10, 70);
	label->setLabel(str);

	// Put the okay button in the centre ...
	puOneShot *okay = new puOneShot(x, y, "OK");
	okay->makeReturnDefault(FALSE);
	okay->setCallback(cb);
	okay->setDefaultValue(true);

	// ... and the cancel button to its right
	int w, h;
	okay->getSize(&w, &h);
	puOneShot *cancel = new puOneShot(x + w + 5, y, "Cancel");
	cancel->makeReturnDefault(FALSE);
	cancel->setCallback(cb);
	cancel->setDefaultValue(false);
    }
    dialogBox->close();
    dialogBox->reveal();

    glutPostRedisplay();
}

//////////////////////////////////////////////////////////////////////
//  PUI Code (widgets)
//////////////////////////////////////////////////////////////////////

void init_gui(bool textureFonts) 
{
    puInit();

    if (textureFonts) {
	//     SGPath font_name(prefs.fg_root.str());
	//     font_name.append("Fonts/helvetica_medium.txf");
	assert(globals.regularFont);
	globals.uiFont.initialize(globals.regularFont, 12.0f);
    }
    puSetDefaultFonts(globals.uiFont, globals.uiFont);
    // Note that the default colour scheme as an alpha of 0.8 - this,
    // and the fact that GL_BLEND is on, means that the widgets will
    // be slightly translucent.  Set to 1.0 if you want them to be
    // completely opaque.
    puSetDefaultColourScheme(0.4, 0.5, 0.9, 0.8);
    puSetDefaultStyle(PUSTYLE_SMALL_BEVELLED);

    //////////////////////////////////////////////////////////////////////
    //
    // Search Interface
    //
    // The search interface is used to search for airports and navaids.

    // The initial location doesn't matter, as we'll later ensure it
    // appears in the upper right corner.
    search_interface = new Search(0, 0, 300, 300);
    search_interface->setCallback(searchFinished);
    search_interface->setSelectCallback(searchItemSelected);
    search_interface->setInputCallback(searchStringChanged);
    search_interface->setSizeCallback(noOfMatches);
    search_interface->setDataCallback(matchAtIndex);
    search_interface->hide();

    if (prefs.softcursor) {
	puShowCursor();
    }
}

//////////////////////////////////////////////////////////////////////
// MainUI
//////////////////////////////////////////////////////////////////////

// Create the main interface, with its lower-left corner at x, y.
// Assumes that puInit() has been called.
MainUI::MainUI(int x, int y): _dirty(true)
{
    const int buttonHeight = 20, checkHeight = 10;
    const int smallSpace = 2, bigSpace = 5;
    const int boxHeight = 55;
    const int 
	preferencesHeight = boxHeight,
	locationHeight = 4 * buttonHeight + 5 * bigSpace, 
	navaidsHeight = 7 * buttonHeight + 2 * bigSpace + 6 * smallSpace, 
	flightTracksHeight = 7 * buttonHeight + 8 * bigSpace;
    const int width = 210, guiWidth = width - (2 * bigSpace), labelWidth = 45;
    const int boxWidth = width / 2;

    int cury = 0, curx = bigSpace;

    gui = new puGroup(x, y);

    //////////////////////////////////////////////////////////////////////
    // Flight tracks
    //////////////////////////////////////////////////////////////////////
    flightTracksFrame = new puFrame(0, cury, width, cury + flightTracksHeight);
    cury += bigSpace;

    const int trackButtonWidth = (width - 4 * bigSpace) / 3;

    // Track size and track size limit.
    curx = bigSpace;
    trackLimitInput = new puInput(curx + labelWidth * 2, cury, 
				  curx + guiWidth, cury + buttonHeight);
    trackLimitInput->setLabel("Track limit: ");
    trackLimitInput->setLabelPlace(PUPLACE_CENTERED_LEFT);
    trackLimitInput->setValidData("0123456789");
    trackLimitInput->setCallback(track_limit_cb);
    cury += buttonHeight + bigSpace;

    // EYE - the two labels, "Track limit:" and "Track size:" don't
    // line up on the left (but they seem to on the right).  What
    // behaviour controls this?
    trackSizeText = new puText(curx, cury);
    // EYE - set label or legend?
    trackSizeText->setLabel("Track size: ");
    cury += buttonHeight + bigSpace;

    // Jump to button and track aircraft toggle
    curx = bigSpace;
    jumpToButton = new puOneShot(curx, cury,
				 curx + trackButtonWidth, cury + buttonHeight);
    jumpToButton->setLegend("Center");
    jumpToButton->setCallback(jump_to_cb);
    curx += trackButtonWidth + bigSpace;

    trackAircraftToggle = new puButton(curx, cury, 
				       curx + checkHeight, cury + checkHeight);
    trackAircraftToggle->setLabel("Follow aircraft");
    trackAircraftToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    trackAircraftToggle->setButtonType(PUBUTTON_VCHECK);
    trackAircraftToggle->setCallback(track_aircraft_cb);

    cury += buttonHeight + bigSpace;

    // Save, save as, and clear  buttons.
    curx = bigSpace;
    saveButton = new puOneShot(curx, cury,
			       curx + trackButtonWidth, cury + buttonHeight);
    saveButton->setLegend("Save");
    saveButton->setCallback(save_cb);
    curx += trackButtonWidth + bigSpace;

    saveAsButton = new puOneShot(curx, cury,
				 curx + trackButtonWidth, cury + buttonHeight);
    saveAsButton->setLegend("Save As");
    saveAsButton->setCallback(save_as_cb);
    curx += trackButtonWidth + bigSpace;

    clearButton = new puOneShot(curx, cury,
				curx + trackButtonWidth, cury + buttonHeight);
    clearButton->setLegend("Clear");
    clearButton->setCallback(clear_ftrack_cb);
    cury += buttonHeight + bigSpace;

    // Unload and detach buttons.
    curx = bigSpace;
    unloadButton = new puOneShot(curx, cury,
				 curx + trackButtonWidth, cury + buttonHeight);
    unloadButton->setLegend("Unload");
    unloadButton->setCallback(unload_cb);
    curx += trackButtonWidth + bigSpace;

    detachButton = new puOneShot(curx, cury,
				 curx + trackButtonWidth, cury + buttonHeight);
    detachButton->setLegend("Detach");
    detachButton->setCallback(detach_cb);
    cury += buttonHeight + bigSpace;

    // Track chooser, and next and previous arrow buttons.
    curx = bigSpace;
    tracksComboBox = new puaComboBox(curx, 
				     cury,
				     curx + guiWidth - buttonHeight,
				     cury + buttonHeight,
				     NULL,
				     FALSE);
    tracksComboBox->setCallback(track_select_cb);
    curx += guiWidth - buttonHeight;
    prevTrackButton = new puArrowButton(curx,
					cury + buttonHeight / 2,
					curx + buttonHeight,
					cury + buttonHeight,
					PUARROW_UP);
    prevTrackButton->setCallback(track_select_cb);
    prevTrackButton->greyOut();
    nextTrackButton = new puArrowButton(curx,
					cury,
					curx + buttonHeight,
					cury + buttonHeight / 2,
					PUARROW_DOWN);
    nextTrackButton->setCallback(track_select_cb);
    nextTrackButton->greyOut();

    cury += buttonHeight + bigSpace;

    // Load and attach buttons.
    curx = bigSpace;
    loadButton = new puOneShot(curx, cury,
			       curx + trackButtonWidth, cury + buttonHeight);
    loadButton->setLegend("Load");
    loadButton->setCallback(load_cb);
    curx += trackButtonWidth + bigSpace;

    attachButton = new puOneShot(curx, cury,
				curx + trackButtonWidth, cury + buttonHeight);
    attachButton->setLegend("Attach");
    attachButton->setCallback(attach_cb);
    cury += buttonHeight + bigSpace;

    //////////////////////////////////////////////////////////////////////
    // Navaids
    //////////////////////////////////////////////////////////////////////
    navaidsFrame = new puFrame(0, cury, width, cury + navaidsHeight);

    curx = bigSpace;
    cury += bigSpace + buttonHeight + smallSpace;

    // Indent these buttons.
    curx += buttonHeight;
    navFIX = new puButton(curx, cury, curx + checkHeight, cury + checkHeight);
    navFIX->setLabel("FIX");
    navFIX->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    navFIX->setButtonType(PUBUTTON_VCHECK);
    navFIX->setCallback(show_cb);
    cury += buttonHeight + smallSpace;

    navDME = new puButton(curx, cury, curx + checkHeight, cury + checkHeight);
    navDME->setLabel("DME");
    navDME->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    navDME->setButtonType(PUBUTTON_VCHECK);
    navDME->setCallback(show_cb);
    cury += buttonHeight + smallSpace;

    navILS = new puButton(curx, cury, curx + checkHeight, cury + checkHeight);
    navILS->setLabel("ILS");
    navILS->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    navILS->setButtonType(PUBUTTON_VCHECK);
    navILS->setCallback(show_cb);
    cury += buttonHeight + smallSpace;

    navNDB = new puButton(curx, cury, curx + checkHeight, cury + checkHeight);
    navNDB->setLabel("NDB");
    navNDB->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    navNDB->setButtonType(PUBUTTON_VCHECK);
    navNDB->setCallback(show_cb);
    cury += buttonHeight + smallSpace;

    navVOR = new puButton(curx, cury, curx + checkHeight, cury + checkHeight);
    navVOR->setLabel("VOR");
    navVOR->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    navVOR->setButtonType(PUBUTTON_VCHECK);
    navVOR->setCallback(show_cb);
    cury += buttonHeight + smallSpace;

    // Unindent.
    curx -= buttonHeight;

    navaidsToggle = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    navaidsToggle->setLabel("Navaids");
    navaidsToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    navaidsToggle->setButtonType(PUBUTTON_VCHECK);
    navaidsToggle->setCallback(show_cb);
    cury += buttonHeight;

    cury += bigSpace;

    // Now move to the right column and do it again, for airports,
    // airways, and labels.
    cury -= navaidsHeight;
    cury += bigSpace;
    curx += guiWidth / 2;

    MEFToggle = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    MEFToggle->setLabel("MEF");
    MEFToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    MEFToggle->setButtonType(PUBUTTON_VCHECK);
    MEFToggle->setCallback(show_cb);
    cury += buttonHeight + smallSpace;

    tracksToggle = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    tracksToggle->setLabel("Flight tracks");
    tracksToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    tracksToggle->setButtonType(PUBUTTON_VCHECK);
    tracksToggle->setCallback(show_cb);
    cury += buttonHeight + smallSpace;

    labelsToggle = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    labelsToggle->setLabel("Labels");
    labelsToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    labelsToggle->setButtonType(PUBUTTON_VCHECK);
    labelsToggle->setCallback(show_cb);
    cury += buttonHeight + smallSpace;

    // Indent for the 2 airways subtoggles.
    curx += buttonHeight;

    awyLow = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    awyLow->setLabel("Low");
    awyLow->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    awyLow->setButtonType(PUBUTTON_VCHECK);
    awyLow->setCallback(show_cb);
    cury += buttonHeight + smallSpace;

    awyHigh = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    awyHigh->setLabel("High");
    awyHigh->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    awyHigh->setButtonType(PUBUTTON_VCHECK);
    awyHigh->setCallback(show_cb);
    cury += buttonHeight + smallSpace;

    // Unindent
    curx -= buttonHeight;

    airwaysToggle = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    airwaysToggle->setLabel("Airways");
    airwaysToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    airwaysToggle->setButtonType(PUBUTTON_VCHECK);
    airwaysToggle->setCallback(show_cb);
    cury += buttonHeight + smallSpace;

    airportsToggle = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    airportsToggle->setLabel("Airports");
    airportsToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    airportsToggle->setButtonType(PUBUTTON_VCHECK);
    airportsToggle->setCallback(show_cb);
    cury += buttonHeight;

    cury += bigSpace;
    curx -= guiWidth / 2;

    //////////////////////////////////////////////////////////////////////
    // Location information
    //////////////////////////////////////////////////////////////////////
    locationFrame = new puFrame(0, cury, width, cury + locationHeight);
    cury += bigSpace;

    // Zoom: an input field and two buttons
    zoomInput = new puInput(curx + labelWidth, cury, 
			    curx + guiWidth - (2 * buttonHeight),
			    cury + buttonHeight);
    zoomInput->setLabel("Zoom:");
    zoomInput->setLabelPlace(PUPLACE_CENTERED_LEFT);
    zoomInput->setCallback(zoom_cb);
    curx += guiWidth - (2 * buttonHeight);
    zoomInButton = new puOneShot(curx, cury,
				 curx + buttonHeight, cury + buttonHeight);
    zoomInButton->setLegend("+");
    zoomInButton->setCallback(zoom_cb);
    curx += buttonHeight;
    zoomOutButton = new puOneShot(curx, cury,
				  curx + buttonHeight, cury + buttonHeight);
    zoomOutButton->setLegend("-");
    zoomOutButton->setCallback(zoom_cb);
    curx = bigSpace;
    cury += buttonHeight + bigSpace;

    // Elevation: a text output
    elevText = new puText(curx, cury);
    elevText->setLabel("Elev: 543 ft");
    curx += boxWidth;

    // Mouse/centre: a text output
    mouseText = new puText(curx, cury);
    showMouse = false;		// EYE - use a method
    elevText->setLabel("centre");

    curx = bigSpace;
    cury += buttonHeight + bigSpace;

    // Longitude: an input field
    lonInput = new puInput(curx + labelWidth, cury, 
			   curx + guiWidth, cury + buttonHeight);
    lonInput->setLabel("Lon:");
    lonInput->setLabelPlace(PUPLACE_CENTERED_LEFT);
    lonInput->setCallback(position_cb);
    cury += buttonHeight + bigSpace;

    // Latitude: an input field
    latInput = new puInput(curx + labelWidth, cury, 
			   curx + guiWidth, cury + buttonHeight);
    latInput->setLabel("Lat:");
    latInput->setLabelPlace(PUPLACE_CENTERED_LEFT);
    latInput->setCallback(position_cb);
    cury += buttonHeight + bigSpace;

    //////////////////////////////////////////////////////////////////////
    // Preferences
    //////////////////////////////////////////////////////////////////////
    preferencesFrame = new puFrame(0, cury, width, cury + preferencesHeight);
    curx = 0;

    // EYE - hard-coded degree symbol
    degMinSecBoxLabels[0] = "dd\260mm'ss\"";
    degMinSecBoxLabels[1] = "dd.ddd\260";
    degMinSecBoxLabels[2] = NULL;
    degMinSecBox = 
	new puButtonBox(curx, cury, 
			curx + boxWidth, cury + boxHeight,
			(char **)degMinSecBoxLabels, TRUE);
    degMinSecBox->setCallback(degMinSec_cb);
    degMinSec = true;

    curx += boxWidth;
    magTrueBoxLabels[0] = "Magnetic";
    magTrueBoxLabels[1] = "True";
    magTrueBoxLabels[2] = NULL;
    magTrueBox = 
	new puButtonBox(curx, cury, 
			curx + boxWidth, cury + boxHeight,
			(char **)magTrueBoxLabels, TRUE);
    magTrueBox->setCallback(magTrue_cb);
//     magnetic = true;

    gui->close();
}

// Resets the strings in the tracks combo box to match those in
// globals.tracks.  We try to maintain the current item (based on
// index value, not contents - if the new list is completely
// different, then a different track may occupy the old index), unless
// the new list is too small, in which case we select the last one.
void MainUI::setTrackList()
{
    if (!_dirty) {
	return;
    }

    static char **trackList = NULL;

    if (trackList != NULL) {
	for (int i = 0; i < tracksComboBox->getNumItems(); i++) {
	    free(trackList[i]);
	}
	free(trackList);
    }

    trackList = (char **)malloc(sizeof(char *) * globals.tracks().size() + 1);
    for (unsigned int i = 0; i < globals.tracks().size(); i++) {
	// The display styles are the same as in the graphs window.
	trackList[i] = strdup(globals.track(i)->niceName());
    }
    trackList[globals.tracks().size()] = (char *)NULL;

    tracksComboBox->newList(trackList);

    _dirty = false;
}

// Updates the main user interface based on the current track and
// track number.
void MainUI::update()
{
    FlightTrack *track = globals.track();
    int trackNo = globals.currentTrackNo();
    if (!track) {
	// If there's no track, we can only load and attach.
	unloadButton->greyOut();
	detachButton->greyOut();
	saveButton->greyOut();
	saveAsButton->greyOut();
	clearButton->greyOut();
	jumpToButton->greyOut();
	setTrackSize(-1);
	trackLimitInput->greyOut();
    } else if (track->live()) {
	// If the track is live (listening for input from FlightGear),
	// then we don't allow unloading (even if it has a file
	// associated with it).  It can be saved if it has a file and
	// has been modified.
	unloadButton->greyOut();
	detachButton->activate();
	saveAsButton->activate();
	clearButton->activate();
	jumpToButton->activate();
	setTrackSize(track->size());
	trackLimitInput->activate();
	trackLimitInput->setValue((int)track->maxBufferSize());
	if (track->hasFile() && track->modified()) {
	    saveButton->activate();
	} else {	    
	    saveButton->greyOut();
	}
    } else if (track->hasFile()) {
	unloadButton->activate();
	detachButton->greyOut();
	saveAsButton->activate();
	// Only live tracks can be cleared.
	clearButton->greyOut();
	jumpToButton->activate();
	setTrackSize(track->size());
	trackLimitInput->greyOut();
	if (track->modified()) {
	    saveButton->activate();
	} else {
	    saveButton->greyOut();
	}
    } else {
	// The track is not live, but has no file.  This can happen if
	// we've detached a live track but haven't saved it yet.  We
	// allow the user to unload it, but not clear it (they
	// actually amount to the same thing here, but I think
	// clearing matches live tracks better, because it implies
	// that more data will be coming in later).
	unloadButton->activate();
	detachButton->greyOut();
	saveButton->greyOut();
	saveAsButton->activate();
	clearButton->greyOut();
	jumpToButton->activate();
	setTrackSize(track->size());
	trackLimitInput->greyOut();
    }
    if (trackNo <= 0) {
	prevTrackButton->greyOut();
    } else {
	prevTrackButton->activate();
    } 
    if (trackNo >= (globals.tracks().size() - 1)) {
	nextTrackButton->greyOut();
    } else {
	nextTrackButton->activate();
    }
}

void MainUI::setTrackSize(int i)
{
    if (i < 0) {
	trackSizeLabel.printf("Track size: n/a");
    } else {
	trackSizeLabel.printf("Track size: %d points", i);
    }
    trackSizeText->setLabel(trackSizeLabel.str());
}

// Sets the ith item in the tracks combo box to be current.
void MainUI::setCurrentItem(int i)
{
    if (i < 0) {
	i = 0;
    } else if (i >= tracksComboBox->getNumItems()) {
	i = tracksComboBox->getNumItems() - 1;
    }

    // We temporarily remove the callback because calling
    // setCurrentItem() will call it, something we generally don't
    // want (we only want it called in response to user input).
    tracksComboBox->setCallback(NULL);
    tracksComboBox->setCurrentItem(i);
    tracksComboBox->setCallback(track_select_cb);
}

void MainUI::setDMS(bool dms) 
{
    degMinSec = dms;
    if (dms) {
	mainUI->degMinSecBox->setValue(0);
    } else {
	mainUI->degMinSecBox->setValue(1);
    }
}

void MainUI::setMagnetic(bool magnetic) 
{
    if (magnetic) {
	mainUI->magTrueBox->setValue(0);
    } else {
	mainUI->magTrueBox->setValue(1);
    }
}

//////////////////////////////////////////////////////////////////////
// InfoUI
//////////////////////////////////////////////////////////////////////
InfoUI::InfoUI(int x, int y)
{
    const int textHeight = 15;
    const int bigSpace = 5;
    // EYE - magic numbers
    const int textWidth = 175;
    const int graphsBoxHeight = 75, graphsBoxWidth = 115;
    const int xAxisBoxHeight = 50, xAxisBoxWidth = 100;
    const int smootherHeight = bigSpace * 4, smootherWidth = graphsBoxWidth;

    const int height = textHeight * 10 + 3 * bigSpace;
    const int width = textWidth + graphsBoxWidth + xAxisBoxWidth;

    int curx, cury;

    gui = new puPopup(x, y);

    infoFrame = new puFrame(0, 0, width, height);

    // EYE - check how the new font baseline is calculated vs what
    // PLIB expects!
    curx = bigSpace;
    cury = 0;
    // EYE - 5 = hack
    ADFColour = new puFrame(curx, cury + 5, 
			    curx + textHeight - 2, cury + 5 + textHeight - 2);
    ADFColour->setStyle(PUSTYLE_PLAIN);
    // EYE - 0.6 alpha comes from puSetDefaultColourScheme()
    ADFColour->setColour(PUCOL_FOREGROUND, 
			 adfColour[0], adfColour[1], adfColour[2], 0.6);
    ADFText = new puText(curx + textHeight, cury); cury += textHeight;

    VOR2Colour = new puFrame(curx, cury + 5, 
			     curx + textHeight - 2, cury + 5 + textHeight - 2);
    VOR2Colour->setStyle(PUSTYLE_PLAIN);
    VOR2Colour->setColour(PUCOL_FOREGROUND, 
			  vor2Colour[0], vor2Colour[1], vor2Colour[2], 0.6);
    VOR2Text = new puText(curx + textHeight, cury); cury += textHeight;

    VOR1Colour = new puFrame(curx, cury + 5, 
			     curx + textHeight - 2, cury + 5 + textHeight - 2);
    VOR1Colour->setStyle(PUSTYLE_PLAIN);
    VOR1Colour->setColour(PUCOL_FOREGROUND, 
			  vor1Colour[0], vor1Colour[1], vor1Colour[2], 0.6);
    VOR1Text = new puText(curx + textHeight, cury); cury += textHeight + bigSpace;

    dstText = new puText(curx, cury); cury += textHeight;
    hmsText = new puText(curx, cury); cury += textHeight;
    spdText = new puText(curx, cury); cury += textHeight;
    hdgText = new puText(curx, cury); cury += textHeight;
    altText = new puText(curx, cury); cury += textHeight;
    lonText = new puText(curx, cury); cury += textHeight;
    latText = new puText(curx, cury); cury += textHeight;

    curx = textWidth;
    cury = height - graphsBoxHeight;

    // EYE - Perhaps we should get these from Graphs.hxx (if not the
    // actual text, at least the number).
    graphsBoxLabels[0] = "Altitude";
    graphsBoxLabels[1] = "Speed";
    graphsBoxLabels[2] = "Climb rate";
    graphsBoxLabels[3] = NULL;
    graphsBox = 
	new puButtonBox(curx, cury, 
			curx + graphsBoxWidth, cury + graphsBoxHeight, 
			(char **)graphsBoxLabels, FALSE);
    graphsBox->setCallback(graph_type_cb);

    cury -= smootherHeight + textHeight;
    smoother = new puSlider(curx, cury, smootherWidth, FALSE, smootherHeight);
    smoother->setLabelPlace(PUPLACE_TOP_CENTERED);
    // EYE - necessary?
    smoother->setLegendFont(globals.uiFont);
    smoother->setLabelFont(globals.uiFont);
    smoother->setLabel("Smoothing (s)");
    smoother->setMinValue(0.0);	 // 0.0 = no smoothing
    smoother->setMaxValue(60.0); // 60.0 = smooth over a 60s interval
    smoother->setStepSize(1.0);
    smoother->setCallback(smoother_cb);

    curx += graphsBoxWidth;
    cury = height - xAxisBoxHeight;

    xAxisBoxLabels[0] = "Time";
    xAxisBoxLabels[1] = "Distance";
    xAxisBoxLabels[2] = NULL;
    xAxisBox = 
	new puButtonBox(curx, cury, 
			curx + xAxisBoxWidth, cury + xAxisBoxHeight, 
			(char **)xAxisBoxLabels, TRUE);
    xAxisBox->setCallback(graph_axis_cb);

    gui->close();

    gui->reveal();
}

// Given a point in 3D space, pc, and a frequency of interest, freq,
// returns the most powerful matching navaid from navaids, where
// "matching" means "having the same frequency", and "most powerful"
// means "having the greatest signal strength at our location".  It
// also returns, by reference, the distance to the navaid in metres
// and the total number of matching navaids.
//
// This works well for VORs, DMEs, and NDBs.  However, for ILS systems
// we often don't get what we want.  This is because many airports
// have identical frequencies for opposing ILS systems (the ones at
// opposite ends of the runway).  In real life, no confusion occurs,
// because when one is turned on, the other is turned off.  In Atlas,
// everything is always on.  And when we approach a runway, it turns
// out that the ILS locator of the opposing runway is the transmitter
// closest to us (the locator is located at the opposite end of the
// runway it serves).  As we pass over the threshold, then the
// glideslope transmitter of our chosen runway becomes the closest.
// Similar switches occur at the opposite end.  And if the ILS has a
// DME transmitter, then we may recognize that as closest at varying
// times.
//
// This hints at another problem - when tuned to an ILS with DME, we
// want the bearing to the ILS, *and* the distance to its DME.  Just
// concentrating on the closest transmitter only solves half the
// problem.

// Called for VORs, VORTACs, VOR-DMEs, and DMEs.
static void VORsAsString(vector<NAV *>& navs, int x, 
			 FlightData *p, int freq, float radial, 
			 AtlasString& str)
{
    NAV *vor = NULL, *dme = NULL;
    string *id;
    double vorStrength = 0.0, dmeStrength = 0.0;
    double dmeDistance = 0.0;
    unsigned int matchingNavaids = 0;

    // Find VOR and/or DME with strongest signal.
    for (unsigned int i = 0; i < navs.size(); i++) {
	NAV *n = navs[i];

	// We assume that signal strength is proportional to the
	// square of the range, and inversely proportional to the
	// square of the distance.  But 'we' may be wrong.  To prevent
	// divide-by-zero errors, we arbitrarily set 1 metre as the
	// minimum distance.
	double d = SG_MAX2(sgdDistanceVec3(p->cart, n->bounds.center), 1.0);
	double s = (double)n->range / d;
	s *= s;

	if ((n->navtype == NAV_VOR) && (s > vorStrength)) {
	    vor = n;
	    id = &(n->id);
	    vorStrength = s;
	} 
	if ((n->navtype == NAV_DME) && (s > dmeStrength)) {
	    dme = n;
	    id = &(n->id);
	    dmeDistance = d - dme->magvar;
	    dmeStrength = s;
	} 
	if ((n->navtype == NAV_VOR) && (s > dmeStrength) &&
	    ((n->navsubtype == VOR_DME) || (n->navsubtype == VORTAC))) {
	    dme = n;
	    id = &(n->id);
	    dmeDistance = d - dme->magvar;
	    dmeStrength = s;
	}
    }
    if (vor) {
	matchingNavaids++;
    }
    if (dme) {
	matchingNavaids++;
    }
    assert((navs.size() == 0) || (matchingNavaids > 0));

    str.printf("NAV");
    if (x > 0) {
	str.appendf(" %d", x);
    }
    str.appendf(": %.2f@%03.0f%C", freq / 1000.0, radial, degreeSymbol);

    if (navs.size() > 0) {
	str.appendf(" (%s", id->c_str());

	if (vor != NULL) {
	    // Calculate the actual radial the aircraft is on,
	    // corrected by the VOR's slaved variation.
	    double ar, endHdg, length;
	    geo_inverse_wgs_84(vor->lat, vor->lon, 
			       p->lat, p->lon, &ar, &endHdg, &length);
	    // 'ar' is the 'actual radial' - the bearing from the
	    // VOR TO the aircraft, adjusted for the VOR's idea of
	    // what the magnetic variation is.
	    ar -= vor->magvar;

	    // We want to show a TO/FROM indication, and the actual
	    // radial we're on.  Since we can think of a single radial
	    // as extending both TO and FROM the VOR, there are really
	    // two radials for every bearing from the VOR (a FROM
	    // radial, and a TO radial 180 degrees different).  We
	    // choose the one that's closest to our dialled-in radial.
	    double diff = normalizeHeading(ar - radial, false);
	    char *fromStr;
	    if ((diff <= 90.0) || (diff >= 270.0)) {
		fromStr = "FROM";
	    } else {
		fromStr = "TO";
		ar = normalizeHeading(ar - 180.0, false);
	    }

	    str.appendf(", %03.0f%C %s", normalizeHeading(rint(ar), false), 
			degreeSymbol, fromStr);
	}

	if (dme != NULL) {
	    str.appendf(", %.1f DME", dmeDistance * SG_METER_TO_NM);
	}

	str.appendf(")");
    }

    // Indicate if there are matching navaids that we aren't
    // displaying.
    if (navs.size() > matchingNavaids) {
	str.appendf(" ...");
    }
}

static void ILSsAsString(vector<NAV *>& navs, int x, 
			 FlightData *p, int freq, float radial, 
			 AtlasString& str)
{
    // To make our life easier and presentation nicer, we first group
    // navaids based on ID.  Then we sort the groups based on
    // localizer strength.
    map<string, vector<NAV *> > groups;

    for (unsigned int i = 0; i < navs.size(); i++) {
	NAV *n = navs[i];
	groups[n->id].push_back(n);
    }

    vector<NAV *> *chosen = NULL;
    const string* id = NULL;
    double weakest;
    for (map<string, vector<NAV *> >::iterator i = groups.begin(); 
	 i != groups.end(); i++) {
	vector<NAV *>& components = i->second;

	for (unsigned int j = 0; j < components.size(); j++) {
	    NAV *n = components[j];
	    if (n->navtype == NAV_ILS) {
		// To prevent divide-by-zero errors, we arbitrarily
		// set 1 metre as the minimum distance.
		double d = 
		    SG_MAX2(sgdDistanceSquaredVec3(p->cart, n->bounds.center), 
			    1.0);
		d *= d;
		double s = (double)n->range * (double)n->range / d;
		if ((chosen == NULL) || (s < weakest)) {
		    chosen = &components;
		    id = &(i->first);
		    weakest = s;
		}
	    }
	}
    }

    str.printf("NAV");
    if (x > 0) {
	str.appendf(" %d", x);
    }
    str.appendf(": %.2f@%.0f%C", freq / 1000.0, radial, degreeSymbol);

    // Navaid information
    if (chosen != NULL) {
	NAV *loc = NULL, *dme = NULL;
	double ar, d;
	char *magTrue = "T";

	for (unsigned int j = 0; j < chosen->size(); j++) {
	    NAV *n = chosen->at(j);
	    if (n->navtype == NAV_ILS) {
		loc = n;

		double endHdg, length;
		geo_inverse_wgs_84(p->lat, p->lon, 
				   n->lat, n->lon, &ar, &endHdg, &length);
		if (globals.magnetic) {
		    magTrue = "";
		    ar -= magneticVariation(loc->lat, loc->lon, loc->elev);
		}
	    } else if (n->navtype == NAV_DME) {
		dme = n;
		d = sgdDistanceVec3(p->cart, dme->bounds.center) - dme->magvar;
	    }
	}
	str.appendf(" (%s", id->c_str());

	if (loc != NULL) {
	    str.appendf(", %.0f%C%s", normalizeHeading(rint(ar), false), 
			degreeSymbol, magTrue);
	}

	if (dme != NULL) {
	    str.appendf(", %.1f DME", d * SG_METER_TO_NM);
	}

	str.appendf(")");

	if (groups.size() > 1) {
	    str.appendf(" ...");
	}
    }
}

// Renders the given navaids, which must match the given frequency, as
// a string.
static void NAVsAsString(vector<NAV *>& navs, int x, 
			 FlightData *p, int freq, float radial, 
			 AtlasString& str)
{
    // Use the frequency (kHz) to decide what we're looking at.  If <
    // 112000 and the hundreds digit is odd, then it is an ILS.
    // Otherwise, it is a VOR.  Note that "VOR" also includes DMEs,
    // and that an ILS can include a DME as well.
    int hundreds = (freq % 1000) / 100;
    if ((freq < 112000) && (hundreds & 0x1)) {
	// ILS
	ILSsAsString(navs, x, p, freq, radial, str);
	return;
    } else {
	VORsAsString(navs, x, p, freq, radial, str);
    }
}

// Renders the given navaids, which must match the given frequency, as
// a string.

// EYE - note about NDBs only, x = 0, why freq must be passed, ...
static void NDBsAsString(vector<NAV *>& ndbs, int x, 
			 FlightData *p, int freq, AtlasString& str)
{
    NAV *match = NULL;
    double strength = 0.0;

    // Find NDB with strongest signal.
    for (unsigned int i = 0; i < ndbs.size(); i++) {
	NAV *n = ndbs[i];

	// We assume that signal strength is proportional to the
	// square of the range, and inversely proportional to the
	// square of the distance.  But 'we' may be wrong.  To prevent
	// divide-by-zero errors, we arbitrarily set 1 metre as the
	// minimum distance.
	double d = SG_MAX2(sgdDistanceSquaredVec3(p->cart, n->bounds.center),
			   1.0);
	double s = (double)n->range * (double)n->range / d;

	if (s > strength) {
	    match = n;
	}
    }
    assert((ndbs.size() == 0) || (match != NULL));

    str.printf("ADF");
    if (x > 0) {
	str.appendf(" %d", x);
    }
    str.appendf(": %d", freq);

    if (ndbs.size() > 0) {
	// Calculate the absolute and relative bearings to the navaid
	// (the absolute bearing is given in magnetic or true
	// degrees).
	double ab, rb;
	double endHdg, length;
	geo_inverse_wgs_84(p->lat, p->lon,
			   match->lat, match->lon,
			   &ab, &endHdg, &length);
	rb = ab - p->hdg;
	char magTrue = 'T';
	if (globals.magnetic) {
	    magTrue = 'M';
	    ab = ab - 
		magneticVariation(p->lat, p->lon, p->alt * SG_FEET_TO_METER);
	}

	str.appendf(" (%s, %03.0f%C%cB, %03.0f%CRB)",
		    match->id.c_str(), 
		    normalizeHeading(rint(ab), false), degreeSymbol, magTrue,
		    normalizeHeading(rint(rb), false), degreeSymbol);
    }
    if (ndbs.size() > 1) {
	str.appendf(" ...");
    }
}

// Sets the text strings displayed in the information dialog, based on
// the current position in the current track (in globals.track).
void InfoUI::setText()
{
    FlightData *p = globals.currentPoint();
    if (p == (FlightData *)NULL) {
	return;
    }

    static AtlasString latStr, lonStr, altStr, hdgStr, spdStr, hmsStr,
	dstStr, vor1Str, vor2Str, adfStr;

    latStr.printf("Lat: %c%s", (p->lat < 0) ? 'S':'N', formatAngle(p->lat));
    lonStr.printf("Lon: %c%s", (p->lon < 0) ? 'W':'E', formatAngle(p->lon));
    if (globals.track()->isAtlasProtocol()) {
	char *magTrue = "T";
	double hdg = p->hdg;
	if (globals.magnetic) {
	    magTrue = "";
	    // EYE - use the time of the flight instead of current time?
	    hdg -= magneticVariation(p->lat, p->lon, p->alt * SG_FEET_TO_METER);
	}
	hdg = normalizeHeading(rint(hdg), false);
	hdgStr.printf("Hdg: %03.0f%C%s", hdg, degreeSymbol, magTrue);
	spdStr.printf("Speed: %.0f kt EAS", p->spd);
    } else {
	char *magTrue = "T";
	double hdg = p->hdg;
	if (globals.magnetic) {
	    magTrue = "";
	    // EYE - use the time of the flight instead of current time?
	    hdg -= magneticVariation(p->lat, p->lon, p->alt * SG_FEET_TO_METER);
	}
	hdg = normalizeHeading(rint(hdg), false);
	hdgStr.printf("Track: %.0f%C%s", hdg, degreeSymbol, magTrue);
	spdStr.printf("Speed: %.0f kt GS", p->spd);
    }
    altStr.printf("Alt: %.0f ft MSL", p->alt);
    int hours, minutes, seconds;
    seconds = lrintf(p->est_t_offset);
    hours = seconds / 3600;
    seconds -= hours * 3600;
    minutes = seconds / 60;
    seconds -= minutes * 60;
    hmsStr.printf("Time: %d:%02d:%02d", hours, minutes, seconds);
    dstStr.printf("Dist: %.1f nm", p->dist * SG_METER_TO_NM);

    // Only the atlas protocol has navaid information.
    if (globals.track()->isAtlasProtocol()) {
	// Navaid information.  Printing a summary of navaid
	// information is complicated, because a single frequency can
	// match several navaids.  Sometimes this is because several
	// independent navaids are within range (this is unusual, but
	// possible), so we probably want to print information on the
	// nearest.  Sometimes it is because they form a "set" (eg, a
	// VOR-DME, an ILS system with a localizer, glideslope, and
	// DME, ...), in which case we want to print information on
	// the set as a whole.  And sometimes it is because of both
	// reasons (ILS systems with identical frequencies at opposite
	// ends of a runway).

	// Separate navaids based on frequency.
	vector<NAV *> VOR1s, VOR2s, NDBs;
	const vector<NAV *>& navaids = p->navaids();
	for (unsigned int i = 0; i < navaids.size(); i++) {
	    NAV *n = navaids[i];
	    if (p->nav1_freq == n->freq) {
		VOR1s.push_back(n);
	    } 
	    if (p->nav2_freq == n->freq) {
		VOR2s.push_back(n);
	    } 
	    if (p->adf_freq == n->freq) {
		NDBs.push_back(n);
	    }
	}
	// Create strings for each.
	NAVsAsString(VOR1s, 1, p, p->nav1_freq, p->nav1_rad, vor1Str);
	NAVsAsString(VOR2s, 2, p, p->nav2_freq, p->nav2_rad, vor2Str);
	NDBsAsString(NDBs, 0, p, p->adf_freq, adfStr);
    } else {
	vor1Str.printf("n/a");
	vor2Str.printf("n/a");
	adfStr.printf("n/a");
    }

    latText->setLabel(latStr.str());
    lonText->setLabel(lonStr.str());
    altText->setLabel(altStr.str());
    hdgText->setLabel(hdgStr.str());
    spdText->setLabel(spdStr.str());
    hmsText->setLabel(hmsStr.str());
    dstText->setLabel(dstStr.str());
    VOR1Text->setLabel(vor1Str.str());
    VOR2Text->setLabel(vor2Str.str());
    ADFText->setLabel(adfStr.str());
}

//////////////////////////////////////////////////////////////////////
// LightingUI
//////////////////////////////////////////////////////////////////////
LightingUI::LightingUI(int x, int y)
{
    // EYE - magic numbers
    const int bigSpace = 5;
    const int labelWidth = 75, labelHeight = 20;
    const int boxHeight = 55, boxWidth = 100;

    const int directionHeight = boxHeight + labelHeight + bigSpace * 3;
    const int smootherHeight = boxHeight, smootherWidth = bigSpace * 4;

    const int paletteHeight = labelHeight * 2 + bigSpace * 3;

    const int height = boxHeight * 3 + directionHeight + paletteHeight;
    const int width = labelWidth + boxWidth;

    const int paletteWidth = width - 2 * bigSpace - labelHeight;

    int curx, cury;

    gui = new puPopup(x, y); {
	frame = new puFrame(0, 0, width, height);

	curx = 0;
	cury = 0;

	//////////////////////////////////////////////////////////////////////
	// Palettes
	//////////////////////////////////////////////////////////////////////
	paletteFrame = 
	    new puFrame(curx, cury, curx + width, cury + paletteHeight);

	curx = bigSpace;
	cury += bigSpace;
	paletteComboBox = 
	    new puaComboBox(curx, cury,
			    curx + paletteWidth, cury + labelHeight,
			    NULL, FALSE);
	paletteComboBox->setCallback(lighting_cb);
	curx += paletteWidth;
	prevPalette = 
	    new puArrowButton(curx, cury + labelHeight / 2,
			      curx + labelHeight, cury + labelHeight,
			      PUARROW_UP);
	prevPalette->setCallback(lighting_cb);
	nextPalette = 
	    new puArrowButton(curx, cury,
			      curx + labelHeight, cury + labelHeight / 2,
			      PUARROW_DOWN);
	nextPalette->setCallback(lighting_cb);
	cury += labelHeight + bigSpace;

	curx = bigSpace;
	paletteLabel = new puText(curx, cury);
	paletteLabel->setLabel("Palette");
	cury += labelHeight + bigSpace;

	//////////////////////////////////////////////////////////////////////
	// Light direction
	//////////////////////////////////////////////////////////////////////

	// Frame surrounding azimuth/elevation stuff.
	curx = 0;
	directionFrame = 
	    new puFrame(curx, cury, curx + width, cury + directionHeight);

	// Azimuth dial and elevation slider.
	curx = bigSpace;
	cury += bigSpace;
	azimuthDial = new puDial(curx, cury, boxHeight, 0.0, 360.0, 1.0);
	azimuthDial->setLabelPlace(PUPLACE_UPPER_RIGHT);
	azimuthDial->setLabel("Azimuth");
	azimuthDial->setCallback(lighting_cb);

	curx = width - bigSpace - smootherWidth;
	elevationSlider = 
	    new puSlider(curx, cury, smootherHeight, TRUE, smootherWidth);
	elevationSlider->setLabelPlace(PUPLACE_LOWER_LEFT);
	elevationSlider->setLabel("Elevation");
	elevationSlider->setMinValue(0.0);
	elevationSlider->setMaxValue(90.0);
	elevationSlider->setStepSize(1.0);
	elevationSlider->setCallback(lighting_cb);

	// That this assumes that globals.lightPosition has already been
	// set and is normalized.  Note that on the azimuth dial, 0
	// degrees is down, 90 degrees left, 180 degrees up, and 270
	// degrees right.
	float azimuth = 
	    atan2(globals.lightPosition[1], globals.lightPosition[0]);
	azimuth = normalizeHeading(270.0 - azimuth * SG_RADIANS_TO_DEGREES);
	azimuthDial->setValue(azimuth);

	float elevation = 
	    asin(globals.lightPosition[2]) * SG_RADIANS_TO_DEGREES;
	elevationSlider->setValue(elevation);

	// Call setLightPosition() to set the dial and slider legends.
	setLightPosition();

	curx = bigSpace;
 	cury += boxHeight + bigSpace;
	directionLabel = new puText(curx, cury);
	directionLabel->setLabel("Light direction");

	cury = paletteHeight + directionHeight;

	//////////////////////////////////////////////////////////////////////
	// Lighting toggles
	//////////////////////////////////////////////////////////////////////

	// Smooth/flat polygon shading
	curx = 0;
	cury += boxHeight / 2;
	polygonsLabel = new puText(curx, cury);
	polygonsLabel->setLabel("Polygons");

	curx += labelWidth;
	cury -= boxHeight / 2;
	polygonsLabels[0] = "smooth";
	polygonsLabels[1] = "flat";
	polygonsLabels[2] = NULL;
	polygons = new puButtonBox(curx, cury, 
				   curx + boxWidth, cury + boxHeight,
				   (char **)polygonsLabels, TRUE);
	polygons->setCallback(lighting_cb);

	curx = 0;
	cury += boxHeight;

	// Lighting
	cury += boxHeight / 2;
	lightingLabel = new puText(curx, cury);
	lightingLabel->setLabel("Lighting");

	curx += labelWidth;
	cury -= boxHeight / 2;
	lightingLabels[0] = "on";
	lightingLabels[1] = "off";
	lightingLabels[2] = NULL;
	lighting = new puButtonBox(curx, cury, 
				   curx + boxWidth, cury + boxHeight,
				   (char **)lightingLabels, TRUE);
	lighting->setCallback(lighting_cb);

	curx = 0;
	cury += boxHeight;

	// Discrete/smoothed contour colours
	cury += boxHeight / 2;
	contoursLabel = new puText(curx, cury);
	contoursLabel->setLabel("Contours");

	curx += labelWidth;
	cury -= boxHeight / 2;
	contoursLabels[0] = "discrete";
	contoursLabels[1] = "smoothed";
	contoursLabels[2] = NULL;
	contours = new puButtonBox(curx, cury, 
				   curx + boxWidth, cury + boxHeight,
				   (char **)contoursLabels, TRUE);
	contours->setCallback(lighting_cb);
	contours->setLegend("you shouldn't see this");

	cury += boxHeight;
    }
    gui->close();
    gui->hide();
}

// Converts an azimuth and elevation (both in degrees) to a 4-vector.
// An azimuth of 0 degrees corresponds to north, 90 degrees to east.
// An elevation of 0 degrees is horizontal, 90 degrees is directly
// overhead.  In the 4-vector, position X is right, positive Y is up,
// and positive Z is towards the viewer.  W is always set to 0.0.
void LightingUI::azimElevToXYZW(float azimuth, float elevation, float *xyzw)
{
    float a = (90.0 - azimuth) * SG_DEGREES_TO_RADIANS;
    float e = elevation * SG_DEGREES_TO_RADIANS;
    xyzw[0] = cos(a) * cos(e);
    xyzw[1] = sin(a) * cos(e);
    xyzw[2] = sin(e);
    xyzw[3] = 0.0;
}

void LightingUI::getSize(int *w, int *h)
{
    gui->getSize(w, h);
}

void LightingUI::setPosition(int x, int y)
{
    gui->setPosition(x, y);
}

// Uses the azimuth dial and elevation slider values to set our
// current lighting vector.
void LightingUI::setLightPosition()
{
    // First, update the widget legends.  Note that on the azimuth
    // dial, 0 degrees is down, 90 degrees left, 180 degrees up, and
    // 270 degrees right.
    static AtlasString azStr, elStr;
    float azimuth = normalizeHeading(180.0 + azimuthDial->getFloatValue());
    float elevation = elevationSlider->getFloatValue();
    azStr.printf("%.0f", azimuth);
    azimuthDial->setLegend(azStr.str());

    elStr.printf("%.0f", elevation);
    elevationSlider->setLegend(elStr.str());

    azimElevToXYZW(azimuth, elevation, globals.lightPosition);
}

// Select the previous palette (if there is one).
void LightingUI::previous()
{
    unsigned int i = paletteComboBox->getCurrentItem();
    if (i > 0) {
	setPalette(i - 1);
    }
}

// Select the next palette (if there is one).
void LightingUI::next()
{
    unsigned int i = paletteComboBox->getCurrentItem();
    setPalette(i + 1);
}

// Updates the lighting interface so that the ith palette is selected.
// This does *not* actually select a palette - it just updates the
// interface.
void LightingUI::setPalette(unsigned int i)
{
    if (i >= palettes->size()) {
	i = palettes->size() - 1;
    }

    if (palettes->size() > 0) {
	paletteComboBox->setCallback(NULL);
	paletteComboBox->setCurrentItem(i);
	paletteComboBox->setCallback(lighting_cb);
    }

    if (i == 0) {
	prevPalette->greyOut();
    } else {
	prevPalette->activate();
    }
    if (i == (palettes->size() - 1)) {
	nextPalette->greyOut();
    } else {
	nextPalette->activate();
    }
}

// Populates the paletteComboBox with palette names.  The names are
// the filename part of the palette paths of the palettes held in the
// 'palettes' variable (in the house that Jack built).
void LightingUI::updatePalettes()
{
    // EYE - we should avoid work if we can - should we test if things
    // have really changed, or hold a _dirty variable?
    static char **paletteList = NULL;

    if (paletteList != NULL) {
	for (int i = 0; i < paletteComboBox->getNumItems(); i++) {
	    free(paletteList[i]);
	}
	free(paletteList);
    }

    const vector<Palette *>& p = palettes->palettes();
    paletteList = (char **)malloc(sizeof(char *) * p.size() + 1);
    for (unsigned int i = 0; i < p.size(); i++) {
	// Display the filename of the palette (but not the path).
	SGPath full(p[i]->path());
	paletteList[i] = strdup(full.file().c_str());
    }
    paletteList[p.size()] = (char *)NULL;

    paletteComboBox->newList(paletteList);

    // Select the current palette.
    setPalette(palettes->currentPaletteNo());
}

//////////////////////////////////////////////////////////////////////
// NetworkPopup
//////////////////////////////////////////////////////////////////////
NetworkPopup::NetworkPopup(int x, int y)
{
    const int buttonHeight = 20, buttonWidth = 80;
    const int bigSpace = 5;
    const int width = 350, height = 4 * buttonHeight + 5 * bigSpace;
    const int labelWidth = 50;

    int curx, cury;

    dialogBox = new puDialogBox(250, 150);
    {
	new puFrame(0, 0, width, height);

	// Cancel and ok buttons
	curx = bigSpace;
	cury = bigSpace;

	cancelButton = 
	    new puOneShot(curx, cury, curx + buttonWidth, cury + buttonHeight);
	cancelButton->setLegend("Cancel");
	cancelButton->setCallback(network_serial_cb);
	curx += buttonWidth + bigSpace;

	okButton = 
	    new puOneShot(curx, cury, curx + buttonWidth, cury + buttonHeight);
	okButton->setLegend("Ok");
	okButton->makeReturnDefault(TRUE);
	okButton->setCallback(network_serial_cb);

	// Network and serial input boxes
	curx = bigSpace + width / 2;
	cury += buttonHeight + bigSpace;
	baudInput = new puInput(curx + labelWidth, cury, 
				width - bigSpace, cury + buttonHeight);
	baudInput->setLabel("baud:");
	baudInput->setLabelPlace(PUPLACE_CENTERED_LEFT);
	baudInput->setValidData("0123456789");
	
	curx = bigSpace;
	cury += buttonHeight + bigSpace;
	portInput = new puInput(curx + labelWidth, cury, 
				width / 2 - bigSpace, cury + buttonHeight);
	portInput->setLabel("port:");
	portInput->setLabelPlace(PUPLACE_CENTERED_LEFT);
	portInput->setValidData("0123456789");

	curx = bigSpace + width / 2;
	deviceInput = new puInput(curx + labelWidth, cury, 
				  width - bigSpace, cury + buttonHeight);
	deviceInput->setLabel("device:");
	deviceInput->setLabelPlace(PUPLACE_CENTERED_LEFT);

	// Network/serial radio buttons
	curx = bigSpace;
	cury += buttonHeight + bigSpace;
	networkButton = new puButton(curx, cury, 
				     curx + buttonHeight, cury + buttonHeight);
	networkButton->setLabel("Network");
	networkButton->setLabelPlace(PUPLACE_CENTERED_RIGHT);
	networkButton->setButtonType(PUBUTTON_CIRCLE);
	networkButton->setCallback(network_serial_toggle_cb);

	curx += width / 2 + bigSpace;
	serialButton = new puButton(curx, cury, 
				     curx + buttonHeight, cury + buttonHeight);
	serialButton->setLabel("Serial");
	serialButton->setLabelPlace(PUPLACE_CENTERED_RIGHT);
	serialButton->setButtonType(PUBUTTON_CIRCLE);
	serialButton->setCallback(network_serial_toggle_cb);
    }
    dialogBox->close();
    dialogBox->reveal();
}

NetworkPopup::~NetworkPopup()
{
    puDeleteObject(dialogBox);
}

//////////////////////////////////////////////////////////////////////
// HelpUI
//////////////////////////////////////////////////////////////////////
HelpUI::HelpUI(int x, int y, Preferences& prefs, TileManager& tm)
{
    const int textHeight = 250;
    const int buttonHeight = 20;
    const int bigSpace = 5;
    const int width = 500, 
	height = 2 * buttonHeight + 4 * bigSpace + textHeight;
    const int guiWidth = width - 2 * bigSpace;

    int curx, cury;

    gui = new puPopup(250, 150);
    {
	new puFrame(0, 0, width, height);

	// Text
	curx = bigSpace;
	cury = bigSpace;

	// EYE - arrows value: 0, 1, 2?
	// EYE - scroller width?
	text = new puaLargeInput(curx, cury, guiWidth, textHeight, 2, 15);
	// EYE - what does this do?
// 	text->rejectInput();
	// Don't allow the user to change the text.
	text->disableInput();
	// Use a fixed-width font so that things line up nicely.
	text->setLegendFont(PUFONT_8_BY_13);

	// General button
	curx = bigSpace;
	cury += textHeight + bigSpace;
	generalButton = new puButton(curx, cury, "General");
	generalButton->setCallback(help_cb);
	
	// Keyboard shortcuts button
	int w, h;
	generalButton->getSize(&w, &h);
	curx += w + bigSpace;
	keyboardButton = new puButton(curx, cury, "Keyboard shortcuts");
	keyboardButton->setCallback(help_cb);

	// Version string and website
	curx = bigSpace;
	// EYE - how tall is a button by default?
	cury += buttonHeight + bigSpace;
	labelText = new puText(curx, cury);
	// EYE - magic website address
	// EYE - clicking it should start a browser
	globalString.printf("Atlas Version %s   http://atlas.sourceforge.net", 
			    VERSION);
	_label = strdup(globalString.str());
	labelText->setLabel(_label);
    }
    gui->close();
    gui->hide();

    // General information.
    globalString.printf("$FG_ROOT\n");
    globalString.appendf("    %s\n", prefs.fg_root.c_str());
    globalString.appendf("$FG_SCENERY\n");
    globalString.appendf("    %s\n", prefs.scenery_root.c_str());
    globalString.appendf("Atlas maps\n");
    globalString.appendf("    %s\n", prefs.path.c_str());
    globalString.appendf("Atlas palette\n");
    globalString.appendf("    %s\n", prefs.palette.c_str());

    globalString.appendf("\n");
    globalString.appendf("Maps\n");
    globalString.appendf("    %d maps/tiles\n", tm.tiles().size());
    globalString.appendf("    resolutions:\n");
    const bitset<TileManager::MAX_MAP_LEVEL>& levels = tm.mapLevels();
    for (unsigned int i = 0; i < TileManager::MAX_MAP_LEVEL; i++) {
	if (levels[i]) {
	    int x = 1 << i;
	    globalString.appendf("      %d (%dx%d)\n", i, x, x);
	}
    }

    globalString.appendf("\n");
    globalString.appendf("Airports\n");
    globalString.appendf("    %s/Airports/apt.dat.gz\n", prefs.fg_root.c_str());
    globalString.appendf("Navaids\n");
    globalString.appendf("    %s/Navaids/nav.dat.gz\n", prefs.fg_root.c_str());
    globalString.appendf("Fixes\n");
    globalString.appendf("    %s/Navaids/fix.dat.gz\n", prefs.fg_root.c_str());
    globalString.appendf("Airways\n");
    globalString.appendf("    %s/Navaids/awy.dat.gz\n", prefs.fg_root.c_str());

    _generalText = strdup(globalString.str());

    // Keyboard shortcuts.
    AtlasString fmt;
    fmt.printf("%%-%ds%%s\n", 8);
    globalString.printf(fmt.str(), "C-x c", "discrete/smooth contours");
    globalString.appendf(fmt.str(), "C-x l", "toggle lighting");
    globalString.appendf(fmt.str(), "C-x p", "smooth/flat polygon shading");
//     globalString.appendf("C-space\n");
    globalString.appendf(fmt.str(), "C-n", "next flight track");
    globalString.appendf(fmt.str(), "C-p", "previous flight track");
    globalString.appendf(fmt.str(), "+", "zoom in");
    globalString.appendf(fmt.str(), "-", "zoom out");
    globalString.appendf(fmt.str(), "?", "toggle this help");
    globalString.appendf(fmt.str(), "a", "toggle airways");
    globalString.appendf(fmt.str(), "A", "toggle airports");
    globalString.appendf(fmt.str(), "c", "centre the map on the mouse");
    globalString.appendf(fmt.str(), "d", "toggle info interface and graphs window");
    globalString.appendf(fmt.str(), "f", "toggle flight tracks");
    globalString.appendf(fmt.str(), "j", "toggle search interface");
    globalString.appendf(fmt.str(), "l", "toggle lighting interface");
    globalString.appendf(fmt.str(), "m", "toggle mouse and centre mode");
    globalString.appendf(fmt.str(), "M", "toggle MEF display");
    globalString.appendf(fmt.str(), "n", "make north point up");
    globalString.appendf(fmt.str(), "N", "toggle navaids");
    globalString.appendf(fmt.str(), "o", "open a flight file");
    globalString.appendf(fmt.str(), "p", "centre map on aircraft");
    globalString.appendf(fmt.str(), "P", "toggle auto-centering");
    globalString.appendf(fmt.str(), "q", "quit");
    globalString.appendf(fmt.str(), "s", "save current track");
    globalString.appendf(fmt.str(), "w", "close current flight track");
    globalString.appendf(fmt.str(), "u", "detach (unattach) current connection");
    globalString.appendf(fmt.str(), "v", "toggle labels");
    globalString.appendf(fmt.str(), "space", "toggle main interface");

    _keyboardText = strdup(globalString.str());

    // General information is display by default.
    setText(generalButton);
}

HelpUI::~HelpUI()
{
    puDeleteObject(gui);
    delete _label;
    delete _generalText;
    delete _keyboardText;
}

void HelpUI::setText(puObject *obj)
{
    if (obj == generalButton) {
	generalButton->setValue(1);
	keyboardButton->setValue(0);
	text->setValue(_generalText);
    } else if (obj == keyboardButton) {
	generalButton->setValue(0);
	keyboardButton->setValue(1);
	text->setValue(_keyboardText);
    }
}

///////////////////////////////////////////////////////////////////////////////
// GLUT event handlers
///////////////////////////////////////////////////////////////////////////////

// Called when the main window is resized.
void reshapeMap(int _width, int _height)
{
    glViewport (0, 0, (GLsizei) _width, (GLsizei) _height); 
    zoomBy(1.0);

    // Ensure that the 'jump to location' widget stays in the upper
    // right corner ...
    int w, h;
    search_interface->getSize(&w, &h);
    search_interface->setPosition(_width - w, _height - h);

    // ... and that the lighting UI stays in the lower right corner
    // (with a 20-pixel space at the bottom and right).
    lightingUI->getSize(&w, &h);
    lightingUI->setPosition(_width - w - 20, 20);
}

void bjsBitmapString(int x, int y, void *font, const char *str)
{
    GLfloat viewport[4];

    glGetFloatv(GL_VIEWPORT, viewport);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix(); {
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix(); {
	    glLoadIdentity();
	    gluOrtho2D(0.0, viewport[2], 0.0, viewport[3]);

	    glColor3f(1.0, 1.0, 1.0);
	    glRasterPos2i(x, y);
	    for (unsigned int i = 0; i < strlen(str); i++) {
		// 	glRasterPos2i(x, y);
		glutBitmapCharacter(font, str[i]);
		// 	x += glutBitmapWidth(font, str[i]);
	    }
	}
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
    }
    glPopMatrix();
}

void redrawMap() 
{
    assert(glutGetWindow() == main_window);
  
    // EYE - do this in a move routine, or in MainUI
    SGGeod geod;
    bool validElevation;
    if (mainUI->showMouse) {
	geod = cursor.geod;
	validElevation = cursor.validElevation;
    } else {
	GLfloat viewport[4];
	glGetFloatv(GL_VIEWPORT, viewport);
	GLfloat width = viewport[2];
	GLfloat height = viewport[3];
	// EYE - another strange correction (see also
	// Scenery::intersection()).  Why?
	double x = width / 2.0 + 1.0, y = height / 2.0;
	SGVec3<double> cart;
	// We ignore the return value because we know the eye point is
	// always valid.
	scenery->intersection(x, y, &cart, &validElevation);
	SGGeodesy::SGCartToGeod(cart, geod);
    }

    AtlasString latStr, lonStr, elevStr;
    if (!mainUI->latInput->isAcceptingInput()) {
	double lat = geod.getLatitudeDeg();
	latStr.printf("%c%s", (lat < 0) ? 'S' : 'N', formatAngle(lat));
	mainUI->latInput->setValue(latStr.str());
    }

    if (!mainUI->lonInput->isAcceptingInput()) {
	double lon = geod.getLongitudeDeg();
	lonStr.printf("%c%s", (lon < 0) ? 'W' : 'E', formatAngle(lon));
	mainUI->lonInput->setValue(lonStr.str());
    }

    if (validElevation) {
	elevStr.printf("Elev: %.0f ft", geod.getElevationFt());
    } else {
	elevStr.printf("Elev: n/a");
    }
    mainUI->elevText->setLabel(elevStr.str());
    if (!mainUI->zoomInput->isAcceptingInput()) {
	mainUI->zoomInput->setValue((float)globals.metresPerPixel);
    }

    // Check errors before...
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
	printf("display (before): %s\n", gluErrorString(error));
    }

    // Clear all pixels and depth buffer.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // The status of the crosshairs overlay depends on the current
    // track, and other overlays.  Draw a crosshair if:
    //
    // (a) there's no track, OR
    // (b) the tracks layer is off, OR
    // (c) there's a track, but we don't auto-centre on it
    //
    // In other words, if there's a track AND the tracks layer is on
    // AND we're auto-centering, then DON'T draw the crosshair.

    // EYE - turn off crosshairs in mouse mode too?
    bool on = !(globals.track() &&
		mainUI->tracksToggle->getIntegerValue() &&
		mainUI->trackAircraftToggle->getIntegerValue());
    globals.overlays->setVisibility(Overlays::CROSSHAIRS, on);

    // Scenery.
    scenery->draw(elevationLabels);

    // Overlays.
    globals.overlays->draw();

    // Render the widgets.
    puDisplay();

    glutSwapBuffers();

    // ... and at the end.
    error = glGetError();
    if (error != GL_NO_ERROR) {
	printf("display (after): %s\n", gluErrorString(error));
    }
}

// Display function for the graphs window.
void redrawGraphs() 
{
    if (globals.track()) {
	graphs->draw();
	glutSwapBuffers();
    }
}

// Called when the graphs window is resized.
void reshapeGraphs(int w, int h)
{
    glViewport(0, 0, (GLsizei) w, (GLsizei) h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, (GLdouble) w, 0.0, (GLdouble) h);

    graphs->reshape(w, h);
    glutPostRedisplay();
}

// Called for mouse motion events (when a mouse button is depressed)
// in the graphs window.
void motionGraphs(int x, int y) 
{
    // Marks are only honoured for non-live tracks.
    if (!globals.track()->live()) {
	globals.track()->setMark(graphs->pixelToPoint(x));

	glutSetWindow(main_window);
	aircraftMoved();
	glutPostRedisplay();

	glutPostWindowRedisplay(graphs_window);
    }
}

// Called for mouse button events in the graphs window.
void mouseGraphs(int button, int state, int x, int y)
{
    // EYE - should we look at the button and state eventually?
    if (state == GLUT_DOWN) {
	motionGraphs(x, y);
    }
}

void keyPressed(unsigned char key, int x, int y);

// Called when the user presses a key in the graphs window.  We just
// pass the key on to the handler for the main window.
void keyboardGraphs(unsigned char key, int x, int y)
{
    // EYE - keyPressed does a call to puKeyboard.  Is this okay
    // (especially if we do the same in the future)?
    glutSetWindow(main_window);
    keyPressed(key, x, y);
    glutSetWindow(graphs_window);
}

// Called when the user presses a "special" key in the graphs window,
// where "special" includes directional keys.
void specialGraphs(int key, int x, int y) 
{
    // The special keys all adjust the mark.  However, live tracks
    // have no mark.
    if (globals.track()->live()) {
	return;
    }

    int offset = 1;
    if (glutGetModifiers() & GLUT_ACTIVE_SHIFT) {
	// If the user presses the shift key, right and left arrow
	// clicks move 10 times as far.
	offset *= 10;
    }

    switch (key + PU_KEY_GLUT_SPECIAL_OFFSET) {
      case PU_KEY_LEFT:
	if (globals.track()->mark() >= offset) {
	    globals.track()->setMark(globals.track()->mark() - offset);
	} else {
	    globals.track()->setMark(0);
	}
	break;
      case PU_KEY_RIGHT:
	if (globals.track()->mark() < (globals.track()->size() - offset)) {
	    globals.track()->setMark(globals.track()->mark() + offset);
	} else {
	    globals.track()->setMark(globals.track()->size() - 1);
	}
	break;
      case PU_KEY_HOME:
	globals.track()->setMark(0);
	break;
      case PU_KEY_END:
	globals.track()->setMark(globals.track()->size() - 1);
	break;
      default:
	return;
    }

    glutSetWindow(main_window);
    aircraftMoved();
    glutPostRedisplay();

    glutSetWindow(graphs_window);
    glutPostRedisplay();
}

// Called periodically to check for input on network and serial ports.
void timer(int value) 
{
    // Ensure that the main window is the current window.
    glutSetWindow(main_window);

    // Check for input on all live tracks.
    for (unsigned int i = 0; i < globals.tracks().size(); i++) {
	if (globals.track(i)->live() && globals.track(i)->checkForInput()) {
	    // Is this track is the currently displayed track?
	    if (globals.track(i) == globals.track()) {
		// Register that the aircraft has moved (this will
		// update the display as necessary).
		aircraftMoved();

		// Update the number of points in the track on the
		// main UI.
		mainUI->setTrackSize(globals.track()->size());

		flightTrackModified();

		// And update our graphs.
		glutPostWindowRedisplay(graphs_window);
	    }

	    // Just in case the main window has changed.
	    glutPostRedisplay();
	}
    }

    // Check again later.
    glutTimerFunc((int)(prefs.update * 1000.0f), timer, value);
}

bool dragging = false;
SGVec3<double> oldC;

void mouseClick(int button, int state, int x, int y) 
{
    // EYE - how can I drag puGroups?
    if (puMouse(button, state, x, y)) {
	glutPostRedisplay();
    } else {
	// PUI didn't consume this event
	switch (button) {
	  case GLUT_LEFT_BUTTON:
	    if (state == GLUT_DOWN) {
		if (cursor.validLocation) {
		    oldC = cursor.cart;
		    dragging = true;
		}
	    } else {
		dragging = false;
	    }
	    break;
	  case 3:		// WM_MOUSEWHEEL (away)
	    if (state == GLUT_DOWN) {
		keyPressed('-', x, y);
	    }
	    break;
	  case 4:		// WM_MOUSEWHEEL (towards)
	    if (state == GLUT_DOWN) {
		keyPressed('+', x, y);
	    }
	    break;
	  default:
	    break;
	}
    }
}

void mouseMotion(int x, int y) 
{
    cursor.x = x;
    cursor.y = y;

    if (dragging) {
	cursor.x = x;
	cursor.y = y;
	SGVec3<double> newC;
	if (scenery->intersection(x, y, &newC)) {
	    // The two vectors, oldC and newC, define the plane and
	    // angle of rotation.  A line perpendicular to this plane,
	    // passing through the origin, is our axis of rotation.
	    sgdVec3 axis;
	    sgdMat4 rot;

	    sgdVectorProductVec3(axis, newC.sg(), oldC.sg());
	    double theta = SGD_RADIANS_TO_DEGREES *
		atan2(sgdLengthVec3(axis), 
		      sgdScalarProductVec3(oldC.sg(), newC.sg()));
	    sgdMakeRotMat4(rot, theta, axis);
	    
	    // Transform the eye point and the camera up vector.

	    // EYE - this calls updateCursor(), which is redundant
	    // (since the cursor's cartesian coordinates haven't
	    // changed).
	    rotatePosition(rot);
	}
    } else {
	// EYE - which policy is correct?
// 	puMouse(x, y);
	if (puMouse(x, y)) {
	    glutPostRedisplay();
	}
    }
}

void passivemotion(int x, int y) 
{
    cursor.x = x;
    cursor.y = y;
    cursorUpdate();

    glutPostRedisplay();
}

void prefixKeypressed(unsigned char key, int x, int y)
{
    switch (key) {
      case 'c':			// Discrete/smooth contours
	lightingUI->contours->setValue(!lightingUI->contours->getValue());
	lighting_cb(lightingUI->contours);
	break;
      case 'l':			// Lighting on/off
	lightingUI->lighting->setValue(!lightingUI->lighting->getValue());
	lighting_cb(lightingUI->lighting);
	break;
      case 'p':			// Smooth/flat polygon shading
	lightingUI->polygons->setValue(!lightingUI->polygons->getValue());
	lighting_cb(lightingUI->polygons);
	break;
      default:
	return;
    }
}

void keyPressed(unsigned char key, int x, int y) 
{
    static bool prefixKey = false;

    if (prefixKey) {
	prefixKeypressed(key, x, y);
	prefixKey = false;

	return;
    }

    if (!puKeyboard(key, PU_DOWN)) {
	switch (key) {
	  case '':
	    // Ctrl-x is a prefix key (a la emacs).
	    prefixKey = true;
	    break;

	  case '':
	    // Next flight track.
	    track_select_cb(mainUI->nextTrackButton);
	    break;

	  case '':
	    // Previous flight track.
	    track_select_cb(mainUI->prevTrackButton);
	    break;

	  case '+':
	    zoomBy(1.0 / zoomFactor);
	    break;

	  case '-':
	    zoomBy(zoomFactor);
	    break;

	  case '?':
	    // Help dialog.
	    if (helpUI->isVisible()) {
		helpUI->hide();
	    } else {
		helpUI->reveal();
	    }
	    glutPostRedisplay();
	    break;

	  case 'a':
	    // Toggle airways.
	    mainUI->airwaysToggle->setValue(!mainUI->airwaysToggle->getValue());
	    show_cb(mainUI->airwaysToggle);
	    glutPostRedisplay();
	    break;

	  case 'A':
	    // Toggle airports.
	    mainUI->airportsToggle->setValue(!mainUI->airportsToggle->getValue());
	    show_cb(mainUI->airportsToggle);
	    glutPostRedisplay();
	    break;

	  case 'c': 
	    // Center the map on the current mouse position.
	    if (cursor.validLocation) {
		movePosition(cursor.cart.sg());
	    }
	    break;

	  case 'd':
	    // Hide/show the info interface and the graphs window.
	    if (globals.tracks().size() > 0) {
		if (!infoUI->isVisible()) {
		    glutSetWindow(graphs_window);
		    glutShowWindow();

		    glutSetWindow(main_window);
		    infoUI->reveal();

		    showGraphWindow = true;
		} else {
		    glutSetWindow(graphs_window);
		    glutHideWindow();

		    glutSetWindow(main_window);
		    infoUI->hide();

		    showGraphWindow = false;
		}
		glutPostRedisplay();
	    }
	    break;

	  case 'f':
	    // Toggle flight tracks.
	    mainUI->tracksToggle->setValue(!mainUI->tracksToggle->getValue());
	    show_cb(mainUI->tracksToggle);
	    glutPostRedisplay();
	    break;

	  case 'j':
	    // Toggle the search interface.
	    if (search_interface->isVisible()) {
		search_interface->hide();
	    } else {
		search_interface->reveal();
		// Record where we were when the search started.  If
		// the search is cancelled, we'll return to this
		// point.
		sgdCopyVec3(searchFrom, eye);
		// If we had a previous search and have moved in the
		// meantime, we'd like to see the results resorted
		// according to their distance from the new eyepoint.
		// We call searchStringChanged() explicitly to do
		// this.
		if (strlen(search_interface->searchString()) > 0) {
		    searchStringChanged(NULL, "");
		}
	    }
	    glutPostRedisplay();
	    break;

	  case 'm':
	    // Toggle between mouse and centre modes.
	    mainUI->showMouse = !mainUI->showMouse;
	    show_cb(mainUI->mouseText);
	    glutPostRedisplay(); // EYE - overkill
	    break;

	  case 'l':
	    // Turn lighting UI on/off.
	    if (lightingUI->isVisible()) {
		lightingUI->hide();
	    } else {
		lightingUI->reveal();
	    }
	    glutPostRedisplay();
	    break;

	  case 'M':
	    // Toggle MEF display on/off.
	    elevationLabels = !elevationLabels;
	    mainUI->MEFToggle->setValue(elevationLabels);
	    glutPostRedisplay();
	    break;

	  case 'N':
	    // Toggle navaids.
	    mainUI->navaidsToggle->setValue(!mainUI->navaidsToggle->getValue());
	    show_cb(mainUI->navaidsToggle);
	    break;    

	  case 'n':
	    // Rotate camera so that north is up.
	    rotateEye();
	    break;

	  case 'o':
	    // Open a flight file (unless the file dialog is already
	    // active doing something else).
	    load_cb(NULL);
	    break;

	  case 'P':
	    // Toggle auto-centering.
	      {
		  puObject *toggle = mainUI->trackAircraftToggle;
		  toggle->setValue(!toggle->getIntegerValue());
		  track_aircraft_cb(toggle);
		  glutPostRedisplay();
	      }
	    break;

	  case 'p':
	    centerMapOnAircraft();
	    break;

	  case 'q':
	    // Quit
	      {
		  // If there are unsaved tracks, warn the user first.
		  bool modifiedTracks = false;
		  for (unsigned int i = 0; i < globals.tracks().size(); i++) {
		      if (globals.track(i)->modified()) {
			  modifiedTracks = true;
			  break;
		      }
		  }
		  if (modifiedTracks) {
		      // Create a warning dialog.
		      makeDialog("You have unsaved tracks.\nIf you exit now, they will be lost.\nDo you want to exit?", exit_ok_cb);
		  } else {
		      // EYE - we should delete allocated objects
		      // first!  This is another argument for a
		      // AtlasController object.
		      exit(0);
		  }
	      }
	    break;

	  case 's':
	    // Save the current track.
	    if (!globals.track()) {
		break;
	    }
	    if (globals.track()->hasFile()) {
		// If it has a file, save without questions.
		save_cb((puObject *)NULL);
	    } else if (fileDialog == NULL) {
		// If it has no file, and the 'save as' dialog isn't
		// active, bring it up.
		save_as_cb((puObject *)NULL);
	    }
	    break;

	  case 'w':
	    // Close (unload) a flight track.
	    unload_cb(mainUI->unloadButton);
	    break;

	  case 'u':
	    // 'u'nattach (ie, detach)
	    detach_cb(mainUI->detachButton);
	    break;

	  case 'v':
	    // Toggle labels.
	    mainUI->labelsToggle->setValue(!mainUI->labelsToggle->getValue());
	    show_cb(mainUI->labelsToggle);
	    break;

	  case 'x':
	    // Toggle x-axis type (time, distance)
	    infoUI->xAxisBox->setValue(!infoUI->xAxisBox->getValue());
	    graph_axis_cb(infoUI->xAxisBox);
	    break;

	  case ' ':
	    // Toggle main interface.
	    if (!mainUI->gui->isVisible()) {
		mainUI->gui->reveal();
	    } else {
		mainUI->gui->hide();
	    }
	    glutPostRedisplay();
	}
    } else {
	// EYE - really?
	glutPostRedisplay();
    }
}

// Called when 'special' keys are pressed.
void specPressed(int key, int x, int y) 
{
    // We give our widgets a shot at the key first, via puKeyboard.
    // If it returns FALSE (ie, none of the widgets eat the key), and
    // if there's a track being displayed, then pass it on to the
    // special key handler of the graph window and give it a shot.
    if (!puKeyboard(key + PU_KEY_GLUT_SPECIAL_OFFSET, PU_DOWN) && 
	globals.track()) {
    	specialGraphs(key, x, y);
    }
}

// Sets up global lighting parameters.
void setLighting()
{
    globals.discreteContours = prefs.discreteContours;
    globals.lightingOn = prefs.lightingOn;
    globals.smoothShading = prefs.smoothShading;
    // Convert azimuth, elevation to x, y, z, w.
    LightingUI::azimElevToXYZW(prefs.azimuth, prefs.elevation, 
			       globals.lightPosition);
}

void initStandardOpenGLAttribs()
{
    // Standard settings for lines and points.  If you change any of
    // these, you *must* return them to their original value after!
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0);
    glPointSize(1.0);
}

// Does graphics-related initialization of the main window.
void init()
{
    // Initalize scenery object.  Note that we specify that all
    // scenery should be displayed in the main window.
    scenery = new Scenery(tileManager, main_window);

    // Background map image.
    SGPath world = prefs.path;
    world.append("background");
    scenery->setBackgroundImage(world);

    globals.overlays = new Overlays(prefs.fg_root.str());

    //  Select clearing (background) color
    glClearColor(0.0, 0.0, 0.0, 0.0);

    setLighting();
    glEnable(GL_DEPTH_TEST);

    initStandardOpenGLAttribs();

    // EYE - If I do this, I get a wire frame - mostly.  Some polygons
    // still are drawn, which seems to indicate that they are drawn
    // clockwise instead of counter-clockwise.  Text is rendered
    // correctly, though, which seems to suggest that they are
    // actually backwards too.
//     glPolygonMode(GL_FRONT, GL_LINE);

    // EYE - probably we should make this a class with a constructor.
    cursor.x = cursor.y = -1;
    cursor.validLocation = false;

}

// I don't know if this constitues a hack or not, but doing something
// like:
//
// glutShowWindow();
// glutPostRedisplay();
//
// doesn't always redisplay the window.  The window comes up, but GLUT
// doesn't call its display function.  
//
// It seems that GLUT doesn't really consider the window visible until
// later (perhaps after getting some sort of notification from the
// native windowing system).  In any case, by creating visibility
// functions and calling for a window redisplay when it becomes
// visible, we ensure that GLUT passes along the redisplay
// notification.
void mainVisibilityFunc(int state)
{
    if (state == GLUT_VISIBLE) {
	glutPostWindowRedisplay(main_window);
    }
}

void graphVisibilityFunc(int state)
{
    if (state == GLUT_VISIBLE) {
	glutPostWindowRedisplay(graphs_window);
    }
}

///////////////////////////////////////////////////////////////////////////////
// PUI code (callbacks)
///////////////////////////////////////////////////////////////////////////////

void smoother_cb(puObject *smoother) 
{
    // This must be static, as PUI will access it later.
    static AtlasString buf;
    buf.printf("%d", smoother->getIntegerValue());
    smoother->setLegend(buf.str());

    // Update the graphs.
    graphs->setSmoothing(smoother->getIntegerValue());
    glutSetWindow(graphs_window);
    glutPostRedisplay();

    // Update the interface.
    glutSetWindow(main_window);
    glutPostRedisplay();
}

void graph_axis_cb(puObject *xAxisBox)
{
    // Update the graphs based on the value in the xAxisBox.

    // EYE - magic number
    if (xAxisBox->getValue() == 0) {
	graphs->setXAxisType(Graphs::TIME);
    } else {
	graphs->setXAxisType(Graphs::DISTANCE);
    }
    glutSetWindow(graphs_window);
    glutPostRedisplay();

    // Update the interface.
    glutSetWindow(main_window);
    glutPostRedisplay();
}

void lighting_cb(puObject *lightingUIObject)
{
    if (lightingUIObject == lightingUI->contours) {
	globals.discreteContours = (lightingUI->contours->getValue() == 0);
	Notification::notify(Notification::DiscreteContours);
    } else if (lightingUIObject == lightingUI->lighting) {
	globals.lightingOn = (lightingUI->lighting->getValue() == 0);
    } else if (lightingUIObject == lightingUI->polygons) {
	globals.smoothShading = (lightingUI->polygons->getValue() == 0);
    } else if (lightingUIObject == lightingUI->azimuthDial) {
	lightingUI->setLightPosition();
    } else if (lightingUIObject == lightingUI->elevationSlider) {
	lightingUI->setLightPosition();
    } else if (lightingUIObject == lightingUI->paletteComboBox) {
	lightingUI->setPalette(lightingUI->currentItem());
	palettes->setPalette(lightingUI->currentItem());
	globals.palette = palettes->currentPalette();
	Notification::notify(Notification::NewPalette);
    } else if (lightingUIObject == lightingUI->prevPalette) {
	lightingUI->previous();
	palettes->setPalette(lightingUI->currentItem());
	globals.palette = palettes->currentPalette();
	Notification::notify(Notification::NewPalette);
    } else if (lightingUIObject == lightingUI->nextPalette) {
	lightingUI->next();
	palettes->setPalette(lightingUI->currentItem());
	globals.palette = palettes->currentPalette();
	Notification::notify(Notification::NewPalette);
    }

    glutPostRedisplay();
}

void zoom_cb(puObject *cb)
{ 
    if (cb == mainUI->zoomInButton) {
	zoomBy(1.0 / zoomFactor);
    } else if (cb == mainUI->zoomOutButton) {
	zoomBy(zoomFactor);
    } else if (cb == mainUI->zoomInput) {
	// Read zoom level from zoom input field.
	char *buffer;
	cb->getValue(&buffer);
	double zoom;
	int n_items = sscanf(buffer, "%lf", &zoom);
	if (n_items != 1) {
	    return;
	}
	zoomTo(zoom);
    }
    glutPostRedisplay();
}

void show_cb(puObject *cb)
{
    bool on = (cb->getValue() != 0);
    if (cb == mainUI->navaidsToggle) {
	globals.overlays->setVisibility(Overlays::NAVAIDS, on);
	if (on) {
	    mainUI->navVOR->activate();
	    mainUI->navNDB->activate();
	    mainUI->navILS->activate();
	    mainUI->navDME->activate();
	    mainUI->navFIX->activate();
	} else {
	    mainUI->navVOR->greyOut();
	    mainUI->navNDB->greyOut();
	    mainUI->navILS->greyOut();
	    mainUI->navDME->greyOut();
	    mainUI->navFIX->greyOut();
	}
    } else if (cb == mainUI->navVOR) {
	globals.overlays->setVisibility(Overlays::VOR, on);
    } else if (cb == mainUI->navNDB) {
	globals.overlays->setVisibility(Overlays::NDB, on);
    } else if (cb == mainUI->navILS) {
	globals.overlays->setVisibility(Overlays::ILS, on);
    } else if (cb == mainUI->navDME) {
	globals.overlays->setVisibility(Overlays::DME, on);
    } else if (cb == mainUI->navFIX) {
	globals.overlays->setVisibility(Overlays::FIXES, on);
    } else if (cb == mainUI->airportsToggle) {
	globals.overlays->setVisibility(Overlays::AIRPORTS, on);
    } else if (cb == mainUI->airwaysToggle) {
	globals.overlays->setVisibility(Overlays::AIRWAYS, on);
	if (on) {
	    mainUI->awyHigh->activate();
	    mainUI->awyLow->activate();
	} else {
	    mainUI->awyHigh->greyOut();
	    mainUI->awyLow->greyOut();
	}
    } else if (cb == mainUI->awyHigh) {
	globals.overlays->setVisibility(Overlays::HIGH, on);
    } else if (cb == mainUI->awyLow) {
	globals.overlays->setVisibility(Overlays::LOW, on);
    } else if (cb == mainUI->labelsToggle) {
	// EYE - this doesn't toggle the labels of fixes.  Should it?
	globals.overlays->setVisibility(Overlays::LABELS, on);
    } else if (cb == mainUI->tracksToggle) {
	globals.overlays->setVisibility(Overlays::TRACKS, on);
    } else if (cb == mainUI->MEFToggle) {
	elevationLabels = on;
    } else if (cb == mainUI->mouseText) {
	if (mainUI->showMouse) {
	    mainUI->mouseText->setLabel("mouse");
	} else {
	    mainUI->mouseText->setLabel("centre");
	}
    }

    glutPostRedisplay();
}

void position_cb(puObject *cb) 
{
    char *buffer;
    cb->getValue(&buffer);

    char ns, deg_ch, min_ch = ' ', sec_ch = ' ';
    float degrees = 0.0, minutes = 0.0, seconds = 0.0;

    // Free-format entry: "N51", "N50.99*", "N50*59 24.1", etc.
    int n_items = 
	sscanf(buffer, " %c %f%c %f%c %f%c",
	       &ns, &degrees, &deg_ch, &minutes, &min_ch, &seconds, &sec_ch);
    if (n_items < 2) {
	return;
    }
    float angle = (degrees + minutes / 60 + seconds / 3600);
    double latitude, longitude;
    // EYE - do this in a move() routine, or just read the value from
    // the text field.
    SGGeod geod;
    SGGeodesy::SGCartToGeod(SGVec3<double>(eye), geod);
    if (cb == mainUI->latInput) {
	latitude = ((ns=='S'||ns=='s')?-1.0f:1.0f) * angle;
	longitude = geod.getLongitudeDeg();
    } else {
	longitude = ((ns=='W'||ns=='w')?-1.0f:1.0f) * angle;
	latitude = geod.getLatitudeDeg();
    }
    movePosition(latitude, longitude);
    glutPostRedisplay();
}

void clear_ftrack_cb(puObject *) 
{
    // EYE - disallow clears of file-based flight tracks?
    if (globals.track() != NULL) {
	globals.track()->clear();
	mainUI->setTrackSize(globals.track()->size());
	flightTrackModified();
    }

    glutPostWindowRedisplay(graphs_window);
    glutPostRedisplay();
}

void degMinSec_cb(puObject *cb)
{
    // EYE - glutPostRedisplay() doesn't seem to be necessary - is
    // PLIB smart enough to update its widgets?
    // EYE - might it be better to set the value based on the widget?
    // EYE - really need some kind of notification mechanism
//     mainUI->degMinSec = !mainUI->degMinSec;
    mainUI->setDMS(!mainUI->degMinSec);
    infoUI->setText();
}

void magTrue_cb(puObject *cb)
{
    // EYE - see notes in degMinSec_cb
    globals.magnetic = !globals.magnetic;
    mainUI->setMagnetic(globals.magnetic);
    infoUI->setText();
    Notification::notify(Notification::MagTrue);
}

// EYE - this counts on the Graphs type values being same as the
// corresponding entries in the button box.
void graph_type_cb (puObject *graphsBox) 
{
    // EYE - need to check if there's a track to display too
    glutSetWindow(graphs_window);
    if (graphsBox->getValue() == 0) {
	glutHideWindow();
    } else {
	graphs->setGraphTypes(graphsBox->getValue());
	glutShowWindow();
	glutPostRedisplay();
    }

    glutSetWindow(main_window);
}

// Called when the user wants to 'save as' a file.
void save_as_cb(puObject *cb)
{
    if (fileDialog == NULL) {
	mainUI->saveAsButton->greyOut();
	fileDialog = new puaFileSelector(250, 150, 500, 400, "", 
					 "Save Flight Track");
	fileDialog->setCallback(save_as_file_cb);
	glutPostRedisplay();
    }
}

// Called when the user presses OK or Cancel on the save file dialog.
void save_as_file_cb(puObject *cb)
{
    // If the user hit "Ok", then the string value of the save dialog
    // will be non-empty.
    char *file = fileDialog->getStringValue();
    if (strcmp(file, "") != 0) {
	// EYE - we should warn the user if they're overwriting an
	// existing file.  Unfortunately, PUI's dialogs are pretty
	// tough to use because they're require setting up callbacks
	// and maintaining our state.  It'll be easier to just switch
	// GUIs, if you ask me.
	assert(globals.track());

	// EYE - it's important that we don't let the current track change
	// while the save dialog is active.
	globals.track()->setFilePath(file);
	globals.track()->save();

	// Update the tracksComboBox with the new file name.
	mainUI->setTrackList();

	// Force the graphs window to update itself (in particular,
	// its title).
	glutPostWindowRedisplay(graphs_window);
    }

    // Unfortunately, being a subclass of puDialogBox, a hidden
    // puaFileSelector will continue to grab all mouse events.  So, it
    // must be deleted, not hidden when we're finished.  This is
    // unfortunate because we can't "start up from where we left off"
    // - each time it's created, it's created anew.
    puDeleteObject(fileDialog);
    fileDialog = NULL;

    mainUI->saveAsButton->activate();
}

// Called to save the current track.
void save_cb(puObject *cb)
{
    if (globals.track() && globals.track()->hasFile()) {
	globals.track()->save();
	glutPostWindowRedisplay(graphs_window);
    }
}

// Called when the user presses OK or Cancel on the load file dialog.
void load_file_cb(puObject *cb)
{
    // If the user hit "Ok", then the string value of the save dialog
    // will be non-empty.
    char *file = fileDialog->getStringValue();
    if (strcmp(file, "") != 0) {
	// Look to see if we've already loaded that flight track.
	unsigned int i = globals.exists(file);
	if (i < globals.tracks().size()) {
	    globals.setCurrent(i);
	} else {
	    // We didn't find the track, so load it.
	    try {
		FlightTrack *t = new FlightTrack(file);
		// Set the mark aircraft to the beginning of the track.
		t->setMark(0);
		// Add track (selected) and display it.
		globals.addTrack(t);
		mainUI->setTrackListDirty();
	    } catch (runtime_error e) {
		// EYE - beep? dialog box? console message?
		printf("Failed to read flight file '%s'\n", file);
	    }
	}
	newFlightTrack();
    }

    puDeleteObject(fileDialog);
    fileDialog = NULL;
}

void load_cb(puObject *cb)
{
    // Open a flight file (unless the file dialog is already active
    // doing something else).
    if (fileDialog == NULL) {
	mainUI->loadButton->greyOut();
	fileDialog = new puaFileSelector(250, 150, 500, 400, "",
					 "Open Flight Track");
	fileDialog->setCallback(load_file_cb);
	glutPostRedisplay();
    }
}

// Unloads the current flight track.  Note the special calling
// convention: if cb is NULL, we unload the current flight track
// silently, even if it hasn't been saved.  If you want the user to be
// warned, cb should be non-NULL.
void unload_cb(puObject *cb)
{
    if (cb != NULL) {
	if (globals.track() && globals.track()->modified()) {
	    // EYE - put this warning in the callback itself?
	    makeDialog("The current track is unsaved.\nIf you close it, the track data will be lost.\nDo you want to close it?\n", close_ok_cb);
	    return;
	}
    }

    // Close the current track.
    FlightTrack *t = globals.removeTrack();
    if (t) {
	mainUI->setTrackListDirty();
	newFlightTrack();
	delete t;
    }
}

void detach_cb(puObject *cb)
{
    if (!globals.track() || !globals.track()->live()) {
	return;
    }
    assert(globals.track()->isNetwork() || globals.track()->isSerial());

    if (globals.track()->size() == 0) {
	unload_cb(cb);
    } else {
	globals.track()->detach();
	globals.track()->setMark(0);

	// EYE - should we do this dirty business?
	mainUI->setTrackListDirty();
	// It's not really a new flight track, but we do change its
	// name.
	newFlightTrack();

	glutPostWindowRedisplay(main_window);
	glutPostWindowRedisplay(graphs_window);
    }
}

// This is called either by the tracksComboBox or one of the arrows
// beside it.
void track_select_cb(puObject *cb)
{
    int i = mainUI->tracksComboBox->getCurrentItem();

    if ((cb == mainUI->prevTrackButton) || (cb == mainUI->nextTrackButton)) {
	if (cb == mainUI->prevTrackButton) {
	    i--;
	} else {
	    i++;
	}
    }

    // Set the current track to the ith track.  If it's different than
    // the previous current track, then update ourselves.
    if (globals.setCurrent(i)) {
	newFlightTrack();
    }
}

void track_aircraft_cb(puObject *cb)
{
    // Toggle auto-centering.
    prefs.autocenter_mode = (cb->getIntegerValue() == 1);
    aircraftMoved();
}

void jump_to_cb(puObject *cb)
{
    centerMapOnAircraft();
}

// Called when return is pressed in the track buffer size input field.
void track_limit_cb(puObject *cb)
{
    assert(globals.track());
    assert(globals.track()->live());
    assert(mainUI);

    int limit = mainUI->trackLimitInput->getIntegerValue();
    assert(limit >= 0);
    globals.track()->setMaxBufferSize(limit);

    glutPostRedisplay();	// EYE - when do I need these things?
}

void attach_cb(puObject *)
{
    networkPopup = new NetworkPopup(100, 100);

    // Fill in default values.
    networkPopup->portInput->setValue((int)Preferences::defaultPort);
    networkPopup->deviceInput->setValue(Preferences::defaultSerialDevice);
    networkPopup->baudInput->setValue(Preferences::defaultBaudRate);

    // Make the network stuff selected by default.
    network_serial_toggle_cb(networkPopup->networkButton);
}

void network_serial_toggle_cb(puObject *o)
{
    if (!networkPopup) {
	return;
    }

    if (o == networkPopup->networkButton) {
	networkPopup->networkButton->setValue(true);
	networkPopup->portInput->activate();
	networkPopup->serialButton->setValue(false);
	networkPopup->deviceInput->greyOut();
	networkPopup->baudInput->greyOut();
    } else if (o == networkPopup->serialButton) {
	networkPopup->networkButton->setValue(false);
	networkPopup->portInput->greyOut();
	networkPopup->serialButton->setValue(true);
	networkPopup->deviceInput->activate();
	networkPopup->baudInput->activate();
    }

    glutPostRedisplay();
}

// Called if the user hits ok or cancel on the network/serial dialog.
void network_serial_cb(puObject *obj)
{
    assert(networkPopup);

    if (obj == networkPopup->okButton) {
	FlightTrack* track = NULL;
	if (networkPopup->networkButton->getIntegerValue()) {
	    // It's a network connection.  Check to make sure we don't
	    // have one already.
	    int port = networkPopup->portInput->getIntegerValue();
	    unsigned int i = globals.exists(port);
	    if (i < globals.tracks().size()) {
		globals.setCurrent(i);
	    } else {
		// We didn't find a match, so open a connection.
		int bufferSize = mainUI->trackLimitInput->getIntegerValue();
		track = new FlightTrack(port, bufferSize);
		assert(track);
		// Add track (selected), and display it.
		globals.addTrack(track);
		mainUI->setTrackListDirty();
	    }
	} else {
	    // It's a serial connection.  Check to make sure we don't
	    // have one already.
	    // EYE - untested!
	    const char *device = networkPopup->deviceInput->getStringValue();
	    int baud = networkPopup->baudInput->getIntegerValue();
	    unsigned int i = globals.exists(device, baud);
	    if (i < globals.tracks().size()) {
		globals.setCurrent(i);
	    } else {
		// We didn't find a match, so open a connection.
		int bufferSize = mainUI->trackLimitInput->getIntegerValue();
		track = new FlightTrack(device, baud, bufferSize);
		assert(track);
		// Add track (selected), and display it.
		globals.addTrack(track);
		mainUI->setTrackListDirty();
	    }
	}
	newFlightTrack();
    }

    delete networkPopup;
    networkPopup = NULL;

    glutPostWindowRedisplay(main_window);
    glutPostWindowRedisplay(graphs_window);
}

// Called when one of the help buttons is pressed.
void help_cb(puObject *obj)
{
    helpUI->setText(obj);
}

int main(int argc, char **argv) 
{
    // Load our preferences.  If there's a problem with any of the
    // arguments, it will print some errors to stderr, and return false.
    if (!prefs.loadPreferences(argc, argv)) {
	exit(1);
    }

    // A bit of post-preference processing.
    if (access(prefs.path.c_str(), F_OK)==-1) {
	printf("\nWarning: path %s doesn't exist. Maps won't be loaded!\n", 
	       prefs.path.c_str());
    }

    // First get our standard palettes.
    SGPath paletteDir = prefs.path;
    paletteDir.append("Palettes");
    try {
	palettes = new Palettes(paletteDir.c_str());
    } catch (runtime_error e) {
	// EYE - but which one?
	fprintf(stderr, "%s: Failed to read palettes from '%s'\n",
		argv[0], paletteDir.c_str());
	exit(0);
    }

    // Load the preferred palette.
    globals.palette = palettes->load(prefs.palette.c_str());
    if (!globals.palette) {
	// Try tacking the palette directory on the front and see what
	// happens.
	paletteDir.append(prefs.palette.str());
	globals.palette = palettes->load(paletteDir.c_str());
	if (!globals.palette) {
	    printf("%s: Failed to read palette file '%s'\n", 
		   argv[0], prefs.palette.c_str());
	    // EYE - exit?
	    exit(0);
	}
    }

    // GLUT initialization.
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
    // EYE - see glutInitWindowPosition man page - call glutInit() after
    // this and pass argc, and argv to glutInit?
    glutInitWindowSize(prefs.width, prefs.height);
    main_window = glutCreateWindow("Atlas");

    // Use a crosshair cursor.
    glutSetCursor(GLUT_CURSOR_CROSSHAIR);

    // Load our scenery fonts.
    // EYE - put in preferences
    SGPath fontDir, fontFile;
    fontDir = prefs.path;
    fontDir.append("Fonts");

    fontFile = fontDir;
    fontFile.append("Helvetica.100.txf");
    globals.regularFont = new atlasFntTexFont;
    assert(globals.regularFont->load(fontFile.c_str()) == TRUE);

    fontFile = fontDir;
    fontFile.append("Helvetica-Bold.100.txf");
    globals.boldFont = new atlasFntTexFont;
    assert(globals.boldFont->load(fontFile.c_str()) == TRUE);

    globals.regular();

    // Create a tile manager.  In its creator it will see which scenery
    // directories we have, and whether there are maps generated for
    // those directories.
    printf("Please wait while checking existing scenery ... "); fflush(stdout);
    tileManager = new TileManager(prefs.scenery_root, prefs.path);
    printf("done.\n");

    // This does some OpenGL initialization.
    init();

    glutReshapeFunc(reshapeMap);
    glutDisplayFunc(redrawMap);
    glutMotionFunc(mouseMotion);
    glutPassiveMotionFunc(passivemotion);
    glutMouseFunc(mouseClick);
    glutKeyboardFunc(keyPressed);
    glutSpecialFunc(specPressed);
    glutVisibilityFunc(mainVisibilityFunc);

    // Read in files.
    for (unsigned int i = 0; i < prefs.flightFiles.size(); i++) {
	// First check if we've loaded that file already.
	const char *file = prefs.flightFiles[i].c_str();
	if (globals.exists(file) == globals.tracks().size()) {
	    // Nope - open it.
	    try {
		FlightTrack *t =new FlightTrack(file);
		// Set the mark aircraft to the beginning of the track.
		t->setMark(0);
		globals.addTrack(t, false);
	    } catch (runtime_error e) {
		printf("Failed to read flight file '%s'\n", file);
	    }
	}
    }
    // Make network connections.
    for (unsigned int i = 0; i < prefs.networkConnections.size(); i++) {
	// Already loaded?.
	int port = prefs.networkConnections[i];
	if (globals.exists(port) == globals.tracks().size()) {
	    FlightTrack *f = 
		new FlightTrack(port, prefs.max_track);
	    globals.addTrack(f, false);
	}
    }
    // Make serial connections.
    for (unsigned int i = 0; i < prefs.serialConnections.size(); i++) {
	// Already loaded?.
	const char *device = prefs.serialConnections[i].device;
	int baud = prefs.serialConnections[i].baud;
	if (globals.exists(device, baud) == globals.tracks().size()) {
	    FlightTrack *f = 
		new FlightTrack(device, baud, prefs.max_track);
	    globals.addTrack(f, false);
	}
    }

    // Check network connections and serial connections periodically (as
    // specified by the "update" user preference).
    glutTimerFunc((int)(prefs.update * 1000.0f), timer, 0);

    // Initial position (this may be changed by the --airport option,
    // or by load flight tracks).
    double latitude = prefs.latitude, longitude = prefs.longitude;
    movePosition(latitude, longitude);
    
    // Handle --airport option.
    if (strlen(prefs.icao) != 0) {
	if (globals.searcher.findMatches(prefs.icao, eye, -1)) {
	    // We found some matches.  The question is: which one to
	    // use?  Answer: the first one that's actually airport,
	    // for lack of a better choice.
	    for (unsigned int i = 0; i < globals.searcher.noOfMatches(); i++) {
		Searchable *s = globals.searcher.getMatch(i);
		ARP *ap;
		if ((ap = dynamic_cast<ARP *>(s))) {
		    latitude = ap->lat;
		    longitude = ap->lon;
		    break;
		}
	    }
	} else {
	    fprintf(stderr, "Unknown airport: '%s'\n", prefs.icao);
	}
    }
  
    init_gui(prefs.textureFonts);
    // EYE - note that the old 'init_gui' routine used to deal with
    // texture fonts vs. GLUT fonts
    mainUI = new MainUI(20, 20);
    infoUI = new InfoUI(260, 20);
    lightingUI = new LightingUI(600, 20);
    helpUI = new HelpUI(250, 500, prefs, *tileManager);

    // Set some defaults for the main GUI.
    // EYE - make shortcuts for these actions?
    mainUI->navaidsToggle->setValue(true);
    show_cb(mainUI->navaidsToggle);
    mainUI->navVOR->setValue(true);
    show_cb(mainUI->navVOR);
    mainUI->navNDB->setValue(true);
    show_cb(mainUI->navNDB);
    mainUI->navILS->setValue(true);
    show_cb(mainUI->navILS);
    mainUI->airportsToggle->setValue(true);
    show_cb(mainUI->airportsToggle);
    mainUI->labelsToggle->setValue(true);
    show_cb(mainUI->labelsToggle);
    mainUI->tracksToggle->setValue(elevationLabels);
    show_cb(mainUI->tracksToggle);
    mainUI->MEFToggle->setValue(true);
    show_cb(mainUI->MEFToggle);
    mainUI->airwaysToggle->setValue(false);
    show_cb(mainUI->airwaysToggle);
    mainUI->awyLow->setValue(true);
    show_cb(mainUI->awyLow);
    mainUI->showMouse = false;
    show_cb(mainUI->mouseText);

    // Update our palettes list.
    lightingUI->updatePalettes();

    // Create the graphs window, placed below the main.  First, get the
    // position of the first window.  We must do this now, because the
    // glutGet call works on the current window.
    int x, y, h;
    x = glutGet(GLUT_WINDOW_X);
    y = glutGet(GLUT_WINDOW_Y);
    h = glutGet(GLUT_WINDOW_HEIGHT);

    graphs_window = glutCreateWindow("-- graphs --");
    glutDisplayFunc(redrawGraphs);
    glutReshapeFunc(reshapeGraphs);
    glutMotionFunc(motionGraphs);
    glutMouseFunc(mouseGraphs);
    glutKeyboardFunc(keyboardGraphs);
    glutSpecialFunc(specialGraphs);
    glutVisibilityFunc(graphVisibilityFunc);

    // EYE - add keyboard function: space (play in real time, pause)

    glutReshapeWindow(800, 200);
    // EYE - this kind of works, but neglects the border OS X adds
    // around the window (and perhaps other effects too), so there
    // is some overlap.
    glutPositionWindow(x, y + h);
    glutHideWindow();

    // Initialize some standard OpenGL attributes.
    initStandardOpenGLAttribs();

    graphs = new Graphs(graphs_window);
//     graphs->setAircraftColor(map_object->getOverlays()->aircraftColor());
//     graphs->setMarkColor(map_object->getOverlays()->aircraftMarkColor());

    // EYE - this counts on the Graphs type values being same as the
    // corresponding entries in the button box.  As well, is there a
    // cleaner way to do this?  Is there a way to force the call to the
    // callback without calling it explicitly?
    infoUI->graphsBox->setValue(Graphs::ALTITUDE | 
				Graphs::SPEED | 
				Graphs::CLIMB_RATE);
    graphs->setGraphTypes(infoUI->graphsBox->getValue());
    // Our initial smoothing value is 10s (rates of climb and descent
    // will be smoothed over a 10s interval).
    // EYE - magic number?
    infoUI->smoother->setValue(10);
    graphs->setSmoothing(10);
    // EYE - we assume that xAxisBox has the correct value already
    graph_axis_cb(infoUI->xAxisBox);
    infoUI->hide();
      
    glutSetWindow(main_window);
    // Set our default zoom and position.
    zoomTo(prefs.zoom);
    movePosition(latitude, longitude);
    if (!globals.tracks().empty()) {
	// If we've loaded some tracks, display the first one and
	// centre the map on the aircraft.
	globals.setCurrent(0);
	centerMapOnAircraft();
    }
    // Update all our track information.
    newFlightTrack();

    glutMainLoop();
 
    return 0;
}
