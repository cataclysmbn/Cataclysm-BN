#include "editor/editor_access.h"

#include "avatar.h"
#include "enums.h"
#include "game.h"
#include "input.h"
#include "string_formatter.h"
#include "translations.h"
#include "type_id.h"

namespace editor {
namespace {

const auto trait_DEBUG_EDITOR = trait_id("DEBUG_EDITOR");
auto session_allowed = false;

} // namespace

auto editor_denial() -> std::string {
    if (session_allowed || get_avatar().has_trait(trait_DEBUG_EDITOR)) { return {}; }
    auto ctxt = input_context("DEFAULTMODE");
    ctxt.register_action("debug");
    //~ %s is the key that opens the debug menu; Player…, Mutate and Debug Editor name its entries.
    return string_format(
        _("You need the Debug Editor trait: open the debug menu (%s), then Player…, Mutate, and "
          "pick Debug Editor."),
        ctxt.get_desc("debug", 1));
}

auto allow_editor_this_session(const bool allowed) -> void { session_allowed = allowed; }

auto in_editor_game() -> bool { return g->gametype() == special_game_type::EDITOR; }

auto end_editor_game() -> void {
    if (!in_editor_game()) { return; }
    get_avatar().set_moves(0);
    g->uquit = QUIT_NOSAVED;
}

} // namespace editor
