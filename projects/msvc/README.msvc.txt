# README.msvc.txt - 2010-04-24 - v0.4.6

Atlas - http://atlas.sourceforge.net/

Compiling Atlas Source in native Windows, using MSVC9 Express (2008).

All projects, Atlas, Map, and win_util.lib are compiled using the
Multi-threads (DLL) runtime, /MD (& /MDd), as are ALL the dependants.
Getting this wrong causes lots of hassles.

There is a general, hand crafted project/msvc/config.h file,
where you can adjust the version, and perhaps the default
location of the FlightGear data, set as C:\FG\32\data in
my case. See FGBASE_DIR, but this can be over-ridden on the
command line, so is not so important.

There are also some options macros in that file to explore.

==========================================================================

Directory Arrangement:
---------------------

1. Using the '3rdparty' system
==============================
In this build, _ALL_ the dependant includes and libraries
are included in the [projects\3rdparty] folder. No other
sources are required if these headers and libraries
are used.

Build Set: Atlas.dsw (Atlas.dsp, Map.dsp, GetMap.dsp win_ulib.dsp)

But if you wish to download, and build all the dependencies
yourself, then read on, else skip to Compiling:

2. External Sources
===================
Choose a folder for the Atlas source. I chose 'Atlas',
and I put most of the dependencies in the root of
Atlas. That is :-
<some root>\Atlas - Full source of Atlas/Map
           \simgear
           \PLIB
           \zlib-1.2.3
           \boost_1_42_0
           \lpng
           \etc...
although some are compiled in my C:\Projects folder,
such as :-
C:\Projects\jpeg-8a
           \glew-1.5.3 (optional)

Build Set: In Atlas-ext.zip - Atlas-ext.dsw -
(Atlas-ext.dsp, Map-ext.dsp, GetMap-ext.dsp)

Naturally, any difference in the Directory Structure must
be adjusted in the MSVC IDE <project> Property Pages.

==========================================================================

Dependencies:
------------

Essentially, Atlas is sort of part of the FlightGear project,
so once you have compiled FlightGear, you will have most of
the following dependencies already.

FlightGear - binary, and at least the base 'data'
site: http://www.flightgear.org/
I used the CVS version, but there are release versions available.

Simgear - Simulator Construction Tools 
site: http://www.simgear.org/
I used the CVS version, downloaded, and compiled as part of FlightGear

PLIB - STEVE'S PORTABLE GAME LIBRARY
site: http://plib.sourceforge.net/index.html
I used the CVS version, downloaded, and compiled as part of FlightGear

GLUT - freeglut, OpenSource alternative to the OpenGL Utility Toolkit (GLUT) library
site: http://freeglut.sourceforge.net/
I used the SVN version, downloaded, and compiled as part of FlightGear

zlib - A Massively Spiffy Yet Delicately Unobtrusive Compression Library
site: http://www.zlib.net/
I used Version 1.2.3 (July 2005), although I note there is now a 1.2.4.
Downloaded: 08/20/2009  19:26 496,597 zlib-1.2.3.tar.gz
and compiled as part of FlightGear

OpenSceneGraph - open source high performance 3D graphics toolkit
site: http://www.openscenegraph.org/projects/osg
I used a relatively recent SVN version, compiled as part of FlightGear

libpng - Portable Network Graphics
site: http://www.libpng.org/
I used version lpng1232.zip, but note there is now a 1.4.1 (lpng141.zip)
Compiled as part of building OSG for FlightGear

Boost - portable C++ source libraries
site: http://www.boost.org/
I alternate between using the SVN trunk, and version 1.39.0 and 1.42.0
SVN trunk failed, but should perhaps be fixed by now.
ONLY the boost headers are required - no compiling.

Jpeg - Independent JPEG Group (IJG)
site: http://www.ijg.org/
Download and compile: 04/16/2010 11:14 1,037,823 jpegsr8a.zip, or later
I compile the libraries as jpeg32.lib and jpeg32d.lib

OpenGL Extensions
-----------------

I added some switches to be able to compile it without these
extensions. Then maps are all generated in a window, instead of in a frame
buffer. Maybe a little slower, but just as effective.

Alternatives are: 
(a) GLew - The OpenGL Extension Wrangler Library
site: http://glew.sourceforge.net/
Download, unzip, and compile: 04/16/2010 13:25 642,576 glew-1.5.3.zip, or later...
Then include <GL/glew.h>, using a GLEW_STATIC macro, and link with glew32s.lib...
So I used this 'static' library, rather than the DLL approach of (b).

(a) OpenLibraries - key building blocks ... to build rich media applications
site: http://sourceforge.net/projects/openlibraries/
Download and install: 04/16/2010  12:49 6,214,295 openlibraries-0.4.0-sdk.exe, or later...
This adds two environment variable, like :-
OPENLIBRARIES_INCLUDE_PATH=C:\Program Files\OpenLibraries\include\openlibraries-0.4.0
OPENLIBRARIES_LIB_PATH=C:\Program Files\OpenLibraries\lib
Could not find the *.lib to link with... so gave up on this...

There may be other GL extension options, and perhaps some later windows systems
have these extensions already installed. Look for a head <GL/glext.h>. If this
is present in your system, then there is a good chance there are also DLLS
to provided these extension.

See [projects/msvc/config.h] for some option macros to control this.

==========================================================================

Compiling:
----------

The folder 'projects/msvc' contains two types of MSVC build files,
and various other needed items. For full compatibility, there
is an Atlas.dsw, which can be loaded, and converted by any version
of MSVC. This is the recommended set. It is better to run
the delsln.bat before loading and converting the dsw/dsp files,
to avoid MSVC questions.

Aternatively there are 'solution' files, Atlas.sln, specifically
for MSVC9 (2008).

Loading one of these build file sets into your MSVC, and build.

Naturally, if you have put external dependencies in different folders,
other than projects\3rdparty\include, and projects\3rdparty\lib,
used in this build file set, then these will need to be adjusted.

The <project> Property Pages -> C/C++ -> General -> Additional
Include Directories (AID) _MUST_ be set right. Likewise for the
Linker -> General -> Additional Library Directories (ALD) must match
the library list in -> Input -> Additional Dependencies (AD)! And
these can vary between Debug and Release builds.

And REMEMBER to _ALWAYS_ carefully check the RUNTIME being used,
both for Atlas, Map, and _ALL_ the dependant components. I chose
Multi-threads (DLL) runtime, /MD, and /MDd for Debug. As stated,
using inconsistent runtimes causes lots of PAIN ;=().

==========================================================================

Running:
--------

1: Map: This need to be run first, to generate the MAPS to be used
by Atlas. The 'default' location is <FG_ROOT>\Atlas. Run
map.exe --help to see the input options...

This will scan the <FG_ROOT>\Scenery, and build 'map' for the
San Francisco area, and any other areas you have downloaded
and installed. These will be output into a series of folders
call 10, 9, 8, 6, 4,... depending...

These are the maps to be loaded by Atlas when it is run.

For example, my Map command line - all one line -
Map.exe --fg-root=C:\FG\32\data --atlas=C:\FG\32\data\Atlas \
--palette=C:\FG\32\data\Atlas\Palettes\default.ap  --fg-scenery=C:\FG\32\data\scenery

Or a verbose form, NOT using the frame buffer extensions - render in window, as before...
Map.exe --verbose  --render-to-window --fg-root=C:\FG\32\data \
--atlas=C:\FG\32\data\Atlas --palette=C:\FG\32\data\Atlas\Palettes\default.ap \
--fg-scenery=C:\FG\32\data\scenery

This source includes a runmap.bat batch file, to give it a try.

2: Atlas: Usually some setup of needed data must be done, in
addition to the above map generation, before Atlas is run.
It needs to be able to load -
(a) backgound.jpg - to provided a global world image.
(b) Palettes\default.ap - to desinate the colors
(c) Fonts\Helvetica-Bold.100.txf and Helvetica.100.txf
(d) And the 'maps' in 10, 9, 8, etc.

Atlas.exe --fg-root=C:\FG\32\data --atlas=C:\FG\32\data\Atlas\temp2 --palette=C:\FG\32\data\Atlas\Palettes\default.ap --fg-scenery=C:\FG\32\data\Scenery
Atlas.exe --fg-root=C:\FG\32\data --fg-scenery=C:\FG\32\data\Scenery --atlas=temp6 --palette=temp6\Palettes\default.ap

This source includes a runatlas.bat batch file, as a work in
progress, to give it a try after you have generated the maps.

I maintain a site specifically on building FlightGear, and related
projects - see : http://geoffair.org/fg/

Have fun ;=))

Geoff.
2010-04-24

# EOF - README.msvc.txt
