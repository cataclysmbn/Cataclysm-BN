#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "bodypart.h"
#include "calendar.h"
#include "catalua_type_operators.h"
#include "enchantments/enchantment.h"
#include "flat_set.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "enums.h"
#include "color.h"

class JsonIn;
class JsonObject;
class JsonOut;
class Character;
class player;
class lua_bionic_callback_actor;

enum class character_stat : char;

void draw_perk_menu( Character &player );

class perk
{
    public:
        perk() = default;
        ~perk() = default;

        static void load_perks( const JsonObject &jo, const std::string &src );

        void load( const JsonObject &jo, const std::string &src );

        static void finalize_all();

        void finalize();

        static void check_consistency();

        void check() const;

        static std::vector<perk> get_all();

        static void reset();

        perk_id id;
        bool was_loaded = false;
        int points;

        bool is_hidden() const;
        std::string get_name() const;
        std::string get_description() const;
        std::string get_category() const;
        std::vector<enchantment> get_enchantments() const;

        // Needed for bindings
        auto operator==( const perk &rhs ) const -> bool { return id == rhs.id; }
        auto operator<( const perk &rhs ) const -> bool { return id < rhs.id; }

    private:
        translation name;
        translation description;
        translation category;
        bool hidden;
        std::vector<enchantment> enchantments;

        void add_enchantment( enchantment &ench );
};
