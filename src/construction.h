#pragma once

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "calendar.h"
#include "translations.h"
#include "type_id.h"
#include "coordinates.h"

class Character;
class inventory;
struct construction;
struct point;

namespace catacurses
{
class window;
} // namespace catacurses
class JsonObject;
class nc_color;
struct tripoint;

struct build_reqs {
    std::map<skill_id, int> skills;
    std::map<requirement_id, int> reqs;
    int time = 0;
};

struct construction {
    public:
        construction_str_id id;
        bool was_loaded = false;

        void load( const JsonObject &jo, const std::string &src );
        void check() const;
        void finalize();

        // Construction type category
        construction_category_id category = construction_category_id( "OTHER" );
        // Which group does this construction belong to.
        construction_group_str_id group;
        // Additional note displayed along with construction requirements.
        translation pre_note;

        // Beginning object for construction
        ter_str_id pre_terrain;
        furn_str_id pre_furniture;

        // Final object after construction
        ter_str_id post_terrain;
        furn_str_id post_furniture;

        // Item group of byproducts created by the construction on success.
        item_group_id byproduct_item_group;

        // Flags beginning terrain must have
        std::set<std::string> pre_flags;

        // Flags that exclude a given terrain tile
        std::set<std::string> deny_flags;

        // Post construction flags
        std::set<std::string> post_flags;

        /** Skill->skill level mapping. Can be empty. */
        std::map<skill_id, int> required_skills;

        // The requirements specified by "using"
        std::vector<std::pair<requirement_id, int>> reqs_using;
        requirement_id requirements;

        // Time required
        time_duration time;

        // Custom constructibility check
        std::function<bool( const tripoint & )> pre_special;
        // Custom after-effects
        std::function<void( const tripoint & )> post_special;
        // Custom error message display
        std::function<void( const tripoint & )> explain_failure;

        bool pre_special_is_valid_for_dirt = true;

        // NPC assistance adjusted
        int adjusted_time() const;
        std::vector<std::string> get_folded_time_string( int width ) const;

        // Result of construction scaling option
        float time_scale() const;

        bool is_blacklisted() const;

        // Whether the construction needs diggable terrain
        bool needs_diggable;

        // If true, the requirements are generated during finalization
        bool vehicle_start = false;

        // Makes the construction available for selection
        bool on_display = true;

        // Can be built in the dark
        bool dark_craftable = false;

    private:
        std::string get_time_string() const;
};

namespace constructions
{
void load( const JsonObject &jo, const std::string &src );
void reset();
void check_consistency();
void finalize();

const std::vector<construction_id> &get_all_sorted();

// Set all constructions to take the specified time.
void override_build_times( time_duration time );
} // namespace constructions

std::optional<construction_id> construction_menu( bool blueprint );
void complete_construction( Character &who, tripoint_abs_ms &where );
bool can_construct( const construction &con, const tripoint &p );
bool player_can_build( Character &ch, const inventory &inv, const construction &con );



