/*-------------------------------------------------------------------------
  Atlas.cxx
  Map browsing utility

  Written by Per Liedman, started February 2000.
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
#include <GL/glut.h>
#include <plib/pu.h>
#include <string>
#include <simgear/io/sg_socket.hxx>
#include <simgear/io/sg_serial.hxx>

#include "MapBrowser.hxx"
#include "Overlays.hxx"

SGIOChannel *input_channel;

bool dragmode = false, display = false;
int drag_x, drag_y;
float scalefactor = 1.0f, mapsize, height;
float latitude  = 33.5f  , copy_lat;
float longitude = -110.5f, copy_lon;
float heading = 0.0f, speed, altitude;

bool slaved = false;
bool network = false;
bool serial = false;
char baud[256] = "4800";
char port[256] = "5500";
char device[256] = "/dev/ttyS0";
int  sock;
float update = 1.0f;
char save_buf[ 2 * 2048 ];
int save_len = 0;

puFont simple_fnt;
MapBrowser *map;

bool parse_nmea(char *buf) {
    cout << "parsing nmea message = " << buf << endl;

  string msg = buf;
  //msg = msg.substr( 0, length );

  string::size_type begin_line, end_line, begin, end;
  begin_line = begin = 0;

  // extract out each line
  end_line = msg.find("\n", begin_line);
  while ( end_line != string::npos ) {
    string line = msg.substr(begin_line, end_line - begin_line);
    begin_line = end_line + 1;

    // leading character
    string start = msg.substr(begin, 1);
    ++begin;

	// sentence
    end = msg.find(",", begin);
    if ( end == string::npos ) {
      return false;
    }
    
    string sentence = msg.substr(begin, end - begin);
    begin = end + 1;

    double lon_deg, lon_min, lat_deg, lat_min;

    if ( sentence == "GPRMC" ) {
      // time
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string utc = msg.substr(begin, end - begin);
      begin = end + 1;

      // junk
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string junk = msg.substr(begin, end - begin);
      begin = end + 1;

      // latitude val
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string lat_str = msg.substr(begin, end - begin);
      begin = end + 1;

      lat_deg = atof( lat_str.substr(0, 2).c_str() );
      lat_min = atof( lat_str.substr(2).c_str() );

      // latitude dir
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string lat_dir = msg.substr(begin, end - begin);
      begin = end + 1;

      latitude = lat_deg + ( lat_min / 60.0 );
      if ( lat_dir == "S" ) {
	latitude *= -1;
      }
      latitude *= M_PI / 180.0f;  // convert to radians

      // longitude val
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string lon_str = msg.substr(begin, end - begin);
      begin = end + 1;

      lon_deg = atof( lon_str.substr(0, 3).c_str() );
      lon_min = atof( lon_str.substr(3).c_str() );

      // longitude dir
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string lon_dir = msg.substr(begin, end - begin);
      begin = end + 1;

      longitude = lon_deg + ( lon_min / 60.0 );
      if ( lon_dir == "W" ) {
	longitude *= -1;
      }
      longitude *= M_PI / 180.0f;  // convert to radians

      // speed
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string speed_str = msg.substr(begin, end - begin);
      begin = end + 1;
      speed = atof( speed_str.c_str() );

	    // heading
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string hdg_str = msg.substr(begin, end - begin);
      begin = end + 1;
      heading = atof( hdg_str.c_str() );
    } else if ( sentence == "GPGGA" ) {
      // time
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string utc = msg.substr(begin, end - begin);
      begin = end + 1;

      // latitude val
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string lat_str = msg.substr(begin, end - begin);
      begin = end + 1;

      lat_deg = atof( lat_str.substr(0, 2).c_str() );
      lat_min = atof( lat_str.substr(2).c_str() );

      // latitude dir
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string lat_dir = msg.substr(begin, end - begin);
      begin = end + 1;

      latitude = lat_deg + ( lat_min / 60.0 );
      if ( lat_dir == "S" ) {
	latitude *= -1;
      }
      latitude *= M_PI / 180.0f;  // convert to radians

      // cur_fdm_state->set_Latitude( latitude * DEG_TO_RAD );

	    // longitude val
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string lon_str = msg.substr(begin, end - begin);
      begin = end + 1;

      lon_deg = atof( lon_str.substr(0, 3).c_str() );
      lon_min = atof( lon_str.substr(3).c_str() );

      // longitude dir
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string lon_dir = msg.substr(begin, end - begin);
      begin = end + 1;

      longitude = lon_deg + ( lon_min / 60.0 );
      if ( lon_dir == "W" ) {
	longitude *= -1;
      }
      longitude *= M_PI / 180.0f;  // convert to radians

      // cur_fdm_state->set_Longitude( longitude * DEG_TO_RAD );

	    // junk
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string junk = msg.substr(begin, end - begin);
      begin = end + 1;

      // junk
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      junk = msg.substr(begin, end - begin);
      begin = end + 1;

      // junk
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      junk = msg.substr(begin, end - begin);
      begin = end + 1;

      // altitude
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string alt_str = msg.substr(begin, end - begin);
      altitude = atof( alt_str.c_str() );
      begin = end + 1;

	    // altitude units
      end = msg.find(",", begin);
      if ( end == string::npos ) {
	return false;
      }
    
      string alt_units = msg.substr(begin, end - begin);
      begin = end + 1;

      if ( alt_units != "F" ) {
	altitude *= 3.28;
      }

    }

    begin = begin_line;
    end_line = msg.find("\n", begin_line);

  }
  
  return true;
}

/*****************************************************************************/
/* Convert degrees to dd mm'ss.s" (DMS-Format)                               */
/*****************************************************************************/
static char *dmshh_format(float degrees, char *buf)
{
 int deg_part;
 int min_part;
 float sec_part;

 if (degrees < 0)
         degrees = -degrees;

 deg_part = (int)degrees;
 min_part = (int)(60.0f * (degrees - deg_part));
 sec_part = 3600.0f * (degrees - deg_part - min_part / 60.0f);

 /* Round off hundredths */
 if (sec_part + 0.005f >= 60.0f)
         sec_part -= 60.0f, min_part += 1;
 if (min_part >= 60)
         min_part -= 60, deg_part += 1;

 sprintf(buf,"%02d*%02d %05.2f",deg_part,min_part,sec_part);

 return buf;
}

static char *coord_format_latlon(float latitude, float longitude, char *buf)
{
 char buf1[16], buf2[16];

 sprintf(buf,"%c %s   %c %s",
                 latitude > 0 ? 'N' : 'S',
                 dmshh_format(latitude * 180.0f / M_PI, buf1),
                 longitude > 0 ? 'E' : 'W',
                 dmshh_format(longitude * 180.0f / M_PI, buf2)       );
 return buf;
}

/******************************************************************************
 GLUT event handlers
******************************************************************************/
void reshapeMap( int width, int _height ) {
  height = (float)_height;
  mapsize = (width > height) ? width : height;

  map->setSize( mapsize );
}

void redrawMap() {
  char buf[256];

  glClearColor( 1.0f, 1.0f, 1.0f, 0.0f );
  glClear( GL_COLOR_BUFFER_BIT );

  map->draw();

  if (display) {
    coord_format_latlon(latitude,longitude, buf);
    glColor3f(1.0f, 0.0f, 0.0f);
    puDrawString(simple_fnt, buf, 0, (int)(mapsize - height) + 10);
    
    if (slaved) {
      sprintf( buf, "HDG: %.0f*", heading);
      puDrawString(simple_fnt, buf, 0, (int)(mapsize - height) + 20);
      sprintf( buf, "ALT: %.0f' MSL", altitude);
      puDrawString(simple_fnt, buf, 0, (int)(mapsize - height) + 30);
      sprintf( buf, "SPD: %.0f KIAS", speed);
      puDrawString(simple_fnt, buf, 0, (int)(mapsize - height) + 40);
    }
  }

  // Draw aircraft if in slave mode
  if (slaved) {
    glPushMatrix();
    glTranslatef( mapsize/2, mapsize/2, 0.0f );
    glRotatef( heading - 90.0f, 0.0f, 0.0f, 1.0f);
    glBegin(GL_LINES);
    glColor3f( 1.0f, 0.0f, 0.0f );
    glVertex2f( 4.0f, 0.0f );
    glVertex2f( -9.0f, 0.0f );
    glVertex2f( 0.0f, -7.0f );  // left wing
    glVertex2f( 0.0f, 7.0f );   // right wing
    glVertex2f( -7.0f, -3.0f );
    glVertex2f( -7.0f, 3.0f );
    glEnd();
    glPopMatrix();
  }

  glutSwapBuffers();
}

void timer(int value) {
  char buffer[512];

  int length;
  while ( (length = input_channel->readline( buffer, 512 )) > 0 ) {
      parse_nmea(buffer);
  }
  
  map->setLocation( latitude, longitude );
  
  glutPostRedisplay();
  glutTimerFunc( (int)(update * 1000.0f), timer, value );
}

void mouseClick( int button, int state, int x, int y ) {
  if (button == GLUT_LEFT_BUTTON) {
    switch (state) {
    case GLUT_DOWN:
      dragmode = true;
      drag_x = x;
      drag_y = y;
      copy_lat = latitude;
      copy_lon = longitude;
      break;
    default:
      dragmode = false;
    }
  } else
    dragmode = false;
}


void dragMap( int x, int y ) {
  if (dragmode) {
    latitude  = (copy_lat + (float)(y - drag_y)*scalefactor / 
		 (float)mapsize * M_PI / 180.0f);
    longitude = (copy_lon + (float)(drag_x - x)*scalefactor / 
		 (float)mapsize * M_PI / 180.0f);
    map->setLocation( latitude, longitude );
    glutPostRedisplay();
  }
}

void keyPressed( unsigned char key, int x, int y ) {
  switch (key) {
  case '+':
    map->setScale( map->getScale() / 2 );
    scalefactor /= 2.0f;
    glutPostRedisplay();
    break;
  case '-':
    map->setScale( map->getScale() * 2 );
    scalefactor *= 2.0f;
    glutPostRedisplay();
    break;
  case 'D':
  case 'd':
    display = !display;
    glutPostRedisplay();
    break;
  case 'A':
  case 'a':
    map->setFeatures( map->getFeatures() ^ Overlays::OVERLAY_AIRPORTS );
    glutPostRedisplay();
    break;
  case 'N':
  case 'n':
    map->setFeatures( map->getFeatures() ^ Overlays::OVERLAY_NAVAIDS );
    glutPostRedisplay();
    break;    
  case 'T':
  case 't':
    map->setTextured( !map->getTextured() );
    glutPostRedisplay();
    break;
  case 'V':
  case 'v':
    map->setFeatures( map->getFeatures() ^ Overlays::OVERLAY_NAMES );
    glutPostRedisplay();
    break;    
  }

}


void print_help() {
  printf("ATLAS - A map browsing utility for FlightGear\n\nUsage:\n");
  printf("   --lat=x      Start browsing at latitude xx (deg. south i neg.)\n");
  printf("   --lon=x      Start browsing at longitude xx (deg. west i neg.)\n");
  printf("   --path=xxx   Set path for map images\n\n");
  printf("   --udp=x      Input read from UDP socket at specified port (defaults to 5500)\n");
  printf("   --serial=dev Input read from serial port with specified device\n");
  printf("   --baud=x     Set serial port baud rate (defaults to 4800)\n");
  printf("   --fgroot=path  Overrides FG_ROOT environment variable\n");
}

int main(int argc, char **argv) {
  char path[512] = "./", fg_root[512] = "\0";

  glutInit( &argc, argv );

  // parse arguments
  for (int i = 1; i < argc; i++) {
    if ( sscanf(argv[i], "--path=%s", path) == 1 ) {
      strcat( path, "/" );
    } else if ( sscanf(argv[i], "--lat=%f", &latitude)  == 1 ) {
      // do nothing
    } else if ( sscanf(argv[i], "--lon=%f", &longitude) == 1 ) {
      // do nothing
    } else if ( sscanf(argv[i], "--udp=%s", port) == 1) {
      slaved = true;
      network = true;
      serial = false;
    } else if ( sscanf(argv[i], "--serial=%s", device) == 1) {
      slaved = true;
      serial = true;
      network = false;
    } else if ( sscanf(argv[i], "--baud=%s", baud) == 1) {
      // do nothing
    } else if ( sscanf(argv[i], "--update=%f", &update) == 1) {
      // do nothing
    } else if ( sscanf(argv[i], "--fgroot=%s", fg_root) == 1 ) {
      // do nothing
    } else if ( strcmp(argv[i], "--help") == 0 ) {
      print_help();
      return 0;
    } else {
      print_help();
      fprintf( stderr, "%s: unknown flag \"%s\".\n", argv[0], argv[i] );
      return 1;
    }
  }

  printf(" udp = %s  serial = %s  baud = %s\n", port, device, baud );

  if (path[0] == 0) {
    print_help();
    fprintf( stderr, "%s: No map path given - try using --path=xxx.\n", argv[0] );
    return 1;
  }

  latitude  *= M_PI / 180.0f;
  longitude *= M_PI / 180.0f;

  glutInitDisplayMode( GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE );
  glutInitWindowSize( 512, 512 );
  glutCreateWindow( "Atlas" );

  glutReshapeFunc( reshapeMap );
  glutDisplayFunc( redrawMap );

  map = new MapBrowser( 0.0f, 0.0f, 512.0f, 
			Overlays::OVERLAY_AIRPORTS | 
			Overlays::OVERLAY_NAVAIDS |
			Overlays::OVERLAY_GRIDLINES | 
			Overlays::OVERLAY_NAMES |
			Overlays::OVERLAY_FLIGHTTRACK,
			NULL );
  map->setTextured(false);
  map->setMapPath(path);
  if (fg_root[0] != 0)
    map->setFGRoot(fg_root);

  if (!slaved) {
    glutMotionFunc( dragMap );
    glutMouseFunc( mouseClick );
  } else {
    glutTimerFunc( (int)(update*1000.0f), timer, 0 );

    if ( network ) {
	input_channel = new SGSocket( "", port, "udp" );
    } else if ( serial ) {
	input_channel = new SGSerial( device, baud );
    } else {
	printf("unknown input, defaulting to network on port 5500\n");
	input_channel = new SGSocket( "", "5500", "udp" );
    }
    input_channel->open( SG_IO_IN );
  }

  glutKeyboardFunc( keyPressed );

  map->setLocation( latitude, longitude );
  printf("Please wait while loading databases..."); fflush(stdout);
  map->loadDb();
  printf("done.\n");

  glutMainLoop();
 
  if (slaved)
      input_channel->close();

  return 0;
}

