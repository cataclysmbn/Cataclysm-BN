#include <utility>

#include "map_feature_descriptions.h"

#include "json.h"

namespace map_feature_descriptions
{

namespace
{
std::vector<map_feature_description> descriptions;

map_feature_description::test_type parse_test_type( const std::string &test, const JsonObject &jo )
{
    if( test == "bashable" ) {
        return map_feature_description::test_type::bashable;
    }
    if( test == "diggable" ) {
        return map_feature_description::test_type::diggable;
    }
    if( test == "flag" ) {
        return map_feature_description::test_type::flag;
    }
    jo.throw_error( "Unknown map feature test type: " + test, "test" );
    return map_feature_description::test_type::flag;
}
} // namespace

void load_map_feature_descriptions( const JsonObject &jo )
{
    JsonArray features = jo.get_array( "features" );
    while( features.has_more() ) {
        JsonObject feature = features.next_object();
        map_feature_description entry;
        entry.text = to_translation( feature.get_string( "text" ) );
        entry.test = parse_test_type( feature.get_string( "test" ), feature );
        if( entry.test == map_feature_description::test_type::flag ) {
            entry.flag = feature.get_string( "flag" );
        }
        descriptions.push_back( std::move( entry ) );
    }
}

const std::vector<map_feature_description> &get_map_feature_descriptions()
{
    return descriptions;
}

void reset_map_feature_descriptions()
{
    descriptions.clear();
}

} // namespace map_feature_descriptions
