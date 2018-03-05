/*-------------------------------------------------------------------------
  OOGL.hxx

  Written by Brian Schack

  Copyright (C) 2018 Brian Schack

  Some useful OpenGL code snippets, written in an object-oriented
  style.  These hopefully will remove much of the ugly and
  non-intuitive OpenGL boilerplate.

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

#ifndef _OOGL_H_
#define _OOGL_H_

#if defined( __APPLE__)		// For GLuint
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif

//////////////////////////////////////////////////////////////////////
// DisplayList
//////////////////////////////////////////////////////////////////////

// This is a convenience class for managing OpenGL display lists.
// Calling begin() starts the compilation of a display list (and
// generates a display list, if this is the first call to begin()).
// Every call to begin() must be paired with a call to end().  Nested
// begin()/end() pairs are not allowed.  The resulting display list
// can be rendered by calling call().  The display list is deleted
// when the destructor is called.
//
// A display list is valid if a begin()/end() pair has been called for
// it.  Calling call() will fail for an invalid display list.  They
// can be invalidated explicitly by calling invalidate().
//
// This class does a bit of error checking.  Calls to begin() are not
// allowed if a begin() is already in progress.  Calls to end() fail
// if there has been no corresponding begin().  "Failure" for this
// class means a failed assertion, resulting in program termination.
// Maybe one day I'll implement exceptions instead.
//
// All display lists are compiled using GL_COMPILE, rather than
// GL_COMPILE_AND_EXECUTE.
class DisplayList {
  public:
    DisplayList();
    ~DisplayList();

    // Define the display list.  Put OpenGL rendering calls between
    // the calls to begin() and end().
    void begin();
    void end();

    // Render/draw the display list.
    void call();

    void invalidate() { _valid = false; }
    bool valid() { return _valid; }

    // Return the display list "name".
    GLuint dl() { return _dl; }

  protected:
    // True if begin() has been called for some display list (only one
    // can be compiled at a time).  When end() is called, it is reset
    // to false.
    static bool _compiling;

    GLuint _dl;
    bool _valid;
};

#endif
