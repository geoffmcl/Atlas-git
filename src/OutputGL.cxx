#include <iostream.h>
#include <png.h>
#include "OutputGL.hxx"

OutputGL::OutputGL( char *filename, int size ) : 
  GfxOutput::GfxOutput(filename, size), filename(filename)
{
  glViewport( 0, 0, size, size );
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity();
  glOrtho( 0.0f, (GLfloat)size, (GLfloat)size, 0.0f, -1.0f, 1.0f );
  glMatrixMode( GL_MODELVIEW );
  glLoadIdentity();

  glEnable(GL_BLEND);
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
}

OutputGL::~OutputGL() {
  closeOutput();
}

void OutputGL::closeOutput() {
  if (!open)
    return;

  open = false;

  GLubyte *image = new GLubyte[size*size*3];
  if (!image)
    return;

  // grab screen
  glFinish();
  glReadPixels(0, 0, size, size,
	       GL_RGB, GL_UNSIGNED_BYTE, image);

  FILE *fp = fopen(filename, "wb");
  if (!fp) {
    return;
  }

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
  png_set_IHDR(png_ptr, info_ptr, size, size, 8, PNG_COLOR_TYPE_RGB,
	       PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
	       PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png_ptr, info_ptr);

  png_byte **row_pointers = new png_byte*[size];
  for (int i = 1; i <= size; i++) {
    row_pointers[i-1] = (png_byte*)(image + size * (size-i) * 3);
  }

  // actually write the image
  png_write_image(png_ptr, row_pointers);

  delete row_pointers;

  png_write_end(png_ptr, info_ptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fp);

  delete image;

  return;           
}

void OutputGL::setColor( const float *rgba ) {
  glColor4fv(rgba);
}

void OutputGL::clear( const float *rgb ) {
  glClearColor(rgb[0], rgb[1], rgb[2], 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void OutputGL::drawTriangle( sgVec2 *p ) {
  glBegin(GL_TRIANGLES);
  glVertex2fv( p[0] );
  glVertex2fv( p[1] );
  glVertex2fv( p[2]);
  glEnd();
}

void OutputGL::drawQuad( sgVec2 *p ) {
  glBegin(GL_QUADS);
  glVertex2fv( p[0] );
  glVertex2fv( p[1] );
  glVertex2fv( p[2] );
  glVertex2fv( p[3] );
  glEnd();
}

void OutputGL::drawCircle( sgVec2 p, int radius ) {
  static const int SUBDIVISIONS = 18;

  glBegin(GL_LINE_STRIP);
  for (int i = 0; i <= SUBDIVISIONS; i++) {
    glVertex2f( p[0] + cos(2*SG_PI / SUBDIVISIONS * i) * radius,
		p[1] + sin(2*SG_PI / SUBDIVISIONS * i) * radius );
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
  puDrawString( simpleFont, text, (int)p[0], (int)p[1] );
}
