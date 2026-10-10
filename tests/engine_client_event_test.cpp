#include "catch/catch.hpp"
#include "engine_client_event.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <numeric>
#include <string>
#include <utility>

namespace {
using namespace engine_client;

auto menu(const std::string& id = "boundary:1") -> boundary_state {
    return {.id = id, .ready = {.phase = "menu"}};
}
auto dirt() -> look { return {.kind = "terrain", .id = "t_dirt", .glyph = ".", .color = "brown"}; }
auto zombie() -> look {
    return {.kind = "monster", .id = "mon_zombie", .glyph = "Z", .color = "green"};
}
auto pos(const int x, const int y = 0) -> position { return {.dim = "", .x = x, .y = y, .z = 0}; }
auto area(const int low, const int high) -> bounds {
    return {.min = pos(low, -5), .max = pos(high, 5)};
}
auto visible_cell(const int x) -> cell {
    return {.at = pos(x), .known = knowledge::visible, .terrain = dirt()};
}
auto remembered_cell(const int x) -> cell {
    return {
        .at = pos(x), .known = knowledge::remembered, .memory = memory_layers{.terrain = dirt()}};
}
auto monster(const std::string& id, const int x) -> entity {
    return {.id = id, .at = pos(x), .known = knowledge::visible, .appearance = zombie()};
}
auto world_of(const std::vector<cell>& cells, const std::vector<entity>& entities = {})
    -> state_value {
    auto result = state_value{.interaction = menu()};
    result.world.coverage = area(0, 10);
    for (const auto& entry : cells) { result.world.cells[entry.at] = entry; }
    for (const auto& entry : entities) { result.world.entities[entry.id] = entry; }
    return result;
}
auto any_visible(const state_value& value) -> bool {
    return std::ranges::any_of(
               value.world.cells,
               [](const auto& entry) { return entry.second.known == knowledge::visible; })
        || std::ranges::any_of(value.world.entities, [](const auto& entry) {
               return entry.second.known == knowledge::visible;
           });
}
auto stream_of(const state_value& initial, const std::string& epoch = "epoch:a") -> event_stream {
    auto created = event_stream::create(epoch, initial);
    REQUIRE(created);
    return std::move(*created);
}
auto publish(event_stream& stream, state_value next) -> public_event {
    const auto published = stream.publish({.next = std::move(next)});
    REQUIRE(published);
    REQUIRE(*published);
    return **published;
}
} // namespace

TEST_CASE("diff then apply reconstructs the target state", "[engine_client_event]") {
    auto from = world_of({visible_cell(1), visible_cell(2), visible_cell(3)}, {monster("e:1", 2)});
    auto to = world_of({visible_cell(1), remembered_cell(2), visible_cell(4)}, {monster("e:2", 4)});
    to.world.avatar = avatar_value{.id = "e:avatar", .at = pos(1), .name = "Ada"};
    to.world.environment = environment_value{.turn = "5", .time = "t", .weather = "clear"};
    to.interaction = menu("boundary:2");

    const auto delta = diff(from, to);
    CHECK(delta.cells.size() == 2); // 2 became remembered, 4 appeared; 1 is unchanged
    CHECK(delta.forgotten == std::vector{pos(3)});
    REQUIRE(delta.gone.size() == 1);
    CHECK(delta.gone.front().id == "e:1");
    CHECK(delta.gone.front().reason == "lost_sight");
    CHECK(classify(delta) == "cells.seen");
    auto rebuilt = from;
    REQUIRE(apply(rebuilt, delta));
    CHECK(same_state(rebuilt, to));
    CHECK(diff(to, to).empty());
}

TEST_CASE("coverage shrink drops outside facts without listing each one", "[engine_client_event]") {
    auto from = world_of({visible_cell(1), visible_cell(9)}, {monster("e:1", 9)});
    auto to = from;
    to.world.coverage = area(0, 5);
    to.world.cells.erase(pos(9));
    to.world.entities.clear();
    const auto delta = diff(from, to);
    CHECK(delta.coverage);
    CHECK(delta.forgotten.empty());
    CHECK(delta.gone.empty());
    CHECK(classify(delta) == "coverage.moved");
    auto rebuilt = from;
    REQUIRE(apply(rebuilt, delta));
    CHECK(same_state(rebuilt, to));
}

TEST_CASE("sight loss leaves no visible fact behind", "[engine_client_event]") {
    auto before = world_of({visible_cell(1), visible_cell(2)}, {monster("e:1", 1)});
    auto after = before;
    after.world.cells[pos(1)] = remembered_cell(1); // memorized
    after.world.cells.erase(pos(2));                // never memorized
    after.world.entities.clear();                   // out of sight
    auto stream = stream_of(before);
    auto receiver = stream.current();
    const auto event = publish(stream, after);
    REQUIRE(apply_batch(receiver, {.epoch = "epoch:a", .events = {event}}));
    CHECK_FALSE(any_visible(receiver.value));
    CHECK(receiver.value.world.cells.size() == 1);
    CHECK(receiver.value.world.cells.at(pos(1)).known == knowledge::remembered);
}

TEST_CASE("stream numbering and receiver continuity", "[engine_client_event]") {
    auto stream = stream_of(world_of({visible_cell(1)}));
    auto receiver = stream.current();
    const auto first = publish(stream, world_of({visible_cell(1), visible_cell(2)}));
    const auto second = publish(stream, world_of({visible_cell(2)}));
    CHECK(first.value().sequence == 1);
    CHECK(first.value().revision == 1);
    CHECK(second.value().sequence == 2);
    CHECK(second.value().revision == 2);
    CHECK(stream.current().at == clock_point{"epoch:a", 2, 2});

    SECTION("in order") {
        REQUIRE(apply_batch(receiver, {.epoch = "epoch:a", .events = {first, second}}));
        CHECK(receiver.at == stream.current().at);
        CHECK(same_state(receiver.value, stream.current().value));
    }
    SECTION("duplicate") {
        REQUIRE(apply_event(receiver, "epoch:a", first));
        const auto duplicate = apply_event(receiver, "epoch:a", first);
        REQUIRE_FALSE(duplicate);
        CHECK(duplicate.error() == error::resync_required);
        CHECK(receiver.at.sequence == 1);
    }
    SECTION("gap") {
        CHECK_FALSE(apply_event(receiver, "epoch:a", second));
        CHECK(receiver.at.sequence == 0);
    }
    SECTION("epoch switch") {
        CHECK_FALSE(apply_event(receiver, "epoch:b", first));
        CHECK(receiver.at.sequence == 0);
    }
    SECTION("invalid suffix applies no prefix") {
        const auto before = receiver;
        REQUIRE_FALSE(apply_batch(receiver, {.epoch = "epoch:a", .events = {first, first}}));
        CHECK(receiver.at == before.at);
        CHECK(same_state(receiver.value, before.value));
    }
    SECTION("changes that do not fit leave the receiver unchanged") {
        auto bad = second.value();
        bad.delta.forgotten = {pos(7)}; // never known
        REQUIRE(apply_event(receiver, "epoch:a", first));
        const auto before = receiver;
        CHECK_FALSE(apply_event(receiver, "epoch:a", public_event{bad}));
        CHECK(receiver.at == before.at);
        CHECK(same_state(receiver.value, before.value));
    }
}

TEST_CASE("withheld and unchanged candidates consume nothing", "[engine_client_event]") {
    auto stream = stream_of(world_of({visible_cell(1)}));
    const auto withheld = stream.publish(
        {.decision = disclosure::withheld, .next = world_of({visible_cell(1), visible_cell(2)})});
    REQUIRE(withheld);
    CHECK_FALSE(*withheld);
    CHECK(stream.current().at == clock_point{"epoch:a", 0, 0});
    const auto unchanged = stream.publish({.next = world_of({visible_cell(1)})});
    REQUIRE(unchanged);
    CHECK_FALSE(*unchanged);
    CHECK(stream.current().at.sequence == 0);
    const auto next = publish(stream, world_of({visible_cell(1), visible_cell(2)}));
    CHECK(next.value().sequence == 1);
}

TEST_CASE("cause and command identify earlier published work", "[engine_client_event]") {
    auto stream = stream_of(world_of({visible_cell(1)}));
    const auto future = stream.publish({.next = world_of({}), .cause = 5});
    REQUIRE_FALSE(future);
    CHECK(future.error() == error::validation_failed);
    CHECK(stream.current().at.sequence == 0);
    const auto first = publish(stream, world_of({visible_cell(1), visible_cell(2)}));
    const auto caused = stream.publish(
        {.next = world_of({visible_cell(2)}), .command = "c:1", .cause = first.value().sequence});
    REQUIRE(caused);
    REQUIRE(*caused);
    CHECK((*caused)->value().cause == 1);
    CHECK((*caused)->value().command == "c:1");
}

TEST_CASE("rebase adopts a state and advances only the revision", "[engine_client_event]") {
    auto stream = stream_of(world_of({visible_cell(1)}));
    publish(stream, world_of({visible_cell(2)}));
    REQUIRE(stream.rebase(world_of({visible_cell(3)})));
    CHECK(stream.current().at == clock_point{"epoch:a", 1, 2});
    CHECK(stream.current().value.world.cells.contains(pos(3)));
}

TEST_CASE("events serialize with ids as decimal strings", "[engine_client_event]") {
    auto stream = stream_of(world_of({visible_cell(1)}));
    const auto event = publish(stream, world_of({visible_cell(1), remembered_cell(2)}));
    const auto wire = serialize_events({.epoch = "epoch:a", .events = {event}});
    CHECK(wire.find(R"("sequence":"1","revision":"1","type":"cells.seen")") != std::string::npos);
    CHECK(wire.find(R"("known":"remembered")") != std::string::npos);
    CHECK(wire.find(R"("epoch":"epoch:a")") != std::string::npos);
}

TEST_CASE(
    "snapshot parts cover every cell exactly once within the byte bound", "[engine_client_event]") {
    auto value = world_of({});
    value.world.coverage = bounds{.min = pos(0, -5), .max = pos(1000, 5)};
    // Cells with heavy item lists force byte-based splitting well before the cell-count cap.
    for (auto x = 0; x < 300; ++x) {
        auto heavy = visible_cell(x);
        heavy.items = std::vector<
            look>(200, look{.kind = "item", .id = "rock", .glyph = "*", .color = "gray"});
        value.world.cells[pos(x)] = heavy;
    }
    const auto stream = stream_of(value);
    const auto wire = serialize_snapshot(stream.current());
    REQUIRE(wire);
    CHECK(wire->parts.size() > 2);
    CHECK(
        wire->header.find("\"parts\":" + std::to_string(wire->parts.size())) != std::string::npos);
    auto seen = std::vector<int>{};
    for (auto index = std::size_t{0}; index < wire->parts.size(); ++index) {
        const auto& part = wire->parts[index];
        CHECK(part.size() <= maximum_inline_bytes);
        const auto parsed = nlohmann::json::parse(part);
        CHECK(parsed["index"] == index);
        CHECK(parsed["last"] == (index + 1 == wire->parts.size()));
        CHECK_FALSE(parsed["cells"].empty());
        for (const auto& cell : parsed["cells"]) { seen.push_back(cell["at"]["x"]); }
    }
    auto expected = std::vector<int>(300);
    std::iota(expected.begin(), expected.end(), 0);
    CHECK(seen == expected);
}

TEST_CASE("a snapshot without cells has no parts", "[engine_client_event]") {
    const auto wire = serialize_snapshot(stream_of(world_of({})).current());
    REQUIRE(wire);
    CHECK(wire->parts.empty());
    CHECK(wire->header.find("\"parts\":0") != std::string::npos);
}

TEST_CASE(
    "a value that cannot fit one frame is a recoverable resource limit", "[engine_client_event]") {
    auto huge = visible_cell(1);
    huge.items =
        std::vector<look>(20000, look{.kind = "item", .id = "rock", .glyph = "*", .color = "gray"});
    const auto big_cell = serialize_snapshot(stream_of(world_of({huge})).current());
    REQUIRE_FALSE(big_cell);
    CHECK(big_cell.error() == error::resource_limit);
    auto crowded = world_of({});
    crowded.world.avatar = avatar_value{.id = "e:avatar", .at = pos(1), .name = "Ada"};
    crowded.world.avatar->inventory = std::vector<inventory_entry>(
        6000, inventory_entry{.appearance = zombie(), .name = std::string(40, 'n')});
    const auto big_header = serialize_snapshot(stream_of(crowded).current());
    REQUIRE_FALSE(big_header);
    CHECK(big_header.error() == error::resource_limit);
}

TEST_CASE(
    "a capture that drops coverage, avatar or environment forces a resync",
    "[engine_client_event]") {
    auto full = world_of({visible_cell(1)});
    full.world.avatar = avatar_value{.id = "e:avatar", .at = pos(1), .name = "Ada"};
    full.world.environment = environment_value{.turn = "1", .time = "t", .weather = "clear"};
    for (const auto what : {0, 1, 2}) {
        CAPTURE(what);
        auto stream = stream_of(full);
        auto next = full;
        next.interaction = menu("boundary:2");
        if (what == 0) {
            next.world.coverage.reset();
            next.world.cells.clear();
        }
        if (what == 1) { next.world.avatar.reset(); }
        if (what == 2) { next.world.environment.reset(); }
        const auto before = stream.current().at;
        const auto published = stream.publish({.next = next});
        REQUIRE_FALSE(published);
        CHECK(published.error() == error::resync_required);
        CHECK(stream.current().at == before);
        REQUIRE(stream.rebase(next));
        CHECK(stream.current().at == clock_point{"epoch:a", 0, 1});
        CHECK(same_state(stream.current().value, next));
    }
}

TEST_CASE("knowledge limits what a value may disclose", "[engine_client_event]") {
    const auto rejects = [](const state_value& value) {
        const auto created = event_stream::create("epoch:a", value);
        REQUIRE_FALSE(created);
        CHECK(created.error() == error::validation_failed);
    };
    auto remembered_with_terrain = remembered_cell(1);
    remembered_with_terrain.terrain = dirt();
    rejects(world_of({remembered_with_terrain}));
    auto remembered_without_memory = remembered_cell(1);
    remembered_without_memory.memory.reset();
    rejects(world_of({remembered_without_memory}));
    auto sensed_cell = cell{.at = pos(1), .known = knowledge::sensed, .terrain = dirt()};
    rejects(world_of({sensed_cell}));
    auto sensed_looks = monster("e:1", 1);
    sensed_looks.known = knowledge::sensed;
    rejects(world_of({}, {sensed_looks}));
    auto visible_with_sense = monster("e:1", 1);
    visible_with_sense.sense = "sound";
    rejects(world_of({}, {visible_with_sense}));

    // A bad capture later in the stream publishes nothing and consumes nothing.
    auto stream = stream_of(world_of({visible_cell(1)}));
    const auto leak = stream.publish({.next = world_of({remembered_with_terrain})});
    REQUIRE_FALSE(leak);
    CHECK(leak.error() == error::validation_failed);
    CHECK(stream.current().at == clock_point{"epoch:a", 0, 0});
    auto sensed_entity =
        entity{.id = "e:2", .at = pos(2), .known = knowledge::sensed, .sense = "sound"};
    CHECK(stream.publish({.next = world_of({visible_cell(1)}, {sensed_entity})}));
}
