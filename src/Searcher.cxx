/*-------------------------------------------------------------------------
  Searcher.cxx

  Written by Brian Schack

  Copyright (C) 2009 Brian Schack

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Searcher.hxx"

#include <sstream>

using namespace std;

// This is a function that may be useful to searchables.  It just
// chops up the given string into 'words', where a word is any
// whitespace-delimited string.
void Searchable::tokenize(const string& str, vector<string>& tokens)
{
    istringstream stream(str);
    string aToken;
    stream >> aToken;
    while (stream) {
	tokens.push_back(aToken);
	stream >> aToken;
    }
}

Searcher::Searcher(): _lastSearchString("")
{
    // Create a default value _matches.  This will be thrown out
    // immediately, since by default our comparator uses an impossible
    // value for its centre, but they make the program logic in
    // newMatches() simpler.
    _matches = new set<Searchable *, SearchableLessThan>(_comparator);
}

Searcher::~Searcher()
{
    for (unsigned int i = 0; i < _searchables.size(); i++) {
	delete _searchables[i];
    }

    delete _matches;
}

// Adds a searchable to our _searchable vector and its tokens to our
// _tokens vector.
void Searcher::add(Searchable *s)
{
    _searchables.push_back(s);

    const vector<string>& tokens = s->tokens();
    for (unsigned int i = 0; i < tokens.size(); i++) {
	_tokens.insert(make_pair(tokens[i], s));
    }
}

// Starts (if str is new) or continues (if str is the same as the
// previous call) a search in the _searchables vector (which is
// assumed to be sorted) for str.  The results are placed in the
// _matches vector, sorted by their distance from the given centre of
// interest.  Returns true if the _matches vector changed (including
// because the centre of interest changed).  Finds at most maxMatches
// results (this was added so that a GUI-based application can get a
// reasonable response for a potentially large search).  If maxMatches
// < 0, then there is no limit, and all matches are found in a single
// call.
//
// Matching
//
// A searchable has a bunch of tokens.  These are just the
// whitespace-separated strings in the name and the id.  For example,
// the ARP record:
//
//   id = 'CYYC', name = 'Calgary Intl'
//
// has 3 tokens, "CYYC", "Calgary", and "Intl".
//
// A search string also consists of a set of whitespace-separated
// search tokens.  A token with trailing whitespace is called a
// "complete" token, while a token with no whitespace following is an
// "incomplete" token.  In a search string, there can be 0 or more
// complete tokens, and 0 or 1 incomplete tokens.
//
// Complete tokens and incomplete tokens match differently.  A
// complete token must match exactly, whereas an incomplete token only
// needs to match the head of a string.  For example, the complete
// token "Foo" only matches "foo", "FOO", "fOo", etc, (case doesn't
// matter), whereas the incomplete token "Foo" matches "foo",
// "FoOt", "FOOBAR", etc.
//
// A search string (which contains a set of tokens) matches a
// searchable if all of the tokens in the search string have matches
// in the tokens in the record.
//
// Using the ARP record above, the partial search strings "C", "cy",
// "CA", "INTL" all match.  As well, "Calgary CY" matches (a complete
// match with "Calgary" and a partial match with "CY"), but "Calgary
// CY " doesn't (both "Calgary" and "CY" are complete because they
// have trailing whitespace, and "CY" fails to match anything).
bool Searcher::findMatches(const string& str, const sgdVec3 centre, 
			   int maxMatches)
{
    // True if we change _matches.  This is our return value.
    bool changed = false;
    
    // A bit of a silly case, but it seemed important to allow it.
    if (maxMatches == 0) {
	return changed;
    }

    if (maxMatches < 0) {
	maxMatches = _tokens.size();
    }

    // We need to start a new search if the search string has changed.
    if (str != _lastSearchString) {
	// New.  Reset and regenerate the static variables.
	_lastSearchString = str;
	_end = _tokens.end();
	_matches->clear();
	changed = true;

	// Tokenize the search string.  All tokens except the last are
	// complete tokens.  The last may or may not be complete.
	_completeSearchTokens.clear();
	_partialSearchToken = "";
	_aToken = "";
	_isPartial = false;

	istringstream stream(str);
	stream >> _aToken;
	while (stream) {
	    // To determine if the current token is complete or not, we
	    // just see if we've gone to the end of the stream.  If we're
	    // at the very end, then the current token is incomplete.
	    unsigned int loc = stream.tellg();
	    if (loc == str.length()) {
		_partialSearchToken = _aToken;
	    } else {
		_completeSearchTokens.push_back(_aToken);
	    }
	    stream >> _aToken;
	}

	// Now grab a search token.  It doesn't really matter which
	// one we choose, so we select the last complete token, or, if
	// there are no complete tokens, the partial token.
	if (_completeSearchTokens.size() > 0) {
	    _aToken = _completeSearchTokens.back();
	    _completeSearchTokens.pop_back();
	    _isPartial = false;
	} else if (_partialSearchToken.length() > 0) {
	    _aToken = _partialSearchToken;
	    _isPartial = true;
	    _partialSearchToken = "";
	} else {
	    // No tokens in the search string at all.  Does that mean
	    // we match everything, or nothing?  I choose nothing.  We
	    // set aToken to an empty string, which will cause us to
	    // return immediately later.
	    _aToken = "";
	}
    }

    // If our centre of interest has changed, we'll need to recreate
    // the _matches set with a new comparator created with the new
    // centre.  We also need to mark ourselves as changed.

    // EYE - sgdCompareVec has different return values than
    // sgCompareVec!
    if (sgdCompareVec3(centre, _comparator.centre(), 0.0) != 0) {
	// Set the new centre of interest.
	_comparator.setCentre(centre);

	// Create a set using the comparator, and load it with the
	// data from the old set.
	set<Searchable *, SearchableLessThan> *tmp = 
	    new set<Searchable *, SearchableLessThan>(_comparator);
	copy(_matches->begin(), _matches->end(), inserter(*tmp, tmp->begin()));
	
	// Now make _matches point to the new set.
	delete _matches;
	_matches = tmp;

	changed = true;
    }

    // If there's no token to search for, just return now.
    if (_aToken.empty()) {
	return changed;
    }

    multimap<string, Searchable *, CaseFreeLessThan>::const_iterator i;
    int noOfMatches;

    // EYE - what happens if new tokens are added during a search?

    // If end is _tokens.end(), that means we're beginning a new search.
    if (_end == _tokens.end()) {
	// Search for the first matching token.  We need to do a
	// different kind of comparison depending on whether it's a
	// complete or partial token.
	for (i = _tokens.begin(); i != _tokens.end(); i++) {
	    int res;
	    if (_isPartial) {
		res = strncasecmp(_aToken.c_str(), i->first.c_str(), 
				  _aToken.length());
	    } else {
		res = strcasecmp(_aToken.c_str(), i->first.c_str());
	    }
	    if (res == 0) {
		// We found the start of the range.
		break;
	    }
	}

	// Did we find anything?
	if (i == _tokens.end()) {
	    // Nope.
	    return changed;
	}

	// Yes.
	_end = --i;
    }

    // At this point, end is just before the range we want to check.
    // We keep going until we get maxMatches more matches, we run out
    // of matches, or we hit the end of the _tokens map.
    i = ++_end;
    noOfMatches = 0;
    while ((noOfMatches < maxMatches) && (i != _tokens.end())) {
	int res;
	if (_isPartial) {
	    res = strncasecmp(_aToken.c_str(), i->first.c_str(), 
			      _aToken.length());
	} else {
	    res = strcasecmp(_aToken.c_str(), i->first.c_str());
	}
	if (res != 0) {
	    // We've run out of matches for this token, so bail.
	    break;
	}
	// A token from this searchable matches aToken.  See if
	// matches all the tokens in the given search string.
	if (_match(i->second, _completeSearchTokens, _partialSearchToken)) {
	    // It does.  If it isn't in the list already, add it.
	    Searchable *s = i->second;
	    if (_matches->find(s) == _matches->end()) {
		_matches->insert(s);
		noOfMatches++;
		changed = true;
	    }
	}
	i++;
    }
    _end = --i;

    return changed;
}

Searchable *Searcher::getMatch(unsigned int i)
{
    if (i >= _matches->size()) {
	return NULL;
    }

    // EYE - not too efficient.
    set<Searchable *, SearchableLessThan>::const_iterator iter = 
	_matches->begin();
    for (unsigned int j = 0; j < i; j++) {
	iter++;
    }

    return *iter;
}

// Does a complete match operation between the given searchable and a
// set of tokens (one of which may be partial).
bool Searcher::_match(Searchable *s, 
		      const vector<string>& completeSearchTokens,
		      const string& partialSearchToken)
{
    const vector<string>& tmp = s->tokens();

    // Search the vector for all the complete search tokens.  We just
    // do a dumb linear search, but this shouldn't be too wasteful,
    // because completeSearchTokens will usually be small (one or two
    // strings), as will the tokens from the searchable (maybe three
    // or four strings).  If this does become onerous, though, we
    // could sort both tmp and completeSearchTokens, although we
    // almost certainly would have to convert them to lower case as
    // well.
    for (unsigned int k = 0; k < completeSearchTokens.size(); k++) {
	unsigned int j;
	for (j = 0; j < tmp.size(); j++) {
	    // Complete search tokens require an exact match.
	    if (strcasecmp(completeSearchTokens[k].c_str(), 
			   tmp[j].c_str()) == 0) {
		break;
	    }
	}
	if (j == tmp.size()) {
	    // We found no match, so bail.
	    return false;
	}
    }
    // Search the vector for the partial search token.
    if (partialSearchToken != "") {
	unsigned int len = partialSearchToken.length();
	unsigned int j;
	for (j = 0; j < tmp.size(); j++) {
	    // Partial search tokens require a "head" match.
	    if (strncasecmp(partialSearchToken.c_str(), 
			    tmp[j].c_str(), len) == 0) {
		break;
	    }
	}
	if (j == tmp.size()) {
	    // We found no match, so bail.
	    return false;
	}
    }

    return true;
}

