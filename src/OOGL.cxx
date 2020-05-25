/*-------------------------------------------------------------------------
  OOGL.cxx

  Written by Brian Schack

  Copyright (C) 2008 - 2014 Brian Schack

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
#ifdef _MSC_VER //this needs to be the first!
#include "config.h"
#endif // _MSC_VER

// Our include file
#include "OOGL.hxx"

// C system files
#include <assert.h>

//////////////////////////////////////////////////////////////////////
// DisplayList
//////////////////////////////////////////////////////////////////////

// True if begin() has been called without a corresponding end().
bool DisplayList::_compiling = false;

DisplayList::DisplayList(): _dl(0), _valid(false)
{
}

DisplayList::~DisplayList()
{
    glDeleteLists(_dl, 1);
}

void DisplayList::begin()
{
    assert(!_compiling);

    // Generate the display list if necessary.
    if (_dl == 0) {
	_dl = glGenLists(1);
	assert(_dl);
    }

    // Start compiling the display list.
    _valid = false;
    glNewList(_dl, GL_COMPILE);
    _compiling = true;
}

void DisplayList::end()
{
    assert(_compiling);
    glEndList();
    _valid = true;
    _compiling = false;
}

void DisplayList::call()
{
    // Although it's not illegal to call an undefined display list,
    // it's probably a logic error.
    assert(_dl);
    assert(_valid);
    glCallList(_dl);
}

