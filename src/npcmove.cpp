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

namespace {
const std::vector<bionic_id> power_cbms = {{
    bio_advreactor,
    bio_furnace,
    bio_reactor,
}};

const std::vector<bionic_id> health_cbms = {{bio_leukocyte, bio_plutfilter}};


} // namespace

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


static bool wants_to_reload(const npc& who, const item& it) {
    if (!who.can_reload(it)) { return false; }

    const int required = it.ammo_required();
    // TODO: Add bandolier check here, once they can be reloaded
    if (required < 1 && !it.is_magazine()) { return false; }

    const int remaining = it.ammo_remaining();
    return remaining < required || remaining < it.ammo_capacity();
}

static bool wants_to_reload_with(const item& weap, const item& ammo, bool danger) {
    // Only reload loose ammo if gun has integral magazine or not in danger.
    bool combat_reload = !ammo.is_magazine() && (danger || weap.magazine_integral());
    // If in danger, only swap magazines if ammo is both greater and it's sufficient for a shot
    // To prevent them from constantly swapping magazines while entering range.
    // If not in danger, swap magazines if ammo is greater than current.
    bool want_swap =
        ammo.ammo_remaining() > weap.ammo_remaining()
        && (!danger || ammo.ammo_remaining() > weap.ammo_required());
    return combat_reload || want_swap;
}

void npc::check_or_reload_cbm() {
    if (get_npc_ai_info_cache(npc_ai_info::reloadable_cbms) >= 0.0) {
        add_msg(m_debug, "Cancelling cbm reload check as cache is not negative.");
        return;
    }

    std::vector<std::pair<bionic_id, item*>> checklist = find_reloadable_cbms(*this);

    if (!checklist.empty()) {
        for (auto& [bid, itm] : checklist) {
            bionic& bio = get_bionic_state(bid);
            const item* it_loc = reload_selection::prepare(*this, *itm).selected.ammo;
            if (it_loc && wants_to_reload_with(*itm, *it_loc, ai_cache.danger > 0)) {
                do_reload(*itm);
                bio.ammo_loaded =
                    itm->ammo_data() != nullptr ? itm->ammo_data()->get_id() : itype_id::NULL_ID();
                bio.ammo_count = static_cast<unsigned int>(itm->ammo_remaining());
                return;
            }
        }
    }

    set_npc_ai_info_cache(npc_ai_info::reloadable_cbms, 5.0);
    return;
}

item& npc::find_reloadable() {
    if (get_npc_ai_info_cache(npc_ai_info::reloadables) >= 0.0) {
        add_msg(m_debug, "Cancelling reload check as cache is not negative.");
        return null_item_reference();
    }
    // Check wielded gun, non-wielded guns, mags and tools
    // TODO: Build a proper gun->mag->ammo DAG (Directed Acyclic Graph)
    // to avoid checking same properties over and over
    // TODO: Make this understand bandoliers, pouches etc.
    // TODO: Cache items checked for reloading to avoid re-checking same items every turn
    // TODO: Make it understand smaller and bigger magazines
    item* reloadable = nullptr;
    visit_items([this, &reloadable](item* node) {
        if (!wants_to_reload(*this, *node)) { return VisitResponse::NEXT; }
        const auto it_loc = reload_selection::prepare(*this, *node).selected.ammo;
        if (it_loc && wants_to_reload_with(*node, *it_loc, ai_cache.danger > 0)) {
            reloadable = node;
            return VisitResponse::ABORT;
        }

        return VisitResponse::NEXT;
    });

    if (reloadable != nullptr) { return *reloadable; }

    set_npc_ai_info_cache(npc_ai_info::reloadables, 5.0);
    return null_item_reference();
}

const item& npc::find_reloadable() const {
    return const_cast<const item&>(const_cast<npc*>(this)->find_reloadable());
}

bool npc::can_reload_current() {
    if (!primary_weapon().is_gun() || !wants_to_reload(*this, primary_weapon())) { return false; }

    return static_cast<bool>(find_usable_ammo(primary_weapon()));
}

item* npc::find_usable_ammo(item& weap) {
    if (!can_reload(weap)) { return nullptr; }

    auto loc = reload_selection::prepare(*this, weap).selected.ammo;
    if (!loc || !wants_to_reload_with(weap, *loc, ai_cache.danger > 0)) { return nullptr; }

    return loc;
}

item* npc::find_usable_ammo(item& weap) const {
    return const_cast<npc*>(this)->find_usable_ammo(weap);
}

void npc::adjust_power_cbms() {
    if (!is_player_ally() || wants_to_recharge_cbm()) { return; }
    for (const bionic_id& cbm_id : power_cbms) { deactivate_bionic_by_id(cbm_id); }
}

void npc::activate_combat_cbms() {
    for (bionic& bio : get_bionic_collection()) {
        if (bio.info().has_flag(flag_COMBAT_NPC_USE)) { activate_bionic(bio); }
    }
    if (can_use_offensive_cbm()) { check_or_use_weapon_cbm(); }
}

void npc::deactivate_combat_cbms() {
    for (bionic& bio : get_bionic_collection()) {
        if (bio.info().has_flag(flag_COMBAT_NPC_USE)) { deactivate_bionic(bio); }
    }
    deactivate_bionic_by_id(bio_hydraulics);
    deactivate_weapon_cbm(*this);
    cbm_active = bionic_id::NULL_ID();
    cbm_fake_active.release();
    cbm_toggled = bionic_id::NULL_ID();
    cbm_fake_toggled.release();
}

bool npc::activate_bionic_by_id(const bionic_id& cbm_id, bool eff_only) {
    if (has_bionic(cbm_id)) {
        bionic& bio = get_bionic_state(cbm_id);
        if (!bio.powered) { return activate_bionic(bio, eff_only); }
    }
    return false;
}

bool npc::use_bionic_by_id(const bionic_id& cbm_id, bool eff_only) {
    if (has_bionic(cbm_id)) {
        bionic& bio = get_bionic_state(cbm_id);
        if (bio.powered) {
            return true;
        } else {
            return activate_bionic(bio, eff_only);
        }
    }
    return false;
}

bool npc::deactivate_bionic_by_id(const bionic_id& cbm_id, bool eff_only) {
    if (has_bionic(cbm_id)) {
        bionic& bio = get_bionic_state(cbm_id);
        if (bio.powered) { return deactivate_bionic(bio, eff_only); }
    }
    return false;
}

bool npc::wants_to_recharge_cbm() {
    const units::energy curr_power = get_power_level();
    const float allowed_ratio = static_cast<int>(rules.cbm_recharge) / 100.0f;
    const units::energy max_pow_allowed = get_max_power_level() * allowed_ratio;

    if (curr_power < max_pow_allowed) {
        for (const bionic_id& bid : get_fueled_bionics()) {
            if (!has_active_bionic(bid)) { return true; }
        }
        return get_fueled_bionics().empty(); // NPC might have power CBM that doesn't use the json
                                             // fuel_opts entry
    }
    return false;
}

bool npc::can_use_offensive_cbm() const {
    const float allowed_ratio = static_cast<int>(rules.cbm_reserve) / 100.0f;
    return get_power_level() > get_max_power_level() * allowed_ratio;
}

bool npc::consume_cbm_items(const std::function<bool(const item&)>& filter) {
    const_invslice slice = inv.const_slice();
    int index = -1;
    for (size_t i = 0; i < slice.size(); i++) {
        item* const& it = slice[i]->front();
        const item& real_item = it->is_container() ? it->contents.front() : *it;
        if (filter(real_item)) {
            index = i;
            break;
        }
    }
    if (index < 0) { return false; }
    int old_moves = moves;
    consume(i_at(index));
    // TODO: a more reliable check for whether item has been consumed
    return old_moves != moves;
}

bool npc::recharge_cbm() {
    // non-allied NPCs don't consume resources to recharge
    if (!is_player_ally()) {
        mod_power_level(get_max_power_level());
        return true;
    }

    for (bionic_id& bid : get_fueled_bionics()) {
        if (has_active_bionic(bid)) { continue; }

        if (!get_fuel_available(bid).empty()) {
            use_bionic_by_id(bid);
            return true;
        } else {
            const std::function<bool(const item&)> fuel_filter = [bid](const item& it) {
                for (const itype_id& fid : bid->fuel_opts) {
                    return it.typeId() == fid
                        || (!it.is_container_empty() && it.contents.front().typeId() == fid);
                }
                return false;
            };

            if (consume_cbm_items(fuel_filter)) {
                use_bionic_by_id(bid);
                return true;
            } else {
                const std::vector<itype_id> fuel_op = bid->fuel_opts;
                const bool need_alcohol =
                    std::find(fuel_op.begin(), fuel_op.end(), itype_chem_ethanol) != fuel_op.end()
                    || std::find(fuel_op.begin(), fuel_op.end(), itype_chem_methanol)
                           != fuel_op.end()
                    || std::find(fuel_op.begin(), fuel_op.end(), itype_denat_alcohol)
                           != fuel_op.end();
                const bool need_environment =
                    std::find(fuel_op.begin(), fuel_op.end(), fuel_sunlight) != fuel_op.end()
                    || std::find(fuel_op.begin(), fuel_op.end(), fuel_wind) != fuel_op.end();

                if (std::find(fuel_op.begin(), fuel_op.end(), itype_battery) != fuel_op.end()) {
                    complain_about("need_batteries", 3_hours, "<need_batteries>", false);
                } else if (need_alcohol) {
                    complain_about("need_booze", 3_hours, "<need_booze>", false);
                } else if (need_environment) {
                    // No Need for NPCs to complain about the weather and time of day...
                    continue;
                } else {
                    complain_about("need_fuel", 3_hours, "<need_fuel>", false);
                }
            }
        }
    }

    if (use_bionic_by_id(bio_furnace)) {
        const std::function<bool(const item&)> furnace_filter = [](const item& it) {
            return it.typeId() == itype_id("withered") || it.typeId() == itype_id("file")
                || it.has_flag(flag_FIREWOOD);
        };
        if (consume_cbm_items(furnace_filter)) {
            return true;
        } else {
            complain_about("need_junk", 3_hours, "<need_junk>", false);
        }
    }

    return false;
}

healing_options npc::patient_assessment(const Character& c) {
    healing_options try_to_fix;
    try_to_fix.clear_all();

    for (const auto& part : c.get_all_body_parts(true)) {
        const auto& bp = c.get_part(part);
        if (c.has_effect(effect_bleed, part.id())) { try_to_fix.bleed = true; }

        if (c.has_effect(effect_bite, part.id())) { try_to_fix.bite = true; }

        if (c.has_effect(effect_infected, part.id())) { try_to_fix.infect = true; }
        int part_threshold = 75;
        if (part == bodypart_str_id("head")) {
            part_threshold += 20;
        } else if (part == bodypart_str_id("torso")) {
            part_threshold += 10;
        }
        part_threshold = std::min(80, part_threshold);
        part_threshold = part_threshold * bp.get_hp_max() / 100;

        if (bp.get_hp_cur() <= part_threshold) {
            if (!c.has_effect(effect_bandaged, part.id())) { try_to_fix.bandage = true; }
            if (!c.has_effect(effect_disinfected, part.id())) { try_to_fix.disinfect = true; }
        }
    }
    return try_to_fix;
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

bool npc::update_path(const tripoint_bub_ms& p, const bool no_bashing, bool force) {
    ZoneScopedN("npc_update_path");
    ZoneValue(rl_dist(bub_pos(), p));

    if (p == bub_pos()) {
        path.clear();
        return true;
    }

    while (!path.empty() && path[0] == bub_pos()) { path.erase(path.begin()); }

    if (!path.empty()) {
        const auto& last = path[path.size() - 1];
        if (last == p && (path[0].z() != bub_pos().z() || rl_dist(path[0], bub_pos()) <= 1)) {
            // Our path already leads to that point, no need to recalculate
            return true;
        }
    }

    auto new_path = [&]() {
        ZoneScopedN("npc_update_path_route");
        return get_map().route(
            bub_pos(), p, get_legacy_pathfinding_settings(no_bashing), get_legacy_path_avoid());
    }();
    if (new_path.empty()) {
        if (!ai_cache.sound_alerts.empty()) {
            ai_cache.sound_alerts.erase(ai_cache.sound_alerts.begin());
            add_msg(m_debug, "failed to path to sound alert %d,%d,%d->%d,%d,%d", bub_pos().x(),
                    bub_pos().y(), bub_pos().z(), p.x(), p.y(), p.z());
        }
        add_msg(m_debug, "Failed to path %d,%d,%d->%d,%d,%d", bub_pos().x(), bub_pos().y(),
                bub_pos().z(), p.x(), p.y(), p.z());
    }

    while (!new_path.empty() && new_path[0] == bub_pos()) { new_path.erase(new_path.begin()); }

    if (!new_path.empty() || force) {
        path = std::move(new_path);
        return true;
    }

    return false;
}

bool npc::can_open_door(const tripoint_bub_ms& p, const bool inside) const {
    return !rules.has_flag(ally_rule::avoid_doors) && get_map().can_open_door(this, p, inside);
}

bool npc::can_move_to(const tripoint_bub_ms& p, bool no_bashing) const {
    map& here = get_map();
    // Allow moving into any bashable spots, but penalize them during pathing
    // Doors are not passable for hallucinations
    return (
        rl_dist(bub_pos(), p) <= 1 && here.has_floor(p) && !g->is_dangerous_tile(p)
        && (here.passable(p)
            || (can_open_door(p, !here.is_outside(bub_pos())) && !is_hallucination())
            || (!no_bashing && here.bash_rating(smash_ability(), p) > 0)));
}

void npc::move_to(const tripoint_bub_ms& pt, bool no_bashing, std::set<tripoint_bub_ms>* nomove) {
    auto p = pt;

    const auto hook_results = cata::run_hooks("on_npc_try_move", [&, this](sol::table& params) {
        params["npc"] = this;
        params["from"] = cata::detail::lua_coords::to_lua(bub_pos());
        params["to"] = cata::detail::lua_coords::to_lua(p);
        params["movement_mode"] = get_movement_mode();
        params["via_ramp"] = false;
        if (is_mounted()) {
            params["mounted"] = true;
            params["mount"] = mounted_creature.get();
        } else {
            params["mounted"] = false;
        }
    });

    const auto char_hook_results =
        cata::run_hooks("on_character_try_move", [&, this](sol::table& params) {
            params["char"] = static_cast<Character*>(this);
            params["from"] = cata::detail::lua_coords::to_lua(bub_pos());
            params["to"] = cata::detail::lua_coords::to_lua(p);
            params["movement_mode"] = get_movement_mode();
            params["via_ramp"] = false;
            if (is_mounted()) {
                params["mounted"] = true;
                params["mount"] = mounted_creature.get();
            } else {
                params["mounted"] = false;
            }
        });

    if (!hook_results.get_or("allowed", true) || !char_hook_results.get_or("allowed", true)) {
        return;
    }

    map& here = get_map();
    bool ceiling_blocking_climb =
        !here.has_floor_or_support(bub_pos()) || here.has_floor_or_support(p + tripoint_above);
    if (sees_dangerous_field(p) || (nomove != nullptr && nomove->contains(p))) {
        // Move to a neighbor field instead, if possible.
        // Maybe this code already exists somewhere?
        auto other_points = here.get_dir_circle(bub_pos(), p);
        for (const tripoint_bub_ms& ot : other_points) {
            if (could_move_onto(ot) && (nomove == nullptr || !nomove->contains(ot))) {

                p = ot;
                break;
            }
        }
    }

    recoil = MAX_RECOIL;

    if (has_effect(effect_stunned)) {
        p.x() = rng(bub_pos().x() - 1, bub_pos().x() + 1);
        p.y() = rng(bub_pos().y() - 1, bub_pos().y() + 1);
        p.z() = bub_pos().z();
    }

    // nomove is used to resolve recursive invocation, so reset destination no
    // matter it was changed by stunned effect or not.
    if (nomove != nullptr && nomove->contains(p)) { p = bub_pos(); }

    // "Long steps" are allowed when crossing z-levels
    // Stairs teleport the player too
    if (rl_dist(bub_pos(), p) > 1 && p.z() == bub_pos().z()) {
        // On the same level? Not so much. Something weird happened
        path.clear();
        move_pause();
    }

    if (here.obstructed_by_vehicle_rotation(bub_pos(), p)) {
        move_pause();
        return;
    }

    bool attacking = false;
    if (g->critter_at<monster>(p)) { attacking = true; }
    if (!move_effects(attacking)) {
        mod_moves(-100);
        return;
    }

    Creature* critter = g->critter_at(p);
    if (critter != nullptr) {
        if (critter == this) { // We're just pausing!
            move_pause();
            return;
        }
        const auto att = attitude_to(*critter);
        if (att == Attitude::A_HOSTILE) {
            if (!no_bashing) {
                warn_about("cant_flee", 5_turns + rng(0, 5) * 1_turns);
                melee_attack(*critter, true);
            } else {
                move_pause();
            }

            return;
        }

        if (critter->is_avatar()) { say("<let_me_pass>"); }

        // Let NPCs push each other when non-hostile
        // TODO: Have them attack each other when hostile
        npc* np = dynamic_cast<npc*>(critter);
        if (!is_hallucination() && np != nullptr && !np->in_sleep_state()) {
            std::unique_ptr<std::set<tripoint_bub_ms>> newnomove;
            std::set<tripoint_bub_ms>* realnomove;
            if (nomove != nullptr) {
                realnomove = nomove;
            } else {
                // create the no-move list
                newnomove = std::make_unique<std::set<tripoint_bub_ms>>();
                realnomove = newnomove.get();
            }
            // other npcs should not try to move into this npc anymore,
            // so infinite loop can be avoided.
            realnomove->insert(bub_pos());
            // Don't spam player with messages over followers blunder into each other.
            if (!np->is_following()) { say("<let_me_pass>"); }
            np->move_away_from(bub_pos(), true, realnomove);
            // if we moved NPC, readjust their path, so NPCs don't jostle each other out of their
            // activity paths.
            if (np->attitude == NPCATT_ACTIVITY) {
                std::vector<tripoint_bub_ms> activity_route = np->get_auto_move_route();
                if (!activity_route.empty() && !np->has_destination_activity()) {
                    tripoint_bub_ms final_destination;
                    if (destination_point) {
                        final_destination = abs_to_bub(*destination_point);
                    } else {
                        final_destination = activity_route.back();
                    }
                    np->update_path(final_destination);
                }
            }
        }

        if (critter->bub_pos() == p) {
            move_pause();
            return;
        }
    }

    // Boarding moving vehicles is fine, unboarding isn't
    bool moved = false;
    if (const optional_vpart_position vp = here.veh_at(bub_pos())) {
        const optional_vpart_position ovp = here.veh_at(p);
        if (vp->vehicle().is_moving()
            && (veh_pointer_or_null(ovp) != veh_pointer_or_null(vp)
                || !ovp.part_with_feature(VPFLAG_BOARDABLE, true))) {
            move_pause();
            return;
        }
    }

    if (p.z() != bub_pos().z()) {
        // Z-level move
        // For now just teleport to the destination
        // TODO: Make it properly find the tile to move to
        if (is_mounted()) {
            move_pause();
            return;
        }
        moves -= 100;
        moved = true;
    } else if (here.passable(p) && !here.has_flag("DOOR", p)) {
        bool diag = trigdist && bub_pos().x() != p.x() && bub_pos().y() != p.y();
        if (is_mounted()) {
            const double base_moves =
                run_cost(here.combined_movecost(bub_pos(), p), diag) * 100.0
                / mounted_creature->get_speed();
            const double encumb_moves = get_weight() / 4800.0_gram;
            moves -= static_cast<int>(std::ceil(base_moves + encumb_moves));
            if (mounted_creature->has_flag(MF_RIDEABLE_MECH)) {
                mounted_creature->use_mech_power(-1);
            }
        } else {
            moves -= run_cost(here.combined_movecost(bub_pos(), p), diag);
        }
        moved = true;
    } else if (here.can_open_door(this, p, !here.is_outside(bub_pos()))) {
        if (!is_hallucination()) { // hallucinations don't open doors
            here.open_door(this, p, !here.is_outside(bub_pos()));
            moves -= 100;
        } else { // hallucinations teleport through doors
            moves -= 100;
            moved = true;
        }
    } else if (get_dex() > 1 && here.has_flag_ter_or_furn("CLIMBABLE", p)
               && !ceiling_blocking_climb) {
        ///\EFFECT_DEX_NPC increases chance to climb CLIMBABLE furniture or terrain
        int climb = get_dex();
        if (one_in(climb)) {
            add_msg_if_npc(
                m_neutral, _("%1$s tries to climb the %2$s but slips."), name, here.tername(p));
            moves -= 400;
        } else {
            add_msg_if_npc(m_neutral, _("%1$s climbs over the %2$s."), name, here.tername(p));
            moves -= (500 - (rng(0, climb) * 20));
            moved = true;
        }
    } else if (!no_bashing && smash_ability() > 0 && here.is_bashable(p)
               && here.bash_rating(smash_ability(), p) > 0) {
        moves -= !is_armed() ? 80 : attack_cost(primary_weapon()) * 0.8;
        here.bash(p, smash_ability());
    } else {
        if (attitude == NPCATT_MUG || attitude == NPCATT_KILL
            || attitude == NPCATT_WAIT_FOR_LEAVE) {
            set_attitude(NPCATT_FLEE_TEMP);
        }

        moves = 0;
    }

    if (moved) {
        const auto old_pos = bub_pos();
        setpos_preserving_movement_state(p);
        set_underwater(g->m.is_divable(p));
        if (old_pos.x() - p.x() < 0) {
            facing = FD_RIGHT;
        } else {
            facing = FD_LEFT;
        }
        if (is_hallucination()) { return; }
        if (is_mounted()) {
            if (mounted_creature->bub_pos() != bub_pos()) {
                mounted_creature->setpos(bub_pos());
                mounted_creature->facing = facing;
                mounted_creature->process_triggers();
                here.creature_in_field(*mounted_creature);
                here.creature_on_trap(*mounted_creature);
            }
        }
        if (here.has_flag("UNSTABLE", bub_pos())) {
            add_effect(effect_bouldering, 1_turns, bodypart_str_id::NULL_ID());
        } else if (has_effect(effect_bouldering)) {
            remove_effect(effect_bouldering);
        }

        if (here.has_flag_ter_or_furn(TFLAG_NO_SIGHT, bub_pos())) {
            add_effect(effect_no_sight, 1_turns, bodypart_str_id::NULL_ID());
        } else if (has_effect(effect_no_sight)) {
            remove_effect(effect_no_sight);
        }

        if (in_vehicle) { here.unboard_vehicle(old_pos); }

        // Close doors behind self (if you can)
        if ((rules.has_flag(ally_rule::close_doors) && is_player_ally()) && !is_hallucination()) {
            doors::close_door(here, *this, old_pos);
        }

        if (here.veh_at(p).part_with_feature(VPFLAG_BOARDABLE, true)) {
            here.board_vehicle(p, this);
        }

        here.creature_on_trap(*this);
        here.creature_in_field(*this, /*movement_only=*/true);
        here.creature_in_field(*this);
    }
}

void npc::move_to_next() {
    while (!path.empty() && bub_pos() == path[0]) { path.erase(path.begin()); }

    if (path.empty()) {
        add_msg(m_debug,
                "npc::move_to_next() called with an empty path or path "
                "containing only current position");
        move_pause();
        return;
    }

    move_to(path[0]);
    if (!path.empty() && bub_pos() == path[0]) { // Move was successful
        path.erase(path.begin());
    }
}

void npc::avoid_friendly_fire() {
    // TODO: To parameter
    const tripoint_bub_ms& tar = current_target()->bub_pos();
    // Calculate center of weight of friends and move away from that
    tripoint_bub_ms center;
    for (const auto& fr : ai_cache.friends) {
        if (shared_ptr_fast<Creature> fr_p = fr.lock()) { center += fr_p->bub_pos().raw(); }
    }

    float friend_count = ai_cache.friends.size();
    center.x() = std::round(center.x() / friend_count);
    center.y() = std::round(center.y() / friend_count);
    center.z() = std::round(center.z() / friend_count);

    std::vector<tripoint_bub_ms> candidates = closest_points_first(bub_pos(), 1);
    candidates.erase(candidates.begin());
    std::sort(
        candidates.begin(), candidates.end(),
        [&tar, &center](const tripoint_bub_ms& l, const tripoint_bub_ms& r) {
            return (rl_dist(l, tar) - rl_dist(l, center)) < (rl_dist(r, tar) - rl_dist(r, center));
        });

    for (const auto& pt : candidates) {
        if (can_move_to(pt)) {
            move_to(pt);
            return;
        }
    }

    /* If we're still in the function at this point, maneuvering can't help us. So,
     * might as well address some needs.
     * We pass a <danger> value of NPC_DANGER_VERY_LOW + 1 so that we won't start
     * eating food (or, god help us, sleeping).
     */
    npc_action action = address_needs(NPC_DANGER_VERY_LOW + 1);
    if (action == npc_undecided) { move_pause(); }
    execute_action(action);
}

void npc::escape_explosion() {
    if (ai_cache.dangerous_explosives.empty()) { return; }

    warn_about("explosion", 1_minutes);

    move_away_from(ai_cache.dangerous_explosives, true);
}

void npc::move_away_from(
    const tripoint_bub_ms& pt, bool no_bash_atk, std::set<tripoint_bub_ms>* nomove) {
    auto best_pos = bub_pos();
    int best = -1;
    int chance = 2;
    map& here = get_map();
    for (const tripoint_bub_ms& p : here.points_in_radius(bub_pos(), 1)) {
        if (nomove != nullptr && nomove->contains(p)) { continue; }

        if (p == bub_pos()) { continue; }

        if (p == get_player_character().bub_pos()) { continue; }

        const int cost = here.combined_movecost(bub_pos(), p);
        if (cost <= 0) { continue; }

        const int dst =
            std::abs(p.x() - pt.x()) + std::abs(p.y() - pt.y()) + std::abs(p.z() - pt.z());
        const int val = dst * 1000 / cost;
        if (val > best && can_move_to(p, no_bash_atk)) {
            best_pos = p;
            best = val;
            chance = 2;
        } else if ((val == best && one_in(chance)) && can_move_to(p, no_bash_atk)) {
            best_pos = p;
            best = val;
            chance++;
        }
    }

    move_to(best_pos, no_bash_atk, nomove);
}

void npc::move_pause()

{
    // make sure we're using the best weapon
    if (has_new_items) { scan_new_items(); }
    if (action_time_scale::once_every_this_tick(1_hours)) {
        deactivate_bionic_by_id(bio_soporific);
        for (const bionic_id& bio_id : health_cbms) { activate_bionic_by_id(bio_id); }
    }
    // NPCs currently always aim when using a gun, even with no target
    // This simulates them aiming at stuff just at the edge of their range
    if (!primary_weapon().is_gun() && cbm_active.is_null()) {
        character_funcs::do_pause(*this);
        return;
    }

    // Stop, drop, and roll
    if (has_effect(effect_onfire)) {
        character_funcs::do_pause(*this);
    } else {
        aim();
        moves = std::min(moves, 0);
    }
}

static std::optional<tripoint_bub_ms> nearest_passable(
    const tripoint_bub_ms& p, const tripoint_bub_ms& closest_to) {
    map& here = get_map();
    if (here.passable(p)) { return p; }

    // We need to path to adjacent tile, not the exact one
    // Let's pick the closest one to us that is passable
    std::vector<tripoint_bub_ms> candidates = closest_points_first(p, 1);
    std::sort(candidates.begin(), candidates.end(),
              [closest_to](const tripoint_bub_ms& l, const tripoint_bub_ms& r) {
                  return rl_dist(closest_to, l) < rl_dist(closest_to, r);
              });
    auto iter =
        std::find_if(candidates.begin(), candidates.end(), [&here, &p](const tripoint_bub_ms& pt) {
            return here.passable(pt) && !here.obstructed_by_vehicle_rotation(p, pt);
        });
    if (iter != candidates.end()) { return *iter; }

    return std::nullopt;
}

void npc::move_away_from(const std::vector<sphere>& spheres, bool no_bashing) {
    if (spheres.empty()) { return; }

    auto minp = bub_pos();
    auto maxp = bub_pos();

    for (const auto& elem : spheres) {
        minp.x() = std::min(minp.x(), elem.center.x - elem.radius);
        minp.y() = std::min(minp.y(), elem.center.y - elem.radius);
        maxp.x() = std::max(maxp.x(), elem.center.x + elem.radius);
        maxp.y() = std::max(maxp.y(), elem.center.y + elem.radius);
    }

    const tripoint_range<tripoint_bub_ms> range(minp, maxp);

    std::vector<tripoint_bub_ms> escape_points;

    map& here = get_map();
    std::copy_if(range.begin(), range.end(), std::back_inserter(escape_points),
                 [&here](const tripoint_bub_ms& elem) { return here.passable(elem); });

    cata::sort_by_rating(
        escape_points.begin(), escape_points.end(), [&](const tripoint_bub_ms& elem) {
            const int danger = std::
                accumulate(spheres.begin(), spheres.end(), 0, [&](const int sum, const sphere& s) {
                    return sum + std::max(s.radius - rl_dist(elem.raw(), s.center), 0);
                });

            const int distance = rl_dist(bub_pos(), elem);
            const int move_cost = here.move_cost(elem);

            return std::make_tuple(danger, distance, move_cost);
        });

    for (const auto& elem : escape_points) {
        update_path(elem, no_bashing);

        if (elem == bub_pos() || !path.empty()) { break; }
    }

    if (!path.empty()) {
        move_to_next();
    } else {
        move_pause();
    }
}

void npc::see_item_say_smth(const itype_id& object, const std::string& smth) {
    map& here = get_map();
    for (const tripoint_bub_ms& p : closest_points_first(bub_pos(), 6)) {
        if (here.sees_some_items(p, *this) && sees(p)) {
            for (const item* const& it : here.i_at(p)) {
                if (one_in(100) && (it->typeId() == object)) { say(smth); }
            }
        }
    }
}

void npc::find_item() {
    if (is_hallucination()) {
        see_item_say_smth(itype_thorazine, "<no_to_thorazine>");
        see_item_say_smth(itype_lsd, "<yes_to_lsd>");
        return;
    }

    if (is_player_ally() && !rules.has_flag(ally_rule::allow_pick_up)) {
        // Grabbing stuff not allowed by our "owner"
        return;
    }

    fetching_item = false;
    int best_value = minimum_item_value();
    // Not perfect, but has to mirror pickup code
    units::volume volume_allowed = volume_capacity() - volume_carried();
    units::mass weight_allowed = weight_capacity() - weight_carried();
    // For some reason range limiting by vision doesn't work properly
    const int range = 6;
    // int range = sight_range( g->light_level( bub_pos().z() ) );
    // range = std::max( 1, std::min( 12, range ) );

    static const zone_type_id zone_type_no_npc_pickup("NO_NPC_PICKUP");

    const item* wanted = nullptr;

    const bool whitelisting = has_item_whitelist();

    if (volume_allowed <= 0_ml || weight_allowed <= 0_gram) { return; }

    const auto consider_item =
        [&wanted, &best_value, whitelisting, volume_allowed, weight_allowed,
         this](const item& it, const tripoint_bub_ms& p) {
            if (it.made_of(LIQUID)) {
                // Don't even consider liquids.
                return;
            }
            std::vector<npc*> followers;
            for (auto& elem : g->get_follower_list()) {
                shared_ptr_fast<npc> npc_to_get = get_overmapbuffer(get_dimension()).find_npc(elem);
                if (!npc_to_get) { continue; }
                npc* npc_to_add = npc_to_get.get();
                followers.push_back(npc_to_add);
            }
            Character& player_character = get_player_character();
            for (auto& elem : followers) {
                if (!it.is_owned_by(*this, true)
                    && (player_character.sees(bub_pos()) || player_character.sees(wanted_item_pos)
                        || elem->sees(bub_pos()) || elem->sees(wanted_item_pos))) {
                    return;
                }
            }
            if (whitelisting && !item_whitelisted(it)) { return; }

            // When using a whitelist, skip the value check
            // TODO: Whitelist hierarchy?
            int itval = whitelisting ? 1000 : value(it);

            if (itval > best_value
                && (it.volume() <= volume_allowed && it.weight() <= weight_allowed)) {
                wanted_item_pos = p;
                wanted = &(it);
                best_value = itval;
            }
        };

    map& here = get_map();
    // Harvest item doesn't exist, so we'll be checking by its name
    std::string wanted_name;
    const auto consider_terrain =
        [this, whitelisting, volume_allowed, &wanted, &wanted_name,
         &here](const tripoint_bub_ms& p) {
            // We only want to pick plants when there are no items to pick
            if (!whitelisting || wanted != nullptr || !wanted_name.empty()
                || volume_allowed < 250_ml) {
                return;
            }

            const auto& harvest = here.get_harvest_names(p);
            for (const auto& entry : harvest) {
                if (item_name_whitelisted(entry)) {
                    wanted_name = entry;
                    wanted_item_pos = p;
                    break;
                }
            }
        };

    for (const tripoint_abs_ms& p : closest_points_first(abs_pos(), range)) {
        // TODO: Make this sight check not overdraw nearby tiles
        // TODO: Optimize that zone check
        const auto eq_bub_pos = p - abs_pos() + bub_pos();
        if (is_player_ally() && g->check_zone(zone_type_no_npc_pickup, eq_bub_pos)) { continue; }
        const int prev_num_items = ai_cache.searched_tiles.get(p, -1);
        // Prefetch the number of items present so we can bail out if we already checked here.
        const map_stack m_stack = here.i_at(eq_bub_pos);
        int num_items = m_stack.size();
        const optional_vpart_position vp = here.veh_at(p);
        if (vp) {
            const std::optional<vpart_reference> cargo = vp.part_with_feature(VPFLAG_CARGO, true);
            if (cargo) {
                vehicle_stack v_stack = cargo->vehicle().get_items(cargo->part_index());
                num_items += v_stack.size();
            }
        }
        if (prev_num_items == num_items) { continue; }
        auto cache_tile = [this, &p, num_items, &wanted]() {
            if (wanted == nullptr) { ai_cache.searched_tiles.insert(1000, p, num_items); }
        };
        bool can_see = false;
        if (here.sees_some_items(eq_bub_pos, *this) && sees(eq_bub_pos)) {
            can_see = true;
            for (const item* const& it : m_stack) { consider_item(*it, eq_bub_pos); }
        }

        // Not cached because it gets checked once and isn't expected to change.
        if (can_see || sees(eq_bub_pos)) {
            can_see = true;
            consider_terrain(eq_bub_pos);
        }

        if (!vp || vp->vehicle().is_moving() || !(can_see || sees(eq_bub_pos))) {
            cache_tile();
            continue;
        }
        const std::optional<vpart_reference> cargo = vp.part_with_feature(VPFLAG_CARGO, true);
        static const std::string locked_string("LOCKED");
        // TODO: Let player know what parts are safe from NPC thieves
        if (!cargo || cargo->has_feature(locked_string)) {
            cache_tile();
            continue;
        }

        static const std::string cargo_locking_string("CARGO_LOCKING");
        if (vp.part_with_feature(cargo_locking_string, true)) {
            cache_tile();
            continue;
        }

        for (const item* const& it : cargo->vehicle().get_items(cargo->part_index())) {
            consider_item(*it, eq_bub_pos);
        }
        cache_tile();
    }

    if (wanted != nullptr) { wanted_name = wanted->tname(); }

    if (wanted_name.empty()) { return; }

    fetching_item = true;

    // TODO: Move that check above, make it multi-target pathing and use it
    // to limit tiles available for choice of items
    const int dist_to_item = rl_dist(wanted_item_pos, bub_pos());
    if (const std::optional<tripoint_bub_ms> dest = nearest_passable(wanted_item_pos, bub_pos())) {
        update_path(*dest);
    }

    if (path.empty() && dist_to_item > 1) {
        // Item not reachable, let's just totally give up for now
        fetching_item = false;
    }

    if (fetching_item && rl_dist(wanted_item_pos, bub_pos()) > 1 && is_walking_with()) {
        say(_("Hold on, I want to pick up that %s."), wanted_name);
    }
}

void npc::pick_up_item() {
    if (is_hallucination()) { return; }

    if (!rules.has_flag(ally_rule::allow_pick_up) && is_player_ally()) {
        add_msg(m_debug, "%s::pick_up_item(); Canceling on player's request", name);
        fetching_item = false;
        moves -= 1;
        return;
    }

    map& here = get_map();
    const std::optional<vpart_reference> vp =
        here.veh_at(wanted_item_pos).part_with_feature(VPFLAG_CARGO, false);
    const bool has_cargo = vp && !vp->has_feature("LOCKED");

    if ((!here.has_items(wanted_item_pos) && !has_cargo && !here.is_harvestable(wanted_item_pos)
         && sees(wanted_item_pos))
        || (is_player_ally() && g->check_zone(zone_type_id("NO_NPC_PICKUP"), wanted_item_pos))) {
        // Items we wanted no longer exist and we can see it
        // Or player who is leading us doesn't want us to pick it up
        fetching_item = false;
        move_pause();
        add_msg(m_debug, "Canceling pickup - no items or new zone");
        return;
    }

    add_msg(m_debug, "%s::pick_up_item(); [%d, %d, %d] => [%d, %d, %d]", name, bub_pos().x(),
            bub_pos().y(), bub_pos().z(), wanted_item_pos.x(), wanted_item_pos.y(),
            wanted_item_pos.z());
    if (const std::optional<tripoint_bub_ms> dest = nearest_passable(wanted_item_pos, bub_pos())) {
        update_path(*dest);
    }

    const int dist_to_pickup = rl_dist(bub_pos(), wanted_item_pos);

    bool cant_reach =
        dist_to_pickup > 1 || get_map().obstructed_by_vehicle_rotation(bub_pos(), wanted_item_pos);
    if (cant_reach && !path.empty()) {
        add_msg(m_debug, "Moving; [%d, %d, %d] => [%d, %d, %d]", bub_pos().x(), bub_pos().y(),
                bub_pos().z(), path[0].x(), path[0].y(), path[0].z());

        move_to_next();
        return;
    } else if (cant_reach && path.empty()) {
        add_msg(m_debug, "Can't find path");
        // This can happen, always do something
        fetching_item = false;
        move_pause();
        return;
    }

    // We're adjacent to the item; grab it!

    auto picked_up = pick_up_item_map(wanted_item_pos);
    if (picked_up.empty() && has_cargo) {
        picked_up = pick_up_item_vehicle(vp->vehicle(), vp->part_index());
    }

    if (picked_up.empty()) {
        // Last chance: plant harvest
        if (here.is_harvestable(wanted_item_pos)) {
            here.examine(*this, wanted_item_pos);
            // Note: we didn't actually pick up anything, just spawned items
            // but we want the item picker to find new items
            fetching_item = false;
            return;
        }
    }
    Character& player_character = get_player_character();
    // Describe the pickup to the player
    bool u_see = player_character.sees(*this) || player_character.sees(wanted_item_pos);
    if (u_see) {
        if (picked_up.size() == 1) {
            add_msg(_("%1$s picks up a %2$s."), name, picked_up.front()->tname());
        } else if (picked_up.size() == 2) {
            add_msg(_("%1$s picks up a %2$s and a %3$s."), name, picked_up.front()->tname(),
                    picked_up.back()->tname());
        } else if (picked_up.size() > 2) {
            add_msg(_("%s picks up several items."), name);
        } else {
            add_msg(_("%s looks around nervously, as if searching for something."), name);
        }
    }

    for (auto& it : picked_up) {
        int itval = value(*it);
        if (itval < worst_item_value) { worst_item_value = itval; }
        i_add(it->detach());
    }

    moves -= 100;
    fetching_item = false;
    has_new_items = true;
}

template <typename T> std::vector<item*> npc_pickup_from_stack(npc& who, T& items) {
    const bool whitelisting = who.has_item_whitelist();
    auto volume_allowed = who.volume_capacity() - who.volume_carried();
    auto weight_allowed = who.weight_capacity() - who.weight_carried();
    auto min_value = whitelisting ? 0 : who.minimum_item_value();
    std::vector<item*> picked_up;

    for (auto& iter : items) {
        item& it = *iter;
        if (it.made_of(LIQUID)) { continue; }

        if (whitelisting && !who.item_whitelisted(it)) { continue; }

        auto volume = it.volume();
        if (volume > volume_allowed) { continue; }

        auto weight = it.weight();
        if (weight > weight_allowed) { continue; }

        int itval = whitelisting ? 1000 : who.value(it);
        if (itval < min_value) { continue; }

        volume_allowed -= volume;
        weight_allowed -= weight;
        picked_up.push_back(&it);
    }

    return picked_up;
}

std::vector<item*> npc::pick_up_item_map(const tripoint_bub_ms& where) {
    map_stack stack = get_map().i_at(where);
    return npc_pickup_from_stack(*this, stack);
}

std::vector<item*> npc::pick_up_item_vehicle(vehicle& veh, int part_index) {
    auto stack = veh.get_items(part_index);
    return npc_pickup_from_stack(*this, stack);
}

// Used in npc::drop_items()
struct ratio_index {
    double ratio;
    int index;
    ratio_index(double R, int I): ratio(R), index(I) {}
};

/* As of October 2019, this is buggy, do not use!! */
void npc::drop_items(units::mass drop_weight, units::volume drop_volume, int min_val) {
    /* Remove this when someone debugs it back to functionality */
    return;

    add_msg(
        m_debug,
        "%s is dropping items-%3.2f kg, %3.2f L (%d items, wgt %3.2f/%3.2f kg, "
        "vol %3.2f/%3.2f L)",
        name, units::to_kilogram(drop_weight), units::to_liter(drop_volume), inv.size(),
        units::to_kilogram(weight_carried()), units::to_kilogram(weight_capacity()),
        units::to_liter(volume_carried()), units::to_liter(volume_capacity()));

    units::mass weight_dropped = units::from_gram(0);
    units::volume volume_dropped = units::from_liter(0);
    std::vector<ratio_index> rWgt, rVol; // Weight/Volume to value ratios

    // First fill our ratio vectors, so we know which things to drop first
    const_invslice slice = inv.const_slice();
    for (size_t i = 0; i < slice.size(); i++) {
        item& it = *slice[i]->front();
        double wgt_ratio = 0.0;
        double vol_ratio = 0.0;
        if (value(it) == 0 || value(it) <= min_val) {
            wgt_ratio = 99999;
            vol_ratio = 99999;
        } else {
            wgt_ratio = units::to_gram<double>(it.weight()) / value(it);
            vol_ratio = units::to_liter(it.volume() / value(it));
        }
        bool added_wgt = false;
        bool added_vol = false;
        for (size_t j = 0; j < rWgt.size() && !added_wgt; j++) {
            if (wgt_ratio > rWgt[j].ratio) {
                added_wgt = true;
                rWgt.insert(rWgt.begin() + j, ratio_index(wgt_ratio, i));
            }
        }
        if (!added_wgt) { rWgt.emplace_back(ratio_index(wgt_ratio, i)); }
        for (size_t j = 0; j < rVol.size() && !added_vol; j++) {
            if (vol_ratio > rVol[j].ratio) {
                added_vol = true;
                rVol.insert(rVol.begin() + j, ratio_index(vol_ratio, i));
            }
        }
        if (!added_vol) { rVol.emplace_back(ratio_index(vol_ratio, i)); }
    }

    map& here = get_map();
    std::string item_name;     // For description below
    int num_items_dropped = 0; // For description below
    // Now, drop items, starting from the top of each list
    while (weight_dropped < drop_weight || volume_dropped < drop_volume) {
        // weight and volume may be passed as 0 or a negative value, to indicate that
        // decreasing that variable is not important.
        int dWeight =
            units::to_gram<int>(drop_weight) <= 0
                ? -1
                : units::to_gram<int>(drop_weight - weight_dropped) / 250;
        const auto d_volume =
            drop_volume <= 0_ml
                ? -1
                : std::min(units::to_milliliter(drop_volume - volume_dropped) / 250,
                           static_cast<decltype(units::to_milliliter(drop_volume))>(INT_MAX));
        const auto dVolume = static_cast<int>(d_volume);
        int index;
        // Which is more important, weight or volume?
        if (dWeight > dVolume) {
            index = rWgt[0].index;
            rWgt.erase(rWgt.begin());
            // Fix the rest of those indices.
            for (auto& elem : rWgt) {
                if (elem.index > index) { elem.index--; }
            }
        } else {
            index = rVol[0].index;
            rVol.erase(rVol.begin());
            // Fix the rest of those indices.
            for (size_t i = 0; i < rVol.size(); i++) {
                if (i > rVol.size()) {
                    debugmsg("npc::drop_items() - looping through rVol - Size is %d, i is %d",
                             rVol.size(), i);
                }
                if (rVol[i].index > index) { rVol[i].index--; }
            }
        }
        weight_dropped += slice[index]->front()->weight();
        volume_dropped += slice[index]->front()->volume();
        detached_ptr<item> dropped = i_rem(index);
        num_items_dropped++;
        if (num_items_dropped == 1) {
            item_name += dropped->tname();
        } else if (num_items_dropped == 2) {
            item_name += _(" and ") + dropped->tname();
        }
        if (!is_hallucination()) { // hallucinations can't drop real items
            here.add_item_or_charges(bub_pos(), std::move(dropped));
        }
    }
    // Finally, describe the action if u can see it
    if (get_player_character().sees(*this)) {
        if (num_items_dropped >= 3) {
            add_msg(vgettext("%s drops %d item.", "%s drops %d items.", num_items_dropped), name,
                    num_items_dropped);
        } else {
            add_msg(_("%1$s drops a %2$s."), name, item_name);
        }
    }
    update_worst_item_value();
}

bool npc::find_corpse_to_pulp() {
    Character& player_character = get_player_character();
    if ((is_player_ally()
         && (!rules.has_flag(ally_rule::allow_pulp) || player_character.in_vehicle))
        || is_hallucination()) {
        return false;
    }

    map& here = get_map();
    // Pathing with overdraw can get expensive, limit it
    int path_counter = 4;
    const auto check_tile = [this, &path_counter, &here](const tripoint_bub_ms& p) -> const item* {
        if (!here.sees_some_items(p, *this) || !sees(p)) { return nullptr; }

        const map_stack items = here.i_at(p);
        const item* found = nullptr;
        for (const item* const& it : items) {
            // Pulp only stuff that revives, but don't pulp acid stuff
            // That is, if you aren't protected from this stuff!
            if (it->can_revive()) {
                // If the first encountered corpse bleeds something dangerous then
                // it is not safe to bash.
                if (is_dangerous_field(field_entry(it->get_mtype()->bloodType(), 1, 0_turns))) {
                    return nullptr;
                }

                found = it;
                break;
            }
        }

        if (found != nullptr) {
            path_counter--;
            // Only return corpses we can path to
            return update_path(p, false, false) ? found : nullptr;
        }

        return nullptr;
    };

    const int range = 6;

    const item* corpse = nullptr;
    if (pulp_location && square_dist(bub_pos(), *pulp_location) <= range) {
        corpse = check_tile(*pulp_location);
    }

    // Find the old target to avoid spamming
    const item* old_target = corpse;

    if (corpse == nullptr) {
        // If we're following the player, don't wander off to pulp corpses
        const auto& around = is_walking_with() ? player_character.bub_pos() : bub_pos();
        for (item*& location :
             here.get_active_items_in_radius(around, range, special_item_type::corpse)) {
            corpse = check_tile(location->bub_pos());

            if (corpse != nullptr) {
                pulp_location.emplace(location->bub_pos());
                break;
            }

            if (path_counter <= 0) { break; }
        }
    }

    if (corpse != nullptr && corpse != old_target && is_walking_with()) {
        say(_("Hold on, I want to pulp that %s."), corpse->tname());
    }

    return corpse != nullptr;
}

bool npc::do_pulp() {
    if (!pulp_location) { return false; }

    if (rl_dist(*pulp_location, bub_pos()) > 1 || pulp_location->z() != bub_pos().z()) {
        return false;
    }
    // TODO: Don't recreate the activity every time
    int old_moves = moves;
    assign_activity(ACT_PULP, calendar::INDEFINITELY_LONG, 0);
    activity->placement = bub_to_abs(*pulp_location);
    activity->do_turn(*this);
    return moves != old_moves;
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

bool npc::wield_better_weapon() {
    const Creature* critter = current_target();
    const int dist = critter ? rl_dist(bub_pos(), critter->bub_pos()) : -1;

    if (get_npc_ai_info_cache(npc_ai_info::range) == dist) {
        add_msg(m_debug, "Distance unchanged from last check, cancelling.");
        return false;
    }
    if (primary_weapon().has_flag(flag_NO_UNWIELD) && cbm_toggled.is_null()) {
        add_msg(m_debug, "Cannot unwield %s, not switching.",
                primary_weapon().type->get_id().str());
        return false;
    }

    // TODO: Allow wielding weaker weapons against weaker targets
    bool can_use_gun =
        ((!is_player_ally() || rules.has_flag(ally_rule::use_guns))
         && (ai_cache.danger >= 3 || emergency() || dist < 0));
    bool use_silent = (is_player_ally() && rules.has_flag(ally_rule::use_silent));

    // Check if there's something better to wield
    item* best = &primary_weapon();
    double best_dps = -1;
    std::map<itype_id, gun_mode_id> mode_pairs;

    const auto compare_weapon =
        [this, &best, &best_dps, can_use_gun, use_silent, dist, &mode_pairs](const item& it) {
            bool gun_usable = can_use_gun && dist != 0 && (!use_silent || it.is_silent());
            double dps = 0.0f;
            auto [mode_id, mode_] = npc_ai::best_mode_for_range(*this, it, dist);

            if (mode_ && gun_usable) {
                dps = it.ideal_ranged_dps(*this, mode_);
                mode_pairs[it.typeId()] = mode_id;

                if (dps > best_dps) {
                    best = const_cast<item*>(&it);
                    best_dps = dps;
                }
            } else {
                if (dist > 0 && dist > it.reach_range(*this)) { return; }
                dps = npc_ai::melee_value(*this, it);

                if (dps > best_dps) {
                    if (it.is_gun()) { mode_pairs[it.typeId()] = it.gun_get_mode_id(); }
                    best = const_cast<item*>(&it);
                    best_dps = dps;
                }
            }
            add_msg(m_debug, "Evaluated %s at %.1f for distance %d", it.tname(), dps, dist);
        };

    compare_weapon(primary_weapon());

    // Fists aren't checked below
    compare_weapon(null_item_reference());

    // TOD: Once NPCs respect wielding costs more, find an efficient way
    // to have NPCs wield weapons with shorter ranges than dist in preparation
    // if they don't have a weapon with appropriate range/ammo.
    visit_items([&compare_weapon, this](item* node) {
        // For worn items, only compare if they have a weapon category defined.
        if (is_worn(*node) && node->type->weapon_category.empty()) { return VisitResponse::SKIP; }
        // Otherwise, compare any melee usable item, guns or holstered items
        if (node->is_melee() || node->is_gun()) {
            compare_weapon(*node);
        } else if (node->get_use("holster") && !node->contents.empty()
                   && node != &primary_weapon()) {
            // TODO: special case for "wield from wielded holster"
            const item& holstered = node->get_contained();
            if (holstered.is_melee() || holstered.is_gun()) { compare_weapon(holstered); }
        }
        return VisitResponse::SKIP;
    });

    std::map<item*, bionic_id> toggled_list = check_toggle_cbm();
    for (const auto& [it, _] : toggled_list) { compare_weapon(*it); }

    set_npc_ai_info_cache(npc_ai_info::range, dist);

    // TODO: Reimplement switching to empty guns
    // Needs to check reload speed, RELOAD_ONE etc.
    // Until then, the NPCs should reload the guns as a last resort

    if (best == &primary_weapon()) {
        add_msg(m_debug, "Wielded %s is best at %.1f, not switching", best->type->get_id().str(),
                best_dps);
        if (best_dps >= 0 && primary_weapon().is_gun()
            && !primary_weapon().gun_set_mode(mode_pairs[primary_weapon().typeId()])) {
            debugmsg("Failed to set mode %s for %s", mode_pairs[primary_weapon().typeId()].c_str(),
                     primary_weapon().tname());
        }
        return false;
    }

    add_msg(m_debug, "Wielding %s at value %.1f", best->type->get_id().str(), best_dps);

    if (toggled_list[best].is_valid()) {
        cbm_toggled = toggled_list[best];
        cbm_fake_toggled = item::spawn(*best);
        if (is_armed()) { stow_weapon(); }
        activate_bionic_by_id(cbm_toggled);
        if (primary_weapon().is_gun()
            && !primary_weapon().gun_set_mode(mode_pairs[primary_weapon().typeId()])) {
            debugmsg("Failed to set mode for %s", primary_weapon().tname());
        }
        if (get_player_character().sees(bub_pos())) {
            add_msg(m_info, _("%s activates their %s."), disp_name(), cbm_toggled->name);
        }

        if (!cbm_fake_active->is_null() && best->is_gun()) {
            // They'll need time to swap weapons anyway, consolidates the comparisons into
            // check_or_use_bionics.
            cbm_fake_active.release();
            cbm_active = bionic_id::NULL_ID();
        }
        moves -= 15;
        return true;
    } else if (primary_weapon().typeId() == cbm_fake_toggled->typeId()) {
        deactivate_bionic_by_id(cbm_toggled);
        cbm_toggled = bionic_id::NULL_ID();
        cbm_fake_toggled.release();
    }

    wield(*best);
    if (primary_weapon().is_gun()
        && !primary_weapon().gun_set_mode(mode_pairs[primary_weapon().typeId()])) {
        debugmsg("Failed to set mode for %s", primary_weapon().tname());
    }
    return true;
}

void npc::scan_new_items() {
    add_msg(m_debug, "%s scanning new items", name);
    wield_better_weapon();
    has_new_items = false;
    return;
    // TODO: Armor?
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

void npc::activate_item(int item_index) {
    if (is_hallucination()) {
        move_pause();
        return;
    }
    const int oldmoves = moves;
    item& it = i_at(item_index);
    if (it.is_tool() || it.is_food()) { it.type->invoke(*this, it, bub_pos()); }

    if (moves == oldmoves) {
        // HACK: A hack to prevent debugmsgs when NPCs activate 0 move items
        // while not removing the debugmsgs for other 0 move actions
        moves--;
    }
}

void npc::heal_player(Character& patient) {
    int dist = rl_dist(bub_pos(), patient.bub_pos());

    if (dist > 1) {
        // We need to move to the player
        update_path(patient.bub_pos());
        move_to_next();
        return;
    }

    Character& player_character = get_player_character();
    // Close enough to heal!
    bool u_see = player_character.sees(*this) || player_character.sees(patient);
    if (u_see) { add_msg(_("%1$s heals %2$s."), disp_name(), patient.disp_name()); }

    item& used = get_healing_item(ai_cache.can_heal);
    if (used.is_null()) {
        debugmsg("%s tried to heal you but has no healing item", disp_name());
        return;
    }
    if (!is_hallucination()) {
        int charges_used = used.type->invoke(*this, used, patient.bub_pos(), "heal");
        consume_charges(used, charges_used);
    } else {
        pretend_heal(patient, used);
    }
}

void npc::pretend_heal(Character& patient, item& used) {
    if (get_player_character().sees(*this)) {
        add_msg(_("%1$s heals %2$s."), disp_name(),
                patient.disp_name()); // you can tell that it's not real by looking at your HP
                                      // though
    }
    consume_charges(used, 1); // empty hallucination's inventory to avoid spammming
    moves -= 100;             // consumes moves to avoid infinite loop
}

void npc::heal_self() {
    if (is_hallucination()) {
        move_pause();
        return;
    }
    if (has_effect(effect_asthma)) {
        item* treatment = &null_item_reference();
        std::string iusage = "OXYGEN_BOTTLE";
        if (has_charges(itype_inhaler, 1)) {
            treatment = &inv.find_item(inv.position_by_type(itype_inhaler));
            iusage = "INHALER";
        } else if (has_charges(itype_oxygen_tank, 1)) {
            treatment = &inv.find_item(inv.position_by_type(itype_oxygen_tank));
        } else if (has_charges(itype_smoxygen_tank, 1)) {
            treatment = &inv.find_item(inv.position_by_type(itype_smoxygen_tank));
        }
        if (!treatment->is_null()) {
            treatment->type->invoke(*this, *treatment, bub_pos(), iusage);
            consume_charges(*treatment, 1);
            return;
        }
    }

    item& used = get_healing_item(ai_cache.can_heal);
    if (used.is_null()) {
        debugmsg("%s tried to heal self but has no healing item", disp_name());
        return;
    }

    if (get_player_character().sees(*this)) {
        add_msg(_("%s applies a %s"), disp_name(), used.tname());
    }
    warn_about("heal_self", 1_turns);

    int charges_used = used.type->invoke(*this, used, bub_pos(), "heal");
    if (used.is_medication()) { consume_charges(used, charges_used); }
}

void npc::use_painkiller() {
    if (is_hallucination()) {
        move_pause();
        return;
    }
    // First, find the best painkiller for our pain level
    item* it = inv.most_appropriate_painkiller(get_pain());

    if (it->is_null()) {
        debugmsg("NPC tried to use painkillers, but has none!");
        move_pause();
    } else {
        if (get_player_character().sees(*this)) {
            add_msg(_("%1$s takes some %2$s."), disp_name(), it->tname());
        }
        consume(*it);
        moves = 0;
    }
}

// We want our food to:
// Provide enough nutrition and quench
// Not provide too much of either (don't waste food)
// Not be unhealthy
// Not have side effects
// Be eaten before it rots (favor soon-to-rot perishables)
static float rate_food(const item& it, int want_nutr, int want_quench) {
    const auto& food = it.get_comestible();
    if (!food) { return 0.0f; }

    if (food->parasites && !it.has_flag(flag_NO_PARASITES)) { return 0.0; }

    int nutr = food->get_default_nutr();
    int quench = food->quench;

    if (nutr <= 0 && quench <= 0) {
        // Not food - may be salt, drugs etc.
        return 0.0f;
    }

    if (!it.type->use_methods.empty()) {
        // TODO: Get a good method of telling apart:
        // raw meat (parasites - don't eat unless mutant)
        // zed meat (poison - don't eat unless mutant)
        // alcohol (debuffs, health drop - supplement diet but don't bulk-consume)
        // caffeine (fine to consume, but expensive and prevents sleep)
        // hallucination mushrooms (NPCs don't hallucinate, so don't eat those)
        // honeycomb (harmless iuse)
        // royal jelly (way too expensive to eat as food)
        // mutagenic crap (don't eat, we want player to micromanage muties)
        // marloss (NPCs don't turn fungal)
        // weed brownies (small debuff)
        // seeds (too expensive)

        // For now skip all of those
        return 0.0f;
    }

    double relative_rot = it.get_relative_rot();
    if (relative_rot >= 1.0f) {
        // TODO: Allow sapro mutants to eat it anyway and make them prefer it
        return 0.0f;
    }

    float weight = std::max(1.0, 10.0 * relative_rot);
    if (it.get_comestible_fun() < 0) {
        // This helps to avoid eating stuff like flour
        weight /= (-it.get_comestible_fun()) + 1;
    }

    if (food->healthy < 0) { weight /= (-food->healthy) + 1; }

    // Avoid wasting quench values unless it's about to rot away
    if (relative_rot < 0.9f && quench > want_quench) {
        weight -= (1.0f - relative_rot) * (quench - want_quench);
    }

    if (quench < 0 && want_quench > 0 && want_nutr < want_quench) {
        // Avoid stuff that makes us thirsty when we're more thirsty than hungry
        weight = weight * want_nutr / want_quench;
    }

    if (nutr > want_nutr) {
        // TODO: Allow overeating in some cases
        if (nutr >= 5) { return 0.0f; }

        if (relative_rot < 0.9f) { weight /= nutr - want_nutr; }
    }

    if (it.poison > 0) { weight -= it.poison; }

    return weight;
}

bool npc::consume_food() {
    if (is_hallucination()) {
        move_pause();
        return false;
    }
    float best_weight = 0.0f;
    int index = -1;
    int want_hunger = std::max<int>(0, (max_stored_kcal() - get_stored_kcal()) / 10);
    int want_quench = std::max(0, get_thirst());
    const_invslice slice = inv.const_slice();
    for (size_t i = 0; i < slice.size(); i++) {
        const item& it = *slice[i]->front();
        if (const item* food_item = it.get_food()) {
            float cur_weight = rate_food(*food_item, want_hunger, want_quench);
            // Note: will_eat is expensive, avoid calling it if possible
            if (cur_weight > best_weight && will_eat(*food_item).success()) {
                best_weight = cur_weight;
                index = i;
            }
        }
    }

    if (index == -1) {
        if (!is_player_ally()) {
            // TODO: Remove this and let player "exploit" hungry NPCs
            set_stored_kcal(max_stored_kcal());
            set_thirst(0);
        }
        return false;
    }

    // consume doesn't return a meaningful answer, we need to compare moves
    // TODO: Make player::consume return false if it fails to consume
    int old_moves = moves;
    consume(i_at(index));
    // TODO: a more reliable check for whether item has been consumed
    bool consumed = old_moves != moves;
    if (!consumed) { debugmsg("%s failed to consume %s", name, i_at(index).tname()); }

    return consumed;
}

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

void npc::look_for_player(const Character& sought) {
    complain_about("look_for_player", 5_minutes, "<wait>", false);
    update_path(sought.bub_pos());
    move_to_next();
    return;
    // The part below is not implemented properly
    /*
    if( sees( sought ) ) {
        move_pause();
        return;
    }

    if (!path.empty()) {
        const tripoint_bub_ms &dest = path[path.size() - 1];
        if( !sees( dest ) ) {
            move_to_next();
            return;
        }
        path.clear();
    }
    std::vector<point> possibilities;
    for (int x = 1; x < g_mapsize_x; x += 11) { // 1, 12, 23, 34
        for (int y = 1; y < g_mapsize_y; y += 11) {
            if( sees( x, y ) ) {
                possibilities.push_back(point(x, y));
            }
        }
    }
    if (possibilities.empty()) { // We see all the spots we'd like to check!
        say("<wait>");
        move_pause();
    } else {
        if (one_in(6)) {
            say("<wait>");
        }
        update_path( tripoint( random_entry( possibilities ), bub_pos().z() ) );
        move_to_next();
    }
    */
}

bool npc::saw_player_recently() const {
    return last_player_seen_pos && get_map().inbounds(*last_player_seen_pos)
        && last_seen_player_turn > 0;
}

bool npc::has_omt_destination() const { return goal != no_goal_point; }

void npc::reach_omt_destination() {
    if (!omt_path.empty()) { omt_path.clear(); }
    map& here = get_map();
    if (is_travelling()) {
        guard_pos = abs_pos();
        goal = no_goal_point;
        if (is_player_ally()) {
            Character& player_character = get_player_character();
            talk_function::assign_guard(*this);
            if (rl_dist(player_character.bub_pos(), bub_pos()) > SEEX * 2
                || !player_character.sees(bub_pos())) {
                if ((player_character.has_item_with_flag(flag_TWO_WAY_RADIO, true)
                     || player_character.has_enchantment_flag(enchantment_flag_id("RADIO")))
                    && (has_item_with_flag(flag_TWO_WAY_RADIO, true)
                        || has_enchantment_flag(enchantment_flag_id("RADIO")))) {
                    add_msg(m_info,
                            _("From your two-way radio you hear %s reporting in, "
                              "'I've arrived, boss!'"),
                            disp_name());
                }
            }
        } else {
            // for now - they just travel to a nearby place they want as a base
            // and chill there indefinitely, the plan is to add systems for them to build
            // up their base, then go out on looting missions,
            // then return to base afterwards.
            set_mission(NPC_MISSION_GUARD);
            if (!needs.empty() && needs[0] == need_safety) {
                // we found our base.
                base_location = abs_omt_pos();
            }
        }
        return;
    }
    // Guarding NPCs want a specific point, not just an overmap tile
    // Rest stops having a goal after reaching it
    if (!(is_guarding() || is_patrolling())) {
        goal = no_goal_point;
        return;
    }
    // If we are guarding, remember our position in case we get forcibly moved
    goal = abs_omt_pos();
    if (guard_pos == abs_pos()) {
        // This is the specific point
        return;
    }

    if (path.size() > 1) {
        // No point recalculating the path to get home
        move_to_next();
    } else if (guard_pos != tripoint_abs_ms::min()) {
        update_path(abs_to_bub(guard_pos));
        move_to_next();
    } else {
        guard_pos = abs_pos();
    }
}

void npc::set_omt_destination() {
    /* TODO: Make NPCs' movement more intelligent.
     * Right now this function just makes them attempt to address their needs:
     *  if we need ammo, go to a gun store, if we need food, go to a grocery store,
     *  and if we don't have any needs, pick a random spot.
     * What it SHOULD do is that, if there's time; but also look at our mission and
     *  our faction to determine more meaningful actions, such as attacking a rival
     *  faction's base, or meeting up with someone friendly.  NPCs should also
     *  attempt to reach safety before nightfall, and possibly similar goals.
     * Also, NPCs should be able to assign themselves missions like "break into that
     *  lab" or "map that river bank."
     */
    if (is_stationary(true)) {
        guard_current_pos();
        return;
    }

    tripoint_abs_omt surface_omt_loc = abs_omt_pos();
    // We need that, otherwise find_closest won't work properly
    surface_omt_loc.z() = 0;

    // also, don't bother looking if the CITY_SIZE is 0, just go somewhere at random
    const int city_size = get_option<int>("CITY_SIZE");
    if (city_size == 0) {
        goal = surface_omt_loc + point(rng(-90, 90), rng(-90, 90));
        return;
    }

    decide_needs();
    if (needs.empty()) { // We don't need anything in particular.
        needs.push_back(need_none);
    }

    std::string dest_type;
    for (const auto& fulfill : needs) {
        auto cache_iter = goal_cache.find(fulfill);
        if (cache_iter != goal_cache.end() && cache_iter->second.omt_loc == surface_omt_loc) {
            goal = cache_iter->second.goal;
        } else {
            // look for the closest occurrence of any of that locations terrain types
            omt_find_params find_params;
            for (const oter_type_id& elem : get_location_for(fulfill)->get_all_terrains()) {
                find_params.types.emplace_back(elem.id().str(), ot_match_type::type);
            }
            // note: no shuffle of `find_params.types` is needed, because `find_closest`
            // disregards `types` order anyway, and already returns random result among
            // those having equal minimal distance
            find_params.search_range = {0, 75};
            find_params.search_layers = omt_find_all_layers;

            auto& dim_ob = get_overmapbuffer(get_dimension());
            goal = dim_ob.find_closest(surface_omt_loc, find_params);
            npc_need_goal_cache& cache = goal_cache[fulfill];
            cache.goal = goal;
            cache.omt_loc = surface_omt_loc;
        }
        omt_path.clear();
        if (goal != overmap::invalid_tripoint) {
            omt_path =
                get_overmapbuffer(get_dimension())
                    .get_travel_path(surface_omt_loc, goal, overmap_path_params::for_npc());
        }
        if (!omt_path.empty()) { break; }
    }

    // couldn't find any places to go, so go somewhere.
    if (goal == overmap::invalid_tripoint || omt_path.empty()) {
        auto& dim_ob = get_overmapbuffer(get_dimension());
        goal = surface_omt_loc + point(rng(-90, 90), rng(-90, 90));
        omt_path = dim_ob.get_travel_path(surface_omt_loc, goal, overmap_path_params::for_npc());
        // try one more time
        if (omt_path.empty()) {
            goal = surface_omt_loc + point(rng(-90, 90), rng(-90, 90));
            omt_path =
                dim_ob.get_travel_path(surface_omt_loc, goal, overmap_path_params::for_npc());
        }
        if (omt_path.empty()) { goal = no_goal_point; }
        return;
    }

    DebugLog(DL::Info, DC::Main) << "npc::set_omt_destination - new goal for NPC [" << get_name()
                                 << "] with [" << get_need_str_id(needs.front()) << "] is ["
                                 << dest_type << "] in " << goal.to_string() << ".";
}

void npc::go_to_omt_destination() {
    ZoneScopedN("npc_go_to_omt_destination");

    map& here = get_map();
    if (ai_cache.guard_pos) {
        if (abs_pos() == *ai_cache.guard_pos) {
            path.clear();
            ai_cache.guard_pos = std::nullopt;
            move_pause();
            return;
        }
    }
    if (goal == no_goal_point || omt_path.empty()) {
        add_msg(m_debug, "npc::go_to_destination with no goal");
        move_pause();
        reach_omt_destination();
        return;
    }
    const tripoint_abs_omt omt_pos = abs_omt_pos();
    if (goal == omt_pos) {
        // We're at our desired map square!  Pause to keep the NPC infinite loop counter happy
        move_pause();
        reach_omt_destination();
        return;
    }
    if (!path.empty()) {
        // we already have a path, just use that until we can't.
        move_to_next();
        return;
    }
    // get the next path point
    if (omt_path.back() == omt_pos) {
        // this should be the square we are at.
        omt_path.pop_back();
    }
    if (!omt_path.empty()) {
        point_rel_omt omt_diff = omt_path.back().xy() - omt_pos.xy();
        if (omt_diff.x() > 3 || omt_diff.x() < -3 || omt_diff.y() > 3 || omt_diff.y() < -3) {
            // we've gone wandering somehow, reset destination.
            if (!is_player_ally()) {
                set_omt_destination();
            } else {
                talk_function::assign_guard(*this);
            }
            return;
        }
    }
    // TODO: fix point types
    auto sm_tri = abs_to_bub(project_to<coords::ms>(omt_path.back()));
    auto centre_sub = sm_tri + point(SEEX, SEEY);
    here.clip_to_bounds(centre_sub);
    if (!here.passable(centre_sub)) {
        auto candidates = here.points_in_radius(centre_sub, 2);
        for (const auto& elem : candidates) {
            if (here.passable(elem)) {
                centre_sub = elem;
                break;
            }
        }
    }
    {
        ZoneScopedN("npc_goto_destination_route");
        path = here.route(
            bub_pos(), centre_sub, get_legacy_pathfinding_settings(), get_legacy_path_avoid());
    }
    while (!path.empty() && path.front() == bub_pos()) { path.erase(path.begin()); }
    add_msg(m_debug, "%s going %s->%s", name, omt_pos.to_string(), goal.to_string());

    if (!path.empty()) {
        move_to_next();
        return;
    }
    move_pause();
}

void npc::guard_current_pos() {
    goal = abs_omt_pos();
    guard_pos = abs_pos();
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

void npc::do_reload(item& it) {
    if (is_hallucination()) {
        move_pause();
        return;
    }
    auto reload_opt = reload_selection::prepare(*this, it).selected;

    if (!reload_opt) {
        debugmsg("do_reload failed: no usable ammo for %s", it.tname());
        return;
    }

    // Note: we may be reloading the magazine inside, not the gun itself
    // Maybe TODO: allow reload functions to understand such reloads instead of const casts
    item& target = (*reload_opt.target);
    item* usable_ammo = reload_opt.ammo;

    // If in danger, don't spend multiple turns reloading a weapon to full one by one.
    // Get enough to shoot the enemy once, then unload it on them.
    int qty =
        ai_cache.danger > 0
            ? std::max(1, std::min(usable_ammo->charges, it.ammo_required() - it.ammo_remaining()))
            : std::max(1, std::min(usable_ammo->charges, it.ammo_capacity() - it.ammo_remaining()));
    int reload_time = item_reload_cost(it, *usable_ammo, qty);
    // TODO: Consider printing this info to player too
    const std::string ammo_name = usable_ammo->tname();
    if (!target.reload(*this, *usable_ammo, qty)) {
        debugmsg("do_reload failed: item %s could not be reloaded with %ld charge(s) of %s",
                 it.tname(), qty, ammo_name);
        return;
    }

    moves -= reload_time;
    recoil = MAX_RECOIL;

    if (get_player_character().sees(*this)) {
        add_msg(_("%1$s reloads their %2$s."), name, it.tname());
        sfx::play_variant_sound(
            "reload", it.typeId().str(), sfx::get_heard_volume(bub_pos(), 60),
            sfx::get_heard_angle(bub_pos()));
    }

    // Otherwise the NPC may not equip the weapon until they see danger
    has_new_items = true;
    // Reloading clears mode choice.
    clear_npc_ai_info_cache(npc_ai_info::range);
}

bool npc::adjust_worn() {
    bool any_broken = false;
    for (const bodypart_id& bp : get_all_body_parts()) {
        if (is_limb_broken(bp)) {
            any_broken = true;
            break;
        }
    }

    if (!any_broken) { return false; }
    const auto covers_broken = [this](const item& it, side s) {
        const body_part_set covered = it.get_covered_body_parts(s);
        for (const bodypart_str_id& bp_id : covered) {
            if (is_limb_broken(bp_id) && covered.test(bp_id)) { return true; }
        }
        return false;
    };

    item* splint = nullptr;
    for (auto& elem : worn) {
        if (!elem->has_flag(flag_SPLINT)) { continue; }

        if (!covers_broken(*elem, elem->get_side())) {
            const bool needs_change = covers_broken(*elem, opposite_side(elem->get_side()));
            // Try to change side (if it makes sense), or take off.
            if (needs_change && change_side(*elem)) { return true; }

            if (can_takeoff(*elem).success()) {
                splint = elem;
                break;
            }
        }
    }
    if (splint) {
        takeoff(*splint);
        return true;
    }

    return false;
}

void npc::set_movement_mode(character_movemode new_mode) { move_mode = new_mode; }
