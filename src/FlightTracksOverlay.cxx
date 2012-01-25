/*-------------------------------------------------------------------------
  FlightTracksOverlay.cxx

  Written by Brian Schack

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cassert>

#include "FlightTracksOverlay.hxx"
#include "Globals.hxx"
#include "Preferences.hxx"
#include "Geographics.hxx"
#include "AtlasController.hxx"
#include "AtlasWindow.hxx"

using namespace std;

FlightTracksOverlay::FlightTracksOverlay(Overlays& overlays):
    _overlays(overlays), _isDirty(false), _dl(0)
{
    // Load image for airplane display if requested.
    if (globals.prefs.airplaneImage.str().empty()) {
	_haveImage = false;
    } else if (globals.prefs.airplaneImage.exists()) {
	_airplaneTexture.load(globals.prefs.airplaneImage);
	if (!_airplaneTexture.loaded()) {
	    fprintf(stderr, "Airplane image file %s could not be read.\n"
		    "Falling back to line drawing\n",
		    globals.prefs.airplaneImage.c_str());
	    _haveImage = false;
	} else {
	    _haveImage = true;
	}
    } else {
	fprintf(stderr, "Airplane image file %s was not found.\n"
		"Falling back to line drawing\n",
		globals.prefs.airplaneImage.c_str());
	_haveImage = false;
    }

    // Subscribe to the flight track change notifications.  The first
    // tells us if a new flight track has been made current.  The
    // second tells us if the current flight track has changed.
    subscribe(Notification::NewFlightTrack);
    subscribe(Notification::FlightTrackModified);
}

FlightTracksOverlay::~FlightTracksOverlay()
{
    // for (map<FlightTrack*, TRACK_INFO>::const_iterator i = _tracks.begin(); 
    // 	 i != _tracks.end(); i++) {
    // 	glDeleteLists(i->second.DL, 1);
    // }
    glDeleteLists(_dl, 1);
}

// void FlightTracksOverlay::addTrack(FlightTrack *t, 
// 				   sgVec4 trackColour, 
// 				   sgVec4 planeColour)
// {
//     TRACK_INFO info;

//     if (t) {
// 	sgCopyVec4(info.trackColour, trackColour);
// 	sgCopyVec4(info.planeColour, planeColour);
// 	info.DL = 0;

// 	_tracks[t] = info;
//     }
// }

// void FlightTracksOverlay::removeTrack(FlightTrack *t)
// {
//     if (t) {
// 	_tracks.erase(t);
//     } else {
// 	_tracks.clear();
//     }
// }

void FlightTracksOverlay::setDirty()
{
    _isDirty = true;
}

// void FlightTracksOverlay::draw()
// {
//     // We save tracks in display lists, as they rarely change, and are
//     // rendered the same regardless of zooms and moves.  The airplane,
//     // however, is drawn anew each time - it's cheap to do, it changes
//     // more often, and it has to be drawn differently when we zoom.
//     if (_isDirty) {
// 	for (map<FlightTrack*, TRACK_INFO>::const_iterator i = _tracks.begin(); 
// 	     i != _tracks.end(); i++) {
// 	    FlightTrack *t = i->first;
// 	    TRACK_INFO info = i->second;

// 	    if (info.DL == 0) {
// 		info.DL = glGenLists(1);
// 		assert(info.DL != 0);
// 	    }

// 	    glNewList(info.DL, GL_COMPILE);
// 	    glPushAttrib(GL_LINE_BIT); {
// 		// Draw the track.
// 		glColor4fv(info.trackColour);
//                 glLineWidth(prefs.lineWidth);

// 		glBegin(GL_LINE_STRIP); {
// 		    for (size_t i = 0; i < t->size(); i++) {
// 			FlightData *d = t->at(i);
// 			glVertex3dv(d->cart);
// 		    }
// 		}
// 		glEnd();
// 	    }
// 	    glPopAttrib();
// 	    glEndList();

// 	    _tracks[t] = info;
// 	}
	
// 	_isDirty = false;
//     }

//     for (map<FlightTrack*, TRACK_INFO>::const_iterator i = _tracks.begin(); 
// 	 i != _tracks.end(); i++) {
// 	// Draw the track.
// 	glCallList(i->second.DL);

// 	// Draw the airplane.  If the track is live, we draw an
// 	// airplane at the end, in the track colour.  We also draw an
// 	// airplane at the mark, in the plane colour.
// 	FlightTrack *t = i->first;
// 	if (t->live()) {
// 	    _drawAirplane(t->last(), i->second.trackColour);
// 	}
// 	_drawAirplane(t->current(), i->second.planeColour);
//     }
// }

// EYE - instead of just drawing the current track, we should
// eventually have the option of drawing all tracks.
void FlightTracksOverlay::draw()
{
    FlightTrack *t = _overlays.ac()->currentTrack();
    if (!t) {
	return;
    }

    // We save tracks in display lists, as they rarely change, and are
    // rendered the same regardless of zooms and moves.  The airplane,
    // however, is drawn anew each time - it's cheap to do, it changes
    // more often, and it has to be drawn differently when we zoom.
    if (_isDirty) {
	if (_dl == 0) {
	    _dl = glGenLists(1);
	    assert(_dl != 0);
	}

	glNewList(_dl, GL_COMPILE);
	glPushAttrib(GL_LINE_BIT); {
	    // Draw the track.
	    glColor4fv(globals.trackColour);
	    glLineWidth(globals.prefs.lineWidth);

	    glBegin(GL_LINE_STRIP); {
		for (size_t i = 0; i < t->size(); i++) {
		    FlightData *d = t->at(i);
		    glVertex3dv(d->cart);
		}
	    }
	    glEnd();
	}
	glPopAttrib();
	glEndList();
	
	_isDirty = false;
    }

    // Draw the track.
    glCallList(_dl);

    // Draw the airplane.  If the track is live, we draw an
    // airplane at the end, in the track colour.  We also draw an
    // airplane at the mark, in the plane colour.
    if (t->live()) {
	_drawAirplane(t->last(), globals.trackColour);
    }
    _drawAirplane(t->current(), globals.markColour);
}

// Called when somebody posts a notification that we've subscribed to.
void FlightTracksOverlay::notification(Notification::type n)
{
    if (n == Notification::NewFlightTrack) {
	setDirty();
    } else if (n == Notification::FlightTrackModified) {
	setDirty();
    } else {
	assert(false);
    }
}

// Draw the airplane at the given point in the given track in the
// given colour.  We draw it in the XY plane, with the nose pointing
// in the positive Y direction.  The XY plane, however, has been
// translated, rotated, scaled, and otherwise shamelessly manipulated
// so that it maps onto the correct place in the correct orientation
// in XYZ space.
void FlightTracksOverlay::_drawAirplane(FlightData *d, const sgVec4 colour)
{
    if (d == NULL) {
	return;
    }

    // EYE - draw a trail (eg, 10s) as well?
    glColor4fv(colour);
    geodPushMatrix(d->cart, d->lat, d->lon, d->hdg); {
	double scale = _overlays.aw()->scale();
	glScaled(scale, scale, scale);

	// EYE - why does the aircraft not draw itself exactly on the
	// track?  Check for places where floats are used instead of
	// doubles.
	if (_haveImage) {
	    // Draw texture.
	    float b = globals.prefs.airplaneImageSize / 2.0;
	    glEnable(GL_TEXTURE_2D);
	    glBindTexture(GL_TEXTURE_2D, _airplaneTexture.name());
	    glBegin(GL_QUADS); {
		glTexCoord2f(0.0, 1.0); glVertex3f(-b, -b, 0.0);
		glTexCoord2f(1.0, 1.0); glVertex3f( b, -b, 0.0);
		glTexCoord2f(1.0, 0.0); glVertex3f( b,  b, 0.0);
		glTexCoord2f(0.0, 0.0); glVertex3f(-b,  b, 0.0);
	    } glEnd();
	    glDisable(GL_TEXTURE_2D);
	} else {
	    // Draw crude stick-figure aircraft.
	    glBegin(GL_LINES); {
		// "fuselage"
		glVertex3f(0.0, 4.0, 0.0);
		glVertex3f(0.0, -9.0, 0.0);

		// "wing"
		glVertex3f(-7.0, 0.0, 0.0);
		glVertex3f(7.0, 0.0, 0.0);

		// "tail"
		glVertex3f(-3.0, -7.0, 0.0);
		glVertex3f(3.0, -7.0, 0.0);
	    }
	    glEnd();
	}
    }
    geodPopMatrix();
}
