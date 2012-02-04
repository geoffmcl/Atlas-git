/*-------------------------------------------------------------------------
  Geographics.cxx

  Written by Brian Schack

  Copyright (C) 2009 - 2012 Brian Schack

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

// C++ system files
#include <stdexcept>

// Our include file
#include "Geographics.hxx"

// Our project's include files
#include "LayoutManager.hxx"

// Draw text at the given latitude and longitude, with hdg pointing
// up.
void geodDrawText(LayoutManager& lm, const sgdVec3 cart, double lat, 
		  double lon, double hdg, GeodTextFiddling fiddling)
{
    LayoutManager::Point anchor = lm.anchor();

    // Flip it if required.
    if (fiddling != NO_FIDDLING) {
	hdg = normalizeHeading(hdg);
	if ((hdg > 90.0) && (hdg <= 270.0)) {
	    hdg -= 180.0;
	    // If we need to flip the orientation of the labels, we
	    // also flip the positioning of the layout.  For example,
	    // if the original layout specified that the lower right
	    // corner of the box should be placed at <lat, lon>, then,
	    // with the text upside-down, the upper left corner of the
	    // box should be placed there.  Any offset from that
	    // position needs to be reversed as well.
	    if (fiddling == FIDDLE_TEXT) {
		if (anchor == LayoutManager::UL) {
		    lm.moveTo(-lm.x(), -lm.y(), LayoutManager::LR);
		} else if (anchor == LayoutManager::CL) {
		    lm.moveTo(-lm.x(), -lm.y(), LayoutManager::CR);
		} else if (anchor == LayoutManager::LL) {
		    lm.moveTo(-lm.x(), -lm.y(), LayoutManager::UR);
		} else if (anchor == LayoutManager::UC) {
		    lm.moveTo(-lm.x(), -lm.y(), LayoutManager::LC);
		} else if (anchor == LayoutManager::CC) {
		    lm.moveTo(-lm.x(), -lm.y(), LayoutManager::CC);
		} else if (anchor == LayoutManager::LC) {
		    lm.moveTo(-lm.x(), -lm.y(), LayoutManager::UC);
		} else if (anchor == LayoutManager::UR) {
		    lm.moveTo(-lm.x(), -lm.y(), LayoutManager::LL);
		} else if (anchor == LayoutManager::CR) {
		    lm.moveTo(-lm.x(), -lm.y(), LayoutManager::CL);
		} else if (anchor == LayoutManager::LR) {
		    lm.moveTo(-lm.x(), -lm.y(), LayoutManager::UL);
		}
	    } else {
		if (anchor == LayoutManager::UL) {
		    lm.moveTo(-lm.x(), lm.y(), LayoutManager::UR);
		} else if (anchor == LayoutManager::CL) {
		    lm.moveTo(-lm.x(), lm.y(), LayoutManager::CR);
		} else if (anchor == LayoutManager::LL) {
		    lm.moveTo(-lm.x(), lm.y(), LayoutManager::LR);
		} else if (anchor == LayoutManager::UR) {
		    lm.moveTo(-lm.x(), lm.y(), LayoutManager::UL);
		} else if (anchor == LayoutManager::CR) {
		    lm.moveTo(-lm.x(), lm.y(), LayoutManager::CL);
		} else if (anchor == LayoutManager::LR) {
		    lm.moveTo(-lm.x(), lm.y(), LayoutManager::LL);
		}
	    }
	}
    }

    geodPushMatrix(cart, lat, lon, hdg); {
	lm.drawText();
    }
    geodPopMatrix();
    lm.setAnchor(anchor);
}

void geodDrawText(LayoutManager& lm, double lat, double lon, double hdg, 
		  GeodTextFiddling fiddling)
{
    sgdVec3 cart;
    sgGeodToCart(lat * SGD_DEGREES_TO_RADIANS, 
		 lon * SGD_DEGREES_TO_RADIANS,
		 0.0, 
		 cart);
    geodDrawText(lm, cart, lat, lon, hdg, fiddling);
}

void geodDrawText(LayoutManager& lm, const sgdVec3 cart, double hdg, 
		  GeodTextFiddling fiddling)
{
    double lat, lon, alt;
    sgCartToGeod(cart, &lat, &lon, &alt);
    lat *= SGD_RADIANS_TO_DEGREES;
    lon *= SGD_RADIANS_TO_DEGREES;
    geodDrawText(lm, cart, lat, lon, hdg, fiddling);
}

// Draw a vertex at the given lat/lon, given in degrees.
void geodVertex3f(double lat, double lon, bool normals)
{
    SGVec3<double> cart;

    SGGeodesy::SGGeodToCart(SGGeod::fromDeg(lon, lat), cart);
    if (normals) {
	SGVec3<double> normal;
	normal = cart * (1/length(cart));
	glNormal3f(normal[0], normal[1], normal[2]);
    }
    glVertex3f(cart[0], cart[1], cart[2]);
}

void geodPushMatrix(const sgdVec3 cart, double lat, double lon, double hdg)
{
    glPushMatrix();
    glTranslated(cart[0], cart[1], cart[2]);
    glRotatef(lon + 90.0, 0.0, 0.0, 1.0);
    glRotatef(90.0 - lat, 1.0, 0.0, 0.0);
    glRotatef(-hdg, 0.0, 0.0, 1.0);
}

void geodPushMatrix(double lat, double lon, double hdg, double elev)
{
    sgdVec3 cart;
    sgGeodToCart(lat * SGD_DEGREES_TO_RADIANS, 
		 lon * SGD_DEGREES_TO_RADIANS,
		 elev * SG_FEET_TO_METER, 
		 cart);
    geodPushMatrix(cart, lat, lon, hdg);
}

void geodPushMatrix(const sgdVec3 cart, double hdg)
{
    double lat, lon, alt;
    sgCartToGeod(cart, &lat, &lon, &alt);
    lat *= SGD_RADIANS_TO_DEGREES;
    lon *= SGD_RADIANS_TO_DEGREES;
    geodPushMatrix(cart, lat, lon, hdg);
}

void geodPopMatrix() 
{ 
    glPopMatrix(); 
}

//////////////////////////////////////////////////////////////////////
// GreatCircle
//////////////////////////////////////////////////////////////////////

// EYE - pass SGVec3<double>?
GreatCircle::GreatCircle(SGGeod& start, SGGeod& end): _start(start), _end(end)
{
    geo_inverse_wgs_84(_start, _end, &_toAz, &_fromAz, &_distance);
}

GreatCircle::~GreatCircle()
{
}

void GreatCircle::draw(double metresPerPixel, const sgdFrustum& frustum,
		       const sgdMat4& m)
{
    // To draw the great circle arc as an arc, we approximate it with
    // straight-line segments.  The question is: how many?  
    //
    // Because the earth is essentially flat when zoomed in close, we
    // say that no segment need be shorter than degreesPerSegment.
    //
    // Depending on the zoom, that may result in very short segments
    // (when zoomed out far), or very long segments (when zoomed in
    // close).  We don't let segments shrink to less than
    // pixeslPerSegment.
    const float degreesPerSegment = 1 / 60.0;
    const int minPixelsPerSegment = 10;
    const float metresPerDegree = 1e7 / 90.0;
    double metresPerSegment = 
	std::max((double)(degreesPerSegment * metresPerDegree),
		 (double)(minPixelsPerSegment * metresPerPixel));

    // Now create the segments.  We start with a single segment
    // covering the whole great circle, then ask it to subdivide
    // itself.
    _Segment n(_start, _end, _toAz, _fromAz, _distance);
    std::vector<SGVec3<double> > segments;
    n.subdivide(frustum, m, metresPerSegment, segments);

    glBegin(GL_LINE_STRIP); {
	for (size_t i = 0; i < segments.size(); i++) {
	    glVertex3f(segments[i][0], segments[i][1], segments[i][2]);
	}
    }
    glEnd();
}

// Create a segment.  We strictly don't need to pass in toAz, fromAz,
// and distance, as these can be derived from start and end.  However,
// they've already been calculated in GreatCircle, so we might as well
// avoid recalculation.
GreatCircle::_Segment::_Segment(SGGeod& start, SGGeod& end, 
				double toAz, double fromAz, double distance):
    _start(start), _end(end), _toAz(toAz), _fromAz(fromAz), 
    _distance(distance), _A(NULL), _B(NULL)
{
    // Create a bounding sphere.  First, find out the centre of the
    // sphere.
    geo_direct_wgs_84(_start, _toAz, _distance / 2.0, _middle, &_midAz);

    SGVec3<double> midCart;
    SGGeodesy::SGGeodToCart(_middle, midCart);
    _bounds.setCenter(midCart.x(), midCart.y(), midCart.z());
    // EYE - not strictly correct, as _distance is the distance along
    // the earth's surface, not straight-line.
    _bounds.setRadius(_distance / 2.0);
}

GreatCircle::_Segment::~_Segment()
{
    _prune();
}

void GreatCircle::_Segment::subdivide(const sgdFrustum& frustum, 
				      const sgdMat4& m,
				      double minimumLength,
				      std::vector<SGVec3<double> >& points)
{
    atlasSphere tmp = _bounds;
    tmp.orthoXform(m);

    if (frustum.contains(&tmp) == SG_OUTSIDE) {
    	// We're not in the frustum, so we can just return.  Delete
    	// any children we have.
    	_prune();
    } else if (_distance <= minimumLength) {
    	// We don't need to subdivide any further.  Add the end point
    	// to the points vector (and the start, if we're the first
    	// subsegment).
	SGVec3<double> cart;
    	if (points.size() == 0) {
	    SGGeodesy::SGGeodToCart(_start, cart);
    	    points.push_back(cart);
    	}
	SGGeodesy::SGGeodToCart(_end, cart);
	points.push_back(cart);

    	// Make sure any children are deleted.
    	_prune();
    } else {
    	// We need to subdivide.
    	if (_A == NULL) {
    	    _A = new _Segment(_start, _middle, _toAz, _midAz, 
			      _distance / 2.0);
    	}
    	_A->subdivide(frustum, m, minimumLength, points);

    	if (_B == NULL) {
    	    _B = new _Segment(_middle, _end, _midAz + 180.0, _fromAz, 
			      _distance / 2.0);
    	}
    	_B->subdivide(frustum, m, minimumLength, points);
    }
}
    
void GreatCircle::_Segment::_prune()
{
    if (_A != NULL) {
	delete _A;
    }
    if (_B != NULL) {
	delete _B;
    }
}

//////////////////////////////////////////////////////////////////////
// AtlasCoord
//////////////////////////////////////////////////////////////////////

AtlasCoord::AtlasCoord(): _geodValid(false), _cartValid(false)
{
}

AtlasCoord::AtlasCoord(double lat, double lon, double elev): 
    _cartValid(false)
{
    set(lat, lon, elev);
}

AtlasCoord::AtlasCoord(SGGeod& geod): _cartValid(false)
{
    set(geod);
}

AtlasCoord::AtlasCoord(SGVec3<double>& cart): _geodValid(false)
{
    set(cart);
}

AtlasCoord::AtlasCoord(sgdVec3 cart): _geodValid(false)
{
    set(cart);
}

bool AtlasCoord::valid() const
{
    return (_geodValid || _cartValid);
}

void AtlasCoord::invalidate()
{
    _geodValid = _cartValid = false;
}

const SGGeod& AtlasCoord::geod()
{
    if (!valid()) {
	throw std::runtime_error("invalid AtlasCoord");
    }
    if (!_geodValid) {
	_cartToGeod();
    }
    return _geod;
}

double AtlasCoord::lat()
{
    return geod().getLatitudeDeg();
}

double AtlasCoord::lon()
{
    return geod().getLongitudeDeg();
}

double AtlasCoord::elev()
{
    return geod().getElevationM();
}

const SGVec3<double>& AtlasCoord::cart()
{
    if (!valid()) {
	throw std::runtime_error("invalid AtlasCoord");
    }
    if (!_cartValid) {
	_geodToCart();
    }
    return _cart;
}

const double *AtlasCoord::data()
{
    return cart().data();
}

double AtlasCoord::x()
{
    return cart().x();
}

double AtlasCoord::y()
{
    return cart().y();
}

double AtlasCoord::z()
{
    return cart().z();
}

void AtlasCoord::set(double lat, double lon, double elev)
{
    _geod.setLatitudeDeg(lat);
    _geod.setLongitudeDeg(lon);
    _geod.setElevationM(elev);
    _geodValid = true;
    _cartValid = false;
}

void AtlasCoord::set(const SGGeod& geod)
{
    _geod = geod;
    _geodValid = true;
    _cartValid = false;
}

void AtlasCoord::set(const SGVec3<double>& cart)
{
    _cart = cart;
    _cartValid = true;
    _geodValid = false;
}

void AtlasCoord::set(const sgdVec3 cart)
{
    _cart[0] = cart[0];
    _cart[1] = cart[1];
    _cart[2] = cart[2];
    _cartValid = true;
    _geodValid = false;
}

void AtlasCoord::_cartToGeod()
{
    assert(_cartValid && !_geodValid);
    SGGeodesy::SGCartToGeod(_cart, _geod);
    _geodValid = true;
}

void AtlasCoord::_geodToCart()
{
    assert(_geodValid && !_cartValid);
    SGGeodesy::SGGeodToCart(_geod, _cart);
    _cartValid = true;
}

