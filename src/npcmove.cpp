#include "action_time_scale.h"
#include "active_item_cache.h"
#include "activity_handlers.h"
#include "bionics.h"
#include "bodypart.h"
#include "calendar.h"
#include "catalua.h"
#include "catalua_coord.h"
#include "catalua_hooks.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "character.h"
#include "character_functions.h"
#include "character_id.h"
#include "character_turn.h"
#include "clzones.h"
#include "creature_tracker.h"
#include "damage.h"
#include "debug.h"
#include "dispersion.h"
#include "effect.h"
#include "enums.h"
#include "explosion.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "gates.h"
#include "gun_mode.h"
#include "init.h"
#include "item.h"
#include "item_contents.h"
#include "item_functions.h"
#include "itype.h"
#include "iuse.h"
#include "iuse_actor.h"
#include "line.h"
#include "map/field.h"
#include "map/field_type.h"
#include "map/map.h"
#include "map/mapdata.h"
#include "map_iterator.h"
#include "messages.h"
#include "mission.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h" // IWYU pragma: associated
#include "npc_ai.h"
#include "npc_class.h"
#include "npctalk.h"
#include "options.h"
#include "overmap/overmap.h"
#include "overmap/overmap_location.h"
#include "overmap/overmapbuffer.h"
#include "overmap/overmapbuffer_registry.h"
#include "player_activity.h"
#include "pldata.h"
#include "profile.h"
#include "projectile.h"
#include "ranged.h"
#include "reload/reload.h"
#include "reload/reload_selection.h"
#include "ret_val.h"
#include "rng.h"
#include "sounds.h"
#include "stomach.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "utils/algo.h"
#include "value_ptr.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "vehicle/vpart_range.h"
#include "visitable.h"

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <numeric>
#include <ostream>
#include <tuple>
#include <unordered_map>


static const std::unordered_map<std::string, npc_action> npc_action_map = {
    {"npc_pause", npc_pause},
    {"npc_reload", npc_reload},
    {"npc_sleep", npc_sleep},
    {"npc_pickup", npc_pickup},
    {"npc_heal", npc_heal},
    {"npc_use_painkiller", npc_use_painkiller},
    {"npc_drop_items", npc_drop_items},
    {"npc_flee", npc_flee},
    {"npc_melee", npc_melee},
    {"npc_shoot", npc_shoot},
    {"npc_look_for_player", npc_look_for_player},
    {"npc_heal_player", npc_heal_player},
    {"npc_follow_player", npc_follow_player},
    {"npc_follow_embarked", npc_follow_embarked},
    {"npc_talk_to_player", npc_talk_to_player},
    {"npc_mug_player", npc_mug_player},
    {"npc_goto_to_this_pos", npc_goto_to_this_pos},
    {"npc_goto_destination", npc_goto_destination},
    {"npc_avoid_friendly_fire", npc_avoid_friendly_fire},
    {"npc_escape_explosion", npc_escape_explosion},
    {"npc_reach_attack", npc_reach_attack},
    {"npc_aim", npc_aim},
    {"npc_investigate_sound", npc_investigate_sound},
    {"npc_return_to_guard_pos", npc_return_to_guard_pos},
    {"npc_player_activity", npc_player_activity},
};


std::string npc_action_name(npc_action action);

void print_action(const char* prepend, npc_action action);

bool compare_sound_alert(const dangerous_sound& sound_a, const dangerous_sound& sound_b);


void npc::execute_action(const std::string& action_str) {
    if (const auto _act = npc_action_map.find(action_str); _act != npc_action_map.end()) {
        this->execute_action(_act->second);
    } else {
        debugmsg("Unknown npc action %s", action_str);
    }
}

void npc::execute_action(npc_action action) {
    ZoneScoped;
    const auto oldmoves = moves;
    switch (action) {
        case npc_pause:
            act_pause();
            break;
        case npc_reload:
            act_reload();
            break;
        case npc_investigate_sound:
            act_investigate_sound();
            break;
        case npc_return_to_guard_pos:
            act_return_to_guard_pos();
            break;
        case npc_sleep:
            act_sleep();
            break;
        case npc_pickup:
            act_pickup();
            break;
        case npc_heal:
            act_heal();
            break;
        case npc_use_painkiller:
            act_use_painkiller();
            break;
        case npc_drop_items:
            act_drop_items();
            break;
        case npc_flee:
            act_flee();
            break;
        case npc_reach_attack:
            act_reach_attack();
            break;
        case npc_melee:
            act_melee();
            break;
        case npc_aim:
            act_aim();
            break;
        case npc_shoot:
            act_shoot();
            break;
        case npc_look_for_player:
            act_look_for_player();
            break;
        case npc_heal_player:
            act_heal_player();
            break;
        case npc_follow_player:
            act_follow_player();
            break;
        case npc_follow_embarked:
            act_follow_embarked();
            break;
        case npc_talk_to_player:
            act_talk_to_player();
            break;
        case npc_mug_player:
            act_mug_player();
            break;
        case npc_goto_to_this_pos:
            act_goto_to_this_pos();
            break;
        case npc_goto_destination:
            act_goto_destination();
            break;
        case npc_avoid_friendly_fire:
            act_avoid_friendly_fire();
            break;
        case npc_escape_explosion:
            act_escape_explosion();
            break;
        case npc_player_activity:
            act_player_activity();
            break;
        case npc_undecided:
            act_undecided();
            break;
        case npc_noop:
            act_noop();
            return;
        default: {
            debugmsg("Unknown NPC action (%d)", action);
        }
    }

    if (oldmoves == moves) {
        add_msg(m_debug, "NPC didn't use its moves.  Action %s (%d).", npc_action_name(action),
                action);
    }
}

void npc::act_pause() {
    ZoneScoped;
    move_pause();
    return;
}

void npc::act_reload() {
    ZoneScoped;
    do_reload(primary_weapon());
}

void npc::act_investigate_sound() {
    ZoneScoped;
    auto cur_pos = bub_pos();
    update_path(abs_to_bub(ai_cache.s_abs_pos));
    move_to_next();
    if (bub_pos() == cur_pos) { ai_cache.stuck += 1; }
}

void npc::act_return_to_guard_pos() {
    ZoneScoped;
    const auto local_guard_pos = abs_to_bub(*ai_cache.guard_pos);
    update_path(local_guard_pos);
    if (bub_pos() == local_guard_pos || path.empty()) {
        move_pause();
        ai_cache.guard_pos = std::nullopt;
        path.clear();
    } else {
        move_to_next();
    }
}

void npc::act_sleep() {
    ZoneScoped;
    Character& player_character = get_player_character();
    // TODO: Allow stims when not too tired
    auto sleep_or_move = [this, &player_character](tripoint_abs_ms target_pos) {
        // TODO: Handle empty path better
        if (target_pos == abs_pos() || path.empty()) {
            sleep_at_this_pos = std::nullopt;
            move_pause();
            if (!has_effect(effect_lying_down)) {
                activate_bionic_by_id(bio_soporific);
                add_effect(effect_lying_down, 30_minutes, bodypart_str_id::NULL_ID(), 1);
                if (player_character.sees(*this) && !player_character.in_sleep_state()) {
                    add_msg(_("%s lies down to sleep."), name);
                }
            }
        } else {
            move_to_next();
        }
    };

    if (sleep_at_this_pos.has_value()) {
        sleep_or_move(*sleep_at_this_pos);
        return;
    }

    // Find a nice spot to sleep
    int best_sleepy = character_funcs::rate_sleep_spot(*this, bub_pos());
    auto best_spot = bub_pos();
    for (const auto& p : closest_points_first(bub_pos(), 6)) {
        if (!could_move_onto(p) || !g->is_empty(p)) { continue; }

        // TODO: Blankets when it's cold
        const int sleepy = character_funcs::rate_sleep_spot(*this, p);
        if (sleepy > best_sleepy) {
            best_sleepy = sleepy;
            best_spot = p;
        }
    }
    if (is_walking_with()) { complain_about("napping", 30_minutes, _("<warn_sleep>")); }
    sleep_at_this_pos = bub_to_abs(best_spot);
    update_path(best_spot);
    sleep_or_move(*sleep_at_this_pos);
}

void npc::act_pickup() {
    ZoneScoped;
    pick_up_item();
    return;
}

void npc::act_heal() {
    ZoneScoped;
    heal_self();
    return;
}

void npc::act_use_painkiller() {
    ZoneScoped;
    use_painkiller();
    return;
}

void npc::act_drop_items() {
    ZoneScoped;
    /* NPCs can't choose this action anymore, but at least it works */
    drop_invalid_inventory();
    /* drop_items is still broken
     * drop_items( weight_carried() - weight_capacity(),
     *             volume_carried() - volume_capacity() );
     */
    move_pause();
    return;
}

void npc::act_flee() {
    ZoneScoped;
    auto tar = good_escape_direction(false);
    if (path.empty()) {
        move_to(tar);
    } else {
        move_to_next();
    }
    return;
}

void npc::act_reach_attack() {
    ZoneScoped;
    auto* cur = current_target();
    auto tar = cur != nullptr ? cur->bub_pos() : bub_pos();
    if (can_use_offensive_cbm()) { activate_bionic_by_id(bio_hydraulics); }
    reach_attack(tar);
    return;
}

void npc::act_melee() {
    ZoneScoped;
    auto* cur = current_target();
    auto tar = cur != nullptr ? cur->bub_pos() : bub_pos();
    Character& player_character = get_player_character();
    update_path(tar);
    if (path.size() > 1) {
        move_to_next();
    } else if (path.size() == 1) {
        if (cur != nullptr) {
            if (can_use_offensive_cbm()) { activate_bionic_by_id(bio_hydraulics); }
            melee_attack(*cur, true);
        }
    } else {
        look_for_player(player_character);
    }
    return;
}

void npc::act_aim() {
    ZoneScoped;
    gun_mode mode =
        cbm_active.is_null()
            ? primary_weapon().gun_current_mode()
            : cbm_fake_active->gun_current_mode();
    if (!mode) {
        std::string error_weapon =
            cbm_active.is_null() ? primary_weapon().tname() : cbm_fake_active->tname();
        debugmsg("NPC tried to aim %s without valid mode.", error_weapon);
    }

    bool did_aim = aim();
    if (!did_aim) {
        debugmsg("%s is trying to aim, but failing repeatedly", disp_name().c_str());
        set_moves(0);
    }
}

void npc::act_shoot() {
    ZoneScoped;
    auto* cur = current_target();
    auto tar = cur != nullptr ? cur->bub_pos() : bub_pos();
    gun_mode mode =
        cbm_active.is_null()
            ? primary_weapon().gun_current_mode()
            : cbm_fake_active->gun_current_mode();
    if (!mode) {
        std::string error_weapon =
            cbm_active.is_null() ? primary_weapon().tname() : cbm_fake_active->tname();
        debugmsg("NPC tried to shoot %s without valid mode.", error_weapon);
    }

    aim();
    if (is_hallucination()) {
        pretend_fire(this, mode.qty, *mode);
    } else {
        add_msg(m_debug, "%s recoil on firing: %s", name, recoil);
        ranged::fire_gun(*this, tar, mode.qty, *mode, nullptr);
        // Clear the ranged cbm entry and item so next turn a new comparison is made.
        if (!cbm_active.is_null()) { discharge_cbm_weapon(); }
    }
    // Important, once they've fired their gun, wield calculations have to be redone
    // else they'll fail to realize when they run out of ammo.
    clear_npc_ai_info_cache(npc_ai_info::range);
    return;
}

void npc::act_look_for_player() {
    ZoneScoped;
    Character& player_character = get_player_character();
    if (saw_player_recently() && last_player_seen_pos && sees(*last_player_seen_pos)) {
        update_path(*last_player_seen_pos);
        move_to_next();
    } else {
        look_for_player(player_character);
    }
    return;
}

void npc::act_heal_player() {
    ZoneScoped;
    player* patient = dynamic_cast<player*>(current_ally());
    if (patient) {
        update_path(patient->bub_pos());
        if (path.size() == 1) { // We're adjacent to u, and thus can heal u
            heal_player(*patient);
        } else if (!path.empty()) {
            say(_("Hold still %s, I'm coming to help you."), patient->disp_name());
            move_to_next();
        } else {
            move_pause();
        }
    }
    return;
}

void npc::act_follow_player() {
    ZoneScoped;
    Character& player_character = get_player_character();
    update_path(player_character.bub_pos());
    move_mode =
        rules.has_flag(ally_rule::move_own_pace)
            ? ((static_cast<int>(path.size()) > follow_distance() * 4) ? CMM_RUN : CMM_WALK)
            : player_character.get_movement_mode();
    if (static_cast<int>(path.size()) <= follow_distance()
        && player_character.bub_pos().z() == bub_pos().z()) { // We're close enough to u.
        move_pause();
    } else if (!path.empty()) {
        move_to_next();
    } else {
        move_pause();
    }
    // TODO: Make it only happen when it's safe
    complain();
    return;
}

void npc::act_follow_embarked() {
    ZoneScoped;
    Character& player_character = get_player_character();
    map& here = get_map();
    move_mode =
        rules.has_flag(ally_rule::move_own_pace)
            ? ((static_cast<int>(path.size()) > follow_distance() * 4) ? CMM_RUN : CMM_WALK)
            : player_character.get_movement_mode();
    const optional_vpart_position vp = here.veh_at(player_character.bub_pos());

    if (!vp) {
        debugmsg("Following an embarked player with no vehicle at their location?");
        // TODO: change to wait? - for now pause
        move_pause();
        return;
    }
    vehicle* const veh = &vp->vehicle();

    // Try to find the last destination
    // This is mount point, not actual position
    point last_dest(INT_MIN, INT_MIN);
    if (!path.empty() && veh_pointer_or_null(here.veh_at(path[path.size() - 1])) == veh) {
        last_dest = vp->mount().xy().raw();
    }

    // Prioritize last found path, then seats
    // Don't change spots if ours is nice
    int my_spot = -1;
    std::vector<std::pair<int, int>> seats;
    for (const vpart_reference& vp : veh->get_avail_parts(VPFLAG_BOARDABLE)) {
        const player* passenger = veh->get_passenger(vp.part_index());
        if (passenger != this && passenger != nullptr) { continue; }

        // a seat is available if either unassigned or assigned to us
        auto available_seat = [&](const vehicle_part& pt) {
            if (!pt.is_seat()) { return false; }
            const npc* who = pt.crew();
            return !who || who->getID() == getID();
        };

        const vehicle_part& pt = vp.part();

        int priority = 0;

        if (vp.mount().xy().raw() == last_dest) {
            // Shares mount point with last known path
            // We probably wanted to go there in the last turn
            priority = 4;

        } else if (available_seat(pt)) {
            // Assuming player "owns" a sensible vehicle seats should be in good spots to occupy
            // Prefer our assigned seat if we have one
            const npc* who = pt.crew();
            priority = who && who->getID() == getID() ? 3 : 2;

        } else if (vp.is_inside()) {
            priority = 1;
        }

        if (passenger == this) { my_spot = priority; }

        seats.emplace_back(priority, static_cast<int>(vp.part_index()));
    }

    if (my_spot >= 3) {
        // We won't get any better, so don't try
        move_pause();
        return;
    }

    std::sort(
        seats.begin(), seats.end(), [](const std::pair<int, int>& l, const std::pair<int, int>& r) {
            return l.first > r.first;
        });

    if (seats.empty()) {
        // TODO: be angry at player, switch to wait or leave - for now pause
        move_pause();
        return;
    }

    // Only check few best seats - pathfinding can get expensive
    const size_t try_max = std::min<size_t>(4, seats.size());
    for (size_t i = 0; i < try_max; i++) {
        if (seats[i].first <= my_spot) {
            // We have a nicer spot than this
            // Note: this will make NPCs steal player's seat...
            break;
        }

        const int cur_part = seats[i].second;

        tripoint_bub_ms pp = veh->bub_part_location(cur_part);
        update_path(pp, true);
        if (!path.empty()) {
            // All is fine
            move_to_next();
            break;
        }
    }

    // TODO: Check the rest
    move_pause();
}

void npc::act_talk_to_player() {
    ZoneScoped;
    talk_to_u();
    moves = 0;
    return;
}

void npc::act_mug_player() {
    ZoneScoped;
    Character& player_character = get_player_character();
    mug_player(player_character);
    return;
}

void npc::act_goto_to_this_pos() {
    ZoneScoped;
    if (!goto_to_this_pos.has_value()) {
        debugmsg("npc_goto_to_this_pos set to true, but no target set");
        return;
    }
    update_path(abs_to_bub(goto_to_this_pos.value()));
    move_to_next();

    if (abs_pos() == goto_to_this_pos.value()) {
        add_msg(m_debug, "%s reached target", disp_name());
        goto_to_this_pos = std::nullopt;
    }
    return;
}

void npc::act_goto_destination() {
    ZoneScoped;
    go_to_omt_destination();
    return;
}

void npc::act_avoid_friendly_fire() {
    ZoneScoped;
    avoid_friendly_fire();
    return;
}

void npc::act_escape_explosion() {
    ZoneScoped;
    escape_explosion();
    return;
}

void npc::act_player_activity() {
    ZoneScoped;
    do_player_activity();
    return;
}

void npc::act_undecided() {
    ZoneScoped;
    complain();
    move_pause();
    return;
}

void npc::act_noop() {
    ZoneScoped;
    add_msg(m_debug, "%s skips turn (noop)", disp_name());
    return;
}


double npc::confidence_mult() const {
    if (!is_player_ally() || is_player()) { return 1.0f; }

    switch (rules.aim) {
        case aim_rule::WHEN_CONVENIENT:
            return emergency() ? 1.5f : 1.0f;
        case aim_rule::SPRAY:
            return 2.0f;
        case aim_rule::PRECISE:
            return emergency() ? 1.0f : 0.75f;
        case aim_rule::STRICTLY_PRECISE:
            return 0.5f;
    }

    return 1.0f;
}

int npc::confident_shoot_range(const item& it, int recoil) const {
    if (!it.is_gun()) { return 0; }
    const auto gun_mode_cmp =
        [](const std::pair<gun_mode_id, gun_mode>& lhs,
           const std::pair<gun_mode_id, gun_mode>& rhs) { return lhs.second.qty < rhs.second.qty; };
    std::map<gun_mode_id, gun_mode> modes = it.gun_all_modes();
    if (modes.empty()) {
        debugmsg("%s has no gun modes", it.tname());
        return 0;
    }
    auto best = std::min_element(modes.begin(), modes.end(), gun_mode_cmp);
    return confident_gun_mode_range((*best).second, recoil);
}

int npc::confident_gun_mode_range(const gun_mode& gun, int at_recoil) const {
    if (!gun || gun.melee()) { return 0; }

    const std::optional<shape_factory> shaped = ranged::get_shape_factory(*gun.target);
    if (shaped) { return static_cast<int>(shaped->get_range()) - 1; }

    // Doesn't use calculate_dispersion because that requires a map
    // TODO: Turn this into a common function.
    int gun_recoil = gun->gun_recoil();
    int eff_recoil = at_recoil + (gun.qty > 1 ? ranged::burst_penalty(*this, *gun, gun_recoil) : 0);
    dispersion_sources mode_disp = ranged::get_weapon_dispersion(*this, *gun);
    mode_disp.add_range(eff_recoil);
    double max_dispersion = mode_disp.max();
    if (gun->ammo_current()) { max_dispersion += gun->ammo_current()->ammo->dispersion; }
    double even_chance_range = range_with_even_chance_of_good_hit(max_dispersion);
    double confident_range = even_chance_range * confidence_mult();
    add_msg(m_debug, "%s confident_gun (%s<=%.2f) at %.1f", gun->tname(), gun.name(),
            confident_range, max_dispersion);
    return std::max<int>(confident_range, 1);
}

int npc::confident_throw_range(const item& thrown, Creature* target) const {
    double average_dispersion = ranged::throwing_dispersion(*this, thrown, target, false) / 2.0;
    double even_chance_range =
        (target == nullptr ? 0.5 : target->ranged_target_size()) / average_dispersion;
    double confident_range = even_chance_range * confidence_mult();
    add_msg(m_debug, "confident_throw_range == %d", static_cast<int>(confident_range));
    return static_cast<int>(confident_range);
}

auto item::ideal_ranged_dps(const Character& who, std::optional<gun_mode>& mode) const -> double {
    if (!is_gun() || is_gunmod() || !mode) { return 0; }
    damage_instance gun_damage = this->gun_damage();
    if (ammo_current()) {
        itype_id ammo = ammo_current();
        gun_damage.add(ammo->ammo->damage);
    } else if (ammo_default()) {
        itype_id ammo = ammo_default();
        gun_damage.add(ammo->ammo->damage);
    }
    int burst_size = mode->qty;
    if (burst_size <= 0) {
        debugmsg("gun_mode for %s has burst size of 0", this->tname());
        burst_size = 1;
    }
    float damage_factor = gun_damage.total_damage() * burst_size;

    int move_cost = ranged::time_to_attack(who, *this, nullptr);
    if (ammo_remaining() == 0) {
        int reload_cost =
            get_reload_time() + who.encumb(body_part_hand_l) + who.encumb(body_part_hand_r);
        // HACK: Doesn't check how much ammo they'll actually get from the reload. Because we don't
        // know. DPS is less impacted the larger the magazine being swapped.
        reload_cost /= magazine_integral() ? 1 : std::max(1, ammo_capacity() / burst_size);
        move_cost += reload_cost;
    }
    std::vector<ranged::aim_type> aim_types = ranged::get_aim_types(who, *this);
    auto regular = std::find_if(aim_types.begin(), aim_types.end(), [](const ranged::aim_type& at) {
        return at.action == std::string("AIMED_SHOT");
    });
    if (regular == aim_types.end()) {
        debugmsg("Could not find REGULAR aim type for gun %s", tname());
        return 0;
    }
    move_cost += ranged::gun_engagement_moves(who, *this, (*regular).threshold);

    double dps = damage_factor / (move_cost / 100.0f);

    return dps;
}

// Index defaults to -1, i.e., wielded weapon
bool npc::wont_hit_friend(const tripoint_bub_ms& tar, const item& it, bool throwing) const {
    // TODO: Get actual dispersion instead of extracting it (badly) from confident range
    int confident =
        throwing ? confident_throw_range(it, nullptr)
                 : confident_shoot_range(it, ranged::recoil_total(*this));
    // if there is no confidence at using weapon, it's not used at range
    // zero confidence leads to divide by zero otherwise
    if (confident < 1) { return true; }

    if (rl_dist(bub_pos(), tar) == 1) {
        return true; // If we're *really* sure that our aim is dead-on
    }

    units::angle target_angle = coord_to_angle(bub_pos(), tar);

    // TODO: Base on dispersion
    units::angle safe_angle = 30_degrees;

    for (const auto& fr : ai_cache.friends) {
        const shared_ptr_fast<Creature> ally_p = fr.lock();
        if (!ally_p) { continue; }
        const Creature& ally = *ally_p;

        // TODO: Extract common functions with turret target selection
        units::angle safe_angle_ally = safe_angle;
        int ally_dist = rl_dist(bub_pos(), ally.bub_pos());
        // Skip adjacent allies - ballistics code now protects them
        if (ally_dist <= 1) { continue; }
        if (ally_dist < 3) { safe_angle_ally += (3 - ally_dist) * 30_degrees; }

        units::angle ally_angle = coord_to_angle(bub_pos(), ally.bub_pos());
        units::angle angle_diff = units::fabs(ally_angle - target_angle);
        angle_diff = std::min(360_degrees - angle_diff, angle_diff);
        if (angle_diff < safe_angle_ally) {
            // TODO: Disable NPC whining is it's other NPC who prevents aiming
            return false;
        }
    }

    return true;
}

bool npc::enough_time_to_reload(const item& gun) const {
    int rltime =
        item_reload_cost(gun, *item::spawn_temporary(gun.ammo_default()), gun.ammo_capacity());
    const float turns_til_reloaded = static_cast<float>(rltime) / get_speed();

    const Creature* target = current_target();
    if (target == nullptr) {
        // No target, plenty of time to reload
        return true;
    }

    const auto distance = rl_dist(bub_pos(), target->bub_pos());
    const float target_speed = target->speed_rating();
    const float turns_til_reached = distance / target_speed;
    if (target->is_player() || target->is_npc()) {
        auto& c = dynamic_cast<const Character&>(*target);
        if (sees(c) && c.primary_weapon().is_gun() && rltime > 200
            && c.primary_weapon().gun_range(true) > distance + turns_til_reloaded / target_speed) {
            // Don't take longer than 2 turns if player has a gun
            return false;
        }
    }

    // TODO: Handle monsters with ranged attacks and players with CBMs
    return turns_til_reloaded < turns_til_reached;
}

bool npc::aim() {
    gun_mode mode =
        cbm_active.is_null()
            ? primary_weapon().gun_current_mode()
            : cbm_fake_active->gun_current_mode();
    if (!mode) {
        std::string error_weapon =
            cbm_active.is_null() ? primary_weapon().tname() : cbm_fake_active->tname();
        debugmsg("NPC tried to aim %s without valid mode.", error_weapon);
    }

    bool did_aim = false;
    double aim_amount = ranged::aim_per_move(*this, *mode.target, recoil);
    while (aim_amount > 0 && recoil > 0 && moves > 0) {
        did_aim = true;
        moves--;
        recoil -= aim_amount;
        recoil = std::max(0.0, recoil);
        aim_amount = ranged::aim_per_move(*this, *mode.target, recoil);
    }

    return did_aim;
}


bool npc::do_player_activity() {
    ZoneScopedN("npc_do_player_activity");

    const auto old_moves = moves;
    if (moves > 200 && activity
        && (activity->is_multi_type() || activity->id() == activity_id("ACT_TIDY_UP"))) {
        // a huge backlog of a multi-activity type can forever loop
        // instead; just scan the map ONCE for a task to do, and if it returns false
        // then stop scanning, abandon the activity, and kill the backlog of moves.
        if (!generic_multi_activity_handler(*activity, *this->as_player(), true)) {
            revert_after_activity();
            set_moves(0);
            return true;
        }
    }
    // the multi-activity types can sometimes cancel the activity, and return without using up any
    // moves. ( when they are setting a destination etc. ) normally this isn't a problem, but in the
    // main game loop, if the NPC has a huge backlog of moves; then each of these occurrences will
    // nudge the infinite loop counter up by one. ( even if other move-using things occur inbetween
    // ) so here - if no moves are used in a multi-type activity do_turn(), then subtract a nominal
    // amount to satisfy the infinite loop counter.
    const auto multi_type = activity ? activity->is_multi_type() : false;
    const auto moves_before = moves;
    {
        ZoneScopedN("npc_do_player_activity_loop");
        auto activity_turns = 0;
        while (moves > 0 && activity && *activity) {
            activity->do_turn(*this);
            activity_turns++;
            if (!is_active()) {
                ZoneValue(activity_turns);
                return true;
            }
        }
        ZoneValue(activity_turns);
    }
    if (multi_type && moves == moves_before) { moves -= 1; }
    /* if the activity is finished, grab any backlog or change the mission */
    if (!has_destination() && (!activity || !*activity)) {
        if (!backlog.empty()) {
            activity = std::move(backlog.front());
            backlog.pop_front();
            current_activity_id = activity->id();
        } else {
            const std::string failure_msg = consume_activity_failure_message();
            const bool suppressed = consume_suppress_activity_complete_message();
            if (!failure_msg.empty()) {
                add_msg(m_info, failure_msg);
            } else if (is_player_ally() && !suppressed) {
                add_msg(m_info, string_format(_("%s completed the assigned task."), disp_name()));
            }
            current_activity_id = activity_id::NULL_ID();
            revert_after_activity();
            // if we loaded after being out of the bubble for a while, we might have more
            // moves than we need, so clear them
            set_moves(0);
        }
    }
    return moves != old_moves;
}


static void npc_throw(npc& np, item& it, const tripoint_bub_ms& pos) {
    if (get_player_character().sees(np)) { add_msg(_("%1$s throws a %2$s."), np.name, it.tname()); }

    detached_ptr<item> det = it.count_by_charges() ? it.split(1) : it.detach();

    if (!np.is_hallucination()) { // hallucinations only pretend to throw
        ranged::throw_item(np, pos, std::move(det), std::nullopt);
    }
    np.clear_npc_ai_info_cache(npc_ai_info::range);
}

bool npc::alt_attack() {
    if ((is_player_ally() && !rules.has_flag(ally_rule::use_grenades)) || is_hallucination()) {
        return false;
    }

    Creature* critter = current_target();
    if (critter == nullptr) {
        // This function shouldn't be called...
        debugmsg("npc::alt_attack() called with no target");
        move_pause();
        return false;
    }

    auto tar = critter->bub_pos();

    const int dist = rl_dist(bub_pos(), tar);
    item* used = nullptr;
    // Remember if we have an item that is dangerous to hold
    bool used_dangerous = false;

    // TODO: The active bomb with shortest fuse should be thrown first
    const auto check_alt_item = [&used, &used_dangerous, dist, this](item& it) {
        const bool dangerous = it.has_flag(flag_NPC_THROW_NOW);
        if (!dangerous && used_dangerous) { return; }

        // Guns with bayonets inherit the thrown flag, prevent NPCs from throwing it.
        if (it.is_gun()) { return; }

        // Not alt attack
        if (!dangerous && !it.has_flag(flag_NPC_ALT_ATTACK)) { return; }

        // TODO: Non-thrown alt items
        if (!dangerous && throw_range(it) < dist) { return; }

        // Low priority items
        if (!dangerous && used != nullptr) { return; }

        used = &it;
        used_dangerous = used_dangerous || dangerous;
    };

    check_alt_item(primary_weapon());
    for (auto& sl : inv.const_slice()) {
        // TODO: Cached values - an itype slot maybe?
        check_alt_item(*sl->front());
    }

    if (used == nullptr) { return false; }

    int weapon_index = get_item_position(used);
    if (weapon_index == INT_MIN) {
        debugmsg("npc::alt_attack() couldn't find expected item %s", used->tname());
        return false;
    }

    // Are we going to throw this item?
    if (!used->is_active() && used->has_flag(flag_NPC_ACTIVATE)) {
        activate_item(weapon_index);
        // Note: intentional lack of return here
        // We want to ignore player-centric rules to avoid carrying live explosives
        // TODO: Non-grenades
    }

    // We are throwing it!
    int conf = confident_throw_range(*used, critter);
    const bool wont_hit = wont_hit_friend(tar, *used, true);
    if (dist <= conf && wont_hit) {
        npc_throw(*this, *used, tar);
        return true;
    }

    if (wont_hit) {
        // Within this block, our chosen target is outside of our range
        update_path(tar);
        move_to_next(); // Move towards the target
    }

    // Danger of friendly fire
    if (!wont_hit && !used_dangerous) {
        // Safe to hold on to, for now
        // Maneuver around player
        avoid_friendly_fire();
        return true;
    }

    map& here = get_map();
    // We need to throw this live (grenade, etc) NOW! Pick another target?
    for (int dist = 2; dist <= conf; dist++) {
        for (const tripoint_bub_ms& pt : here.points_in_radius(bub_pos(), dist)) {
            const monster* const target_ptr = g->critter_at<monster>(pt);
            int newdist = rl_dist(bub_pos(), pt);
            // TODO: Change "newdist >= 2" to "newdist >= safe_distance(used)"
            if (newdist <= conf && newdist >= 2 && target_ptr && wont_hit_friend(pt, *used, true)) {
                // Friendlyfire-safe!
                ai_cache.target = g->shared_from(*target_ptr);
                if (!one_in(100)) {
                    // Just to prevent infinite loops...
                    if (alt_attack()) { return true; }
                }
                return false;
            }
        }
    }
    /* If we have reached THIS point, there's no acceptable monster to throw our
     * grenade or whatever at.  Since it's about to go off in our hands, better to
     * just chuck it as far away as possible--while being friendly-safe.
     */
    int best_dist = 0;
    for (int dist = 2; dist <= conf; dist++) {
        for (const tripoint_bub_ms& pt : here.points_in_radius(bub_pos(), dist)) {
            int new_dist = rl_dist(bub_pos(), pt);
            if (new_dist > best_dist && wont_hit_friend(pt, *used, true)) {
                best_dist = new_dist;
                tar = pt;
            }
        }
    }
    /* Even if tar.x/tar.y didn't get set by the above loop, throw it anyway.  They
     * should be equal to the original location of our target, and risking friendly
     * fire is better than holding on to a live grenade / whatever.
     */
    npc_throw(*this, *used, tar);
    return true;
}


// We want our food to:
// Provide enough nutrition and quench
// Not provide too much of either (don't waste food)
// Not be unhealthy
// Not have side effects
// Be eaten before it rots (favor soon-to-rot perishables)


void npc::mug_player(Character& mark) {
    if (mark.is_armed()) { make_angry(); }

    if (rl_dist(bub_pos(), mark.bub_pos()) > 1) { // We have to travel
        update_path(mark.bub_pos());
        move_to_next();
        return;
    }

    Character& player_character = get_player_character();
    const bool u_see = player_character.sees(*this) || player_character.sees(mark);
    if (mark.cash > 0) {
        if (!is_hallucination()) { // hallucinations can't take items
            cash += mark.cash;
            mark.cash = 0;
        }
        moves = 0;
        // Describe the action
        if (mark.is_npc()) {
            if (u_see) { add_msg(_("%1$s takes %2$s's money!"), name, mark.name); }
        } else {
            add_msg(m_bad, _("%s takes your money!"), name);
        }
        return;
    }

    // We already have their money; take some goodies!
    // value_mod affects at what point we "take the money and run"
    // A lower value means we'll take more stuff
    double value_mod =
        1 - ((10 - personality.bravery) * .05) - ((10 - personality.aggression) * .04)
        - ((10 - personality.collector) * .06);
    if (!mark.is_npc()) {
        value_mod += (op_of_u.fear * .08);
        value_mod -= ((8 - op_of_u.value) * .07);
    }
    double best_value = minimum_item_value() * value_mod;
    item* to_steal = nullptr;
    const_invslice slice = mark.inv_const_slice();
    for (const std::vector<item*>* stack : slice) {
        item& front_stack = *stack->front();
        if (value(front_stack) >= best_value && can_pick_volume(front_stack)
            && can_pick_weight(front_stack, true)) {
            best_value = value(front_stack);
            to_steal = &front_stack;
        }
    }
    if (to_steal == nullptr) { // Didn't find anything worthwhile!
        set_attitude(NPCATT_FLEE_TEMP);
        if (!one_in(3)) { say("<done_mugging>"); }
        moves -= 100;
        return;
    }
    if (!is_hallucination()) {
        i_add(to_steal->detach());
        if (mark.is_npc()) {
            if (u_see) {
                add_msg(_("%1$s takes %2$s's %3$s."), name, mark.name, to_steal->tname());
            }
        } else {
            add_msg(m_bad, _("%1$s takes your %2$s."), name, to_steal->tname());
        }
    }
    moves -= 100;
    if (!mark.is_npc()) {
        op_of_u.value -= rng(0, 1); // Decrease the value of the player
    }
}


std::string npc_action_name(npc_action action) {
    switch (action) {
        case npc_undecided:
            return "Undecided";
        case npc_pause:
            return "Pause";
        case npc_reload:
            return "Reload";
        case npc_investigate_sound:
            return "Investigate sound";
        case npc_return_to_guard_pos:
            return "Returning to guard position";
        case npc_sleep:
            return "Sleep";
        case npc_pickup:
            return "Pick up items";
        case npc_heal:
            return "Heal self";
        case npc_use_painkiller:
            return "Use painkillers";
        case npc_drop_items:
            return "Drop items";
        case npc_flee:
            return "Flee";
        case npc_melee:
            return "Melee";
        case npc_reach_attack:
            return "Reach attack";
        case npc_aim:
            return "Aim";
        case npc_shoot:
            return "Shoot";
        case npc_look_for_player:
            return "Look for player";
        case npc_heal_player:
            return "Heal player or ally";
        case npc_follow_player:
            return "Follow player";
        case npc_follow_embarked:
            return "Follow player (embarked)";
        case npc_talk_to_player:
            return "Talk to player";
        case npc_mug_player:
            return "Mug player";
        case npc_goto_to_this_pos:
            return "Go to position";
        case npc_goto_destination:
            return "Go to destination";
        case npc_avoid_friendly_fire:
            return "Avoid friendly fire";
        case npc_escape_explosion:
            return "Escape explosion";
        case npc_player_activity:
            return "Performing activity";
        default:
            return "Unnamed action";
    }
}

void print_action(const char* prepend, npc_action action) {
    if (action != npc_undecided) { add_msg(m_debug, prepend, npc_action_name(action)); }
}

const Creature* npc::current_target() const {
    // TODO: Arguably we should return a shared_ptr to ensure that the returned
    // object stays alive while the caller uses it.  Not doing that for now.
    return ai_cache.target.lock().get();
}

Creature* npc::current_target() {
    // TODO: As above.
    return ai_cache.target.lock().get();
}

const Creature* npc::current_ally() const {
    // TODO: Arguably we should return a shared_ptr to ensure that the returned
    // object stays alive while the caller uses it.  Not doing that for now.
    return ai_cache.ally.lock().get();
}

Creature* npc::current_ally() {
    // TODO: As above.
    return ai_cache.ally.lock().get();
}

// Maybe TODO: Move to Character method and use map methods
static bodypart_str_id bp_affected(npc& who, const efftype_id& effect_type) {
    bodypart_str_id ret = bodypart_str_id::NULL_ID();
    int highest_intensity = INT_MIN;
    for (const body_part bp : all_body_parts) {
        const auto& eff = who.get_effect(effect_type, convert_bp(bp));
        if (!eff.is_null() && eff.get_intensity() > highest_intensity) {
            ret = convert_bp(bp);
            highest_intensity = eff.get_intensity();
        }
    }

    return ret;
}

static std::string distance_string(int range) {
    if (range < 6) {
        return "<danger_close_distance>";
    } else if (range < 11) {
        return "<close_distance>";
    } else if (range < 26) {
        return "<medium_distance>";
    } else {
        return "<far_distance>";
    }
}

// This should really be in npctalk.cpp, along with complain_about and complain
void npc::warn_about(
    const std::string& type, const time_duration& d, const std::string& name, int range,
    const tripoint_bub_ms& danger_pos) {
    std::string snip;
    sounds::sound_t spriority = sounds::sound_t::alert;
    if (type == "monster") {
        snip = is_enemy() ? "<monster_warning_h>" : "<monster_warning>";
    } else if (type == "explosion") {
        snip = is_enemy() ? "<fire_in_the_hole_h>" : "<fire_in_the_hole>";
    } else if (type == "general_danger") {
        snip = is_enemy() ? "<general_danger_h>" : "<general_danger>";
        spriority = sounds::sound_t::speech;
    } else if (type == "relax") {
        snip = is_enemy() ? "<its_safe_h>" : "<its_safe>";
        spriority = sounds::sound_t::speech;
    } else if (type == "kill_npc") {
        snip = is_enemy() ? "<kill_npc_h>" : "<kill_npc>";
    } else if (type == "kill_player") {
        snip = is_enemy() ? "<kill_player_h>" : "";
    } else if (type == "run_away") {
        snip = "<run_away>";
    } else if (type == "cant_flee") {
        snip = "<cant_flee>";
    } else if (type == "fire_bad") {
        snip = "<fire_bad>";
    } else if (type == "speech_noise") {
        snip = "<speech_warning>";
        spriority = sounds::sound_t::speech;
    } else if (type == "combat_noise") {
        snip = "<combat_noise_warning>";
        spriority = sounds::sound_t::speech;
    } else if (type == "movement_noise") {
        snip = "<movement_noise_warning>";
        spriority = sounds::sound_t::speech;
    } else if (type == "heal_self") {
        snip = "<heal_self>";
        spriority = sounds::sound_t::speech;
    } else {
        return;
    }
    const std::string warning_name = "warning_" + type + name;
    if (name.empty()) {
        complain_about(warning_name, d, snip, is_enemy(), spriority);
    } else {
        const std::string range_str =
            range < 1
                ? "<punc>"
                : string_format(_(" %s, %s"), direction_name(direction_from(bub_pos(), danger_pos)),
                                distance_string(range));
        const std::string speech = string_format(_("%s %s%s"), snip, name, range_str);
        complain_about(warning_name, d, speech, is_enemy(), spriority);
    }
}

bool npc::complain_about(
    const std::string& issue, const time_duration& dur, const std::string& speech, const bool force,
    const sounds::sound_t priority) {
    // Don't have a default constructor for time_point, so accessing it in the
    // complaints map is a bit difficult, those lambdas should cover it.
    const auto complain_since = [this](const std::string& key, const time_duration& d) {
        const auto iter = complaints.find(key);
        return iter == complaints.end() || iter->second < calendar::turn - d;
    };
    const auto set_complain_since = [this](const std::string& key) {
        const auto iter = complaints.find(key);
        if (iter == complaints.end()) {
            complaints.emplace(key, calendar::turn);
        } else {
            iter->second = calendar::turn;
        }
    };

    // Don't wake player up with non-serious complaints
    // Stop complaining while asleep
    const bool do_complain =
        force
        || (rules.has_flag(ally_rule::allow_complain) && !get_player_character().in_sleep_state()
            && !in_sleep_state());

    if (complain_since(issue, dur) && do_complain) {
        say(speech, priority);
        set_complain_since(issue);
        return true;
    }
    return false;
}

bool npc::complain() {
    static const std::string infected_string = "infected";
    static const std::string fatigue_string = "fatigue";
    static const std::string bite_string = "bite";
    static const std::string bleed_string = "bleed";
    static const std::string radiation_string = "radiation";
    static const std::string hunger_string = "hunger";
    static const std::string thirst_string = "thirst";

    if (!is_player_ally() || !get_player_character().sees(*this)) { return false; }

    // When infected, complain every (4-intensity) hours
    // At intensity 3, ignore player wanting us to shut up
    if (has_effect(effect_infected)) {
        bodypart_str_id bp = bp_affected(*this, effect_infected);
        const auto& eff = get_effect(effect_infected, bp);
        int intensity = eff.get_intensity();
        const std::string speech = string_format(_("My %s wound is infected…"), body_part_name(bp));
        if (complain_about(infected_string, time_duration::from_hours(4 - intensity), speech,
                           intensity >= 3)) {
            // Only one complaint per turn
            return true;
        }
    }

    // When bitten, complain every hour, but respect restrictions
    if (has_effect(effect_bite)) {
        bodypart_str_id bp = bp_affected(*this, effect_bite);
        const std::string speech =
            string_format(_("The bite wound on my %s looks bad."), body_part_name(bp));
        if (complain_about(bite_string, 1_hours, speech)) { return true; }
    }

    // When tired, complain every 30 minutes
    // If massively tired, ignore restrictions
    if (get_fatigue() > fatigue_levels::tired
        && complain_about(fatigue_string, 30_minutes, _("<yawn>"),
                          get_fatigue() > fatigue_levels::massive - 100)) {
        return true;
    }

    // Radiation every 10 minutes
    if (get_rad() > 90) {
        activate_bionic_by_id(bio_radscrubber);
        std::string speech = _("I'm suffering from radiation sickness…");
        if (complain_about(radiation_string, 10_minutes, speech, get_rad() > 150)) { return true; }
    } else if (!get_rad()) {
        deactivate_bionic_by_id(bio_radscrubber);
    }

    // Hunger every 3-6 hours
    // Since NPCs can't starve to death, respect the rules
    if (get_kcal_percent() < 0.9
        && complain_about(
            hunger_string,
            std::max(3_hours, time_duration::from_minutes(get_kcal_percent() * 60 * 7)),
            _("<hungry>"))) {
        return true;
    }

    // Thirst every 2 hours
    // Since NPCs can't dry to death, respect the rules
    if (get_thirst() > thirst_levels::very_thirsty
        && complain_about(thirst_string, 2_hours, _("<thirsty>"))) {
        return true;
    }

    // Bleeding every 5 minutes
    if (has_effect(effect_bleed)) {
        bodypart_str_id bp = bp_affected(*this, effect_bleed);
        std::string speech = string_format(_("My %s is bleeding!"), body_part_name(bp));
        if (complain_about(bleed_string, 5_minutes, speech)) { return true; }
    }

    return false;
}
