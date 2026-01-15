#include "catalua_bindings.h"

#include "catalua_bindings_utils.h"
#include "catalua.h"
#include "catalua_log.h"
#include "catalua_luna_doc.h"
#include "catalua_luna.h"

#include "artifact.h"
#include "bodypart.h"
#include "calendar.h"
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
#include "vitamin.h"

std::vector<std::string> luna::detail::current_comment;

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
                luna::bases<std::map<skill_id, SkillLevel >> (),
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

        luna::set( ut, "name", &ma_technique::name );
        luna::set_fx( ut, "get_description", &ma_technique::get_description );
        luna::set( ut, "avatar_message", &ma_technique::avatar_message );
        luna::set( ut, "npc_message", &ma_technique::npc_message );
        luna::set( ut, "defensive", &ma_technique::defensive );
        luna::set( ut, "side_switch", &ma_technique::side_switch );
        luna::set( ut, "down_dur", &ma_technique::down_dur );
        luna::set( ut, "stun_dur", &ma_technique::stun_dur );
        luna::set( ut, "knockback_dist", &ma_technique::knockback_dist );
        luna::set( ut, "knockback_spread", &ma_technique::knockback_spread );
        luna::set( ut, "powerful_knockback", &ma_technique::powerful_knockback );
        luna::set( ut, "crit_tec", &ma_technique::crit_tec );
        luna::set( ut, "crit_ok", &ma_technique::crit_ok );
        luna::set( ut, "knockback_follow", &ma_technique::knockback_follow );
        luna::set( ut,  "disarms", &ma_technique::disarms );
        luna::set( ut,  "take_weapon", &ma_technique::take_weapon );
        luna::set( ut,  "dodge_counter", &ma_technique::dodge_counter );
        luna::set( ut,  "block_counter", &ma_technique::block_counter );
        luna::set( ut,  "miss_recovery", &ma_technique::miss_recovery );
        luna::set( ut,  "grab_break", &ma_technique::grab_break );

    }
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
    reg_enum<ot_match_type>( lua );
    reg_enum<sfx::channel>( lua );
    reg_enum<mission_origin>( lua );
    reg_enum<mission_goal>( lua );
    reg_enum<art_charge>( lua );
    reg_enum<art_charge_req>( lua );
    reg_enum<art_effect_active>( lua );
    reg_enum<art_effect_passive>( lua );
    reg_enum<vitamin_type>( lua );
    reg_enum<moon_phase>( lua );
}

void cata::detail::reg_hooks_examples( sol::state &lua )
{
    DOC( "Documentation for hooks" );
    luna::userlib lib = luna::begin_lib( lua, "hooks" );

    DOC( "Called when game is about to save." );
    luna::set_fx( lib, "on_game_save", []() {} );

    DOC( "Called right after game has loaded." );
    luna::set_fx( lib, "on_game_load", []() {} );

    DOC( "Called when the game has first started." );
    luna::set_fx( lib, "on_game_started", []() {} );

    DOC( "Called when the weather has changed.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `weather_id` (string): Current weather ID  " );
    DOC( "* `old_weather_id` (string): Previous weather ID  " );
    DOC( "* `temperature` (float): Current temperature in Celsius  " );
    DOC( "* `temperature_f` (float): Current temperature in Fahrenheit  " );
    DOC( "* `windspeed` (float): Wind speed  " );
    DOC( "* `winddirection` (integer): Wind direction in degrees  " );
    DOC( "* `humidity` (float): Humidity percentage  " );
    DOC( "* `pressure` (float): Atmospheric pressure  " );
    DOC( "* `is_sheltered` (boolean): Whether player is sheltered  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_weather_changed", []( const sol::table & ) {} );

    DOC( "Called every 5 minutes when weather data is updated.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `weather_id` (string): Current weather ID  " );
    DOC( "* `temperature` (float): Current temperature in Celsius  " );
    DOC( "* `temperature_f` (float): Current temperature in Fahrenheit  " );
    DOC( "* `windspeed` (float): Wind speed  " );
    DOC( "* `winddirection` (integer): Wind direction in degrees  " );
    DOC( "* `humidity` (float): Humidity percentage  " );
    DOC( "* `pressure` (float): Atmospheric pressure  " );
    DOC( "* `is_sheltered` (boolean): Whether player is sheltered  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_weather_updated", []( const sol::table & ) {} );

    DOC( "Called when a character or monster successfully dodges.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `char` (Character)  " );
    DOC( "* `source` (Creature)  " );
    DOC( "* `difficulty` (integer)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_creature_dodged", []( const sol::table & ) {} );

    DOC( "Called when a character successfully blocks.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `char` (Character)  " );
    DOC( "* `source` (Creature)  " );
    DOC( "* `bodypart_id` (BodyPartTypeId)  " );
    DOC( "* `damage_instance` (DamageInstance)  " );
    DOC( "* `damage_blocked` (float)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_creature_blocked", []( const sol::table & ) {} );

    DOC( "Called when a character has performed a technique.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `char` (Character)  " );
    DOC( "* `technique` (MartialArtsTechniqueRaw)  " );
    DOC( "* `target` (Creature)  " );
    DOC( "* `damage_instance` (DamageInstance)  " );
    DOC( "* `move_cost` (integer)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_creature_performed_technique", []( const sol::table & ) {} );

    DOC( "Called after a character or monster has attacked in melee.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `char` (Character)  " );
    DOC( "* `target` (Creature)  " );
    DOC( "* `success` (bool)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_creature_melee_attacked", []( const sol::table & ) {} );

    DOC( "Called when a character attempts to move.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `char` (Character)  " );
    DOC( "* `from` (Tripoint)  " );
    DOC( "* `to` (Tripoint)  " );
    DOC( "* `movement_mode` (CharacterMoveMode)  " );
    DOC( "* `via_ramp` (bool)  " );
    DOC( "Return false to block the move." );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_character_try_move", []( const sol::table & ) {} );

    DOC( "Called when character stat gets reset.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `character` (Character)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_character_reset_stats", []( const sol::table & ) {} );

    DOC( "Called when character gets the effect which has `EFFECT_LUA_ON_ADDED` flag.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `char` (Character)  " );
    DOC( "* `effect` (Effect)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_character_effect_added", []( const sol::table & ) {} );

    DOC( "Called when character is on the effect which has `EFFECT_LUA_ON_TICK` flag.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `character` (Character)  " );
    DOC( "* `effect` (Effect)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_character_effect", []( const sol::table & ) {} );

    DOC( "Called when character loses the effect which has `EFFECT_LUA_ON_REMOVED` flag.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `character` (Character)  " );
    DOC( "* `effect` (Effect)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_character_effect_removed", []( const sol::table & ) {} );

    DOC( "Called when a character is dead.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `char` (Character)  " );
    DOC( "* `killer` (Creature)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_character_death", []( const sol::table & ) {} );

    DOC( "Called when shot(s) is fired from a gun.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `shooter` (Character)  " );
    DOC( "* `target_pos` (Tripoint)  " );
    DOC( "* `shots` (int)  " );
    DOC( "* `gun` (item)  " );
    DOC( "* `ammo` (item): For `RELOAD_AND_SHOOT` guns like a bow. On the others, it returns `nil` value.  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_shoot", []( const sol::table & ) {} );

    DOC( "Called when an item is thrown.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `thrower` (Character)  " );
    DOC( "* `target_pos` (Tripoint)  " );
    DOC( "* `throw_from_pos` (Tripoint)  " );
    DOC( "* `thrown` (item)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_throw", []( const sol::table & ) {} );

    DOC( "Called when monster gets the effect which has `EFFECT_LUA_ON_ADDED` flag.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `mon` (Monster)  " );
    DOC( "* `effect` (Effect)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_mon_effect_added", []( const sol::table & ) {} );

    DOC( "Called when character is on the effect which has `EFFECT_LUA_ON_TICK` flag.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `mon` (Monster)  " );
    DOC( "* `effect` (Effect)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_mon_effect", []( const sol::table & ) {} );

    DOC( "Called when monster loses the effect which has `EFFECT_LUA_ON_REMOVED` flag.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `mon` (Monster)  " );
    DOC( "* `effect` (Effect)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_mon_effect_removed", []( const sol::table & ) {} );

    DOC( "Called when a monster is dead.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `mon` (Monster)  " );
    DOC( "* `killer` (Creature)  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_mon_death", []( const sol::table & ) {} );

    DOC( "Called every in-game period" );
    luna::set_fx( lib, "on_every_x", []( const sol::table & ) {} );

    DOC( "Called right after mapgen has completed.  " );
    DOC( "The hook receives a table with keys:  " );
    DOC( "* `map` (Map): The tinymap that represents 24x24 area (2x2 submaps, or 1x1 omt).  " );
    DOC( "* `omt` (Tripoint): The absolute overmap pos.  " );
    DOC( "* `when` (TimePoint): The current time (for time-based effects).  " );
    DOC_PARAMS( "params" );
    luna::set_fx( lib, "on_mapgen_postprocess", []( const sol::table & ) {} );

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
        luna::set_fx( ut, "sunrise", &sunrise );
        luna::set_fx( ut, "sunset", &sunset );
        luna::set_fx( ut, "moon_phase", &get_moon_phase );
        luna::set_fx( ut, "season", []( const time_point & tp ) { return calendar::name_season( season_of_year( tp ) ); } );


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
    reg_overmap( lua );
    reg_constants( lua );
    reg_hooks_examples( lua );
    reg_technique( lua );
    reg_types( lua );
    reg_time_types( lua );
    reg_effect( lua );
    reg_testing_library( lua );
    reg_requirement( lua );
    reg_inventory( lua );
}
