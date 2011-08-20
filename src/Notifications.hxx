/*-------------------------------------------------------------------------
  Notifications.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2011 Brian Schack

  The notification class allows classes to send messages to interested
  parties (subscribers).  This is useful when you don't want classes
  to be too closely tied together, but still need to know about what's
  happening elsewhere.  It's also useful because the sender doesn't
  need to know who's listening - it just says "something happened",
  and all its subscribers will be notified.

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
    enum type {Moved = 0, 	 // Eyepoint moved
	       Zoomed, 		 // Zoomed in/out
	       Rotated,		 // View rotated about centre
	       NewScenery,	 // New live scenery loaded
	       AircraftMoved,
	       FlightTrackModified,
	       NewFlightTrack,
	       MagTrue,		 // Switched between true/magnetic headings
	       DiscreteContours, // Switched between smooth/discrete contours
	       NewPalette,	 // Loaded new palette
	       All};		 // This must not be removed

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
    // Unsubscribe.  If you subscribe from 'All', all current
    // subscriptions will be cancelled.
    void unsubscribe(Notification::type n = Notification::All);

    // This must be implemented by subclasses.  It will be called
    // whenever Notification::notify() is called with something we've
    // subscribed to.

    // EYE - why bool?
    virtual bool notification(Notification::type n) = 0;
};

#endif
