/*-------------------------------------------------------------------------
  LoadJpg.cxx
  Some routines to make it easier to load JPEG images
  This is *NOT* intended to be a complete JPEG-loader

  Written by Fred Bouvier, started January 2005.
  Copyright (C) 2005 Fred Bouvier, fredb@users.sourceforge.net

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
---------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
extern "C" {
#include <jpeglib.h>
}

// reads
char *loadJpg( char *filename, int *width, int *height ) {
  char *header[8];

  FILE *fp = fopen(filename, "rb");
  if (!fp) {
    //fprintf(stderr, "loadPng: Unable to open file \"%s\".\n", filename);
    return NULL;
  }

  jpeg_decompress_struct cinfo;
  memset( &cinfo, 0, sizeof cinfo );

  jpeg_error_mgr jerr;
  memset( &jerr, 0, sizeof jerr );

  cinfo.err = jpeg_std_error( &jerr );
  jpeg_create_decompress( &cinfo );
  jpeg_stdio_src( &cinfo, fp );
  jpeg_read_header( &cinfo, TRUE );

  *width = cinfo.image_width;
  *height = cinfo.image_height;

  jpeg_start_decompress( &cinfo );

  char *image = 0;
  if ( cinfo.out_color_space == JCS_RGB ) {
   image = new char[ cinfo.output_width * cinfo.output_height * 3 ];

   while ( cinfo.output_scanline < cinfo.output_height) {
      char *buf;
      if ( 0 ) {
	 buf = &image[ cinfo.output_width * ( cinfo.output_height - ( cinfo.output_scanline + 1 ) ) * 3 ];
      } else {
	 buf = &image[ cinfo.output_width * cinfo.output_scanline * 3 ];
      }
      jpeg_read_scanlines( &cinfo, (JSAMPARRAY)&buf, 1 );
   }
  }

  jpeg_finish_decompress( &cinfo );
  jpeg_destroy_decompress( &cinfo );

  fclose( fp );

  return image;
}
