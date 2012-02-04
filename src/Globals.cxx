/*-------------------------------------------------------------------------
  Globals.cxx

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

// Our include file
#include "Globals.hxx"

// C++ system include files
#include <algorithm>
#include <limits>

// Our project's include files
#include "Bucket.hxx"

Globals globals;

Globals::Globals()
{
    sgSetVec4(trackColour, 0.0, 0.0, 1.0, 0.7);
    sgSetVec4(markColour, 1.0, 1.0, 0.0, 0.7);
    // EYE - eventually these should be placed in preferences.  Also,
    // we'll eventually need to deal with more than 3 radios.
    sgSetVec4(vor1Colour, 0.000, 0.420, 0.624, 0.7);
    sgSetVec4(vor2Colour, 0.420, 0.624, 0.0, 0.7);
    sgSetVec4(adfColour, 0.525, 0.294, 0.498, 0.7);
}
