#include "armor_layers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include "avatar.h"
#include "cata_utility.h"
#include "catacharset.h" // used for utf8_width()
#include "character_display.h"
#include "clothing_utils.h"
#include "debug.h"
#include "engine_client_component.h"
#include "engine_client_requirements.h"
#include "engine_client_world.h"
#include "enums.h"
#include "flag.h"
#include "game.h"
#include "game_inventory.h"
#include "input.h"
#include "inventory.h"
#include "item.h"
#include "item_serial.h"
#include "line.h"
#include "output.h"
#include "player_activity.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "ui_manager.h"
#include "units_utility.h"


namespace
{
const activity_id ACT_ARMOR_LAYERS( "ACT_ARMOR_LAYERS" );
const bodypart_str_id body_part_appendix( "num_bp" );
const flag_id json_flag_HIDDEN( "HIDDEN" );

std::string clothing_layer( const item &worn_item );
/// A named number of an item's details, shown by the middle pane and published by the screen.
struct item_stat {
    std::string key;
    std::string label;
    std::string value;
};
auto property_stats( const item &worn_item, const Character &c,
                     const bodypart_id &bp ) -> std::vector<item_stat>;
auto protection_stats( const item &worn_item ) -> std::vector<item_stat>;
std::vector<std::string> clothing_properties(
    const item &worn_item, int width, const Character &, const bodypart_id &bp );
std::vector<std::string> clothing_protection( const item &worn_item, int width );
std::vector<std::string> clothing_flags_description( const item &worn_item );

struct item_penalties {
    std::vector<bodypart_id> body_parts_with_stacking_penalty;
    std::vector<bodypart_id> body_parts_with_out_of_order_penalty;
    std::set<std::string> bad_items_within;

    int badness() const {
        return !body_parts_with_stacking_penalty.empty() +
               !body_parts_with_out_of_order_penalty.empty();
    }

    nc_color color_for_stacking_badness() const {
        switch( badness() ) {
            case 0:
                return c_light_gray;
            case 1:
                return c_yellow;
            case 2:
                return c_light_red;
        }
        debugmsg( "Unexpected badness %d", badness() );
        return c_light_gray;
    }
};

// Figure out encumbrance penalties this clothing is involved in
item_penalties get_item_penalties( const location_vector<item>::const_iterator &worn_item_it,
                                   const Character &c, const bodypart_id & )
{
    item *const &worn_item = *worn_item_it;
    layer_level layer = worn_item->get_layer();

    std::vector<bodypart_id> body_parts_with_stacking_penalty;
    std::vector<bodypart_id> body_parts_with_out_of_order_penalty;
    std::vector<std::set<std::string>> lists_of_bad_items_within;

    for( const bodypart_id &bp : c.get_all_body_parts() ) {
        if( !worn_item->covers( bp ) ) {
            continue;
        }
        const int num_items = std::count_if( c.worn.begin(), c.worn.end(),
        [&layer, &bp, &c]( item * const & i ) {
            return i->get_layer() == layer && i->covers( bp )
                   && !( i->has_flag( flag_SEMITANGIBLE ) || is_compact( *i, c ) );
        } );
        if( num_items > 1 ) {
            body_parts_with_stacking_penalty.push_back( bp );
        }

        std::set<std::string> bad_items_within;
        for( auto it = c.worn.begin(); it != worn_item_it; ++it ) {
            if( ( *it )->get_layer() > layer && ( *it )->covers( bp ) ) {
                bad_items_within.insert( ( *it )->type_name() );
            }
        }
        if( !bad_items_within.empty() ) {
            body_parts_with_out_of_order_penalty.push_back( bp );
            lists_of_bad_items_within.push_back( bad_items_within );
        }
    }

    // We intersect all the lists_of_bad_items_within so that if there is one
    // common bad item we're wearing this one over it can be mentioned in the
    // message explaining the penalty.
    while( lists_of_bad_items_within.size() > 1 ) {
        std::set<std::string> intersection_of_first_two;
        std::set_intersection(
            lists_of_bad_items_within[0].begin(), lists_of_bad_items_within[0].end(),
            lists_of_bad_items_within[1].begin(), lists_of_bad_items_within[1].end(),
            std::inserter( intersection_of_first_two, intersection_of_first_two.begin() )
        );
        lists_of_bad_items_within.erase( lists_of_bad_items_within.begin() );
        lists_of_bad_items_within[0] = std::move( intersection_of_first_two );
    }

    if( lists_of_bad_items_within.empty() ) {
        lists_of_bad_items_within.emplace_back();
    }

    return { std::move( body_parts_with_stacking_penalty ),
             std::move( body_parts_with_out_of_order_penalty ),
             std::move( lists_of_bad_items_within[0] ) };
}

auto stacking_penalty_message( const item &worn_item,
                               const item_penalties &penalties ) -> std::string;
auto out_of_order_penalty_message( const item_penalties &penalties ) -> std::string;

std::string body_part_names( const std::vector<bodypart_id> &parts )
{
    if( parts.empty() ) {
        debugmsg( "Asked for names of empty list" );
        return {};
    }

    std::vector<std::string> names;
    names.reserve( parts.size() );
    for( size_t i = 0; i < parts.size(); ++i ) {
        const bodypart_id &part = parts[i];
        if( i + 1 < parts.size() &&
            parts[i + 1] == convert_bp( static_cast<body_part>( bp_aiOther[part->token] ) ).id() ) {
            // Can combine two body parts (e.g. arms)
            names.push_back( body_part_name_accusative( part, 2 ) );
            ++i;
        } else {
            names.push_back( body_part_name_accusative( part ) );
        }
    }

    return enumerate_as_string( names );
}

void draw_mid_pane( const catacurses::window &w_sort_middle,
                    location_vector<item>::const_iterator const &worn_item_it,
                    const Character &c, const bodypart_id &bp )
{
    item *const &worn_item = *worn_item_it;
    const int win_width = getmaxx( w_sort_middle );
    const size_t win_height = static_cast<size_t>( getmaxy( w_sort_middle ) );
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    size_t i = fold_and_print( w_sort_middle, point( 1, 0 ), win_width - 1, c_white,
                               worn_item->type_name( 1 ) ) - 1;
    std::vector<std::string> props = clothing_properties( *worn_item, win_width - 3, c, bp );
    nc_color color = c_light_gray;
    for( std::string &iter : props ) {
        print_colored_text( w_sort_middle, point( 2, ++i ), color, c_light_gray, iter );
    }

    std::vector<std::string> prot = clothing_protection( *worn_item, win_width - 3 );
    if( i + prot.size() < win_height ) {
        for( std::string &iter : prot ) {
            print_colored_text( w_sort_middle, point( 2, ++i ), color, c_light_gray, iter );
        }
    } else {
        return;
    }

    i++;
    std::vector<std::string> layer_desc = foldstring( clothing_layer( *worn_item ), win_width );
    if( i + layer_desc.size() < win_height && !clothing_layer( *worn_item ).empty() ) {
        for( std::string &iter : layer_desc ) {
            mvwprintz( w_sort_middle, point( 0, ++i ), c_light_blue, iter );
        }
    }

    i++;
    std::vector<std::string> desc = clothing_flags_description( *worn_item );
    if( !desc.empty() ) {
        for( size_t j = 0; j < desc.size() && i + j < win_height; ++j ) {
            i += fold_and_print( w_sort_middle, point( 0, i ), win_width, c_light_blue, desc[j] );
        }
    }

    const item_penalties penalties = get_item_penalties( worn_item_it, c, bp );

    const std::string stacking = stacking_penalty_message( *worn_item, penalties );
    if( !stacking.empty() ) {
        i += fold_and_print( w_sort_middle, point( 0, i ), win_width, c_light_gray, stacking );
    }

    const std::string out_of_order = out_of_order_penalty_message( penalties );
    if( !out_of_order.empty() ) {
        fold_and_print( w_sort_middle, point( 0, i ), win_width, c_light_gray, out_of_order );
    }
}

/// Why the item adds encumbrance by sharing a layer with others; empty when it does not.
auto stacking_penalty_message( const item &worn_item,
                               const item_penalties &penalties ) -> std::string
{
    if( penalties.body_parts_with_stacking_penalty.empty() ) {
        return {};
    }
    std::string layer_description = [&]() {
        switch( worn_item.get_layer() ) {
            case PERSONAL_LAYER:
                return _( "in your <color_light_blue>personal aura</color>" );
            case UNDERWEAR_LAYER:
                return _( "<color_light_blue>close to your skin</color>" );
            case REGULAR_LAYER:
                return _( "of <color_light_blue>normal</color> clothing" );
            case WAIST_LAYER:
                return _( "on your <color_light_blue>waist</color>" );
            case OUTER_LAYER:
                return _( "of <color_light_blue>outer</color> clothing" );
            case BELTED_LAYER:
                return _( "<color_light_blue>strapped</color> to you" );
            case AURA_LAYER:
                return _( "an <color_light_blue>aura</color> around you" );
            default:
                return _( "Unexpected layer" );
        }
    }
    ();
    std::string body_parts = body_part_names( penalties.body_parts_with_stacking_penalty );
    return string_format(
               vgettext( "Wearing multiple items %s on your "
                         "<color_light_red>%s</color> is adding encumbrance there.",
                         "Wearing multiple items %s on your "
                         "<color_light_red>%s</color> is adding encumbrance there.",
                         penalties.body_parts_with_stacking_penalty.size() ),
               layer_description, body_parts
           );
}

/// Why the item adds encumbrance by being worn out of order; empty when it does not.
auto out_of_order_penalty_message( const item_penalties &penalties ) -> std::string
{
    if( penalties.body_parts_with_out_of_order_penalty.empty() ) {
        return {};
    }
    std::string body_parts = body_part_names( penalties.body_parts_with_out_of_order_penalty );
    if( penalties.bad_items_within.empty() ) {
        return string_format(
                   vgettext( "Wearing this outside items it would normally be beneath "
                             "is adding encumbrance to your <color_light_red>%s</color>.",
                             "Wearing this outside items it would normally be beneath "
                             "is adding encumbrance to your <color_light_red>%s</color>.",
                             penalties.body_parts_with_out_of_order_penalty.size() ),
                   body_parts
               );
    }
    std::string bad_item_name = *penalties.bad_items_within.begin();
    return string_format(
               vgettext( "Wearing this outside your <color_light_blue>%s</color> "
                         "is adding encumbrance to your <color_light_red>%s</color>.",
                         "Wearing this outside your <color_light_blue>%s</color> "
                         "is adding encumbrance to your <color_light_red>%s</color>.",
                         penalties.body_parts_with_out_of_order_penalty.size() ),
               bad_item_name, body_parts
           );
}

std::string clothing_layer( const item &worn_item )
{
    std::string layer;

    if( worn_item.has_flag( flag_PERSONAL ) ) {
        layer = _( "This is in your personal aura." );
    } else if( worn_item.has_flag( flag_SKINTIGHT ) ) {
        layer = _( "This is worn next to the skin." );
    } else if( worn_item.has_flag( flag_WAIST ) ) {
        layer = _( "This is worn on or around your waist." );
    } else if( worn_item.has_flag( flag_OUTER ) ) {
        layer = _( "This is worn over your other clothes." );
    } else if( worn_item.has_flag( flag_BELTED ) ) {
        layer = _( "This is strapped onto you." );
    } else if( worn_item.has_flag( flag_AURA ) ) {
        layer = _( "This is an aura around you." );
    }

    return layer;
}

auto property_stats( const item &worn_item, const Character &c,
                     const bodypart_id &bp ) -> std::vector<item_stat>
{
    const int coverage = bp.id().is_null() ? worn_item.get_avg_coverage() :
                         worn_item.get_coverage( bp );
    const int encumbrance = bp.id().is_null() ? worn_item.get_avg_encumber(
                                c ) : worn_item.get_encumber( c, bp );
    return {
        { "coverage", _( "Coverage:" ), string_format( "%3d", coverage ) },
        { "encumbrance", _( "Encumbrance:" ), string_format( "%3d", encumbrance ) },
        { "warmth", _( "Warmth:" ), string_format( "%3d", worn_item.get_warmth() ) },
        {
            "storage", string_format( _( "Storage (%s):" ), volume_units_abbr() ),
            format_volume( worn_item.get_storage() )
        },
    };
}

auto protection_stats( const item &worn_item ) -> std::vector<item_stat>
{
    return {
        { "bash", _( "Bash:" ), string_format( "%3d", worn_item.bash_resist() ) },
        { "cut", _( "Cut:" ), string_format( "%3d", worn_item.cut_resist() ) },
        { "ballistic", _( "Ballistic:" ), string_format( "%3d", worn_item.bullet_resist() ) },
        { "acid", _( "Acid:" ), string_format( "%3d", worn_item.acid_resist() ) },
        { "fire", _( "Fire:" ), string_format( "%3d", worn_item.fire_resist() ) },
        { "environmental", _( "Environmental:" ), string_format( "%3d", worn_item.get_env_resist() ) },
    };
}

std::vector<std::string> clothing_properties(
    const item &worn_item, const int width, const Character &c, const bodypart_id &bp )
{
    std::vector<std::string> props;
    props.reserve( 5 );

    const std::string space = "  ";
    props.push_back( string_format( "<color_c_green>[%s]</color>", _( "Properties" ) ) );
    for( const item_stat &stat : property_stats( worn_item, c, bp ) ) {
        props.push_back( name_and_value( space + stat.label, stat.value, width ) );
    }
    return props;
}

std::vector<std::string> clothing_protection( const item &worn_item, const int width )
{
    std::vector<std::string> prot;
    prot.reserve( 7 );

    const std::string space = "  ";
    prot.push_back( string_format( "<color_c_green>[%s]</color>", _( "Protection" ) ) );
    for( const item_stat &stat : protection_stats( worn_item ) ) {
        prot.push_back( name_and_value( space + stat.label, stat.value, width ) );
    }
    return prot;
}

std::vector<std::string> clothing_flags_description( const item &worn_item )
{
    std::vector<std::string> description_stack;

    if( worn_item.has_flag( flag_FIT ) ) {
        description_stack.emplace_back( _( "It fits you well." ) );
    } else if( worn_item.has_flag( flag_VARSIZE ) ) {
        description_stack.emplace_back( _( "It could be refitted." ) );
    }

    if( worn_item.has_flag( flag_HOOD ) ) {
        description_stack.emplace_back( _( "It has a hood." ) );
    }
    if( worn_item.has_flag( flag_POCKETS ) ) {
        description_stack.emplace_back( _( "It has pockets." ) );
    }
    if( worn_item.has_flag( flag_WATERPROOF ) ) {
        description_stack.emplace_back( _( "It is waterproof." ) );
    }
    if( worn_item.has_flag( flag_WATER_FRIENDLY ) ) {
        description_stack.emplace_back( _( "It is water friendly." ) );
    }
    if( worn_item.has_flag( flag_FANCY ) ) {
        description_stack.emplace_back( _( "It looks fancy." ) );
    }
    if( worn_item.has_flag( flag_SUPER_FANCY ) ) {
        description_stack.emplace_back( _( "It looks really fancy." ) );
    }
    if( worn_item.has_flag( flag_FLOTATION ) ) {
        description_stack.emplace_back( _( "You will not drown today." ) );
    }
    if( worn_item.has_flag( flag_OVERSIZE ) ) {
        description_stack.emplace_back( _( "It is very bulky." ) );
    }
    if( worn_item.has_flag( flag_SWIM_GOGGLES ) ) {
        description_stack.emplace_back( _( "It helps you to see clearly underwater." ) );
    }
    if( worn_item.has_flag( flag_SEMITANGIBLE ) ) {
        description_stack.emplace_back( _( "It can occupy the same space as other things." ) );
    }
    if( worn_item.has_flag( flag_COMPACT ) ) {
        description_stack.emplace_back( _( "It won't encumber you when worn with other things." ) );
    }

    return description_stack;
}

} //namespace

struct layering_item_info {
    item_penalties penalties;
    int encumber;
    std::string name;
    /// Where the item is among the worn ones; not part of what makes two entries alike.
    std::size_t position = 0;

    // Operator overload required to leverage vector equality operator.
    bool operator ==( const layering_item_info &o ) const {
        // This is used to merge e.g. both arms into one entry when their items
        // are equivalent.  For that purpose we don't care about the exact
        // penalities because they will list different body parts; we just
        // check that the badness is the same (which is all that matters for
        // rendering the right-hand list).
        return this->penalties.badness() == o.penalties.badness() &&
               this->encumber == o.encumber &&
               this->name == o.name;
    }
};

static std::vector<layering_item_info> items_cover_bp( const Character &c, const bodypart_id &bp )
{
    std::vector<layering_item_info> s;
    for( auto elem_it = c.worn.begin(); elem_it != c.worn.end(); ++elem_it ) {
        item *const &elem = *elem_it;
        if( elem->covers( bp ) ) {
            s.push_back( { get_item_penalties( elem_it, c, bp ),
                           elem->get_encumber( c, bp ),
                           elem->tname(),
                           static_cast<std::size_t>( elem_it - c.worn.begin() )
                         } );
        }
    }
    return s;
}

/// Whether a body part is listed together with its opposite: both have the same items, so one entry stands for the pair.
static auto combines_with_opposite( const Character &who, const bodypart_id &cover ) -> bool
{
    const bodypart_id opposite = cover.obj().opposite_part;
    return cover != opposite && items_cover_bp( who, cover ) == items_cover_bp( who, opposite );
}

/// The body parts the right list has an entry for: a combined pair once.
static auto listed_body_parts( const Character &who,
                               const cata::flat_set<bodypart_id> &parts ) -> cata::flat_set<bodypart_id>
{
    cata::flat_set<bodypart_id> listed;
    for( const bodypart_id cover : parts ) {
        if( !combines_with_opposite( who, cover ) || listed.count( cover.obj().opposite_part ) == 0 ) {
            listed.insert( cover );
        }
    }
    return listed;
}

namespace
{
namespace ec = engine_client;

constexpr auto tabs_id = "armor:tabs";
constexpr auto worn_rows_id = "armor:worn:rows";
constexpr auto select_part_id = "armor:part";
constexpr auto select_item_id = "armor:pick";

/// What the screen shows, as its loop keeps it.
struct armor_model {
    Character &who;
    const cata::flat_set<bodypart_id> &parts;
    const int &tab;
    /// Where in the worn items each item of the body part's list is.
    const std::vector<int> &listed;
    const int &cursor;
    /// The list index of the item grabbed to be moved, or -1.
    const int &grabbed;
};

/// The identity of a body part filter, and of a part in the lists.
auto part_id( const bodypart_id &part ) -> std::string
{
    return part.id().is_null() ? "part:all" : "part:" + part.id().str();
}

/// The identity of each worn item: its own serial, so it keeps it when the layers are reordered.
auto worn_ids( const Character &who ) -> std::vector<std::string>
{
    return who.worn | std::views::transform( []( const item * worn ) { return game_client::item_row_id( *worn ); } )
    | std::ranges::to<std::vector>();
}

auto stat_of( const item_stat &stat ) -> ec::panel_stat
{
    return { .key = stat.key, .label = ec::caption( stat.label ), .value = trim( stat.value ) };
}

auto worn_rows( const armor_model &model ) -> std::vector<ec::row>
{
    const auto ids = worn_ids( model.who );
    const auto &part = model.parts[model.tab];
    auto rows = std::vector<ec::row> {};
    for( const auto &[index, position] : std::views::enumerate( model.listed ) ) {
        const auto it = std::next( model.who.worn.begin(), position );
        const item &worn = **it;
        rows.push_back( {
            .id = ids[position], .label = remove_color_tags( worn.display_name() ),
            .selected = static_cast<int>( index ) == model.grabbed,
            .columns = {
                { .key = "storage", .label = string_format( _( "Storage (%s)" ), volume_units_abbr() ), .value = format_volume( worn.get_storage() ) },
                { .key = "hidden", .label = _( "Hidden" ), .value = worn.has_flag( json_flag_HIDDEN ) ? _( "Yes" ) : _( "No" ) },
            },
            .color = string_from_color( get_item_penalties( it, model.who, part ).color_for_stacking_badness() ),
            .look = ec::world::item_appearance( worn ),
        } );
    }
    return rows;
}

/// What the focused item is: its figures, then what it is worn as and what it costs in encumbrance.
auto detail_panel( const armor_model &model ) -> ec::component
{
    if( model.listed.empty() ) {
        return { .id = "armor:detail", .body = ec::text_body{ .markup = _( "Nothing to see here!" ) } };
    }
    const auto it = std::next( model.who.worn.begin(), model.listed[model.cursor] );
    const item &worn = **it;
    const auto &part = model.parts[model.tab];
    const auto penalties = get_item_penalties( it, model.who, part );
    auto lines = std::vector<std::string> {};
    if( const auto layer = clothing_layer( worn ); !layer.empty() ) {
        lines.push_back( colorize( layer, c_light_blue ) );
    }
    for( const auto &line : clothing_flags_description( worn ) ) {
        lines.push_back( colorize( line, c_light_blue ) );
    }
    for( const auto &message : { stacking_penalty_message( worn, penalties ), out_of_order_penalty_message( penalties ) } ) {
        if( !message.empty() ) {
            lines.push_back( message );
        }
    }
    const auto stats = []( const std::vector<item_stat> &values ) {
        return values | std::views::transform( stat_of ) | std::ranges::to<std::vector>();
    };
    return ec::panel( { .id = "armor:detail", .title = worn.type_name( 1 ), .child = ec::split( {
            .id = "armor:detail:parts", .children = ec::children_of(
            ec::panel( { .id = "armor:properties", .title = _( "Properties" ), .stats = stats( property_stats( worn, model.who, part ) ) } ),
            ec::panel( { .id = "armor:protection", .title = _( "Protection" ), .stats = stats( protection_stats( worn ) ) } ),
            ec::component{ .id = "armor:notes", .body = ec::text_body{ .markup = join( lines, "\n" ) } } ) } ) } );
}

/// The protection the worn items give a body part together.
auto total_protection( const Character &who,
                       const bodypart_id &part ) -> std::vector<ec::panel_stat>
{
    if( part.id().is_null() ) {
        return {};
    }
    const auto number = []( const float value ) { return string_format( "%g", value ); };
    return {
        { .key = "bash", .label = ec::caption( _( "Bash:" ) ), .value = number( who.get_armor_bash( part ) ) },
        { .key = "cut", .label = ec::caption( _( "Cut:" ) ), .value = number( who.get_armor_cut( part ) ) },
        { .key = "stab", .label = ec::caption( _( "Stab:" ) ), .value = number( std::round( who.get_armor_cut( part ) * 0.8f ) ) },
        { .key = "ballistic", .label = ec::caption( _( "Ballistic:" ) ), .value = number( who.get_armor_bullet( part ) ) },
    };
}

/// The items over each body part, innermost first, and what each adds in encumbrance.
auto layer_rows( const armor_model &model ) -> std::vector<ec::row>
{
    const auto ids = worn_ids( model.who );
    const auto &selected = model.parts[model.tab];
    auto rows = std::vector<ec::row> {};
    for( const bodypart_id cover : listed_body_parts( model.who, model.parts ) ) {
        if( cover.id().is_null() ) {
            continue;
        }
        const auto combined = combines_with_opposite( model.who, cover );
        const auto highlighted = cover == selected || ( combined &&
                                 static_cast<bodypart_id>( cover.obj().opposite_part ) == selected );
        rows.push_back( { .id = "cover:" + part_id( cover ), .label = body_part_name_as_heading( cover, combined ? 2 : 1 ),
                          .enabled = false, .selectable = false, .color = string_from_color( highlighted ? c_yellow : c_white ),
                          .header = true } );
        for( const auto &elem : items_cover_bp( model.who, cover ) ) {
            rows.push_back( {
                .id = "layer:" + part_id( cover ) + ":" + ids[elem.position], .label = remove_color_tags( elem.name ),
                .columns = { {
                        .key = "encumbrance", .label = _( "Encumbrance" ),
                        .value = std::to_string( elem.encumber ) + ( elem.penalties.badness() > 0 ? "+" : "" )
                    }
                },
                .color = string_from_color( elem.penalties.color_for_stacking_badness() ),
            } );
        }
    }
    return rows;
}

/// The encumbrance and warmth of each body part, with the parts the focused item covers marked.
auto encumbrance_rows( const armor_model &model ) -> std::vector<ec::row>
{
    const auto *const focused = model.listed.empty() ? nullptr :
                                *std::next( model.who.worn.begin(), model.listed[model.cursor] );
    using namespace std::views;
    return character_display::encumbrance_lines( model.who,
    focused ) | transform( [&]( const auto & line ) {
        return ec::row{
            .id = "encumbrance:part:" + line.part.str(),
            .label = body_part_name_as_heading( line.part, line.combined ? 2 : 1 ), .selectable = false,
            .columns = {
                { .key = "encumbrance", .label = _( "Encumbrance" ), .value = std::to_string( line.encumbrance ), .color = string_from_color( line.encumbrance_color ) },
                { .key = "layering", .label = _( "Layering" ), .value = std::to_string( line.layering ), .color = string_from_color( line.encumbrance_color ) },
                { .key = "warmth", .label = _( "Warmth" ), .value = std::to_string( line.warmth ), .color = string_from_color( line.warmth_color ) },
            },
            .color = string_from_color( line.covered ? c_green : c_light_gray ),
        };
    } ) | std::ranges::to<std::vector>();
}

auto armor_view( const armor_model &model ) -> ec::screen_view
{
    auto entries = std::vector<ec::tab_entry> {};
    for( const auto &[index, part] : std::views::enumerate( model.parts ) ) {
        entries.push_back( { .id = part_id( part ), .label = part.id() ? body_part_name_as_heading( part, 1 ) : _( "All" ),
                             .selected = static_cast<int>( index ) == model.tab } );
    }
    const auto ids = worn_ids( model.who );
    auto worn_parts = ec::children_of( ec::list( { .id = worn_rows_id, .rows = worn_rows( model ), .selection = "single", .activate = select_item_id } ) );
    if( model.listed.empty() ) {
        worn_parts.push_back( { .id = "armor:worn:none", .body = ec::text_body{ .markup = _( "<empty>" ) } } );
    }
    auto panes = ec::children_of(
                     ec::panel( { .id = "armor:worn", .title = _( "(Innermost)" ), .subtitle = _( "(Outermost)" ),
                                  .stats = total_protection( model.who, model.parts[model.tab] ),
                                  .child = ec::split( { .id = "armor:worn:parts", .children = std::move( worn_parts ) } ) } ),
                     detail_panel( model ),
                     ec::panel( { .id = "armor:encumbrance", .title = _( "Encumbrance and Warmth" ),
                                  .child = ec::list( { .id = "armor:encumbrance:rows", .rows = encumbrance_rows( model ), .selection = "none" } ) } ),
                     ec::panel( { .id = "armor:layers", .title = _( "Encumbrance" ),
                                  .child = ec::list( { .id = "armor:layers:rows", .rows = layer_rows( model ), .selection = "none" } ) } ) );
    return {
        .title = _( "Sort Armor" ),
        .root = ec::tabs( {
            .id = tabs_id, .tabs = std::move( entries ), .select = select_part_id,
            .child = ec::split( { .id = "armor:panes", .active = "armor:worn", .children = std::move( panes ) } ) } ),
        .affordances = {
            ec::invoke_affordance( { .id = select_part_id, .label = _( "Category" ), .args = { .row = ec::row_shape{ .component_id = tabs_id } } } ),
            ec::invoke_affordance( { .id = select_item_id, .label = _( "Select" ), .args = { .row = ec::row_shape{ .component_id = worn_rows_id } }, .role = "select" } ),
        },
        .focus_component = worn_rows_id,
.focus_row = model.listed.empty() ? std::string{} :
        ids[model.listed[model.cursor]],
    };
}
} // namespace

static void draw_grid( const catacurses::window &w, int left_pane_w, int mid_pane_w )
{
    const int win_w = getmaxx( w );
    const int win_h = getmaxy( w );

    draw_border( w );
    mvwhline( w, point( 1, 2 ), 0, win_w - 2 );
    mvwvline( w, point( left_pane_w + 1, 3 ), 0, win_h - 4 );
    mvwvline( w, point( left_pane_w + mid_pane_w + 2, 3 ), 0, win_h - 4 );

    // intersections
    mvwputch( w, point( 0, 2 ), BORDER_COLOR, LINE_XXXO );
    mvwputch( w, point( win_w - 1, 2 ), BORDER_COLOR, LINE_XOXX );
    mvwputch( w, point( left_pane_w + 1, 2 ), BORDER_COLOR, LINE_OXXX );
    mvwputch( w, point( left_pane_w + 1, win_h - 1 ), BORDER_COLOR, LINE_XXOX );
    mvwputch( w, point( left_pane_w + mid_pane_w + 2, 2 ), BORDER_COLOR, LINE_OXXX );
    mvwputch( w, point( left_pane_w + mid_pane_w + 2, win_h - 1 ), BORDER_COLOR, LINE_XXOX );

    wnoutrefresh( w );
}

void show_armor_layers_ui( Character &who )
{
    /* Define required height of the right pane:
    * + 3 - horizontal lines;
    * + 1 - caption line;
    * + 2 - innermost/outermost string lines;
    * + num_of_parts - sub-categories (torso, head, eyes, etc.);
    * + 1 - gap;
    * number of lines required for displaying all items is calculated dynamically,
    * because some items can have multiple entries (i.e. cover a few parts of body).
    */

    const auto all_parts = who.get_all_body_parts();
    const int num_of_parts = all_parts.size();

    // FIXME: get_all_body_parts() doesn't return a sorted list
    //        and bodypart_id is not compatible with std::sort()
    //        so let's use a dirty hack
    cata::flat_set<bodypart_id> armor_cat;
    for( const bodypart_id &it : all_parts ) {
        armor_cat.insert( it );
    }
    armor_cat.insert( bodypart_str_id::NULL_ID().id() );

    int req_right_h = 3 + 1 + 2 + num_of_parts + 1;
    for( const bodypart_id &cover : armor_cat ) {
        for( const item * const &elem : who.worn ) {
            if( elem->covers( cover ) ) {
                req_right_h++;
            }
        }
    }

    /* Define required height of the mid pane:
    * + 3 - horizontal lines;
    * + 1 - caption line;
    * + 8 - general properties
    * + 13 - ASSUMPTION: max possible number of flags @ item
    * + num_of_parts+1 - warmth & enc block
    */
    const int req_mid_h = 3 + 1 + 8 + 13 + num_of_parts + 1;

    int win_h = 0;
    int win_w = 0;
    point win;

    int cont_h   = 0;
    int left_w   = 0;
    int right_w  = 0;
    int middle_w = 0;

    int tabindex = 0;
    const int tabcount = num_of_parts + 1;

    int leftListIndex  = 0;
    int leftListOffset = 0;
    int selected       = -1;

    int rightListOffset = 0;

    int leftListLines = 0;
    int rightListLines = 0;

    std::vector<int> tmp_worn;

    auto access_tmp_worn = [&]( int index ) {
        int worn_index = tmp_worn[index];
        location_vector<item>::iterator it = who.worn.begin();
        std::advance( it, worn_index );
        return it;
    };

    // Layout window
    catacurses::window w_sort_armor;
    // Subwindows (between lines)
    catacurses::window w_sort_cat;
    catacurses::window w_sort_left;
    catacurses::window w_sort_middle;
    catacurses::window w_sort_right;
    catacurses::window w_encumb;

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        win_h = std::min( TERMY, std::max( { FULL_SCREEN_HEIGHT, req_right_h, req_mid_h } ) );
        win_w = FULL_SCREEN_WIDTH + ( TERMX - FULL_SCREEN_WIDTH ) * 3 / 4;
        win.x = TERMX / 2 - win_w / 2;
        win.y = TERMY / 2 - win_h / 2;
        cont_h = win_h - 4;
        left_w = ( win_w - 4 ) / 3;
        right_w = left_w;
        middle_w = ( win_w - 4 ) - left_w - right_w;
        leftListLines = rightListLines = cont_h - 2;
        w_sort_armor = catacurses::newwin( win_h, win_w, win );
        w_sort_cat = catacurses::newwin( 1, win_w - 4, win + point( 2, 1 ) );
        w_sort_left = catacurses::newwin( cont_h, left_w, win + point( 1, 3 ) );
        w_sort_middle = catacurses::newwin( cont_h - num_of_parts - 1, middle_w,
                                            win + point( 2 + left_w, 3 ) );
        w_sort_right = catacurses::newwin( cont_h, right_w,
                                           win + point( 3 + left_w + middle_w, 3 ) );
        w_encumb = catacurses::newwin( num_of_parts + 1, middle_w,
                                       win + point( 2 + left_w, -1 + 3 + cont_h - num_of_parts ) );
        ui.position_from_window( w_sort_armor );
    } );
    ui.mark_resize();

    input_context ctxt( "SORT_ARMOR" );
    ctxt.register_cardinal();
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "PREV_TAB" );
    ctxt.register_action( "NEXT_TAB" );
    ctxt.register_action( "MOVE_ARMOR" );
    ctxt.register_action( "CHANGE_SIDE" );
    ctxt.register_action( "TOGGLE_CLOTH" );
    ctxt.register_action( "ASSIGN_INVLETS" );
    ctxt.register_action( "SORT_ARMOR" );
    ctxt.register_action( "EQUIP_ARMOR" );
    ctxt.register_action( "EQUIP_ARMOR_HERE" );
    ctxt.register_action( "REMOVE_ARMOR" );
    ctxt.register_action( "USAGE_HELP" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    auto do_return_entry = []() {
        avatar &you = get_avatar();
        you.assign_activity( ACT_ARMOR_LAYERS, 0 );
        you.activity->auto_resume = true;
        you.activity->moves_left = INT_MAX;
    };

    int leftListSize = 0;
    int rightListSize = 0;

    ui.on_redraw( [&]( ui_adaptor & ui ) {
        draw_grid( w_sort_armor, left_w, middle_w );

        werase( w_sort_cat );
        werase( w_sort_left );
        werase( w_sort_middle );
        werase( w_sort_right );
        werase( w_encumb );

        const bodypart_id &bp = armor_cat[ tabindex ];

        // top bar
        wprintz( w_sort_cat, c_white, _( "Sort Armor" ) );
        const auto name = bp.id() ? body_part_name_as_heading( bp, 1 ) : _( "All" );
        wprintz( w_sort_cat, c_yellow, "  << %s >>", name );
        right_print( w_sort_cat, 0, 0, c_white, string_format(
                         _( "[<color_yellow>%s</color>] Hide sprite.  "
                            "[<color_yellow>%s</color>] Change side.  "
                            "Press [<color_yellow>%s</color>] for help.  "
                            "Press [<color_yellow>%s</color>] to change keybindings." ),
                         ctxt.get_desc( "TOGGLE_CLOTH" ),
                         ctxt.get_desc( "CHANGE_SIDE" ),
                         ctxt.get_desc( "USAGE_HELP" ),
                         ctxt.get_desc( "HELP_KEYBINDINGS" ) ) );

        if( leftListLines > leftListSize ) {
            leftListOffset = 0;
        } else if( leftListOffset + leftListLines > leftListSize ) {
            leftListOffset = leftListSize - leftListLines;
        }
        if( leftListOffset > leftListIndex ) {
            leftListOffset = leftListIndex;
        } else if( leftListOffset + leftListLines <= leftListIndex ) {
            leftListOffset = leftListIndex + 1 - leftListLines;
        }

        // Left header
        mvwprintz( w_sort_left, point_zero, c_light_gray, _( "(Innermost)" ) );
        right_print( w_sort_left, 0, 0, c_light_gray, string_format( _( "Storage (%s)" ),
                     volume_units_abbr() ) );
        // Left list
        const int max_drawindex = std::min( leftListSize - leftListOffset, leftListLines );
        for( int drawindex = 0; drawindex < max_drawindex; drawindex++ ) {
            int itemindex = leftListOffset + drawindex;

            if( itemindex == leftListIndex ) {
                mvwprintz( w_sort_left, point( 0, drawindex + 1 ), c_yellow, ">>" );
            }

            std::string worn_armor_name = ( *access_tmp_worn( itemindex ) )->display_name();
            item_penalties const penalties =
                get_item_penalties( access_tmp_worn( itemindex ), who, bp );

            const int offset_x = ( itemindex == selected ) ? 4 : 3;
            trim_and_print( w_sort_left, point( offset_x, drawindex + 1 ), left_w - offset_x - 3,
                            penalties.color_for_stacking_badness(), worn_armor_name );
            right_print( w_sort_left, drawindex + 1, 0, c_light_gray,
                         format_volume( ( *access_tmp_worn( itemindex ) )->get_storage() ) );
            if( ( *access_tmp_worn( itemindex ) )->has_flag( json_flag_HIDDEN ) ) {
                //~ Hint: Letter to show which piece of armor is Hidden in the layering menu
                mvwprintz( w_sort_left, point( offset_x - 1, drawindex + 1 ), c_cyan, _( "H" ) );
            }
        }

        // Total armor for the given part (Ignoring the "All" condition)
        if( bp.id() ) {
            mvwprintz( w_sort_left, point( 0, cont_h - 6 ), c_white, _( "Total Protection:" ) );
            mvwprintz( w_sort_left, point( 2, cont_h - 5 ), c_light_gray,
                       _( "Bash: " + std::to_string( who.get_armor_bash( bp ) ) ) );
            mvwprintz( w_sort_left, point( 2, cont_h - 4 ), c_light_gray,
                       _( "Cut: " + std::to_string( who.get_armor_cut( bp ) ) ) );
            // Assuming that float armor values are rounded and not floored or truncated
            mvwprintz( w_sort_left, point( 2, cont_h - 3 ), c_light_gray,
                       _( "Stab: " + std::to_string( static_cast<int>( std::round( who.get_armor_cut(
                                   bp ) * 0.8f ) ) ) ) );
            mvwprintz( w_sort_left, point( 2, cont_h - 2 ), c_light_gray,
                       _( "Ballistic: " + std::to_string( who.get_armor_bullet( bp ) ) ) );
        }

        // Left footer
        mvwprintz( w_sort_left, point( 0, cont_h - 1 ), c_light_gray, _( "(Outermost)" ) );
        if( leftListOffset + leftListLines < leftListSize ) {
            // TODO: replace it by right_print()
            mvwprintz( w_sort_left, point( left_w - utf8_width( _( "<more>" ) ), cont_h - 1 ),
                       c_light_blue, _( "<more>" ) );
        }
        if( leftListSize == 0 ) {
            // TODO: replace it by right_print()
            mvwprintz( w_sort_left, point( left_w - utf8_width( _( "<empty>" ) ), cont_h - 1 ),
                       c_light_blue, _( "<empty>" ) );
        }

        // Items stats
        if( leftListSize > 0 ) {
            draw_mid_pane( w_sort_middle, access_tmp_worn( leftListIndex ), who, bp );
        } else {
            // NOLINTNEXTLINE(cata-use-named-point-constants)
            fold_and_print( w_sort_middle, point( 1, 0 ), middle_w - 1, c_white,
                            _( "Nothing to see here!" ) );
        }

        mvwprintz( w_encumb, point_east, c_white, _( "Encumbrance and Warmth" ) );
        character_display::print_encumbrance( ui, w_encumb, who, -1,
                                              ( leftListSize > 0 ) ? *access_tmp_worn( leftListIndex ) : nullptr );

        // Right header
        mvwprintz( w_sort_right, point_zero, c_light_gray, _( "(Innermost)" ) );
        right_print( w_sort_right, 0, 0, c_light_gray, _( "Encumbrance" ) );

        const auto &combine_bp = [&who]( const bodypart_id & cover ) -> bool {
            return combines_with_opposite( who, cover );
        };

        const cata::flat_set<bodypart_id> rl = listed_body_parts( who, armor_cat );

        // Right list
        rightListSize = 0;
        for( const bodypart_id cover : rl ) {
            rightListSize += items_cover_bp( who, cover ).size() + 1;
        }
        if( rightListLines > rightListSize ) {
            rightListOffset = 0;
        } else if( rightListOffset + rightListLines > rightListSize ) {
            rightListOffset = rightListSize - rightListLines;
        }
        int pos = 1, curr = 0;
        for( const bodypart_id cover : rl ) {
            if( cover.id().is_null() ) {
                continue;
            }
            if( curr >= rightListOffset && pos <= rightListLines ) {
                const bool is_highlighted = cover == bp || ( combine_bp( cover ) &&
                                            static_cast<bodypart_id>( cover.obj().opposite_part ) == bp );

                mvwprintz( w_sort_right, point( 1, pos ), ( is_highlighted ? c_yellow : c_white ),
                           "%s:", body_part_name_as_heading( cover, combine_bp( cover ) ? 2 : 1 ) );
                pos++;
            }
            curr++;
            for( layering_item_info &elem : items_cover_bp( who, cover ) ) {
                if( curr >= rightListOffset && pos <= rightListLines ) {
                    nc_color color = elem.penalties.color_for_stacking_badness();
                    trim_and_print( w_sort_right, point( 2, pos ), right_w - 5, color,
                                    elem.name );
                    char plus = elem.penalties.badness() > 0 ? '+' : ' ';
                    mvwprintz( w_sort_right, point( right_w - 4, pos ), c_light_gray, "%3d%c",
                               elem.encumber, plus );
                    pos++;
                }
                curr++;
            }
        }

        // Right footer
        mvwprintz( w_sort_right, point( 0, cont_h - 1 ), c_light_gray, _( "(Outermost)" ) );
        if( rightListOffset + rightListLines < rightListSize ) {
            // TODO: replace it by right_print()
            mvwprintz( w_sort_right, point( right_w - utf8_width( _( "<more>" ) ), cont_h - 1 ), c_light_blue,
                       _( "<more>" ) );
        }
        // F5
        wnoutrefresh( w_sort_cat );
        wnoutrefresh( w_sort_left );
        wnoutrefresh( w_sort_middle );
        wnoutrefresh( w_sort_right );
        wnoutrefresh( w_encumb );
    } );

    const auto model = armor_model{ .who = who, .parts = armor_cat, .tab = tabindex, .listed = tmp_worn, .cursor = leftListIndex, .grabbed = selected };
    const auto screen = ec::screen_scope( ctxt, { .view = [&]( const ec::boundary_state & /*state*/ ) { return armor_view( model ); } } );

    avatar &you = get_avatar();
    bool exit = false;
    while( !exit ) {
        if( who.is_avatar() ) {
            // Totally hoisted this from advanced_inv
            if( you.moves < 0 ) {
                do_return_entry();
                return;
            }
        } else {
            // Player is sorting NPC's armor here
            if( rl_dist( you.bub_pos(), who.bub_pos() ) > 1 ) {
                you.add_msg_if_npc( m_bad, _( "%s is too far to sort armor." ), who.name );
                return;
            }
            if( you.attitude_to( you ) != Attitude::A_FRIENDLY ) {
                you.add_msg_if_npc( m_bad, _( "%s is not friendly!" ), who.name );
                return;
            }
        }

        // Create ptr list of items to display
        tmp_worn.clear();
        const bodypart_id &bp = armor_cat[ tabindex ];
        if( bp.id().is_null() ) {
            // All
            int i = 0;
            for( auto it = who.worn.begin(); it != who.worn.end(); ++it ) {
                tmp_worn.push_back( i++ );
            }
        } else {
            // bp_*
            int i = 0;
            for( auto it = who.worn.begin(); it != who.worn.end(); ++it ) {
                if( ( *it )->covers( bp ) ) {
                    tmp_worn.push_back( i );
                }
                i++;
            }
        }
        leftListSize = tmp_worn.size();

        // Ensure leftListIndex is in bounds
        int new_index_upper_bound = std::max( 0, leftListSize - 1 );
        leftListIndex = std::min( leftListIndex, new_index_upper_bound );

        ui_manager::redraw();
        const auto action = ctxt.handle_input();
        // A client picked a body part or an item: the cursor moves there.
        if( const auto picked = ec::invoked( ctxt.get_raw_input() ); picked && picked->args.row ) {
            if( picked->affordance_id == select_part_id ) {
                const auto found = std::ranges::find( armor_cat, *picked->args.row, part_id );
                if( found != armor_cat.end() ) {
                    tabindex = static_cast<int>( found - armor_cat.begin() );
                    leftListIndex = leftListOffset = 0;
                    selected = -1;
                }
            } else if( picked->affordance_id == select_item_id && selected < 0 ) {
                // A grabbed item moves with the cursor, which only the move actions do.
                const auto ids = worn_ids( who );
                const auto worn = std::ranges::find( ids, *picked->args.row );
                const auto listed = worn == ids.end() ? tmp_worn.end() :
                                    std::ranges::find( tmp_worn, static_cast<int>( worn - ids.begin() ) );
                if( listed != tmp_worn.end() ) {
                    leftListIndex = static_cast<int>( listed - tmp_worn.begin() );
                }
            }
            continue;
        }
        if( who.is_npc() && action == "ASSIGN_INVLETS" ) {
            // It doesn't make sense to assign invlets to NPC items
            continue;
        }

        // Helper function for moving items in the list
        auto shift_selected_item = [&]() {
            if( selected >= 0 ) {
                auto selected_it = access_tmp_worn( selected );
                auto left_it = access_tmp_worn( leftListIndex );

                std::swap( *selected_it, *left_it );

                int temp = tmp_worn[selected];
                tmp_worn[selected] = tmp_worn[leftListIndex];
                tmp_worn[leftListIndex] = temp;
                selected = leftListIndex;
                who.reset_encumbrance();
            }
        };

        if( action == "UP" && leftListSize > 0 ) {
            if( leftListIndex > 0 ) {
                leftListIndex--;
                if( leftListIndex < leftListOffset ) {
                    leftListOffset = leftListIndex;
                }
            } else {
                leftListIndex = leftListSize - 1;
                if( leftListLines >= leftListSize ) {
                    leftListOffset = 0;
                } else {
                    leftListOffset = leftListSize - leftListLines;
                }
            }

            shift_selected_item();
        } else if( action == "DOWN" && leftListSize > 0 ) {
            if( leftListIndex + 1 < leftListSize ) {
                leftListIndex++;
                if( leftListIndex >= leftListOffset + leftListLines ) {
                    leftListOffset = leftListIndex + 1 - leftListLines;
                }
            } else {
                leftListIndex = 0;
                leftListOffset = 0;
            }

            shift_selected_item();
        } else if( action == "LEFT" ) {
            tabindex--;
            if( tabindex < 0 ) {
                tabindex = tabcount - 1;
            }
            leftListIndex = leftListOffset = 0;
            selected = -1;
        } else if( action == "RIGHT" ) {
            tabindex = ( tabindex + 1 ) % tabcount;
            leftListIndex = leftListOffset = 0;
            selected = -1;
        } else if( action == "NEXT_TAB" ) {
            if( rightListOffset + rightListLines < rightListSize ) {
                rightListOffset++;
            }
        } else if( action == "PREV_TAB" ) {
            if( rightListOffset > 0 ) {
                rightListOffset--;
            }
        } else if( action == "MOVE_ARMOR" ) {
            if( selected >= 0 ) {
                selected = -1;
            } else {
                selected = leftListIndex;
            }
        } else if( action == "CHANGE_SIDE" ) {
            if( leftListIndex < leftListSize && ( *access_tmp_worn( leftListIndex ) )->is_sided() ) {
                if( you.query_yn( _( "Swap side for %s?" ),
                                  colorize( ( *access_tmp_worn( leftListIndex ) )->tname(),
                                            ( *access_tmp_worn( leftListIndex ) )->color_in_inventory() ) ) ) {
                    who.change_side( *access_tmp_worn( leftListIndex ) );
                }
            }
        } else if( action == "TOGGLE_CLOTH" && leftListSize > 0 ) {
            if( !( *access_tmp_worn( leftListIndex ) )->has_flag( json_flag_HIDDEN ) ) {
                ( *access_tmp_worn( leftListIndex ) )->set_flag( json_flag_HIDDEN );
            } else {
                ( *access_tmp_worn( leftListIndex ) )->unset_flag( json_flag_HIDDEN );
            }
        } else if( action == "SORT_ARMOR" ) {
            std::stable_sort( who.worn.begin(),
                              who.worn.end(),
            []( item * const & l, item * const & r ) {
                return l->get_layer() < r->get_layer();
            }
                            );
            who.reset_encumbrance();
        } else if( action == "EQUIP_ARMOR" ) {
            // filter inventory for all items that are armor/clothing
            item *loc = game_menus::inv::wear( *who.as_player() );

            // only equip if something valid selected!
            if( loc ) {
                // wear the item
                loc->obtain( who );
                bool equipped = who.as_player()->wear_possessed( *loc );
                if( equipped ) {
                    const bodypart_id &bp = armor_cat[tabindex];
                    if( tabindex == num_of_parts || loc->covers( bp ) ) {
                        // Set ourselves up to be pointing at the new item
                        // TODO: This doesn't work yet because we don't save our
                        // state through other activities, but that's a thing
                        // that would be nice to do.
                        bool found = false;
                        leftListIndex =
                            std::count_if( who.worn.begin(), who.worn.end(),
                        [&]( item * const & i ) {
                            if( i == loc ) {
                                found = true;
                            }
                            return !found && ( tabindex == num_of_parts || i->covers( bp ) );
                        } );
                    }
                } else if( who.is_npc() ) {
                    // TODO: Pass the reason here
                    popup( _( "Can't put this on!" ) );
                }
            }
        } else if( action == "EQUIP_ARMOR_HERE" ) {
            // filter inventory for all items that are armor/clothing
            item *loc = game_menus::inv::wear( *who.as_player() );

            // only equip if something valid selected!
            if( loc ) {
                // wear the item
                loc->obtain( who );
                const std::optional<location_vector<item>::iterator> position = ( leftListSize > 0 ) ?
                        access_tmp_worn( leftListIndex ) : std::optional<location_vector<item>::iterator>( std::nullopt );
                if( !who.as_player()->wear_possessed( *loc, true, position ) &&
                    who.is_npc() ) {
                    // TODO: Pass the reason here
                    popup( _( "Can't put this on!" ) );
                }
            }
        } else if( action == "REMOVE_ARMOR" ) {
            // query (for now)
            if( leftListIndex < leftListSize ) {
                if( you.query_yn( _( "Remove selected armor?" ) ) ) {
                    do_return_entry();
                    // remove the item, asking to drop it if necessary
                    item &to_takeoff = **access_tmp_worn( leftListIndex );
                    who.as_player()->takeoff( to_takeoff );
                    if( !you.has_activity( ACT_ARMOR_LAYERS ) ) {
                        // An activity has been created to take off the item;
                        // we must surrender control until it is done.
                        return;
                    }
                    you.cancel_activity();
                    selected = -1;
                    leftListIndex = std::max( 0, leftListIndex - 1 );
                }
            }
        } else if( action == "ASSIGN_INVLETS" ) {
            assert( who.is_avatar() );
            // prompt first before doing this (yes, yes, more popups...)
            if( query_yn( _( "Reassign invlets for armor?" ) ) ) {
                // Start with last armor (the most unimportant one?)
                auto iiter = inv_chars.rbegin();
                auto witer = who.worn.rbegin();
                while( witer != who.worn.rend() && iiter != inv_chars.rend() ) {
                    const char invlet = *iiter;
                    item &w = **witer;
                    if( invlet == w.invlet ) {
                        ++witer;
                    } else if( who.invlet_to_item( invlet ) != nullptr ) {
                        ++iiter;
                    } else {
                        who.inv_reassign_item( w, invlet );
                        ++witer;
                        ++iiter;
                    }
                }
            }
        } else if( action == "USAGE_HELP" ) {
            popup_getkey(
                _( "Use the [<color_yellow>arrow- or keypad keys</color>] to navigate the left list.\n"
                   "[<color_yellow>%s</color>] to select highlighted armor for reordering.\n"
                   "[<color_yellow>%s</color>] / [<color_yellow>%s</color>] to scroll the right list.\n"
                   "[<color_yellow>%s</color>] to assign special inventory letters to clothing.\n"
                   "[<color_yellow>%s</color>] to change the side on which item is worn.\n"
                   "[<color_yellow>%s</color>] to sort armor into natural layer order.\n"
                   "[<color_yellow>%s</color>] to equip a new item.\n"
                   "[<color_yellow>%s</color>] to equip a new item at the currently selected position.\n"
                   "[<color_yellow>%s</color>] to remove selected armor from oneself.\n"
                   "\n"
                   "\n"
                   "Encumbrance explanation:\n"
                   "\n"
                   "<color_light_gray>The first number is the summed encumbrance from all clothing "
                   "on that bodypart.  The second number is an additional encumbrance penalty "
                   "caused by wearing either multiple items on one of the bodypart's layers or "
                   "wearing items the wrong way (e.g. a shirt over a backpack).  "
                   "The sum of these values is the effective encumbrance value "
                   "your character has for that bodypart.</color>" ),
                ctxt.get_desc( "MOVE_ARMOR" ),
                ctxt.get_desc( "PREV_TAB" ),
                ctxt.get_desc( "NEXT_TAB" ),
                ctxt.get_desc( "ASSIGN_INVLETS" ),
                ctxt.get_desc( "CHANGE_SIDE" ),
                ctxt.get_desc( "TOGGLE_CLOTH" ),
                ctxt.get_desc( "SORT_ARMOR" ),
                ctxt.get_desc( "EQUIP_ARMOR" ),
                ctxt.get_desc( "EQUIP_ARMOR_HERE" ),
                ctxt.get_desc( "REMOVE_ARMOR" )
            );
        } else if( action == "QUIT" ) {
            exit = true;
        }
    }
}
