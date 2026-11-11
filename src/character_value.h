#pragma once
#include "enum_traits.h"
#include "json.h"
#include "string_id.h"
#include "translations.h"
#include "type_id.h"

class Character;
/**
 *  Character wide resources, for things like UPSes, stamina and so on
 */
class character_value
{
    public:
        /**
         * Hardcoded values on the character
         * These are non-item related resources stored in on character values
         * These are enums because I cant think of a better way to do it
         */
        enum hardcode_character_value {
            // Stats
            strength,
            dexterity,
            intelligence,
            perception,
            health,
            health_points,
            // Stored resources
            mana,
            stamina,
            bionic_power,
            // Character Status
            fatigue,
            thirst,
            pain,
            radiation,
            hunger,
            morale,
            focus,
            none
        };
        character_value() = default;
        static void load_character_values( const JsonObject &jo, const std::string &src );
        void load( const JsonObject &jo, const std::string &src );
        static void check_consistency();
        void check() const;
        static auto get_all() -> std::vector<character_value>;
        static void reset();
        character_value_id id;
        bool was_loaded = false;

        // Hardcoded character value, used for non-variable based things
        hardcode_character_value hardcode_value = hardcode_character_value::none;
        // Character variable value, key for the string string map, of which the second value should be an int
        std::string character_var_value;
        // Limits on the value; Permits balancing
        int min_value;
        int max_value;

        int get_amount( const Character &ch ) const;
        // Needed for bindings
        auto operator==( const character_value &rhs ) const -> bool { return id == rhs.id; }
        auto operator<( const character_value &rhs ) const -> bool { return id < rhs.id; }
};
