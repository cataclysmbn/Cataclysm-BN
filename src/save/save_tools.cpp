#include "save_tools.h"

#include "compress.h"
#include "game_constants.h"
#include "json.h"
#include "sqlite3.h"
#include "string_formatter.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <limits>
#include <optional>
#include <ranges>
#include <sstream>
#include <stdexcept>

namespace save_tools {
namespace {

struct sqlite_db {
    explicit sqlite_db(const std::filesystem::path& path) {
        auto* opened = static_cast<sqlite3*>(nullptr);
        auto ret = sqlite3_open_v2(path.string().c_str(), &opened, SQLITE_OPEN_READWRITE, nullptr);
        if (ret != SQLITE_OK) {
            const auto message =
                opened != nullptr ? sqlite3_errmsg(opened) : "unknown sqlite error";
            if (opened != nullptr) { sqlite3_close(opened); }
            throw std::runtime_error(
                string_format("Failed to open %s: %s", path.string().c_str(), message));
        }
        db = opened;
    }

    sqlite_db(const sqlite_db&) = delete;
    auto operator=(const sqlite_db&) -> sqlite_db& = delete;

    ~sqlite_db() {
        if (db != nullptr) { sqlite3_close(db); }
    }

    auto get() const -> sqlite3* { // *NOPAD*
        return db;
    }

private:
    sqlite3* db = nullptr;
};

struct sqlite_stmt {
    sqlite_stmt(sqlite3* db, const char* sql) {
        auto* prepared = static_cast<sqlite3_stmt*>(nullptr);
        if (sqlite3_prepare_v2(db, sql, -1, &prepared, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(db));
        }
        stmt = prepared;
    }

    sqlite_stmt(const sqlite_stmt&) = delete;
    auto operator=(const sqlite_stmt&) -> sqlite_stmt& = delete;

    ~sqlite_stmt() {
        if (stmt != nullptr) { sqlite3_finalize(stmt); }
    }

    auto get() const -> sqlite3_stmt* { // *NOPAD*
        return stmt;
    }

private:
    sqlite3_stmt* stmt = nullptr;
};

struct transaction {
    explicit transaction(sqlite3* db): db(db) { exec_sql("BEGIN TRANSACTION"); }

    transaction(const transaction&) = delete;
    auto operator=(const transaction&) -> transaction& = delete;

    ~transaction() {
        if (active) { sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr); }
    }

    auto commit() -> void {
        exec_sql("COMMIT");
        active = false;
    }

private:
    auto exec_sql(const char* sql) -> void {
        auto* err = static_cast<char*>(nullptr);
        if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            auto message = std::string(err != nullptr ? err : sqlite3_errmsg(db));
            sqlite3_free(err);
            throw std::runtime_error(message);
        }
    }

    sqlite3* db;
    bool active = true;
};

enum class save_path_kind {
    unknown,
    map_omt,
    overmap,
    overmap_seen,
    map_memory,
};

struct save_path_info {
    save_path_kind kind = save_path_kind::unknown;
    point_abs_om overmap;
    point_abs_omt omt;
};

auto sqlite_files(const std::filesystem::path& world_path) -> std::vector<std::filesystem::path> {
    auto files = std::vector<std::filesystem::path>{};
    if (!std::filesystem::exists(world_path)) {
        throw std::runtime_error(
            string_format("World path does not exist: %s", world_path.string().c_str()));
    }
    for (const auto& entry : std::filesystem::directory_iterator(world_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".sqlite3") {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    return files;
}

auto strip_dimension_prefix(const std::string& path) -> std::string {
    constexpr auto prefix = std::string_view("dimensions/");
    if (!path.starts_with(prefix)) { return path; }
    const auto after_prefix = prefix.size();
    const auto separator = path.find('/', after_prefix);
    if (separator == std::string::npos) { return path; }
    return path.substr(separator + 1);
}

auto parse_int_tail(const std::string& text, const char* format) -> std::optional<tripoint> {
    auto x = 0;
    auto y = 0;
    auto z = 0;
    if (std::sscanf(text.c_str(), format, &x, &y, &z) == 3) { return tripoint(x, y, z); }
    return std::nullopt;
}

auto parse_point_tail(const std::string& text, const char* format) -> std::optional<point> {
    auto x = 0;
    auto y = 0;
    if (std::sscanf(text.c_str(), format, &x, &y) == 2) { return point(x, y); }
    return std::nullopt;
}

auto classify_save_path(const std::string& path) -> save_path_info {
    const auto local_path = strip_dimension_prefix(path);
    if (local_path.starts_with("maps/")) {
        const auto file_start = local_path.find_last_of("/\\");
        const auto filename =
            file_start == std::string::npos ? local_path : local_path.substr(file_start + 1);
        if (const auto parsed = parse_int_tail(filename, "%d.%d.%d.map")) {
            const auto omt = tripoint_abs_omt(parsed->x, parsed->y, parsed->z);
            return {
                .kind = save_path_kind::map_omt,
                .overmap = project_to<coords::om>(omt).xy(),
                .omt = omt.xy(),
            };
        }
    }
    if (local_path.starts_with("o.")) {
        if (const auto parsed = parse_point_tail(local_path, "o.%d.%d")) {
            return {
                .kind = save_path_kind::overmap,
                .overmap = point_abs_om(parsed->x, parsed->y),
                .omt = {},
            };
        }
    }
    if (local_path.starts_with(".seen.")) {
        if (const auto parsed = parse_point_tail(local_path, ".seen.%d.%d")) {
            return {
                .kind = save_path_kind::overmap_seen,
                .overmap = point_abs_om(parsed->x, parsed->y),
                .omt = {},
            };
        }
    }
    if (local_path.ends_with(".mmr")) {
        if (const auto parsed = parse_int_tail(local_path, "%d.%d.%d.mmr")) {
            const auto mmr = tripoint_abs_mmr(parsed->x, parsed->y, parsed->z);
            return {
                .kind = save_path_kind::map_memory,
                .overmap = project_to<coords::om>(mmr).xy(),
                .omt = {},
            };
        }
    }
    return {};
}

auto is_inside_bubble(const point_abs_omt& pos, const save_prune_bubble& bubble) -> bool {
    const auto dx = std::abs(pos.x() - bubble.center.x());
    const auto dy = std::abs(pos.y() - bubble.center.y());
    return std::max(dx, dy) <= bubble.radius;
}

auto overmap_origin_omt(const point_abs_om& overmap) -> point_abs_omt {
    return {overmap.x() * OMAPX, overmap.y() * OMAPY};
}

auto overmap_intersects_bubble(const point_abs_om& overmap, const save_prune_bubble& bubble)
    -> bool {
    const auto origin = overmap_origin_omt(overmap);
    const auto min_x = origin.x();
    const auto max_x = origin.x() + OMAPX - 1;
    const auto min_y = origin.y();
    const auto max_y = origin.y() + OMAPY - 1;
    const auto closest_x = std::clamp(bubble.center.x(), min_x, max_x);
    const auto closest_y = std::clamp(bubble.center.y(), min_y, max_y);
    return is_inside_bubble(point_abs_omt(closest_x, closest_y), bubble);
}

auto local_omt(const point_abs_om& overmap, const int x, const int y) -> point_abs_omt {
    const auto origin = overmap_origin_omt(overmap);
    return {origin.x() + x, origin.y() + y};
}

auto blob_to_string(const void* data, const int size) -> std::string {
    if (data == nullptr || size <= 0) { return {}; }
    return std::string(static_cast<const char*>(data), static_cast<std::size_t>(size));
}

auto row_data_to_string(sqlite3_stmt* stmt) -> std::string {
    const auto* blob = sqlite3_column_blob(stmt, 1);
    const auto blob_size = sqlite3_column_bytes(stmt, 1);
    const auto* compression_raw = sqlite3_column_text(stmt, 2);
    const auto compression =
        compression_raw == nullptr ? std::string{} : reinterpret_cast<const char*>(compression_raw);
    if (compression.empty()) { return blob_to_string(blob, blob_size); }
    if (compression == "zlib") {
        auto data = std::string{};
        zlib_decompress(blob, blob_size, data);
        return data;
    }
    return {};
}

auto strip_save_version_header(const std::string& data) -> std::string {
    if (!data.starts_with('#')) { return data; }
    const auto json_start = data.find('\n');
    if (json_start == std::string::npos) { return {}; }
    return data.substr(json_start + 1);
}

auto read_text_file(const std::filesystem::path& path) -> std::string {
    auto input = std::ifstream(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error(string_format("Failed to open %s", path.string().c_str()));
    }
    auto stream = std::ostringstream{};
    stream << input.rdbuf();
    return stream.str();
}

auto write_text_file(const std::filesystem::path& path, const std::string& data) -> void {
    auto output = std::ofstream(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error(string_format("Failed to write %s", path.string().c_str()));
    }
    output << data;
}

auto seed_digits_range(const std::string& data) -> std::pair<std::size_t, std::size_t> {
    const auto seed_key = data.find("\"seed\"");
    if (seed_key == std::string::npos) {
        throw std::runtime_error("master.gsav does not contain a seed member");
    }
    const auto colon = data.find(':', seed_key);
    if (colon == std::string::npos) {
        throw std::runtime_error("master.gsav seed member is malformed");
    }
    auto first = colon + 1;
    while (first < data.size() && std::isspace(static_cast<unsigned char>(data[first]))) {
        first++;
    }
    auto last = first;
    while (last < data.size() && std::isdigit(static_cast<unsigned char>(data[last]))) { last++; }
    if (first == last) { throw std::runtime_error("master.gsav seed member is not numeric"); }
    return {first, last};
}

auto read_seed_from_master(const std::string& data) -> unsigned int {
    const auto [first, last] = seed_digits_range(data);
    const auto seed = std::stoull(data.substr(first, last - first));
    if (seed > std::numeric_limits<unsigned int>::max()) {
        throw std::runtime_error("master.gsav seed member is out of range");
    }
    return static_cast<unsigned int>(seed);
}

auto make_replacement_seed(const unsigned int old_seed) -> unsigned int {
    const auto now = static_cast<unsigned int>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    auto new_seed = old_seed ^ now ^ 0x9e3779b9U;
    if (new_seed == old_seed) { new_seed++; }
    return new_seed;
}

auto replace_seed_in_master(std::string& data, const unsigned int new_seed) -> void {
    const auto [first, last] = seed_digits_range(data);
    data.replace(first, last - first, std::to_string(new_seed));
}

auto reroll_world_seed(const std::filesystem::path& world_path)
    -> std::pair<unsigned int, unsigned int> {
    const auto master_path = world_path / "master.gsav";
    auto master = read_text_file(master_path);
    const auto old_seed = read_seed_from_master(master);
    const auto new_seed = make_replacement_seed(old_seed);
    replace_seed_in_master(master, new_seed);
    write_text_file(master_path, master);
    return {old_seed, new_seed};
}

auto read_object_name(JsonIn& json) -> std::string {
    auto name = std::string{};
    json.start_object();
    while (!json.end_object()) {
        const auto member = json.get_member_name();
        if (member == "name") {
            json.read(name);
        } else {
            json.skip_value();
        }
    }
    return name;
}

auto append_unique(std::vector<std::string>& out, const std::string& value) -> void {
    if (value.empty() || std::ranges::contains(out, value)) { return; }
    out.push_back(value);
}

auto read_compacted_terrain_layer(JsonIn& json) -> std::vector<std::string> {
    auto values = std::vector<std::string>(OMAPX * OMAPY, "field");
    auto index = std::size_t{0};
    json.start_array();
    while (!json.end_array()) {
        auto terrain = std::string{};
        auto count = 0;
        json.start_array();
        json.read(terrain);
        json.read(count);
        json.end_array();
        for (auto i = 0; i < count && index < values.size(); i++) {
            values[index] = terrain;
            index++;
        }
    }
    return values;
}

auto write_json_string(std::ostream& stream, const std::string& value) -> void {
    auto json = JsonOut(stream, false);
    json.write(value);
}

auto write_compacted_terrain_layer(std::ostream& stream, const std::vector<std::string>& values)
    -> void {
    auto count = 0;
    auto last_value = std::string{};
    auto have_value = false;
    stream << '[';
    for (const auto& value : values) {
        if (!have_value || value != last_value) {
            if (have_value) { stream << ',' << count << ']'; }
            if (have_value) { stream << ','; }
            stream << '[';
            write_json_string(stream, value);
            last_value = value;
            count = 1;
            have_value = true;
        } else {
            count++;
        }
    }
    if (have_value) { stream << ',' << count << ']'; }
    stream << ']';
}

auto default_terrain_for_layer(const int layer_index) -> std::string {
    if (layer_index < OVERMAP_DEPTH) { return "empty_rock"; }
    if (layer_index > OVERMAP_DEPTH) { return "open_air"; }
    return "field";
}

auto prune_terrain_layer(
    std::vector<std::string>& values, const int layer_index, const point_abs_om& overmap,
    const save_prune_bubble& bubble) -> void {
    auto index = std::size_t{0};
    for (auto y = 0; y < OMAPY; y++) {
        for (auto x = 0; x < OMAPX; x++) {
            if (!is_inside_bubble(local_omt(overmap, x, y), bubble)) {
                values[index] = default_terrain_for_layer(layer_index);
            }
            index++;
        }
    }
}

auto read_compacted_layer(JsonIn& json) -> std::vector<bool> {
    auto values = std::vector<bool>(OMAPX * OMAPY, false);
    auto index = std::size_t{0};
    json.start_array();
    while (!json.end_array()) {
        auto value = false;
        auto count = 0;
        json.start_array();
        json.read(value);
        json.read(count);
        json.end_array();
        for (auto i = 0; i < count && index < values.size(); i++) {
            values[index] = value;
            index++;
        }
    }
    return values;
}

auto write_compacted_layer(JsonOut& json, const std::vector<bool>& values) -> void {
    auto count = 0;
    auto last_value = false;
    auto have_value = false;
    json.start_array();
    for (const auto value : values) {
        if (!have_value || value != last_value) {
            if (have_value) {
                json.write(count);
                json.end_array();
            }
            json.start_array();
            json.write(value);
            last_value = value;
            count = 1;
            have_value = true;
        } else {
            count++;
        }
    }
    if (have_value) {
        json.write(count);
        json.end_array();
    }
    json.end_array();
}

auto prune_visibility_layer(
    std::vector<bool>& values, const point_abs_om& overmap, const save_prune_bubble& bubble)
    -> void {
    auto index = std::size_t{0};
    for (auto y = 0; y < OMAPY; y++) {
        for (auto x = 0; x < OMAPX; x++) {
            if (!is_inside_bubble(local_omt(overmap, x, y), bubble)) { values[index] = false; }
            index++;
        }
    }
}

struct saved_note {
    int z = 0;
    int x = 0;
    int y = 0;
    std::string text;
    bool dangerous = false;
    int danger_radius = 0;
};

struct saved_extra {
    int z = 0;
    int x = 0;
    int y = 0;
    std::string id;
};

struct overmap_terrain_data {
    std::array<std::vector<std::string>, OVERMAP_LAYERS> layers;
    std::string region_id;
    std::vector<std::string> tracked_vehicles;
    std::vector<std::string> npcs;
};

struct visibility_data {
    std::array<std::vector<bool>, OVERMAP_LAYERS> visible;
    std::array<std::vector<bool>, OVERMAP_LAYERS> explored;
    std::array<std::vector<bool>, OVERMAP_LAYERS> path;
    std::vector<saved_note> notes;
    std::vector<saved_extra> extras;
};

auto read_terrain_layers(JsonIn& json) -> std::array<std::vector<std::string>, OVERMAP_LAYERS> {
    auto layers = std::array<std::vector<std::string>, OVERMAP_LAYERS>{};
    json.start_array();
    for (auto& layer : layers) { layer = read_compacted_terrain_layer(json); }
    json.end_array();
    return layers;
}

auto read_visibility_layers(JsonIn& json) -> std::array<std::vector<bool>, OVERMAP_LAYERS> {
    auto layers = std::array<std::vector<bool>, OVERMAP_LAYERS>{};
    json.start_array();
    for (auto& layer : layers) { layer = read_compacted_layer(json); }
    json.end_array();
    return layers;
}

auto read_visibility_data(const std::string& data) -> visibility_data {
    auto result = visibility_data{};
    auto stream = std::istringstream(strip_save_version_header(data));
    auto json = JsonIn(stream);
    json.start_object();
    while (!json.end_object()) {
        const auto member = json.get_member_name();
        if (member == "visible") {
            result.visible = read_visibility_layers(json);
        } else if (member == "explored") {
            result.explored = read_visibility_layers(json);
        } else if (member == "path") {
            result.path = read_visibility_layers(json);
        } else if (member == "notes") {
            json.start_array();
            for (auto z = 0; z < OVERMAP_LAYERS; z++) {
                json.start_array();
                while (!json.end_array()) {
                    auto note = saved_note{};
                    note.z = z;
                    json.start_array();
                    json.read(note.x);
                    json.read(note.y);
                    json.read(note.text);
                    json.read(note.dangerous);
                    json.read(note.danger_radius);
                    json.end_array();
                    result.notes.push_back(note);
                }
            }
            json.end_array();
        } else if (member == "extras") {
            json.start_array();
            for (auto z = 0; z < OVERMAP_LAYERS; z++) {
                json.start_array();
                while (!json.end_array()) {
                    auto extra = saved_extra{};
                    extra.z = z;
                    json.start_array();
                    json.read(extra.x);
                    json.read(extra.y);
                    json.read(extra.id);
                    json.end_array();
                    result.extras.push_back(extra);
                }
            }
            json.end_array();
        } else {
            json.skip_value();
        }
    }
    return result;
}

auto prune_visibility_data(
    visibility_data& data, const point_abs_om& overmap, const save_prune_bubble& bubble) -> void {
    for (auto& layer : data.visible) { prune_visibility_layer(layer, overmap, bubble); }
    for (auto& layer : data.explored) { prune_visibility_layer(layer, overmap, bubble); }
    for (auto& layer : data.path) { prune_visibility_layer(layer, overmap, bubble); }
    std::erase_if(data.notes, [&](const saved_note& note) {
        return !is_inside_bubble(local_omt(overmap, note.x, note.y), bubble);
    });
    std::erase_if(data.extras, [&](const saved_extra& extra) {
        return !is_inside_bubble(local_omt(overmap, extra.x, extra.y), bubble);
    });
}

auto write_visibility_data(const visibility_data& data) -> std::string {
    auto stream = std::ostringstream{};
    stream << "# version 29\n";
    auto json = JsonOut(stream, false);
    json.start_object();
    const auto write_layers =
        [&](const std::string& name, const std::array<std::vector<bool>, OVERMAP_LAYERS>& layers) {
            json.member(name);
            json.start_array();
            for (const auto& layer : layers) { write_compacted_layer(json, layer); }
            json.end_array();
        };
    write_layers("visible", data.visible);
    write_layers("explored", data.explored);
    write_layers("path", data.path);
    json.member("notes");
    json.start_array();
    for (auto z = 0; z < OVERMAP_LAYERS; z++) {
        json.start_array();
        for (const auto& note :
             data.notes | std::views::filter([&](const saved_note& n) { return n.z == z; })) {
            json.start_array();
            json.write(note.x);
            json.write(note.y);
            json.write(note.text);
            json.write(note.dangerous);
            json.write(note.danger_radius);
            json.end_array();
        }
        json.end_array();
    }
    json.end_array();
    json.member("extras");
    json.start_array();
    for (auto z = 0; z < OVERMAP_LAYERS; z++) {
        json.start_array();
        for (const auto& extra :
             data.extras | std::views::filter([&](const saved_extra& e) { return e.z == z; })) {
            json.start_array();
            json.write(extra.x);
            json.write(extra.y);
            json.write(extra.id);
            json.end_array();
        }
        json.end_array();
    }
    json.end_array();
    json.end_object();
    return stream.str();
}

auto collect_vehicle_names_from_map(const std::string& data, std::vector<std::string>& vehicles)
    -> void {
    auto stream = std::istringstream(data);
    auto json = JsonIn(stream);
    json.start_array();
    while (!json.end_array()) {
        json.start_object();
        while (!json.end_object()) {
            const auto member = json.get_member_name();
            if (member == "vehicles") {
                json.start_array();
                while (!json.end_array()) { append_unique(vehicles, read_object_name(json)); }
            } else {
                json.skip_value();
            }
        }
    }
}

auto collect_overmap_vehicles(
    JsonIn& json, const point_abs_om& overmap, const save_prune_bubble& bubble,
    std::vector<std::string>& out) -> void {
    json.start_array();
    while (!json.end_array()) {
        auto name = std::string{};
        auto x = 0;
        auto y = 0;
        json.start_object();
        while (!json.end_object()) {
            const auto member = json.get_member_name();
            if (member == "name") {
                json.read(name);
            } else if (member == "x") {
                json.read(x);
            } else if (member == "y") {
                json.read(y);
            } else {
                json.skip_value();
            }
        }
        if (is_inside_bubble(local_omt(overmap, x, y), bubble)) { append_unique(out, name); }
    }
}

auto read_abs_pos_omt(JsonIn& json) -> point_abs_omt {
    auto x = 0;
    auto y = 0;
    auto z = 0;
    json.start_array();
    json.read(x);
    json.read(y);
    json.read(z);
    json.end_array();
    return project_to<coords::omt>(tripoint_abs_ms(x, y, z)).xy();
}

auto collect_overmap_npcs(
    JsonIn& json, const save_prune_bubble& bubble, std::vector<std::string>& out) -> void {
    json.start_array();
    while (!json.end_array()) {
        auto name = std::string{};
        auto omt = std::optional<point_abs_omt>{};
        json.start_object();
        while (!json.end_object()) {
            const auto member = json.get_member_name();
            if (member == "name") {
                json.read(name);
            } else if (member == "abs_pos") {
                omt = read_abs_pos_omt(json);
            } else {
                json.skip_value();
            }
        }
        if (omt && is_inside_bubble(*omt, bubble)) { append_unique(out, name); }
    }
}

auto collect_names_from_overmap(
    const std::string& data, const point_abs_om& overmap, const save_prune_bubble& bubble,
    save_prune_preview& preview) -> void {
    auto stream = std::istringstream(strip_save_version_header(data));
    auto json = JsonIn(stream);
    json.start_object();
    while (!json.end_object()) {
        const auto member = json.get_member_name();
        if (member == "npcs") {
            collect_overmap_npcs(json, bubble, preview.npcs);
        } else if (member == "tracked_vehicles") {
            collect_overmap_vehicles(json, overmap, bubble, preview.vehicles);
        } else {
            json.skip_value();
        }
    }
}

auto vehicle_object_is_inside_bubble(
    const JsonObject& vehicle, const point_abs_om& overmap, const save_prune_bubble& bubble)
    -> bool {
    if (!vehicle.has_int("x") || !vehicle.has_int("y")) { return false; }
    return is_inside_bubble(local_omt(overmap, vehicle.get_int("x"), vehicle.get_int("y")), bubble);
}

auto npc_object_abs_omt(const JsonObject& npc) -> std::optional<point_abs_omt> {
    if (!npc.has_array("abs_pos")) { return std::nullopt; }
    const auto pos = npc.get_array("abs_pos");
    if (pos.size() < 3) { return std::nullopt; }
    return project_to<coords::omt>(tripoint_abs_ms(pos.get_int(0), pos.get_int(1), pos.get_int(2)))
        .xy();
}

auto npc_object_is_inside_bubble(const JsonObject& npc, const save_prune_bubble& bubble) -> bool {
    const auto omt = npc_object_abs_omt(npc);
    return omt && is_inside_bubble(*omt, bubble);
}

auto read_overmap_terrain_data(
    const std::string& data, const point_abs_om& overmap, const save_prune_bubble& bubble)
    -> overmap_terrain_data {
    auto result = overmap_terrain_data{};
    auto stream = std::istringstream(strip_save_version_header(data));
    auto json = JsonIn(stream);
    json.start_object();
    while (!json.end_object()) {
        const auto member = json.get_member_name();
        if (member == "layers") {
            result.layers = read_terrain_layers(json);
        } else if (member == "region_id") {
            json.read(result.region_id);
        } else if (member == "tracked_vehicles") {
            json.start_array();
            while (!json.end_array()) {
                const auto vehicle = json.get_object();
                if (vehicle_object_is_inside_bubble(vehicle, overmap, bubble)) {
                    result.tracked_vehicles.push_back(vehicle.str());
                }
            }
        } else if (member == "npcs") {
            json.start_array();
            while (!json.end_array()) {
                const auto npc = json.get_object();
                if (npc_object_is_inside_bubble(npc, bubble)) { result.npcs.push_back(npc.str()); }
            }
        } else {
            json.skip_value();
        }
    }
    return result;
}

auto prune_overmap_terrain_data(
    overmap_terrain_data& data, const point_abs_om& overmap, const save_prune_bubble& bubble)
    -> void {
    for (auto layer_index = 0; layer_index < OVERMAP_LAYERS; layer_index++) {
        prune_terrain_layer(data.layers[layer_index], layer_index, overmap, bubble);
    }
}

auto json_value_without_separator(const std::string& raw) -> std::string {
    auto first = raw.find_first_not_of(" \t\r\n,");
    if (first == std::string::npos) { return {}; }
    auto last = raw.find_last_not_of(" \t\r\n,");
    return raw.substr(first, last - first + 1);
}

auto write_raw_json_array(std::ostream& stream, const std::vector<std::string>& values) -> void {
    stream << '[';
    auto first = true;
    for (const auto& value : values) {
        if (!first) { stream << ','; }
        stream << json_value_without_separator(value);
        first = false;
    }
    stream << ']';
}

auto write_pruned_overmap_stash(const overmap_terrain_data& data) -> std::string {
    auto stream = std::ostringstream{};
    stream << "{\"tracked_vehicles\":";
    write_raw_json_array(stream, data.tracked_vehicles);
    stream << ",\"npcs\":";
    write_raw_json_array(stream, data.npcs);
    stream << "}\n";
    return stream.str();
}

auto stash_path_for_overmap_path(const std::string& path) -> std::string {
    return "pruned_overmap/" + path;
}

auto insert_blob_row(sqlite3* db, const std::string& path, const std::string& data) -> void {
    auto insert = sqlite_stmt(
        db, "INSERT OR REPLACE INTO files(path, parent, compression, data) "
            "VALUES(:path, '', NULL, :data)");
    sqlite3_bind_text(insert.get(), sqlite3_bind_parameter_index(insert.get(), ":path"),
                      path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(insert.get(), sqlite3_bind_parameter_index(insert.get(), ":data"),
                      data.data(), static_cast<int>(data.size()), SQLITE_TRANSIENT);
    if (sqlite3_step(insert.get()) != SQLITE_DONE) { throw std::runtime_error(sqlite3_errmsg(db)); }
}

auto collect_safe_bubble_names(
    const std::filesystem::path& db_path, const save_prune_bubble& bubble,
    save_prune_preview& preview) -> void {
    auto db = sqlite_db(db_path);
    auto stmt = sqlite_stmt(db.get(), "SELECT path, data, compression FROM files");
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const auto* path_raw = sqlite3_column_text(stmt.get(), 0);
        if (path_raw == nullptr) { continue; }
        const auto path = std::string(reinterpret_cast<const char*>(path_raw));
        const auto info = classify_save_path(path);
        if (info.kind == save_path_kind::unknown
            || !overmap_intersects_bubble(info.overmap, bubble)) {
            continue;
        }
        try {
            const auto data = row_data_to_string(stmt.get());
            if (info.kind == save_path_kind::map_omt) {
                collect_vehicle_names_from_map(data, preview.vehicles);
            } else if (info.kind == save_path_kind::overmap) {
                collect_names_from_overmap(data, info.overmap, bubble, preview);
            }
        } catch (const JsonError&) { continue; } catch (const std::exception&) {
            continue;
        }
    }
}

auto update_blob_row(
    sqlite3* db, const std::string& path, const std::string& data, const char* compression)
    -> void {
    auto update = sqlite_stmt(
        db, "UPDATE files SET data = :data, compression = :compression WHERE "
            "path = :path");
    sqlite3_bind_blob(update.get(), sqlite3_bind_parameter_index(update.get(), ":data"),
                      data.data(), static_cast<int>(data.size()), SQLITE_TRANSIENT);
    if (compression == nullptr) {
        sqlite3_bind_null(update.get(), sqlite3_bind_parameter_index(update.get(), ":compression"));
    } else {
        sqlite3_bind_text(update.get(), sqlite3_bind_parameter_index(update.get(), ":compression"),
                          compression, -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_text(update.get(), sqlite3_bind_parameter_index(update.get(), ":path"),
                      path.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(update.get()) != SQLITE_DONE) { throw std::runtime_error(sqlite3_errmsg(db)); }
}

auto update_blob_row(
    sqlite3* db, const std::string& path, const std::vector<std::byte>& data,
    const char* compression) -> void {
    auto update = sqlite_stmt(
        db, "UPDATE files SET data = :data, compression = :compression WHERE "
            "path = :path");
    sqlite3_bind_blob(update.get(), sqlite3_bind_parameter_index(update.get(), ":data"),
                      data.data(), static_cast<int>(data.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(update.get(), sqlite3_bind_parameter_index(update.get(), ":compression"),
                      compression, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(update.get(), sqlite3_bind_parameter_index(update.get(), ":path"),
                      path.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(update.get()) != SQLITE_DONE) { throw std::runtime_error(sqlite3_errmsg(db)); }
}

auto update_blob_row_preserving_compression(
    sqlite3* db, const std::string& path, const std::string& data, const std::string& compression)
    -> void {
    if (compression == "zlib") {
        auto compressed = std::vector<std::byte>{};
        zlib_compress(data, compressed);
        update_blob_row(db, path, compressed, "zlib");
        return;
    }
    if (compression.empty()) {
        update_blob_row(db, path, data, nullptr);
        return;
    }
    throw std::runtime_error(
        string_format("Unsupported compression '%s' for %s", compression, path.c_str()));
}

auto rewrite_db_blobs(const std::filesystem::path& db_path, blob_compression_mode mode)
    -> blob_compression_stats {
    auto stats = blob_compression_stats{.databases = 1};
    auto db = sqlite_db(db_path);
    auto tx =
        mode == blob_compression_mode::info
            ? std::optional<transaction>{}
            : std::optional<transaction>{std::in_place, db.get()};
    auto stmt = sqlite_stmt(db.get(), "SELECT path, data, compression FROM files");
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        stats.rows++;
        const auto* path_raw = sqlite3_column_text(stmt.get(), 0);
        const auto* blob = sqlite3_column_blob(stmt.get(), 1);
        const auto blob_size = sqlite3_column_bytes(stmt.get(), 1);
        const auto* compression_raw = sqlite3_column_text(stmt.get(), 2);
        const auto compression =
            compression_raw == nullptr
                ? std::string{}
                : reinterpret_cast<const char*>(compression_raw);
        const auto path =
            path_raw == nullptr ? std::string{} : reinterpret_cast<const char*>(path_raw);
        stats.stored_bytes += static_cast<std::uint64_t>(blob_size);
        if (compression.empty()) {
            stats.uncompressed_rows++;
            stats.raw_bytes += static_cast<std::uint64_t>(blob_size);
            if (mode == blob_compression_mode::compress) {
                const auto data = blob_to_string(blob, blob_size);
                auto compressed = std::vector<std::byte>{};
                zlib_compress(data, compressed);
                update_blob_row(db.get(), path, compressed, "zlib");
                stats.changed_rows++;
            }
        } else if (compression == "zlib") {
            stats.compressed_rows++;
            auto data = std::string{};
            zlib_decompress(blob, blob_size, data);
            stats.raw_bytes += static_cast<std::uint64_t>(data.size());
            if (mode == blob_compression_mode::decompress) {
                update_blob_row(db.get(), path, data, nullptr);
                stats.changed_rows++;
            }
        } else {
            stats.unknown_compression_rows++;
        }
    }
    if (tx) { tx->commit(); }
    return stats;
}

auto merge_stats(blob_compression_stats& lhs, const blob_compression_stats& rhs) -> void {
    lhs.databases += rhs.databases;
    lhs.rows += rhs.rows;
    lhs.compressed_rows += rhs.compressed_rows;
    lhs.uncompressed_rows += rhs.uncompressed_rows;
    lhs.unknown_compression_rows += rhs.unknown_compression_rows;
    lhs.changed_rows += rhs.changed_rows;
    lhs.stored_bytes += rhs.stored_bytes;
    lhs.raw_bytes += rhs.raw_bytes;
}

auto timestamp_suffix() -> std::string {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto tm = std::tm{};
#if defined(_WIN32)
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    auto buffer = std::array<char, 32>{};
    std::strftime(buffer.data(), buffer.size(), "%Y%m%d-%H%M%S", &tm);
    return buffer.data();
}

auto count_or_prune_db_rows(
    const std::filesystem::path& db_path, const save_prune_bubble& bubble, const bool do_prune)
    -> save_prune_preview {
    auto preview = save_prune_preview{};
    preview.databases = 1;
    auto to_prune = std::vector<std::string>{};
    auto db = sqlite_db(db_path);
    auto tx =
        do_prune ? std::optional<transaction>{std::in_place, db.get()}
                 : std::optional<transaction>{};
    auto select = sqlite_stmt(db.get(), "SELECT path, data, compression FROM files");
    while (sqlite3_step(select.get()) == SQLITE_ROW) {
        const auto* path_raw = sqlite3_column_text(select.get(), 0);
        if (path_raw == nullptr) {
            preview.ignored_rows++;
            continue;
        }
        const auto path = std::string(reinterpret_cast<const char*>(path_raw));
        const auto info = classify_save_path(path);
        if (info.kind == save_path_kind::unknown) {
            preview.ignored_rows++;
        } else if (info.kind == save_path_kind::map_omt) {
            if (is_inside_bubble(info.omt, bubble)) {
                preview.kept_rows++;
            } else {
                preview.pruned_rows++;
                to_prune.push_back(path);
            }
        } else if (info.kind == save_path_kind::overmap
                   && overmap_intersects_bubble(info.overmap, bubble)) {
            preview.pruned_rows++;
            if (do_prune) {
                const auto data = row_data_to_string(select.get());
                const auto overmap_data = read_overmap_terrain_data(data, info.overmap, bubble);
                insert_blob_row(db.get(), stash_path_for_overmap_path(path),
                                write_pruned_overmap_stash(overmap_data));
            }
            to_prune.push_back(path);
        } else if (overmap_intersects_bubble(info.overmap, bubble)) {
            preview.kept_rows++;
            if (do_prune && info.kind == save_path_kind::overmap_seen) {
                const auto* compression_raw = sqlite3_column_text(select.get(), 2);
                const auto compression =
                    compression_raw == nullptr
                        ? std::string{}
                        : reinterpret_cast<const char*>(compression_raw);
                const auto data = row_data_to_string(select.get());
                auto visibility = read_visibility_data(data);
                prune_visibility_data(visibility, info.overmap, bubble);
                update_blob_row_preserving_compression(
                    db.get(), path, write_visibility_data(visibility), compression);
            }
        } else {
            preview.pruned_rows++;
            to_prune.push_back(path);
        }
    }
    if (do_prune && !to_prune.empty()) {
        auto del = sqlite_stmt(db.get(), "DELETE FROM files WHERE path = :path");
        for (const auto& path : to_prune) {
            sqlite3_reset(del.get());
            sqlite3_clear_bindings(del.get());
            sqlite3_bind_text(del.get(), sqlite3_bind_parameter_index(del.get(), ":path"),
                              path.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(del.get()) != SQLITE_DONE) {
                throw std::runtime_error(sqlite3_errmsg(db.get()));
            }
        }
    }
    if (tx) { tx->commit(); }
    return preview;
}

auto merge_prune_preview(save_prune_preview& lhs, const save_prune_preview& rhs) -> void {
    lhs.databases += rhs.databases;
    lhs.kept_rows += rhs.kept_rows;
    lhs.pruned_rows += rhs.pruned_rows;
    lhs.ignored_rows += rhs.ignored_rows;
    for (const auto& vehicle : rhs.vehicles) { append_unique(lhs.vehicles, vehicle); }
    for (const auto& npc : rhs.npcs) { append_unique(lhs.npcs, npc); }
}

} // namespace

auto rewrite_world_blobs(const std::filesystem::path& world_path, const blob_compression_mode mode)
    -> blob_compression_stats {
    auto total = blob_compression_stats{};
    for (const auto& db_path : sqlite_files(world_path)) {
        merge_stats(total, rewrite_db_blobs(db_path, mode));
    }
    return total;
}

auto preview_prune_world_outside_bubble(
    const std::filesystem::path& world_path, const save_prune_bubble& bubble)
    -> save_prune_preview {
    auto total = save_prune_preview{};
    for (const auto& db_path : sqlite_files(world_path)) {
        auto db_preview = count_or_prune_db_rows(db_path, bubble, false);
        collect_safe_bubble_names(db_path, bubble, db_preview);
        merge_prune_preview(total, db_preview);
    }
    std::ranges::sort(total.vehicles);
    std::ranges::sort(total.npcs);
    return total;
}

auto make_world_backup(const std::filesystem::path& world_path) -> std::filesystem::path {
    if (!std::filesystem::exists(world_path)) {
        throw std::runtime_error(
            string_format("World path does not exist: %s", world_path.string().c_str()));
    }
    auto backup_path = world_path;
    backup_path += " (Pre-prune Backup ";
    backup_path += timestamp_suffix();
    backup_path += ")";
    auto suffix = 1;
    while (std::filesystem::exists(backup_path)) {
        backup_path = world_path;
        backup_path +=
            string_format(" (Pre-prune Backup %s-%d)", timestamp_suffix().c_str(), suffix);
        suffix++;
    }
    std::filesystem::copy(
        world_path, backup_path,
        std::filesystem::copy_options::recursive | std::filesystem::copy_options::copy_symlinks);
    return backup_path;
}

auto prune_world_outside_bubble(
    const std::filesystem::path& world_path, const save_prune_bubble& bubble) -> save_prune_result {
    auto result = save_prune_result{};
    result.preview = preview_prune_world_outside_bubble(world_path, bubble);
    result.backup_path = make_world_backup(world_path);
    const auto [old_seed, new_seed] = reroll_world_seed(world_path);
    result.old_seed = old_seed;
    result.new_seed = new_seed;
    auto pruned = save_prune_preview{};
    for (const auto& db_path : sqlite_files(world_path)) {
        merge_prune_preview(pruned, count_or_prune_db_rows(db_path, bubble, true));
    }
    result.preview.kept_rows = pruned.kept_rows;
    result.preview.pruned_rows = pruned.pruned_rows;
    result.preview.ignored_rows = pruned.ignored_rows;
    result.preview.databases = pruned.databases;
    return result;
}

} // namespace save_tools
