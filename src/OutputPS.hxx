/*-------------------------------------------------------------------------
  OutputPS.hxx
  PostScript graphics output

  Written by Per Liedman, started May 2000.
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

#ifndef __OUTPUTPS_H__
#define __OUTPUTPS_H__

#include <stdio.h>
#include "Output.hxx"

class OutputPS : public GfxOutput {
public:
  OutputPS( char *filename, int size, bool smooth_shading = true );
  virtual ~OutputPS();

  virtual void closeOutput();

  virtual void setColor( const float *rgb );
  virtual void clear( const float *rgb );
  virtual void drawTriangle( const sgVec2 *p, const sgVec3 *normals );
  virtual void drawQuad    ( const sgVec2 *p, const sgVec3 *normals );
  virtual void drawCircle  ( sgVec2 p, int radius );
  virtual void drawLine    ( sgVec2 p1, sgVec2 p2 );
  virtual void drawText    ( sgVec2 p, char *text );

protected:
  int quadrant( const sgVec2 p, bool checkoutside = true );

  FILE *ps_file;
};

#endif
