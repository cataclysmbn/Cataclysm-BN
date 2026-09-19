#include "ascii_art.h"

#include <set>

#include "assign.h"
#include "catacharset.h"
#include "debug.h"
#include "generic_factory.h"
#include "output.h"
#include "string_id.h"
#include "type_id_implement.h"

static const int ascii_art_width = 42;

namespace
{
generic_factory<ascii_art> ascii_art_factory( "ascii_art" );
} // namespace

IMPLEMENT_STRING_AND_INT_IDS( ascii_art, ascii_art_factory );

void ascii_art::reset()
{
    ascii_art_factory.reset();
}

void ascii_art::load_ascii_art( const JsonObject &jo, const std::string &src )
{
    ascii_art_factory.load( jo, src );
}

void ascii_art::load( const JsonObject &jo, const std::string & )
{
    assign( jo, "id", id );

    assign( jo, "picture", picture );
    for( std::string &line : picture ) {
        if( utf8_width( remove_color_tags( line ) ) > ascii_art_width ) {
            line = trim_by_length( line, ascii_art_width );
            debugmsg( "picture in %s contains a line too long to be displayed (>%i char).", id.c_str(),
                      ascii_art_width );
        }
    }
}

