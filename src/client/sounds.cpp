#include "sounds.h"

#include "active_tile_data.h"
#include "avatar.h"
#include "calendar.h"
#include "character.h"
#include "construction.h"
#include "coordinates.h"
#include "creature.h"
#include "debug.h"
#include "enums.h"
#include "faction.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "itype.h"
#include "line.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map/mapdata.h"
#include "map/submap.h"
#include "map_iterator.h"
#include "messages.h"
#include "monfaction.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "overmap/omdata.h"
#include "overmap/overmapbuffer.h"
#include "overmap/overmapbuffer_registry.h"
#include "player.h"
#include "player_activity.h"
#include "point.h"
#include "profile.h"
#include "rng.h"
#include "safemode_ui.h"
#include "sound_attenuation.h"
#include "string_formatter.h"
#include "string_id.h"
#include "thread_pool.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "units_angle.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "weather/weather.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <ostream>
#include <queue>
#include <set>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(SDL_SOUND)
#    include <thread>
#    if defined(_WIN32) && !defined(_MSC_VER)
#        include "mingw.thread.h"
#    endif
#    define dbg(x) DebugLogFL((x), DC::SDL)
#endif

#if defined(SDL_SOUND)
weather_type_id previous_weather;
int prev_hostiles = 0;
int previous_speed = 0;
int previous_gear = 0;
bool audio_muted = false;
auto start_sfx_timestamp = std::chrono::high_resolution_clock::now();
auto end_sfx_timestamp = std::chrono::high_resolution_clock::now();
auto sfx_time = end_sfx_timestamp - start_sfx_timestamp;
activity_id act;
std::pair<std::string, std::string> engine_external_id_and_variant;
static const itype_id fuel_type_muscle("muscle");
static const itype_id fuel_type_wind("wind");
static const itype_id fuel_type_battery("battery");
static const itype_id itype_weapon_fire_suppressed("weapon_fire_suppressed");
#endif
extern float g_sfx_volume_multiplier;
#if defined(SDL_SOUND)
void sfx::do_vehicle_engine_sfx() {
    if (test_mode) { return; }

    static const channel ch = channel::interior_engine_sound;
    const Character& player_character = get_player_character();
    if (!player_character.in_vehicle) {
        fade_audio_channel(ch, 300);
        add_msg(m_debug, "STOP interior_engine_sound, OUT OF CAR");
        return;
    }
    if (player_character.in_sleep_state() && !audio_muted) {
        fade_audio_channel(channel::any, 300);
        audio_muted = true;
        return;
    } else if (player_character.in_sleep_state() && audio_muted) {
        return;
    }
    optional_vpart_position vpart_opt = get_map().veh_at(player_character.bub_pos());
    vehicle* veh;
    if (vpart_opt.has_value()) {
        veh = &vpart_opt->vehicle();
    } else {
        return;
    }
    if (!veh->engine_on) {
        fade_audio_channel(ch, 100);
        add_msg(m_debug, "STOP interior_engine_sound");
        return;
    }

    std::pair<std::string, std::string> id_and_variant;

    for (size_t e = 0; e < veh->engines.size(); ++e) {
        if (veh->is_engine_on(e)) {
            if (sfx::has_variant_sound(
                    "engine_working_internal", veh->part_info(veh->engines[e]).get_id().str())) {
                id_and_variant = std::make_pair(
                    "engine_working_internal", veh->part_info(veh->engines[e]).get_id().str());
            } else if (veh->is_engine_type(e, fuel_type_muscle)) {
                id_and_variant = std::make_pair("engine_working_internal", "muscle");
            } else if (veh->is_engine_type(e, fuel_type_wind)) {
                id_and_variant = std::make_pair("engine_working_internal", "wind");
            } else if (veh->is_engine_type(e, fuel_type_battery)) {
                id_and_variant = std::make_pair("engine_working_internal", "electric");
            } else {
                id_and_variant = std::make_pair("engine_working_internal", "combustion");
            }
        }
    }

    if (!is_channel_playing(ch)) {
        // We take the generic dB volume of a running engine to be 80dB
        play_ambient_variant_sound(
            id_and_variant.first, id_and_variant.second,
            sfx::get_heard_volume(player_character.bub_pos(), 80), ch, 1000);
        add_msg(m_debug, "START %s %s", id_and_variant.first, id_and_variant.second);
    } else {
        add_msg(m_debug, "PLAYING");
    }
    int current_speed = veh->velocity;
    bool in_reverse = false;
    if (current_speed <= -1) {
        current_speed = current_speed * -1;
        in_reverse = true;
    }
    double pitch = 1.0;
    int safe_speed = veh->safe_velocity();
    int current_gear;
    if (in_reverse) {
        current_gear = -1;
    } else if (current_speed == 0) {
        current_gear = 0;
    } else if (current_speed > 0 && current_speed <= safe_speed / 12) {
        current_gear = 1;
    } else if (current_speed > safe_speed / 12 && current_speed <= safe_speed / 5) {
        current_gear = 2;
    } else if (current_speed > safe_speed / 5 && current_speed <= safe_speed / 4) {
        current_gear = 3;
    } else if (current_speed > safe_speed / 4 && current_speed <= safe_speed / 3) {
        current_gear = 4;
    } else if (current_speed > safe_speed / 3 && current_speed <= safe_speed / 2) {
        current_gear = 5;
    } else {
        current_gear = 6;
    }
    if (veh->has_engine_type(fuel_type_muscle, true)
        || veh->has_engine_type(fuel_type_wind, true)) {
        current_gear = previous_gear;
    }

    if (current_gear > previous_gear) {
        // We take the generic dB volume of a running vehicle engine to be 80dB
        play_variant_sound(
            "vehicle", "gear_shift", get_heard_volume(player_character.bub_pos(), 80), 0_degrees,
            0.8, 0.8);
        add_msg(m_debug, "GEAR UP");
    } else if (current_gear < previous_gear) {
        play_variant_sound(
            "vehicle", "gear_shift", get_heard_volume(player_character.bub_pos(), 80), 0_degrees,
            1.2, 1.2);
        add_msg(m_debug, "GEAR DOWN");
    }
    if ((safe_speed != 0)) {
        if (current_gear == 0) {
            pitch = 1.0;
        } else if (current_gear == -1) {
            pitch = 1.2;
        } else {
            pitch = 1.0 - static_cast<double>(current_speed) / static_cast<double>(safe_speed);
        }
    }
    pitch = std::max(pitch, 0.5);

    if (current_speed != previous_speed) {
        fade_audio_channel(ch, 0);
        add_msg(m_debug, "STOP speed %d =/= %d", current_speed, previous_speed);
        play_ambient_variant_sound(
            id_and_variant.first, id_and_variant.second,
            sfx::get_heard_volume(player_character.bub_pos(), 80), ch, 1000, pitch);
        add_msg(m_debug, "PITCH %f", pitch);
    }
    previous_speed = current_speed;
    previous_gear = current_gear;
}

void sfx::do_vehicle_exterior_engine_sfx() {
    if (test_mode) { return; }

    static const channel ch = channel::exterior_engine_sound;
    const avatar& player_character = get_avatar();
    const auto& ploc = player_character.bub_pos();
    // early bail-outs for efficiency
    if (player_character.in_vehicle) {
        fade_audio_channel(ch, 300);
        add_msg(m_debug, "STOP exterior_engine_sound, IN CAR");
        return;
    }
    if (player_character.in_sleep_state() && !audio_muted) {
        fade_audio_channel(channel::any, 300);
        audio_muted = true;
        return;
    } else if (player_character.in_sleep_state() && audio_muted) {
        return;
    }
    const auto& map = get_map();
    VehicleList vehs = get_map().get_vehicles();
    unsigned char noise_factor = 0;
    unsigned char vol = 0;
    vehicle* veh = nullptr;


    const short t_absorp_player =
        map.get_cache_ref(ploc.z())
            .absorption_cache[map.get_cache_ref(ploc.z()).idx(ploc.x(), ploc.y())];

    for (wrapped_vehicle vehicle : vehs) {
        if (vehicle.v->vehicle_noise > 0) {
            const auto& veh_loc = vehicle.v->bub_ms_location();
            // This is a jank fix to get vehicles to not be deafening from accross the map.
            const int dist = rl_dist(ploc, veh_loc);
            const short unadjusted_vol = std::
                min(MAXIMUM_VOLUME_ATMOSPHERE,
                    dBspl_to_mdBspl(static_cast<short>(vehicle.v->vehicle_noise)));
            const auto& veh_idx = map.get_cache_ref(veh_loc.z()).idx(veh_loc.x(), veh_loc.y());
            const auto& t_absorp_avg = static_cast<short>(std::round(
                (t_absorp_player + (map.get_cache_ref(veh_loc.z()).absorption_cache[veh_idx]))
                / 2));
            const short adjusted_vol = mdBspl_to_dBspl(std::max(
                0,
                unadjusted_vol - get_cumulative_vol_dist_loss(1, dist, t_absorp_avg)
                    - vol_z_adjust(
                        {.source = veh_loc,
                         .listener = ploc,
                         .line_of_sight = player_character.sees(veh_loc)})));
            if (adjusted_vol > noise_factor) {

                noise_factor = adjusted_vol;
                veh = vehicle.v;
            }
        }
    }
    if (!noise_factor || !veh) {
        fade_audio_channel(ch, 300);
        add_msg(m_debug, "STOP exterior_engine_sound, NO NOISE");
        return;
    }
    // we only want volume going from 0 - 100. Our dB level goes from 0 - 191, but we want to hit
    // the top of our range near 120.
    vol = static_cast<unsigned char>(
        (noise_factor >= 120) ? 100
        : (noise_factor > 90)
            ? std::round(noise_factor * 0.8)
            : noise_factor);
    std::pair<std::string, std::string> id_and_variant;

    for (size_t e = 0; e < veh->engines.size(); ++e) {
        if (veh->is_engine_on(e)) {
            if (sfx::has_variant_sound(
                    "engine_working_external", veh->part_info(veh->engines[e]).get_id().str())) {
                id_and_variant = std::make_pair(
                    "engine_working_external", veh->part_info(veh->engines[e]).get_id().str());
            } else if (veh->is_engine_type(e, fuel_type_muscle)) {
                id_and_variant = std::make_pair("engine_working_external", "muscle");
            } else if (veh->is_engine_type(e, fuel_type_wind)) {
                id_and_variant = std::make_pair("engine_working_external", "wind");
            } else if (veh->is_engine_type(e, fuel_type_battery)) {
                id_and_variant = std::make_pair("engine_working_external", "electric");
            } else {
                id_and_variant = std::make_pair("engine_working_external", "combustion");
            }
        }
    }

    if (is_channel_playing(ch)) {
        if (engine_external_id_and_variant == id_and_variant) {
            set_channel_3d_position(ch, get_heard_angle(veh->bub_ms_location()));
            set_channel_volume(ch, vol);
            add_msg(m_debug, "PLAYING exterior_engine_sound, vol: %d", vol);
        } else {
            engine_external_id_and_variant = id_and_variant;
            fade_audio_channel(ch, 0);
            add_msg(m_debug, "STOP exterior_engine_sound, change id/var");
            play_ambient_variant_sound(id_and_variant.first, id_and_variant.second, 128, ch, 0);
            set_channel_3d_position(ch, get_heard_angle(veh->bub_ms_location()));
            set_channel_volume(ch, vol);
            add_msg(m_debug, "START exterior_engine_sound %s %s vol: %d", id_and_variant.first,
                    id_and_variant.second, get_channel_volume(ch));
        }
    } else {
        play_ambient_variant_sound(id_and_variant.first, id_and_variant.second, 128, ch, 0);
        set_channel_3d_position(ch, get_heard_angle(veh->bub_ms_location()));
        set_channel_volume(ch, vol);
        add_msg(m_debug, "START exterior_engine_sound NEW %s %s vol: ex:%d true:%d",
                id_and_variant.first, id_and_variant.second, vol, get_channel_volume(ch));
    }
}

void sfx::do_ambient() {
    if (test_mode) { return; }

    Character& player_character = get_player_character();
    if (player_character.in_sleep_state() && !audio_muted) {
        fade_audio_channel(channel::any, 300);
        audio_muted = true;
        return;
    } else if (player_character.in_sleep_state() && audio_muted) {
        return;
    }
    audio_muted = false;
    const bool is_deaf = player_character.is_deaf();
    // If the source is the player avatar, our ambient volume is returned as 100 *
    // g_sfx_volume_multiplier anyways. const int heard_volume = get_heard_volume(
    // player_character.bub_pos(), 100 );
    const int heard_volume = std::ceil(100 * g_sfx_volume_multiplier);
    const bool is_underground = player_character.bub_pos().z() < 0;
    const bool is_sheltered = g->is_sheltered(player_character.bub_pos());
    const bool weather_changed = get_weather().weather_id != previous_weather;
    // Step in at night time / we are not indoors
    if (is_night(calendar::turn) && !is_sheltered
        && !is_channel_playing(channel::nighttime_outdoors_env) && !is_deaf) {
        fade_audio_group(group::time_of_day, 1000);
        play_ambient_variant_sound(
            "environment", "nighttime", heard_volume, channel::nighttime_outdoors_env, 1000);
        // Step in at day time / we are not indoors
    } else if (!is_night(calendar::turn) && !is_channel_playing(channel::daytime_outdoors_env)
               && !is_sheltered && !is_deaf) {
        fade_audio_group(group::time_of_day, 1000);
        play_ambient_variant_sound(
            "environment", "daytime", heard_volume, channel::daytime_outdoors_env, 1000);
    }
    // We are underground
    if ((is_underground && !is_channel_playing(channel::underground_env) && !is_deaf)
        || (is_underground && weather_changed && !is_deaf)) {
        fade_audio_group(group::weather, 1000);
        fade_audio_group(group::time_of_day, 1000);
        play_ambient_variant_sound(
            "environment", "underground", heard_volume, channel::underground_env, 1000);
        // We are indoors
    } else if (
        (is_sheltered && !is_underground && !is_channel_playing(channel::indoors_env) && !is_deaf)
        || (is_sheltered && !is_underground && weather_changed && !is_deaf)) {
        fade_audio_group(group::weather, 1000);
        fade_audio_group(group::time_of_day, 1000);
        play_ambient_variant_sound(
            "environment", "indoors", heard_volume, channel::indoors_env, 1000);
    }

    // We are indoors and it is also raining
    if (get_weather().weather_id->rains
        && get_weather().weather_id->precip != precip_class::very_light && !is_underground
        && is_sheltered && !is_channel_playing(channel::indoors_rain_env)) {
        play_ambient_variant_sound(
            "environment", "indoors_rain", heard_volume, channel::indoors_rain_env, 1000);
    }
    if ((!is_sheltered && get_weather().weather_id->sound_category != weather_sound_category::silent
         && !is_deaf && !is_channel_playing(channel::outdoors_snow_env)
         && !is_channel_playing(channel::outdoors_flurry_env)
         && !is_channel_playing(channel::outdoors_thunderstorm_env)
         && !is_channel_playing(channel::outdoors_rain_env)
         && !is_channel_playing(channel::outdoors_drizzle_env)
         && !is_channel_playing(channel::outdoor_blizzard))
        || (!is_sheltered && weather_changed && !is_deaf)) {
        fade_audio_group(group::weather, 1000);
        // We are outside and there is precipitation
        switch (get_weather().weather_id->sound_category) {
            case weather_sound_category::drizzle:
                play_ambient_variant_sound(
                    "environment", "WEATHER_DRIZZLE", heard_volume, channel::outdoors_drizzle_env,
                    1000);
                break;
            case weather_sound_category::rainy:
                play_ambient_variant_sound(
                    "environment", "WEATHER_RAINY", heard_volume, channel::outdoors_rain_env, 1000);
                break;
            case weather_sound_category::thunder:
                play_ambient_variant_sound(
                    "environment", "WEATHER_THUNDER", heard_volume,
                    channel::outdoors_thunderstorm_env, 1000);
                break;
            case weather_sound_category::flurries:
                play_ambient_variant_sound(
                    "environment", "WEATHER_FLURRIES", heard_volume, channel::outdoors_flurry_env,
                    1000);
                break;
            case weather_sound_category::snowstorm:
                play_ambient_variant_sound(
                    "environment", "WEATHER_SNOWSTORM", heard_volume, channel::outdoor_blizzard,
                    1000);
                break;
            case weather_sound_category::snow:
                play_ambient_variant_sound(
                    "environment", "WEATHER_SNOW", heard_volume, channel::outdoors_snow_env, 1000);
                break;
            case weather_sound_category::silent:
                break;
            case weather_sound_category::last:
                debugmsg("Invalid weather sound category.");
                break;
        }
    }
    // Keep track of weather to compare for next iteration
    previous_weather = get_weather().weather_id;
}

// firing is the item that is fired. It may be the wielded gun, but it can also be an attached
// gunmod.
void sfx::generate_gun_sound(
    const tripoint_bub_ms& source, const item& firing, const short& origin_vol) {
    if (test_mode) { return; }

    end_sfx_timestamp = std::chrono::high_resolution_clock::now();
    sfx_time = end_sfx_timestamp - start_sfx_timestamp;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(sfx_time).count() < 80) { return; }
    int heard_volume = get_heard_volume(source, origin_vol);
    heard_volume = std::max(heard_volume, 10);

    itype_id weapon_id = firing.typeId();
    units::angle angle = 0_degrees;
    std::string selected_sound;
    const avatar& player_character = get_avatar();
    // this does not mean p == avatar (it could be a vehicle turret)
    if (player_character.bub_pos() == source) {
        selected_sound = "fire_gun";

        const auto mods = firing.gunmods();
        if (std::ranges::any_of(mods, [](const item* e) {
                return e->type->gunmod->loudness < -20;
            })) {
            weapon_id = itype_weapon_fire_suppressed;
        }

    } else {
        angle = get_heard_angle(source);
        if (heard_volume >= 100) {
            selected_sound = "fire_gun";
        } else {
            selected_sound = "fire_gun_distant";
        }
    }

    play_variant_sound(selected_sound, weapon_id.str(), heard_volume, angle, 0.8, 1.2, true);
    start_sfx_timestamp = std::chrono::high_resolution_clock::now();
}

void sfx::generate_melee_sound(
    const tripoint_bub_ms& source, const tripoint_bub_ms& target, bool hit, bool targ_mon,
    const std::string& material) {
    if (test_mode) { return; }
    const player* p = g->critter_at<npc>(source);
    const int heard_volume = get_heard_volume(source, 80);

    skill_id weapon_skill;
    int weapon_volume;
    // volume and angle for calls to play_variant_sound
    units::angle ang_src;
    int vol_src;
    int vol_targ;
    units::angle ang_targ;
    if (!p) {
        p = &g->u;
        // sound comes from the same place as the player is, calculation of angle wouldn't work
        ang_src = 0_degrees;
        vol_src = heard_volume;
        vol_targ = heard_volume;
    } else {
        ang_src = get_heard_angle(source);
        vol_src = std::max(heard_volume - 30, 0);
        vol_targ = std::max(heard_volume - 20, 0);
    }
    ang_targ = get_heard_angle(target);
    weapon_skill = p->primary_weapon().melee_skill();
    weapon_volume = p->primary_weapon().volume() / units::legacy_volume_factor;

    std::string variant_used;

    static const skill_id skill_bashing("bashing");
    static const skill_id skill_cutting("cutting");
    static const skill_id skill_stabbing("stabbing");

    if (weapon_skill == skill_bashing && weapon_volume <= 8) {
        variant_used = "small_bash";
        play_variant_sound("melee_swing", "small_bash", vol_src, ang_src, 0.8, 1.2);
    } else if (weapon_skill == skill_bashing && weapon_volume >= 9) {
        variant_used = "big_bash";
        play_variant_sound("melee_swing", "big_bash", vol_src, ang_src, 0.8, 1.2);
    } else if ((weapon_skill == skill_cutting || weapon_skill == skill_stabbing)
               && weapon_volume <= 6) {
        variant_used = "small_cutting";
        play_variant_sound("melee_swing", "small_cutting", vol_src, ang_src, 0.8, 1.2);
    } else if ((weapon_skill == skill_cutting || weapon_skill == skill_stabbing)
               && weapon_volume >= 7) {
        variant_used = "big_cutting";
        play_variant_sound("melee_swing", "big_cutting", vol_src, ang_src, 0.8, 1.2);
    } else {
        variant_used = "default";
        play_variant_sound("melee_swing", "default", vol_src, ang_src, 0.8, 1.2);
    }
    if (hit) {
        if (targ_mon) {
            if (material == "steel") {
                play_variant_sound("melee_hit_metal", variant_used, vol_targ, ang_targ, 0.8, 1.2);
            } else {
                play_variant_sound("melee_hit_flesh", variant_used, vol_targ, ang_targ, 0.8, 1.2);
            }
        } else {
            play_variant_sound("melee_hit_flesh", variant_used, vol_targ, ang_targ, 0.8, 1.2);
        }
    }
}

void sfx::do_projectile_hit(const Creature& target) {
    if (test_mode) { return; }
    // Take projectile impacts at a baseline of 80dB
    const int heard_volume = sfx::get_heard_volume(target.bub_pos(), 80);
    const units::angle angle = get_heard_angle(target.bub_pos());
    if (target.is_monster()) {
        const monster& mon = dynamic_cast<const monster&>(target);
        static const std::set<material_id> fleshy = {
            material_id("flesh"), material_id("hflesh"), material_id("iflesh"),
            material_id("veggy"), material_id("bone"),
        };
        const bool is_fleshy = std::ranges::any_of(fleshy, [&mon](const material_id& m) {
            return mon.made_of(m);
        });

        if (is_fleshy) {
            play_variant_sound("bullet_hit", "hit_flesh", heard_volume, angle, 0.8, 1.2);
            return;
        } else if (mon.made_of(material_id("stone"))) {
            play_variant_sound("bullet_hit", "hit_wall", heard_volume, angle, 0.8, 1.2);
            return;
        } else if (mon.made_of(material_id("steel"))) {
            play_variant_sound("bullet_hit", "hit_metal", heard_volume, angle, 0.8, 1.2);
            return;
        } else {
            play_variant_sound("bullet_hit", "hit_flesh", heard_volume, angle, 0.8, 1.2);
            return;
        }
    }
    play_variant_sound("bullet_hit", "hit_flesh", heard_volume, angle, 0.8, 1.2);
}

void sfx::do_player_death_hurt(const player& target, bool death) {
    if (test_mode) { return; }
    // Take the origin volume at 80dB
    int heard_volume = get_heard_volume(target.bub_pos(), 80);
    const bool male = target.male;
    if (!male && !death) {
        play_variant_sound("deal_damage", "hurt_f", heard_volume);
    } else if (male && !death) {
        play_variant_sound("deal_damage", "hurt_m", heard_volume);
    } else if (!male && death) {
        play_variant_sound("clean_up_at_end", "death_f", heard_volume);
    } else if (male && death) {
        play_variant_sound("clean_up_at_end", "death_m", heard_volume);
    }
}

void sfx::do_danger_music() {
    if (test_mode) { return; }

    avatar& player_character = get_avatar();
    if (player_character.in_sleep_state() && !audio_muted) {
        fade_audio_channel(channel::any, 100);
        audio_muted = true;
        return;
    } else if ((player_character.in_sleep_state() && audio_muted)
               || is_channel_playing(channel::chainsaw_theme)) {
        fade_audio_group(group::context_themes, 1000);
        return;
    }
    audio_muted = false;
    const int hostiles = player_character.get_mon_visible().combat_hostile_count;
    if (hostiles == prev_hostiles) { return; }
    if (hostiles <= 4) {
        fade_audio_group(group::context_themes, 1000);
        prev_hostiles = hostiles;
        return;
    } else if (hostiles >= 5 && hostiles <= 9 && !is_channel_playing(channel::danger_low_theme)) {
        fade_audio_group(group::context_themes, 1000);
        play_ambient_variant_sound("danger_low", "default", 100, channel::danger_low_theme, 1000);
        prev_hostiles = hostiles;
        return;
    } else if (hostiles >= 10 && hostiles <= 14
               && !is_channel_playing(channel::danger_medium_theme)) {
        fade_audio_group(group::context_themes, 1000);
        play_ambient_variant_sound(
            "danger_medium", "default", 100, channel::danger_medium_theme, 1000);
        prev_hostiles = hostiles;
        return;
    } else if (hostiles >= 15 && hostiles <= 19 && !is_channel_playing(channel::danger_high_theme)) {
        fade_audio_group(group::context_themes, 1000);
        play_ambient_variant_sound("danger_high", "default", 100, channel::danger_high_theme, 1000);
        prev_hostiles = hostiles;
        return;
    } else if (hostiles >= 20 && !is_channel_playing(channel::danger_extreme_theme)) {
        fade_audio_group(group::context_themes, 1000);
        play_ambient_variant_sound(
            "danger_extreme", "default", 100, channel::danger_extreme_theme, 1000);
        prev_hostiles = hostiles;
        return;
    }
    prev_hostiles = hostiles;
}

void sfx::do_fatigue() {
    if (test_mode) { return; }

    avatar& player_character = get_avatar();
    /*15: Stamina 75%
    16: Stamina 50%
    17: Stamina 25%*/
    if (player_character.get_stamina() >= player_character.get_stamina_max() * .75) {
        fade_audio_group(group::fatigue, 2000);
        return;
    } else if (player_character.get_stamina() <= player_character.get_stamina_max() * .74
               && player_character.get_stamina() >= player_character.get_stamina_max() * .5
               && player_character.male && !is_channel_playing(channel::stamina_75)) {
        fade_audio_group(group::fatigue, 1000);
        play_ambient_variant_sound("plmove", "fatigue_m_low", 100, channel::stamina_75, 1000);
        return;
    } else if (player_character.get_stamina() <= player_character.get_stamina_max() * .49
               && player_character.get_stamina() >= player_character.get_stamina_max() * .25
               && player_character.male && !is_channel_playing(channel::stamina_50)) {
        fade_audio_group(group::fatigue, 1000);
        play_ambient_variant_sound("plmove", "fatigue_m_med", 100, channel::stamina_50, 1000);
        return;
    } else if (player_character.get_stamina() <= player_character.get_stamina_max() * .24
               && player_character.get_stamina() >= 0 && player_character.male
               && !is_channel_playing(channel::stamina_35)) {
        fade_audio_group(group::fatigue, 1000);
        play_ambient_variant_sound("plmove", "fatigue_m_high", 100, channel::stamina_35, 1000);
        return;
    } else if (player_character.get_stamina() <= player_character.get_stamina_max() * .74
               && player_character.get_stamina() >= player_character.get_stamina_max() * .5
               && !player_character.male && !is_channel_playing(channel::stamina_75)) {
        fade_audio_group(group::fatigue, 1000);
        play_ambient_variant_sound("plmove", "fatigue_f_low", 100, channel::stamina_75, 1000);
        return;
    } else if (player_character.get_stamina() <= player_character.get_stamina_max() * .49
               && player_character.get_stamina() >= player_character.get_stamina_max() * .25
               && !player_character.male && !is_channel_playing(channel::stamina_50)) {
        fade_audio_group(group::fatigue, 1000);
        play_ambient_variant_sound("plmove", "fatigue_f_med", 100, channel::stamina_50, 1000);
        return;
    } else if (player_character.get_stamina() <= player_character.get_stamina_max() * .24
               && player_character.get_stamina() >= 0 && !player_character.male
               && !is_channel_playing(channel::stamina_35)) {
        fade_audio_group(group::fatigue, 1000);
        play_ambient_variant_sound("plmove", "fatigue_f_high", 100, channel::stamina_35, 1000);
        return;
    }
}

void sfx::do_hearing_loss(int turns) {
    if (test_mode) { return; }

    g_sfx_volume_multiplier = .1;
    fade_audio_group(group::weather, 50);
    fade_audio_group(group::time_of_day, 50);
    // Negative duration is just insuring we stay in sync with player condition,
    // don't play any of the sound effects for going deaf.
    if (turns == -1) { return; }
    play_variant_sound("environment", "deafness_shock", 100);
    play_variant_sound("environment", "deafness_tone_start", 100);
    if (turns <= 35) {
        play_ambient_variant_sound(
            "environment", "deafness_tone_light", 90, channel::deafness_tone, 100);
    } else if (turns <= 90) {
        play_ambient_variant_sound(
            "environment", "deafness_tone_medium", 90, channel::deafness_tone, 100);
    } else if (turns >= 91) {
        play_ambient_variant_sound(
            "environment", "deafness_tone_heavy", 90, channel::deafness_tone, 100);
    }
}

void sfx::remove_hearing_loss() {
    if (test_mode) { return; }
    stop_sound_effect_fade(channel::deafness_tone, 300);
    g_sfx_volume_multiplier = 1;
    do_ambient();
}

void sfx::do_footstep() {
    if (test_mode) { return; }

    end_sfx_timestamp = std::chrono::high_resolution_clock::now();
    sfx_time = end_sfx_timestamp - start_sfx_timestamp;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(sfx_time).count() > 400) {
        const avatar& player_character = get_avatar();
        // Take footsteps at 60dB.
        int heard_volume = sfx::get_heard_volume(player_character.bub_pos(), 60);
        const auto terrain = get_map().ter(player_character.bub_pos()).id();
        static const std::set<ter_str_id> grass = {
            ter_str_id("t_grass"),
            ter_str_id("t_shrub"),
            ter_str_id("t_shrub_peanut"),
            ter_str_id("t_shrub_peanut_harvested"),
            ter_str_id("t_shrub_blueberry"),
            ter_str_id("t_shrub_blueberry_harvested"),
            ter_str_id("t_shrub_strawberry"),
            ter_str_id("t_shrub_strawberry_harvested"),
            ter_str_id("t_shrub_blackberry"),
            ter_str_id("t_shrub_blackberry_harvested"),
            ter_str_id("t_shrub_huckleberry"),
            ter_str_id("t_shrub_huckleberry_harvested"),
            ter_str_id("t_shrub_raspberry"),
            ter_str_id("t_shrub_raspberry_harvested"),
            ter_str_id("t_shrub_grape"),
            ter_str_id("t_shrub_grape_harvested"),
            ter_str_id("t_shrub_rose"),
            ter_str_id("t_shrub_rose_harvested"),
            ter_str_id("t_shrub_hydrangea"),
            ter_str_id("t_shrub_hydrangea_harvested"),
            ter_str_id("t_shrub_lilac"),
            ter_str_id("t_shrub_lilac_harvested"),
            ter_str_id("t_underbrush"),
            ter_str_id("t_underbrush_harvested_spring"),
            ter_str_id("t_underbrush_harvested_summer"),
            ter_str_id("t_underbrush_harvested_autumn"),
            ter_str_id("t_underbrush_harvested_winter"),
            ter_str_id("t_moss"),
            ter_str_id("t_moss_underground"),
            ter_str_id("t_grass_white"),
            ter_str_id("t_grass_long"),
            ter_str_id("t_grass_tall"),
            ter_str_id("t_grass_dead"),
            ter_str_id("t_grass_golf"),
            ter_str_id("t_golf_hole"),
            ter_str_id("t_trunk"),
            ter_str_id("t_stump"),
        };
        static const std::set<ter_str_id> dirt = {
            ter_str_id("t_dirt"),
            ter_str_id("t_dirtmound"),
            ter_str_id("t_dirtmoundfloor"),
            ter_str_id("t_sand"),
            ter_str_id("t_clay"),
            ter_str_id("t_dirtfloor"),
            ter_str_id("t_palisade_gate_o"),
            ter_str_id("t_sandbox"),
            ter_str_id("t_claymound"),
            ter_str_id("t_sandmound"),
            ter_str_id("t_rootcellar"),
            ter_str_id("t_railroad_rubble"),
            ter_str_id("t_railroad_track"),
            ter_str_id("t_railroad_track_h"),
            ter_str_id("t_railroad_track_v"),
            ter_str_id("t_railroad_track_d"),
            ter_str_id("t_railroad_track_d1"),
            ter_str_id("t_railroad_track_d2"),
            ter_str_id("t_railroad_tie"),
            ter_str_id("t_railroad_tie_d"),
            ter_str_id("t_railroad_tie_d"),
            ter_str_id("t_railroad_tie_h"),
            ter_str_id("t_railroad_tie_v"),
            ter_str_id("t_railroad_tie_d"),
            ter_str_id("t_railroad_track_on_tie"),
            ter_str_id("t_railroad_track_h_on_tie"),
            ter_str_id("t_railroad_track_v_on_tie"),
            ter_str_id("t_railroad_track_d_on_tie"),
            ter_str_id("t_railroad_tie"),
            ter_str_id("t_railroad_tie_h"),
            ter_str_id("t_railroad_tie_v"),
            ter_str_id("t_railroad_tie_d1"),
            ter_str_id("t_railroad_tie_d2"),
        };
        static const std::set<ter_str_id> metal = {
            ter_str_id("t_ov_smreb_cage"),   ter_str_id("t_metal_floor"),
            ter_str_id("t_grate"),           ter_str_id("t_bridge"),
            ter_str_id("t_elevator"),        ter_str_id("t_guardrail_bg_dp"),
            ter_str_id("t_slide"),           ter_str_id("t_conveyor"),
            ter_str_id("t_machinery_light"), ter_str_id("t_machinery_heavy"),
            ter_str_id("t_machinery_old"),   ter_str_id("t_machinery_electronic"),
        };
        static const std::set<ter_str_id> water = {
            ter_str_id("t_water_moving_sh"), ter_str_id("t_water_moving_dp"),
            ter_str_id("t_water_sh"),        ter_str_id("t_water_dp"),
            ter_str_id("t_swater_sh"),       ter_str_id("t_swater_dp"),
            ter_str_id("t_water_pool"),      ter_str_id("t_sewage"),
        };
        static const std::set<ter_str_id> chain_fence = {
            ter_str_id("t_chainfence"),
        };

        const auto play_plmove_sound_variant = [&](const std::string& variant) {
            play_variant_sound("plmove", variant, heard_volume, 0_degrees, 0.8, 1.2);
            start_sfx_timestamp = std::chrono::high_resolution_clock::now();
        };

        auto veh_displayed_part = g->m.veh_at(g->u.bub_pos()).part_displayed();

        if (!veh_displayed_part && (water.contains(terrain))) {
            play_plmove_sound_variant("walk_water");
            return;
        }
        if (!g->u.wearing_something_on(bodypart_id(bp_foot_l))) {
            play_plmove_sound_variant("walk_barefoot");
            return;
        }
        if (veh_displayed_part) {
            const std::string& part_id = veh_displayed_part->part().info().get_id().str();
            if (has_variant_sound("plmove", part_id)) {
                play_plmove_sound_variant(part_id);
            } else if (veh_displayed_part->has_feature(VPFLAG_AISLE)) {
                play_plmove_sound_variant("walk_tarmac");
            } else {
                play_plmove_sound_variant("clear_obstacle");
            }
            return;
        }
        if (sfx::has_variant_sound("plmove", terrain.str())) {
            play_plmove_sound_variant(terrain.str());
            return;
        }
        if (grass.contains(terrain)) {
            play_plmove_sound_variant("walk_grass");
            return;
        }
        if (dirt.contains(terrain)) {
            play_plmove_sound_variant("walk_dirt");
            return;
        }
        if (metal.contains(terrain)) {
            play_plmove_sound_variant("walk_metal");
            return;
        }
        if (chain_fence.contains(terrain)) {
            play_plmove_sound_variant("clear_obstacle");
            return;
        }

        play_plmove_sound_variant("walk_tarmac");
    }
}

void sfx::do_obstacle(const std::string& obst) {
    if (test_mode) { return; }
    // Take volume at 60dB
    int heard_volume = sfx::get_heard_volume(get_avatar().bub_pos(), 60);

    static const std::set<std::string> water = {
        "t_water_sh",  "t_water_dp",  "t_water_moving_sh", "t_water_moving_dp",
        "t_swater_sh", "t_swater_dp", "t_water_pool",      "t_sewage",
    };
    if (sfx::has_variant_sound("plmove", obst)) {
        play_variant_sound("plmove", obst, heard_volume, 0_degrees, 0.8, 1.2);
    } else if (water.contains(obst)) {
        play_variant_sound("plmove", "walk_water", heard_volume, 0_degrees, 0.8, 1.2);
    } else {
        play_variant_sound("plmove", "clear_obstacle", heard_volume, 0_degrees, 0.8, 1.2);
    }
    // prevent footsteps from triggering
    start_sfx_timestamp = std::chrono::high_resolution_clock::now();
}

void sfx::play_activity_sound(const std::string& id, const std::string& variant, int volume) {
    if (test_mode) { return; }

    avatar& player_character = get_avatar();
    if (act != player_character.activity->id()) {
        act = player_character.activity->id();
        play_ambient_variant_sound(id, variant, volume, channel::player_activities, 0);
    }
}

void sfx::end_activity_sounds() {
    if (test_mode) { return; }
    act = activity_id::NULL_ID();
    fade_audio_channel(channel::player_activities, 2000);
}

#else // if defined(SDL_SOUND)

/** Dummy implementations for builds without sound */
/*@{*/
void sfx::load_sound_effects(const JsonObject&) {}
void sfx::load_sound_effect_preload(const JsonObject&) {}
void sfx::load_playlist(const JsonObject&) {}
void sfx::play_variant_sound(
    const std::string&, const std::string&, int, units::angle, double, double, const bool) {}
void sfx::play_variant_sound(const std::string&, const std::string&, int, bool) {}
void sfx::play_ambient_variant_sound(
    const std::string&, const std::string&, int, channel, int, double, int) {}
void sfx::play_activity_sound(const std::string&, const std::string&, int) {}
void sfx::end_activity_sounds() {}
void sfx::generate_gun_sound(const tripoint_bub_ms&, const item&, const short& origin_vol) {}
void sfx::generate_melee_sound(
    const tripoint_bub_ms&, const tripoint_bub_ms&, bool, bool, const std::string&) {}
void sfx::do_hearing_loss(int) {}
void sfx::remove_hearing_loss() {}
void sfx::do_projectile_hit(const Creature&) {}
void sfx::do_footstep() {}
void sfx::do_danger_music() {}
void sfx::do_vehicle_engine_sfx() {}
void sfx::do_vehicle_exterior_engine_sfx() {}
void sfx::do_ambient() {}
void sfx::fade_audio_group(group, int) {}
void sfx::fade_audio_channel(channel, int) {}
bool sfx::is_channel_playing(channel) { return false; }
int sfx::set_channel_volume(channel, int) { return 0; }
bool sfx::has_variant_sound(const std::string&, const std::string&) { return false; }
void sfx::stop_sound_effect_fade(channel, int) {}
void sfx::stop_sound_effect_timed(channel, int) {}
void sfx::do_player_death_hurt(const player&, bool) {}
void sfx::do_fatigue() {}
void sfx::do_obstacle(const std::string&) {}
/*@}*/

#endif // if defined(SDL_SOUND)
