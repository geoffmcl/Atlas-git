#ifndef __OUTPUTGL_H__
#define __OUTPUTGL_H__

#include <GL/glut.h>
#include <plib/fnt.h>
#include <plib/pu.h>
#include "Output.hxx"

class OutputGL : public GfxOutput {
public:
  OutputGL( char *filename, int size, bool smooth_shading,
	    bool useTexturedFont, char *fontname );
  ~OutputGL();

  virtual void closeOutput();

  virtual void setShade( bool shade );
  virtual bool getShade();
  virtual void setLightVector( sgVec3 light );
  virtual void setColor( const float *rgb );
  virtual void clear( const float *rgb );
  virtual void drawTriangle( const sgVec2 *p, const sgVec3 *normals );
  virtual void drawQuad    ( const sgVec2 *p, const sgVec3 *normals );
  virtual void drawCircle  ( sgVec2 p, int radius );
  virtual void drawLine    ( sgVec2 p1, sgVec2 p2 );
  virtual void drawText    ( sgVec2 p, char *text );

  //void setFont( bool useTexturedFont );

protected:
  static const float BRIGHTNESS = 0.6f;
  static const int SUBDIVISIONS = 18;
  static float circle_x[SUBDIVISIONS], circle_y[SUBDIVISIONS];

  fntTexFont *font;
  fntRenderer textRenderer;
  puFont *glutFont;
  char *filename;
  bool useTexturedFont, shade;

  sgVec4 light_vector;
};

#endif

