/*-------------------------------------------------------------------------
  AirportsOverlay.hxx

  Written by Brian Schack

  Copyright (C) 2008 Brian Schack

  The airports overlay manages the loading and drawing of airports.

  This file is part of Atlas.

  Atlas is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Atlas is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU General Public License
  along with Atlas.  If not, see <http://www.gnu.org/licenses/>.
  ---------------------------------------------------------------------------*/

#ifndef _AIRPORTS_OVERLAY_H
#define _AIRPORTS_OVERLAY_H

#include <list>
#include <vector>
#include <string>
#include <map>
#include <set>

#include <zlib.h>

#include <plib/sg.h>

#include "Overlays.hxx"
#include "Culler.hxx"
#include "Searcher.hxx"
#include "Notifications.hxx"

struct RWY {
    std::string id;
    double lat, lon;
    float hdg;
    float length, width;

    atlasSphere bounds;
    sgdVec3 ahead, aside, above;
};

enum ATCCodeType {WEATHER = 50, UNICOM, DEL, GND, TWR, APP, DEP};

// A mapping from strings (like "ATLANTA APP") to a set of frequencies
// associated with them.
typedef std::map<std::string, std::set<int> > FrequencyMap;

struct ARP: public Searchable, Cullable {
  public:
    // Searchable interface.
    const double *location() const { return bounds.center; }
    virtual double distanceSquared(const sgdVec3 from) const;
    virtual const std::vector<std::string>& tokens();
    virtual const std::string& asString();

    // Cullable interface.
    // EYE - change Bounds() to something better
    const atlasSphere& Bounds() { return bounds; }
    double latitude() { return lat; }
    double longitude() { return lon; }

    std::string name, id;
    double lat, lon;
    float elev;
    bool controlled;
    bool lighting;		// True if any runway has any kind of
				// runway lighting.
    bool beacon;
    // EYE - change this lat, lon stuff to a structure.  Use SGGeod?
    double beaconLat, beaconLon;
    std::vector<RWY *> rwys;
    std::map<ATCCodeType, FrequencyMap> freqs;

    atlasSphere bounds;

  protected:
    std::vector<std::string> _tokens;
    std::string _str;
};

// Determines how airports are drawn.
// EYE - add colours, runway label stuff, fonts?
struct AirportPolicy {
    // Maximum radius of outer circle (pixels).
    unsigned int rO;

    // Maximum radius of inner circle (pixels).  This is the area of
    // the circle occupied by the airport.  If the airport is larger
    // than this, the outer circle is drawn as an outline instead
    // (with the width of the border being rO - rI pixels).  If the
    // airport is smaller than this, then we start scaling the circle
    // so that the outer circle is always rO / rI times bigger than
    // the airport.  In other words, the airport will occupy the same
    // proportion of the circle.
    unsigned int rI;

    // The mimimum size of the outer circle (pixels).  The outer
    // circle will never be shrunk below this value.  A value of 0
    // means nothing is drawn if it would be too small.  The airport
    // will continue to shrink however - we always draw the airport to
    // scale.
    unsigned int rMin;

    // Minimum airport radius (pixels).  If an airport is less than
    // rAMin, we don't draw its runways.
    unsigned int rAMin;

    // Direction from center of airport to airport label (degrees,
    // where north = 0.0, east = 90.0, ...)
    float labelHeading;

    // Maximum distance from airport center to label (pixels).  Labels
    // will be placed outside of the airport's outer circle, until
    // this limit is hit.  If 0, labels will always be placed outside
    // of the airport's outer circle.
    unsigned int maxLabelDist;
};

class Overlays;
class AirportsOverlay: public Subscriber {
  public:
    AirportsOverlay(Overlays& overlays);
    ~AirportsOverlay();

    bool load(const std::string& fgDir);

    void setDirty();

    void drawBackgrounds();
    void drawForegrounds();
    void drawLabels();		// Runways and airports

    // EYE - Airports::Policy instead?
    void setPolicy(const AirportPolicy& p);
    AirportPolicy policy();

    // Subscriber interface.
    bool notification(Notification::type n);

  protected:
    Culler *_culler;
    Culler::FrustumSearch *_frustum;
    double _metresPerPixel;

    void _createBeacon();
    void _createAirportIcon();

    bool _load810(const gzFile& arp);

    void _drawIcon(ARP *ap, float radius);

    void _labelAirport(ARP *ap, int rA);
    void _labelRunway(RWY *rwy);
    void _labelRunwayEnd(const char *str, float hdg, RWY *rwy);

    Overlays& _overlays;

    std::vector<ARP *> _airports;

    GLuint _backgroundsDisplayList, _runwaysDisplayList, _labelsDisplayList;
    GLuint _beaconDL, _airportIconDL;
    bool _FGDirty, _BGDirty, _labelsDirty;

    AirportPolicy _policy;
};

#endif