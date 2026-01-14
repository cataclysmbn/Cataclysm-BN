#include "explosion.h" // IWYU pragma: associated
#include "fragment_cloud.h" // IWYU pragma: associated

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <utility>
#include <variant>
#include <vector>

#include "animation.h"
#include "avatar.h"
#include "ballistics.h"
#include "catalua_hooks.h"
#include "catalua_sol.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_utility.h"
#include "cata_algo.h"
#include "color.h"
#include "creature.h"
#include "damage.h"
#include "debug.h"
#include "enums.h"
#include "explosion_queue.h"
#include "field_type.h"
#include "flat_set.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "int_id.h"
#include "item.h"
#include "item_factory.h"
#include "itype.h"
#include "json.h"
#include "line.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "material.h"
#include "math_defines.h"
#include "messages.h"
#include "mongroup.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "point.h"
#include "posix_time.h"
#include "projectile.h"
#include "rng.h"
#include "shadowcasting.h"
#include "sounds.h"
#include "string_formatter.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "units.h"
#include "ui_manager.h"
#include "units_mass.h"
#include "units_volume.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

static const ammo_effect_str_id ammo_effect_NULL_SOURCE( "NULL_SOURCE" );

static const efftype_id effect_blind( "blind" );
static const efftype_id effect_deaf( "deaf" );
static const efftype_id effect_emp( "emp" );
static const efftype_id effect_stunned( "stunned" );
static const efftype_id effect_teleglow( "teleglow" );

static const species_id ROBOT( "ROBOT" );

static const trait_id trait_LEG_TENT_BRACE( "LEG_TENT_BRACE" );
static const trait_id trait_PER_SLIME( "PER_SLIME" );
static const trait_id trait_PER_SLIME_OK( "PER_SLIME_OK" );

static const mongroup_id GROUP_NETHER( "GROUP_NETHER" );

static const bionic_id bio_ears( "bio_ears" );
static const bionic_id bio_sunglasses( "bio_sunglasses" );

static const itype_id itype_battery( "battery" );
static const itype_id itype_e_handcuffs( "e_handcuffs" );
static const itype_id itype_rm13_armor_on( "rm13_armor_on" );

static float obstacle_blast_percentage( float range, float distance )
{
    return distance > range ? 0.0f : distance > range / 2 ? 0.5f : 1.0f;
}
static float critter_blast_percentage( Creature *c, float range, float distance )
{
    const float radius_reduction = distance > range ? 0.0f : distance > range / 2 ? 0.5f : 1.0f;

    switch( c->get_size() ) {
        case( creature_size::tiny ):
            return 0.5 * radius_reduction;
        case( creature_size::small ):
            return 0.8 * radius_reduction;
        case( creature_size::medium ):
            return 1.0 * radius_reduction;
        case( creature_size::large ):
            return 1.5 * radius_reduction;
        case( creature_size::huge ):
            return 2.0 * radius_reduction;
        default:
            return 1.0 * radius_reduction;
    }
}

static float item_blast_percentage( float range, float distance )
{
    const float radius_reduction = 1.0f - distance / range;
    return radius_reduction;
}

explosion_data load_explosion_data( const JsonObject &jo )
{
    explosion_data ret;
    // First new explosions
    if( jo.has_int( "damage" ) || jo.has_object( "fragment" ) ) {
        jo.read( "damage", ret.damage );
        jo.read( "radius", ret.radius );
        jo.read( "fragment", ret.fragment );
    } else {
        // Legacy
        float power = jo.get_float( "power" );
        ret.damage = power * explosion_handler::power_to_dmg_mult;
        // Don't reuse old formula, it gave way too big blasts
        float distance_factor = jo.get_float( "distance_factor", 0.8f );
        ret.radius = explosion_handler::blast_radius_from_legacy( power, distance_factor );
        if( jo.has_int( "shrapnel" ) ) {
            ret.fragment = explosion_handler::shrapnel_from_legacy( power, ret.radius );
            // Outdated, unused
            jo.get_int( "shrapnel" );
        } else if( jo.has_object( "shrapnel" ) ) {
            ret.fragment = explosion_handler::shrapnel_from_legacy( power, ret.radius );

            auto shr = jo.get_object( "shrapnel" );
            // Legacy bad design - we don't migrate those
            shr.get_int( "casing_mass" );
            shr.get_float( "fragment_mass", 0.15 );
            shr.get_int( "recovery", 0 );
            shr.get_string( "drop", "" );
        }
    }

    ret.fire = jo.get_bool( "fire", false );

    return ret;
}


// Programmer-friendly numbers to tweak
namespace ExplosionConstants
{
// Assumed distance between z-levels.
// Affects depths of craters and such.
constexpr int Z_LEVEL_DIST = 4;

// Shrapnel hitting terrain and vehicle parts should not inflict full damage
// This constant specifies by how much the damage of shrapnel is multiplied
//   on terrain/vehicle hits.
constexpr float SHRAPNEL_OBSTACLE_REDUCTION = 0.25;

// To destroy terrain consistently, we bash it several times
//   at linearly dissipating strength.
// This determines how many times.
constexpr int MULTIBASH_COUNT = 5;

// Even though bash the terrain several times, we want to limit
//   the total amount of damage inflicted
// For terrain and furniture this doesn't matter, but it does
//   for vehicles
// This coeff specifies total amount of damage inflicted after every bash
constexpr float VEHICLE_DAMAGE_MULT = 2.0;

// We use this upper bound for the linear interpolation of terfurn strength
// The intent is to make explosions consistent, but have jagged edges
constexpr float BASH_RANDOM_FACTOR = 0.3;

// Flinging entities
// Whenever an entity is struck by the blast wave
//   it is given velocity
// Velocity is the range the object will fly

// Refer to MOB_FLING_FACTOR & ITEM_FLING_FACTOR
constexpr int EXPLOSION_CALIBRATION_POWER = 100;

// This is the amount of grams a mob staying at the
//   blast center has to weight
//   in order to fly exactly one blast radius away when struck by
//   an explosion of strength EXPLOSION_CALIBRATION_STRENGTH
constexpr int MOB_FLING_FACTOR = 40750;

// This is the amount of grams an item staying at the
//   blast center has to weight
//   in order to fly exactly one blast radius away when struck by
//   an explosion of strength EXPLOSION_CALIBRATION_STRENGTH
constexpr int ITEM_FLING_FACTOR = 1500;

// To make items propagate a bit more interestingly, we add a random amount to effective distance
//   as rng_float(-ITEM_FLING_RANDOM_FACTOR, +ITEM_FLING_RANDOM_FACTOR);
constexpr float ITEM_FLING_RANDOM_FACTOR = 2.5;

// If the entity strikes an obstacle, we multiply its velocity by this factor
//   and reflect it back
constexpr float RESTITUTION_COEFF = 0.3;

// If a flung entity would get more than this speed,
//   it instead will choose a random value between
//   FLING_THRESHOLD and FLING_MAX_RANGE
constexpr float FLING_THRESHOLD = 30.0;

// See above
constexpr float FLING_MAX_RANGE = 50.0;

// Normally, items and mobs flung would fly at such high speeds that
//   it becomes a problem to show it
// This constant slows down the propagation of items without affecting
//   the final distance they fly
// Put it another way, entity movement and regular explosion have two
//   different time scale and this constant is the scaling factor
//   between the two
constexpr float FLING_SLOWDOWN = 5.0;
} // namespace ExplosionConstants

namespace explosion_handler
{
class ExplosionEvent
{
    public:
        struct PropelledEntity {
            std::variant<Creature *, safe_reference<item>> target;

            rl_vec2d position;
            float angle;
            float velocity; //< more of a number of tiles it can fly

            float cur_relative_time;
        };
        struct FieldToAdd {
            field_type_id field;
            int intensity;
            bool hit_player;
        };
        using target_types =
            std::variant<std::monostate, PropelledEntity, FieldToAdd, field_type_id, int>;

        enum class Kind { ITEM_MOVEMENT, MOB_MOVEMENT, BLAST, SHRAPNEL, FIELD_ADDITION, FIELD_REMOVAL } kind;
        target_types target;
        tripoint position;

        static ExplosionEvent mob_movement( const tripoint position, Creature *mob, float angle,
                                            float velocity, float cur_relative_time ) {
            return ExplosionEvent( Kind::MOB_MOVEMENT, position,
            PropelledEntity{
                mob,
                rl_vec2d( position.x + 0.5, position.y + 0.5 ),
                angle,
                velocity,
                cur_relative_time
            } );
        }
        static ExplosionEvent item_movement( const tripoint position, safe_reference<item> item,
                                             float angle, float velocity, float cur_relative_time ) {
            return ExplosionEvent( Kind::ITEM_MOVEMENT, position,
            PropelledEntity{
                item,
                rl_vec2d( position.x + 0.5, position.y + 0.5 ),
                angle,
                velocity,
                cur_relative_time
            } );
        }
        static ExplosionEvent tile_blast( const tripoint position, const int distance ) {
            return ExplosionEvent( Kind::BLAST, position, distance );
        }
        static ExplosionEvent tile_shrapnel( const tripoint position ) {
            return ExplosionEvent( Kind::SHRAPNEL, position );
        }
        static ExplosionEvent field_addition(
            const tripoint position, field_type_id target,
            const int intensity = INT_MAX, const bool hit_player = false
        ) {
            return ExplosionEvent( Kind::FIELD_ADDITION, position, FieldToAdd{target, intensity, hit_player} );
        }
        static ExplosionEvent field_removal( const tripoint position, field_type_id target ) {
            return ExplosionEvent( Kind::FIELD_REMOVAL, position, target );
        }

    private:
        ExplosionEvent( Kind kind, const tripoint position ) :
            kind( kind ), position( position ) {};
        ExplosionEvent( Kind kind, const tripoint position, target_types target ) :
            kind( kind ), target( std::move( target ) ), position( position ) {};
};

class ExplosionProcess
{
    public:
        // Where did the explosion originate from?
        const tripoint center;

        // Explosion damage.
        const int blast_power;

        // Explosion radius, 0 to disable
        const int blast_radius;

        // Is the fire created by the explosion actually left behind?
        const bool is_fiery;

        // Shrapnel data, nullopt to disable
        const std::optional<projectile> shrapnel;

        // Who do we attribute the explosion & shrapnel to? nullopt to disable
        const std::optional<Creature *> emitter;
    private:
        using dist_point_pair = std::pair<float, tripoint>;
        using time_event_pair = std::pair<float, ExplosionEvent>;

        std::vector<dist_point_pair> blast_map;
        std::vector<dist_point_pair> shrapnel_map;
        std::priority_queue<time_event_pair, std::vector<time_event_pair>, pair_greater_cmp_first>
        event_queue;

        std::optional<player *> player_flung;
        std::map<const Creature *, int> mobs_blasted;
        std::map<const Creature *, int> mobs_shrapneled;
        std::set<const Creature *> flung_set;
        std::vector<tripoint> recombination_targets;

        float cur_relative_time;
        long long last_update_ms;
        bool request_redraw;
    public:
        void run();

        std::map<const Creature *, int> get_blasted() {
            return mobs_blasted;
        };
        std::map<const Creature *, int> get_shrapneled() {
            return mobs_shrapneled;
        };

        ExplosionProcess(
            const tripoint blast_center,
            const int blast_power,
            const int blast_radius,
            const std::optional<projectile> &proj = std::nullopt,
            const bool is_fiery = false,
            const std::optional<Creature *> responsible = std::nullopt
        ) : center( blast_center ),
            blast_power( blast_power ),
            blast_radius( blast_radius ),
            is_fiery( is_fiery ),
            shrapnel( proj ),
            emitter( responsible ),
            player_flung( std::nullopt ),
            cur_relative_time( 0.0 ),
            last_update_ms(
                std::chrono::time_point_cast<std::chrono::milliseconds>
                ( std::chrono::system_clock::now() ).time_since_epoch().count()
            ),
            request_redraw( false ) {}
    private:
        static bool dist_comparator( dist_point_pair a, dist_point_pair b ) {
            return a.first < b.first;
        };
        static bool time_comparator( const time_event_pair &a, const time_event_pair &b ) {
            return a.first < b.first;
        };

        void update_timings() {
            if( !is_animated() ) {
                // Arbitrary large number since for null delays
                //   we just want to scroll thru events as fast as possible
                cur_relative_time += 1e6;
                return;
            }
            const int animation_delay = get_option<int>( "ANIMATION_DELAY" );
            const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>
                             ( std::chrono::system_clock::now() ).time_since_epoch().count();
            const long long ms_diff = now - last_update_ms;
            // Multiplied by 10x to calibrate it such that at 10 animation delay, an explosion
            //   of radius 10 will take exactly 1 second to fully propagate
            const float rel_diff = static_cast<float>( ms_diff ) / ( 10.0 * animation_delay );
            cur_relative_time += rel_diff;
            last_update_ms = now;
        }

        void fill_maps();
        void init_event_queue();
        inline float generate_fling_angle( const tripoint from, const tripoint to );
        inline bool is_occluded( const tripoint from, const tripoint to );
        void add_event( const float delay, const ExplosionEvent &event ) {
            assert( delay >= 0 );
            event_queue.emplace( cur_relative_time + delay + std::numeric_limits<float>::epsilon(), event );
        }
        auto is_animated() const -> bool {
            if( test_mode || get_option<int>( "ANIMATION_DELAY" ) <= 0 ) { return false; }

            const int skip_after = get_option<int>( "SKIP_EXPLOSION_ANIMATION_AFTER" );
            return skip_after == 0 || get_explosion_queue().get_count() <= skip_after;
        }

        bool process_next();
        void blast_tile( const tripoint position, const int rl_distance );
        void project_shrapnel( const tripoint position );
        void add_field( const tripoint position, const field_type_id field,
                        const int intensity, const bool hit_player );
        void remove_field( const tripoint position, const field_type_id target );
        void move_entity( const tripoint position, const ExplosionEvent::PropelledEntity &datum,
                          bool is_mob );

        // How long should it take for an entity to travel 1 unit of distance at `velocity`?
        float one_tile_at_vel( float velocity ) {
            assert( velocity > 0 );
            return ExplosionConstants::FLING_SLOWDOWN / velocity;
        };
};

void ExplosionProcess::fill_maps()
{
    map &here = get_map();

    const int shrapnel_range = shrapnel.has_value() ? shrapnel.value().range : 0;
    const int aoe_radius = std::max( blast_radius, shrapnel_range );
    const int z_levels_affected = aoe_radius / ExplosionConstants::Z_LEVEL_DIST;
    const tripoint_range<tripoint> affected_block(
        center + tripoint( -aoe_radius, -aoe_radius, -z_levels_affected ),
        center + tripoint( aoe_radius, aoe_radius, z_levels_affected )
    );

    for( const tripoint &target : affected_block ) {
        if( !here.inbounds( target ) ) {
            continue;
        }

        // Uses this ternany check instead of rl_dist because it converts trig_dist's distance to int implicitly
        const float distance = (
                                   trigdist ?
                                   trig_dist( center, target ) :
                                   square_dist( center, target )
                               );
        const float z_distance = abs( target.z - center.z );
        const float z_aware_distance = distance + ( ExplosionConstants::Z_LEVEL_DIST - 1 ) * z_distance;
        // We static_cast<int> in order to keep parity with legacy blasts using rl_dist for distance
        //   which, as stated above, converts trig_dist into int implicitly
        if( blast_radius > 0 && static_cast<int>( z_aware_distance ) <= blast_radius ) {
            blast_map.emplace_back( z_aware_distance, target );
        }

        if( shrapnel && static_cast<int>( distance ) <= shrapnel_range && target.z == center.z &&
            !is_occluded( center, target ) ) {
            shrapnel_map.emplace_back( distance, target );
        }
    }

    std::stable_sort( blast_map.begin(), blast_map.end(), dist_comparator );
    std::stable_sort( shrapnel_map.begin(), shrapnel_map.end(), dist_comparator );
}
void ExplosionProcess::init_event_queue()
{
    // Start with shrapnel first
    // In how many blast steps should the animation for shrapnel complete?
    const float SHRAPNEL_EQUIV = 3.0;
    const float shrapnel_delay = shrapnel.has_value() ? SHRAPNEL_EQUIV : 0.0;

    for( const auto &[distance, position] : shrapnel_map ) {
        const float random_factor = rng_float( -0.2, 0.2 );
        const float range_percent = distance / shrapnel.value().range;
        const float timing = std::min( std::max( range_percent + random_factor, 0.0f ), 1.0f );
        const float time_taken = SHRAPNEL_EQUIV * timing;

        add_event( time_taken, ExplosionEvent::tile_shrapnel( position ) );
    }

    // Then apply blasting
    for( const auto &[distance, position] : blast_map ) {
        const float time_taken = shrapnel_delay + distance;
        // We static_cast<int> in order to keep parity with legacy blasts using rl_dist for distance
        //   which, as stated before, converts trig_dist into int implicitly
        add_event( time_taken, ExplosionEvent::tile_blast( position, static_cast<int>( distance ) ) );
    }
}
inline bool ExplosionProcess::is_occluded( const tripoint from, const tripoint to )
{
    if( from == to ) {
        return false;
    }

    map &here = get_map();
    tripoint last_position = from;

    std::vector<tripoint> line_of_movement = line_to( from, to );
    // Annoyingly, line_to does not include the origin point
    //   so it has to be added manually
    line_of_movement.insert( line_of_movement.begin(), from );
    for( const auto &position : line_of_movement ) {
        // position != to necessary because we do want to strike the
        //   target obstacle
        if( position != to && here.impassable( position ) ) {
            return true;
        }
        // position != to is unneeded here though because we want to
        //   make sure stuff does not fly into vehicles
        if( here.obstructed_by_vehicle_rotation( last_position, position ) ) {
            return true;
        }
        last_position = position;
    }
    return false;
}

inline float ExplosionProcess::generate_fling_angle( const tripoint from, const tripoint to )
{
    if( from != to ) {
        // -+ 0.95 added to add a half-arc
        // It should be noted that this mathematically has a bias towards diagonal directions
        //   but this is the shortest way to get good enough results
        return units::atan2(
                   to.y - from.y + rng_float( -0.95, 0.95 ),
                   to.x - from.x + rng_float( -0.95, 0.95 )
               ).value();
    } else {
        return rng_float( -M_PI, M_PI );
    }
}

bool ExplosionProcess::process_next()
{
    if( event_queue.empty() ) {
        return false;
    }

    // We don't need to wait in testing mode or if there is no animation delay
    if( is_animated() ) {
        const float next_event_time = event_queue.top().first;
        const double relative_time_step = static_cast<double>( next_event_time - cur_relative_time );
        const double animation_delay = static_cast<double>( get_option<int>( "ANIMATION_DELAY" ) );

        // We balance the timing in such a way
        //   that, at 10 ANIMATION_DELAY, it will take an explosion of radius 10
        //   exactly 1 second to propagate fully
        // NOLINTNEXTLINE(cata-no-long)
        const long int delay_ms = static_cast<long int>( relative_time_step * 10.0 * animation_delay );
        if( delay_ms > 0 ) {
            const timespec delay = timespec {0, delay_ms * 1000000L};
            nanosleep( &delay, nullptr );
        }
    }
    update_timings();

    while( !event_queue.empty() && cur_relative_time >= event_queue.top().first ) {
        const auto &event = event_queue.top().second;

        switch( event.kind ) {
            case ExplosionEvent::Kind::SHRAPNEL:
                project_shrapnel( event.position );
                break;
            case ExplosionEvent::Kind::BLAST:
                blast_tile( event.position, std::get<int>( event.target ) );
                break;
            case ExplosionEvent::Kind::FIELD_ADDITION: {
                const auto &[field, intensity,
                                    hit_player] = std::get<ExplosionEvent::FieldToAdd>( event.target );
                add_field( event.position, field, intensity, hit_player );
                break;
            }
            case ExplosionEvent::Kind::FIELD_REMOVAL:
                remove_field( event.position, std::get<field_type_id>( event.target ) );
                break;
            case ExplosionEvent::Kind::ITEM_MOVEMENT:
            case ExplosionEvent::Kind::MOB_MOVEMENT:
                move_entity(
                    event.position,
                    std::get<ExplosionEvent::PropelledEntity>( event.target ),
                    event.kind == ExplosionEvent::Kind::MOB_MOVEMENT
                );
                break;
        };
        event_queue.pop();
    }

    return true;
}

void ExplosionProcess::project_shrapnel( const tripoint position )
{
    map &here = get_map();

    assert( shrapnel );

    if( is_occluded( center, position ) ) {
        return;
    }

    projectile fragment = shrapnel.value();
    fragment.add_effect( ammo_effect_NULL_SOURCE );

    auto critter = g->critter_at( position );
    if( critter && !critter->is_dead_state() ) {
        int damage_taken = 0;
        const auto bps = critter->get_all_body_parts( true );
        // Humans get hit in all body parts
        if( critter->is_player() ) {
            for( bodypart_id bp : bps ) {
                // TODO: Apply projectile effects
                // TODO: Penalize low coverage armor
                // Halve damage to be closer to what monsters take
                damage_instance half_impact = fragment.impact;
                half_impact.mult_damage( 0.5f );
                dealt_damage_instance dealt = critter->deal_damage( emitter.value_or( nullptr ), bp,
                                              fragment.impact );
                if( dealt.total_damage() > 0 ) {
                    damage_taken += dealt.total_damage();
                }
            }
        } else {
            dealt_damage_instance dealt = critter->deal_damage( emitter.value_or( nullptr ), bps[0],
                                          fragment.impact );
            if( dealt.total_damage() > 0 ) {
                damage_taken += dealt.total_damage();
            }
            critter->check_dead_state();
        }
        mobs_shrapneled[critter] = damage_taken;
    }

    if( here.impassable( position ) ) {
        const int damage = fragment.impact.total_damage() * ExplosionConstants::SHRAPNEL_OBSTACLE_REDUCTION;
        if( optional_vpart_position vp = here.veh_at( position ) ) {
            vp->vehicle().damage( vp->part_index(), damage );
        } else {
            // Terrain should be affected by shrapnel less
            here.bash( position, damage, true );
        }
    }

    if( is_animated() ) {
        std::vector<tripoint> buf = line_to( position, center );
        buf.resize( 2 );
        g->draw_line( position, buf );
    }
    request_redraw |= true;
}

void ExplosionProcess::blast_tile( const tripoint position, const int rl_distance )
{
    assert( blast_radius > 0 );
    // Verify we have view of the center
    if( is_occluded( center, position ) ) {
        return;
    }

    map &here = get_map();

    if( blast_power ) {
        // Item damage comes first in order to prevent dropped loot from being destroyed immediately.
        {
            const std::string cause = _( "force of the explosion" );
            const int smash_force = blast_power * item_blast_percentage( blast_radius, rl_distance );
            here.smash_trap( position, smash_force, string_format( _( "The %1$s" ), cause ) );
            here.smash_items( position, smash_force, cause, true );
            // Don't forget to mark them as explosion smashed already
            for( auto &item : here.i_at( position ) ) {
                item->set_flag( flag_EXPLOSION_SMASHED );
            }
        }

        // Damage creatures. Done first to reduce the amount of flung enemies.
        {
            Creature *critter = g->critter_at( position );

            if( critter != nullptr && !mobs_blasted.contains( critter ) ) {
                const int blast_damage = blast_power * critter_blast_percentage( critter, blast_radius,
                                         rl_distance );
                const auto shockwave_dmg = damage_instance::physical( blast_damage, 0, 0, 0.4f );

                if( critter->is_player() ) {
                    player *player_ptr = critter->as_player();
                    player_ptr->add_msg_if_player( m_bad, _( "You're caught in the explosion!" ) );

                    struct blastable_part {
                        bodypart_id bp;
                        float low_mul;
                        float high_mul;
                        float armor_mul;
                    };

                    static const std::array<blastable_part, 6> blast_parts = { {
                            { bodypart_id( "torso" ), 0.5f, 1.0f, 0.5f },
                            { bodypart_id( "head" ),  0.5f, 1.0f, 0.5f },
                            { bodypart_id( "leg_l" ), 0.75f, 1.25f, 0.4f },
                            { bodypart_id( "leg_r" ), 0.75f, 1.25f, 0.4f },
                            { bodypart_id( "arm_l" ), 0.75f, 1.25f, 0.4f },
                            { bodypart_id( "arm_r" ), 0.75f, 1.25f, 0.4f },
                        }
                    };

                    for( const auto &blast_part : blast_parts ) {
                        const int part_dam = rng( blast_damage * blast_part.low_mul, blast_damage * blast_part.high_mul );
                        const auto dmg_instance = damage_instance( DT_BASH, part_dam, 0, blast_part.armor_mul );
                        const auto result = player_ptr->deal_damage( emitter.value_or( nullptr ), blast_part.bp,
                                            dmg_instance );
                        const int res_dmg = result.total_damage();

                        if( res_dmg > 0 ) {
                            mobs_blasted[critter] = res_dmg;
                        }
                    }
                } else {
                    critter->deal_damage( emitter.value_or( nullptr ), bodypart_id( "torso" ), shockwave_dmg );
                    critter->check_dead_state();
                    mobs_blasted[critter] = blast_damage;
                }
            }
        }

        // Fling creatures
        {
            Creature *critter = g->critter_at( position );

            if( critter != nullptr && !flung_set.contains( critter ) ) {
                const int push_strength = ( blast_radius - rl_distance ) * blast_power;
                const float move_power = ExplosionConstants::MOB_FLING_FACTOR * push_strength;

                const int weight = to_gram( critter->get_weight() );
                const int inertia = std::max( weight, 1 ) * ExplosionConstants::EXPLOSION_CALIBRATION_POWER;
                const float real_velocity = move_power / inertia;
                const float velocity = real_velocity > ExplosionConstants::FLING_THRESHOLD ?
                                       rng_float( ExplosionConstants::FLING_THRESHOLD, ExplosionConstants::FLING_MAX_RANGE ) :
                                       real_velocity;

                if( velocity >= 1.0 ) {
                    if( critter->is_player() ) {
                        player *player_ptr = critter->as_player();
                        player_flung = std::make_optional( player_ptr );
                    }

                    add_event( one_tile_at_vel( velocity ),
                               ExplosionEvent::mob_movement(
                                   position, critter,
                                   generate_fling_angle( center, position ), velocity, cur_relative_time
                               )
                             );
                    flung_set.insert( critter );
                }
            }
        }

        {
            // This reduces the randomness factor in terrain bash significantly
            // Which makes explosions have a more well-defined shape.
            const float offset_distance = std::max( rl_distance - 1.0, 0.0 );
            const float terrain_random_factor = rng_float( 0.0, ExplosionConstants::BASH_RANDOM_FACTOR );
            const float terrain_factor = std::min( std::max( offset_distance / blast_radius -
                                                   terrain_random_factor, 0.0f ), 1.0f );
            float terrain_blast_force = blast_power * obstacle_blast_percentage( blast_radius, rl_distance );

            // Multibash is done by bashing the tile with decaying force.
            // The reason for this existing is because a number of tiles undergo multiple bashed states
            // Things like doors and wall -> floor -> ground.

            const float blast_force_decay = ( ExplosionConstants::VEHICLE_DAMAGE_MULT - 1.0 ) *
                                            blast_power / ExplosionConstants::MULTIBASH_COUNT;
            assert( blast_force_decay > 0 );
            while( terrain_blast_force > 0 ) {
                bash_params bash{
                    static_cast<int>( terrain_blast_force ),
                    true,
                    false,
                    here.passable( position + tripoint_below ),
                    terrain_factor,
                    center.z > position.z,
                    true
                };
                // Despite what you might expect, this is NOT the same as smash_items
                here.bash_items( position, bash );
                here.bash_field( position, bash );
                here.bash_vehicle( position, bash );
                here.bash_ter_furn( position, bash );
                terrain_blast_force -= blast_force_decay;
            }
        }

        {
            // Split items here into stacks

            for( auto &it : here.i_clear( position ) ) {
                while( true ) {
                    const int amt = it->count();
                    const int split_cnt = rng( 1, amt - 1 );
                    const bool is_final = amt <= 1;

                    // If the item is already propelled, ignore it
                    if( !is_final && !it->has_flag( flag_EXPLOSION_PROPELLED ) ) {
                        here.add_item( position, it->split( split_cnt ) );
                    } else {
                        here.add_item( position, std::move( it ) );
                        break;
                    }
                }
            }
            recombination_targets.push_back( position );
        }

        // Now give items velocity
        for( auto &it : here.i_at( position ) ) {
            // If the item is already propelled, ignore it
            if( it->has_flag( flag_EXPLOSION_PROPELLED ) ) {
                continue;
            };

            const float random_factor = rng_float( -ExplosionConstants::ITEM_FLING_RANDOM_FACTOR,
                                                   ExplosionConstants::ITEM_FLING_RANDOM_FACTOR );
            const float push_strength = std::max( blast_radius - rl_distance + random_factor,
                                                  0.0f ) * blast_power;
            const float move_power = ExplosionConstants::ITEM_FLING_FACTOR * push_strength;

            const int weight = to_gram( it->weight() );
            const float inertia = std::max( weight, 1 ) * ExplosionConstants::EXPLOSION_CALIBRATION_POWER;
            const float real_velocity = move_power / inertia;
            const float velocity = real_velocity > ExplosionConstants::FLING_THRESHOLD ?
                                   rng_float( ExplosionConstants::FLING_THRESHOLD, ExplosionConstants::FLING_MAX_RANGE ) :
                                   real_velocity;

            if( velocity < 1.0 ) {
                continue;
            }

            it->set_flag( flag_EXPLOSION_PROPELLED );

            add_event(
                one_tile_at_vel( velocity ),
                ExplosionEvent::item_movement(
                    position, safe_reference<item>( it ),
                    generate_fling_angle( center, position ), velocity, cur_relative_time )
            );
        }
    }

    // Finally, add fields if we can
    if( here.passable( position ) ) {
        const float radius_percent = static_cast<float>( rl_distance ) / blast_radius;
        // Create fresh smoke
        // 50% of the radius is guaranteed to be covered in thin smoke afterwards
        if( here.get_field( position, fd_smoke ) == nullptr ) {
            const float radius_offset = 2 * radius_percent - 1;
            add_event( 0, ExplosionEvent::field_addition( position, fd_smoke ) );
            if( radius_offset > 0 ) {
                const float delay = blast_radius * rng_float( 0.0, 1.1 - radius_offset );
                add_event( delay, ExplosionEvent::field_removal( position, fd_smoke ) );
            }
        }

        // Create fresh fire fields
        if( here.get_field( position, fd_fire ) == nullptr ) {
            add_event(
                0.5,
                ExplosionEvent::field_addition( position, fd_fire,
                                                1 + ( blast_power > 10 ) + ( blast_power > 30 ),
                                                is_fiery
                                              )
            );
            if( !is_fiery ) {
                // Remove at an accelerating pace
                const float delay = 1.0 + rng_float( 2.0, 4.0 ) * ( 1.1 - radius_percent );
                add_event( delay, ExplosionEvent::field_removal( position, fd_fire ) );
            }
        }
    }
    request_redraw |= position.z == g->u.posz();
}

void ExplosionProcess::add_field( const tripoint position,
                                  const field_type_id field,
                                  const int intensity,
                                  const bool hit_player )
{
    map &here = get_map();
    here.add_field( position, field, intensity, 0_turns, hit_player );
    request_redraw |= position.z == g->u.posz();
}

void ExplosionProcess::remove_field( const tripoint position, field_type_id target )
{
    map &here = get_map();
    here.remove_field( position, target );
    request_redraw |= position.z == g->u.posz();
}

void ExplosionProcess::move_entity( const tripoint position,
                                    const ExplosionEvent::PropelledEntity &datum,
                                    const bool is_mob )
{
    if( datum.velocity < 1 ) {
        return;
    }

    std::variant<Creature *, safe_reference<item>> cur_target = datum.target;

    if( !is_mob && !std::get<safe_reference<item>>( cur_target ) ) {
        return;
    }

    map &here = get_map();


    const float time_delta = cur_relative_time - datum.cur_relative_time;
    const float adjusted_delta = time_delta / ExplosionConstants::FLING_SLOWDOWN;
    const float distance_to_travel = std::min( datum.velocity * adjusted_delta, datum.velocity );

    tripoint new_position = position;
    float new_velocity = datum.velocity;
    float new_angle = datum.angle;

    // Sometimes items fly more than one tile at once
    //   so we want to make sure we do not hit any obstacles on the way
    // Hence this complication
    {
        const int intermediate_steps = ceil( 1.5 * distance_to_travel );

        for( int step = 0; step <= intermediate_steps; step++ ) {
            const float progress = static_cast<float>( step ) / static_cast<float>( intermediate_steps );
            const float cur_distance_travelled = distance_to_travel * progress;
            rl_vec2d new_position_vec = datum.position +
                                        rl_vec2d( cur_distance_travelled, 0.0 ).rotated( datum.angle );
            tripoint maybe_new_position = tripoint( static_cast<int>( new_position_vec.x ),
                                                    static_cast<int>( new_position_vec.y ),
                                                    position.z );
            if( !here.inbounds( maybe_new_position ) ||
                here.impassable( maybe_new_position ) ||
                ( is_mob && maybe_new_position != position && g->critter_at( maybe_new_position ) ) ||
                here.obstructed_by_vehicle_rotation( position, maybe_new_position ) ) {
                // TODO: Bash the obstacle with whatever is flung?

                // Just a 180 degree flip
                // Too expensive to compute the proper reflection angle
                new_angle += M_PI;
                new_velocity = datum.velocity - cur_distance_travelled;
                new_velocity *= ExplosionConstants::RESTITUTION_COEFF;
                break;
            }
            new_position = maybe_new_position;
            new_velocity = datum.velocity - cur_distance_travelled;
        }
    }

    bool do_next = new_velocity >= 1;

    if( new_position != position ) {
        if( is_mob ) {
            std::get<Creature *>( cur_target )->setpos( new_position );
        } else {
            item *target = &*std::get<safe_reference<item>>( cur_target );

            if( get_option<bool>( "ITEM_DAMAGE_ON_FLYING_IMPACT" ) ) {
                const auto weight = target->weight();
                const auto vol = target->volume();
                const auto total_vol_ml = units::to_milliliter( vol );
                const auto unit_vol_ml =
                    units::to_milliliter( vol ) / static_cast<double>( std::max( 1, target->count() ) );

                const auto mass_kg = weight / 1000.0_gram;

                const auto max_hardness = ( target->made_of()
                | std::views::transform( []( const auto & x ) { return x.obj().bash_resist(); } )
                | cata::ranges::max() ).value_or( 0 );

                const auto acts_as_shrapnel = max_hardness >= 3 && unit_vol_ml < 50.0;
                const auto effective_velocity_ms = new_velocity * ( acts_as_shrapnel ? 10.0 : 3.0 );
                const auto kinetic_energy = 0.5 * mass_kg * std::pow( effective_velocity_ms, 2 );

                // add_msg( "name: %s, ke: %.4f, vel: %.4f, mass: %.4fkg, vol: %dml, hard: %d, shrap: %d",
                //          target->tname(), kinetic_energy, effective_velocity_ms,
                //          mass_kg, units::to_milliliter( vol ),
                //          max_hardness, static_cast<int>( acts_as_shrapnel ) );
                damage_instance dmg;

                const auto damage_per_joule = acts_as_shrapnel ? 0.15 : 0.1;
                if( acts_as_shrapnel ) {
                    const auto penetration_factor = 1.0 + ( 15.0 / ( total_vol_ml + 1.0 ) );
                    const auto hardness_factor = 1.0 + ( max_hardness * 0.1 );

                    const auto base_damage = kinetic_energy * penetration_factor * hardness_factor * damage_per_joule;
                    const auto velocity_damage = effective_velocity_ms * 0.05;

                    auto final_damage = static_cast<int>( base_damage + velocity_damage );
                    if( effective_velocity_ms > 20 && final_damage < 1 ) { final_damage = 1; }

                    // add_msg( m_debug, "SHRAPNEL: %s | KE: %.1f | Base: %.1f | VelDmg: %.1f | Final: %d",
                    //          target->tname(), kinetic_energy, base_damage, velocity_damage, final_damage );

                    const auto ap_val = static_cast<int>( final_damage * 0.8 );

                    const damage_instance &thrown_dmg = target->base_damage_thrown();
                    auto dt = DT_BASH;
                    auto max_dmg = 0.0f;
                    for( const damage_unit &du : thrown_dmg.damage_units ) {
                        if( du.amount > max_dmg ) {
                            max_dmg = du.amount;
                            dt = du.type;
                        }
                    }
                    dmg.add_damage( dt, final_damage, ap_val );
                } else {
                    auto dispersion =
                        vol > 500_ml ? 500.0 / units::to_milliliter( vol ) : 1.0;
                    if( max_hardness < 2 ) { dispersion *= 0.1; }

                    const auto base_damage = kinetic_energy * dispersion * damage_per_joule;
                    const auto momentum_damage = mass_kg * effective_velocity_ms * 0.5;
                    const auto bash_damage = base_damage + momentum_damage;
                    // add_msg( "BLUNT: %s | Mass: %.1f | Vel: %.1f | KE: %.1f | Mom: %.1f | Dmg: %f",
                    //          target->tname(), mass_kg, effective_velocity_ms, kinetic_energy,
                    //          momentum_damage, bash_damage );
                    dmg.add_damage( DT_BASH, bash_damage, 0.0f, 3.0f );
                }

                if( !target->base_damage_thrown().empty() && new_velocity > 15 ) {
                    dmg.add( target->base_damage_thrown() );
                }

                const auto to_next = [&] {
                    new_velocity *= ExplosionConstants::RESTITUTION_COEFF;
                    do_next = new_velocity >= 1;
                };
                const auto total_dmg = dmg.total_damage();
                auto *hit_creature = g->critter_at( new_position );
                if( total_dmg >= 1 && hit_creature ) {
                    auto hit_part = bodypart_id( "torso" );
                    if( one_in( 3 ) ) {
                        hit_part = bodypart_id( "head" );
                    } else if( one_in( 4 ) ) {
                        hit_part = one_in( 2 ) ? bodypart_id( "arm_l" ) : bodypart_id( "arm_r" );
                    } else if( one_in( 4 ) ) {
                        hit_part = one_in( 2 ) ? bodypart_id( "leg_l" ) : bodypart_id( "leg_r" );
                    }

                    auto *attacker = emitter.has_value() ? emitter.value() : nullptr;
                    auto dealt_damage = hit_creature->deal_damage( attacker, hit_part, dmg ).total_damage();

                    if( get_avatar().sees( *hit_creature ) ) {
                        if( dealt_damage > 0 ) {
                            //~ %1$s: entity dealing damage, %2$s: target creature name, %3$d: damage value
                            add_msg( _( "%1$s flies and hits %2$s for %3$d damage!" ),
                                     target->tname(), hit_creature->disp_name(), dealt_damage );
                        } else {
                            //~ %1$s: entity dealing damage, %2$s: target creature name
                            add_msg( _( "%1$s flies and hits %2$s but deals no damage." ),
                                     target->tname(), hit_creature->disp_name() );
                        }
                    }
                    to_next();
                } else if( total_dmg >= 1 ) {
                    const auto bash_str = static_cast<int>( std::lerp( std::sqrt( total_dmg ), total_dmg, 0.6 ) );
                    const auto bash_result = here.bash( new_position, bash_str );

                    if( bash_result.did_bash && bash_result.success && get_avatar().sees( new_position ) ) {
                        //~ %1$s: item name
                        add_msg( _( "%1$s flies and smashes into something!" ), target->tname() );
                    }
                    if( bash_result.did_bash ) { to_next(); }
                }
            }

            detached_ptr<item> detached = target->detach();

            here.add_item( new_position, std::move( detached ) );

            recombination_targets.push_back( position );
            recombination_targets.push_back( new_position );
        }
        request_redraw |= position.z == g->u.posz();
        request_redraw |= new_position.z == g->u.posz();
    }

    if( do_next ) {
        add_event(
            one_tile_at_vel( new_velocity ),
            is_mob ?
            ExplosionEvent::mob_movement( new_position,
                                          std::get<Creature *>( cur_target ), new_angle, new_velocity, cur_relative_time ) :
            ExplosionEvent::item_movement( new_position,
                                           std::get<safe_reference<item>>( cur_target ), new_angle, new_velocity, cur_relative_time )
        );
    }
}

void ExplosionProcess::run()
{
    fill_maps();
    init_event_queue();

    // We need to temporary disable it because
    //   larger explosions may end up filling
    //   the texture pool, causing a crash
    bool disable_minimap = is_animated() && pixel_minimap_option;
    if( disable_minimap ) {
        g->toggle_pixel_minimap();
    }

    map &here = get_map();
    while( process_next() ) {
        // No need to redraw in testing mode
        if( request_redraw && is_animated() ) {
            ui_manager::redraw();
            refresh_display();
            request_redraw = false;
        }
        update_timings();
    };

    // Reenable disabled options
    if( disable_minimap ) {
        g->toggle_pixel_minimap();
    }

    // Remove temporary flags
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; z++ ) {
        for( const auto &pos : here.points_on_zlevel( z ) ) {
            for( auto &it : here.i_at( pos ) ) {
                it->unset_flag( flag_EXPLOSION_SMASHED );
                it->unset_flag( flag_EXPLOSION_PROPELLED );
            }
        }
    }

    // Make sure the map is centered around the player
    if( player_flung.has_value() ) {
        g->update_map( *player_flung.value() );
    }

    // Finally, recombine thrown items into full stacks again
    std::sort( recombination_targets.begin(), recombination_targets.end() );
    auto end = std::unique( recombination_targets.begin(), recombination_targets.end() );
    recombination_targets.erase( end, recombination_targets.end() );

    for( const auto &position : recombination_targets ) {
        for( detached_ptr<item> &it : here.i_clear( position ) ) {
            here.add_item_or_charges( position, std::move( it ) );
        }
    }
}

void explosion( const tripoint &p, Creature *source, float power, float factor, bool fire,
                int legacy_casing_mass,
                float )
{
    if( factor >= 1.0f ) {
        debugmsg( "called game::explosion with factor >= 1.0 (infinite size)" );
    }
    explosion_data data;
    data.damage = power * explosion_handler::power_to_dmg_mult;
    data.radius = explosion_handler::blast_radius_from_legacy( power, factor );
    data.fire = fire;
    if( legacy_casing_mass > 0 ) {
        data.fragment = explosion_handler::shrapnel_from_legacy( power, data.radius );
    }
    explosion( p, data, source );
}

void explosion( const tripoint &p, const explosion_data &ex, Creature *source )
{
    queued_explosion qe( p, ExplosionType::Regular, source );
    qe.exp_data = ex;
    get_explosion_queue().add( std::move( qe ) );
}

static std::map<const Creature *, int> legacy_shrapnel( const tripoint &src,
        const projectile &fragment,
        Creature *source )
{
    std::map<const Creature *, int> damaged;

    projectile proj = fragment;
    proj.add_effect( ammo_effect_NULL_SOURCE );

    float obstacle_cache[MAPSIZE_X][MAPSIZE_Y] = {};
    float visited_cache[MAPSIZE_X][MAPSIZE_Y] = {};

    map &here = get_map();

    diagonal_blocks( &blocked_cache )[MAPSIZE_X][MAPSIZE_Y] = here.access_cache(
                src.z ).vehicle_obstructed_cache;

    // TODO: Calculate range based on max effective range for projectiles.
    // Basically bisect between 0 and map diameter using shrapnel_calc().
    // Need to update shadowcasting to support limiting range without adjusting initial distance.
    const tripoint_range<tripoint> area = here.points_on_zlevel( src.z );

    here.build_obstacle_cache( area.min(), area.max() + tripoint_south_east, obstacle_cache );

    // Shadowcasting normally ignores the origin square,
    // so apply it manually to catch monsters standing on the explosive.
    // This "blocks" some fragments, but does not apply deceleration.
    visited_cache[src.x][src.y] = 1.0f;

    // This is used to limit radius
    // By default, the radius is 60, so negative values can be helpful here
    const int offset_distance = 60 - 1 - fragment.range;
    castLightAll<float, float, shrapnel_calc, shrapnel_check,
                 update_fragment_cloud, accumulate_fragment_cloud>
                 ( visited_cache, obstacle_cache, blocked_cache, src.xy(),
                   offset_distance, fragment.range + 1.0f );

    // Now visited_caches are populated with density and velocity of fragments.
    for( const tripoint &target : area ) {
        if( visited_cache[target.x][target.y] <= 0.0f || rl_dist( src, target ) > fragment.range ) {
            continue;
        }
        auto critter = g->critter_at( target );
        if( critter && !critter->is_dead_state() ) {
            // dealt_dag->m.total_damage() == 0 means armor block
            // dealt_dag->m.total_damage() > 0 means took damage
            // Need to diffentiate target among player, npc, and monster
            int damage_taken = 0;
            auto bps = critter->get_all_body_parts( true );
            // Humans get hit in all body parts
            if( critter->is_player() ) {
                for( bodypart_id bp : bps ) {
                    // TODO: Apply projectile effects
                    // TODO: Penalize low coverage armor
                    // Halve damage to be closer to what monsters take
                    damage_instance half_impact = proj.impact;
                    half_impact.mult_damage( 0.5f );
                    dealt_damage_instance dealt = critter->deal_damage( source, bp, proj.impact );
                    if( dealt.total_damage() > 0 ) {
                        damage_taken += dealt.total_damage();
                    }
                }
            } else {
                dealt_damage_instance dealt = critter->deal_damage( source, bps[0], proj.impact );
                if( dealt.total_damage() > 0 ) {
                    damage_taken += dealt.total_damage();
                }
            }
            damaged[critter] = damage_taken;
        }
        if( here.impassable( target ) ) {
            int damage = proj.impact.total_damage();
            if( optional_vpart_position vp = here.veh_at( target ) ) {
                vp->vehicle().damage( vp->part_index(), damage / 100 );
            } else {
                here.bash( target, damage / 100, true );
            }
        }
    }

    const tripoint &blast_center = src;
    const float raw_blast_radius = fragment.range;

    using dist_point_pair = std::pair<float, tripoint>;
    const int Z_LEVEL_DIST = 4;

    const int z_levels_affected = raw_blast_radius / Z_LEVEL_DIST;
    const tripoint_range<tripoint> affected_block(
        blast_center + tripoint( -raw_blast_radius, -raw_blast_radius, -z_levels_affected ),
        blast_center + tripoint( raw_blast_radius, raw_blast_radius, z_levels_affected )
    );

    static std::vector<dist_point_pair> blast_map( MAPSIZE_X * MAPSIZE_Y );
    static std::map<tripoint, nc_color> explosion_colors;
    blast_map.clear();
    explosion_colors.clear();

    for( const tripoint &target : affected_block ) {
        if( !get_map().inbounds( target ) ) {
            continue;
        }

        // Uses this ternany check instead of rl_dist because it converts trig_dist's distance to int implicitly
        const float distance = (
                                   trigdist ?
                                   trig_dist( blast_center, target ) :
                                   square_dist( blast_center, target )
                               );
        const float z_distance = abs( target.z - blast_center.z );
        const float z_aware_distance = distance + ( Z_LEVEL_DIST - 1 ) * z_distance;
        if( z_aware_distance <= raw_blast_radius ) {
            blast_map.emplace_back( z_aware_distance, target );
        }
    }

    std::stable_sort( blast_map.begin(), blast_map.end(), []( dist_point_pair pair1,
    dist_point_pair pair2 ) {
        return pair1.first < pair2.first;
    } );

    int animated_explosion_range = 0.0f;
    std::map<const Creature *, int> blasted;

    int i = 0;
    for( const dist_point_pair &pair : blast_map ) {
        float distance;
        tripoint position;
        tripoint last_position = blast_center;
        std::tie( distance, position ) = pair;

        const std::vector<tripoint> line_of_movement = line_to( blast_center, position );
        const bool has_obstacles = std::any_of( line_of_movement.begin(),
        line_of_movement.end(), [position, &last_position]( tripoint ray_position ) {
            if( get_map().obstructed_by_vehicle_rotation( last_position, ray_position ) ) {
                return true;
            }
            last_position = ray_position;
            return ray_position != position && get_map().impassable( ray_position );
        } );

        // Don't bother animating explosions that are on other levels
        const bool to_animate = get_player_character().posz() == position.z;

        // Animate the explosion by drawing the shock wave rather than the whole explosion
        if( to_animate && distance > animated_explosion_range ) {
            // Draw only 1/2 rings to speed up the animation
            if( i % 2 == 0 ) {
                draw_custom_explosion( blast_center, explosion_colors, "fd_smoke" );
            }
            i++;
            explosion_colors.clear();
            animated_explosion_range++;
        }

        if( has_obstacles ) {
            continue;
        }

        if( to_animate ) {
            explosion_colors[position] = c_white;
        }
    }
    // Final blast wave points
    draw_custom_explosion( blast_center, explosion_colors, "fd_smoke" );
    // END DRAWING EXPLOSION

    return damaged;
}

// (C1001) Compiler Internal Error on Visual Studio 2015 with Update 2
static std::map<const Creature *, int> legacy_blast( const tripoint &p, const float power,
        const float radius, const bool fire, Creature *source )
{
    std::map<const Creature *, int> blasted;

    if( radius <= 0.0f ) {
        return blasted; // Just return an empty one
    }

    const float tile_dist = 1.0f;
    const float diag_dist = trigdist ? M_SQRT2 * tile_dist : 1.0f * tile_dist;
    const float zlev_dist = 4.0f; // Penalty for going up/down
    // 7 3 5
    // 1 . 2
    // 6 4 8
    // 9 and 10 are up and down
    static const int x_offset[10] = { -1, 1,  0, 0,  1, -1, -1, 1, 0, 0 };
    static const int y_offset[10] = { 0, 0, -1, 1, -1,  1, -1, 1, 0, 0 };
    static const int z_offset[10] = { 0, 0,  0, 0,  0,  0,  0, 0, 1, -1 };
    map &here = get_map();
    const size_t max_index = here.has_zlevels() ? 10 : 8;

    here.bash( p, fire ? power : ( 2 * power ), true, false, false );

    std::priority_queue< std::pair<float, tripoint>, std::vector< std::pair<float, tripoint> >, pair_greater_cmp_first >
    open;
    std::set<tripoint> closed;
    std::map<tripoint, float> dist_map;
    open.emplace( 0.0f, p );
    dist_map[p] = 0.0f;
    // Find all points to blast
    while( !open.empty() ) {
        const float distance = open.top().first;
        const tripoint pt = open.top().second;
        open.pop();

        if( closed.contains( pt ) ) {
            continue;
        }

        closed.insert( pt );

        const float force = power * obstacle_blast_percentage( radius, distance );
        if( force <= 1.0f ) {
            continue;
        }

        if( here.impassable( pt ) && pt != p ) {
            // Don't propagate further
            continue;
        }

        // Iterate over all neighbors. Bash all of them, propagate to some
        for( size_t i = 0; i < max_index; i++ ) {
            tripoint dest( pt + tripoint( x_offset[i], y_offset[i], z_offset[i] ) );
            if( closed.contains( dest ) || !here.inbounds( dest ) ||
                here.obstructed_by_vehicle_rotation( pt, dest ) ) {
                continue;
            }

            if( z_offset[i] == 0 ) {
                // Horizontal - no floor bashing
                here.bash( dest, force, true, false, false );
            } else if( z_offset[i] > 0 ) {
                // Should actually bash through the floor first, but that's not really possible yet
                here.bash( dest, force, true, false, true );
            } else if( !here.valid_move( pt, dest, false, true ) ) {
                // Only bash through floor if it doesn't exist
                // Bash the current tile's floor, not the one's below
                here.bash( pt, force, true, false, true );
            }

            float next_dist = distance;
            next_dist += ( x_offset[i] == 0 || y_offset[i] == 0 ) ? tile_dist : diag_dist;
            if( z_offset[i] != 0 ) {
                if( !here.valid_move( pt, dest, false, true ) ) {
                    continue;
                }

                next_dist += zlev_dist;
            }

            if( !dist_map.contains( dest ) || dist_map[dest] > next_dist ) {
                open.emplace( next_dist, dest );
                dist_map[dest] = next_dist;
            }
        }
    }

    // Draw the explosion
    std::map<tripoint, nc_color> explosion_colors;
    for( auto &pt : closed ) {
        if( here.impassable( pt ) ) {
            continue;
        }

        float percentage = obstacle_blast_percentage( radius, dist_map.at( pt ) );
        if( percentage > 0.0f ) {
            static const std::array<nc_color, 3> colors = { {
                    c_red, c_yellow, c_white
                }
            };
            size_t color_index = ( power > 30 ? 1 : 0 ) + ( percentage > 0.5f ? 1 : 0 );

            explosion_colors[pt] = colors[color_index];
        }
    }

    draw_custom_explosion( g->u.pos(), explosion_colors, "explosion" );

    for( const tripoint &pt : closed ) {
        const float force = power * obstacle_blast_percentage( radius, dist_map.at( pt ) );
        if( force < 1.0f ) {
            // Too weak to matter
            continue;
        }

        here.smash_items( pt, force, _( "force of the explosion" ), true );

        if( fire ) {
            int intensity = 1 + ( force > 10.0f ) + ( force > 30.0f );

            if( !here.has_zlevels() && here.is_outside( pt ) && intensity == 2 ) {
                // In 3D mode, it would have fire fields above, which would then fall
                // and fuel the fire on this tile
                intensity++;
            }

            here.add_field( pt, fd_fire, intensity );
        }

        if( const optional_vpart_position vp = here.veh_at( pt ) ) {
            // TODO: Make this weird unit used by vehicle::damage more sensible
            vp->vehicle().damage( vp->part_index(), force, fire ? DT_HEAT : DT_BASH, false );
        }

        Creature *critter = g->critter_at( pt, true );
        if( critter == nullptr ) {
            continue;
        }

        player *pl = dynamic_cast<player *>( critter );
        if( pl == nullptr ) {
            const double dmg = std::max( force - critter->get_armor_bash( bodypart_id( "torso" ) ) / 3.0, 0.0 );
            const int actual_dmg = rng_float( dmg, dmg * 2 );
            critter->apply_damage( source, bodypart_id( "torso" ), actual_dmg );
            critter->check_dead_state();
            blasted[critter] = actual_dmg;
            continue;
        }

        // Print messages for all NPCs
        pl->add_msg_if_player( m_bad, _( "You're caught in the explosion!" ) );

        struct blastable_part {
            bodypart_id bp;
            float low_mul;
            float high_mul;
            float armor_mul;
        };

        static const std::array<blastable_part, 6> blast_parts = { {
                { bodypart_id( "torso" ), 0.5f, 1.0f, 0.5f },
                { bodypart_id( "head" ),  0.5f, 1.0f, 0.5f },
                // Hit limbs harder so that it hurts more without being much more deadly
                { bodypart_id( "leg_l" ), 0.75f, 1.25f, 0.4f },
                { bodypart_id( "leg_r" ), 0.75f, 1.25f, 0.4f },
                { bodypart_id( "arm_l" ), 0.75f, 1.25f, 0.4f },
                { bodypart_id( "arm_r" ), 0.75f, 1.25f, 0.4f },
            }
        };

        for( const auto &blp : blast_parts ) {
            const int part_dam = rng( force * blp.low_mul, force * blp.high_mul );
            const std::string hit_part_name = body_part_name_accusative( blp.bp->token );
            const auto dmg_instance = damage_instance( DT_BASH, part_dam, 0, blp.armor_mul );
            const auto result = pl->deal_damage( source, blp.bp, dmg_instance );
            const int res_dmg = result.total_damage();

            add_msg( m_debug, "%s for %d raw, %d actual", hit_part_name, part_dam, res_dmg );
            if( res_dmg > 0 ) {
                blasted[critter] += res_dmg;
            }
        }
    }

    return blasted;
}

void explosion_funcs::regular( const queued_explosion &qe )
{
    const tripoint &p = qe.pos;
    const explosion_data &ex = qe.exp_data;
    auto &shr = ex.fragment;

    cata::run_hooks( "on_explosion_start", [&]( sol::table & params ) {
        params["pos"] = p;
        params["damage"] = ex.damage;
        params["radius"] = static_cast<int>( ex.radius );
        params["fire"] = ex.fire;
    } );

    int base_noise = ex.damage;
    if( shr ) {
        base_noise = shr.value().impact.total_damage();
    }

    const int noise = base_noise * ( ex.fire ? 2 : 10 );
    if( noise >= 1000 ) {
        sounds::sound( p, noise, sounds::sound_t::combat, _( "a huge explosion!" ), false, "explosion",
                       "huge" );
    } else if( noise >= 100 ) {
        sounds::sound( p, noise, sounds::sound_t::combat, _( "an explosion!" ), false, "explosion",
                       "default" );
    } else if( noise > 0 ) {
        sounds::sound( p, 3, sounds::sound_t::combat, _( "a loud pop!" ), false, "explosion", "small" );
    }

    std::map<const Creature *, int> damaged_by_blast;
    std::map<const Creature *, int> damaged_by_shrapnel;

    if( get_option<bool>( "OLD_EXPLOSIONS" ) ) {
        if( shr ) {
            damaged_by_shrapnel = legacy_shrapnel( p, shr.value(), qe.source );
        }
        damaged_by_blast = legacy_blast( p, ex.damage, ex.radius, ex.fire, qe.source );
    } else {
        ExplosionProcess process( p, ex.damage, ex.radius, shr, ex.fire, std::make_optional( qe.source ) );
        process.run();
        damaged_by_blast = process.get_blasted();
        damaged_by_shrapnel = process.get_shrapneled();
    }

    // Not the cleanest way to do it
    std::map<const Creature *, int> total_damaged;
    for( const auto &pr : damaged_by_blast ) {
        total_damaged[pr.first] += pr.second;
    }
    for( const auto &pr : damaged_by_shrapnel ) {
        total_damaged[pr.first] += pr.second;
    }

    const auto print_damage = [&]( const std::pair<const Creature *, int> &pr,
    const std::function<bool( const Creature & )> &predicate ) {
        if( predicate( *pr.first ) && g->u.sees( *pr.first ) ) {
            const Creature *critter = pr.first;
            bool blasted = damaged_by_blast.contains( critter );
            bool shredded = damaged_by_shrapnel.contains( critter );
            std::string cause_description = ( blasted && shredded ) ? _( "the explosion and shrapnel" ) :
                                            blasted ? _( "the explosion" ) :
                                            _( "the shrapnel" );
            std::string damage_description = ( pr.second > 0 ) ?
                                             string_format( _( "taking %d damage" ), pr.second ) :
                                             _( "but takes no damage" );
            if( critter->is_player() ) {
                add_msg( _( "You are hit by %s, %s." ),
                         cause_description, damage_description );
            } else if( critter->is_npc() ) {
                critter->add_msg_if_npc(
                    _( "<npcname> is hit by %s, %s." ),
                    cause_description, damage_description );
            } else {
                add_msg( _( "%s is hit by %s, %s." ),
                         critter->disp_name( false, true ), cause_description, damage_description );
            }

        }
    };

    // TODO: Clean this up, without affecting the results
    for( const auto &pr : total_damaged ) {
        print_damage( pr, []( const Creature & c ) {
            return c.is_monster();
        } );
    }
    for( const auto &pr : total_damaged ) {
        print_damage( pr, []( const Creature & c ) {
            return c.is_npc();
        } );
    }
    for( const auto &pr : total_damaged ) {
        print_damage( pr, []( const Creature & c ) {
            return c.is_avatar();
        } );
    }
}

void flashbang( const tripoint &p, bool player_immune, const std::string &exp_name )
{
    // flashbangs cannot kill, so skip the source
    queued_explosion qe( p, ExplosionType::Flashbang, nullptr );
    qe.affects_player = !player_immune;
    qe.graphics_name = exp_name;
    get_explosion_queue().add( std::move( qe ) );
}

void explosion_funcs::flashbang( const queued_explosion &qe )
{
    const tripoint &p = qe.pos;
    map &here = get_map();

    draw_explosion( p, 8, c_white, qe.graphics_name );
    int dist = rl_dist( g->u.pos(), p );
    if( dist <= 8 && qe.affects_player ) {
        if( !g->u.has_bionic( bio_ears ) && !g->u.is_wearing( itype_rm13_armor_on ) ) {
            g->u.add_effect( effect_deaf, time_duration::from_turns( 40 - dist * 4 ) );
        }
        if( here.sees( g->u.pos(), p, 8 ) ) {
            int flash_mod = 0;
            if( g->u.has_trait( trait_PER_SLIME ) ) {
                if( one_in( 2 ) ) {
                    flash_mod = 3; // Yay, you weren't looking!
                }
            } else if( g->u.has_trait( trait_PER_SLIME_OK ) ) {
                flash_mod = 8; // Just retract those and extrude fresh eyes
            } else if( g->u.has_bionic( bio_sunglasses ) ||
                       g->u.is_wearing( itype_rm13_armor_on ) ) {
                flash_mod = 6;
            } else if( g->u.worn_with_flag( flag_BLIND ) ||
                       g->u.worn_with_flag( flag_FLASH_PROTECTION ) ) {
                flash_mod = 3; // Not really proper flash protection, but better than nothing
            }
            g->u.add_env_effect( effect_blind, body_part_eyes, ( 12 - flash_mod - dist ) / 2,
                                 time_duration::from_turns( 10 - dist ) );
        }
    }
    for( monster &critter : g->all_monsters() ) {
        if( critter.type->in_species( ROBOT ) ) {
            continue;
        }
        // TODO: can the following code be called for all types of creatures
        dist = rl_dist( critter.pos(), p );
        if( dist <= 8 ) {
            if( dist <= 4 ) {
                critter.add_effect( effect_stunned, time_duration::from_turns( 10 - dist ) );
            }
            if( critter.has_flag( MF_SEES ) && here.sees( critter.pos(), p, 8 ) ) {
                critter.add_effect( effect_blind, time_duration::from_turns( 18 - dist ) );
            }
            if( critter.has_flag( MF_HEARS ) ) {
                critter.add_effect( effect_deaf, time_duration::from_turns( 60 - dist * 4 ) );
            }
        }
    }
    sounds::sound( p, 12, sounds::sound_t::combat, _( "a huge boom!" ), false, "misc", "flashbang" );
    // TODO: Blind/deafen NPC
}

void shockwave( const tripoint &p, const shockwave_data &sw, const std::string &exp_name,
                Creature *source )
{
    queued_explosion qe( p, ExplosionType::Shockwave, source );
    qe.swave_data = sw;
    qe.graphics_name = exp_name;
    get_explosion_queue().add( std::move( qe ) );
}

void explosion_funcs::shockwave( const queued_explosion &qe )
{
    const tripoint &p = qe.pos;
    const shockwave_data &sw = qe.swave_data;

    draw_explosion( p, sw.radius, c_blue, qe.graphics_name );

    sounds::sound( p, sw.force * sw.force * sw.dam_mult / 2, sounds::sound_t::combat, _( "Crack!" ),
                   false,
                   "misc", "shockwave" );

    for( monster &critter : g->all_monsters() ) {
        if( critter.posz() != p.z ) {
            continue;
        }
        if( rl_dist( critter.pos(), p ) <= sw.radius ) {
            add_msg( _( "%s is caught in the shockwave!" ), critter.name() );
            g->knockback( p, critter.pos(), sw.force, sw.stun, sw.dam_mult, qe.source );
        }
    }
    // TODO: combine the two loops and the case for g->u using all_creatures()
    for( npc &guy : g->all_npcs() ) {
        if( guy.posz() != p.z ) {
            continue;
        }
        if( rl_dist( guy.pos(), p ) <= sw.radius ) {
            add_msg( _( "%s is caught in the shockwave!" ), guy.name );
            g->knockback( p, guy.pos(), sw.force, sw.stun, sw.dam_mult, qe.source );
        }
    }
    if( rl_dist( g->u.pos(), p ) <= sw.radius && sw.affects_player &&
        ( !g->u.has_trait( trait_LEG_TENT_BRACE ) || g->u.footwear_factor() == 1 ||
          ( g->u.footwear_factor() == .5 && one_in( 2 ) ) ) ) {
        add_msg( m_bad, _( "You're caught in the shockwave!" ) );
        g->knockback( p, g->u.pos(), sw.force, sw.stun, sw.dam_mult, qe.source );
    }
}

void scrambler_blast( const tripoint &p )
{
    if( monster *const mon_ptr = g->critter_at<monster>( p ) ) {
        monster &critter = *mon_ptr;
        if( critter.has_flag( MF_ELECTRONIC ) ) {
            critter.make_friendly();
        }
        add_msg( m_warning, _( "The %s sparks and begins searching for a target!" ),
                 critter.name() );
    }
}

void emp_blast( const tripoint &p )
{
    map &here = get_map();
    Character &u = get_player_character();
    const bool sight = u.sees( p );
    if( here.has_flag( "CONSOLE", p ) ) {
        if( sight ) {
            add_msg( _( "The %s is rendered non-functional!" ), here.tername( p ) );
        }
        here.ter_set( p, t_console_broken );
        return;
    }
    // TODO: More terrain effects.
    if( here.ter( p ) == t_card_science || here.ter( p ) == t_card_military ||
        here.ter( p ) == t_card_industrial ) {
        int rn = rng( 1, 100 );
        if( rn > 92 || rn < 40 ) {
            if( sight ) {
                add_msg( _( "The card reader is rendered non-functional." ) );
            }
            here.ter_set( p, t_card_reader_broken );
        }
        if( rn > 80 ) {
            if( sight ) {
                add_msg( _( "The nearby doors slide open!" ) );
            }
            for( int i = -3; i <= 3; i++ ) {
                for( int j = -3; j <= 3; j++ ) {
                    tripoint p2 = p + tripoint( i, j, 0 );
                    if( here.ter( p2 ) == t_door_metal_locked ) {
                        here.ter_set( p2, t_floor );
                    }
                }
            }
        }
        if( rn >= 40 && rn <= 80 ) {
            if( sight ) {
                add_msg( _( "Nothing happens." ) );
            }
        }
    }
    if( monster *const mon_ptr = g->critter_at<monster>( p ) ) {
        monster &critter = *mon_ptr;
        if( critter.has_flag( MF_ELECTRONIC ) ) {
            int deact_chance = 0;
            const auto mon_item_id = critter.type->revert_to_itype;
            switch( critter.get_size() ) {
                case creature_size::tiny:
                    deact_chance = 6;
                    break;
                case creature_size::small:
                    deact_chance = 3;
                    break;
                default:
                    // Currently not used, I have no idea what chances bigger bots should have,
                    // Maybe export this to json?
                    break;
            }
            if( !mon_item_id.is_empty() && deact_chance != 0 && one_in( deact_chance ) ) {
                if( sight ) {
                    add_msg( _( "The %s beeps erratically and deactivates!" ), critter.name() );
                }
                here.add_item_or_charges( p, critter.to_item() );
                for( auto &ammodef : critter.ammo ) {
                    if( ammodef.second > 0 ) {
                        here.spawn_item( p, ammodef.first, 1, ammodef.second, calendar::turn );
                    }
                }
                g->remove_zombie( critter );
            } else {
                if( sight ) {
                    add_msg( _( "The EMP blast fries the %s!" ), critter.name() );
                }
                int dam = dice( 10, 10 );
                critter.apply_damage( nullptr, bodypart_id( "torso" ), dam );
                critter.check_dead_state();
                if( !critter.is_dead() && one_in( 6 ) ) {
                    critter.make_friendly();
                }
            }
        } else if( critter.has_flag( MF_ELECTRIC_FIELD ) ) {
            if( !critter.has_effect( effect_emp ) ) {
                if( sight ) {
                    add_msg( m_good, _( "The %s's electrical field momentarily goes out!" ), critter.name() );
                }
                critter.add_effect( effect_emp, 3_minutes );
            } else if( critter.has_effect( effect_emp ) ) {
                int dam = dice( 3, 5 );
                if( sight ) {
                    add_msg( m_good, _( "The %s's disabled electrical field reverses polarity!" ),
                             critter.name() );
                    add_msg( m_good, _( "It takes %d damage." ), dam );
                }
                critter.add_effect( effect_emp, 1_minutes );
                critter.apply_damage( nullptr, bodypart_id( "torso" ), dam );
                critter.check_dead_state();
            }
        } else if( sight ) {
            add_msg( _( "The %s is unaffected by the EMP blast." ), critter.name() );
        }
    }
    if( u.pos() == p ) {
        if( u.get_power_level() > 0_kJ ) {
            add_msg( m_bad, _( "The EMP blast drains your power." ) );
            int max_drain = ( u.get_power_level() > 1000_kJ ? 1000 : units::to_kilojoule(
                                  u.get_power_level() ) );
            u.mod_power_level( units::from_kilojoule( -rng( 1 + max_drain / 3, max_drain ) ) );
        }
        // TODO: More effects?
        //e-handcuffs effects
        item &cuffs = u.primary_weapon();
        if( cuffs.typeId() == itype_e_handcuffs && cuffs.charges > 0 ) {
            cuffs.unset_flag( flag_NO_UNWIELD );
            cuffs.charges = 0;
            cuffs.deactivate();
            add_msg( m_good, _( "The %s on your wrists spark briefly, then release your hands!" ),
                     cuffs.tname() );
        }
    }
    // Drain any items of their battery charge
    for( auto &it : here.i_at( p ) ) {
        if( it->is_tool() && it->ammo_current() == itype_battery ) {
            it->charges = 0;
        }
    }
    // TODO: Drain NPC energy reserves
}

void resonance_cascade( const tripoint &p )
{
    get_explosion_queue().add( queued_explosion( p, ExplosionType::ResonanceCascade, nullptr ) );
}

void explosion_funcs::resonance_cascade( const queued_explosion &qe )
{
    map &here = get_map();
    const tripoint &p = qe.pos;

    const time_duration maxglow = time_duration::from_turns( 100 - 5 * trig_dist( p, g->u.pos() ) );
    if( maxglow > 0_turns ) {
        const time_duration minglow = std::max( 0_turns, time_duration::from_turns( 60 - 5 * trig_dist( p,
                                                g->u.pos() ) ) );
        g->u.add_effect( effect_teleglow, rng( minglow, maxglow ) * 100 );
    }

    constexpr half_open_rectangle<point> map_bounds( point_zero, point( MAPSIZE_X, MAPSIZE_Y ) );
    constexpr point cascade_reach( 8, 8 );

    point start = clamp( p.xy() - cascade_reach, map_bounds );
    point end = clamp( p.xy() + cascade_reach, map_bounds );

    std::vector<int> rolls;

    tripoint dest = p;
    for( dest.y = start.y; dest.y < end.y; dest.y++ ) {
        for( dest.x = start.x; dest.x < end.x; dest.x++ ) {
            switch( rng( 0, 80 ) ) {
                case 1:
                case 2:
                    emp_blast( dest );
                    break;
                case 3:
                case 4:
                case 5:
                    for( int k = -1; k <= 1; k++ ) {
                        for( int l = -1; l <= 1; l++ ) {
                            field_type_id type = fd_null;
                            switch( rng( 1, 7 ) ) {
                                case 1:
                                    type = fd_blood;
                                    break;
                                case 2:
                                    type = fd_bile;
                                    break;
                                case 3:
                                case 4:
                                    type = fd_slime;
                                    break;
                                case 5:
                                    type = fd_fire;
                                    break;
                                case 6:
                                case 7:
                                    type = fd_nuke_gas;
                                    break;
                            }
                            if( !one_in( 3 ) ) {
                                here.add_field( dest + point( k, l ), type, 3 );
                            }
                        }
                    }
                    break;
                case  6:
                case  7:
                case  8:
                case  9:
                case 10:
                    here.trap_set( dest, tr_portal );
                    break;
                case 11:
                case 12:
                    here.trap_set( dest, tr_goo );
                    break;
                case 13:
                case 14:
                case 15: {
                    MonsterGroupResult spawn_details = MonsterGroupManager::GetResultFromGroup( GROUP_NETHER );
                    g->place_critter_at( spawn_details.name, dest );
                    break;
                }
                case 16:
                case 17:
                case 18:
                    here.destroy( dest );
                    break;
                case 19: {
                    explosion_data ex;
                    ex.radius = 1;
                    ex.damage = rng( 1, 10 );
                    ex.fire = one_in( 4 );
                    explosion( dest, ex, qe.source );
                    break;
                }
                default:
                    break;
            }
        }
    }
}

projectile shrapnel_from_legacy( int power, float blast_radius )
{
    int range = 2 * blast_radius;
    // Damage approximately equal to blast damage at epicenter
    int damage = power * power_to_dmg_mult;
    projectile proj;
    proj.speed = 1000;
    proj.range = range;
    proj.impact.add_damage( DT_BULLET, damage, 0.0f, 3.0f );

    return proj;
}

float blast_radius_from_legacy( int power, float distance_factor )
{
    return std::pow( power * power_to_dmg_mult, ( 1.0 / 4.0 ) ) *
           ( std::log( 0.75f ) / std::log( distance_factor ) );
}

explosion_queue &get_explosion_queue()
{
    static explosion_queue singleton;
    return singleton;
}

void explosion_queue::execute()
{
    explosion_count = 0;
    while( !elems.empty() ) {
        queued_explosion exp = std::move( elems.front() );
        elems.pop_front();
        explosion_count++;
        switch( exp.type ) {
            case ExplosionType::Regular:
                explosion_funcs::regular( exp );
                break;
            case ExplosionType::Flashbang:
                explosion_funcs::flashbang( exp );
                break;
            case ExplosionType::ResonanceCascade:
                explosion_funcs::resonance_cascade( exp );
                break;
            case ExplosionType::Shockwave:
                explosion_funcs::shockwave( exp );
                break;
            default:
                debugmsg( "Explosion type not implemented." );
                break;
        }
    }
}

} // namespace explosion_handler

float shrapnel_calc( const float &intensity, const float &last_obstacle, const int & )
{
    return intensity - last_obstacle;
}

bool shrapnel_check( const float &obstacle, const float &last_intensity )
{
    return last_intensity - obstacle > 0.0f;
}

void update_fragment_cloud( float &output, const float &new_intensity, quadrant )
{
    output = std::max( output, new_intensity );
}

float accumulate_fragment_cloud( const float &cumulative_obstacle, const float &current_obstacle,
                                 const int & )
{
    return std::max( cumulative_obstacle, current_obstacle ) + 1;
}

int explosion_data::safe_range() const
{
    return std::max<int>( radius, fragment ? fragment->range : 0 ) + 1;
}

explosion_data::operator bool() const
{
    return damage > 0 || fragment;
}
