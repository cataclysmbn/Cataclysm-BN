#pragma once

#include "coordinates.h"

#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>

class vehicle;
struct vehicle_part;
class player;
class vpart_info;

enum vpart_bitflags : int;
class vpart_reference;

/**
 * Reference to a position (a point) of the @ref vehicle.
 * It does not refer to a specific vehicle part, but to a mount point of a
 * vehicle that contains one or more vehicle parts.
 *
 * It's supposed to be the basic vehicle interface for the @ref map.
 * Another class shall be used to get a reference to a specific part of the
 * vehicle.
 *
 * Note that it must be created with a valid vehicle reference and a valid
 * part index. An instance can become invalid when the referenced vehicle is
 * changed (parts added / removed or whole vehicle removed). There is no way
 * to detect this (it behaves like C++ references).
 */
class vpart_position {
private:
    std::reference_wrapper<::vehicle> vehicle_;
    // silence GCC maybe-uninitialized warning by giving it a default value
    size_t part_index_ = std::numeric_limits<size_t>::max();

public:
    vpart_position(::vehicle& v, const size_t part): vehicle_(v), part_index_(part) {}
    vpart_position(const vpart_position&) = default;
    auto operator=(const vpart_position&) -> vpart_position& = default;

    auto vehicle() const -> ::vehicle& { return vehicle_.get(); }
    // TODO: remove this, add a vpart_reference class instead
    auto part_index() const -> size_t { return part_index_; }

    auto is_inside() const -> bool;

    /**
     * Sets the label at this part of the vehicle. Removes the label if @p text is empty.
     */
    void set_label(const std::string& text) const;
    /**
     * @returns The label at this part of the vehicle, if there is any.
     */
    auto get_label() const -> std::optional<std::string>;
    /// @see vehicle::part_with_feature
    auto part_with_feature(const std::string& f, bool unbroken) const
        -> std::optional<vpart_reference>;
    /// @see vehicle::part_with_feature
    auto part_with_feature(vpart_bitflags f, bool unbroken) const -> std::optional<vpart_reference>;
    /**
     * Returns the obstacle that exists at this point of the vehicle (if any).
     * Open doors don't count as obstacles, but closed one do.
     * Broken parts are also never obstacles.
     */
    auto obstacle_at_part() const -> std::optional<vpart_reference>;
    /**
     * Returns the part displayed at this point of the vehicle.
     */
    auto part_displayed() const -> std::optional<vpart_reference>;
    /**
     * Returns the position of this part in the coordinates system that @ref game::m uses.
     * Postcondition (if the vehicle cache of the map is correct and if there are un-removed
     * parts at this positions):
     * `g->m.veh_at( this->pos() )` (there is a vehicle there)
     * `g->m.veh_at( this->pos() )->vehicle() == this->vehicle()` (it's this one)
     */
    // Name chosen to match Creature::pos
    auto pos() const -> tripoint_bub_ms;
    /**
     * Returns the mount point: the point in the vehicles own coordinate system.
     * This system is independent of movement / rotation.
     */
    auto mount() const -> tripoint_mnt_veh;
};

/**
 * Simple wrapper to forward functions that may return a @ref std::optional
 * to @ref vpart_position. They generally return an empty `optional`, or
 * forward to the same function in `vpart_position`.
 */
class optional_vpart_position: public std::optional<vpart_position> {
public:
    optional_vpart_position(std::optional<vpart_position> p): std::optional<vpart_position>(p) {}

    auto get_label() const -> std::optional<std::string> {
        return has_value() ? value().get_label() : std::nullopt;
    }
    auto part_with_feature(const std::string& f, bool unbroken) const
        -> std::optional<vpart_reference>;
    auto part_with_feature(vpart_bitflags f, bool unbroken) const -> std::optional<vpart_reference>;
    auto obstacle_at_part() const -> std::optional<vpart_reference>;
    auto part_displayed() const -> std::optional<vpart_reference>;
};

/**
 * This is a wrapper over a vehicle pointer and a reference to a part of it.
 *
 * The class does not support an "invalid" state, it is created from a
 * valid reference and the user must ensure it's still valid when used.
 * Most functions just forward to the equally named functions in the @ref vehicle
 * class, so see there for documentation.
 */
class vpart_reference: public vpart_position {
public:
    vpart_reference(::vehicle& v, const size_t part): vpart_position(v, part) {}
    vpart_reference(const vpart_reference&) = default;
    auto operator=(const vpart_reference&) -> vpart_reference& = default;

    using vpart_position::vehicle;

    /// Yields the \ref vehicle_part object referenced by this. @see vehicle::parts
    auto part() const -> vehicle_part&;
    /// See @ref vehicle_part::info
    auto info() const -> const vpart_info&;
    /**
     * Returns whether the part *type* has the given feature.
     * Note that this is different from part flags (which apply to part
     * instances).
     * For example a feature is "CARGO" (the part can store items).
     */
    /**@{*/
    auto has_feature(const std::string& f) const -> bool;
    auto has_feature(vpart_bitflags f) const -> bool;
    /**@}*/

    /// Returns the passenger in this part, or nullptr if no passenger.
    auto get_passenger() const -> player*;
};

// For legacy code, phase out, don't use in new code.
// TODO: remove this
inline auto veh_pointer_or_null(const optional_vpart_position& p) -> vehicle* {
    return p ? &p->vehicle() : nullptr;
}
