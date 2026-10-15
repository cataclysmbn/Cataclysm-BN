#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

class item;

namespace game_client
{

/// An item's identity within the session, for the rows of the screens: given the first time the item is
/// published, kept while it moves between inventory, worn gear and the ground, never saved, never in the state
/// hash, and never reused. Not a safe_reference id, which is time-prefixed and written into saves.
auto item_serial( const item &it ) -> std::uint64_t;

/// The row id of one item instance: `item:<serial>`.
auto item_row_id( const item &it ) -> std::string;

/// The row id of a stack of interchangeable items, from the serial of its lowest member: `item:<serial>` for
/// one item, `stack:<serial>` for several. Never from a position.
auto stack_row_id( std::span<const item *const> members ) -> std::string;

/// The serial an item already holds; unlike `item_serial` it gives none.
auto find_item_serial( const item &it ) -> std::optional<std::uint64_t>;

/// The item holding `serial`, or null when it never did or is gone.
auto item_with_serial( std::uint64_t serial ) -> const item *;

/// The items a `stack:<lowest>` row was last published for, as serials of the items still alive.
auto stack_member_serials( std::uint64_t lowest ) -> std::vector<std::uint64_t>;

/// Drops the serial of an item that is being destroyed.
auto forget_item_serial( const item *it ) -> void;

/// Drops every serial and starts the numbering again: for a loaded world, and for the start of a recorded or
/// replayed session, whose inputs name rows by these ids. The serials of diary pages go with them.
auto reset_item_serials() -> void;

/// How many items hold a serial.
auto item_serial_count() -> std::size_t;

} // namespace game_client
