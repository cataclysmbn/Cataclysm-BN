#include "engine_client_world.h"
#include "engine_client_part_type.h"
#include "engine_client_vehicle_look.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iterator>
#include <ranges>
#include <set>
#include <tuple>
#include <utility>

#include "avatar.h"
#include "cached_options.h"
#include "calendar.h"
#include "catacharset.h"
#include "color.h"
#include "creature.h"
#include "game.h"
#include "game_session.h"
#include "inventory_ui.h"
#include "language.h"
#include "line.h"
#include "bionics.h"
#include "effect.h"
#include "engine_client_event.h"
#include "engine_client_mode.h"
#include "enum_conversions.h"
#include "item.h"
#include "item_serial.h"
#include "tileray.h"
#include "units_angle.h"
#include "itype.h"
#include "client_render_hooks.h"
#include "mutation.h"
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
#include "player_activity.h"
#include "panels_snapshot.h"
#include "panels_utility.h"
#include "profile.h"
#include "string_utils.h"
#include "trap.h"
#include "translations.h"
#include "type_id.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "vehicle/vpart_range.h"
#include "weather/weather.h"
#include "weather/weather_type.h"

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

/// The ids of the tints the client's tileset has, from its hello: only keys among them are published.
auto tint_ids() -> std::set<std::string> &
{
    static auto ids = std::set<std::string> {};
    return ids;
}
/// Changes with the tints, which every look carries the keys of.
auto tint_generation() -> std::size_t & // *NOPAD*
{
    static auto generation = std::size_t{};
    return generation;
}

auto flag_text( const std::string &flag ) -> const std::string & { return flag; } // *NOPAD*
auto flag_text( const flag_id &flag ) -> const std::string & { return flag.str(); } // *NOPAD*

/// What a tileset's tints are tried by for a thing, in the native order, kept to those the client has:
/// its flags, then its id.
template<typename Flags>
auto tint_keys_of( const Flags &flags, const std::string &id ) -> std::vector<std::string>
{
    auto keys = std::vector<std::string> {};
    for( const auto &flag : flags ) {
        if( tint_ids().contains( flag_text( flag ) ) ) { keys.push_back( flag_text( flag ) ); }
    }
    if( tint_ids().contains( id ) ) { keys.push_back( id ); }
    return keys;
}

/// One layer of the tint an item's variables ask for: the variable of the layer, else the shared one.
auto item_tint_layer( const data_vars::data_set &vars, const std::string &layer ) -> look_tint_layer
{
    auto result = look_tint_layer{};
    const auto var = [&]( const char *name ) { return std::string( "tint_" ) + name; };
    const auto of_layer = [&]( const char *name ) { return var( name ) + "_" + layer; };
    auto color = RGBColor{};
    if( vars.try_get( of_layer( "color" ), color ) || vars.try_get( var( "color" ), color ) ) {
        result.color = string_format( "#%02x%02x%02x", color.r, color.g, color.b );
    }
    auto text = std::string{};
    if( vars.try_get( of_layer( "blend_mode" ), text ) || vars.try_get( var( "blend_mode" ), text ) ) {
        result.blend_mode = text;
    }
    const auto number = [&]( const char *name ) -> std::optional<float> {
        auto value = 0.0f;
        if( vars.try_get( of_layer( name ), value ) || vars.try_get( var( name ), value ) ) { return value; }
        return std::nullopt;
    };
    result.contrast = number( "contrast" );
    result.saturation = number( "saturation" );
    result.brightness = number( "brightness" );
    return result;
}

/// What a thing's own variables ask for of its tint, over the tileset's: nothing when they set none.
auto tint_vars_of( const data_vars::data_set &vars ) -> std::optional<look_tint_vars>
{
    if( !std::ranges::any_of( vars, []( const auto & entry ) { return entry.first.starts_with( "tint_" ); } ) ) {
        return std::nullopt;
    }
    auto tint = look_tint_vars{ .bg = item_tint_layer( vars, "bg" ), .fg = item_tint_layer( vars, "fg" ) };
    if( tint.bg.empty() && tint.fg.empty() ) { return std::nullopt; }
    return tint;
}

/// What a tileset tints the sprite of an item by: its flags and id, and the item's own variables.
auto with_item_tint( look result, const item &thing ) -> look
{
    result.tint_keys = tint_keys_of( thing.get_flags(), thing.typeId().str() );
    result.tint_vars = tint_vars_of( thing.item_vars() );
    return result;
}

/// Whether the data flags it as a plant: a shrub, a flower, a plant or a sapling.
auto is_plant( const map_data_common_t &data ) -> bool
{
    return data.has_flag( TFLAG_SHRUB ) || data.has_flag( TFLAG_FLOWER ) ||
           data.has_flag( TFLAG_PLANT ) || data.has_flag( TFLAG_YOUNG );
}

/// What terrain stands as: a door, closed, open or locked; a fence; a plant; none for anything else.
auto terrain_form( const ter_t &ter ) -> std::string
{
    if( !ter.has_flag( "WINDOW" ) &&
        ( ter.has_flag( "DOOR" ) || ter.has_flag( "LOCKED" ) || !ter.close.is_null() ) ) { return "door"; }
    if( is_plant( ter ) ) { return "plant"; }
    if( ter.has_flag( TFLAG_THIN_OBSTACLE ) ) { return "fence"; }
    return {};
}

/// What furniture stands as: a plant, a bed (a seat as comfortable to sleep on as a bed), a seat, a table; none for
/// anything else.
auto furniture_form( const furn_t &furn ) -> std::string
{
    constexpr auto bed_comfort = 5;
    if( is_plant( furn ) ) { return "plant"; }
    if( furn.has_flag( "CAN_SIT" ) ) { return furn.comfort >= bed_comfort ? "bed" : "seat"; }
    if( furn.has_flag( "FLAT_SURF" ) ) { return "table"; }
    return {};
}

auto terrain_look( const ter_id &id, const int symbol, const map_perception::orientation &shape,
                   const data_vars::data_set *vars = nullptr ) -> look
{
    auto result = make_look( "terrain", id.id().str(), glyph_of( symbol ), id->color() );
    result.name = id->name();
    result.looks_like = looks_like_chain<ter_str_id>( id->looks_like );
    result.tint_keys = tint_keys_of( id->get_flags(), id.id().str() );
    result.wall = id->has_flag( TFLAG_WALL ) || ( id->has_flag( TFLAG_CONNECT_TO_WALL ) &&
                  !id->has_flag( TFLAG_FLAT ) );
    result.window = id->has_flag( "WINDOW" );
    result.coverage = std::clamp( id->coverage, 0, 100 );
    result.form = terrain_form( *id );
    if( vars != nullptr ) { result.tint_vars = tint_vars_of( *vars ); }
    return with_shape( std::move( result ), shape );
}

auto furniture_look( const furn_id &id, const map_perception::orientation &shape,
                     const data_vars::data_set *vars = nullptr ) -> look
{
    auto result = make_look( "furniture", id.id().str(), glyph_of( id->symbol() ), id->color() );
    result.name = id->name();
    result.looks_like = looks_like_chain<furn_str_id>( id->looks_like );
    result.tint_keys = tint_keys_of( id->get_flags(), id.id().str() );
    result.coverage = std::clamp( id->coverage, 0, 100 );
    result.form = furniture_form( *id );
    if( vars != nullptr ) { result.tint_vars = tint_vars_of( *vars ); }
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
    return with_item_tint( std::move( result ), thing );
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

/// The level the native view draws: the avatar's, moved by the look cursor or another cursor that changed level, as
/// `cata_tiles::draw` draws from its center.
auto shown_level( const avatar &you ) -> int { return you.bub_pos().z() + you.view_offset.z(); }

/// What part `index` of `veh` shows, or the roof over it, facing `degrees`, in the colors the native view
/// paints it with.
auto part_look( const vehicle &veh, const int index, const bool roof, const int degrees ) -> look
{
    auto modifier = char{};
    const auto &part = veh.part_id_string( index, roof, modifier );
    auto result = part.is_valid() ? vehicle_part_look( part.obj(), modifier, degrees ) :
                  look{ .kind = "vehicle_part", .id = part.str() };
    result.glyph = glyph_of( special_symbol( veh.part_sym( index, roof ) ) );
    result.color = color_of( veh.part_color( index, roof ) );
    // The native view paints the displayed part, or the roof over it, with the part's own colors.
    const auto painted = roof ? veh.roof_at_part( index ) : veh.part_displayed_at( veh.cpart(
                             index ).mount );
    if( painted != -1 ) {
        const auto &shown = veh.cpart( painted );
        result.tint_keys = tint_keys_of( shown.info().get_flags(), shown.info().get_id().str() );
        const auto [bg, fg] = shown.get_color();
        if( !hex_of( bg ).empty() || !hex_of( fg ).empty() ) {
            result.tint = look_tint{ .bg = hex_of( bg ), .fg = hex_of( fg ) };
        }
    }
    return result;
}

auto vehicle_part_at( const map &here, const tripoint_bub_ms &p, const avatar &you )
-> std::optional<look>
{
    const auto vp = here.veh_at( p );
    if( !vp ) { return std::nullopt; }
    const auto &veh = vp->vehicle();
    // The native view draws the roof over a part below the level it draws, unless a creature stands there or the
    // part shows over its roof. Only a creature the avatar sees lifts it, so the look does not give an unseen one away.
    const auto displayed = vp.part_displayed();
    const auto under_roof = [&] {
        return displayed && !displayed->has_feature( VPFLAG_ROOF ) && displayed->info().location != "on_roof" &&
        !displayed->info().has_flag( "NO_ROOF_NEEDED" ) &&
        veh.roof_at_part( static_cast<int>( displayed->part_index() ) ) >= 0;
    };
    const auto seen_creature = [&] {
        const auto *critter = g->critter_at( p, true );
        return critter != nullptr && you.sees( *critter );
    };
    const auto roof = p.z() < shown_level( you ) && under_roof() && !seen_creature();
    const auto facing = veh.part_display_direction( vp->part_index(), roof );
    return part_look( veh, vp->part_index(), roof,
                      static_cast<int>( std::round( to_degrees( facing ) ) ) );
}

auto position_of( const map &here, const tripoint_bub_ms &p,
                  const std::string &dimension ) -> position
{
    const auto abs = map_local_to_abs( here, p );
    return { .dim = dimension, .x = abs.x(), .y = abs.y(), .z = abs.z() };
}

/// Whether a square has nothing a cell publishes: terrain the engine never memorizes, and no furniture,
/// field, item or vehicle on it. Open air above the ground is most of what the avatar sees outdoors.
auto bare_unmemorized( const map &here, const tripoint_bub_ms &p, const avatar &you ) -> bool
{
    return here.ter( p )->has_flag( TFLAG_NO_MEMORY ) && here.furn( p ) == f_null &&
           !here.veh_at( p ) && !here.sees_some_items( p, you ) &&
    std::ranges::none_of( here.field_at( p ), []( const auto & field ) { return field.second.is_field_alive(); } );
}


/// How many levels down the native view draws a square in sight that it looks through, as the level loop of
/// `cata_tiles::draw` decides: it looks down to the first square with a floor, or a remembered one with
/// something on it, and draws that square when it is in sight, remembered or a vehicle under open air in
/// sight, else the lowest square in sight with something on it on the way down. Where there is none, that floor
/// is ground the avatar neither sees nor remembers, which the native view leaves dark and which is only the
/// depth here, with no cell of its own. None when it draws nothing.
auto drawn_depth( map &here, const tripoint_bub_ms &p,
                  const avatar &you ) -> std::optional<int>
{
    // The square is in sight, so one with a floor is drawn as itself.
    if( here.dont_draw_lower_floor( p ) ) { return std::nullopt; }
    auto lowest_seen = std::optional<int> {};
    auto seen_open_air = false;
    for( auto z = p.z(); z >= -OVERMAP_DEPTH; --z ) {
        const auto at = tripoint_bub_ms( p.xy(), z );
        const auto open_air = here.ter( at ) == t_open_air;
        const auto seen = map_perception::visible_at( here, at );
        if( seen && !open_air ) { lowest_seen = z; }
        seen_open_air = seen_open_air || ( seen && open_air );
        auto memory = std::optional<bool> {};
        const auto remembered = [&]() {
            if( !memory ) {
                const auto abs = map_local_to_abs( here, at );
                memory = !you.get_terrain_tile( abs ).tile.empty() || !you.get_memorized_tile( abs ).tile.empty();
            }
            return *memory;
        };
        if( !here.dont_draw_lower_floor( at ) && ( z == p.z() || open_air || !remembered() ) ) { continue; }
        const auto shown = seen || remembered() || ( seen_open_air && here.veh_at( at ) );
        const auto drawn = shown ? z : lowest_seen.value_or( z );
        if( drawn == p.z() ) { return std::nullopt; }
        return p.z() - drawn;
    }
    return std::nullopt;
}

/// The variables of the terrain at a square, none when it has none. Reads without adding an empty entry.
auto ter_vars_at( const map &here, const tripoint_bub_ms &p ) -> const data_vars::data_set *
{
    auto offset = point_sm_ms{};
    const auto *const sm = here.get_submap_at( p, offset );
    return sm == nullptr ? nullptr : &sm->get_ter_vars( offset );
}

/// The colored light the native tiles view tints a square in sight with (`colored_light_tint_for_tile`):
/// none without colored lighting, on open air, or through night vision or environmental goggles or water.
auto glow_at( map &here, const tripoint_bub_ms &p, const avatar &you ) -> std::optional<light_glow>
{
    const auto &cache = here.access_cache( p.z() );
    if( !colored_lighting || !cache.colored_light_cache_active || here.ter( p ) == t_open_air ) {
        return std::nullopt;
    }
    const auto packed = cache.colored_light_cache[cache.idx( p.x(), p.y() )];
    const auto strength = static_cast<int>( packed >> 24 );
    const auto rgb = packed & 0x00ffffffu;
    if( strength == 0 || rgb == 0 ) { return std::nullopt; }
    const auto vision = you.get_vision_modes();
    if( vision[NV_GOGGLES] || vision[ENV_GOGGLES] || you.is_underwater() ) { return std::nullopt; }
    return light_glow{ .color = string_format( "#%06x", rgb ), .strength = strength };
}

/// Fill the facts of a currently visible cell that is not bare. False when it is plain terrain the engine never memorizes.
auto fill_visible( map &here, const tripoint_bub_ms &p, const avatar &you,
                   const map_perception::sight &view, cell &out ) -> bool
{
    const auto shapes = map_perception::visible_orientations( here, p, view );
    if( here.furn( p ) != f_null ) { out.furniture = furniture_look( here.furn( p ), shapes.furniture, here.furn_vars( p ) ); }
    const auto &displayed = here.field_at( p ).displayed_field_type();
    for( const auto &[type, entry] : here.field_at( p ) ) {
        if( !entry.is_field_alive() ) { continue; }
        auto appearance = make_look( "field", type.id().str(), entry.symbol(), entry.color() );
        appearance.looks_like = looks_like_chain<field_type_str_id>( type->looks_like );
        appearance.form = type->has_fire ? "fire" : type->phase == phase_id::GAS ? "gas" : "";
        appearance.name = type->get_name( std::max( entry.get_field_intensity() - 1, 0 ) );
        // The native view paints a field by the tint its type sets, both layers alike.
        const auto rgb = type->get_tint_rgb().or_else( [&]() -> std::optional<RGBColor> {
            return type->get_tint() == c_unset ? std::nullopt : std::optional( curses_color_to_RGB( type->get_tint() ) );
        } );
        if( rgb && !hex_of( *rgb ).empty() ) { appearance.tint = look_tint{ .bg = hex_of( *rgb ), .fg = hex_of( *rgb ) }; }
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
        out.fields.push_back( { .appearance = std::move( appearance ), .intensity = entry.get_field_intensity(),
                                .hides_items = !type->display_items } );
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
    if( const auto vp = here.veh_at( p ) ) {
        const auto cargo = vp.part_with_feature( "CARGO", true );
        out.cargo = cargo && !vp->vehicle().get_items( cargo->part_index() ).empty();
    }
    if( here.could_see_items( p, you ) ) {
        out.reviving = std::ranges::any_of( here.i_at( p ), []( const item * thing ) {
            return thing != nullptr && thing->is_corpse() && ( thing->can_revive() ||
                    ( thing->get_mtype()->zombify_into && !thing->has_flag( flag_PULPED ) ) );
        } );
    }
    if( here.ter( p )->has_flag( TFLAG_NO_MEMORY ) && !out.furniture && out.fields.empty() &&
        out.items.empty() && !out.vehicle ) {
        return out.below.has_value();
    }
    out.terrain = terrain_look( here.ter( p ), map_perception::connected_wall_symbol( here, p ),
                                shapes.terrain, ter_vars_at( here, p ) );
    const auto &cache = here.access_cache( p.z() );
    out.light = static_cast<int>( cache.visibility_cache[cache.idx( p.x(), p.y() )] );
    out.glow = glow_at( here, p, you );
    return true;
}

/// What the avatar's map memory shows of a square, which depends on nothing but that memory.
struct remembered_square {
    tripoint_abs_ms abs;
    std::optional<memory_layers> memory;
};

/// Every memorized square with its memory, rebuilt only when the avatar's map memory has changed: a
/// capture happens at every input prompt, and memory usually holds thousands of squares that did not.
auto remembered_squares( const avatar &you ) -> const std::vector<remembered_square> &
{
    static auto squares = std::vector<remembered_square> {};
    static auto built = memory_revision{};
    static auto tints = std::size_t{};
    if( const auto now = you.map_memory_revision();
        now != built || tints != tint_generation() || squares.empty() ) {
        ZoneScopedN( "engine_client_remembered_squares" );
        squares.clear();
        for( const auto &abs : you.memorized_positions() ) {
            squares.push_back( { .abs = abs, .memory = memory_at( you, abs ) } );
        }
        built = now;
        tints = tint_generation();
    }
    return squares;
}

auto cells_built = std::size_t {};

/// Marks a key, so that none is nothing.
constexpr auto plain_mark = std::uint64_t { 1 } << 63;

/// What the cell of a visible square that holds nothing but its terrain is a function of, packed, or none
/// when something else is on the square (furniture, a field, a trap, an item, a vehicle, variables of the
/// terrain, colored light), which a cell is then built for every time. Such a cell is built from the terrain, the shape the
/// squares around it the avatar knows give it (and its symbol, for a wall), its light, what the native view
/// draws below it, the season (symbol and color) and whether a roof covers it; the language and the tints of the looks are
/// the capture's, not the square's. So two captures that find the same key at a square would build the same
/// cell there, and the second need not build it.
auto plain_key( map &here, const tripoint_bub_ms &p, const avatar &you,
                const map_perception::sight &view ) -> std::uint64_t
{
    const auto tile = here.maptile_at( p );
    const auto *const vars = ter_vars_at( here, p );
    if( tile.get_furn() != f_null || !tile.get_trap_t().is_null() || tile.get_item_count() != 0 ||
        tile.get_field().field_count() != 0 || here.veh_at( p ) || ( vars != nullptr && !vars->empty() ) ||
        glow_at( here, p, you ) ) {
        return 0;
    }
    const auto shape = map_perception::terrain_orientation( here, p, view );
    const auto below = p.z() == shown_level( you ) ? drawn_depth( here, p, you ) : std::nullopt;
    const auto &cache = here.access_cache( p.z() );
    auto key = plain_mark;
    const auto put = [&]( const auto value, const int shift ) { key |= static_cast<std::uint64_t>( value ) << shift; };
    put( tile.get_ter().to_i(), 0 );
    put( shape.subtile, 20 );
    put( shape.rotation, 24 );
    put( cache.visibility_cache[cache.idx( p.x(), p.y() )], 28 );
    put( below ? *below + 1 : 0, 36 );
    put( season_of_year( calendar::turn ), 44 );
    put( !here.is_outside( p ), 48 );
    return key;
}

/// The cell of a square the avatar sees now, none when it is plain terrain the engine never memorizes.
auto visible_cell( map &here, const tripoint_bub_ms &p, const avatar &you,
                   const map_perception::sight &view, const std::string &dimension ) -> std::optional<cell>
{
    ++cells_built;
    // A square on the shown level without a floor shows what the native view draws through it.
    const auto below = p.z() == shown_level( you ) ? drawn_depth( here, p, you ) : std::nullopt;
    // Bare open air elsewhere, or showing nothing, is unknown, and most of what the avatar sees outdoors:
    // it must not cost a cell.
    const auto bare = bare_unmemorized( here, p, you );
    if( bare && !below ) { return std::nullopt; }
    auto out = cell{ .at = position_of( here, p, dimension ), .roofed = !here.is_outside( p ), .below = below };
    if( !bare && !fill_visible( here, p, you, view, out ) ) { return std::nullopt; }
    return out;
}

/// The cell of a square the avatar does not see: what it memorized, or a vehicle it is entitled to know
/// about. `memory` is what `memory_at` gives for the square when the caller already has it.
auto remembered_cell( map &here, const tripoint_bub_ms &p, const avatar &you,
                      const std::string &dimension,
                      const std::optional<std::optional<memory_layers>> &memory = std::nullopt )
-> std::optional<cell>
{
    if( !here.inbounds( p ) || map_perception::visible_at( here, p ) ) { return std::nullopt; }
    auto out = cell{ .at = position_of( here, p, dimension ), .known = knowledge::remembered,
                     .memory = memory ? *memory : memory_at( you, map_local_to_abs( here, p ) ) };
    // A vehicle the avatar is entitled to know about is perceived live, so its square is a visible one;
    // the ground beneath it is not perceived.
    if( map_perception::detailed_at( here, p ) ) {
        out.vehicle = vehicle_part_at( here, p, you );
        if( out.vehicle ) { out.known = knowledge::visible; }
    }
    if( !out.memory && !out.vehicle ) { return std::nullopt; }
    return out;
}

/// The squares of the parts of every vehicle in the reality bubble.
auto vehicle_squares( map &here ) -> std::set<tripoint_bub_ms>
{
    auto squares = std::set<tripoint_bub_ms> {};
    for( const auto &wrapped : here.get_vehicles() ) {
        for( const auto &part : wrapped.v->get_all_parts() ) { squares.insert( part.pos() ); }
    }
    return squares;
}

/// `bare_submap` for the squares of one capture, asked once per run of squares in a submap. Squares on the
/// shown level are never bare here: one without a floor shows what the native view draws below it.
class bare_scan
{
    public:
        bare_scan( map &here, const avatar &you ) : here_( here ), level_( shown_level( you ) ),
            vehicles_( vehicle_submaps( here ) ) {}
        auto operator()( const tripoint_bub_ms &p ) -> bool {
            if( p.z() == level_ ) { return false; }
            const auto at = project_to<coords::sm>( p );
            if( last_ != at ) {
                last_ = at;
                bare_ = bare_submap( here_, p, vehicles_ );
            }
            return bare_;
        }
    private:
        const map &here_;
        int level_;
        std::set<tripoint_bub_sm> vehicles_;
        std::optional<tripoint_bub_sm> last_;
        bool bare_ = false;
};

/// Cells the avatar knows by memory or by a vehicle it is entitled to perceive; nothing else costs time.
auto capture_known( map &here, const avatar &you, const std::string &dimension,
                    std::map<position, cell> &cells ) -> void
{
    ZoneScopedN( "engine_client_capture_known" );
    {
        ZoneScopedN( "engine_client_capture_visible" );
        const auto view = map_perception::current_sight();
        auto bare = bare_scan( here, you );
        for( const auto &p : map_perception::visible_cells( here ) ) {
            if( bare( p ) ) { continue; }
            if( auto out = visible_cell( here, p, you, view, dimension ) ) { cells.emplace( out->at, std::move( *out ) ); }
        }
    }
    ZoneNamedN( remembered_zone, "engine_client_capture_remembered", true );
    const auto remember = [&]( const tripoint_bub_ms & p,
    const std::optional<std::optional<memory_layers>> &memory ) {
        if( cells.contains( position_of( here, p, dimension ) ) ) { return; }
        if( auto out = remembered_cell( here, p, you, dimension, memory ) ) { cells.emplace( out->at, std::move( *out ) ); }
    };
    for( const auto &square : remembered_squares( you ) ) {
        remember( abs_to_map_local( here, square.abs ), square.memory );
    }
    for( const auto &p : vehicle_squares( here ) ) { remember( p, std::nullopt ); }
}

/// Opaque IDs for creatures the avatar has been shown. The weak pointer pins the creature's control
/// block, so an address is never mistaken for a new creature, and an expired entry means it is gone.
class entity_ids
{
    public:
        auto id_for( const Creature &critter ) -> std::string {
            const auto owner = g->shared_from( critter );
            if( !owner ) { return {}; }
            const auto found = std::ranges::find_if( entries_, [&]( const entry & e ) {
                return !e.owner.owner_before( owner ) && !owner.owner_before( e.owner );
            } );
            if( found != entries_.end() ) { return found->id; }
            entries_.push_back( { owner, "e:" + std::to_string( next_++ ) } );
            return entries_.back().id;
        }
        auto alive( const std::string &id ) const -> bool {
            const auto found = std::ranges::find( entries_, id, &entry::id );
            const auto owner = found == entries_.end() ? nullptr : found->owner.lock();
            return owner && !owner->is_dead_state();
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
    // The schema bounds the list like the native lookup bounds the chain; each step names two ids.
    result.looks_like.assign( std::next( candidates.begin() ), candidates.end() );
    result.looks_like.resize( std::min( result.looks_like.size(), looks_like_limit ) );
    return result;
}

auto overlays_of( const Creature &critter ) -> std::vector<look>
{
    const auto *character = critter.as_character();
    if( character == nullptr ) { return {}; }
    return character->get_overlay_ids() | std::views::transform( [&]( const auto & entry ) {
        auto result = overlay_look( entry.id, character->male );
        if( const auto *const mut = std::get_if<const mutation *>( &entry.entry ) ) {
            const auto &branch = ( *mut )->first.obj();
            result.mutation = look_mutation{
                .id = ( *mut )->first.str(),
                .types = { branch.types.begin(), branch.types.end() },
                .flags = branch.flags | std::views::transform( &trait_flag_str_id::str ) | std::ranges::to<std::vector>() };
        } else if( const auto *const thing = std::get_if<const item *>( &entry.entry ) ) {
            result = with_item_tint( std::move( result ), **thing );
        } else if( const auto *const bio = std::get_if<const bionic *>( &entry.entry ) ) {
            result.tint_keys = tint_keys_of( ( *bio )->id->flags, ( *bio )->id.str() );
        } else if( const auto *const eff = std::get_if<const effect *>( &entry.entry ) ) {
            result.tint_keys = tint_keys_of( std::vector<std::string> {}, ( *eff )->get_id().str() );
        }
        return result;
    } ) | std::ranges::to<std::vector>();
}

/// The ids of the mutations of a character by mutation type.
auto traits_of( const Creature &critter ) -> std::map<std::string, std::vector<std::string>>
{
    auto result = std::map<std::string, std::vector<std::string>> {};
    const auto *character = critter.as_character();
    if( character == nullptr ) { return result; }
    for( const auto &id : character->get_mutations() ) {
        for( const auto &type : id.obj().types ) { result[type].push_back( id.str() ); }
    }
    return result;
}

/// The creatures a snapshot discloses: the monsters and NPCs the avatar sees.
/// A hurt creature's health in the fifths and color of the native look-around bar; none at full health.
auto health_of( const Creature &critter ) -> std::optional<entity_health>
{
    const auto hp = critter.get_hp();
    const auto max = critter.get_hp_max();
    if( max <= 0 || hp >= max ) { return std::nullopt; }
    const auto bar = get_hp_bar( hp, max, critter.is_monster() );
    return entity_health{ .fifths = std::clamp( 5 * hp / max, 0, 5 ), .color = color_of( bar.second ) };
}

auto disclosed_creatures( const avatar &you ) -> std::vector<const Creature *>
{
    const auto all = g->get_creatures_if( []( const Creature & ) { return true; } );
    return all | std::views::filter( [&]( const Creature * critter ) {
        return critter != &you && you.sees( *critter ) && ( critter->as_monster() || critter->as_npc() );
    } ) | std::ranges::to<std::vector<const Creature *>>();
}

/// The look of a monster or NPC the avatar sees.
auto disclosed_look( const Creature &critter ) -> look
{
    const auto *monster = critter.as_monster();
    const auto type = monster != nullptr ? monster->type->id.str() : critter.as_npc()->myclass.str();
    auto appearance = creature_look( critter, monster != nullptr ? "monster" : "npc", type );
    if( monster != nullptr ) {
        appearance.tint_keys = tint_keys_of( std::vector<std::string> {}, type );
        appearance.looks_like = looks_like_chain<mtype_id>( monster->type->looks_like );
    }
    return appearance;
}

auto capture_entities( const map &here, const avatar &you, const std::string &dimension )
-> std::map<std::string, entity>
{
    auto entities = std::map<std::string, entity> {};
    ids.forget_expired();
    for( const auto *critter : disclosed_creatures( you ) ) {
        auto id = ids.id_for( *critter );
        entities.emplace( id, entity{
            .id = id,
            .at = position_of( here, critter->bub_pos(), dimension ),
            .appearance = disclosed_look( *critter ),
            .name = critter->get_name(),
            .overlays = overlays_of( *critter ),
            .traits = traits_of( *critter ),
            .attitude = Creature::attitude_raw_string( critter->attitude_to( you ) ),
            .aware = critter->sees( you ) && !you.has_trait( trait_INATTENTIVE ),
            .health = health_of( *critter ) } );
    }
    return entities;
}

/// The files of game data that add sprites to tilesets, in the order the native view loads them.
auto mod_tileset_files() -> std::vector<mod_tileset_file>
{
    return game_client::declared_mod_tilesets() | std::views::transform( []( const auto & file ) {
        return mod_tileset_file{ .path = file.full_path, .base = file.base_path, .index = file.index,
                                 .compatibility = file.compatibility };
    } ) | std::ranges::to<std::vector>();
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

/// The row text of a stack: the first cell the native inventory list prints for these items, then where
/// a wielded or worn one is.
auto stack_label( const std::vector<item *> &items,
                  const std::optional<std::string> &slot ) -> std::string
{
    static const auto preset = inventory_selector_preset{};
    const auto caption = preset.get_cell_text( ::inventory_entry{ items }, 0 );
    if( slot == "wielded" ) { return string_format( _( "%s (wielded)" ), caption ); }
    if( slot == "worn" ) { return string_format( _( "%s (worn)" ), caption ); }
    return caption;
}

/// One inventory row for a stack of equal items, named the way the native lists show it.
auto stack_entry( const std::vector<item *> &items,
                  std::optional<std::string> slot = std::nullopt ) -> inventory_entry
{
    const auto &thing = *items.front();
    // display_name() leads with the colored marker exactly when the native lists show one.
    const auto marker = colorize( thing.damage_symbol() + "\u00A0", thing.damage_color() );
    auto durability = thing.display_name().starts_with( marker )
                      ? std::optional{ item_durability{ .symbol = thing.damage_symbol(), .color = color_of( thing.damage_color() ) } }
                      :
                      std::nullopt;
    // Native units, as the native lists count what can be chosen: charges of a charges item, else items.
    const auto count = std::ranges::fold_left( items | std::views::transform( &item::count ), 0,
                       std::plus<> {} );
    return { .appearance = item_look( thing ), .name = remove_color_tags( thing.display_name() ), .count = static_cast<std::uint64_t>( count ),
             .slot = slot, .durability = std::move( durability ), .label = stack_label( items, slot ) };
}

/// Gives each row the id of its items (`game_client::stack_row_id`), which follows them when rows reorder.
auto identified( std::vector<item_stack> rows ) -> std::vector<item_stack>
{
    for( auto &stack : rows ) {
        stack.row.id = game_client::stack_row_id( stack.items |
                       std::ranges::to<std::vector<const item *>>() );
    }
    return rows;
}

/// The creatures the native compass panel lists: what the avatar sees alive, within the loaded map and not on
/// its square, by the heading of the horizontal offset (the native bucket the panel's own count uses).
auto capture_compass( const avatar &you ) -> std::vector<compass_direction>
{
    using namespace std::views;
    namespace ranges = std::ranges;
    const auto shown = disclosed_creatures( you ) | filter( [&]( const Creature * critter ) {
        return !critter->is_dead_state() && critter->bub_pos() != you.bub_pos() &&
               rl_dist( you.bub_pos(), critter->bub_pos() ) <= g_mapsize_x;
    } ) | ranges::to<std::vector<const Creature *>>();
    struct native_heading {
        direction dir;
        std::string label;
    };
    // The labels the native compass panel prints, clockwise from north; it prints none for its own column.
    const auto clockwise = std::array{
        native_heading{ direction::NORTH, _( "North:" ) }, native_heading{ direction::NORTHEAST, _( "NE:" ) },
        native_heading{ direction::EAST, _( "East:" ) }, native_heading{ direction::SOUTHEAST, _( "SE:" ) },
        native_heading{ direction::SOUTH, _( "South:" ) }, native_heading{ direction::SOUTHWEST, _( "SW:" ) },
        native_heading{ direction::WEST, _( "West:" ) }, native_heading{ direction::NORTHWEST, _( "NW:" ) },
        native_heading{ direction::CENTER, "" } };
    return clockwise | transform( [&]( const native_heading & heading ) {
        const auto dir = heading.dir;
        auto result = compass_direction{ .toward = heading_of( dir ), .label = heading.label };
        for( const auto *critter : shown ) {
            const auto offset = direction_from( you.bub_pos().xy(), critter->bub_pos().xy() );
            if( std::to_underlying( offset ) != std::to_underlying( dir ) ) { continue; }
            result.creatures.push_back( {
                .id = ids.id_for( *critter ), .appearance = disclosed_look( *critter ), .name = critter->get_name(),
                .attitude = Creature::attitude_raw_string( critter->attitude_to( you ) ),
                .distance = rl_dist( you.bub_pos(), critter->bub_pos() ) } );
        }
        ranges::stable_sort( result.creatures, {}, &compass_creature::distance );
        return result;
    } ) | filter( []( const compass_direction & group ) { return !group.creatures.empty(); } ) |
    ranges::to<std::vector>();
}

/// What the avatar is busy with, as the native wait popup names it; none for an activity it does not name.
auto capture_activity( const avatar &you ) -> std::optional<activity_value>
{
    const auto &act = you.activity;
    if( !act || act->get_verb().empty() ) { return std::nullopt; }
    auto result = activity_value{ .id = act->id().str(), .name = act->get_verb().translated() };
    const auto total = act->get_moves_total();
    if( total > 0 && total != calendar::INDEFINITELY_LONG ) {
        const auto left = static_cast<long long>( act->get_moves_left() );
        result.done = static_cast<int>( std::clamp( 1000 - 1000 * left / total, 0LL, 1000LL ) );
    }
    return result;
}

auto capture_avatar( map &here, const avatar &you,
                     const std::string &dimension ) -> avatar_value
{
    auto result = avatar_value{
        .id = "e:avatar", .at = position_of( here, you.bub_pos(), dimension ), .name = you.name,
        .appearance = creature_look( you, "avatar", "avatar" ),
        .overlays = overlays_of( you ),
        .traits = traits_of( you ),
        .stats = {
            { "strength", _( "Strength" ), std::to_string( you.get_str() ), color_of( color_compare_base( you.get_str_base(), you.get_str() ) ), trim( _( "Str " ) ) },
            { "dexterity", _( "Dexterity" ), std::to_string( you.get_dex() ), color_of( color_compare_base( you.get_dex_base(), you.get_dex() ) ), trim( _( "Dex " ) ) },
            { "intelligence", _( "Intelligence" ), std::to_string( you.get_int() ), color_of( color_compare_base( you.get_int_base(), you.get_int() ) ), trim( _( "Int " ) ) },
            { "perception", _( "Perception" ), std::to_string( you.get_per() ), color_of( color_compare_base( you.get_per_base(), you.get_per() ) ), trim( _( "Per " ) ) },
            { "moves", _( "Moves" ), std::to_string( you.get_moves() ) },
            { "pain", _( "Pain" ), std::to_string( you.get_pain() ) }
        },
        .sidebar = sidebar_snapshot( you ), .compass = capture_compass( you ), .modes = engine_modes(),
        .activity = capture_activity( you ) };
    const auto stacks = item_stacks( here, you );
    result.inventory = stacks.inventory | std::views::transform( &item_stack::row ) |
                       std::ranges::to<std::vector>();
    result.ground = stacks.ground | std::views::transform( &item_stack::row ) |
                    std::ranges::to<std::vector>();
    return result;
}

} // namespace

auto item_stacks( map &here, const avatar &you ) -> avatar_item_stacks
{
    auto result = avatar_item_stacks{};
    you.visit_items( [&]( const item * visited ) {
        // The rows are read-only here; the item operations that change the items pass the avatar they own.
        auto *thing = const_cast<item *>( visited );
        if( you.is_wielding( *thing ) ) { result.inventory.push_back( { stack_entry( { thing }, "wielded" ), { thing } } ); }
        else if( you.is_worn( *thing ) ) { result.inventory.push_back( { stack_entry( { thing }, "worn" ), { thing } } ); }
        return VisitResponse::NEXT;
    } );
    for( const auto *stack : you.inv_const_slice() ) {
        if( stack != nullptr ) { result.inventory.push_back( { stack_entry( *stack, "carried" ), *stack } ); }
    }
    if( here.accessible_items( you.bub_pos() ) ) {
        // The native pickup stacks equal items the same way; each stack is one choice there.
        struct pile {
            itype_id type;
            std::string name;
            std::vector<item *> items;
        };
        auto piles = std::vector<pile> {};
        for( auto *thing : here.i_at( you.bub_pos() ) ) {
            auto name = remove_color_tags( thing->display_name() );
            const auto same = std::ranges::find_if( piles, [&]( const auto & other ) {
                return other.type == thing->typeId() && other.name == name;
            } );
            if( same != piles.end() ) { same->items.push_back( thing ); }
            else { piles.push_back( { .type = thing->typeId(), .name = std::move( name ), .items = { thing } } ); }
        }
        const auto stacked = []( const pile & each ) { return item_stack{ stack_entry( each.items ), each.items }; };
        result.ground = piles | std::views::transform( stacked ) | std::ranges::to<std::vector>();
    }
    result.inventory = identified( std::move( result.inventory ) );
    result.ground = identified( std::move( result.ground ) );
    return result;
}

auto position_at( const tripoint_bub_ms &p ) -> position
{
    const auto &here = get_map();
    return position_of( here, p, here.get_bound_dimension().str() );
}

auto color_name( const nc_color &color ) -> std::string
{
    return color_of( color );
}

auto heading_of( const direction dir ) -> heading
{
    static constexpr auto columns = std::array{ "northwest", "north", "northeast", "west", "center", "east",
                          "southwest", "south", "southeast" };
    const auto offset = displace( dir );
    const auto column = std::string( columns[( offset.y + 1 ) * 3 + offset.x + 1] );
    const auto level = std::string( offset.z > 0 ? "above" : offset.z < 0 ? "below" : "" );
    const auto vertical = offset.x == 0 && offset.y == 0;
    return { .id = level.empty() ? column : vertical ? level : column + "_" + level, .name = direction_name( dir ) };
}

auto entity_id( const Creature &critter ) -> std::string
{
    return critter.is_avatar() ? "e:avatar" : ids.id_for( critter );
}

auto disclosed_entity_ids() -> std::set<std::string>
{
    if( !g ) { return {}; }
    const auto seen = disclosed_creatures( get_avatar() );
    return seen | std::views::transform( []( const Creature * critter ) { return entity_id( *critter ); } )
    |
    std::ranges::to<std::set>();
}

auto entity_alive( const std::string &id ) -> bool { return ids.alive( id ); }

auto item_appearance( const item &thing ) -> look { return item_look( thing ); }

auto terrain_appearance( const ter_id &id ) -> look
{
    return terrain_look( id, id->symbol(), {} );
}

auto furniture_appearance( const furn_id &id ) -> look { return furniture_look( id, {} ); }

auto trap_appearance( const trap &tr ) -> look { return trap_look( tr, {} ); }

auto vehicle_part_appearance( const vehicle &veh, const int part, const int degrees ) -> look
{
    return part_look( veh, part, false, degrees );
}

auto vehicle_part_type_appearance( const vpart_info &info ) -> look { return vehicle_part_look( info, 0, 270 ); }

auto vehicle_part_own_appearance( const vehicle &veh, const int part, const int degrees ) -> look
{
    const auto &shown = veh.cpart( part );
    const auto modifier = veh.part_flag( part, VPFLAG_OPENABLE ) && shown.open ? char{ 1 } :
                          shown.is_broken() ? char{ 2 } :
                          char{ 0 };
    const auto &info = shown.proxy_part_id.is_null() ? shown.info() : shown.proxy_part_id.obj();
    auto result = vehicle_part_look( info, modifier, degrees );
    result.glyph = glyph_of( special_symbol( veh.part_sym( part, true ) ) );
    result.color = color_of( veh.part_color( part, true ) );
    result.tint_keys = tint_keys_of( shown.info().get_flags(), shown.info().get_id().str() );
    const auto [bg, fg] = shown.get_color();
    if( !hex_of( bg ).empty() || !hex_of( fg ).empty() ) {
        result.tint = look_tint{ .bg = hex_of( bg ), .fg = hex_of( fg ) };
    }
    return result;
}

auto set_tint_ids( std::set<std::string> ids ) -> void
{
    tint_ids() = std::move( ids );
    ++tint_generation();
}

namespace
{
/// How far `now` is from `start` to `end`, in percent.
auto percent_between( const time_point &start, const time_point &end, const time_point &now ) -> int
{
    return static_cast<int>( std::lround( 100.0 * ( now - start ) / ( end - start ) ) );
}

/// The sun's way from sunrise to sunset at `now`; empty at night.
auto sun_crossing( const time_point &now ) -> std::optional<int>
{
    const auto rise = sunrise( now );
    const auto set = sunset( now );
    if( now < rise || now >= set ) { return std::nullopt; }
    return percent_between( rise, set, now );
}

/// The moon's way through the night from sunset to sunrise at `now`; empty by day.
auto moon_crossing( const time_point &now ) -> std::optional<int>
{
    if( now >= sunset( now ) ) { return percent_between( sunset( now ), sunrise( now + 1_days ), now ); }
    if( now < sunrise( now ) ) { return percent_between( sunset( now - 1_days ), sunrise( now ), now ); }
    return std::nullopt;
}

auto moon_phase_name( const moon_phase phase ) -> std::string
{
    static constexpr auto names = std::array{ "new", "waxing_crescent", "half_moon_waxing", "waxing_gibbous", "full",
                          "waning_gibbous", "half_moon_waning", "waning_crescent" };
    return names.at( phase );
}

auto capture_environment() -> environment_value
{
    const auto &weather = get_weather().weather_id.obj();
    const auto daylight = std::lround( 100.0 * sunlight( calendar::turn ) / current_daylight_level(
                                           calendar::turn ) );
    return environment_value{
        .turn = std::to_string( to_turn<int>( calendar::turn ) ), .time = to_string( calendar::turn ),
        .weather = get_weather().weather_id.str(), .season = season_name(),
        .sky = sky_value{
            .daylight = static_cast<int>( std::clamp( daylight, 0L, 100L ) ),
            .precipitation = io::enum_to_string( weather.precip ), .rain = weather.rains,
            .dimming = std::max( -weather.light_modifier, 0 ), .sun = sun_crossing( calendar::turn ),
            .moon = moon_crossing( calendar::turn ), .moon_phase = moon_phase_name( get_moon_phase( calendar::turn ) ) },
        .mod_tilesets = mod_tileset_files() };
}

/// Every section of the world but its cells.
auto capture_sections( map &here, const avatar &you, const std::string &dimension ) -> world_state
{
    const auto size = here.getmapsize();
    auto state = world_state{
        .coverage = bounds{
            .min = position_of( here, tripoint_bub_ms( 0, 0, -OVERMAP_DEPTH ), dimension ),
            .max = position_of( here, tripoint_bub_ms( size * SEEX - 1, size * SEEY - 1, OVERMAP_HEIGHT ),
                                dimension ) },
        .entities = capture_entities( here, you, dimension ),
        .avatar = capture_avatar( here, you, dimension ),
        .environment = capture_environment() };
    // The terrain window moves with the view, not the world: its squares are where a click lands, on the level
    // the native view draws.
    if( const auto window = g->click_window() ) {
        const auto z = shown_level( you );
        state.view = bounds{
            .min = position_of( here, tripoint_bub_ms( window->p_min.x(), window->p_min.y(), z ), dimension ),
            .max = position_of( here, tripoint_bub_ms( window->p_max.x() - 1, window->p_max.y() - 1, z ),
                                dimension ) };
    }
    for( const auto &step : g->get_destination_preview() ) {
        state.route.push_back( position_of( here, step, dimension ) );
    }
    return state;
}

/// What the last patch captured the cells from, which is all the next one needs to know to name only the
/// cells that can differ. A cell the avatar does not see depends on nothing but its map memory, the tints
/// of its looks, where the map is, whether the square is seen, and the vehicle part on it; so a square is
/// named when it is seen now or was before, is a vehicle part now or was, or when its memory changed.
struct patch_basis {
    bool valid = false;
    std::string epoch = {};
    counter revision = 0;
    std::optional<bounds> coverage = std::nullopt;
    std::uint64_t memory_owner = 0;
    std::size_t tints = 0;
    std::string language = {};
    /// The squares the avatar saw, in the order of `by_level`. Where they are in the map of that patch.
    std::vector<tripoint_bub_ms> visible = {};
    /// The seen squares that made a cell, in the same order.
    std::vector<tripoint_bub_ms> live = {};
    /// `plain_key` of each of those, as of the last patch.
    std::vector<std::uint64_t> keys = {};
    std::set<tripoint_bub_ms> vehicles = {};
};
/// The order `map_perception::visible_cells` lists squares in, so that two lists merge in one pass.
auto by_level( const tripoint_bub_ms &a, const tripoint_bub_ms &b ) -> bool
{
    return std::tuple( a.z(), a.x(), a.y() ) < std::tuple( b.z(), b.x(), b.y() );
}

/// How far the map moved, in squares, between two coverages that keep the size and dimension of the map;
/// none when they do not, and the basis then says nothing about the new map.
auto shift_between( const bounds &before, const bounds &after ) -> std::optional<point>
{
    const auto extent = []( const bounds & b ) { return std::tuple( b.min.dim, b.max.x - b.min.x, b.max.y - b.min.y, b.min.z, b.max.z ); };
    if( extent( before ) != extent( after ) ) { return std::nullopt; }
    return point( before.min.x - after.min.x, before.min.y - after.min.y );
}

/// The basis in the coordinates of a map that moved by `by`, without the squares that left the map.
auto shifted( patch_basis from, const map &here, const point &by ) -> patch_basis
{
    const auto translate = [&]( const tripoint_bub_ms & p ) { return p + tripoint( by, 0 ); };
    const auto kept = [&]( const tripoint_bub_ms & p ) { return here.inbounds( p ); };
    const auto moved = [&]( const auto & squares ) {
        return squares | std::views::transform( translate ) | std::views::filter( kept ) |
               std::ranges::to<std::vector>();
    };
    auto live = std::vector<tripoint_bub_ms> {};
    auto keys = std::vector<std::uint64_t> {};
    for( const auto i : std::views::iota( std::size_t{}, from.live.size() ) ) {
        if( const auto p = translate( from.live[i] ); kept( p ) ) {
            live.push_back( p );
            keys.push_back( from.keys[i] );
        }
    }
    from.visible = moved( from.visible );
    from.live = std::move( live );
    from.keys = std::move( keys );
    from.vehicles = from.vehicles | std::views::transform( translate ) | std::views::filter(
                        kept ) | std::ranges::to<std::set>();
    return from;
}
auto basis = patch_basis {};
} // namespace

auto visible_cells_built() -> std::size_t { return cells_built; }

auto vehicle_submaps( map &here ) -> std::set<tripoint_bub_sm>
{
    auto submaps = std::set<tripoint_bub_sm> {};
    for( const auto &wrapped : here.get_vehicles() ) {
        for( const auto &part : wrapped.v->get_all_parts() ) { submaps.insert( project_to<coords::sm>( part.pos() ) ); }
    }
    return submaps;
}

auto bare_submap( const map &here, const tripoint_bub_ms &p,
                  const std::set<tripoint_bub_sm> &vehicles ) -> bool
{
    const auto *const sm = here.get_submap_at( p );
    return sm != nullptr && sm->is_uniform && here.ter( p )->has_flag( TFLAG_NO_MEMORY ) &&
           !vehicles.contains( project_to<coords::sm>( p ) );
}

auto capture_world() -> world_state
{
    ZoneScopedN( "engine_client_capture_world" );
    if( !g || !game_session::running() ) { return {}; }
    auto &here = get_map();
    const auto &you = get_avatar();
    const auto dimension = here.get_bound_dimension().str();
    auto state = capture_sections( here, you, dimension );
    capture_known( here, you, dimension, state.cells );
    return state;
}

auto capture_worded() -> world_state
{
    if( !g || !game_session::running() ) { return {}; }
    auto &here = get_map();
    return { .avatar = capture_avatar( here, get_avatar(), here.get_bound_dimension().str() ),
             .environment = capture_environment() };
}

auto capture_patch( const snapshot &previous, const bool consecutive ) -> world_patch
{
    ZoneScopedN( "engine_client_capture_patch" );
    if( !g || !game_session::running() ) {
        basis.valid = false;
        return { .exhaustive = true };
    }
    auto &here = get_map();
    const auto &you = get_avatar();
    const auto dimension = here.get_bound_dimension().str();
    auto patch = world_patch{ .world = capture_sections( here, you, dimension ) };
    // Taken at every patch, so that what it holds is what changed since the last one.
    const auto changes = map_perception::take_memory_changes();
    const auto memory_owner = you.map_memory_revision().owner;
    // The map moves with the avatar; what the last patch held is then where it was, so it is moved with the map.
    const auto moved = basis.coverage && patch.world.coverage ?
                       shift_between( *basis.coverage, *patch.world.coverage ) : std::nullopt;
    const auto continues = consecutive && basis.valid && basis.epoch == previous.at.epoch &&
                           previous.at.revision >= basis.revision && previous.at.revision - basis.revision <= 1 &&
                           moved && changes && basis.memory_owner == memory_owner &&
                           basis.tints == tint_generation() &&
                           basis.language == get_language().id;
    const auto view = map_perception::current_sight();
    auto visible = map_perception::visible_cells( here );
    auto vehicles = vehicle_squares( here );
    auto live = std::vector<tripoint_bub_ms> {};
    auto keys = std::vector<std::uint64_t> {};
    if( !continues ) {
        capture_known( here, you, dimension, patch.cells );
        patch.exhaustive = true;
        auto bare = bare_scan( here, you );
        std::ranges::copy_if( visible, std::back_inserter( live ), [&]( const auto & p ) { return !bare( p ) && patch.cells.contains( position_of( here, p, dimension ) ); } );
        std::ranges::transform( live, std::back_inserter( keys ), [&]( const auto & p ) { return plain_key( here, p, you, view ); } );
    } else {
        const auto &known = previous.value.world.cells;
        if( *moved != point_zero ) { basis = shifted( std::move( basis ), here, *moved ); }
        // A square seen now is named once, by what the avatar knows of it, and not at all when that is what
        // the last patch said. A plain one that has the key it had then is not even built.
        auto bare = bare_scan( here, you );
        auto last = std::size_t{};
        for( const auto &p : visible ) {
            if( bare( p ) ) { continue; }
            while( last < basis.live.size() && by_level( basis.live[last], p ) ) { ++last; }
            const auto key = plain_key( here, p, you, view );
            if( key != 0 && last < basis.live.size() && basis.live[last] == p && basis.keys[last] == key ) {
                live.push_back( p );
                keys.push_back( key );
                continue;
            }
            if( auto out = visible_cell( here, p, you, view, dimension ) ) {
                live.push_back( p );
                keys.push_back( key );
                const auto before = known.find( out->at );
                if( before == known.end() || before->second != *out ) { patch.cells.emplace( out->at, std::move( *out ) ); }
            }
        }
        const auto seen = [&]( const tripoint_bub_ms & p ) { return std::ranges::binary_search( visible, p, by_level ); };
        const auto lived = [&]( const tripoint_bub_ms & p ) { return std::ranges::binary_search( live, p, by_level ); };
        const auto unknown = [&]( const tripoint_bub_ms & p ) { patch.unknown.insert( position_of( here, p, dimension ) ); };
        // A seen square made a cell before only if the last patch made one of it, and one that is gone is unknown.
        auto gone = std::vector<tripoint_bub_ms> {};
        std::ranges::set_difference( basis.live, live, std::back_inserter( gone ), by_level );
        for( const auto &p : gone | std::views::filter( seen ) ) { unknown( p ); }
        // A square that comes into sight without a cell may have been remembered with one.
        auto arrived = std::vector<tripoint_bub_ms> {};
        std::ranges::set_difference( visible, basis.visible, std::back_inserter( arrived ), by_level );
        for( const auto &p : arrived ) {
            if( !lived( p ) && known.contains( position_of( here, p, dimension ) ) ) { unknown( p ); }
        }
        // The rest is named by what the avatar remembers of it.
        const auto name = [&]( const tripoint_bub_ms & p ) {
            const auto at = position_of( here, p, dimension );
            if( seen( p ) || patch.cells.contains( at ) || patch.unknown.contains( at ) ) { return; }
            if( auto out = remembered_cell( here, p, you, dimension ) ) { patch.cells.emplace( at, std::move( *out ) ); }
            else if( known.contains( at ) ) { patch.unknown.insert( at ); }
        };
        auto left = std::vector<tripoint_bub_ms> {};
        std::ranges::set_difference( basis.visible, visible, std::back_inserter( left ), by_level );
        std::ranges::for_each( left, name );
        std::ranges::for_each( vehicles, name );
        if( vehicles != basis.vehicles ) { std::ranges::for_each( basis.vehicles, name ); }
        for( const auto &abs : *changes ) { name( abs_to_map_local( here, abs ) ); }
        // A square the map came to cover is named by what the avatar remembers of it.
        if( *moved != point_zero ) {
            const auto before = tripoint( *moved, 0 );
            for( const auto &abs : you.memorized_positions() ) {
                if( const auto p = abs_to_map_local( here, abs ); here.inbounds( p ) &&
                    !here.inbounds( p - before ) ) { name( p ); }
            }
        }
    }
    basis = {
        .valid = true, .epoch = previous.at.epoch, .revision = previous.at.revision,
        .coverage = patch.world.coverage, .memory_owner = memory_owner, .tints = tint_generation(),
        .language = get_language().id, .visible = std::move( visible ), .live = std::move( live ),
        .keys = std::move( keys ), .vehicles = std::move( vehicles )
    };
    return patch;
}

} // namespace engine_client::world
