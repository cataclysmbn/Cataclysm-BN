#include "procgen/proc_item.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "calendar.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "flag.h"
#include "init.h"
#include "item.h"
#include "units_mass.h"
#include "units_volume.h"

namespace
{

struct lua_validate_result {
    bool ok = true;
    std::string reason;
};

struct lua_function_ref {
    cata::lua_state *state = nullptr;
    sol::protected_function fn;
};

struct lua_params_options {
    const proc::schema &sch;
    const std::vector<proc::part_fact> &facts;
    const proc::fast_blob &blob;
    const std::vector<proc::craft_pick> *picks = nullptr;
    std::optional<itype_id> result_override = std::nullopt;
};

auto ceil_div( const int64_t value, const int divisor ) -> int
{
    if( value <= 0 || divisor <= 0 ) {
        return 0;
    }
    return static_cast<int>( ( value + divisor - 1 ) / divisor );
}

auto scaled_food_servings( const item &it, const proc::fast_blob &blob ) -> int
{
    if( !it.count_by_charges() ) {
        return 1;
    }

    const auto default_servings = std::max( it.charges, 1 );
    const auto default_mass_g = std::max( units::to_gram( it.weight() ), 1L );
    const auto default_volume_ml = std::max( units::to_milliliter( it.volume() ), 1 );
    const auto mass_servings = blob.mass_g > 0 ?
                               ceil_div( static_cast<int64_t>( blob.mass_g ) * default_servings,
                                         default_mass_g ) : default_servings;
    const auto volume_servings = blob.volume_ml > 0 ?
                                 ceil_div( static_cast<int64_t>( blob.volume_ml ) * default_servings,
                                           default_volume_ml ) : default_servings;
    return std::max( { default_servings, mass_servings, volume_servings } );
}

auto split_path( const std::string &path ) -> std::vector<std::string>
{
    auto ret = std::vector<std::string> {};
    auto start = size_t{ 0 };
    while( start <= path.size() ) {
        const auto pos = path.find( '.', start );
        if( pos == std::string::npos ) {
            ret.push_back( path.substr( start ) );
            break;
        }
        ret.push_back( path.substr( start, pos - start ) );
        start = pos + 1;
    }
    return ret;
}

auto active_lua_state( cata::lua_state *state ) -> cata::lua_state *
{
    if( state != nullptr ) {
        return state;
    }
    if( DynamicDataLoader::get_instance().lua ) {
        return DynamicDataLoader::get_instance().lua.get();
    }
    return nullptr;
}

auto find_lua_function( sol::state &lua,
                        const std::string &path ) -> std::optional<sol::protected_function>
{
    if( path.empty() ) {
        return std::nullopt;
    }

    const auto parts = split_path( path );
    if( parts.empty() ) {
        return std::nullopt;
    }

    auto current = lua.globals().get_or<sol::object>( parts.front(), sol::lua_nil );
    std::ranges::for_each( parts | std::views::drop( 1 ), [&]( const std::string & part ) {
        if( current == sol::lua_nil || !current.is<sol::table>() ) {
            current = sol::make_object( lua, sol::lua_nil );
            return;
        }
        const auto table = current.as<sol::table>();
        current = table.get_or<sol::object>( part, sol::lua_nil );
    } );

    if( current == sol::lua_nil ) {
        return std::nullopt;
    }
    if( !( current.is<sol::function>() || current.is<sol::protected_function>() ) ) {
        return std::nullopt;
    }
    return current.as<sol::protected_function>();
}

auto resolve_lua_function( cata::lua_state *state,
                           const std::string &path ) -> std::optional<lua_function_ref>
{
    auto *active = active_lua_state( state );
    if( active == nullptr ) {
        return std::nullopt;
    }

    const auto fn = find_lua_function( active->lua, path );
    if( !fn ) {
        return std::nullopt;
    }

    return lua_function_ref { .state = active, .fn = *fn };
}

auto fact_table( sol::state &lua, const proc::part_fact &fact ) -> sol::table
{
    auto tbl = lua.create_table();
    tbl["ix"] = fact.ix;
    tbl["id"] = fact.id.str();
    tbl["mass_g"] = fact.mass_g;
    tbl["volume_ml"] = fact.volume_ml;
    tbl["kcal"] = fact.kcal;
    tbl["hp"] = fact.hp;
    tbl["chg"] = fact.chg;
    tbl["proc"] = fact.proc.empty() ? sol::make_object( lua, sol::lua_nil ) : sol::make_object( lua,
                  fact.proc );

    auto tag_tbl = lua.create_table();
    auto flag_tbl = lua.create_table();
    auto mat_tbl = lua.create_table();
    auto vit_tbl = lua.create_table();
    auto qual_tbl = lua.create_table();

    auto idx = int{ 1 };
    std::ranges::for_each( fact.tag, [&]( const std::string & entry ) {
        tag_tbl[idx++] = entry;
    } );
    idx = 1;
    std::ranges::for_each( fact.flag, [&]( const flag_id & entry ) {
        flag_tbl[idx++] = entry.str();
    } );
    idx = 1;
    std::ranges::for_each( fact.mat, [&]( const material_id & entry ) {
        mat_tbl[idx++] = entry.str();
    } );
    std::ranges::for_each( fact.vit, [&]( const std::pair<const vitamin_id, int> &entry ) {
        vit_tbl[entry.first.str()] = entry.second;
    } );
    std::ranges::for_each( fact.qual, [&]( const std::pair<const quality_id, int> &entry ) {
        qual_tbl[entry.first.str()] = entry.second;
    } );

    tbl["tag"] = tag_tbl;
    tbl["flag"] = flag_tbl;
    tbl["mat"] = mat_tbl;
    tbl["vit"] = vit_tbl;
    tbl["qual"] = qual_tbl;
    return tbl;
}

auto blob_table( sol::state &lua, const proc::fast_blob &blob ) -> sol::table
{
    auto tbl = lua.create_table();
    tbl["mass_g"] = blob.mass_g;
    tbl["volume_ml"] = blob.volume_ml;
    tbl["kcal"] = blob.kcal;
    tbl["name"] = blob.name;
    tbl["description"] = blob.description;

    auto vit_tbl = lua.create_table();
    std::ranges::for_each( blob.vit, [&]( const std::pair<const vitamin_id, int> &entry ) {
        vit_tbl[entry.first.str()] = entry.second;
    } );
    tbl["vit"] = vit_tbl;

    auto melee_tbl = lua.create_table();
    melee_tbl["bash"] = blob.melee.bash;
    melee_tbl["cut"] = blob.melee.cut;
    melee_tbl["stab"] = blob.melee.stab;
    melee_tbl["to_hit"] = blob.melee.to_hit;
    melee_tbl["dur"] = blob.melee.dur;
    melee_tbl["moves"] = blob.melee.moves;
    tbl["melee"] = melee_tbl;
    return tbl;
}

auto slot_role( const proc::schema &sch, const proc::slot_id &slot ) -> std::string;

auto make_lua_params( sol::state &lua, const lua_params_options &opts ) -> sol::table
{
    auto params = lua.create_table();
    params["schema_id"] = opts.sch.id.str();
    params["schema_res"] = opts.sch.res.str();
    if( opts.result_override.has_value() ) {
        params["result_override"] = opts.result_override->str();
    }
    params["blob"] = blob_table( lua, opts.blob );

    auto facts_tbl = lua.create_table();
    auto idx = int { 1 };
    std::ranges::for_each( opts.facts, [&]( const proc::part_fact & fact ) {
        facts_tbl[idx++] = fact_table( lua, fact );
    } );
    params["facts"] = facts_tbl;

    auto picks_tbl = lua.create_table();
    idx = int { 1 };
    const auto picks = opts.picks == nullptr ? std::vector<proc::craft_pick> {} :
                       *opts.picks;
    std::ranges::for_each( picks, [&]( const proc::craft_pick & pick ) {
        auto pick_tbl = lua.create_table();
        pick_tbl["ix"] = pick.ix;
        pick_tbl["slot"] = pick.slot.str();
        pick_tbl["role"] = slot_role( opts.sch, pick.slot );
        picks_tbl[idx++] = pick_tbl;
    } );
    params["picks"] = picks_tbl;
    return params;
}

auto slot_role( const proc::schema &sch, const proc::slot_id &slot ) -> std::string
{
    const auto iter = std::ranges::find_if( sch.slots, [&]( const proc::slot_data & entry ) {
        return entry.id == slot;
    } );
    return iter == sch.slots.end() ? std::string {} :
           iter->role;
}

auto picks_from_slots( const std::vector<proc::part_fact> &facts,
                       const std::vector<proc::slot_id> &slots ) -> std::vector<proc::craft_pick>
{
    const auto count = std::min( facts.size(), slots.size() );
    auto ret = std::vector<proc::craft_pick> {};
    ret.reserve( count );
    std::ranges::for_each( std::views::iota( size_t { 0 }, count ), [&]( const size_t idx ) {
        ret.push_back( proc::craft_pick { .slot = slots[idx], .ix = facts[idx].ix } );
    } );
    return ret;
}

auto call_lua_blob( const std::string &path, const proc::schema &sch,
                    const std::vector<proc::part_fact> &facts,
                    const std::vector<proc::craft_pick> &picks, const proc::fast_blob &blob,
                    cata::lua_state *state,
                    const std::optional<itype_id> &result_override = std::nullopt ) -> std::optional<sol::table>
{
    const auto ref = resolve_lua_function( state, path );
    if( !ref ) {
        return std::nullopt;
    }

    auto &lua = ref->state->lua;
    const auto params = make_lua_params( lua, {
        .sch = sch,
        .facts = facts,
        .blob = blob,
        .picks = &picks,
        .result_override = result_override,
    } );

    auto res = ref->fn( params );
    check_func_result( res );
    if( !res.valid() || res.return_count() == 0 ) {
        return std::nullopt;
    }
    const auto obj = res.get<sol::object>();
    if( obj == sol::lua_nil || !obj.is<sol::table>() ) {
        return std::nullopt;
    }
    return obj.as<sol::table>();
}

auto call_lua_validate( const std::string &path, const proc::schema &sch,
                        const std::vector<proc::part_fact> &facts, const proc::fast_blob &blob,
                        cata::lua_state *state ) -> std::optional<lua_validate_result>
{
    const auto ref = resolve_lua_function( state, path );
    if( !ref ) {
        return std::nullopt;
    }

    auto &lua = ref->state->lua;
    const auto params = make_lua_params( lua, {
        .sch = sch,
        .facts = facts,
        .blob = blob,
    } );

    auto res = ref->fn( params );
    check_func_result( res );
    if( !res.valid() || res.return_count() == 0 ) {
        return lua_validate_result{};
    }
    const auto obj = res.get<sol::object>();
    if( obj == sol::lua_nil ) {
        return lua_validate_result{};
    }
    if( obj.is<bool>() ) {
        return lua_validate_result{ .ok = obj.as<bool>(), .reason = "" };
    }
    if( obj.is<std::string>() ) {
        const auto reason = obj.as<std::string>();
        return lua_validate_result{ .ok = reason.empty(), .reason = reason };
    }
    if( !obj.is<sol::table>() ) {
        return lua_validate_result{};
    }

    const auto tbl = obj.as<sol::table>();
    auto out = lua_validate_result{};
    const auto err_obj = tbl.get_or<sol::object>( "err", sol::lua_nil );
    if( err_obj != sol::lua_nil && err_obj.is<std::string>() ) {
        out.ok = false;
        out.reason = err_obj.as<std::string>();
        return out;
    }
    const auto ok_obj = tbl.get_or<sol::object>( "ok", sol::lua_nil );
    if( ok_obj != sol::lua_nil && ok_obj.is<bool>() ) {
        out.ok = ok_obj.as<bool>();
    }
    const auto reason_obj = tbl.get_or<sol::object>( "reason", sol::lua_nil );
    if( reason_obj != sol::lua_nil && reason_obj.is<std::string>() ) {
        out.reason = reason_obj.as<std::string>();
    }
    if( !out.ok && out.reason.empty() ) {
        out.reason = "invalid selection";
    }
    return out;
}

auto parse_blob_into( proc::fast_blob &blob, const sol::table &tbl ) -> void
{
    blob.mass_g = tbl.get_or( "mass_g", blob.mass_g );
    blob.volume_ml = tbl.get_or( "volume_ml", blob.volume_ml );
    blob.kcal = tbl.get_or( "kcal", blob.kcal );
    blob.name = tbl.get_or<std::string>( "name", blob.name );
    blob.description = tbl.get_or<std::string>( "description", blob.description );
    const auto vit_obj = tbl.get_or<sol::object>( "vit", sol::lua_nil );
    if( vit_obj != sol::lua_nil && vit_obj.is<sol::table>() ) {
        blob.vit.clear();
        const auto vit_tbl = vit_obj.as<sol::table>();
        std::ranges::for_each( vit_tbl, [&]( const std::pair<sol::object, sol::object> &entry ) {
            if( entry.first.is<std::string>() && entry.second.is<int>() ) {
                blob.vit[vitamin_id( entry.first.as<std::string>() )] = entry.second.as<int>();
            }
        } );
    }
    const auto melee_obj = tbl.get_or<sol::object>( "melee", sol::lua_nil );
    if( melee_obj != sol::lua_nil && melee_obj.is<sol::table>() ) {
        const auto melee_tbl = melee_obj.as<sol::table>();
        blob.melee.bash = melee_tbl.get_or( "bash", blob.melee.bash );
        blob.melee.cut = melee_tbl.get_or( "cut", blob.melee.cut );
        blob.melee.stab = melee_tbl.get_or( "stab", blob.melee.stab );
        blob.melee.to_hit = melee_tbl.get_or( "to_hit", blob.melee.to_hit );
        blob.melee.dur = melee_tbl.get_or( "dur", blob.melee.dur );
        blob.melee.moves = melee_tbl.get_or( "moves", blob.melee.moves );
    }
}

auto damage_from_hp( const item &it, const float hp ) -> int
{
    if( it.max_damage() <= 0 ) {
        return 0;
    }
    const auto clamped = std::clamp( hp, 0.0f, 1.0f );
    return std::clamp( static_cast<int>( ( 1.0f - clamped ) * static_cast<float>( it.max_damage() ) ),
                       0, it.max_damage() );
}

auto spawn_fact_item( const proc::part_fact &fact ) -> detached_ptr<item>
{
    auto spawned = item::spawn( fact.id, calendar::turn );
    if( spawned->max_damage() > 0 ) {
        spawned->set_damage( damage_from_hp( *spawned, fact.hp ) );
    }
    if( spawned->count_by_charges() && fact.chg > 0 ) {
        spawned->charges = fact.chg;
    }
    if( !fact.proc.empty() ) {
        proc::set_payload_from_json( *spawned, fact.proc );
    }
    return spawned;
}

} // namespace

auto proc::run_full( const schema &sch, const std::vector<part_fact> &facts,
                     const fast_blob &blob, const lua_opts &opts ) -> full_blob
{
    auto out = full_blob{ .data = blob };
    if( const auto tbl = call_lua_blob( sch.lua.full, sch, facts, opts.picks, blob, opts.state ) ) {
        parse_blob_into( out.data, *tbl );
    }
    if( const auto tbl = call_lua_blob( sch.lua.name, sch, facts, opts.picks, out.data,
                                        opts.state ) ) {
        parse_blob_into( out.data, *tbl );
    }
    return out;
}

auto proc::validate_selection( const schema &sch, const std::vector<part_fact> &facts,
                               const fast_blob &blob,
                               const validate_opts &opts ) -> std::expected<void, std::string>
{
    if( sch.lua.validate.empty() ) {
        return {};
    }
    const auto result = call_lua_validate( sch.lua.validate, sch, facts, blob, opts.state );
    if( !result.has_value() || result->ok ) {
        return {};
    }
    if( result->reason.empty() ) {
        return std::unexpected( std::string( "invalid selection" ) );
    }
    return std::unexpected( result->reason );
}

auto proc::make_item( const schema &sch, const std::vector<part_fact> &facts,
                      const make_opts &opts ) -> detached_ptr<item>
{
    auto preview = fast_blob{};
    auto lua_picks = std::vector<craft_pick> {};
    auto result_override = std::optional<itype_id> {};
    if( !opts.slots.empty() ) {
        auto state = build_state( sch, facts );
        const auto count = std::min( facts.size(), opts.slots.size() );
        std::ranges::for_each( std::views::iota( size_t{ 0 }, count ), [&]( const size_t idx ) {
            state.chosen[opts.slots[idx]].push_back( facts[idx].ix );
        } );
        const auto picks = selected_picks( state, sch );
        lua_picks = picks_from_slots( facts, opts.slots );
        result_override = proc::preview_result_override( sch, facts, picks );
        preview = rebuild_fast( state );
    } else {
        std::ranges::for_each( facts, [&]( const part_fact & fact ) {
            preview.mass_g += fact.mass_g;
            preview.volume_ml += fact.volume_ml;
            preview.kcal += fact.kcal;
            std::ranges::for_each( fact.vit, [&]( const std::pair<const vitamin_id, int> &entry ) {
                preview.vit[entry.first] += entry.second;
            } );
        } );
    }
    auto full = run_full( sch, facts, preview, { .state = opts.state, .picks = lua_picks } );
    auto mode = opts.mode;
    auto result = item::spawn( sch.res, calendar::turn );

    if( const auto tbl = call_lua_blob( sch.lua.make, sch, facts, lua_picks, full.data,
                                        opts.state,
                                        result_override ) ) {
        parse_blob_into( full.data, *tbl );
        const auto result_id = tbl->get_or<std::string>( "result", "" );
        if( !result_id.empty() ) {
            result->convert( itype_id( result_id ) );
        }
        const auto mode_name = tbl->get_or<std::string>( "mode", "" );
        if( !mode_name.empty() ) {
            mode = io::string_to_enum<proc::hist>( mode_name );
        }
    }

    if( full.data.name.empty() ) {
        full.data.name = !sch.cat.empty() ? sch.cat + " " + sch.id.str() : sch.id.str();
    }

    if( result->is_comestible() && result->count_by_charges() ) {
        result->charges = scaled_food_servings( *result, full.data );
    }

    auto out_payload = payload{};
    out_payload.id = sch.id;
    out_payload.mode = mode;
    out_payload.blob = full.data;
    out_payload.servings = result->is_comestible() && result->count_by_charges() ?
                           std::max( result->charges, 1 ) : 0;
    out_payload.fp = fast_fp( sch, full.data, facts );
    if( mode == hist::compact ) {
        out_payload.parts = !opts.slots.empty() ? make_compact_parts( facts, opts.slots ) :
                            make_compact_parts( facts, sch );
    }
    write_payload( *result, out_payload );

    if( mode == hist::full ) {
        auto &components = result->get_components();
        if( !opts.used.empty() ) {
            std::ranges::for_each( opts.used, [&]( const item * used ) {
                if( used != nullptr ) {
                    components.push_back( item::spawn( *used ) );
                }
            } );
        } else {
            std::ranges::for_each( facts, [&]( const part_fact & fact ) {
                components.push_back( spawn_fact_item( fact ) );
            } );
        }
    }

    return result;
}
