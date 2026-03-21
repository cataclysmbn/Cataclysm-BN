#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "requirements.h"
#include "translations.h"
#include "type_id.h"

class JsonObject;
class item;
class time_duration;
class Character;
template<typename T>
class detached_ptr;

enum class recipe_filter_flags : int {
    none = 0,
    no_rotten = 1,
};

constexpr recipe_filter_flags operator&( recipe_filter_flags l, recipe_filter_flags r )
{
    return static_cast<recipe_filter_flags>(
               static_cast<unsigned>( l ) & static_cast<unsigned>( r ) );
}

class recipe
{
        friend class recipe_dictionary;

    private:
        itype_id result_ = itype_id::NULL_ID();

    public:
        recipe();

        operator bool() const {
            return !result_.is_null();
        }

        const itype_id &result() const {
            return result_;
        }

        std::vector<std::pair<recipe_id, mod_id>> src;

        bool obsolete = false;

        std::string category;
        std::string subcategory;

        translation description;

        int time = 0; // in movement points (100 per turn)
        int difficulty = 0;

        /** Fetch combined requirement data (inline and via "using" syntax).
         *
         * Use simple_requirements() for player display or when you just want to
         * know the requirements as listed in the json files.  Use
         * deduped_requirements() to calculate actual craftability of a recipe. */
        const requirement_data &simple_requirements() const {
            return requirements_;
        }

        const deduped_requirement_data &deduped_requirements() const {
            return deduped_requirements_;
        }

        const recipe_id &ident() const {
            return ident_;
        }

        bool is_blacklisted() const {
            return requirements_.is_blacklisted();
        }

        // Slower equivalent of is_blacklisted that needs to be used before
        // recipe finalization happens
        bool will_be_blacklisted() const;

        std::function<bool( const item & )> get_component_filter(
            recipe_filter_flags = recipe_filter_flags::none ) const;

        /** Prevent this recipe from ever being added to the player's learned recipies ( used for special NPC crafting ) */
        bool never_learn = false;

        /** If recipe can be used for disassembly fetch the combined requirements */
        requirement_data disassembly_requirements() const {
            if( reversible ) {
                return simple_requirements().disassembly_requirements();
            } else {
                return {};
            }
        }

        /// @returns The name (@ref item::nname) of the resulting item (@ref result).
        /// @param decorated whether the result includes decoration (favorite mark, etc).
        std::string result_name( bool decorated = false ) const;

        std::map<itype_id, int> byproducts;

        skill_id skill_used;
        std::map<skill_id, int> required_skills;

        std::map<skill_id, int> autolearn_requirements; // Skill levels required to autolearn
        std::map<skill_id, int> learn_by_disassembly; // Skill levels required to learn by disassembly
        std::map<itype_id, int> booksets; // Books containing this recipe, and the skill level required
        std::set<flag_id> flags_to_delete; // Flags to delete from the resultant item.

        // Create a string list to describe the skill requirements for this recipe
        // Format: skill_name(level/amount), skill_name(level/amount)
        // Character object (if provided) used to color levels

        // These are primarily used by the crafting menu.
        // Format the primary skill string.
        std::string primary_skill_string( const Character *c, bool print_skill_level ) const;

        // Format the other skills string.  This is also used for searching within the crafting
        // menu which includes the primary skill.
        std::string required_skills_string( const Character *, bool include_primary_skill,
                                            bool print_skill_level ) const;

        // This is used by the basecamp bulletin board.
        std::string required_all_skills_string() const;


        // Create a string to describe the time savings of batch-crafting, if any.
        // Format: "N% at >M units" or "none"
        std::string batch_savings_string() const;

        // Create an item instance as if the recipe was just finished,
        // Contain charges multiplier
        detached_ptr<item> create_result() const;
        std::vector<detached_ptr<item>> create_results( int batch = 1 ) const;

        // Create byproduct instances as if the recipe was just finished
        std::vector<detached_ptr<item>> create_byproducts( int batch = 1 ) const;

        bool has_byproducts() const;

        int batch_time( int batch, float multiplier, size_t assistants ) const;
        time_duration batch_duration( int batch = 1, float multiplier = 1.0,
                                      size_t assistants = 0 ) const;

        bool has_flag( const std::string &flag_name ) const;

        bool is_reversible() const {
            return reversible;
        }

        void load( const JsonObject &jo, const std::string &src );
        void finalize();

        /** Returns a non-empty string describing an inconsistency (if any) in the recipe. */
        std::string get_consistency_error() const;

        bool hot_result() const;

        bool dehydrate_result() const;

        /** Returns the amount or charges recipe will produce. */
        int makes_amount() const;
        /** Returns number of charges of the item needed for single disassembly. */
        int disassembly_batch_size() const;

    private:
        void add_requirements( const std::vector<std::pair<requirement_id, int>> &reqs );

    private:
        recipe_id ident_ = recipe_id::NULL_ID();

        /** Abstract recipes can be inherited from but are themselves disposed of at finalization */
        bool abstract = false;

        /** set learning requirements equal to required skills at finalization? */
        bool autolearn = false;

        /** Does the item spawn contained in container? */
        bool contained = false;

        /** Can recipe be used for disassembly of @ref result via @ref disassembly_requirements */
        bool reversible = false;

        /** What does the item spawn contained in? Unset ("null") means default container. */
        itype_id container = itype_id::NULL_ID();

        /** External requirements (via "using" syntax) where second field is multiplier */
        std::vector<std::pair<requirement_id, int>> reqs_external;

        /** Requires specified inline with the recipe (and replaced upon inheritance) */
        std::vector<std::pair<requirement_id, int>> reqs_internal;

        /** Combined requirements cached when recipe finalized */
        requirement_data requirements_;

        /** Deduped version constructed from the above requirements_ */
        deduped_requirement_data deduped_requirements_;

        std::set<std::string> flags;

        /** If set (zero or positive) set charges of output result for items counted by charges */
        std::optional<int> charges;

        // maximum achievable time reduction, as percentage of the original time.
        // if zero then the recipe has no batch crafting time reduction.
        double batch_rscale = 0.0;
        int batch_rsize = 0; // minimum batch size to needed to reach batch_rscale
        int result_mult = 1; // used by certain batch recipes that create more than one stack of the result

        translation bp_name;
        std::vector<itype_id> bp_resources;
        std::vector<std::pair<std::string, int>> bp_provides;
        std::vector<std::pair<std::string, int>> bp_requires;
        std::vector<std::pair<std::string, int>> bp_excludes;

};

