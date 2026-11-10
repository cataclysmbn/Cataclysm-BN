#pragma once

#include "client_interaction.h"
#include "coordinates.h"
#include "engine_client_component.h"

#include <optional>
#include <string>
#include <vector>

class input_context;
struct input_event;

/// The screens that move a cursor over the map, such as aiming and looking: the map with the cursor's facts
/// and the one affordance that puts the cursor on a square.
namespace target_screen
{

inline constexpr auto panel_id = "target";
inline constexpr auto map_id = "target:map";
inline constexpr auto move_id = "target:move";

struct screen_options {
    std::string title;
    /// The cursor, its range and what is on its square; positions are in the bubble.
    game_client::interaction_target target;
    /// What the screen reports besides the cursor: ranges, ammo and so on.
    std::vector<engine_client::panel_stat> stats;
    /// The screen's input context, which names the move.
    const input_context &ctxt;
};

/// The target screen as components, composed from the cursor's model state.
auto screen_of( const screen_options &opts ) -> engine_client::screen_view;

/// The id of a candidate the cursor can jump to: where it is and what kind it is; no text goes in.
auto candidate_id( const tripoint_bub_ms &at, bool creature ) -> std::string;

/// The square a client moved the cursor to in the input read that just returned; none for any other input.
auto moved_to( const input_event &event ) -> std::optional<tripoint_bub_ms>;

} // namespace target_screen
