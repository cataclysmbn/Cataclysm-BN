#include "replay/row_locator.h"

#include "avatar.h"
#include "diary.h"
#include "game.h"
#include "item.h"
#include "item_serial.h"
#include "map/map.h"
#include "map/map_selector.h"
#include "npc.h"
#include "page_serial.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_selector.h"
#include "visitable.h"

#include <algorithm>
#include <charconv>
#include <functional>
#include <numeric>
#include <optional>
#include <ranges>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace replay {
namespace {

/// A place that holds items, written `u` (the avatar), `n<x>,<y>,<z>` (the npc on a tile),
/// `t<x>,<y>,<z>` (the items on a tile) or `v<x>,<y>,<z>` (the cargo of the vehicle on a tile),
/// then `:<n>`, the place of the item among everything the holder visits, containers' contents
/// included.
struct holder {
    char kind = 'u';
    std::vector<int> numbers;
};

auto number(const std::string_view text) -> std::optional<int> {
    auto result = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
    return error == std::errc{} && end == text.data() + text.size()
             ? std::optional(result)
             : std::nullopt;
}

auto holder_text(const holder& place) -> std::string {
    auto text = std::string(1, place.kind);
    for (const auto [index, value] : place.numbers | std::views::enumerate) {
        text += (index == 0 ? "" : ",") + std::to_string(value);
    }
    return text;
}

auto parse_holder(const std::string_view text) -> std::optional<holder> {
    if (text.empty()) { return std::nullopt; }
    auto result = holder{.kind = text.front()};
    for (const auto part : std::views::split(text.substr(1), ',')) {
        if (const auto value = number(std::string_view(part))) {
            result.numbers.push_back(*value);
        } else {
            return std::nullopt;
        }
    }
    return result;
}

using visit_t = std::function<auto(const item*)->VisitResponse>;

/// Walks what a holder holds in a fixed order; false when the holder is not there.
auto visit_holder(const holder& place, const visit_t& visit) -> bool {
    switch (place.kind) {
        case 'u':
            std::as_const(get_avatar()).visit_items(visit);
            return true;
        case 'n': {
            if (place.numbers.size() != 3) { return false; }
            auto* const who = g->critter_at<npc>(
                tripoint_abs_ms(place.numbers[0], place.numbers[1], place.numbers[2]));
            if (who == nullptr) { return false; }
            std::as_const(*who).visit_items(visit);
            return true;
        }
        case 't': {
            if (place.numbers.size() != 3) { return false; }
            const auto tile = map_cursor(
                tripoint_abs_ms(place.numbers[0], place.numbers[1], place.numbers[2]));
            tile.visit_items(visit);
            return true;
        }
        case 'v': {
            if (place.numbers.size() != 3) { return false; }
            const auto at = get_map().veh_at(
                tripoint_abs_ms(place.numbers[0], place.numbers[1], place.numbers[2]));
            if (!at) { return false; }
            auto& vehicle = at->vehicle();
            for (const auto part : std::views::iota(0, vehicle.part_count())) {
                const auto cargo = vehicle_cursor(vehicle, part);
                if (cargo.visit_items(visit) == VisitResponse::ABORT) { break; }
            }
            return true;
        }
        default:
            return false;
    }
}

/// Where `wanted` is among what `place` holds.
auto ordinal_in(const holder& place, const item& wanted) -> std::optional<int> {
    auto count = 0;
    auto found = std::optional<int>{};
    visit_holder(place, [&](const item* node) {
        if (node == &wanted) {
            found = count;
            return VisitResponse::ABORT;
        }
        ++count;
        return VisitResponse::NEXT;
    });
    return found;
}

auto item_at(const holder& place, const int ordinal) -> const item* {
    auto count = 0;
    const item* found = nullptr;
    visit_holder(place, [&](const item* node) {
        if (count++ != ordinal) { return VisitResponse::NEXT; }
        found = node;
        return VisitResponse::ABORT;
    });
    return found;
}

auto holders_of(const item& thing) -> std::vector<holder> {
    auto result = std::vector<holder>{holder{.kind = 'u', .numbers = {}}};
    const auto* root = &thing;
    while (root->parent_item() != nullptr) { root = root->parent_item(); }
    const auto where = root->where();
    if (where == item_location_type::character) {
        if (const auto* const who = g->critter_at<npc>(root->abs_pos())) {
            const auto at = who->abs_pos();
            result.push_back({.kind = 'n', .numbers = {at.x(), at.y(), at.z()}});
        }
    }
    if (where == item_location_type::map || where == item_location_type::vehicle) {
        const auto at = root->abs_pos();
        result.push_back(
            {.kind = where == item_location_type::map ? 't' : 'v',
             .numbers = {at.x(), at.y(), at.z()}});
    }
    return result;
}

/// `<holder>:<n>`, the place of one item in the simulation.
auto locate_item(const item& thing) -> std::optional<std::string> {
    for (const auto& place : holders_of(thing)) {
        if (const auto ordinal = ordinal_in(place, thing)) {
            return holder_text(place) + ":" + std::to_string(*ordinal);
        }
    }
    return std::nullopt;
}

auto resolve_item(const std::string_view where) -> const item* {
    const auto colon = where.rfind(':');
    if (colon == std::string_view::npos) { return nullptr; }
    const auto place = parse_holder(where.substr(0, colon));
    const auto ordinal = number(where.substr(colon + 1));
    return place && ordinal ? item_at(*place, *ordinal) : nullptr;
}

auto locate_page(const std::uint64_t serial) -> std::optional<std::string> {
    const auto* const book = get_avatar().get_avatar_diary();
    if (book == nullptr) { return std::nullopt; }
    for (const auto index : std::views::iota(size_t{0}, book->page_count())) {
        if (game_client::find_diary_page_serial(*book->page_at(index)) == serial) {
            return std::to_string(index);
        }
    }
    return std::nullopt;
}

auto resolve_page(const std::string_view where) -> std::optional<std::string> {
    const auto* const book = get_avatar().get_avatar_diary();
    const auto index = number(where);
    const auto* const page =
        book != nullptr && index && *index >= 0
            ? book->page_at(static_cast<size_t>(*index))
            : nullptr;
    return page != nullptr ? std::optional(game_client::diary_page_row_id(*page)) : std::nullopt;
}

auto join(const std::vector<std::string>& parts) -> std::string {
    return std::accumulate(
        parts.begin() + 1, parts.end(), parts.front(),
        [](const std::string& so_far, const std::string& part) { return so_far + "+" + part; });
}

/// The locator of a row id of the session, or nothing when it names nothing alive.
auto locate_id(const std::string& kind, const std::uint64_t serial) -> std::optional<std::string> {
    if (kind == "page") {
        return locate_page(serial).transform([](const auto& at) { return "page:@d" + at; });
    }
    auto members =
        kind == "stack"
            ? game_client::stack_member_serials(serial)
            : std::vector<std::uint64_t>{serial};
    auto places = std::vector<std::string>{};
    for (const auto member : members) {
        const auto* const thing = game_client::item_with_serial(member);
        if (const auto at = thing != nullptr ? locate_item(*thing) : std::nullopt) {
            places.push_back(*at);
        }
    }
    if (places.empty()) { return std::nullopt; }
    return (places.size() == 1 && kind == "item" ? "item:@" : "stack:@") + join(places);
}

/// The row id of this session for a locator (the part after `@`).
auto resolve_locator(const std::string& kind, const std::string& where)
    -> std::optional<std::string> {
    if (kind == "page") {
        return where.starts_with("d")
                 ? resolve_page(std::string_view(where).substr(1))
                 : std::nullopt;
    }
    auto members = std::vector<const item*>{};
    for (const auto part : std::views::split(where, '+')) {
        if (const auto* const thing = resolve_item(std::string_view(part))) {
            members.push_back(thing);
        }
    }
    if (members.empty()) { return std::nullopt; }
    return game_client::stack_row_id(members);
}

auto rewrite_ids(const std::string& text, const bool locating) -> std::string {
    static const auto serial_id = std::regex(R"(\b(item|stack|page):([0-9]+)\b)");
    static const auto locator_id = std::regex(R"(\b(item|stack|page):@([^\s"]+))");
    auto result = std::string{};
    auto rest = text;
    const auto& pattern = locating ? serial_id : locator_id;
    for (auto match = std::smatch{}; std::regex_search(rest, match, pattern);
         rest = match.suffix().str()) {
        result += match.prefix().str();
        const auto replaced =
            locating ? locate_id(match[1].str(), std::stoull(match[2].str()))
                     : resolve_locator(match[1].str(), match[2].str());
        result += replaced.value_or(match[0].str());
    }
    return result + rest;
}

auto rewrite_event(game_client::interaction_event event, const bool locating)
    -> game_client::interaction_event {
    const auto rewrite = [&](std::string& text) { text = rewrite_ids(text, locating); };
    rewrite(event.target_id);
    if (event.operation != game_client::interaction_operation::fill) { rewrite(event.value); }
    if (event.arguments) {
        if (event.arguments->row) { rewrite(*event.arguments->row); }
        if (event.arguments->rows) {
            for (auto& row : *event.arguments->rows) { rewrite(row.id); }
        }
    }
    return event;
}

} // namespace

auto locate_row_ids(game_client::interaction_event event) -> game_client::interaction_event {
    return rewrite_event(std::move(event), true);
}

auto resolve_row_ids(game_client::interaction_event event) -> game_client::interaction_event {
    return rewrite_event(std::move(event), false);
}

} // namespace replay
