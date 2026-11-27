#pragma once

#pragma once

#include "character_id.h"
#include "coordinates.h"
#include "hsv_color.h"
#include "item.h"
#include "item_group.h"
#include "location_ptr.h"
#include "point.h"
#include "type_id.h"
#include "visitable.h"

#include <set>
#include <stack>

class vehicle;
class item_location;
class vehicle_cursor;
class npc;

/**
 * Structure, describing vehicle part (i.e., wheel, seat)
 */
struct vehicle_part {
public:
    friend vehicle;
    friend class veh_interact;
    friend visitable<vehicle_cursor>;
    friend location_visitable<vehicle_cursor>;
    friend class turret_data;
    friend class vehicle_base_item_location;

    enum vp_state_flag : int {
        passenger_flag = 1,
        animal_flag = 2,
        carried_flag = 4,
        carrying_flag = 8,
        tracked_flag = 16, // carried vehicle part with tracking enabled
        targets_grid = 32, // Jumper cable is to grid, not vehicle
    };

    vehicle_part();
    vehicle_part(vehicle*);

    vehicle_part(
        const vpart_id& vp, const tripoint_mnt_veh& dp, detached_ptr<item>&& obj, vehicle*);
    vehicle_part(const vehicle_part&, vehicle*);

    vehicle_part(vehicle_part&&);
    auto operator=(vehicle_part&&) -> vehicle_part&;

    /** Check this instance is non-null (not default constructed) */
    explicit operator bool() const;

    auto has_flag(const vp_state_flag flag) const noexcept -> bool { return flag & flags; }
    auto set_flag(const vp_state_flag flag) noexcept -> int { return flags |= flag; }
    auto remove_flag(const vp_state_flag flag) noexcept -> int { return flags &= ~flag; }

    /** this can be removed when vehicles are made into GOs */
    void set_vehicle_hack(vehicle*);
    void refresh_locations_hack(vehicle*);
    auto get_hack_id() const -> int { return hack_id; }

    /**
     * Translated name of a part inclusive of any current status effects
     * with_prefix as true indicates the durability symbol should be prepended
     */
    auto name(bool with_prefix = true) const -> std::string;

    static constexpr int name_offset = 7;
    /** Stack of the containing vehicle's name, when it it stored as part of another vehicle */
    std::stack<std::string, std::vector<std::string>> carry_names;

    /** Specific type of fuel, charges or ammunition currently contained by a part */
    auto ammo_current() const -> itype_id;

    /** Maximum amount of fuel, charges or ammunition that can be contained by a part */
    auto ammo_capacity() const -> int;

    /** Amount of fuel, charges or ammunition currently contained by a part */
    auto ammo_remaining() const -> int;

    /** Type of fuel used by an engine */
    auto fuel_current() const -> itype_id;
    /** Set an engine to use a different type of fuel */
    auto fuel_set(const itype_id& fuel) -> bool;
    /**
     * Set fuel, charges or ammunition for this part removing any existing ammo
     * @param ammo specific type of ammo (must be compatible with vehicle part)
     * @param qty maximum ammo (capped by part capacity) or negative to fill to capacity
     * @return amount of ammo actually set or negative on failure
     */
    auto ammo_set(const itype_id& ammo, int qty = -1) -> int;

    /** Remove all fuel, charges or ammunition (if any) from this part */
    void ammo_unset();

    /**
     * Consume fuel, charges or ammunition (if available)
     * @param qty maximum amount of ammo that should be consumed
     * @param pos current bubble location of part from which ammo is being consumed
     * @return amount consumed which will be between 0 and specified qty
     */
    auto ammo_consume(int qty, const tripoint_bub_ms& pos) -> int;

    /**
     * Consume fuel by energy content.
     * @param ftype Type of fuel to consume
     * @param energy_j Energy to consume, in J
     * @return Energy actually consumed, in J
     */
    auto consume_energy(const itype_id& ftype, double energy_j) -> double;

    /* @retun true if part in current state be reloaded optionally with specific itype_id */
    auto can_reload(const item* obj = nullptr) const -> bool;

    /**
     * If this part is capable of wholly containing something, process the
     * items in there.
     * @param pos Position of this part for item::process
     * @param e_heater Engine has a heater and is on
     */
    void process_contents(const tripoint_bub_ms& pos, bool e_heater, int turns = 1);

    /**
     *  Try adding @param liquid to tank optionally limited by @param qty
     *  @return the remaining liquid, if any
     */
    auto fill_with(detached_ptr<item>&& liquid, int qty = INT_MAX) -> detached_ptr<item>;

    /** Current faults affecting this part (if any) */
    auto faults() const -> const std::set<fault_id>&;

    /** Faults which could potentially occur with this part (if any) */
    auto faults_potential() const -> std::set<fault_id>;

    /** Try to set fault returning false if specified fault cannot occur with this item */
    auto fault_set(const fault_id& f) -> bool;

    /** Get wheel diameter times wheel width (millimeters^2) or return 0 if part is not wheel */
    auto wheel_area() const -> int;

    /** Get wheel diameter (millimeters) or return 0 if part is not wheel */
    auto wheel_diameter() const -> int;

    /** Get wheel width (millimeters) or return 0 if part is not wheel */
    auto wheel_width() const -> int;

    /**
     *  Get NPC currently assigned to this part (seat, turret etc)?
     *  @note checks crew member is alive and currently allied to the player
     *  @return nullptr if no valid crew member is currently assigned
     */
    auto crew() const -> npc*;

    /** Set crew member for this part (seat, turret etc) who must be a player ally)
     *  @return true if part can have crew members and passed npc was suitable
     */
    auto set_crew(const npc& who) -> bool;

    /** Remove any currently assigned crew member for this part */
    void unset_crew();

    /** Reset the target for this part. */
    void reset_target(const tripoint_abs_ms& pos);

    /**
     * @name Part capabilities
     *
     * A part can provide zero or more capabilities. Some capabilities are mutually
     * exclusive, for example a part cannot be both a fuel tank and battery
     */
    /*@{*/

    /** Can this part provide power or propulsion? */
    auto is_engine() const -> bool;

    /** Is this any type of vehicle light? */
    auto is_light() const -> bool;

    /** Can this part store fuel of any type
     * @skip_broke exclude broken parts
     */
    auto is_fuel_store(bool skip_broke = true) const -> bool;

    /** Can this part contain liquid fuels? */
    auto is_tank() const -> bool;

    /** Can this part store electrical charge? */
    auto is_battery() const -> bool;

    /** Is this part a reactor? */
    auto is_reactor() const -> bool;

    /** Does this part provide always-on electrical power? */
    auto is_perpetual_power_source() const -> bool;

    /** is this part currently unable to retain to fluid/charge?
     *  this doesn't take into account whether or not the part has any contents
     *  remaining to leak
     */
    auto is_leaking() const -> bool;

    /** Can this part function as a turret? */
    auto is_turret() const -> bool;

    /** Can a player or NPC use this part as a seat? */
    auto is_seat() const -> bool;

    /* if this is a carried part, what is the name of the carried vehicle */
    auto carried_name() const -> std::string;
    /*@}*/

public:
    /** mount point: x is on the forward/backward axis, y is on the left/right axis */
    tripoint_mnt_veh mount;

    /** mount translated to face.dir [0] and turn_dir [1]; XY rotation only, z is always mount.z()
     */
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    std::array<point_rel_ms, 2> precalc = {{point_rel_ms(-1, -1), point_rel_ms(-1, -1)}};

    /** terrain-topology z offset relative to vehicle origin, double-buffered like precalc */
    std::array<int, 2> z_terrain = {0, 0};

    /** current part health with range [0,durability] */
    auto hp() const -> int;

    /** Current part damage in same units as item::damage. */
    auto damage() const -> int;
    /** max damage of part base */
    auto max_damage() const -> int;

    /** Current part damage level in same units as item::damage_level */
    auto damage_level(int max) const -> int;

    /** Current part damage as a percentage of maximum, with 0.0 being perfect condition */
    auto damage_percent() const -> double;
    /** Current part health as a percentage of maximum, with 1.0 being perfect condition */
    auto health_percent() const -> double;

    /** parts are considered broken at zero health */
    auto is_broken() const -> bool;

    /** parts are unavailable if broken or if carried is true, if they have the CARRIED flag */
    auto is_unavailable(bool carried = true) const -> bool;
    /** parts are available if they aren't unavailable */
    auto is_available(bool carried = true) const -> bool;

    /** how much blood covers part (in turns). */
    int blood = 0;

    /**
     * if tile provides cover.
     * WARNING: do not read it directly, use vpart_position::is_inside() instead
     */
    bool inside = false;

    /**
     * true if this part is removed. The part won't disappear until the end of the turn
     * so our indices can remain consistent.
     */
    bool removed = false;
    bool enabled = true;
    int flags = 0;

    /** ID of player passenger */
    character_id passenger_id;

    /** door is open */
    bool open = false;

    /** direction the part is facing */
    units::angle direction = 0_degrees;


    vpart_id proxy_part_id = vpart_id::NULL_ID();
    char proxy_sym = '\0';
    /**
     * Coordinates for some kind of target; jumper cables and turrets use this
     * Two coordinate pairs are stored: actual target point, and target vehicle center.
     * Both cases use absolute coordinates (relative to world origin)
     */
    std::pair<tripoint_abs_ms, tripoint_abs_ms> target =
        {tripoint_abs_ms(tripoint_min), tripoint_abs_ms(tripoint_min)};

private:
    RGBColorPair part_color_{};

    /** Copies static (i.e. non-item) properties from another part */
    void copy_static_from(const vehicle_part& source);

    /** What type of part is this? */
    vpart_id id;

    /** As a performance optimization we cache the part information here on first lookup */
    mutable const vpart_info* info_cache = nullptr;

    int hack_id = 0; // Hack until they're made into game objects
    location_ptr<item, true> base;
    location_vector<item> items; // inventory

    /** Preferred ammo type when multiple are available */
    itype_id ammo_pref = itype_id::NULL_ID();

    /**
     *  What NPC (if any) is assigned to this part (seat, turret etc)?
     *  @see vehicle_part::crew() accessor which excludes dead and non-allied NPC's
     */
    character_id crew_id;

public:
    // POWER_DRAW_LINKED_PORTAL: portal tap link state (persisted per-part instance).
    dimension_id portal_tap_dim_id;
    tripoint_abs_ms portal_tap_pos;
    bool portal_tap_linked = false;
    /** Get part definition common to all parts of this type */
    auto info() const -> const vpart_info&;

    void serialize(JsonOut& json) const;
    void deserialize(JsonIn& jsin);

    auto get_base() const -> item&;
    auto set_base(detached_ptr<item>&& new_base) -> detached_ptr<item>;

    auto get_items() const -> const std::vector<item*>& { return items.as_vector(); }

    auto clear_items() -> std::vector<detached_ptr<item>> { return items.clear(); }

    void add_item(detached_ptr<item>&& item);

    auto remove_item(item& it) -> detached_ptr<item> { return items.remove(&it); }

    /**
     * Generate the corresponding item from this vehicle part. It includes
     * the hp (item damage), fuel charges (battery or liquids), aspect, ...
     */
    auto properties_to_item() const -> detached_ptr<item>;
    /**
     * Returns an std::vector<item *> of the pieces that should arise from breaking
     * this part.
     */
    auto pieces_for_broken_part() const -> std::vector<detached_ptr<item>>;

    auto get_color(bool ignore_default = false) const -> RGBColorPair;
    void set_color(const RGBColorPair& color) { set_color(color.bg, color.fg); }
    void set_color(const RGBColor& bg, const RGBColor& fg);
};
