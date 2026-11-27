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

namespace {
const std::vector<bionic_id> power_cbms = {{
    bio_advreactor,
    bio_furnace,
    bio_reactor,
}};


} // namespace

// Used in npc::drop_items()
struct ratio_index {
    double ratio;
    int index;
    ratio_index(double R, int I): ratio(R), index(I) {}
};

static auto wants_to_reload(const npc& who, const item& it) -> bool {
    if (!who.can_reload(it)) { return false; }

    const int required = it.ammo_required();
    // TODO: Add bandolier check here, once they can be reloaded
    if (required < 1 && !it.is_magazine()) { return false; }

    const int remaining = it.ammo_remaining();
    return remaining < required || remaining < it.ammo_capacity();
}

static auto wants_to_reload_with(const item& weap, const item& ammo, bool danger) -> bool {
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

auto npc::find_reloadable() -> item& {
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

auto npc::find_reloadable() const -> const item& {
    return const_cast<const item&>(const_cast<npc*>(this)->find_reloadable());
}

auto npc::can_reload_current() -> bool {
    if (!primary_weapon().is_gun() || !wants_to_reload(*this, primary_weapon())) { return false; }

    return static_cast<bool>(find_usable_ammo(primary_weapon()));
}

auto npc::find_usable_ammo(item& weap) -> item* {
    if (!can_reload(weap)) { return nullptr; }

    auto loc = reload_selection::prepare(*this, weap).selected.ammo;
    if (!loc || !wants_to_reload_with(weap, *loc, ai_cache.danger > 0)) { return nullptr; }

    return loc;
}

auto npc::find_usable_ammo(item& weap) const -> item* {
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

auto npc::activate_bionic_by_id(const bionic_id& cbm_id, bool eff_only) -> bool {
    if (has_bionic(cbm_id)) {
        bionic& bio = get_bionic_state(cbm_id);
        if (!bio.powered) { return activate_bionic(bio, eff_only); }
    }
    return false;
}

auto npc::use_bionic_by_id(const bionic_id& cbm_id, bool eff_only) -> bool {
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

auto npc::deactivate_bionic_by_id(const bionic_id& cbm_id, bool eff_only) -> bool {
    if (has_bionic(cbm_id)) {
        bionic& bio = get_bionic_state(cbm_id);
        if (bio.powered) { return deactivate_bionic(bio, eff_only); }
    }
    return false;
}

auto npc::wants_to_recharge_cbm() -> bool {
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

auto npc::can_use_offensive_cbm() const -> bool {
    const float allowed_ratio = static_cast<int>(rules.cbm_reserve) / 100.0f;
    return get_power_level() > get_max_power_level() * allowed_ratio;
}

auto npc::consume_cbm_items(const std::function<bool(const item&)>& filter) -> bool {
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

auto npc::recharge_cbm() -> bool {
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

auto npc::patient_assessment(const Character& c) -> healing_options {
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

static auto nearest_passable(const tripoint_bub_ms& p, const tripoint_bub_ms& closest_to)
    -> std::optional<tripoint_bub_ms> {
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

template <typename T> auto npc_pickup_from_stack(npc& who, T& items) -> std::vector<item*> {
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

auto npc::pick_up_item_map(const tripoint_bub_ms& where) -> std::vector<item*> {
    map_stack stack = get_map().i_at(where);
    return npc_pickup_from_stack(*this, stack);
}

auto npc::pick_up_item_vehicle(vehicle& veh, int part_index) -> std::vector<item*> {
    auto stack = veh.get_items(part_index);
    return npc_pickup_from_stack(*this, stack);
}

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

auto npc::find_corpse_to_pulp() -> bool {
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

auto npc::do_pulp() -> bool {
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

auto npc::wield_better_weapon() -> bool {
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

static auto rate_food(const item& it, int want_nutr, int want_quench) -> float {
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

auto npc::consume_food() -> bool {
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

auto npc::adjust_worn() -> bool {
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
