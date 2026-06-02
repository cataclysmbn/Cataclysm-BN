#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include "type_id.h"
#include "point.h"

#include "coordinates.h"
#include "enum_traits.h"
#include "units_angle.h"

class Character;
class npc;
class Creature;
class JsonObject;
class item;
class monster;
class player;
class translation;
struct sound_event;
template <typename E> struct enum_traits;

namespace sounds
{
enum class sound_t : int {
    background = 0,
    weather,
    music,
    movement,
    speech,
    electronic_speech, // Any electronic sound that's not music/alarm: Robot speech, radio, etc.
    activity,
    destructive_activity,
    alarm,
    combat, // any violent sounding activity
    alert, // louder than speech to get attention
    order,  // loudest to get attention
    _LAST // must always be last
};

void sound( const sound_event &soundevent );
// Legacy sound methods kept as comments for documentation and compatability reference.
//
/** Functions identical to sound, but all "from" bools are set to false. */
//void ambient_sound( const tripoint &p, short vol, sound_t category,
//                    const std::string &description );
///** Creates a list of coordinates at which to draw footsteps. */
//void add_footstep( const tripoint &p, short volume,
//                   const std::string &footstep, faction_id faction );
//void add_footstep( const tripoint &p, short volume,
//                   const std::string &footstep, mfaction_str_id monsterfaction );

/* Make sure the sounds are all reset when we start a new game. */
void reset_sounds();
/* Clear our floodfill que. */
void clear_floodfill_que( const bool &soundperf );
// Shift bubble-coordinate sound caches after a map shift.
auto shift_sound_positions( const point_rel_ms &offset ) -> void;
/* Reset the sound markers in the player UI so sounds do not blot out everything else. Call this after processing player input, not at the end of every turn. */
void reset_markers();

// Methods for processing sound events
// process_sounds() applies the sounds since the last turn to monster AI
void process_sounds();
// Processes sounds for a given NPC at their tripoint and applies the sounds to the NPC's AI
void process_sounds_npc();
// process_sound_markers applies sound events to the player and records them for display.
void process_sound_markers( Character *who );
// Returns the sound direction index from source to listener.
auto direction_index_to_sound_source( const tripoint_bub_ms &source,
                                      const tripoint_bub_ms &listener ) -> uint8_t;

// Return list of points that have sound events the player can hear.
std::vector<tripoint_bub_ms> get_footstep_markers();
// Return a vector of all sound events not from monsters, and all sound events from monsters.
std::pair< std::vector<tripoint_bub_ms>, std::vector<tripoint_bub_ms>> get_monster_sounds();
// retrieve the sound event(s?) at a location.
std::string sound_at( const tripoint_bub_ms &location );
/** Tells us if sound has been enabled in options */
extern bool sound_enabled;



} // namespace sounds

template<>
struct enum_traits<sounds::sound_t> {
    static constexpr auto last = sounds::sound_t::_LAST;
};

// The base "unit" is the Bel, 10 deciBels to the Bel, 100 centibels to the Bel, 1000 milliBels to the Bel and finially 100 millibels to the deciBel.
static constexpr short dBspl_to_mdBspl_coeff = 100;
static constexpr double mdBspl_to_dBspl_coeff = 0.01;

// Converts decibels sound pressure level to milli-decibels sound pressure level.
// We do this often enough its worth it to have a constexpr even though its just *100
static constexpr short dBspl_to_mdBspl( const short &dB )
{
    return ( dBspl_to_mdBspl_coeff * dB );
}
// Converts milli-decibels sound pressure level to decibels sound pressure level.
static constexpr short mdBspl_to_dBspl( const short &mdB )
{
    return ( mdBspl_to_dBspl_coeff * mdB );
}
// Maximum mdB spl value a sound can have in atmosphere.
static constexpr short MAXIMUM_VOLUME_ATMOSPHERE = 19100;
// Sounds are not valid to be properly flood filled below this threshold.
// The sound will be only flooded to the adjacent tiles if below this threshold, and no further.
// We also take this as our approximate minimum audible volume for filtering purposes.
static constexpr short SOUND_MINIMUM_VOLUME_FOR_PROPAGATION = 2000;
// Volume loss in mdB spl per underground zlevel difference.
// Cache this because we call it every time we check a sound to see if a monster hears it, which adds up quickly.
static constexpr short SOUND_ABSORPTION_PER_ZLEV = 4200;
// The base ambient volume above ground in mdB spl. Called frequently enough to warrant caching, and to avoid magic number usage.
static constexpr short AMBIENT_VOLUME_ABOVEGROUND = 4500;
// The base ambient volume underground in mdB spl. Called frequently enough to warrant caching, and to avoid magic number usage.
static constexpr short AMBIENT_VOLUME_UNDERGROUND = 3500;

// Well made residential walls with sound proofing materials can have transmission loss values of upwards of 63 dB.
// STC ratings (in dB of sound reduction) range from 25 to 55+
// We dont have a good way of differentiating walls, so we take an average of 40dB
// Applies to more than just walls, applies to any terrain with the block_wind flag.
// Only applies when sound is being cast if it has at least two adjacent terrain of equivalent sound absorption, and all have a roof.
// In mdB spl, 100ths of a dB spl
static constexpr short SOUND_ABSORPTION_WALL = 4000;
// This is equivalent to a well designed highway sound barrier. 20dB spl, 2000mdB spl
// If a wind blocking wall does not have a roof, it gets this.
static constexpr short SOUND_ABSORPTION_THICK_BARRIER = 2000;
// This is what sealed connect_to_wall terrain offers. 5dB spl, 500mdB spl
static constexpr short SOUND_ABSORPTION_BARRIER = 500;
// If a block_wind terrain is completely alone, it does nothing to block sound.
// This is the default for most terrain.
// Maybe silly to cache this, but we call this frequently.
static constexpr short SOUND_ABSORPTION_OPEN_FIELD = 0;
// Per tile sound attenuation in mdB spl of light vegitation tiles such as farmland and swamps
static constexpr short SOUND_ABSORPTION_LIGHT_VEGITATION = 6;
// Per tile sound attenuation in mdB spl of forests during the autumn/fall season.
static constexpr short SOUND_ABSORPTION_FOREST_FALL = 9;
// Per tile sound attenuation in mdB spl of forests or other heavy vegitation tiles
static constexpr short SOUND_ABSORPTION_FOREST = 20;
// Per tile sound attenuation bonus in mdB spl provided by snow.
static constexpr short SOUND_ABSORPTION_SNOW_BONUS = 128;

// Use these to tweak sound floodfilling.
// Only here for completeness and characters with super hearing or bionic ears in an anechoic chamber.
static constexpr uint8_t flood_radius_SILENT = 1; // 3x3 flood.
static constexpr uint8_t flood_radius_NEARLY_SILENT = 3;
static constexpr uint8_t flood_radius_QUIET = 4;
static constexpr uint8_t flood_radius_NORMAL = 6;
static constexpr uint8_t flood_radius_LOUD = 8;
static constexpr uint8_t flood_radius_VERY_LOUD = 10;
static constexpr uint8_t flood_radius_DEAFENING = 12; // radius 12 equates to a 25x25 flooded zone.
// in dB spl
static constexpr short vol_threshold_SILENT = 1;
// in dB spl
static constexpr short vol_threshold_NEARLY_SILENT = 20;
// in dB spl
static constexpr short vol_threshold_QUIET = 45;
// in dB spl
static constexpr short vol_threshold_NORMAL = 75;
// in dB spl
static constexpr short vol_threshold_LOUD = 95;
// in dB spl
static constexpr short vol_threshold_VERY_LOUD = 125;
// in dB spl
static constexpr short vol_threshold_DEAFENING = 191;

// The total checked radius is + 1 of its equivalent flood radius.
// We need to check all tiles adjacent to the ones we flood to, hence the widened area.
// We declare these so that we can actually generate a 2d array of the correct size.
// Use checkvar radius when navigating a checkvar envelope, and total checkvar envelope when declaring the size of one.

static constexpr uint8_t total_check_radius_SILENT = flood_radius_SILENT + 1;
static constexpr uint8_t total_check_radius_NEARLY_SILENT = flood_radius_NEARLY_SILENT + 1;
static constexpr uint8_t total_check_radius_QUIET = flood_radius_QUIET + 1;
static constexpr uint8_t total_check_radius_NORMAL = flood_radius_NORMAL + 1;
static constexpr uint8_t total_check_radius_LOUD = flood_radius_LOUD + 1;
static constexpr uint8_t total_check_radius_VERY_LOUD = flood_radius_VERY_LOUD + 1;
static constexpr uint8_t total_check_radius_DEAFENING = flood_radius_DEAFENING + 1;
// The total check envelopes should only be used when defining the size of a checkvars array.
static constexpr uint8_t total_check_envelope_SILENT = ( 1 + ( 2 * total_check_radius_SILENT ) );
static constexpr uint8_t total_check_envelope_NEARLY_SILENT = ( 1 + ( 2 *
        total_check_radius_NEARLY_SILENT ) );
static constexpr uint8_t total_check_envelope_QUIET = ( 1 + ( 2 * total_check_radius_QUIET ) );
static constexpr uint8_t total_check_envelope_NORMAL = ( 1 + ( 2 * total_check_radius_NORMAL ) );
static constexpr uint8_t total_check_envelope_LOUD = ( 1 + ( 2 * total_check_radius_LOUD ) );
static constexpr uint8_t total_check_envelope_VERY_LOUD = ( 1 + ( 2 *
        total_check_radius_VERY_LOUD ) );
static constexpr uint8_t total_check_envelope_DEAFENING = ( 1 + ( 2 *
        total_check_radius_DEAFENING ) );

enum class sound_vol_for_flood_dist : int {
    SILENT,
    NEARLY_SILENT,
    QUIET,
    NORMAL,
    LOUD,
    VERY_LOUD,
    DEAFENING,
    _LAST
};

template<>
struct enum_traits<sound_vol_for_flood_dist> {
    static constexpr auto last = sound_vol_for_flood_dist::_LAST;
};

enum class floodfill_checkvar_envelope_size : uint8_t {
    SILENT = total_check_envelope_SILENT,
    NEARLY_SILENT = total_check_envelope_NEARLY_SILENT,
    QUIET = total_check_envelope_QUIET,
    NORMAL = total_check_envelope_NORMAL,
    LOUD = total_check_envelope_LOUD,
    VERY_LOUD = total_check_envelope_VERY_LOUD,
    DEAFENING = total_check_envelope_DEAFENING,
    _LAST = 28,
};

template<>
struct enum_traits<floodfill_checkvar_envelope_size> {
    static constexpr auto last = floodfill_checkvar_envelope_size::_LAST;
};

static constexpr auto flood_dist_enum_by_index = std::array<sound_vol_for_flood_dist, 7>
{
    {
        sound_vol_for_flood_dist::SILENT,
                                 sound_vol_for_flood_dist::NEARLY_SILENT,
                                 sound_vol_for_flood_dist::QUIET,
                                 sound_vol_for_flood_dist::NORMAL,
                                 sound_vol_for_flood_dist::LOUD,
                                 sound_vol_for_flood_dist::VERY_LOUD,
                                 sound_vol_for_flood_dist::DEAFENING
    }
};

static constexpr uint8_t get_flood_envelope_by_enum( const sound_vol_for_flood_dist &dist_enum )
{
    switch( dist_enum ) {
        case sound_vol_for_flood_dist::SILENT:
            return ( flood_radius_SILENT * 2 ) + 1;
        case sound_vol_for_flood_dist::NEARLY_SILENT:
            return ( flood_radius_NEARLY_SILENT * 2 ) + 1;
        case sound_vol_for_flood_dist::QUIET:
            return ( flood_radius_QUIET * 2 ) + 1;
        case sound_vol_for_flood_dist::NORMAL:
            return ( flood_radius_NORMAL * 2 ) + 1;
        case sound_vol_for_flood_dist::LOUD:
            return ( flood_radius_LOUD * 2 ) + 1;
        case sound_vol_for_flood_dist::VERY_LOUD:
            return ( flood_radius_VERY_LOUD * 2 ) + 1;
        case sound_vol_for_flood_dist::DEAFENING:
            return ( flood_radius_DEAFENING * 2 ) + 1;
        case sound_vol_for_flood_dist::_LAST:
            return ( flood_radius_SILENT * 2 ) + 1;
    }
}

static constexpr auto get_flood_dist_enum( const short &dB_vol )
{
    if( dB_vol > vol_threshold_VERY_LOUD ) {
        return sound_vol_for_flood_dist::DEAFENING;
    } else if( dB_vol > vol_threshold_LOUD ) {
        return sound_vol_for_flood_dist::VERY_LOUD;
    } else if( dB_vol > vol_threshold_NORMAL ) {
        return sound_vol_for_flood_dist::LOUD;
    } else if( dB_vol > vol_threshold_QUIET ) {
        return sound_vol_for_flood_dist::NORMAL;
    } else if( dB_vol > vol_threshold_NEARLY_SILENT ) {
        return sound_vol_for_flood_dist::QUIET;
    } else if( dB_vol > vol_threshold_SILENT ) {
        return sound_vol_for_flood_dist::NEARLY_SILENT;
    } else {
        return sound_vol_for_flood_dist::SILENT;
    }
}

static constexpr auto get_flood_radius_by_enum( const enum sound_vol_for_flood_dist &dist_enum )
{
    switch( dist_enum ) {
        case sound_vol_for_flood_dist::SILENT:
            return flood_radius_SILENT;
        case sound_vol_for_flood_dist::NEARLY_SILENT:
            return flood_radius_NEARLY_SILENT;
        case sound_vol_for_flood_dist::QUIET:
            return flood_radius_QUIET;
        case sound_vol_for_flood_dist::NORMAL:
            return flood_radius_NORMAL;
        case sound_vol_for_flood_dist::LOUD:
            return flood_radius_LOUD;
        case sound_vol_for_flood_dist::VERY_LOUD:
            return flood_radius_VERY_LOUD;
        case sound_vol_for_flood_dist::DEAFENING:
            return flood_radius_DEAFENING;
        case sound_vol_for_flood_dist::_LAST:
            return flood_radius_SILENT;
    }
}

// static constexpr auto flood_dist_by_index = std::array<uint8_t, 7>
// {
//     {flood_radius_SILENT, flood_radius_NEARLY_SILENT, flood_radius_QUIET, flood_radius_NORMAL, flood_radius_LOUD, flood_radius_VERY_LOUD, flood_radius_DEAFENING}
// };

static constexpr auto total_check_radius_by_index = std::array<uint8_t, 7>
{
    {total_check_radius_SILENT, total_check_radius_NEARLY_SILENT, total_check_radius_QUIET, total_check_radius_NORMAL, total_check_radius_LOUD, total_check_radius_VERY_LOUD, total_check_radius_DEAFENING}
};

// static constexpr auto total_check_envelop_by_index = std::array<uint8_t, 7>
// {
//     {total_check_envelope_SILENT, total_check_envelope_NEARLY_SILENT, total_check_envelope_QUIET, total_check_envelope_NORMAL, total_check_envelope_LOUD, total_check_envelope_VERY_LOUD, total_check_envelope_DEAFENING}
// };

static constexpr auto get_total_check_radius_by_enum( const enum sound_vol_for_flood_dist
        &dist_enum )
{
    switch( dist_enum ) {
        case sound_vol_for_flood_dist::SILENT:
            return total_check_radius_SILENT;
        case sound_vol_for_flood_dist::NEARLY_SILENT:
            return total_check_radius_NEARLY_SILENT;
        case sound_vol_for_flood_dist::QUIET:
            return total_check_radius_QUIET;
        case sound_vol_for_flood_dist::NORMAL:
            return total_check_radius_NORMAL;
        case sound_vol_for_flood_dist::LOUD:
            return total_check_radius_LOUD;
        case sound_vol_for_flood_dist::VERY_LOUD:
            return total_check_radius_VERY_LOUD;
        case sound_vol_for_flood_dist::DEAFENING:
            return total_check_radius_DEAFENING;
        case sound_vol_for_flood_dist::_LAST:
            return total_check_radius_SILENT;
    }
}

// static constexpr auto get_total_checkvar_length(const enum sound_vol_for_flood_dist &dist_enum) {
//     switch( dist_enum ) {
//         case sound_vol_for_flood_dist::SILENT:
//             return total_check_envelope_SILENT;
//         case sound_vol_for_flood_dist::NEARLY_SILENT:
//             return total_check_envelope_NEARLY_SILENT;
//         case sound_vol_for_flood_dist::QUIET:
//             return total_check_envelope_QUIET;
//         case sound_vol_for_flood_dist::NORMAL:
//             return total_check_envelope_NORMAL;
//         case sound_vol_for_flood_dist::LOUD:
//             return total_check_envelope_LOUD;
//         case sound_vol_for_flood_dist::VERY_LOUD:
//             return total_check_envelope_VERY_LOUD;
//         case sound_vol_for_flood_dist::DEAFENING:
//             return total_check_envelope_DEAFENING;
//         case sound_vol_for_flood_dist::_LAST:
//             return total_check_envelope_SILENT;
//     }
// }

// static constexpr auto get_total_checkvar_length( const enum floodfill_checkvar_envelope_size &checkvar_enum )
// {
//     switch( checkvar_enum ) {
//         case floodfill_checkvar_envelope_size::SILENT:
//             return total_check_radius_SILENT;
//         case floodfill_checkvar_envelope_size::NEARLY_SILENT:
//             return total_check_radius_NEARLY_SILENT;
//         case floodfill_checkvar_envelope_size::QUIET:
//             return total_check_radius_QUIET;
//         case floodfill_checkvar_envelope_size::NORMAL:
//             return total_check_radius_NORMAL;
//         case floodfill_checkvar_envelope_size::LOUD:
//             return total_check_radius_LOUD;
//         case floodfill_checkvar_envelope_size::VERY_LOUD:
//             return total_check_radius_VERY_LOUD;
//         case floodfill_checkvar_envelope_size::DEAFENING:
//             return total_check_radius_DEAFENING;
//         case floodfill_checkvar_envelope_size::_LAST:
//             return total_check_radius_SILENT;
//     }
// }

//static constexpr auto dummy_floodfill_checkvars_SILENT = std::array<std::array<std::bitset<8>, total_check_envelope_SILENT>, total_check_envelope_SILENT>{{{0}}};
//static constexpr auto dummy_floodfill_checkvars_NEARLY_SILENT = std::array<std::array<std::bitset<8>, total_check_envelope_NEARLY_SILENT>, total_check_envelope_NEARLY_SILENT>{{{0}}};
//static constexpr auto dummy_floodfill_checkvars_QUIET = std::array<std::array<std::bitset<8>, total_check_envelope_QUIET>, total_check_envelope_QUIET>{{{0}}};
//static constexpr auto dummy_floodfill_checkvars_NORMAL = std::array<std::array<std::bitset<8>, total_check_envelope_NORMAL>, total_check_envelope_NORMAL>{{{0}}};
//static constexpr auto dummy_floodfill_checkvars_LOUD = std::array<std::array<std::bitset<8>, total_check_envelope_LOUD>, total_check_envelope_LOUD>{{{0}}};
//static constexpr auto dummy_floodfill_checkvars_VERY_LOUD = std::array<std::array<std::bitset<8>, total_check_envelope_VERY_LOUD>, total_check_envelope_VERY_LOUD>{{{0}}};
//static constexpr auto dummy_floodfill_checkvars_DEAFENING = std::array<std::array<std::bitset<8>, total_check_envelope_DEAFENING>, total_check_envelope_DEAFENING>{{{0}}};
//template <typename T, std::size_t Row, std::size_t Col>
//using tot_checkvar_env = std::array<std::array<T,Col>, Row>;


namespace sfx
{
//Channel assignments:
enum class channel : int {
    any = -1,                   //Finds the first available channel
    daytime_outdoors_env = 0,
    nighttime_outdoors_env,
    underground_env,
    indoors_env,
    indoors_rain_env,
    outdoors_snow_env,
    outdoors_flurry_env,
    outdoors_thunderstorm_env,
    outdoors_rain_env,
    outdoors_drizzle_env,
    outdoor_blizzard,
    deafness_tone,
    danger_extreme_theme,
    danger_high_theme,
    danger_medium_theme,
    danger_low_theme,
    stamina_75,
    stamina_50,
    stamina_35,
    idle_chainsaw,
    chainsaw_theme,
    player_activities,
    exterior_engine_sound,
    interior_engine_sound,
    radio,
    MAX_CHANNEL                 //the last reserved channel
};

//Group Assignments:
enum class group : int {
    weather = 1,    //SFX related to weather
    time_of_day,    //SFX related to time of day
    context_themes, //SFX related to context themes
    fatigue         //SFX related to fatigue
};

void load_sound_effects( const JsonObject &jsobj );
void load_sound_effect_preload( const JsonObject &jsobj );
void load_playlist( const JsonObject &jsobj );
void play_variant_sound( const std::string &id, const std::string &variant, int volume,
                         units::angle angle, double pitch_min = -1.0, double pitch_max = -1.0 );
void play_variant_sound( const std::string &id, const std::string &variant, int volume );
void play_ambient_variant_sound( const std::string &id, const std::string &variant, int volume,
                                 channel channel, int fade_in_duration, double pitch = -1.0, int loops = -1 );
void play_activity_sound( const std::string &id, const std::string &variant, int volume );
void end_activity_sounds();
void generate_gun_sound( const tripoint_bub_ms &source, const item &firing,
                         const short &origin_vol );
void generate_melee_sound( const tripoint_bub_ms &source, const tripoint_bub_ms &target, bool hit,
                           bool targ_mon = false, const std::string &material = "flesh" );
void do_hearing_loss( int turns = -1 );
void remove_hearing_loss();
void do_projectile_hit( const Creature &target );
int get_heard_volume( const tripoint_bub_ms &source, const short &origin_volume,
                      const bool &in_mdB = false );
units::angle get_heard_angle( const tripoint_bub_ms &source );
void do_footstep();
void do_danger_music();
void do_ambient();
void do_vehicle_engine_sfx();
void do_vehicle_exterior_engine_sfx();
void fade_audio_group( group group, int duration );
void fade_audio_channel( channel channel, int duration );
bool is_channel_playing( channel channel );
bool has_variant_sound( const std::string &id, const std::string &variant );
void stop_sound_effect_fade( channel channel, int duration );
void stop_sound_effect_timed( channel channel, int time );
int set_channel_volume( channel channel, int volume );
void set_channel_3d_position( channel channel, units::angle angle, int distance = 0 );
int get_channel_volume( channel channel );
void do_player_death_hurt( const player &target, bool death );
void do_fatigue();
// @param obst should be string id of obstacle terrain or vehicle part
void do_obstacle( const std::string &obst = "" );
} // namespace sfx

template<>
struct enum_traits<sfx::channel> {
    static constexpr auto last = sfx::channel::MAX_CHANNEL;
};
/**
* sound_event to pass to the sounds::sound() to flood fill out the sound or pass ambient noise to the player.
* How loud a sound is at the source tile (or how loud an ambient sound is), in Decibels Sound Pressure Level (dB spl, or dB) at a reference distance of 1 meter from the sound source.
* Valid input is 0-191. Volumes louder than 191dB will be reduced to 191dB.
* See the wall of text below for more context, physics and proper usage if desired.
* @param volume
* What is the tripoint position of the sound source?
* @param origin
* What enum sound category is this?
* @param category
* String description of the sound.
* @param description
* Is this sound from movement?
* @param movement_noise
* If all three from_xxxx bools are false, a sound qualifies to be ambient noise.
* Did the player make this noise?
* @param from_player
* Did a monster make this noise?
* @param from_monster
* Did an NPC make this noise?
* @param from_npc
* ID and variant are used to select what SFX audio to play
* @param id
* @param variant
* faction and monfaction are the source creature's NPC and monster factions respectively. Do not set for sounds that are not from a creature.
* @param faction
* @param monfaction
*/
struct sound_event {
    // How loud a sound is at a reference distance of 1 meter (or how loud an ambient sound is), in Decibels Sound Pressure Level (dB spl, or just dB from now on), 0 - 191
    // In real life if the volume of a sound is expressed in dB, it almost certainly is refering to dB spl or its a speaker manufacturer trying to impress with similarly named but unapplicable units.
    //
    // 0 dB spl reference is 2x10^-5 Pascals, which is an accademic and industry standard. This is the threshold of human hearing at 1kHz.
    // As a general rule, 0-40 dB is almost perfectly silent to very quiet,
    // 40-60 is quiet,
    // 60-80 dB is noisy,
    // 100 dB is very noisy,
    // 120 dB is intolerable and is the low threshold for instantaneous hearing loss and pain,
    // 140 dB is the high threshold for pain,
    // 150 dB is garunteed temporary hearing loss,
    // 160 dB is a general ballpark for how loud unsuppressed gunfire is for the shooter,
    // 180+ dB will start to knock humans unconscious and cause injury.
    // Above 191 dB a pressure wave is a supersonic shockwave, and does not get to be a "sound wave" until it ceases being supersonic.
    // Outside of good conditions humans generally will not notice sounds below 20 dB. The ambient noise level of a quiet room is around 40 dB, a quiet street is around 50 dB.
    // For a more detailed example list, see https://www.engineeringtoolbox.com/sound-pressure-d_711.html
    //
    // Decibels are a relative unit of measurment that expresses the ratio of two values of some quantity on a logarithmic scale.
    // By itself it is unitless, since it is just a ratio. When expressing something in Decibels, what is being compared must be stated as there are dozens of different units that use Decibels.
    // It is used because there is such a vast range of technically audible sounds, 0.00002 Pa to above 101000 Pa
    // dB spl can be used to find the pressure reduction between two distances given a reference distance. A dB spl measurment is meaningless without a reference distance.
    // In game we take the base reference distance for all dB spl measurments to be 1 meter.
    // Sound wave pressure loss over distance follows the inverse square law.
    // Taking 1 tile to be ~1 meters, in an open field 10 tiles distance reduces received dB by 20, 100 tiles distance reduces dB by 40, etc.
    // Adding dB spl has to be done by the root mean square method (100dB + 100dB + 100 dB = ~104.7 dB spl) as it is itself a rms value of pressure fluctuations.
    // Adding several dB values together results in something usually almost identical to the largest source, (100 dB + 100 dB + 113 dB = 114.3 dB)
    // so we will generally just take the largest volume if its 10 greater than other sounds.
    //
    // Unfortunantly there are three different accoustic quantities that are expressed in a Decibel ratio.
    // Each is related to eachother but are very different quantities and are mistaken for one another in almost all non-engineering or non-accademic contexts.
    // To make it even worse its still not uncommon to find mix ups in technical sources or to just have all three called dB and the authors expect you to just know which one they mean.
    // It is very common for audio electronics manufacturers especially headset or microphone manufacturers to just call all three properties sound intensity.
    // Saying someone probably got something wrong is not really an insult here, its just a fact of life with accoustics because of how poorly communicated so much information is.
    // (Be very, very careful second guessing musicians/sound techs however.)
    //
    // *Sound Pressure Level* (sound pressure, dB spl) is the rms (root mean square) pressure deviation due to a pressure wave (sound wave) from a reference pressure at some specific point.
    //      Proper unit is Pascals. This is what we are using in game. Unless specified, all dB units in game are dB spl. dB spl = 10*Log10(P^2/Pref^2) = 20*Log10(P/Pref).
    //      When discussing pressure levels in accoustics unless someone explicitly says they are referencing the peak amplitude of a sound wave, what they are refering to is the sound pressure level.
    //      Sound Pressure level is a root means square value, and is the effective pressure of that sinusoidal pressure wave at some point as it flip flops from positive to negative amplitude upwards of a couple thousand times a second.
    //      Almost all reference dB values for sounds are in sound pressure level, health regulations/medical figures, and this is what is measured by a point sensor like microphones or decibel meters.
    //      Doubling the pressure of a sound wave increases the dB value by 6. A difference of 60 dB is a 1024x increase in pressure!
    //      Maximum dB spl in air is taken at 191 dB, the maximum peak (not rms) dB is 194.
    //      Pressures of a higher dB rating are not sound pressure waves, they are supersonic blast/shock waves and should be modeled as damaging explosions. 195dB will always rupture human eardrums. 200dB+ tends to be fatal from pressure alone.
    //
    //      If a source says that the sound pressure doubles every 3 dB, they gotten something wrong and have likely conflated sound pressure level with sound intensity level.
    //      This is probably the second most common mistake when discussing sound/accoustics, second only to not listing reference distance for dB spl measurments. dB spl measurments are meaningless without a reference distance.
    //
    // Sound Power Level (dB swl) is a the rate at which sound energy changes with time across some surface, and is effectively a vector quantity. This is not perceptible.
    //      Proper unit is in Watts. dB swl = 10*Log10(W/Wref)
    //      Very useful from an engineering/physics standpoint and still useful to the end user, as it only depends on the noise source and is independant of the acoustic enviornment.
    //      But we dont really care about that here, and dont need to deal with all of the extra math and calcs that we would have to add.
    //
    // Sound Intensity Level (dB sil) is the power carried by sound waves per unit area in a direction perpendicular to some area. This is not perceptible.
    //      Proper unit is Watts/meter^2 (I). dB sil = 10*Log10(I/Iref) This is a field quantity and we are not trying to simulate the universe.
    //      Sound intensity doubles for every 3 dB sil.
    //      This value cannot really be directly measured, and is only really useful from an engineering/physics standpoint or to professional audio technicians.
    //      It is very common for electronics manufacturers to either call dB spl metrics this since it sounds cooler, or to substitute the numbers they got in a test chamber for dB spl metrics.
    //
    // We are in control of reality, and only really care about the perceived sound of creatures at explicit points in time at explicit distances which we control, so we use sound pressure level.
    // Thank you for attending my T.E.D. talk.
    short volume = 0;

    // What is the position of the sound source?
    tripoint_bub_ms origin;

    // What enum sound category is this?
    sounds::sound_t category = sounds::sound_t::background;

    // String description of the sound.
    std::string description = std::string( "" );

    // Is this sound from movement?
    bool movement_noise = false;

    // If all three froms are false, its ambient noise. Only one should be true.
    // Did the player make this noise?
    bool from_player = false;

    // Did a monster make this noise?
    bool from_monster = false;

    // Did an NPC make this noise?
    bool from_npc = false;

    // This stuff is for selecting actual sfx to play through an audio device in THE REAL WORLD. (spooky)
    std::string id = std::string( "" );
    std::string variant = std::string( "default" );
    // NPC and monster faction of the source creature, if applicable. Do not set for sounds not from creatures.
    faction_id faction = faction_id( "no_faction" );
    mfaction_str_id monfaction = mfaction_str_id( "" );
};


// Volume loss for moving to a new distance, in 100ths of dB (mdB). Nominal calc is 100 * (20 * Log10( dist / ( dist - 1 )))
// dist_vol_loss[2] provides the dB loss moving from 1m to 2m, dist_vol_loss[5] provides the dB loss moving from 4m to 5m, etc.
// Technically we should not have a value for going from 0m to 1m, but we might ask for it with how sound flood filling is handled around corners etc.
// Mathmatically it should theoretically be somewhere around 20dB, though the rules for calcing sound pressure break down at very small distances
// And finding the actual answer is the realm of neat pressure calc tricks or L'Hopital's shenanagins. Just take 15dB.
//
// Yes, this does make the assumption that 1 tile is 1 meter.
//
// Store this so we dont have to calc distance loss every time we floodfill a tile for sound.
// These values will be used very frequently, probably a couple hundred times per sound cast for anything but very quiet sounds.
// Doing the calc out every time for those would bog things down.
// With this, we should be able to do everything with addition/subtraction.
// Distance 1 only happens at the source of a sound, i.e., the reference volume.
static constexpr auto dist_vol_loss = std::array<short, 122>
{
    0, 1500, 602, 352, 250, 194, 158, 134, 116, 102, 92, 83, 76, 70, 64, 60, 56, 53, 50, 47, 45, 42, 40,
    39, 37, 35, 34, 33, 32, 30, 29, 28, 28, 27, 26, 25, 24, 24, 23, 23, 22, 21, 21, 20, 20, 20, 19, 19,
    18, 18, 18, 17, 17, 17, 16, 16, 16, 15, 15, 15, 15, 14, 14, 14, 14, 13, 13, 13, 13, 13, 12, 12, 12,
    12, 12, 12, 12, 11, 11, 11, 11, 11, 11, 11, 10, 10, 10, 10, 10, 10, 10, 10, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7
};

// Provides an array of the bubble points adjacent to a point.
// The index of an adjacent tile is used as the direction index to that tile.
// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ @ ] [ 3 ]
// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
static constexpr std::array<point_bub_ms, 8> get_adjacent_tiles( const point_bub_ms &p )
{
    return std::array<point_bub_ms, 8> { {
            p + point_rel_ms::north_west(), // Direction 0
            p + point_rel_ms::north(),      // Direction 1
            p + point_rel_ms::north_east(), // Direction 2
            p + point_rel_ms::east(),       // Direction 3
            p + point_rel_ms::south_east(), // Direction 4
            p + point_rel_ms::south(),      // Direction 5
            p + point_rel_ms::south_west(), // Direction 6
            p + point_rel_ms::west()        // Direction 7
        }
    };
}

// Use this overload sparingly, it is preferable to
// Provides an array of the bubble points adjacent to some submap point.
// The index of an adjacent tile is used as the direction index to that tile.
// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ @ ] [ 3 ]
// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
// static constexpr std::array<point_bub_ms, 8> get_adjacent_tiles( const point_sm_ms &sp,
//         const point_bub_sm &grid_pos )
// {
//     point_bub_ms bp = project_combine( grid_pos, sp );
//     const auto array = std::array<point_bub_ms, 8> { {
//             bp + point_rel_ms::north_west(), // Direction 0
//             bp + point_rel_ms::north(),      // Direction 1
//             bp + point_rel_ms::north_east(), // Direction 2
//             bp + point_rel_ms::east(),       // Direction 3
//             bp + point_rel_ms::south_east(), // Direction 4
//             bp + point_rel_ms::south(),      // Direction 5
//             bp + point_rel_ms::south_west(), // Direction 6
//             bp + point_rel_ms::west()        // Direction 7
//         }
//     };
//     return array;
// }

// Provides an array of the tripoints adjacent to some bubble tripoint.
// The index of an adjacent tile is used as the direction index to that tile.
// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ @ ] [ 3 ]
// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
static constexpr std::array<tripoint_bub_ms, 8> get_adjacent_tripoints( const tripoint_bub_ms &p )
{
    return std::array<tripoint_bub_ms, 8> { {
            p + tripoint_rel_ms::north_west(), // Direction 0
            p + tripoint_rel_ms::north(),      // Direction 1
            p + tripoint_rel_ms::north_east(), // Direction 2
            p + tripoint_rel_ms::east(),       // Direction 3
            p + tripoint_rel_ms::south_east(), // Direction 4
            p + tripoint_rel_ms::south(),      // Direction 5
            p + tripoint_rel_ms::south_west(), // Direction 6
            p + tripoint_rel_ms::west()        // Direction 7
        }
    };
}

// Provides an array of the bubble tripoints adjacent to some submap point.
// The index of an adjacent tile is used as the direction index to that tile.
// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ @ ] [ 3 ]
// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
// static constexpr std::array<tripoint_bub_ms, 8> get_adjacent_tripoints( const point_sm_ms &sp,
//         const tripoint_bub_sm &grid_pos )
// {
//     tripoint_bub_ms btri = project_combine( grid_pos, sp );
//     const auto array = std::array<tripoint_bub_ms, 8> { {
//             btri + tripoint_rel_ms::north_west(), // Direction 0
//             btri + tripoint_rel_ms::north(),      // Direction 1
//             btri + tripoint_rel_ms::north_east(), // Direction 2
//             btri + tripoint_rel_ms::east(),       // Direction 3
//             btri + tripoint_rel_ms::south_east(), // Direction 4
//             btri + tripoint_rel_ms::south(),      // Direction 5
//             btri + tripoint_rel_ms::south_west(), // Direction 6
//             btri + tripoint_rel_ms::west()        // Direction 7
//         }
//     };
//     return array;
// }

// Provides an array of the bubble tripoints adjacent to some submap point.
// The index of an adjacent tile is used as the direction index to that tile.
// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ @ ] [ 3 ]
// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
// Sorry (not sorry at all) for making this abomination of out of bounds memory access requests.
// This is the preferd array getter to use when working through a submap for the absorption cache.
// Sanitize your inputs.
static constexpr std::array<point_sm_ms, 8> get_adjacent_submap_points( const point_sm_ms &sp )
{

    return std::array<point_sm_ms, 8> { {
            sp + point_rel_ms::north_west(), // Direction 0
            sp + point_rel_ms::north(),      // Direction 1
            sp + point_rel_ms::north_east(), // Direction 2
            sp + point_rel_ms::east(),       // Direction 3
            sp + point_rel_ms::south_east(), // Direction 4
            sp + point_rel_ms::south(),      // Direction 5
            sp + point_rel_ms::south_west(), // Direction 6
            sp + point_rel_ms::west()
        }
    };   // Direction 7
}

// All of these values are kept as static constexprs
// So that we can use compiler optimized and pre-cached results when using sound logic.
// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]    [ SDI_NW ] [ SDI_N ] [ SDI_NE ]
// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [8/9] [ 3 ] =  [ SDI_W  ] [SDI_U/D] [ SDI_E  ]
// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]    [ SDI_SW ] [ SDI_S ] [ SDI_SE ]

// Sound Direction Index, North West
static constexpr uint8_t SDI_NW     = 0;
// Sound Direction Index, North
static constexpr uint8_t SDI_N      = 1;
// Sound Direction Index, North East
static constexpr uint8_t SDI_NE     = 2;
// Sound Direction Index, East
static constexpr uint8_t SDI_E      = 3;
// Sound Direction Index, South East
static constexpr uint8_t SDI_SE     = 4;
// Sound Direction Index, South
static constexpr uint8_t SDI_S      = 5;
// Sound Direction Index, South West
static constexpr uint8_t SDI_SW     = 6;
// Sound Direction Index, West
static constexpr uint8_t SDI_W      = 7;
// Sound Direction Index, Down -Z
static constexpr uint8_t SDI_DOWN   = 8;
// Sound Direction Index, Up +Z
static constexpr uint8_t SDI_UP     = 9;

static constexpr auto sanitized_sound_direction_indexes_full = std::array<uint8_t, 10>
{
    {SDI_NW, SDI_N, SDI_NE, SDI_E, SDI_SE, SDI_S, SDI_SW, SDI_W, SDI_DOWN, SDI_UP}
};

static constexpr auto sanitized_sound_direction_indexes = std::array<uint8_t, 8>
{
    {SDI_NW, SDI_N, SDI_NE, SDI_E, SDI_SE, SDI_S, SDI_SW, SDI_W}
};
// Provides the cartesion sound direction indexes in clockwise order starting from zero.
static constexpr auto sanitized_sound_direction_indexes_cartesian = std::array<uint8_t, 4>
{
    { SDI_N, SDI_E, SDI_S, SDI_W }
};
// Provides the diagonal sound direction indexes in clockwise order starting from zero.
static constexpr auto sanitized_sound_direction_indexes_diagonal = std::array<uint8_t, 4>
{
    { SDI_NW, SDI_NE, SDI_SE, SDI_SW }
};

// Return the static constexpr version of given direction index.
// Useful for sanitizing index inputs to use faster logic.
static constexpr auto get_sound_direction_index( const uint8_t &dir )
{
    if( dir > SDI_UP ) {
        return SDI_UP;
    } else {
        return sanitized_sound_direction_indexes_full[dir];
    }
}

// Given a source tile's sound direction as the index, provides an array of which directions are valid to propagate sound to.
// Member entries a listed in clockwise order. First and last members are subject to a distance penalty.
// Direction index and adjacent tile index are set such that direction 0 refers to adjacent tile 0, etc.
// If a tile's direction is set to 8 for any reason, it is invalid to propagate to.
// Store this as we iterate through it every time we check a tile's neighboors for update, for potentially hundreds of tiles per sound.
// We do additional map checking to check for corner invalidation due to walls, but having this stored still helps speed things up.
// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ 8 ] [ 3 ]
// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
static constexpr auto spropagation_tiles_by_sdirection = std::array<std::array<uint8_t, 5>, 8 >
{
    { {SDI_SW, SDI_W, SDI_NW, SDI_N, SDI_NE},   //Direction "0"
        {SDI_W, SDI_NW, SDI_N, SDI_NE, SDI_E},  //Direction "1"
        {SDI_NW, SDI_N, SDI_NE, SDI_E, SDI_SE}, //Direction "2"
        {SDI_N, SDI_NE, SDI_E, SDI_SE, SDI_S},  //Direction "3"
        {SDI_NE, SDI_E, SDI_SE, SDI_S, SDI_SW}, //Direction "4"
        {SDI_E, SDI_SE, SDI_S, SDI_SW, SDI_W},  //Direction "5"
        {SDI_SE, SDI_S, SDI_SW, SDI_W, SDI_NW}, //Direction "6"
        {SDI_S, SDI_SW, SDI_W, SDI_NW, SDI_N}   //Direction "7"
    }
};

// Given a direction index, provides which adjacent tile indexes to check for walls.
// Only odd direction/adjacent tile indexes are checked for walls, walls directly in the path of travel are not checked for.
// This means that only two walls are checked for per direction. Walls prevent diagonal sound propagation.
// Member entries a listed in clockwise order. Direction index and adjacent tile index are set such that direction 0 refers to adjacent tile 0, etc.
// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ 8 ] [ 3 ]
// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
static constexpr auto wall_check_by_sdirection = std::array<std::pair<uint8_t, uint8_t >, 8>
{
    { {SDI_W, SDI_N},   //Direction "0"
        {SDI_W, SDI_E}, //Direction "1"
        {SDI_N, SDI_E}, //Direction "2"
        {SDI_N, SDI_S}, //Direction "3"
        {SDI_E, SDI_S}, //Direction "4"
        {SDI_E, SDI_W}, //Direction "5"
        {SDI_S, SDI_W}, //Direction "6"
        {SDI_S, SDI_N}  //Direction "7"
    }
};
// Conversely, A wall at the given adjacent tile/direction index invalidates propagation through the listed direction index
// Only cardinal direction walls apply this behavior, but we keep the array the same size for compatability.
// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ 8 ] [ 3 ]
// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
// This is also used when building out the absorption and sound wall caches.
static constexpr auto wall_sdir_invalidation = std::array<std::pair<uint8_t, uint8_t >, 8>
{
    { {SDI_W, SDI_N},       //Direction "0"
        {SDI_NW, SDI_NE},   //Direction "1"
        {SDI_N, SDI_E},     //Direction "2"
        {SDI_NE, SDI_SE},   //Direction "3"
        {SDI_E, SDI_S},     //Direction "4"
        {SDI_SE, SDI_SW},   //Direction "5"
        {SDI_S, SDI_W},     //Direction "6"
        {SDI_SW, SDI_NW}    //Direction "7"
    }
};

// A simple array that provides the opposite tile index by sound direction index
// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ 8 ] [ 3 ]
// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
static constexpr auto opposite_tile_by_sdir = std::array<uint8_t, 8>
{
    {
        SDI_SE, //Direction "0"
        SDI_S,  //Direction "1"
        SDI_SW, //Direction "2"
        SDI_W,  //Direction "3"
        SDI_NW, //Direction "4"
        SDI_N,  //Direction "5"
        SDI_NE, //Direction "6"
        SDI_E   //Direction "7"
    }
};
// We only actually check this on cardinal directions
// Provides an array of 3 sdir indexes to check for a roof clockwise of a given direction index.
static constexpr auto roof_to_check_by_sdir = std::array<std::array<uint8_t, 3>, 8>
{
    {
        {SDI_N, SDI_NE, SDI_E},     //Direction "0"
        {SDI_NE, SDI_E, SDI_SE},    //Direction "1"
        {SDI_E, SDI_SE, SDI_S},     //Direction "2"
        {SDI_SE, SDI_S, SDI_SW},    //Direction "3"
        {SDI_S, SDI_SW, SDI_W},     //Direction "4"
        {SDI_SW, SDI_W, SDI_NW},    //Direction "5"
        {SDI_W, SDI_NW, SDI_N},     //Direction "6"
        {SDI_NW, SDI_N, SDI_NE}     //Direction "7"
    }
};

// We always take our base volume readings for sounds at 1m, even if we are in the same tile.
// Calcing the volume very close to a sound source is the perview of sound stage technicians and physicists.
static constexpr uint8_t MINIMUM_DISTANCE_FOR_SOUND_PROPAGATION = 1;
// We have entries for our dist_vol_loss[] array out to an index of 121
// Stop one short of it for safeties sake.
// The rate of change of volume past this distance is only significant at kilometer distances, so we dont care.
static constexpr uint8_t MAXIMUM_DISTANCE_FOR_SOUND_PROPAGATION = 120;

// used to sanitize dist_for_vol_loss[] requests so we dont get out of bound memorry access segfaults.
static constexpr uint8_t get_distance_for_volume_loss( const uint8_t &tile_distance,
        const bool &propagating_perpendicular )
{
    if( tile_distance < MINIMUM_DISTANCE_FOR_SOUND_PROPAGATION ) {
        return MINIMUM_DISTANCE_FOR_SOUND_PROPAGATION;
    } else if( tile_distance > MAXIMUM_DISTANCE_FOR_SOUND_PROPAGATION ) {
        return MAXIMUM_DISTANCE_FOR_SOUND_PROPAGATION;
    } else {
        return tile_distance + ( propagating_perpendicular ? -1 : 1 );
    }
}

/** Returns cumulative volume lost over some distance in mdB SPL.  For use getting the heard volume of a sound outside of its floodfill radius.
    For dB, dL = 20 * Log(R1/R2). For use with mdB we just multiply by 2000 instead which is conveniently our minvol for sound propagation constant.
    Tiles are taken at 1 meter. Will give gibberish answers if a non-tile distance is provided.
    @param dist1: Direct distance from the source to the point where we have our volume measurment. Almost always our floodfill radius.
    @param dist2: Direct distance between the listener and the origin of the sound.
    @param average_terrain_absorption: Average terrain sound absorption of the terrain between the sound source and the listener in mdB spl.
*/
static constexpr short get_cumulative_vol_dist_loss( const int &dist1, const int &dist2,
        const short &t_absorp )
{
    // One of our potential problem cases.
    if( dist1 == dist2 ) {
        return 0;
    }
    const int result = ( std::floor( SOUND_MINIMUM_VOLUME_FOR_PROPAGATION * log10( static_cast<float>
                                     ( dist2 ) / static_cast<float>( dist1 ) ) ) + ( ( dist2 > dist1 ) ? ( (
                                             dist2 - dist1 ) * t_absorp ) : 0 ) );
    return std::min( static_cast<int>( MAXIMUM_VOLUME_ATMOSPHERE ), result );

}

/** Returns an int tile distance before the volume in question drops to out minimum volume for propagation, or a seperate optional volume. Will approximate for terrain sound absorption if given a value.
*    @param dist1: tile distance for our guidline measurment. Usually flood radius.
*    @param vol1: mdB spl volume that is being used as the basis for the estimate.
*    @param t_absorp: mdB spl sound absorption of the terrain. Defaults to zero, use a non zero value if you want to approximate for terrain absorption.
*    @param vol2: This defaults to the minimum volume for sound propagation, 20dB spl, as a guestimate cutoff threshold
*
*    If we ever get a dist2 that is less than dist1, we just return dist1.
*/
static constexpr int average_minvol_distance( const int &dist1, const short &vol1,
        const short t_absorp = 0, const short &vol2 = SOUND_MINIMUM_VOLUME_FOR_PROPAGATION )
{
    // Deal with our undefined behavior.
    // if vol2 is greater than vol1, that is a very good indication that our sound failed to propagate to the boundary of its envelope.
    // Just return the envelope radius.
    if( vol2 >= vol1 ) {
        return dist1 + 3;
    }

    int delta_vol_req = vol1 - vol2;
    int approx_dist = dist1;
    uint8_t check_dist = std::min( 120, dist1 );
    while( delta_vol_req > 0 && approx_dist < 250 ) {
        approx_dist++;
        check_dist = get_distance_for_volume_loss( check_dist, false );
        delta_vol_req -= ( dist_vol_loss[check_dist] + t_absorp );
    }
    return approx_dist;
}
/** Returns an approximated source volume in dB or mdB given a legacy volume in tile distance.
*   @param legacy_dist: The legacy volume in tile distance that we want to approximate
*   @param return_mdB: Do we want our answers back in mdB instead of dB.
*    We make the wild assumption that the legacy distance is to an approximate minvol of 20dB.
*/
static constexpr short approximate_dB_volume_from_legacy_tile_distance_vol( const int &legacy_dist,
        const bool &return_mdB = false )
{
    // All sounds are taken at a base measurment distance of 1 meter, and tile distances are taken at one meter.
    // If different distance assumptions are desired, the math used to make these functions is in the various function comments.
    // If our desired distance is 0 or 1, just return 20dB.
    if( legacy_dist <= MINIMUM_DISTANCE_FOR_SOUND_PROPAGATION ) {
        return ( return_mdB ) ? SOUND_MINIMUM_VOLUME_FOR_PROPAGATION : mdBspl_to_dBspl(
                   SOUND_MINIMUM_VOLUME_FOR_PROPAGATION ); // 20dB is quiet enough that the only people who MIGHT hear it are the people in that tile.
    }
    // So long as our desired minvol distance is greater than one, we can just step through the dist_vol_loss table adding volume instead of subtracting it until we have our desired distance.
    // To account for some potential terrain absorption from shrubbery, add 1 free tile distance to the volume calc for every multiple of 12 legacy distance.
    // We can safely divide because we have already dealt with a potential legacy_dist of zero.
    const int total_dist = legacy_dist + legacy_dist / 12;
    // We want a vol incase we somehow run over what a short can hold. Somehow.
    // We are working in mdB spl, will need to convert out of this if we are asked for dB.
    int approx_vol = SOUND_MINIMUM_VOLUME_FOR_PROPAGATION;
    uint8_t check_dist = MINIMUM_DISTANCE_FOR_SOUND_PROPAGATION;
    int approx_dist = MINIMUM_DISTANCE_FOR_SOUND_PROPAGATION;

    while( approx_dist < total_dist ) {
        approx_dist++;
        check_dist = get_distance_for_volume_loss( check_dist, false );
        approx_vol += dist_vol_loss[check_dist];
    }
    approx_vol = std::min( static_cast<int>( MAXIMUM_VOLUME_ATMOSPHERE ), approx_vol );
    return ( return_mdB ) ? approx_vol : mdBspl_to_dBspl( approx_vol );
}
