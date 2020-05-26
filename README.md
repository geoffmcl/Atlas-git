# Atlas Project - fork

This is a **fork** of the hgcode src at https://sourceforge.net/p/atlas/hgcode/ci/default/tree/

This project is GPL'd.  For complete details on our licensing please see the "COPYING" file.

This fork uses CMake to generate the build file of your choice.

To use the Atlas program, you must have some of the scenery from FlightGear
(a freeware flight simulator) installed. These files can be found at the
FlightGear homepage: http://www.flightgear.org

You might need to help Atlas find the FlightGear base data directory
(often /usr/local/lib/FlightGear).  If your FlightGear base
directory is somewhere else, you can either set the FG_ROOT environment
variable, or use the --fgroot=path command-line option to specify the
FlightGear base directory.

To build under Unix/linux/mac, using the default `Unix Makefile`, follow these steps:-

	cd build    # avoid building in the root directory
    cmake .. -DCMAKE_BUILD_TYPE=Release [-DCMAKE_INSTALL_PREFIX:PATH=/usr]
    make
    [sudo] make install
    
To build under Windows, follow these steps:-
    
	cd build    # avoid building in the root directory
    cmake .. [-DCMAKE_INSTALL_PREFIX:PATH=D:\Projects\3rdParty.x64]
    cmake --build . --config Release
    
In both cases, Atlas depends of a considerable number of other, so called 3rdPary, headers, and libraries installed:-

    GLEW: http://glew.sourceforge.net/
    GLUT: http://freeglut.sourceforge.net/
    ZLIB: https://zlib.net/
    PNG: http://www.libpng.org/pub/png/libpng.html
    PLIB: https://github.com/www2000/libplib
    JPEG: https://sourceforge.net/projects/libjpeg/ 
    Boost: https://www.boost.org/users/download/ - headers only, no libs
    CURL: https://curl.haxx.se/dev/source.html - Optional, for GetMap

Now, for unix/linux/mac, these packages are usually available through distributions... But for Windows, it is quite a **TASK** to build, and install **all** these packages.

Thankfully most/all support a CMake build. The best plan is to:-

    1. Choose a new, empty directory, here called the `<ROOT>`;
    2. Download each source to a sub-directory, like `<ROOT>\glew-2.1.0`, `<ROOT>\FreeGLUT`, `<ROOT>\zlib-1.2.11`, etc;
    3. Build, and install each into `<ROOT>\3rdParty[.x64]`;
    4. Finally, in `<ROOT>\Atlas-git\build`, build atlas, setting `-DCMAKE_INSTALL_PREFIX=<ROOT>\3rdParty.x64` and/or `-DCMAKE_PREFIX_PATH=<ROOT>\3rdParty.x64`... good luck.

Actually sf.net/p/flightgear has a [windows-3rd-party][100] which contains **most** of the above dependencies, including `boost` and `curl`, but regretably is missing **GLEW**, and appears to have an ancient `GLUT`, which may not work.

  [100]: https://sourceforge.net/p/flightgear/windows-3rd-party/ci/master/tree/

#### Installing & Running Atlas

**Atlas** will provide a `moving` map, `tracking` your flight in **fgfs**, but some considerable setup is required.

As mentioned above, **Atlas** needs to receive UDP packets from **fgfs**, giving the current position of your aircraft. That can be achieved by running **fgfs** with `--atlas=socket,out,0.5,<host>,<port>,udp`, or `--nmea=<params>`, and running **Atlas** with `--udp=<port>`... 

And needs access to the same `fgdata`, specifically `Airports/apt.dat.gz`, `Navaids/nav.dat.gz, fix.dat.gz, awy.dat.gz`... That can be achieved by using the same environment variables as **fgfs**, namely `FG_ROOT=/path/to/fgdat`, or command line `--fg-root=/path/to/fgdata`, or compiled into the **Atlas** executable as `FGBASE_DIR=/path/to/fgdata`.

And needs access to the `same` scenery that **fgfs** is using, specifically the `<chunk>/<tile>/<index>.btg.gz, and <ICAO>.btg.gz` file, for the current location. In certain cases, that can be in the `<fg-root>\Scenery`, of per the environment varaiable `FG_SCENERY=/path/to/terrain`, or coomand `--fg-scenery=/path/to/terrain`

Finally, **Atlas** needs access to its `airplane_image.png`, `background.jpg` files, and its `Font`, and `Palette` directories. This location defaults to `<fg-root>/Atlas`,and the files in this repo, in `src/data` can be copied there. This is also the location where it will generated/rendered the map `jpg/png` files, in numbered `4-10` subdirectories. Or this can be set on the **Atlas** command line as `--atlas=/path/to/atlas/data`...

Have you got all that? Some help with the **Atlas** commands can be seen by running `atlas --help`, or after running **Atlas**, accessed by the `?` key. Not all easy...

Some of this information is documented in the FG Wiki - http://wiki.flightgear.org/Atlas - where it clearly states **"This article or section contains obsolete information. Please refer to Phi instead."**. And maybe - http://wiki.flightgear.org/Phi - better suits your needs. No, multiple, source builds - it is run in your browser; Simply run **fgfs** with `--httpd=8080`, then open your browser and enter `http://localhost:8080`... check it out...

Some also very outdated information can be from - http://atlas.sourceforge.net/index.php?page=run - and my own ancient efforts to support Atlas - http://geoffair.net/fg/atlas-07.htm - but look at the 2010 date. Will try to do something about that.

HTH - 20200526 - Geoff.
