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

using namespace std;

#include "LayoutManager.hxx"

LayoutManager::LayoutManager(): _italics(0.0), _noOfChunks(0)
{
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

    _currentLine.width = _currentLine.ascent = _currentLine.descent = 0.0;
    _currentLine.chunks.clear();

    _lines.clear();
    _noOfChunks = 0;
    _chunkMap.clear();
}

void LayoutManager::setFont(fntRenderer &f, float pointSize, float italics)
{
    _f = &f;
    _pointSize = pointSize;
    _italics = italics;
}

void LayoutManager::addText(const string &s)
{
    addText(s.c_str());
}

void LayoutManager::addText(const char *s)
{
    float left, right, bottom, top, width;
    TextChunk *chunk;

    if ((s == (char *)NULL) || (strlen(s) == 0)) {
	return;
    }

    // The bounding box doesn't tell us about character origins, so
    // it's best to add as a big a chunk of text as possible, letting
    // PLIB correctly calculate character spacing.
    _f->getFont()->getBBox(s, _pointSize, 0.0, &left, &right, &bottom, &top);
    width = right - left;

    chunk = new TextChunk;
    chunk->s = s;
    // EYE - cast!
    chunk->f = (atlasFntTexFont *)_f->getFont();
    chunk->pointSize = _pointSize;
    chunk->italics = _italics;

    chunk->x = _currentLine.width;
    chunk->width = width;
    chunk->ascent = chunk->f->ascent() * _pointSize;
    chunk->descent = chunk->f->descent() * _pointSize;

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
    
    // Now adjust all the lines.
    _height = _width = 0;
    for (unsigned int i = 0; i < _lines.size(); i++) {
	Line &l = _lines[i];
	_height += l.ascent - l.descent;
	if (l.width > _width) {
	    _width = l.width;
	}
    }

    float curY = _y - (_height / 2.0);
    for (int i = _lines.size() - 1; i >= 0; i--) {
	Line &l = _lines[i];

	float indent = (_width - l.width) / 2.0;
	l.x = indent + _x - (_width / 2.0);
	l.y = curY - _lines[i].descent;
	curY += l.ascent - l.descent;
	for (unsigned int j = 0; j < l.chunks.size(); j++) {
	    l.chunks[j]->x += l.x;
	    l.chunks[j]->y = l.y;
	}
    }    
}

void LayoutManager::size(float *width, float *height)
{
    *width = _width;
    *height = _height;
}

void LayoutManager::moveTo(float x, float y)
{
    float incX = x - _x, incY = y - _y;
    
    _x = x;
    _y = y;
    for (unsigned int i = 0; i < _lines.size(); i++) {
	Line &l = _lines[i];
	l.x += incX;
	l.y += incY;

	for (unsigned int j = 0; j < l.chunks.size(); j++) {
	    Chunk *c = l.chunks[j];
	    c->x += x;
	    c->y += y;
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
		// EYE - set font?
		_f->setPointSize(c->pointSize);
		_f->setFont(c->f);
		_f->setSlant(c->italics);
		_f->start3f(c->x, l.y, 0.0);
		_f->puts(c->s.c_str());
	    }
	}
    }    
}
