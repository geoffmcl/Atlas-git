/*-------------------------------------------------------------------------
  GLUTWindow.cxx

  Written by Brian Schack

  Copyright (C) 2012 - 2014 Brian Schack

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

// Our include file
#include "GLUTWindow.hxx"

// C system files
#include <stdlib.h>

// C++ system files
#include <assert.h>

// Other libraries' include files
#if defined( __APPLE__)
#  include <GLUT/glut.h>
#else
#  ifdef WIN32
#    include <windows.h>
#  endif
#  include <GL/glut.h>
#endif

using namespace std;

// Class methods
int GLUTWindow::set(int id)
{
    int oldID = glutGetWindow();
    // Switch to the new window if it's different than this one (and
    // if it's a legal GLUT window id).
    if ((oldID != id) && (id != 0)) {
	glutSetWindow(id);
    }
    return oldID;
}

void GLUTWindow::postRedisplay(int id)
{
    glutPostWindowRedisplay(id);
}

GLUTWindow *GLUTWindow::windowWithID(int id)
{
    GLUTWindow *result = NULL;
    map<int, GLUTWindow *>::const_iterator i = __instanceMap.find(id);
    if (i != __instanceMap.end()) {
	result = i->second;
    }
    return result;
}

GLUTWindow::GLUTWindow(const char *title): _visible(true)
{
    _id = glutCreateWindow(title);
    __instanceMap[_id] = this;
    glutReshapeFunc(__reshapeFunc);
    glutDisplayFunc(__displayFunc);
    glutMouseFunc(__mouseFunc);
    glutMotionFunc(__motionFunc);
    glutPassiveMotionFunc(__passiveMotionFunc);
    glutKeyboardFunc(__keyboardFunc);
    glutSpecialFunc(__specialFunc);
    glutVisibilityFunc(__visibilityFunc);
}

GLUTWindow::~GLUTWindow()
{
    __instanceMap.erase(_id);
    glutDestroyWindow(_id);
}

void GLUTWindow::setTitle(const char *title)
{
    // Ensure that we are the current window.
    int currentWindow = set();

    // Set our title.
    glutSetWindowTitle(title);

    // Restore current window (if necessary);
    set(currentWindow);
}

void GLUTWindow::reshape(int width, int height)
{
    // Ensure that we are the current window.
    int currentWindow = set();

    // Resize.
    glutReshapeWindow(width, height);

    // Restore current window (if necessary);
    set(currentWindow);
}

void GLUTWindow::startTimer(unsigned int msecs, cb method)
{
    // It's possible to for each instance to start several
    // simultaneous timers.  We only have one timer callback function
    // (__timerFunc).  We use the 'value' parameter of the callback to
    // differentiate timers, so we need to generate a unique id for
    // each new timer.  One approach would be to keep a set (or a
    // bitset) of used ids, but that still requires doing some kind of
    // search to see which are used.  Simpler is just to choose random
    // numbers until we find one that's unique - the odds that it has
    // been used already are extremely small, unless we have very very
    // many timers (ie, around 2^n, where n is the number of bits in
    // an integer).
    int id = random();
    while (__activeTimerIDs.count(id) > 0) {
	id = random();
    }

    // We have a unique id, so update our __activeTimerIDs and
    // __activeTimers variables.
    __activeTimerIDs.insert(id);
    __activeTimers[id] = make_pair(this, method);

    // Start the timer.
    glutTimerFunc(msecs, __timerFunc, id);
}

void GLUTWindow::reveal()
{
    set();
    glutShowWindow();
    _visible = true;
}

void GLUTWindow::hide()
{
    set();
    glutHideWindow();
    _visible = false;
}

int GLUTWindow::width()
{
    // Ensure that we are the current window.
    int currentWindow = set();

    // Get our width.
    int result = glutGet(GLUT_WINDOW_WIDTH);

    // Restore current window (if necessary);
    set(currentWindow);

    return result;
}

int GLUTWindow::height()
{
    // Ensure that we are the current window.
    int currentWindow = set();

    // Get our height.
    int result = glutGet(GLUT_WINDOW_HEIGHT);

    // Restore current window (if necessary);
    set(currentWindow);

    return result;
}

void GLUTWindow::attach(int button, GLUTMenu *menu)
{
    // Ensure that we are the current window.
    int currentWindow = set();

    // EYE - make a method in GLUTMenu to do this, like set() in
    // GLUTWindow?
    int oldMenu = glutGetMenu();
    if (oldMenu != menu->id()) {
	glutSetMenu(menu->id());
    }

    // Attach the menu.
    glutAttachMenu(button);

    // Restore the old menu (if necessary).
    if (oldMenu != menu->id()) {
	glutSetMenu(oldMenu);
    }
    // Restore current window (if necessary);
    set(currentWindow);
}

void GLUTWindow::detach(int button)
{
    // Ensure that we are the current window.
    int currentWindow = set();

    // Detach whatever menu is on that button.
    glutDetachMenu(button);

    // Restore current window (if necessary);
    set(currentWindow);
}

void GLUTWindow::__displayFunc()
{
    GLUTWindow *win = GLUTWindow::__getWindow();
    if (win) {
	win->_display();
    }
}

void GLUTWindow::__reshapeFunc(int w, int h)
{
    GLUTWindow *win = GLUTWindow::__getWindow();
    if (win) {
	win->_reshape(w, h);
    }
}

void GLUTWindow::__mouseFunc(int button, int state, int x, int y)
{
    GLUTWindow *win = GLUTWindow::__getWindow();
    if (win) {
	win->_mouse(button, state, x, y);
    }
}

void GLUTWindow::__motionFunc(int x, int y)
{
    GLUTWindow *win = GLUTWindow::__getWindow();
    if (win) {
	win->_motion(x, y);
    }
}

void GLUTWindow::__passiveMotionFunc(int x, int y)
{
    GLUTWindow *win = GLUTWindow::__getWindow();
    if (win) {
	win->_passiveMotion(x, y);
    }
}

void GLUTWindow::__keyboardFunc(unsigned char key, int x, int y)
{
    GLUTWindow *win = GLUTWindow::__getWindow();
    if (win) {
	win->_keyboard(key, x, y);
    }
}

void GLUTWindow::__specialFunc(int key, int x, int y)
{
    GLUTWindow *win = GLUTWindow::__getWindow();
    if (win) {
	win->_special(key, x, y);
    }
}

void GLUTWindow::__visibilityFunc(int state)
{
    GLUTWindow *win = GLUTWindow::__getWindow();
    if (win) {
	win->_visibility(state);
    }
}

map<int, GLUTWindow *> GLUTWindow::__instanceMap;

GLUTWindow *GLUTWindow::__getWindow()
{
    return windowWithID(glutGetWindow());
}

map<int, pair<GLUTWindow *, GLUTWindow::cb> > 
GLUTWindow::__activeTimers;
set<int> GLUTWindow::__activeTimerIDs;

// Our GLUT timer function.  This is called directly by GLUT when the
// timer fires.  We use the given id to find out which GLUTWindow
// instance and method we should call.
void GLUTWindow::__timerFunc(int id)
{
    assert(__activeTimers.find(id) != __activeTimers.end());
    assert(__activeTimerIDs.find(id) != __activeTimerIDs.end());
    pair<GLUTWindow *, cb> p = __activeTimers[id];
    __activeTimerIDs.erase(id);
    __activeTimers.erase(id);

    // Extract the GLUTWindow and timer method
    GLUTWindow *win = p.first;
    cb method = p.second;

    // Set it to be the current window, and call it.
    int currentWindow = win->set();
    (win->*method)();

    // Restore the old window to be current.
    set(currentWindow);
}

//////////////////////////////////////////////////////////////////////
// GLUTMenu
//////////////////////////////////////////////////////////////////////
GLUTMenu::GLUTMenu()
{
    _menuID = glutCreateMenu(__menuFunc);
}

GLUTMenu::~GLUTMenu()
{
    clear();
    glutDestroyMenu(_menuID);
}

void GLUTMenu::addItem(const char *label, cb method)
{
    // Make sure we're current.
    int oldMenu = glutGetMenu();
    if (oldMenu != _menuID) {
	glutSetMenu(_menuID);
    }

    // Get a unique id.  See startTimer() for a discussion of the
    // algorithm.
    int menuItemID = random();
    while (__menuItemIDs.count(menuItemID) > 0) {
	menuItemID = random();
    }

    // Add it to our instance vector and class maps.
    _menuItemIDs.push_back(menuItemID);
    __menuItemIDs.insert(menuItemID);
    __menuItems[menuItemID] = make_pair(this, method);

    // Create a GLUT menu entry.
    glutAddMenuEntry(label, menuItemID);

    // Restore the old menu (if necessary).
    if (oldMenu != _menuID) {
	glutSetMenu(oldMenu);
    }
}

void GLUTMenu::addSubMenu(const char *label, GLUTMenu *subMenu)
{
    // Make sure we're current.
    int oldMenu = glutGetMenu();
    if (oldMenu != _menuID) {
	glutSetMenu(_menuID);
    }

    // Add it to our instance vector.
    _subMenus.push_back(subMenu);

    // Create the GLUT submenu entry.
    glutAddSubMenu(label, subMenu->id());

    // Restore the old menu (if necessary).
    if (oldMenu != _menuID) {
	glutSetMenu(oldMenu);
    }
}

void GLUTMenu::clear()
{
    for (size_t i = 0; i < _menuItemIDs.size(); i++) {
	int id = _menuItemIDs[i];
	__menuItems.erase(id);
	__menuItemIDs.erase(id);
    }
    _menuItemIDs.clear();

    for (size_t i = 0; i < _subMenus.size(); i++) {
	delete _subMenus[i];
    }
    _subMenus.clear();

    // Make sure our menu is current and tell GLUT to forget all of
    // its items.
    int oldMenu = glutGetMenu();
    if (oldMenu != _menuID) {
	glutSetMenu(_menuID);
    }
    while (glutGet(GLUT_MENU_NUM_ITEMS)) {
	// EYE - will this remove submenus correctly?
	glutRemoveMenuItem(1);
    }
    // Restore the old menu (if necessary).
    if (oldMenu != _menuID) {
	glutSetMenu(oldMenu);
    }
}

map<int, pair<GLUTMenu *, GLUTMenu::cb> > GLUTMenu::__menuItems;
set<int> GLUTMenu::__menuItemIDs;

// Our GLUT menu function.  This is called directly by GLUT when a
// menu item is selected.  We use the given id to find out which
// GLUTMenu instance and method we should call.
void GLUTMenu::__menuFunc(int id)
{
    assert(__menuItems.find(id) != __menuItems.end());
    assert(__menuItemIDs.find(id) != __menuItemIDs.end());
    pair<GLUTMenu *, cb> p = __menuItems[id];

    // Extract the GLUTMenu and menu method and call it.  Note that we
    // don't need to set the window to be current, as GLUT does that
    // for us.
    GLUTMenu *win = p.first;
    cb method = p.second;
    (win->*method)();
}
