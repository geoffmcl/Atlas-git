/*-------------------------------------------------------------------------
  Tile.hxx

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

#ifndef __TILE_H__
#define __TILE_H__

#include <ctime>
#include <sstream>
#include <map>

#include "Preferences.hxx"

// A Tile represents the scenery in one of the subdirectories of the
// scenery tree.  Generally this is an area with a north-south extent
// of 1 degree of latitude and an east-west extent of 1 degree of
// longitude, but at extreme latitudes (83 degrees or higher), one
// subdirectory can contain scenery for 1x2, 1x4, 1x8, or even 1x360
// degree areas.  
//
// The last case, 1x360 tiles, occurs from 89 degrees latitude to the
// poles.  It is particularly taxing for software: each "tile" is
// actually a donut, with the last one being a circle.  In addition,
// they have no center, since any longitude will serve equally well as
// a center.
class Tile {
public:
    enum TaskState {NOT_STARTED, 
		    CHECKING_OBJECTS, SYNCING_OBJECTS, 
		    CHECKING_TERRAIN, SYNCING_TERRAIN, 
		    MAPPING, 
		    FINISHED};
    enum Task {NO_TASK = 0,
	       SYNC_SCENERY = 1 << 0,
	       GENERATE_HIRES_MAP = 1 << 1,
	       GENERATE_LOWRES_MAP = 1 << 2};

    Tile(float latitude, float longitude, Preferences &prefs);
    Tile(char *name, Preferences &prefs);
    ~Tile();

    // The "standard" name of the tile (eg, "w128n37").  Always 7
    // characters long.
    const char *name();

    // Returns the latitude and longitude of the center of the tile.
    float lat();
    float lon();

    unsigned int hiresSize();
    unsigned int lowresSize();
    bool hasHiresMap();
    bool hasLowresMap();

    int toBeSyncedFiles();
    int toBeSyncedSize();
    int syncedFiles();
    int syncedSize();
    Tile::TaskState taskState();

    void setTasks(unsigned int t);
    unsigned int tasks();
    Tile::Task currentTask();
    void nextTask();

    Tile::Task doSomeWork();

protected:
    Preferences &_prefs;

    void _initTile();
    FILE *_startCommand(const char *command);
    bool _getRealLine(std::string& str);
    void _latLonToNames(float latitude, float longitude, 
			char *oneDegName, char *tenDegName);
    void _startChecking();
    bool _continueChecking();
    void _startSyncing();
    bool _continueSyncing();
    void _startMapping();
    bool _continueMapping();
    bool _pngSize(const char *file, unsigned int *width, unsigned int *height);

    // The tile's scenery directory (eg, "w132n37"), and its parent
    // scenery directory (eg, "w140n30").
    char _name[8];
    char _dir[8];

    // The latitude and longitude, in degrees, of the center of the
    // tile.  Southern latitudes and western longitudes are negative.
    float _lat, _lon;

    // If maps exist for this tile, these are set to their width.
    // Otherwise, they're set to 0.
    unsigned int _hiresSize, _lowresSize;

    // These keep track of the task as we execute it.  The first says
    // which tasks we have scheduled; the second indicates the
    // progress within the current task.
    unsigned int _tasks;
    TaskState _taskState;

    // These are used to indicate progress while synchronizing
    // scenery.  The "toBeSynced" variables are updated during each
    // "checking" phase, while the "synced" variables are updated
    // during each "syncing" phase.
    int _toBeSyncedFiles, _toBeSyncedSize;
    int _syncedFiles, _syncedSize;
    
    // While checking, if we find that any file is not up to date, we
    // set this to true.  If, at the end of checking, all files are up
    // to date, we skip syncing.
    bool _upToDate;

    FILE *_f;			// Pipe to rsync process.
    std::string _buf;		// Accumulates input from rsync.
    bool _eof;			// True if we've encountered EOF on
				// our currently open stream (ie,
				// we've read all available data).
    std::map<std::string, int> _files;	// Map of file names and their
					// sizes, as given by rsync.
};

#endif        // __TILE_H__
