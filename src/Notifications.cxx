/*-------------------------------------------------------------------------
  Notifications.cxx

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

#include <cassert>

#include "Notifications.hxx"

using namespace std;

Notification::Notification()
{
}

Notification::~Notification()
{
}

// Create a vector big enough to handle all notifications.
vector<set<Subscriber *> > Notification::_notifications (Notification::All);

bool Notification::subscribe(Subscriber *s, type n)
{
    if (n == All) {
	return false;
    }

    _notifications[n].insert(s);

    return true;
}

void Notification::unsubscribe(Subscriber *s, type n)
{
    if (n == All) {
	assert(_notifications.size() == All);
	for (unsigned int i = 0; i < _notifications.size(); i++) {
	    _notifications[i].erase(s);
	}
    } else {
	_notifications[n].erase(s);
    }
}

void Notification::notify(type n)
{
    if (n == All) {
	return;
    }

    set<Subscriber *>& subscribers = _notifications[n];
    set<Subscriber *>::iterator i;
    for (i = subscribers.begin(); i != subscribers.end(); i++) {
	(*i)->notification(n);
    }
}

Subscriber::Subscriber()
{
}

Subscriber::~Subscriber()
{
    Notification::unsubscribe(this, Notification::All);
}

bool Subscriber::subscribe(Notification::type n)
{
    return Notification::subscribe(this, n);
}

void Subscriber::unsubscribe(Notification::type n)
{
    Notification::unsubscribe(this, n);
}
