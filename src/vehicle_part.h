#pragma once

#pragma once

#include <stack>
#include <set>

#include "character_id.h"
#include "item.h"
#include "item_group.h"
#include "point.h"
#include "visitable.h"
#include "location_ptr.h"

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

        enum : int { passenger_flag = 1,
                     animal_flag = 2,
                     carried_flag = 4,
                     carrying_flag = 8,
                     tracked_flag = 16, //carried vehicle part with tracking enabled
                     targets_grid = 32, // Jumper cable is to grid, not vehicle
                   };

        vehicle_part();
        vehicle_part( vehicle * );

        vehicle_part( const vpart_id &vp, point dp, detached_ptr<item> &&obj, vehicle * );
        vehicle_part( const vehicle_part &, vehicle * );

        vehicle_part( vehicle_part && );
        vehicle_part &operator=( vehicle_part && );

        /** Check this instance is non-null (not default constructed) */
        explicit operator bool() const;

        // TODO: Make all of those use the above enum
        bool has_flag( const int flag ) const noexcept {
            return flag & flags;
        }
        int  set_flag( const int flag )       noexcept {
            return flags |= flag;
        }
        int  remove_flag( const int flag )    noexcept {
            return flags &= ~flag;
        }

        /** this can be removed when vehicles are made into GOs */
        void set_vehicle_hack( vehicle * );
        void refresh_locations_hack( vehicle * );

        /**
         * Translated name of a part inclusive of any current status effects
         * with_prefix as true indicates the durability symbol should be prepended
         */
        std::string name( bool with_prefix = true ) const;

        static constexpr int name_offset = 7;
        /** Stack of the containing vehicle's name, when it it stored as part of another vehicle */
        std::stack<std::string, std::vector<std::string> > carry_names;

        /** Specific type of fuel, charges or ammunition currently contained by a part */
        itype_id ammo_current() const;

        /** Maximum amount of fuel, charges or ammunition that can be contained by a part */
        int ammo_capacity() const;

        /** Amount of fuel, charges or ammunition currently contained by a part */
        int ammo_remaining() const;

        /** Type of fuel used by an engine */
        itype_id fuel_current() const;
        /** Set an engine to use a different type of fuel */
        bool fuel_set( const itype_id &fuel );
        /**
         * Set fuel, charges or ammunition for this part removing any existing ammo
         * @param ammo specific type of ammo (must be compatible with vehicle part)
         * @param qty maximum ammo (capped by part capacity) or negative to fill to capacity
         * @return amount of ammo actually set or negative on failure
         */
        int ammo_set( const itype_id &ammo, int qty = -1 );

        /** Remove all fuel, charges or ammunition (if any) from this part */
        void ammo_unset();

        /**
         * Consume fuel, charges or ammunition (if available)
         * @param qty maximum amount of ammo that should be consumed
         * @param pos current global location of part from which ammo is being consumed
         * @return amount consumed which will be between 0 and specified qty
         */
        int ammo_consume( int qty, const tripoint &pos );

        /**
         * Consume fuel by energy content.
         * @param ftype Type of fuel to consume
         * @param energy_j Energy to consume, in J
         * @return Energy actually consumed, in J
         */
        double consume_energy( const itype_id &ftype, double energy_j );

        /* @retun true if part in current state be reloaded optionally with specific itype_id */
        bool can_reload( const item *obj = nullptr ) const;

        /**
         * If this part is capable of wholly containing something, process the
         * items in there.
         * @param pos Position of this part for item::process
         * @param e_heater Engine has a heater and is on
         */
        void process_contents( const tripoint &pos, bool e_heater );

        /**
         *  Try adding @param liquid to tank optionally limited by @param qty
         *  @return the remaining liquid, if any
         */
        detached_ptr<item> fill_with( detached_ptr<item> &&liquid, int qty = INT_MAX );

        /** Current faults affecting this part (if any) */
        const std::set<fault_id> &faults() const;

        /** Faults which could potentially occur with this part (if any) */
        std::set<fault_id> faults_potential() const;

        /** Try to set fault returning false if specified fault cannot occur with this item */
        bool fault_set( const fault_id &f );

        /** Get wheel diameter times wheel width (inches^2) or return 0 if part is not wheel */
        int wheel_area() const;

        /** Get wheel diameter (inches) or return 0 if part is not wheel */
        int wheel_diameter() const;

        /** Get wheel width (inches) or return 0 if part is not wheel */
        int wheel_width() const;

        /**
         *  Get NPC currently assigned to this part (seat, turret etc)?
         *  @note checks crew member is alive and currently allied to the player
         *  @return nullptr if no valid crew member is currently assigned
         */
        npc *crew() const;

        /** Set crew member for this part (seat, turret etc) who must be a player ally)
         *  @return true if part can have crew members and passed npc was suitable
         */
        bool set_crew( const npc &who );

        /** Remove any currently assigned crew member for this part */
        void unset_crew();

        /** Reset the target for this part. */
        void reset_target( const tripoint &pos );

        /**
         * @name Part capabilities
         *
         * A part can provide zero or more capabilities. Some capabilities are mutually
         * exclusive, for example a part cannot be both a fuel tank and battery
         */
        /*@{*/

        /** Can this part provide power or propulsion? */
        bool is_engine() const;

        /** Is this any type of vehicle light? */
        bool is_light() const;

        /** Can this part store fuel of any type
         * @skip_broke exclude broken parts
         */
        bool is_fuel_store( bool skip_broke = true ) const;

        /** Can this part contain liquid fuels? */
        bool is_tank() const;

        /** Can this part store electrical charge? */
        bool is_battery() const;

        /** Is this part a reactor? */
        bool is_reactor() const;

        /** is this part currently unable to retain to fluid/charge?
         *  this doesn't take into account whether or not the part has any contents
         *  remaining to leak
         */
        bool is_leaking() const;

        /** Can this part function as a turret? */
        bool is_turret() const;

        /** Can a player or NPC use this part as a seat? */
        bool is_seat() const;

        /* if this is a carried part, what is the name of the carried vehicle */
        std::string carried_name() const;
        /*@}*/

    public:
        /** mount point: x is on the forward/backward axis, y is on the left/right axis */
        point mount;

        /** mount translated to face.dir [0] and turn_dir [1] */
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        std::array<tripoint, 2> precalc = { { tripoint( -1, -1, 0 ), tripoint( -1, -1, 0 ) } };

        /** current part health with range [0,durability] */
        int hp() const;

        /** Current part damage in same units as item::damage. */
        int damage() const;
        /** max damage of part base */
        int max_damage() const;

        /** Current part damage level in same units as item::damage_level */
        int damage_level( int max ) const;

        /** Current part damage as a percentage of maximum, with 0.0 being perfect condition */
        double damage_percent() const;
        /** Current part health as a percentage of maximum, with 1.0 being perfect condition */
        double health_percent() const;

        /** parts are considered broken at zero health */
        bool is_broken() const;

        /** parts are unavailable if broken or if carried is true, if they have the CARRIED flag */
        bool is_unavailable( bool carried = true ) const;
        /** parts are available if they aren't unavailable */
        bool is_available( bool carried = true ) const;

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
        std::pair<tripoint, tripoint> target = { tripoint_min, tripoint_min };

    private:

        /** Copies static (i.e. non-item) properties from another part */
        void copy_static_from( const vehicle_part &source );

        /** What type of part is this? */
        vpart_id id;

        /** As a performance optimization we cache the part information here on first lookup */
        mutable const vpart_info *info_cache = nullptr;

        int hack_id = 0; //Hack until they're made into game objects
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
        /** Get part definition common to all parts of this type */
        const vpart_info &info() const;

        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );

        item &get_base() const;
        detached_ptr<item> set_base( detached_ptr<item> &&new_base );

        const std::vector<item *> &get_items() const {
            return items.as_vector();
        }

        std::vector<detached_ptr<item>> clear_items() {
            return items.clear();
        }

        void add_item( detached_ptr<item> &&item );

        detached_ptr<item> remove_item( item &it ) {
            return items.remove( &it );
        }

        /**
         * Generate the corresponding item from this vehicle part. It includes
         * the hp (item damage), fuel charges (battery or liquids), aspect, ...
         */
        detached_ptr<item> properties_to_item() const;
        /**
         * Returns an std::vector<item *> of the pieces that should arise from breaking
         * this part.
         */
        std::vector<detached_ptr<item>> pieces_for_broken_part() const;
};


