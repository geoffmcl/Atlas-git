// win_utils.h
#ifndef _win_utils_h_
#define _win_utils_h_
/*-------------------------------------------------------------------------
  win_utils.h

  Written by Geoff R. McLane, started January 2008.

  Copyright (C) 2008 Geoff R. McLane

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

// win_utils.h
// Various minor utility functions for Atlas/Map suite of applications

#include <string>

#if defined(_MSC_VER) && (_MSC_VER < 1900)
float remainderf( float x, float y );
#endif // OLD _MSC_VER
extern char * basename( char * name );
extern void set_win_path_sep( std::string & s );
extern bool is_valid_path( std::string path );
extern void print_version_details(void);

#if 0 // ------------------
extern void ll2pt( double obj_latr, double obj_lonr,
           double cent_latr, double cent_lonr,
           double map_size, double map_zoom,
           double * px_pixel, double * py_pixel );
extern void pt2ll( double * pobj_latr, double * pobj_lonr,
           double cent_latr, double cent_lonr,
           double map_size,  double map_zoom,
           double x_pixel, double y_pixel );
extern bool check_map_executable( std::string exe );
extern std::string get_compiler_version_string( void );

extern int jpg_or_png_exists( const char * pf, int base );

#ifdef _DEBUG
extern void set_crt_dbg_mem(void);
extern void show_crt_dbg_mem(void);
#endif // _DEBUG


extern void win_exit( int val );
#endif // 0 - not yet needed

#endif // #ifndef _win_utils_h_
// eof - win_utils.h
