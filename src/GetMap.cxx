/*-------------------------------------------------------------------------
  Program for getting landsat image and preparing them for Atlas

  Written by Frederic Bouvier, started September 2004.

  Copyright (C) 2005 Frederic Bouvier, fredb@users.sourceforge.net

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
---------------------------------------------------------------------------

Try with --base-url='http://wms.jpl.nasa.gov/cgi-bin/wms.cgi?LAYERS=modis,global_mosaic&styles=default,visual&'

---------------------------------------------------------------------------
  CHANGES
  2005-09-28        Initial version
---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <curl/curl.h>

bool verbose = false;
int size = 256;
int min_lat = 1000;
int min_lon = 1000;
int max_lat = -1000;
int max_lon = -1000;
std::string outp;
std::string base_url;
size_t file_size;
std::ostringstream memstream;

/*****************************************************************************/

void print_help() {
  printf("GetMap - FlightGear mapping retrieval utility\n\n");
  printf("Usage:\n");
  printf("  --size=pixels           Create map of size pixels*pixels (default 256)\n");
  printf("  --base-url=url          Beginning of the url of the WMS server\n");
  printf("  --atlas=path            Create maps of all scenery, and store them in path\n");
  printf("  --min-lat               Minimum latitude\n");
  printf("  --max-lat               Maximum latitude\n");
  printf("  --min-lon               Minimum longitude\n");
  printf("  --max-lon               Maximum longitude\n");
  printf("  --verbose               Display information during processing\n");
}

bool parse_arg(char* arg) {

  if ( sscanf(arg, "--size=%d", &size) == 1 ) {
    // Nothing
  } else if ( sscanf(arg, "--min-lat=%d", &min_lat) == 1 ) {
    // Nothing
  } else if ( sscanf(arg, "--min-lon=%d", &min_lon) == 1 ) {
    // Nothing
  } else if ( sscanf(arg, "--max-lat=%d", &max_lat) == 1 ) {
    // Nothing
  } else if ( sscanf(arg, "--max-lon=%d", &max_lon) == 1 ) {
    // Nothing
  } else if ( strncmp(arg, "--atlas=", 8) == 0 ) {
    outp = arg+8;
  } else if ( strncmp(arg, "--base-url=", 11) == 0 ) {
    base_url = arg+11;
  } else if ( strcmp(arg, "--verbose") == 0 ) {
    verbose = true;
  } else if ( strcmp(arg, "--help") == 0 ) {
    print_help();
    exit(0);
  } else {
    return false;
  }

  return true;
}

size_t writeData_cb( void *buffer, size_t size, size_t nmemb, void *userp ) {
  memstream.write( (char *)buffer, size * nmemb );
  file_size += size * nmemb;
  return nmemb;
}

int progress_cb( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow ) {
  printf( "Gotten %lu bytes                  \r", (unsigned long)dlnow );
  return 0;
}

int main( int argc, char **argv ) {
  if (argc == 0)
    print_help();

  // process command line arguments
  for (int arg = 1; arg < argc; arg++) {
    if (!parse_arg(argv[arg])) {
      fprintf(stderr, "%s: unknown argument '%s'.\n", argv[0], argv[arg]);
      print_help();
      exit(1);
    }
  }

  if ( size & ( size-1 ) ) {
    fprintf(stderr, "%s: --size should be a power of 2.\n", argv[0]);
    exit(1);
  }

  if ( outp.empty() ) {
    fprintf(stderr, "%s: --atlas option missing.\n", argv[0]);
    exit(1);
  }

  if ( base_url.empty() ) {
    fprintf(stderr, "%s: --base-url option missing.\n", argv[0]);
    exit(1);
  }
  if ( min_lat == 1000 ) {
    fprintf(stderr, "%s: --min-lat option missing.\n", argv[0]);
    exit(1);
  }
  if ( min_lat < -90 || min_lat >= 90 ) {
    fprintf(stderr, "%s: --min-lat out of range. Should be between -90 and 90.\n", argv[0]);
    exit(1);
  }
  if ( min_lon == 1000 ) {
    fprintf(stderr, "%s: --min-lon option missing.\n", argv[0]);
    exit(1);
  }
  if ( min_lon < -180 || min_lon >= 180 ) {
    fprintf(stderr, "%s: --min-lon out of range. Should be between -180 and 180.\n", argv[0]);
    exit(1);
  }
  if ( max_lat == -1000 ) {
    fprintf(stderr, "%s: --max-lat option missing.\n", argv[0]);
    exit(1);
  }
  if ( max_lat < -90 || max_lat >= 90 ) {
    fprintf(stderr, "%s: --max-lat out of range. Should be between -90 and 90.\n", argv[0]);
    exit(1);
  }
  if ( max_lon == -1000 ) {
    fprintf(stderr, "%s: --max-lon option missing.\n", argv[0]);
    exit(1);
  }
  if ( max_lon < -180 || max_lon >= 180 ) {
    fprintf(stderr, "%s: --max-lon out of range. Should be between -180 and 180.\n", argv[0]);
    exit(1);
  }

  if ( min_lon > max_lon ) {
    max_lon += 360;
  }

  std::cout << "Getting landsat images from " << base_url.c_str() << std::endl;

  CURL *ceh = curl_easy_init();
  if ( ceh != 0 ) {
    for ( int y = min_lat; y < max_lat; y += 1 ) {
      for ( int x = min_lon; x < max_lon; x += 1 ) {
        int rx = x;
        if ( rx >= 180 ) {
          rx -= 360;
        }
        std::ostringstream fname;
        fname << outp << "/" << ( rx < 0 ? "w" : "e" ) << std::setw( 3 ) << std::setfill( '0' ) << abs(rx)
                            << ( y < 0 ? "s" : "n" ) << std::setw( 2 ) << std::setfill( '0' ) << abs(y)
                            << ".jpg";

        struct stat stat_buf;
        if ( stat( fname.str().c_str(), &stat_buf ) == 0 )
          continue;  

        std::ostringstream surl;
        surl << base_url << "REQUEST=GetMap&VERSION=1.1.1&WIDTH=" << size << "&HEIGHT=" << size << "&BBOX="
            << rx << "," << y << "," << rx+1 << "," << y+1 << "&FORMAT=image/jpeg&SRS=EPSG:4326";

        std::string url = surl.str();
        curl_easy_setopt( ceh, CURLOPT_URL, url.c_str() );
        curl_easy_setopt( ceh, CURLOPT_WRITEFUNCTION, writeData_cb );
        curl_easy_setopt( ceh, CURLOPT_WRITEDATA, 0 );
	curl_easy_setopt( ceh, CURLOPT_NOPROGRESS, 0 );
	curl_easy_setopt( ceh, CURLOPT_PROGRESSFUNCTION, progress_cb );
	curl_easy_setopt( ceh, CURLOPT_PROGRESSDATA, 0 );
	//curl_easy_setopt( ceh, CURLOPT_INFILESIZE, -1 );

        file_size = 0;
        memstream.str("");
        int success = curl_easy_perform( ceh );

        if ( success == 0 ) {
          char *cType;
          curl_easy_getinfo( ceh, CURLINFO_CONTENT_TYPE, &cType );
          if ( strcmp( cType, "image/jpeg" ) == 0 ) {
            std::ofstream fstr( fname.str().c_str(), std::ios::binary );
            fstr.write( memstream.str().data(), file_size );
            fstr.close();
            std::cout << "Written '" << fname.str().c_str() << "'" << std::endl;
          }
          else {
              std::cerr << "Not 'image/jpeg'! got " << cType << "'" << std::endl;
          }
        }
        else {
            std::cerr << "Failed '" << url.c_str() << "'" << std::endl;
        }
      }
    }
  }
}
