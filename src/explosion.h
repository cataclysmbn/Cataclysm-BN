#pragma once

#include <optional>
#include <string>

#include "projectile.h"

struct tripoint;
class JsonObject;
class nc_color;

struct explosion_data {
    int damage              = 0;
    float radius            = 0;
    bool fire               = false;
    std::optional<projectile> fragment = std::nullopt;

    /** Returns the range at which blast damage is 0 and shrapnel is out of range. */
    int safe_range() const;
    explicit operator bool() const;
};

struct shockwave_data {
    int radius              = 0;
    int force               = 0;
    int stun                = 0;
    int dam_mult            = 0;
    bool affects_player     = false;
};

// handles explosion related functions
namespace explosion_handler
{
/**
 * Legacy explosion function.
 * Updated values are calculated from distance factor.
 * If power > 0, blast is produced.
 * If casing mass > 0, shrapnel is produced.
 */
void explosion(
    const tripoint &p, Creature *source, float power, float factor = 0.8f,
    bool fire = false, int legacy_casing_mass = 0, float legacy_frag_mass = 0.05
);

void explosion( const tripoint &p, const explosion_data &ex, Creature *source );

constexpr float power_to_dmg_mult = 2.0f / 15.0f;

/** Triggers a flashbang explosion at p. */
void flashbang( const tripoint &p, bool player_immune, const std::string &exp_name );
/** Triggers a resonance cascade at p. */
void resonance_cascade( const tripoint &p );
/** Triggers a scrambler blast at p. */
void scrambler_blast( const tripoint &p );
/** Triggers an EMP blast at p. */
void emp_blast( const tripoint &p );
/** Shockwave applies knockback with given parameters to all targets within radius of p. */
void shockwave( const tripoint &p, const shockwave_data &sw, const std::string &exp_name,
                Creature *source );

projectile shrapnel_from_legacy( int power, float blast_radius );
float blast_radius_from_legacy( int power, float distance_factor );
} // namespace explosion_handler

explosion_data load_explosion_data( const JsonObject &jo );


