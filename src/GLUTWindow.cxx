/*-------------------------------------------------------------------------
  GLUTWindow.cxx

  Written by Brian Schack

  Copyright (C) 2012 Brian Schack

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

#if defined( __APPLE__)
#  include <GLUT/glut.h>
#else
#  ifdef WIN32
#    include <windows.h>
#  endif
#  include <GL/glut.h>
#endif

#include <assert.h>

#include "GLUTWindow.hxx"

using namespace std;

// Class methods
int GLUTWindow::set(int id)
{
    int oldID = glutGetWindow();
    if (oldID != id) {
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

GLUTWindow::GLUTWindow(const char *title)
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

void GLUTWindow::startTimer(unsigned int msecs, void (GLUTWindow::*method)())
{
    // It's possible to for each instance to start several
    // simultaneous timers.  We only have one timer callback function
    // (__timerFunc).  We use the 'value' parameter of the callback to
    // differentiate timers, so we need to generate a unique id for
    // each new timer.  One approach would be to keep a set (or a
    // bitset) or used ids, but that still requires doing some kind of
    // search to see which are used.  Simpler is just to choose random
    // numbers until we find one that's unique - the odds that it has
    // been used already are extremely small, unless we have very very
    // many timers (id, around 2^n, where n is the number of bits in
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
    // EYE - save current window and restore at end?
    set();
    glutShowWindow();
}

void GLUTWindow::hide()
{
    // EYE - save current window and restore at end?
    set();
    glutHideWindow();
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

map<int, pair<GLUTWindow *, void (GLUTWindow::*)()> > 
GLUTWindow::__activeTimers;
set<int> GLUTWindow::__activeTimerIDs;

// Our GLUT timer function.  This is called directly by GLUT when the
// timer fires.  We use the given id to find out which GLUTWindow
// instance and method we should call.
void GLUTWindow::__timerFunc(int id)
{
    assert(__activeTimers.find(id) != __activeTimers.end());
    pair<GLUTWindow *, void (GLUTWindow::*)()> p = __activeTimers[id];
    __activeTimerIDs.erase(id);
    __activeTimers.erase(id);

    // Extract the GLUTWindow and timer method
    GLUTWindow *win = p.first;
    void (GLUTWindow::*cb)() = p.second;

    // Set it to be the current window, and call it.
    int currentWindow = win->set();
    (win->*cb)();

    // Restore the old window to be current.
    glutSetWindow(currentWindow);
}
