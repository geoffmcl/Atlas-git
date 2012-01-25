/*-------------------------------------------------------------------------
  FixesOverlay.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2012 Brian Schack

  The fixes overlay manages the loading and display of navigational
  fixes.

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

#ifndef _FIXES_OVERLAY_H
#define _FIXES_OVERLAY_H

#include "Overlays.hxx"
#include "Culler.hxx"
#include "Searcher.hxx"
#include "Notifications.hxx"
#include "LayoutManager.hxx"
#include "NavData.hxx"

class Overlays;
class FixesOverlay: public Subscriber {
  public:
    FixesOverlay(Overlays& overlays);
    ~FixesOverlay();

    void setDirty();

    void draw(NavData *navData);

    // Subscriber interface.
    void notification(Notification::type n);

  protected:
    double _metresPerPixel;

    // bool _load600(const gzFile& arp);

    void _render(const FIX *f);
    void _label(const FIX *f, LayoutManager& lm);

    Overlays& _overlays;

    GLuint _DL;			// Display list of all rendered fixes.
    bool _isDirty;
};

#endif
