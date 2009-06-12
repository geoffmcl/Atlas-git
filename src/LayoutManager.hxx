/*-------------------------------------------------------------------------
  LayoutManager.hxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

  The layout manager is used for rendering text in OpenGL.  It
  provides a few (cheesy) facilities to make handling text easier.  It
  adds "chunks", where a chunk can be a line of text or a box (which
  can contain anything - the layout manager just reserves space for
  them), and the layout manager will try to align everything nicely.
  It can handle changes in fonts.  Text is laid out with positive x
  pointing to the right and positive y pointing up.  The upper-left
  corner text can be placed at an arbitrary location (the default is
  <0.0, 0.0>).

  When creating a piece of laid-out text, you must call begin() first.
  Add the text and boxes that you want (including newlines, which must
  be specified explicitly), the terminate everything with an end().
  At this point it can be drawn with drawText().  Boxes have to be
  drawn "by hand" - query the layout manager using nthChunk() to find
  out where they ended up (the addBox() method returns an id that can
  be passed to nthChunk()), then use those coordinates to draw the
  box.  As I said - cheesy.

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

#ifndef _LAYOUT_MANAGER_H
#define _LAYOUT_MANAGER_H

#include <vector>
#include <string>
#include <map>

#include <plib/fnt.h>

#include "misc.hxx"

class LayoutManager {
  public:
    LayoutManager();
    ~LayoutManager();

    // Begin a layout session, with the lower-left corner at <x, y>.
    void begin(float x = 0.0, float y = 0.0);
    // Set the font to the given PLIB font renderer.
    void setFont(fntRenderer &f, float pointSize, float italics = 0.0);
    // Add some text.  Don't put newlines in the text - break the text
    // into single lines and use the newline() method to tell the
    // layout manager about new lines.  For better results, the text
    // chunks should be as big as possible (the character metrics are
    // more accurately calculated that way).
    void addText(const std::string &s);
    void addText(const char *s);
    // Add a box of the given size.  If <x, y> is <0, 0>, then the
    // lower-left corner of the box will be placed on the baseline of
    // the current line at the current x position.
    int addBox(float width, float height, float x = 0.0, float y = 0.0);
    void newline();
    void end();

    // Get the width and height of the laid-out text.
    void size(float *width, float *height);
    // Move the lower-left corner to the given point.
    void moveTo(float x, float y);

    // Get the lower-left corner of the nth chunk.
    void nthChunk(int n, float *x, float *y);

    void drawText();

  protected:
    bool _layingOut;
    float _x, _y, _width, _height;
    fntRenderer *_f;
    float _pointSize, _italics;

    class Chunk {
      public:
	virtual ~Chunk() {}
	float x, y, width, ascent, descent;
    };
    class TextChunk: public Chunk {
      public:
	std::string s;
	atlasFntTexFont *f;
	float pointSize, italics;
    };
    class Line {
      public:
	float x, y, width, ascent, descent;
	std::vector<Chunk *> chunks;
    };

    Line _currentLine;
    std::vector<Line> _lines;
    unsigned int _noOfChunks;
    std::map<int, Chunk*> _chunkMap;
};

#endif
