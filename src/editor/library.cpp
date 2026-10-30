#include "editor/library.h"

#include "catacharset.h"
#include "fstream_utils.h"
#include "path_info.h"
#include "string_formatter.h"
#include "translations.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <utility>

namespace editor {
namespace {

/// Deeper than any piece nests; text that goes deeper is cut there, so that reading it does not
/// overflow the stack.
constexpr auto deepest = 32;

/// `text` read as JSON, its values nested deeper than `deepest` left out; discarded when it is not
/// JSON.
auto parsed(auto&& text) -> nlohmann::ordered_json {
    return nlohmann::ordered_json::parse(
        std::forward<decltype(text)>(text),
        [](const int depth, nlohmann::ordered_json::parse_event_t /*event*/,
           nlohmann::ordered_json& /*value*/) { return depth <= deepest; },
        false);
}

} // namespace

auto library_file(const std::string& kind) -> std::string {
    return PATH_INFO::config_dir() + "editor_library_" + kind + ".json";
}

auto library_pieces(const std::string& kind) -> std::vector<library_piece> {
    auto in = std::ifstream(library_file(kind), std::ios::binary);
    const auto read = parsed(in);
    if (!read.is_array()) { return {}; }
    auto pieces = std::vector<library_piece>{};
    // An entry in another shape, as a hand edit leaves, is skipped.
    for (const auto& entry : read) {
        if (!entry.is_object() || !entry.contains("name") || !entry["name"].is_string()
            || !entry.contains("data")) {
            continue;
        }
        pieces.push_back({.name = entry["name"].get<std::string>(), .data = entry["data"]});
    }
    return pieces;
}

auto keep_piece(const std::string& kind, const library_piece& piece)
    -> std::expected<void, std::string> {
    const auto file = library_file(kind);
    auto written = nlohmann::ordered_json::array();
    // The file stays when it cannot be read, so that a hand edit gone wrong loses no piece.
    if (auto error = std::error_code{}; std::filesystem::exists(file, error) || error) {
        auto in = std::ifstream(file, std::ios::binary);
        written = parsed(in);
        if (!written.is_array()) {
            //~ %s is the file of the editor's library.
            return std::unexpected(string_format(
                _("The game cannot read the library file %s; fix or remove it to save there."),
                file));
        }
    }
    // Entries in another shape stay as they are.
    written.erase(
        std::ranges::remove_if(
            written,
            [&](const nlohmann::ordered_json& entry) {
                return entry.is_object() && entry.contains("name") && entry["name"] == piece.name;
            })
            .begin(),
        written.end());
    written.push_back({{"name", piece.name}, {"data", piece.data}});
    auto error = std::error_code{};
    std::filesystem::create_directories(PATH_INFO::config_dir(), error);
    if (!write_to_file(file, [&](std::ostream& out) { out << written.dump(2) << '\n'; }, "")) {
        //~ %s is the file of the editor's library.
        return std::unexpected(
            string_format(_("The game could not write the library file %s."), file));
    }
    return {};
}

auto piece_text(const std::string& kind, const nlohmann::ordered_json& data) -> std::string {
    return base64_encode(nlohmann::ordered_json{{"kind", kind}, {"data", data}}.dump());
}

auto piece_of_text(const std::string& kind, const std::string& text)
    -> std::optional<nlohmann::ordered_json> {
    auto trimmed = text;
    std::erase_if(trimmed, [](const char letter) {
        return std::isspace(static_cast<unsigned char>(letter));
    });
    if (trimmed.empty()) { return std::nullopt; }
    const auto read = parsed(base64_decode(trimmed));
    if (!read.is_object() || !read.contains("kind") || read["kind"] != kind
        || !read.contains("data")) {
        return std::nullopt;
    }
    return read["data"];
}

} // namespace editor
