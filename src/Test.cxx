#include <stdio.h>
#include <GL/glut.h>

void reshape(int, int) {}
void redraw() {}

int main(int argc, char **argv) {
  glutInit( &argc, argv );
  glutInitDisplayMode( GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE );
  glutInitWindowSize( 512, 512 );
  glutCreateWindow( "Atlas" );

  glutReshapeFunc( reshape );
  glutDisplayFunc( redraw );
  glutMainLoop();
 
  return 0;
}

