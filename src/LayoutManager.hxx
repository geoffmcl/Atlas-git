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
  pointing to the right and positive y pointing up.  The text can be
  placed at an arbitrary location (the default is <0.0, 0.0>).  Lines
  are centre justified.

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
    // They call begin() and end().  The text is centred at 0,0.
    LayoutManager(const std::string& s, atlasFntTexFont *f, float pointSize);
    LayoutManager(const char *s, atlasFntTexFont *f, float pointSize);
    ~LayoutManager();

    // Points on the bounding box.  U = upper, C = centre, L =
    // lower/left, R = right, with the Y position given before the X
    // position.  Therefore, for example, LC means "lower-centre" (the
    // mid-point of the bottom of the box), while CL means
    // "centre-left" (the mid-point of the left side).
    enum Point { UL, UC, UR, 
		 CL, CC, CR, 
		 LL, LC, LR };

    // Returns and sets the "anchor point" (the point of the bounding
    // box placed on the <x, y> location specified in begin()).  This
    // can be changed anytime.  By default, the anchor point is CC
    // (the centre of the bounding box).
    Point anchor() const { return _anchor; }
    void setAnchor(Point p) { _anchor = p; }

    // Text can be set with an invisible (or, if you have a box,
    // visible) margin.  By default, with no box, the margin is zero.
    // Note that if you call setBoxed(true), the margin will be set to
    // pointSize / 5.0, clobbering whatever was there before.  If you
    // call setBox(false), the margin will be set to 0.0.
    float margin() const { return _margin; }
    void setMargin(float size) { _margin = size; }

    // The text can be drawn in a box.  The box is outlined and has a
    // background by default, but both of these can be turned off, and
    // their colours adjusted.  As well, the margin around the text
    // can be adjusted.  By default, the box is drawn with a
    // translucent white background, the margin is pointSize / 5.0
    // units (which means there must be a valid point size when it's
    // called), and the outline is drawn in the same colour as the
    // text.
    void setBoxed(bool boxed, bool background = true, bool outline = true);
    bool isBoxed() const { return _boxed; }
    bool hasBackground() const { return _hasBackground; }
    bool hasOutline() const { return _hasOutline; }

    const sgVec4& backgroundColour() const { return _backgroundColour; }
    const sgVec4& outlineColour() const { return _outlineColour; }

    void setBackgroundColour(const sgVec4& colour);
    void setOutlineColour(const sgVec4& colour);

    // Begin a layout session, with the anchor point (p) at <x, y>.
    void begin(float x, float y, Point p);
    // Begin a layout session at <x, y>, using the current anchor point.
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
    // the current line at the current x position.  Returns an
    // identifier for the box that can be used in a call to
    // nthChunk() to find out where it was typeset.
    int addBox(float width, float height, float x = 0.0, float y = 0.0);
    void newline();
    void end();

    // These are convenience routines - they do a begin(), addText(s),
    // and end().
    void setText(const std::string &s);
    void setText(const char *s);

    //----------------------------------------------------------------------
    // Do not call the following routines until end() has been called.
    // They all depend on the final bounding box having been
    // calculated, and this is not done until end() is called.

    // Get the width and height of the laid-out text.
    void size(float *width, float *height);
    float width() { return _textWidth + (_margin * 2.0); }
    float height() { return _textHeight + (_margin * 2.0); }

    // Returns the coordinate of the given point on the bounding box.
    float x(Point p = CC);
    float y(Point p = CC);

    // Adjust the text so that the anchor point is at <x, y>.
    void moveTo(float x, float y);
    // Adjust the text <x, y> and the anchor.
    void moveTo(float x, float y, Point p);

    // Get the lower-left corner of the nth chunk.
    void nthChunk(int n, float *x, float *y);
    //----------------------------------------------------------------------

    // Renders the text (but not generic boxes added via addBox()).
    void drawText();

  protected:
    // True if begin() has been called but end() has not.
    bool _layingOut;
    // Our metrics.  These are not valid until end() is called.
    // Internally, <_x, _y> always specifies the centre of the
    // bounding box (after end() has been called, that is).
    float _x, _y, _textWidth, _textHeight;
    // The anchor specifies how <_x, _y> should be adjusted when the
    // text is rendered.  By default, _anchor = CC (ie, <_x, _y>
    // specifies the centre of the bounding box).
    Point _anchor;
    // These return the distance from the anchor to the centre.
    float _deltaX();
    float _deltaY();

    atlasFntRenderer _renderer;

    // Current font properties.  Whenever addText() is called, these
    // are used to set the properties for that chunk of text.
    atlasFntTexFont *_font;
    float _pointSize, _italics;

    // Box properties.
    bool _boxed, _hasBackground, _hasOutline;
    bool _useNaturalOutlineColour;
    sgVec4 _backgroundColour, _outlineColour;
    float _margin;

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
