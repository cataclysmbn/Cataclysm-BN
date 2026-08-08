#pragma once

#include "activity_actor.h"
#include "craft_command.h"

#include <optional>
#include <unordered_set>

#include "character_id.h"
#include "coordinates.h"
#include "crafting.h"
#include "enums.h"
#include "item_handling_util.h"
#include "location_ptr.h"
#include "locations.h"
#include "mapdata.h"
#include "memory_fast.h"
#include "pickup_token.h"
#include "point.h"
#include "requirements.h"
#include "safe_reference.h"
#include "type_id.h"
#include "units_energy.h"

class Creature;
class vehicle;
class mapbuffer;
struct partial_con;

// Forward declarations for enum types defined elsewhere
enum class do_activity_reason : int;

class aim_activity_actor : public activity_actor
{
    private:
        safe_reference<item> weapon;
        location_ptr<item> fake_weapon;
        units::energy bp_cost_per_shot = 0_J;
        int stamina_cost_per_shot = 0;
        std::vector<tripoint_abs_ms> fin_trajectory;

    public:
        std::string action;
        int aif_duration = 0; // Counts aim-and-fire duration
        bool aiming_at_critter = false; // Whether aiming at critter or a tile
        bool snap_to_target = false;
        bool shifting_view = false;
        tripoint_rel_ms initial_view_offset;
        /** Target UI requested to abort aiming */
        bool aborted = false;
        /** RELOAD_AND_SHOOT weapon is kept loaded by the activity */
        bool loaded_RAS_weapon = false;
        /** Item location for RAS weapon reload */
        safe_reference<item> reload_loc;
        /** if true abort if no targets are available when re-entering aiming ui after shooting */
        bool abort_if_no_targets = false;
        /**
         * Target UI requested to abort aiming and reload weapon
         * Implies aborted = true
         */
        bool reload_requested = false;
        /**
         * A friendly creature may enter line of fire during aim-and-shoot,
         * and that generates a warning to proceed/abort. If player decides to
         * proceed, that creature is saved in this vector to prevent the same warning
         * from popping up on the following turn(s).
         */
        std::vector<weak_ptr_fast<Creature>> acceptable_losses;

        aim_activity_actor();

        /** Aiming wielded gun */
        static std::unique_ptr<aim_activity_actor> use_wielded();

        /** Aiming fake gun provided by a bionic */
        static std::unique_ptr<aim_activity_actor> use_bionic( detached_ptr<item> &&fake_gun,
                const units::energy &cost_per_shot );

        /** Aiming gun provided by gear */
        static std::unique_ptr<aim_activity_actor> use_gear( item *gun );

        /** Aiming fake gun provided by a mutation */
        static std::unique_ptr<aim_activity_actor> use_mutation( detached_ptr<item> &&fake_gun );

        activity_id get_type() const override {
            return activity_id( "ACT_AIM" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void canceled( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );

        item *get_weapon();
        void restore_view();
        // Load/unload a RELOAD_AND_SHOOT weapon
        bool load_RAS_weapon();
        void unload_RAS_weapon();
};

class autodrive_activity_actor : public activity_actor
{
    private:
        vehicle *player_vehicle = nullptr;

    public:
        autodrive_activity_actor() = default;

        activity_id get_type() const override {
            return activity_id( "ACT_AUTODRIVE" );
        }

        void start( player_activity &act, Character & ) override;
        void do_turn( player_activity &, Character & ) override;
        void canceled( player_activity &, Character & ) override;
        void finish( player_activity &act, Character & ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};

class craft_activity_actor final : public activity_actor
{
    protected:
        const recipe *rec = nullptr;
        int batch_size = 1;
        int craft_counter = 0;  // 0 to 10,000,000 — mirrors item's counter field
        tripoint_abs_ms location;

        bench_type bench = bench_type::ground;
        int tools_mult_percent = 100;
        tripoint_abs_ms bench_pos;

        std::vector<comp_selection<item_comp>> item_selections;
        std::vector<comp_selection<tool_comp>> tool_selections;

        bool tools_prepaid = false;
        bool is_long = false;
        bool is_valid = false;
        int last_turn_nr = -1;  // turn# when last do_turn ran; -1 = never set
        float cached_tools_mult = 0.0f;   // 0 = not yet computed; set once in start()

        bool can_resume_with_internal( const activity_actor &other, const Character & ) const override {
            const auto &c_actor = static_cast<const craft_activity_actor &>( other );
            return equivalent_activity( c_actor );
        }

        bool equivalent_activity( const craft_activity_actor &other ) const {
            return location == other.location &&
                   rec == other.rec &&
                   batch_size == other.batch_size &&
                   bench == other.bench &&
                   bench_pos == other.bench_pos;
        }

    public:
        craft_activity_actor() = default;
        explicit craft_activity_actor(
            const recipe *rec,
            int batch_size = 1,
            int craft_counter = 0,
            const tripoint_abs_ms &location = tripoint_abs_ms::zero(),
            bench_type bench = bench_type::ground,
            int tools_mult_percent = 100,
            const tripoint_abs_ms &bench_pos = tripoint_abs_ms::zero(),
            std::vector<comp_selection<item_comp>> item_selections = {},
            std::vector<comp_selection<tool_comp>> tool_selections = {},
            bool tools_prepaid = false,
            bool is_long = false
        );

        activity_id get_type() const override {
            return activity_id( "ACT_CRAFT" );
        }

        void calc_all_moves( player_activity &act, Character &who ) override;
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void canceled( player_activity &/*act*/, Character &/*who*/ ) override {}

        const recipe *get_recipe() const { return rec; }
        int get_batch_size() const { return batch_size; }
        int get_craft_counter() const { return craft_counter; }
        const tripoint_abs_ms &get_location() const { return location; }
        bool are_tools_prepaid() const { return tools_prepaid; }
        bench_type get_bench_type() const { return bench; }
        int get_tools_mult_percent() const { return tools_mult_percent; }
        const tripoint_abs_ms &get_bench_pos() const { return bench_pos; }

        act_progress_message get_progress_message( const player_activity &act,
                const Character &who ) const override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );

    private:
        auto find_in_progress_craft( const player_activity &act,
                                     Character &who ) const -> item *; // *NOPAD*
        void do_complete_craft( player_activity &act, Character &who );
        void refresh_speed( player_activity &act, const Character &who, const item &craft_item,
                            std::optional<bench_location> bench = std::nullopt ) const;

};

class dig_activity_actor : public activity_actor
{
    private:
        int moves_total;
        /** location of the dig **/
        tripoint_abs_ms location;
        std::string result_terrain;
        tripoint_abs_ms byproducts_location;
        std::string byproducts_item_group;

        /**
         * Returns true if @p other and `this` are "equivalent" in the sense that
         *  `this` can be resumed instead of starting @p other.
         */
        bool equivalent_activity( const dig_activity_actor &other ) const {
            return  location == other.location &&
                    result_terrain == other.result_terrain &&
                    byproducts_location == other.byproducts_location &&
                    byproducts_item_group == other.byproducts_item_group;
        }

        /**
         * @pre @p other is a `dig_activity_actor`
         */
        bool can_resume_with_internal( const activity_actor &other, const Character & ) const override {
            const dig_activity_actor &d_actor = static_cast<const dig_activity_actor &>( other );
            return equivalent_activity( d_actor );
        }

    public:
        dig_activity_actor(
            int dig_moves, const tripoint_abs_ms &dig_loc,
            const std::string &resulting_ter, const tripoint_abs_ms &dump_loc,
            const std::string &dump_item_group
        ):
            moves_total( dig_moves ), location( dig_loc ),
            result_terrain( resulting_ter ),
            byproducts_location( dump_loc ),
            byproducts_item_group( dump_item_group ) {}

        activity_id get_type() const override {
            return activity_id( "ACT_DIG" );
        }

        void start( player_activity &act, Character & ) override;
        void do_turn( player_activity &, Character & ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};

class dig_channel_activity_actor : public activity_actor
{
    private:
        int moves_total;
        /** location of the dig **/
        tripoint_abs_ms location;
        std::string result_terrain;
        tripoint_abs_ms byproducts_location;
        std::string byproducts_item_group;

        /**
         * Returns true if @p other and `this` are "equivalent" in the sense that
         *  `this` can be resumed instead of starting @p other.
         */
        bool equivalent_activity( const dig_channel_activity_actor &other ) const {
            return  location == other.location &&
                    result_terrain == other.result_terrain &&
                    byproducts_location == other.byproducts_location &&
                    byproducts_item_group == other.byproducts_item_group;
        }

        /**
         * @pre @p other is a `dig_activity_actor`
         */
        bool can_resume_with_internal( const activity_actor &other, const Character & ) const override {
            const dig_channel_activity_actor &dc_actor = static_cast<const dig_channel_activity_actor &>
                    ( other );
            return equivalent_activity( dc_actor );
        }

    public:
        dig_channel_activity_actor(
            int dig_moves, const tripoint_abs_ms &dig_loc,
            const std::string &resulting_ter, const tripoint_abs_ms &dump_loc,
            const std::string &dump_item_group
        ):
            moves_total( dig_moves ), location( dig_loc ),
            result_terrain( resulting_ter ),
            byproducts_location( dump_loc ),
            byproducts_item_group( dump_item_group ) {}

        activity_id get_type() const override {
            return activity_id( "ACT_DIG_CHANNEL" );
        }

        void start( player_activity &act, Character & ) override;
        void do_turn( player_activity &, Character & ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};

class disassemble_activity_actor : public activity_actor
{
    private:
        std::vector<iuse_location> targets;
        tripoint_abs_ms pos;
        bool recursive = false;

    public:
        disassemble_activity_actor() = default;
        disassemble_activity_actor(
            std::vector<iuse_location> &&targets,
            tripoint_abs_ms pos,
            bool recursive
        ) : targets( std::move( targets ) ), pos( pos ), recursive( recursive ) {}
        ~disassemble_activity_actor() = default;

        activity_id get_type() const override {
            return activity_id( "ACT_DISASSEMBLE" );
        }
        void calc_all_moves( player_activity &act, Character &who ) override;
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &, Character & ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );

        bool try_start_single( player_activity &act, Character &who );
        void process_target( player_activity &, iuse_location &target );
};

class drop_activity_actor : public activity_actor
{
    private:
        std::list<pickup::act_item> items;
        bool force_ground = false;
        tripoint_rel_ms relpos;

    public:
        drop_activity_actor() = default;
        drop_activity_actor( Character &ch, const drop_locations &items,
                             bool force_ground, const tripoint_rel_ms &relpos );

        activity_id get_type() const override {
            return activity_id( "ACT_DROP" );
        }

        void start( player_activity &, Character & ) override;
        void do_turn( player_activity &, Character &who ) override;
        void finish( player_activity &, Character & ) override {};

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};

class hacking_activity_actor : public activity_actor
{
    private:
        bool using_bionic = false;
        tripoint_abs_ms target_pos;

    public:
        struct use_bionic {};

        hacking_activity_actor() = default;
        hacking_activity_actor( use_bionic );
        hacking_activity_actor( use_bionic, const tripoint_abs_ms &pos );
        explicit hacking_activity_actor( const tripoint_abs_ms &pos );

        activity_id get_type() const override {
            return activity_id( "ACT_HACKING" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &, Character & ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class hacksaw_activity_actor : public activity_actor
{
    public:
        explicit hacksaw_activity_actor( const tripoint_abs_ms &target,
                                         const safe_reference<item> &tool ) : target( target ), tool( tool ) {};

        activity_id get_type() const override {
            return activity_id( "ACT_HACKSAW" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &/*act*/, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );

        // debugmsg causes a backtrace when fired during cata_test
        bool testing = false;  // NOLINT(cata-serialize)
    private:
        tripoint_abs_ms target;
        safe_reference<item> tool;

        bool can_resume_with_internal( const activity_actor &other,
                                       const Character &/*who*/ ) const override {
            const hacksaw_activity_actor &actor = static_cast<const hacksaw_activity_actor &>
                                                  ( other );
            return actor.target == target;
        }
};

class boltcutting_activity_actor : public activity_actor
{
    public:
        explicit boltcutting_activity_actor( const tripoint_abs_ms &target,
                                             const safe_reference<item> tool ) : target( target ), tool( tool ) {};

        activity_id get_type() const override {
            return activity_id( "ACT_BOLTCUTTING" );
        }

        void start( player_activity &act, Character &/*who*/ ) override;
        void do_turn( player_activity &/*act*/, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );

        // debugmsg causes a backtrace when fired during cata_test
        bool testing = false;

    private:
        tripoint_abs_ms target;
        safe_reference<item> tool;

        bool can_resume_with_internal( const activity_actor &other,
                                       const Character &/*who*/ ) const override {
            const boltcutting_activity_actor &actor = static_cast<const boltcutting_activity_actor &>
                    ( other );
            return actor.target == target && actor.tool == tool;
        }
};

class lockpick_activity_actor : public activity_actor
{
    private:
        int moves_total;
        safe_reference<item> lockpick;
        location_ptr<item> fake_lockpick;
        tripoint_abs_ms target;

        lockpick_activity_actor(
            int moves_total,
            safe_reference<item> lockpick,
            detached_ptr<item> &&fake_lockpick,
            const tripoint_abs_ms &target
        ) : moves_total( moves_total ), lockpick( lockpick ), fake_lockpick( new fake_item_location() ),
            target( target ) {
            this->fake_lockpick = std::move( fake_lockpick );
        };

    public:
        /** Use regular lockpick. 'target' is in global coords */
        static std::unique_ptr<lockpick_activity_actor> use_item(
            int moves_total,
            item &lockpick,
            const tripoint_abs_ms &target
        );

        /** Use bionic lockpick. 'target' is in global coords */
        static std::unique_ptr<lockpick_activity_actor> use_bionic(
            detached_ptr<item> &&fake_lockpick,
            const tripoint_abs_ms &target
        );

        activity_id get_type() const override {
            return activity_id( "ACT_LOCKPICK" );
        }

        void start( player_activity &act, Character & ) override;
        void do_turn( player_activity &, Character & ) override;
        void finish( player_activity &act, Character &who ) override;

        static bool is_pickable( mapbuffer &here, const tripoint_abs_ms &p );
        static std::optional<tripoint_abs_ms> select_location( avatar &you );

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};

class migration_cancel_activity_actor : public activity_actor
{
    public:
        migration_cancel_activity_actor() = default;

        activity_id get_type() const override {
            return activity_id( "ACT_MIGRATION_CANCEL" );
        }

        void start( player_activity &, Character & ) override {};
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &, Character & ) override {};

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};

class move_items_activity_actor : public activity_actor
{
    private:
        std::vector<safe_reference<item>> target_items;
        std::vector<int> quantities;
        bool to_vehicle;
        tripoint_rel_ms relative_destination;

    public:
        move_items_activity_actor( std::vector<item *> items, std::vector<int> quantities,
                                   bool to_vehicle, tripoint_rel_ms relative_destination ) :
            quantities( quantities ), to_vehicle( to_vehicle ),
            relative_destination( relative_destination ) {

            for( item *&it : items ) {
                target_items.emplace_back( it );
            }
        }

        activity_id get_type() const override {
            return activity_id( "ACT_MOVE_ITEMS" );
        }

        void start( player_activity &, Character & ) override {};
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &, Character & ) override {};


        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};

class toggle_gate_activity_actor : public activity_actor
{
    private:
        int moves_total;
        tripoint_abs_ms placement;

        /**
         * @pre @p other is a toggle_gate_activity_actor
         */
        bool can_resume_with_internal( const activity_actor &other, const Character & ) const override {
            const toggle_gate_activity_actor &og_actor = static_cast<const toggle_gate_activity_actor &>
                    ( other );
            return placement == og_actor.placement;
        }

    public:
        toggle_gate_activity_actor( int gate_moves, const tripoint_abs_ms &gate_placement ) :
            moves_total( gate_moves ), placement( gate_placement ) {}

        activity_id get_type() const override {
            return activity_id( "ACT_TOGGLE_GATE" );
        }

        void start( player_activity &act, Character & ) override;
        void do_turn( player_activity &, Character & ) override;
        void finish( player_activity &act, Character & ) override;


        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};

class pickup_activity_actor : public activity_actor
{
    private:
        /** Target items and the quantities thereof */
        std::vector<pickup::pick_drop_selection> target_items;

        /**
         * Position of the character when the activity is started. This is
         * stored so that we can cancel the activity if the player moves
         * (e.g. if the player is in a moving vehicle). This should be null
         * if not grabbing from the ground.
         */
        std::optional<tripoint_abs_ms> starting_pos;

        bool thievery_witness = false;

    public:
        pickup_activity_actor( const std::vector<pickup::pick_drop_selection> &target_items,
                               const std::optional<tripoint_abs_ms> &starting_pos )
            : target_items( target_items )
            , starting_pos( starting_pos ) {}
        void set_thievery_witness() { thievery_witness = true; }

        activity_id get_type() const override {
            return activity_id( "ACT_PICKUP" );
        }

        void start( player_activity &, Character & ) override {};
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &, Character & ) override {};


        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};

class stash_activity_actor : public activity_actor
{
    private:
        std::list<pickup::act_item> items;
        tripoint_rel_ms relpos;

    public:
        stash_activity_actor() = default;
        stash_activity_actor( Character &ch, const drop_locations &items, const tripoint_rel_ms &relpos );

        activity_id get_type() const override {
            return activity_id( "ACT_STASH" );
        }

        void start( player_activity &, Character & ) override;
        void do_turn( player_activity &, Character &who ) override;
        void finish( player_activity &, Character & ) override {};

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};

class throw_activity_actor : public activity_actor
{
    private:

        safe_reference<item> target;
        std::optional<tripoint_abs_ms> blind_throw_from_pos;

    public:
        throw_activity_actor() = default;
        throw_activity_actor(
            item &target,
            std::optional<tripoint_abs_ms> blind_throw_from_pos
        ) : target( &target ),
            blind_throw_from_pos( blind_throw_from_pos ) {}
        ~throw_activity_actor() = default;

        activity_id get_type() const override {
            return activity_id( "ACT_THROW" );
        }

        void start( player_activity &, Character & ) override {};
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &, Character & ) override {};

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};


class oxytorch_activity_actor : public activity_actor
{
    public:
        explicit oxytorch_activity_actor( const tripoint_abs_ms &target,
                                          const safe_reference<item> &tool ) : target( target ), tool( tool ) {};

        activity_id get_type() const override {
            return activity_id( "ACT_OXYTORCH" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &/*act*/, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );

        // debugmsg causes a backtrace when fired during cata_test
        bool testing = false;  // NOLINT(cata-serialize)
    private:
        tripoint_abs_ms target;
        safe_reference<item> tool;

        bool can_resume_with_internal( const activity_actor &other,
                                       const Character &/*who*/ ) const override {
            const oxytorch_activity_actor &actor = static_cast<const oxytorch_activity_actor &>
                                                   ( other );
            return actor.target == target;
        }
};

class construction_activity_actor : public activity_actor
{
    private:
        tripoint_abs_ms target;
        partial_con *pc;
    public:
        explicit construction_activity_actor( const tripoint_abs_ms &target ) : target( target ) {
        };

        activity_id get_type() const override {
            return activity_id( "ACT_BUILD" );
        }

        void calc_all_moves( player_activity &act, Character &who ) override;

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};

class assist_activity_actor : public activity_actor
{
    public:
        explicit assist_activity_actor() {
        };

        activity_id get_type() const override {
            return activity_id( "ACT_ASSIST" );
        }

        void calc_all_moves( player_activity & /*act*/, Character &/*who*/ ) override {};

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &/*act*/, Character &/*who*/ ) override {};
        void finish( player_activity &/*act*/, Character &/*who*/ ) override {};

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );

};

class salvage_activity_actor : public activity_actor
{
    private:
        iuse_locations targets;
        tripoint_abs_ms pos;
        bool mute_prompts = false;
    public:
        salvage_activity_actor() = default;
        salvage_activity_actor(
            iuse_locations &&targets,
            tripoint_abs_ms pos,
            bool mute_prompts = false
        ) : targets( std::move( targets ) ), pos( pos ), mute_prompts( mute_prompts ) {}

        ~salvage_activity_actor() = default;

        activity_id get_type() const override {
            return activity_id( "ACT_LONGSALVAGE" );
        }

        void calc_all_moves( player_activity & /*act*/, Character &/*who*/ ) override;

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &/*act*/, Character &/*who*/ ) override;
        void finish( player_activity &/*act*/, Character &/*who*/ ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};

class liquid_transfer_actor : public activity_actor
{
    private:
        liquid_source_type source_type;
        tripoint_abs_ms source_pos;
        int source_part_index = -1;

        liquid_target_type target_type;
        tripoint_abs_ms target_pos;
        safe_reference<item> target_container;

        bool can_resume_with_internal( const activity_actor &other,
                                       const Character & ) const override {
            const auto &c = static_cast<const liquid_transfer_actor &>( other );
            return source_type == c.source_type &&
                   source_pos == c.source_pos &&
                   source_part_index == c.source_part_index &&
                   target_type == c.target_type &&
                   target_pos == c.target_pos &&
                   target_container == c.target_container;
        }

    public:
        liquid_transfer_actor() = default;
        liquid_transfer_actor(
            liquid_source_type src_type,
            const tripoint_abs_ms &src_pos,
            int src_part_index,
            liquid_target_type tgt_type,
            const tripoint_abs_ms &tgt_pos,
            safe_reference<item> tgt_container
        );

        activity_id get_type() const override {
            return activity_id( "ACT_FILL_LIQUID" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

struct vehicle_work_actor_options {
    char command = 0;
    tripoint_abs_ms part_pos = tripoint_abs_ms::zero();
    tripoint_mnt_veh cursor_mount = tripoint_mnt_veh::zero();
    vpart_id part_type;
    int part_index = -1;
    int moves_total = 0;
    std::unordered_set<tripoint_abs_ms> vehicle_points;
};

class vehicle_work_actor : public activity_actor
{
    private:
        char command;
        tripoint_abs_ms part_pos;
        tripoint_mnt_veh cursor_mount;
        vpart_id part_type;
        int part_index;
        int moves_total;
        std::unordered_set<tripoint_abs_ms> vehicle_points;

        bool can_resume_with_internal( const activity_actor &other,
                                       const Character & ) const override {
            const auto &c = static_cast<const vehicle_work_actor &>( other );
            return command == c.command &&
                   part_pos == c.part_pos &&
                   cursor_mount == c.cursor_mount &&
                   part_type == c.part_type &&
                   part_index == c.part_index &&
                   moves_total == c.moves_total;
        }

    public:
        vehicle_work_actor() = default;
        explicit vehicle_work_actor( vehicle_work_actor_options options );

        activity_id get_type() const override {
            return activity_id( "ACT_VEHICLE" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );

        int get_part_index() const { return part_index; }
        const tripoint_abs_ms &get_part_pos() const { return part_pos; }
};

class enchant_activity_actor : public activity_actor
{
    private:
        safe_reference<item> target;
        furn_str_id furn;
        std::string enchanter_id;
        int moves_total;

    public:
        enchant_activity_actor() = default;
        enchant_activity_actor(
            item &target,
            furn_str_id furn,
            std::string enchanter_id,
            int moves
        ) : target( &target ),
            furn( furn ),
            enchanter_id( enchanter_id ),
            moves_total( moves ) {}
        ~enchant_activity_actor() = default;

        activity_id get_type() const override {
            return activity_id( "ACT_ENCHANT" );
        }

        void start( player_activity &, Character & ) override;
        void do_turn( player_activity &, Character & ) override;
        void finish( player_activity &, Character & ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
};

// Repeat type for repair activity
enum repeat_type : int {
    // REPEAT_INIT should be zero. In some scenarios (veh welder), activity value default to zero.
    REPEAT_INIT = 0,    // Haven't found repeat value yet.
    REPEAT_ONCE,        // Repeat just once
    REPEAT_FOREVER,     // Repeat for as long as possible
    REPEAT_FULL,        // Repeat until damage==0
    REPEAT_EVENT,       // Repeat until something interesting happens
    REPEAT_CANCEL,      // Stop repeating
};

class repair_actor : public activity_actor
{
    private:
        bool is_hack = false;
        hack_type_t hack_type = static_cast<hack_type_t>( 0 );
        tripoint_abs_ms target_pos;
        itype_id tool_type;
        int crafter_part_index = -1;
        std::string iuse_name;
        safe_reference<item> tool;
        int item_pos = 0;
        int repeat = 0;                    // repeat_type enum
        safe_reference<item> fix_target;   // item being repaired (targets[1])

        bool can_resume_with_internal( const activity_actor &other,
                                       const Character & ) const override {
            const auto &c = static_cast<const repair_actor &>( other );
            if( is_hack != c.is_hack ) { return false; }
            if( is_hack ) {
                return hack_type == c.hack_type &&
                       target_pos == c.target_pos &&
                       tool_type == c.tool_type &&
                       crafter_part_index == c.crafter_part_index;
            }
            return iuse_name == c.iuse_name && tool == c.tool && item_pos == c.item_pos;
        }

    public:
        repair_actor() = default;
        repair_actor( hack_type_t htype, const tripoint_abs_ms &tpos,
                      const itype_id &ttool, int cpart_idx = -1 );
        repair_actor( const std::string &name, safe_reference<item> tool_ref, int pos );

        bool is_hack_path() const { return is_hack; }
        hack_type_t get_hack_type() const { return hack_type; }
        const tripoint_abs_ms &get_target_pos() const { return target_pos; }
        const itype_id &get_tool_type() const { return tool_type; }
        int get_crafter_part_index() const { return crafter_part_index; }
        const std::string &get_iuse_name() const { return iuse_name; }
        safe_reference<item> &get_tool() { return tool; }
        int get_item_pos() const { return item_pos; }
        int get_repeat() const { return repeat; }
        safe_reference<item> &get_fix_target() { return fix_target; }

        activity_id get_type() const override { return activity_id( "ACT_REPAIR_ITEM" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

struct train_skill_activity_actor_options {
    std::string training_skill;
    int training_skill_xp = 0;
    int training_skill_xp_chance = 0;
    int training_skill_max_level = 0;
    int training_skill_fatigue = 0;
    int training_skill_interval = 0;
    int moves_total = 0;
    safe_reference<item> tool;
    bool pseudo_tool = false;
    tripoint_abs_ms pseudo_tool_pos = tripoint_abs_ms::zero();
    itype_id pseudo_tool_type;
};

class train_skill_activity_actor : public activity_actor
{
    private:
        std::string training_skill;
        int training_skill_xp = 0;
        int training_skill_xp_chance = 0;
        int training_skill_max_level = 0;
        int training_skill_fatigue = 0;
        int training_skill_interval = 0;
        int moves_total = 0;
        safe_reference<item> tool;
        bool pseudo_tool = false;
        tripoint_abs_ms pseudo_tool_pos = tripoint_abs_ms::zero();
        itype_id pseudo_tool_type;

    public:
        train_skill_activity_actor() = default;
        explicit train_skill_activity_actor( train_skill_activity_actor_options options );

        activity_id get_type() const override { return activity_id( "ACT_TRAIN_SKILL" ); }
        auto start( player_activity &act, Character &who ) -> void override;
        auto do_turn( player_activity &act, Character &who ) -> void override;
        auto finish( player_activity &act, Character &who ) -> void override;
        auto serialize( JsonOut &jsout ) const -> void override;
        static auto deserialize( JsonIn &jsin ) -> std::unique_ptr<activity_actor>;
        static auto legacy_deserialize( const JsonObject &data ) -> std::unique_ptr<activity_actor>;

    private:
        auto get_tool( Character &who ) const -> item *;
        auto apply_training( Character &who, item &training_tool ) const -> bool;
};

class wear_actor : public activity_actor
{
    public:
        struct wear_target {
            safe_reference<item> item_ref;
            int quantity;
            bool operator==( const wear_target &other ) const {
                return item_ref == other.item_ref && quantity == other.quantity;
            }
        };
        std::vector<wear_target> to_wear;

        bool can_resume_with_internal( const activity_actor &other,
                                       const Character & ) const override {
            const auto &c = static_cast<const wear_actor &>( other );
            return to_wear == c.to_wear;
        }

    public:
        wear_actor() = default;
        explicit wear_actor( std::vector<wear_target> targets );

        activity_id get_type() const override {
            return activity_id( "ACT_WEAR" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class wait_stamina_actor : public activity_actor
{
    private:
        int stamina_threshold;
        int stamina_initial = -1;

    public:
        wait_stamina_actor() = default;
        explicit wait_stamina_actor( int threshold );

        activity_id get_type() const override {
            return activity_id( "ACT_WAIT_STAMINA" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class hand_crank_charge_actor : public activity_actor
{
    private:
        int charge_interval_turns;
        int charge_amount;
        int fatigue_amount;
        itype_id ammo_type;

    public:
        hand_crank_charge_actor() = default;
        explicit hand_crank_charge_actor(
            int interval_turns,
            int charges,
            int fatigue,
            const itype_id &ammo
        );

        activity_id get_type() const override {
            return activity_id( "ACT_HAND_CRANK" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class wait_npc_actor : public activity_actor
{
    private:
        std::string npc_name;

    public:
        wait_npc_actor() = default;
        explicit wait_npc_actor( const std::string &name );

        activity_id get_type() const override {
            return activity_id( "ACT_WAIT_NPC" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class clear_rubble_actor : public activity_actor
{
    private:
        tripoint_abs_ms rubble_pos;

        bool can_resume_with_internal( const activity_actor &other,
                                       const Character & ) const override {
            const auto &c = static_cast<const clear_rubble_actor &>( other );
            return rubble_pos == c.rubble_pos;
        }

    public:
        clear_rubble_actor() = default;
        explicit clear_rubble_actor( const tripoint_abs_ms &pos );

        activity_id get_type() const override {
            return activity_id( "ACT_CLEAR_RUBBLE" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class read_activity_actor : public activity_actor
{
    public:
        struct npc_learner {
            character_id id;
            float penalty = 1.0f;       // 1.0 = no penalty (fun-learners or self)
            bool operator==( const npc_learner &other ) const {
                return id == other.id && penalty == other.penalty;
            }
        };

        safe_reference<item> book;
        std::vector<npc_learner> learners;
        bool is_martial_arts = false;
        int stamina_at_start = 0;
        int total_moves = 0;
        int continuous_reader_id = 0;

        bool can_resume_with_internal( const activity_actor &other,
                                       const Character & ) const override {
            const auto &c = static_cast<const read_activity_actor &>( other );
            return book == c.book &&
                   is_martial_arts == c.is_martial_arts &&
                   learners == c.learners;
        }

    public:
        read_activity_actor() = default;
        explicit read_activity_actor(
            safe_reference<item> book_ref,
            std::vector<npc_learner> npcs = {},
            bool martial_arts = false,
            int total_moves = 0
        );

        activity_id get_type() const override {
            return activity_id( "ACT_READ" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        auto get_progress_message( const player_activity &act,
                                   const Character &who ) const -> act_progress_message override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class move_loot_activity_actor : public activity_actor
{
    private:
        int items_processed = 0;
        int stage = 0;              // 0=INIT, 1=THINK, 2=DO
        tripoint_abs_ms current_src;
        std::unordered_set<tripoint_abs_ms> zone_points;

    public:
        move_loot_activity_actor() = default;
        explicit move_loot_activity_actor(
            int processed,
            int init_stage,
            const std::unordered_set<tripoint_abs_ms> &zpoints
        );

        activity_id get_type() const override {
            return activity_id( "ACT_MOVE_LOOT" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class fetch_required_actor : public activity_actor
{
    private:
        do_activity_reason reason = static_cast<do_activity_reason>( 0 );
        requirement_data fetch_requirements;
        tripoint_abs_ms placement_pos;
        tripoint_abs_ms source_zone_pos;

    public:
        fetch_required_actor() = default;
        explicit fetch_required_actor(
            do_activity_reason reason,
            const requirement_data &reqs,
            const tripoint_abs_ms &placement,
            const tripoint_abs_ms &source_zone
        );

        activity_id get_type() const override {
            return activity_id( "ACT_FETCH_REQUIRED" );
        }

        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;

        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class tree_communion_actor : public activity_actor
{
    private:
        int startup_turns = 0;
    public:
        tree_communion_actor() = default;
        explicit tree_communion_actor( int turns );
        activity_id get_type() const override { return activity_id( "ACT_TREE_COMMUNION" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class shear_actor : public activity_actor
{
    private:
        tripoint_abs_ms target_pos;
        std::string tied_state;
        safe_reference<item> shears;
    public:
        shear_actor() = default;
        explicit shear_actor( const tripoint_abs_ms &pos,
                              const std::string &tied = "",
                              safe_reference<item> shears_ref = safe_reference<item>() );
        activity_id get_type() const override { return activity_id( "ACT_SHEAR" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class milk_actor : public activity_actor
{
    private:
        tripoint_abs_ms target_pos;
        std::string tied_state;
    public:
        milk_actor() = default;
        explicit milk_actor( const tripoint_abs_ms &pos,
                             const std::string &tied = "" );
        activity_id get_type() const override { return activity_id( "ACT_MILK" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class pulp_actor : public activity_actor
{
    private:
        tripoint_abs_ms target_pos;
        bool auto_pulp_no_acid = false;
        int num_corpses = 0;
    public:
        pulp_actor() = default;
        explicit pulp_actor( const tripoint_abs_ms &pos, bool auto_no_acid = false,
                             int num_corpses = 0 );
        activity_id get_type() const override { return activity_id( "ACT_PULP" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class hotwire_car_actor : public activity_actor
{
    private:
        tripoint_abs_ms veh_pos;
        int mechanics_skill = 0;
        int moves_total = 0;
    public:
        hotwire_car_actor() = default;
        hotwire_car_actor( const tripoint_abs_ms &pos, int skill, int moves );
        activity_id get_type() const override { return activity_id( "ACT_HOTWIRE_CAR" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class start_engines_actor : public activity_actor
{
    private:
        int take_control = 0;
        tripoint_abs_ms placement;
        int moves_total = 0;
    public:
        start_engines_actor() = default;
        start_engines_actor( int control, const tripoint_abs_ms &pos, int moves );
        activity_id get_type() const override { return activity_id( "ACT_START_ENGINES" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class start_fire_actor : public activity_actor
{
    private:
        int light_level = 0;
        tripoint_abs_ms placement;
        int practice_difficulty = 0;
    public:
        start_fire_actor() = default;
        explicit start_fire_actor( int light, const tripoint_abs_ms &pos,
                                   int difficulty = 0 );
        activity_id get_type() const override { return activity_id( "ACT_START_FIRE" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class make_zlave_actor : public activity_actor
{
    private:
        int success_chance = 0;
        std::string corpse_name;
        safe_reference<item> corpse;
    public:
        make_zlave_actor() = default;
        explicit make_zlave_actor( int success, const std::string &name,
                                   safe_reference<item> corpse_ref = safe_reference<item>() );
        activity_id get_type() const override { return activity_id( "ACT_MAKE_ZLAVE" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class study_spell_actor : public activity_actor
{
    private:
        std::string spell_type;
        std::string study_mode;       // "study" or "learn"
        std::string gain_level_flag;  // "gain_level" or ""
        int total_xp = 0;
        int total_levels = 0;
        int dark = 0;                 // -1 = too dark, 0 = normal
        int tick_counter = 0;
        int xp_snapshot = 0;
    public:
        study_spell_actor() = default;
        explicit study_spell_actor( const std::string &type,
                                    const std::string &mode = "learn",
                                    const std::string &gain = "" );
        activity_id get_type() const override { return activity_id( "ACT_STUDY_SPELL" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class firstaid_actor : public activity_actor
{
    private:
        std::string heal_type;
        safe_reference<item> target_item;
    public:
        firstaid_actor() = default;
        explicit firstaid_actor( const std::string &type,
                                 safe_reference<item> target = safe_reference<item>() );
        const std::string &get_heal_type() const { return heal_type; }
        activity_id get_type() const override { return activity_id( "ACT_FIRSTAID" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class play_with_pet_actor : public activity_actor
{
    private:
        weak_ptr_fast<monster> pet;
        std::string pet_name;
    public:
        play_with_pet_actor() = default;
        explicit play_with_pet_actor( weak_ptr_fast<monster> pet_ref, const std::string &name );
        activity_id get_type() const override { return activity_id( "ACT_PLAY_WITH_PET" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class train_pet_actor : public activity_actor
{
    private:
        weak_ptr_fast<monster> pet;
        std::string pet_name;
    public:
        train_pet_actor() = default;
        explicit train_pet_actor( weak_ptr_fast<monster> pet_ref, const std::string &name );
        activity_id get_type() const override { return activity_id( "ACT_TRAIN_PET" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class socialize_actor : public activity_actor
{
    private:
        std::string npc_name;
    public:
        socialize_actor() = default;
        explicit socialize_actor( const std::string &name );
        activity_id get_type() const override { return activity_id( "ACT_SOCIALIZE" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class train_actor : public activity_actor
{
    private:
        std::string skill_name;
        int expert_multiplier = 0;
        int trainer_id = -1;
    public:
        train_actor() = default;
        explicit train_actor( const std::string &name, int expert = 0, int trainer = -1 );
        const std::string &get_name() const { return skill_name; }
        int get_expert_multiplier() const { return expert_multiplier; }
        int get_trainer_id() const { return trainer_id; }
        activity_id get_type() const override { return activity_id( "ACT_TRAIN" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class operation_actor : public activity_actor
{
    private:
        std::string op_type;        // "install" or "uninstall"
        std::string bionic_id;
        std::string installer_name;
        bool autodoc = false;
        int difficulty = 0;
        int success = 0;
        int capacity = 0;
        int pl_skill = 0;
        int operation_attempted = 0; // tracks whether install/uninstall has been performed
    public:
        operation_actor() = default;
        explicit operation_actor( const std::string &type, const std::string &bid,
                                  const std::string &installer, bool adoc, int diff, int succ, int cap, int skill );
        const std::string &get_op_type() const { return op_type; }
        const std::string &get_bionic_id() const { return bionic_id; }
        bool is_autodoc() const { return autodoc; }
        int get_success() const { return success; }
        activity_id get_type() const override { return activity_id( "ACT_OPERATION" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class butcher_actor : public activity_actor
{
    private:
        activity_id act_type;
        safe_reference<item> corpse;
        std::vector<safe_reference<item>> extra_corpses;
    public:
        bool ready_for_next = true;   // false = currently processing corpse, true = ready for next target

        butcher_actor() = default;
        explicit butcher_actor( const activity_id &type, safe_reference<item> corpse_ref );
        void add_extra_corpse( safe_reference<item> corpse_ref ) {
            extra_corpses.push_back( std::move( corpse_ref ) );
        }
        const activity_id &get_act_type() const { return act_type; }
        safe_reference<item> &get_corpse() { return corpse; }
        activity_id get_type() const override { return act_type; }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};

class gunmod_add_actor : public activity_actor
{
    private:
        int roll = 0;           // chance of success (%)
        int risk = 0;           // chance of damage (%)
        int qty = 0;            // tool charges used
        std::string tool_name;  // tool used
        safe_reference<item> gun;
        safe_reference<item> mod;
    public:
        gunmod_add_actor() = default;
        explicit gunmod_add_actor( int roll, int risk, int qty,
                                   const std::string &tool, safe_reference<item> gun_ref, safe_reference<item> mod_ref );
        int get_roll() const { return roll; }
        int get_risk() const { return risk; }
        int get_qty() const { return qty; }
        const std::string &get_tool_name() const { return tool_name; }
        safe_reference<item> &get_gun() { return gun; }
        safe_reference<item> &get_mod() { return mod; }
        activity_id get_type() const override { return activity_id( "ACT_GUNMOD_ADD" ); }
        void start( player_activity &act, Character &who ) override;
        void do_turn( player_activity &act, Character &who ) override;
        void finish( player_activity &act, Character &who ) override;
        void serialize( JsonOut &jsout ) const override;
        static std::unique_ptr<activity_actor> deserialize( JsonIn &jsin );
        static std::unique_ptr<activity_actor> legacy_deserialize( const JsonObject &data );
};
