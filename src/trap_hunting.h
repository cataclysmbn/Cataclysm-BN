#pragma once

#include <expected>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "calendar.h"
#include "coordinates.h"
#include "string_id.h"
#include "type_id.h"

class JsonObject;
class item;
class map;
class mapbuffer;
class overmap_land_use_code;
struct mtype;
enum class mapbuffer_lookup_mode : int;

using overmap_land_use_code_id = string_id<overmap_land_use_code>;

/// Prey pool and base catch rate for overmap tiles with given land use codes.
class hunting_habitat
{
    public:
        hunting_habitat_id id;
        bool was_loaded = false;
        /// Land use codes that belong to this habitat.
        std::vector<overmap_land_use_code_id> land_use_codes;
        /// Monster group prey is drawn from. Entry frequencies and seasons are respected.
        mongroup_id prey_group;
        /// Expected catches per day with neutral trap, bait, skill and full cover.
        double catch_rate = 0.0;

        auto load( const JsonObject &jo, const std::string &src ) -> void;
        auto check() const -> void;

        static auto load_hunting_habitat( const JsonObject &jo, const std::string &src ) -> void;
        static auto finalize() -> void;
        static auto check_consistency() -> void;
        static auto reset() -> void;
        static auto get_all() -> const std::vector<hunting_habitat> &; // *NOPAD*
        /// Habitat that claims @p code.
        static auto for_land_use( const overmap_land_use_code_id &code ) ->
        std::optional<hunting_habitat_id>;
};

/// Items that can be set as bait on a hunting trap.
class hunting_bait
{
    public:
        hunting_bait_id id;
        bool was_loaded = false;
        /// Item types that count as this bait.
        std::vector<itype_id> items;
        /// Multiplier applied to the catch rate.
        double attraction = 1.0;
        /// Prey species this bait lures.
        std::set<species_id> species;
        /// Prey types this bait lures, on top of @ref species. Both empty lures any prey.
        std::set<mtype_id> monsters;

        auto load( const JsonObject &jo, const std::string &src ) -> void;
        auto check() const -> void;
        auto lures( const mtype &prey ) const -> bool;

        static auto load_hunting_bait( const JsonObject &jo, const std::string &src ) -> void;
        static auto finalize() -> void;
        static auto check_consistency() -> void;
        static auto reset() -> void;
        static auto get_all() -> const std::vector<hunting_bait> &; // *NOPAD*
        /// Bait profile of item type @p type.
        static auto for_item( const itype_id &type ) -> std::optional<hunting_bait_id>;
};

namespace trap_hunting
{

/// How suitable a tile is for baiting a hunting trap.
struct site_quality {
    /// Expected catches per day from the surrounding habitat, before other modifiers.
    double habitat_rate = 0.0;
    /// Multiplier from natural ground and vegetation near the trap, 0 to 1.
    double cover = 0.0;
};

/// Evaluate @p pos in the reality bubble as a bait site. Fails with a player-facing reason.
auto evaluate_site( const map &here,
                    const tripoint_bub_ms &pos ) -> std::expected<site_quality, std::string>;

struct bait_mark_options {
    tripoint_abs_ms pos;
    time_point placed_at;
    int trapper_skill = 0;
    double cover = 0.0;
};

/// Tag @p it as bait on the hunting trap at @p opts.pos. The tags hold all hunting state, so
/// taking the item away by any means simply leaves an empty trap.
auto mark_as_bait( item &it, const bait_mark_options &opts ) -> void;
/// Remove bait tags so @p it becomes an ordinary item.
auto release_bait( item &it ) -> void;
/// Whether @p it is bait set on the hunting trap at @p pos.
auto is_bait_for( const item &it, const tripoint_abs_ms &pos ) -> bool;
/// Whether @p it carries bait tags at all.
auto has_bait_tags( const item &it ) -> bool;

/// Bait set on the hunting trap at @p pos, or nullptr.
auto find_bait( map &here, const tripoint_bub_ms &pos ) -> item *; // *NOPAD*

/// Catch rate multiplier for an observer @p distance tiles away from the trap.
auto presence_factor( int distance ) -> double;
/// Catch rate multiplier from the trapper's skill.
auto skill_factor( int trapper_skill ) -> double;

struct catch_roll_options {
    /// Expected catches per day.
    double rate_per_day = 0.0;
    /// Time the bait was out and fresh.
    time_duration exposure = 0_turns;
    /// Uniform random number in [0, 1).
    double roll = 0.0;
};

/// Chance of at least one catch, modeled as a Poisson process.
auto catch_chance( double rate_per_day, time_duration exposure ) -> double;
/// Time until the first catch, if one happens within the exposure.
auto catch_delay( const catch_roll_options &opts ) -> std::optional<time_duration>;

struct resolve_options {
    mapbuffer &buffer;
    tripoint_abs_ms pos;
    time_point now;
    /// Multiplier from creatures nearby, see @ref presence_factor.
    double presence = 1.0;
    mapbuffer_lookup_mode lookup_mode;
};

/// Roll for a catch since the bait at @p opts.pos was last checked.
/// On a catch the bait is eaten and the prey's corpse is left on the trap. Traps that are removed
/// when triggered are removed and leave their drops.
auto resolve( const resolve_options &opts ) -> std::optional<mtype_id>;

/// Resolve every baited hunting trap in the simulated area of @p buffer.
auto process_simulated( mapbuffer &buffer, const tripoint_abs_ms &observer ) -> void;

/// Release any bait on @p pos after the trap there changed.
auto release_bait_at( mapbuffer &buffer, const tripoint_abs_ms &pos,
                      mapbuffer_lookup_mode mode ) -> void;

} // namespace trap_hunting
