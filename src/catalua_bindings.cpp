#include "catalua_bindings.h"

#include "bodypart.h"
#include "calendar.h"
#include "catalua_bindings_utils.h"
#include "catalua.h"
#include "catalua_log.h"
#include "catalua_luna_doc.h"
#include "catalua_luna.h"
#include "character.h"
#include "creature.h"
#include "damage.h"
#include "distribution_grid.h"
#include "enum_conversions.h"
#include "enums.h"
#include "field_type.h"
#include "game.h"
#include "itype.h"
#include "map.h"
#include "martialarts.h"
#include "material.h"
#include "mission.h"
#include "monfaction.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "player.h"
#include "rng.h"
#include "skill.h"
#include "sounds.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "units_angle.h"
#include "units_energy.h"
#include "units_mass.h"
#include "units_volume.h"

std::string_view luna::detail::current_comment;

std::string cata::detail::fmt_lua_va( sol::variadic_args va )
{
    lua_State *L = va.lua_state();
    sol::state_view lua( L );

    std::string msg;
    for( auto it : va ) {
        msg += lua["tostring"]( it );
    }

    return msg;
}

void cata::detail::reg_units( sol::state &lua )
{
    {
        sol::usertype<units::angle> ut =
            luna::new_usertype<units::angle>(
                lua,
                luna::no_bases,
                luna::no_constructor
            );

        luna::set_fx( ut, "from_radians", &units::from_radians<double> );
        luna::set_fx( ut, "to_radians", &units::to_radians );
        luna::set_fx( ut, "from_degrees", &units::from_degrees<double> );
        luna::set_fx( ut, "to_degrees", &units::to_degrees );
        luna::set_fx( ut, "from_arcmin", &units::from_arcmin<double> );
        luna::set_fx( ut, "to_arcmin", &units::to_arcmin );

        luna::set_fx( ut, sol::meta_function::equal_to, &units::angle::operator== );
        luna::set_fx( ut, sol::meta_function::less_than, &units::angle::operator< );
        luna::set_fx( ut, sol::meta_function::less_than_or_equal_to, &units::angle::operator<= );
    }
    {
        sol::usertype<units::energy> ut =
            luna::new_usertype<units::energy>(
                lua,
                luna::no_bases,
                luna::no_constructor
            );

        luna::set_fx( ut, "from_joule", &units::from_joule<int> );
        luna::set_fx( ut, "to_joule", &units::to_joule<int> );
        luna::set_fx( ut, "from_kilojoule", &units::from_kilojoule<int> );
        luna::set_fx( ut, "to_kilojoule", &units::to_kilojoule<int> );

        luna::set_fx( ut, sol::meta_function::equal_to, &units::energy::operator== );
        luna::set_fx( ut, sol::meta_function::less_than, &units::energy::operator< );
        luna::set_fx( ut, sol::meta_function::less_than_or_equal_to, &units::energy::operator<= );
    }
    {
        sol::usertype<units::mass> ut =
            luna::new_usertype<units::mass>(
                lua,
                luna::no_bases,
                luna::no_constructor
            );

        luna::set_fx( ut, "from_milligram", &units::from_milligram<std::int64_t> );
        luna::set_fx( ut, "to_milligram", &units::to_milligram<std::int64_t> );
        luna::set_fx( ut, "from_gram", &units::from_gram<std::int64_t> );
        luna::set_fx( ut, "to_gram", &units::to_gram<std::int64_t> );
        luna::set_fx( ut, "from_kilogram", &units::from_kilogram<std::int64_t> );
        luna::set_fx( ut, "to_kilogram", &units::to_kilogram<std::int64_t> );
        luna::set_fx( ut, "from_newton", &units::from_newton<std::int64_t> );
        luna::set_fx( ut, "to_newton", &units::to_newton<std::int64_t> );

        luna::set_fx( ut, sol::meta_function::equal_to, &units::mass::operator== );
        luna::set_fx( ut, sol::meta_function::less_than, &units::mass::operator< );
        luna::set_fx( ut, sol::meta_function::less_than_or_equal_to, &units::mass::operator<= );
    }
    {
        sol::usertype<units::volume> ut =
            luna::new_usertype<units::volume>(
                lua,
                luna::no_bases,
                luna::no_constructor
            );

        luna::set_fx( ut, "from_milliliter", &units::from_milliliter<int> );
        luna::set_fx( ut, "from_liter", &units::from_liter<int> );
        luna::set_fx( ut, "to_milliliter", &units::to_milliliter<int> );
        luna::set_fx( ut, "to_liter", &units::to_liter );

        luna::set_fx( ut, sol::meta_function::equal_to, &units::volume::operator== );
        luna::set_fx( ut, sol::meta_function::less_than, &units::volume::operator< );
        luna::set_fx( ut, sol::meta_function::less_than_or_equal_to, &units::volume::operator<= );
    }
}

void cata::detail::reg_skill_level_map( sol::state &lua )
{
    {
        sol::usertype<SkillLevel> ut =
            luna::new_usertype<SkillLevel>(
                lua,
                luna::no_bases,
                luna::no_constructor
            );

        luna::set_fx( ut, "is_training", &SkillLevel::isTraining );
        luna::set_fx( ut, "level", sol::resolve<int() const>( &SkillLevel::level ) );
        luna::set_fx( ut, "highest_level", &SkillLevel::highestLevel );
        luna::set_fx( ut, "train", &SkillLevel::train );
        luna::set_fx( ut, "can_train", &SkillLevel::can_train );
    }
    {
        sol::usertype<SkillLevelMap> ut =
            luna::new_usertype<SkillLevelMap>(
                lua,
                luna::bases<std::map<skill_id, SkillLevel>>(),
                luna::no_constructor
            );
        luna::set_fx( ut, "mod_skill_level", &SkillLevelMap::mod_skill_level );
        luna::set_fx( ut, "get_skill_level",
                      sol::resolve<int( const skill_id & ) const>
                      ( &SkillLevelMap::get_skill_level ) );
        luna::set_fx( ut, "get_skill_level_object",
                      sol::resolve<SkillLevel &( const skill_id & )>
                      ( &SkillLevelMap::get_skill_level_object ) );
    }
}

void cata::detail::reg_damage_instance( sol::state &lua )
{
#define UT_CLASS damage_unit
    {
        DOC( "Represents a damage amount" );
        DOC( "Constructors are:" );
        DOC( "new()" );
        DOC( "new(damageType, amount, armorPen, remainingArmorMultiplier, damageMultiplier)" );
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::constructors <
            UT_CLASS( damage_type, float, float, float, float )
            > ()
        );

        SET_MEMB( type );
        SET_MEMB( amount );
        SET_MEMB( res_pen );
        SET_MEMB( res_mult );
        SET_MEMB( damage_multiplier );

        luna::set_fx( ut, sol::meta_function::equal_to, &UT_CLASS::operator== );

    }
#undef UT_CLASS // #define UT_CLASS damage_unit
#define UT_CLASS damage_instance
    {
        DOC( "Represents a bunch of damage amounts" );
        DOC( "Constructors are:" );
        DOC( "new(damageType, amount, armorPen, remainingArmorMultiplier, damageMultiplier)" );
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::constructors <
            UT_CLASS(),
            UT_CLASS( damage_type, float, float, float, float )
            > ()
        );

        SET_MEMB( damage_units );

        SET_FX( mult_damage );
        SET_FX( type_damage );
        SET_FX( total_damage );
        SET_FX( clear );
        SET_FX( empty );
        SET_FX( add_damage );
        SET_FX_T( add, void( const damage_unit & ) );

        luna::set_fx( ut, sol::meta_function::equal_to, &UT_CLASS::operator== );
    }
#undef UT_CLASS // #define UT_CLASS damage_instance
#define UT_CLASS dealt_damage_instance
    {
        DOC( "Represents the final dealt damage" );
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        SET_MEMB( dealt_dams );
        SET_MEMB( bp_hit );

        SET_FX( type_damage );
        SET_FX( total_damage );
    }
#undef UT_CLASS // #define UT_CLASS dealt_damage_instance
}

void cata::detail::reg_technique( sol::state &lua )
{
    {
        sol::usertype<ma_technique> ut =
            luna::new_usertype<ma_technique>(
                lua,
                luna::no_bases,
                luna::no_constructor
            );

        luna::set_fx( ut, "name", &ma_technique::name );
        luna::set_fx( ut, "get_description", &ma_technique::get_description );
        luna::set_fx( ut, "avatar_message", &ma_technique::avatar_message );
        luna::set_fx( ut, "npc_message", &ma_technique::npc_message );
        luna::set_fx( ut, "defensive", &ma_technique::defensive );
        luna::set_fx( ut, "side_switch", &ma_technique::side_switch );
        luna::set_fx( ut, "down_dur", &ma_technique::down_dur );
        luna::set_fx( ut, "stun_dur", &ma_technique::stun_dur );
        luna::set_fx( ut, "knockback_dist", &ma_technique::knockback_dist );
        luna::set_fx( ut, "knockback_spread", &ma_technique::knockback_spread );
        luna::set_fx( ut, "powerful_knockback", &ma_technique::powerful_knockback );
        luna::set_fx( ut, "crit_tec", &ma_technique::crit_tec );
        luna::set_fx( ut, "crit_ok", &ma_technique::crit_ok );
        luna::set_fx( ut, "knockback_follow", &ma_technique::knockback_follow );
        luna::set_fx( ut,  "disarms", &ma_technique::disarms );
        luna::set_fx( ut,  "take_weapon", &ma_technique::take_weapon );
        luna::set_fx( ut,  "dodge_counter", &ma_technique::dodge_counter );
        luna::set_fx( ut,  "block_counter", &ma_technique::block_counter );
        luna::set_fx( ut,  "miss_recovery", &ma_technique::miss_recovery );
        luna::set_fx( ut,  "grab_break", &ma_technique::grab_break );

    }
}

void cata::detail::reg_item( sol::state &lua )
{
#define UT_CLASS item
    {
        sol::usertype<item> ut = luna::new_usertype<item>( lua, luna::no_bases, luna::no_constructor );

        luna::set_fx( ut, "get_type", &item::typeId );
        DOC( "Almost for a corpse." );
        luna::set_fx( ut, "get_mtype",
        []( const item & it ) { return it.get_mtype() ? it.get_mtype()->id : mtype_id::NULL_ID(); } );

        DOC( "Translated item name with prefixes" );
        luna::set_fx( ut, "tname", &item::tname );

        DOC( "Display name with all bells and whistles like ammo and prefixes" );
        luna::set_fx( ut, "display_name", &item::display_name );

        DOC( "Weight of the item. The first `bool` is whether including contents, second `bool` is whether it is `integral_weight`." );
        luna::set_fx( ut, "weight", [](
                          item & it,
                          sol::optional<bool> include_contents,
                          sol::optional<bool> integral
        ) { return it.weight( include_contents.value_or( true ), integral.value_or( false ) ); } );

        DOC( "Volume of the item. `bool` is whether it is `integral_volume`." );
        luna::set_fx( ut, "volume",
        []( item & it, sol::optional<bool> integral ) { return it.volume( integral.value_or( false ) ); } );

        DOC( "Cents of the item. `bool` is whether it is a post-cataclysm value." );
        luna::set_fx( ut, "price", &item::price );

        DOC( "Check for variable of any type" );
        luna::set_fx( ut, "has_var", &item::has_var );
        DOC( "Erase variable" );
        luna::set_fx( ut, "erase_var", &item::erase_var );
        DOC( "Erase all variables" );
        luna::set_fx( ut, "clear_vars", &item::clear_vars );

        luna::set_fx( ut, "is_null", &item::is_null );
        luna::set_fx( ut, "is_unarmed_weapon", &item::is_unarmed_weapon );
        luna::set_fx( ut, "is_sided", &item::is_sided );
        luna::set_fx( ut, "is_power_armor", &item::is_power_armor );
        luna::set_fx( ut, "is_money", &item::is_money );
        luna::set_fx( ut, "is_gun", &item::is_gun );
        luna::set_fx( ut, "is_firearm", &item::is_firearm );
        luna::set_fx( ut, "is_silent", &item::is_silent );
        luna::set_fx( ut, "is_gunmod", &item::is_gunmod );
        luna::set_fx( ut, "is_bionic", &item::is_bionic );
        luna::set_fx( ut, "is_ammo_belt", &item::is_ammo_belt );
        luna::set_fx( ut, "is_bandolier", &item::is_bandolier );
        luna::set_fx( ut, "is_holster", &item::is_holster );
        luna::set_fx( ut, "is_ammo", &item::is_ammo );
        luna::set_fx( ut, "is_comestible", &item::is_comestible );
        luna::set_fx( ut, "is_food", &item::is_food );
        luna::set_fx( ut, "is_medication", &item::is_medication );
        luna::set_fx( ut, "is_brewable", &item::is_brewable );
        luna::set_fx( ut, "is_food_container", &item::is_food_container );
        luna::set_fx( ut, "is_med_container", &item::is_med_container );
        luna::set_fx( ut, "is_corpse", &item::is_corpse );
        luna::set_fx( ut, "is_ammo_container", &item::is_ammo_container );
        luna::set_fx( ut, "is_armor", &item::is_armor );
        luna::set_fx( ut, "is_book", &item::is_book );
        luna::set_fx( ut, "is_map", &item::is_map );
        luna::set_fx( ut, "is_container", &item::is_container );
        luna::set_fx( ut, "is_watertight_container", &item::is_watertight_container );
        luna::set_fx( ut, "is_non_resealable_container", &item::is_non_resealable_container );
        luna::set_fx( ut, "is_bucket", &item::is_bucket );
        luna::set_fx( ut, "is_bucket_nonempty", &item::is_bucket_nonempty );
        luna::set_fx( ut, "is_engine", &item::is_engine );
        luna::set_fx( ut, "is_wheel", &item::is_wheel );
        luna::set_fx( ut, "is_fuel", &item::is_fuel );
        luna::set_fx( ut, "is_toolmod", &item::is_toolmod );
        luna::set_fx( ut, "is_faulty", &item::is_faulty );
        luna::set_fx( ut, "is_irremovable", &item::is_irremovable );
        luna::set_fx( ut, "is_container_empty", &item::is_container_empty );
        luna::set_fx( ut, "is_salvageable", &item::is_salvageable );
        luna::set_fx( ut, "is_craft", &item::is_craft );
        luna::set_fx( ut, "is_emissive", &item::is_emissive );
        luna::set_fx( ut, "is_deployable", &item::is_deployable );
        luna::set_fx( ut, "is_tool", &item::is_tool );
        luna::set_fx( ut, "is_transformable", &item::is_transformable );
        luna::set_fx( ut, "is_artifact", &item::is_artifact );
        luna::set_fx( ut, "is_relic", &item::is_relic );
        luna::set_fx( ut, "is_seed", &item::is_seed );
        luna::set_fx( ut, "is_dangerous", &item::is_dangerous );
        luna::set_fx( ut, "is_tainted", &item::is_tainted );
        luna::set_fx( ut, "is_soft", &item::is_soft );
        luna::set_fx( ut, "is_reloadable", &item::is_reloadable );
        DOC( "DEPRECATED: Items are no longer filthy" );
        luna::set_fx( ut, "is_filthy", []() { return false; } );
        luna::set_fx( ut, "is_active", &item::is_active );
        luna::set_fx( ut, "is_upgrade", &item::is_upgrade );

        luna::set_fx( ut, "activate", &item::activate );
        luna::set_fx( ut, "deactivate", &item::deactivate );

        DOC( "Is this item an effective melee weapon for the given damage type?" );
        luna::set_fx( ut, "is_melee", sol::resolve<bool( damage_type ) const>
                      ( &item::is_melee ) );

        DOC( "Is this a magazine? (batteries are magazines)" );
        luna::set_fx( ut, "is_magazine", &item::is_magazine );

        DOC( "DEPRECATED: Is this a battery? (spoiler: it isn't)" );
        luna::set_fx( ut, "is_battery", &item::is_battery );

        luna::set_fx( ut, "conductive", &item::conductive );

        luna::set_fx( ut, "is_stackable", sol::resolve<bool() const> ( &item::count_by_charges ) );

        luna::set( ut, "charges", &item::charges );

        luna::set_fx( ut, "energy_remaining", &item::energy_remaining );

        luna::set_fx( ut, "has_infinite_charges", &item::has_infinite_charges );

        luna::set_fx( ut, "mod_charges", &item::mod_charges );

        luna::set_fx( ut, "made_of",
                      sol::resolve < auto() const -> const std::vector<material_id> & > ( &item::made_of ) );

        luna::set_fx( ut, "is_made_of",
                      sol::resolve < auto( const material_id & ) const -> bool > ( &item::made_of ) );

        luna::set_fx( ut, "get_kcal", []( item & it ) -> int { return it.is_comestible() ? it.get_comestible()->default_nutrition.kcal : 0; } );
        luna::set_fx( ut, "get_quench", []( item & it ) -> int { return it.is_comestible() ? it.get_comestible()->quench : 0; } );
        luna::set_fx( ut, "get_comestible_fun", []( item & it ) -> int { return it.get_comestible_fun(); } );
        DOC( "Gets the TimeDuration until this item rots" );
        luna::set_fx( ut, "get_rot", &item::get_rot );

        DOC( "Gets the category id this item is in" );
        luna::set_fx( ut, "get_category_id", &item::get_category_id );

        DOC( "Gets the faction id that owns this item" );
        luna::set_fx( ut, "get_owner", &item::get_owner );

        DOC( "Sets the ownership of this item to a faction" );
        luna::set_fx( ut, "set_owner",
                      sol::resolve<void( const faction_id & )>
                      ( &item::set_owner ) );

        DOC( "Sets the ownership of this item to a character" );
        luna::set_fx( ut, "set_owner",
                      sol::resolve<void( const Character & )>
                      ( &item::set_owner ) );

        luna::set_fx( ut, "get_owner_name", &item::get_owner_name );

        DOC( "Checks if this item owned by a character" );
        luna::set_fx( ut, "is_owned_by", &item::is_owned_by );

        DOC( "Checks if this item has the technique as an addition. Doesn't check original techniques." );
        luna::set_fx( ut, "has_technique",
                      sol::resolve<bool( const matec_id & ) const> ( &item::has_technique ) );
        DOC( "Gets all techniques. Including original techniques." );
        luna::set_fx( ut, "get_techniques",
                      sol::resolve<std::set<matec_id>() const> ( &item::get_techniques ) );
        DOC( "Adds the technique. It isn't treated original, but additional." );
        luna::set_fx( ut, "add_technique",
                      sol::resolve<void( const matec_id & )> ( &item::add_technique ) );
        DOC( "Removes the additional technique. Doesn't affect originial techniques." );
        luna::set_fx( ut, "remove_technique",
                      sol::resolve<void( const matec_id & )> ( &item::remove_technique ) );

        DOC( "Checks if this item can contain another" );
        luna::set_fx( ut, "can_contain",
                      sol::resolve<bool( const item & ) const>
                      ( &item::can_contain ) );

        DOC( "Gets the remaining space available for a type of liquid" );
        luna::set_fx( ut, "remaining_capacity_for_id", &item::get_remaining_capacity_for_id );

        DOC( "Gets maximum volume this item can hold (liquids, ammo, etc)" );
        luna::set_fx( ut, "total_capacity", &item::get_total_capacity );

        DOC( "Gets the current magazine" );
        luna::set_fx( ut, "current_magazine",
                      sol::resolve<const item*() const> ( &item::magazine_current ) );

        DOC( "Gets the maximum capacity of a magazine" );
        luna::set_fx( ut, "ammo_capacity",
                      sol::resolve<int( const bool ) const>
                      ( &item::ammo_capacity ) );

        DOC( "Get remaining ammo, works with batteries & stuff too" );
        luna::set_fx( ut, "ammo_remaining", &item::ammo_remaining );

        luna::set_fx( ut, "ammo_data", &item::ammo_data );
        luna::set_fx( ut, "ammo_required", &item::ammo_required );
        luna::set_fx( ut, "ammo_current", &item::ammo_current );

        luna::set_fx( ut, "ammo_consume", &item::ammo_consume );
        luna::set_fx( ut, "ammo_set", &item::ammo_set );
        luna::set_fx( ut, "ammo_unset", &item::ammo_unset );

        luna::set_fx( ut, "get_reload_time", &item::get_reload_time );

        DOC( "Adds an item(s) to contents" );
        luna::set_fx( ut, "add_item_with_id", &item::add_item_with_id );

        DOC( "Checks item contents for a given item id" );
        luna::set_fx( ut, "has_item_with_id", &item::has_item_with_id );

        DOC( "Checks if the item covers a bodypart" );
        luna::set_fx( ut, "covers", &item::covers );

        luna::set_fx( ut, "set_flag", &item::set_flag );
        luna::set_fx( ut, "unset_flag", &item::unset_flag );
        luna::set_fx( ut, "has_flag", &item::has_flag );
        luna::set_fx( ut, "has_own_flag", &item::has_own_flag );
        luna::set_fx( ut, "set_flag_recursive", &item::set_flag_recursive );
        luna::set_fx( ut, "unset_flags", &item::unset_flags );

        DOC( "Converts the item as given `ItypeId`." );
        luna::set_fx( ut, "convert", &item::convert );

        DOC( "Get variable as string" );
        luna::set_fx( ut, "get_var_str",
                      sol::resolve<std::string( const std::string &, const std::string & ) const>
                      ( &item::get_var ) );
        DOC( "Get variable as float number" );
        luna::set_fx( ut, "get_var_num",
                      sol::resolve<double( const std::string &, double ) const>( &item::get_var ) );
        DOC( "Get variable as tripoint" );
        luna::set_fx( ut, "get_var_tri",
                      sol::resolve<tripoint( const std::string &, const tripoint & ) const>
                      ( &item::get_var ) );

        luna::set_fx( ut, "set_var_str", sol::resolve<void( const std::string &, const std::string & )>
                      ( &item::set_var ) );
        luna::set_fx( ut, "set_var_num",
                      sol::resolve<void( const std::string &, double )>( &item::set_var ) );
        luna::set_fx( ut, "set_var_tri",
                      sol::resolve<void( const std::string &, const tripoint & )>( &item::set_var ) );

        SET_FX( attack_cost );
        SET_FX( stamina_cost );
    }
#undef UT_CLASS // #define UT_CLASS item
}


void cata::detail::reg_constants( sol::state &lua )
{
    DOC( "Various game constants" );
    luna::userlib lib = luna::begin_lib( lua, "const" );

    luna::set( lib, "OM_OMT_SIZE", OMAPX );
    luna::set( lib, "OM_SM_SIZE", OMAPX * 2 );
    luna::set( lib, "OM_MS_SIZE", OMAPX * 2 * SEEX );
    luna::set( lib, "OMT_SM_SIZE", 2 );
    luna::set( lib, "OMT_MS_SIZE", SEEX * 2 );
    luna::set( lib, "SM_MS_SIZE", SEEX );

    luna::finalize_lib( lib );
}

static void lua_log_info_impl( sol::variadic_args va )
{
    std::string msg = cata::detail::fmt_lua_va( va );

    DebugLog( DL::Info, DC::Lua ) << msg;
    cata::get_lua_log_instance().add( cata::LuaLogLevel::Info, std::move( msg ) );
}

static void lua_log_warn_impl( sol::variadic_args va )
{
    std::string msg = cata::detail::fmt_lua_va( va );

    DebugLog( DL::Warn, DC::Lua ) << msg;
    cata::get_lua_log_instance().add( cata::LuaLogLevel::Warn, std::move( msg ) );
}

static void lua_log_error_impl( sol::variadic_args va )
{
    std::string msg = cata::detail::fmt_lua_va( va );

    DebugLog( DL::Error, DC::Lua ) << msg;
    cata::get_lua_log_instance().add( cata::LuaLogLevel::Error, std::move( msg ) );
}

static void lua_debugmsg_impl( sol::variadic_args va )
{
    std::string msg = cata::detail::fmt_lua_va( va );

    debugmsg( "%s", msg );
    cata::get_lua_log_instance().add( cata::LuaLogLevel::DebugMsg, std::move( msg ) );
}

void cata::detail::reg_debug_api( sol::state &lua )
{
    DOC( "Debugging and logging API." );
    luna::userlib lib = luna::begin_lib( lua, "gdebug" );

    luna::set_fx( lib, "log_info", &lua_log_info_impl );
    luna::set_fx( lib, "log_warn", &lua_log_warn_impl );
    luna::set_fx( lib, "log_error", &lua_log_error_impl );
    luna::set_fx( lib, "debugmsg", &lua_debugmsg_impl );
    luna::set_fx( lib, "clear_lua_log", []() { cata::get_lua_log_instance().clear(); } );
    luna::set_fx( lib, "set_log_capacity", []( int v ) { cata::get_lua_log_instance().set_log_capacity( v ); } );
    luna::set_fx( lib, "reload_lua_code", &cata::reload_lua_code );
    luna::set_fx( lib, "save_game", []() -> bool { return g->save( false ); } );

    luna::finalize_lib( lib );
}

void cata::detail::override_default_print( sol::state &lua )
{
    lua.globals()["print"] = &lua_log_info_impl;
}

void cata::detail::forbid_unsafe_functions( sol::state &lua )
{
    auto g = lua.globals();
    g["dofile"] = sol::nil;
    g["loadfile"] = sol::nil;
    g["load"] = sol::nil;
    g["loadstring"] = sol::nil;
}

// We have to alias the function here because VS compiler
// confuses namespaces 'detail' and 'cata::detail'
const auto &gettext_raw = sol::resolve<const char *( const char * )>
                          ( &detail::_translate_internal );

void cata::detail::reg_locale_api( sol::state &lua )
{
    DOC( "Localization API." );
    luna::userlib lib = luna::begin_lib( lua, "locale" );

    DOC( "Expects english source string, returns translated string." );
    luna::set_fx( lib, "gettext", gettext_raw );
    DOC( "First is english singular string, second is english plural string. Number is amount to translate for." );
    luna::set_fx( lib, "vgettext", &vgettext );
    DOC( "First is context string. Second is english source string." );
    luna::set_fx( lib, "pgettext", &pgettext );
    DOC( "First is context string. Second is english singular string. third is english plural. Number is amount to translate for." );
    luna::set_fx( lib, "vpgettext", &vpgettext );

    luna::finalize_lib( lib );
}

template<typename E>
void reg_enum( sol::state &lua )
{
    // Sol2 has new_enum<E>(...) function, but it needs to know all value-string
    // pairs at compile time, so we can't use it with io::enum_to_string.
    //
    // As such, hack it by creating read-only table.

    luna::userenum<E> et = luna::begin_enum<E>( lua );

    using Int = std::underlying_type_t<E>;
    constexpr Int max = static_cast<Int>( enum_traits<E>::last );

    for( Int i = 0; i < max; ++i ) {
        E e = static_cast<E>( i );
        std::string key = io::enum_to_string<E>( e );
        luna::add_val( et, key, e );
    }

    luna::finalize_enum( et );
}

void cata::detail::reg_colors( sol::state &lua )
{
    // Colors are not enums, we have to do them manually
    luna::userenum<color_id> et = luna::begin_enum<color_id>( lua );

    using Int = std::underlying_type_t<color_id>;
    constexpr Int max = static_cast<Int>( color_id::num_colors );

    for( Int i = 0; i < max; ++i ) {
        color_id e = static_cast<color_id>( i );
        std::string key = get_all_colors().id_to_name( e );
        luna::add_val( et, key, e );
    }

    luna::finalize_enum( et );
}

void cata::detail::reg_enums( sol::state &lua )
{
    reg_enum<add_type>( lua );
    reg_enum<Attitude>( lua );
    reg_enum<body_part>( lua );
    reg_enum<character_movemode>( lua );
    reg_enum<damage_type>( lua );
    reg_enum<game_message_type>( lua );
    reg_enum<mf_attitude>( lua );
    reg_enum<m_flag>( lua );
    reg_enum<monster_attitude>( lua );
    reg_enum<creature_size>( lua );
    reg_enum<npc_attitude>( lua );
    reg_enum<npc_need>( lua );
    reg_enum<sfx::channel>( lua );
    reg_enum<mission_origin>( lua );
    reg_enum<mission_goal>( lua );
}

void cata::detail::reg_hooks_examples( sol::state &lua )
{
    DOC( "Documentation for hooks" );
    luna::userlib lib = luna::begin_lib( lua, "hooks" );

    DOC( "Called when game is about to save" );
    luna::set_fx( lib, "on_game_save", []() {} );
    DOC( "Called right after game has loaded" );
    luna::set_fx( lib, "on_game_load", []() {} );
    DOC( "Called when the game has first started" );
    luna::set_fx( lib, "on_game_started", []() {} );
    DOC( "Called when a character successfully dodges" );
    luna::set_fx( lib, "on_creature_dodged", []() {} );
    DOC( "Called when a character successfully blocks" );
    luna::set_fx( lib, "on_creature_blocked", []() {} );
    DOC( "Called when a character has performed technique" );
    luna::set_fx( lib, "on_creature_performed_technique", []() {} );
    DOC( "Called after a character has attacked in melee" );
    luna::set_fx( lib, "on_creature_melee_attacked", []() {} );
    DOC( "Called when character stat gets reset" );
    luna::set_fx( lib, "on_character_reset_stats", []() {} );
    DOC( "Called when a character is dead" );
    luna::set_fx( lib, "on_char_death", []() {} );
    DOC( "Called when a monster is dead" );
    luna::set_fx( lib, "on_mon_death", []() {} );
    DOC( "Called every in-game period" );
    luna::set_fx( lib, "on_every_x", []() {} );
    DOC( "Called right after mapgen has completed. "
         "Map argument is the tinymap that represents 24x24 area (2x2 submaps, or 1x1 omt), "
         "tripoint is the absolute omt pos, and time_point is the current time (for time-based effects)."
       );
    luna::set_fx( lib, "on_mapgen_postprocess", []( map &, const tripoint &, const time_point & ) {} );

    luna::finalize_lib( lib );
}

void cata::detail::reg_time_types( sol::state &lua )
{
    DOC( "Library for dealing with time primitives." );

    {
        DOC( "Represent fixed point in time" );
        sol::usertype<time_point> ut =
            luna::new_usertype<time_point>(
                lua,
                luna::no_bases,
                luna::constructors < time_point() > ()
            );

        // Constructor method
        luna::set_fx( ut, "from_turn", &time_point::from_turn );

        // Methods
        luna::set_fx( ut, "to_turn", []( const time_point & tp ) -> int { return to_turn<int>( tp ); } );

        luna::set_fx( ut, "is_night", &is_night );
        luna::set_fx( ut, "is_day", &is_day );
        luna::set_fx( ut, "is_dusk", &is_dusk );
        luna::set_fx( ut, "is_dawn", &is_dawn );

        luna::set_fx( ut, "second_of_minute", []( const time_point & tp ) -> int { return to_turn<int>( tp ) % 60; } );
        luna::set_fx( ut, "minute_of_hour", []( const time_point & tp ) -> int { return minute_of_hour<int>( tp ); } );
        luna::set_fx( ut, "hour_of_day", []( const time_point & tp ) -> int { return hour_of_day<int>( tp ); } );

        // (De-)Serialization
        reg_serde_functions( ut );

        luna::set_fx( ut, "to_string_time_of_day", &to_string_time_of_day );

        // To string
        // We're using Lua meta function here to make it work seamlessly with native Lua tostring()
        luna::set_fx( ut, sol::meta_function::to_string,
                      sol::resolve<std::string( const time_point & )>( to_string ) );

        // Equality operator
        // It's defined as inline friend function inside point class, we can't access it and so have to improvise
        luna::set_fx( ut, sol::meta_function::equal_to, []( const time_point & a, const time_point & b ) { return a == b; } );

        // Less-then operator
        // Same deal as with equality operator
        luna::set_fx( ut, sol::meta_function::less_than, []( const time_point & a, const time_point & b ) { return a < b; } );

        // Arithmetic operators
        luna::set_fx( ut, sol::meta_function::addition,
                      []( const time_point & a, const time_duration & b ) -> time_point { return a + b; } );
        luna::set_fx( ut, sol::meta_function::subtraction,
                      sol::overload(
                          []( const time_point & a, const time_point & b ) -> time_duration { return a - b; },
                          []( const time_point & a, const time_duration & b ) -> time_point { return a - b; }
                      ) );
    }
    {
        DOC( "Represent duration between 2 fixed points in time" );
        sol::usertype<time_duration> ut =
            luna::new_usertype<time_duration>(
                lua,
                luna::no_bases,
                luna::constructors < time_duration() > ()
            );

        // Constructor methods
        luna::set_fx( ut, "from_turns", []( int t ) { return time_duration::from_turns( t ); } );
        luna::set_fx( ut, "from_seconds", []( int t ) { return time_duration::from_seconds( t ); } );
        luna::set_fx( ut, "from_minutes", []( int t ) { return time_duration::from_minutes( t ); } );
        luna::set_fx( ut, "from_hours", []( int t ) { return time_duration::from_hours( t ); } );
        luna::set_fx( ut, "from_days", []( int t ) { return time_duration::from_days( t ); } );
        luna::set_fx( ut, "from_weeks", []( int t ) { return time_duration::from_weeks( t ); } );

        luna::set_fx( ut, "make_random", []( const time_duration & lo, const time_duration & hi ) { return rng( lo, hi ); } );

        luna::set_fx( ut, "to_turns", []( const time_duration & t ) -> int { return to_turns<int>( t ); } );
        luna::set_fx( ut, "to_seconds", []( const time_duration & t ) -> int { return to_seconds<int>( t ); } );
        luna::set_fx( ut, "to_minutes", []( const time_duration & t ) -> int { return to_minutes<int>( t ); } );
        luna::set_fx( ut, "to_hours", []( const time_duration & t ) -> int { return to_hours<int>( t ); } );
        luna::set_fx( ut, "to_days", []( const time_duration & t ) -> int { return to_days<int>( t ); } );
        luna::set_fx( ut, "to_weeks", []( const time_duration & t ) -> int { return to_weeks<int>( t ); } );

        // (De-)Serialization
        reg_serde_functions( ut );

        // To string
        // We're using Lua meta function here to make it work seamlessly with native Lua tostring()
        luna::set_fx( ut, sol::meta_function::to_string,
                      sol::resolve<std::string( const time_duration & )>( to_string ) );

        luna::set_fx( ut, sol::meta_function::addition,
        []( const time_duration & a, const time_duration & b ) { return a + b; } );
        luna::set_fx( ut, sol::meta_function::subtraction,
        []( const time_duration & a, const time_duration & b ) { return a - b; } );
        luna::set_fx( ut, sol::meta_function::multiplication, []( const time_duration & a, int b ) { return a * b; } );
        luna::set_fx( ut, sol::meta_function::division, []( const time_duration & a, int b ) { return a / b; } );
        luna::set_fx( ut, sol::meta_function::unary_minus, []( const time_duration & a ) { return -a; } );
    }
}

void cata::detail::reg_testing_library( sol::state &lua )
{
    DOC( "Library for testing purposes" );
    luna::userlib lib = luna::begin_lib( lua, "tests_lib" );

    // Regression test for https://github.com/ThePhD/sol2/issues/1444
    luna::set_fx( lib, "my_awesome_lambda_1", []() -> int { return 1; } );
    luna::set_fx( lib, "my_awesome_lambda_2", []() -> int { return 2; } );

    luna::finalize_lib( lib );
}

void cata::reg_all_bindings( sol::state &lua )
{
    using namespace detail;

    override_default_print( lua );
    forbid_unsafe_functions( lua );
    reg_debug_api( lua );
    reg_game_api( lua );
    reg_locale_api( lua );
    reg_units( lua );
    reg_skill_level_map( lua );
    reg_damage_instance( lua );
    reg_creature_family( lua );
    reg_point_tripoint( lua );
    reg_item( lua );
    reg_map( lua );
    reg_distribution_grid( lua );
    reg_ui_elements( lua );
    reg_colors( lua );
    reg_enums( lua );
    reg_game_ids( lua );
    mod_mutation_branch( lua );
    reg_magic( lua );
    reg_mission( lua );
    reg_mission_type( lua );
    reg_recipe( lua );
    reg_coords_library( lua );
    reg_constants( lua );
    reg_hooks_examples( lua );
    reg_technique( lua );
    reg_types( lua );
    reg_time_types( lua );
    reg_testing_library( lua );
}
