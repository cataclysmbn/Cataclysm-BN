#include "proc_item.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

#include "calendar.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "debug.h"
#include "flag.h"
#include "fstream_utils.h"
#include "init.h"
#include "item.h"
#include "itype.h"
#include "json.h"
#include "proc_fact.h"
#include "units_mass.h"
#include "units_volume.h"

namespace
{

struct legacy_sandwich_part_spec {
    std::string role;
    itype_id id = itype_id::NULL_ID();
    int count = 1;
};

inline constexpr auto proc_var_key = std::string_view { "proc" };
inline constexpr auto proc_craft_var_key = std::string_view { "proc_craft" };

auto default_food_blob( const item &it ) -> proc::fast_blob
{
    auto blob = proc::fast_blob{};
    blob.mass_g = units::to_gram( it.weight() );
    blob.volume_ml = units::to_milliliter( it.volume() );
    blob.name = it.type_name();
    if( !it.is_comestible() ) {
        return blob;
    }

    const auto charges = std::max( it.charges, 1 );
    blob.kcal = it.get_comestible()->default_nutrition.kcal * charges;
    blob.vit = it.get_comestible()->default_nutrition.vitamins;
    std::ranges::for_each( blob.vit, [&]( std::pair<const vitamin_id, int> &entry ) {
        entry.second *= charges;
    } );
    return blob;
}

auto make_legacy_sandwich_part( const legacy_sandwich_part_spec &spec ) -> proc::compact_part
{
    const auto sample = item::spawn( spec.id, calendar::turn );
    auto part = proc::compact_part{};
    part.role = spec.role;
    part.id = spec.id;
    part.n = spec.count;
    part.hp = 1.0f;
    part.dmg = 0;
    part.chg = sample->count_by_charges() ? spec.count : 0;
    part.mat = sample->made_of();
    return part;
}

auto legacy_sandwich_specs( const itype_id &id ) -> std::vector<legacy_sandwich_part_spec>
{
    if( id == itype_id( "sandwich_t" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "meat", .id = itype_id( "meat_cooked" ), .count = 1 }
        };
    }
    if( id == itype_id( "sandwich_veggy" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "veg", .id = itype_id( "lettuce" ), .count = 1 }
        };
    }
    if( id == itype_id( "sandwich_cheese" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "cheese", .id = itype_id( "cheese" ), .count = 1 }
        };
    }
    if( id == itype_id( "sandwich_sauce" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "cond", .id = itype_id( "mustard" ), .count = 1 }
        };
    }
    if( id == itype_id( "sandwich_deluxe" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "meat", .id = itype_id( "meat_cooked" ), .count = 1 },
            legacy_sandwich_part_spec{ .role = "cheese", .id = itype_id( "cheese" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "veg", .id = itype_id( "lettuce" ), .count = 1 },
            legacy_sandwich_part_spec{ .role = "cond", .id = itype_id( "mustard" ), .count = 1 }
        };
    }
    if( id == itype_id( "sandwich_okay" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "meat", .id = itype_id( "meat_cooked" ), .count = 1 },
            legacy_sandwich_part_spec{ .role = "veg", .id = itype_id( "lettuce" ), .count = 1 }
        };
    }
    if( id == itype_id( "sandwich_deluxe_nocheese" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "meat", .id = itype_id( "meat_cooked" ), .count = 1 },
            legacy_sandwich_part_spec{ .role = "veg", .id = itype_id( "lettuce" ), .count = 1 },
            legacy_sandwich_part_spec{ .role = "cond", .id = itype_id( "mustard" ), .count = 1 }
        };
    }
    if( id == itype_id( "fish_sandwich" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "meat", .id = itype_id( "fish_cooked" ), .count = 1 },
            legacy_sandwich_part_spec{ .role = "veg", .id = itype_id( "lettuce" ), .count = 1 },
            legacy_sandwich_part_spec{ .role = "cond", .id = itype_id( "mustard" ), .count = 1 }
        };
    }
    if( id == itype_id( "sandwich_cucumber" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "veg", .id = itype_id( "cucumber" ), .count = 1 }
        };
    }
    return {};
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
    tbl["melee"] = melee_tbl;
    return tbl;
}

auto call_lua_blob( const std::string &path, const proc::schema &sch,
                    const std::vector<proc::part_fact> &facts, const proc::fast_blob &blob,
                    cata::lua_state *state ) -> std::optional<sol::table>
{
    auto *active = active_lua_state( state );
    if( active == nullptr ) {
        return std::nullopt;
    }
    auto &lua = active->lua;
    const auto fn = find_lua_function( lua, path );
    if( !fn ) {
        return std::nullopt;
    }

    auto params = lua.create_table();
    params["schema_id"] = sch.id.str();
    params["schema_res"] = sch.res.str();
    params["blob"] = blob_table( lua, blob );

    auto facts_tbl = lua.create_table();
    auto idx = int{ 1 };
    std::ranges::for_each( facts, [&]( const proc::part_fact & fact ) {
        facts_tbl[idx++] = fact_table( lua, fact );
    } );
    params["facts"] = facts_tbl;

    auto res = ( *fn )( params );
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

auto parse_blob_into( proc::fast_blob &blob, const sol::table &tbl ) -> void
{
    blob.mass_g = tbl.get_or( "mass_g", blob.mass_g );
    blob.volume_ml = tbl.get_or( "volume_ml", blob.volume_ml );
    blob.kcal = tbl.get_or( "kcal", blob.kcal );
    blob.name = tbl.get_or<std::string>( "name", blob.name );
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

auto to_json( JsonOut &jsout, const proc::part_fact &fact ) -> void
{
    jsout.start_object();
    jsout.member( "ix", fact.ix );
    jsout.member( "id", fact.id );
    jsout.member( "tag", fact.tag );
    jsout.member( "flag" );
    jsout.start_array();
    std::ranges::for_each( fact.flag, [&]( const flag_id & entry ) {
        jsout.write( entry );
    } );
    jsout.end_array();
    jsout.member( "qual" );
    jsout.start_object();
    std::ranges::for_each( fact.qual, [&]( const std::pair<const quality_id, int> &entry ) {
        jsout.member( entry.first.str(), entry.second );
    } );
    jsout.end_object();
    jsout.member( "mat" );
    jsout.start_array();
    std::ranges::for_each( fact.mat, [&]( const material_id & entry ) {
        jsout.write( entry );
    } );
    jsout.end_array();
    jsout.member( "vit" );
    jsout.start_object();
    std::ranges::for_each( fact.vit, [&]( const std::pair<const vitamin_id, int> &entry ) {
        jsout.member( entry.first.str(), entry.second );
    } );
    jsout.end_object();
    jsout.member( "mass_g", fact.mass_g );
    jsout.member( "volume_ml", fact.volume_ml );
    jsout.member( "kcal", fact.kcal );
    jsout.member( "hp", fact.hp );
    jsout.member( "chg", fact.chg );
    jsout.member( "uses", fact.uses );
    if( !fact.proc.empty() ) {
        jsout.member( "proc", fact.proc );
    }
    jsout.end_object();
}

auto from_json( JsonObject &jo, proc::part_fact &fact ) -> void
{
    jo.read( "ix", fact.ix );
    jo.read( "id", fact.id );
    jo.read( "tag", fact.tag );
    if( jo.has_array( "flag" ) ) {
        auto arr = jo.get_array( "flag" );
        while( arr.has_more() ) {
            fact.flag.push_back( flag_id( arr.next_string() ) );
        }
    }
    if( jo.has_object( "qual" ) ) {
        const auto qual_jo = jo.get_object( "qual" );
        for( const JsonMember &member : qual_jo ) {
            fact.qual.emplace( quality_id( member.name() ), member.get_int() );
        }
    }
    if( jo.has_array( "mat" ) ) {
        auto arr = jo.get_array( "mat" );
        while( arr.has_more() ) {
            fact.mat.push_back( material_id( arr.next_string() ) );
        }
    }
    if( jo.has_object( "vit" ) ) {
        const auto vit_jo = jo.get_object( "vit" );
        for( const JsonMember &member : vit_jo ) {
            fact.vit.emplace( vitamin_id( member.name() ), member.get_int() );
        }
    }
    jo.read( "mass_g", fact.mass_g );
    jo.read( "volume_ml", fact.volume_ml );
    jo.read( "kcal", fact.kcal );
    jo.read( "hp", fact.hp );
    jo.read( "chg", fact.chg );
    jo.read( "uses", fact.uses );
    jo.read( "proc", fact.proc );
}

} // namespace

auto proc::to_json( JsonOut &jsout, const compact_part &part ) -> void
{
    jsout.start_object();
    jsout.member( "role", part.role );
    jsout.member( "id", part.id );
    jsout.member( "n", part.n );
    jsout.member( "hp", part.hp );
    jsout.member( "dmg", part.dmg );
    jsout.member( "chg", part.chg );
    jsout.member( "mat" );
    jsout.start_array();
    std::ranges::for_each( part.mat, [&]( const material_id & entry ) {
        jsout.write( entry );
    } );
    jsout.end_array();
    if( !part.proc.empty() ) {
        jsout.member( "proc", part.proc );
    }
    jsout.end_object();
}

auto proc::from_json( JsonIn &jsin, compact_part &part ) -> void
{
    const auto jo = jsin.get_object();
    jo.read( "role", part.role );
    jo.read( "id", part.id );
    jo.read( "n", part.n );
    jo.read( "hp", part.hp );
    jo.read( "dmg", part.dmg );
    jo.read( "chg", part.chg );
    if( jo.has_array( "mat" ) ) {
        auto arr = jo.get_array( "mat" );
        while( arr.has_more() ) {
            part.mat.push_back( material_id( arr.next_string() ) );
        }
    }
    jo.read( "proc", part.proc );
}

auto proc::to_json( JsonOut &jsout, const payload &data ) -> void
{
    jsout.start_object();
    jsout.member( "id", data.id );
    jsout.member( "mode", io::enum_to_string( data.mode ) );
    jsout.member( "fp", data.fp );
    jsout.member( "blob" );
    jsout.start_object();
    jsout.member( "mass_g", data.blob.mass_g );
    jsout.member( "volume_ml", data.blob.volume_ml );
    jsout.member( "kcal", data.blob.kcal );
    jsout.member( "name", data.blob.name );
    jsout.member( "melee" );
    jsout.start_object();
    jsout.member( "bash", data.blob.melee.bash );
    jsout.member( "cut", data.blob.melee.cut );
    jsout.member( "stab", data.blob.melee.stab );
    jsout.member( "to_hit", data.blob.melee.to_hit );
    jsout.member( "dur", data.blob.melee.dur );
    jsout.end_object();
    jsout.member( "vit" );
    jsout.start_object();
    std::ranges::for_each( data.blob.vit, [&]( const std::pair<const vitamin_id, int> &entry ) {
        jsout.member( entry.first.str(), entry.second );
    } );
    jsout.end_object();
    jsout.end_object();
    jsout.member( "parts" );
    jsout.start_array();
    std::ranges::for_each( data.parts, [&]( const compact_part & entry ) {
        to_json( jsout, entry );
    } );
    jsout.end_array();
    jsout.end_object();
}

auto proc::from_json( JsonIn &jsin, payload &data ) -> void
{
    const auto jo = jsin.get_object();
    jo.read( "id", data.id );
    data.mode = jo.has_member( "mode" ) ? io::string_to_enum<proc::hist>( jo.get_string( "mode" ) ) :
                proc::hist::none;
    jo.read( "fp", data.fp );
    if( jo.has_object( "blob" ) ) {
        const auto blob_jo = jo.get_object( "blob" );
        blob_jo.read( "mass_g", data.blob.mass_g );
        blob_jo.read( "volume_ml", data.blob.volume_ml );
        blob_jo.read( "kcal", data.blob.kcal );
        blob_jo.read( "name", data.blob.name );
        if( blob_jo.has_object( "melee" ) ) {
            const auto melee_jo = blob_jo.get_object( "melee" );
            melee_jo.read( "bash", data.blob.melee.bash );
            melee_jo.read( "cut", data.blob.melee.cut );
            melee_jo.read( "stab", data.blob.melee.stab );
            melee_jo.read( "to_hit", data.blob.melee.to_hit );
            melee_jo.read( "dur", data.blob.melee.dur );
        }
        if( blob_jo.has_object( "vit" ) ) {
            const auto vit_jo = blob_jo.get_object( "vit" );
            std::for_each( vit_jo.begin(), vit_jo.end(), [&]( const JsonMember & member ) {
                data.blob.vit[vitamin_id( member.name() )] = member.get_int();
            } );
        }
    }
    if( jo.has_array( "parts" ) ) {
        auto arr = jo.get_array( "parts" );
        while( arr.has_more() ) {
            auto part = compact_part{};
            const auto part_jo = arr.next_object();
            part_jo.read( "role", part.role );
            part_jo.read( "id", part.id );
            part_jo.read( "n", part.n );
            part_jo.read( "hp", part.hp );
            part_jo.read( "dmg", part.dmg );
            part_jo.read( "chg", part.chg );
            if( part_jo.has_array( "mat" ) ) {
                auto mat_arr = part_jo.get_array( "mat" );
                while( mat_arr.has_more() ) {
                    part.mat.push_back( material_id( mat_arr.next_string() ) );
                }
            }
            part_jo.read( "proc", part.proc );
            data.parts.push_back( std::move( part ) );
        }
    }
}

auto proc::to_json( JsonOut &jsout, const craft_plan &data ) -> void
{
    jsout.start_object();
    jsout.member( "mode", io::enum_to_string( data.mode ) );
    jsout.member( "slots" );
    jsout.start_array();
    std::ranges::for_each( data.slots, [&]( const slot_id & slot ) {
        jsout.write( slot );
    } );
    jsout.end_array();
    jsout.member( "facts" );
    jsout.start_array();
    std::ranges::for_each( data.facts, [&]( const part_fact & fact ) {
        ::to_json( jsout, fact );
    } );
    jsout.end_array();
    jsout.end_object();
}

auto proc::from_json( JsonIn &jsin, craft_plan &data ) -> void
{
    const auto jo = jsin.get_object();
    data.mode = jo.has_member( "mode" ) ? io::string_to_enum<proc::hist>( jo.get_string( "mode" ) ) :
                proc::hist::none;
    if( jo.has_array( "slots" ) ) {
        auto arr = jo.get_array( "slots" );
        while( arr.has_more() ) {
            data.slots.push_back( slot_id( arr.next_string() ) );
        }
    }
    if( jo.has_array( "facts" ) ) {
        auto arr = jo.get_array( "facts" );
        while( arr.has_more() ) {
            auto fact = part_fact {};
            auto fact_jo = arr.next_object();
            ::from_json( fact_jo, fact );
            data.facts.push_back( std::move( fact ) );
        }
    }
}

auto proc::payload_json( const payload &data ) -> std::string
{
    return serialize_wrapper( [&]( JsonOut & jsout ) {
        to_json( jsout, data );
    } );
}

auto proc::read_payload( const item &it ) -> std::optional<payload>
{
    if( !it.has_var( std::string( proc_var_key ) ) ) {
        return std::nullopt;
    }

    auto ret = payload{};
    try {
        deserialize_wrapper( [&]( JsonIn & jsin ) {
            from_json( jsin, ret );
        }, it.get_var( std::string( proc_var_key ) ) );
    } catch( const JsonError &err ) {
        debugmsg( "Failed to read proc payload: %s", err.what() );
        return std::nullopt;
    }
    return ret;
}

auto proc::read_craft_plan( const item &it ) -> std::optional<craft_plan>
{
    if( !it.has_var( std::string( proc_craft_var_key ) ) ) {
        return std::nullopt;
    }

    auto ret = craft_plan{};
    try {
        deserialize_wrapper( [&]( JsonIn & jsin ) {
            from_json( jsin, ret );
        }, it.get_var( std::string( proc_craft_var_key ) ) );
    } catch( const JsonError &err ) {
        debugmsg( "Failed to read proc craft plan: %s", err.what() );
        return std::nullopt;
    }
    return ret;
}

auto proc::write_payload( item &it, const payload &data ) -> void
{
    it.set_var( std::string( proc_var_key ), payload_json( data ) );
    if( !data.blob.name.empty() ) {
        it.set_var( "name", data.blob.name );
    }
    if( it.is_comestible() && !data.blob.empty() ) {
        it.set_flag( flag_NUTRIENT_OVERRIDE );
    }
}

auto proc::write_craft_plan( item &it, const craft_plan &data ) -> void
{
    it.set_var( std::string( proc_craft_var_key ), serialize_wrapper( [&]( JsonOut & jsout ) {
        to_json( jsout, data );
    } ) );
}

namespace proc
{

auto legacy_sandwich_payload( const item &it ) -> std::optional<payload>
{
    const auto specs = legacy_sandwich_specs( it.typeId() );
    if( specs.empty() ) {
        return std::nullopt;
    }

    auto out = payload{};
    out.id = schema_id( "sandwich" );
    out.mode = hist::compact;
    out.fp = "sandwich:legacy:" + it.typeId().str();
    out.blob = default_food_blob( it );
    std::ranges::transform( specs,
    std::back_inserter( out.parts ), [&]( const legacy_sandwich_part_spec & spec ) {
        return make_legacy_sandwich_part( spec );
    } );
    return out;
}

} // namespace proc

auto proc::set_payload_from_json( item &it, const std::string &json ) -> void
{
    it.set_var( std::string( proc_var_key ), json );
}

auto proc::clear_payload( item &it ) -> void
{
    it.erase_var( std::string( proc_var_key ) );
}

auto proc::make_compact_parts( const std::vector<part_fact> &facts,
                               const schema &sch ) -> std::vector<compact_part>
{
    auto ret = std::vector<compact_part> {};
    std::ranges::for_each( sch.slots, [&]( const slot_data & slot ) {
        std::ranges::for_each( facts, [&]( const part_fact & fact ) {
            if( !matches_slot( slot, fact ) ) {
                return;
            }
            auto part = compact_part{};
            part.role = slot.role;
            part.id = fact.id;
            part.n = std::max( fact.chg, 1 );
            part.hp = fact.hp;
            part.dmg = 0;
            part.chg = fact.chg;
            part.mat = fact.mat;
            part.proc = fact.proc;
            ret.push_back( part );
        } );
    } );
    return ret;
}

auto proc::make_compact_parts( const std::vector<part_fact> &facts,
                               const std::vector<slot_id> &slots ) -> std::vector<compact_part>
{
    auto ret = std::vector<compact_part> {};
    const auto count = std::min( facts.size(), slots.size() );
    std::ranges::for_each( std::views::iota( size_t{ 0 }, count ), [&]( const size_t idx ) {
        auto part = compact_part{};
        part.role = slots[idx].str();
        part.id = facts[idx].id;
        part.n = std::max( facts[idx].chg, 1 );
        part.hp = facts[idx].hp;
        part.dmg = 0;
        part.chg = facts[idx].chg;
        part.mat = facts[idx].mat;
        part.proc = facts[idx].proc;
        ret.push_back( part );
    } );
    return ret;
}

auto proc::restore_parts( const payload &data ) -> std::vector<detached_ptr<item>>
{
    auto ret = std::vector<detached_ptr<item>> {};
    std::ranges::for_each( data.parts, [&]( const compact_part & part ) {
        auto spawned = item::spawn( part.id, calendar::turn );
        if( spawned->max_damage() > 0 ) {
            const auto damage = part.dmg > 0 ? part.dmg : damage_from_hp( *spawned, part.hp );
            spawned->set_damage( damage );
        }
        if( spawned->count_by_charges() && part.chg > 0 ) {
            spawned->charges = part.chg;
        }
        if( !part.proc.empty() ) {
            set_payload_from_json( *spawned, part.proc );
        }
        ret.push_back( std::move( spawned ) );
    } );
    return ret;
}

auto proc::apply_on_damage( item &it, const int qty ) -> void
{
    if( qty <= 0 ) {
        return;
    }
    const auto payload = read_payload( it );
    if( !payload || payload->mode != hist::compact ) {
        return;
    }

    auto updated = *payload;
    const auto max_damage = std::max( it.max_damage(), 1 );
    std::ranges::for_each( updated.parts, [&]( compact_part & part ) {
        part.dmg += qty;
        part.hp = std::max( 0.0f, part.hp - static_cast<float>( qty ) / static_cast<float>( max_damage ) );
    } );
    write_payload( it, updated );
}

auto proc::run_full( const schema &sch, const std::vector<part_fact> &facts,
                     const fast_blob &blob, const lua_opts &opts ) -> full_blob
{
    auto out = full_blob{ .data = blob };
    if( const auto tbl = call_lua_blob( sch.lua.full, sch, facts, blob, opts.state ) ) {
        parse_blob_into( out.data, *tbl );
    }
    if( const auto tbl = call_lua_blob( sch.lua.name, sch, facts, out.data, opts.state ) ) {
        parse_blob_into( out.data, *tbl );
    }
    return out;
}

auto proc::make_item( const schema &sch, const std::vector<part_fact> &facts,
                      const make_opts &opts ) -> detached_ptr<item>
{
    auto preview = fast_blob{};
    if( !opts.slots.empty() ) {
        auto state = build_state( sch, facts );
        const auto count = std::min( facts.size(), opts.slots.size() );
        std::ranges::for_each( std::views::iota( size_t{ 0 }, count ), [&]( const size_t idx ) {
            state.chosen[opts.slots[idx]].push_back( facts[idx].ix );
        } );
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
    auto full = run_full( sch, facts, preview, { .state = opts.state } );
    auto mode = opts.mode;
    auto result = item::spawn( sch.res, calendar::turn );

    if( const auto tbl = call_lua_blob( sch.lua.make, sch, facts, full.data, opts.state ) ) {
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

    auto out_payload = payload{};
    out_payload.id = sch.id;
    out_payload.mode = mode;
    out_payload.blob = full.data;
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

auto proc::blob_kcal( const item &it ) -> std::optional<int>
{
    const auto payload = read_payload( it );
    return payload ? std::optional<int>( payload->blob.kcal ) : std::nullopt;
}

auto proc::blob_vitamins( const item &it ) -> std::optional<std::map<vitamin_id, int>>
{
    const auto payload = read_payload( it );
    return payload ? std::optional<std::map<vitamin_id, int>>( payload->blob.vit ) : std::nullopt;
}

auto proc::blob_mass( const item &it ) -> std::optional<int>
{
    const auto payload = read_payload( it );
    return payload ? std::optional<int>( payload->blob.mass_g ) : std::nullopt;
}

auto proc::blob_volume( const item &it ) -> std::optional<int>
{
    const auto payload = read_payload( it );
    return payload ? std::optional<int>( payload->blob.volume_ml ) : std::nullopt;
}

auto proc::blob_melee( const item &it ) -> std::optional<melee_blob>
{
    const auto payload = read_payload( it );
    return payload ? std::optional<melee_blob>( payload->blob.melee ) : std::nullopt;
}

auto proc::component_hash( const item &it ) -> std::optional<std::uint64_t>
{
    const auto payload = read_payload( it );
    if( !payload ) {
        return std::nullopt;
    }
    return std::hash<std::string> {}( payload->fp );
}
