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

#include "Overlays.hxx"
#include "Geodesy.hxx"

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
const float Overlays::grid_color[4]     = {0.639, 0.371, 0.620, 0.3};
const float Overlays::track_color[4]    = {0.071, 0.243, 0.427, 0.5};

const float Overlays::dummy_normals[][3] = {{0.0f, 0.0f, 0.0f},
					    {0.0f, 0.0f, 0.0f},
					    {0.0f, 0.0f, 0.0f},
					    {0.0f, 0.0f, 0.0f}};


// nav radios (global hack)
float nav1_freq, nav1_rad;
float nav2_freq, nav2_rad;
float adf_freq;


Overlays::Overlays( char *fg_root = NULL, float scale = 1.0f,
		    float width = 512.0f ) :
  scale(scale) {
  if (fg_root == NULL) {
    if ( (this->fg_root = getenv("FG_ROOT")) == NULL ) {
      this->fg_root = "/usr/local/lib/FlightGear";
    }
  } else {
    this->fg_root = fg_root;
  }
  
  setAirportColor( airport_color1, airport_color2 );
  setNavaidColor( navaid_color );
  setVorToColor( static_vor_to_color );
  setVorFromColor( static_vor_to_color );
  setGridColor( grid_color );
  setTrackColor( track_color );
  flight_track = new FlightTrack;
  projection= new Projection;

  time_params = new SGTime();
  time_params->update( 0.0, 0.0, 0 );

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
  int deg, min, sec;
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

  sgVec3 xyr;
  sgVec2 p1, p2;
  bool first;
  char lable_buffer[50];

  output->setColor(grd_color);

  // draw line labels
  for (float glon = rint((lon - dalpha) / grid_alpha) * grid_alpha; 
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
  for (float glon = rint((lon - dalpha) / grid_alpha) * grid_alpha; 
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

void Overlays::draw_flighttrack() {
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
  }
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

  for (ARP **i = airports.begin(); i < airports.end(); i++) {
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

      sgVec2 outlines[ap->rwys.size()*4];
      sgVec2 insides[ap->rwys.size()*4];
      int oc = 0, ic = 0;
      for (list<RWY*>::iterator j = ap->rwys.begin(); j != ap->rwys.end();
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
      for (unsigned int k = 0; k < ap->rwys.size(); k++) {
	output->drawQuad( outlines + k*4, dummy_normals );
      }
      output->setColor( arp_color2 );
      for (unsigned int k = 0; k < ap->rwys.size(); k++) {
	output->drawQuad( insides + k*4, dummy_normals );
      }
    }
  }

  output->setShade(save_shade);
}

// Draws all navaids in the specified region
void Overlays::draw_navaids( float theta, float alpha, 
			     float dtheta, float dalpha ) {
  load_navaids();
  load_fixes();

  bool save_shade = output->getShade();
  output->setShade(false);

  for (NAV **i = navaids.begin(); i != navaids.end(); i++) {
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
  char freqbuf[8];
  sprintf( freqbuf, "%.0f", n->freq );

  output->setColor( nav_color );
  output->drawCircle( p, RADIUS );

  p[0] -= RADIUS+25; p[1] -= 10;
  if (features & OVERLAY_NAMES) output->drawText( p, n->name );
  p[1] += 10;
  if (features & OVERLAY_IDS) output->drawText( p, n->id );
  p[1] += 10;
  if (features & OVERLAY_ANY_LABEL) output->drawText( p, freqbuf );
}

// Draw one specified VOR
void Overlays::draw_vor( NAV *n, sgVec2 p ) {
  static const int SUBDIVISIONS = 12;
  static const int RADIUS = 15;

  char freqbuf[8];
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

  // if this nav is selected, draw radial
  if ( fabs( n->freq - nav1_freq ) < SG_EPSILON ) {
      sgVec2 end;
      sgSetVec2( end,
		 p[0] + sin(nav1_rad + n->magvar) * 500,
		 p[1] + cos(nav1_rad + n->magvar) * 500 );
      output->setColor( static_vor_from_color );
      output->drawLine(p, end);
      sgSetVec2( end,
		 p[0] - sin(nav1_rad + n->magvar) * 500,
		 p[1] - cos(nav1_rad + n->magvar) * 500 );
      output->setColor( static_vor_to_color );
      output->drawLine(p, end);
  }
  if ( fabs( n->freq - nav2_freq ) < SG_EPSILON ) {
      sgVec2 end;
      sgSetVec2( end,
		 p[0] + sin(nav2_rad + n->magvar) * 500,
		 p[1] + cos(nav2_rad + n->magvar) * 500 );
      output->setColor( static_vor_from_color );
      output->drawLine(p, end);
      sgSetVec2( end,
		 p[0] - sin(nav2_rad + n->magvar) * 500,
		 p[1] - cos(nav2_rad + n->magvar) * 500 );
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

/* Loads the airport database if it isn't already loaded
   (if *one* instance of the Overlays class has loaded the db,
    no other instance will have to load it again, but it's safe
    to call this function multiple times, since it will just
    return immediately) */
void Overlays::load_airports() {
  char *arpname = new char[strlen(fg_root) + 512];
  char line[256];

  if (airports_loaded)
    return;
  
  strcpy( arpname, fg_root );
  strcat( arpname, "/Airports/default.apt.gz" );

  gzFile arp;

  arp = gzopen( arpname, "rb" );
  if (arp == NULL) {
    fprintf( stderr, "load_airports: Couldn't open \"%s\" .\n", arpname );
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
}

/* Loads the navaid database if it isn't already loaded
   (if *one* instance of the Overlays class has loaded the db,
    no other instance will have to load it again, but it's safe
    to call this function multiple times, since it will just
    return immediately) */
void Overlays::load_navaids() {
  char *navname = new char[strlen(fg_root) + 512];
  char line[256];
  //double magdummy[6];

  if (navaids_loaded)
    return;
  
  strcpy( navname, fg_root );
  strcat( navname, "/Navaids/default.nav.gz" );

  gzFile nav;

  nav = gzopen( navname, "rb" );
  if (nav == NULL) {
    fprintf( stderr, "load_navaids: Couldn't open \"%s\" .\n", navname );
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
	n = NULL;
    }
  }
  if (n != NULL)
    delete n;

  gzclose(nav);
  delete navname;
 
  navaids_loaded = true;
}

void Overlays::load_fixes() {
  char *navname = new char[strlen(fg_root) + 512];
  char line[256];

  if (fixes_loaded)
    return;
  
  strcpy( navname, fg_root );
  strcat( navname, "/Navaids/default.fix.gz" );

  gzFile fix;

  fix = gzopen( navname, "rb" );
  if (fix == NULL) {
    fprintf( stderr, "load_fixes: Couldn't open \"%s\" .\n", navname );
    return;
  }

  NAV *n = NULL;
  while (!gzeof(fix)) {
    if (n == NULL)
      n = new NAV;
    
    gzgets( fix, line, 256 );
    if ( sscanf(line, "%s %f %f", &n->name, &n->lat, &n->lon) == 3 ) {
      strcpy(n->id, n->name);
      n->navtype = NAV_FIX;
      n->lat *= SG_DEGREES_TO_RADIANS;
      n->lon *= SG_DEGREES_TO_RADIANS;
      navaids.push_back(n);
      n = NULL;
    }
  }

  delete navname;
  gzclose(fix);
  fixes_loaded = true;
}

Overlays::ARP *Overlays::findAirport( const char *name ) {
  for (ARP **i = airports.begin(); i < airports.end(); i++) {
    if ( strcmp( (*i)->name, name ) == 0 ) {
      return *i;
    }
  }

  return NULL;
}

Overlays::NAV *Overlays::findNav( const char *name ) {
  for (NAV **i = navaids.begin(); i < navaids.end(); i++) {
    if ( strcmp( (*i)->name, name ) == 0 ) {
      return *i;
    }
  }

  return NULL;
}

Overlays::NAV *Overlays::findNav( float lat, float lon, float freq ) {
  NAV   *closest = NULL;
  float closest_dist = 1e12f;

  for (NAV **i = navaids.begin(); i < navaids.end(); i++) {
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
