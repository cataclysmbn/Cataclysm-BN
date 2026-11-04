#include "cached_options.h"
#include "debug.h"
#include "filesystem.h"
#include "loading_ui.h"
#include "loading_ui_client.h"
#include "mod_manager.h"
#include "options.h"
#include "path_info.h"
#include "string_utils.h"
#include "utils/algo.h"
#include "worldfactory.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <unordered_set>
#include <vector>

namespace game_client {
namespace {
/// Loading image order is presentation-only and must never advance the simulation RNG.
auto loading_image_random_engine() -> std::minstd_rand0& // *NOPAD*
{
    // NOLINTNEXTLINE(cata-determinism)
    static auto engine = std::minstd_rand0(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return engine;
}
auto get_loading_image_search_roots(const MOD_INFORMATION& mod) -> std::unordered_set<std::string> {
    using namespace std::views;

    const auto modinfo_root =
        mod.path_full.empty()
            ? std::string{}
            : std::filesystem::path(mod.path_full).parent_path().generic_string();

    const auto root_paths = std::array<std::string, 2>{mod.path, modinfo_root};
    return root_paths | filter([](const std::string& root_path) { return !root_path.empty(); })
         | transform([](const std::string& root_path) {
               return std::filesystem::path(root_path).lexically_normal().generic_string();
           })
         | std::ranges::to<std::unordered_set>();
}

auto path_is_inside_root(
    const std::filesystem::path& root_path, const std::filesystem::path& candidate_path) -> bool {
    const auto normalized_root = root_path.lexically_normal();
    const auto normalized_candidate = candidate_path.lexically_normal();
    const auto mismatch = std::mismatch(
        normalized_root.begin(), normalized_root.end(), normalized_candidate.begin(),
        normalized_candidate.end());
    return mismatch.first == normalized_root.end();
}

auto can_choose_loading_image_path() -> bool {
    return world_generator != nullptr && world_generator->active_world;
}

auto get_loading_image_author(const std::string& loading_image_path) -> std::optional<std::string> {
    const auto image_stem = std::filesystem::path(loading_image_path).stem().generic_string();
    const auto author_parts = string_split(image_stem, '_');

    if (author_parts.size() < 2 || author_parts.front().empty()) { return {}; }
    return author_parts.front();
}

struct loading_image_match_options {
    const MOD_INFORMATION& mod;
    const std::string& image_name;
};

auto has_loading_image_extension(const std::string& path) -> bool {
    static const auto exts =
        std::unordered_set<std::string>{".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp"};
    auto ext = std::filesystem::path(path).extension().generic_string();
    std::ranges::transform(ext, ext.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return exts.contains(ext);
}

auto get_loading_images_from_directory(const std::string& directory_path)
    -> std::vector<std::string> {
    using namespace std::views;

    return get_files_from_path("", directory_path, true) | filter([](const std::string& path) {
               return file_exist(path) && has_loading_image_extension(path);
           })
         | std::ranges::to<std::vector>();
}

auto get_loading_image_matches_at_root(const std::string& image_name, const std::string& root_path)
    -> std::vector<std::string> {
    using namespace std::views;

    const auto normalized_root = std::filesystem::path(root_path).lexically_normal();
    const auto direct_path = (normalized_root / image_name).lexically_normal();
    if (!path_is_inside_root(normalized_root, direct_path)) {
        log_loading_image(
            string_format("mod loading image '%s' escapes root '%s'", image_name, root_path));
        return {};
    }

    const auto normalized_direct_path = direct_path.generic_string();
    if (file_exist(normalized_direct_path) && has_loading_image_extension(normalized_direct_path)) {
        return {normalized_direct_path};
    }
    if (dir_exist(normalized_direct_path)) {
        return get_loading_images_from_directory(normalized_direct_path);
    }

    const auto image_filename = std::filesystem::path(image_name).filename().generic_string();
    if (image_filename.empty()) { return {}; }

    const auto author_prefixed_filename = "_" + image_filename;

    return get_files_from_path(image_filename, root_path, true, true)
         | filter([&normalized_root, &image_filename,
                   &author_prefixed_filename](const std::string& path) {
               const auto normalized_path = std::filesystem::path(path).lexically_normal();
               const auto filename = normalized_path.filename().generic_string();
               return path_is_inside_root(normalized_root, normalized_path) && file_exist(path)
                   && has_loading_image_extension(path)
                   && (filename == image_filename || filename.ends_with(author_prefixed_filename));
           })
         | std::ranges::to<std::vector>();
}

auto get_loading_image_matches(const loading_image_match_options& opts)
    -> std::vector<std::string> {
    using namespace cata::ranges;

    return get_loading_image_search_roots(opts.mod) | flat_map([&](const std::string& root_path) {
               return get_loading_image_matches_at_root(opts.image_name, root_path);
           })
         | std::ranges::to<std::vector>();
}

auto get_loading_image_matches_for_mod(const MOD_INFORMATION* mod) -> std::vector<std::string> {
    using namespace cata::ranges;

    return mod->loading_images | flat_map([mod](const std::string& image_name) {
               return get_loading_image_matches(
                   loading_image_match_options{.mod = *mod, .image_name = image_name});
           })
         | std::ranges::to<std::vector>();
}

auto get_loading_image_paths(const std::vector<mod_id>& mods) -> std::unordered_set<std::string> {
    using namespace cata::ranges;
    using namespace std::views;

    const auto paths =
        mods | filter([](const mod_id& mod) { return mod.is_valid(); })
        | transform([](const mod_id& mod) { return &*mod; })
        | filter([](const MOD_INFORMATION* mod) { return !mod->loading_images.empty(); })
        | flat_map(get_loading_image_matches_for_mod) | std::ranges::to<std::vector>();
    return std::unordered_set<std::string>(paths.begin(), paths.end());
}

auto choose_loading_image_paths() -> std::vector<std::string> {
    if (!can_choose_loading_image_path()) { return {}; }

    const auto& world_info = *world_generator->active_world->info;
    const auto candidate_set = get_loading_image_paths(world_info.active_mod_order);
    auto candidates = std::vector<std::string>(candidate_set.begin(), candidate_set.end());
    game_client::shuffle_loading_image_paths(candidates);
    return candidates;
}

auto factory = loading_image_factory{};
auto observer = loading_observer{};

class selecting_only_renderer final: public selecting_loading_image_renderer {
public:
    auto draw(loading_image_selection_state& state) -> void override { select(state); }
};
} // namespace

auto log_loading_image(const std::string& message) -> void {
    static auto logged_messages = std::unordered_set<std::string>{};
    if (logged_messages.insert(message).second) {
        DebugLog(DL::Info, DC::Main) << "[loading_images] " << message;
    }
}

auto set_loading_image_factory(loading_image_factory value) -> void { factory = value; }
auto make_loading_image_renderer() -> std::unique_ptr<loading_image_renderer> {
    return factory ? factory() : nullptr;
}
auto make_selecting_loading_image_renderer() -> std::unique_ptr<loading_image_renderer> {
    return std::make_unique<selecting_only_renderer>();
}
auto set_loading_observer(const loading_observer value) -> void { observer = value; }
auto has_loading_observer() -> bool { return observer != nullptr; }
auto notify_loading(const loading_progress& progress) -> void {
    if (observer != nullptr) { observer(progress); }
}
auto shuffle_loading_image_paths(std::vector<std::string>& paths) -> void {
    std::shuffle(paths.begin(), paths.end(), loading_image_random_engine());
}

auto advance_loading_image(loading_image_selection_state& state) -> bool {
    if (state.paths.empty()) {
        state.current_path.clear();
        state.current_author.reset();
        return false;
    }
    if (state.next_path >= state.paths.size()) { state.next_path = 0; }

    state.current_path = state.paths[state.next_path++];
    state.current_author = get_loading_image_author(state.current_path);
    return true;
}

auto selecting_loading_image_renderer::select(loading_image_selection_state& state) -> void {
    if (!get_option<bool>("LOADING_SCREEN_IMAGES")) { return; }
    if (!state.lookup_attempted && can_choose_loading_image_path()) {
        state.paths = choose_loading_image_paths();
        state.next_path = 0;
        state.lookup_attempted = true;
    }
    if (!selected && state.lookup_attempted) {
        selected = true;
        advance_loading_image(state);
    }
}
} // namespace game_client
