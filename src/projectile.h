#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "damage.h"
#include "point.h"
#include "string_id.h"
#include "detached_ptr.h"

class Creature;
struct explosion_data;
class item;
struct ammo_effect;

using ammo_effect_str_id = string_id<ammo_effect>;

struct projectile {
        damage_instance impact;
        // Speed of the projectile in meters per second.
        // How hard is it to dodge? essentially rolls to-hit.
        // Projectiles travel their full distance instantly, and speed does not effect range.
        // Speed of sound is roughly 340 m/s. Technically changes based on temp and altitude, but that is not worth calculating.
        int speed = 0;
        int range = 0;
        /**
        * Bonus to the potential maximum damage multiplier a projectile can receive from a ranged "crit"
        * A value of 0.5 is +50%
        * Does nothing without stats,skills, or aimedcrit bonus to make use of it
        */
        double aimedcritmaxbonus = 0.0;
        /**
        * Bonus to the damage multiplier for any ranged attack that is a "goodhit" or better (acc < 0.5).
        * Can increase damage up to the maximum potential multiplier, or down to 0%.
        * A value of 0.5 is +50%
        */
        double aimedcritbonus = 0.0;


        /**
         * Returns an item that should be dropped or an item for which is_null() is true
         *  when item to drop is unset.
         */
        item *get_drop() const;
        /** Copies item `it` as a drop for this projectile. */
        void set_drop( detached_ptr<item> &&it );
        detached_ptr<item> unset_drop();

        const explosion_data &get_custom_explosion() const;
        void set_custom_explosion( const explosion_data &ex );
        void unset_custom_explosion();

        const std::set<ammo_effect_str_id> &get_ammo_effects() const {
            return proj_effects;
        }

        projectile();
        projectile( const projectile & );
        projectile( projectile && ) noexcept ;
        projectile &operator=( const projectile & );
        ~projectile();

        void deserialize( JsonIn &jsin );
        void load( JsonObject &jo );

        bool has_effect( const ammo_effect_str_id &id ) const {
            return proj_effects.contains( id );
        }
        void add_effect( const ammo_effect_str_id &id ) {
            proj_effects.insert( id );
        }

    private:
        // Actual item used (to drop contents etc.).
        // Null in case of bullets (they aren't "made of cartridges").
        detached_ptr<item> drop;
        std::unique_ptr<explosion_data> custom_explosion;
        std::set<ammo_effect_str_id> proj_effects;
};

struct dealt_projectile_attack {
    projectile proj; // What we used to deal the attack
    Creature *hit_critter; // The critter that stopped the projectile or null
    dealt_damage_instance dealt_dam; // If hit_critter isn't null, hit data is written here
    tripoint end_point; // Last hit tile (is hit_critter is null, drops should spawn here)
    double missed_by; // Accuracy of dealt attack
    std::vector<tripoint> trajectory; // Actual simulated path, including source tile
    bool suppress_damage_message = false;
};

void apply_ammo_effects( const tripoint &p, const std::set<ammo_effect_str_id> &effects,
                         Creature *source );
// Legacy. TODO: Remove
void apply_ammo_effects( const tripoint &p, const std::set<std::string> &effects,
                         Creature *source );
int max_aoe_size( const std::set<ammo_effect_str_id> &tags );
int max_aoe_size( const std::set<std::string> &tags );
