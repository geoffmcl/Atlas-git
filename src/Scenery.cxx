#include <simgear/compiler.h>
#include <simgear/misc/sg_path.hxx>

#include "MapMaker.hxx"

SG_USING_STD(vector);
SG_USING_STD(string);

typedef vector<string> string_list;

extern MapMaker mapobj;
extern string_list fg_scenery;
extern unsigned int max_path_length;

// Cribbed from FlightGear, with the objects code commented out.
// The supplied path is appended with 'Terrain' if it exists, not if otherwise.
// NOTE: MapMaker::setFGRoot() should be called first.
void set_fg_scenery(const string &scenery) {
    SGPath s;
    
    char* fg_root = mapobj.getFGRoot();
    if (scenery.empty() && fg_root != NULL) {
        s.set( fg_root );
        s.append( "Scenery" );
    } else {
        s.set( scenery );
    }
    
    string_list path_list = sgPathSplit( s.str() );
    fg_scenery.clear();
    
    for (unsigned i = 0; i < path_list.size(); i++) {

        max_path_length = ( path_list[i].size() > max_path_length ? path_list[i].size() : max_path_length );
        ulDir *d = ulOpenDir( path_list[i].c_str() );
        if (d == NULL)
            continue;
        ulCloseDir( d );

        //SGPath pt( path_list[i], po( path_list[i] );
	SGPath pt( path_list[i] );
        pt.append("Terrain");
        //po.append("Objects");

        ulDir *td = ulOpenDir( pt.c_str() );
        //ulDir *od = ulOpenDir( po.c_str() );

        //if (td == NULL && od == NULL)
	if(td == NULL) { // ie. it doen't exist with Terrain appended - push back the original
            fg_scenery.push_back( path_list[i] );
        } else {
            if (td != NULL) {
                fg_scenery.push_back( pt.str() );
                ulCloseDir( td );
            }
	    /*
            if (od != NULL) {
                fg_scenery.push_back( po.str() );
                ulCloseDir( od );
            }
	    */
        }
    }
}
