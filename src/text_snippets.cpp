#include "text_snippets.h"

#include <cstddef>
#include <random>
#include <utility>

#include "debug.h"
#include "fstream_utils.h"
#include "generic_factory.h"
#include "json.h"
#include "path_info.h"
#include "rng.h"

snippet_library SNIPPET;

void snippet_library::load_snippet( const JsonObject &jsobj )
{
    if( hash_to_id_migration.has_value() ) {
        debugmsg( "snippet_library::load_snippet called after snippet_library::migrate_hash_to_id." );
    }
    hash_to_id_migration = std::nullopt;
    const std::string category = jsobj.get_string( "category" );
    if( jsobj.has_array( "text" ) ) {
        add_snippets_from_json( category, jsobj.get_array( "text" ) );
    } else {
        add_snippet_from_json( category, jsobj );
    }
}

void snippet_library::add_snippets_from_json( const std::string &category, const JsonArray &jarr )
{
    if( hash_to_id_migration.has_value() ) {
        debugmsg( "snippet_library::add_snippets_from_json called after snippet_library::migrate_hash_to_id." );
    }
    hash_to_id_migration = std::nullopt;
    for( const JsonValue entry : jarr ) {
        if( entry.test_string() ) {
            translation text;
            if( !entry.read( text ) ) {
                entry.throw_error( "Error reading snippet from JSON array" );
            }
            snippets_by_category[category].no_id.emplace_back( text );
        } else {
            JsonObject jo = entry.get_object();
            add_snippet_from_json( category, jo );
        }
    }
}

void snippet_library::add_snippet_from_json( const std::string &category, const JsonObject &jo )
{
    if( hash_to_id_migration.has_value() ) {
        debugmsg( "snippet_library::add_snippet_from_json called after snippet_library::migrate_hash_to_id." );
    }
    hash_to_id_migration = std::nullopt;
    translation text;
    mandatory( jo, false, "text", text );
    if( jo.has_member( "id" ) ) {
        snippet_id id;
        jo.read( "id", id );
        if( id.is_null() ) {
            jo.throw_error( "Null snippet id specified", "id" );
        }
        if( snippets_by_id.contains( id ) ) {
            jo.throw_error( "Duplicate snippet id", "id" );
        }
        snippets_by_category[category].ids.emplace_back( id );
        snippets_by_id[id] = text;

        translation name;
        optional( jo, false, "name", name );
        if( !name.empty() ) {
            name_by_id[id] = name;
        }
    } else {
        snippets_by_category[category].no_id.emplace_back( text );
    }
}

void snippet_library::clear_snippets()
{
    hash_to_id_migration = std::nullopt;
    snippets_by_category.clear();
    snippets_by_id.clear();
}

bool snippet_library::has_category( const std::string &category ) const
{
    return snippets_by_category.contains( category );
}

std::optional<translation> snippet_library::get_snippet_by_id( const snippet_id &id ) const
{
    const auto it = snippets_by_id.find( id );
    if( it == snippets_by_id.end() ) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<translation> snippet_library::get_name_by_id( const snippet_id &id ) const
{
    const auto it = name_by_id.find( id );
    if( it == name_by_id.end() ) {
        return std::nullopt;
    }
    return it->second;
}

const translation &snippet_library::get_snippet_ref_by_id( const snippet_id &id ) const
{
    const auto it = snippets_by_id.find( id );
    if( it == snippets_by_id.end() ) {
        static const translation empty_translation;
        return empty_translation;
    }
    return it->second;
}

bool snippet_library::has_snippet_with_id( const snippet_id &id ) const
{
    return snippets_by_id.contains( id );
}

std::string snippet_library::expand( const std::string &str ) const
{
    size_t tag_begin = str.find( '<' );
    if( tag_begin == std::string::npos ) {
        return str;
    }
    size_t tag_end = str.find( '>', tag_begin + 1 );
    if( tag_end == std::string::npos ) {
        return str;
    }

    std::string symbol = str.substr( tag_begin, tag_end - tag_begin + 1 );
    std::optional<translation> replacement = random_from_category( symbol );
    if( !replacement.has_value() ) {
        return str.substr( 0, tag_end + 1 )
               + expand( str.substr( tag_end + 1 ) );
    }
    return str.substr( 0, tag_begin )
           + expand( replacement.value().translated() )
           + expand( str.substr( tag_end + 1 ) );
}

snippet_id snippet_library::random_id_from_category( const std::string &cat ) const
{
    const auto it = snippets_by_category.find( cat );
    if( it == snippets_by_category.end() ) {
        return snippet_id::NULL_ID();
    }
    if( !it->second.no_id.empty() ) {
        debugmsg( "ids are required, but not specified for some snippets in category %s", cat );
    }
    if( it->second.ids.empty() ) {
        return snippet_id::NULL_ID();
    }
    return random_entry( it->second.ids );
}

std::optional<translation> snippet_library::random_from_category( const std::string &cat ) const
{
    return random_from_category( cat, rng_bits() );
}

std::optional<translation> snippet_library::random_from_category( const std::string &cat,
        unsigned int seed ) const
{
    const auto it = snippets_by_category.find( cat );
    if( it == snippets_by_category.end() ) {
        return std::nullopt;
    }
    if( it->second.ids.empty() && it->second.no_id.empty() ) {
        return std::nullopt;
    }
    const size_t count = it->second.ids.size() + it->second.no_id.size();
    // uniform_int_distribution always returns zero when the random engine is
    // cata_default_random_engine aka std::minstd_rand0 and the seed is small,
    // so std::mt19937 is used instead. This engine is deterministcally seeded,
    // so acceptable.
    // NOLINTNEXTLINE(cata-determinism)
    std::mt19937 generator( seed );
    std::uniform_int_distribution<size_t> dis( 0, count - 1 );
    const size_t index = dis( generator );
    if( index < it->second.ids.size() ) {
        return get_snippet_by_id( it->second.ids[index] );
    } else {
        return it->second.no_id[index - it->second.ids.size()];
    }
}

snippet_id snippet_library::migrate_hash_to_id( const int old_hash )
{
    if( !hash_to_id_migration.has_value() ) {
        hash_to_id_migration.emplace();
        for( const auto &id_and_text : snippets_by_id ) {
            std::pair<bool, int> hash = id_and_text.second.legacy_hash();
            if( hash.first ) {
                hash_to_id_migration->emplace( hash.second, id_and_text.first );
            }
        }
    }
    const auto it = hash_to_id_migration->find( old_hash );
    if( it == hash_to_id_migration->end() ) {
        return snippet_id::NULL_ID();
    }
    return it->second;
}

template<> const translation &snippet_id::obj() const
{
    return SNIPPET.get_snippet_ref_by_id( *this );
}

template<> bool snippet_id::is_valid() const
{
    return SNIPPET.has_snippet_with_id( *this );
}

std::string get_random_tip_of_the_day()
{
    static bool did_load = false;
    static std::vector<std::string> all_tips;

    if( !did_load ) {
        did_load = true;
        bool success = read_from_file_json( PATH_INFO::main_menu_tips(), [&]( JsonIn & jsin ) {
            JsonArray jarr = jsin.get_array();
            if( jarr.size() != 1 ) {
                jarr.throw_error( "expected 1 element in main array" );
            }
            all_tips.reserve( jarr.size() );
            for( JsonValue jval : jarr ) {
                JsonObject jobj = jval.get_object();
                if( jobj.get_string( "type" ) != "snippet" ) {
                    jobj.throw_error( "expected 'snippet' type", "type" );
                }
                if( jobj.get_string( "category" ) != "tip" ) {
                    jobj.throw_error( "expected 'tip' category", "category" );
                }
                std::vector<std::string> text = jobj.get_string_array( "text" );
                all_tips.insert( all_tips.end(), text.begin(), text.end() );
            }
        } );
        if( !success ) {
            all_tips.clear();
        }
    }

    if( all_tips.empty() ) {
        return _( "Failed to load tip of the day.  You'll have to figure things out on your own :(" );
    } else {
        // uniform_int_distribution always returns zero when the random engine is
        // cata_default_random_engine aka std::minstd_rand0 and the seed is small,
        // so std::mt19937 is used instead. This engine is deterministcally seeded,
        // so acceptable.
        // NOLINTNEXTLINE(cata-determinism)
        std::mt19937 generator( rng_bits() );
        std::uniform_int_distribution<size_t> dis( 0, all_tips.size() - 1 );
        const size_t index = dis( generator );
        return _( all_tips[ index ] );
    }
}
