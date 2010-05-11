
// asprintf.cxx
// native WIN32 implementation of asprintf(), of GNU fame
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // #ifdef HAVE_CONFIG_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void
simpleVasprintf(char **      retvalP,
                const char * const fmt,
                va_list            varargs) {
/*----------------------------------------------------------------------------
   This is a poor man's implementation of vasprintf(), of GNU fame.
-----------------------------------------------------------------------------*/
    size_t const initialSize = 4096;
    char * result;

    result = (char *)malloc(initialSize);
    if (result != NULL) {
        size_t bytesNeeded;
        bytesNeeded = vsnprintf(result, initialSize, fmt, varargs);
        if (bytesNeeded == -1) {    // returns -1 if truncated
            bytesNeeded = (initialSize * 2); // got truncation - try DOUBLE the size
        }
        if (bytesNeeded > initialSize) {
            free(result);
            result = (char *)malloc(bytesNeeded);
            if (result != NULL) {
                bytesNeeded = vsnprintf(result, bytesNeeded, fmt, varargs);
                if (bytesNeeded == -1) {    // returns -1 if truncated
                    free(result);
                    result = NULL;  // treat like a MEMORY FAILURE
                }
            }
        } else if (bytesNeeded == initialSize) {
            if (result[initialSize-1] != '\0') {
                /* This is one of those old systems where vsnprintf()
                   returns the number of bytes it used, instead of the
                   number that it needed, and it in fact needed more than
                   we gave it.  Rather than mess with this highly unlikely
                   case (old system and string > 4095 characters), we just
                   treat this like an out of memory failure.
                */
                free(result);
                result = NULL;
            }
        }
    }
    *retvalP = result;
}

void
atlas_vasprintf(char ** retvalP,
                 const char *  const fmt,
                 va_list             varargs) {
    
    char * string;

#if HAVE_ASPRINTF
    vasprintf(&string, fmt, varargs);
#else
    simpleVasprintf(&string, fmt, varargs);
#endif

    if (string == NULL)
    {
       // *retvalP = atlas_strsol;
       fprintf( stderr, "CRITICAL ERROR: MEMORY ALLOCATION FAILED! Aborting!!\n" );
       exit(1);
    }
    else
        *retvalP = string;
}

void
atlas_asprintf(char ** retvalP, const char * const fmt, ...) {

    va_list varargs;  /* pointer into the function stack (simple, not mysterious, nor a structure */

    va_start(varargs, fmt); /* set ponter to fmt + 1 */

    atlas_vasprintf(retvalP, fmt, varargs);

    va_end(varargs);
}

// eof - asprintf.cxx
