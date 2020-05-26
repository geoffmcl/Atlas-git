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

Thankfully most/all support a CMake build. The best plan is to `<1>` choose a new, empty directory, here called the `<ROOT>`; `<2>` Download each source to a sub-directory, like `<ROOT>\glew-2.1.0`, `<ROOT>\FreeGLUT`, `<ROOT>\zlib-1.2.11`, etc; `<3>` Build, and install each into `<ROOT>\3rdParty[.x64]`; `<4>` Finally, in `<ROOT>\Atlas-git\build`, build atlas, setting `-DCMAKE_INSTALL_PREFIX=<ROOT>\3rdParty.x64` and/or `-DCMAKE_PREFIX_PATH=<ROOT>\3rdParty.x64`... good luck.

Actually sf.net/p/flightgear has a [windows-3rd-party][100] which contains **most** of the above dependency, including `boost` and `curl`, but regretably is missing **GLEW**.


  [100]: https://sourceforge.net/p/flightgear/windows-3rd-party/ci/master/tree/

