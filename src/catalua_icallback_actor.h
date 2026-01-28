#pragma once

#include "iuse.h"
#include "catalua_sol.h"
#include "ret_val.h"

#include <string>

class Character;
class Creature;
class item;
struct dealt_damage_instance;
struct tripoint;

/** Dynamic iuse_actor provided by Lua. */
class lua_iuse_actor : public iuse_actor
{
    private:
        sol::protected_function use_func;
        sol::protected_function can_use_func;
        sol::protected_function tick_func;

    public:
        lua_iuse_actor( const std::string &type,
                        sol::protected_function &&use_func,
                        sol::protected_function &&can_use_func );
        ~lua_iuse_actor() override;
        void load( const JsonObject &obj ) override;
        int use( player &who, item &itm, bool tick, const tripoint &pos ) const override;
        ret_val<bool> can_use( const Character &, const item &, bool, const tripoint & ) const override;
        std::unique_ptr<iuse_actor> clone() const override;
};

/**
 * Lightweight base for Lua item callback actors.
 * Each callback type stores its item_id and optional sol::protected_functions.
 */
class lua_icallback_actor_base
{
    public:
        std::string item_id;
        virtual ~lua_icallback_actor_base() = default;
    protected:
        explicit lua_icallback_actor_base( const std::string &item_id );
};

/** Lua callbacks for wield/unwield events. */
class lua_iwieldable_actor : public lua_icallback_actor_base
{
    private:
        sol::protected_function on_wield_func;
        sol::protected_function on_unwield_func;
        sol::protected_function can_wield_func;
        sol::protected_function can_unwield_func;

    public:
        lua_iwieldable_actor( const std::string &item_id,
                              sol::protected_function &&on_wield,
                              sol::protected_function &&on_unwield,
                              sol::protected_function &&can_wield,
                              sol::protected_function &&can_unwield );

        void call_on_wield( Character &who, item &it, int mv ) const;
        void call_on_unwield( Character &who, item &it ) const;
        /** Returns false to block wielding. */
        bool call_can_wield( const Character &who, const item &it ) const;
        /** Returns false to block unwielding. */
        bool call_can_unwield( const Character &who, const item &it ) const;
};

/** Lua callbacks for wear/takeoff events. */
class lua_iwearable_actor : public lua_icallback_actor_base
{
    private:
        sol::protected_function on_wear_func;
        sol::protected_function on_takeoff_func;
        sol::protected_function can_wear_func;
        sol::protected_function can_takeoff_func;

    public:
        lua_iwearable_actor( const std::string &item_id,
                             sol::protected_function &&on_wear,
                             sol::protected_function &&on_takeoff,
                             sol::protected_function &&can_wear,
                             sol::protected_function &&can_takeoff );

        void call_on_wear( Character &who, item &it ) const;
        void call_on_takeoff( Character &who, item &it ) const;
        /** Returns false to block wearing. */
        bool call_can_wear( const Character &who, const item &it ) const;
        /** Returns false to block takeoff. */
        bool call_can_takeoff( const Character &who, const item &it ) const;
};

/** Lua callbacks for durability change, repair, and break events. */
class lua_iequippable_actor : public lua_icallback_actor_base
{
    private:
        sol::protected_function on_durability_change_func;
        sol::protected_function on_repair_func;
        sol::protected_function on_break_func;

    public:
        lua_iequippable_actor( const std::string &item_id,
                               sol::protected_function &&on_durability_change,
                               sol::protected_function &&on_repair,
                               sol::protected_function &&on_break );

        void call_on_durability_change( Character &who, item &it,
                                        int old_damage, int new_damage ) const;
        void call_on_repair( Character &who, item &it ) const;
        void call_on_break( Character &who, item &it ) const;
};

/** Lua callbacks for state changes: tick, pickup, drop. */
class lua_istate_actor : public lua_icallback_actor_base
{
    private:
        sol::protected_function on_tick_func;
        sol::protected_function on_pickup_func;
        sol::protected_function on_drop_func;

    public:
        lua_istate_actor( const std::string &item_id,
                          sol::protected_function &&on_tick,
                          sol::protected_function &&on_pickup,
                          sol::protected_function &&on_drop );

        bool has_on_tick() const;
        int call_on_tick( Character &who, item &it, const tripoint &pos ) const;
        void call_on_pickup( Character &who, item &it ) const;
        bool call_on_drop( Character &who, item &it, const tripoint &pos ) const;
};

/** Lua callbacks for melee combat events. */
class lua_imelee_actor : public lua_icallback_actor_base
{
    private:
        sol::protected_function on_melee_attack_func;
        sol::protected_function on_hit_func;
        sol::protected_function on_block_func;
        sol::protected_function on_miss_func;

    public:
        lua_imelee_actor( const std::string &item_id,
                          sol::protected_function &&on_melee_attack,
                          sol::protected_function &&on_hit,
                          sol::protected_function &&on_block,
                          sol::protected_function &&on_miss );

        /** Called before hit resolution. Returns false to force a miss. */
        bool call_on_melee_attack( Character &who, Creature &target, item &it ) const;
        void call_on_hit( Character &who, Creature &target, item &it,
                          const dealt_damage_instance &dealt ) const;
        void call_on_block( Character &who, Creature &source, item &it,
                            int damage_blocked ) const;
        void call_on_miss( Character &who, item &it ) const;
};

/** Lua callbacks for ranged combat events. */
class lua_iranged_actor : public lua_icallback_actor_base
{
    private:
        sol::protected_function on_fire_func;
        sol::protected_function on_reload_func;
        sol::protected_function can_fire_func;
        sol::protected_function can_reload_func;

    public:
        lua_iranged_actor( const std::string &item_id,
                           sol::protected_function &&on_fire,
                           sol::protected_function &&on_reload,
                           sol::protected_function &&can_fire,
                           sol::protected_function &&can_reload );

        /** Called after firing. Returns false to force all shots to miss. */
        bool call_on_fire( Character &who, item &gun,
                           const tripoint &target, int shots ) const;
        void call_on_reload( Character &who, item &it ) const;
        /** Returns false to block firing entirely (before any ammo is consumed). */
        bool call_can_fire( const Character &who, const item &gun ) const;
        /** Returns false to block reloading. */
        bool call_can_reload( const Character &who, const item &it ) const;
};
