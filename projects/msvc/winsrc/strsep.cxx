// strsep.cxx
// Just a simple native WIN32 port of 'strsep'
// from : http://www.nersc.gov/~scottc/misc/docs/bro.1.0.3/strsep_8c-source.html
/*-
 * Copyright (c) 1990, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
 
#include "config.h"
 
#ifndef HAVE_STRSEP

#include <string>
#include "strsep.h"

/*
 * Get next token from string *stringp, where tokens are possibly-empty
 * strings separated by characters from delim.
 *
 * Writes NULs into the string at *stringp to end tokens.
 * delim need not remain constant from call to call.
 * On return, *stringp points past the last NUL written (if there might
 * be further tokens), or is NULL (if there are definitely no more tokens).
 *
 * If *stringp is NULL, strsep returns NULL.
 */

char * strsep(char ** stringp, const char *delim)
{
   char *s;
   const char *spanp;
   int c, sc;
   char *tok;

   if ((s = *stringp) == NULL)
      return (NULL);
   for (tok = s;;) {
      c = *s++;
      spanp = delim;
      do {
         if ((sc = *spanp++) == c) {
            if (c == 0)
               s = NULL;
            else
               s[-1] = 0;
            *stringp = s;
            return (tok);
         }
      } while (sc != 0);
   }

   return (NULL); /* NOTREACHED */
}

/* ================================================================================
   If *stringp is NULL, the strsep() function returns NULL and does nothing else.
   Otherwise, this function finds the first token in the string *stringp, where
   tokens are delimited by symbols in the string delim.  This token is terminated
   with a '\0' character (by overwriting the delimiter) and *stringp is updated to
   point past the token.  In case no delimiter was found, the token is taken to be
   the entire string *stringp, and *stringp is made NULL.
   ================================================================================ */
char * strsep_failed(char * * stringp, const char * delim)
{
   char * cp = NULL;
   char * found, * fnd;
   const char * match;
   int   cyc = 1;

   if( stringp )
      cp = *stringp;

   if(!cp)
      return NULL;

   if( !delim || (*delim == 0) )
   {
      *stringp = NULL;
      return cp;  /* return whole string as token */
   }

   found = strchr(cp, *delim);
   if(!found)  /* have not even found first match */
   {
      *stringp = NULL;
      return cp;  /* return whole string as token */
   }
   while( cyc )
   {
      match = delim;
      fnd = found;
      match++; /* start matching to this */
      fnd++;   /* commencing next char of found */
      while(*match) {
         if( *match != *fnd ) {
            /* failed to match string */
            fnd = found + 1;  /* move on one char */
            found = strchr(fnd, *delim);  /* do another search for first */
            if( !found )   /* if NOT found ... it is all over */
            {
               *stringp = NULL;  /* null the next, and */
               return cp;        /* return whole string as token */
            }
         }
         match++; /* bump to next match */
         fnd++;   /* and next in string */
      }

      /* ran out of match, thus a successful find */
      *stringp = fnd;   /* return pointer to next after match */
      *found = 0;       /* send found zero - MODIFYING the string */
      cyc = 0;          /* release from cycle - could just break */
   }
   return cp;  /* return start of token, now zero terminated */
}

#endif // #ifndef HAVE_STRSEP

// eof - strsep.cxx
