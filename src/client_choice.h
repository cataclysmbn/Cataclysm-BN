#pragma once

#include "client_interaction.h"
#include "input.h"

#include <optional>
#include <string>

namespace game_client
{

/// What a client chose in the input read that just returned on `ctxt`: a row id, or a cancel.
struct chosen_row {
    std::string id;
    bool cancelled = false;
};

inline auto chosen_in( input_context &ctxt ) -> std::optional<chosen_row>
{
    const auto event = ctxt.get_raw_input();
    if( !event.interaction ) { return std::nullopt; }
    if( event.interaction->operation == interaction_operation::cancel ) {
        return chosen_row{ .cancelled = true };
    }
    if( event.interaction->operation == interaction_operation::choose ) {
        return chosen_row{ .id = event.interaction->target_id };
    }
    return std::nullopt;
}

/// The native action of the input read that just returned: `native` unless a client chose a row. A cancel
/// is `QUIT`, an `action:<NAME>` row is that action and any other row is applied by `on_row`, which
/// returns the action it stands for.
template<typename OnRow>
auto action_of( input_context &ctxt, const std::string &native, OnRow &&on_row ) -> std::string
{
    const auto chosen = chosen_in( ctxt );
    if( !chosen ) { return native; }
    if( chosen->cancelled ) { return "QUIT"; }
    if( chosen->id.starts_with( "action:" ) ) { return chosen->id.substr( 7 ); }
    return on_row( chosen->id );
}

} // namespace game_client
