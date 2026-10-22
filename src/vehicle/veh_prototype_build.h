#pragma once

#include "type_id.h"

class JsonObject;
struct vehicle_prototype;

/// Reads a prototype's flags, colour palette, parts and item spawns from `jo` onto `proto`, after
/// the parts it already has; reports what is wrong through debugmsg as the loader does.
auto read_vehicle_prototype(const JsonObject& jo, vehicle_prototype& proto) -> void;

/// Builds `proto`'s blueprint vehicle as the game does once all data is loaded; reports what is
/// wrong through debugmsg.
auto finalize_vehicle_prototype(const vproto_id& id, vehicle_prototype& proto) -> void;

/// Puts `proto` in the game's registry as the prototype `id`, in place of the one it had.
auto register_vehicle_prototype(const vproto_id& id, vehicle_prototype&& proto) -> void;
