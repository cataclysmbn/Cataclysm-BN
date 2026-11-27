#pragma once

#include "catalua_type_operators.h"
#include "enum_traits.h"
#include "translations.h"
#include "type_id.h"

#include <string>

class JsonObject;

// Legacy identifier for the hardcoded addiction types. JSON-defined
// addiction types may reference one for save and event compatibility.
enum class add_type : int {
    NONE,
    CAFFEINE, ALCOHOL, SLEEP, PKILLER, SPEED, CIG,
    COKE, CRACK, MUTAGEN, DIAZEPAM,
    MARLOSS_R, MARLOSS_B, MARLOSS_Y,
    NUM_ADD_TYPES // last
};

template<>
struct enum_traits<add_type> {
    static constexpr add_type last = add_type::NUM_ADD_TYPES;
};

class addiction_type_data
{
    public:
        addiction_type_id id;
        bool was_loaded = false;

        // Display name; finishes the sentence "Became addicted to ____".
        translation name;
        // Long description shown in the character status UI.
        translation description;
        // Morale type applied for cravings; null when the type causes none.
        morale_type craving_morale;
        // Legacy enum alias used for old save files and game events.
        add_type legacy_enum = add_type::NONE;
        // Optional Lua callback ("lua:NAME") that replaces the hardcoded
        // withdrawal behavior while suffering from this addiction.
        std::string on_withdrawal;

        static void load_type( const JsonObject &jo, const std::string &src );
        static void check_all();
        static void reset();

        void load( const JsonObject &jo, const std::string &src );
        void check() const;

        LUA_TYPE_OPS( addiction_type_data, id );
};
