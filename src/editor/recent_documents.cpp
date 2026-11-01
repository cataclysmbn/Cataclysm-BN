#include "editor/recent_documents.h"

#include "dependency_tree.h"
#include "editor/session_file.h"
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

auto mods_for(const std::filesystem::path& path) -> std::vector<mod_id> {
    auto mods = loaded_mods();
    if (const auto own = mod_of(path); own && !std::ranges::contains(mods, *own)) {
        mods.push_back(*own);
    }
    return mods;
}

auto editor_world_mods(const std::vector<mod_id>& initial) -> std::vector<mod_id> {
    auto& manager = world_generator->get_mod_manager();
    const auto kept =
        initial | std::views::filter(&mod_id::is_valid) | std::ranges::to<std::vector>();
    auto mods = kept.empty() ? manager.get_default_mods() : kept;
    auto ui = mod_ui(manager);
    // A core, or a mod needing a core the list lacks, would replace the game's and its mods.
    const auto needs_other_core = [&](const mod_id& mod) {
        return std::ranges::any_of(
            manager.get_tree().get_dependencies_of_X_as_strings(mod), [&](const mod_id& needed) {
                return needed.is_valid() && needed->core && !std::ranges::contains(mods, needed);
            });
    };
    for (const auto& mod : manager.all_mods()) {
        if (!mod->obsolete && !mod->core && inside(mod->path_full, PATH_INFO::user_moddir())
            && !needs_other_core(mod)) {
            // One on the list already, in conflict or with a dependency problem is refused.
            ui.try_add(mod, mods);
        }
    }
    return mods;
}

} // namespace editor
