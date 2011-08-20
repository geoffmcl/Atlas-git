/*-------------------------------------------------------------------------
  AirwaysOverlay.hxx

  Written by Brian Schack

  Copyright (C) 2008 - 2011 Brian Schack

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

// AwyLabel - one end of an airway.  It has a name, a location, and a
// pointer to a NAV struct (if isNavaid is true) or a FIX struct (if
// isNavaid is false).
struct AwyLabel {
    std::string id;
    double lat, lon;
    bool isNavaid;
    void *n;
};

// AWY - a single airway segment, perhaps shared by several airways.
struct AWY: public Cullable {
    // Cullable interface.
    const atlasSphere& Bounds() { return bounds; }
    // EYE - an approximation - use centre?
    double latitude() { return start.lat; }
    double longitude() { return start.lon; }

    std::string name;
    struct AwyLabel start, end;
    bool isLow;			// True if a low airway, false if high
    int base, top;		// Base and top of airway in 100's of feet
    atlasSphere bounds;
    double length;		// Length of segment in metres
};

class Overlays;
class AirwaysOverlay: public Subscriber {
  public:
    AirwaysOverlay(Overlays& overlays);
    ~AirwaysOverlay();

    bool load(const std::string& fgDir);

    void draw(bool drawHigh, bool drawLow, bool label);

    void toggle(bool forward = true);

    // Subscriber interface.
    bool notification(Notification::type n);

  protected:
    Culler *_culler;
    Culler::FrustumSearch *_frustum;
    double _metresPerPixel;

    bool _load640(const gzFile& arp);
    void _checkEnd(AwyLabel &end, bool isLow);

    void _render(const AWY *n) const;
    bool _label(const AWY *a) const;

    Overlays& _overlays;

    std::vector<AWY *> _segments; // Individual airway segments.

    GLuint _highDL, _lowDL;	// Display lists
};

#endif
