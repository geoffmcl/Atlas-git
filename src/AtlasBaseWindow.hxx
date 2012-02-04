/*-------------------------------------------------------------------------
  AtlasBaseWindow.hxx

  Written by Brian Schack

  Copyright (C) 2012 Brian Schack

  This is a small specialization of GLUTWindow, adding the ability to
  specify regular and bold fonts for the window (PLIB fonts are
  implemented using OpenGL textures, which makes them window-local).

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

#ifndef _ATLAS_BASE_WINDOW_H_
#define _ATLAS_BASE_WINDOW_H_

#include "GLUTWindow.hxx"	// GLUTWindow

// Forward class declarations
class atlasFntTexFont;

class AtlasBaseWindow: public GLUTWindow {
  public:
    AtlasBaseWindow(const char *name,
		    const char *regularFontFile,
		    const char *boldFontFile);
    ~AtlasBaseWindow();

    atlasFntTexFont *regularFont() { return _regularFont; }
    atlasFntTexFont *boldFont() { return _boldFont; }

  protected:
    atlasFntTexFont *_regularFont;
    atlasFntTexFont *_boldFont;
    
};

#endif
