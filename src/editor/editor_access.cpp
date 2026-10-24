#include "editor/editor_access.h"

#include "avatar.h"
#include "translations.h"
#include "type_id.h"

namespace editor {
namespace {

const auto trait_DEBUG_EDITOR = trait_id("DEBUG_EDITOR");
auto session_allowed = false;

} // namespace

auto editor_denial() -> std::string {
    return session_allowed || get_avatar().has_trait(trait_DEBUG_EDITOR)
             ? std::string{}
             : _("You need the Debug Editor trait.");
}

auto allow_editor_this_session(const bool allowed) -> void { session_allowed = allowed; }

} // namespace editor
