#include "engine_client_world.h"

#include <algorithm>
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
#include "itype.h"
#include "map/field.h"
#include "map/field_type.h"
#include "map/map.h"
#include "map/mapdata.h"
#include "map_memory.h"
#include "map_perception.h"
#include "memory_fast.h"
#include "monster.h"
#include "npc.h"
#include "output.h"
#include "trap.h"
#include "translations.h"
#include "type_id.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vpart_position.h"
#include "vehicle/vpart_range.h"
#include "weather/weather.h"

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

auto terrain_look( const ter_id &id ) -> look
{
    return make_look( "terrain", id.id().str(), glyph_of( id->symbol() ), id->color() );
}

auto furniture_look( const furn_id &id ) -> look
{
    return make_look( "furniture", id.id().str(), glyph_of( id->symbol() ), id->color() );
}

auto trap_look( const trap &tr ) -> look
{
    return make_look( "trap", tr.id.str(), glyph_of( tr.sym ), tr.color );
}

auto vehicle_part_look( const vpart_info &info ) -> look
{
    return make_look( "vehicle_part", info.id.str(), glyph_of( special_symbol( info.sym ) ),
                      info.color );
}

auto item_look( const item &thing ) -> look
{
    return make_look( "item", thing.typeId().str(), thing.symbol(), thing.color() );
}

/// Stored memory names data ids only; an id the data no longer knows keeps its id with an empty appearance.
auto remembered_look( const std::string &tile, const bool overlay ) -> look
{
    if( overlay && tile.starts_with( "vp_" ) ) {
        const auto part = vpart_id( tile.substr( 3 ) );
        if( part.is_valid() ) { return vehicle_part_look( part.obj() ); }
    } else if( overlay ) {
        if( const auto furniture = furn_str_id( tile ); furniture.is_valid() ) {
            return furniture_look( furniture.id() );
        }
        if( const auto trap = trap_str_id( tile ); trap.is_valid() ) { return trap_look( trap.obj() ); }
    } else if( const auto terrain = ter_str_id( tile ); terrain.is_valid() ) {
        return terrain_look( terrain.id() );
    }
    return { .kind = overlay ? "furniture" : "terrain", .id = tile };
}

auto memory_at( const avatar &you, const tripoint_abs_ms &abs ) -> std::optional<memory_layers>
{
    const auto terrain = you.get_terrain_tile( abs ).tile;
    const auto overlay = you.get_memorized_tile( abs ).tile;
    if( terrain.empty() && overlay.empty() ) { return std::nullopt; }
    // Without a terrain layer only the stored glyph is known.
    auto result = memory_layers{ .terrain = terrain.empty() ?
                                            look{ .kind = "terrain", .glyph = glyph_of( you.get_memorized_symbol( abs ) ) } :
                                            remembered_look( terrain, false ) };
    if( !overlay.empty() ) { result.overlay = remembered_look( overlay, true ); }
    return result;
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
    return make_look( "vehicle_part", part.str(),
                      glyph_of( special_symbol( veh.part_sym( vp->part_index(), roof ) ) ),
                      veh.part_color( vp->part_index(), roof ) );
}

auto position_of( const map &here, const tripoint_bub_ms &p,
                  const std::string &dimension ) -> position
{
    const auto abs = map_local_to_abs( here, p );
    return { .dim = dimension, .x = abs.x(), .y = abs.y(), .z = abs.z() };
}

/// Fill the facts of a currently visible cell. False when it is plain terrain the engine never memorizes.
auto fill_visible( map &here, const tripoint_bub_ms &p, const avatar &you, cell &out ) -> bool
{
    if( here.furn( p ) != f_null ) { out.furniture = furniture_look( here.furn( p ) ); }
    for( const auto &[type, entry] : here.field_at( p ) ) {
        if( !entry.is_field_alive() ) { continue; }
        out.fields.push_back( { .appearance = make_look( "field", type.id().str(), entry.symbol(),
                                              entry.color() ),
                                .intensity = entry.get_field_intensity() } );
    }
    if( const auto &tr = here.tr_at( p ); !tr.is_null() && tr.can_see( p, you ) ) {
        out.traps.push_back( trap_look( tr ) );
    }
    if( here.sees_some_items( p, you ) ) {
        auto seen = std::set<std::string> {};
        for( const auto *thing : here.i_at( p ) ) {
            if( thing != nullptr && seen.insert( thing->typeId().str() ).second ) {
                out.items.push_back( item_look( *thing ) );
            }
        }
    }
    out.vehicle = vehicle_part_at( here, p, you );
    if( here.ter( p )->has_flag( TFLAG_NO_MEMORY ) && !out.furniture && out.fields.empty() &&
        out.items.empty() && !out.vehicle ) {
        return false;
    }
    out.terrain = terrain_look( here.ter( p ) );
    return true;
}

/// Cells the avatar knows by memory or by a vehicle it is entitled to perceive; nothing else costs time.
auto capture_known( map &here, const avatar &you, const std::string &dimension,
                    std::map<position, cell> &cells ) -> void
{
    for( const auto &p : map_perception::visible_cells( here ) ) {
        auto out = cell{ .at = position_of( here, p, dimension ) };
        if( fill_visible( here, p, you, out ) ) { cells.emplace( out.at, std::move( out ) ); }
    }
    const auto remember = [&]( const tripoint_bub_ms & p ) {
        if( !here.inbounds( p ) || map_perception::visible_at( here, p ) ) { return; }
        const auto at = position_of( here, p, dimension );
        if( cells.contains( at ) ) { return; }
        auto out = cell{ .at = at, .known = knowledge::remembered,
                         .memory = memory_at( you, map_local_to_abs( here, p ) ) };
        // A vehicle the avatar is entitled to know about is perceived live, the ground beneath it is not.
        if( map_perception::detailed_at( here, p ) ) { out.vehicle = vehicle_part_at( here, p, you ); }
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
        entities.emplace( id, entity{
            .id = id,
            .at = position_of( here, critter->bub_pos(), dimension ),
            .appearance = look{
                .kind = monster != nullptr ? "monster" : "npc", .id = type, .glyph = critter->symbol(),
                .color = color_of( critter->basic_symbol_color() )
            },
            .name = critter->get_name() } );
    }
    return entities;
}

auto capture_avatar( const map &here, const avatar &you,
                     const std::string &dimension ) -> avatar_value
{
    auto result = avatar_value{
        .id = "e:avatar", .at = position_of( here, you.bub_pos(), dimension ), .name = you.name,
        .stats = {
            { "strength", _( "Strength" ), std::to_string( you.get_str() ) },
            { "dexterity", _( "Dexterity" ), std::to_string( you.get_dex() ) },
            { "intelligence", _( "Intelligence" ), std::to_string( you.get_int() ) },
            { "perception", _( "Perception" ), std::to_string( you.get_per() ) },
            { "moves", _( "Moves" ), std::to_string( you.get_moves() ) },
            { "pain", _( "Pain" ), std::to_string( you.get_pain() ) }
        } };
    for( const auto *stack : you.inv_const_slice() ) {
        if( stack == nullptr ) { continue; }
        for( const auto *thing : *stack ) {
            if( thing != nullptr ) {
                result.inventory.push_back( { .appearance = item_look( *thing ), .name = thing->type->nname( 1 ),
                                              .count = static_cast<std::uint64_t>( thing->count() ) } );
            }
        }
    }
    return result;
}

} // namespace

auto capture_world() -> world_state
{
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
            .weather = get_weather().weather_id.str() } };
    capture_known( here, you, dimension, state.cells );
    for( const auto &step : g->get_destination_preview() ) {
        state.route.push_back( position_of( here, step, dimension ) );
    }
    return state;
}

} // namespace engine_client::world
