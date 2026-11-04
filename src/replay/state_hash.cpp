#include "replay/state_hash.h"

#include "avatar.h"
#include "calendar.h"
#include "game.h"
#include "json.h"
#include "map/field.h"
#include "map/map.h"
#include "monster.h"
#include "npc.h"
#include "rng.h"

#include <format>
#include <fstream>
#include <functional>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace replay {
namespace {

constexpr auto state_hash_version = 1;
/// Squares around the avatar, on its z-level, whose terrain, furniture, fields and items are
/// hashed.
constexpr auto map_radius = std::size_t{30};

struct state_log {
    std::ofstream stream;
    std::size_t step = 0;
};

auto log_state() -> state_log& { // *NOPAD*
    static auto value = state_log{};
    return value;
}

auto digest_of(const std::string_view text) -> std::string {
    return std::format("{:016x}", std::hash<std::string_view>{}(text));
}

auto serialized(const auto& value) -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut(stream);
    value.serialize(json);
    return stream.str();
}

auto rng_text() -> std::string {
    auto stream = std::ostringstream{};
    stream.imbue(std::locale::classic());
    stream << rng_get_engine();
    return stream.str();
}

auto monsters_text() -> std::string {
    auto text = std::string{};
    for (const auto& monster : g->all_monsters()) { text += serialized(monster); }
    return text;
}

auto npcs_text() -> std::string {
    auto text = std::string{};
    for (const auto& guy : g->all_npcs()) { text += serialized(guy); }
    return text;
}

auto map_text() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut(stream);
    auto& here = get_map();
    for (const auto& pos : here.points_in_radius(get_avatar().bub_pos(), map_radius)) {
        json.write(pos);
        json.write(here.ter(pos).id().str());
        json.write(here.furn(pos).id().str());
        for (const auto& [id, entry] : here.field_at(pos)) {
            json.write(id.id().str());
            json.write(entry.get_field_intensity());
        }
        for (const auto& it : here.i_at(pos)) { it->serialize(json); }
    }
    return stream.str();
}

} // namespace

auto hash_game_state() -> state_hash {
    return {
        .turn = to_turn<int>(calendar::turn),
        .parts =
            {
                {.name = "rng", .digest = digest_of(rng_text())},
                {.name = "avatar", .digest = digest_of(serialized(get_avatar()))},
                {.name = "monsters", .digest = digest_of(monsters_text())},
                {.name = "npcs", .digest = digest_of(npcs_text())},
                {.name = "map", .digest = digest_of(map_text())},
            },
    };
}

auto format_state_hash(const std::size_t step, const state_hash& hash) -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut(stream);
    json.start_object();
    json.member("kind", "state");
    json.member("step", step);
    json.member("turn", hash.turn);
    json.member("parts");
    json.start_object();
    for (const auto& part : hash.parts) { json.member(part.name, part.digest); }
    json.end_object();
    json.end_object();
    return stream.str();
}

auto configure_state_hash_log(const std::string& path) -> void {
    auto& log = log_state();
    if (log.stream.is_open()) { throw std::runtime_error("State hash log is already configured"); }
    log.stream.open(path, std::ios::out | std::ios::binary | std::ios::noreplace);
    if (!log.stream) { throw std::runtime_error("Could not create new state hash log: " + path); }
    auto json = JsonOut(log.stream);
    json.start_object();
    json.member("kind", "header");
    json.member("format", "cataclysm-bn-state-hash");
    json.member("version", state_hash_version);
    json.end_object();
    log.stream << '\n' << std::flush;
    if (!log.stream) { throw std::runtime_error("Could not write state hash log: " + path); }
}

auto log_state_hash() -> void {
    auto& log = log_state();
    if (!log.stream.is_open()) { return; }
    log.stream << format_state_hash(++log.step, hash_game_state()) << '\n' << std::flush;
}

auto stop_state_hash_log() -> void { log_state() = state_log{}; }

} // namespace replay
