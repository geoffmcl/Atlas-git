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

#include <simgear/misc/fgpath.hxx>

#include "Overlays.hxx"
#include "Geodesy.hxx"

bool Overlays::airports_loaded = false;
bool Overlays::navaids_loaded = false;
vector<Overlays::ARP*> Overlays::airports;
vector<Overlays::NAV*> Overlays::navaids;

const float Overlays::airport_color1[4] = {0.439, 0.271, 0.420, 0.7}; 
const float Overlays::airport_color2[4] = {0.824, 0.863, 0.824, 0.7};
const float Overlays::navaid_color[4]   = {0.439, 0.271, 0.420, 0.7}; 
const float Overlays::grid_color[4]     = {0.639, 0.371, 0.620, 0.3};
const float Overlays::track_color[4]    = {0.071, 0.243, 0.427, 0.5};

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
  setGridColor( grid_color );
  setTrackColor( track_color );
  flight_track = new FlightTrack;

  time_params = new SGTime();
  time_params->update( 0.0, 0.0, 0 );

  mag = new SGMagVar();
}

void Overlays::drawOverlays() {
  const float GRIDC = 2;

  float dtheta, dalpha;

  lat_ab( size / scale / 2.0f, size / scale / 2.0f, lat, lon, 
	  &dtheta, &dalpha );
  dtheta -= lat;
  dalpha -= lon;

  if (features & OVERLAY_AIRPORTS) {
    airport_labels(lat, lon, dtheta, dalpha );
  }

  if (features & OVERLAY_NAVAIDS) {
    draw_navaids(lat, lon, dtheta, dalpha );
  }

  if (features & OVERLAY_GRIDLINES) {
    draw_gridlines( dtheta * 1.5f, dalpha * 1.5f, 
		    rint(dalpha / (0.2f * SG_DEGREES_TO_RADIANS)) * 
		    (0.2f * SG_DEGREES_TO_RADIANS) / GRIDC );
  }

  if (features & OVERLAY_FLIGHTTRACK) {
    draw_flighttrack();
  }
}

void Overlays::draw_gridlines( float dtheta, float dalpha, float spacing ) {
  // Divide gridlines into 10' steps
  const float STEP = 10.0f / 60.0f * SG_DEGREES_TO_RADIANS;

  float cx, cy, r;
  sgVec2 p1, p2;
  bool first;

  output->setColor(grd_color);

  // draw north-south parallell lines
  for (float glon = rint((lon - dalpha) / spacing) * spacing; 
       glon <= lon + dalpha; glon += spacing) {
    first = true;

    for (float glat = lat - dtheta; glat <= lat + dtheta; glat += STEP) {
      ab_lat( glat, glon, lat, lon, &cx, &cy, &r );
      sgSetVec2( p1, ::scale(cx, output->getSize(), scale), 
		 ::scale(cy, output->getSize(), scale) );

      if (first) {
	first = false;
      } else {
	output->drawLine(p1, p2);
      }

      sgCopyVec2( p2, p1 );
    }
  }

  // draw east-west parallell lines
  for (float glat = rint(lat - dtheta / spacing) * spacing; 
       glat <= lat + dtheta; glat += spacing) {
    first = true;

    for (float glon = lon - dalpha; glon <= lon + dalpha; glon += STEP) {
      ab_lat( glat, glon, lat, lon, &cx, &cy, &r );
      sgSetVec2( p1, ::scale(cx, output->getSize(), scale), 
		 ::scale(cy, output->getSize(), scale) );

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
      float cx, cy, r;
      ab_lat( point->lat, point->lon, lat, lon, &cx, &cy, &r );
      
      sgSetVec2( p1, ::scale(cx, output->getSize(), scale), 
		 ::scale(cy, output->getSize(), scale) );

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

  for (ARP **i = airports.begin(); i < airports.end(); i++) {
    ARP *ap = *i;
    float cx, cy, r;

    if (fabs(ap->lat - theta) < dtheta && fabs(ap->lon - alpha) < dalpha) {
      ab_lat( ap->lat, ap->lon, theta, alpha, &cx, &cy, &r );
      sgVec2 p;

      sgSetVec2( p, ::scale(cx, output->getSize(), scale), 
		 ::scale(cy, output->getSize(), scale) );
	    
      if (ap->name[0] != 0) {
	output->setColor( arp_color1 );
	p[0] += 10; p[1] -= 5;
	if (features & OVERLAY_IDS) output->drawText( p, ap->id );
	p[1] += 10;
	if (features & OVERLAY_NAMES) output->drawText( p, ap->name+4 );
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
	ab_lat( (*j)->lat, (*j)->lon, theta, alpha, &cx, &cy, &r );
	// runway center point
	rwyc[0] = ::scale(cx, output->getSize(), scale);
	rwyc[1] = ::scale(cy, output->getSize(), scale);

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
	output->drawQuad( outlines + k*4 );
      }
      output->setColor( arp_color2 );
      for (unsigned int k = 0; k < ap->rwys.size(); k++) {
	output->drawQuad( insides + k*4 );
      }
    }
  }
}

// Draws all navaids in the specified region
void Overlays::draw_navaids( float theta, float alpha, 
			     float dtheta, float dalpha ) {
  load_navaids();

  for (NAV **i = navaids.begin(); i != navaids.end(); i++) {
    NAV *n = *i;

    float cx, cy, r;
    sgVec2 p;

    if (fabs(n->lat - theta) < dtheta && fabs(n->lon - alpha) < dalpha) {
      ab_lat( n->lat, n->lon, theta, alpha, &cx, &cy, &r );

      sgSetVec2( p,  ::scale(cx, output->getSize(), scale), 
		 ::scale(cy, output->getSize(), scale) );

      switch (n->navtype) {
      case NAV_VOR:
      case NAV_DME:
	draw_vor(n, p);
	break;
      case NAV_NDB:
	draw_ndb(n, p);
	break;
      }
    }
  }
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

  p[0] -= RADIUS+25; p[1] -= 10;
  if (features & OVERLAY_NAMES) output->drawText( p, n->name );
  p[1] += 10;
  if (features & OVERLAY_IDS) output->drawText( p, n->id );
  p[1] += 10;
  if (features & OVERLAY_ANY_LABEL) output->drawText( p, freqbuf );
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

  gzgets( arp, line, 256 );

  while (!gzeof(arp)) {
    ARP *ap;
    char id[8], *name;
    float lat, lon;
    int elev;

    int lineread = 0;

    if (line[0] == 'A') {
      if (sscanf( line, "A %s %f %f %d", id, &lat, &lon, &elev ) == 4) {
	ap = new ARP;
	  
	line[ strlen(line)-1 ] = 0;
	name = strstr( line, "CCY" );

	ap->lat = lat * SG_DEGREES_TO_RADIANS;
	ap->lon = lon * SG_DEGREES_TO_RADIANS;

	if (name != NULL) {
	  int wordcount = 0, len = 0;
	  while (wordcount < 4 && name[len] != 0) {
	    if (name[len] == ' ') wordcount++;
	    len++;
	  }
	  strncpy( ap->name, name, len );
	  ap->name[len] = 0;
	} else
	  ap->name[0] = 0;

	strcpy( ap->id, id );

	line[0] = 'R'; // ouch, ugly hack
	while (!gzeof(arp) && line[0] == 'R') {
	  char rwyid[8];
	  float heading;
	  float length, width;

	  gzgets( arp, line, 256 );
	  lineread = 1;

	  if (sscanf(line, "R %s %f %f %f %f %f", 
		     rwyid, &lat, &lon, &heading, &length, &width) == 6) {
	    RWY *rwy    = new RWY;
	    rwy->lat    = lat    * SG_DEGREES_TO_RADIANS;
	    rwy->lon    = lon    * SG_DEGREES_TO_RADIANS;
	    rwy->hdg    = heading* SG_DEGREES_TO_RADIANS;
	    rwy->length = (int)(length * 0.3048);   // feet to meters
	    rwy->width  = (int)(width  * 0.3048);
	    ap->rwys.push_back( rwy );
	  }
	}

	airports.push_back( ap );


      } else {
	// skip all non 'A' records!       line[0] = 'R'; // ouch, ugly hack
	while (!gzeof(arp) && line[0] == 'R' ) {
	  gzgets( arp, line, 256 );
	  lineread = 1;
	}
      }
    }
    
    if (!lineread)
      gzgets( arp, line, 256 );
  }
  
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
    int elev, iDummy;

    if (n == NULL)
      n = new NAV;
    
    gzgets( nav, line, 256 );
    
    if ( sscanf(line, "%c %f %f %d %f %d %c %s",
		&cNavtype, &n->lat, &n->lon, &elev, &n->freq, &iDummy,
		&cDummy, n->id) == 8 ) {
      switch (cNavtype) {
      case 'V': n->navtype = NAV_VOR; break;
      case 'D': n->navtype = NAV_DME; break;
      case 'N': n->navtype = NAV_NDB; break;
      }

      n->lat *= SG_DEGREES_TO_RADIANS;
      n->lon *= SG_DEGREES_TO_RADIANS;

      mag->update( n->lat, n->lon, elev, time_params->getJD() );
      // n->magvar = SGMagVar( n->lat, n->lon, elev, 
      //		    yymmdd_to_julian_days(2000, 05, 22), magdummy );
      n->magvar = mag->get_magvar() * DEG_TO_RAD;
      // printf("magvar = %.2f\n", n->magvar);

      strcpy(n->name, line+55);  // 51?

      navaids.push_back(n);
      n = NULL;
    }
  }
  
  navaids_loaded = true;
}
