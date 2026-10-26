#include "editor/recent_documents.h"

#include "mod_manager.h"
#include "path_info.h"
#include "world.h"
#include "worldfactory.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <ranges>

namespace editor {

auto recent_documents_file() -> std::string {
    return PATH_INFO::config_dir() + "editor_recent.json";
}

auto recent_documents() -> std::vector<recent_document> {
    auto in = std::ifstream(recent_documents_file(), std::ios::binary);
    const auto read = nlohmann::json::parse(in, nullptr, false);
    if (!read.is_array()) { return {}; }
    auto recent = std::vector<recent_document>{};
    const auto text = [](const nlohmann::json& entry, const char* const key) {
        return entry.contains(key) && entry[key].is_string();
    };
    // An entry in another shape, as a hand edit leaves, is skipped; mods that are no names are.
    for (const auto& entry : read) {
        if (!entry.is_object() || !text(entry, "editor") || !text(entry, "name")) { continue; }
        auto mods = std::vector<mod_id>{};
        if (entry.contains("mods") && entry["mods"].is_array()) {
            for (const auto& mod : entry["mods"]) {
                if (mod.is_string()) { mods.emplace_back(mod.get<std::string>()); }
            }
        }
        recent.push_back(
            {.editor = entry["editor"].get<std::string>(),
             .name = entry["name"].get<std::string>(),
             .mods = std::move(mods)});
    }
    return recent;
}

auto remember_document(const recent_document& opened) -> void {
    auto recent = recent_documents();
    std::erase_if(recent, [&](const recent_document& entry) {
        return entry.editor == opened.editor && entry.name == opened.name;
    });
    recent.insert(recent.begin(), opened);
    recent.resize(std::min(recent.size(), recent_limit));
    auto written = nlohmann::json::array();
    for (const auto& entry : recent) {
        written.push_back(
            {{"editor", entry.editor},
             {"name", entry.name},
             {"mods",
              entry.mods | std::views::transform(&mod_id::str) | std::ranges::to<std::vector>()}});
    }
    auto out = std::ofstream(recent_documents_file(), std::ios::binary | std::ios::trunc);
    out << written.dump(2) << '\n';
}

auto loaded_mods() -> std::vector<mod_id> {
    return world_generator->active_world
             ? world_generator->active_world->info->active_mod_order
             : std::vector<mod_id>{mod_management::get_default_core_content_pack()};
}

} // namespace editor
