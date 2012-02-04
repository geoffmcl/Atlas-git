/*-------------------------------------------------------------------------
  AtlasBaseWindow.cxx

  Written by Brian Schack

  Copyright (C) 2012 Brian Schack

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
#include "AtlasBaseWindow.hxx"

// C++ system files
#include <stdexcept>

// Our project's include files
#include "misc.hxx"

using namespace std;

AtlasBaseWindow::AtlasBaseWindow(const char *name,
				 const char *regularFontFile,
				 const char *boldFontFile):
    GLUTWindow(name)
{
    puInit();
    fntInit();

    // Load our fonts.
    _regularFont = new atlasFntTexFont;
    // EYE - throw an error?  Fallback to a GLUT font?
    if (_regularFont->load(regularFontFile) != TRUE) {
	fprintf(stderr, "Required font file '%s' not found.\n", 
		regularFontFile);
	exit(-1);
    }

    _boldFont = new atlasFntTexFont;
    // EYE - throw an error?  Fallback to a GLUT font?
    if (_boldFont->load(boldFontFile) != TRUE) {
	fprintf(stderr, "Required font file '%s' not found.\n", 
		boldFontFile);
	exit(-1);
    }

    // Set some interface defaults.
    puFont uiFont(_regularFont, 12.0);
    puSetDefaultFonts(uiFont, uiFont);

    // Note that the default colour scheme has an alpha of 0.8 - this,
    // and the fact that GL_BLEND is on, means that the widgets will
    // be slightly translucent.  Set to 1.0 if you want them to be
    // completely opaque.
    puSetDefaultColourScheme(0.4, 0.5, 0.9, 0.8);
    puSetDefaultStyle(PUSTYLE_SMALL_BEVELLED);
}

AtlasBaseWindow::~AtlasBaseWindow()
{
    delete _regularFont;
    delete _boldFont;
}
