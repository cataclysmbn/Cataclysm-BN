#include "map/utils/map_functions.h"

#include "character.h"
#include "game.h"
#include "line.h"
#include "map.h"
#include "map_iterator.h"
#include "mapbuffer.h"
#include "mapbuffer_registry.h"
#include "messages.h"
#include "monster.h"
#include "sounds.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vpart_position.h"

#include <algorithm>
#include <cstdlib>

static const mtype_id mon_mi_go_myrmidon("mon_mi_go_myrmidon");

namespace {

auto remove_migo_nerve_cage_terrain(mapbuffer& buffer, const tripoint_abs_ms& p) -> bool {
    auto open = false;
    for (const auto& tmp : simulated_tiles_in_radius(buffer, p, 12)) {
        if (tmp.ter() == ter_id("t_wall_resin_cage")) {
            buffer.set_ter(tmp.abs_pos(), ter_id("t_floor_resin"));
            open = true;
        }
    }
    return open;
}

auto finish_migo_nerve_cage_removal(
    mapbuffer& buffer, const tripoint_abs_ms& p, const bool spawn_damaged, const bool open)
    -> void {
    if (open) {
        add_msg(m_good, _("The nerve cluster collapses in on itself, and the nearby cages open!"));
    } else {
        add_msg(_("The nerve cluster collapses in on itself, to no discernible effect."));
    }
    sound_event se;
    se.origin = p;
    se.volume = 120;
    se.category = sounds::sound_t::combat;
    se.description = _("a loud alien shriek reverberating through the structure!");
    se.id = "shout";
    se.variant = "scream_tortured";
    sounds::sound(se);
    monster* const spawn = buffer.place_critter_around(mon_mi_go_myrmidon, p, 1);
    if (spawn_damaged) { spawn->set_hp(spawn->get_hp_max() / 2); }
    // Don't give the mi-go free shots against the player
    spawn->mod_moves(-300);
    if (buffer.get_dimension_id() == get_player_character().get_dimension()
        && get_player_character().sees(p)) {
        add_msg(m_bad,
                _("Something stirs and clambers out of the ruined mass of flesh and nerves!"));
    }
}

auto has_vehicle_obstacle(const optional_vpart_position& vp) -> bool {
    return vp && vp.obstacle_at_part().has_value();
}

auto has_vehicle_floor_or_obstacle(const optional_vpart_position& vp) -> bool {
    return vp.part_with_feature(VPFLAG_BOARDABLE, true).has_value() || has_vehicle_obstacle(vp);
}

auto has_vehicle_ceiling(const optional_vpart_position& vp) -> bool {
    return vp.part_with_feature(VPFLAG_ROOF, true).has_value();
}

auto vehicle_vertical_barrier_between(
    mapbuffer& m, const tripoint_abs_ms& from, const tripoint_abs_ms& to) -> bool {
    const auto upper_z = std::max(from.z(), to.z());
    const auto lower_z = std::min(from.z(), to.z());
    const auto upper_from = tripoint_abs_ms(from.xy(), upper_z);
    const auto upper_to = tripoint_abs_ms(to.xy(), upper_z);
    const auto lower_from = tripoint_abs_ms(from.xy(), lower_z);
    const auto lower_to = tripoint_abs_ms(to.xy(), lower_z);
    return has_vehicle_floor_or_obstacle(m.veh_at(upper_from))
        || has_vehicle_ceiling(m.veh_at(lower_from))
        || (from.xy() != to.xy()
            && (has_vehicle_floor_or_obstacle(m.veh_at(upper_to))
                || has_vehicle_ceiling(m.veh_at(lower_to))));
}

auto physical_floor_between(mapbuffer& m, const tripoint_abs_ms& from, const tripoint_abs_ms& to)
    -> bool {
    const auto z_delta = std::abs(from.z() - to.z());
    if (z_delta == 0) { return false; }
    if (z_delta > 1) { return true; }
    return m.floor_between(from, to) || vehicle_vertical_barrier_between(m, from, to);
}

auto is_inside_vehicle(const optional_vpart_position& vp) -> bool {
    return vp && vp->vehicle().enclosed_at(vp->abs_pos());
}

auto vehicle_enclosure_between(mapbuffer& m, const tripoint_abs_ms& from, const tripoint_abs_ms& to)
    -> bool {
    if (from == to || from.z() != to.z()) { return false; }

    const auto from_vp = m.veh_at(from);
    const auto to_vp = m.veh_at(to);
    return is_inside_vehicle(from_vp) != is_inside_vehicle(to_vp) || has_vehicle_obstacle(from_vp)
        || has_vehicle_obstacle(to_vp);
}

auto physical_barrier_between(mapbuffer& m, const tripoint_abs_ms& from, const tripoint_abs_ms& to)
    -> bool {
    return physical_floor_between(m, from, to) || vehicle_enclosure_between(m, from, to);
}

} // namespace

namespace map_funcs {

auto physical_clear_path(const physical_clear_path_opts& opts) -> bool {
    if (opts.require_clear_path
        && !opts.here.clear_path(opts.from, opts.to, opts.range, opts.cost_min, opts.cost_max)) {
        return false;
    }

    auto previous = opts.from;
    for (const auto& point : line_to(opts.from, opts.to)) {
        if (physical_barrier_between(opts.here, previous, point)) { return false; }
        previous = point;
    }
    return true;
}

auto climbing_cost(mapbuffer& buffer, const tripoint_abs_ms& from, const tripoint_abs_ms& to)
    -> std::optional<int> {
    // TODO: All sorts of mutations, equipment weight etc. for characters
    const auto move_options = mapbuffer_valid_move_options{
        .flying = true,
    };
    if (!buffer.valid_move(from, to, move_options)) { return {}; }

    const auto diff = buffer.climb_difficulty(from);
    if (!diff || *diff > 5) { return {}; }
    return 50 + *diff * 100;
}

auto climbing_cost(const map& m, const tripoint_bub_ms& from, const tripoint_bub_ms& to)
    -> std::optional<int> {
    return climbing_cost(m.get_mapbuffer(), bub_to_abs(from), bub_to_abs(to));
}

auto migo_nerve_cage_removal(mapbuffer& buffer, const tripoint_abs_ms& p, const bool spawn_damaged)
    -> void {
    finish_migo_nerve_cage_removal(
        buffer, p, spawn_damaged, remove_migo_nerve_cage_terrain(buffer, p));
}

void migo_nerve_cage_removal(map& m, const tripoint_bub_ms& p, bool spawn_damaged) {
    auto& buffer = m.get_mapbuffer();
    finish_migo_nerve_cage_removal(
        buffer, bub_to_abs(p), spawn_damaged,
        remove_migo_nerve_cage_terrain(buffer, bub_to_abs(p)));
}

} // namespace map_funcs
