// MapEXT.hxx
#ifndef _MapEXT_hxx_
#define _MapEXT_hxx_
#ifdef __cplusplus
extern "C" {
#endif

extern bool Map_Init_Ext(GLuint * pfbo, GLuint * prbo, GLint textureSize);
extern void Map_Exit_Ext(GLuint * pfbo, GLuint * prbo);

#ifdef __cplusplus
}
#endif
#endif // #ifndef _MapEXT_hxx_
// eof - MapEXT.hxx
