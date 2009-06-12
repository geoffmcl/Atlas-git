/*-------------------------------------------------------------------------
  Notifications.hxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

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

class Notification {
  public:
    enum type {Moved = 0, 	 // Eyepoint moved
	       Zoomed, 		 // Zoomed in/out
	       AircraftMoved,
	       FlightTrackModified,
	       NewFlightTrack,
	       MagTrue,		 // Switched between true/magnetic headings
	       DiscreteContours, // Switched between smooth/discrete contours
	       NewPalette,	 // Loaded new palette
	       All};		 // This must not be removed

    static bool subscribe(Subscriber *s, type n);
    static void unsubscribe(Subscriber *s, type n = All);
    static void notify(type n);

  protected:
    Notification();
    ~Notification();

    static std::vector<std::set<Subscriber *> > _notifications;
};

class Subscriber {
  public:
    Subscriber();
    virtual ~Subscriber() = 0;

    bool subscribe(Notification::type n);
    void unsubscribe(Notification::type n = Notification::All);

    virtual bool notification(Notification::type n) = 0;
};

#endif
