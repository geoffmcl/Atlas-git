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

// Callback for drawing "boxes".  A box is a bit of reserved space on
// line that can contain anything.  When drawing text, the layout
// manager, when encountering a box, will call its callback, at which
// point you are expected to do some drawing.  You will be given the
// LayoutManager, the current position on the line (y will be on the
// baseline), and some user data which you can set when you call
// addBox().
//
// When called, positive x points to the right, positive y points up,
// and z = 0.  The current font metrics can be retrived via the given
// layout manager.
typedef void (*lmCallback)(class LayoutManager *lm, float x, float y, 
			   void *userData);

class LayoutManager {
  public:
    LayoutManager();
    // These constructors makes it easy to lay out a single string.
    // They call begin() and end().  The text is centred at 0,0.
    LayoutManager(const std::string& s, atlasFntTexFont *f, float pointSize);
    LayoutManager(const char *s, atlasFntTexFont *f, float pointSize);
    ~LayoutManager();

    // The text can be drawn in a box.  By default, the box is
    // outlined in the text colour, has a translucent white
    // background, and has a margin of pointSize / 5.0 (which means
    // there must be a valid point size when it's called).  
    //
    // You can call setBoxed() multiple times, but all layout
    // calculations are done when end() is called, using the current
    // layout metrics (including margin) - changes to the margin
    // between end() and drawText() will not affect text layout, but
    // *will* affect box layout, and so is not recommended.  Changes
    // to the background and outline will be effective anytime before
    // drawText().
    void setBoxed(bool boxed, bool background = true, bool outline = true);
    bool isBoxed() const { return _boxed; }
    bool hasBackground() const { return _hasBackground; }
    bool hasOutline() const { return _hasOutline; }

    const sgVec4& backgroundColour() const { return _backgroundColour; }
    const sgVec4& outlineColour() const { return _outlineColour; }
    void setBackgroundColour(const sgVec4& colour);
    void setOutlineColour(const sgVec4& colour);

    // By default, the bounding box is the smallest box that surrounds
    // the text.  Setting a margin expands that bounding box.  Margins
    // are generally used only with boxes, but you can set a margin
    // even without a box.  Note that calls to setBox() will clobber
    // any previously set margins; setBox(true) sets the margin to
    // pointSize / 5.0, and setBox(false) sets the margin to 0.0.
    float margin() const { return _margin; }
    void setMargin(float size) { _margin = size; }

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

    // Set the font to the given PLIB font renderer.  Each time
    // addText() is called, the text is rendered using the most recent
    // font settings.  These can be called before a layout session,
    // and multiple times during a layout session, so that different
    // runs of text can have different font characteristics.
    void setFont(atlasFntTexFont *f, float pointSize, float italics = 0.0);
    void setFont(atlasFntTexFont *f) { _font = f; }
    void setPointSize(float pointSize) { _pointSize = pointSize; }
    void setItalics(float italics) { _italics = italics; }

    // These return the current font settings.
    atlasFntTexFont *font() const { return _font; }
    float pointSize() const { return _pointSize; }
    float italics() const { return _italics; }

    // Begin a layout session with <x, y> placed at the anchor point
    // p.
    void begin(float x, float y, Point p);
    // Begin a layout session with <x, y> placed at the current anchor
    // point.
    void begin(float x, float y);
    // Begin a layout session with current x, y, and anchor.
    void begin();

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
    // left-hand corner on the current baseline at the end of the
    // previous chunk.  When the layout manager encounters this box
    // when drawing the text, it will call the given callback, passing
    // itself, the current position, and the given user data.
    void addBox(float width, float height, lmCallback cb, 
		void *userData = NULL);

    // Start a new line.
    void newline();

    void end();

    // These are convenience routines - they do a begin(), addText(s),
    // and end().  You *must* set a font before calling these.
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
    //----------------------------------------------------------------------

    // Returns the offset from the origin to the current anchor.
    float x() { return _x; }
    float y() { return _y; }
    // Returns the offset from the origin to the given point.
    float x(Point p);
    float y(Point p);

    // Adjust the text so that the current anchor point is at <x, y>.
    void moveTo(float x, float y);
    // Adjust the text <x, y> and the anchor.
    void moveTo(float x, float y, Point p);

    // Renders the text (but not generic boxes added via addBox()).
    void drawText();

  protected:
    // Does initialization common to all constructors.
    void _init();

    // These return the distance from the point on the bounding box to
    // the centre.
    float _deltaX(Point p);
    float _deltaY(Point p);

    // True if begin() has been called but end() has not.
    bool _layingOut;
    // These are not valid until end() is called.
    float _textWidth, _textHeight;
    // Offset from the origin to the anchor.
    float _x, _y;
    Point _anchor;

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

    // A chunk is a block that is added to a line.  It has a location
    // and occupies some space.  The <x, y> location refers to the
    // baseline of the left side of the block.  If descent < 0.0, then
    // the chunk hangs below the line.  The total height of the chunk
    // is ascent - descent.
    class Chunk {
      public:
	virtual ~Chunk() {}
	float x, y, width, ascent, descent;
    };
    // A text chunk is, not surprisingly, a chunk containing text.
    class TextChunk: public Chunk {
      public:
	std::string s;
	atlasFntTexFont *f;
	float pointSize, italics;
    };
    // A generic chunk can contain anything.  To render it, the layout
    // manager calls its callback.
    class GenericChunk: public Chunk {
      public:
	lmCallback cb;
	void *userData;
    };

    // A line is a horizontal row of chunks, aligned along a common
    // baseline.
    class Line {
      public:
	float x, y, width, ascent, descent;
	std::vector<Chunk *> chunks;
    };

    Line _currentLine;
    std::vector<Line> _lines;

    // Add a newly created chunk to the current line.
    void _addChunk(Chunk *chunk);

    // This keeps track of all allocated chunks.  When the layout
    // manager destructor is called, we delete them.  Is it a stupid
    // way to do things?  Probably, but it works.
    std::vector<Chunk *> _allChunks;
};

#endif
