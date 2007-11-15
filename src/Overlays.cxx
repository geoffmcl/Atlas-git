/*-------------------------------------------------------------------------
  Overlays.cxx

  Written by Per Liedman, started July 2000.

  Copyright (C) 2000 Per Liedman, liedman@home.se

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

#include <stdio.h>
#include <zlib.h>
#include <map>

#include "Overlays.hxx"
#include "Geodesy.hxx"

#include <iostream>
#include <sstream>

SG_USING_STD(map);

#ifdef _MSC_VER

double rint( double x) 
// Copyright (C) 2001 Tor M. Aamodt, University of Toronto 
// Permisssion to use for all purposes commercial and otherwise granted. 
// THIS MATERIAL IS PROVIDED "AS IS" WITHOUT WARRANTY, OR ANY CONDITION OR 
// OTHER TERM OF ANY KIND INCLUDING, WITHOUT LIMITATION, ANY WARRANTY 
// OF MERCHANTABILITY, SATISFACTORY QUALITY, OR FITNESS FOR A PARTICULAR 
// PURPOSE. 
{ 
    if( x > 0 ) { 
        __int64 xint = (__int64) (x+0.5); 
        if( xint % 2 ) { 
            // then we might have an even number... 
            double diff = x - (double)xint; 
            if( diff == -0.5 ) 
                return double(xint-1); 
        } 
        return double(xint); 
    } else { 
        __int64 xint = (__int64) (x-0.5); 
        if( xint % 2 ) { 
            // then we might have an even number... 
            double diff = x - (double)xint; 
            if( diff == 0.5 ) 
                return double(xint+1); 
        } 
        return double(xint); 
    } 
}

#endif

bool Overlays::airports_loaded = false;
bool Overlays::navaids_loaded  = false;
bool Overlays::fixes_loaded    = false;
vector<Overlays::ARP*> Overlays::airports;
vector<Overlays::NAV*> Overlays::navaids;

const float Overlays::airport_color1[4] = {0.439, 0.271, 0.420, 0.7}; 
const float Overlays::airport_color2[4] = {0.824, 0.863, 0.824, 0.7};
const float Overlays::navaid_color[4]   = {0.439, 0.271, 0.420, 0.7}; 
const float Overlays::static_vor_to_color[4]   = {0.200, 0.800, 0.200, 0.7}; 
const float Overlays::static_vor_from_color[4] = {0.800, 0.200, 0.200, 0.7};
const float Overlays::static_ils_color[4] = {0.800, 0.200, 0.200, 0.7}; 
const float Overlays::static_ndb_color[4] = {0.200, 0.200, 0.800, 0.7}; 
const float Overlays::grid_color[4]     = {0.639, 0.371, 0.620, 0.3};
const float Overlays::track_color[4]    = {0.071, 0.243, 0.427, 0.5};
const float Overlays::aircraft_color[4]    = {1.0, 0.0, 0.0, 1.0};
const float Overlays::aircraft_mark_color[4]    = {1.0, 1.0, 0.0, 1.0};

const float Overlays::dummy_normals[][3] = {{0.0f, 0.0f, 0.0f},
					    {0.0f, 0.0f, 0.0f},
					    {0.0f, 0.0f, 0.0f},
					    {0.0f, 0.0f, 0.0f}};


Overlays::Overlays( const char *fg_root, float scale,
		    float width ) :
  scale(scale) {
  if (fg_root == NULL) {
    if ( (this->fg_root = getenv("FG_ROOT")) == NULL ) {
      this->fg_root = FGBASE_DIR;
    }
  } else {
    this->fg_root = fg_root;
  }
  
  setAirportColor( airport_color1, airport_color2 );
  setNavaidColor( navaid_color );
  setVorToColor( static_vor_to_color );
  setVorFromColor( static_vor_to_color );
  setIlsColor( static_ils_color );
  setNdbColor( static_ndb_color );
  setGridColor( grid_color );
  setTrackColor( track_color );
  setAircraftColor( aircraft_color );
  setAircraftMarkColor( aircraft_mark_color );
//   flight_track = new FlightTrack;
  flight_track = NULL;		// EYE - how did this work before?
  projection= new Projection;

  time_params = new SGTime();
  time_params->update( 0.0, 0.0, 0, 0 );

  mag = new SGMagVar();
}

void Overlays::drawOverlays() {
  float dtheta, dalpha;

  lat_ab( size / scale / 2.0f, size / scale / 2.0f, lat, lon, 
	  &dtheta, &dalpha );
  dtheta -= lat;
  dalpha -= lon;
  if (dalpha > SG_PI/2) dalpha = SG_PI/2;

  if (features & OVERLAY_AIRPORTS) {
    airport_labels(lat, lon, dtheta, dalpha );
  }

  if (features & OVERLAY_NAVAIDS) {
    draw_navaids(lat, lon, dtheta, dalpha );
  }

  if (features & OVERLAY_GRIDLINES) {
    draw_gridlines( dtheta * 1.5f, dalpha * 1.5f,
                   dtheta * 200 / (size/2) );  // Aim for 200 pixels spacing
  }

  if (features & OVERLAY_FLIGHTTRACK) {
    draw_flighttrack();
  }
}

// Shift an angle (radians) by multiples of 2PI into the range [-PI, PI).
static double wrap_angle(double a_rad) {
    double y = fmod(a_rad, SGD_2PI);  /* range (-2PI, 2PI) */
    if      (y < -SGD_PI) y += SGD_2PI;
    else if (y >= SGD_PI) y -= SGD_2PI;
    return y;
}

// Convert an angle to degrees and possibly minutes and possibly seconds,
// according to the given resolution.
// 'ns' is the prefixes for positive and negative, e.g. "NS" or "EW" or "+-".
// See also dmshh_format in Atlas.cxx
static string dms_format(const char ns[2], float angle_rad, float resolution_rad) {
  int deg = 0;
  int min = 0;
  int sec = 0;
  char s[50];
  const char *format;
  
  if (angle_rad < -resolution_rad/4)
    ns++;
  if (angle_rad < 0)
    angle_rad = -angle_rad;

  // Must do the rounding exactly once, at the displayed resolution.
  if (resolution_rad > 0.9f * SG_DEGREES_TO_RADIANS) {
    deg = (int)rint(angle_rad * SG_RADIANS_TO_DEGREES);
    format = "%c%02d*";
  } else if (resolution_rad > 0.9f / 60 * SG_DEGREES_TO_RADIANS) {
    min = (int)rint(angle_rad * SG_RADIANS_TO_DEGREES * 60);
    deg = min / 60;
    format = "%c%02d*%02d'";
  } else {
    sec = (int)rint(angle_rad * SG_RADIANS_TO_DEGREES * 60 * 60);
    min = sec / 60;
    deg = min / 60;
    format = "%c%02d*%02d'%02d\"";
  }
  sprintf(s, format, *ns, deg, min % 60, sec % 60);
  return string(s);
}

// Draw grid lines in latitude range lat-dtheta to lat+dtheta radians,
// longitude range lon-dalpha to lon+dalpha.
// 'spacing' is the desired approximate latitude interval in radians.
void Overlays::draw_gridlines( float dtheta, float dalpha, float spacing ) {
  // Divide gridlines into 10' steps
  const float STEP = 10.0f / 60.0f * SG_DEGREES_TO_RADIANS;

  float grid_theta, grid_alpha;
  projection->nice_angle_pair(spacing * SG_RADIANS_TO_DEGREES,
		  spacing * SG_RADIANS_TO_DEGREES * dalpha / dtheta,
		  &grid_theta, &grid_alpha);
  grid_theta *= SG_DEGREES_TO_RADIANS;
  grid_alpha *= SG_DEGREES_TO_RADIANS;

  if ( grid_theta == 0.f || grid_alpha == 0.f )
    return;

  sgVec3 xyr;
  sgVec2 p1, p2;
  bool first;

  output->setColor(grd_color);

  // draw line labels
  float glon;
  for (glon = rint((lon - dalpha) / grid_alpha) * grid_alpha; 
       glon <= lon + dalpha; glon += grid_alpha) {
    for (float glat = rint(max(lat - dtheta, -SG_PI/2 + grid_theta) / grid_theta) * grid_theta; 
	 glat <= min(lat + dtheta, SG_PI/2 - grid_theta/2); glat += grid_theta) {
      projection->ab_lat( glat, glon, lat, lon, xyr );
      sgSetVec2( p1, ::scale(xyr[0], output->getSize(), scale), 
		 ::scale(xyr[1], output->getSize(), scale) );
      string label =
	dms_format("NS", glat, grid_theta) + ' ' +
	dms_format("EW", wrap_angle(glon), grid_alpha);
      output->drawText( p1, (char *)label.c_str() );
    }
  }

  // draw north-south parallel lines
  for (glon = rint((lon - dalpha) / grid_alpha) * grid_alpha; 
       glon <= lon + dalpha; glon += grid_alpha) {
    first = true;

    for (float glat = max(lat - dtheta, -SG_PI/2); glat <= lat + dtheta + STEP; glat += STEP) {
      projection->ab_lat( min(glat, SG_PI/2), glon, lat, lon, xyr );
      sgSetVec2( p1, ::scale(xyr[0], output->getSize(), scale), 
		 ::scale(xyr[1], output->getSize(), scale) );

      if (first) {
	first = false;
      } else {
	output->drawLine(p1, p2);
      }

      sgCopyVec2( p2, p1 );
    }
  }

  // draw east-west parallel lines
  for (float glat = rint(max(lat - dtheta, -SG_PI/2 + grid_theta) / grid_theta) * grid_theta; 
       glat <= min(lat + dtheta, SG_PI/2 - grid_theta/2); glat += grid_theta) {
    first = true;

    for (float glon = lon - dalpha; glon <= lon + dalpha + STEP; glon += STEP) {
      projection->ab_lat( glat, glon, lat, lon, xyr );
      sgSetVec2( p1, ::scale(xyr[0], output->getSize(), scale), 
		 ::scale(xyr[1], output->getSize(), scale) );

      if (first) {
	first = false;
      } else {
	output->drawLine(p1, p2);
      }

      sgCopyVec2( p2, p1 );
    }
  }
}

// Rotate a point x, y by an angle (in radians), putting the result in
// the two-element vector v.
static void rotate(float x, float y, float angle, sgVec2 v) {
  sgSetVec2(v,
	    (x * cos(angle)) - (y * sin(angle)),
	    (x * sin(angle)) + (y * cos(angle)));
}

void Overlays::draw_flighttrack() 
{
    if (flight_track != NULL) {
	FlightData *point;
	sgVec2 p1, p2;
	bool first = true;

	flight_track->firstPoint();
    
	output->setColor(trk_color);

	while ( (point = flight_track->getNextPoint()) != NULL ) {
	    sgVec3 xyr;
	    projection->ab_lat( point->lat, point->lon, lat, lon, xyr );
      
	    sgSetVec2( p1, ::scale(xyr[0], output->getSize(), scale), 
		       ::scale(xyr[1], output->getSize(), scale) );

	    if (first) {
		first = false;
	    } else {
		output->drawLine( p1, p2 );
	    }

	    sgCopyVec2( p2, p1 );
	}

	// Draw aircraft at its current position.  For live tracks,
	// this is the end of the track.  For file-based tracks, this
	// is at the current mark.
	if (!first) {
	    FlightData *pos = flight_track->getCurrentPoint();
	    if (flight_track->live()) {
		_drawAircraft(pos, ac_color);
	    } else {
		_drawAircraft(pos, ac_mark_color);
	    }
	}
    }
}

// Draws an aircraft silhouette at the given point, in the given colour.
void Overlays::_drawAircraft(FlightData *point, float color[4])
{
    sgVec2 p1, p2;

    sgVec2 c, offset;
    float heading;

    sgVec3 xyr;

    // A little sanity checking.
    if (point == NULL) {
	return;
    }

    projection->ab_lat(point->lat, point->lon, lat, lon, xyr);

    sgSetVec2(p1, ::scale(xyr[0], output->getSize(), scale), 
	      ::scale(xyr[1], output->getSize(), scale));
    heading = (90.0f - point->hdg) * SG_DEGREES_TO_RADIANS;

    output->setColor(color);

    // Center of aircraft.
    sgCopyVec2(c, p1);

    // Draw aircraft, starting with fuselage.
    rotate(4.0f, 0.0f, heading, offset);
    sgAddVec2(p1, c, offset);
    rotate(-9.0f, 0.0f, heading, offset);
    sgAddVec2(p2, c, offset);
    output->drawLine(p1, p2);

    // Wings
    rotate(0.0f, -7.0f, heading, offset);
    sgAddVec2(p1, c, offset);
    rotate(0.0f, 7.0f, heading, offset);
    sgAddVec2(p2, c, offset);
    output->drawLine(p1, p2);

    // Tail
    rotate(-7.0f, -3.0f, heading, offset);
    sgAddVec2(p1, c, offset);
    rotate(-7.0f, 3.0f, heading, offset);
    sgAddVec2(p2, c, offset);
    output->drawLine(p1, p2);
}

// Fills in the points array with four corner points of the runway (8 floats)
void Overlays::buildRwyCoords( sgVec2 rwyc, sgVec2 rwyl, sgVec2 rwyw, 
			       sgVec2 *corners ) {
  sgCopyVec2( corners[0], rwyc );
  sgAddVec2( corners[1], corners[0], rwyl ); sgAddVec2(corners[1], rwyw);
  sgSubVec2( corners[2], corners[0], rwyl ); sgAddVec2(corners[2], rwyw);
  sgSubVec2( corners[3], corners[0], rwyl ); sgSubVec2(corners[3], rwyw);
  sgAddVec2( corners[0], rwyl );             sgSubVec2(corners[0], rwyw);
}

// Draws the labels of all airports in the specified region
void Overlays::airport_labels(float theta, float alpha,
			      float dtheta, float dalpha ) {
  load_airports();

  bool save_shade = output->getShade();
  output->setShade(false);

  for (vector<ARP*>::iterator i = airports.begin(); i < airports.end(); i++) {
    ARP *ap = *i;
    sgVec3 xyr;

    if (fabs(ap->lat - theta) < dtheta && fabs(wrap_angle(ap->lon - alpha)) < dalpha) {
      projection->ab_lat( ap->lat, ap->lon, theta, alpha, xyr );
      sgVec2 p;

      sgSetVec2( p, ::scale(xyr[0], output->getSize(), scale), 
		 ::scale(xyr[1], output->getSize(), scale) );
	    
      if (ap->name[0] != 0) {
	output->setColor( arp_color1 );
	p[0] += 10; p[1] -= 5;
	if (features & OVERLAY_IDS) output->drawText( p, ap->id );
	p[1] += 10;
	if (features & OVERLAY_NAMES) output->drawText( p, ap->name );
	p[0] -= 10; p[1] -= 5;
      }

      sgVec2 *outlines = new sgVec2[ap->rwys.size()*4];
      sgVec2 *insides = new sgVec2[ap->rwys.size()*4];
      int oc = 0, ic = 0;
      for (list<RWY*>::const_iterator j = ap->rwys.begin(); j != ap->rwys.end();
	   j++) {
	sgVec2 rwyc, rwyl, rwyw;

	// set up two vectors along the length & width of the runway
	sgSetVec2( rwyl,
		   (*j)->length  / 2.0f * scale * sin(-(*j)->hdg),
		   -(*j)->length / 2.0f * scale * cos(-(*j)->hdg) );
	sgSetVec2( rwyw, 
		   (*j)->width*4.0f * scale  * cos(-(*j)->hdg), 
		   (*j)->width*4.0f * scale  * sin(-(*j)->hdg) );
	projection->ab_lat( (*j)->lat, (*j)->lon, theta, alpha, xyr );
	// runway center point
	rwyc[0] = ::scale(xyr[0], output->getSize(), scale);
	rwyc[1] = ::scale(xyr[1], output->getSize(), scale);

	// construct outlines
	buildRwyCoords( rwyc, rwyl, rwyw, outlines + oc );
	oc += 4;

	// construct inner parts
	rwyw[0] *= 0.3f; rwyw[1] *= 0.3f;
	rwyl[0] *= 0.9f; rwyl[1] *= 0.9f;
	buildRwyCoords( rwyc, rwyl, rwyw, insides + ic );
	ic += 4;
      }

      output->setColor( arp_color1 );
      unsigned int k;
      for (k = 0; k < ap->rwys.size(); k++) {
	output->drawQuad( outlines + k*4, dummy_normals );
      }
      output->setColor( arp_color2 );
      for (k = 0; k < ap->rwys.size(); k++) {
	output->drawQuad( insides + k*4, dummy_normals );
      }
      
      delete[] insides;
      delete[] outlines;
    }
  }

  output->setShade(save_shade);
}

// Draws all navaids in the specified region
void Overlays::draw_navaids( float theta, float alpha, 
			     float dtheta, float dalpha ) {
  load_new_navaids();
  load_new_fixes();

  bool save_shade = output->getShade();
  output->setShade(false);

  for (vector<NAV*>::iterator i = navaids.begin(); i != navaids.end(); i++) {
    NAV *n = *i;

    sgVec3 xyr;
    sgVec2 p;

    if (fabs(n->lat - theta) < dtheta && fabs(wrap_angle(n->lon - alpha)) < dalpha) {
      projection->ab_lat( n->lat, n->lon, theta, alpha, xyr );

      sgSetVec2( p,  ::scale(xyr[0], output->getSize(), scale), 
		 ::scale(xyr[1], output->getSize(), scale) );

      switch (n->navtype) {
      case NAV_VOR:
      case NAV_DME:
	if (features & OVERLAY_NAVAIDS_VOR) draw_vor(n, p);
	break;
      case NAV_NDB:
	if (features & OVERLAY_NAVAIDS_NDB) draw_ndb(n, p);
	break;
      case NAV_ILS:
	if (features & OVERLAY_NAVAIDS_ILS) draw_ils(n, p);
	break;
      case NAV_FIX:
	if (features & OVERLAY_NAVAIDS_FIX) draw_fix(n, p);
	break;
      }
    }
  }

  output->setShade(save_shade);
}

// Draw one specified NDB
void Overlays::draw_ndb( NAV *n, sgVec2 p ) {
  static const int RADIUS = 10;
  char freqbuf[20];
  sprintf( freqbuf, "%.0f", n->freq );

  FlightData *pos = NULL;
  if (flight_track) {
      pos = flight_track->getCurrentPoint();
  }
  if (pos && (fabs(n->freq - pos->adf_freq) < SG_EPSILON)) {
      sgVec3 xyr;
      sgVec2 p1;
      projection->ab_lat(pos->lat, pos->lon, lat, lon, xyr );
      
      sgSetVec2( p1, ::scale(xyr[0], output->getSize(), scale), 
		 ::scale(xyr[1], output->getSize(), scale) );

      output->setColor(static_ndb_color);
      output->drawLine(p, p1);

      // The ADF just shows the direction to the NDB, so it's
      // independent of magnetic variation.
      float absoluteBearing, relativeBearing;
      absoluteBearing = 
	  atan2(p[1] - p1[1], p[0] - p1[0]) * SG_RADIANS_TO_DEGREES;
      absoluteBearing = -(absoluteBearing - 90.0);
      if (absoluteBearing < 0.0) {
	  absoluteBearing += 360.0;
      }
      // Note that pos->hdg gives the aircraft's true (not magnetic)
      // heading.
      relativeBearing = absoluteBearing - pos->hdg;
      if (relativeBearing < 0.0) {
	  relativeBearing += 360.0;
      }

      // Tag the aircraft with the absolute and relative bearings to
      // the NDB.  MB = Magnetic Bearing, RB = Relative Bearing.
      char *tag;
      asprintf(&tag, "MB: %.0f\nRB: %.0f", absoluteBearing, relativeBearing);
      // Draw the text a little away from the aircraft.
      p1[0] += 5;
      output->drawText(p1, tag);
      free(tag);
  }

  output->setColor( nav_color );
  output->drawCircle( p, RADIUS );

  p[0] -= RADIUS+25; p[1] -= 10;
  if (features & OVERLAY_NAMES) output->drawText( p, n->name );
  p[1] += 10;
  if (features & OVERLAY_IDS) output->drawText( p, n->id );
  p[1] += 10;
  if (features & OVERLAY_ANY_LABEL) output->drawText( p, freqbuf );
}

// Draw one specified ILS
void Overlays::draw_ils( NAV *n, sgVec2 p ) {
  char freqbuf[20];
  sprintf( freqbuf, "%.2f", n->freq );
  float ilsSize = 3000.0f;
  ilsSize *= scale;  // Clip the min/max size?
  // n->magvar is true heading for ILS
  float tdir = (n->magvar - 3.0) * SG_DEGREES_TO_RADIANS;
  sgVec2 offset1 = {-ilsSize * sin(tdir), -ilsSize * cos(tdir)};
  tdir = (n->magvar + 3.0) * SG_DEGREES_TO_RADIANS;
  sgVec2 offset2 = {-ilsSize * sin(tdir), -ilsSize * cos(tdir)};
  tdir = n->magvar * SG_DEGREES_TO_RADIANS;
  sgVec2 offset3 = {-1.0 * (ilsSize + 10) * sin(tdir), -1.0 * (ilsSize + 10) * cos(tdir)};
  // Shift the origin of the LOC towards the threshold end of the rwy a bit - otherwise it looks daft.
  // It would be better if we could match LOC to rwy (or GS) to do this, but that's probably hard!
  sgVec2 offset0 = {(ilsSize * -0.9) * sin(tdir), (ilsSize * -0.9) * cos(tdir)};
  sgVec2 p0, p1, p2, p3;
  sgAddVec2(p0, p, offset0);
  sgAddVec2(p1, p0, offset1);
  sgAddVec2(p2, p0, offset2);
  sgAddVec2(p3, p0, offset3);
  
  output->setColor( ils_color );
  output->drawLine(p0, p1);
  output->drawLine(p0, p2);
  output->drawLine(p1, p2);
  if (features & OVERLAY_ANY_LABEL) output->drawText( p3, freqbuf );
  p3[1] -= 10;
  if (features & OVERLAY_IDS) output->drawText( p3, n->id ); 
}

// Draw one specified VOR
void Overlays::draw_vor( NAV *n, sgVec2 p ) {
  static const int SUBDIVISIONS = 12;
  static const int RADIUS = 15;

  char freqbuf[20];
  sprintf( freqbuf, "%.2f", n->freq );

  output->setColor( nav_color );
  output->drawCircle( p, RADIUS );

  for (int i = 0; i <= SUBDIVISIONS; i++) {
    int llength = (i%3 == 0)?5:3;
    sgVec2 p1, p2;
    float r = 2.0f*SG_PI / SUBDIVISIONS * i - n->magvar;
    
    sgSetVec2( p1, 
	       p[0] + cos(r)* RADIUS,
	       p[1] + sin(r)* RADIUS);
    sgSetVec2( p2,
	       p[0] + cos(r)*(RADIUS-llength),
	       p[1] + sin(r)*(RADIUS-llength));

    output->drawLine(p1, p2);
  }

  //sgVec2 p2;
  //sgSetVec2( p2, 
  //     p[0] + cos(n->magvar)*RADIUS, p[1] + sin(n->magvar)*RADIUS );
  //output->drawLine(p, p2);

  // Get the current navigation settings.
  FlightData *pos = NULL;
  if (flight_track) {
      pos = flight_track->getCurrentPoint();
  }

  // EYE - draw VOR1 and VOR2 differently?

  // if this nav is selected, draw radial
  if (pos && (fabs(n->freq - pos->nav1_freq) < SG_EPSILON)) {
      sgVec2 end;
      sgSetVec2( end,
		 p[0] + sin(pos->nav1_rad + n->magvar) * 500,
		 p[1] + cos(pos->nav1_rad + n->magvar) * 500 );
      output->setColor( static_vor_from_color );
      output->drawLine(p, end);
      sgSetVec2( end,
		 p[0] - sin(pos->nav1_rad + n->magvar) * 500,
		 p[1] - cos(pos->nav1_rad + n->magvar) * 500 );
      output->setColor( static_vor_to_color );
      output->drawLine(p, end);
  }
  if (pos && (fabs(n->freq - pos->nav2_freq) < SG_EPSILON)) {
      sgVec2 end;
      sgSetVec2( end,
		 p[0] + sin(pos->nav2_rad + n->magvar) * 500,
		 p[1] + cos(pos->nav2_rad + n->magvar) * 500 );
      output->setColor( static_vor_from_color );
      output->drawLine(p, end);
      sgSetVec2( end,
		 p[0] - sin(pos->nav2_rad + n->magvar) * 500,
		 p[1] - cos(pos->nav2_rad + n->magvar) * 500 );
      output->setColor( static_vor_to_color );
      output->drawLine(p, end);
  }

  output->setColor( nav_color );
  p[0] -= RADIUS+25; p[1] -= 10;
  if (features & OVERLAY_NAMES) output->drawText( p, n->name );
  p[1] += 10;
  if (features & OVERLAY_IDS) output->drawText( p, n->id );
  p[1] += 10;
  if (features & OVERLAY_ANY_LABEL) output->drawText( p, freqbuf );
}

void Overlays::draw_fix( NAV *n, sgVec2 p ) {
  static const float SIZE = 10.0f;
  static const sgVec2 offset1 = {-SIZE*0.5, -SIZE*0.25};
  static const sgVec2 offset2 = { SIZE*0.5, -SIZE*0.25};
  static const sgVec2 offset3 = { 0, sqrt( 5 * SIZE*SIZE / 16 )};

  output->setColor( nav_color );
  sgVec2 c1, c2, c3;
  sgAddVec2(c1, p, offset1);
  sgAddVec2(c2, p, offset2);
  sgAddVec2(c3, p, offset3);
  
  output->drawLine(c1, c2);
  output->drawLine(c2, c3);
  output->drawLine(c3, c1);

  p[0] += SIZE/2 + 2; p[1] -= 3;
  if (features & OVERLAY_ANY_LABEL) output->drawText( p, n->name );
}

/* Loads X-Plane format airports (used in FlightGear-0.9.8 onwards) */
void Overlays::load_airports() {
  if(airports_loaded)
    return;

  char *arpname = new char[strlen(fg_root) + 512];
  char line[1024];
  const char space[] = " \t\n\r";
  const char junk[] = "?\n\r";
  char* token;
    
  strcpy( arpname, fg_root );
  strcat( arpname, "/Airports/apt.dat.gz" );

  gzFile arp;

  arp = gzopen( arpname, "rb" );
  if (arp == NULL) {
    fprintf( stderr, "load_airports: Couldn't open \"%s\" .\n", arpname );
    delete[] arpname;
    // Try loading the old format instead
    fprintf( stderr, "Attempting to load old format airports file instead...\n" );
    load_old_airports();
    return;
  } 

  bool line_read = false;
  bool apt_pos_set = false;

  ARP *ap = NULL;
  while (!gzeof(arp) || line_read) {
    if (ap == NULL)
      ap = new ARP;
    
    if (!line_read) {
      gzgets( arp, line, 1024 );
    }

    token = strtok(line, space);
    //cout << token << '\n';
    if(token == NULL) {
      // Must have been a blank line.
      // Do nothing this pass
      line_read = false;
    } else {
      if(!strcmp(token, "99")) {	// TODO - check this with Robin
	break;
      }
      //if(!strcmp(token, "1") || !strcmp(token, "16") || !strcmp(token, "17")) {
      if(!strcmp(token, "1") || !strcmp(token, "16")) {  // 17 is Heliports - don't read these until Atlas can display a H symbol
	apt_pos_set = false;
	token = strtok(NULL, space);  // elev
	token = strtok(NULL, space);  // not used by Atlas
	token = strtok(NULL, space);  // ditto
	token = strtok(NULL, space);  // Code
	strncpy( ap->id, token, sizeof(ap->id) );
	token = strtok(NULL, junk);  // Name
	strncpy( ap->name, token, sizeof(ap->name) );
	
	line_read = false;
	while (!gzeof(arp) && !line_read) {
	  char rwyid[8];
	  float heading;
	  float lat, lon;
	  float length, width;
	  
	  gzgets( arp, line, 256 );
	  line_read = true;
	  
	  token = strtok(line, space);
	  if(token == NULL) {
	    // Do nothing - loop will break
	  } else if(!strcmp(token, "10")) {
	    token = strtok(NULL, space);
	    lat = atof(token);
	    token = strtok(NULL, space);
	    lon = atof(token);
	    token = strtok(NULL, space);
	    if(!strcmp(token, "xxx")) {
	      // It's a taxiway - break
	      // NOTE - this is fragile to changes in format.
	      line_read = true;
	    } else {
	      // Runway!
	      RWY *rwy    = new RWY;
	      strncpy(rwyid, token, sizeof(rwyid)); // TODO - should strip trailing 'x's off the raw ID.
	      token = strtok(NULL, space);
	      heading = atof(token);
	      token = strtok(NULL, space);
	      length = atof(token);
	      token = strtok(NULL, space);  // Dispaced threshold / stopway information for one end
	      token = strtok(NULL, space);  // ditto
	      token = strtok(NULL, space);  // Width
	      width = atof(token);
	      rwy->lat    = lat    * SG_DEGREES_TO_RADIANS;
	      rwy->lon    = lon    * SG_DEGREES_TO_RADIANS;
	      rwy->hdg    = heading* SG_DEGREES_TO_RADIANS;
	      rwy->length = (int)(length * 0.3048);   // feet to meters
	      rwy->width  = (int)(width  * 0.3048);
	      ap->rwys.push_back( rwy );
	      // Set the airport location from the first runway
	      if(!apt_pos_set) {
		ap->lat = rwy->lat;
		ap->lon = rwy->lon;
		apt_pos_set = true;
	      }
	      line_read = false;
	    }
	  }
	}
	airports.push_back( ap );
	
	// Create a search token for this airport.
	tokenizeLocation(Overlays::AIRPORT, ap, tokens);
	ap = NULL;
      } else {
	line_read = false;
      }
    }
  }
	    
  if (ap != NULL)
    delete ap;
  
  gzclose( arp );
  delete[] arpname;

  airports_loaded = true;

  // Sort all the search tokens.
  sort(tokens.begin(), tokens.end());
}  	
	
/* Loads the old format (Flightgear-0.9.3 to 0.9.7) airport 
   database if it isn't already loaded.  Calls the very old format
   database loader instead if the new format is not found.
   (if *one* instance of the Overlays class has loaded the db,
    no other instance will have to load it again, but it's safe
    to call this function multiple times, since it will just
    return immediately) */
void Overlays::load_old_airports() {
  if(airports_loaded)
    return;

  char *arpname = new char[strlen(fg_root) + 512];
  char *rwyname = new char[strlen(fg_root) + 512];
  char line[256];
  
  strcpy( arpname, fg_root );
  strcpy( rwyname, fg_root );
  strcat( arpname, "/Airports/basic.dat.gz" );
  strcat( rwyname, "/Airports/runways.dat.gz" );

  gzFile arp;
  gzFile rwp;

  arp = gzopen( arpname, "rb" );
  rwp = gzopen( rwyname, "rb" );
  if (arp == NULL || rwp == NULL) {
    fprintf( stderr, "load_old_airports: Couldn't open \"%s\" .\n", (arp ? rwyname : arpname) );
    delete[] arpname;
    delete[] rwyname;
    // Try loading the old format instead
    fprintf( stderr, "Attempting to load very old format airports file instead...\n" );
    load_very_old_airports();
    return;
  }
  
  // If we get here then the files have opened OK.
  // We need to index the airports by ID so we can cross reference them with
  // the runways which are in a separate file not necessarily in the same order
  map < string, ARP* > arpmap;

  ARP *ap = NULL;
  while (!gzeof(arp)) {
    float lat, lon;
    int elev, name_pos;

    if(ap == NULL) ap = new ARP;
    
    gzgets( arp, line, 256 );

    switch (line[0]) {
    case 'A':
      if (sscanf( line, "A %4s %f %f %d %*s %n", ap->id, &lat, &lon, &elev, &name_pos ) == 4) {
	line[ strlen(line)-1 ] = 0;
	ap->lat = lat * SG_DEGREES_TO_RADIANS;
	ap->lon = lon * SG_DEGREES_TO_RADIANS;

	strncpy( ap->name, line + name_pos, sizeof(ap->name) );
	arpmap[ap->id] = ap;
	ap = NULL;
      }
    }
  }
  
  bool line_read = false;
  bool pos_needed = false;
  
  // Now do the runways file
  char idc[5];
  while (!gzeof(rwp) || line_read) {
    float lat, lon;
    int elev, name_pos;

    if (!line_read) {
      gzgets( rwp, line, 256 );
    }
    
    switch (line[0]) {
    case 'A':
      if (sscanf( line, "A %4s %d %*s %n", idc, &elev, &name_pos ) == 2) {
	ap = arpmap[idc];

	if(ap == NULL) {
	  // GRRR - some of the airports are only in the runways file, not the airports file.
	  // (In fact, the ones I've looked at it's because they're listed as 'S' (seaplane) in basic.dat, but 'A' in runways.dat).
	  // Create them here and just give them the location of the first runway.
	  ap = new ARP;
	  memcpy(ap->id, idc, 5);
	  pos_needed = true;
	  strncpy( ap->name, line + name_pos, sizeof(ap->name) );
	}
	
	line_read = false;
	while (!gzeof(rwp) && !line_read) {
	  char rwyid[8];
	  float heading;
	  float length, width;
	  
	  gzgets( rwp, line, 256 );
	  line_read = true;
	  
	  if (sscanf(line, "R %*s %7s %f %f %f %f %f", 
	  rwyid, &lat, &lon, &heading, &length, &width) == 6) {
	    RWY *rwy    = new RWY;
	    rwy->lat    = lat    * SG_DEGREES_TO_RADIANS;
	    rwy->lon    = lon    * SG_DEGREES_TO_RADIANS;
	    rwy->hdg    = heading* SG_DEGREES_TO_RADIANS;
	    rwy->length = (int)(length * 0.3048);   // feet to meters
	    rwy->width  = (int)(width  * 0.3048);
	    ap->rwys.push_back( rwy );
	    if(pos_needed) {
	      ap->lat = rwy->lat;
	      ap->lon = rwy->lon;
	      pos_needed = false;
	    }
	    line_read = false;
	  }
	  
	  airports.push_back( ap );
	  pos_needed = false;  // Just in case pos_needed got set by an airport without a runway.  Shouldn't happen!!
	
	  // Create a search token for this airport.
	  tokenizeLocation(Overlays::AIRPORT, ap, tokens);
	}
	ap = NULL;
      } else {
	line_read = false;
      }
      break;
    default:
      line_read = false;
      break;
    }

  }
  
  if (ap != NULL)
    delete ap;
  
  gzclose( arp );
  gzclose( rwp );
  delete[] arpname;
  delete[] rwyname;

  airports_loaded = true;

  // Sort all the search tokens.
  sort(tokens.begin(), tokens.end());
}

/* Loads the original FlightGear airport database format.
   (v-0.9.2 and earlier).  
   Note that this format has been depreciated for ages now,
   and this code will eventually be removed.
   The database is only read if it isn't already loaded
   (if *one* instance of the Overlays class has loaded the db,
    no other instance will have to load it again, but it's safe
    to call this function multiple times, since it will just
    return immediately) */
void Overlays::load_very_old_airports() {
  if (airports_loaded)
    return;
	
  char *arpname = new char[strlen(fg_root) + 512];
  char line[256];
  
  strcpy( arpname, fg_root );
  strcat( arpname, "/Airports/default.apt.gz" );

  gzFile arp;

  arp = gzopen( arpname, "rb" );
  if (arp == NULL) {
    fprintf( stderr, "load_airports: Couldn't open \"%s\" .\n", arpname );
    delete[] arpname;
    return;
  }

  bool line_read = false;

  ARP *ap = NULL;
  while (!gzeof(arp) || line_read) {
    float lat, lon;
    int elev, name_pos;

    if (ap == NULL)
      ap = new ARP;
    
    if (!line_read) {
      gzgets( arp, line, 256 );
    }

    switch (line[0]) {
    case 'A':
      if (sscanf( line, "A %4s %f %f %d %*s %n", ap->id, &lat, &lon, &elev, &name_pos ) == 4) {
	line[ strlen(line)-1 ] = 0;
	ap->lat = lat * SG_DEGREES_TO_RADIANS;
	ap->lon = lon * SG_DEGREES_TO_RADIANS;

	strncpy( ap->name, line + name_pos, sizeof(ap->name) );

	line_read = false;
	while (!gzeof(arp) && !line_read) {
	  char rwyid[8];
	  float heading;
	  float length, width;
	  
	  gzgets( arp, line, 256 );
	  line_read = true;
	  
	  if (sscanf(line, "R %7s %f %f %f %f %f", 
		     rwyid, &lat, &lon, &heading, &length, &width) == 6) {
	    RWY *rwy    = new RWY;
	    rwy->lat    = lat    * SG_DEGREES_TO_RADIANS;
	    rwy->lon    = lon    * SG_DEGREES_TO_RADIANS;
	    rwy->hdg    = heading* SG_DEGREES_TO_RADIANS;
	    rwy->length = (int)(length * 0.3048);   // feet to meters
	    rwy->width  = (int)(width  * 0.3048);
	    ap->rwys.push_back( rwy );
	    line_read = false;
	  }
	}

	airports.push_back( ap );
	
	// Create a search token for this airport.
	tokenizeLocation(Overlays::AIRPORT, ap, tokens);

	ap = NULL;
      } else {
	line_read = false;
      }
      break;
    default:
      line_read = false;
      break;
    }

  }
  if (ap != NULL)
    delete ap;
  
  gzclose( arp );
  delete[] arpname;

  airports_loaded = true;

  // Sort all the search tokens.
  sort(tokens.begin(), tokens.end());
}

/* Loads the new format (FG-0.9.6? onwards) navaid database if it isn't already loaded
   (if *one* instance of the Overlays class has loaded the db,
    no other instance will have to load it again, but it's safe
    to call this function multiple times, since it will just
    return immediately) */
void Overlays::load_new_navaids() {
  if (navaids_loaded)
    return;
	
  char *navname = new char[strlen(fg_root) + 512];
  char line[256];
  //double magdummy[6];
  
  strcpy( navname, fg_root );
  strcat( navname, "/Navaids/nav.dat.gz" );

  gzFile nav;

  nav = gzopen( navname, "rb" );
  if (nav == NULL) {
    fprintf( stderr, "load_navaids: Couldn't open \"%s\" .\n", navname );
    delete[] navname;
    // Try loading the old format instead
    fprintf( stderr, "Attempting to load old format navaids file instead...\n" );
    load_navaids();
    return;
  }

  NAV *n = NULL;
  while (!gzeof(nav)) {
    //char sNavtype[10];
    int iNavtype;
    //char sMagVar[10];
    int elev;

    if (n == NULL)
      n = new NAV;
    
    gzgets( nav, line, 256 );

    int nameStart;
    
    if ( sscanf(line, "%d %f %f %d %f %d %f %s %n",
		&iNavtype, &n->lat, &n->lon, &elev, &n->freq, &n->range, &n->magvar, n->id, &nameStart) == 8 ) {
	strcpy(n->name, line + nameStart);
	n->freq /= 100.0f;
	bool bNavaid = true;
	switch (iNavtype) {
	case 2: n->navtype = NAV_NDB;
	    n->freq *= 100.0f;
	    break;
	case 3: n->navtype = NAV_VOR; break;
	case 4: n->navtype = NAV_ILS; break;	// code 4 is for localisers as part of full ILS - might also consider 5 (LDA & SDF) and 6 (glideslope).
	case 12: n->navtype = NAV_DME; break;
	case 13: n->navtype = NAV_DME; break;
	case 5: n->navtype = NAV_ILS; break;
	//case 6: n->navtype = NAV_GS; break;
	default: bNavaid = false;
	}

	// cout << "lat = " << n->lat << " lon = " << n->lon << " elev = " 
	//      << elev << " JD = " << time_params->getJD() << endl;
	
	if(bNavaid) {

	    //cout << iNavtype << " " << n->lat << " " << n->lon << " " << elev << " " << n->freq << " " << n->magvar << " " << n->id << " " << n->name << '\n';
	    n->lat *= SG_DEGREES_TO_RADIANS;
	    n->lon *= SG_DEGREES_TO_RADIANS;
	    elev = (int) ( elev * SG_FEET_TO_METER );
    
    	    /* TODO - handle new format magvar values
	    if ( strcmp( sMagVar, "XXX" ) == 0 ) {
		// no magvar specified, calculate our own
		n->magvar = sgGetMagVar(n->lon, n->lat, elev, time_params->getJD());
	    } else {
		n->magvar = ( (sMagVar[0] - '0') * 10.0f + (sMagVar[1] - '0') );
		n->magvar *= SG_DEGREES_TO_RADIANS;
		if ( sMagVar[2] == 'W' ) {
		    n->magvar = -n->magvar;
		}
	    }
	    */
	    if(n->navtype == NAV_ILS) {
	      // The magvar is the inbound heading - keep it.
	    } else {
	      n->magvar = sgGetMagVar(n->lon, n->lat, elev, time_params->getJD());
	    }
	    // cout << "navid = " << n->id << " magvar = " << n->magvar * RAD_TO_DEG
	    //      << endl;
    
	    navaids.push_back(n);

	    // Create a search token for this navaid.
	    tokenizeLocation(Overlays::NAVAID, n, tokens);
	    n = NULL;
	}
    }
  }
  if (n != NULL)
    delete n;

  gzclose(nav);
  delete[] navname;
 
  navaids_loaded = true;

  // Sort all the search tokens.
  sort(tokens.begin(), tokens.end());
}

/* Loads the navaid database if it isn't already loaded
   (if *one* instance of the Overlays class has loaded the db,
    no other instance will have to load it again, but it's safe
    to call this function multiple times, since it will just
    return immediately) */
void Overlays::load_navaids() {
  if (navaids_loaded)
    return;
	
  char *navname = new char[strlen(fg_root) + 512];
  char line[256];
  //double magdummy[6];
  
  strcpy( navname, fg_root );
  strcat( navname, "/Navaids/default.nav.gz" );

  gzFile nav;

  nav = gzopen( navname, "rb" );
  if (nav == NULL) {      
    fprintf( stderr, "load_navaids: Couldn't open \"%s\" .\n", navname );
    delete[] navname;
    return;
  }

  NAV *n = NULL;
  while (!gzeof(nav)) {
    char cDummy, cNavtype;
    char sMagVar[10];
    int elev, iDummy;

    if (n == NULL)
      n = new NAV;
    
    gzgets( nav, line, 256 );
    
    if ( sscanf(line, "%c %f %f %d %f %d %c %s %s",
		&cNavtype, &n->lat, &n->lon, &elev, &n->freq, &iDummy,
		&cDummy, n->id, sMagVar) == 9 ) {
	switch (cNavtype) {
	case 'V': n->navtype = NAV_VOR; break;
	case 'D': n->navtype = NAV_DME; break;
	case 'N': n->navtype = NAV_NDB; break;
	}

	// cout << "lat = " << n->lat << " lon = " << n->lon << " elev = " 
	//      << elev << " JD = " << time_params->getJD() << endl;

	n->lat *= SG_DEGREES_TO_RADIANS;
	n->lon *= SG_DEGREES_TO_RADIANS;
	elev = (int) ( elev * SG_FEET_TO_METER );

	if ( strcmp( sMagVar, "XXX" ) == 0 ) {
	    // no magvar specified, calculate our own
	    n->magvar = sgGetMagVar(n->lon, n->lat, elev, time_params->getJD());
	} else {
	    n->magvar = ( (sMagVar[0] - '0') * 10.0f + (sMagVar[1] - '0') );
	    n->magvar *= SG_DEGREES_TO_RADIANS;
	    if ( sMagVar[2] == 'W' ) {
		n->magvar = -n->magvar;
	    }
	}
	// cout << "navid = " << n->id << " magvar = " << n->magvar * RAD_TO_DEG
	//      << endl;

	strcpy(n->name, line+55);  // 51?

	navaids.push_back(n);

	// Create a search token for this navaid.
	tokenizeLocation(Overlays::NAVAID, n, tokens);
	n = NULL;
    }
  }
  if (n != NULL)
    delete n;

  gzclose(nav);
  delete[] navname;
 
  navaids_loaded = true;

  // Sort all the search tokens.
  sort(tokens.begin(), tokens.end());
}

void Overlays::load_new_fixes() {
  if (fixes_loaded)
    return;
	
  char *navname = new char[strlen(fg_root) + 512];
  char line[256];
  
  strcpy( navname, fg_root );
  strcat( navname, "/Navaids/fix.dat.gz" );

  gzFile fix;

  fix = gzopen( navname, "rb" );
  if (fix == NULL) {
    fprintf( stderr, "load_fixes: Couldn't open \"%s\" .\n", navname );
    delete[] navname;
    // Try loading the old format instead
    fprintf( stderr, "Attempting to load old format fixes file instead...\n" );
    load_fixes();
    return;
  }

  NAV *n = NULL;
  while (!gzeof(fix)) {
    if (n == NULL)
      n = new NAV;
    
    gzgets( fix, line, 256 );
    if ( sscanf(line, "%f %f %s", &n->lat, &n->lon, n->name) == 3 ) {
      strcpy(n->id, n->name);
      n->navtype = NAV_FIX;
      n->lat *= SG_DEGREES_TO_RADIANS;
      n->lon *= SG_DEGREES_TO_RADIANS;
      navaids.push_back(n);

      // Create a search token for this fix.
      tokenizeLocation(Overlays::NAVAID, n, tokens);
      n = NULL;
    }
  }

  delete[] navname;
  gzclose(fix);
  fixes_loaded = true;

  // Sort all the search tokens.
  sort(tokens.begin(), tokens.end());
}

void Overlays::load_fixes() {
  if (fixes_loaded)
    return;
	
  char *navname = new char[strlen(fg_root) + 512];
  char line[256];
  
  strcpy( navname, fg_root );
  strcat( navname, "/Navaids/default.fix.gz" );

  gzFile fix;

  fix = gzopen( navname, "rb" );
  if (fix == NULL) {
    fprintf( stderr, "load_fixes: Couldn't open \"%s\" .\n", navname );
    delete[] navname;
    return;
  }

  NAV *n = NULL;
  while (!gzeof(fix)) {
    if (n == NULL)
      n = new NAV;
    
    gzgets( fix, line, 256 );
    if ( sscanf(line, "%s %f %f", n->name, &n->lat, &n->lon) == 3 ) {
      strcpy(n->id, n->name);
      n->navtype = NAV_FIX;
      n->lat *= SG_DEGREES_TO_RADIANS;
      n->lon *= SG_DEGREES_TO_RADIANS;

      // Create a search token for this fix.
      navaids.push_back(n);
      n = NULL;
    }
  }

  delete[] navname;
  gzclose(fix);
  fixes_loaded = true;

  // Sort all the search tokens.
  sort(tokens.begin(), tokens.end());
}

Overlays::ARP *Overlays::findAirport( const char *name ) {
  for (vector<ARP*>::iterator i = airports.begin(); i < airports.end(); i++) {
    if ( strcmp( (*i)->name, name ) == 0 ) {
      return *i;
    }
  }

  return NULL;
}

Overlays::ARP *Overlays::findAirportByCode( const char *id ) {
  for (vector<ARP*>::iterator i = airports.begin(); i < airports.end(); i++) {
    if ( strcmp( (*i)->id, id ) == 0 ) {
      return *i;
    }
  }

  return NULL;
}

Overlays::NAV *Overlays::findNav( const char *name ) {
  for (vector<NAV*>::const_iterator i = navaids.begin(); i < navaids.end(); i++) {
    if ( strcmp( (*i)->name, name ) == 0 ) {
      return *i;
    }
  }

  return NULL;
}

Overlays::NAV *Overlays::findNav( float lat, float lon, float freq ) {
  NAV   *closest = NULL;
  float closest_dist = 1e12f;

  for (vector<NAV*>::const_iterator i = navaids.begin(); i < navaids.end(); i++) {
    if ( fabs(freq - (*i)->freq) < 0.01f ) {
      // ugly distance metric -- could (should?) be replaced by
      // great circle distance, but works ok for now
      float dist = fabs(lat - (*i)->lat) + fabs(wrap_angle(lon - (*i)->lon));
      if (dist < closest_dist) {
	closest_dist = dist;
	closest = *i;
      }
    }
  }

  return closest;
}

// Checks if the record at index i in the tokens vector matches the
// search tokens.  To match, each search token must match a token in
// the record.
bool Overlays::recordMatches(int i, vector<string> completeSearchTokens, 
			     string partialSearchToken)
{
    TOKEN t = tokens[i];
    vector<TOKEN> tmp;
    int j, k;

    // The given token, i, points to a single airport or navaid.
    // Create a vector of TOKEN records for it.
    if (t.t == Overlays::AIRPORT) {
	ARP *ap = (Overlays::ARP *)t.locAddr;
	tokenizeLocation(t.t, t.locAddr, tmp);
    } else {
	NAV *n = (Overlays::NAV *)t.locAddr;
	tokenizeLocation(t.t, t.locAddr, tmp);
    }

    // Search the vector for all the complete search tokens.
    for (k = 0; k < completeSearchTokens.size(); k++) {
	for (j = 0; j < tmp.size(); j++) {
	    // Complete search tokens require an exact match.
	    if (strcasecmp(completeSearchTokens[k].c_str(), 
			   tmp[j].token.c_str()) == 0) {
		break;
	    }
	}
	if (j == tmp.size()) {
	    // We found no match, so bail.
	    return false;
	}
    }
    // Search the vector for the partial search token.
    if (partialSearchToken != "") {
	for (j = 0; j < tmp.size(); j++) {
	    // Partial search tokens require a "head" match.
	    if (strncasecmp(partialSearchToken.c_str(), 
			    tmp[j].token.c_str(),
			    partialSearchToken.size()) == 0) {
		break;
	    }
	}
	if (j == tmp.size()) {
	    // We found no match, so bail.
	    return false;
	}
    }

    return true;
}

// Starts (if str is new) or continues (if str is the same as the
// previous call) a search in the tokens vector (which is assumed to
// be sorted) for str.  The results (TOKEN records) are placed in the
// matches vector.  Returns true if the matches vector changed.  Finds
// at most maxMatches results (this was added so that a GUI-based
// application can get a reasonable response for a potentially large
// search).  If maxMatches < 0, then there is no limit, and all
// matches are found in a single call.
//
// Matching
//
// A location record (ARP or NAV) has a bunch of tokens.  These are
// just the whitespace-separated strings in the name and the id.  For
// example, the ARP record:
//
//   id = 'CYYC', name = 'Calgary Intl'
//
// has 3 tokens, "CYYC", "Calgary", and "Intl".
//
// A search string also consists of a set of whitespace-separated
// search tokens.  A token with trailing whitespace is called a
// "complete" token, while a token with no whitespace following is an
// "incomplete" token.  In a search string, there can be 0 or more
// complete tokens, and 0 or 1 incomplete tokens.
//
// Complete tokens and incomplete tokens match differently.  A
// complete token must match exactly, whereas an incomplete token only
// needs to match the head of a string.  For example, the complete
// token "Foo" only matches "foo", "FOO", "fOo", etc, (case doesn't
// matter), whereas the incomplete token "Foo" matches "foo",
// "FoOt", "FOOBAR", etc.
//
// A search string (which contains a set of tokens) matches a location
// record (ie, and ARP or NAV record) if all of the tokens in the
// search string have matches in the tokens in the record.
//
// Using the ARP record above, the partial search strings "C", "cy",
// "CA", "INTL" all match.  As well, "Calgary CY" matches (a complete
// match with "Calgary" and a partial match with "CY"), but "Calgary
// CY " doesn't (both "Calgary" and "CY" are complete because they
// have trailing whitespace, and "CY" fails to match anything).
bool Overlays::findMatches(char *str, int maxMatches)
{
    static char *lastSearchString = NULL;
    static int end;
    
    istringstream stream(str);
    vector<string> completeSearchTokens;
    string partialSearchToken, aToken;

    int i;
    int noOfMatches;
    bool changed = false;

    // A bit of a silly case, but it seemed important to allow it.
    if (maxMatches == 0) {
	return changed;
    }

    if (maxMatches < 0) {
	maxMatches = tokens.size();
    }

    // Check if this is a new search or a continuation of an old search.
    if ((lastSearchString == NULL) || (strcmp(str, lastSearchString) != 0)) {
	// New.  Reset the static variables.
	free(lastSearchString);
	lastSearchString = strdup(str);
	end = -1;
	matches.clear();
	changed = true;
    }

    // Tokenize the search string.  All tokens except the last are
    // complete tokens.  The last may or may not be complete.
    stream >> aToken;
    while (stream) {
	// To determine if the current token is complete or not, we
	// just see if we've gone to the end of the stream.  If we're
	// at the very end, then the current token is incomplete.
	int loc = stream.tellg();
	if (loc == strlen(str)) {
	    partialSearchToken = aToken;
	} else {
	    completeSearchTokens.push_back(aToken);
	}
	stream >> aToken;
    }

    // Now grab a search token.  It doesn't really matter which one we
    // choose.
    bool isPartial;
    if (completeSearchTokens.size() > 0) {
	aToken = completeSearchTokens.back();
	completeSearchTokens.pop_back();
	isPartial = false;
    } else if (partialSearchToken.length() > 0) {
	aToken = partialSearchToken;
	isPartial = true;
	partialSearchToken = "";
    } else {
	// No tokens in the search string at all.  Does that mean we
	// match everything, or nothing?  I choose nothing.
	return changed;
    }

    if (end == -1) {
	int res;

	// Search for the first matching token.  A binary search would
	// speed things up, but the termination conditions are hairy
	// and my brain couldn't cope.
	for (i = 0; i < tokens.size(); i++) {
	    if (isPartial) {
		res = strncasecmp(aToken.c_str(), 
				  tokens[i].token.c_str(), 
				  aToken.length());
	    } else {
		res = strcasecmp(aToken.c_str(), 
				 tokens[i].token.c_str());
	    }
	    if (res == 0) {
		break;
	    }
	}

	// Did we find anything?
	if (i == tokens.size()) {
	    // Nope.
	    return changed;
	}

	// Yes.
	end = i - 1;
    }

    // At this point, end is just before the range we want to check.
    i = end + 1;
    noOfMatches = 0;
    while ((noOfMatches < maxMatches) &&
	   (i < tokens.size())) {
	int res;
	if (isPartial) {
	    res = strncasecmp(aToken.c_str(),
			      tokens[i].token.c_str(), 
			      aToken.length());
	} else {
	    res = strcasecmp(aToken.c_str(),
			     tokens[i].token.c_str());
	}
	if (res != 0) {
	    // We've run out of matches for this token, so bail.
	    break;
	}
	// This record matches aToken.  See if matches all the others.
	if (recordMatches(i, completeSearchTokens, partialSearchToken)) {
	    // It does.  If it isn't in the list already, add it.
	    // EYE - use a set instead of an array?
	    int j;
	    for (j = 0; j < matches.size(); j++) {
		if (tokens[i].locAddr == matches[j].locAddr) {
		    break;
		}
	    }
	    if (j == matches.size()) {
		matches.push_back(tokens[i]);
		noOfMatches++;
		changed = true;
	    }
	}
	i++;
    }
    end = i - 1;

    return changed;
}

Overlays::TOKEN Overlays::getToken(int i)
{
    if ((i < 0) || (i >= matches.size())) {
	// Out of bounds.  Return an empty token.
	Overlays::TOKEN empty;
	empty.token = "";
	empty.t = NONE;
	empty.locAddr = NULL;

	return empty;
    }

    return matches[i];
}

int Overlays::noOfMatches()
{
    return matches.size();
}

// A token is less than another token if it is lexically before it
// (ignoring case), or if the token is the same and the name is
// lexically before it (ignoring case).
bool operator< (const Overlays::TOKEN& left, const Overlays::TOKEN& right)
{
    int result = strcasecmp(left.token.c_str(), right.token.c_str());
    if (result == 0) {
	char *leftName, *rightName;

	if (left.t == Overlays::AIRPORT) {
	    leftName = ((Overlays::ARP *)left.locAddr)->name;
	} else {
	    leftName = ((Overlays::NAV *)left.locAddr)->name;
	}
	if (right.t == Overlays::AIRPORT) {
	    rightName = ((Overlays::ARP *)right.locAddr)->name;
	} else {
	    rightName = ((Overlays::NAV *)right.locAddr)->name;
	}

	// strcmp() < 1 if leftName < rightName
	return (strcasecmp(leftName, rightName) < 0);
    } else {
	return (result < 0);
    }
}

void Overlays::tokenizeLocation(LocationType lType, void *loc, 
				vector<TOKEN> &vec)
{
    TOKEN t;
    char *id;
    char *name;
    float freq;
    bool isNDB;
    
    if (lType == AIRPORT) {
	id = ((ARP *)loc)->id;
	name = ((ARP *)loc)->name;
    } else {
	NAV *n = (NAV *)loc;
	id = n->id;
	name = n->name;
	freq = n->freq;
	isNDB = (n->navtype == NAV_NDB);
    }

    // Create a token using the id.
    t.token = id;
    t.t = lType;
    t.locAddr = loc;
    vec.push_back(t);

    // Create token(s) using the name.
    istringstream stream(name);
    stream >> t.token;
    while (stream) {
	t.t = lType;
	t.locAddr = loc;
	vec.push_back(t);

	stream >> t.token;
    }

    // Create a token using the frequency (if it has one).
    if (lType == NAVAID) {
	ostringstream buf;

	// NDB frequencies are written as whole numbers; others are
	// written with 2 significant digits after the decimal point.
	if (isNDB) {
	    buf.precision(0);
	} else {
	    buf.precision(2);
	}
	buf << fixed << freq;
	t.token = buf.str();
	t.t = lType;
	t.locAddr = loc;
	vec.push_back(t);
    }
}
