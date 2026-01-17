#pragma once

#include <map>
#include <string>
#include <vector>

#include "calendar.h"
#include "coordinates.h"
#include "detached_ptr.h"
#include "int_id.h"
#include "mtype.h"
#include "string_id.h"
#include "type_id.h"

class Character;
class JsonObject;
class JsonOut;
class player;
class item;

namespace hunting
{

// Result of a snare check
struct snare_result {
    bool success = false;
    mtype_id prey_id;
    std::string message;
};

// Result of processing a snare catch (includes corpse generation)
struct snare_catch_result {
    snare_result hunt_result;
    detached_ptr<item> corpse; // nullptr if failed or prey is "none"
};

// Single prey entry with mtype_id and spawn weight
struct prey_entry {
    mtype_id prey;
    int weight = 100;

    void deserialize( const JsonObject &jo );
};

// Habitat-specific prey data
struct habitat_prey_data {
    std::map<std::string, std::vector<prey_entry>> prey_types; // "SMALL_GAME" -> prey list

    void deserialize( const JsonObject &jo );
};

// Main hunting data structure
struct snaring_hunting_data {
    string_id<snaring_hunting_data> id; // Furniture base name (e.g., "f_wire_snare")
    std::map<std::string, habitat_prey_data> habitats; // ter_str_id -> prey data

    bool was_loaded = false;
    void load( const JsonObject &jo, const std::string &src );
    void deserialize( const JsonObject &jo );
    void check() const;
    static void load_hunting_data( const JsonObject &jo, const std::string &src );
    static void reset();
    static void check_consistency();
    static const std::vector<snaring_hunting_data> &get_all();
};

using snaring_hunting_data_id = string_id<snaring_hunting_data>;

// OMT population tracking
class omt_population_tracker
{
    public:
        struct omt_data {
            int population = 100;
            time_point last_visit;
            time_point last_recovery;
        };
        std::map<tripoint_abs_omt, omt_data> omt_populations;

        void record_visit( const tripoint &pos );
        void apply_catch_penalty( const tripoint &pos );
        int get_population( const tripoint &pos ) const;
        void process_recovery();
        void serialize( JsonOut &json ) const;
        void deserialize( const JsonObject &jo );
};

omt_population_tracker &get_population_tracker();

// Extract bait types from item's BAIT_XXX flags
std::vector<std::string> extract_bait_types( const item *bait_item );

// Find hunting_data by furniture at position
const snaring_hunting_data *find_hunting_data_for_furniture( const tripoint &pos );

// Main hunting functions
snare_result check_snare( const tripoint &pos, const std::vector<std::string> &bait_flags_str,
                          const player &p, int proximity_penalty );

// Process snare catch: run check_snare and generate corpse if successful
snare_catch_result process_snare_catch( const tripoint &pos,
                                        const std::vector<std::string> &bait_flags_str,
                                        const Character &ch, int proximity_penalty,
                                        time_point snared_time );

float calculate_skill_multiplier( const player &p );
int calculate_presence_penalty( const tripoint &pos );

} // namespace hunting
