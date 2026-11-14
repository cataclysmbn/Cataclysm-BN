#include "target_screen.h"

#include "input.h"

namespace ec = engine_client;

namespace target_screen
{

auto screen_of( const screen_options &opts ) -> ec::screen_view
{
    auto map = ec::component{ .id = map_id, .affordances = { move_id }, .body = ec::map_body{ .target = opts.target } };
    return {
        .title = opts.title,
        .root = ec::panel( { .id = panel_id, .title = opts.title, .stats = opts.stats, .child = std::move( map ) } ),
        .affordances = {
            ec::invoke_affordance( { .id = move_id, .label = opts.ctxt.get_action_name( "SELECT" ), .args = { .pos = ec::pos_shape{ .component_id = map_id } } } ),
        },
    };
}

auto candidate_id( const tripoint_bub_ms &at, const bool creature ) -> std::string
{
    return game_client::opaque_interaction_id( "target", {
        std::to_string( at.x() ), std::to_string( at.y() ), std::to_string( at.z() ), creature ? "creature" : "tile"
    } );
}

auto moved_to( const input_event &event ) -> std::optional<tripoint_bub_ms>
{
    const auto invoked = ec::invoked( event );
    if( !invoked || invoked->affordance_id != move_id || !invoked->args.position ) { return std::nullopt; }
    const auto &at = *invoked->args.position;
    return tripoint_bub_ms( at.x, at.y, at.z );
}

} // namespace target_screen
