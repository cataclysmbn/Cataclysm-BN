#include "editor/editor_mode.h"

#include "options.h"

namespace editor {

auto advanced_mode() -> bool { return get_option<bool>("EDITOR_ADVANCED"); }

auto set_advanced_mode(const bool on) -> void {
    get_options().get_option("EDITOR_ADVANCED").setValue(on ? "true" : "false");
    get_options().save();
}

} // namespace editor
