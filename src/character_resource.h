#pragma once

#include "enum_traits.h"
#include "json.h"
#include "string_id.h"
#include "translations.h"
#include "type_id.h"

class Character;
/**
 * Hardcoded resources on the character
 * These are non-item related resources stored in on character values
 */
/**
 *  Character wide resources, for things like UPSes, stamina and so on
 */
class character_resource
{
    public:
        enum hardcode_character_resource {
            health_points,
            mana,
            stamina,
            bionic,
            fatigue,
            thirst,
            pain,
            radiation,
            health,
            hunger,
            none
        };
        character_resource() = default;

        static void load_character_resources( const JsonObject &jo, const std::string &src );

        void load( const JsonObject &jo, const std::string &src );

        static void check_consistency();

        void check() const;

        static auto get_all() -> std::vector<character_resource>;

        static void reset();

        character_resource_id id;

        // Used for explination in enchantments & items
        translation item_info;
        translation need_info;

        bool was_loaded = false;

        // If this drains from a flag, like `IS_UPS`
        flag_id drain_flag = flag_id::NULL_ID();

        hardcode_character_resource drain_resource = hardcode_character_resource::none;

        float charge_scalar = 1.0f;

        int get_charges( const Character &ch ) const;
        bool has_charges( const Character &ch, int amount ) const;
        bool drain_charges( Character &ch, int amount ) const;
        // Needed for bindings
        auto operator==( const character_resource &rhs ) const -> bool { return id == rhs.id; }
        auto operator<( const character_resource &rhs ) const -> bool { return id < rhs.id; }
};
