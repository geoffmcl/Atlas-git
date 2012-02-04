/*-------------------------------------------------------------------------
  Globals.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2012 Brian Schack

  This contains some variables that are needed in several different
  parts of Atlas.  Although we could pass them around via function
  parameters, this is clearer (I think).

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

#ifndef _GLOBALS_H
#define _GLOBALS_H

#include <plib/sg.h>
#include "Preferences.hxx"
#include "misc.hxx"

// Forward class declarations
class AtlasWindow;
class GraphsWindow;

class Globals {
 public:
    Globals();

    // Standard colours.
    sgVec4 trackColour, markColour;
    sgVec4 vor1Colour, vor2Colour, adfColour;

    // Our windows.
    AtlasWindow *aw;
    GraphsWindow *gw;

    // Our preferences.
    Preferences prefs;

    // A shared global string.  It can be used for temporary string
    // manipulation, saving the need to malloc (and possibly realloc)
    // memory.
    AtlasString str;
};

extern Globals globals;

#endif
