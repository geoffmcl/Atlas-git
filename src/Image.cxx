/*-------------------------------------------------------------------------
  Image.cxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

  This file is part of Atlas.

  Atlas is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Atlas is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU General Public License
  along with Atlas.  If not, see <http://www.gnu.org/licenses/>.
  ---------------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cassert>

#include <png.h>
#include <jpeglib.h>

#include <plib/pu.h>		// For OpenGL stuff

#include "Image.hxx"
#include "misc.hxx"

// This is a constant representing "Not an Elevation" - it is
// guaranteed to be less than any possible real elevation value.
static const float NanE = -std::numeric_limits<float>::max();

// Error-handling structures and code.
struct my_error_mgr {
    struct jpeg_error_mgr pub;	// "public" fields
    jmp_buf setjmp_buffer;	// For return to caller
};

typedef struct my_error_mgr * my_error_ptr;

// This routine replaces the standard error_exit method.  Instead of
// just exiting, as the standard method does, we jump to the setjmp
// point (there we will print an error message, clean up and return).
METHODDEF(void) my_error_exit(j_common_ptr cinfo)
{
    // cinfo->err really points to a my_error_mgr struct, so coerce
    // the pointer.
    my_error_ptr myerr = (my_error_ptr)cinfo->err;

    // Return control to the setjmp point.
    longjmp(myerr->setjmp_buffer, 1);
}

char *loadJPEG(const char *filename, int *width, int *height, int *depth,
	       float *maxElev)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
	return NULL;
    }

    jpeg_decompress_struct cinfo;
    memset(&cinfo, 0, sizeof cinfo);

    // Set up the normal JPEG error routines, then override
    // error_exit.
    my_error_mgr jerr;
    memset(&jerr, 0, sizeof jerr);
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
	// When an error occurs, we'll jump to this block of code.
	// First, output the name of the file and the canned JPEG
	// error message.
	fprintf(stderr, "%s: ", filename);
	(cinfo.err->output_message)((jpeg_common_struct *)&cinfo);

	// Clean things up and return.
	jpeg_destroy_decompress(&cinfo);
	fclose(fp);
	return NULL;
    }

    // Create the JPEG decompression object.
    jpeg_create_decompress(&cinfo);

    // Specify the data source.
    jpeg_stdio_src(&cinfo, fp);

    // Map stores elevation information for the map in the APP1
    // marker, so tell the JPEG library we're interested.  This must
    // be done before jpeg_read_header.
    jpeg_save_markers(&cinfo, JPEG_APP0 + 1, 0xFFFF);

    jpeg_read_header(&cinfo, TRUE);

    // Extract useful information from the header.
    *width = cinfo.image_width;
    *height = cinfo.image_height;
    *depth = 0;

    if (maxElev) {
	*maxElev = NanE;
	jpeg_saved_marker_ptr marker;
	for (marker = cinfo.marker_list; 
	     marker != NULL; 
	     marker = marker->next) {
	    if ((marker->marker == JPEG_APP0 + 1) &&
		(sscanf((const char *)marker->data, 
			"Map Maximum Elevation %f", maxElev) == 1)) {
		// Note that a JOCTET is 8 bits, so the cast is safe.
		break;
	    }
	}
    }

    // Now get the image.
    jpeg_start_decompress(&cinfo);

    char *image = NULL;
    if (cinfo.out_color_space == JCS_RGB) {
	*depth = 3;
	image = new char[cinfo.output_width * cinfo.output_height * *depth];

	while (cinfo.output_scanline < cinfo.output_height) {
	    char *buf = 
		&image[cinfo.output_width * cinfo.output_scanline * *depth];
	    jpeg_read_scanlines(&cinfo, (JSAMPARRAY)&buf, 1);
	}
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    fclose(fp);

    return image;
}

char *loadPNG(const char *filename, int *width, int *height, int *depth,
	      float *maxElev)
{
    char *header[8];

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
	return NULL;
    }

    // Check to see if this might be PNG.
    fread(header, 1, 8, fp);
    if (png_sig_cmp((png_bytep)header, 0, 8)) {
	return NULL;
    }

    // Allocate some structures.
    png_structp png_ptr = 
	png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
	return NULL;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
	png_destroy_read_struct(&png_ptr, NULL, NULL);
	return NULL;
    }

    // Initialize IO.
    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);

    double gamma;
    if (png_get_gAMA(png_ptr, info_ptr, &gamma)) {
	png_set_gamma(png_ptr, 2.0, gamma);
    }
  
    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_PALETTE) {
	//     png_set_expand(png_ptr);
	png_set_palette_to_rgb(png_ptr);
    }

    png_read_update_info(png_ptr, info_ptr);

    *width = png_get_image_width(png_ptr, info_ptr);
    *height = png_get_image_height(png_ptr, info_ptr);
    *depth = 0;

    // We only accept RGB and RGBA files.
    int color_type = png_get_color_type(png_ptr, info_ptr);
    if (color_type == PNG_COLOR_TYPE_RGB) {
	*depth = 3;
    } else if (color_type == PNG_COLOR_TYPE_RGBA) {
	*depth = 4;
    } else {
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	return NULL;
    }

    // Allocate image chunk.
    png_bytep *rows = new png_bytep[*height];
    char *image = new char[*width * *height * *depth];
    for (int i = 0; i < *height; i++) {
	rows[i] = (png_bytep)(image + i * *width * *depth);
    }

    // Read the image.
    png_read_image(png_ptr, rows);
    png_read_end(png_ptr, NULL);

    // Read the maximum image height (if it exists);
    if (maxElev) {
	*maxElev = NanE;
	int num_text;
	png_text *text_ptr;
	png_get_text(png_ptr, info_ptr, &text_ptr, &num_text);
	for (int i = 0; i < num_text; i++) {
	    if (strcmp(text_ptr[i].key, "Map Maximum Elevation") == 0) {
		// Got it.  Extract the elevation.
		sscanf(text_ptr[i].text, "%f", maxElev);
		break;
	    }
	}
    }

    delete[] rows;
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    return image;
}

void saveJPEG(const char *file, int quality, 
	      GLubyte *image, int width, int height, float maxElev)
{
    // Open the output file.
    FILE *fp = fopen(file, "wb");
    if (!fp) {
	fprintf(stderr, "saveJPEG: can't create '%s'\n", file);
	return;
    }

    jpeg_compress_struct cinfo;
    memset(&cinfo, 0, sizeof cinfo);

    jpeg_error_mgr jerr;
    memset(&jerr, 0, sizeof jerr);
    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, true);

    jpeg_start_compress(&cinfo, TRUE);

    // We want to pass the maximum elevation along with the image, so
    // we insert it as the marker "Map Maximum Elevation <elev>",
    // where <elev> is in feet, rounded to the nearest foot.
    globalString.printf("Map Maximum Elevation %.0f", maxElev);
    // We use the marker type APP1 (APP0, APP8, and APP14 are commonly
    // used by other applications, and the COM marker should be used
    // for user comments, not program-supplied data).  Note: a JOCTET
    // is 8 bits wide.
    jpeg_write_marker(&cinfo, JPEG_APP0 + 1, 
		      (const JOCTET *)globalString.str(), 
		      strlen(globalString.str()));

    while (cinfo.next_scanline < cinfo.image_height) {
	// A little bit of hackiness.  The jpeg library expects an
	// array of pointers to rows, so we can't just hand it our
	// monolithic array.
	JSAMPROW r[1];
	r[0] = &image[(height - cinfo.next_scanline - 1) * width * 3];
	jpeg_write_scanlines(&cinfo, r, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    fclose(fp);
}

void savePNG(const char *file, 
	     GLubyte *image, int width, int height, float maxElev)
{
    // Open the output file.
    FILE *fp = fopen(file, "wb");
    if (!fp) {
	fprintf(stderr, "savePNG: can't create '%s'\n", file);
	return;
    }

    // Create PNG structure.
    png_structp png_ptr = 
	png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    assert(png_ptr);

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
	png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
	return;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
	return;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, 
		 PNG_COLOR_TYPE_RGB, 
		 PNG_INTERLACE_NONE, 
		 PNG_COMPRESSION_TYPE_DEFAULT,
		 PNG_FILTER_TYPE_DEFAULT);
    
    // We want to pass the maximum elevation along with the image, so
    // we place it in a text comment structure.  The keyword is "Map
    // Maximum Elevation", and the text is the elevation (in feet,
    // rounded to the nearest foot).
    globalString.printf("%.0f", maxElev);

    png_text text_ptr[1];
    text_ptr[0].key = (char *)"Map Maximum Elevation";
    text_ptr[0].text = (char *)globalString.str();
    text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
    png_set_text(png_ptr, info_ptr, text_ptr, 1);

    png_write_info(png_ptr, info_ptr);

    png_byte **row_pointers = new png_byte*[height];
    for (int i = 0; i < height; i++) {
	row_pointers[i] = (png_byte*)(image + width * (height - i - 1) * 3);
    }

    // Write the image.
    png_write_image(png_ptr, row_pointers);

    delete[] row_pointers;

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    fclose(fp);
}

