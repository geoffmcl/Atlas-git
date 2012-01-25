/*-------------------------------------------------------------------------
  Image.hxx

  Written by Brian Schack

  Copyright (C) 2009 - 2012 Brian Schack

  Functions for loading and saving image files.  Currently JPEG and
  PNG are supported.

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

#ifndef _IMAGE_H_
#define _IMAGE_H_

enum ImageType {PNG, JPEG};

char *loadJPEG(const char *filename, int *width, int *height, int *depth,
	       float *maxElev = NULL);
char *loadPNG(const char *filename, int *width, int *height, int *depth,
	      float *maxElev = NULL);

void saveJPEG(const char *file, int quality, 
	      GLubyte *image, int width, int height, float maxElev);
void savePNG(const char *file, 
	     GLubyte *image, int width, int height, float maxElev);

#endif
