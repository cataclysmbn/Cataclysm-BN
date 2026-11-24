#pragma once

#include <optional>

class npc;
enum npc_attitude : int;
enum class attitude_group : int;

struct attitude_change {
    npc_attitude from;
    npc_attitude to;
    /// Attitude the NPC had before it started its current activity.
    npc_attitude before_activity;
};

struct attitude_group_change {
    attitude_group from;
    attitude_group to;
};

/// Returns the attitude group change to show to the player, or nothing when the group stays the
/// same. Show nothing when the NPC starts an activity (NPCATT_ACTIVITY). When the NPC stops an
/// activity, compare with the attitude before the activity.
auto visible_attitude_change(const npc& guy, const attitude_change& change)
    -> std::optional<attitude_group_change>;
