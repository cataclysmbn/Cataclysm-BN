#include "editor/mapgen_legend_edit.h"

#include "editor/document.h"

namespace editor {

auto set_legend(document& doc, const legend_edit& edit) -> std::expected<void, std::string> {
    if (edit.object >= doc.objects().size()) {
        return std::unexpected("no object " + std::to_string(edit.object));
    }
    const auto& value = doc.objects()[edit.object].value;
    // A palette maps its symbols itself; a mapgen in its object.
    const auto palette = value.value("type", "") == "palette";
    if (!palette && (!value.contains("object") || !value["object"].is_object())) {
        return std::unexpected(std::string("the entry has no object"));
    }
    const auto& object = palette ? value : value["object"];
    using json_pointer = nlohmann::ordered_json::json_pointer;
    const auto layer = (palette ? json_pointer() : json_pointer("/object")) / edit.layer;
    const auto mapping = layer / edit.symbol;
    const auto has_layer = object.contains(edit.layer) && object[edit.layer].is_object();
    auto patch = nlohmann::ordered_json::array();
    if (edit.value && !has_layer) {
        patch.push_back(
            {{"op", "add"},
             {"path", layer.to_string()},
             {"value", nlohmann::ordered_json{{edit.symbol, *edit.value}}}});
    } else if (edit.value) {
        patch.push_back({{"op", "add"}, {"path", mapping.to_string()}, {"value", *edit.value}});
    } else if (has_layer && object[edit.layer].size() == 1
               && object[edit.layer].contains(edit.symbol)) {
        patch.push_back({{"op", "remove"}, {"path", layer.to_string()}});
    } else {
        patch.push_back({{"op", "remove"}, {"path", mapping.to_string()}});
    }
    return doc.edit(edit.object, patch);
}

} // namespace editor
