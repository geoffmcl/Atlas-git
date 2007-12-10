#include <iostream>
#include <png.h>
extern "C" {
#include <jpeglib.h>
}
#include "OutputGL.hxx"

float OutputGL::circle_x[];
float OutputGL::circle_y[];

const float OutputGL::BRIGHTNESS = 0.6f;

OutputGL::OutputGL( const char *filename, int size, bool smooth_shading, 
		    bool useTexturedFont, char *fontname, bool jpg, int q, int r ) : 
  GfxOutput(filename, size), filename(filename), 
  useTexturedFont(useTexturedFont), jpeg(jpg), jpeg_quality(q), rescale(r)
{
  if ( filename == NULL )
    openFragment( 0, 0, size );

  glEnable(GL_BLEND);
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

  shade = true;
  glEnable(GL_LIGHTING);
  glShadeModel(smooth_shading ? GL_SMOOTH : GL_FLAT);

  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

  if (useTexturedFont) {
    font = new fntTexFont( fontname );
    textRenderer.setFont( font );
    textRenderer.setPointSize( 12 );
  } else {
    glutFont = new puFont;
  }

  for (int i = 0; i < SUBDIVISIONS; i++) {
    circle_x[i] = cos(2*SG_PI / SUBDIVISIONS * i);
    circle_y[i] = sin(2*SG_PI / SUBDIVISIONS * i);
  }

  image = new GLubyte[size*size*3];
}

OutputGL::~OutputGL() {
  delete [] image;

  if (useTexturedFont) {
    delete font;
  } else {
    delete glutFont;
  }
}

void OutputGL::openFragment( int x, int y, int s )
{
  fragment_size = s;
  posx = x;
  posy = y;

  if ( s == 0 )
      s = size;
  glViewport( 0, 0, s, s );
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity();
  glOrtho( x, x+s, y, y+s, -1.0f, 1.0f );
  glMatrixMode( GL_MODELVIEW );
  glLoadIdentity();
}

void OutputGL::closeFragment() {
  if (!open)
    return;

  GLubyte *subimage = new GLubyte[fragment_size*fragment_size*3];
  if (!subimage)
    return;

  // grab screen
  glFinish();
  glReadPixels(0, 0, fragment_size, fragment_size,
	       GL_RGB, GL_UNSIGNED_BYTE, subimage);

  int sx = fragment_size,
      sy = fragment_size;
  if ( posx + sx > size )
    sx = size - posx;
  if ( posy + sy > size )
    sy = size - posy;
  for ( int i = 0; i < sx; ++i ) {
    for ( int j = 0; j < sy; ++j ) {
      image[(( j+posy ) * size + ( i+posx )) * 3] = subimage[(j * fragment_size + i) * 3];
      image[(( j+posy ) * size + ( i+posx )) * 3 + 1] = subimage[(j * fragment_size + i) * 3 + 1];
      image[(( j+posy ) * size + ( i+posx )) * 3 + 2] = subimage[(j * fragment_size + i) * 3 + 2];
    }
  }
  delete [] subimage;
}

void OutputGL::closeOutput() {
  if (!open)
    return;

  open = false;

  int scale = rescale;
  int current_size = size;
  while ( scale && ( scale & 1 ) == 0 ) {
    int new_size = current_size / 2;
    GLubyte *buffer = new GLubyte[new_size*new_size*3];
    for ( int x2 = 0; x2 < new_size; ++x2 ) {
      for ( int y2 = 0; y2 < new_size; ++y2 ) {
        for ( int c = 0; c < 3; ++c ) {
          int x1 = x2 + x2;
          int x1_1 = x1 + 1;
          int y1 = y2 + y2;
          int y1_1 = y1 + 1;

          int t1 = image[ (y1   * current_size + x1  ) * 3 + c ];
          int t2 = image[ (y1_1 * current_size + x1  ) * 3 + c ];
          int t3 = image[ (y1   * current_size + x1_1) * 3 + c ];
          int t4 = image[ (y1_1 * current_size + x1_1) * 3 + c ];

          buffer[ (y2 * new_size + x2) * 3 + c ] = ( t1 + t2 + t3 + t4 ) / 4 ;
        }
      }
    }
    scale >>= 1;
    current_size = new_size;
    delete[] image;
    image = buffer;
  }

  FILE *fp = fopen(filename, "wb");
  if (!fp) {
    printf("OutputGL::closeOutput: can't create '%s'\n", filename);
    return;
  }

  if ( jpeg ) {
    jpeg_compress_struct cinfo;
    memset( &cinfo, 0, sizeof cinfo );

    jpeg_error_mgr jerr;
    memset( &jerr, 0, sizeof jerr );
    cinfo.err = jpeg_std_error( &jerr );

    jpeg_create_compress( &cinfo );
    jpeg_stdio_dest( &cinfo, fp );

    cinfo.image_width = current_size;
    cinfo.image_height = current_size;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo,jpeg_quality,false);

    jpeg_start_compress( &cinfo, TRUE );

    while ( cinfo.next_scanline < cinfo.image_height ) {
       unsigned char *buf;
       if ( 1 ) {
	  buf = (unsigned char *)&image[ current_size * ( current_size - ( cinfo.next_scanline + 1 ) ) * 3 ];
       } else {
	  buf = (unsigned char *)&image[ current_size * cinfo.next_scanline * 3 ];
       }
       jpeg_write_scanlines( &cinfo, (JSAMPARRAY)&buf, 1 );
    }

    jpeg_finish_compress( &cinfo );
    jpeg_destroy_compress( &cinfo );
  } else {
    png_structp png_ptr = png_create_write_struct
      (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
      return;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
      png_destroy_write_struct(&png_ptr,
			       (png_infopp)NULL);
      return;
    }

    if (setjmp(png_ptr->jmpbuf)) {
      png_destroy_write_struct(&png_ptr, &info_ptr);
      fclose(fp);
      return;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, current_size, current_size, 8, PNG_COLOR_TYPE_RGB,
		 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
		 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    png_byte **row_pointers = new png_byte*[current_size];
    for (int i = 1; i <= current_size; i++) {
      row_pointers[i-1] = (png_byte*)(image + current_size * (current_size-i) * 3);
    }

    // actually write the image
    png_write_image(png_ptr, row_pointers);

    delete[] row_pointers;

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
  }
  fclose(fp);
  printf("Written '%s'\n", filename);
}

void OutputGL::setShade( bool shade ) {
  if (shade) {
    glEnable( GL_LIGHTING );
  } else {
    glDisable( GL_LIGHTING );
  }

  this->shade = shade;
}

bool OutputGL::getShade() {
  return shade;
}

void OutputGL::setLightVector( sgVec3 light ) {
  GLfloat ambient[] = {BRIGHTNESS/3.0f, BRIGHTNESS/3.0f, BRIGHTNESS/3.0f,1.0f};
  GLfloat diffuse[] = {BRIGHTNESS, BRIGHTNESS, BRIGHTNESS, 1.0f};

  sgCopyVec3( light_vector, light );
  light_vector[3] = 0.0f;
  glLightfv( GL_LIGHT0, GL_AMBIENT, ambient );
  glLightfv( GL_LIGHT0, GL_DIFFUSE, diffuse );
  glLightfv( GL_LIGHT0, GL_POSITION, light_vector );
  glEnable( GL_LIGHT0 );
}

void OutputGL::setColor( const float *rgba ) {
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, rgba);
  glColor4fv( rgba );
}

void OutputGL::clear( const float *rgb ) {
  glClearColor(rgb[0], rgb[1], rgb[2], 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void OutputGL::drawTriangle( const sgVec2 *p, const sgVec3 *normals ) {
  glBegin(GL_TRIANGLES);
  glNormal3fv( normals[0] ); glVertex2fv( p[0] );
  glNormal3fv( normals[1] ); glVertex2fv( p[1] );
  glNormal3fv( normals[2] ); glVertex2fv( p[2]);
  glEnd();
}

void OutputGL::drawTriangle( const sgVec2 *p, const sgVec3 *normals, const sgVec4 *color ) {
  glBegin(GL_TRIANGLES);
    glColor4fv( color[0] );glNormal3fv( normals[0] ); glVertex2fv( p[0] );
    glColor4fv( color[1] );glNormal3fv( normals[1] ); glVertex2fv( p[1] );
    glColor4fv( color[2] );glNormal3fv( normals[2] ); glVertex2fv( p[2]);
  glEnd();
}

void OutputGL::drawQuad( const sgVec2 *p, const sgVec3 *normals ) {
  glBegin(GL_QUADS);
  glNormal3fv( normals[0] ); glVertex2fv( p[0] );
  glNormal3fv( normals[1] ); glVertex2fv( p[1] );
  glNormal3fv( normals[2] ); glVertex2fv( p[2] );
  glNormal3fv( normals[3] ); glVertex2fv( p[3] );
  glEnd();
}

void OutputGL::drawQuad( const sgVec2 *p, const sgVec3 *normals, const sgVec4 *color ) {
  glBegin(GL_QUADS);
    glColor4fv( color[0] );glNormal3fv( normals[0] ); glVertex2fv( p[0] );
    glColor4fv( color[1] );glNormal3fv( normals[1] ); glVertex2fv( p[1] );
    glColor4fv( color[2] );glNormal3fv( normals[2] ); glVertex2fv( p[2] );
    glColor4fv( color[3] );glNormal3fv( normals[3] ); glVertex2fv( p[3] );
  glEnd();
}

void OutputGL::drawCircle( sgVec2 p, int radius ) {

  glBegin(GL_LINE_LOOP);
  for (int i = 0; i < SUBDIVISIONS; i++) {
    glVertex2f( p[0] + circle_x[i] * radius,
		p[1] + circle_y[i] * radius );
  }
  glEnd();
}

void OutputGL::drawLine( sgVec2 p1, sgVec2 p2 ) {
  glBegin(GL_LINES);
  glVertex2fv(p1);
  glVertex2fv(p2);
  glEnd();
}

void OutputGL::drawText( sgVec2 p, char *text ) {
  if (useTexturedFont) {
    textRenderer.begin();
    textRenderer.start2fv( p );
    textRenderer.puts( text );
    textRenderer.end();
    glDisable(GL_TEXTURE_2D);
  } else {
    glutFont->drawString( text, (int)p[0], (int)p[1] );
  }
}
