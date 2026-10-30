#include "editor/library.h"

#include "catacharset.h"
#include "path_info.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace editor {

auto library_file(const std::string& kind) -> std::string {
    return PATH_INFO::config_dir() + "editor_library_" + kind + ".json";
}

auto library_pieces(const std::string& kind) -> std::vector<library_piece> {
    auto in = std::ifstream(library_file(kind), std::ios::binary);
    const auto read = nlohmann::ordered_json::parse(in, nullptr, false);
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

auto keep_piece(const std::string& kind, const library_piece& piece) -> void {
    auto pieces = library_pieces(kind);
    std::erase_if(pieces, [&](const library_piece& entry) { return entry.name == piece.name; });
    pieces.push_back(piece);
    auto written = nlohmann::ordered_json::array();
    for (const auto& entry : pieces) {
        written.push_back({{"name", entry.name}, {"data", entry.data}});
    }
    std::filesystem::create_directories(PATH_INFO::config_dir());
    auto out = std::ofstream(library_file(kind), std::ios::binary | std::ios::trunc);
    out << written.dump(2) << '\n';
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
    const auto read = nlohmann::ordered_json::parse(base64_decode(trimmed), nullptr, false);
    if (!read.is_object() || read.value("kind", "") != kind || !read.contains("data")) {
        return std::nullopt;
    }
    return read["data"];
}

} // namespace editor
