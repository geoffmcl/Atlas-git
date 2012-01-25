/*-------------------------------------------------------------------------
  AtlasWindow.hxx

  Written by Brian Schack

  Copyright (C) 2012 Brian Schack

  This is the main Atlas window.  It can draw itself and react to user
  input.  It handles the main window UI, and subsidiary UI's on the
  main interface (eg, help, network popup, ...).  It is the "view"
  part of the model-view-controller (MVC) paradigm.  Most of the data
  it needs it gets via the AtlasController object.

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

#ifndef _ATLASWINDOW_H_
#define _ATLASWINDOW_H_

#include <vector>

#include <plib/puAux.h>

#include "AtlasBaseWindow.hxx"
#include "AtlasController.hxx"
#include "Notifications.hxx"
#include "Overlays.hxx"
#include "Scenery.hxx"
#include "Search.hxx"
#include "Tiles.hxx"
#include "misc.hxx"

//////////////////////////////////////////////////////////////////////
// 
// Route
//
// A route is a set of great circle segments.
//
// EYE - eventually this should be moved out of this file.  Also,
// should I separate the data part from the drawing part (eg, by
// creating a separate DrawableRoute class)?
//
//////////////////////////////////////////////////////////////////////
class Route {
  public:
    Route();
    ~Route();

    void addPoint(SGGeod &p);
    void deleteLastPoint();
    void clear();

    SGGeod lastPoint();

    float distance();

    void draw(double metresPerPixel, const sgdFrustum &frustum,
	      const sgdMat4 &m, const sgdVec3 eye, atlasFntTexFont *fnt,
	      bool magTrue);
    
    // EYE - move out later?
    bool active;

  protected:
    void _draw(bool start, const SGGeod &loc, float az, double metresPerPixel,
	       atlasFntTexFont *fnt, bool magTrue);
    void _draw(GreatCircle &gc, float distance, double metresPerPixel, 
	       const sgdFrustum &frustum, const sgdMat4 &m, 
	       atlasFntTexFont *fnt, bool magTrue);

    std::vector<SGGeod> _points;
    std::vector<GreatCircle> _segments;

    // The text size in pixels.  This must be multiplied by the
    // current scale (metresPerPixel) to be in the appropriate units.
    static const float _pointSize;
};

//////////////////////////////////////////////////////////////////////
//
// ScreenLocation maintains a window x, y coordinate, and its
// corresponding location on the earth (which it determines using a
// Scenery object).  ScreenLocation does not track changes to the
// view, so it must be told when to recalculate its location, via the
// invalidate() call.
//
//////////////////////////////////////////////////////////////////////
class ScreenLocation {
  public:
    ScreenLocation(Scenery &scenery);

    // Set x, y.  This also causes us to be invalidated.
    void set(float x, float y);

    float x() const { return _x; }
    float y() const { return _y; }

    // Return the actual coordinates.  If we are invalid, this will
    // force an intersection test.
    AtlasCoord &coord();
    // True if the x, y location has a real elevation value (ie,
    // there's live scenery at that point).  If we are invalid, this
    // will force an intersection test.
    bool validElevation();

    // Convenience accessors.
    const SGGeod &geod() { return coord().geod(); }
    double lat() { return coord().lat(); }
    double lon() { return coord().lon(); }
    // EYE - should this return Bucket::NanE if !validElevation()?
    double elev() { return coord().elev(); }
    const SGVec3<double> &cart() { return coord().cart(); }
    const double *data() { return coord().data(); }

    // Invalidates us, forcing us to recalculate our location the next
    // time coord() or validElevation() is called.
    void invalidate();

  protected:
    // We use this to figure out what's beneath our given screen
    // coordinates.
    Scenery &_scenery;

    // Note that both ScreenLocation and AtlasCoord have notions of
    // validity.  When the ScreenLocation is valid (_valid = true),
    // that means we've called intersection() with the current x, y
    // coordinates.  When the AtlasCoord is valid (_loc.valid() =
    // true), that means the intersection hit the earth.  Finally,
    // _validElevation tells us if the intersection was with live
    // scenery.
    bool _valid, _validElevation;
    float _x, _y;
    AtlasCoord _loc;
};

//////////////////////////////////////////////////////////////////////
//
// AtlasWindow
//
//////////////////////////////////////////////////////////////////////
// EYE - should the UIs (MainUI, HelpUI, ...) be subclasses of PUI
// widgets?  At the least we wouldn't have to implement isVisible(),
// ...
class MainUI;
class InfoUI;
class LightingUI;
class HelpUI;
class SearchUI;
class AtlasWindow: public AtlasBaseWindow, Subscriber {
  public:
    AtlasWindow(const char *name,
		const char *regularFontFile,
		const char *boldFontFile,
		AtlasController *ac);
    ~AtlasWindow();

    AtlasController *ac() { return _ac; }

    bool isOverlayVisible(Overlays::OverlayType overlay) 
      { return _overlays->isVisible(overlay); }
    void setOverlayVisibility(Overlays::OverlayType overlay, bool on);
    void toggleOverlay(Overlays::OverlayType overlay);

    // Public window callback.  The graphs window needs to pass on
    // keyboard events to us, so we expose it (making sure to set our
    // window to be current).
    void keyboard(unsigned char key, int x, int y);

    // Search methods (called from the _searchUI object.
    void searchFinished(int i);
    void searchItemSelected(int i);
    void searchStringChanged(const char *str);
    int noOfMatches();
    char *matchAtIndex(int i);

    // If we're tracking the mouse, this returns the cursor position,
    // otherwise returns the position at the centre of the window.
    ScreenLocation *currentLocation();
    ScreenLocation *cursor() { return _cursor; }
    void setCursor(float x, float y);
    ScreenLocation *centre() { return _centre; }
    void setCentre(float x, float y);

    // This determines what we draw in the centre of the screen, and
    // what to display for our position information.  "CROSSHAIRS"
    // draws a crosshairs at the centre and displays position
    // information for the centre; "RANGE_RINGS" adds a set of range
    // rings (scaled appropriately).  "MOUSE" displays information
    // about whatever point is under the mouse (and changes the cursor
    // to a set of crosshairs).
    enum CentreType { MOUSE, CROSSHAIRS, RANGE_RINGS };
    CentreType centreType() { return _centreType; }
    void setCentreType(CentreType t);

    // View and position data.
    const sgdVec3 &eye() { return _eye; }
    const sgdVec3 &eyeUp() { return _eyeUp; }
    const sgdFrustum &frustum() { return _frustum; }
    double scale() { return _metresPerPixel; }

    void rotateEye(double heading = 0.0);
    void movePosition(const sgdVec3 dest);
    void movePosition(double lat, double lon);
    void rotatePosition(sgdMat4 rot);
    void zoomTo(double scale);
    void zoomBy(double factor);

    void centreMapOnAircraft();
    bool autocentreMode() { return _autocentreMode; }
    void setAutocentreMode(bool mode);

    // Subscriber method.
    void notification(Notification::type n);

    friend void __atlasWindow_exitOk_cb(puObject *o);

  protected:
    AtlasController *_ac;
    SGVec3<double> _oldC;
    bool _dragging;
    bool _lightingPrefixKey, _debugPrefixKey;
    // If false (the default), scenery is coloured according to
    // absolute height.  If true, the current location elevation is
    // considered to be 0.0, and colouring is done relative to that.
    bool _relativePalette;
    // If true, the map moves to keep the aircraft in the centre of
    // the screen.
    bool _autocentreMode;
    Scenery *_scenery;
    Overlays *_overlays;

    // Basic view geometry - where we're looking from, and our
    // orientation at that point.

    // EYE - should we make these AtlasCoords?
    sgdVec3 _eye;
    sgdVec3 _eyeUp;
    sgdFrustum _frustum;
    double _metresPerPixel;

    // Current cursor and window centre locations.
    ScreenLocation *_cursor, *_centre;

    CentreType _centreType;

    MainUI *_mainUI;
    InfoUI *_infoUI;
    LightingUI *_lightingUI;
    HelpUI *_helpUI;
    SearchUI *_searchUI;

    AtlasDialog *_exitOkDialog;

    // We use _searchTimerScheduled to record if there are pending
    // calls to _searchTimer().  This prevents multiple search threads
    // from beginning.
    bool _searchTimerScheduled;

    // Records where we were when we started a search.  If the search is
    // cancelled, then we'll return to that point.
    sgdVec3 _searchFrom;

    // Window callbacks.
    void _display();
    void _reshape(int w, int h);
    void _mouse(int button, int state, int x, int y);
    void _motion(int x, int y);
    void _passiveMotion(int x, int y);
    void _keyboard(unsigned char key, int x, int y);
    void _special(int key, int x, int y);
    void _visibility(int state);

    // Update methods
    void _setProjection();
    void _setModelView();
    void _setShading();
    void _setAzimuthElevation();
    void _setPaletteBase();
    void _setRelativePalette(bool relative);
    void _setMEFs();
    void _setCentreType();
    void _setFlightTrack();
    void _setTitle();

    // Timers
    void _flightTrackTimer();
    void _searchTimer();

    void _lightingPrefixKeypressed(unsigned char key, int x, int y);
    void _debugPrefixKeypressed(unsigned char key, int x, int y);

    void _exitOk_cb(bool okay);

    // Internal routines that modify _eye and _eyeUp (and OpenGL
    // state).
    void _rotate(double hdg = 0.0);
    void _move();
};

//////////////////////////////////////////////////////////////////////
//
// Main Interface
//
// Contains information about our position, overlays, and tracks.
//
//////////////////////////////////////////////////////////////////////
class NetworkPopup;
class MainUI: public Subscriber {
  public:
    MainUI(int x, int y, AtlasWindow *aw);
    ~MainUI();

    bool isVisible() { return _gui->isVisible(); }
    void reveal() { _gui->reveal(); }
    void hide() { _gui->hide(); }

    // Expose the "save as", "load", and "unload" callback
    // functionality to outside callers (specifically, AtlasWindow,
    // which has keyboard shortcuts for these).
    void saveAs();
    void load();
    void unload();

    // Subscriber method.
    void notification(Notification::type n);

    // Callbacks.
    friend void __mainUI_zoom_cb(puObject *o);
    friend void __mainUI_overlay_cb(puObject *o);
    friend void __mainUI_position_cb(puObject *o);
    friend void __mainUI_saveAsFile_cb(puObject *o);
    friend void __mainUI_saveAs_cb(puObject *o);
    friend void __mainUI_loadFile_cb(puObject *o);
    friend void __mainUI_load_cb(puObject *o);
    friend void __mainUI_unload_cb(puObject *o);
    friend void __mainUI_trackSelect_cb(puObject *o);
    friend void __mainUI_attach_cb(puObject *o);

    friend void __networkPopup_ok_cb(puObject *o);
    friend void __networkPopup_cancel_cb(puObject *o);

    friend void __mainUI_closeOk_cb(puObject *o);

  protected:
    AtlasController *_ac;
    AtlasWindow *_aw;

    puGroup *_gui;
    puFrame *_preferencesFrame, *_locationFrame, *_navaidsFrame, 
	*_flightTracksFrame;

    // Location frame widgets.
    puInput *_latInput, *_lonInput, *_zoomInput;
    puText *_elevText, *_mouseText;
    puOneShot *_zoomInButton, *_zoomOutButton;

    // Preferences frame widgets.
    puButtonBox *_degMinSecBox, *_magTrueBox;
    const char *_degMinSecBoxLabels[3], *_magTrueBoxLabels[3];

    // Navaids frame widgets.
    puButton *_navaidsToggle;
    puButton *_airportsToggle, *_airwaysToggle, *_labelsToggle, *_tracksToggle;
    puButton *_MEFToggle;
    puButton *_navVOR, *_navNDB, *_navILS, *_navDME, *_navFIX;
    puButton *_awyHigh, *_awyLow;

    // Flight tracks frame widgets.
    puOneShot *_loadButton, *_attachButton;
    puaComboBox *_tracksComboBox;
    char **_trackListStrings;
    puArrowButton *_prevTrackButton, *_nextTrackButton;
    puOneShot *_unloadButton, *_detachButton;
    puOneShot *_saveButton, *_saveAsButton, *_clearButton;
    puOneShot *_jumpToButton;
    puButton *_trackAircraftToggle;
    puText *_trackSizeText;
    AtlasString _trackSizeLabel;
    puInput *_trackLimitInput;

    // File dialog.
    puaFileSelector *_fileDialog;
    AtlasDialog *_closeOkDialog;

    NetworkPopup *_networkPopup;

    void _setDegMinSec();
    void _setMagTrue();
    void _setMEFs();
    void _setAutocentreMode();
    void _setPosition();
    void _setCentreType();
    void _setZoom();
    void _setOverlays();
    void _setTrackLimit();
    void _setTrack();
    void _setTrackList();

    void _zoom_cb(puObject *o);
    void _overlay_cb(puObject *o);
    void _position_cb(puObject *o);
    void _saveAsFile_cb(puObject *o);
    void _saveAs_cb(puObject *o);
    void _loadFile_cb(puObject *o);
    void _load_cb(puObject *o);
    void _unload_cb(puObject *o);
    void _trackSelect_cb(puObject *o);
    void _attach_cb(puObject *o);

    void _networkPopup_cb(bool okay);

    void _closeOk_cb(bool okay);
};

//////////////////////////////////////////////////////////////////////
//
// Info Interface
//
// This gives information about the current aircraft position and
// velocities, and navigation receivers.
//
//////////////////////////////////////////////////////////////////////
class InfoUI: public Subscriber {
  public:
    InfoUI(int x, int y, AtlasWindow *aw);
    ~InfoUI();

    void reveal() { _gui->reveal(); }
    void hide() { _gui->hide(); }
    bool isVisible() { return _gui->isVisible(); }

    // Subscriber method.
    void notification(Notification::type n);

  protected:
    AtlasWindow *_aw;
    AtlasController *_ac;

    puPopup *_gui;
    puFrame *_infoFrame;
    puText *_latText, *_lonText, *_altText, *_hdgText, *_spdText, *_hmsText, 
	*_dstText;
    puFrame *_VOR1Colour, *_VOR2Colour, *_ADFColour;
    puText *_VOR1Text, *_VOR2Text, *_ADFText;

    void _setVisibility();
    void _setText();
};

//////////////////////////////////////////////////////////////////////
//
// Network Popup
//
// This popup lets the user specify the parameters to a network or
// serial connection.
//
//////////////////////////////////////////////////////////////////////
class NetworkPopup {
  public:
    NetworkPopup(int x, int y, MainUI *mainUI);
    ~NetworkPopup();

    // Activates/deactivates various buttons depending on whether we
    // want to show network options (true) or serial options (false).
    void setNetwork(bool on);
    bool networkSelected() { return (_networkButton->getIntegerValue() != 0); }
    int port() { return _portInput->getIntegerValue(); }
    void setPort(int port);
    const char *device() { return _deviceInput->getStringValue(); }
    void setDevice(const char *device);
    int baud() { return _baudInput->getIntegerValue(); }
    void setBaud(int baud);

    // Callback.
    friend void __networkPopup_serialToggle_cb(puObject *o);

  protected:
    puDialogBox *_dialogBox;
    puButton *_networkButton, *_serialButton;
    puInput *_portInput, *_deviceInput, *_baudInput;
    puOneShot *_cancelButton, *_okButton;

    void _serialToggle_cb(puObject *o);
};

//////////////////////////////////////////////////////////////////////
//
// Lighting Interface
//
// This allows the user to adjust lighting parameters: discrete/smooth
// contours, contour lines on/off, lighting on/off, smooth/flat
// polygon shading, and current palette.
//
//////////////////////////////////////////////////////////////////////
class LightingUI: public Subscriber {
  public:
    LightingUI(int x, int y, AtlasWindow *aw);
    ~LightingUI();

    void reveal() { _gui->reveal(); }
    void hide() { _gui->hide(); }
    bool isVisible() { return _gui->isVisible(); }

    void getSize(int *w, int *h);
    void setPosition(int x, int y);

    // Subscriber method.
    void notification(Notification::type n);

    friend void __lightingUI_cb(puObject *o);

  protected:
    AtlasWindow *_aw;
    AtlasController *_ac;

    puPopup *_gui;
    puFrame *_frame;

    // Lighting toggles
    puButtonBox *_contours, *_lines, *_lighting, *_polygons;
    puText *_contoursLabel, *_linesLabel, *_lightingLabel, *_polygonsLabel;
    const char *_contoursLabels[3], *_linesLabels[3], *_lightingLabels[3], 
	*_polygonsLabels[3];

    // Lighting direction
    puFrame *_directionFrame;
    puText *_directionLabel;
    puInput *_azimuth, *_elevation;
    puDial *_azimuthDial;
    puSlider *_elevationSlider;

    // Palettes
    puFrame *_paletteFrame;
    puText *_paletteLabel;
    puaComboBox *_paletteComboBox;
    puArrowButton *_prevPalette, *_nextPalette;

    // Sets our UI elements based on model values (in
    // AtlasController).
    void _setDiscreteContours();
    void _setContourLines();
    void _setLightingOn();
    void _setSmoothShading();
    void _setAzimuth();
    void _setElevation();
    void _setPalette();
    void _setPaletteList();

    // Callback for all UI events.  This will set our model values
    // based on the user input.
    void _cb(puObject *o);
};

//////////////////////////////////////////////////////////////////////
//
// Help Interface
//
// Gives the user information on Atlas' startup parameters and
// keyboard shortcuts.
//
//////////////////////////////////////////////////////////////////////
class HelpUI {
  public:
    HelpUI(int x, int y, AtlasWindow *aw);
    ~HelpUI();

    void reveal() { _gui->reveal(); }
    void hide() { _gui->hide(); }
    bool isVisible() { return _gui->isVisible(); }

    // Callback.
    friend void __helpUI_cb(puObject *o);

  protected:
    AtlasController *_ac;
    char *_label, *_generalText, *_keyboardText;

    puPopup *_gui;
    puText *_labelText;
    puaLargeInput *_text;
    puButton *_generalButton, *_keyboardButton;

    void _setText(puObject *o);

    void _cb(puObject *o);
};

//////////////////////////////////////////////////////////////////////
//
// Search Interface
//
// This is a text field that hides in the upper-right corner.  It
// allows the user to search for objects via a text string.
//
//////////////////////////////////////////////////////////////////////
class SearchUI: public Search
{
  public:
    SearchUI(AtlasWindow *aw, int minx, int miny, int maxx, int maxy);
    ~SearchUI();

    // Called when the user hits return or escape.
    void searchFinished(int i);
    // Called when the user selects an item in the list.
    void searchItemSelected(int i);
    // Called when the user changes the search string.  We just call
    // searchTimer() if it's not running already, which will
    // incrementally search for str.
    void searchStringChanged(const char *str);
    // Called by the search interface to find out how many matches
    // there are.
    int noOfMatches();
    // Called by the search interface to get the data for index i.
    char *matchAtIndex(int i);

  protected:
    AtlasWindow *_aw;
};

#endif
