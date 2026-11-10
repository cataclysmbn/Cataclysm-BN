#pragma once

#include <string>
#include <vector>

class JsonObject;

/// A button of the map's shortcut bar, like a Factorio shortcut prototype: it names a registered action of
/// the map's input context and the icon a client draws for it. Mods add buttons with new ids and replace one
/// by loading the same id again.
struct client_shortcut {
    std::string id;
    /// The registered action the button runs, as `action_ident` names it.
    std::string action;
    /// An opaque sprite id a client may map to its own art.
    std::string icon;
    /// What a client without that art draws instead.
    std::string glyph;
    /// Buttons sort by this, then by the order they were loaded.
    int order = 0;
};

namespace client_shortcuts
{
auto load( const JsonObject &jo, const std::string &src ) -> void;
/// Adds a shortcut, or replaces the one of the same id.
auto add( client_shortcut entry ) -> void;
auto reset() -> void;
/// Every loaded shortcut in button order.
auto all() -> const std::vector<client_shortcut> &; // *NOPAD*
} // namespace client_shortcuts
