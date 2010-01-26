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

LayoutManager::LayoutManager(): _font(NULL), _italics(0.0), _noOfChunks(0)
{
}

// Lay out a single string, using the given font and point size.
LayoutManager::LayoutManager(const std::string &s, atlasFntTexFont *f, 
			     float pointSize):
    _font(f), _pointSize(pointSize), _italics(0.0), _noOfChunks(0)
{
    begin();
    addText(s);
    end();
}

LayoutManager::LayoutManager(const char *s, atlasFntTexFont *f, 
			     float pointSize):
    _font(f), _pointSize(pointSize), _italics(0.0), _noOfChunks(0)
{
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

void LayoutManager::begin(float x, float y)
{
    _layingOut = true;
    _x = x;
    _y = y;

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
    
    // Find out the extents of our box.
    _height = _width = 0;
    for (unsigned int i = 0; i < _lines.size(); i++) {
	Line &l = _lines[i];
	_height += l.ascent - l.descent;
	if (l.width > _width) {
	    _width = l.width;
	}
    }

    // Now adjust all the lines so that our centre is at <_x, _y> and
    // each line is centre justified.
    float deltaX = _x - (_width / 2.0);
    float deltaY = _y + (_height / 2.0);
    for (size_t i = 0; i < _lines.size(); i++) {
	Line &l = _lines[i];
	deltaY -= l.ascent;

	float indent = (_width - l.width) / 2.0;
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
    *width = _width;
    *height = _height;
}

// Return the x coordinate at the given point on the bounding box.
float LayoutManager::x(Point p)
{
    if ((p == UL) || (p == CL) || (p == LL)) {
	return _x - _width / 2.0;
    } else if ((p == UR) || (p == CR) || (p == LR)) {
	return _x + _width / 2.0;
    } else {
	return _x;
    }
}

// Return the y coordinate at the given point on the bounding box.
float LayoutManager::y(Point p)
{
    if ((p == UL) || (p == UC) || (p == UR)) {
	return _y + _height / 2.0;
    } else if ((p == LL) || (p == LC) || (p == LR)) {
	return _y - _height / 2.0;
    } else {
	return _y;
    }
}

void LayoutManager::moveTo(float x, float y, Point p)
{
    float incX = x - _x, incY = y - _y;
    
    if ((p == UL) || (p == CL) || (p == LL)) {
	incX += _width / 2.0;
    } else if ((p == UR) || (p == CR) || (p == LR)) {
	incX -= _width / 2.0;
    }
    if ((p == UL) || (p == UC) || (p == UR)) {
	incY -= _height / 2.0;
    } else if ((p == LL) || (p == LC) || (p == LR)) {
	incY += _height / 2.0;
    }

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
	*x = c->x;
	*y = c->y;
    } else {
	*x = 0.0;
	*y = 0.0;
    }
}

void LayoutManager::drawText()
{
    for (unsigned int i = 0; i < _lines.size(); i++) {
	Line &l = _lines[i];
	for (unsigned int j = 0; j < l.chunks.size(); j++) {
	    TextChunk *c = dynamic_cast<TextChunk *>(l.chunks[j]);
	    if (c) {
		_renderer.setPointSize(c->pointSize);
		_renderer.setFont(c->f);
		_renderer.setSlant(c->italics);
		_renderer.start3f(c->x, c->y, 0.0);
		_renderer.puts(c->s.c_str());
	    }
	}
    }    
}
