/*-------------------------------------------------------------------------
  Output.hxx
  Abstract graphics output layer

  Written by Per Liedman, started May 2000.
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

#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#include <plib/sg.h>

class GfxOutput {
public:
  GfxOutput( char *filename, int size, bool smooth_shading = true );
  virtual ~GfxOutput();

  inline bool isOpen() { return open; }
  inline int getSize() { return size; }
  virtual void openFragment( int x, int y, int size ) {}
  virtual void closeFragment() {}
  virtual void closeOutput();

  virtual void setShade( bool shade );
  virtual bool getShade();
  virtual void setLightVector( sgVec3 light );
  virtual void setColor( const float *rgb );
  virtual void clear( const float *rgb );
  virtual void drawTriangle( const sgVec2 *vertices, const sgVec3 *normals );
  virtual void drawQuad    ( const sgVec2 *vertices, const sgVec3 *normals );
  virtual void drawTriangle( const sgVec2 *vertices, const sgVec3 *normals, const sgVec4 *colors );
  virtual void drawQuad    ( const sgVec2 *vertices, const sgVec3 *normals, const sgVec4 *colors );
  virtual void drawCircle  ( sgVec2 p, int radius );
  virtual void drawLine    ( sgVec2 p1, sgVec2 p2 );
  virtual void drawText    ( sgVec2 p, char *text );
  virtual void beginLineStrip();
  virtual void addToLineStrip( sgVec2 p );
  virtual void endLineStrip();

protected:
  bool open;
  int size, fragment_size;
  int posx, posy;
};

#endif
