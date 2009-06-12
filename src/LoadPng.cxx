/*-------------------------------------------------------------------------
  LoadPng.cxx
  Some routines to make it easier to load PNG images
  This is *NOT* intended to be a complete PNG-loader

  Written by Per Liedman, started February 2000.
  Copyright (C) 2000 Per Liedman, liedman@home.se

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

#include <png.h>
#include <stdio.h>

// reads
// char *loadPng( char *filename, int *width, int *height ) {
char *loadPng( char *filename, int *width, int *height, int *depth ) {
  char *header[8];

  FILE *fp = fopen(filename, "rb");
  if (!fp) {
    //fprintf(stderr, "loadPng: Unable to open file \"%s\".\n", filename);
    return NULL;
  }

  // check to see if this might be PNG
  fread( header, 1, 8, fp );
  if (png_sig_cmp((png_bytep)header, 0, 8)) {
    //fprintf(stderr, "loadPng: File \"%s\" is not a PNG image.\n", filename);
    return NULL;
  }

  // allocate some structures
  png_structp png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, 
						NULL, NULL, NULL );
  if (!png_ptr) {
    //fprintf(stderr, "loadPng: Unable to allocate read structure.\n", filename);
    return NULL;
  }

  png_infop info_ptr  = png_create_info_struct( png_ptr );
  if (!png_ptr) {
    //fprintf(stderr, "loadPng: Unable to allocate info structure.\n", filename);
    return NULL;
  }

  png_infop end_info  = png_create_info_struct( png_ptr );
  if (!end_info) {
    //fprintf(stderr, "loadPng: Unable to allocate secondary info structure.\n", filename);
    return NULL;
  }

  // initialize IO
  png_init_io(png_ptr, fp);
  png_set_sig_bytes( png_ptr, 8 );

  png_read_info( png_ptr, info_ptr );

  double gamma;
  if ( png_get_gAMA(png_ptr, info_ptr, &gamma) ) {
    png_set_gamma( png_ptr, 2.0, gamma );
    // EYE - this was changing the colour values of map images we
    // loaded in, making it impossible to match live and rendered
    // scenery.
//   } else {
//     png_set_gamma( png_ptr, 2.0, 0.45455 );
  }
  
  if ( png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_PALETTE )
//     png_set_expand( png_ptr );
    png_set_palette_to_rgb( png_ptr );

  png_read_update_info( png_ptr, info_ptr );

  *width  = png_get_image_width (png_ptr, info_ptr);
  *height = png_get_image_height(png_ptr, info_ptr);

  int color_type = png_get_color_type(png_ptr, info_ptr);
  if (color_type == PNG_COLOR_TYPE_RGB) {
      *depth = 3;
  } else if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
      *depth = 4;
  } else {
      return NULL;
  }

  // allocate image chunk
  png_bytep *rows = new png_bytep[*height];
//   char *image = new char[(*width) * (*height)*3];     // 24 bits per pixel
  char *image = new char[(*width) * (*height) * (*depth)];
  for (int i = 0; i < *height; i++) {
//     rows[i] = (png_bytep)(image + i * (*width) * 3);
      rows[i] = (png_bytep)(image + i * (*width) * (*depth));
  }

  png_read_image(png_ptr, rows);

  delete[] rows;

  png_read_end( png_ptr, end_info );

  png_destroy_read_struct( &png_ptr, &info_ptr, &end_info );

  fclose( fp );

  return image;
}
