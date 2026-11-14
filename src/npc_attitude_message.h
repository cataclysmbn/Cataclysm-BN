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

/// Attitude group change the player sees, or nothing if the group stays the same.
/// NPCATT_ACTIVITY only marks an activity: entering it shows nothing,
/// and leaving it compares with the attitude before the activity.
auto visible_attitude_change( const npc &guy,
                              const attitude_change &change ) -> std::optional<attitude_group_change>;
