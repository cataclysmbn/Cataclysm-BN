#include "engine_client_panels.h"

#include <algorithm>

#include "avatar.h"

namespace engine_client
{
namespace
{
/// The component with nothing on it that runs an affordance.
auto passive( component &value ) -> void
{
    value.affordances.clear();
    std::visit( []( auto & body ) {
        using body_t = std::decay_t<decltype( body )>;
        if constexpr( std::is_same_v<body_t, list_body> ) {
            body.activate.clear();
            body.filterable = false;
        } else if constexpr( std::is_same_v<body_t, tabs_body> ) {
            body.select.clear();
        } else if constexpr( std::is_same_v<body_t, form_body> ) {
            body.submit.clear();
        } else if constexpr( std::is_same_v<body_t, grid_body> ) {
            std::ranges::for_each( body.cells, []( auto & cell ) { cell.affordance.clear(); } );
            body.affordance.clear();
        }
    }, value.body );
    std::ranges::for_each( value.children, passive );
}
} // namespace

auto panel_view( const panel_request &request ) -> std::expected<screen_view, error>
{
    auto view = screen_view{};
    if( request.panel == "character" ) {
        view = character_display::passive_sheet( get_avatar(), request.row );
        view.kind = "character_sheet";
    } else if( request.panel == "inventory" ) {
        view = game_menus::inv::passive_inventory( get_avatar(), request.row );
        view.kind = "inventory";
    } else {
        return std::unexpected( error::validation_failed );
    }
    view.screen_id = "panel:" + request.panel;
    view.role = "screen";
    view.affordances.clear();
    view.denials.clear();
    view.shortcuts.clear();
    if( view.root ) { passive( *view.root ); }
    return view;
}
} // namespace engine_client
