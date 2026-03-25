#include "procgen/proc_item.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "calendar.h"
#include "debug.h"
#include "flag.h"
#include "fstream_utils.h"
#include "item.h"
#include "itype.h"
#include "json.h"
#include "procgen/proc_fact.h"
#include "units_mass.h"
#include "units_volume.h"

namespace
{

struct legacy_sandwich_part_spec {
    std::string role;
    itype_id id = itype_id::NULL_ID();
    int count = 1;
};

struct legacy_weapon_part_spec {
    std::string role;
    itype_id id = itype_id::NULL_ID();
};

struct legacy_weapon_spec {
    proc::schema_id schema = proc::schema_id::NULL_ID();
    std::string fp_prefix;
    std::vector<legacy_weapon_part_spec> parts;
};

inline constexpr auto proc_var_key = std::string_view { "proc" };
inline constexpr auto proc_craft_var_key = std::string_view { "proc_craft" };

auto default_stack_servings( const itype_id &id ) -> int
{
    static auto cache = std::map<itype_id, int> {};
    if( const auto iter = cache.find( id ); iter != cache.end() ) {
        return iter->second;
    }

    const auto sample = item::spawn( id, calendar::turn );
    const auto servings = sample->count_by_charges() ? std::max( sample->charges, 1 ) : 1;
    cache.emplace( id, servings );
    return servings;
}

auto payload_servings( const item &it, const proc::payload &data ) -> int
{
    if( data.servings > 0 ) {
        return data.servings;
    }
    return it.is_comestible() && it.count_by_charges() ? default_stack_servings( it.typeId() ) : 0;
}

auto scaled_payload_total( const item &it, const proc::payload &data, const int total ) -> int
{
    if( !it.count_by_charges() ) {
        return total;
    }

    const auto servings = payload_servings( it, data );
    if( servings <= 0 ) {
        return total;
    }

    return static_cast<int>( std::lround( static_cast<double>( total ) *
                                          static_cast<double>( std::max( it.charges, 0 ) ) /
                                          static_cast<double>( servings ) ) );
}

auto per_serving_payload_value( const item &it, const proc::payload &data, const int total ) -> int
{
    if( !it.count_by_charges() ) {
        return total;
    }

    const auto servings = payload_servings( it, data );
    if( servings <= 0 ) {
        return total;
    }

    return static_cast<int>( std::lround( static_cast<double>( total ) /
                                          static_cast<double>( servings ) ) );
}

auto default_food_blob( const item &it,
                        const itype_id &source_id ) -> proc::fast_blob
{
    const auto sample = item::spawn( source_id, calendar::turn );
    auto blob = proc::fast_blob{};
    const auto sample_charges = sample->count_by_charges() ? std::max( sample->charges, 1 ) : 1;
    const auto current_charges = sample->count_by_charges() ? std::max( it.charges, 1 ) : 1;
    blob.mass_g = static_cast<int>( std::lround( static_cast<double>( units::to_gram(
                                        sample->weight() ) ) *
                                    static_cast<double>( current_charges ) /
                                    static_cast<double>( sample_charges ) ) );
    blob.volume_ml = static_cast<int>( std::lround( static_cast<double>( units::to_milliliter(
                                           sample->volume() ) ) *
                                       static_cast<double>( current_charges ) /
                                       static_cast<double>( sample_charges ) ) );
    blob.name = sample->type_name();
    blob.description = sample->type->description.translated();
    if( !sample->is_comestible() ) {
        return blob;
    }

    blob.kcal = sample->get_comestible()->default_nutrition.kcal * current_charges;
    blob.vit = sample->get_comestible()->default_nutrition.vitamins;
    std::ranges::for_each( blob.vit, [&]( std::pair<const vitamin_id, int> &entry ) {
        entry.second *= current_charges;
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

auto default_weapon_blob( const itype_id &source_id ) -> proc::fast_blob
{
    const auto source = item( source_id, calendar::turn );
    auto blob = proc::fast_blob{};
    blob.mass_g = units::to_gram( source.weight() );
    blob.volume_ml = units::to_milliliter( source.volume() );
    blob.name = source.type_name();
    blob.description = source.type->description.translated();
    blob.melee.bash = source.damage_melee( DT_BASH );
    blob.melee.cut = source.damage_melee( DT_CUT );
    blob.melee.stab = source.damage_melee( DT_STAB );
    blob.melee.to_hit = source.type->m_to_hit;
    blob.melee.dur = std::max( source.max_damage(), 0 );
    return blob;
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

auto make_legacy_weapon_fact( const legacy_weapon_part_spec &spec,
                              const proc::part_ix ix ) -> proc::part_fact
{
    const auto sample = item( spec.id, calendar::turn );
    return proc::normalize_part_fact( sample, {
        .ix = ix,
    } );
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
    if( id == itype_id( "sandwich_honey" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "cond", .id = itype_id( "honey_bottled" ), .count = 1 }
        };
    }
    if( id == itype_id( "sandwich_jam" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "cond", .id = itype_id( "jam_fruit" ), .count = 1 }
        };
    }
    if( id == itype_id( "sandwich_pb" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "cond", .id = itype_id( "peanutbutter" ), .count = 1 }
        };
    }
    if( id == itype_id( "sandwich_pbj" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "cond", .id = itype_id( "peanutbutter" ), .count = 1 },
            legacy_sandwich_part_spec{ .role = "cond", .id = itype_id( "jam_fruit" ), .count = 1 }
        };
    }
    if( id == itype_id( "sandwich_pbh" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "cond", .id = itype_id( "peanutbutter" ), .count = 1 },
            legacy_sandwich_part_spec{ .role = "cond", .id = itype_id( "honey_bottled" ), .count = 1 }
        };
    }
    if( id == itype_id( "sandwich_pbm" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "cond", .id = itype_id( "peanutbutter" ), .count = 1 },
            legacy_sandwich_part_spec{ .role = "cond", .id = itype_id( "syrup" ), .count = 1 }
        };
    }
    if( id == itype_id( "blt" ) ) {
        return {
            legacy_sandwich_part_spec{ .role = "bread", .id = itype_id( "bread" ), .count = 2 },
            legacy_sandwich_part_spec{ .role = "meat", .id = itype_id( "bacon" ), .count = 1 },
            legacy_sandwich_part_spec{ .role = "veg", .id = itype_id( "lettuce" ), .count = 1 },
            legacy_sandwich_part_spec{ .role = "veg", .id = itype_id( "tomato" ), .count = 1 }
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

auto legacy_weapon_specs( const itype_id &id ) -> std::optional<legacy_weapon_spec>
{
    if( id == itype_id( "sword_metal" ) ) {
        return legacy_weapon_spec{
            .schema = proc::schema_id( "sword" ),
            .fp_prefix = "sword",
            .parts = {
                legacy_weapon_part_spec{ .role = "blade", .id = itype_id( "steel_chunk" ) },
                legacy_weapon_part_spec{ .role = "guard", .id = itype_id( "steel_chunk" ) },
                legacy_weapon_part_spec{ .role = "handle", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "grip", .id = itype_id( "leather" ) }
            }
        };
    }
    if( id == itype_id( "sword_wood" ) ) {
        return legacy_weapon_spec{
            .schema = proc::schema_id( "sword" ),
            .fp_prefix = "sword",
            .parts = {
                legacy_weapon_part_spec{ .role = "blade", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "guard", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "handle", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "grip", .id = itype_id( "rag" ) }
            }
        };
    }
    if( id == itype_id( "sword_nail" ) ) {
        return legacy_weapon_spec{
            .schema = proc::schema_id( "sword" ),
            .fp_prefix = "sword",
            .parts = {
                legacy_weapon_part_spec{ .role = "blade", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "guard", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "handle", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "grip", .id = itype_id( "rag" ) },
                legacy_weapon_part_spec{ .role = "reinforcement", .id = itype_id( "nail" ) }
            }
        };
    }
    if( id == itype_id( "sword_crude" ) ) {
        return legacy_weapon_spec{
            .schema = proc::schema_id( "sword" ),
            .fp_prefix = "sword",
            .parts = {
                legacy_weapon_part_spec{ .role = "blade", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "guard", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "handle", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "grip", .id = itype_id( "rag" ) },
                legacy_weapon_part_spec{ .role = "reinforcement", .id = itype_id( "scrap" ) }
            }
        };
    }
    if( id == itype_id( "sword_bone" ) ) {
        return legacy_weapon_spec{
            .schema = proc::schema_id( "sword" ),
            .fp_prefix = "sword",
            .parts = {
                legacy_weapon_part_spec{ .role = "blade", .id = itype_id( "bone" ) },
                legacy_weapon_part_spec{ .role = "handle", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "grip", .id = itype_id( "leather" ) }
            }
        };
    }
    if( id == itype_id( "hand_axe" ) ) {
        return legacy_weapon_spec{
            .schema = proc::schema_id( "axe" ),
            .fp_prefix = "axe",
            .parts = {
                legacy_weapon_part_spec{ .role = "head", .id = itype_id( "rock" ) },
                legacy_weapon_part_spec{ .role = "handle", .id = itype_id( "stick_long" ) }
            }
        };
    }
    if( id == itype_id( "makeshift_axe" ) ) {
        return legacy_weapon_spec{
            .schema = proc::schema_id( "axe" ),
            .fp_prefix = "axe",
            .parts = {
                legacy_weapon_part_spec{ .role = "head", .id = itype_id( "steel_chunk" ) },
                legacy_weapon_part_spec{ .role = "handle", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "binding", .id = itype_id( "string_36" ) }
            }
        };
    }
    if( id == itype_id( "spear_spike" ) ) {
        return legacy_weapon_spec{
            .schema = proc::schema_id( "spear" ),
            .fp_prefix = "spear",
            .parts = {
                legacy_weapon_part_spec{ .role = "tip", .id = itype_id( "spike" ) },
                legacy_weapon_part_spec{ .role = "shaft", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "binding", .id = itype_id( "string_36" ) }
            }
        };
    }
    if( id == itype_id( "spear_knife" ) ) {
        return legacy_weapon_spec{
            .schema = proc::schema_id( "spear" ),
            .fp_prefix = "spear",
            .parts = {
                legacy_weapon_part_spec{ .role = "tip", .id = itype_id( "knife_hunting" ) },
                legacy_weapon_part_spec{ .role = "shaft", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "binding", .id = itype_id( "string_36" ) }
            }
        };
    }
    if( id == itype_id( "spear_knife_superior" ) ) {
        return legacy_weapon_spec{
            .schema = proc::schema_id( "spear" ),
            .fp_prefix = "spear",
            .parts = {
                legacy_weapon_part_spec{ .role = "tip", .id = itype_id( "knife_hunting" ) },
                legacy_weapon_part_spec{ .role = "shaft", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "binding", .id = itype_id( "string_36" ) }
            }
        };
    }
    if( id == itype_id( "spear_stone" ) ) {
        return legacy_weapon_spec{
            .schema = proc::schema_id( "spear" ),
            .fp_prefix = "spear",
            .parts = {
                legacy_weapon_part_spec{ .role = "tip", .id = itype_id( "rock" ) },
                legacy_weapon_part_spec{ .role = "shaft", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "binding", .id = itype_id( "string_36" ) }
            }
        };
    }
    if( id == itype_id( "spear_steel" ) ) {
        return legacy_weapon_spec{
            .schema = proc::schema_id( "spear" ),
            .fp_prefix = "spear",
            .parts = {
                legacy_weapon_part_spec{ .role = "tip", .id = itype_id( "steel_chunk" ) },
                legacy_weapon_part_spec{ .role = "shaft", .id = itype_id( "stick_long" ) },
                legacy_weapon_part_spec{ .role = "binding", .id = itype_id( "string_36" ) }
            }
        };
    }
    return std::nullopt;
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
    if( !data.blob.description.empty() ) {
        jsout.member( "description", data.blob.description );
    }
    jsout.member( "melee" );
    jsout.start_object();
    jsout.member( "bash", data.blob.melee.bash );
    jsout.member( "cut", data.blob.melee.cut );
    jsout.member( "stab", data.blob.melee.stab );
    jsout.member( "to_hit", data.blob.melee.to_hit );
    jsout.member( "dur", data.blob.melee.dur );
    jsout.member( "moves", data.blob.melee.moves );
    jsout.end_object();
    jsout.member( "vit" );
    jsout.start_object();
    std::ranges::for_each( data.blob.vit, [&]( const std::pair<const vitamin_id, int> &entry ) {
        jsout.member( entry.first.str(), entry.second );
    } );
    jsout.end_object();
    jsout.end_object();
    jsout.member( "servings", data.servings );
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
        blob_jo.read( "description", data.blob.description );
        if( blob_jo.has_object( "melee" ) ) {
            const auto melee_jo = blob_jo.get_object( "melee" );
            melee_jo.read( "bash", data.blob.melee.bash );
            melee_jo.read( "cut", data.blob.melee.cut );
            melee_jo.read( "stab", data.blob.melee.stab );
            melee_jo.read( "to_hit", data.blob.melee.to_hit );
            melee_jo.read( "dur", data.blob.melee.dur );
            melee_jo.read( "moves", data.blob.melee.moves );
        }
        if( blob_jo.has_object( "vit" ) ) {
            const auto vit_jo = blob_jo.get_object( "vit" );
            std::for_each( vit_jo.begin(), vit_jo.end(), [&]( const JsonMember & member ) {
                data.blob.vit[vitamin_id( member.name() )] = member.get_int();
            } );
        }
    }
    jo.read( "servings", data.servings );
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
    if( !data.blob.description.empty() ) {
        it.set_var( "description", data.blob.description );
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
    return legacy_sandwich_payload( it, it.typeId() );
}

auto legacy_sandwich_payload( const item &it,
                              const itype_id &legacy_id ) -> std::optional<payload>
{
    const auto specs = legacy_sandwich_specs( legacy_id );
    if( specs.empty() ) {
        return std::nullopt;
    }

    auto out = payload{};
    out.id = schema_id( "sandwich" );
    out.mode = hist::compact;
    out.fp = "sandwich:legacy:" + legacy_id.str();
    out.blob = default_food_blob( it, legacy_id );
    out.servings = it.count_by_charges() ? std::max( it.charges, 1 ) : 1;
    std::ranges::transform( specs,
    std::back_inserter( out.parts ), [&]( const legacy_sandwich_part_spec & spec ) {
        return make_legacy_sandwich_part( spec );
    } );
    return out;
}

auto legacy_weapon_payload( const item &,
                            const itype_id &legacy_id ) -> std::optional<payload>
{
    const auto spec = legacy_weapon_specs( legacy_id );
    if( !spec ) {
        return std::nullopt;
    }

    auto facts = std::vector<part_fact> {};
    auto slots = std::vector<slot_id> {};
    facts.reserve( spec->parts.size() );
    slots.reserve( spec->parts.size() );
    std::ranges::for_each( std::views::iota( size_t{ 0 }, spec->parts.size() ), [&](
    const size_t idx ) {
        facts.push_back( make_legacy_weapon_fact( spec->parts[idx], static_cast<part_ix>( idx ) ) );
        slots.push_back( slot_id( spec->parts[idx].role ) );
    } );

    auto out = payload{};
    out.id = spec->schema;
    out.mode = hist::compact;
    out.fp = spec->fp_prefix + ":legacy:" + legacy_id.str();
    out.blob = default_weapon_blob( legacy_id );
    out.parts = make_compact_parts( facts, slots );
    return out;
}

auto legacy_weapon_payload( const item &it ) -> std::optional<payload>
{
    return legacy_weapon_payload( it, it.typeId() );
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

auto proc::blob_kcal( const item &it ) -> std::optional<int>
{
    const auto payload = read_payload( it );
    return payload ? std::optional<int>( per_serving_payload_value( it, *payload,
                                         payload->blob.kcal ) ) :
           std::nullopt;
}

auto proc::blob_vitamins( const item &it ) -> std::optional<std::map<vitamin_id, int>>
{
    const auto payload = read_payload( it );
    if( !payload ) {
        return std::nullopt;
    }

    auto vitamins = payload->blob.vit;
    std::ranges::for_each( vitamins, [&]( std::pair<const vitamin_id, int> &entry ) {
        entry.second = per_serving_payload_value( it, *payload, entry.second );
    } );
    return vitamins;
}

auto proc::blob_mass( const item &it ) -> std::optional<int>
{
    const auto payload = read_payload( it );
    return payload ? std::optional<int>( scaled_payload_total( it, *payload, payload->blob.mass_g ) ) :
           std::nullopt;
}

auto proc::blob_volume( const item &it ) -> std::optional<int>
{
    const auto payload = read_payload( it );
    return payload ? std::optional<int>( scaled_payload_total( it, *payload,
                                         payload->blob.volume_ml ) ) : std::nullopt;
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
