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

auto npc_action_name(npc_action action) -> std::string;
void print_action(const char* prepend, npc_action action);

auto npc::update_path(const tripoint_bub_ms& p, const bool no_bashing, bool force) -> bool {
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

auto npc::can_open_door(const tripoint_bub_ms& p, const bool inside) const -> bool {
    return !rules.has_flag(ally_rule::avoid_doors) && get_map().can_open_door(this, p, inside);
}

auto npc::can_move_to(const tripoint_bub_ms& p, bool no_bashing) const -> bool {
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
        auto* np = dynamic_cast<npc*>(critter);
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

auto npc::saw_player_recently() const -> bool {
    return last_player_seen_pos && get_map().inbounds(*last_player_seen_pos)
        && last_seen_player_turn > 0;
}

auto npc::has_omt_destination() const -> bool { return goal != no_goal_point; }

void npc::reach_omt_destination() {
    if (!omt_path.empty()) { omt_path.clear(); }
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
    const auto city_size = get_option<int>("CITY_SIZE");
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

void npc::set_movement_mode(character_movemode new_mode) { move_mode = new_mode; }
