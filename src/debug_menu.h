#pragma once

#include <optional>
#include <string>
#include <vector>

#include "coordinates.h"
#include "type_id.h"

class Character;
class Creature;
class itype;
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
/// Item categories of the given item types, in display order. The spawn item
/// menu uses them for its category pages.
auto wish_item_categories( const std::vector<const itype *> &items )
-> std::vector<item_category_id>;
/// Page of the spawn item menu each of the given item types belongs to, using
/// the category list of wish_item_categories(). The first page holds every item,
/// item types without a category only show up there.
auto wish_item_pages( const std::vector<const itype *> &items,
                      const std::vector<item_category_id> &categories ) -> std::vector<std::size_t>;
/// First and last label of the spawn item menu's category tab bar that fit into
/// a bar max_tab_width columns wide together with the current one.
struct wish_item_tab_bounds {
    std::size_t first = 0;
    std::size_t last = 0;
};
/// The part of the tab bar around the current page that fits into max_tab_width
/// columns. The range always contains the current tab, so that the current page
/// stays visible.
auto wish_item_tab_range( const std::vector<std::string> &tabs, std::size_t current,
                          int max_tab_width ) -> wish_item_tab_bounds;
void wishmonster( const std::optional<tripoint_bub_ms> &p );
void wishmutate( Character * );
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


