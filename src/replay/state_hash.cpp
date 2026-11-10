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

auto configure_state_hash_log(const std::string& path) -> void {
    auto& log = log_state();
    if (log.stream.is_open()) { throw std::runtime_error("State hash log is already configured"); }
    log.stream.open(path, std::ios::out | std::ios::binary | std::ios::noreplace);
    if (!log.stream) { throw std::runtime_error("Could not create new state hash log: " + path); }
    auto json = JsonOut(log.stream);
    json.start_object();
    json.member("kind", "header");
    json.member("format", "cataclysm-bn-state-hash");
    json.end_object();
    log.stream << '\n' << std::flush;
    if (!log.stream) { throw std::runtime_error("Could not write state hash log: " + path); }
}

auto log_state_hash() -> void {
    auto& log = log_state();
    if (!log.stream.is_open()) { return; }
    auto json = JsonOut(log.stream);
    json.start_object();
    json.member("kind", "state");
    json.member("step", ++log.step);
    json.member("turn", to_turn<int>(calendar::turn));
    json.member("parts");
    json.start_object();
    json.member("rng", digest_of(rng_text()));
    json.member("avatar", digest_of(serialized(get_avatar())));
    json.member("monsters", digest_of(monsters_text()));
    json.member("npcs", digest_of(npcs_text()));
    json.member("map", digest_of(map_text()));
    json.end_object();
    json.end_object();
    log.stream << '\n' << std::flush;
}

auto stop_state_hash_log() -> void { log_state() = state_log{}; }

} // namespace replay
