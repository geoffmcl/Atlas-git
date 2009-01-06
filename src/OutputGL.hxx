#ifndef __OUTPUTGL_H__
#define __OUTPUTGL_H__

#include <simgear/compiler.h>
//#include SG_GLUT_H
#include <plib/fnt.h>
#include <plib/pu.h>
#include "Output.hxx"

#include <vector>
using std::vector;

class OutputGL : public GfxOutput {
public:
  OutputGL( const char *filename, int size, bool smooth_shading,
	    bool useTexturedFont, char *fontname, bool jpg = false,
            int q = 75, int r = 1 );
  ~OutputGL();

  virtual void openFragment( int x, int y, int size );
  virtual void closeFragment();
  virtual void closeOutput();

  virtual void setShade( bool shade );
  virtual bool getShade();
  virtual void setLightVector( sgVec3 light );
  virtual void setColor( const float *rgb );
  virtual void clear( const float *rgb );
  virtual void drawTriangle( const sgVec2 *p, const sgVec3 *normals );
  virtual void drawQuad    ( const sgVec2 *p, const sgVec3 *normals );
  virtual void drawTriangle( const sgVec2 *p, const sgVec3 *normals, const sgVec4 *color );
  virtual void drawQuad    ( const sgVec2 *p, const sgVec3 *normals, const sgVec4 *color );
  virtual void drawCircle  ( sgVec2 p, int radius );
  virtual void drawLine    ( sgVec2 p1, sgVec2 p2 );
  virtual void drawText    ( sgVec2 p, char *text );

  //void setFont( bool useTexturedFont );

protected:
  enum { SUBDIVISIONS = 18 };

  static const float BRIGHTNESS;
  static float circle_x[SUBDIVISIONS], circle_y[SUBDIVISIONS];

  fntTexFont *font;
  fntRenderer textRenderer;
  puFont *glutFont;
  const char *filename;
  bool useTexturedFont, shade, jpeg;
  int jpeg_quality, rescale;
  GLubyte * image;

  sgVec4 light_vector;
};

#endif

