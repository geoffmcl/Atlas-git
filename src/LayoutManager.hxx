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
  pointing to the right and positive y pointing up.  The centre of the
  text can be placed at an arbitrary location (the default is <0.0,
  0.0>).  Lines are centre justified.

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
    // These constructors makes it easy to lay out a single string.
    LayoutManager(const std::string &s, atlasFntTexFont *f, float pointSize);
    LayoutManager(const char *s, atlasFntTexFont *f, float pointSize);
    ~LayoutManager();

    // Begin a layout session, with the lower-left corner at <x, y>.
    void begin(float x = 0.0, float y = 0.0);
    // Set the font to the given PLIB font renderer.
    void setFont(atlasFntTexFont *f, float pointSize, float italics = 0.0);
    void setFont(atlasFntTexFont *f) { _font = f; }
    void setPointSize(float pointSize) { _pointSize = pointSize; }
    void setItalics(float italics) { _italics = italics; }
    // Add some text.  The text will be rendered with the current
    // font, point size, and italics style.  If x and y are non-zero,
    // the text will be shifted by the amounts given.  Don't put
    // newlines in the text - break the text into single lines and use
    // the newline() method to tell the layout manager about new
    // lines.  For better results, the text chunks should be as big as
    // possible (the character metrics are more accurately calculated
    // that way).
    void addText(const std::string &s, float x = 0.0, float y = 0.0);
    void addText(const char *s, float x = 0.0, float y = 0.0);
    // Adds a box of the given width and height, with its lower
    // left-hand corner at <x, y> (where x is relative to the end of
    // the previous chunk on the line, and y is relative to the
    // baseline of the current line).  If <x, y> is <0, 0>, then the
    // lower-left corner of the box will be placed on the baseline of
    // the current line at the current x position.
    int addBox(float width, float height, float x = 0.0, float y = 0.0);
    void newline();
    void end();

    // These are convenience routines - they do a begin(), addText(s),
    // and end().
    void setText(const std::string &s);
    void setText(const char *s);

    // Do not call the following routines until end() has been called.

    // Get the width and height of the laid-out text.  Not valid
    // unless end() has been called.
    void size(float *width, float *height);
    float width() { return _width; }
    float height() { return _height; }

    // Points on the bounding box.  U = upper, C = centre, L =
    // lower/left, R = right, with the Y position given before the X
    // position.  Therefore, for example, LC means "lower-centre" (the
    // mid-point of the bottom of the box), while CL means
    // "centre-left" (the mid-point of the left side).
    enum Point { UL, UC, UR, 
		 CL, CC, CR, 
		 LL, LC, LR };

    // Returns the coordinate of the given point on the bounding box.
    float x(Point p = CC);
    float y(Point p = CC);

    // Position the text on the given point.  By default we move the
    // centre to the point, but you can specify other points on the
    // bounding box as well.
    void moveTo(float x, float y, Point p = CC);

    // Get the lower-left corner of the nth chunk.
    void nthChunk(int n, float *x, float *y);

    void drawText();

  protected:
    // True if begin() has been called but end() has not.
    bool _layingOut;
    // Our metrics (<_x, _y> gives the *centre* of the layout).  These
    // are not valid until end() is called.
    float _x, _y, _width, _height;
    atlasFntRenderer _renderer;

    // Current font properties.  Whenever addText() is called, these
    // are used to set the properties for that chunk of text.
    atlasFntTexFont *_font;
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
