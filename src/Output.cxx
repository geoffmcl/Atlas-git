/*-------------------------------------------------------------------------
  Output.cxx
  Abstract graphics output layer

  Written by Per Liedman, started February 2000.
  PostScript code and ideas by Christian Mayer (vader@t-online.de)
  Copyright (C) 2000 Per Liedman, liedman@home.se

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
---------------------------------------------------------------------------*/

#include <stdlib.h>
#include "Output.hxx"

GfxOutput::GfxOutput( char *filename, int size ) : size(size) {
  if (filename == NULL) {
    open = false;
  } else {
    open = true;
  }
}

GfxOutput::~GfxOutput() {}

void GfxOutput::closeOutput() {}
void GfxOutput::setColor( const float *rgb ) {}
void GfxOutput::clear( const float *rgb ) {}
void GfxOutput::drawTriangle( sgVec2 *p ) {}
void GfxOutput::drawQuad    ( sgVec2 *p ) {}
void GfxOutput::drawCircle  ( sgVec2 p, int radius ) {}
void GfxOutput::drawLine    ( sgVec2 p1, sgVec2 p2 ) {}
void GfxOutput::drawText    ( sgVec2 p, char *text ) {}
void GfxOutput::beginLineStrip() {}
void GfxOutput::addToLineStrip( sgVec2 p ) {}
void GfxOutput::endLineStrip() {}
