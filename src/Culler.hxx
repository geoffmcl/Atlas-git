/*-------------------------------------------------------------------------
  Culler.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2014 Brian Schack

  A simple culling structure.  Atlas uses it to quickly look up
  navaids, airports, etc in a given area.

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

#ifndef _CULLER_H
#define _CULLER_H

#include <vector>
#include <set>

#include <plib/sg.h>

// Forward class declarations
class atlasSphere;

// A nice set of articles about culling can be found at:
//
// http://www.lighthouse3d.com/opengl/viewfrustum/
//
// It doesn't talk about hierarchically pruning what is to be
// rendered, but it does give lots of information about the nuts and
// bolts of testing whether objects would be visible.

// A simple culling structure.  
//
// Culler divides the world into a hierarchy of areas.  At the first
// level, the world is divided into 18 equal triangles.  At the second
// level, each triangle is divided into 36 not-so-equal "squares",
// each 10 degrees by 10 degrees.  Finally, at the third level, each
// square is further divided into 100 potentially very not-so-equal
// squares, each 1 degree by 1 degree.  The "not-so-equal"ness becomes
// more extreme as latitudes increase.
//
// Objects are added to the hierarchy with the addObject() method.  An
// object will be placed in the hierarchy based on the centre of its
// bounding sphere.  Note that the culler doesn't own anything you
// give it - you're still responsible for the object.
//
// The intersections() method returns a vector of pointers to objects
// of a given type which are in the given bounds.
//
// Separately there are searchers.  One or more searchers can be
// attached to a culler, and they provide spatial search functions on
// the data in the culler.  Currently there are frustum and point
// searchers.
//
// Searchers need to be told about the current view frustum, via
// zoom().  They also need to be told if the modelview matrix has
// changed, via move().  This doesn't cause any searching - searchers
// are lazy and just records the fact that the view has changed.
// Searching will not occur until the next time you ask for
// intersections().
//
// The strategy of delaying searches until the call to intersections()
// can result in saving work, if multiple calls to zoom() or move()
// are made for each call to intersections().  However, there is a
// catch - if you've obtained a reference to some intersections
// before, then call zoom() or move(), you're results will be invalid.
// To be safe, call intersections() if you're not sure of the validity
// of your results.  It won't result in unnecessary work on the part
// of Culler.

// The culler stores and retrieves cullables.  A cullable is very
// simple - it just has a bounds (represented by an atlasSphere), and
// can return its latitude and longitude (in degrees).
class Cullable {
  public:
    virtual ~Cullable() {}

    virtual const atlasSphere& bounds() = 0;
    virtual double latitude() = 0;
    virtual double longitude() = 0;
};

class Culler {
  public:
    class Search;
    class FrustumSearch;
    class PointSearch;

    // EYE - add (private) copy constructor?
    Culler();
    ~Culler();

    // EYE - addChild, pass a Child ref?
    // EYE - eventually we'll need to add a deleteObject (eg, so that
    // we can rescan the scenery directories when new scenery is
    // downloaded).
    void addObject(Cullable *obj);

    void addSearcher(Culler::Search* s);
    void removeSearcher(Culler::Search* s);

    // Performs a search for objects within the given frustum and view
    // coordinates.  Returns a reference to a vector (one for each
    // type of object) of vectors of intersecting objects.  Note that
    // this causes a full search each time it is called, regardless of
    // whether the view has changed in the meantime.  For efficiency,
    // it's better to use a Searcher object, which is smart enough to
    // know whether a full search should be done.
    void intersections(const sgdFrustum& frustum, sgdMat4 m,
		       std::vector<Cullable *>& intersections);
    // Performs a search for objects that intersect the given point.
    void intersections(const sgdVec3 point, 
		       std::vector<Cullable *>& intersections);

  protected:
    class Node;
    class Branch;
    class Leaf;
    class Child;

    static const int __noOfLevels = 3;
    static int __limits[__noOfLevels];

    void _latLonToIndices(double latitude, double longitude, 
			  int indices[__noOfLevels]);
    Node *_child(Branch *parent, int i, int childLevel);

    void _setDirty();

    Branch *_root;
    unsigned int _noOfTypes;

    std::set<Culler::Search *> _searchers;
};

// The Culler::Search classes implement searching through a Culler (a
// Culler just acts as a spatial repository).  They can accumulate
// results of searches, and are smart enough not to repeat searches if
// nothing has changed.  Culler::Search is a virtual class - the
// concrete subclasses implement particular search strategies.
class Culler::Search {
  public:
    Search(Culler &c): _c(c), _isDirty(true) { _c.addSearcher(this); }
    virtual ~Search() { _c.removeSearcher(this); }

    Culler& culler() { return _c; }
    void setDirty() { _isDirty = true; }
    bool isDirty() { return _isDirty; }

    // EYE - would a set or list be better?
    virtual const std::vector<Cullable *>& intersections() = 0;

  protected:
    void _setDirty(bool d) { _isDirty = d; }

    Culler& _c;
    bool _isDirty;
    std::vector<Cullable *> _intersections;
};

// Culler::FrustumSearch implements searching for objects in or
// intersecting the given frustum.  This is the main search class, and
// is used by the scenery and overlay systems.
class Culler::FrustumSearch: public Culler::Search {
  public:
    FrustumSearch(Culler &c);
    ~FrustumSearch();

    void zoom(double left, double right, 
	      double bottom, double top,
	      double near, double far);
    void move(const sgdMat4 modelViewMatrix);

    bool intersects(atlasSphere bounds) const;
    // EYE - would a set or list be better?
    const std::vector<Cullable *>& intersections();

  protected:
    sgdFrustum _frustum;
    sgdMat4 _modelViewMatrix;
};

// Culler::PointSearch implements searching for objects which
// intersect a given point.  This is used to find out which navaids
// are within range of a point (usually the aircraft in a flight
// track).
class Culler::PointSearch: public Culler::Search {
  public:
    PointSearch(Culler &c);
    ~PointSearch();

    void move(sgdVec3 point);

    // EYE - would a set or list be better?
    const std::vector<Cullable *>& intersections();

  protected:
    sgdVec3 _point;
};

#endif
