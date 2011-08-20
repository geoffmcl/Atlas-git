/*-------------------------------------------------------------------------
  Search.hxx

  Written by Brian Schack, started July 2007.

  Copyright (C) 2007 - 2011 Brian Schack

  Implements a widget used for text-based searching.

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

#ifndef _SEARCH_H_
#define _SEARCH_H_

#include <plib/pu.h>

class Search : public puGroup {
public:
    Search(int minx, int miny, int maxx, int maxy);

    // Returns the contents of the input field.
    char *searchString();

    // The user calls this to tell us when the data array changes.
    void reloadData();

    // Called when the user makes their final selection (by hitting
    // return or escape).  Passes the index (in the user's array) of
    // that selection.  -1 means there was no selection.
    void setCallback(void(*cb)(Search *, int));
    // Called whenever the selection changes.
    void setSelectCallback(void(*cb)(Search *, int));
    // Called whenever the search string changes.  The user should
    // make a copy of the string if they want to use it.
    void setInputCallback(void(*cb)(Search *, const char *));
    // Called whenever we need to know the size of the user's data
    // array.
    void setSizeCallback(int(*cb)(Search *));
    // Called when we need an element of the array.  We are
    // responsible for the string returned, and will free it when
    // we're done with it.
    void setDataCallback(char *(*cb)(Search *, int));

    void setFont(puFont font);

    void draw(int dx, int dy) ;
    int checkHit(int button, int updown, int x, int y);
    int checkKey(int key, int updown);

    void reveal();

    // Not to be called by the user.
    void __list_cb();
    void __rejectInput();
protected:
    char **_lines;
    int _numOfLines;

    puInput *_input;
    puListBox *_list;

    void(*_cb)(Search *, int);
    void(*_select_cb)(Search *, int);
    void(*_input_cb)(Search *, const char *);
    int(*_size_cb)(Search *);
    char *(*_data_cb)(Search *, int);

    // _top is the index of the top line of "real" text IN _LIST (not
    // in the data array).  It will be either 0 or 1.  _bottom is the
    // index of the last line of "real" text PLUS ONE.  It will be
    // _numOfLines or less.
    int _top, _bottom;

    // _selected is the index of the currently selected item IN THE
    // DATA ARRAY (not in _list).  _above and _below give the number
    // of items in the data array above and below the currently
    // displayed set.
    int _selected, _above, _below;

    void _setSizes();
    void _setSelected(int s, bool reset = false);
    bool _inRange(int s);
    int _realLines();
};

#endif	// _SEARCH_H_
