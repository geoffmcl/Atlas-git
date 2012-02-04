/*-------------------------------------------------------------------------
  CrosshairsOverlay.cxx

  Written by Brian Schack

  Copyright (C) 2009 - 2012 Brian Schack

  Draws crosshairs.

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

#ifndef _CROSSHAIRS_OVERLAY_H
#define _CROSSHAIRS_OVERLAY_H

// Forward class declarations
class Overlays;

class CrosshairsOverlay {
  public:
    CrosshairsOverlay(Overlays& overlays);
    ~CrosshairsOverlay();

    void draw();

  protected:
    Overlays& _overlays;
};

#endif
