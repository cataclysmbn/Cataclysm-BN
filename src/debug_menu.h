#pragma once

#include <optional>
#include <string>
#include <vector>

#include "coordinates.h"
#include "type_id.h"

class Character;
class Creature;
class player;

namespace debug_menu
{
enum bench_kind {
    DRAW,
    FPS
};

void teleport_short();
void teleport_long();
void teleport_overmap( bool specific_coordinates = false );

void spawn_nested_mapgen();
void character_edit_menu( Character &c );
void effect_edit_menu( Creature &c );
void wishitem( Character *who = nullptr );
void wishitem( Character *, const tripoint_bub_ms & );
void wishmonster( const std::optional<tripoint_bub_ms> &p );
void wishmutate( Character * );
/// One mutation as the mutation menu groups it into the pages of its tab bar.
struct wish_mutation_group {
    /// The category pages the mutation is listed on: the categories it names and
    /// the categories it is the threshold of.
    std::vector<mutation_category_id> categories;
    /// The mutation type pages the mutation is listed on.
    std::vector<std::string> types;
    /// Whether the mutation only exists for NPCs and belongs to the NPC page.
    bool npc = false;
    /// Whether the mutation only exists for debugging and belongs to the debug page.
    bool debug = false;
};
/// The groups of every mutation, in the order of mutation_branch::get_all().
using wish_mutation_group_list = std::vector<wish_mutation_group>;
/// Group every loaded mutation into the pages of the mutation menu's tab bar.
/// The categories come from the category and the threshold of the mutation and the
/// type pages from its mutation types, so that mutations that mods add to an
/// existing category or type, and the categories and types they add themselves,
/// show up in the menu without further changes.
auto wish_mutation_groups() -> wish_mutation_group_list;
/// Mutation categories that at least one of the given mutations belongs to,
/// ordered by their id.
auto wish_mutation_categories( const wish_mutation_group_list &groups )
-> std::vector<mutation_category_id>;
/// Mutation types that at least one of the given mutations belongs to, ordered by
/// their id.
auto wish_mutation_types( const wish_mutation_group_list &groups ) -> std::vector<std::string>;
/// The label of the page of a mutation category: its name, or its id when the
/// loaded data has no category with that id.
auto wish_mutation_category_label( const mutation_category_id &category ) -> std::string;
/// The label of the page of a mutation type: the name the game shows for the type,
/// or its id when it has none.
auto wish_mutation_type_label( const std::string &type ) -> std::string;
/// The pages of the mutation menu's category tab bar: for every mutation, in the
/// order of the given groups of the mutations, the pages it is listed on.
using wish_mutation_page_list = std::vector<std::vector<std::size_t>>;
/// For every mutation the pages of the tab bar it is listed on: the unfiltered
/// first page, the page of every mutation type, the NPC page, the page of every
/// category, the debug page and the generic page for the mutations that have no
/// page of their own.
auto wish_mutation_pages( const wish_mutation_group_list &groups,
                          const std::vector<mutation_category_id> &categories,
                          const std::vector<std::string> &types )
-> wish_mutation_page_list;
void wishbionics( Character &c );
void wishskill( Character * );
void mutation_wish();
void benchmark( int max_difference, bench_kind kind );

void debug();

/* Splits a string by @param delimiter and push_back's the elements into _Container */
template<typename Container>
Container string_to_iterable( const std::string_view str, const std::string_view delimiter )
{
    Container res;

    size_t pos = 0;
    size_t start = 0;
    while( ( pos = str.find( delimiter, start ) ) != std::string::npos ) {
        if( pos > start ) {
            res.emplace_back( str.substr( start, pos - start ) );
        }
        start = pos + delimiter.length();
    }
    if( start != str.length() ) {
        res.emplace_back( str.substr( start, str.length() - start ) );
    }

    return res;
}

/* Merges iterable elements into std::string with
 * @param delimiter between them
 * @param f is callable that is called to transform each value
 * */
template<typename Container, typename Mapper>
std::string iterable_to_string( const Container &values, const std::string_view delimiter,
                                const Mapper &f )
{
    std::string res;
    for( auto iter = values.begin(); iter != values.end(); ++iter ) {
        if( iter != values.begin() ) {
            res += delimiter;
        }
        res += f( *iter );
    }
    return res;
}

template<typename Container>
std::string iterable_to_string( const Container &values, const std::string_view delimiter )
{
    return iterable_to_string( values, delimiter, []( const std::string_view f ) {
        return f;
    } );
}

} // namespace debug_menu


