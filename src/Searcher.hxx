/*-------------------------------------------------------------------------
  Searcher.hxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

  A Searchable is anything that can be added to and used by a
  Searcher.  It is capable of representing itself as a
  nicely-formatted string, breaking itself down into searchable
  tokens, and returning its location.  

  The Searchable is purely virtual, with no data - it is intended to
  be used much like an Objective-C protocol, specifying a pure
  interface, leaving the implementation up to the inheritor.

  A Searcher does the actual searching.  You add Searchables to it,
  then call findMatches() to do the search.  The search can be done in
  steps - each call to findMatches() will do a little bit more work.
  The search results are accessed via the noOfMatches() and getMatch()
  methods.

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

#ifndef _SEARCHER_H_
#define _SEARCHER_H_

#include <string>
#include <vector>
#include <set>
#include <map>

#include <plib/sg.h>

// A Searchable can return its tokens (used for searching), and a
// nicely formatted string (for use in a user interface), its
// location, and its distance to another location (this is used to
// sort search results).
class Searchable {
  public:
    virtual ~Searchable() {}

    static void tokenize(const std::string& str, 
			 std::vector<std::string>& tokens);

    // All of our tokens.
    virtual const std::vector<std::string>& tokens() = 0;
    // A nicely printed representation of ourselves.
    virtual const std::string& asString() = 0;

    // Cartesian coordinate (sgdVec3).
    virtual const double *location() const = 0;
    // Distance to our location from 'from' (squared, to make it
    // faster to compute).
    virtual double distanceSquared(const sgdVec3 from) const = 0;
};

// This is the comparator we use for the multimap of tokens.  It does
// a caseless comparison.
class CaseFreeLessThan {
  public:
    bool operator()(const std::string& left, const std::string& right) {
	return (strcasecmp(left.c_str(), right.c_str()) < 0);
    }
};

// This is the comparator we use for the set of matches generated by
// the Searcher object.  It sorts results based on their distance from
// a given point.
class SearchableLessThan {
  public:
    SearchableLessThan() { sgdZeroVec3(_centre); }
    bool operator()(const Searchable *left, const Searchable *right) const {
	return (left->distanceSquared(_centre) < 
		right->distanceSquared(_centre));
    }
    void setCentre(const sgdVec3 c) { sgdCopyVec3(_centre, c); }
    const double *centre() { return _centre; }
  protected:
    sgdVec3 _centre;
};

// A Searcher object holds a bunch of Searchables, and implements a
// search interface.
class Searcher {
  public:
    Searcher();
    ~Searcher();

    // Adds the given searchable to our data structures.
    void add(Searchable *s);
    // EYE - we should also create a remove() method.

    // Finds matches for the given string, returning true if any
    // (more) are found.  If maxMatches is not -1, it will limit
    // itself to that many matches.  This allows it to be called
    // multiple times by a user interface to steadily accumulate a
    // full set of matches.  Matches are sorted by distance from
    // centre.  Searches are case-insensitive.
    bool findMatches(const std::string& str, const sgdVec3 centre, 
		     int maxMatches = -1);

    // Accessor functions for the matches.
    unsigned int noOfMatches() { return _matches->size(); }
    Searchable *getMatch(unsigned int i);

  protected:
    // Checks if the given searchable completely matches the given
    // tokens.
    bool _match(Searchable *s, 
		const std::vector<std::string>& completeSearchTokens,
		const std::string& partialSearchToken);

    // All our searchable objects.
    std::vector<Searchable *> _searchables;

    // All tokens from all searchables, paired with the searchables
    // they come from.  These are sorted alphabetically, disregarding
    // case.
    std::multimap<std::string, Searchable *, CaseFreeLessThan> _tokens;

    // Accumulated matches from findMatches(), sorted by distance
    // (nearest first) from the 'centre' parameter to findMatches().
    std::set<Searchable *, SearchableLessThan> *_matches;
    // Our current comparator for _matches.
    SearchableLessThan _comparator;
};

#endif // _SEARCHER_H_
