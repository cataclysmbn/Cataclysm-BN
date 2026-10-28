#pragma once

#include <string>
#include <string_view>

namespace editor {

/// What a legend value places, as a person reads it.
struct legend_value_text {
    /// By game name, the weight of one of several choices after it: `dirt x3, grass`,
    /// `parameter roof`.
    std::string names;
    /// The ids it names, each once, in order: `t_dirt, t_grass`.
    std::string ids;
};

/// How the legend member `layer` reads, localized: `Terrain`, `Item groups`, `Signs`.
auto legend_layer_label(std::string_view layer) -> std::string;

/// The JSON text `value` that the legend member `layer` maps a symbol to, as names and ids.
auto legend_value(std::string_view layer, std::string_view value) -> legend_value_text;

/// A symbol as it can be seen: a space as `␣`.
auto shown_symbol(const std::string& symbol) -> std::string;

} // namespace editor
