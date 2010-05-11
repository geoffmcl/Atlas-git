# README.map.txt

2010-05-08 - 2010-04-17: Some notes...

1. Finding the SCENERY
----------------------

If you do NOT have FG_ROOT nor FG_SCENERY set, and
you do NOT give a --fg-scenery=<DIR>, but do give
--fg-root=<DIR>, map will NEVER find the scenery!

It tries using the fg-root, but ONLY adds 'Terrain'!
It should add 'Scenery' first...

If FG_ROOT and/or FG_SCENERY is in the environment, then the code does
    char *env = getenv("FG_ROOT");
    if (env == NULL) {
	fg_root.set(FGBASE_DIR);
    } else {
	fg_root.set(env);
    }

    env = getenv("FG_SCENERY");
    if (env == NULL) {
	fg_scenery.set(fg_root.str());
    } else {
	fg_scenery.set(env);
    }

So it seems
        fg_scenery.set(fg_root.str());
should be
        fg_scenery.set(fg_root.str());
        fg_scenery.append("Scenery");

as it does when these are NOT set in the environment...
	// Default: $FG_ROOT/Scenery
	scenery.set(fg_root.str());
	scenery.append("Scenery");
        
2. Creating the MAPS
--------------------

If you create once, then Map detects the files exist,
and does NOTHING. That is good...

If you then delete one folder, say the '10', then
Map will FAIL to re-create 10, thus FAIL to write
the Maps...

Maybe this is something to do with the SGPath class
NOT correctly reporting 'directories'.exist(). It uses
open( "dir", "r" ) succeeds, but in windows open a
directory will ALWAYS fail.

See patch below...

3. SGPath problems in windows
-----------------------------

As suggested above, SGPath.exists() FAILS if the
path is a directory.

So the code in Tiles.cxx of -
   if (!_maps.exists() && createDirs) {
      if (_maps.create_dir(0755) < 0) {
        // If we can't create it, throw an error.
        throw runtime_error("couldn't create maps directory");
	  }
   }
would ALWAYS be run, but thankfully SGPath.create() returns
0, and not an error, if the path already exists...

I added a NEW, windows only at present, service,
bool is_valid_path( std::string path );
where I used 'stat', instead of 'open'.

But this needed some checking if a trailing '/'
had been added, since 'stat' will FAIL if there
is a trailing '/'.

Have added a patch to use a win_ulib service
#ifdef _MSC_VER
    if (!is_valid_path(_maps.str()) && createDirs)
#else
    if (!_maps.exists() && createDirs)
#endif

Similarly with adding the trailing '/'
#ifdef _MSC_VER
    size_t len = _maps.str().length();
    if (( len != 0 ) && (_maps.str().rfind("/") != (len - 1)))
        _maps.append("");
#else
    if (!_maps.file().empty()) {
	_maps.append("");
    }
#endif

Note that SGPath unconditionally, and ALWAYS uses
a '/' path separator, but in MOST cases this is NOT a problem
in windows. Some, but NOT all commands/functions accept either.

GL Extensions
-------------

Although windows comes with <GL/gl.h> installed, together with
the supporting DLLS, OPENGL32.DLL, and GLU32.DLL, the 'extended'
functions are NOT exposed through the 'export' header, but
many are none the less supported.

SimGear has a header <simgear/screen/extensions.hxx> where these
are emumerated, but this requires some further handling...
(a) The functions are declared as a pointer
(b) An 'init' must be called to set these pointers,
(c) Then the functions can be used through these pointers.

I have added a new file to the win_ulib - sg_ext_funcs.(cxx|hxx)
to handle all this through a class sg_ext_funcs, where the pointers
are initialized during instantiation.

Then in Map, I have added MapEXP.cxx, and MapEXP.hxx to do the
extened intialization, and setup, if --render-offscreen is
given. In windows I also thus reversed the default - thus Map
will render in a window (--render-in_window) by default,
else will try to setup the extended functions.

This appears to be working fine.

My Debug Command:
--verbose --fg-root=C:\FG\33\data --fg-scenery=C:\FG\33\data\Scenery --atlas=temp1 --palette=temp1\Palettes\default.ap
Also adding --no-lighting|--lighting (def), --render-offscreen|--render-to-window (win32 def)

# EOF - REDME.map.txt

