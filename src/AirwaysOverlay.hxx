/*-------------------------------------------------------------------------
  AirwaysOverlay.hxx

  Written by Brian Schack

  Copyright (C) 2008 - 2018 Brian Schack

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

#include "Notifications.hxx"	// Subscriber
#include "OOGL.hxx"		// DisplayList

// Forward class declarations
class Overlays;
class NavData;
class Segment;
class LayoutManager;

class Overlays;
class AirwaysOverlay: public Subscriber {
  public:
    AirwaysOverlay(Overlays& overlays);
    ~AirwaysOverlay();

    void draw(NavData *navData);

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    double _metresPerPixel;
    float _labelPointSize;	// Label size, in pixels.

    void _render(const Segment *seg) const;
    bool _label(const Segment *seg) const;

    Overlays& _overlays;
    bool _visibleHigh, _visibleLow, _labels;

    DisplayList _high, _low;
};

#endif
