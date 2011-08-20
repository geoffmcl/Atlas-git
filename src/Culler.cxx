/*-------------------------------------------------------------------------
  Culler.cxx

  Written by Brian Schack

  Copyright (C) 2009 - 2011 Brian Schack

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Culler.hxx"
#include "Globals.hxx"

#include <cassert>
#include <vector>

#include <simgear/math/sg_geodesy.hxx>

using namespace std;

// EYE - we should come up with better debugging output
// #define DEBUG_OUTPUT

// Returns true if the bounding sphere A completely contains the
// bounding sphere B.  A contains B if the distance from A's centre to
// B's centre, plus B's radius, is less than A's radius.
bool contains(const atlasSphere& A, const atlasSphere& B)
{
    double d = sgdDistanceVec2(A.center, B.center) + B.radius;
    return (d <= A.radius);
}

class Culler::Node {
  public:
    Node(bool isDirty = true);
    virtual ~Node() {}		// See Thinking in C++, Vol 1, Ch 15

    virtual void intersections(const sgdFrustum& frustum, sgdMat4 m,
			       vector<Cullable *>& intersections) = 0;
    virtual void intersections(const sgdVec3 point, 
			       vector<Cullable *>& intersections) = 0;
    virtual void grabAll(vector<Cullable *>& intersections) = 0;

    virtual void calcBounds() = 0;
    const atlasSphere& bounds() { return _bounds; }

    const bool isDirty() { return _isDirty; }
    void setDirty(bool d) { _isDirty = d; }

#ifdef DEBUG_OUTPUT
    static unsigned int totalBounds, checks, grabs, extensions, largest;
#endif

  protected:
    atlasSphere _bounds;
    bool _isDirty;
};

// A branch has a fixed number of children.  The children can be
// branches or leaves.
class Culler::Branch: public Culler::Node {
  public:
    Branch(int size);
    ~Branch();

    Node *child(int i) const;
    void setChild(int i, Node &child);
    void intersections(const sgdFrustum& frustum, sgdMat4 m,
		       vector<Cullable *>& intersections);
    void intersections(const sgdVec3 point,
		       vector<Cullable *>& intersections);

  protected:
    void grabAll(vector<Cullable *>& intersections);
    void calcBounds();

    Node **children;
    int size;
};

// A leaf has an arbitrary number of children, added one at a time.
// Each child is a Child.
class Culler::Leaf: public Culler::Node {
  public:
    Leaf();
    void push_back(Cullable *obj);

  protected:
    void intersections(const sgdFrustum& frustum, sgdMat4 m,
		       vector<Cullable *>& intersections);
    void intersections(const sgdVec3 point,
		       vector<Cullable *>& intersections);
    void grabAll(vector<Cullable *>& intersections);
    void calcBounds();

    vector<Cullable *> children;
};

#ifdef DEBUG_OUTPUT
unsigned int Culler::Node::totalBounds = 0;
unsigned int Culler::Node::checks = 0;
unsigned int Culler::Node::grabs = 0;
unsigned int Culler::Node::extensions = 0;
unsigned int Culler::Node::largest = 0;
#endif

Culler::Node::Node(bool isDirty): _isDirty(isDirty)
{
#ifdef DEBUG_OUTPUT
    totalBounds++;
#endif
}

Culler::Branch::Branch(int size): Node(), size(size)
{
    children = new Node*[size];
    memset(children, 0, sizeof(Node *) * size);
}

Culler::Branch::~Branch()
{
    for (int i = 0; i < size; i++) {
	if (children[i] != NULL) {
	    delete children[i];
	}
    }

    delete []children;
}

Culler::Node *Culler::Branch::child(int i) const
{
    assert((i >= 0) && (i < size));
    return children[i];
}

void Culler::Branch::setChild(int i, Node &child)
{
    assert((i >= 0) && (i < size));
    children[i] = &child;
}

void Culler::Branch::intersections(const sgdFrustum& frustum, sgdMat4 m,
				   vector<Cullable *>& intersections)
{
    if (_isDirty) {
	calcBounds();
    }
    
#ifdef DEBUG_OUTPUT
    checks++;
#endif

    // Transform into the frustum's coordinate space.
    atlasSphere tmp = _bounds;
    tmp.orthoXform(m);

    // Do the intersection test.
    int result = frustum.contains(&tmp);
    if (result == SG_OUTSIDE) {
	return;
    }

    if (result == SG_INSIDE) {
	// The test volume contains everything in this branch -
	// short-circuit the search.
	grabAll(intersections);
    } else {
	for (int i = 0; i < size; i++) {
	    if (children[i] != NULL) {
		children[i]->intersections(frustum, m, intersections);
	    }
	}
    }
}

void Culler::Branch::intersections(const sgdVec3 point,
				   vector<Cullable *>& intersections)
{
    if (_isDirty) {
	calcBounds();
    }
    
#ifdef DEBUG_OUTPUT
    checks++;
#endif

    // Is the point within the bound sphere of this branch?
    double result = sgdDistanceVec3(point, _bounds.getCenter());
    if (result > _bounds.getRadius()) {
	// Nope.
	return;
    }

    // The point is inside this branch, so recurse.
    for (int i = 0; i < size; i++) {
	if (children[i] != NULL) {
	    children[i]->intersections(point, intersections);
	}
    }
}

void Culler::Branch::grabAll(vector<Cullable *>& intersections)
{
#ifdef DEBUG_OUTPUT
    grabs++;
#endif
    for (int i = 0; i < size; i++) {
	if (children[i] != NULL) {
	    children[i]->grabAll(intersections);
	}
    }
}

void Culler::Branch::calcBounds()
{
    if (!_isDirty) {
	return;
    }

    _bounds.empty();
    for (int i = 0; i < size; i++) {
	if (children[i] != NULL) {
	    if (children[i]->isDirty()) {
		children[i]->calcBounds();
	    }
#ifdef DEBUG_OUTPUT
	    extensions++;
#endif
	    _bounds.extend(&(children[i]->bounds()));
	}
    }

    setDirty(false);
}

Culler::Leaf::Leaf(): Node(false)
{
    // By default, we reserve space for 10 children.
    children.reserve(10);
}

void Culler::Leaf::push_back(Cullable *obj)
{
    children.push_back(obj);
#ifdef DEBUG_OUTPUT
    extensions++;
    if (children.size() > largest) {
	largest++;
    }
#endif
    _bounds.extend(&(obj->Bounds()));
}

void Culler::Leaf::intersections(const sgdFrustum& frustum, sgdMat4 m,
				 vector<Cullable *>& intersections)
{
    assert(!_isDirty);
    
#ifdef DEBUG_OUTPUT
    checks++;
#endif

    // Transform into the frustum's coordinate space.
    atlasSphere tmp = _bounds;
    tmp.orthoXform(m);

    // Do the intersection test.
    int result = frustum.contains(&tmp);
    if (result == SG_OUTSIDE) {
	return;
    }

    for (unsigned int i = 0; i < children.size(); i++) {
	Cullable *c = children[i];

	tmp = c->Bounds();
	tmp.orthoXform(m);
	result = frustum.contains(&tmp);
	if (result != SG_OUTSIDE) {
	    intersections.push_back(c);
	}
    }
}

void Culler::Leaf::intersections(const sgdVec3 point,
				 vector<Cullable *>& intersections)
{
    assert(!_isDirty);
    
#ifdef DEBUG_OUTPUT
    checks++;
#endif

    // Is the point within the bound sphere of this leaf?
    double result = sgdDistanceVec3(point, _bounds.getCenter());
    if (result > _bounds.getRadius()) {
	// Nope.
	return;
    }

    // The point is inside this leaf, so test it against its children.
    for (unsigned int i = 0; i < children.size(); i++) {
	Cullable *c = children[i];

	result = sgdDistanceVec3(point, c->Bounds().getCenter());
	if (result <= c->Bounds().getRadius()) {
	    intersections.push_back(c);
	}
    }
}

void Culler::Leaf::grabAll(vector<Cullable *>& intersections)
{
    assert(!_isDirty);

#ifdef DEBUG_OUTPUT
    grabs++;
#endif
    for (unsigned int i = 0; i < children.size(); i++) {
	Cullable *c = children[i];
	intersections.push_back(c);
    }
}

void Culler::Leaf::calcBounds()
{
    assert(!_isDirty);
    return;
}

int Culler::__limits[] = {18, 36, 100}; // EYE - magic numbers!

Culler::Culler()
{
    _root = new Branch(__limits[0]);
}

Culler::~Culler()
{
    delete _root;
}

void Culler::_latLonToIndices(double latitude, double longitude, int *indices)
{
    latitude += 90.0;
    longitude += 180.0;

    // EYE - this is tres hacky
    if (latitude >= 180.0) {
	latitude = 179.999999;
    }
    if (longitude >= 360.0) {
	longitude = 359.999999;
    }

    int lat = (int)floor(latitude / 60);
    int lon = (int)floor(longitude / 60);
    indices[0] = (lat * 6) + lon;

    lat = (int)floor(latitude / 10);
    lon = (int)floor(longitude / 10);
    lat %= 6;
    lon %= 6;
    indices[1] = (lat * 6) + lon;

    lat = (int)floor(latitude);
    lon = (int)floor(longitude);
    lat %= 10;
    lon %= 10;
    indices[2] = (lat * 10) + lon;
}

Culler::Node *Culler::_child(Branch *parent, int i, int childLevel)
{
    Node *child = parent->child(i);

    if (child == NULL) {
	if (childLevel < __noOfLevels) {
	    Branch *b = new Branch(__limits[childLevel]);
	    parent->setChild(i, *b);
	} else {
	    Leaf *l = new Leaf();
	    parent->setChild(i, *l);
	}
	child = parent->child(i);
    }

    return child;
}

void Culler::addObject(Cullable *obj)
{
    _setDirty();

    // Get "path" of object in our hierarchy.
    int indices[__noOfLevels];
    _latLonToIndices(obj->latitude(), obj->longitude(), indices);

    Branch *b = _root;
    Leaf *l;
    for (int i = 0; i < __noOfLevels; i++) {
	b->setDirty(true);

	if ((i + 1) < __noOfLevels) {
	    b = dynamic_cast<Branch *>(_child(b, indices[i], i + 1));
	} else {
	    l = dynamic_cast<Leaf *>(_child(b, indices[i], i + 1));
	}
    }
    l->push_back(obj);
}

void Culler::addSearcher(Culler::Search* s)
{
    _searchers.insert(s);
}

void Culler::removeSearcher(Culler::Search* s)
{
    _searchers.erase(s);
}

void Culler::intersections(const sgdFrustum& frustum, sgdMat4 m,
			   vector<Cullable *>& intersections)
{
    _root->intersections(frustum, m, intersections);
}

void Culler::intersections(const sgdVec3 point,
			   vector<Cullable *>& intersections)
{
    _root->intersections(point, intersections);
}

void Culler::_setDirty()
{
    // We don't maintain a dirty state ourselves, but the searchers
    // do, so tell them something has changed here.
    for (set<Culler::Search *>::iterator i = _searchers.begin();
	 i != _searchers.end(); i++) {
	(*i)->setDirty();
    }
}

Culler::FrustumSearch::FrustumSearch(Culler &c): Culler::Search(c)
{
}

Culler::FrustumSearch::~FrustumSearch()
{
}

void Culler::FrustumSearch::zoom(double left, double right, 
				 double bottom, double top,
				 double nnear, double ffar)
{
    // EYE - change to setOrtho(w, h), and have near and far set to
    // contants (eg, earth radius)?
    _frustum.setOrtho(left, right, bottom, top, nnear, ffar);
    _setDirty(true);
}

void Culler::FrustumSearch::move(const sgdMat4 modelViewMatrix)
{
    sgdCopyMat4(_modelViewMatrix, modelViewMatrix);
    _setDirty(true);
}

bool Culler::FrustumSearch::intersects(atlasSphere bounds) const
{
    bounds.orthoXform(_modelViewMatrix);
    return (_frustum.contains(&bounds) != SG_OUTSIDE);
}

const vector<Cullable *>& Culler::FrustumSearch::intersections()
{
    if (_isDirty) {
#ifdef DEBUG_OUTPUT
	Node::checks = 0;
	Node::grabs = 0;
	Node::extensions = 0;
#endif

	// Clear out the old intersections.
	_intersections.clear();

	// Find new intersections.
	_c.intersections(_frustum, _modelViewMatrix, _intersections);

	_setDirty(false);

#ifdef DEBUG_OUTPUT
	printf("%x: checks:%d, grabs:%d, bounds:%d (extensions:%d, largest:%d)\n", 
	       this, Node::checks, Node::grabs, Node::totalBounds, 
	       Node::extensions, Node::largest);
#endif
    }

    return _intersections;
}

Culler::PointSearch::PointSearch(Culler &c): Culler::Search(c)
{
}

Culler::PointSearch::~PointSearch()
{
}

void Culler::PointSearch::move(sgdVec3 point)
{
    sgdCopyVec3(_point, point);
    _setDirty(true);
}

const vector<Cullable *>& Culler::PointSearch::intersections()
{
    if (_isDirty) {
#ifdef DEBUG_OUTPUT
	Node::checks = 0;
	Node::grabs = 0;
	Node::extensions = 0;
#endif
	// Clear out the old intersections.
	_intersections.clear();

	// Find new intersections.
	_c.intersections(_point, _intersections);

	_setDirty(false);

#ifdef DEBUG_OUTPUT
	printf("%x: checks:%d, grabs:%d, bounds:%d (extensions:%d, largest:%d)\n", 
	       this, Node::checks, Node::grabs, Node::totalBounds, 
	       Node::extensions, Node::largest);
#endif
    }

    return _intersections;
}

