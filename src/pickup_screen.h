#pragma once

#include "engine_client_component.h"
#include "item_stack.h"
#include "units.h"

#include <cstddef>
#include <list>
#include <optional>
#include <string>
#include <vector>

class input_context;

/// One stack of the pickup list: whether it is marked, how many of it, and how the stacks nest.
struct pickup_count {
    bool pick = false;
    // nullopt if the whole stack is being picked up, nonzero otherwise.
    std::optional<int> count;
    std::optional<size_t> parent;
    std::vector<size_t> children;
    bool all_children_picked = false;
};

namespace pickup
{

/// Where the stacks of a pickup list lie.
enum class source_kind { ground, vehicle_cargo, nearby };

/// The affordances the pickup screen runs from its own loop, and the list they act on.
inline constexpr auto items_id = "pickup:items";
inline constexpr auto toggle_id = "pickup:toggle";
inline constexpr auto count_id = "pickup:count";
inline constexpr auto confirm_id = "pickup:confirm";
inline constexpr auto cancel_id = "pickup:cancel";

/// The live state of a pickup list.
struct screen_options {
    const std::vector<std::list<item_stack::iterator>> &stacked_here;
    const std::vector<pickup_count> &getitem;
    /// The indexes of the stacks the filter lets through, in the order the list shows them.
    const std::vector<int> &matches;
    const std::string &filter;
    source_kind source;
    /// Where the stacks lie, as data: it goes into the row ids.
    const std::string &source_identity;
    /// The cursor, as an index into `matches`.
    int selected;
    /// What the avatar would carry once every marked stack is picked up.
    units::mass weight_predict;
    units::volume volume_predict;
    /// The letters the native list prints before its rows, in row order.
    std::string hotkeys;
    /// The pickup screen's input context, which names the actions.
    const input_context &ctxt;
};

/// The id of each row of `matches`, from where its items lie and what they are: no text goes in.
auto choice_ids( const screen_options &opts ) -> std::vector<std::string>;

/// The pickup list as components, composed from its model state.
auto screen_of( const screen_options &opts ) -> engine_client::screen_view;

} // namespace pickup
