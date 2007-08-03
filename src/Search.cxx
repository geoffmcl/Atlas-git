/*-------------------------------------------------------------------------
  Search.cxx

  Written by Brian Schack, started July 2007.

  Copyright (C) 2007 Brian Schack

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

#include "Search.hxx"

// EYE - put in .hxx file?
void _list_cb(puObject *cb);
void _down_cb(puObject *cb);

Search::Search(int minx, int miny, int maxx, int maxy) :
    puGroup(minx, miny)
{
    // Initialize variables to reasonable default values.
    _lines = NULL;
    _numOfLines = 0;

    _cb = NULL;
    _select_cb = NULL;
    _input_cb = NULL;
    _size_cb = NULL;
    _data_cb = NULL;

    _top = _bottom = _above = _below = 0;
    _selected = -1;

    // The height of _input and _list will be adjusted later, in
    // _setSizes().  For now, just make _input 10 pixels high,
    // situated at the top, and make _list occupy the remainder.
    _input = new puInput(0, maxy - miny - 10, maxx - minx, maxy - miny);
    _input->setUserData(this);
    _input->setStyle(PUSTYLE_SMALL_SHADED);
    
    // EYE - What does this really do?
//     /* Share 'string' value with input box */
//     char *stringval ;
//     input->getValue(&stringval);
//     setValuator(stringval);

    _list = new puListBox(0, 0, maxx - minx, maxy - miny - 10);
    _list->setStyle(PUSTYLE_PLAIN);
    _list->setUserData(this);
    _list->setCallback(_list_cb);

    _setSizes();
    
    // Hack!  We want to know about all cases where one of our widgets
    // is deactivated.  Why?  So we can remove focus from the input
    // widget.  Normally this is handled automatically by PUI, which
    // assumes that an input widget with focus is the active widget.
    // However, we have special behaviour, where the input widget
    // accepts input when even when the list is active.
    _input->setDownCallback(_down_cb);
    _list->setDownCallback(_down_cb);
    this->setDownCallback(_down_cb);

    close();
}

// Returns the current search string.
char *Search::searchString()
{
    return _input->getStringValue();
}

// The user should call this when the data has changed.  We'll reload
// and redisplay (which means we'll call the size and data callbacks).
void Search::reloadData()
{
    int i;

    _setSelected(-1, true);
}

// Called when the user hits return.
void Search::setCallback(void(*cb)(Search *, int))
{
    _cb = cb;
}

// Called whenever the selection changes.
void Search::setSelectCallback(void(*cb)(Search *, int))
{
    _select_cb = cb;
}

// Called whenever the search string changes.
void Search::setInputCallback(void(*cb)(Search *, char *))
{
    _input_cb = cb;
}

// Called when we need to find out the size of the data array.
void Search::setSizeCallback(int(*cb)(Search *))
{
    _size_cb = cb;
}

// Called when we need to get an entry in the data array.
void Search::setDataCallback(char *(*cb)(Search *, int))
{
    _data_cb = cb;
}

// Sets the font, readjusts the input field, recalculates _numOfLines
// and redisplays the current data.
void Search::setFont(puFont font)
{
    _input->setLegendFont(font);
    _list->setLegendFont(font);
    _setSizes();
}

void Search::draw(int dx, int dy)
{
    if (!visible || (window != puGetWindow())) {
	return;
    }

    // EYE - why?
//     draw_label(dx, dy);

    puGroup::draw(dx, dy);
}

int Search::checkHit(int button, int updown, int x, int y)
{
    if (_input->checkHit(button, updown, x - abox.min[0], y - abox.min[1])) {
	return TRUE;
    } else if (puGroup::checkHit(button, updown, x, y)) {
	// The click is outside of the input field, but in our widget.
	// We force the input field to keep accepting input.
	_input->acceptInput();
	return TRUE;
    }

    // The click was outside of us, so turn off the input field.
    _input->rejectInput();

    return FALSE;
}

int Search::checkKey(int key, int updown)
{
    if (updown == PU_UP || 
	!_input->isAcceptingInput() || 
	!isVisible() || 
	!isActive() || 
	(window != puGetWindow())) {
	return FALSE;
    }

    switch (key) {
    case PU_KEY_HOME:
	_setSelected(0);
	break;
    case PU_KEY_END:
	if (_size_cb != NULL) {
	    _setSelected(_size_cb(this) - 1);
	}
	break;
    case PU_KEY_UP:
	_setSelected(_selected - 1);
	break;
    case PU_KEY_DOWN:
	_setSelected(_selected + 1);
	break;
    case PU_KEY_PAGE_UP:
	_setSelected(_selected - _realLines());
	break;
    case PU_KEY_PAGE_DOWN:
	if (_selected != -1) {
	    _setSelected(_selected + _realLines());
	} else {
	    // A special case.  If _selected == -1, that means nothing
	    // is selected.  Paging down by _realLines() would leave
	    // us at the end of the first "page", but we really want
	    // to move to the start of the second "page", so we add 1.
	    _setSelected(_selected + _realLines() + 1);
	}
	break;
    case 13:
	// Return.  We notify the user, then hide ourselves.
	if (_cb != NULL) {
	    if ((_selected == -1) && (_realLines() == 1)) {
		// Special case.  If there's only one match, then we
		// interpret a return to mean "I want that one",
		// regardless of whether it's selected or not.
		_cb(this, _top);
	    } else {
		_cb(this, _selected);
	    }
	}
	hide();
	break;
    case 27:
	// Escape.  We tell the user there's no selection, then hide
	// ourselves.
	if (_cb != NULL) {
	    _cb(this, -1);
	}
	hide();
	break;
    default:
	char *before = strdup(searchString());
	bool result = _input->checkKey(key, updown);
	// Hack!  PUI will "turn off" the input field if it isn't
	// active, but we want to force it to keep accepting input.
	// It also turns off the active widget, so we reset it.
	// EYE - is this true?
	_input->acceptInput();
	puSetActiveWidget(_list, 0, 0);
	if (result) {
	    if (strcmp(before, searchString()) != 0) {
		// The string has changed.  Tell the user.
		if (_input_cb != NULL) {
		    _input_cb(this, searchString());
		}
	    }
	}
	free(before);

	return result;
	break;
    }

    return TRUE;
}

void Search::reveal()
{
    puGroup::reveal();

    _input->acceptInput();
    puSetActiveWidget(_list, 0, 0);
}

// Called from callback function when there's a click in the list.  We
// use it to set our _selected variable.  We also undo the action if
// the user clicked outside the active list (ie, on the top or bottom
// lines when they indicate "%d more ...").
void Search::__list_cb()
{
    int selectedLine = _list->getIntegerValue();

    if (selectedLine < _top) {
	// Clicked the top line when it indicates "%d more ...".
	// Reselect the old value.
	_list->setValue(_selected - _above + _top);
    } else if (selectedLine >= _bottom) {
	// Clicked the bottom line when it indicates "%d more ...".
	// Reselect the old value.
	_list->setValue(_selected - _above + _top);
    } else {
	// Clicked on a real line.  If it's different than the
	// currently selected line, update _selected and inform the
	// user.
	if ((selectedLine + _above - _top) != _selected) {
	    _selected = selectedLine + _above - _top;
	    if (_select_cb != NULL) {
		_select_cb(this, _selected);
	    }
	}
    }
}

// A hook to be called from _down_cb only.
void Search::__rejectInput()
{
    _input->rejectInput();
}

// Sets the size of the input widget based on the current font,
// calculates how many lines will fit into the list widget, allocates
// space for the _lines array, and fills it in.
void Search::_setSizes()
{
    int x, y;
    int w, h;
    int lineSize = _input->getLegendFont().getStringHeight() + PUSTR_TGAP;

    getPosition(&x, &y);
    getSize(&w, &h);

    _input->setPosition(0, h - lineSize - PUSTR_TGAP);
    _input->setSize(w, lineSize + PUSTR_TGAP);

    _list->setPosition(0, 0);
    _list->setSize(w, h - lineSize - PUSTR_TGAP);

    // Free old array.
    int i;
    for (i = 0; i < _numOfLines; i++) {
	free(_lines[i]);
    }
    free(_lines);

    // Create a new one.
    _numOfLines = (h - lineSize - PUSTR_BGAP) / lineSize;
    _lines = (char **)malloc(sizeof(char *) * (_numOfLines + 1));
    for (int i = 0; i <= _numOfLines; i++) {
	_lines[i] = NULL;
    }

    // Fill it in.
    reloadData();
}

// Sets _selected to the given value, updating the list value (the
// selected item) and calling the callback if required.  Note that if
// _selected is -1 when we're called that means that we're in the
// "parked" position.  In that situation, we're allowed to move from
// -1 to any in-range value.  We're never allowed to move from an
// in-range value to -1 (or any other out-of-range value).
void Search::_setSelected(int s, bool reset)
{
    int size;

    if ((_size_cb == NULL) || (_data_cb == NULL)) {
	return;
    }

    size = _size_cb(this);
    if (reset) {
	s = -1;
    } else {
	if (s < 0) {
	    s = 0;
	} else if (s >= size) {
	    s = size - 1;
	}	

	if (s == _selected) {
	    // Nothing to do.
	    return;
	}
    }

    // Make sure the currently displayed window of data contains the
    // current selection.
    _selected = s;
    if (!_inRange(_selected)) {
	// We're not in range, so shift the window.  We prefer to have
	// the selection in the center, except when we're at the
	// beginning or end, in which case we prefer to get rid of the
	// header or the footer.

	// Now go through the four cases.
	if (size <= _numOfLines) {
	    // No header or footer.
	    _top = 0;
	    _bottom = size;

	    _above = 0;
	    _below = 0;
	} else if (_selected < (_numOfLines - 1)) {
	    // No header needed.
	    _top = 0;
	    _bottom = _numOfLines - 1;

	    _above = 0;
	    _below = size - _bottom;
	} else if (_selected > (size - _numOfLines)) {
	    // No footer needed.
	    _top = 1;
	    _bottom = _numOfLines;

	    _above = size - _realLines();
	    _below = 0;
	} else {
	    // Header and footer needed.  Center on _selected.
	    _top = 1;
	    _bottom = _numOfLines - 1;

	    _above = _selected - (_realLines() / 2);
	    _below = size - _above - (_numOfLines - 2);
	}

	// Set our string array and tell _list that it has changed.
	int i = 0;
	if (_top > 0) {
	    // Header.
	    char *entry;

	    free(_lines[i]);
	    asprintf(&entry, "%d more ...", _above);
	    _lines[i] = entry;
	}
	for (i = _top; i < _bottom; i++) {
	    // Strings.  We don't copy the string data, but we are
	    // owners of it, and free it when we're finished.
	    free(_lines[i]);
	    _lines[i] = _data_cb(this, i + _above - _top);
	}
	for (; i < _numOfLines - 1; i++) {
	    // Blank lines.
	    free(_lines[i]);
	    _lines[i] = NULL;
	}
	if (_below > 0) {
	    // Footer.
	    char *entry;

	    free(_lines[i]);
	    asprintf(&entry, "%d more ...", _below);
	    _lines[i] = entry;
	}

	_list->newList(_lines);
    }

    // Set the selection in _list.
    if ((s >= 0) && (s < size)) {
	_list->setValue(_selected - _above + _top);
    }

    // Tell the user the selection has changed.
    if (_select_cb != NULL) {
	_select_cb(this, _selected);
    }
}

// Returns true if the value (representing an index into the data
// array, not _list) is being currently displayed in _list.
bool Search::_inRange(int s)
{
    int size;

    if (_size_cb == NULL) {
	return false;
    }

    size = _size_cb(this);
    if (s < _above) {
	return false;
    }
    if (s >= (size - _below)) {
	return false;
    }

    return true;
}

// Returns the number of "real" lines (ie, everything but the header
// and footer).  Depends on _bottom and _top being set correctly.
int Search::_realLines()
{
    return (_bottom - _top);
}

// This is called when the user clicks in the list.  It's merely a
// convenience function that passes the message in to the class.
void _list_cb(puObject *cb)
{
    Search *me = (Search *)cb->getUserData();
    me->__list_cb();
}

// Major hackiness.  This is called whenever a widget is deactivated.
// We tell the input field to reject all input, just in case we're
// leaving our Search widget altogether.  If not, another routine
// (either checkKey or checkHit) will turn it back on again.  There
// must be a better way!?
void _down_cb(puObject *cb)
{
    Search *me = (Search *)cb->getUserData();
    me->__rejectInput();
}
