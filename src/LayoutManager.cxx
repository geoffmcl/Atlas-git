/*-------------------------------------------------------------------------
  LayoutManager.cxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

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

#include <cassert>

#include "LayoutManager.hxx"

using namespace std;

// EYE - make a common constructor?
LayoutManager::LayoutManager(): _anchor(CC), _font(NULL), _italics(0.0), 
				_boxed(false), _margin(0.0), _noOfChunks(0)
{
    sgSetVec4(_backgroundColour, 1.0, 1.0, 1.0, 0.5);
}

// Lay out a single string, using the given font and point size.
LayoutManager::LayoutManager(const std::string& s, atlasFntTexFont *f, 
			     float pointSize):
    _anchor(CC), _font(f), _pointSize(pointSize), _italics(0.0), _boxed(false),
    _margin(0.0), _noOfChunks(0)
{
    sgSetVec4(_backgroundColour, 1.0, 1.0, 1.0, 0.5);
    begin();
    addText(s);
    end();
}

LayoutManager::LayoutManager(const char *s, atlasFntTexFont *f, 
			     float pointSize):
    _anchor(CC), _font(f), _pointSize(pointSize), _italics(0.0), _boxed(false),
    _margin(0.0), _noOfChunks(0)

{
    sgSetVec4(_backgroundColour, 1.0, 1.0, 1.0, 0.5);
    begin();
    addText(s);
    end();
}

LayoutManager::~LayoutManager()
{
    // A bit counter-intuitive, but the begin() method clears
    // everything out, just the way we want.
    begin(0.0, 0.0);
}

void LayoutManager::setBoxed(bool boxed, bool background, bool outline)
{
    _boxed = boxed;
    _hasBackground = background;
    _hasOutline = outline;

    // By default, we draw the outline in the "natural" outline
    // colour, which is the whatever colour we use to draw the text
    // (ie, the colour current at the time when drawText() is called).
    // EYE - what if setBoxed() is called multiple times?
    _useNaturalOutlineColour = true;

    // EYE - we need to set some rules about what can be done when.
    if (_boxed) {
	_margin = _pointSize / 5.0;
    } else {
	_margin = 0.0;
    }
}

void LayoutManager::setBackgroundColour(const sgVec4& colour)
{
    sgCopyVec4(_backgroundColour, colour);
}

void LayoutManager::setOutlineColour(const sgVec4& colour)
{
    sgCopyVec4(_backgroundColour, colour);
    _useNaturalOutlineColour = false;
}


void LayoutManager::begin(float x, float y, Point p)
{
    _layingOut = true;
    _x = x;
    _y = y;
    _anchor = p;

    assert(_noOfChunks == _chunkMap.size());
    for (unsigned int i = 0; i < _chunkMap.size(); i++) {
	delete _chunkMap[i];
    }

    // Each line initially has its lower-left corner at <0.0, 0.0>.
    // When we finish (when end() is called), we'll adjust all the
    // lines so that the entire layout is centred at <_x, _y>, and
    // each line is individually centre justified.
    _currentLine.x = _currentLine.y = 0.0;
    _currentLine.width = _currentLine.ascent = _currentLine.descent = 0.0;
    _currentLine.chunks.clear();

    _lines.clear();
    _noOfChunks = 0;
    _chunkMap.clear();
}

void LayoutManager::begin(float x, float y)
{
    begin(x, y, _anchor);
}

void LayoutManager::setFont(atlasFntTexFont *f, float pointSize, float italics)
{
    _font = f;
    _pointSize = pointSize;
    _italics = italics;
}

void LayoutManager::addText(const string &s, float x, float y)
{
    addText(s.c_str(), x, y);
}

void LayoutManager::addText(const char *s, float x, float y)
{
    float left, right, bottom, top, width;
    TextChunk *chunk;

    if ((s == (char *)NULL) || (strlen(s) == 0)) {
	return;
    }

    // The bounding box doesn't tell us about character origins, so
    // it's best to add as a big a chunk of text as possible, letting
    // PLIB correctly calculate character spacing.
    assert(_font != NULL);
    _font->getBBox(s, _pointSize, 0.0, &left, &right, &bottom, &top);
    width = right - left;

    chunk = new TextChunk;
    chunk->s = s;
    chunk->f = _font;
    chunk->pointSize = _pointSize;
    chunk->italics = _italics;

    chunk->x = _currentLine.width + x;
    chunk->y = _currentLine.y + y;
    chunk->width = width;
    chunk->ascent = chunk->f->ascent() * _pointSize + y;
    chunk->descent = chunk->f->descent() * _pointSize - y;

    _currentLine.chunks.push_back(chunk);
    _chunkMap[_noOfChunks] = chunk;
    _noOfChunks++;
    // EYE - make this a method, add padding?  Or don't do at all here?
    _currentLine.width += width;
    if (_currentLine.ascent < chunk->ascent) {
	_currentLine.ascent = chunk->ascent;
    }
    if (_currentLine.descent > chunk->descent) {
	_currentLine.descent = chunk->descent;
    }
}

// Adds a box of the given width and height, with its lower left-hand
// corner at x, y (where x is relative to the end of the previous
// chunk on the line, and y is relative to the baseline of the current
// line).
int LayoutManager::addBox(float width, float height, float x, float y)
{
    Chunk *chunk = new Chunk;

    chunk->x = _currentLine.width + x;
    chunk->ascent = height + y;
    chunk->descent = -y;

    _currentLine.chunks.push_back(chunk);
    _chunkMap[_noOfChunks] = chunk;
    _noOfChunks++;
    // EYE - make this a method, add padding?  Or don't do at all here?
    _currentLine.width += width;

    return (_noOfChunks - 1);
}

void LayoutManager::newline()
{
    _lines.push_back(_currentLine);

    _currentLine.width = _currentLine.ascent = _currentLine.descent = 0;
    _currentLine.chunks.clear();
}

void LayoutManager::end()
{
    // Add remaining line.
    _lines.push_back(_currentLine);

    _layingOut = false;
    
    _textHeight = _textWidth = 0;
    for (unsigned int i = 0; i < _lines.size(); i++) {
	Line &l = _lines[i];
	_textHeight += l.ascent - l.descent;
	if (l.width > _textWidth) {
	    _textWidth = l.width;
	}
    }

    // Now adjust all the lines so that our centre is at <_x, _y> and
    // each line is centre justified.
    float deltaX = _x - (_textWidth / 2.0);
    float deltaY = _y + (_textHeight / 2.0);
    for (size_t i = 0; i < _lines.size(); i++) {
	Line &l = _lines[i];
	deltaY -= l.ascent;

	float indent = (_textWidth - l.width) / 2.0;
	l.x += deltaX + indent; 
	l.y += deltaY;
	for (unsigned int j = 0; j < l.chunks.size(); j++) {
	    l.chunks[j]->x += deltaX + indent;
	    l.chunks[j]->y += deltaY;
	}

	deltaY += l.descent;
    }
}

void LayoutManager::setText(const std::string &s)
{
    begin();
    addText(s);
    end();
}

void LayoutManager::setText(const char *s)
{
    begin();
    addText(s);
    end();
}

void LayoutManager::size(float *width, float *height)
{
    *width = LayoutManager::width();
    *height = LayoutManager::height();
}

// Return the x coordinate at the given point on the bounding box.
float LayoutManager::x(Point p)
{
    return _x + _deltaX();
}

// Return the y coordinate at the given point on the bounding box.
float LayoutManager::y(Point p)
{
    return _y + _deltaY();
}

void LayoutManager::moveTo(float x, float y)
{
    moveTo(x, y, _anchor);
}

void LayoutManager::moveTo(float x, float y, Point p)
{
    _anchor = p;

    float incX = x - _x, incY = y - _y;
    _x = x;
    _y = y;
    for (unsigned int i = 0; i < _lines.size(); i++) {
	Line &l = _lines[i];
	l.x += incX;
	l.y += incY;

	for (unsigned int j = 0; j < l.chunks.size(); j++) {
	    Chunk *c = l.chunks[j];
	    c->x += incX;
	    c->y += incY;
	}
    }
}

void LayoutManager::nthChunk(int n, float *x, float *y)
{
    Chunk *c = _chunkMap[n];
    if (c) {
	*x = c->x + _deltaX();
	*y = c->y + _deltaY();
    } else {
	*x = 0.0;
	*y = 0.0;
    }
}

// Render the text.
void LayoutManager::drawText()
{
    float width_2 = width() / 2.0, height_2 = height() / 2.0;;
    float deltaX = _deltaX(), deltaY = _deltaY();

    if (_boxed && _hasBackground) {
	// Draw background.
	glPushAttrib(GL_CURRENT_BIT); { // For current colour
	    float x = _x + deltaX;
	    float y = _y + deltaY;
	    glColor4fv(_backgroundColour);
	    glBegin(GL_QUADS); {
		glColor4f(1.0, 1.0, 1.0, 0.5);
		glVertex2f(x - width_2, y - height_2);
		glVertex2f(x + width_2, y - height_2);
		glVertex2f(x + width_2, y + height_2);
		glVertex2f(x - width_2, y + height_2);
	    }
	    glEnd();
	}
	glPopAttrib();
    }

    // Draw text.
    for (unsigned int i = 0; i < _lines.size(); i++) {
	Line &l = _lines[i];
	for (unsigned int j = 0; j < l.chunks.size(); j++) {
	    TextChunk *c = dynamic_cast<TextChunk *>(l.chunks[j]);
	    if (c) {
		_renderer.setPointSize(c->pointSize);
		_renderer.setFont(c->f);
		_renderer.setSlant(c->italics);
		_renderer.start3f(c->x + deltaX, c->y + deltaY, 0.0);
		_renderer.puts(c->s.c_str());
	    }
	}
    }    

    if (_boxed && _hasOutline) {
	// Draw outline.
	glPushAttrib(GL_CURRENT_BIT); { // For current colour
	    if (!_useNaturalOutlineColour) {
		glColor4fv(_outlineColour);
	    }
	    float x = _x + deltaX;
	    float y = _y + deltaY;
	    glBegin(GL_LINE_LOOP); {
		glVertex2f(x - width_2, y - height_2);
		glVertex2f(x + width_2, y - height_2);
		glVertex2f(x + width_2, y + height_2);
		glVertex2f(x - width_2, y + height_2);
	    }
	    glEnd();
	}
	glPopAttrib();
    }
}

float LayoutManager::_deltaX()
{
    float width_2 = width() / 2.0;
    float result;
    if ((_anchor == UL) || (_anchor == CL) || (_anchor == LL)) {
	result = width_2;
    } else if ((_anchor == UR) || (_anchor == CR) || (_anchor == LR)) {
	result = -width_2;
    } else {
	result = 0.0;
    }

    return result;
}

float LayoutManager::_deltaY()
{
    float height_2 = height() / 2.0;
    float result;
    if ((_anchor == UL) || (_anchor == UC) || (_anchor == UR)) {
	result = -height_2;
    } else if ((_anchor == LL) || (_anchor == LC) || (_anchor == LR)) {
	result = height_2;
    } else {
	result = 0.0;
    }

    return result;
}
