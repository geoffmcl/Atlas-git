/*-------------------------------------------------------------------------
  Notifications.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2012 Brian Schack

  The notification class allows classes to send messages to interested
  parties (subscribers).  This is useful when you don't want classes
  to be too closely tied together, but still need to know about what's
  happening elsewhere.  It's also useful because the sender doesn't
  need to know who's listening - it just says "something happened",
  and all its subscribers will be notified.

  Notifications are often used within the model-view-controller (MVC)
  paradigm in Atlas, and we try to use them in a consistent fashion.
  Model objects neither send nor receive notifications.  They are
  intended to be passive objects, and shouldn't have to know about
  anything else.  Controller objects send notifications, but do not
  receive them.  Objects "above" them (if we consider models to be at
  the bottom of the MVC hierarchy and views at the top) listen for the
  notifications sent by controllers.  Views listen to notifications,
  and contact their controllers directly if they need to access or
  modify data in models.  The reasoning behind this is that views tend
  to be owned by a single controller; therefore, they know who to go
  to for data.  Controllers, on the other hand, may be controlling
  data (models) that are of interest to many other views (or
  intermediate controllers, if any exist).  It's easier for them to
  simply broadcast a notification when a model changes than keep track
  of and notify each interested party.

  Note that there's no guarantee about which subscriber will get
  notified first.

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

#ifndef _NOTIFICATION_H
#define _NOTIFICATION_H

#include <set>
#include <vector>

class Subscriber;

// The notification class acts as the messaging centre.  All
// notifications are sent here, and then sent out to subscribers.
// Note that the constructor and destructor are private and all
// variables and methods are class variables and methods - we never
// allow instances of this class to be created.
//
// The only method of interest is notify() - you call this whenever
// you want to tell others that one of the events below happened.  If
// you want to create a new type of event, you must add it to the
// enumeration.
class Notification {
  public:
    // EYE - add: WindowResized (for keeping UI elements in fixed
    // locations), MouseMoved (for any mouse moves; replace MouseMoved
    // with CursorLocationChanged, and add CentreLocationChanged and
    // LocationChanged events), SceneryMode/MappingMode.
    enum type {Moved = 0, 	 // Eyepoint moved
	       Zoomed, 		 // Zoomed in/out
	       Rotated,		 // View rotated about centre
	       CursorLocation,	 // Cursor location changed (in mouse mode)
	       MouseMoved,	 // Mouse moved (in any mode)
	       ShowOutlines,     // Show chunk and tile outlines
	       CentreType,	 // Centre type changed
	       // EYE - does this overlap with SceneryChanged?
	       NewScenery,	 // New maps or live scenery loaded
	       AircraftMoved,	 // Mark changed
	       FlightTrackModified, // Point added/deleted/modified,
				    // name changed
	       // EYE - change to just "FlightTrack"?  We really need
	       // to be consistent about naming here - use past tense
	       // verbs (FlightTrackSelected, DidSelectFlightTrack),
	       // the object that changed (FlightTrack), the new
	       // situation (NewFlightTrack), or something else?
	       NewFlightTrack,	 // A new flight track has been chosen.
	       FlightTrackList,	 // List of flight tracks has changed.
	       ShowTrackInfo,	 // Toggled display of flight track
				 // info (including the graph window)

	       DegMinSec,        // Switched between DMS/decimal degrees
	       MagTrue,		 // Switched between true/magnetic headings
	       MEFs,		 // Toggle MEFs
	       OverlayToggled,	 // Overlay turned on or off
	       AutocentreMode,	 // Autocentre mode toggled

	       DiscreteContours, // Switched between smooth/discrete contours
	       ContourLines,     // Toggled contour lines
	       LightingOn,       // Toggled lights
	       SmoothShading,    // Switched between smooth/flat polygon shading
	       Azimuth,	         // Changed light azimuth
	       Elevation,        // Changed light elevation

	       ImageType,        // Changed image type (JPEG, PNG)
	       JPEGQuality,      // Changed JPEG quality

	       // EYE - does this overlap with NewScenery?
	       SceneryChanged,	 // Scenery or maps added or deleted

	       Palette,		 // Current palette changed
	       PaletteList,	 // Palette list changed

	       TileDispatched,   // The dispatcher has scheduled a
				 // tile to be mapped

	       All};		 // This must not be removed and must
				 // be at the end.

    // EYE - it would be nice if we could pass some user data along
    // with it, or make a notification class that has the type and
    // some data packaged together.
    static void notify(type n);

  protected:
    Notification();
    ~Notification();

    friend class Subscriber;
    static bool subscribe(Subscriber *s, type n);
    static void unsubscribe(Subscriber *s, type n = All);

    static std::vector<std::set<Subscriber *> > _notifications;
};

class Subscriber {
  public:
    Subscriber();
    virtual ~Subscriber() = 0;

    // Subscribe to an event.  Returns true if the subscription was
    // successful (it always is, unless you try to subscribe to 'All',
    // which is not allowed).
    bool subscribe(Notification::type n);
    // Unsubscribe.  If you unsubscribe from 'All', all current
    // subscriptions will be cancelled.
    void unsubscribe(Notification::type n = Notification::All);

    // This must be implemented by subclasses.  It will be called
    // whenever Notification::notify() is called with something we've
    // subscribed to.
    virtual void notification(Notification::type n) = 0;
};

#endif
