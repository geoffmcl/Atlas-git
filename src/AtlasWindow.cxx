/*-------------------------------------------------------------------------
  AtlasWindow.cxx

  Written by Brian Schack

  Copyright (C) 2012 Brian Schack

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdexcept>

#include <plib/pu.h>
#include <plib/puAux.h>
#include <simgear/math/sg_geodesy.hxx>
#include <simgear/sg_inlines.h>	       // SG_MAX2
#include <simgear/bucket/newbucket.hxx>

#include "AtlasWindow.hxx"
#include "Globals.hxx"
#include "Search.hxx"
#include "Bucket.hxx"
#include "Preferences.hxx"
#include "Graphs.hxx"
#include "Scenery.hxx"
#include "NavData.hxx"

using namespace std;

// Take 10 steps (0.1) to zoom by a factor of 10.
const double zoomFactor = pow(10.0, 0.1);

//////////////////////////////////////////////////////////////////////
// Forward declarations of all callbacks.
//////////////////////////////////////////////////////////////////////
void __atlasWindow_exitOk_cb(puObject *o);

void __mainUI_zoom_cb(puObject *o);
void __mainUI_overlay_cb(puObject *o);
void __mainUI_MEF_cb(puObject *o);
void __mainUI_position_cb (puObject *o);
void __mainUI_clearFlightTrack_cb(puObject *o);
void __mainUI_degMinSec_cb(puObject *o);
void __mainUI_magTrue_cb(puObject *o);
void __mainUI_saveAsFile_cb(puObject *o);;
void __mainUI_saveAs_cb(puObject *o);
void __mainUI_save_cb(puObject *o);
void __mainUI_loadFile_cb(puObject *o);
void __mainUI_load_cb(puObject *o);
void __mainUI_unload_cb(puObject *o);
void __mainUI_detach_cb(puObject *o);
void __mainUI_trackSelect_cb(puObject *o);
void __mainUI_trackAircraft_cb(puObject *o);
void __mainUI_centre_cb(puObject *o);
void __mainUI_trackLimit_cb(puObject *o);
void __mainUI_attach_cb(puObject *o);
void __mainUI_closeOk_cb(puObject *o);

void __networkPopup_ok_cb(puObject *o);
void __networkPopup_cancel_cb(puObject *o);
void __networkPopup_serialToggle_cb(puObject *o);

void __lightingUI_cb(puObject *o);

void __helpUI_cb(puObject *o);

//////////////////////////////////////////////////////////////////////
// NetworkPopup
//////////////////////////////////////////////////////////////////////
NetworkPopup::NetworkPopup(int x, int y, MainUI *mainUI)
{
    const int buttonHeight = 20, buttonWidth = 80;
    const int bigSpace = 5;
    const int width = 350, height = 4 * buttonHeight + 5 * bigSpace;
    const int labelWidth = 50;

    int curx, cury;

    _dialogBox = new puDialogBox(250, 150);
    {
	new puFrame(0, 0, width, height);

	// Cancel and ok buttons
	curx = bigSpace;
	cury = bigSpace;

	_cancelButton = 
	    new puOneShot(curx, cury, curx + buttonWidth, cury + buttonHeight);
	_cancelButton->setLegend("Cancel");
	_cancelButton->setUserData(mainUI);
	_cancelButton->setCallback(__networkPopup_cancel_cb);
	curx += buttonWidth + bigSpace;

	_okButton = 
	    new puOneShot(curx, cury, curx + buttonWidth, cury + buttonHeight);
	_okButton->setLegend("Ok");
	_okButton->makeReturnDefault(TRUE);
	_okButton->setUserData(mainUI);
	_okButton->setCallback(__networkPopup_ok_cb);

	// Network and serial input boxes
	curx = bigSpace + width / 2;
	cury += buttonHeight + bigSpace;
	_baudInput = new puInput(curx + labelWidth, cury, 
				width - bigSpace, cury + buttonHeight);
	_baudInput->setLabel("baud:");
	_baudInput->setLabelPlace(PUPLACE_CENTERED_LEFT);
	_baudInput->setValidData("0123456789");
	
	curx = bigSpace;
	cury += buttonHeight + bigSpace;
	_portInput = new puInput(curx + labelWidth, cury, 
				width / 2 - bigSpace, cury + buttonHeight);
	_portInput->setLabel("port:");
	_portInput->setLabelPlace(PUPLACE_CENTERED_LEFT);
	_portInput->setValidData("0123456789");

	curx = bigSpace + width / 2;
	_deviceInput = new puInput(curx + labelWidth, cury, 
				  width - bigSpace, cury + buttonHeight);
	_deviceInput->setLabel("device:");
	_deviceInput->setLabelPlace(PUPLACE_CENTERED_LEFT);

	// Network/serial radio buttons
	curx = bigSpace;
	cury += buttonHeight + bigSpace;
	_networkButton = new puButton(curx, cury, 
				     curx + buttonHeight, cury + buttonHeight);
	_networkButton->setLabel("Network");
	_networkButton->setLabelPlace(PUPLACE_CENTERED_RIGHT);
	_networkButton->setButtonType(PUBUTTON_CIRCLE);
	_networkButton->setUserData(this);
	_networkButton->setCallback(__networkPopup_serialToggle_cb);

	curx += width / 2 + bigSpace;
	_serialButton = new puButton(curx, cury, 
				     curx + buttonHeight, cury + buttonHeight);
	_serialButton->setLabel("Serial");
	_serialButton->setLabelPlace(PUPLACE_CENTERED_RIGHT);
	_serialButton->setButtonType(PUBUTTON_CIRCLE);
	_serialButton->setUserData(this);
	_serialButton->setCallback(__networkPopup_serialToggle_cb);
    }
    _dialogBox->close();
    _dialogBox->reveal();
}

NetworkPopup::~NetworkPopup()
{
    puDeleteObject(_dialogBox);
}

void NetworkPopup::setNetwork(bool on)
{
    if (on) {
	_networkButton->setValue(true);
	_portInput->activate();
	_serialButton->setValue(false);
	_deviceInput->greyOut();
	_baudInput->greyOut();
    } else {
	_networkButton->setValue(false);
	_portInput->greyOut();
	_serialButton->setValue(true);
	_deviceInput->activate();
	_baudInput->activate();
    }
}

void NetworkPopup::setPort(int port)
{
    _portInput->setValue(port);
}

void NetworkPopup::setDevice(const char *device)
{
    // EYE - do we need to copy it?
    _deviceInput->setValue(device);
}

void NetworkPopup::setBaud(int baud)
{
    _baudInput->setValue(baud);
}

void NetworkPopup::_serialToggle_cb(puObject *o)
{
    if (o == _networkButton) {
	setNetwork(true);
    } else if (o == _serialButton) {
	setNetwork(false);
    }
}

//////////////////////////////////////////////////////////////////////
// MainUI
//////////////////////////////////////////////////////////////////////

// Create the main interface, with its lower-left corner at x, y.
// Assumes that puInit() has been called.
MainUI::MainUI(int x, int y, AtlasWindow *aw): 
    _ac(aw->ac()), _aw(aw), _trackListStrings(NULL), _fileDialog(NULL),
    _closeOkDialog(NULL), _networkPopup(NULL)
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

    _gui = new puGroup(x, y);

    //////////////////////////////////////////////////////////////////////
    // Flight tracks
    //////////////////////////////////////////////////////////////////////
    _flightTracksFrame = new puFrame(0, cury, width, cury + flightTracksHeight);
    cury += bigSpace;

    const int trackButtonWidth = (width - 4 * bigSpace) / 3;

    // Track size and track size limit.
    curx = bigSpace;
    _trackLimitInput = new puInput(curx + labelWidth * 2, cury, 
				  curx + guiWidth, cury + buttonHeight);
    _trackLimitInput->setLabel("Track limit: ");
    _trackLimitInput->setLabelPlace(PUPLACE_CENTERED_LEFT);
    _trackLimitInput->setValidData("0123456789");
    _trackLimitInput->setUserData(_ac);
    _trackLimitInput->setCallback(__mainUI_trackLimit_cb);
    cury += buttonHeight + bigSpace;

    // EYE - the two labels, "Track limit:" and "Track size:" don't
    // line up on the left (but they seem to on the right).  What
    // behaviour controls this?
    _trackSizeText = new puText(curx, cury);
    _trackSizeText->setLabel("Track size: ");
    cury += buttonHeight + bigSpace;

    // Jump to button and track aircraft toggle
    curx = bigSpace;
    _jumpToButton = new puOneShot(curx, cury,
				 curx + trackButtonWidth, cury + buttonHeight);
    _jumpToButton->setLegend("Centre");
    _jumpToButton->setUserData(_aw);
    _jumpToButton->setCallback(__mainUI_centre_cb);
    curx += trackButtonWidth + bigSpace;

    _trackAircraftToggle = new puButton(curx, cury, 
				       curx + checkHeight, cury + checkHeight);
    _trackAircraftToggle->setLabel("Follow aircraft");
    _trackAircraftToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _trackAircraftToggle->setButtonType(PUBUTTON_VCHECK);
    _trackAircraftToggle->setUserData(_aw);
    _trackAircraftToggle->setCallback(__mainUI_trackAircraft_cb);

    cury += buttonHeight + bigSpace;

    // Save, save as, and clear  buttons.
    curx = bigSpace;
    _saveButton = new puOneShot(curx, cury,
			       curx + trackButtonWidth, cury + buttonHeight);
    _saveButton->setLegend("Save");
    _saveButton->setUserData(_ac);
    _saveButton->setCallback(__mainUI_save_cb);
    curx += trackButtonWidth + bigSpace;

    _saveAsButton = new puOneShot(curx, cury,
				 curx + trackButtonWidth, cury + buttonHeight);
    _saveAsButton->setLegend("Save As");
    _saveAsButton->setUserData(this);
    _saveAsButton->setCallback(__mainUI_saveAs_cb);
    curx += trackButtonWidth + bigSpace;

    _clearButton = new puOneShot(curx, cury,
				curx + trackButtonWidth, cury + buttonHeight);
    _clearButton->setLegend("Clear");
    _clearButton->setUserData(_ac);
    _clearButton->setCallback(__mainUI_clearFlightTrack_cb);
    cury += buttonHeight + bigSpace;

    // Unload and detach buttons.
    curx = bigSpace;
    _unloadButton = new puOneShot(curx, cury,
				 curx + trackButtonWidth, cury + buttonHeight);
    _unloadButton->setLegend("Unload");
    _unloadButton->setUserData(this);
    _unloadButton->setCallback(__mainUI_unload_cb);
    curx += trackButtonWidth + bigSpace;

    _detachButton = new puOneShot(curx, cury,
				 curx + trackButtonWidth, cury + buttonHeight);
    _detachButton->setLegend("Detach");
    _detachButton->setUserData(_ac);
    _detachButton->setCallback(__mainUI_detach_cb);
    cury += buttonHeight + bigSpace;

    // Track chooser, and next and previous arrow buttons.
    curx = bigSpace;
    _tracksComboBox = new puaComboBox(curx, 
				     cury,
				     curx + guiWidth - buttonHeight,
				     cury + buttonHeight,
				     NULL,
				     FALSE);
    _tracksComboBox->setUserData(this);
    _tracksComboBox->setCallback(__mainUI_trackSelect_cb);
    curx += guiWidth - buttonHeight;
    _prevTrackButton = new puArrowButton(curx,
					cury + buttonHeight / 2,
					curx + buttonHeight,
					cury + buttonHeight,
					PUARROW_UP);
    _prevTrackButton->setUserData(this);
    _prevTrackButton->setCallback(__mainUI_trackSelect_cb);
    _prevTrackButton->greyOut();
    _nextTrackButton = new puArrowButton(curx,
					cury,
					curx + buttonHeight,
					cury + buttonHeight / 2,
					PUARROW_DOWN);
    _nextTrackButton->setUserData(this);
    _nextTrackButton->setCallback(__mainUI_trackSelect_cb);
    _nextTrackButton->greyOut();

    cury += buttonHeight + bigSpace;

    // Load and attach buttons.
    curx = bigSpace;
    _loadButton = new puOneShot(curx, cury,
			       curx + trackButtonWidth, cury + buttonHeight);
    _loadButton->setLegend("Load");
    _loadButton->setUserData(this);
    _loadButton->setCallback(__mainUI_load_cb);
    curx += trackButtonWidth + bigSpace;

    _attachButton = new puOneShot(curx, cury,
				curx + trackButtonWidth, cury + buttonHeight);
    _attachButton->setLegend("Attach");
    _attachButton->setUserData(this);
    _attachButton->setCallback(__mainUI_attach_cb);
    cury += buttonHeight + bigSpace;

    //////////////////////////////////////////////////////////////////////
    // Navaids
    //////////////////////////////////////////////////////////////////////
    _navaidsFrame = new puFrame(0, cury, width, cury + navaidsHeight);

    curx = bigSpace;
    cury += bigSpace + buttonHeight + smallSpace;

    // Indent these buttons.
    curx += buttonHeight;
    _navFIX = new puButton(curx, cury, curx + checkHeight, cury + checkHeight);
    _navFIX->setLabel("FIX");
    _navFIX->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _navFIX->setButtonType(PUBUTTON_VCHECK);
    // EYE - this wasn't set before - how did it work?  (Ditto for
    // navDME, navILS, navNDB, and navVOR).
    _navFIX->setUserData(this);
    _navFIX->setCallback(__mainUI_overlay_cb);
    cury += buttonHeight + smallSpace;

    _navDME = new puButton(curx, cury, curx + checkHeight, cury + checkHeight);
    _navDME->setLabel("DME");
    _navDME->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _navDME->setButtonType(PUBUTTON_VCHECK);
    _navDME->setUserData(this);
    _navDME->setCallback(__mainUI_overlay_cb);
    cury += buttonHeight + smallSpace;

    _navILS = new puButton(curx, cury, curx + checkHeight, cury + checkHeight);
    _navILS->setLabel("ILS");
    _navILS->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _navILS->setButtonType(PUBUTTON_VCHECK);
    _navILS->setUserData(this);
    _navILS->setCallback(__mainUI_overlay_cb);
    cury += buttonHeight + smallSpace;

    _navNDB = new puButton(curx, cury, curx + checkHeight, cury + checkHeight);
    _navNDB->setLabel("NDB");
    _navNDB->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _navNDB->setButtonType(PUBUTTON_VCHECK);
    _navNDB->setUserData(this);
    _navNDB->setCallback(__mainUI_overlay_cb);
    cury += buttonHeight + smallSpace;

    _navVOR = new puButton(curx, cury, curx + checkHeight, cury + checkHeight);
    _navVOR->setLabel("VOR");
    _navVOR->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _navVOR->setButtonType(PUBUTTON_VCHECK);
    _navVOR->setUserData(this);
    _navVOR->setCallback(__mainUI_overlay_cb);
    cury += buttonHeight + smallSpace;

    // Unindent.
    curx -= buttonHeight;

    _navaidsToggle = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    _navaidsToggle->setLabel("Navaids");
    _navaidsToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _navaidsToggle->setButtonType(PUBUTTON_VCHECK);
    _navaidsToggle->setUserData(this);
    _navaidsToggle->setCallback(__mainUI_overlay_cb);
    cury += buttonHeight;

    cury += bigSpace;

    // Now move to the right column and do it again, for airports,
    // airways, and labels.
    cury -= navaidsHeight;
    cury += bigSpace;
    curx += guiWidth / 2;

    _MEFToggle = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    _MEFToggle->setLabel("MEF");
    _MEFToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _MEFToggle->setButtonType(PUBUTTON_VCHECK);
    _MEFToggle->setUserData(_ac);
    _MEFToggle->setCallback(__mainUI_MEF_cb);
    cury += buttonHeight + smallSpace;

    _tracksToggle = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    _tracksToggle->setLabel("Flight tracks");
    _tracksToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _tracksToggle->setButtonType(PUBUTTON_VCHECK);
    _tracksToggle->setUserData(this);
    _tracksToggle->setCallback(__mainUI_overlay_cb);
    cury += buttonHeight + smallSpace;

    _labelsToggle = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    _labelsToggle->setLabel("Labels");
    _labelsToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _labelsToggle->setButtonType(PUBUTTON_VCHECK);
    _labelsToggle->setUserData(this);
    _labelsToggle->setCallback(__mainUI_overlay_cb);
    cury += buttonHeight + smallSpace;

    // Indent for the 2 airways subtoggles.
    curx += buttonHeight;

    _awyLow = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    _awyLow->setLabel("Low");
    _awyLow->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _awyLow->setButtonType(PUBUTTON_VCHECK);
    _awyLow->setUserData(this);
    _awyLow->setCallback(__mainUI_overlay_cb);
    cury += buttonHeight + smallSpace;

    _awyHigh = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    _awyHigh->setLabel("High");
    _awyHigh->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _awyHigh->setButtonType(PUBUTTON_VCHECK);
    _awyHigh->setUserData(this);
    _awyHigh->setCallback(__mainUI_overlay_cb);
    cury += buttonHeight + smallSpace;

    // Unindent
    curx -= buttonHeight;

    _airwaysToggle = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    _airwaysToggle->setLabel("Airways");
    _airwaysToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _airwaysToggle->setButtonType(PUBUTTON_VCHECK);
    _airwaysToggle->setUserData(this);
    _airwaysToggle->setCallback(__mainUI_overlay_cb);
    cury += buttonHeight + smallSpace;

    _airportsToggle = new puButton(curx, cury, 
				 curx + checkHeight, cury + checkHeight);
    _airportsToggle->setLabel("Airports");
    _airportsToggle->setLabelPlace(PUPLACE_CENTERED_RIGHT);
    _airportsToggle->setButtonType(PUBUTTON_VCHECK);
    _airportsToggle->setUserData(this);
    _airportsToggle->setCallback(__mainUI_overlay_cb);
    cury += buttonHeight;

    cury += bigSpace;
    curx -= guiWidth / 2;

    //////////////////////////////////////////////////////////////////////
    // Location information
    //////////////////////////////////////////////////////////////////////
    _locationFrame = new puFrame(0, cury, width, cury + locationHeight);
    cury += bigSpace;

    // Zoom: an input field and two buttons
    _zoomInput = new puInput(curx + labelWidth, cury, 
			    curx + guiWidth - (2 * buttonHeight),
			    cury + buttonHeight);
    _zoomInput->setLabel("Zoom:");
    _zoomInput->setLabelPlace(PUPLACE_CENTERED_LEFT);
    _zoomInput->setUserData(this);
    _zoomInput->setCallback(__mainUI_zoom_cb);
    curx += guiWidth - (2 * buttonHeight);
    _zoomInButton = new puOneShot(curx, cury,
				 curx + buttonHeight, cury + buttonHeight);
    _zoomInButton->setLegend("+");
    _zoomInButton->setUserData(this);
    _zoomInButton->setCallback(__mainUI_zoom_cb);
    curx += buttonHeight;
    _zoomOutButton = new puOneShot(curx, cury,
				  curx + buttonHeight, cury + buttonHeight);
    _zoomOutButton->setLegend("-");
    _zoomOutButton->setUserData(this);
    _zoomOutButton->setCallback(__mainUI_zoom_cb);
    curx = bigSpace;
    cury += buttonHeight + bigSpace;

    // Elevation: a text output
    _elevText = new puText(curx, cury);
    _elevText->setLabel("Elev: 543 ft");
    curx += boxWidth;

    // Mouse/centre: a text output
    _mouseText = new puText(curx, cury);
    _mouseText->setLabel("centre");

    curx = bigSpace;
    cury += buttonHeight + bigSpace;

    // Longitude: an input field
    _lonInput = new puInput(curx + labelWidth, cury, 
			   curx + guiWidth, cury + buttonHeight);
    _lonInput->setLabel("Lon:");
    _lonInput->setLabelPlace(PUPLACE_CENTERED_LEFT);
    _lonInput->setUserData(this);
    _lonInput->setCallback(__mainUI_position_cb);
    cury += buttonHeight + bigSpace;

    // Latitude: an input field
    _latInput = new puInput(curx + labelWidth, cury, 
			   curx + guiWidth, cury + buttonHeight);
    _latInput->setLabel("Lat:");
    _latInput->setLabelPlace(PUPLACE_CENTERED_LEFT);
    _latInput->setUserData(this);
    _latInput->setCallback(__mainUI_position_cb);
    cury += buttonHeight + bigSpace;

    //////////////////////////////////////////////////////////////////////
    // Preferences
    //////////////////////////////////////////////////////////////////////
    _preferencesFrame = new puFrame(0, cury, width, cury + preferencesHeight);
    curx = 0;

    // EYE - Do all C compilers accept unicode in constant strings?
    // If so, get rid of degreeSymbol (in misc.hxx).
    _degMinSecBoxLabels[0] = "dd°mm'ss\"";
    _degMinSecBoxLabels[1] = "dd.ddd°";
    _degMinSecBoxLabels[2] = NULL;
    _degMinSecBox = 
	new puButtonBox(curx, cury, 
			curx + boxWidth, cury + boxHeight,
			(char **)_degMinSecBoxLabels, TRUE);
    _degMinSecBox->setUserData(_ac);
    _degMinSecBox->setCallback(__mainUI_degMinSec_cb);

    curx += boxWidth;
    _magTrueBoxLabels[0] = "Magnetic";
    _magTrueBoxLabels[1] = "True";
    _magTrueBoxLabels[2] = NULL;
    _magTrueBox = 
	new puButtonBox(curx, cury, 
			curx + boxWidth, cury + boxHeight,
			(char **)_magTrueBoxLabels, TRUE);
    _magTrueBox->setUserData(_ac);
    _magTrueBox->setCallback(__mainUI_magTrue_cb);

    _gui->close();

    // Initialize our widgets.
    _setDegMinSec();
    _setMagTrue();
    _setMEFs();
    _setAutocentreMode();
    _setPosition();
    _setCentreType();
    _setZoom();
    _setOverlays();
    _setTrackLimit();
    _setTrack();
    _setTrackList();

    // Subscribe to notifications of interest.
    subscribe(Notification::DegMinSec);
    subscribe(Notification::MagTrue);
    subscribe(Notification::MEFs);
    subscribe(Notification::AutocentreMode);
    subscribe(Notification::Moved);
    subscribe(Notification::MouseMoved);
    subscribe(Notification::NewScenery);
    subscribe(Notification::CentreType);
    subscribe(Notification::Zoomed);
    subscribe(Notification::OverlayToggled);
    subscribe(Notification::FlightTrackModified);
    subscribe(Notification::NewFlightTrack);
    subscribe(Notification::FlightTrackList);
}

MainUI::~MainUI()
{
    if (_fileDialog) {
	puDeleteObject(_fileDialog);
    }
    if (_closeOkDialog) {
	puDeleteObject(_closeOkDialog);
    }
    if (_networkPopup) {
	delete _networkPopup;
    }

    puDeleteObject(_gui);
}

void MainUI::saveAs()
{
    _saveAs_cb(_saveAsButton);
}

void MainUI::load()
{
    _load_cb(_loadButton);
}

void MainUI::unload()
{
    _unload_cb(_unloadButton);
}

void MainUI::notification(Notification::type n)
{
    if (n == Notification::DegMinSec) {
	_setDegMinSec();
	_setPosition();
    } else if (n == Notification::MagTrue) {
	_setMagTrue();
    } else if (n == Notification::MEFs) {
	_setMEFs();
    } else if (n == Notification::AutocentreMode) {
	_setAutocentreMode();
    } else if ((n == Notification::Moved) ||
    	       (n == Notification::MouseMoved) ||
    	       (n == Notification::NewScenery)) {
    	_setPosition();
    } else if (n == Notification::CentreType) {
    	_setCentreType();
	_setPosition();
    } else if (n == Notification::Zoomed) {
	_setZoom();
	// If we zoom in while in mouse mode, our position information
	// will probably change (unless the mouse is dead centre).
	if (_aw->centreType() == AtlasWindow::MOUSE) {
	    _setPosition();
	}
    } else if (n == Notification::OverlayToggled) {
	_setOverlays();
    } else if (n == Notification::FlightTrackModified) {
	// If a flight track changes we only care about its new
	// length.
	_setTrackLimit();
    } else if (n == Notification::NewFlightTrack) {
	// A new flight track is to be displayed.
	_setTrack();
    } else if (n == Notification::FlightTrackList) {
	// The flight track list has changed.
	_setTrackList();
	_setTrack();
    } else {
	assert(0);
    }

    _aw->postRedisplay();
}

void MainUI::_zoom_cb(puObject *o) 
{
    if (o == _zoomInButton) {
	_aw->zoomBy(1.0 / zoomFactor);
    } else if (o == _zoomOutButton) {
	_aw->zoomBy(zoomFactor);
    } else if (o == _zoomInput) {
	// Read zoom level from zoom input field.
	char *buffer;
	o->getValue(&buffer);
	double zoom;
	int n_items = sscanf(buffer, "%lf", &zoom);
	if (n_items != 1) {
	    return;
	}
	_aw->zoomTo(zoom);
    }

    _aw->postRedisplay();
}

void MainUI::_overlay_cb(puObject *o)
{
    bool on = (o->getValue() != 0);
    if (o == _navaidsToggle) {
	_aw->setOverlayVisibility(Overlays::NAVAIDS, on);
    } else if (o == _navVOR) {
	_aw->setOverlayVisibility(Overlays::VOR, on);
    } else if (o == _navNDB) {
	_aw->setOverlayVisibility(Overlays::NDB, on);
    } else if (o == _navILS) {
	_aw->setOverlayVisibility(Overlays::ILS, on);
    } else if (o == _navDME) {
	_aw->setOverlayVisibility(Overlays::DME, on);
    } else if (o == _navFIX) {
	_aw->setOverlayVisibility(Overlays::FIXES, on);
    } else if (o == _airportsToggle) {
	_aw->setOverlayVisibility(Overlays::AIRPORTS, on);
    } else if (o == _airwaysToggle) {
	_aw->setOverlayVisibility(Overlays::AIRWAYS, on);
    } else if (o == _awyHigh) {
	_aw->setOverlayVisibility(Overlays::HIGH, on);
    } else if (o == _awyLow) {
	_aw->setOverlayVisibility(Overlays::LOW, on);
    } else if (o == _labelsToggle) {
	_aw->setOverlayVisibility(Overlays::LABELS, on);
    } else if (o == _tracksToggle) {
	_aw->setOverlayVisibility(Overlays::TRACKS, on);
    }
}

// Parses the latitude or longitude in the given puInput and returns
// the corresponding value.  Southern latitudes and western longitudes
// are negative.  If the string cannot be parsed, returns
// numeric_limits<float>::max().
static double __scanLatLon(puInput *p)
{
    float result = std::numeric_limits<float>::max();
    char *buffer;
    p->getValue(&buffer);

    char nsew, deg_ch, min_ch = ' ', sec_ch = ' ';
    float degrees = 0.0, minutes = 0.0, seconds = 0.0;

    // Free-format entry: "N51", "N50.99*", "N50*59 24.1", etc.
    int n_items = 
	sscanf(buffer, " %c %f%c %f%c %f%c",
	       &nsew, &degrees, &deg_ch, &minutes, &min_ch, &seconds, &sec_ch);
    if (n_items >= 2) {
	result = (degrees + minutes / 60 + seconds / 3600);
	if (strchr("SsWw", nsew) != NULL) {
	    result = -result;
	}
    }
    
    return result;
}

void MainUI::_position_cb(puObject *o) 
{
    puInput *input = (puInput *)o;
    if (input->isAcceptingInput()) {
	// Text field has just received keyboard focus, so do nothing.
	return;
    }

    double lat = __scanLatLon(_latInput);
    double lon = __scanLatLon(_lonInput);
    if ((lat != std::numeric_limits<double>::max()) &&
	(lon != std::numeric_limits<double>::max())) {
	// Both values are valid, so move to the point they specify.
	_aw->movePosition(lat, lon);
    }
}

void MainUI::_saveAsFile_cb(puObject *o) 
{
    // If the user hit "Ok", then the string value of the save dialog
    // will be non-empty.
    char *file = _fileDialog->getStringValue();
    if (strcmp(file, "") != 0) {
	// EYE - we should warn the user if they're overwriting an
	// existing file.  Unfortunately, PUI's dialogs are pretty
	// tough to use because they're require setting up callbacks
	// and maintaining our state.  It'll be easier to just switch
	// GUIs, if you ask me.
	assert(_ac->currentTrack());

	// EYE - it's important that we don't let the current track change
	// while the save dialog is active.
	_ac->saveTrackAs(file);
    }

    // Unfortunately, being a subclass of puDialogBox, a hidden
    // puaFileSelector will continue to grab all mouse events.  So, it
    // must be deleted, not hidden when we're finished.  This is
    // unfortunate because we can't "start up from where we left off"
    // - each time it's created, it's created anew.
    puDeleteObject(_fileDialog);
    _fileDialog = NULL;
    // EYE - is this always true?
    _saveAsButton->activate();
}

void MainUI::_saveAs_cb(puObject *o) 
{
    if (_fileDialog == NULL) {
	_saveAsButton->greyOut();
	_fileDialog = new puaFileSelector(250, 150, 500, 400, "", 
					 "Save Flight Track");
	_fileDialog->setUserData(this);
	_fileDialog->setCallback(__mainUI_saveAsFile_cb);
	_aw->postRedisplay();
    }
}

void MainUI::_loadFile_cb(puObject *o)
{
    // If the user hit "Ok", then the string value of the save dialog
    // will be non-empty.
    char *file = _fileDialog->getStringValue();
    if (strcmp(file, "") != 0) {
	_ac->loadTrack(file);
    }

    puDeleteObject(_fileDialog);
    _fileDialog = NULL;
}

void MainUI::_load_cb(puObject *o) 
{
    // Open a flight file (unless the file dialog is already active
    // doing something else).
    if (_fileDialog == NULL) {
	_fileDialog = new puaFileSelector(250, 150, 500, 400, "",
					  "Open Flight Track");
	_fileDialog->setUserData(this);
	_fileDialog->setCallback(__mainUI_loadFile_cb);
	_aw->postRedisplay();
    }
}

// Unloads the current flight track.
void MainUI::_unload_cb(puObject *o)
{
    if (!_ac->currentTrack()) {
	return;
    } 

    if (_ac->currentTrack()->modified()) {
	_closeOkDialog = new AtlasDialog("The current track is unsaved.\nIf you close it, the track data will be lost.\nDo you want to close it?\n", 
					 "OK", "Cancel", "", 
					 __mainUI_closeOk_cb, this);
    } else {
	// Remove the current track.
	_ac->removeTrack();
    }
}

void MainUI::_trackSelect_cb(puObject *o)
{
    int i = _tracksComboBox->getCurrentItem();

    if (o == _prevTrackButton) {
	i--;
    } else if (o == _nextTrackButton) {
	i++;
    }

    _ac->setCurrentTrack(i);
}

void MainUI::_attach_cb(puObject *o) 
{
    _networkPopup = new NetworkPopup(100, 100, this);

    // Fill in default values.
    _networkPopup->setPort((int)Preferences::defaultPort);
    _networkPopup->setDevice(Preferences::defaultSerialDevice);
    _networkPopup->setBaud(Preferences::defaultBaudRate);

    // Make the network stuff selected by default.
    _networkPopup->setNetwork(true);
}

void MainUI::_networkPopup_cb(bool okay)
{
    if (okay) {
	FlightTrack* track = NULL;
	if (_networkPopup->networkSelected()) {
	    // It's a network connection.  Check to make sure we don't
	    // have one already.
	    int port = _networkPopup->port();
	    if (_ac->find(port) == FlightTracks::NaFT) {
		// We didn't find a match, so open a connection.
		int bufferSize = _trackLimitInput->getIntegerValue();
		track = new FlightTrack(_ac->navData(), port, bufferSize);
		assert(track);
		// Add track (selected), and display it.
		_ac->addTrack(track);
	    }
	} else {
	    // It's a serial connection.  Check to make sure we don't
	    // have one already.
	    // EYE - untested!
	    const char *device = _networkPopup->device();
	    int baud = _networkPopup->baud();
	    if (_ac->find(device, baud) == FlightTracks::NaFT) {
		// We didn't find a match, so open a connection.
		int bufferSize = _trackLimitInput->getIntegerValue();
		track = new FlightTrack(_ac->navData(), device, baud, 
					bufferSize);
		assert(track);
		// Add track (selected), and display it.
		_ac->addTrack(track);
	    }
	}
    }

    delete _networkPopup;
    _networkPopup = NULL;
}

void MainUI::_closeOk_cb(bool okay) 
{
    puDeleteObject(_closeOkDialog);
    _closeOkDialog = NULL;
    if (okay) {
	// Unload the track with extreme prejudice.
	_ac->removeTrack();
    }
}

void MainUI::_setDegMinSec()
{
    if (_degMinSecBox->getValue() == _ac->degMinSec()) {
	_degMinSecBox->setValue(!_ac->degMinSec());
    }
}

void MainUI::_setMagTrue()
{
    if (_magTrueBox->getValue() == _ac->magTrue()) {
	_magTrueBox->setValue(!_ac->magTrue());
    }
}

void MainUI::_setMEFs()
{
    _MEFToggle->setValue(_ac->MEFs());
}

void MainUI::_setAutocentreMode()
{
    _trackAircraftToggle->setValue(_aw->autocentreMode());
}

// Updates the lat/lon/elev text fields.
void MainUI::_setPosition()
{
    // Get our current "active" location (either the mouse or the
    // centre of the screen).
    ScreenLocation *loc = _aw->currentLocation();
    if (!loc->coord().valid()) {
	return;
    }

    double lat = loc->lat();
    double lon = loc->lon();
    double elev = Bucket::NanE;
    if (loc->validElevation()) {
	elev = loc->elev();
    }

    // EYE - if the keyboard focus leaves one of these text fields,
    // the fields don't update unless an event happens (like wiggling
    // the mouse).  This should be fixed.
    static AtlasString latStr, lonStr, elevStr;
    if (!_latInput->isAcceptingInput()) {
	latStr.printf("%c%s", (lat < 0) ? 'S' : 'N', 
		      formatAngle(lat, _ac->degMinSec()));
	_latInput->setValue(latStr.str());
    }

    if (!_lonInput->isAcceptingInput()) {
	lonStr.printf("%c%s", (lon < 0) ? 'W' : 'E', 
		      formatAngle(lon, _ac->degMinSec()));
	_lonInput->setValue(lonStr.str());
    }

    if (elev != Bucket::NanE) {
	elevStr.printf("Elev: %.0f ft", elev * SG_METER_TO_FEET);
    } else {
	elevStr.printf("Elev: n/a");
    }
    _elevText->setLabel(elevStr.str());
}

void MainUI::_setCentreType()
{
    if (_aw->centreType() == AtlasWindow::MOUSE) {
	_mouseText->setLabel("mouse");
    } else {
	_mouseText->setLabel("centre");
    }
}

// Updates the zoom field.
void MainUI::_setZoom()
{
    if (!_zoomInput->isAcceptingInput()) {
	// _zoomInput->setValue((float)_ac->metresPerPixel());
	_zoomInput->setValue((float)_aw->scale());
    }
}

// Sets all the overlay toggles.
void MainUI::_setOverlays()
{
    _navaidsToggle->setValue(_aw->isOverlayVisible(Overlays::NAVAIDS));
    if (_navaidsToggle->getValue()) {
	_navVOR->activate();
	_navNDB->activate();
	_navILS->activate();
	_navDME->activate();
	_navFIX->activate();
    } else {
	_navVOR->greyOut();
	_navNDB->greyOut();
	_navILS->greyOut();
	_navDME->greyOut();
	_navFIX->greyOut();
    }
    _navVOR->setValue(_aw->isOverlayVisible(Overlays::VOR));
    _navNDB->setValue(_aw->isOverlayVisible(Overlays::NDB));
    _navILS->setValue(_aw->isOverlayVisible(Overlays::ILS));
    _navDME->setValue(_aw->isOverlayVisible(Overlays::DME));
    _navFIX->setValue(_aw->isOverlayVisible(Overlays::FIXES));
    _airportsToggle->setValue(_aw->isOverlayVisible(Overlays::AIRPORTS));
    _airwaysToggle->setValue(_aw->isOverlayVisible(Overlays::AIRWAYS));
    if (_airwaysToggle->getValue()) {
	_awyHigh->activate();
	_awyLow->activate();
    } else {
	_awyHigh->greyOut();
	_awyLow->greyOut();
    }
    _awyHigh->setValue(_aw->isOverlayVisible(Overlays::HIGH));
    _awyLow->setValue(_aw->isOverlayVisible(Overlays::LOW));
    _labelsToggle->setValue(_aw->isOverlayVisible(Overlays::LABELS));
    _tracksToggle->setValue(_aw->isOverlayVisible(Overlays::TRACKS));
}

void MainUI::_setTrackLimit()
{
    if (_ac->currentTrack() == NULL) {
	_trackSizeLabel.printf("Track size: n/a");
    } else {
	_trackSizeLabel.printf("Track size: %d points", 
			       _ac->currentTrack()->size());
    }
    _trackSizeText->setLabel(_trackSizeLabel.str());
}

// Called when a new track has been selected.

// EYE - instead of having a bunch of if/then/else's to determine the
// state, maybe I should just set each button depending on the state.
void MainUI::_setTrack()
{
    FlightTrack *track = _ac->currentTrack();
    if (!track) {
	// If there's no track, we can only load and attach.
	_unloadButton->greyOut();
	_detachButton->greyOut();
	_saveButton->greyOut();
	_saveAsButton->greyOut();
	_clearButton->greyOut();
	_jumpToButton->greyOut();
	_trackLimitInput->greyOut();
    } else if (track->live()) {
	// If the track is live (listening for input from FlightGear),
	// then we don't allow unloading (even if it has a file
	// associated with it).  It can be saved if it has a file and
	// has been modified.
	_unloadButton->greyOut();
	_detachButton->activate();
	_saveAsButton->activate();
	_clearButton->activate();
	_jumpToButton->activate();
	_trackLimitInput->activate();
	_trackLimitInput->setValue((int)track->maxBufferSize());
	if (track->hasFile() && track->modified()) {
	    _saveButton->activate();
	} else {	    
	    _saveButton->greyOut();
	}
    } else if (track->hasFile()) {
	_unloadButton->activate();
	_detachButton->greyOut();
	_saveAsButton->activate();
	// Only live tracks can be cleared.
	_clearButton->greyOut();
	_jumpToButton->activate();
	_trackLimitInput->greyOut();
	if (track->modified()) {
	    _saveButton->activate();
	} else {
	    _saveButton->greyOut();
	}
    } else {
	// The track is not live, but has no file.  This can happen if
	// we've detached a live track but haven't saved it yet.  We
	// allow the user to unload it, but not clear it (they
	// actually amount to the same thing here, but I think
	// clearing matches live tracks better, because it implies
	// that more data will be coming in later).
	_unloadButton->activate();
	_detachButton->greyOut();
	_saveButton->greyOut();
	_saveAsButton->activate();
	_clearButton->greyOut();
	_jumpToButton->activate();
	_trackLimitInput->greyOut();
    }

    // Select the current track in the combo box.
    size_t trackNo = _ac->currentTrackNo();
    if (trackNo != FlightTracks::NaFT) {
	// We temporarily remove the callback because calling
	// setCurrentItem() will call it, something we generally don't
	// want (we only want it called in response to user input).
	_tracksComboBox->setCallback(NULL);
	_tracksComboBox->setCurrentItem(trackNo);
	_tracksComboBox->setCallback(__mainUI_trackSelect_cb);
    }

    // Activate or deactivate the previous and next buttons as appropriate.
    if ((trackNo == 0) || (trackNo == FlightTracks::NaFT)) {
	_prevTrackButton->greyOut();
    } else {
	_prevTrackButton->activate();
    } 
    if (trackNo >= (_ac->tracks().size() - 1)) {
	_nextTrackButton->greyOut();
    } else {
	_nextTrackButton->activate();
    }

    _setTrackLimit();
}

// Called when we need to set our track list.
void MainUI::_setTrackList()
{
    if (_trackListStrings != NULL) {
	for (int i = 0; i < _tracksComboBox->getNumItems(); i++) {
	    free(_trackListStrings[i]);
	}
	free(_trackListStrings);
    }

    _trackListStrings = (char **)malloc(sizeof(char *) * 
					(_ac->tracks().size() + 1));
    for (unsigned int i = 0; i < _ac->tracks().size(); i++) {
	// The display styles are the same as in the graphs window.
	_trackListStrings[i] = strdup(_ac->trackAt(i)->niceName());
    }
    _trackListStrings[_ac->tracks().size()] = (char *)NULL;

    _tracksComboBox->newList(_trackListStrings);
}

//////////////////////////////////////////////////////////////////////
// InfoUI
//////////////////////////////////////////////////////////////////////
InfoUI::InfoUI(int x, int y, AtlasWindow *aw): _aw(aw), _ac(aw->ac())
{
    const int textHeight = 15;
    const int bigSpace = 5;
    // EYE - magic numbers
    const int textWidth = 200;

    const int height = textHeight * 8 + 3 * bigSpace;
    const int width = textWidth * 2;

    int curx, cury;

    _gui = new puPopup(x, y);

    _infoFrame = new puFrame(0, 0, width, height);

    // EYE - check how the new font baseline is calculated vs what
    // PLIB expects!
    curx = bigSpace;
    cury = 0;
    // EYE - 5 = hack
    _ADFColour = new puFrame(curx, cury + 5, 
			    curx + textHeight - 2, cury + 5 + textHeight - 2);
    _ADFColour->setStyle(PUSTYLE_PLAIN);
    // EYE - 0.6 alpha comes from puSetDefaultColourScheme()
    _ADFColour->setColour(PUCOL_FOREGROUND, 
			  globals.adfColour[0], 
			  globals.adfColour[1], 
			  globals.adfColour[2], 
			  0.6);
    _ADFText = new puText(curx + textHeight, cury); cury += textHeight;

    _VOR2Colour = new puFrame(curx, cury + 5, 
			      curx + textHeight - 2, cury + 5 + textHeight - 2);
    _VOR2Colour->setStyle(PUSTYLE_PLAIN);
    _VOR2Colour->setColour(PUCOL_FOREGROUND, 
			   globals.vor2Colour[0], 
			   globals.vor2Colour[1], 
			   globals.vor2Colour[2], 
			   0.6);
    _VOR2Text = new puText(curx + textHeight, cury); cury += textHeight;

    _VOR1Colour = new puFrame(curx, cury + 5, 
			      curx + textHeight - 2, cury + 5 + textHeight - 2);
    _VOR1Colour->setStyle(PUSTYLE_PLAIN);
    _VOR1Colour->setColour(PUCOL_FOREGROUND, 
			   globals.vor1Colour[0], 
			   globals.vor1Colour[1], 
			   globals.vor1Colour[2], 
			   0.6);
    _VOR1Text = new puText(curx + textHeight, cury); 
    cury += textHeight + bigSpace;

    _spdText = new puText(curx, cury); cury += textHeight;
    _hdgText = new puText(curx, cury); cury += textHeight;
    _altText = new puText(curx, cury); cury += textHeight;
    _lonText = new puText(curx, cury); cury += textHeight;
    _latText = new puText(curx, cury); cury += textHeight;

    curx += textWidth;
    cury = height - 2 * bigSpace - 2 * textHeight;
    _dstText = new puText(curx, cury); cury += textHeight;
    _hmsText = new puText(curx, cury); cury += textHeight;

    _gui->close();

    // Initialize our displayed data.
    _setVisibility();
    _setText();

    // Subscribe to flight track events, and events that change the
    // display of flight track data.
    subscribe(Notification::AircraftMoved);
    subscribe(Notification::FlightTrackModified);
    subscribe(Notification::NewFlightTrack);
    subscribe(Notification::ShowTrackInfo);
    subscribe(Notification::DegMinSec);
    subscribe(Notification::MagTrue);
}

InfoUI::~InfoUI()
{
    puDeleteObject(_gui);
}

void InfoUI::notification(Notification::type n)
{
    if ((n == Notification::AircraftMoved) ||
	(n == Notification::FlightTrackModified) ||
	(n == Notification::DegMinSec) ||
	(n == Notification::MagTrue)) {
	_setText();
    } else if (n == Notification::NewFlightTrack) {
	_setVisibility();
	_setText();
    } else if (n == Notification::ShowTrackInfo) {
	_setVisibility();
    } else {
	assert(0);
    }

    _aw->postRedisplay();
}

// The following functions create nicely formatted strings describing
// tuned-in navaids.  For example, if we're near KSFO and we've set
// NAV1 to 115.8 and its OBS to 280, it might give us a string like:
//
// "NAV 1: 115.8@280° (SFO, 277° TO, 0.9 DME)"
//
// To create the string, we need to know:
//
// - what navaids are in range (SFO in the example above)
// - which radio is being used (NAV 1)
// - the radio settings (115.8@280°)
// - the aircraft position, from which we derive the relative position
//   of the navaid (277° TO, 0.9 DME)
// 
// All the functions have the following parameters:
//
// navs - a vector of in-range, tuned-in navaids
// x - the "number" of the radio (ie, 1 for NAV 1, 2 for NAV 2).  Set
//     it to 0 if you don't want a number to appear in the string
// p - the flight data for the current aircraft position
// freq - the frequency of the radio
// str - the formatted string
//
// These are specific to certain functions:
//
// radial - the OBS position (VORs and ILSs only)
// magTrue - whether headings should be printed as magnetic or true
//           (ILSs and NDBs only)
//
// Generally, for a given aircraft position and radio setting, at most
// one navaid will be tuned in.  However, it is possible for more than
// one to be in range (generally for ILS systems).  In these cases, we
// print out information for the one with the strongest signal, and
// indicate there are more by adding an ellipsis ("...").

// Called for VORs, VORTACs, VOR-DMEs, and DMEs.
static void __VORsAsString(vector<NAV *> &navs, int x, 
			   FlightData *p, int freq, float radial, 
			   AtlasString &str)
{
    NAV *vor = NULL, *dme = NULL;
    string *id = NULL;
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
    str.appendf(": %s@%03.0f%c", formatFrequency(freq), radial, degreeSymbol);

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
	    const char *fromStr;
	    if ((diff <= 90.0) || (diff >= 270.0)) {
		fromStr = "FROM";
	    } else {
		fromStr = "TO";
		ar = normalizeHeading(ar - 180.0, false);
	    }

	    str.appendf(", %03.0f%c %s", normalizeHeading(rint(ar), false), 
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

// Called for ILSs.  Note that many airports have identical
// frequencies for opposing ILS systems (the ones at opposite ends of
// the runway).  In real life, no confusion occurs, because when one
// is turned on, the other is turned off.  In Atlas, everything is
// always on.  When we approach a runway, it turns out that the ILS
// localizer of the opposing runway is the transmitter closest to us
// (the localizer is located at the opposite end of the runway it
// serves).  As we pass over the threshold, then the glideslope
// transmitter of our chosen runway becomes the closest.  Similar
// switches occur at the opposite end.  And if the ILS has a DME
// transmitter, then we may recognize that as closest at varying
// times.
//
// Our solution is to first group all transmitters together based on
// ID.  The various bits of an ILS system (localizer, optional
// glideslope, optional DME) will have identical names.  We then
// compare the signal strengths of the all the localizers (which are
// guaranteed to exist, unlike glideslopes and DMEs) and choose the
// group containing the *weakest* localizer signal.  This heuristic
// works when both ends of a single runway have the same ILS
// frequencies.  It will fail if, for example, two different runways
// (perhaps at two different airports) have the same ILS frequencies.
// As far as I know, this situation never occurs.
static void __ILSsAsString(vector<NAV *> &navs, int x, 
			   FlightData *p, int freq, float radial, 
			   bool magTrue, AtlasString &str)
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
	vector<NAV *> &components = i->second;

	for (unsigned int j = 0; j < components.size(); j++) {
	    NAV *n = components[j];
	    if (n->navtype == NAV_ILS) {
		// To prevent divide-by-zero errors, we arbitrarily
		// set 1 metre as the minimum distance.
		double d = 
		    SG_MAX2(sgdDistanceSquaredVec3(p->cart, n->bounds.center), 
			    1.0);
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
    str.appendf(": %s@%03.0f%c", formatFrequency(freq), radial, degreeSymbol);

    // Navaid information
    if (chosen != NULL) {
	NAV *loc = NULL, *dme = NULL;
	double ar, d;
	const char *magTrueChar = "T";

	for (unsigned int j = 0; j < chosen->size(); j++) {
	    NAV *n = chosen->at(j);
	    if (n->navtype == NAV_ILS) {
		loc = n;

		double endHdg, length;
		geo_inverse_wgs_84(p->lat, p->lon, 
				   n->lat, n->lon, &ar, &endHdg, &length);
		if (magTrue) {
		    magTrueChar = "";
		    ar -= magneticVariation(loc->lat, loc->lon, loc->elev);
		}
	    } else if (n->navtype == NAV_DME) {
		dme = n;
		d = sgdDistanceVec3(p->cart, dme->bounds.center) - dme->magvar;
	    }
	}
	str.appendf(" (%s", id->c_str());

	if (loc != NULL) {
	    str.appendf(", %03.0f%c%s", normalizeHeading(rint(ar), false), 
			degreeSymbol, magTrueChar);
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

// Called for any non-NDB navaids.  Depending on the given frequency,
// we pass things on to __VORsAsString or __ILSsAsString.
static void __NAVsAsString(vector<NAV *> &navs, int x, 
			   FlightData *p, int freq, float radial, 
			   bool magTrue, AtlasString &str)
{
    // Use the frequency (kHz) to decide what we're looking at.  If <
    // 112000 and the hundreds digit is odd, then it is an ILS.
    // Otherwise, it is a VOR.  Note that "VOR" also includes DMEs,
    // and that an ILS can include a DME as well.
    int hundreds = (freq % 1000) / 100;
    if ((freq < 112000) && (hundreds & 0x1)) {
	// ILS
	__ILSsAsString(navs, x, p, freq, radial, magTrue, str);
	return;
    } else {
	__VORsAsString(navs, x, p, freq, radial, str);
    }
}

// Renders the given NDBs, which must match the given frequency, as a
// string.
static void __NDBsAsString(vector<NAV *> &navs, int x, 
			   FlightData *p, int freq, bool magTrue,
			   AtlasString &str)
{
    NAV *match = NULL;
    double strength = 0.0;

    // Find NDB with strongest signal.
    for (unsigned int i = 0; i < navs.size(); i++) {
	NAV *n = navs[i];

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
    assert((navs.size() == 0) || (match != NULL));

    str.printf("ADF");
    if (x > 0) {
	str.appendf(" %d", x);
    }
    str.appendf(": %d", freq);

    if (navs.size() > 0) {
	// Calculate the absolute and relative bearings to the navaid
	// (the absolute bearing is given in magnetic or true
	// degrees).
	double ab, rb;
	double endHdg, length;
	geo_inverse_wgs_84(p->lat, p->lon,
			   match->lat, match->lon,
			   &ab, &endHdg, &length);
	rb = ab - p->hdg;
	char magTrueChar = 'T';
	if (magTrue) {
	    magTrueChar = 'M';
	    ab = ab - 
		magneticVariation(p->lat, p->lon, p->alt * SG_FEET_TO_METER);
	}

	str.appendf(" (%s, %03.0f%c%cB, %03.0f%cRB)",
		    match->id.c_str(), 
		    normalizeHeading(rint(ab), false), degreeSymbol, 
		    magTrueChar,
		    normalizeHeading(rint(rb), false), degreeSymbol);
    }
    if (navs.size() > 1) {
	str.appendf(" ...");
    }
}

// Sets the text strings displayed in the information dialog, based on
// the current position in the current track.
void InfoUI::_setText()
{
    FlightData *p = _ac->currentPoint();
    if (p == (FlightData *)NULL) {
	return;
    }

    static AtlasString latStr, lonStr, altStr, hdgStr, spdStr, hmsStr,
	dstStr, vor1Str, vor2Str, adfStr;

    latStr.printf("Lat: %c%s", (p->lat < 0) ? 'S':'N', 
		  formatAngle(p->lat, _ac->degMinSec()));
    lonStr.printf("Lon: %c%s", (p->lon < 0) ? 'W':'E', 
		  formatAngle(p->lon, _ac->degMinSec()));
    if (_ac->currentTrack()->isAtlasProtocol()) {
	const char *magTrueChar = "T";
	double hdg = p->hdg;
	if (_ac->magTrue()) {
	    magTrueChar = "";
	    // EYE - use the time of the flight instead of current time?
	    hdg -= magneticVariation(p->lat, p->lon, p->alt * SG_FEET_TO_METER);
	}
	hdg = normalizeHeading(rint(hdg), false);
	hdgStr.printf("Hdg: %03.0f%c%s", hdg, degreeSymbol, magTrueChar);
	spdStr.printf("Speed: %.0f kt EAS", p->spd);
    } else {
	const char *magTrueChar = "T";
	double hdg = p->hdg;
	if (_ac->magTrue()) {
	    magTrueChar = "";
	    // EYE - use the time of the flight instead of current time?
	    hdg -= magneticVariation(p->lat, p->lon, p->alt * SG_FEET_TO_METER);
	}
	hdg = normalizeHeading(rint(hdg), false);
	hdgStr.printf("Track: %03.0f%c%s", hdg, degreeSymbol, magTrueChar);
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
    if (_ac->currentTrack()->isAtlasProtocol()) {
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
    	const vector<NAV *> &navaids = p->navaids();
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
    	bool mt = _ac->magTrue();
    	__NAVsAsString(VOR1s, 1, p, p->nav1_freq, p->nav1_rad, mt, vor1Str);
    	__NAVsAsString(VOR2s, 2, p, p->nav2_freq, p->nav2_rad, mt, vor2Str);
    	__NDBsAsString(NDBs, 0, p, p->adf_freq, mt, adfStr);
    } else {
	vor1Str.printf("n/a");
	vor2Str.printf("n/a");
	adfStr.printf("n/a");
    }

    _latText->setLabel(latStr.str());
    _lonText->setLabel(lonStr.str());
    _altText->setLabel(altStr.str());
    _hdgText->setLabel(hdgStr.str());
    _spdText->setLabel(spdStr.str());
    _hmsText->setLabel(hmsStr.str());
    _dstText->setLabel(dstStr.str());
    _VOR1Text->setLabel(vor1Str.str());
    _VOR2Text->setLabel(vor2Str.str());
    _ADFText->setLabel(adfStr.str());
}

// Sets us to be hidden or revealed, based on the existence of a
// flight track and the value of showTrackInfo.
void InfoUI::_setVisibility()
{
    if (_ac->currentTrack() && _ac->showTrackInfo()) {
	reveal();
    } else {
	hide();
    }
}

//////////////////////////////////////////////////////////////////////
// LightingUI
//////////////////////////////////////////////////////////////////////
LightingUI::LightingUI(int x, int y, AtlasWindow *aw): _aw(aw), _ac(aw->ac())
{
    // EYE - magic numbers
    const int bigSpace = 5;
    const int labelWidth = 75, labelHeight = 20;
    const int boxHeight = 55, boxWidth = 100;

    const int directionHeight = boxHeight + labelHeight + bigSpace * 3;
    const int sliderHeight = boxHeight, sliderWidth = bigSpace * 4;

    const int paletteHeight = labelHeight * 2 + bigSpace * 3;

    // const int height = boxHeight * 3 + directionHeight + paletteHeight;
    const int height = boxHeight * 4 + directionHeight + paletteHeight;
    const int width = labelWidth + boxWidth;

    const int paletteWidth = width - 2 * bigSpace - labelHeight;

    int curx, cury;

    _gui = new puPopup(x, y); {
	_frame = new puFrame(0, 0, width, height);

	curx = 0;
	cury = 0;

	//////////////////////////////////////////////////////////////////////
	// Palettes
	//////////////////////////////////////////////////////////////////////
	_paletteFrame = 
	    new puFrame(curx, cury, curx + width, cury + paletteHeight);

	curx = bigSpace;
	cury += bigSpace;
	_paletteComboBox = 
	    new puaComboBox(curx, cury,
			    curx + paletteWidth, cury + labelHeight,
			    NULL, FALSE);
	_paletteComboBox->setUserData(this);
	_paletteComboBox->setCallback(__lightingUI_cb);
	curx += paletteWidth;
	_prevPalette = 
	    new puArrowButton(curx, cury + labelHeight / 2,
			      curx + labelHeight, cury + labelHeight,
			      PUARROW_UP);
	_prevPalette->setUserData(this);
	_prevPalette->setCallback(__lightingUI_cb);
	_nextPalette = 
	    new puArrowButton(curx, cury,
			      curx + labelHeight, cury + labelHeight / 2,
			      PUARROW_DOWN);
	_nextPalette->setUserData(this);
	_nextPalette->setCallback(__lightingUI_cb);
	cury += labelHeight + bigSpace;

	curx = bigSpace;
	_paletteLabel = new puText(curx, cury);
	_paletteLabel->setLabel("Palette");
	cury += labelHeight + bigSpace;

	//////////////////////////////////////////////////////////////////////
	// Light direction
	//////////////////////////////////////////////////////////////////////

	// Frame surrounding azimuth/elevation stuff.
	curx = 0;
	_directionFrame = 
	    new puFrame(curx, cury, curx + width, cury + directionHeight);

	// Azimuth dial and elevation slider.
	curx = bigSpace;
	cury += bigSpace;
	_azimuthDial = new puDial(curx, cury, boxHeight, 0.0, 360.0, 1.0);
	_azimuthDial->setLabelPlace(PUPLACE_UPPER_RIGHT);
	_azimuthDial->setLabel("Azimuth");
	_azimuthDial->setUserData(this);
	_azimuthDial->setCallback(__lightingUI_cb);

	curx = width - bigSpace - sliderWidth;
	_elevationSlider = 
	    new puSlider(curx, cury, sliderHeight, TRUE, sliderWidth);
	_elevationSlider->setLabelPlace(PUPLACE_LOWER_LEFT);
	_elevationSlider->setLabel("Elevation");
	_elevationSlider->setMinValue(0.0);
	_elevationSlider->setMaxValue(90.0);
	_elevationSlider->setStepSize(1.0);
	_elevationSlider->setUserData(this);
	_elevationSlider->setCallback(__lightingUI_cb);

	curx = bigSpace;
 	cury += boxHeight + bigSpace;
	_directionLabel = new puText(curx, cury);
	_directionLabel->setLabel("Light direction");

	cury = paletteHeight + directionHeight;

	//////////////////////////////////////////////////////////////////////
	// Lighting toggles
	//////////////////////////////////////////////////////////////////////

	// Smooth/flat polygon shading
	curx = 0;
	cury += boxHeight / 2;
	_polygonsLabel = new puText(curx, cury);
	_polygonsLabel->setLabel("Polygons");

	curx += labelWidth;
	cury -= boxHeight / 2;
	_polygonsLabels[0] = "smooth";
	_polygonsLabels[1] = "flat";
	_polygonsLabels[2] = NULL;
	_polygons = new puButtonBox(curx, cury, 
				   curx + boxWidth, cury + boxHeight,
				   (char **)_polygonsLabels, TRUE);
	_polygons->setUserData(this);
	_polygons->setCallback(__lightingUI_cb);

	curx = 0;
	cury += boxHeight;

	// Lighting
	cury += boxHeight / 2;
	_lightingLabel = new puText(curx, cury);
	_lightingLabel->setLabel("Lighting");

	curx += labelWidth;
	cury -= boxHeight / 2;
	_lightingLabels[0] = "on";
	_lightingLabels[1] = "off";
	_lightingLabels[2] = NULL;
	_lighting = new puButtonBox(curx, cury, 
				   curx + boxWidth, cury + boxHeight,
				   (char **)_lightingLabels, TRUE);
	_lighting->setUserData(this);
	_lighting->setCallback(__lightingUI_cb);

	curx = 0;
	cury += boxHeight;

	// Contour _lines
	cury += boxHeight / 2;
	_linesLabel = new puText(curx, cury);
	_linesLabel->setLabel("Contour\nlines");

	curx += labelWidth;
	cury -= boxHeight / 2;
	_linesLabels[0] = "on";
	_linesLabels[1] = "off";
	_linesLabels[2] = NULL;
	_lines = new puButtonBox(curx, cury, 
				curx + boxWidth, cury + boxHeight,
				(char **)_linesLabels, TRUE);
	_lines->setUserData(this);
	_lines->setCallback(__lightingUI_cb);
	_lines->setLegend("you shouldn't see this");

	curx = 0;
	cury += boxHeight;

	// Discrete/smoothed contour colours
	cury += boxHeight / 2;
	_contoursLabel = new puText(curx, cury);
	_contoursLabel->setLabel("Contours");

	curx += labelWidth;
	cury -= boxHeight / 2;
	_contoursLabels[0] = "discrete";
	_contoursLabels[1] = "smoothed";
	_contoursLabels[2] = NULL;
	_contours = new puButtonBox(curx, cury, 
				   curx + boxWidth, cury + boxHeight,
				   (char **)_contoursLabels, TRUE);
	_contours->setUserData(this);
	_contours->setCallback(__lightingUI_cb);
	_contours->setLegend("you shouldn't see this");

	cury += boxHeight;
    }
    _gui->close();
    _gui->hide();

    // Initialize our widgets.
    _setDiscreteContours();
    _setContourLines();
    _setLightingOn();
    _setSmoothShading();
    _setAzimuth();
    _setElevation();
    _setPaletteList();

    // Subscribe to notifications of interest.
    subscribe(Notification::DiscreteContours);
    subscribe(Notification::ContourLines);
    subscribe(Notification::LightingOn);
    subscribe(Notification::SmoothShading);
    subscribe(Notification::Azimuth);
    subscribe(Notification::Elevation);
    subscribe(Notification::Palette);
    subscribe(Notification::PaletteList);
}

LightingUI::~LightingUI()
{
    puDeleteObject(_gui);
}

void LightingUI::getSize(int *w, int *h)
{
    _gui->getSize(w, h);
}

void LightingUI::setPosition(int x, int y)
{
    _gui->setPosition(x, y);
}

void LightingUI::notification(Notification::type n)
{
    if (n == Notification::DiscreteContours) {
	_setDiscreteContours();
    } else if (n == Notification::ContourLines) {
	_setContourLines();
    } else if (n == Notification::LightingOn) {
	_setLightingOn();
    } else if (n == Notification::SmoothShading) {
	_setSmoothShading();
    } else if (n == Notification::Azimuth) {
	_setAzimuth();
    } else if (n == Notification::Elevation) {
	_setElevation();
    } else if (n == Notification::Palette) {
	_setPalette();
    } else if (n == Notification::PaletteList) {
	_setPaletteList();
    } else {
	assert(0);
    }

    _aw->postRedisplay();
}

void LightingUI::_setDiscreteContours()
{
    if (_contours->getValue() == _ac->discreteContours()) {
	_contours->setValue(!_ac->discreteContours());
    }
}

void LightingUI::_setContourLines()
{
    if (_lines->getValue() == _ac->contourLines()) {
	_lines->setValue(!_ac->contourLines());
    }
}

void LightingUI::_setLightingOn()
{
    if (_lighting->getValue() == _ac->lightingOn()) {
	_lighting->setValue(!_ac->lightingOn());
    }
}

void LightingUI::_setSmoothShading()
{
    if (_polygons->getValue() == _ac->smoothShading()) {
	_polygons->setValue(!_ac->smoothShading());
    }
}

void LightingUI::_setAzimuth()
{
    // Note that on the azimuth dial, 0 degrees is down, 90 degrees
    // left, 180 degrees up, and 270 degrees right.
    float dialPos = normalizeHeading(180.0 + _ac->azimuth());
    _azimuthDial->setValue(dialPos);

    static AtlasString azStr;
    azStr.printf("%.0f", _ac->azimuth());
    _azimuthDial->setLegend(azStr.str());
}

void LightingUI::_setElevation()
{
    _elevationSlider->setValue(_ac->elevation());

    static AtlasString elStr;
    elStr.printf("%.0f", _ac->elevation());
    _elevationSlider->setLegend(elStr.str());
}

void LightingUI::_setPalette()
{
    // EYE - what if size() == 0?
    size_t i = _ac->currentPaletteNo();
    const vector<Palette *>& p = _ac->palettes();

    if (i >= p.size()) {
	i = p.size() - 1;
    }

    if (p.size() > 0) {
	// Temporarily unset the callback (we don't want our lighting
	// callback to be called when we call setCurrentItem()).
	_paletteComboBox->setCallback(NULL);
	_paletteComboBox->setCurrentItem(i);
	_paletteComboBox->setUserData(this);
	_paletteComboBox->setCallback(__lightingUI_cb);
    }

    if (i == 0) {
	_prevPalette->greyOut();
    } else {
	_prevPalette->activate();
    }
    if (i == (p.size() - 1)) {
	_nextPalette->greyOut();
    } else {
	_nextPalette->activate();
    }
}

void LightingUI::_setPaletteList()
{
    // EYE - we should avoid work if we can - should we test if things
    // have really changed, or hold a _dirty variable?
    static char **paletteList = NULL;

    if (paletteList != NULL) {
	for (int i = 0; i < _paletteComboBox->getNumItems(); i++) {
	    free(paletteList[i]);
	}
	free(paletteList);
    }

    const vector<Palette *>& p = _ac->palettes();
    paletteList = (char **)malloc(sizeof(char *) * (p.size() + 1));
    for (unsigned int i = 0; i < p.size(); i++) {
	// Display the filename of the palette (but not the path).
	SGPath full(p[i]->path());
	paletteList[i] = strdup(full.file().c_str());
    }
    paletteList[p.size()] = (char *)NULL;

    _paletteComboBox->newList(paletteList);

    // Now that we've updated the palette list, set the currently
    // selected one.
    _setPalette();
}

void LightingUI::_cb(puObject *o)
{
    if (o == _contours) {
	_ac->setDiscreteContours(_contours->getValue() == 0);
    } else if (o == _lines) {
	_ac->setContourLines(_lines->getValue() == 0);
    } else if (o == _lighting) {
	_ac->setLightingOn(_lighting->getValue() == 0);
    } else if (o == _polygons) {
	_ac->setSmoothShading(_polygons->getValue() == 0);
    } else if (o == _azimuthDial) {
	float azimuth = normalizeHeading(180.0 + _azimuthDial->getFloatValue());
	_ac->setAzimuth(azimuth);
    } else if (o == _elevationSlider) {
	float elevation = _elevationSlider->getFloatValue();
	_ac->setElevation(elevation);
    } else if (o == _paletteComboBox) {
	_ac->setCurrentPalette(_paletteComboBox->getCurrentItem());
    } else if (o == _prevPalette) {
	_ac->setCurrentPalette(_paletteComboBox->getCurrentItem() - 1);
    } else if (o == _nextPalette) {
	_ac->setCurrentPalette(_paletteComboBox->getCurrentItem() + 1);
    }

    glutPostRedisplay();
}

//////////////////////////////////////////////////////////////////////
// HelpUI
//////////////////////////////////////////////////////////////////////
HelpUI::HelpUI(int x, int y, AtlasWindow *aw):
    _ac(aw->ac())
{
    const int textHeight = 250;
    const int buttonHeight = 20;
    const int bigSpace = 5;
    const int width = 500, 
	height = 2 * buttonHeight + 4 * bigSpace + textHeight;
    const int guiWidth = width - 2 * bigSpace;

    int curx, cury;

    _gui = new puPopup(250, 150);
    {
	new puFrame(0, 0, width, height);

	// Text
	curx = bigSpace;
	cury = bigSpace;

	// EYE - arrows value: 0, 1, 2?
	// EYE - scroller width?
	_text = new puaLargeInput(curx, cury, guiWidth, textHeight, 2, 15);
	// EYE - what does this do?
// 	text->rejectInput();
	// Don't allow the user to change the text.
	_text->disableInput();
	// Use a fixed-width font so that things line up nicely.
	_text->setLegendFont(PUFONT_8_BY_13);

	// General button
	curx = bigSpace;
	cury += textHeight + bigSpace;
	_generalButton = new puButton(curx, cury, "General");
	_generalButton->setUserData(this);
	_generalButton->setCallback(__helpUI_cb);
	
	// Keyboard shortcuts button
	int w, h;
	_generalButton->getSize(&w, &h);
	curx += w + bigSpace;
	_keyboardButton = new puButton(curx, cury, "Keyboard shortcuts");
	_keyboardButton->setUserData(this);
	_keyboardButton->setCallback(__helpUI_cb);

	// Version string and website
	curx = bigSpace;
	// EYE - how tall is a button by default?
	cury += buttonHeight + bigSpace;
	_labelText = new puText(curx, cury);
	// EYE - magic website address
	// EYE - clicking it should start a browser
	globals.str.printf("Atlas Version %s   http://atlas.sourceforge.net", 
			    VERSION);
	_label = strdup(globals.str.str());
	_labelText->setLabel(_label);
    }
    _gui->close();
    _gui->hide();

    // General information.
    globals.str.printf("$FG_ROOT\n");
    globals.str.appendf("    %s\n", globals.prefs.fg_root.c_str());
    globals.str.appendf("$FG_SCENERY\n");
    globals.str.appendf("    %s\n", globals.prefs.scenery_root.c_str());
    globals.str.appendf("Atlas maps\n");
    globals.str.appendf("    %s\n", globals.prefs.path.c_str());
    // EYE - this can change!  We need to track changes in the
    // palette, or indicate that this is the default palette.  Also,
    // we really need a function to give us the palette path.
    SGPath palette(globals.prefs.path);
    palette.append("Palettes");
    palette.append(globals.prefs.palette);
    globals.str.appendf("Atlas palette\n");
    globals.str.appendf("    %s\n", palette.c_str());

    // EYE - this can change!
    globals.str.appendf("\n");
    globals.str.appendf("Maps\n");
    globals.str.appendf("    %d maps/tiles\n", 
			 _ac->tileManager()->tileCount(TileManager::DOWNLOADED));
    globals.str.appendf("    resolutions:\n");
    const bitset<TileManager::MAX_MAP_LEVEL> &levels = 
	_ac->tileManager()->mapLevels();
    for (unsigned int i = 0; i < TileManager::MAX_MAP_LEVEL; i++) {
	if (levels[i]) {
	    int x = 1 << i;
	    globals.str.appendf("      %d (%dx%d)\n", i, x, x);
	}
    }

    globals.str.appendf("\n");
    globals.str.appendf("Airports\n");
    globals.str.appendf("    %s/Airports/apt.dat.gz\n", 
			 globals.prefs.fg_root.c_str());
    globals.str.appendf("Navaids\n");
    globals.str.appendf("    %s/Navaids/nav.dat.gz\n", 
			 globals.prefs.fg_root.c_str());
    globals.str.appendf("Fixes\n");
    globals.str.appendf("    %s/Navaids/fix.dat.gz\n", 
			 globals.prefs.fg_root.c_str());
    globals.str.appendf("Airways\n");
    globals.str.appendf("    %s/Navaids/awy.dat.gz\n", 
			 globals.prefs.fg_root.c_str());

    globals.str.appendf("\nOpenGL\n");
    globals.str.appendf("    vendor: %s\n", glGetString(GL_VENDOR));
    globals.str.appendf("    renderer: %s\n", glGetString(GL_RENDERER));
    globals.str.appendf("    version: %s\n", glGetString(GL_VERSION));
    globals.str.appendf("\n");	// puaLargeInput seems to require an extra LF

    _generalText = strdup(globals.str.str());

    // Keyboard shortcuts.
    AtlasString fmt;
    fmt.printf("%%-%ds%%s\n", 16);
    globals.str.printf(fmt.str(), "C-x c", "toggle contour lines");
    globals.str.appendf(fmt.str(), "C-x d", "discrete/smooth contours");
    globals.str.appendf(fmt.str(), "C-x e", "polygon edges on/off");
    globals.str.appendf(fmt.str(), "C-x l", "toggle lighting");
    globals.str.appendf(fmt.str(), "C-x p", "smooth/flat polygon shading");
    globals.str.appendf(fmt.str(), "C-x r", 
			 "palette contours relative to track/mouse/centre");
    globals.str.appendf(fmt.str(), "C-x R", 
			 "palette contours follow track/mouse/centre");
    globals.str.appendf(fmt.str(), "C-x 0", "set palette base to 0.0");
    globals.str.appendf(fmt.str(), "C-space", "mark a point in a route");
    globals.str.appendf(fmt.str(), "C-space C-space", "deactivate route");
    globals.str.appendf(fmt.str(), "C-n", "next flight track");
    globals.str.appendf(fmt.str(), "C-p", "previous flight track");
    globals.str.appendf(fmt.str(), "space", "toggle main interface");
    globals.str.appendf(fmt.str(), "+", "zoom in");
    globals.str.appendf(fmt.str(), "-", "zoom out");
    globals.str.appendf(fmt.str(), "?", "toggle this help");
    globals.str.appendf(fmt.str(), "a", "toggle airways");
    globals.str.appendf(fmt.str(), "A", "toggle airports");
    globals.str.appendf(fmt.str(), "c", "centre the map on the mouse");
    globals.str.appendf(fmt.str(), "d", "toggle info interface and graphs window");
    globals.str.appendf(fmt.str(), "f", "toggle flight tracks");
    globals.str.appendf(fmt.str(), "i", "enlarge airplane image");
    globals.str.appendf(fmt.str(), "I", "shrink airplane image");
    globals.str.appendf(fmt.str(), "j", "toggle search interface");
    globals.str.appendf(fmt.str(), "l", "toggle lighting interface");
    globals.str.appendf(fmt.str(), "m", "toggle mouse and centre mode");
    globals.str.appendf(fmt.str(), "M", "toggle MEF display");
    globals.str.appendf(fmt.str(), "n", "make north point up");
    globals.str.appendf(fmt.str(), "N", "toggle navaids");
    globals.str.appendf(fmt.str(), "o", "open a flight file");
    globals.str.appendf(fmt.str(), "p", "centre map on aircraft");
    globals.str.appendf(fmt.str(), "P", "toggle auto-centering");
    globals.str.appendf(fmt.str(), "q", "quit");
    globals.str.appendf(fmt.str(), "r", "activate/deactivate route");
    globals.str.appendf(fmt.str(), "s", "save current track");
    globals.str.appendf(fmt.str(), "w", "close current flight track");
    globals.str.appendf(fmt.str(), "u", "detach (unattach) current connection");
    globals.str.appendf(fmt.str(), "v", "toggle labels");
    globals.str.appendf(fmt.str(), "x", "toggle x-axis type (time/dist)");
    globals.str.appendf(fmt.str(), "delete", 
			 "delete inactive route/last point of active route");
    globals.str.appendf("\n");	// puaLargeInput seems to require an extra LF

    _keyboardText = strdup(globals.str.str());

    // General information is display by default.
    _setText(_generalButton);
}

HelpUI::~HelpUI()
{
    puDeleteObject(_gui);

    delete _label;
    delete _generalText;
    delete _keyboardText;
}

// This doesn't do much, but I kept it just to make it consistent with
// the callback pattern used elsewhere.
void HelpUI::_cb(puObject *o)
{
    _setText(o);
}

void HelpUI::_setText(puObject *o)
{
    if (o == _generalButton) {
	_generalButton->setValue(1);
	_keyboardButton->setValue(0);
	_text->setValue(_generalText);
    } else if (o == _keyboardButton) {
	_generalButton->setValue(0);
	_keyboardButton->setValue(1);
	_text->setValue(_keyboardText);
    }
}

//////////////////////////////////////////////////////////////////////
// SearchUI
//////////////////////////////////////////////////////////////////////
SearchUI::SearchUI(AtlasWindow *aw, int minx, int miny, int maxx, int maxy):
    Search(minx, miny, maxx, maxy), _aw(aw)
{
}

SearchUI::~SearchUI()
{
}

void SearchUI::searchFinished(int i)
{
    _aw->searchFinished(i);
}

void SearchUI::searchItemSelected(int i)
{
    _aw->searchItemSelected(i);
}

void SearchUI::searchStringChanged(const char *str)
{
    _aw->searchStringChanged(str);
}

int SearchUI::noOfMatches()
{
    return _aw->noOfMatches();
}

char *SearchUI::matchAtIndex(int i)
{
    return _aw->matchAtIndex(i);
}

// Some compilers don't allow floats to be initialized in the class
// declaration, so we do it out here.
const float Route::_pointSize = 10.0;

Route::Route(): active(false)
{
}

Route::~Route()
{
}

void Route::addPoint(SGGeod &p)
{
    _points.push_back(p);
    if (_points.size() > 1) {
	size_t i = _points.size() - 2;
	_segments.push_back(GreatCircle(_points[i], _points[i + 1]));
    }
}

void Route::deleteLastPoint()
{
    // You'd think pop_back() would be smart enough to check for an
    // empty vector, but it's not.
    if (!_points.empty()) {
	_points.pop_back();
    }
    if (!_segments.empty()) {
	_segments.pop_back();
    }
}

void Route::clear()
{
    _points.clear();
    _segments.clear();
}

SGGeod Route::lastPoint()
{
    if (_points.empty()) {
	// EYE - magic number - use Bucket::NanE?  Move Bucket::NanE
	// to Geographics?
	return SGGeod::fromDegM(0.0, 0.0, -100000.0);
    } else {
	return _points.back();
    }
}

float Route::distance()
{
    float result = 0.0;

    for (size_t i = 0; i < _segments.size(); i++) {
	GreatCircle &gc = _segments[i];
	result += gc.distance();
    }

    return result;
}

void Route::draw(double metresPerPixel, const sgdFrustum &frustum, 
		 const sgdMat4 &m, const sgdVec3 eye, atlasFntTexFont *fnt,
		 bool magTrue)
{
    float distance = 0.0;
    for (size_t i = 0; i < _segments.size(); i++) {
	GreatCircle &gc = _segments[i];
	distance += gc.distance();
	_draw(gc, distance, metresPerPixel, frustum, m, fnt, magTrue);
    }
    if (active && !_points.empty()) {
	SGGeod from = lastPoint(), to;
	SGGeodesy::SGCartToGeod(SGVec3<double>(eye[0], eye[1], eye[2]), to);
	GreatCircle gc(from, to);

	glPushAttrib(GL_LINE_BIT); {
	    glEnable(GL_LINE_STIPPLE);
	    glLineStipple(2, 0xAAAA);
	    distance += gc.distance();
	    _draw(gc, distance, metresPerPixel, frustum, m, fnt, magTrue);
	}
	glPopAttrib();
    }
}

void _arrowCallback(LayoutManager *lm, float x, float y, void *userData)
{
    // True if the arrow points right.
    bool right = (bool)userData;
    atlasFntTexFont *f = (atlasFntTexFont *)lm->font();
    float pointSize = lm->pointSize();
    float ascent = f->ascent() * pointSize;

    float xMin, xMax;
    if (right) {
	xMin = x + pointSize * 0.1;
	xMax = x + pointSize;
    } else {
	xMin = x + pointSize * 0.9;
	xMax = x;
    }
    glBegin(GL_LINES); {
	glVertex2f(xMin, y + ascent / 2.0);
	glVertex2f(xMax, y + ascent / 2.0);

	glVertex2f(xMax, y + ascent / 2.0);
	glVertex2f(x + pointSize / 2.0, y + ascent * 0.75);
		    
	glVertex2f(xMax, y + ascent / 2.0);
	glVertex2f(x + pointSize / 2.0, y + ascent * 0.25);
    }
    glEnd();
}

// Draws one end.
void Route::_draw(bool start, const SGGeod &loc, float az, 
		  double metresPerPixel, atlasFntTexFont *fnt, bool magTrue)
{
    float lat = loc.getLatitudeDeg(), lon = loc.getLongitudeDeg();
    float magvar = 0.0;
    const char *mt = "T";
    if (magTrue) {
	mt = "";
	magvar = magneticVariation(lat, lon);
    }
    float radial = normalizeHeading(rint(az - magvar), false);
    AtlasString str;
    str.printf("%03.0f%c%s", radial, degreeSymbol, mt);

    // Create the label, which consists of the azimuth string
    // and an arrow.
    az = normalizeHeading(az);
    // True if the azimuth is pointing right (0 to 180 degrees).
    bool right = ((az >= 0.0) && (az < 180.0));

    LayoutManager lm;
    const float pointSize = _pointSize * metresPerPixel;
    lm.setFont(fnt, pointSize);
    lm.setBoxed(true, true, false);
    lm.setMargin(0.0);
    if (start) {
	lm.setAnchor(LayoutManager::LL);
    } else {
	lm.setAnchor(LayoutManager::LR);
    }
    lm.begin(); {
	if (right) {
	    lm.addText(str.str());
	    lm.addBox(pointSize, 0.0, _arrowCallback, (void *)true);
	} else {
	    lm.addBox(pointSize, 0.0, _arrowCallback, (void *)false);
	    lm.addText(str.str());
	}
    }
    lm.end();

    float offset = 5.0 * metresPerPixel;
    if (start) {
	lm.moveTo(offset, offset);
    } else {
    	az += 180;
	lm.moveTo(-offset, offset);
    }

    geodDrawText(lm, lat, lon, az - 90.0, FIDDLE_ALL);
}

// Draws one great circle segment.
void Route::_draw(GreatCircle &gc, float distance, double metresPerPixel, 
		  const sgdFrustum &frustum, const sgdMat4 &m, 
		  atlasFntTexFont *fnt, bool magTrue)
{
    glPushAttrib(GL_DEPTH_BUFFER_BIT | GL_POINT_BIT); {
	glDisable(GL_DEPTH_TEST);

	if (active) {
	    glColor3f(1.0, 0.0, 0.0);
	} else {
	    glColor3f(1.0, 0.4, 0.4); // salmon
	}
	gc.draw(metresPerPixel, frustum, m);

	// EYE - for all this stuff, should we somehow check for visibility?
	// EYE - check for space for labels?  Share code with airways?

	// Draw a dot at the beginning and end of the segment.
	glPointSize(3.0);
	glBegin(GL_POINTS); {
	    geodVertex3f(gc.from().getLatitudeDeg(),
			 gc.from().getLongitudeDeg());
	    geodVertex3f(gc.to().getLatitudeDeg(),
			 gc.to().getLongitudeDeg());
	}
	glEnd();

	//--------------------
	// Draw the azimuth and arrow at the beginning of the segment.
	_draw(true, gc.from(), gc.toAzimuth(), metresPerPixel, fnt, magTrue);
	// EYE - control the other end with a switch?
	// _draw(false, gc.to(), gc.fromAzimuth(), metresPerPixel);

	//--------------------
	// Print the segment length in the middle.
	LayoutManager lm;
	const float pointSize = _pointSize * metresPerPixel;
	float offset = 5.0 * metresPerPixel;
	// EYE - have a separate constant for segment length point size?
	lm.setFont(fnt, pointSize * 0.8);
	lm.setBoxed(true, true, false);
	lm.setMargin(0.0);
	lm.moveTo(0.0, -offset, LayoutManager::UC);

	AtlasString str;
	str.printf("%.0f nm", gc.distance() * SG_METER_TO_NM);
	lm.setText(str.str());

	// EYE - have this function in GreatCircle?
	SGGeod midGeod;
	double fromAz;
	geo_direct_wgs_84(gc.from(), gc.toAzimuth(), gc.distance() / 2.0, 
			  midGeod, &fromAz);
	
	// EYE - magic "number"
	glColor3f(0.0, 0.0, 1.0);
	geodDrawText(lm, 
		     midGeod.getLatitudeDeg(), midGeod.getLongitudeDeg(),
		     fromAz + 90.0, FIDDLE_ALL);

	//--------------------
	// Print the total route length at the end.
	str.printf("%.0f nm", distance * SG_METER_TO_NM);
	lm.setPointSize(pointSize);
	lm.setText(str.str());
	lm.moveTo(-offset, offset, LayoutManager::LR);

	geodDrawText(lm, 
		     gc.to().getLatitudeDeg(), gc.to().getLongitudeDeg(),
		     gc.fromAzimuth() + 90.0, FIDDLE_ALL);
    }
    glPopAttrib();
}

// Global route.
// EYE - move into globals?  Allow many to be created?
Route route;

ScreenLocation::ScreenLocation(Scenery &scenery): 
    _scenery(scenery), _valid(false), _validElevation(false)
{
}

void ScreenLocation::set(float x, float y)
{
    invalidate();
    _x = x;
    _y = y;
}

AtlasCoord &ScreenLocation::coord()
{
    if (!_valid) {
    	SGVec3<double> cart;
	// EYE - Should scenery be an overlay?
    	_valid = _scenery.intersection(_x, _y, &cart, &_validElevation);
    	if (_valid) {
    	    _loc.set(cart);
    	} else {
    	    // We don't throw an error - the user needs to check _loc
    	    _loc.invalidate();
    	}
    }

    return _loc;
}

bool ScreenLocation::validElevation() 
{ 
    // Force validation if necessary.
    coord();
    return _validElevation;
}

void ScreenLocation::invalidate()
{
    _loc.invalidate();
    _valid = _validElevation = false;
}

// This constructor assumes that there exists a valid OpenGL context.
AtlasWindow::AtlasWindow(const char *name, 
			 const char *regularFontFile,
			 const char *boldFontFile,
			 AtlasController *ac): 
    AtlasBaseWindow(name, regularFontFile, boldFontFile), _ac(ac), 
    _dragging(false), _lightingPrefixKey(false), _debugPrefixKey(false), 
    _overlays(NULL), _exitOkDialog(NULL), _searchTimerScheduled(false)
{
    // Initialize OpenGL, starting with clearing (background) color
    // and enabling depth testing.
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glEnable(GL_DEPTH_TEST);

    // Turn on backface culling.  We use the OpenGL standard of
    // counterclockwise winding for front faces.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Standard settings for lines and points.  If you change any of
    // these, you *must* return them to their original value after!
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0);
    glPointSize(1.0);

    // Tie material ambient and diffuse values to the current colour.
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);

    // Set the light brightness.
    const float brightness = 0.8;
    GLfloat diffuse[] = {brightness, brightness, brightness, 1.0};
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);

    // We use light0.
    glEnable(GL_LIGHT0);

    // Initalize scenery object.  Note that we specify that scenery
    // should be displayed in this window.
    _scenery = new Scenery(_ac->tileManager(), this);

    // Background map image.

    // EYE - I think we need three possible backgrounds:
    //
    // (1) An arbitrary image (basically what we have now)
    //
    // (2) Blue ocean (ideal for those with complete scenery)
    //
    // (3) FlightGear scenery tile map
    //
    // So we could have a default: backgroundType: file, ocean, scenery
    // And for the file option, have another: backgroundFilename
    SGPath world = globals.prefs.path;
    world.append("background");
    _scenery->setBackgroundImage(world);

    // Create our screen location objects.  They track the lat/lon
    // (and elevation, if available) of what's beneath the cursor and
    // the centre of the screen, respectively.
    _cursor = new ScreenLocation(*_scenery);
    _centre = new ScreenLocation(*_scenery);

    // Create our overlays and initialize them.
    _overlays = new Overlays(this);
    setOverlayVisibility(Overlays::NAVAIDS, true);
    setOverlayVisibility(Overlays::VOR, true);
    setOverlayVisibility(Overlays::NDB, true);
    setOverlayVisibility(Overlays::ILS, true);
    setOverlayVisibility(Overlays::AIRPORTS, true);
    setOverlayVisibility(Overlays::LABELS, true);
    setOverlayVisibility(Overlays::TRACKS, true);
    setOverlayVisibility(Overlays::AIRWAYS, false);
    setOverlayVisibility(Overlays::LOW, true);
    setOverlayVisibility(Overlays::HIGH, false);

    // Create our user (sub)interfaces.
    _mainUI = new MainUI(20, 20, this);
    _infoUI = new InfoUI(260, 20, this);
    _lightingUI = new LightingUI(600, 20, this);
    _helpUI = new HelpUI(250, 500, this);

    // The search interface is used to search for airports and navaids.
    _searchUI = new SearchUI(this, 0, 0, 300, 300);
    _searchUI->hide();

    if (globals.prefs.softcursor) {
	puShowCursor();
    }

    // EYE - who should initialize this - the controller or the
    // window?
    setCentre(globals.prefs.width / 2.0, globals.prefs.height / 2.0);
    setCentreType(CROSSHAIRS);
    setAutocentreMode(globals.prefs.autocentre_mode);
    
    // Initialize our view and lighting variables.
    _setShading();
    _setAzimuthElevation();
    _setFlightTrack();
    // EYE - This looks inconsistent because we have the "model"
    // (_relativePalette)
    _setRelativePalette(false);
    // EYE - call other '_set' functions?
    _setMEFs();
    _setCentreType();

    // EYE - make sure we don't subscribe to ones we produce!
    subscribe(Notification::AircraftMoved);
    subscribe(Notification::AutocentreMode);
    subscribe(Notification::Azimuth);
    subscribe(Notification::CentreType);
    subscribe(Notification::ContourLines);
    subscribe(Notification::DiscreteContours);
    subscribe(Notification::Elevation);
    subscribe(Notification::FlightTrackModified);
    subscribe(Notification::LightingOn);
    subscribe(Notification::MEFs);
    subscribe(Notification::NewScenery);
    subscribe(Notification::OverlayToggled);
    subscribe(Notification::Palette);
    subscribe(Notification::PaletteList);
    subscribe(Notification::SmoothShading);

    // We want to know when the flight track changes so that we can
    // change our window title.
    subscribe(Notification::NewFlightTrack);

    // Check network connections and serial connections periodically (as
    // specified by the "update" user preference).
    startTimer((int)(globals.prefs.update * 1000.0), 
	       (void (GLUTWindow::*)())&AtlasWindow::_flightTrackTimer);

    // // EYE - hacked in for now.
    // glutTimerFunc(MPTimerInterval, MPAircraftTimer, 0);
}

AtlasWindow::~AtlasWindow()
{
    delete _overlays;

    delete _mainUI;
    delete _infoUI;
    delete _helpUI;
    delete _searchUI;
}

// #include "MPAircraft.hxx"
// map<string, MPAircraft *> MPAircraftMap;
void AtlasWindow::_display()
{
    assert(glutGetWindow() == id());
  
    // Check errors before...
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
    	printf("AtlasWindow::_display (before): %s\n", gluErrorString(error));
    }

    // Clear all pixels and depth buffer.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Scenery.
    _scenery->draw(_ac->lightingOn());

    // Overlays.
    _overlays->draw(_ac->navData());

    // Draw our route.
    sgdMat4 mvm;
    glGetDoublev(GL_MODELVIEW_MATRIX, (GLdouble *)mvm);
    route.draw(_metresPerPixel, _frustum, 
    	       mvm, eye(), regularFont(),
	       _ac->magTrue());

    // // Draw the MP aircraft.
    // map<string, MPAircraft*>::const_iterator i = MPAircraftMap.begin();
    // for (; i != MPAircraftMap.end(); i++) {
    // 	MPAircraft *t = i->second;
    // 	t->draw();
    // }

    // Render the widgets.
    puDisplay();

    glutSwapBuffers();

    // ... and check errors at the end.
    error = glGetError();
    if (error != GL_NO_ERROR) {
	printf("AtlasWindow::_display (after): %s\n", gluErrorString(error));
    }
}

void AtlasWindow::_reshape(int width, int height)
{
    assert(glutGetWindow() == id());
  
    glViewport(0, 0, (GLsizei)width, (GLsizei)height); 
    setCentre(width / 2.0, height / 2.0);
    zoomBy(1.0);

    // Ensure that the 'jump to location' widget stays in the upper
    // right corner ...
    // EYE - just subscribe and adjust it in the notification?
    int w, h;
    _searchUI->getSize(&w, &h);
    _searchUI->setPosition(width - w, height - h);

    // ... and that the lighting UI stays in the lower right corner
    // (with a 20-pixel space at the bottom and right).
    _lightingUI->getSize(&w, &h);
    _lightingUI->setPosition(width - w - 20, 20);
}

void AtlasWindow::_mouse(int button, int state, int x, int y) 
{
    assert(glutGetWindow() == id());
  
    // EYE - how can I drag puGroups?
    if (puMouse(button, state, x, y)) {
	postRedisplay();
    } else {
	// PUI didn't consume this event
	switch (button) {
	  case GLUT_LEFT_BUTTON:
	    if (state == GLUT_DOWN) {
		if (_scenery->intersection(x, y, &_oldC)) {
		    // EYE - do we need to set _dragging here, or
		    // should we wait until mouseMotion gets called?
		    _dragging = true;
		}
	    } else {
		_dragging = false;
	    }
	    break;
	  case 3:		// WM_MOUSEWHEEL (away)
	    if (state == GLUT_DOWN) {
		_keyboard('+', x, y);
	    }
	    break;
	  case 4:		// WM_MOUSEWHEEL (towards)
	    if (state == GLUT_DOWN) {
		_keyboard('-', x, y);
	    }
	    break;
	  default:
	    break;
	}
    }
}

void AtlasWindow::_motion(int x, int y)
{
    assert(glutGetWindow() == id());
  
#if defined(__APPLE__)
    // EYE - the cursor crosshair's hotspot seems to be off by 1 in
    // both x and y (at least on OS X).
    x--;
    y--;
#endif
    // The x, y given by GLUT marks the upper-left corner of the
    // cursor (and y increases down in GLUT coordinates).  We add 0.5
    // to both to get the centre of the cursor.
    // // _cursor.set(x + 0.5, y + 0.5);
    setCursor(x + 0.5, y + 0.5);

    if (_dragging) {
	SGVec3<double> newC;
	if (_scenery->intersection(x, y, &newC)) {
	    // The two vectors, _oldC and newC, define the plane and
	    // angle of rotation.  A line perpendicular to this plane,
	    // passing through the origin, is our axis of rotation.
	    // However, if the two vectors are the same, there is no
	    // motion (nor do they define a plane), so we just return
	    // immediately.
	    if (_oldC == newC) {
		return;
	    }

	    sgdVec3 axis;
	    sgdMat4 rot;

	    sgdVectorProductVec3(axis, newC.data(), _oldC.data());
	    double theta = SGD_RADIANS_TO_DEGREES *
		atan2(sgdLengthVec3(axis), 
		      sgdScalarProductVec3(_oldC.data(), newC.data()));
	    sgdMakeRotMat4(rot, theta, axis);
	    
	    // Transform the eye point and the camera up vector.
	    rotatePosition(rot);
	}
    } else if (puMouse(x, y)) {
	postRedisplay();
    }
}

void AtlasWindow::_passiveMotion(int x, int y) 
{
    assert(glutGetWindow() == id());
  
#if defined(__APPLE__)
    // EYE - the cursor crosshair's hotspot seems to be off by 1 in
    // both x and y (at least on OS X).
    x--;
    y--;
#endif
    // The x, y given by GLUT marks the upper-left corner of the
    // cursor (and y increases down in GLUT coordinates).  We add 0.5
    // to both to get the centre of the cursor.
    setCursor(x + 0.5, y + 0.5);
}

void AtlasWindow::_keyboard(unsigned char key, int x, int y) 
{
    assert(glutGetWindow() == id());
  
    if (_lightingPrefixKey) {
	_lightingPrefixKeypressed(key, x, y);
	_lightingPrefixKey = false;

	return;
    } else if (_debugPrefixKey) {
	_debugPrefixKeypressed(key, x, y);
	_debugPrefixKey = false;

	return;
    }

    // EYE - this is a temporary patch, because of a bug in the
    // Windows implementation of FreeGLUT (as of March 2011).  This
    // should be checked periodically to see if the bug still exists
    // and removed if it has been fixed.
#ifdef _MSC_VER
    if (( key == ' ' ) &&
        ( glutGetModifiers() & GLUT_ACTIVE_CONTROL ))
        key = 0;
#endif // _MSC_VER

    if (!puKeyboard(key, PU_DOWN)) {
	switch (key) {
	  case 24:	       // ctrl-x
	    // Ctrl-x is a prefix key (a la emacs).
	    _lightingPrefixKey = true;
	    break;

	  case 8:		// ctrl-h
	    // Ditto, for debugging stuff.
	    _debugPrefixKey = true;
	    break;

	  case 0:		// ctrl-space
	    if (!route.active) {
		// EYE - later we should push a new route onto the route stack.
		route.clear();
		route.active = true;
	    }
	    {
		SGGeod geod;
		SGGeodesy::SGCartToGeod(SGVec3<double>(eye()[0], 
						       eye()[1], 
						       eye()[2]), 
					geod);
		if (geod == route.lastPoint()) {
		    // EYE - check if there have been intervening
		    // moves, keypresses, ...?
		    route.active = false;
		} else {
		    route.addPoint(geod);
		}
	    }
	    postRedisplay();
	    break;

	  case 14:		// ctrl-n
	    // Next flight track.  The setCurrentTrack() method is
	    // smart enough to ignore indexes beyond the end of the
	    // flight track array.
	      {
		  size_t i = _ac->currentTrackNo();
		  if (i != FlightTracks::NaFT) {
		      _ac->setCurrentTrack(i + 1);
		  }
	      }
	    break;

	  case 16:		// ctrl-p
	    // Previous flight track.
	      {
		  size_t i = _ac->currentTrackNo();
		  if ((i != FlightTracks::NaFT) && (i > 0)) {
		      _ac->setCurrentTrack(i - 1);
		  }
	    }
	    break;

	  case ' ':
	    // Toggle main interface.
	    if (!_mainUI->isVisible()) {
		_mainUI->reveal();
	    } else {
		_mainUI->hide();
	    }
	    postRedisplay();
	    break;

	  case '+':
	    zoomBy(1.0 / zoomFactor);
	    break;

	  case '-':
	    zoomBy(zoomFactor);
	    break;

	  case '?':
	    // Help dialog.
	    if (_helpUI->isVisible()) {
		_helpUI->hide();
	    } else {
		_helpUI->reveal();
	    }
	    postRedisplay();
	    break;

	  case 'a':
	    // Toggle airways.  We cycle through no airways, low
	    // airways, then high airways.
	    if (!isOverlayVisible(Overlays::AIRWAYS)) {
		// No airways -> low airways
		setOverlayVisibility(Overlays::AIRWAYS, true);
		setOverlayVisibility(Overlays::LOW, true);
		setOverlayVisibility(Overlays::HIGH, false);
	    } else {
		if (isOverlayVisible(Overlays::LOW) &&
		    !isOverlayVisible(Overlays::HIGH)) {
		    // Low airways -> high airways
		    setOverlayVisibility(Overlays::LOW, false);
		    setOverlayVisibility(Overlays::HIGH, true);
		} else {
		    // High airways -> no airways
		    setOverlayVisibility(Overlays::AIRWAYS, false);
		    setOverlayVisibility(Overlays::LOW, false);
		    setOverlayVisibility(Overlays::HIGH, false);
		}
	    }
	    break;

	  case 'A':
	    // Toggle airports.
	    toggleOverlay(Overlays::AIRPORTS);
	    break;

	  case 'c': 
	    if (_cursor->coord().valid()) {
	    	movePosition(_cursor->data());
	    }
	    break;

	  case 'd':
	    // Hide/show the info interface and the graphs window.
	    _ac->setShowTrackInfo(!_ac->showTrackInfo());
	    break;

	  case 'f':
	    // Toggle flight tracks.
	    toggleOverlay(Overlays::TRACKS);
	    break;

 	  case 'i':
	    // Zoom airplane image.
 	    globals.prefs.airplaneImageSize *= 1.1;
	    postRedisplay();
 	    break;

 	  case 'I':
	    // Shrink airplane image.
 	    globals.prefs.airplaneImageSize /= 1.1;
	    postRedisplay();
 	    break;

	  case 'j':
	    // Toggle the search interface.
	    if (_searchUI->isVisible()) {
		_searchUI->hide();
	    } else {
		_searchUI->reveal();
		// Record where we were when the search started.  If
		// the search is cancelled, we'll return to this
		// point.
		sgdCopyVec3(_searchFrom, eye());
		// If we had a previous search and have moved in the
		// meantime, we'd like to see the results resorted
		// according to their distance from the new eyepoint.
		// We call searchStringChanged() explicitly to do
		// this.
		if (strlen(_searchUI->searchString()) > 0) {
		    searchStringChanged("");
		}
	    }
	    postRedisplay();
	    break;

	  case 'l':
	    // Turn lighting UI on/off.
	    if (!_lightingUI->isVisible()) {
	  	_lightingUI->reveal();
	    } else {
	  	_lightingUI->hide();
	    }
	    glutPostRedisplay();
	    break;

	  case 'm':
	    // Toggle between mouse, crosshairs, and range rings
	    // modes.  The status of the cursor and the
	    // crosshairs/range rings overlays depends on the mouse
	    // mode - if we're in mouse mode, use a crosshairs cursor
	    // and turn off both overlays.  If we're in crosshairs
	    // mode, use a regular cursor and select the crosshairs
	    // overlay.  Finally, in range rings mode, use a regular
	    // cursor and turn on the range rings overlay.
	    if (centreType() == MOUSE) {
	    	setCentreType(CROSSHAIRS);
	    	glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
	    } else if (centreType() == CROSSHAIRS) {
	    	setCentreType(RANGE_RINGS);
	    	glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
	    } else {
	    	setCentreType(MOUSE);
	    	glutSetCursor(GLUT_CURSOR_CROSSHAIR);
	    }
	    postRedisplay();
	    break;

	  case 'M':
	    // Toggle MEF display on/off.
	    _ac->setMEFs(!_ac->MEFs());
	    break;

	  case 'n':
	    // Rotate camera so that north is up.
	    rotateEye();
	    break;

	  case 'N':
	    // Toggle navaids.
	    toggleOverlay(Overlays::NAVAIDS);
	    break;    

	  case 'o':
	    // Open a flight file (unless the file dialog is already
	    // active doing something else).
	    _mainUI->load();
	    break;

	  case 'p':
	    centreMapOnAircraft();
	    break;

	  case 'P':
	    // Toggle auto-centering.
	    setAutocentreMode(!autocentreMode());
	    break;

	  case 'q':
	    // Quit
	      {
		  // If there are unsaved tracks, warn the user first.
		  bool modifiedTracks = false;
		  for (unsigned int i = 0; i < _ac->tracks().size(); i++) {
		      if (_ac->trackAt(i)->modified()) {
			  modifiedTracks = true;
			  break;
		      }
		  }
		  if (modifiedTracks) {
		      // Create a warning dialog.
		      _exitOkDialog = new AtlasDialog("You have unsaved tracks.\nIf you exit now, they will be lost.\nDo you want to exit?", 
						      "OK", "Cancel", "",
						      __atlasWindow_exitOk_cb, 
						      this);
		      postRedisplay();
		  } else {
		      exit(0);
		  }
	      }
	      break;

	  case 'r':
	    route.active = !route.active;
	    postRedisplay();
	    break;

	  case 's':
	    // Save the current track.
	    if (!_ac->currentTrack()) {
		break;
	    }
	    if (_ac->currentTrack()->hasFile()) {
		// If it has a file, save without questions.
		_ac->saveTrack();
	    } else {
		// If it has no file, call up the save as dialog.
		_mainUI->saveAs();
	    }
	    break;

	  case 'u':
	    // 'u'nattach (ie, detach)
	    _ac->detachTrack();
	    break;

	  case 'v':
	    // Toggle labels.
	    toggleOverlay(Overlays::LABELS);
	    break;

	  case 'w':
	    // Close (unload) a flight track.
	    _mainUI->unload();
	    break;

	  case 'x':
	    // Toggle x-axis type (time, distance)
	    globals.gw->toggleXAxisType();
	    break;

	  case 127:	// delete
	    // EYE - delete same on non-OS X systems?
	    if (route.active) {
		route.deleteLastPoint();
	    } else {
		route.clear();
	    }
	    postRedisplay();
	    break;
	}
    } else {
	// EYE - really?
	postRedisplay();
    }
}

// Called when 'special' keys are pressed.
void AtlasWindow::_special(int key, int x, int y) 
{
    assert(glutGetWindow() == id());
  
    // We give our widgets a shot at the key first, via puKeyboard.
    // If it returns FALSE (ie, none of the widgets eat the key), and
    // if there's a track being displayed, then pass it on to the
    // special key handler of the graph window and give it a shot.
    if (!puKeyboard(key + PU_KEY_GLUT_SPECIAL_OFFSET, PU_DOWN) && 
	_ac->currentTrack()) {
	globals.gw->special(key, x, y);
    }
}

// I don't know if this constitutes a hack or not, but doing something
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
void AtlasWindow::_visibility(int state)
{
    assert(glutGetWindow() == id());
  
    if (state == GLUT_VISIBLE) {
	postRedisplay();
    }
}

void AtlasWindow::_setShading()
{
    int currentWin = set();
    glShadeModel(_ac->smoothShading() ? GL_SMOOTH : GL_FLAT);
    set(currentWin);
}

// Set the light position (in eye coordinates, not world coordinates).
void AtlasWindow::_setAzimuthElevation()
{
    // Convert the azimuth and elevation (both in degrees) to a
    // 4-vector.  An azimuth of 0 degrees corresponds to north, 90
    // degrees to east.  An elevation of 0 degrees is horizontal, 90
    // degrees is directly overhead.  In the 4-vector, position X is
    // right, positive Y is up, and positive Z is towards the viewer.
    // W is always set to 0.0.
    sgVec4 lightPosition;
    float a = (90.0 - _ac->azimuth()) * SG_DEGREES_TO_RADIANS;
    float e = _ac->elevation() * SG_DEGREES_TO_RADIANS;
    lightPosition[0] = cos(a) * cos(e);
    lightPosition[1] = sin(a) * cos(e);
    lightPosition[2] = sin(e);
    lightPosition[3] = 0.0;

    // Now set OpenGL's LIGHT0.
    int currentWin = set();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix(); {
	glLoadIdentity();
	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    }
    glPopMatrix();
    set(currentWin);
}

// Make the palette base relative.  A palette base of 0.0 (AKA
// absolute colouring) is the default, and it just means that
// elevations are coloured according to their distance above sea
// level.  A non-zero palette base (AKA relative colouring) just
// changes what "sea level" means.
//
// If there's a displayed flight track, then sea level is the
// aircraft's current elevation.  If the mouse mode is 'mouse', sea
// level is the elevation of the scenery under the mouse.  Otherwise,
// it's the elevation of the scenery at the centre.
void AtlasWindow::_setPaletteBase()
{
    double elev = _ac->currentPalette()->base();
    ScreenLocation *loc = currentLocation();
    if (_ac->currentTrack() && 
	isOverlayVisible(Overlays::TRACKS) && 
	autocentreMode()) {
	elev = _ac->currentPoint()->alt * SG_FEET_TO_METER;
    } else if (loc->validElevation()) {
	elev = loc->elev();
    }
    _ac->setPaletteBase(elev);
}

void AtlasWindow::_setRelativePalette(bool relative)
{
    if (relative != _relativePalette) {
	_relativePalette = relative;
	if (relative) {
	    _setPaletteBase();
	}
    }
}

void AtlasWindow::_setMEFs()
{
    _scenery->setMEFs(_ac->MEFs());
}

void AtlasWindow::_setCentreType()
{
    // EYE - combine with setCentreType()?
    if (centreType() == MOUSE) {
	setOverlayVisibility(Overlays::CROSSHAIRS, false);
	setOverlayVisibility(Overlays::RANGE_RINGS, false);
    } else if (centreType() == CROSSHAIRS) {
	setOverlayVisibility(Overlays::CROSSHAIRS, true);
	setOverlayVisibility(Overlays::RANGE_RINGS, false);
    } else {
	setOverlayVisibility(Overlays::CROSSHAIRS, false);
	setOverlayVisibility(Overlays::RANGE_RINGS, true);
    }
}

void AtlasWindow::_setFlightTrack()
{
    _setTitle();
    // EYE - note that if we have a new network connection, this won't
    // do anything, as the flight track will be empty.  It's only
    // after getting the first point that we can centre the aircraft.
    centreMapOnAircraft();
}

void AtlasWindow::_setTitle()
{
    FlightTrack *track = _ac->currentTrack();
    if (!track) {
	setTitle("Atlas");
    } else {
	globals.str.printf("Atlas - %s", track->niceName());
	setTitle(globals.str.str());
    }
}

// Called periodically to check for input on network and serial ports.
void AtlasWindow::_flightTrackTimer()
{
    // EYE - is this putting too much functionality in the controller?
    _ac->checkForInput();

    // Check again later.
    startTimer((int)(globals.prefs.update * 1000.0), 
	       (void (GLUTWindow::*)())&AtlasWindow::_flightTrackTimer);
}

// Called to initiate a new search or continue an active search.  If
// the search is big, it will only do a portion of it, then reschedule
// itself to continue the search.
void AtlasWindow::_searchTimer()
{
    // If the search interface is hidden, we take that as a signal
    // that the search has ended.
    if (!_searchUI->isVisible()) {
	_searchTimerScheduled = false;
	return;
    }

    // Check if we have any more hits.
    char *str;
    static int maxMatches = 100;

    str = _searchUI->searchString();
    if (_ac->searcher()->findMatches(str, eye(), maxMatches)) {
	_searchUI->reloadData();
	postRedisplay();

	// Continue the search in 100ms.
	assert(_searchTimerScheduled == true);
	startTimer(100, (void (GLUTWindow::*)())&AtlasWindow::_searchTimer);
    } else {
	// No new matches, so our search is finished.
	_searchTimerScheduled = false;
    }
}

// #include <sstream>
// // EYE - need a destructor
// // const int MPTimerInterval = 5000;	// 5000ms = 5s
// const int MPTimerInterval = 2500;	// 2500ms = 2.5s
// void MPAircraftTimer(int value)
// {
//     // EYE - hard-wired for now
//     // EYE - need to check for live internet connection
//     // EYE - it's unfortunate we need to build up and break down TCP
//     //       connections on each call to this function.
//     SGSocket *io = new SGSocket("mpserver01.flightgear.org", "5001", "tcp");
//     // EYE - we need to be OUT, even though we're just reading
//     //       information.  Apparently we qualify as a "client".
//     // EYE - if the network connection goes down, we block on this
//     //       call for a loooong time (about a minute).  We really need
//     //       some kind of non-blocking io.  After the first block,
//     //       though, things respond instantly.  Why?
//     if (!io->open(SG_IO_OUT)) {
// 	fprintf(stderr, "Couldn't open socket!\n");
// 	// Try again later.
// 	// EYE - need a way to be signalled of a live network rather
// 	//       than polling.
// 	glutTimerFunc(MPTimerInterval, MPAircraftTimer, value + 1);
// 	io->close();
// 	delete io;
// 	return;
//     }

//     const int bufferSize = 1024;
//     int noOfBytes;
//     char bytes[bufferSize];
//     string str;
//     noOfBytes = io->read(bytes, bufferSize);
//     // EYE - -2 = timeout
//     while ((noOfBytes == -2) || (noOfBytes > 0)) {
// 	if (noOfBytes > 0) {
// 	    str.append(bytes, noOfBytes);
// 	}
// 	noOfBytes = io->read(bytes, bufferSize);
//     }
//     io->close();
//     delete io;

//     istringstream stream(str);
//     string line;
//     getline(stream, line);
//     while (!stream.eof()) {
// 	// Only process it if it's not a comment line.
// 	if (line[0] != '#') {
// 	    istringstream stream(line);
// 	    string id;
// 	    float x, y, z, lat, lon, alt, x_orient, y_orient, z_orient;
// 	    string model;
// 	    stream >> id
// 		   >> x >> y >> z
// 		   >> lat >> lon >> alt
// 		   >> x_orient >> y_orient >> z_orient
// 		   >> model;
// 	    // Example id: "ugadec4@85.214.37.14"
// 	    id = id.substr(0, id.find("@"));
// 	    // Example model: "Aircraft/747-200/Models/boeing747-200.xml"
// 	    model = model.substr(model.find("/") + 1);
// 	    model = model.substr(0, model.find("/"));

// 	    // EYE - need a way to clear out deadwood
// 	    map<string, MPAircraft *>::const_iterator i = 
// 		MPAircraftMap.find(id);
// 	    MPAircraft *a;
// 	    if (i == MPAircraftMap.end()) {
// 		printf("'%s': new aircraft\n", id.c_str());
// 		a = new MPAircraft(id, model);
// 		MPAircraftMap[id] = a;
// 	    } else {
// 		a = i->second;
// 	    }
// 	    sgdVec3 cart;
// 	    sgdSetVec3(cart, x, y, z);
// 	    a->addPoint(cart);
// 	}

// 	getline(stream, line);
//     }
//     glutPostRedisplay();

//     glutTimerFunc(MPTimerInterval, MPAircraftTimer, value + 1);
// }

void AtlasWindow::setOverlayVisibility(Overlays::OverlayType overlay, 
					   bool on)
{
    if (_overlays->isVisible(overlay) != on) {
	_overlays->setVisibility(overlay, on);
	// EYE - should we post a notification or just manipulate
	// things directly?
	Notification::notify(Notification::OverlayToggled);
    }
}

// EYE - make protected?
void AtlasWindow::toggleOverlay(Overlays::OverlayType overlay)
{
    setOverlayVisibility(overlay, !_overlays->isVisible(overlay));
}

void AtlasWindow::keyboard(unsigned char key, int x, int y)
{
    int win = set();
    _keyboard(key, x, y);
    set(win);
}

void AtlasWindow::searchFinished(int i)
{
    if (i != -1) {
	// User hit return.  Jump to the selected point.
	const Searchable *match = _ac->searcher()->getMatch(i);
	movePosition(match->location());
    } else {
	// User hit escape, so return to our original point.
	// EYE - restore original orientation too
	movePosition(_searchFrom);
    }

    postRedisplay();
}

void AtlasWindow::searchItemSelected(int i)
{
    if (i != -1) {
	const Searchable *match = _ac->searcher()->getMatch(i);
	movePosition(match->location());
    }

    postRedisplay();
}

void AtlasWindow::searchStringChanged(const char *str)
{
    if (!_searchTimerScheduled) {
	_searchTimerScheduled = true;
	startTimer(0, (void (GLUTWindow::*)())&AtlasWindow::_searchTimer);
    }
}

int AtlasWindow::noOfMatches()
{
    return _ac->searcher()->noOfMatches();
}

char *AtlasWindow::matchAtIndex(int i)
{
    Searchable *searchable = _ac->searcher()->getMatch(i);

    // The search interface owns the strings we give it, so make a
    // copy.
    return strdup(searchable->asString().c_str());
}

ScreenLocation *AtlasWindow::currentLocation()
{
    if (centreType() == MOUSE) {
	return _cursor;
    } else {
	return _centre;
    }
}

void AtlasWindow::setCursor(float x, float y)
{
    _cursor->set(x, y);
    if (centreType() == MOUSE) {
	if (_relativePalette) {
	    _setPaletteBase();
	}
	// EYE - call movePosition()?  Update UI's directly?
	Notification::notify(Notification::MouseMoved);
    }
}

void AtlasWindow::setCentre(float x, float y)
{
    _centre->set(x, y);
    if (centreType() != MOUSE) {
	// EYE - does this really constitute a move?  Call
	// movePosition()?  Update UI's directly?
	Notification::notify(Notification::Moved);
    }
}

void AtlasWindow::setCentreType(CentreType t)
{
    if (t != _centreType) {
	_centreType = t;
	Notification::notify(Notification::CentreType);
    }
}

// Sets the up vector to point to the given heading (default is north)
// from the current eye vector.
void AtlasWindow::rotateEye(double heading)
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
void AtlasWindow::movePosition(const sgdVec3 dest)
{
    sgdCopyVec3(_eye, dest);
    _rotate(0.0);

    _move();			// This will inform the culler.
}

// Moves eye point to the given lat, lon, setting up vector to north.
void AtlasWindow::movePosition(double lat, double lon)
{
    // Convert from lat, lon to x, y, z.
    sgdVec3 cart;
    atlasGeodToCart(lat, lon, 0.0, cart);

    movePosition(cart);
}

// EYE - rename this?  The "rotate" is a bit misleading

// Rotates the eye point and up vector using the given rotation
// matrix.
void AtlasWindow::rotatePosition(sgdMat4 rot)
{
    // Rotate the eye and eye up vectors.
    sgdXformVec3(_eye, rot);
    sgdXformVec3(_eyeUp, rot);

    // The eye point is always assumed to lie on the surface of the
    // earth (at sea level).  Rotating it might leave it above or
    // below (because he earth is not perfectly spherical), so we need
    // to normalize it.
    _eye[2] *= SGGeodesy::STRETCH;
    sgdScaleVec3(_eye, SGGeodesy::EQURAD / sgdLengthVec3(_eye));
    _eye[2] /= SGGeodesy::STRETCH;

    _move();			// This will inform the culler.
}

// Zoom to the given scale.  Note that we need to explicitly set
// ourselves to be the current window, as this can get called at any
// time.  The same goes for any method that does OpenGL calls, except
// GLUTWindow callbacks - these are guaranteed to only be called when
// we are the current window.
void AtlasWindow::zoomTo(double scale)
{
    _metresPerPixel = scale;

    // Calculate clip planes.  Why 'nnear' and 'ffar', not 'near' and
    // 'far'?  Windows.
    double left, right, bottom, top, nnear, ffar;
    // The x, y coordinates of the centre ScreenLocation variable
    // are equivalent to half of the window width and height
    // respectively.
    right = _centre->x() * _metresPerPixel;
    left = -right;
    top = _centre->y() * _metresPerPixel;
    bottom = -top;

    double l = sgdLengthVec3(eye());
    //     nnear = l - SGGeodesy::EQURAD - 10000;
    nnear = -100000.0;		// EYE - magic number
    ffar = l;			// EYE - need better magic number (use bounds?)

    // Set our frustum.  This is used by various subsystems to find
    // out what's going on.
    _frustum.setOrtho(left, right, bottom, top, nnear, ffar);
    
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
//     f.setOrtho(planes[0], planes[1],
// 		 planes[2], planes[3],
// 		 -planes[4], -planes[5]);

//     // The zoom depends on the size of the viewing window.  We
//     // assume the scale is the same vertically as horizontally, so
//     // we just look at width (viewport[2]).
//     GLdouble viewport[4];
//     glGetDoublev(GL_VIEWPORT, viewport);
//     double metresPerPixel = (planes[1] - planes[0]) / viewport[2];

    int currentWin = set();
    glPushAttrib(GL_TRANSFORM_BIT); { // Save current matrix mode.
    	glMatrixMode(GL_PROJECTION);
    	glLoadIdentity();
    	glOrtho(_frustum.getLeft(), 
    		_frustum.getRight(), 
    		_frustum.getBot(), 
    		_frustum.getTop(), 
    		_frustum.getNear(), 
    		_frustum.getFar());
    }
    glPopAttrib();
    set(currentWin);

    // Tell our scenery and navdata objects about the zoom.
    _scenery->zoom(_frustum, _metresPerPixel);
    _ac->navData()->zoom(_frustum);

    // When we zoom, whatever's under the cursor changes.  The centre
    // should remain unaffected.
    _cursor->invalidate();

    // EYE - check this and other calls to _setPaletteBase in "core"
    // routines.  Should they be here?  Are they correct?  In this
    // case, it seems to be one step behind.  Is the scenery out of
    // sync?
    if (_relativePalette && (centreType() == MOUSE)) {
    	_setPaletteBase();
    }

    // Tell all interested parties that we've zoomed.
    Notification::notify(Notification::Zoomed);
}

void AtlasWindow::zoomBy(double factor)
{
    zoomTo(_metresPerPixel * factor);
}

void AtlasWindow::centreMapOnAircraft()
{
    FlightData *pos = _ac->currentPoint();
    if (pos != (FlightData *)NULL) {
	movePosition(pos->lat, pos->lon);
    }
}

void AtlasWindow::setAutocentreMode(bool mode)
{
    if (mode != _autocentreMode) {
	_autocentreMode = mode;
	Notification::notify(Notification::AutocentreMode);
    }
}

void AtlasWindow::notification(Notification::type n)
{
    if (n == Notification::SmoothShading) {
	_setShading();
    } else if ((n == Notification::Azimuth) ||
	       (n == Notification::Elevation)) {
	_setAzimuthElevation();
    } else if (n == Notification::NewScenery) {
    	// EYE - should we just automatically invalidate both instead
    	// of trying to be clever?
    	if (centreType() == MOUSE) {
    	    _cursor->invalidate();
    	} else {
    	    _centre->invalidate();
    	}
    } else if ((n == Notification::AircraftMoved) ||
	       (n == Notification::AutocentreMode)) {
	if (autocentreMode()) {
	    centreMapOnAircraft();
	}
    } else if (n == Notification::FlightTrackModified) {
	// This notification could mean several things.  Most we don't
	// care about, but we do care if the title of the flight track
	// has changed.
	_setTitle();
    } else if ((n == Notification::OverlayToggled) ||
	       (n == Notification::DiscreteContours) ||
	       (n == Notification::ContourLines) ||
	       (n == Notification::LightingOn) ||
	       (n == Notification::Palette) ||
	       (n == Notification::PaletteList)) {
	// EYE - should I do anything else?
    } else if (n == Notification::MEFs) {
	_setMEFs();
    } else if (n == Notification::CentreType) {
	_setCentreType();
    } else if (n == Notification::NewFlightTrack) {
	_setFlightTrack();
    } else {
	assert(0);
    }

    postRedisplay();
}

void AtlasWindow::_lightingPrefixKeypressed(unsigned char key, int x, int y)
{
    switch (key) {
      case 'c':			// Contour lines on/off
	_ac->setContourLines(!_ac->contourLines());
	break;
      case 'd':			// Discrete/smooth contours
	_ac->setDiscreteContours(!_ac->discreteContours());
	break;
      case 'e':			// Polygon edges on/off
	// EYE - do this through the controller?
	Bucket::polygonEdges = !Bucket::polygonEdges;
	postRedisplay();
	break;
      case 'l':			// Lighting on/off
	_ac->setLightingOn(!_ac->lightingOn());
	break;
      case 'p':			// Smooth/flat polygon shading
	_ac->setSmoothShading(!_ac->smoothShading());
	break;
      case 'r':			// Set base to current elevation
	_setRelativePalette(false);
	_setPaletteBase();
	break;
      case 'R':			// Base continously tracks elevation
	_setRelativePalette(!_relativePalette);
	break;
      case '0':			// Set base to 0.0 (the default)
	_setRelativePalette(false);
	_ac->setPaletteBase(0.0);
	break;
      default:
	return;
    }
}

void AtlasWindow::_debugPrefixKeypressed(unsigned char key, int x, int y)
{
    switch (key) {
      case 'm':
	// Dump information about the tile at the centre of the
	// window.
	{
	    double lat = _centre->lat(), lon = _centre->lon(), 
	    	elev = _centre->elev();
	    // EYE - can we trust this SGBucket constructor?
	    SGBucket b(lon, lat);
	    const SGVec3<double> &cart = _centre->cart();
	    printf("<%f, %f> %fm\n\t<%f, %f, %f>\n\t%s/%ld.btg\n", 
		   lat, lon, elev, 
		   cart.x(), cart.y(), cart.z(),
		   b.gen_base_path().c_str(), b.gen_index());
        }
	break;
      case 'r':
	// We maintain our current position in eye, which is the point
	// on the earth's surface (not adjusted for local terrain) at
	// the centre of the screen.  
	//
	// Ideally, if we use the intersection() function to tell us
	// what's at the centre, it should return the same result.
	// However, it usually doesn't.  I think this is because our
	// viewing vector is defined by the eye point and the earth's
	// centre.  This means that it is not quite perpendicular to
	// the earth (which is not quite a sphere).
	//
	// However, these are just guesses.  It could be a problem in
	// the SimGear geography library.  It could be a
	// rounding/precision problem.  It would be nice to find out
	// for sure what is going on.
	{
	    // First compare eye with the results of an an
	    // intersection() call at the centre of the screen.
	    SGGeod eyeGeod;
	    SGVec3<double> eyeCart(eye());
	    SGGeodesy::SGCartToGeod(eyeCart, eyeGeod);

	    SGGeod centreGeod;
	    SGVec3<double> centreCart;
	    bool foo;
 	    _scenery->intersection(_centre->x(), _centre->y(), &centreCart,
				   &foo);
 	    SGGeodesy::SGCartToGeod(centreCart, centreGeod);
 
 	    printf("%.8f, %.8f, %f (%f metres)\n",
 	    	   eyeGeod.getLatitudeDeg() - centreGeod.getLatitudeDeg(),
 	    	   eyeGeod.getLongitudeDeg() - centreGeod.getLongitudeDeg(),
 	    	   eyeGeod.getElevationM() - centreGeod.getElevationM(),
 	    	   dist(eyeCart, centreCart));

	    // Use SGGeod to find a point 1000m below the eye point,
	    // then do a gluLookAt that point from the eye point.
	    // This should ensure that we are perpendicular to the
	    // surface.  Repeat the intersection call and see if the
	    // results differ.
	    SGGeod lookAtGeod = SGGeod::fromGeodM(eyeGeod, -1000.0);
	    SGVec3<double> lookAtCart;
	    SGGeodesy::SGGeodToCart(lookAtGeod, lookAtCart);

	    // Now adjust the view axis.
	    glLoadIdentity();
	    gluLookAt(eye()[0], eye()[1], eye()[2],
		      lookAtCart[0], lookAtCart[1], lookAtCart[2],
		      eyeUp()[0], eyeUp()[1], eyeUp()[2]);

	    _scenery->intersection(_centre->x(), _centre->y(), &centreCart, 
				   &foo);
	    SGGeodesy::SGCartToGeod(centreCart, centreGeod);

	    printf("\t%.8f, %.8f, %f (%f metres)\n",
		   eyeGeod.getLatitudeDeg() - centreGeod.getLatitudeDeg(),
		   eyeGeod.getLongitudeDeg() - centreGeod.getLongitudeDeg(),
		   eyeGeod.getElevationM() - centreGeod.getElevationM(),
		   dist(eyeCart, centreCart));

	    // Reset our viewpoint.
	    movePosition(eye());
	  }
      break;
    }
}

void AtlasWindow::_exitOk_cb(bool okay)
{
    puDeleteObject(_exitOkDialog);
    _exitOkDialog = NULL;
    if (okay) {
	exit(0);
    }
}

///////////////////////////////////////////////////////////////////////////////
// PUI code (callbacks)
///////////////////////////////////////////////////////////////////////////////

// Called if the user hits a button on the exit dialog box.
void __atlasWindow_exitOk_cb(puObject *o)
{
    AtlasWindow *aw = (AtlasWindow *)o->getUserData();
    AtlasDialog::CallbackButton pos = 
	(AtlasDialog::CallbackButton)o->getDefaultValue();
    bool okay = (pos == AtlasDialog::LEFT);
    aw->_exitOk_cb(okay);
}

void __mainUI_zoom_cb(puObject *o)
{ 
    MainUI *mainUI = (MainUI *)o->getUserData();
    mainUI->_zoom_cb(o);
}

void __mainUI_overlay_cb(puObject *o)
{
    MainUI *mainUI = (MainUI *)o->getUserData();
    mainUI->_overlay_cb(o);
}

void __mainUI_MEF_cb(puObject *o)
{
    AtlasController *ac = (AtlasController *)o->getUserData();
    ac->setMEFs(o->getValue() != 0);
}

void __mainUI_position_cb(puObject *o) 
{
    MainUI *mainUI = (MainUI *)o->getUserData();
    mainUI->_position_cb(o);
}

void __mainUI_clearFlightTrack_cb(puObject *o) 
{
    AtlasController *ac = (AtlasController *)o->getUserData();
    ac->clearTrack();
}

void __mainUI_degMinSec_cb(puObject *o)
{
    AtlasController *ac = (AtlasController *)o->getUserData();
    ac->setDegMinSec(o->getValue() == 0);
}

void __mainUI_magTrue_cb(puObject *o)
{
    AtlasController *ac = (AtlasController *)o->getUserData();
    ac->setMagTrue(o->getValue() == 0);
}

// Called when the user presses OK or Cancel on the save file dialog.
void __mainUI_saveAsFile_cb(puObject *o)
{
    MainUI *mainUI = (MainUI *)o->getUserData();
    mainUI->_saveAsFile_cb(o);
}

// Called to save the current track.
void __mainUI_save_cb(puObject *o)
{
    AtlasController *ac = (AtlasController *)o->getUserData();
    ac->saveTrack();
}

// Called when the user wants to 'save as' a file.
void __mainUI_saveAs_cb(puObject *o)
{
    MainUI *mainUI = (MainUI *)o->getUserData();
    mainUI->_saveAs_cb(o);
}

// Called when the user presses OK or Cancel on the load file dialog.
void __mainUI_loadFile_cb(puObject *o)
{
    MainUI *mainUI = (MainUI *)o->getUserData();
    mainUI->_loadFile_cb(o);
}

void __mainUI_load_cb(puObject *o)
{
    MainUI *mainUI = (MainUI *)o->getUserData();
    mainUI->_load_cb(o);
}

// Unloads the current flight track.
void __mainUI_unload_cb(puObject *o)
{
    MainUI *mainUI = (MainUI *)o->getUserData();
    mainUI->_unload_cb(o);
}

void __mainUI_detach_cb(puObject *o)
{
    AtlasController *ac = (AtlasController *)o->getUserData();
    ac->detachTrack();
}

// This is called either by the tracksComboBox or one of the arrows
// beside it.
void __mainUI_trackSelect_cb(puObject *o)
{
    MainUI *mainUI = (MainUI *)o->getUserData();
    mainUI->_trackSelect_cb(o);
}

void __mainUI_trackAircraft_cb(puObject *o)
{
    AtlasWindow *aw = (AtlasWindow *)o->getUserData();
    aw->setAutocentreMode(o->getValue() != 0);
}

void __mainUI_centre_cb(puObject *o)
{
    AtlasWindow *aw = (AtlasWindow *)o->getUserData();
    aw->centreMapOnAircraft();
}

// Called when return is pressed in the track buffer size input field.
void __mainUI_trackLimit_cb(puObject *o)
{
    AtlasController *ac = (AtlasController *)o->getUserData();
    ac->setTrackLimit(o->getIntegerValue());
}

void __mainUI_attach_cb(puObject *o)
{
    MainUI *mainUI = (MainUI *)o->getUserData();
    mainUI->_attach_cb(o);
}

// Called if the user hits a button on the close track dialog box.
void __mainUI_closeOk_cb(puObject *o)
{
    MainUI *mainUI = (MainUI *)o->getUserData();
    AtlasDialog::CallbackButton pos = 
	(AtlasDialog::CallbackButton)o->getDefaultValue();
    bool okay = (pos == AtlasDialog::LEFT);
    mainUI->_closeOk_cb(okay);
}

void __networkPopup_ok_cb(puObject *o)
{
    MainUI *mainUI = (MainUI *)o->getUserData();
    mainUI->_networkPopup_cb(true);
}

void __networkPopup_cancel_cb(puObject *o)
{
    MainUI *mainUI = (MainUI *)o->getUserData();
    mainUI->_networkPopup_cb(false);
}

// Called if the user hits ok or cancel on the network/serial dialog.
void __networkPopup_serialToggle_cb(puObject *o)
{
    NetworkPopup *popup = (NetworkPopup *)o->getUserData();
    popup->_serialToggle_cb(o);
}

void __lightingUI_cb(puObject *o)
{
    LightingUI *lightingUI = (LightingUI *)o->getUserData();
    lightingUI->_cb(o);
}

void __helpUI_cb(puObject *o)
{
    HelpUI *helpUI = (HelpUI *)o->getUserData();
    helpUI->_cb(o);
}

// EYE - are these docs correct?

// Sets _eyeUp to point directly north from the current _eye vector.
// If the _eye is at the north or south pole, then we arbitrarily
// align it with lon = 0.  Changes _eyeUp.
void AtlasWindow::_rotate(double hdg)
{
    // North or south pole?
    if ((_eye[0] == 0.0) && (_eye[1] == 0.0)) {
	sgdSetVec3(_eyeUp, 1.0, 0.0, 0.0);
	// EYE - return?  What about invalidation, notification, ...?
	return;
    }

    // There are probably more efficient ways to accomplish this, but
    // this approach has the advantage that I understand it.
    // Basically what we do is rotate a unit vector that initially
    // points up (north).  We roll it by the heading passed in, and
    // then rotate by the longitude of the current eye point, then
    // pitch it up by the latitude.  Note that the eye point uses
    // geocentric, not geodetic, coordinates.
    SGVec3<double> c(_eye[0], _eye[1], _eye[2]);
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
    sgdXformVec3(_eyeUp, up, rot);

    // Rotations invalidate the cursor (unless it happens to be dead
    // centre, but we don't check).
    _cursor->invalidate();

    // Notify subscribers that we've rotated.
    Notification::notify(Notification::Rotated);
}

// Called after a change to _eye or _eyeUp.  Sets the global model
// view matrix, notifies everyone that we've moved.  Assumes that _eye
// and/or _eyeUp has been correctly set.  In general, this routine
// shouldn't be called directly - use movePosition() or
// rotatePosition() instead.
void AtlasWindow::_move()
{
    // We do OpenGL stuff, so we need to make sure our context is
    // current.
    int win = set();
    // Note that we always look at the origin.  This means that our
    // views will not quite be perpendicular to the earth's surface,
    // since the earth is not perfectly spherical.
    glLoadIdentity();
    gluLookAt(_eye[0], _eye[1], _eye[2], 
    	      0.0, 0.0, 0.0, 
    	      _eyeUp[0], _eyeUp[1], _eyeUp[2]);

    // Retrieve the new model view matrix (we need to pass this in to
    // the scenery and navaid data objects).
    sgdMat4 mvm;
    glGetDoublev(GL_MODELVIEW_MATRIX, (GLdouble *)mvm);

    // Return to the former context.
    set(win);

    // // This does the equivalent of the above, using the pseudo-code
    // // given on the gluLookAt man page.
    // sgdVec3 f, upPrimed, s, u;
    // sgdMat4 t;
    // sgdNegateVec3(f, _eye);
    // sgdNormalizeVec3(f);
    // sgdNormalizeVec3(upPrimed, _eyeUp);
    // sgdVectorProductVec3(s, f, upPrimed);
    // sgdVectorProductVec3(u, s, f);
    // sgdSetVec4(mvm[0], s[0], u[0], -f[0], 0.0);
    // sgdSetVec4(mvm[1], s[1], u[1], -f[1], 0.0);
    // sgdSetVec4(mvm[2], s[2], u[2], -f[2], 0.0);
    // sgdSetVec4(mvm[3], 0.0, 0.0, 0.0, 1.0);
    // sgdSetVec4(t[0], 1.0, 0.0, 0.0, 0.0);
    // sgdSetVec4(t[1], 0.0, 1.0, 0.0, 0.0);
    // sgdSetVec4(t[2], 0.0, 0.0, 1.0, 0.0);
    // sgdSetVec4(t[3], -_eye[0], -_eye[1], -_eye[2], 1.0);
    // sgdPreMultMat4(mvm, t);

    // Tell our scenery and navdata objects about the move.
    _scenery->move(mvm, _eye);
    _ac->navData()->move(mvm);

    // Moves invalidate the cursor and the centre.
    _cursor->invalidate();
    _centre->invalidate();

    if (_relativePalette) {
    	_setPaletteBase();
    }

    // Notify subscribers that we've moved.
    Notification::notify(Notification::Moved);
}

