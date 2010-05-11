/* config.h - hand crafted just for MSVC - 2010-04-24 */
#ifndef _win32_config_h_
#define _win32_config_h_
/* ==============================================================
 * This file has nothing to do with the auto-generated src/config.h
 * used in unix with autotools. It is instead of, and a full
 * replacement of that auto file, which should be deleted,
 * if present in WIN32 to avoid conflict.
 * ============================================================== */

/* ==============================================================
 * THIS IS USER DEPENDANT - set it to wherever FG data is stored
 * There will be a file 'version' in the root.
 * ============================================================== */
#ifndef FGBASE_DIR
#define FGBASE_DIR "C:/FG/33/data"
#endif

/* --------------------------------------------------------------
 * Option: USE_GLEW_153
 * Use GLEW 1.5.3, or higher, to add GL extensions                */
//#define USE_GLEW_153 1  // just an experiment with GLEW library - in Map.cxx
#undef USE_GLEW_153
#undef HAVE_GLEXT_H
#define EXCLUDE_GLEXT_H
#define HAVE_SGGLEXT_H

/* -------------------------------------------------------------- */

/* --------------------------------------------------------------
 * Option: HAVE_TRI_UNORDERED
 * if you are in a windows system which HAS unordered_set
 * and unordered_map, as part of the std::tr1 implementation, then
 * you can try defining HAVE_TRI_UNORDERED                        */
#undef HAVE_TRI_UNORDERED
/* -------------------------------------------------------------- */

/* Remove some MSVC warnings */
#pragma warning(disable:4996) // pesky depreciation warning
#pragma warning(disable:4800) // pesky performance warning
#pragma warning(disable:4018) // signed/unsigned mismatch
#pragma warning(disable:4554) // check operator precedence

/* some simple 'defines' to get unix functions */
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define va_copy(a,b)    a = b
#define log2(a)     ( log(a) / log(2.0) )
#define log2f(a)    (float)log2(a)
//#define round(d) (int)(d + 0.5) // conflict with SGMisc::round<double>()!!!
#define rint(x) ((x > 0.0) ? floor( x + 0.5 ) : (x < 0.0) ? ceil( x - 0.5 ) : 0.0)
#define lrintf(x) rint(x)
#define asprintf atlas_asprintf

#if (defined(ATLAS_MAP) && defined(USE_GLEW_153))
/* ------------------------------------------------------------
 * Only for Map.cxx - ie ATLAS_MAP defined -
 * Use glext.h, from http://www.opengl.org/registry/api/glext.h,
 * functions by GLEW 1.5.3 from http://glew.sourceforge.net/
 * ------------------------------------------------------------ */
#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#ifndef HAVE_GLEXT_H
#define HAVE_GLEXT_H
#endif
#ifdef EXCLUDE_GLEXT_H
#undef EXCLUDE_GLEXT_H
#endif
#include <GL/glew.h>    // get GL extensions
/* Use the GLEW static library */
#ifdef NDEBUG
#pragma comment (lib, "glew32s.lib")   /* glew-1.5.3 static lib - Release  */
#else
#pragma comment (lib, "glew32sd.lib")   /* glew-1.5.3 static lib - Debug  */
#endif
#endif  /* #if (defined(ATLAS_MAP) && defined(USE_GLEW_153)) */

/* some replacement functions  */
#include "strsep.h"
#include "asprintf.h"
#include "win_utils.h"
#include <io.h>

#ifndef VERSION
#define VERSION  "0.4.8-MSVC9-WIN32"
#endif

#endif // #ifndef _win32_config_h_
// eof - config.h
