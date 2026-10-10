#include "engine_client_world.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <set>

#include "avatar.h"
#include "calendar.h"
#include "catacharset.h"
#include "color.h"
#include "creature.h"
#include "game.h"
#include "game_session.h"
#include "item.h"
#include "tileray.h"
#include "units_angle.h"
#include "itype.h"
#include "map/field.h"
#include "map/field_type.h"
#include "map/map.h"
#include "map/mapdata.h"
#include "map/submap.h"
#include "map_memory.h"
#include "map_perception.h"
#include "memory_fast.h"
#include "monster.h"
#include "npc.h"
#include "output.h"
#include "panels_snapshot.h"
#include "panels_utility.h"
#include "profile.h"
#include "trap.h"
#include "translations.h"
#include "type_id.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "vehicle/vpart_range.h"
#include "weather/weather.h"

static const itype_id itype_corpse( "corpse" );
static const trait_id trait_INATTENTIVE( "INATTENTIVE" );
static const flag_id flag_PULPED( "PULPED" );

namespace engine_client::world
{
namespace
{

auto glyph_of( const int symbol ) -> std::string
{
    switch( symbol ) {
        case LINE_XOXO:
            return LINE_XOXO_S;
        case LINE_OXOX:
            return LINE_OXOX_S;
        case LINE_XXOO:
            return LINE_XXOO_S;
        case LINE_OXXO:
            return LINE_OXXO_S;
        case LINE_OOXX:
            return LINE_OOXX_S;
        case LINE_XOOX:
            return LINE_XOOX_S;
        case LINE_XXXO:
            return LINE_XXXO_S;
        case LINE_XXOX:
            return LINE_XXOX_S;
        case LINE_XOXX:
            return LINE_XOXX_S;
        case LINE_OXXX:
            return LINE_OXXX_S;
        case LINE_XXXX:
            return LINE_XXXX_S;
        default:
            return symbol > 0 && symbol < 0x110000 ? utf32_to_utf8( symbol ) : "#";
    }
}

auto color_of( const nc_color &color ) -> std::string { return get_all_colors().get_name( color ); }

auto make_look( const char *kind, const std::string &id, const std::string &glyph,
                const nc_color &color ) -> look
{
    return { .kind = kind, .id = id, .glyph = glyph, .color = color_of( color ) };
}

/// Same bound as the native `looks_like` lookup.
constexpr auto looks_like_limit = std::size_t { 10 };

/// The `looks_like` chain of a data object, nearest first, starting at `first` (the object's own value).
template<typename Id>
auto looks_like_chain( std::string first ) -> std::vector<std::string>
{
    auto chain = std::vector<std::string> {};
    for( auto id = std::move( first ); !id.empty() && chain.size() < looks_like_limit; ) {
        chain.push_back( id );
        const auto next = Id( id );
        id = next.is_valid() ? next->looks_like : std::string{};
    }
    return chain;
}

auto item_chain( const itype_id &first ) -> std::vector<std::string>
{
    auto chain = std::vector<std::string> {};
    for( auto id = first; id.is_valid() && !id.is_empty() && chain.size() < looks_like_limit;
         id = id->looks_like ) {
        chain.push_back( id.str() );
    }
    return chain;
}

auto with_shape( look value, const map_perception::orientation &shape ) -> look
{
    value.subtile = map_perception::multitile_key( shape.subtile );
    value.rotation = shape.rotation;
    return value;
}

/// Vehicle parts turn by the four directions of their facing, as the native tile selection does.
auto vehicle_rotation( const int degrees ) -> int
{
    return 3 - tileray( units::from_degrees( degrees ) ).dir4();
}

auto terrain_look( const ter_id &id, const int symbol,
                   const map_perception::orientation &shape ) -> look
{
    auto result = make_look( "terrain", id.id().str(), glyph_of( symbol ), id->color() );
    result.name = id->name();
    result.looks_like = looks_like_chain<ter_str_id>( id->looks_like );
    return with_shape( std::move( result ), shape );
}

auto furniture_look( const furn_id &id, const map_perception::orientation &shape ) -> look
{
    auto result = make_look( "furniture", id.id().str(), glyph_of( id->symbol() ), id->color() );
    result.name = id->name();
    result.looks_like = looks_like_chain<furn_str_id>( id->looks_like );
    return with_shape( std::move( result ), shape );
}

auto trap_look( const trap &tr, const map_perception::orientation &shape ) -> look
{
    auto result = make_look( "trap", tr.id.str(), glyph_of( tr.sym ), tr.color );
    result.name = tr.name();
    result.looks_like = looks_like_chain<trap_str_id>( tr.looks_like );
    return with_shape( std::move( result ), shape );
}

/// `modifier` and `degrees` are the native part modifier (1 open, 2 broken) and facing.
auto vehicle_part_look( const vpart_info &info, const char modifier, const int degrees ) -> look
{
    auto result = make_look( "vehicle_part", info.id.str(), glyph_of( special_symbol( info.sym ) ),
                             info.color );
    result.name = info.name();
    result.tile = "vp_" + info.id.str();
    for( const auto &id : looks_like_chain<vpart_id>( info.looks_like ) ) { result.looks_like.push_back( "vp_" + id ); }
    result.subtile = map_perception::multitile_key( modifier == 1 ? open_ : modifier == 2 ? broken :
                     0 );
    result.rotation = vehicle_rotation( degrees );
    return result;
}

auto item_look( const item &thing ) -> look
{
    auto result = make_look( "item", thing.typeId().str(), thing.symbol(), thing.color() );
    result.name = thing.type_name();
    const auto *monster = thing.get_mtype();
    if( thing.typeId() == itype_corpse && monster != nullptr ) {
        result.tile = "corpse_" + monster->id.str();
        result.looks_like = item_chain( itype_corpse );
    } else {
        result.looks_like = item_chain( thing.type->looks_like );
    }
    return result;
}

/// Stored memory names data ids only; an id the data no longer knows keeps its id with an empty appearance.
auto remembered_look( const memorized_terrain_tile &stored, const bool overlay ) -> look
{
    const auto &tile = stored.tile;
    const auto shape = map_perception::orientation{ .subtile = stored.subtile, .rotation = stored.rotation };
    if( overlay && tile.starts_with( "vp_" ) ) {
        const auto part = vpart_id( tile.substr( 3 ) );
        if( part.is_valid() ) {
            // Memory keeps the part modifier in the subtile and the facing in degrees.
            return vehicle_part_look( part.obj(),
                                      stored.subtile == open_ ? 1 : stored.subtile == broken ? 2 : 0,
                                      stored.rotation );
        }
    } else if( overlay ) {
        if( const auto furniture = furn_str_id( tile ); furniture.is_valid() ) {
            return furniture_look( furniture.id(), shape );
        }
        if( const auto trap = trap_str_id( tile ); trap.is_valid() ) { return trap_look( trap.obj(), shape ); }
    } else if( const auto terrain = ter_str_id( tile ); terrain.is_valid() ) {
        const auto &id = terrain.id();
        // Walls draw by the lines they connected when last seen, as in the native text view.
        return terrain_look( id, id->has_flag( TFLAG_AUTO_WALL_SYMBOL ) ?
                             map_perception::remembered_wall_symbol( stored, id->symbol() ) : id->symbol(), shape );
    }
    return { .kind = overlay ? "furniture" : "terrain", .id = tile };
}

auto memory_at( const avatar &you, const tripoint_abs_ms &abs ) -> std::optional<memory_layers>
{
    const auto terrain = you.get_terrain_tile( abs );
    const auto overlay = you.get_memorized_tile( abs );
    if( terrain.tile.empty() && overlay.tile.empty() ) { return std::nullopt; }
    // Without a terrain layer only the stored glyph is known.
    auto result = memory_layers{ .terrain = terrain.tile.empty() ?
                                            look{ .kind = "terrain", .glyph = glyph_of( you.get_memorized_symbol( abs ) ) } :
                                            remembered_look( terrain, false ) };
    if( !overlay.tile.empty() ) { result.overlay = remembered_look( overlay, true ); }
    return result;
}

auto hex_of( const RGBColor &color ) -> std::string
{
    if( color == RGBColor{} ) { return {}; }
    return string_format( "#%02x%02x%02x", color.r, color.g, color.b );
}

auto vehicle_part_at( const map &here, const tripoint_bub_ms &p, const avatar &you )
-> std::optional<look>
{
    const auto vp = here.veh_at( p );
    if( !vp ) { return std::nullopt; }
    const auto roof = p.z() < you.bub_pos().z();
    const auto &veh = vp->vehicle();
    auto modifier = char{};
    const auto &part = veh.part_id_string( vp->part_index(), roof, modifier );
    const auto facing = veh.part_display_direction( vp->part_index(), roof );
    auto result = part.is_valid() ?
                  vehicle_part_look( part.obj(), modifier, static_cast<int>( std::round( to_degrees( facing ) ) ) ) :
                  look{ .kind = "vehicle_part", .id = part.str() };
    result.glyph = glyph_of( special_symbol( veh.part_sym( vp->part_index(), roof ) ) );
    result.color = color_of( veh.part_color( vp->part_index(), roof ) );
    // The native view paints the displayed part, or the roof over it, with the part's own colors.
    const vehicle_part *painted = nullptr;
    if( roof ) {
        if( const auto index = veh.roof_at_part( vp->part_index() ); index != -1 ) { painted = &veh.cpart( index ); }
    } else if( const auto shown = vp.part_displayed() ) {
        painted = &shown->part();
    }
    if( painted != nullptr ) {
        const auto [bg, fg] = painted->get_color();
        if( !hex_of( bg ).empty() || !hex_of( fg ).empty() ) {
            result.tint = look_tint{ .bg = hex_of( bg ), .fg = hex_of( fg ) };
        }
    }
    return result;
}

auto position_of( const map &here, const tripoint_bub_ms &p,
                  const std::string &dimension ) -> position
{
    const auto abs = map_local_to_abs( here, p );
    return { .dim = dimension, .x = abs.x(), .y = abs.y(), .z = abs.z() };
}

/// Fill the facts of a currently visible cell. False when it is plain terrain the engine never memorizes.
auto fill_visible( map &here, const tripoint_bub_ms &p, const avatar &you,
                   const map_perception::sight &view, cell &out ) -> bool
{
    const auto shapes = map_perception::visible_orientations( here, p, view );
    if( here.furn( p ) != f_null ) { out.furniture = furniture_look( here.furn( p ), shapes.furniture ); }
    const auto &displayed = here.field_at( p ).displayed_field_type();
    for( const auto &[type, entry] : here.field_at( p ) ) {
        if( !entry.is_field_alive() ) { continue; }
        auto appearance = make_look( "field", type.id().str(), entry.symbol(), entry.color() );
        appearance.looks_like = looks_like_chain<field_type_str_id>( type->looks_like );
        appearance.name = type->get_name( std::max( entry.get_field_intensity() - 1, 0 ) );
        // Only the displayed field is drawn, joined to neighbours showing the same field.
        if( type == displayed ) {
            auto mask = uint8_t{ 0 };
            for( const auto i : std::views::iota( 0, 4 ) ) {
                const auto neighbour = p + std::array{ point_south, point_east, point_west, point_north } [i];
                if( here.inbounds( neighbour ) && here.field_at( neighbour ).displayed_field_type() == displayed ) {
                    mask |= 1 << i;
                }
            }
            appearance = with_shape( std::move( appearance ), map_perception::orient( mask ) );
        }
        out.fields.push_back( { .appearance = std::move( appearance ), .intensity = entry.get_field_intensity() } );
    }
    // Draw order: the displayed field is the one the native view draws, so it comes last.
    std::ranges::stable_partition( out.fields, [&]( const field_entry & entry ) {
        return entry.appearance.id != displayed.id().str();
    } );
    if( const auto &tr = here.tr_at( p ); !tr.is_null() && tr.can_see( p, you ) ) {
        out.traps.push_back( trap_look( tr, shapes.trap ) );
    }
    if( here.sees_some_items( p, you ) ) {
        // The native view draws the uppermost item, so it comes last.
        const auto &top = here.maptile_at( p ).get_uppermost_item();
        auto seen = std::set<std::string> { top.typeId().str() };
        for( const auto *thing : here.i_at( p ) ) {
            if( thing != nullptr && seen.insert( thing->typeId().str() ).second ) {
                out.items.push_back( item_look( *thing ) );
            }
        }
        out.items.push_back( item_look( top ) );
        if( const auto count = here.maptile_at( p ).get_item_count(); count > 1 ) {
            out.items.back().stack = static_cast<int>( count );
        }
    }
    out.vehicle = vehicle_part_at( here, p, you );
    if( here.could_see_items( p, you ) ) {
        out.reviving = std::ranges::any_of( here.i_at( p ), []( const item * thing ) {
            return thing != nullptr && thing->is_corpse() && ( thing->can_revive() ||
                    ( thing->get_mtype()->zombify_into && !thing->has_flag( flag_PULPED ) ) );
        } );
    }
    if( here.ter( p )->has_flag( TFLAG_NO_MEMORY ) && !out.furniture && out.fields.empty() &&
        out.items.empty() && !out.vehicle ) {
        return false;
    }
    out.terrain = terrain_look( here.ter( p ), map_perception::connected_wall_symbol( here, p ),
                                shapes.terrain );
    const auto &cache = here.access_cache( p.z() );
    out.light = static_cast<int>( cache.visibility_cache[cache.idx( p.x(), p.y() )] );
    return true;
}

/// Cells the avatar knows by memory or by a vehicle it is entitled to perceive; nothing else costs time.
auto capture_known( map &here, const avatar &you, const std::string &dimension,
                    std::map<position, cell> &cells ) -> void
{
    ZoneScopedN( "engine_client_capture_known" );
    const auto view = map_perception::current_sight();
    {
        ZoneScopedN( "engine_client_capture_visible" );
        for( const auto &p : map_perception::visible_cells( here ) ) {
            auto out = cell{ .at = position_of( here, p, dimension ) };
            if( fill_visible( here, p, you, view, out ) ) { cells.emplace( out.at, std::move( out ) ); }
        }
    }
    ZoneNamedN( remembered_zone, "engine_client_capture_remembered", true );
    const auto remember = [&]( const tripoint_bub_ms & p ) {
        if( !here.inbounds( p ) || map_perception::visible_at( here, p ) ) { return; }
        const auto at = position_of( here, p, dimension );
        if( cells.contains( at ) ) { return; }
        auto out = cell{ .at = at, .known = knowledge::remembered,
                         .memory = memory_at( you, map_local_to_abs( here, p ) ) };
        // A vehicle the avatar is entitled to know about is perceived live, so its square is a visible one;
        // the ground beneath it is not perceived.
        if( map_perception::detailed_at( here, p ) ) {
            out.vehicle = vehicle_part_at( here, p, you );
            if( out.vehicle ) { out.known = knowledge::visible; }
        }
        if( out.memory || out.vehicle ) { cells.emplace( at, std::move( out ) ); }
    };
    for( const auto &abs : you.memorized_positions() ) { remember( abs_to_map_local( here, abs ) ); }
    for( const auto &wrapped : here.get_vehicles() ) {
        for( const auto &part : wrapped.v->get_all_parts() ) { remember( part.pos() ); }
    }
}

/// Opaque IDs for creatures the avatar has been shown. The weak pointer pins the creature's control
/// block, so an address is never mistaken for a new creature, and an expired entry means it is gone.
class entity_ids
{
    public:
        auto id_for( const Creature &critter ) -> std::string {
            const auto owner = g->shared_from( critter );
            const auto found = std::ranges::find_if( entries_, [&]( const entry & e ) {
                return !e.owner.owner_before( owner ) && !owner.owner_before( e.owner );
            } );
            if( found != entries_.end() ) { return found->id; }
            entries_.push_back( { owner, "e:" + std::to_string( next_++ ) } );
            return entries_.back().id;
        }
        auto forget_expired() -> void {
            std::erase_if( entries_, []( const entry & e ) { return e.owner.expired(); } );
        }
    private:
        struct entry {
            weak_ptr_fast<Creature> owner;
            std::string id;
        };
        std::vector<entry> entries_;
        long next_ = 1;
};

auto ids = entity_ids {};

/// Creatures face left or right; characters draw as their gender's sprite.
auto creature_look( const Creature &critter, const char *kind, const std::string &id ) -> look
{
    auto result = make_look( kind, id, critter.symbol(), critter.basic_symbol_color() );
    if( const auto *character = critter.as_character() ) {
        result.tile = std::string( character->is_npc() ? "npc_" : "player_" ) +
                      ( character->male ? "male" : "female" );
    }
    if( critter.facing == FD_LEFT ) { result.facing = "left"; }
    if( critter.facing == FD_RIGHT ) { result.facing = "right"; }
    return result;
}

/// The sprite ids a tileset may know an overlay by, in the order of the native overlay lookup: the
/// gendered and the plain id of the overlay, then those of what its item or mutation looks like.
auto overlay_look( const std::string &overlay, const bool male ) -> look
{
    auto looks_like = overlay;
    auto type = std::string{};
    for( const auto *prefix : { "worn_", "wielded_" } ) {
        if( overlay.starts_with( prefix ) ) {
            type = prefix;
            looks_like = overlay.substr( type.size() );
            break;
        }
    }
    auto candidates = std::vector<std::string> {};
    for( auto step = std::size_t{}; step < looks_like_limit && !looks_like.empty(); ++step ) {
        candidates.push_back( std::string( male ? "overlay_male_" : "overlay_female_" ) + type +
                              looks_like );
        candidates.push_back( "overlay_" + type + looks_like );
        if( looks_like.starts_with( "mutation_active_" ) ) {
            looks_like = "mutation_" + looks_like.substr( std::string( "mutation_active_" ).size() );
            continue;
        }
        const auto item = itype_id( looks_like );
        if( !item.is_valid() ) { break; }
        looks_like = item->looks_like.str();
    }
    auto result = look{ .kind = "overlay", .id = overlay, .tile = candidates.front() };
    result.looks_like.assign( std::next( candidates.begin() ), candidates.end() );
    return result;
}

auto overlays_of( const Creature &critter ) -> std::vector<look>
{
    const auto *character = critter.as_character();
    if( character == nullptr ) { return {}; }
    return character->get_overlay_ids() | std::views::transform( [&]( const auto & entry ) {
        return overlay_look( entry.id, character->male );
    } ) | std::ranges::to<std::vector>();
}

auto capture_entities( const map &here, const avatar &you, const std::string &dimension )
-> std::map<std::string, entity>
{
    auto entities = std::map<std::string, entity> {};
    ids.forget_expired();
    for( const auto *critter : g->get_creatures_if( []( const Creature & ) { return true; } ) ) {
        if( critter == &you || !you.sees( *critter ) ) { continue; }
        const auto *monster = critter->as_monster();
        const auto *person = critter->as_npc();
        if( monster == nullptr && person == nullptr ) { continue; }
        const auto type = monster != nullptr ? monster->type->id.str() : person->myclass.str();
        auto id = ids.id_for( *critter );
        auto appearance = creature_look( *critter, monster != nullptr ? "monster" : "npc", type );
        if( monster != nullptr ) {
            appearance.looks_like = looks_like_chain<mtype_id>( monster->type->looks_like );
        }
        entities.emplace( id, entity{
            .id = id,
            .at = position_of( here, critter->bub_pos(), dimension ),
            .appearance = std::move( appearance ),
            .name = critter->get_name(),
            .overlays = overlays_of( *critter ),
            .attitude = Creature::attitude_raw_string( critter->attitude_to( you ) ),
            .aware = critter->sees( you ) && !you.has_trait( trait_INATTENTIVE ) } );
    }
    return entities;
}

auto season_name() -> std::string
{
    switch( season_of_year( calendar::turn ) ) {
        case SPRING:
            return "spring";
        case SUMMER:
            return "summer";
        case AUTUMN:
            return "autumn";
        case WINTER:
            return "winter";
        default:
            return "";
    }
}

/// One inventory row for a stack of equal items, named the way the native lists show it.
auto stack_entry( const item &thing, const std::size_t count,
                  std::optional<std::string> slot = std::nullopt ) -> inventory_entry
{
    return { .appearance = item_look( thing ), .name = remove_color_tags( thing.display_name() ), .count = count,
             .slot = std::move( slot ) };
}

auto capture_avatar( map &here, const avatar &you,
                     const std::string &dimension ) -> avatar_value
{
    auto result = avatar_value{
        .id = "e:avatar", .at = position_of( here, you.bub_pos(), dimension ), .name = you.name,
        .appearance = creature_look( you, "avatar", "avatar" ),
        .overlays = overlays_of( you ),
        .stats = {
            { "strength", _( "Strength" ), std::to_string( you.get_str() ), color_of( color_compare_base( you.get_str_base(), you.get_str() ) ) },
            { "dexterity", _( "Dexterity" ), std::to_string( you.get_dex() ), color_of( color_compare_base( you.get_dex_base(), you.get_dex() ) ) },
            { "intelligence", _( "Intelligence" ), std::to_string( you.get_int() ), color_of( color_compare_base( you.get_int_base(), you.get_int() ) ) },
            { "perception", _( "Perception" ), std::to_string( you.get_per() ), color_of( color_compare_base( you.get_per_base(), you.get_per() ) ) },
            { "moves", _( "Moves" ), std::to_string( you.get_moves() ) },
            { "pain", _( "Pain" ), std::to_string( you.get_pain() ) }
        },
        .sidebar = sidebar_snapshot( you ) };
    you.visit_items( [&]( const item * thing ) {
        if( you.is_wielding( *thing ) ) { result.inventory.push_back( stack_entry( *thing, 1, "wielded" ) ); }
        else if( you.is_worn( *thing ) ) { result.inventory.push_back( stack_entry( *thing, 1, "worn" ) ); }
        return VisitResponse::NEXT;
    } );
    for( const auto *stack : you.inv_const_slice() ) {
        if( stack != nullptr ) { result.inventory.push_back( stack_entry( *stack->front(), stack->size(), "carried" ) ); }
    }
    if( here.accessible_items( you.bub_pos() ) ) {
        // The native pickup stacks equal items the same way; each stack is one choice there.
        for( const auto *thing : here.i_at( you.bub_pos() ) ) {
            const auto same = std::ranges::find_if( result.ground, [&]( const auto & entry ) {
                return entry.name == remove_color_tags( thing->display_name() ) &&
                       entry.appearance.id == thing->typeId().str();
            } );
            if( same != result.ground.end() ) { ++*same->count; }
            else { result.ground.push_back( stack_entry( *thing, 1 ) ); }
        }
    }
    return result;
}

} // namespace

auto position_at( const tripoint_bub_ms &p ) -> position
{
    const auto &here = get_map();
    return position_of( here, p, here.get_bound_dimension().str() );
}

auto color_name( const nc_color &color ) -> std::string
{
    return color_of( color );
}

auto capture_world() -> world_state
{
    ZoneScopedN( "engine_client_capture_world" );
    if( !g || !game_session::running() ) { return {}; }
    auto &here = get_map();
    const auto &you = get_avatar();
    const auto dimension = here.get_bound_dimension().str();
    const auto size = here.getmapsize();
    auto state = world_state{
        .coverage = bounds{
            .min = position_of( here, tripoint_bub_ms( 0, 0, -OVERMAP_DEPTH ), dimension ),
            .max = position_of( here, tripoint_bub_ms( size * SEEX - 1, size * SEEY - 1, OVERMAP_HEIGHT ),
                                dimension ) },
        .entities = capture_entities( here, you, dimension ),
        .avatar = capture_avatar( here, you, dimension ),
        .environment = environment_value{
            .turn = std::to_string( to_turn<int>( calendar::turn ) ), .time = to_string( calendar::turn ),
            .weather = get_weather().weather_id.str(), .season = season_name() } };
    // The terrain window moves with the view, not the world: its squares are where a click lands.
    if( const auto window = g->click_window() ) {
        const auto z = g->get_levz();
        state.view = bounds{
            .min = position_of( here, tripoint_bub_ms( window->p_min.x(), window->p_min.y(), z ), dimension ),
            .max = position_of( here, tripoint_bub_ms( window->p_max.x() - 1, window->p_max.y() - 1, z ),
                                dimension ) };
    }
    capture_known( here, you, dimension, state.cells );
    for( const auto &step : g->get_destination_preview() ) {
        state.route.push_back( position_of( here, step, dimension ) );
    }
    return state;
}

} // namespace engine_client::world
