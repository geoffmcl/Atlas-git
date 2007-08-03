/*-------------------------------------------------------------------------
  TileManager.hxx

  Written by Brian Schack, started July 2007.

  Copyright (C) 2007 Brian Schack

  The tile manager keeps track of tiles scheduled to be updated, and
  manages their processing.  Adding a tile schedules it for an update,
  which includes rsync'ing the scenery directories, and creating hires
  and lowres maps via the Map program.

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

#ifndef __TILEMANAGER_H__
#define __TILEMANAGER_H__

#include "Tile.hxx"
#include "Preferences.hxx"

#include <string>
#include <list>

class TileManager {
public:
    TileManager(Preferences &prefs);

    void checkScenery();

    void addTile(Tile *t);
    void removeTile(Tile *t);
    int noOfTiles();
    Tile *nthTile(int n);
    Tile *tileAtLatLon(float lat, float lon);
    Tile *tileWithName(char *name);

    static void latLonToTileInfo(float latitude, float longitude,
				 char *name, char *dir,
				 float *centerLat, float *centerLon);
    static void nameToLatLon(char *name, float *latitude, float *longitude);

protected:
    // List of tiles that need to be processed.
    std::list<Tile *> _tiles;

    // Atlas preferences.
    Preferences _prefs;
};

#endif        // __TILEMANAGER_H__
