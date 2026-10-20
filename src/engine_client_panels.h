#pragma once
#ifndef CATA_SRC_ENGINE_CLIENT_PANELS_H
#define CATA_SRC_ENGINE_CLIENT_PANELS_H

#include <expected>
#include <string>
#include <string_view>

#include "engine_client_component.h"
#include "engine_client_contract.h"
#include "engine_client_wire.h"

class avatar;
class Character;

namespace engine_client
{
/// A screen read without opening it, so that a client keeps it in a window of its own while the game goes on.
struct panel_request {
    std::string epoch = {};
    /// `character` or `inventory`.
    std::string panel = {};
    /// The row the window's focus is on; empty for the row the screen opens on.
    std::string row = {};
};
auto decode_panel_request( std::string_view input ) -> std::expected<panel_request, decode_error>;
/// The screen `request` names as it would open on the avatar, with nothing on it to invoke; nothing is drawn,
/// run or changed. `validation_failed` for an unknown panel.
auto panel_view( const panel_request &request ) -> std::expected<screen_view, error>;
auto serialize_screen_view( const screen_view &view ) -> std::string;
} // namespace engine_client

namespace character_display
{
/// The character sheet as it opens on `you`, with the tab of `row` in front and `row` focused when it has it.
auto passive_sheet( const Character &you, const std::string &row ) -> engine_client::screen_view;
} // namespace character_display

namespace game_menus::inv
{
/// The inventory as it opens on `you`, with the cursor on `row` when it has it.
auto passive_inventory( avatar &you, const std::string &row ) -> engine_client::screen_view;
} // namespace game_menus::inv

#endif // CATA_SRC_ENGINE_CLIENT_PANELS_H
