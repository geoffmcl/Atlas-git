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

#include <memory.h>
#include <stdio.h>
#include <GL/glut.h>
#include <plib/fnt.h>
#include <plib/pu.h>
#include <string>
#include <simgear/io/sg_socket.hxx>
#include <simgear/io/sg_serial.hxx>

#include "MapBrowser.hxx"
#include "Overlays.hxx"
#include "FlightTrack.hxx"

#define SCALECHANGEFACTOR 1.3f

SGIOChannel *input_channel;

bool dragmode = false;
int drag_x, drag_y;
float scalefactor = 1.0f, mapsize, width, height;
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

fntTexFont *texfont;
puFont *font;
puPopup *main_interface, *minimized, *info_interface;
puFrame *frame, *info_frame;
puOneShot *zoomin, *zoomout, *minimize_button, *minimized_button;
puOneShot *clear_ftrack, *choose_projection_button;
puButton *show_arp, *show_nav, *show_name, *show_id;
puButton *show_vor, *show_ndb, *show_fix;
puButton *show_ftrack, *follow;
puText *labeling, *txt_lat, *txt_lon;
puText *txt_info_lat, *txt_info_lon, *txt_info_alt;
puText *txt_info_hdg, *txt_info_spd;
puInput *inp_lat, *inp_lon;
puPopupMenu *choose_projection_menu;
puObject *proj_item[MAX_NUM_PROJECTIONS];

bool softcursor = false;
char lat_str[80], lon_str[80], alt_str[80], hdg_str[80], spd_str[80];

char fg_root[512] = "";
char path[512]="";
char lowrespath[512]="";
int lowres_avlble;

MapBrowser *map_object;
FlightTrack *track = NULL;

bool parse_nmea(char *buf) {
  //  cout << "parsing nmea message = " << buf << endl;

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
	    latitude *= SG_DEGREES_TO_RADIANS;  // convert to radians

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
	    longitude *= SG_DEGREES_TO_RADIANS;  // convert to radians

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
	    latitude *= SG_DEGREES_TO_RADIANS;  // convert to radians

	    // cur_fdm_state->set_Latitude( latitude * SG_DEGREES_TO_RADIANS );

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
	    longitude *= SG_DEGREES_TO_RADIANS;  // convert to radians

	    // cur_fdm_state->set_Longitude( longitude * SG_DEGREES_TO_RADIANS );

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
	} else if ( sentence == "PATLA" ) {
	    // nav1 freq
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string nav1_freq_str = msg.substr(begin, end - begin);
	    begin = end + 1;
	    cout << "  nav1_freq = " << nav1_freq_str << endl;

	    // nav1 selected radial
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string nav1_rad_str = msg.substr(begin, end - begin);
	    begin = end + 1;
	    cout << "  nav1_rad = " << nav1_rad_str << endl;

	    // nav2 freq
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string nav2_freq_str = msg.substr(begin, end - begin);
	    begin = end + 1;
	    cout << "  nav2_freq = " << nav2_freq_str << endl;

	    // nav2 selected radial
	    end = msg.find(",", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string nav2_rad_str = msg.substr(begin, end - begin);
	    begin = end + 1;
	    cout << "  nav2_rad = " << nav2_rad_str << endl;

	    // adf freq
	    end = msg.find("*", begin);
	    if ( end == string::npos ) {
		return false;
	    }
    
	    string adf_freq_str = msg.substr(begin, end - begin);
	    begin = end + 1;
	    cout << "  adf_freq = " << adf_freq_str << endl;

	    nav1_freq = atof( nav1_freq_str.c_str() );
	    nav1_rad =  atof( nav1_rad_str.c_str() ) * 
	      SGD_DEGREES_TO_RADIANS;
	    nav2_freq = atof( nav2_freq_str.c_str() );
	    nav2_rad =  atof( nav2_rad_str.c_str() ) * 
	      SGD_DEGREES_TO_RADIANS;
	    adf_freq =  atof( adf_freq_str.c_str() );
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
                 dmshh_format(latitude * SG_RADIANS_TO_DEGREES, buf1),
                 longitude > 0 ? 'E' : 'W',
                 dmshh_format(longitude * SG_RADIANS_TO_DEGREES, buf2)       );
 return buf;
}

/******************************************************************************
 PUI code (HANDLERS)
******************************************************************************/
void zoom_cb ( puObject *cb )
{ float new_scale, prev_scale;
   
  prev_scale=map_object->getScale();
  
  if (cb == zoomin) { 
    map_object->setScale( map_object->getScale() / SCALECHANGEFACTOR );
    scalefactor /= SCALECHANGEFACTOR;
  } else {
    map_object->setScale( map_object->getScale() * SCALECHANGEFACTOR );
    scalefactor *= SCALECHANGEFACTOR;
  }
  new_scale=map_object->getScale();
   
  printf("scale: %f\n", new_scale);
  
  //set map math depending on resolution
  if (lowres_avlble) {
     if (new_scale > 1000000 && prev_scale <=1000000) {
	puts("Switching to low resolution maps");
	map_object->changeResolution(lowrespath);
     } else if (new_scale <= 1000000 && prev_scale > 1000000) {
	puts("Switching to default resolution maps");
	map_object->changeResolution(path);
     }
  }
  glutPostRedisplay();
}

void show_cb ( puObject *cb )
{
  int feature;
  if (cb == show_arp) { 
    feature = Overlays::OVERLAY_AIRPORTS;
  } else if (cb == show_vor) {
    feature = Overlays::OVERLAY_NAVAIDS_VOR;
  } else if (cb == show_ndb) {
    feature = Overlays::OVERLAY_NAVAIDS_NDB;
  } else if (cb == show_fix) {
    feature = Overlays::OVERLAY_NAVAIDS_FIX;
  } else if (cb == show_nav) {
    feature = Overlays::OVERLAY_NAVAIDS;
  } else if (cb == show_name) {
    feature = Overlays::OVERLAY_NAMES;
  } else if (cb == show_id) {
    feature = Overlays::OVERLAY_IDS;
  } else if (cb == show_ftrack) {
    feature = Overlays::OVERLAY_FLIGHTTRACK;
  }
  if (cb->getValue()) {
    map_object->setFeatures( map_object->getFeatures() | feature );
  } else {
    map_object->setFeatures( map_object->getFeatures() & ~feature );
  }
  glutPostRedisplay();
}

void position_cb ( puObject *cb ) {
  char *buffer;
  cb->getValue(&buffer);

  char ns, deg_ch, min_ch = ' ', sec_ch = ' ';
  float degrees = 0, minutes = 0, seconds = 0;

  // Free-format entry: "N51", "N50.99*", "N50*59 24.1", etc.
  int n_items = sscanf(buffer, " %c %f%c %f%c %f%c",
    &ns, &degrees, &deg_ch, &minutes, &min_ch, &seconds, &sec_ch);
  if (n_items < 2) return;
  // if (!strchr(" m'", min_ch) || !strchr(" s\"", sec_ch)) return;
  float angle = (degrees + minutes / 60 + seconds / 3600) *
    SG_DEGREES_TO_RADIANS;
  if (cb == inp_lat) {
    latitude = ((ns=='S'||ns=='s')?-1.0f:1.0f) * angle;
  } else {
    longitude = ((ns=='W'||ns=='w')?-1.0f:1.0f) * angle;
  }
  map_object->setLocation(latitude, longitude);
  glutPostRedisplay();
}

void clear_ftrack_cb ( puObject *cb ) {
  (cb, cb);

  if (track != NULL) {
    track->clear();
  }

  glutPostRedisplay();
}

void minimize_cb ( puObject *cb ) {
  (cb, cb);

  main_interface->hide();
  minimized->reveal();
}

void restore_cb ( puObject *cb ) {
  (cb, cb);

  minimized->hide();
  main_interface->reveal();
}

void projection_cb (puObject *cb) {
   if (cb == choose_projection_button) {
      choose_projection_menu->reveal();
   }
   else {
      int i;
      for (i=0;i<map_object->getNumProjections();i++) {
	 if (cb==proj_item[i])
	   break;
      }
      map_object->setProjectionByID(i);
      choose_projection_menu->hide();
   }
   glutPostRedisplay();
}
      
/*****************************************************************************
 PUI Code (WIDGETS)
*****************************************************************************/
void init_gui(bool textureFonts) {
  puInit();

  int curx,cury;

  int puxoff=20,puyoff=20,puxsiz=205,puysiz=420;

  if (textureFonts) {
    char font_name[512];
    strcpy( font_name, fg_root );
    strcat( font_name, "/Fonts/helvetica_medium.txf" );

    texfont = new fntTexFont( font_name );
    font = new puFont( texfont, 16.0f );
  } else {
    font = new puFont();
  }
  puSetDefaultFonts(*font, *font);
  puSetDefaultColourScheme(0.4f, 0.4f, 0.8f, 0.6f);

  main_interface = new puPopup(puxoff,puyoff);
  frame = new puFrame(puxoff,puyoff,puxoff+puxsiz,puyoff+puysiz);

  curx=puxoff+10; cury=puyoff+10;

  zoomin = new puOneShot(curx, cury, "Zoom In");
  zoomin->setCallback(zoom_cb);
  zoomin->setSize(90, 24);
  zoomout = new puOneShot(curx+95, cury, "Zoom Out");
  zoomout->setCallback(zoom_cb);
  zoomout->setSize(90, 24);

  cury+=35;
  
  show_vor = new puButton(curx, cury, "VOR");
  show_vor->setSize(60,24);
  show_vor->setCallback(show_cb);
  show_vor->setValue(1);
  show_ndb = new puButton(curx+62, cury, "NDB");
  show_ndb->setSize(60,24);
  show_ndb->setCallback(show_cb);
  show_ndb->setValue(1);
  show_fix = new puButton(curx+124, cury, "FIX");
  show_fix->setSize(60,24);
  show_fix->setCallback(show_cb);
  show_fix->setValue(1);

  cury+=25;

  show_nav = new puButton(curx, cury, "Show Navaids");
  show_nav->setSize(185, 24);
  show_nav->setCallback(show_cb);
  show_nav->setValue(1);

  cury+=25;

  show_arp = new puButton(curx, cury, "Show Airports");
  show_arp->setSize(185, 24);
  show_arp->setCallback(show_cb);
  show_arp->setValue(1);

  cury+=35;
  
  labeling = new puText(curx,cury+20);
  labeling->setLabel("Labeling:");
  show_name = new puButton(curx, cury, "Name");
  show_id   = new puButton(curx+95, cury, "Id");
  show_name ->setSize(90, 24);
  show_id   ->setSize(90, 24);
  show_name ->setCallback(show_cb);
  show_id   ->setCallback(show_cb);
  show_name->setValue(1);

  cury+=55;

  txt_lat = new puText(curx, cury+70);
  txt_lat->setLabel("Latitude:");
  txt_lon = new puText(curx, cury+20);
  txt_lon->setLabel("Longitude:");
  inp_lat = new puInput(curx, cury+50, curx+185, cury+74);
  inp_lon = new puInput(curx, cury, curx+185, cury+24);
  inp_lat->setValue(lat_str);
  inp_lon->setValue(lon_str);
  inp_lat->setCallback(position_cb);
  inp_lon->setCallback(position_cb);
  inp_lat->setStyle(PUSTYLE_BEVELLED);
  inp_lon->setStyle(PUSTYLE_BEVELLED);

  cury+=104;
  if (slaved) {
    show_ftrack  = new puButton(curx, cury, "Show Flight Track");
    clear_ftrack = new puOneShot(curx, cury+25, "Clear Flight Track");
    show_ftrack  -> setSize(185, 24);
    clear_ftrack -> setSize(185, 24);
    show_ftrack  -> setValue(1);
    show_ftrack  -> setCallback( show_cb );
    clear_ftrack -> setCallback( clear_ftrack_cb );
  }

  cury+=60;
  choose_projection_button = new puOneShot(curx, cury, "Change Projection");
  choose_projection_button->setSize(182,24);
  choose_projection_button->setCallback(projection_cb);
   
  cury=puyoff+puysiz-10;

  minimize_button = new puOneShot(curx+185-20, cury-24, "X");
  minimize_button->setSize(20, 24);
  minimize_button->setCallback(minimize_cb);

  main_interface->close();
  main_interface->reveal();

  minimized = new puPopup(20, 20);
  minimized_button = new puOneShot(20, 20, "X");
  minimized_button->setCallback(restore_cb);
  minimized->close();

  if (slaved) {
    info_interface = new puPopup(260, 20);
    info_frame = new puFrame(260, 20, 470, 120);
    txt_info_spd = new puText(270, 30);
    txt_info_hdg = new puText(270, 45);
    txt_info_alt = new puText(270, 60);
    txt_info_lon = new puText(270, 75);
    txt_info_lat = new puText(270, 90);
    info_interface->close();
    info_interface->reveal();
  }

  if (softcursor) {
    puShowCursor();
  }

  choose_projection_menu = new puPopupMenu(260, 150);
  
  for (int i=0; i<map_object->getNumProjections(); i++) {
	proj_item[i]=choose_projection_menu->add_item(map_object->getProjectionNameByID(i), projection_cb);
  }	
  choose_projection_menu->close();
	
}
/******************************************************************************
 GLUT event handlers
******************************************************************************/
void reshapeMap( int _width, int _height ) {
  width  = (float)_width  ;
  height = (float)_height ;
  mapsize = (width > height) ? width : height;

  map_object->setSize( mapsize );
}

void redrawMap() {
  char buf[256];
  
  glClearColor( 0.643f, 0.714f, 0.722f, 0.0f );
  glClear( GL_COLOR_BUFFER_BIT );

  /* Fix so that center of map is actually the center of the window.
     This should probably be taken care of in OutputGL... */
  glPushMatrix();
  if (width > height) {
    glTranslatef( 0.0f, -(width - height) / 2.0f, 0.0f );
  } else {
    glTranslatef( -(height - width) / 2.0f, 0.0f, 0.0f );
  }

  map_object->draw();

  glPushMatrix();
  glTranslatef( mapsize/2, mapsize/2, 0.0f );
  glColor3f( 1.0f, 0.0f, 0.0f );

// Draw aircraft if in slave mode
  if (slaved) {
    glRotatef( 90.0f - heading, 0.0f, 0.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex2f( 4.0f, 0.0f );
    glVertex2f( -9.0f, 0.0f );
    glVertex2f( 0.0f, -7.0f );  // left wing
    glVertex2f( 0.0f, 7.0f );   // right wing
    glVertex2f( -7.0f, -3.0f );
    glVertex2f( -7.0f, 3.0f );
  } else {
  // Draw Crosshair if slaved==false
    glBegin(GL_LINES);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.0f, 20.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(0.0f, -20.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(20.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(-20.0f, 0.0f);
  }
  glEnd(); 
  glPopMatrix();
   
  if (!inp_lat->isAcceptingInput()) {
    sprintf( lat_str, "%c%s", 
	     (latitude<0)?'S':'N', 
	     dmshh_format(latitude * SG_RADIANS_TO_DEGREES, buf) );
    inp_lat->setValue(lat_str);
  }

  if (!inp_lon->isAcceptingInput()) {
    sprintf( lon_str, "%c%s", 
	     (longitude<0)?'W':'E', 
	     dmshh_format(longitude * SG_RADIANS_TO_DEGREES, buf) );
    inp_lon->setValue(lon_str);
  }

  if (slaved) {
    sprintf( hdg_str, "HDG: %.0f*", heading);    
    sprintf( alt_str, "ALT: %.0f ft MSL", altitude);
    sprintf( spd_str, "SPD: %.0f KIAS", speed);
    txt_info_lat->setLabel(lat_str);
    txt_info_lon->setLabel(lon_str);
    txt_info_alt->setLabel(alt_str);
    txt_info_hdg->setLabel(hdg_str);
    txt_info_spd->setLabel(spd_str);
  }

  // Remove our translation
  glPopMatrix();

  puDisplay();
  /* I have no idea why I suddenly need to set the viewport here -
     I think this might be a pui bug, since I didn't have to do this
     before some plib update. Commenting puDisplay out makes it unnessecary. */
  glViewport(0, 0, (int)mapsize, (int)mapsize);
  glutSwapBuffers();
}

void timer(int value) {
  char buffer[512];

  int length;
  while ( (length = input_channel->readline( buffer, 512 )) > 0 ) {
      parse_nmea(buffer);
  }

  // record flight
  FlightData *d = new FlightData;
  d->lat = latitude;
  d->lon = longitude;
  d->alt = altitude;
  d->hdg = heading;
  d->spd = speed;
  track->addPoint(d);

  map_object->setLocation( latitude, longitude );
  
  glutPostRedisplay();
  glutTimerFunc( (int)(update * 1000.0f), timer, value );
}

void mouseClick( int button, int state, int x, int y ) {
  if ( !puMouse( button, state, x, y ) ) {
    // PUI didn't consume this event
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
  } else {
    glutPostRedisplay();
  }
}


void mouseMotion( int x, int y ) {
  if ( !puMouse(x, y) ) {
    // PUI didn't consume this event
    if (dragmode) {
      latitude  = (copy_lat + (float)(y - drag_y)*scalefactor / 
		   (float)mapsize * SG_DEGREES_TO_RADIANS);
      longitude = (copy_lon + (float)(drag_x - x)*scalefactor / 
		   (float)mapsize * SG_DEGREES_TO_RADIANS);
      map_object->setLocation( latitude, longitude );
    }
  }

  glutPostRedisplay();
}

void keyPressed( unsigned char key, int x, int y ) {
  if (!puKeyboard(key, PU_DOWN)) {
    switch (key) {
    case '+':
      zoom_cb(zoomin);
      break;
    case '-':
      zoom_cb(zoomout);
      break;
    case 'D':
    case 'd':
      if (slaved) {
	if (!info_interface->isVisible()) {
	  info_interface->reveal();
	} else {
	  info_interface->hide();
	}
	glutPostRedisplay();
      }
      break;
    case 'A':
    case 'a':
      show_arp->setValue(!show_arp->getValue());
      show_cb(show_arp);
      break;
    case 'N':
    case 'n':
      show_nav->setValue(!show_nav->getValue());
      show_cb(show_nav);
      break;    
    case 'T':
    case 't':
      map_object->setTextured( !map_object->getTextured() );
      glutPostRedisplay();
      break;
    case 'V':
    case 'v':
      show_name->setValue(!show_name->getValue());
      show_cb(show_name);
      break;
    case ' ':
      if (!main_interface->isVisible()) {
	main_interface->reveal();
	minimized->hide();
      } else {
	main_interface->hide();
	minimized->hide();
      }
      glutPostRedisplay();
    }
  } else {
    glutPostRedisplay();
  }
}

void specPressed(int key, int x, int y) {
  if (puKeyboard(key + PU_KEY_GLUT_SPECIAL_OFFSET, PU_DOWN )) {
    glutPostRedisplay();
  }
}


void print_help() {
  printf("ATLAS - A map browsing utility for FlightGear\n\nUsage:\n");
  printf("   --lat=x      Start browsing at latitude xx (deg. south i neg.)\n");
  printf("   --lon=x      Start browsing at longitude xx (deg. west i neg.)\n");
  printf("   --path=xxx   Set path for map images\n");
  printf("   --fgroot=path  Overrides FG_ROOT environment variable\n");
  printf("   --glutfonts  Use GLUT bitmap fonts (fast for software rendering)\n");
  printf("   --geometry=[width]x[height] Set initial window size\n");
  printf("   --softcursor Draw mouse cursor using OpenGL (for fullscreen Voodoo cards)\n\n");
  printf("   --udp=x      Input read from UDP socket at specified port (defaults to 5500)\n");
  printf("   --serial=dev Input read from serial port with specified device\n");
  printf("   --baud=x     Set serial port baud rate (defaults to 4800)\n");
}

int main(int argc, char **argv) {
  bool textureFonts = true;
  int width = 800, height = 600;

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
    } else if ( strcmp(argv[i], "--glutfonts") == 0 ) {
      textureFonts = false;
    } else if ( strcmp(argv[i], "--softcursor") == 0 ) {
      softcursor = true;
    } else if ( sscanf(argv[i], "--geometry=%dx%d", &width, &height) == 2 ) {
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

  if (fg_root[0] == 0) {
    char *env = getenv("FG_ROOT");
    if (env == NULL) {
      strcpy(fg_root, "/usr/local/lib/FlightGear");
    } else {
      strcpy(fg_root, env);
    }
  }

  if (path[0] == 0) {
    if (fg_root[0] != 0) {
      strcpy(path, fg_root);
      strcat(path, "/");
    } else {
      strcpy(path, "/usr/local/lib/FlightGear/");
    }

    strcat(path, "Atlas/");
    if (access(path, F_OK)==-1) {
       printf("\nWarning: path %s doesn't exist. Maps won't be loaded!\n", path);
    } else {
       strcpy(lowrespath, path);
       strcat(lowrespath, "lowres/");
       if (access(lowrespath, F_OK)==-1) {
	  printf("\nWarning: path %s doesn't exist. Low resolution maps won't be loaded\n", lowrespath);
	  lowres_avlble=0;
       } else {
	  lowres_avlble=1;
       }
    }
  }

  latitude  *= SG_DEGREES_TO_RADIANS;
  longitude *= SG_DEGREES_TO_RADIANS;

  glutInitDisplayMode( GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE );
  glutInitWindowSize( width, height );
  glutCreateWindow( "Atlas" );

  glutReshapeFunc( reshapeMap );
  glutDisplayFunc( redrawMap );

  mapsize = (float)( (width>height)?width:height );
   map_object = new MapBrowser( 0.0f, 0.0f, mapsize, 
                               Overlays::OVERLAY_AIRPORTS  | 
                               Overlays::OVERLAY_NAVAIDS   |
                               Overlays::OVERLAY_NAVAIDS_VOR |
                               Overlays::OVERLAY_NAVAIDS_NDB |
                               Overlays::OVERLAY_NAVAIDS_FIX |
                               Overlays::OVERLAY_FIXES     |
                               Overlays::OVERLAY_GRIDLINES | 
                               Overlays::OVERLAY_NAMES     |
			       Overlays::OVERLAY_FLIGHTTRACK,
			       fg_root[0] == 0 ? NULL : fg_root, 
			       textureFonts );
  map_object->setTextured(true);
  map_object->setMapPath(path);

  if (slaved) {
    glutTimerFunc( (int)(update*1000.0f), timer, 0 );

    track = new FlightTrack();
    map_object->setFlightTrack(track);

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

  glutMotionFunc       ( mouseMotion );
  glutPassiveMotionFunc( mouseMotion );
  glutMouseFunc        ( mouseClick  );
  glutKeyboardFunc     ( keyPressed  );
  glutSpecialFunc      ( specPressed );

  map_object->setLocation( latitude, longitude );
  printf("Please wait while loading databases..."); fflush(stdout);
  map_object->loadDb();
  printf("done.\n");

  init_gui(textureFonts);

  glutMainLoop();
 
  if (slaved)
      input_channel->close();

  return 0;
}

