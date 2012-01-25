/*-------------------------------------------------------------------------
  AirwaysOverlay.hxx

  Written by Brian Schack

  Copyright (C) 2008 - 2012 Brian Schack

  The airways overlay manages the loading and drawing of airways.

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

#ifndef _AIRWAYS_OVERLAY_H
#define _AIRWAYS_OVERLAY_H

#include <set>

#include "Overlays.hxx"
#include "Culler.hxx"
#include "Notifications.hxx"
#include "NavData.hxx"

class Overlays;
class AirwaysOverlay: public Subscriber {
  public:
    AirwaysOverlay(Overlays& overlays);
    ~AirwaysOverlay();

    void draw(bool drawHigh, bool drawLow, bool label, NavData *navData);

    void toggle(bool forward = true);

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    double _metresPerPixel;

    void _render(const AWY *n) const;
    bool _label(const AWY *a) const;

    Overlays& _overlays;

    GLuint _highDL, _lowDL;	// Display lists
};

#endif
