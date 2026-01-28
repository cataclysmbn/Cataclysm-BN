#include "catalua_icallback_actor.h"

#include "bionics.h"
#include "catalua_impl.h"
#include "character.h"
#include "creature.h"
#include "damage.h"
#include "debug.h"
#include "item.h"
#include "player.h"

// --- lua_iuse_actor ---

lua_iuse_actor::lua_iuse_actor( const std::string &type,
                                sol::protected_function &&use_func,
                                sol::protected_function &&can_use_func )
    : iuse_actor( type ),
      use_func( use_func ),
      can_use_func( can_use_func ),
      tick_func( sol::lua_nil ) {}

lua_iuse_actor::~lua_iuse_actor() = default;

void lua_iuse_actor::load( const JsonObject & )
{
    // TODO: custom data
}

int lua_iuse_actor::use( player &who, item &itm, bool tick, const tripoint &pos ) const
{
    try {
        if( !tick ) {
            sol::protected_function_result res = use_func( who.as_character(), itm, pos );
            check_func_result( res );
            int ret = res;
            return ret;
        } else if( tick_func != sol::lua_nil ) {
            sol::protected_function_result res = tick_func( who.as_character(), itm, pos );
            check_func_result( res );
            int ret = res;
            return ret;
        }
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iuse_function k='%s': %s", type, e.what() );
    }
    return 1;
}

ret_val<bool> lua_iuse_actor::can_use( const Character &who, const item &item, bool,
                                       const tripoint &pos ) const
{
    if( can_use_func != sol::lua_nil ) {
        sol::protected_function_result res = can_use_func( who.as_character(), item, pos );
        check_func_result( res );
        const bool ret = res;
        return ret
               ? ret_val<bool>::make_success()
               : ret_val<bool>::make_failure();
    }
    return ret_val<bool>::make_success();
}

std::unique_ptr<iuse_actor> lua_iuse_actor::clone() const
{
    return std::make_unique<lua_iuse_actor>( *this );
}

// --- lua_icallback_actor_base ---

lua_icallback_actor_base::lua_icallback_actor_base( const std::string &item_id )
    : item_id( item_id ) {}

// --- lua_iwieldable_actor ---

lua_iwieldable_actor::lua_iwieldable_actor( const std::string &item_id,
        sol::protected_function &&on_wield,
        sol::protected_function &&on_unwield,
        sol::protected_function &&can_wield,
        sol::protected_function &&can_unwield )
    : lua_icallback_actor_base( item_id ),
      on_wield_func( std::move( on_wield ) ),
      on_unwield_func( std::move( on_unwield ) ),
      can_wield_func( std::move( can_wield ) ),
      can_unwield_func( std::move( can_unwield ) ) {}

void lua_iwieldable_actor::call_on_wield( Character &who, item &it, int mv ) const
{
    if( on_wield_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_wield_func( who, it, mv );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iwieldable on_wield for '%s': %s", item_id, e.what() );
    }
}

void lua_iwieldable_actor::call_on_unwield( Character &who, item &it ) const
{
    if( on_unwield_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_unwield_func( who, it );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iwieldable on_unwield for '%s': %s", item_id, e.what() );
    }
}

bool lua_iwieldable_actor::call_can_wield( const Character &who, const item &it ) const
{
    if( can_wield_func == sol::lua_nil ) {
        return true;
    }
    try {
        sol::protected_function_result res = can_wield_func( who, it );
        check_func_result( res );
        bool ret = res;
        return ret;
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iwieldable can_wield for '%s': %s", item_id, e.what() );
    }
    return true;
}

bool lua_iwieldable_actor::call_can_unwield( const Character &who, const item &it ) const
{
    if( can_unwield_func == sol::lua_nil ) {
        return true;
    }
    try {
        sol::protected_function_result res = can_unwield_func( who, it );
        check_func_result( res );
        bool ret = res;
        return ret;
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iwieldable can_unwield for '%s': %s", item_id, e.what() );
    }
    return true;
}

// --- lua_iwearable_actor ---

lua_iwearable_actor::lua_iwearable_actor( const std::string &item_id,
        sol::protected_function &&on_wear,
        sol::protected_function &&on_takeoff,
        sol::protected_function &&can_wear,
        sol::protected_function &&can_takeoff )
    : lua_icallback_actor_base( item_id ),
      on_wear_func( std::move( on_wear ) ),
      on_takeoff_func( std::move( on_takeoff ) ),
      can_wear_func( std::move( can_wear ) ),
      can_takeoff_func( std::move( can_takeoff ) ) {}

void lua_iwearable_actor::call_on_wear( Character &who, item &it ) const
{
    if( on_wear_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_wear_func( who, it );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iwearable on_wear for '%s': %s", item_id, e.what() );
    }
}

void lua_iwearable_actor::call_on_takeoff( Character &who, item &it ) const
{
    if( on_takeoff_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_takeoff_func( who, it );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iwearable on_takeoff for '%s': %s", item_id, e.what() );
    }
}

bool lua_iwearable_actor::call_can_wear( const Character &who, const item &it ) const
{
    if( can_wear_func == sol::lua_nil ) {
        return true;
    }
    try {
        sol::protected_function_result res = can_wear_func( who, it );
        check_func_result( res );
        bool ret = res;
        return ret;
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iwearable can_wear for '%s': %s", item_id, e.what() );
    }
    return true;
}

bool lua_iwearable_actor::call_can_takeoff( const Character &who, const item &it ) const
{
    if( can_takeoff_func == sol::lua_nil ) {
        return true;
    }
    try {
        sol::protected_function_result res = can_takeoff_func( who, it );
        check_func_result( res );
        bool ret = res;
        return ret;
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iwearable can_takeoff for '%s': %s", item_id, e.what() );
    }
    return true;
}

// --- lua_iequippable_actor ---

lua_iequippable_actor::lua_iequippable_actor( const std::string &item_id,
        sol::protected_function &&on_durability_change,
        sol::protected_function &&on_repair,
        sol::protected_function &&on_break )
    : lua_icallback_actor_base( item_id ),
      on_durability_change_func( std::move( on_durability_change ) ),
      on_repair_func( std::move( on_repair ) ),
      on_break_func( std::move( on_break ) ) {}

void lua_iequippable_actor::call_on_durability_change( Character &who, item &it,
        int old_damage, int new_damage ) const
{
    if( on_durability_change_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_durability_change_func( who, it, old_damage,
                                             new_damage );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iequippable on_durability_change for '%s': %s", item_id, e.what() );
    }
}

void lua_iequippable_actor::call_on_repair( Character &who, item &it ) const
{
    if( on_repair_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_repair_func( who, it );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iequippable on_repair for '%s': %s", item_id, e.what() );
    }
}

void lua_iequippable_actor::call_on_break( Character &who, item &it ) const
{
    if( on_break_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_break_func( who, it );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iequippable on_break for '%s': %s", item_id, e.what() );
    }
}

// --- lua_istate_actor ---

lua_istate_actor::lua_istate_actor( const std::string &item_id,
                                    sol::protected_function &&on_tick,
                                    sol::protected_function &&on_pickup,
                                    sol::protected_function &&on_drop )
    : lua_icallback_actor_base( item_id ),
      on_tick_func( std::move( on_tick ) ),
      on_pickup_func( std::move( on_pickup ) ),
      on_drop_func( std::move( on_drop ) ) {}

bool lua_istate_actor::has_on_tick() const
{
    return on_tick_func != sol::lua_nil;
}

int lua_istate_actor::call_on_tick( Character &who, item &it, const tripoint &pos ) const
{
    if( on_tick_func == sol::lua_nil ) {
        return 0;
    }
    try {
        sol::protected_function_result res = on_tick_func( who, it, pos );
        check_func_result( res );
        int ret = res;
        return ret;
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run istate on_tick for '%s': %s", item_id, e.what() );
    }
    return 0;
}

void lua_istate_actor::call_on_pickup( Character &who, item &it ) const
{
    if( on_pickup_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_pickup_func( who, it );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run istate on_pickup for '%s': %s", item_id, e.what() );
    }
}

bool lua_istate_actor::call_on_drop( Character &who, item &it, const tripoint &pos ) const
{
    if( on_drop_func == sol::lua_nil ) {
        return false;
    }
    try {
        sol::protected_function_result res = on_drop_func( who, it, pos );
        check_func_result( res );
        bool ret = res;
        return ret;
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run istate on_drop for '%s': %s", item_id, e.what() );
    }
    return false;
}

// --- lua_imelee_actor ---

lua_imelee_actor::lua_imelee_actor( const std::string &item_id,
                                    sol::protected_function &&on_melee_attack,
                                    sol::protected_function &&on_hit,
                                    sol::protected_function &&on_block,
                                    sol::protected_function &&on_miss )
    : lua_icallback_actor_base( item_id ),
      on_melee_attack_func( std::move( on_melee_attack ) ),
      on_hit_func( std::move( on_hit ) ),
      on_block_func( std::move( on_block ) ),
      on_miss_func( std::move( on_miss ) ) {}

bool lua_imelee_actor::call_on_melee_attack( Character &who, Creature &target,
        item &it ) const
{
    if( on_melee_attack_func == sol::lua_nil ) {
        return true;
    }
    try {
        sol::protected_function_result res = on_melee_attack_func( who, target, it );
        check_func_result( res );
        bool ret = res;
        return ret;
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run imelee on_melee_attack for '%s': %s", item_id, e.what() );
    }
    return true;
}

void lua_imelee_actor::call_on_hit( Character &who, Creature &target, item &it,
                                    const dealt_damage_instance &dealt ) const
{
    if( on_hit_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_hit_func( who, target, it, dealt );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run imelee on_hit for '%s': %s", item_id, e.what() );
    }
}

void lua_imelee_actor::call_on_block( Character &who, Creature &source, item &it,
                                      int damage_blocked ) const
{
    if( on_block_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_block_func( who, source, it, damage_blocked );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run imelee on_block for '%s': %s", item_id, e.what() );
    }
}

void lua_imelee_actor::call_on_miss( Character &who, item &it ) const
{
    if( on_miss_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_miss_func( who, it );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run imelee on_miss for '%s': %s", item_id, e.what() );
    }
}

// --- lua_iranged_actor ---

lua_iranged_actor::lua_iranged_actor( const std::string &item_id,
                                      sol::protected_function &&on_fire,
                                      sol::protected_function &&on_reload,
                                      sol::protected_function &&can_fire,
                                      sol::protected_function &&can_reload )
    : lua_icallback_actor_base( item_id ),
      on_fire_func( std::move( on_fire ) ),
      on_reload_func( std::move( on_reload ) ),
      can_fire_func( std::move( can_fire ) ),
      can_reload_func( std::move( can_reload ) ) {}

bool lua_iranged_actor::call_on_fire( Character &who, item &gun,
                                      const tripoint &target, int shots ) const
{
    if( on_fire_func == sol::lua_nil ) {
        return true;
    }
    try {
        sol::protected_function_result res = on_fire_func( who, gun, target, shots );
        check_func_result( res );
        bool ret = res;
        return ret;
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iranged on_fire for '%s': %s", item_id, e.what() );
    }
    return true;
}

void lua_iranged_actor::call_on_reload( Character &who, item &it ) const
{
    if( on_reload_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_reload_func( who, it );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iranged on_reload for '%s': %s", item_id, e.what() );
    }
}

bool lua_iranged_actor::call_can_fire( const Character &who, const item &gun ) const
{
    if( can_fire_func == sol::lua_nil ) {
        return true;
    }
    try {
        sol::protected_function_result res = can_fire_func( who, gun );
        check_func_result( res );
        bool ret = res;
        return ret;
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iranged can_fire for '%s': %s", item_id, e.what() );
    }
    return true;
}

bool lua_iranged_actor::call_can_reload( const Character &who, const item &it ) const
{
    if( can_reload_func == sol::lua_nil ) {
        return true;
    }
    try {
        sol::protected_function_result res = can_reload_func( who, it );
        check_func_result( res );
        bool ret = res;
        return ret;
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run iranged can_reload for '%s': %s", item_id, e.what() );
    }
    return true;
}

// --- lua_bionic_callback_actor ---

lua_bionic_callback_actor::lua_bionic_callback_actor( const std::string &bionic_str_id,
        sol::protected_function &&on_activate,
        sol::protected_function &&on_deactivate,
        sol::protected_function &&on_installed,
        sol::protected_function &&on_removed )
    : bionic_str_id( bionic_str_id ),
      on_activate_func( std::move( on_activate ) ),
      on_deactivate_func( std::move( on_deactivate ) ),
      on_installed_func( std::move( on_installed ) ),
      on_removed_func( std::move( on_removed ) ) {}

void lua_bionic_callback_actor::call_on_activate( Character &who, bionic &bio ) const
{
    if( on_activate_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_activate_func( who, bio );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run bionic on_activate for '%s': %s", bionic_str_id, e.what() );
    }
}

void lua_bionic_callback_actor::call_on_deactivate( Character &who, bionic &bio ) const
{
    if( on_deactivate_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_deactivate_func( who, bio );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run bionic on_deactivate for '%s': %s", bionic_str_id, e.what() );
    }
}

void lua_bionic_callback_actor::call_on_installed( Character &who, const bionic_id &bid ) const
{
    if( on_installed_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_installed_func( who, bid );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run bionic on_installed for '%s': %s", bionic_str_id, e.what() );
    }
}

void lua_bionic_callback_actor::call_on_removed( Character &who, const bionic_id &bid ) const
{
    if( on_removed_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_removed_func( who, bid );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run bionic on_removed for '%s': %s", bionic_str_id, e.what() );
    }
}

// --- lua_mutation_callback_actor ---

lua_mutation_callback_actor::lua_mutation_callback_actor( const std::string &trait_str_id,
        sol::protected_function &&on_activate,
        sol::protected_function &&on_deactivate,
        sol::protected_function &&on_gain,
        sol::protected_function &&on_loss )
    : trait_str_id( trait_str_id ),
      on_activate_func( std::move( on_activate ) ),
      on_deactivate_func( std::move( on_deactivate ) ),
      on_gain_func( std::move( on_gain ) ),
      on_loss_func( std::move( on_loss ) ) {}

void lua_mutation_callback_actor::call_on_activate( Character &who, const trait_id &tid ) const
{
    if( on_activate_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_activate_func( who, tid );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run mutation on_activate for '%s': %s", trait_str_id, e.what() );
    }
}

void lua_mutation_callback_actor::call_on_deactivate( Character &who, const trait_id &tid ) const
{
    if( on_deactivate_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_deactivate_func( who, tid );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run mutation on_deactivate for '%s': %s", trait_str_id, e.what() );
    }
}

void lua_mutation_callback_actor::call_on_gain( Character &who, const trait_id &tid ) const
{
    if( on_gain_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_gain_func( who, tid );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run mutation on_gain for '%s': %s", trait_str_id, e.what() );
    }
}

void lua_mutation_callback_actor::call_on_loss( Character &who, const trait_id &tid ) const
{
    if( on_loss_func == sol::lua_nil ) {
        return;
    }
    try {
        sol::protected_function_result res = on_loss_func( who, tid );
        check_func_result( res );
    } catch( std::runtime_error &e ) {
        debugmsg( "Failed to run mutation on_loss for '%s': %s", trait_str_id, e.what() );
    }
}
