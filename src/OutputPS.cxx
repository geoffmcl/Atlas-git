#include "OutputPS.hxx"

OutputPS::OutputPS( char *filename, int size, bool smooth_shading ) :
  GfxOutput( filename, size ), filename(filename) {
  ps_file = fopen( filename, "wb" );
  
  //write some (E)PS infos
  fprintf( ps_file, "%%!PS-Adobe EPSF-3.0\n"                   );  //we want a EPS
  fprintf( ps_file, "%%%%Creator: TerraGear's Scenery2Map\n"   );
  fprintf( ps_file, "%%%%Title: FlightGear Map\n"              );
  fprintf( ps_file, "%%%%CreationDate:\n"                      );
  fprintf( ps_file, "%%%%DocumentData: Clean7Bit\n"            );
  fprintf( ps_file, "%%%%DocumentFonts: Helvetica-Bold\n"      );
  fprintf( ps_file, "%%%%DocumentNeededFonts: Helvetica-Bold\n");
  fprintf( ps_file, "%%%%DocumentSuppliedFonts:\n"             );
  fprintf( ps_file, "%%%%Origin: [0 %i]\n", size               );
  fprintf( ps_file, "%%%%BoundingBox: 0 0 %i %i\n", size, size );
  fprintf( ps_file, "%%%%LanguageLevel: 2\n"                   );
  fprintf( ps_file, "%%%%Pages: 1\n"                           );
  fprintf( ps_file, "%%%%EndComments\n"                        );
  fprintf( ps_file, "%%%%BeginProlog\n"                        );
  
  
  //define shortcuts to reduce pagesize:
  
  //"r g b c" => "r/255 g/255 b/255 setrgbcolor"
  fprintf( ps_file, "/c{3{3 -1 roll 255 div}repeat setrgbcolor}def\n" );
  
  //"x1 y1 x2 y2 x3 y3 t"
  //becomes
  //"newpath      " 
  //"x3 y3 moveto "
  //"x2 y2 rlineto"
  //"x1 y1 rlineto"
  //"closepath    "
  //"fill         " 
  //this is caused by the PS stack. BTW: drawing a triangle
  //p1-p2-3 or p3-p2-p1 doesn't matter...
  fprintf( ps_file, 
	   "/t{newpath moveto rlineto rlineto closepath fill}def\n" );
  
  //"x1 y1 x2 y2 x3 y3 x4 y4 q"
  //becomes
  //"newpath      " 
  //"x4 y4 moveto "
  //"x3 y3 rlineto"
  //"x2 y2 rlineto"
  //"x1 y1 rlineto"
  //"closepath    "
  //"fill         " 
  fprintf( ps_file, "/q{newpath moveto rlineto rlineto rlineto "\
	   "closepath fill}def\n" );
  
  //"(s) x y a"
  //becomes         
  //"x y ARP_RADIUS arc stroke"
  //"x+10 y+10 moveto         "
  //"(s) show                 "
  //fprintf( ps_file, "/a{2 copy %i 0 360 arc stroke 10 add "
  //	   "exch 10 add exch moveto show}def\n", ARP_RADIUS);
  
  //"dx dy x y l  "
  //becomes
  //" x  y moveto "
  //"dx dy rlineto"
  //"stroke       "
  fprintf( ps_file, "/l{newpath moveto rlineto stroke}def\n");  
  fprintf( ps_file, "%%%%EndProlog\n" );
  fprintf( ps_file, "%%%%Page: 1 1\n" );
  
  //clip to outputarea
  fprintf( ps_file, "clippath\n" );
  fprintf( ps_file, "newpath\n" );
  fprintf( ps_file, "0 0 moveto\n" );
  fprintf( ps_file, "0 %i lineto\n", size );
  fprintf( ps_file, "%i %i lineto\n", size, size );
  fprintf( ps_file, "%i 0 lineto\n", size );
  fprintf( ps_file, "closepath\n" );
  fprintf( ps_file, "eoclip\n" );
  
  //set font
  fprintf( ps_file, "/Helvetica-Bold findfont 10 scalefont setfont\n");
  
}

OutputPS::~OutputPS() {
  closeOutput();
}

void OutputPS::closeOutput() {
  if (!open)
    return;

  open = false;
  // write resulting map image
  fprintf( ps_file," showpage\n" );
  fprintf( ps_file, "%%%%Trailer\n" );
  fprintf( ps_file, "%%%%EOF\n\n" );
  fclose( ps_file );
  printf("Written '%s'\n", filename);
}

void OutputPS::clear( const float *rgb ) {
  //draw ocean for default colour
  fprintf( ps_file, "%i %i %i c\n", 
	   (int)(rgb[0]*255.0f), (int)(rgb[1]*255.0f), (int)(rgb[2]*255.0f) );
  fprintf( ps_file, "0 -%i %i 0 0 %i 0 0 q\n", size, size, size );
}

int OutputPS::quadrant(const sgVec2 p, bool checkoutside) {
  // Written by Christian Mayer (vader@t-online.de)
  //   |
  //   | 2 | 1
  //   |---+---
  //   | 3 | 4
  //   +--------
  // 0
  float x = p[0], y = p[1];
  
  if (x > size)
    {
      if (y > size)
	return 1;
      else
	if ( (checkoutside == true) && (y < 0) )
	  return 0;
	else
	  return 2;
    }
  else
    {
      if (y > size)
	if ( (checkoutside == true) && (x < 0) )
	  return 0;
	else
	  return 4;
      else
	if ( (checkoutside == true) && (y < 0) )
	  return 0;
	else
	  return 3;
    }
}

void OutputPS::setColor( const float *rgb ) {
  fprintf( ps_file, "%i %i %i c\n", 
	   (int)(rgb[0]*255.0f), (int)(rgb[1]*255.0f), (int)(rgb[2]*255.0f) );
}

void OutputPS::drawTriangle(const sgVec2 *p, const sgVec3 *normals) {
  // do I need to draw that triangle?
  if ((quadrant(p[0]) != 3) && (quadrant(p[1]) != 3) 
      && (quadrant(p[2]) != 3)) {
    // I don't know yet, but all points are outside of my boundingbox
    if ((quadrant(p[0]) + quadrant(p[1]) != 6) &&
	(quadrant(p[2]) + quadrant(p[1]) != 6) &&
	(quadrant(p[0]) + quadrant(p[2]) != 6)) {
      // there isn't one side of the triangle inside 
      // my boundingbox => skip triangle
      return;
    }
  }
    
  fprintf( ps_file, "%.3f %.3f %.3f %.3f %.3f %.3f t\n", 
	   (p[2][0]-p[1][0]), (p[2][1]-p[1][1]), 
	   (p[1][0]-p[0][0]), (p[1][1]-p[0][1]), 
	   (p[0][0]), (p[0][1]) );
}

/****************************************************************************/
/* Draw the PS quad (x1/y1)-(x2/y2)-(x3/x3)-(x4/x4) with the colour (r/g/b) */
/* in the file ps_file							    */
/****************************************************************************/
void OutputPS::drawQuad(const sgVec2 *p, const sgVec3 *normals) {
  // do I need to draw that quad?
  if ((quadrant(p[0]) != 3) && (quadrant(p[1]) != 3) && 
      (quadrant(p[2]) != 3) && (quadrant(p[3]) != 3)) {
    // I don't know yet, but all points are outside of my boundingbox
    if ((quadrant(p[0]) + quadrant(p[1]) != 6) &&
	(quadrant(p[1]) + quadrant(p[2]) != 6) &&
	(quadrant(p[2]) + quadrant(p[3]) != 6) &&
	(quadrant(p[3]) + quadrant(p[0]) != 6)) {
      // there isn't one side of the triangle inside 
      // my boundingbox => skip triangle
      return;
    }
  }
    
  fprintf( ps_file, "%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f q\n", 
	   (p[3][0]-p[2][0]), (p[3][1]-p[2][1]), 
	   (p[2][0]-p[1][0]), (p[2][1]-p[1][1]), 
	   (p[1][0]-p[0][0]), (p[1][1]-p[0][1]), 
	   (p[0][0]), (p[0][1]) );
}

void OutputPS::drawLine( sgVec2 p1, sgVec2 p2 ) {
  // do I need to draw that line?
  if ((quadrant(p1) != 3) && (quadrant(p2) != 3)) {
    // I don't know yet, but all points are outside of my boundingbox
    if ((quadrant(p1) + quadrant(p2) != 6)) {
      return;
    }
  }
    
  fprintf( ps_file, "%.3f %.3f %.3f %.3f l\n", 
	   (p2[0]-p1[0]), (p2[1]-p1[1]), 
	   (p1[0]), (p1[1]) );
}

void OutputPS::drawText( sgVec2 p, char *s ) {
  fprintf(ps_file, "%.3f %.3f moveto\n(%s) show\n", p[0], p[1], s);
}

void OutputPS::drawCircle( sgVec2 p, int radius ) {
  fprintf(ps_file, "newpath %.3f %.3f %i 0 360 arc stroke\n",
	  p[0], p[1], radius );
}
