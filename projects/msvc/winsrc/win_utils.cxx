/*-------------------------------------------------------------------------
  win_utils.cxx

  Written by Geoff R. McLane, started January 2008.

  Copyright (C) 2008-2010 Geoff R. McLane

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

// win_utils.cxx
// Various minor utility functions for Atlas/Map suite of applications

#undef EXTRA_DEBUG_WIN_UTILS

#pragma warning(disable:4996)

#include <sys\types.h>
#include <sys\stat.h>   // for stat
#include "config.h"

#include <string>
#include <sstream>
#include <iostream>
#include <windows.h>    // for Sleep(ms)
#include <math.h>
#include <conio.h>      // for _kbhit() and _getch()
#include <simgear/compiler.h>
#include <zlib.h>
#include <png.h>
#include <jpeglib.h>
#include <strsafe.h>
#include "win_utils.h"
#include "win_versions.h"

using namespace std;

#define AT_STRINGIZE(X) AT_DO_STRINGIZE(X)
#define AT_DO_STRINGIZE(X) #X

#ifndef EndBuf
#define EndBuf(a)   ( a + strlen(a) )
#endif

static char * c_date = __DATE__;
static char * c_time = __TIME__;

int check_key_available( void )
{
   int chr = _kbhit();
   return chr;
}

void make_beep_sound(void)
{
    //printf( 0x07 );
    //    freq.duration
    Beep( 750, 300 );
}

void win_terminate(void)
{
    // do nothing
}

// ALL ABOUT WINDOWS VERSIONS
// ==========================
// from: http://msdn.microsoft.com/en-us/library/ms724429(VS.85).aspx
#pragma comment(lib, "User32.lib")

#ifndef BUFSIZE
#define BUFSIZE 256
#endif

typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
typedef BOOL (WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);
BOOL GetOSDisplayString( LPTSTR pszOS)
{
   OSVERSIONINFOEX osvi;
   SYSTEM_INFO si;
   PGNSI pGNSI;
   PGPI pGPI;
   BOOL bOsVersionInfoEx;
   DWORD dwType;

   ZeroMemory(&si, sizeof(SYSTEM_INFO));
   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));

   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

   if( !(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi)) )
      return FALSE;

   // Call GetNativeSystemInfo if supported or GetSystemInfo otherwise.
   pGNSI = (PGNSI) GetProcAddress( GetModuleHandle(TEXT("kernel32.dll")), "GetNativeSystemInfo");
   if(NULL != pGNSI)
      pGNSI(&si);
   else GetSystemInfo(&si);

   if ( VER_PLATFORM_WIN32_NT == osvi.dwPlatformId && 
        osvi.dwMajorVersion > 4 )
   {
      StringCchCopy(pszOS, BUFSIZE, TEXT("Microsoft "));

      // Test for the specific product.
      if ( osvi.dwMajorVersion == 6 )
      {
         if( osvi.dwMinorVersion == 0 )
         {
            if( osvi.wProductType == VER_NT_WORKSTATION )
                StringCchCat(pszOS, BUFSIZE, TEXT("Windows Vista "));
            else StringCchCat(pszOS, BUFSIZE, TEXT("Windows Server 2008 " ));
         }

         if ( osvi.dwMinorVersion == 1 )
         {
            if( osvi.wProductType == VER_NT_WORKSTATION )
                StringCchCat(pszOS, BUFSIZE, TEXT("Windows 7 "));
            else StringCchCat(pszOS, BUFSIZE, TEXT("Windows Server 2008 R2 " ));
         }
         
         pGPI = (PGPI) GetProcAddress(
            GetModuleHandle(TEXT("kernel32.dll")), 
            "GetProductInfo");

         pGPI( osvi.dwMajorVersion, osvi.dwMinorVersion, 0, 0, &dwType);

         switch( dwType )
         {
            case PRODUCT_ULTIMATE:
               StringCchCat(pszOS, BUFSIZE, TEXT("Ultimate Edition" ));
               break;
            case PRODUCT_PROFESSIONAL:
               StringCchCat(pszOS, BUFSIZE, TEXT("Professional" ));
               break;
            case PRODUCT_HOME_PREMIUM:
               StringCchCat(pszOS, BUFSIZE, TEXT("Home Premium Edition" ));
               break;
            case PRODUCT_HOME_BASIC:
               StringCchCat(pszOS, BUFSIZE, TEXT("Home Basic Edition" ));
               break;
            case PRODUCT_ENTERPRISE:
               StringCchCat(pszOS, BUFSIZE, TEXT("Enterprise Edition" ));
               break;
            case PRODUCT_BUSINESS:
               StringCchCat(pszOS, BUFSIZE, TEXT("Business Edition" ));
               break;
            case PRODUCT_STARTER:
               StringCchCat(pszOS, BUFSIZE, TEXT("Starter Edition" ));
               break;
            case PRODUCT_CLUSTER_SERVER:
               StringCchCat(pszOS, BUFSIZE, TEXT("Cluster Server Edition" ));
               break;
            case PRODUCT_DATACENTER_SERVER:
               StringCchCat(pszOS, BUFSIZE, TEXT("Datacenter Edition" ));
               break;
            case PRODUCT_DATACENTER_SERVER_CORE:
               StringCchCat(pszOS, BUFSIZE, TEXT("Datacenter Edition (core installation)" ));
               break;
            case PRODUCT_ENTERPRISE_SERVER:
               StringCchCat(pszOS, BUFSIZE, TEXT("Enterprise Edition" ));
               break;
            case PRODUCT_ENTERPRISE_SERVER_CORE:
               StringCchCat(pszOS, BUFSIZE, TEXT("Enterprise Edition (core installation)" ));
               break;
            case PRODUCT_ENTERPRISE_SERVER_IA64:
               StringCchCat(pszOS, BUFSIZE, TEXT("Enterprise Edition for Itanium-based Systems" ));
               break;
            case PRODUCT_SMALLBUSINESS_SERVER:
               StringCchCat(pszOS, BUFSIZE, TEXT("Small Business Server" ));
               break;
            case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
               StringCchCat(pszOS, BUFSIZE, TEXT("Small Business Server Premium Edition" ));
               break;
            case PRODUCT_STANDARD_SERVER:
               StringCchCat(pszOS, BUFSIZE, TEXT("Standard Edition" ));
               break;
            case PRODUCT_STANDARD_SERVER_CORE:
               StringCchCat(pszOS, BUFSIZE, TEXT("Standard Edition (core installation)" ));
               break;
            case PRODUCT_WEB_SERVER:
               StringCchCat(pszOS, BUFSIZE, TEXT("Web Server Edition" ));
               break;
         }
      }

      if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2 )
      {
         if( GetSystemMetrics(SM_SERVERR2) )
            StringCchCat(pszOS, BUFSIZE, TEXT( "Windows Server 2003 R2, "));
         else if ( osvi.wSuiteMask & VER_SUITE_STORAGE_SERVER )
            StringCchCat(pszOS, BUFSIZE, TEXT( "Windows Storage Server 2003"));
         else if ( osvi.wSuiteMask & VER_SUITE_WH_SERVER )
            StringCchCat(pszOS, BUFSIZE, TEXT( "Windows Home Server"));
         else if( osvi.wProductType == VER_NT_WORKSTATION &&
                  si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
         {
            StringCchCat(pszOS, BUFSIZE, TEXT( "Windows XP Professional x64 Edition"));
         }
         else StringCchCat(pszOS, BUFSIZE, TEXT("Windows Server 2003, "));

         // Test for the server type.
         if ( osvi.wProductType != VER_NT_WORKSTATION )
         {
            if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_IA64 )
            {
                if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                   StringCchCat(pszOS, BUFSIZE, TEXT( "Datacenter Edition for Itanium-based Systems" ));
                else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                   StringCchCat(pszOS, BUFSIZE, TEXT( "Enterprise Edition for Itanium-based Systems" ));
            }
            else if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 )
            {
                if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                   StringCchCat(pszOS, BUFSIZE, TEXT( "Datacenter x64 Edition" ));
                else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                   StringCchCat(pszOS, BUFSIZE, TEXT( "Enterprise x64 Edition" ));
                else StringCchCat(pszOS, BUFSIZE, TEXT( "Standard x64 Edition" ));
            }
            else
            {
                if ( osvi.wSuiteMask & VER_SUITE_COMPUTE_SERVER )
                   StringCchCat(pszOS, BUFSIZE, TEXT( "Compute Cluster Edition" ));
                else if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                   StringCchCat(pszOS, BUFSIZE, TEXT( "Datacenter Edition" ));
                else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                   StringCchCat(pszOS, BUFSIZE, TEXT( "Enterprise Edition" ));
                else if ( osvi.wSuiteMask & VER_SUITE_BLADE )
                   StringCchCat(pszOS, BUFSIZE, TEXT( "Web Edition" ));
                else StringCchCat(pszOS, BUFSIZE, TEXT( "Standard Edition" ));
            }
         }
      }

      if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 )
      {
         StringCchCat(pszOS, BUFSIZE, TEXT("Windows XP "));
         if( osvi.wSuiteMask & VER_SUITE_PERSONAL )
            StringCchCat(pszOS, BUFSIZE, TEXT( "Home Edition" ));
         else StringCchCat(pszOS, BUFSIZE, TEXT( "Professional" ));
      }

      if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0 )
      {
         StringCchCat(pszOS, BUFSIZE, TEXT("Windows 2000 "));

         if ( osvi.wProductType == VER_NT_WORKSTATION )
         {
            StringCchCat(pszOS, BUFSIZE, TEXT( "Professional" ));
         }
         else 
         {
            if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
               StringCchCat(pszOS, BUFSIZE, TEXT( "Datacenter Server" ));
            else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
               StringCchCat(pszOS, BUFSIZE, TEXT( "Advanced Server" ));
            else StringCchCat(pszOS, BUFSIZE, TEXT( "Server" ));
         }
      }

       // Include service pack (if any) and build number.
      //if( _tcslen(osvi.szCSDVersion) > 0 )
      if (strlen(osvi.szCSDVersion) > 0)
      {
          StringCchCat(pszOS, BUFSIZE, TEXT(" ") );
          StringCchCat(pszOS, BUFSIZE, osvi.szCSDVersion);
      }

      TCHAR buf[80];

      StringCchPrintf( buf, 80, TEXT(" (build %d)"), osvi.dwBuildNumber);
      StringCchCat(pszOS, BUFSIZE, buf);

      if ( osvi.dwMajorVersion >= 6 )
      {
         if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 )
            StringCchCat(pszOS, BUFSIZE, TEXT( ", 64-bit" ));
         else if (si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL )
            StringCchCat(pszOS, BUFSIZE, TEXT(", 32-bit"));
      }
      
      return TRUE; 
   }
   else
   {  
      printf( "This sample does not support this version of Windows.\n");
   }
   return FALSE;
}


BOOL GetSystemVersion( PTSTR lps )
{
    OSVERSIONINFOEX osvi;
    BOOL bOsVersionInfoEx;
    char * env;
   // Try calling GetVersionEx using the OSVERSIONINFOEX structure,
   // which is supported on Windows 2000.
   //
   // If that fails, try using the OSVERSIONINFO structure.
   if( !lps )
      return FALSE;

   *lps = 0;
   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
   if( !(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi)) )
   {
      // If OSVERSIONINFOEX doesn't work, try OSVERSIONINFO.
      osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
      if (! GetVersionEx ( (OSVERSIONINFO *) &osvi) ) 
         return FALSE;
   }

   switch (osvi.dwPlatformId)
   {
      case VER_PLATFORM_WIN32_NT:
      // Test for the product.
         if ( osvi.dwMajorVersion <= 4 )
            StringCchPrintf( lps, BUFSIZE, "MS Windows NT ");
         else if ( osvi.dwMajorVersion == 5 )
            StringCchPrintf( lps, BUFSIZE, "MS Windows 2000 ");

      // Test for workstation versus server.
#ifdef _WIN98_
         if( bOsVersionInfoEx )
         {
            if ( osvi.wProductType == VER_NT_WORKSTATION )
               StringCchPrintf(EndBuf(lps), BUFSIZE, "Professional " );

            if ( osvi.wProductType == VER_NT_SERVER )
               StringCchPrintf(EndBuf(lps), BUFSIZE, "Server " );
         }
         else
#endif // #ifdef _WIN98_
         {
            HKEY hKey;
            char szProductType[80];
            DWORD dwBufLen;

            RegOpenKeyEx( HKEY_LOCAL_MACHINE,
               "SYSTEM\\CurrentControlSet\\Control\\ProductOptions",
               0, KEY_QUERY_VALUE, &hKey );
            RegQueryValueEx( hKey, "ProductType", NULL, NULL,
               (LPBYTE) szProductType, &dwBufLen);
            RegCloseKey( hKey );
            if ( lstrcmpi( "WINNT", szProductType) == 0 )
               StringCchPrintf( EndBuf(lps), BUFSIZE, "Workstation " );
            if ( lstrcmpi( "SERVERNT", szProductType) == 0 )
               StringCchPrintf( EndBuf(lps), BUFSIZE, "Server " );
         }

         break;

      case VER_PLATFORM_WIN32_WINDOWS:

         if ((osvi.dwMajorVersion > 4) || 
            ((osvi.dwMajorVersion == 4) && (osvi.dwMinorVersion > 0)))
         {
             StringCchPrintf(lps, BUFSIZE, "MS Windows 98 ");
         } 
         else StringCchPrintf(lps, BUFSIZE, "MS Windows 95 ");

         break;

      case VER_PLATFORM_WIN32s:
         StringCchPrintf(lps, BUFSIZE, "MS Win32s ");
         break;
   }

   // Display version, service pack (if any), and build number.
   StringCchPrintf(EndBuf(lps), BUFSIZE, "version %d.%d %s (Build %d).",
            osvi.dwMajorVersion,
            osvi.dwMinorVersion,
            osvi.szCSDVersion,
            osvi.dwBuildNumber & 0xFFFF );

   env = getenv("OS");
   if (env)
       StringCchPrintf(EndBuf(lps),BUFSIZE, " (OS=%s)",env);

   return TRUE; 
}

bool get_processor_stg(char * cp)
{
    char * env;
    *cp = 0;
    env = getenv("PROCESSOR_ARCHITECTURE"); // like =x86
    if (env)
        StringCchPrintf(EndBuf(cp),BUFSIZE,"Arch: %s ", env);
    env = getenv("PROCESSOR_IDENTIFIER"); // like =x86 Family 6 Model 15 Stepping 11, GenuineIntel
    if (env)
        StringCchPrintf(EndBuf(cp),BUFSIZE,"Ident: %s ", env);
    env = getenv("PROCESSOR_LEVEL");
    if (env)
        StringCchPrintf(EndBuf(cp),BUFSIZE,"Lev: %s ", env);
    env = getenv("PROCESSOR_REVISION"); // =0f0b
    if (env)
        StringCchPrintf(EndBuf(cp),BUFSIZE,"Rev: %s ", env);
    env = getenv("NUMBER_OF_PROCESSORS");
    if (env)
        StringCchPrintf(EndBuf(cp),BUFSIZE,"Cnt: %s ", env);

    if (*cp)
        return true;

    return false;
}

void print_version_details(void)
{
    SYSTEMTIME st;
    char _tmp_buf[BUFSIZE];
    char * cp = _tmp_buf;
    char * env;
    char * cfg;
#ifdef NDEBUG
    cfg = "Release";
#else
    cfg = "Debug";
#endif

    printf("Default base dir : [%s]\n", FGBASE_DIR );
#if defined(USE_GLEW_153) // just an experiment with GLEW library - in Map.cxx
    printf("Using GLEW 1.5.3 library for GL extension functions.\n");
#else
    printf("NOT using GLEW 1.5.3 library for GL extension functions.\n");
#endif
    printf("Zlib version    : %s\n", zlibVersion());
    printf("PNG version     : %s", PNG_HEADER_VERSION_STRING); // png_get_header_version());
    printf("Jpeglib version : %d\n", JPEG_LIB_VERSION);
    printf("Compiler        : [%s] Config: %s\n", SG_COMPILER_STR, cfg);
    printf("Compile Date    : %s at %s\n", c_date, c_time);

    if (GetOSDisplayString(cp))
        printf("OS version      : %s\n",cp);
    else if (GetSystemVersion(cp))
        printf("os version      : %s\n",cp);

    if (get_processor_stg(cp))
        printf("Processor       : %s\n", cp);

    *cp = 0;
    env = getenv("COMPUTERNAME");
    if (env)
        StringCchPrintf(EndBuf(cp),BUFSIZE,"Running in      : %s ", env);
    env = getenv("USERNAME");
    if (env)
        StringCchPrintf(EndBuf(cp),BUFSIZE,"User: %s ", env);
    GetLocalTime(&st);
    StringCchPrintf(EndBuf(cp),BUFSIZE,"On: %4d/%02d/%02d, At: %02d:%02d:%02d",
        (st.wYear & 0xffff), (st.wMonth & 0xffff), (st.wDay & 0xffff),
        (st.wHour & 0xffff), (st.wMinute & 0xffff), (st.wSecond & 0xffff));

    if (*cp)
        printf("%s\n",cp);
}

// return TRUE if a valid path/file
// note: (windows) stat will FAIL on a path with a trailing '/',
// so the jiggery pokey stuff is to 'erase' a trailing '/', if it exists
// This is NOT a passed 'reference' so no harm is done to the passed path!
bool is_valid_path( std::string path )
{
    struct stat buf;
    size_t len = path.length();
    size_t pos = path.rfind("/");
    if ( pos == (len - 1) )
        path.erase( path.end() - 1 );
    if ( stat(path.c_str(),&buf) == 0 )
        return true;
    return false;
}

#if 0 // ==================================

#define ADD_WINDOW_DESTROY
extern int main_window, graphs_window;

void destroy_glut_windows(void)
{
#ifdef TRY_EXIT_GMAIN  // try using glutSetOption( GLUT_ACTION_ON_WINDOW_CLOSE
    glutSetOption( GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS  );
#endif
#ifdef ADD_WINDOW_DESTROY
    if (graphs_window)
        glutDestroyWindow(graphs_window);
    if (main_window)
        glutDestroyWindow(main_window);
#endif
}

void clear_abort_behaviour(void)
{
    unsigned int i1, i2;
    //terminate_handler oldHand = set_terminate(win_terminate);
    // Suppress the abort message - but does not seem to work!!!
    i1 = _set_abort_behavior( 0, _WRITE_ABORT_MSG );
    i2 = _set_abort_behavior( 0, _CALL_REPORTFAULT );
#ifdef EXTRA_DEBUG_WIN_UTILS
    printf( "_set_abort_behavior returned %d and %d\n", i1, i2 );
#endif
    i1 = _set_abort_behavior( 0, _WRITE_ABORT_MSG );
    i2 = _set_abort_behavior( 0, _CALL_REPORTFAULT );
    if( i1 || i2 ) {
        printf( "_set_abort_behavior still returned %d and %d\n", i1, i2 );
    }
}

int wait_for_key(void)
{
    int chr;
    int sleep_for = 55; // ms
    long new_beep = 0;
    long next_beep = 15000;
    //std::cin >> c;
    while( !check_key_available() ) {
        new_beep += sleep_for;
        if (new_beep > next_beep) {
            make_beep_sound();
            new_beep = 0;
        } else
            Sleep(sleep_for);  // sleep for 55 ms
    }
    // a keyboard character is waiting - get it
    chr = _getch();
    if (chr < ' ' )
        chr += '@';
    return chr;
}

// special trap
void win_exit( int ret )
{
#if (defined(_MSC_VER) && defined(_DEBUG) && defined(ADD_MEMORY_DEBUG))
    show_crt_dbg_mem();
#endif // _MSC_VER and _DEBUG
#ifndef NDEBUG
    int chr;
    make_beep_sound();
    printf("Exit through win_exit(%d)...any KEY to exit : ", ret);
    chr = wait_for_key();
    printf( "%c - ", chr );
#else
    clear_abort_behaviour();
    destroy_glut_windows();
#endif
    if(ret) {
        exit(ret);
    }
#ifdef TRY_EXIT_GMAIN
    else
        glutLeaveMainLoop();
#else
    else
        exit(ret);
    //abort();
    //terminate();
#endif
#ifdef EXTRA_DEBUG_WIN_UTILS
    printf("Returning to glutMainLoop...\n");
#endif

}

#endif // 0 some excluded things

/* -----------------------
DESCRIPTION
The remainder () function computes the remainder of dividing x by y.
The return value is x " - " n " * " y, where n is the value x " / " y,
rounded to the nearest integer. If this quotient is 1/2 (mod 1), it is
rounded to the nearest even number (independent of the current rounding 
mode). If the return value is 0, it has the sign of x. The drem () function
does precisely the same thing. 
or
The remainder() function returns the remainder r: 
x - n * y;
where n is the integer nearest the exact value of x/y; if |n - x/y| = 1/2,
then n is even. 
   ----------------------- */

float remainderf( float x, float y )
{
   int n = (int)(( x / y ) + 0.5);
   float result = x - ( n * y );
   return result;
}

char * basename( char * name )
{
   size_t len = strlen(name);
   size_t i, j;
   int c;
   j = 0;
   for( i = 0; i < len; i++ ) {
      c = name[i];
      if(( c == '/' )||
         ( c == '\\')||
         ( c == ':' ))
      {
         j = i + 1;
      }
   }
   return &name[j];
}

void set_win_path_sep( std::string & s )
{
   size_t fnd;
   while( (fnd = s.find('/')) != -1 )
      s.replace(fnd, 1, "\\");
}

#if 0 // excluded stuff

double er_equ = 6378137.0;   // earth radius, equator (6,378.1370 km?)
double er_pol = 6356752.314; // earth radius, polar   (6,356.7523 km?)
double get_erad( double lat )
{
	// case SANSON_FLAMSTEED:
	double a = cos(lat) / er_equ;
	double b = sin(lat) / er_pol;
	return (1.0 / sqrt( a * a + b * b ));
}

void ll2pt( double obj_latr, double obj_lonr,
           double cent_latr, double cent_lonr,
           double map_size,  double map_zoom,
           double * px_pixel, double * py_pixel )
{
	double x,y,r;
	r = get_erad(cent_latr); // case SANSON_FLAMSTEED:
   x = r * cos(obj_latr)*(obj_lonr - cent_lonr);
   y = r * (obj_latr - cent_latr);
	*px_pixel = (map_size / 2.0) + (x * map_zoom);
	*py_pixel = (map_size / 2.0) + (y * map_zoom);
}

void pt2ll( double * pobj_latr, double * pobj_lonr,
           double cent_latr, double cent_lonr,
           double map_size,  double map_zoom,
           double x_pixel, double y_pixel )
{
	double x,y,r;
	r = get_erad(cent_latr); // case SANSON_FLAMSTEED:
	x = (x_pixel - (map_size / 2.0)) / map_zoom;
	y = (y_pixel - (map_size / 2.0)) / map_zoom;
      *pobj_latr = (y / r) + cent_latr;
      *pobj_lonr = (x / (r * cos(*pobj_latr))) + cent_lonr; 
}

bool  check_map_executable( std::string exe )
{
   bool failed = true;  // assume it will
   FILE *f;
   std::ostringstream cmd;
   std::string str = "";
   char c;
   size_t n;

   set_win_path_sep(exe);
   cmd << exe << " --version";
   if ((f = _popen(cmd.str().c_str(), "r")) == NULL) {
   } else {
      while (true) {
         n = fread(&c, 1, 1, f);
         if (n == 0) {
            if (feof(f)) {
               fclose(f);
               // check for 'version' is output by map
               if(( str.find(": version: ") != -1 )||   // found new version
                  ( str.find("FlightGear mapping utility") != -1 )) // or even OLD version
                  failed = false;
               break;
            }
         }
         str.append(1, c);
      }
   }
   if(failed) {
      if( str.length() > 1 )
         printf("WARNING: Got output '%s'\n", str.c_str() );
      printf("WARNING: The exe '%s' doesn't exist.\nAny missing maps can not be generated!\n",
         exe.c_str() );
      printf("Continue regardless? Enter y to continue, else will exit: ");
      std::cin >> c;
      if(( c == 'y' )||( c == 'Y' )) {
         printf( " continuing ...\n" );
         failed = false;
      } else {
         printf( " aborting ...\n" );
         ATLAS_EXIT(1);
      }
   }
   return failed;
}

std::string get_compiler_version_string( void )
{
   string s = ATLAS_COMPILER_STR;
   s += "\n";
   s += "Compiled on ";
   s += __DATE__;
   s += " at ";
   s += __TIME__;
   char * env = getenv("COMPUTERNAME");
   if(env) {
      s += "\n";
      s += "In ";
      s += env;
      env = getenv("USERNAME");
      if(env) {
         s += " by ";
         s += env;
      }
   }
   return s;
}

// stat - from : http://msdn.microsoft.com/en-us/library/14h5k7ff(VS.71).aspx
int jpg_or_png_exists( const char * pf, int base )
{
    static char _s_jp_buf[MAX_PATH];
    char * cp = _s_jp_buf;
    struct stat buf;
    if(base) {
        strcpy(cp,pf);
        buf.st_mode = 0;
        if (( stat(cp,&buf) == 0 )&& (buf.st_mode & _S_IFREG) )
            return 3;
    }
    strcpy(cp,pf);
    strcat(cp,".jpg");
    buf.st_mode = 0;
    if (( stat(cp,&buf) == 0 )&& (buf.st_mode & _S_IFREG) )
        return 1;
    strcpy(cp,pf);
    strcat(cp,".png");
    buf.st_mode = 0;
    if (( stat(cp,&buf) == 0 )&& (buf.st_mode & _S_IFREG) )
        return 2;
    return 0;
}

#ifdef _DEBUG
// ================================================
// debug memory - only works in _DEBUG mode
//int newFlag = _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_DELAY_FREE_MEM_DF |
//    _CRTDBG_LEAK_CHECK_DF;
static int newFlag = _CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF |  _CRTDBG_LEAK_CHECK_DF;
static int oldFlag = 0;

void set_crt_out_file( _HFILE hf )
{
   // Send all reports to STDOUT
   _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_WARN, hf );
   _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_ERROR, hf );
   //_CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
   _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_ASSERT, hf );
}

void set_crt_dbg_mem(void)
{
    int tmpDbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    tmpDbgFlag |= newFlag;
    oldFlag = _CrtSetDbgFlag( tmpDbgFlag );
    set_crt_out_file( _CRTDBG_FILE_STDOUT );
}

HANDLE get_win_file_handle( char * fname )
{
    HANDLE hand = CreateFile(
        fname,                          // LPCTSTR lpFileName,
        GENERIC_READ|GENERIC_WRITE,     // DWORD dwDesiredAccess,
        0,                              // DWORD dwShareMode,
        NULL,                           // LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        CREATE_ALWAYS,                  // DWORD dwCreationDisposition,
        FILE_ATTRIBUTE_ARCHIVE,         // DWORD dwFlagsAndAttributes,
        NULL );                         // HANDLE hTemplateFile
    return hand;
}

void show_crt_dbg_mem(void)
{
    HANDLE hand;
    char * msg1 = "Dump of memory statistics...\n";
    char * msg2 = "Dump of memory objects...\n";
    _CrtMemState s1;
    _CrtMemCheckpoint( &s1 );
    hand = get_win_file_handle( "tempmemdbg.txt" );
    if ( hand && (hand != INVALID_HANDLE_VALUE) ) {
        DWORD dww;
        set_crt_out_file( hand );
        printf(msg1);
        WriteFile(hand, msg1, strlen(msg1), &dww, NULL);
        _CrtMemDumpStatistics( &s1 );
        printf(msg2);
        WriteFile(hand, msg2, strlen(msg2), &dww, NULL);
        _CrtMemDumpAllObjectsSince( NULL );
    } else {
        printf(msg1);
        _CrtMemDumpStatistics( &s1 );
        printf(msg2);
        _CrtMemDumpAllObjectsSince( NULL );
    }
}

#endif // _DEBUG
#endif // 0 exclude now

// eof - win_utils.cxx
