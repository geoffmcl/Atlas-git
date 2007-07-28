/*-------------------------------------------------------------------------
  Tile.cxx

  Written by Brian Schack, started June 2007.

  Copyright (C) 2007 Brian Schack

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

#include <string>
#include <plib/sg.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <arpa/inet.h>

#include <simgear/misc/sg_path.hxx>

#include "Tile.hxx"
#include "TileManager.hxx"
#include "fg_mkdir.hxx"

// Latitude and longitude must be in degrees.
Tile::Tile(float latitude, float longitude, 
	   std::map<std::string, std::string> globalVars) :
    _globalVars(globalVars)
{
    TileManager::latLonToTileInfo(latitude, longitude, _name, _dir,
				  &_lat, &_lon);
    _initTile();
}

Tile::Tile(char *name, std::map<std::string, std::string> globalVars) :
    _globalVars(globalVars)
{
    float lat, lon;
    TileManager::nameToLatLon(name, &lat, &lon);
    TileManager::latLonToTileInfo(lat, lon, _name, _dir,
				  &_lat, &_lon);
    _initTile();
}

Tile::~Tile()
{
    // Close the file descriptor if it's still open.  In this class,
    // we always (well, always try to) set _f to NULL when we close
    // it.
    if (_f) {
	pclose(_f);
    }
}

unsigned int Tile::hiresSize()
{
    return _hiresSize;
}

unsigned int Tile::lowresSize()
{
    return _lowresSize;
}

bool Tile::hasHiresMap()
{
    return hiresSize() > 0;
}

bool Tile::hasLowresMap()
{
    return lowresSize() > 0;
}

int Tile::toBeSyncedFiles()
{
    return _toBeSyncedFiles;
}

int Tile::toBeSyncedSize()
{
    return _toBeSyncedSize;
}

int Tile::syncedFiles()
{
    return _syncedFiles;
}

int Tile::syncedSize()
{
    return _syncedSize;
}

Tile::TaskState Tile::taskState()
{
    return _taskState;
}

void Tile::setTasks(unsigned int t)
{
    _tasks = t;
}

unsigned int Tile::tasks()
{
    return _tasks;
}

Tile::Task Tile::currentTask()
{
    if (_tasks & SYNC_SCENERY) {
	return SYNC_SCENERY;
    } else if (_tasks & GENERATE_HIRES_MAP) {
	return GENERATE_HIRES_MAP;
    } else if (_tasks & GENERATE_LOWRES_MAP) {
	return GENERATE_LOWRES_MAP;
    } else {
	return NO_TASK;
    }
}

// Clears the current task, and sets _taskState to NOT_STARTED, in
// preparation for the next task.
void Tile::nextTask()
{
    if (_tasks & SYNC_SCENERY) {
	_tasks ^= SYNC_SCENERY;

	if (_syncedFiles == 0) {
	    // Be a bit clever.  If _syncedFiles = 0, that means there
	    // wasn't any data at all.  Don't bother generating any
	    // maps in that case.
	    _tasks = NO_TASK;
	} else {
	    // Be very clever, maybe too clever.  If every file was
	    // up-to-date, and we have maps of the correct size, don't
	    // regenerate them.  This isn't completely reliable, but
	    // it would be a strange situation indeed where this test
	    // doesn't work, and it saves a lot of calls to Map.
	    int s, t;
	    s = strtol(_globalVars["map_size"].c_str(), NULL, 10);
	    t = strtol(_globalVars["lowres_map_size"].c_str(), NULL, 10);
	    if (_upToDate && (s == _hiresSize) && (t == _lowresSize)) {
		_tasks = NO_TASK;
	    }
	}
    } else if (_tasks & GENERATE_HIRES_MAP) {
	_tasks ^= GENERATE_HIRES_MAP;
    } else if (_tasks & GENERATE_LOWRES_MAP) {
	_tasks ^= GENERATE_LOWRES_MAP;
    }

    _taskState = NOT_STARTED;
}

char* Tile::name()
{
    return _name;
}

float Tile::lat()
{
    return _lat;
}

float Tile::lon()
{
    return _lon;
}

int setNonBlocking(FILE *f)
{
    int fd;
    int flags;

    fd = fileno(f);

    if ((flags = fcntl(fd, F_GETFL, 0)) == -1) {
        flags = 0;
    }

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Starts the given command using popen, and sets the file descriptor
// to non-blocking.
FILE* Tile::_startCommand(const char *command)
{
    FILE *f;

    if ((f = popen(command, "r")) == NULL) {
	_eof = true;
	return NULL;
    }
    if (setNonBlocking(f) == -1) {
	_eof = true;
	pclose(f);
	return NULL;
    }
    
    _buf.clear();
    _eof = false;

    return f;
}

// Try to get a single line from our pipe, _f.  Return true if we've
// managed to read a line.  A line is defined as everything up to the
// next '\n', or EOF if there's no '\n'.  If there's no '\n' or EOF,
// then return false.
bool Tile::_getRealLine(std::string& str)
{
    char c;
    size_t n;

    if (_eof) {
	return false;
    }

    // Read until: (a) we get to the end of the file, (b) there's no
    // input (temporarily), (c) we get to the end of the line.
    while (true) {
	// Perhaps it's inefficient to read one character at a time,
	// but the logic is simpler.
	n = fread(&c, 1, 1, _f);
	if (n == 0) {
	    if (feof(_f)) {
		// (a) No input because of EOF.  Set _eof, and return
		// what we've got.
		_eof = true;
		pclose(_f);
		_f = NULL;

		str = _buf;
		_buf.clear();
		return true;
	    } else {
		// (b) Temporarily no input.  Return false.
		return false;
	    }
	}

	if (c == '\n') {
	    // (c) End of line.  Return what we've accumulated.
	    str = _buf;
	    _buf.clear();
	    return true;
	}

	// End of nothing.  Append the character and go back for
	// another.
	_buf.append(1, c);
    }
}

// This is the routine that does all the work.  It synchronizes the
// scenery for the tile with the server, and creates a map.  Because
// the work is being done asynchronously, it will probably need to be
// called several times.
//
// For the caller, the procedure is pretty simple - just keep calling
// doSomeWork() until it returns NO_TASK.
//
// During each call, doSomeWork() will, well, do some work.  It
// monitors the current activity.  If that activity is finished, it
// will start the next one.  The activities, in order, are: check
// objects, sync objects, check terrain, sync terrain, create map.
//
// This routine updates a set of variables that can be checked to
// measure its progress: _taskState, _toBeSyncedFiles (the number of
// files that need to be synced), _toBeSyncedSize (the number of bytes
// that need to be possibly downloaded), _syncedFiles (the number of
// files that have been synced), _syncedSize (the number of bytes that
// have been downloaded), _upToDate (true if all the files, both in
// objects and terrain, are up to date).
//
// Note that due to the way rsync optimizes downloading, it may in
// reality download far less than the number of files and bytes
// indicated by _toBeSyncedFiles and _toBeSyncedSize.  However,
// _syncedFiles and _syncedSize are updated *as if* the files were
// actually downloaded.
Tile::Task Tile::doSomeWork()
{
    switch (currentTask()) {
    case SYNC_SCENERY:
	switch (_taskState) {
	case NOT_STARTED:
	    // We're right at the beginning of a scenery sync.
	    _taskState = CHECKING_OBJECTS;
	    _startChecking();
	    break;
	case CHECKING_OBJECTS:
	case CHECKING_TERRAIN:
	    if (!_continueChecking()) {
		// We've finished checking.  Usually we go on to
		// actually sync the files.  However, if the checking
		// reveals that there are no files, we skip on to the
		// next step (either checking terrain, or finishing).
		if (_toBeSyncedFiles > 0) {
		    // There are files, so sync them.
		    if (_taskState == CHECKING_OBJECTS) {
			_taskState = SYNCING_OBJECTS;
			
			// Before we start our syncing process, assume
			// everything is up to date.  If we find an
			// out-of-date file, this will be set to false
			// (in _continueSyncing()).
			_upToDate = true;
		    } else {
			_taskState = SYNCING_TERRAIN;
		    }
		    _startSyncing();
		} else if (_taskState == CHECKING_OBJECTS) {
		    // No files to be synced, and we just finished
		    // checking objects.  Schedule terrain to be
		    // checked.
		    _taskState = CHECKING_TERRAIN;
		    _startChecking();
		} else {
		    // No files to be synced, and we just finished
		    // checking terrains.  We're done.
		    _taskState = FINISHED;
		}
	    }
	    break;
	case SYNCING_OBJECTS:
	case SYNCING_TERRAIN:
	    if (!_continueSyncing()) {
		// We've finished syncing.  Either we need to start
		// checking terrain, or we're done with syncing
		// scenery.
		if (_taskState == SYNCING_OBJECTS) {
		    _taskState = CHECKING_TERRAIN;
		    _startChecking();
		} else {
		    // We're all done.
		    _taskState = FINISHED;
		}
	    }
	    break;
	}
	break;
    case GENERATE_HIRES_MAP:
    case GENERATE_LOWRES_MAP:
	switch (_taskState) {
	case NOT_STARTED:
	    _taskState = MAPPING;
	    _startMapping();
	case MAPPING:
	    if (!_continueMapping()) {
		_taskState = FINISHED;
	    }
	    break;
	}
	break;
    }

    // If we've finished the current task, move on to the next one.
    if (_taskState == FINISHED) {
	nextTask();
    }

    return currentTask();
}

// Starts a "checking" rsync process, for either objects or terrain.
void Tile::_startChecking()
{
    std::ostringstream cmd;
    SGPath p("Scenery");

    _toBeSyncedFiles = 0;
    _toBeSyncedSize = 0;
    _files.clear();

    if (_taskState == CHECKING_OBJECTS) { 
	p.append("Objects");
    } else if (_taskState == CHECKING_TERRAIN) {
	p.append("Terrain");
    } else {
	// Really shouldn't get here.
	assert(false);
    }
    p.append(_dir);
    p.append(_name);

    // Only check - don't specify a destination.
    // EYE - will Windows blow up on this?
    cmd << "rsync -v -a " << _globalVars["rsync_server"] << "::" 
	<< p.str() << " 2> /dev/null";

    if ((_f = _startCommand(cmd.str().c_str())) == NULL) {
	// EYE - reset a few more state variables before returning?
	// EYE - it would be nice to have a way to report errors.
	fprintf(stderr, "startCommand error on '%s', bailing\n", 
		cmd.str().c_str());
	_taskState = FINISHED;
	return;
    }
}

// Continues an already-started "checking" rsync process, for either
// objects or terrain.  Returns false if the process has finished.
bool Tile::_continueChecking()
{
    std::string str;
    char *fileName;
    int x;

    // Try to parse what we have.  Matching is the same for objects
    // and terrain.  We do it on a line-by-line basis.  If we don't
    // match a line, we ignore it and continue.
    while (_getRealLine(str)) {
//     if (_getRealLine(str)) {
	// Match lines like this:
	// -rw-rw-r--        4260 2006/01/09 04:01:05 w120n37/5CL0.btg.gz
	// But not this:
	// drwxrwxr-x        4096 2006/01/10 04:33:58 w120n37
	const char *cStr = str.c_str();
	fileName = (char *)calloc(strlen(cStr) + 1, sizeof(char));
	if (sscanf(cStr, "-%*s %d %*10s %*8s %*1c%*3d%*1c%*2d/%s", &x, 
		   fileName) == 2) {
	    _toBeSyncedFiles++;
	    _toBeSyncedSize += x;
	    _files[fileName] = x;
	}
	free(fileName);
    }
	    
    // At this point, we've read everything we have to read, but may
    // or may not be at EOF.  If we are, that means we're ready to
    // move on to the next phase.
    return !_eof;
}

// Starts a "syncing" rsync process, for either objects or terrain.
void Tile::_startSyncing()
{
    std::ostringstream cmd;
    std::string source("Scenery");
    SGPath dest(_globalVars["scenery_root"]);
    int err;

    _syncedFiles = 0;
    _syncedSize = 0;

    // Note: I assume that rsync servers all "speak Unix", and that,
    // for example, they expect directories to be separated by forward
    // slashes.  So, I construct the source path directly, rather than
    // using SGPath.  The destination, though, is local, so we use
    // SGPath for that.
    if (_taskState == SYNCING_OBJECTS) { 
	source = source + "/Objects/";
	dest.append("Objects");
    } else if (_taskState == SYNCING_TERRAIN) {
	source = source + "/Terrain/";
	dest.append("Terrain");
    } else {
	// Really shouldn't get here.
	assert(true);
    }
    source = source + _dir + "/" + _name;
    dest.append(_dir);

    // First, make sure there's a parent directory into which we can
    // put things.
    fg_mkdir(dest.c_str());

    // Now start the actual rsync.  We ask for double verbosity (-v
    // -v) because we want it to tell us about all files being
    // considered, whether or not they are actually downloaded.
    cmd.clear();
    cmd.str("");
    cmd << "rsync -v -v -a --delete " << _globalVars["rsync_server"] << "::" 
	<< source << " " << dest.str();

    if ((_f = _startCommand(cmd.str().c_str())) == NULL) {
	// EYE - reset a few more state variables before returning.
	fprintf(stderr, "startCommand error on '%s', bailing\n", 
		cmd.str().c_str());
	_taskState = FINISHED;
	return;
    }
}

// Continues an already-started "syncing" rsync process, for either
// objects or terrain.  Returns false if the process has finished.
bool Tile::_continueSyncing()
{
    std::string str;
    char *fileName;

    // If there are no files to be synced, just skip it.
    if (_toBeSyncedFiles == 0) {
	return false;
    }

    // First try to parse what we have.  Matching is the same for
    // objects and terrain.  We do it on a line-by-line basis.  If we
    // don't match a line, we ignore it and continue.
    while (_getRealLine(str)) {
//     if (_getRealLine(str)) {
	// The feedback we get from rsync depends on whether files
	// are being updated or not.  If a file is updated, we get:
	//   e006n43/3055936.btg.gz
	// But if it's up-to-date, we get:
	//   e006n43/3055936.btg.gz is uptodate
	const char *cStr = str.c_str();
	fileName = (char *)calloc(strlen(cStr) + 1, sizeof(char));
	if (sscanf(cStr, "%*1c%*3d%*1c%*2d/%s", fileName) == 1) {
	    _syncedFiles++;
	    _syncedSize += _files[fileName];

	    // Check if the file is up to date.
	    if (strstr(str.c_str(), "uptodate") == NULL) {
		_upToDate = false;
	    }
	}
	free(fileName);
    }

    return !_eof;
}

// Starts a Map process.
void Tile::_startMapping()
{
    std::ostringstream cmd;

    // EYE - when Map is called, Atlas moves to the background (since
    // Map is a windowed app, although without a window?).  It would
    // be nice if Map had a truly non-windowed version.

    // Notes on Map call:
    // - lat,lon is *center* of map
    // - autoscale is required
    // - we write to a temporary file (just the name without '.png'),
    //   so that Atlas doesn't try to read an incomplete png file.
    //   After the file has been downloaded in its entirety and is
    //   safe for Atlas to read, we rename it.
    if (currentTask() == GENERATE_HIRES_MAP) {
	cmd << _globalVars["map_executable"] 
	    << " --fg-root=" << _globalVars["fg_root"] 
	    << " --fg-scenery=" << _globalVars["scenery_root"]
	    << " --lat=" << _lat
	    << " --lon=" << _lon
	    << " --output=" << _globalVars["atlas_root"] << _name
	    << " --size=" << _globalVars["map_size"]
	    << " --headless --autoscale 2> /dev/null";
	// EYE - I pipe stderr into /dev/null because I get messages
	// like this if I try to run more than one instance of Map:
	// 2007-07-10 17:11:00.547 Map[18200] CFLog (0): CFMessagePort: bootstrap_register(): failed 1103 (0x44f), port = 0x3203, name = 'Map.ServiceProvider'
	// See /usr/include/servers/bootstrap_defs.h for the error codes.
	// 2007-07-10 17:11:00.547 Map[18200] CFLog (99): CFMessagePortCreateLocal(): failed to name Mach port (Map.ServiceProvider)
    } else {
	cmd << _globalVars["map_executable"] 
	    << " --fg-root=" << _globalVars["fg_root"] 
	    << " --fg-scenery=" << _globalVars["scenery_root"]
	    << " --lat=" << _lat
	    << " --lon=" << _lon
	    << " --output=" << _globalVars["atlas_root"] << "lowres/" << _name
	    << " --size=" << _globalVars["lowres_map_size"]
	    << " --headless --autoscale 2> /dev/null";
    }
    if ((_f = _startCommand(cmd.str().c_str())) == NULL) {
	// EYE - need to do more cleanup.
	// EYE - try parsing command output to detect some errors (eg,
	// "tcsh: Map: Command not found.")
	fprintf(stderr, "Map error: bailing\n");
	_taskState = FINISHED;
	return;
    }
}

// Continues an already-started Map process.  Returns false if the
// process has finished.
bool Tile::_continueMapping()
{
    std::string str;

    // We won't try to parse the output, which really isn't very
    // useful anyway.
    while (_getRealLine(str)) {
//     if (_getRealLine(str)) {
    }

    if (_eof) {
	// Done!  Move temporary file to its final resting place.
	SGPath source, dest;
	std::string cmd;
	int err;

	source.set(_globalVars["atlas_root"]);
	if (currentTask() == GENERATE_LOWRES_MAP) {
	    source.append("lowres");
	}
	source.append(_name);

	dest = source;
	dest.concat(".png");

	cmd = "mv " + source.str() + " " + dest.str();
	if (err = system(cmd.c_str())) {
	    // EYE - need to do more cleanup?
	    fprintf(stderr, "mv error: %d, bailing\n", err);
	    _taskState = FINISHED;
	    return false;
	}
    }

    return !_eof;
}

// Sets all the internal variables to default values.  Checks the
// sizes of the hires and lowres maps (if they exist).  If they don't,
// it sets their sizes to 0.
void Tile::_initTile()
{
    SGPath map;
    unsigned int width, height;

    _toBeSyncedFiles = _toBeSyncedSize = 0;
    _syncedFiles = _syncedSize = 0;
    _taskState = NOT_STARTED;
    _f = NULL;

    // Check the hires map.
    map.set(_globalVars["atlas_root"]);
    map.append(_name);
    map.concat(".png");
    if (map.exists() && _pngSize(map.c_str(), &width, &height)) {
	// We assume square tiles.
	_hiresSize = width;
    } else {
	_hiresSize = 0;
    }

    // Now lowres.
    map.set(_globalVars["atlas_root"]);
    map.append("lowres");
    map.append(_name);
    map.concat(".png");
    if (map.exists() && _pngSize(map.c_str(), &width, &height)) {
	// We assume square tiles.
	_lowresSize = width;
    } else {
	_lowresSize = 0;
    }
}

// Returns the width and height of the image in the given file, which
// must be a PNG file.  Returns true if the file is a valid PNG file,
// false otherwise.
bool Tile::_pngSize(const char *file, unsigned int *width, unsigned int *height)
{
    int f;
    char buf[24];
    uint32_t n;

    if (!(f = open(file, O_RDONLY, 0))) {
	return false;
    }

    // We're only interested in the first 24 bytes.
    if (read(f, buf, 24) < 24) {
	close(f);
	return false;
    }
    close(f);

    // Check the PNG signature.
    if (strncmp(buf, "\211\120\116\107\015\012\032\012", 8) != 0) {
	return false;
    }

    // Make sure the first chunk is the IHDR (bytes 8 to 11 are a
    // length, by the way).
    if (strncmp(buf + 12, "IHDR", 4) != 0) {
	return false;
    }

    // Extract the width and height.  Width and height are stored in
    // the file in network byte order.
    n = *(uint32_t *)(buf + 16);
    *width = ntohl(n);

    n = *(uint32_t *)(buf + 20);
    *height = ntohl(n);

    return true;
}
