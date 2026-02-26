#pragma once

#include <string>
#include <utility>
#include <vector>
#include "type_id.h"
#include "point.h"

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
struct tripoint;
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

// Methods for recording sound events.
/**
 * Sound at position (p) of intensity (vol) in dB spl measured 1 meter from the sound source
 *
 * If the description parameter is a non-empty string, then a string message about the
 * sound is generated for the player.
 *
 * @param p position of sound.
 * @param vol Volume of sound (dB spl).
 * @param category general type of sound for faster parsing
 * @param description Description of the sound for the player
 * @param bool is this sound from movement?
 * @param bool is this sound from the player?
 * @param bool is this sound from a monster?
 * @param bool is this sound from a NPC?
 * @param id Id of sound effect
 * @param variant Variant of sound effect given in id
 * @param string_id of the faction_id
 * @param string_id of the mfaction_str_id
 */
//void sound( const tripoint &p, short vol, sound_t category, const std::string &description,
//            bool movement_noise = false, bool from_player = false, bool from_monster = false,
//            bool from_npc = false, const std::string &id = "",
//            const std::string &variant = "default", const faction_id faction = faction_id( "no_faction" ),
//            const mfaction_str_id monfaction = mfaction_str_id( "" ) );
//void sound( const tripoint &p, short vol, sound_t category, const translation &description,
//            bool movement_noise = false, bool from_player = false, bool from_monster = false,
//            bool from_npc = false, const std::string &id = "",
//            const std::string &variant = "default", const faction_id faction = faction_id( "no_faction" ),
//            const mfaction_str_id monfaction = mfaction_str_id( "" ) );

void sound( const sound_event &soundevent );

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
void reset_markers();

// Methods for processing sound events
// process_sounds() applies the sounds since the last turn to monster AI
void process_sounds();
// Processes sounds for a given NPC at their tripoint and applies the sounds to the NPC's AI
void process_sounds_npc();
// process_sound_markers applies sound events to the player and records them for display.
void process_sound_markers( Character *who );

// Return list of points that have sound events the player can hear.
std::vector<tripoint> get_footstep_markers();
// Return a vector of all sound events not from monsters, and all sound events from monsters.
std::pair< std::vector<tripoint>, std::vector<tripoint>> get_monster_sounds();
// retrieve the sound event(s?) at a location.
std::string sound_at( const tripoint &location );
/** Tells us if sound has been enabled in options */
extern bool sound_enabled;
} // namespace sounds

template<>
struct enum_traits<sounds::sound_t> {
    static constexpr auto last = sounds::sound_t::_LAST;
};

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
void generate_gun_sound( const tripoint &source, const item &firing );
void generate_melee_sound( const tripoint &source, const tripoint &target, bool hit,
                           bool targ_mon = false, const std::string &material = "flesh" );
void do_hearing_loss( int turns = -1 );
void remove_hearing_loss();
void do_projectile_hit( const Creature &target );
int get_heard_volume( const tripoint &source );
units::angle get_heard_angle( const tripoint &source );
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
void do_player_death_hurt( const player &target, bool death );
void do_fatigue();
// @param obst should be string id of obstacle terrain or vehicle part
void do_obstacle( const std::string &obst = "" );
} // namespace sfx

template<>
struct enum_traits<sfx::channel> {
    static constexpr auto last = sfx::channel::MAX_CHANNEL;
};

struct sound_event {
    // How loud a sound is at 1 meter away (or how loud an ambient sound is), in Decibels Sound Pressure Level (dB spl, or just dB from now on), 0 - 191
    //
    // As a general rule, 0-40 dB is very quiet to quiet, 60-80 dB is noisy, 100 dB is very noisy,
    // 120 dB is intolerable and is the low threshold for instantaneous hearing loss and pain, 140 dB is the high threshold for pain, 150 dB is garunteed temporary hearing loss, 160 dB is a general ballpark for how loud unsuppressed gunfire is for the shooter, 180+ dB will start to knock humans unconscious and cause injury.
    // Above 191 dB a pressure wave is a supersonic shockwave, and does not get to be a "sound wave" until it ceases being supersonic. Outside of good conditions humans generally will not notice sounds below 20 dB. The ambient noise level of a quiet room is around 40 dB, a quiet street is around 50 dB.
    // For a more detailed example list, see https://www.engineeringtoolbox.com/sound-pressure-d_711.html
    //
    // 0 dB spl reference is 2x10^-5 Pascals, which is an accademic and industry standard. This is the threshold of human hearing at 1kHz.
    // dB spl can be used to find the pressure reduction over distance of a sound wave.
    // Taking 1 tile to be ~1 meters, in an open field 10 tiles distance reduces perceived dB by 20, 100 tiles distance reduces dB by 40, etc.
    // Adding dB spl has to be done by the root mean square method (100dB + 100dB + 100 dB = ~104.7 dB spl) as it is itself a rms value of pressure fluctuations.
    // Adding several dB values together results in something usually almost identical to the largest source, (100 dB + 100 dB + 113 dB = 114.3 dB)
    // so we will generally just take the largest volume if its 10 greater than other sounds.
    //
    // Decibels are a relative unit of measurment that expresses the ratio of two values of some quantity on a logarithmic scale.
    // By itself it is unitless, since it is just a ratio. It is used because there is such a vast range of technically audible sounds, 0.00002 Pa to above 101000 Pa
    // Unfortunantly there are three different accoustic quantities that all use some version of dB,
    // are related to eachother but are very different quantities and are
    // mistaken for one another in almost all non-engineering or non-accademic contexts.
    // To make it even worse its still not uncommon to find mix ups in technical sources or to just have all three called dB and expecting you to just know which one they mean.
    // It is very common for audio electronics manufacturers, especially headset or microphone manufacturers, to just call all three properties sound intensity.
    // Saying someone probably got something wrong is not really an insult here, its just a fact of life with accoustics because of how poorly communicated so much information is.
    // (Be very, very careful second guessing musicians/sound techs however.)
    //
    // *Sound Pressure Level* (sound pressure, dB spl) is the rms pressure deviation due to a pressure wave (sound wave) from a reference pressure at some specific point.
    //      Proper unit is Pascals, This is what we are using. dB spl = 10*Log10(P^2/Pref^2) = 20*Log10(P/Pref).
    //      When discussing pressure levels in accoustics unless someone explicitly says they are referencing the peak amplitude of a sound wave, what they are refering too is the sound pressure level.
    //      Sound Pressure level is a root means square value, and is the effective pressure of that sinusoidal pressure wave at some point as it flip flops from positive to negative amplitude a couple thousand times a second.
    //      Almost all reference dB values for sounds are in sound pressure level, health regulations/medical figures, and this is what is measured by a point sensor like a microphone or decibel meters.
    //      Doubling the pressure of a sound wave increases the dB value by 6. A difference of 60 dB is a 1024x increase in pressure!
    //      Maximum dB spl in air is 191 dB, the maximum peak (not rms) dB is 194.
    //      Pressures of a higher dB rating are not sound pressure waves, they are supersonic blast/shock waves and should be modeled as damaging explosions. 195dB will rupture human eardrums. 200dB+ tends to be fatal from pressure alone.
    //
    //      If a source says that the sound pressure doubles every 3 dB, they gotten something wrong and have likely conflated sound pressure level with sound intensity level.
    //      This is probably the second most common mistake when discussing sound/accoustics, second only to not listing reference distance for dB spl measurments. dB spl measurments are useless without this.
    //
    // Sound Power Level (dB swl) is a the rate at which sound energy changes with time across some surface, and is effectively a vector quantity.
    //      Proper unit is in Watts dB swl = 10*Log10(W/Wref)
    //      Very useful from an engineering/physics standpoint and still useful to the end user, as it only depends on the noise source and is independant of the acoustic enviornment.
    //      But we dont really care about that here, and dont need to deal with all of the extra math and calcs that we would have to add.
    //
    // Sound Intensity Level (dB sil) is the power caried by sound waves per unit area in a direction perpendicular to an area.
    //      Proper unit is Watts/meter^2 (I). This is a field quantity and we are not trying to simulate the universe. dB sil = 10*Log10(I/Iref)
    //      Sound intensity doubles for every 3 dB sil.
    //      This value cannot really be directly measured, and is only really useful from an engineering/physics standpoint.
    //
    // We are in control of reality, and only really care about the perceived sound of creatures at explicit points in time at explicit distances which we control, so we use sound pressure level.
    short volume = 0;

    // What is the position of the sound source?
    tripoint origin;

    // What enum sound category is this?
    sounds::sound_t category = sounds::sound_t::background;

    // String description of the sound.
    std::string description = std::string( "" );

    // Is this sound from movement?
    bool movement_noise = false;

    // If all three froms are false, its ambient noise.
    // Did the player make this noise?
    bool from_player = false;

    // Did a monster make this noise?
    bool from_monster = false;

    // Did an NPC make this noise?
    bool from_npc = false;

    //This stuff is for selecting actual sfx to play through an audio device in THE REAL WORLD. (spooky)
    std::string id = std::string( "" );
    std::string variant = std::string( "default" );

    faction_id faction = faction_id( "no_faction" );
    mfaction_str_id monfaction = mfaction_str_id( "" );
};

// The dB loss for moving to a new distance, in 100ths of dB. Nominal calc is 100 * (20 * Log10( dist / ( dist - 1 )))
// dist_vol_loss[2] provides the dB loss moving from 1m to 2m
// dist_vol_loss[5] provides the dB loss moving from 4m to 5m, etc.
// Technically we should not have a value for going from 0m to 1m, but we might ask for it with how sound flood filling is handled around corners etc.
// Mathmatically it should theoretically be somewhere around 20dB, though the rules for calcing sound pressure break down at very small distances
// And finding the actual answer is the realm of neat pressure calc tricks or L'Hopital's shenanagins. Just take 15dB.
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

// Given a source tile's sound direction as the index, provides an array of which adjacent tiles should have a propagation distance penalty.
// Store this as we reference it every time we check a tile's neighboors for update, for potentially hundreds of tiles per sound.
// Doing the boolean logic to find this in the code otherwise is slightly slower, but every little bit helps with how many tiles are checked.
// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ 8 ] [ 3 ]
// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
// We have the 9th slot (index 8) for setting initial flood fill conditions and if we somehow check against the source tile of a sound.
static constexpr auto sound_direction_distance_modifier = std::array<std::array<bool, 8>, 9 >
{
    { {false, false, true, false, false, false, true, false},
        {false, false, false, true, false, false, false, true},
        {true, false, false, false, true, false, false, false},
        {false, true, false, false, false, true, false, false},
        {false, false, true, false, false, false, true, false},
        {false, false, false, true, false, false, false, true},
        {true, false, false, false, true, false, false, false},
        {false, true, false, false, false, true, false, false},
        {false, false, false, false, false, false, false, false}
    }
};

// Given a source tile's sound direction as the index, provides an array of which adjacent tiles are valid to attempt to propagate sound to.
// Store this as we reference it every time we check a tile's neighboors for update, for potentially hundreds of tiles per sound.
// We do additional map checking to check for corner invalidation due to walls, but having this stored still helps speed things up.
// [-1 , 1 ] [ 0 , 1 ] [ 1 , 1 ]   [ 0 ] [ 1 ] [ 2 ]
// [-1 , 0 ] [ 0 , 0 ] [ 1 , 0 ] = [ 7 ] [ 8 ] [ 3 ]
// [-1 , -1] [ 0 , -1] [ 1 , -1]   [ 6 ] [ 5 ] [ 4 ]
// We have the 9th slot (index 8) for setting initial flood fill conditions and if we somehow check against the source tile of a sound again.
static constexpr auto sound_direction_propagation_valid = std::array<std::array<bool, 8>, 9 >
{
    { {true, true, true, false, false, false, true, true},
        {true, true, true, true, false, false, false, true},
        {true, true, true, true, true, false, false, false},
        {false, true, true, true, true, true, false, false},
        {false, false, true, true, true, true, true, false},
        {false, false, false, true, true, true, true, true},
        {true, false, false, false, true, true, true, true},
        {true, true, false, false, false, true, true, true},
        {true, true, true, true, true, true, true, true}
    }
};

