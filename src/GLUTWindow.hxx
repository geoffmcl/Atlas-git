/*-------------------------------------------------------------------------
  GLUTWindow.hxx

  Written by Brian Schack

  Copyright (C) 2012 Brian Schack

  A GLUTWindow class can manage a single GLUT window.  Callbacks to
  the window go to methods in the class, which should make it easier
  to manage multiple GLUT windows.  Each window can have its own set
  of timers, which are started via the startTimer() method.  For
  callback methods and timer methods, GLUTWindow ensures that the
  window is the current GLUT window.

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

#ifndef _GLUTWINDOW_H_
#define _GLUTWINDOW_H_

#include <vector>
#include <map>
#include <set>

class GLUTMenu;
class GLUTWindow {
  public:
    GLUTWindow(const char *title);
    virtual ~GLUTWindow();

    // Set the window title.  This can safely be called (as can all
    // these methods) at any time - no need to check whether we are
    // the current GLUT window.
    void setTitle(const char *title);
    // Resize the window.
    void reshape(int width, int height);
    // Start a timer.  After the given interval, call the given
    // callback method.
    typedef void (GLUTWindow::*cb)();
    void startTimer(unsigned int msecs, cb method);

    // EYE - don't make virtual?
    virtual void reveal();
    virtual void hide();
    bool isVisible() { return _visible; }

    // Our GLUT window id.
    int id() { return _id; }

    // Set the new current GLUT window to id, returning the id of the
    // previous GLUT window.
    static int set(int id);
    // Set ourselves to be the new current window, returning the
    // window id of the previous window.
    int set() { return set(_id); }

    // Mark the given window as being ready for redisplay.
    static void postRedisplay(int id);
    // Mark us as being ready for redisplay.
    void postRedisplay() { postRedisplay(_id); }

    // Find the GLUTWindow with the given id, NULL if none exists.
    static GLUTWindow *windowWithID(int id);

    // Our window width and height.
    int width();
    int height();

    // Attach and detach a menu to/from the given button (one of
    // GLUT_LEFT_BUTTON, GLUT_MIDDLE_BUTTON, or GLUT_RIGHT_BUTTON).
    void attach(int button, GLUTMenu *menu);
    void detach(int button);

  protected:
    // We make the callbacks protected so we can always assume that
    // our OpenGL context is current.  If you want others to be able
    // to call them, create a public method that sets the window
    // current, calls the corresponding protected method, then
    // restores the previous current window.

    // EYE - do more of the callbacks (idleFunc, entryFunc)?
    virtual void _display() {};
    virtual void _reshape(int w, int h) {};
    virtual void _mouse(int button, int state, int x, int y) {};
    virtual void _motion(int x, int y) {};
    virtual void _passiveMotion(int x, int y) {};
    virtual void _keyboard(unsigned char key, int x, int y) {};
    virtual void _special(int key, int x, int y) {};
    virtual void _visibility(int state) {};

    // Static callbacks.  All of them when called use __getWindow() to
    // get the GLUTWindow, then call the corresponding instance
    // method.
    static void __displayFunc();
    static void __reshapeFunc(int w, int h);
    static void __mouseFunc(int button, int state, int x, int y);
    static void __motionFunc(int x, int y);
    static void __passiveMotionFunc(int x, int y);
    static void __keyboardFunc(unsigned char key, int x, int y);
    static void __specialFunc(int key, int x, int y);
    static void __visibilityFunc(int state);

    // Our window id.
    int _id;

    // Our visibility.  We track it ourselves because GLUT has no
    // ability to query a window's visibility.
    bool _visible;

    ////////// Window management
    // Map from a window id (given as an int in GLUT) to a GLUTWindow.
    static std::map<int, GLUTWindow *> __instanceMap;
    // Lookup function for a GLUTWindow using the current GLUT window
    // id (as given by glutGetWindow()).
    static GLUTWindow *__getWindow();

    ////////// Timer management
    // A map from an id (passed to the GLUT timer) to a GLUTWindow
    // instance and a method.
    static std::map<int, std::pair<GLUTWindow *, cb> > __activeTimers;
    // The set of all active timer ids.  We check this when generating
    // ids for new timers.
    static std::set<int> __activeTimerIDs;
    // Called by GLUT when a timer fires.  We use 'id' to tell us
    // which timer it is.
    static void __timerFunc(int id);
};

class GLUTMenu {
  public:
    GLUTMenu();
    virtual ~GLUTMenu();
    
    // Our GLUT menu id.
    int id() { return _menuID; }

    // Adds a menu entry.  The given callback method will be called
    // when the item is selected.
    typedef void (GLUTMenu::*cb)();
    void addItem(const char *label, cb method);
    // Adds the given submenu.  We take ownership of the subment, and
    // will deleted it when required.
    void addSubMenu(const char *label, GLUTMenu *subMenu);

    // EYE - we should probably add methods to allow interrogation of
    // the menu, add items at certain indices, delete individual
    // items, change callbacks and labels, etc.

    // Clears all entries (including submenus).
    void clear();

  protected:
    int _menuID;		   // Our GLUT menu id.
    std::vector<int> _menuItemIDs;     // GLUT IDs of all our menu items.
    std::vector<GLUTMenu *> _subMenus; // All of our immediate submenus.

    // In GLUT, when a menu item is selected, the menu's callback is
    // called with the menu item's id.  Since we can't create an
    // arbitrary number of menu callback functions, we just have one
    // static callback for all instances of this class.  We ensure
    // that each menu item (over all menus) has a unique id.  When the
    // callback is called, we use the id to get a <GLUTMenu *,
    // callback> pair, then call it.
    static std::map<int, std::pair<GLUTMenu *, cb> > __menuItems;
    // The set of all menu item ids.  We check this when generating
    // ids for new menu items.
    static std::set<int> __menuItemIDs;
    // Called by GLUT when a menu item is selected.  We use 'id' to
    // tell us which menu item it was.
    static void __menuFunc(int id);
};

#endif
