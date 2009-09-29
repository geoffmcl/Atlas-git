/*-------------------------------------------------------------------------
  FlightTracksOverlay.cxx

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

#include "FlightTracksOverlay.hxx"
#include "Globals.hxx"

using namespace std;

FlightTracksOverlay::FlightTracksOverlay(Overlays& overlays):
    _overlays(overlays), _isDirty(false)
{
    // Subscribe to the flight track change notifications.  The first
    // tells us if a new flight track has been made current.  The
    // second tells us if the current flight track has changed.
    subscribe(Notification::NewFlightTrack);
    subscribe(Notification::FlightTrackModified);
}

FlightTracksOverlay::~FlightTracksOverlay()
{
    for (map<FlightTrack*, TRACK_INFO>::const_iterator i = _tracks.begin(); 
	 i != _tracks.end(); i++) {
	glDeleteLists(i->second.DL, 1);
    }
}

void FlightTracksOverlay::addTrack(FlightTrack *t, 
				   sgVec4 trackColour, 
				   sgVec4 planeColour)
{
    TRACK_INFO info;

    if (t) {
	sgCopyVec4(info.trackColour, trackColour);
	sgCopyVec4(info.planeColour, planeColour);
	info.DL = 0;

	_tracks[t] = info;
    }
}

void FlightTracksOverlay::removeTrack(FlightTrack *t)
{
    if (t) {
	_tracks.erase(t);
    } else {
	_tracks.clear();
    }
}

void FlightTracksOverlay::setDirty()
{
    _isDirty = true;
}

// Draw the airplane at the given point in the given track in the
// given colour.  We draw it in the XY plane, with the nose pointing
// in the positive Y direction.  The XY plane, however, has been
// translated, rotated, scaled, and otherwise shamelessly manipulated
// so that it maps onto the correct place in the correct orientation
// in XYZ space.
static void __drawAirplane(FlightData *d, const sgVec4 colour)
{
    if (d == NULL) {
	return;
    }

    // EYE - draw a trail (eg, 10s) as well?
    glColor4fv(colour);
    glPushMatrix(); {
	sgdVec3 cart;
	atlasGeodToCart(d->lat, d->lon, d->alt * SG_FEET_TO_METER, 
			cart);

	glTranslated(cart[0], cart[1], cart[2]);
	glRotatef(d->lon + 90.0, 0.0, 0.0, 1.0);
	glRotatef(90.0 - d->lat, 1.0, 0.0, 0.0);
	glRotatef(-d->hdg, 0.0, 0.0, 1.0);
	double metresPerPixel = globals.metresPerPixel;
	glScalef(metresPerPixel, metresPerPixel, metresPerPixel);

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
    glPopMatrix();
}

void FlightTracksOverlay::draw()
{
    // We save tracks in display lists, as they rarely change, and are
    // rendered the same regardless of zooms and moves.  The airplane,
    // however, is drawn anew each time - it's cheap to do, it changes
    // more often, and it has to be drawn differently when we zoom.
    if (_isDirty) {
	for (map<FlightTrack*, TRACK_INFO>::const_iterator i = _tracks.begin(); 
	     i != _tracks.end(); i++) {
	    FlightTrack *t = i->first;
	    TRACK_INFO info = i->second;

	    if (info.DL == 0) {
		info.DL = glGenLists(1);
		assert(info.DL != 0);
	    }

	    glNewList(info.DL, GL_COMPILE); {
		// Draw the track.
		glColor4fv(info.trackColour);

		glBegin(GL_LINE_STRIP); {
		    for (size_t i = 0; i < t->size(); i++) {
			FlightData *d = t->at(i);
			glVertex3dv(d->cart);
		    }
		}
		glEnd();
	    }
	    glEndList();

	    _tracks[t] = info;
	}
	
	_isDirty = false;
    }

    for (map<FlightTrack*, TRACK_INFO>::const_iterator i = _tracks.begin(); 
	 i != _tracks.end(); i++) {
	// Draw the track.
	glCallList(i->second.DL);

	// Draw the airplane.  If the track is live, we draw an
	// airplane at the end, in the track colour.  We also draw an
	// airplane at the mark, in the plane colour.
	FlightTrack *t = i->first;
	if (t->live()) {
	    __drawAirplane(t->last(), i->second.trackColour);
	}
	__drawAirplane(t->current(), i->second.planeColour);
    }
}

// Called when somebody posts a notification that we've subscribed to.
bool FlightTracksOverlay::notification(Notification::type n)
{
    if (n == Notification::NewFlightTrack) {
	setDirty();
    } else if (n == Notification::FlightTrackModified) {
	setDirty();
    } else {
	assert(false);
    }

    return true;
}
