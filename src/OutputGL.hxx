#ifndef __OUTPUTGL_H__
#define __OUTPUTGL_H__

#include <GL/glut.h>
#include <plib/fnt.h>
#include <plib/pu.h>
#include "Output.hxx"

class OutputGL : public GfxOutput {
public:
  OutputGL( char *filename, int size, bool useTexturedFont = true );
  ~OutputGL();

  virtual void closeOutput();

  virtual void setColor( const float *rgb );
  virtual void clear( const float *rgb );
  virtual void drawTriangle( sgVec2 *p );
  virtual void drawQuad    ( sgVec2 *p );
  virtual void drawCircle  ( sgVec2 p, int radius );
  virtual void drawLine    ( sgVec2 p1, sgVec2 p2 );
  virtual void drawText    ( sgVec2 p, char *text );

  //void setFont( bool useTexturedFont );

protected:
  fntTexFont *font;
  fntRenderer textRenderer;
  puFont *glutFont;
  char *filename;
  bool useTexturedFont;
};

#endif

