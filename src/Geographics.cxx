/*-------------------------------------------------------------------------
  Geographics.cxx

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

#include "Geographics.hxx"

void geodDrawText(LayoutManager& lm, double lat, double lon, double hdg, 
		  bool alwaysUp)
{
    // Flip it if required.
    if (alwaysUp) {
	hdg = normalizeHeading(hdg);
	if ((hdg > 90.0) && (hdg < 270.0)) {
	    hdg -= 180.0;
	}
    }
    geodPushMatrix(lat, lon, hdg); {
	lm.drawText();
    }
    geodPopMatrix();
}

void geodDrawText(LayoutManager& lm, const sgdVec3 cart, double lat, 
		  double lon, double hdg, bool alwaysUp)
{
    // Flip it if required.
    if (alwaysUp) {
	hdg = normalizeHeading(hdg);
	if ((hdg > 90.0) && (hdg < 270.0)) {
	    hdg = -hdg;
	}
    }
    geodPushMatrix(cart, lat, lon, hdg); {
	lm.drawText();
    }
    geodPopMatrix();
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

void geodPopMatrix() 
{ 
    glPopMatrix(); 
}
