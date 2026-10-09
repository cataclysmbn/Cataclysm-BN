#include "catch/catch.hpp"
#include "engine_client_delivery.h"

#include <chrono>
#include <future>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace {
using namespace engine_client;
using namespace std::chrono_literals;

auto scope() -> delivery_scope { return {.epoch = "epoch:test", .stream = 7}; }
auto at(std::chrono::milliseconds value = 0ms) -> delivery_time { return delivery_time{} + value; }
auto options() -> delivery_options {
    return {
        .scope = scope(),
        .batch_events = 2,
        .batch_bytes = 10,
        .queue_events = 3,
        .queue_bytes = 20,
        .history_events = 4,
        .history_bytes = 30};
}
auto make_queue(delivery_options opts = options()) -> delivery_queue {
    auto queue = delivery_queue::create(std::move(opts));
    REQUIRE(queue);
    return std::move(*queue);
}
auto packet(std::uint64_t sequence, std::size_t bytes = 5, delivery_scope identity = scope())
    -> delivery_packet {
    auto prepared = encoded_delivery_packet::prepare(
        {.scope = std::move(identity),
         .clock = {.sequence = sequence, .revision = sequence},
         .encoded = "x",
         .accounted_bytes = bytes});
    REQUIRE(prepared);
    return *prepared;
}
auto publish(delivery_queue& queue, delivery_packet value, delivery_time now = at()) -> void {
    auto reservation = queue.reserve_delivery(std::move(value), now);
    REQUIRE(reservation);
    REQUIRE(reservation->commit());
}
auto credit(delivery_queue& queue, std::uint64_t serial = 1) -> void {
    REQUIRE(queue.grant_credit({.scope = scope(), .serial = serial, .events = 3, .bytes = 20}));
}
auto take(delivery_queue& queue, delivery_flush flush = {.now = at(), .force = true})
    -> delivery_output_batch {
    auto result = queue.take_flush_batch(flush);
    REQUIRE(result);
    REQUIRE(result->has_value());
    return std::move(**result);
}

template <typename Packet> auto check_factory_only_packet() -> void {
    // Runtime checks produce an executable RED, rather than preventing the test binary building.
    CHECK_FALSE(std::is_copy_constructible_v<Packet>);
    CHECK_FALSE(std::is_move_constructible_v<Packet>);
    CHECK_FALSE(std::is_copy_assignable_v<Packet>);
    CHECK_FALSE(std::is_move_assignable_v<Packet>);
    if constexpr (std::is_copy_constructible_v<Packet> && std::is_copy_assignable_v<Packet>) {
        auto original = packet(1);
        auto alias = std::make_shared<Packet>(*original);
        auto queue = make_queue();
        auto replacement = encoded_delivery_packet::prepare(
            {.scope = {.epoch = "epoch:alias", .stream = 77},
             .clock = {.sequence = 9, .revision = 9},
             .encoded = "mutated",
             .accounted_bytes = 8});
        REQUIRE(replacement);
        SECTION("mutation between reservation and commit") {
            auto pending = queue.reserve_delivery(alias, at());
            REQUIRE(pending);
            *alias = **replacement;
            CHECK(alias->scope() == original->scope());
            CHECK(alias->clock().sequence == original->clock().sequence);
            CHECK(alias->clock().revision == original->clock().revision);
            CHECK(alias->encoded() == original->encoded());
            CHECK(alias->accounted_bytes() == original->accounted_bytes());
            CHECK(queue.status().queue_bytes == original->accounted_bytes());
            // The mutated charge remains safely within every capacity. No crash/UB reproducer.
            REQUIRE(pending->commit());
            CHECK(queue.status().latest_sequence == original->clock().sequence);
        }
        SECTION("mutation of already queued and retained values") {
            publish(queue, alias);
            auto retained = queue.resume_from({.scope = scope(), .sequence = 0});
            REQUIRE(retained);
            REQUIRE(retained->coverage == delivery_coverage::complete);
            REQUIRE(retained->packets.size() == 1);
            *alias = **replacement;
            const auto& admitted = retained->packets.front();
            CHECK(admitted->scope() == original->scope());
            CHECK(admitted->clock().sequence == original->clock().sequence);
            CHECK(admitted->clock().revision == original->clock().revision);
            CHECK(admitted->encoded() == original->encoded());
            CHECK(admitted->accounted_bytes() == original->accounted_bytes());
            CHECK(queue.status().latest_sequence == admitted->clock().sequence);
            CHECK(queue.status().queue_bytes == admitted->accounted_bytes());
            CHECK(queue.status().history_bytes == admitted->accounted_bytes());
            // Stop before output/eviction could use the inconsistent charge. Disconnect is safe.
            queue.disconnect();
        }
    }
}
} // namespace

TEST_CASE(
    "delivery prepares immutable values and validates caller overhead",
    "[engine_client_delivery]") {
    static_assert(std::is_nothrow_move_constructible_v<delivery_reservation>);
    static_assert(std::is_nothrow_move_constructible_v<delivery_output_batch>);
    static_assert(noexcept(std::declval<delivery_reservation&>().commit()));
    auto source = std::string("owned");
    auto identity = scope();
    auto prepared = encoded_delivery_packet::prepare(
        {.scope = identity, .clock = {.sequence = 1}, .encoded = source, .accounted_bytes = 8});
    REQUIRE(prepared);
    source.clear();
    identity.epoch.clear();
    CHECK((*prepared)->encoded() == "owned");
    CHECK((*prepared)->scope() == scope());
    CHECK((*prepared)->accounted_bytes() == 8);
    CHECK_FALSE(encoded_delivery_packet::prepare(
        {.scope = scope(), .encoded = "owned", .accounted_bytes = 4}));
    CHECK_FALSE(encoded_delivery_packet::prepare({.scope = scope(), .accounted_bytes = 1}));
    CHECK_FALSE(encoded_delivery_packet::prepare(
        {.scope = {.epoch = std::string(257, 'e')}, .encoded = "x", .accounted_bytes = 1}));
    CHECK(encoded_delivery_packet::prepare(
        {.scope = {.epoch = std::string(128, 'e')}, .encoded = "x", .accounted_bytes = 1}));
}

TEST_CASE(
    "delivery rejects invalid and overflowing negotiated limits", "[engine_client_delivery]") {
    auto opts = options();
    SECTION("zero batch count") { opts.batch_events = 0; }
    SECTION("zero batch bytes") { opts.batch_bytes = 0; }
    SECTION("zero queue count") { opts.queue_events = 0; }
    SECTION("zero queue bytes") { opts.queue_bytes = 0; }
    SECTION("zero history count") { opts.history_events = 0; }
    SECTION("zero history bytes") { opts.history_bytes = 0; }
    SECTION("batch bytes above queue") { opts.batch_bytes = 21; }
    SECTION("empty epoch") { opts.scope.epoch.clear(); }
    SECTION("bounded epoch") { opts.scope.epoch = std::string(257, 'e'); }
    SECTION("negative interval") { opts.flush_interval = -1ms; }
    SECTION("overflow interval") { opts.flush_interval = std::chrono::milliseconds::max(); }
    SECTION("overflow queue indices") {
        opts.queue_events = std::numeric_limits<std::size_t>::max();
    }
    SECTION("overflow history indices") {
        opts.history_events = std::numeric_limits<std::size_t>::max();
    }
    SECTION("overflow queue allocation size") {
        opts.queue_events = std::numeric_limits<std::size_t>::max() / 4;
    }
    SECTION("overflow history allocation size") {
        opts.history_events = std::numeric_limits<std::size_t>::max() / 4;
    }
    CHECK(delivery_queue::create(opts).error() == delivery_error::invalid_options);
}

TEST_CASE(
    "delivery admission is atomic and reservations cancel without clock consumption",
    "[engine_client_delivery]") {
    auto queue = make_queue();
    auto first = packet(1);
    auto weak = std::weak_ptr<const encoded_delivery_packet>(first);
    {
        auto reservation = queue.reserve_delivery(first, at());
        REQUIRE(reservation);
        CHECK(queue.status().queue_events == 1);
        CHECK(queue.status().queue_bytes == 5);
        CHECK(queue.status().history_events == 0);
        CHECK(queue.status().latest_sequence == 0);
        CHECK(queue.reserve_delivery(packet(1), at()).error() == delivery_error::busy);
        CHECK_FALSE(queue.take_flush_batch({.now = at(100ms), .force = true})->has_value());
        first.reset();
        CHECK_FALSE(weak.expired());
    }
    CHECK(weak.expired());
    CHECK(queue.status().queue_events == 0);
    auto reservation = queue.reserve_delivery(packet(1), at());
    REQUIRE(reservation);
    auto moved = std::move(*reservation);
    CHECK(reservation->commit().error() == delivery_error::invalid_packet);
    REQUIRE(moved.commit());
    CHECK(moved.commit().error() == delivery_error::invalid_packet);
    CHECK(queue.status().latest_sequence == 1);
    CHECK(queue.status().history_events == 1);
    CHECK(queue.reserve_delivery(packet(1), at()).error() == delivery_error::bad_clock);
    CHECK(queue.reserve_delivery(packet(3), at()).error() == delivery_error::bad_clock);
    CHECK(queue.reserve_delivery(packet(2, 11), at()).error() == delivery_error::too_large);
    CHECK(queue.reserve_delivery(packet(2, std::numeric_limits<std::size_t>::max()), at()).error()
          == delivery_error::too_large);
    CHECK(queue.reserve_delivery(nullptr, at()).error() == delivery_error::invalid_packet);
    CHECK(queue.reserve_delivery(packet(2, 5, {.epoch = "stale", .stream = 7}), at()).error()
          == delivery_error::wrong_scope);
    CHECK(queue.reserve_delivery(packet(2, 5, {.epoch = "epoch:test", .stream = 8}), at()).error()
          == delivery_error::wrong_scope);
    CHECK(queue.status().latest_sequence == 1);
    publish(queue, packet(2, 10));
    CHECK(queue.status().queue_bytes == 15);
}

TEST_CASE(
    "delivery queue count and byte boundaries include reservations and partial output",
    "[engine_client_delivery]") {
    auto opts = options();
    SECTION("count limit") {
        opts.queue_events = 2;
        auto queue = make_queue(opts);
        publish(queue, packet(1, 1));
        auto pending = queue.reserve_delivery(packet(2, 1), at());
        REQUIRE(pending);
        REQUIRE(pending->commit());
        CHECK(queue.reserve_delivery(packet(3, 1), at()).error() == delivery_error::full);
        REQUIRE(queue.grant_credit({.scope = scope(), .serial = 1, .events = 1, .bytes = 1}));
        auto output = take(queue);
        CHECK(output.packets().front()->clock().sequence == 1);
        CHECK(queue.status().queue_events == 2);
        CHECK(queue.reserve_delivery(packet(3, 1), at()).error() == delivery_error::full);
        CHECK(queue.take_flush_batch({.now = at(), .force = true}).error() == delivery_error::busy);
        CHECK(queue.acknowledge({.scope = scope(), .sequence = 1}).error()
              == delivery_error::invalid_ack);
        REQUIRE(output.finish());
        CHECK(queue.status().queue_events == 1);
        REQUIRE(queue.wait_for_capacity(1));
        publish(queue, packet(3, 1));
    }
    SECTION("byte limit") {
        auto queue = make_queue(opts);
        publish(queue, packet(1, 10));
        auto pending = queue.reserve_delivery(packet(2, 10), at());
        REQUIRE(pending);
        CHECK(queue.status().queue_bytes == 20);
        pending->cancel();
        CHECK(queue.status().queue_bytes == 10);
        publish(queue, packet(2, 10));
        CHECK(queue.reserve_delivery(packet(3, 1), at()).error() == delivery_error::full);
        credit(queue);
        auto output = take(queue);
        CHECK(output.packets().size() == 1);
        CHECK(queue.status().queue_bytes == 20);
        REQUIRE(output.finish());
        publish(queue, packet(3, 10));
        CHECK(queue.status().queue_bytes == 20);
    }
    SECTION("history single-packet bound") {
        opts.history_bytes = 4;
        auto queue = make_queue(opts);
        CHECK(queue.reserve_delivery(packet(1, 5), at()).error() == delivery_error::too_large);
        publish(queue, packet(1, 4));
    }
}

TEST_CASE(
    "delivery credit controls stay independent bounded scoped and transactional",
    "[engine_client_delivery]") {
    auto queue = make_queue();
    publish(queue, packet(1));
    publish(queue, packet(2));
    publish(queue, packet(3));
    CHECK_FALSE(queue.take_flush_batch({.now = at(100ms)})->has_value());
    CHECK(queue.status().queue_events == 3);
    auto request = delivery_credit{.scope = scope(), .serial = 1, .events = 3, .bytes = 20};
    SECTION("stale epoch") {
        request.scope.epoch = "stale";
        CHECK(queue.grant_credit(request).error() == delivery_error::wrong_scope);
    }
    SECTION("other stream") {
        ++request.scope.stream;
        CHECK(queue.grant_credit(request).error() == delivery_error::wrong_scope);
    }
    SECTION("event overflow") {
        request.events = std::numeric_limits<std::size_t>::max();
        CHECK(queue.grant_credit(request).error() == delivery_error::overgrant);
    }
    SECTION("byte overflow") {
        request.bytes = std::numeric_limits<std::size_t>::max();
        CHECK(queue.grant_credit(request).error() == delivery_error::overgrant);
    }
    SECTION("noncontiguous serial") {
        request.serial = std::numeric_limits<std::uint64_t>::max();
        CHECK(queue.grant_credit(request).error() == delivery_error::duplicate);
    }
    CHECK(queue.status().credit_events == 0);
    CHECK(queue.status().credit_bytes == 0);
    credit(queue);
    CHECK(queue.grant_credit({.scope = scope(), .serial = 1, .events = 1}).error()
          == delivery_error::duplicate);
    CHECK(queue.grant_credit({.scope = scope(), .serial = 2, .events = 1}).error()
          == delivery_error::overgrant);
    CHECK(queue.grant_credit({.scope = scope(), .serial = 2, .bytes = 1}).error()
          == delivery_error::overgrant);
    auto output = take(queue);
    CHECK(output.packets().size() == 2);
    CHECK(output.packets()[0]->clock().sequence == 1);
    CHECK(output.packets()[1]->clock().sequence == 2);
    CHECK(queue.status().credit_events == 1);
    CHECK(queue.status().credit_bytes == 10);
    REQUIRE(output.finish());
    CHECK(queue.acknowledge({.scope = scope(), .sequence = 3}).error()
          == delivery_error::invalid_ack);
    CHECK(
        queue.acknowledge({.scope = scope(), .sequence = std::numeric_limits<std::uint64_t>::max()})
            .error()
        == delivery_error::invalid_ack);
    REQUIRE(queue.acknowledge({.scope = scope(), .sequence = 2}));
    CHECK(queue.acknowledge({.scope = scope(), .sequence = 2}).error()
          == delivery_error::invalid_ack);
    CHECK(queue.acknowledge({.scope = scope(), .sequence = 1}).error()
          == delivery_error::invalid_ack);
    CHECK(queue.acknowledge({.scope = {.epoch = "stale", .stream = 7}, .sequence = 2}).error()
          == delivery_error::wrong_scope);
    CHECK(queue.status().acknowledged_sequence == 2);
    CHECK(queue.status().credit_events == 1);
    auto suffix = take(queue);
    CHECK(suffix.packets().front()->clock().sequence == 3);
    REQUIRE(suffix.finish());
    REQUIRE(queue.acknowledge({.scope = scope(), .sequence = 3}));
    CHECK(queue.status().acknowledged_sequence == 3);
}

TEST_CASE(
    "delivery monotonic first-message deadlines persist through zero and partial credits",
    "[engine_client_delivery]") {
    auto queue = make_queue();
    publish(queue, packet(1, 6), at(10ms));
    CHECK(queue.status().flush_deadline == at(26ms));
    CHECK_FALSE(queue.take_flush_batch({.now = at(25ms)})->has_value());
    CHECK_FALSE(queue.take_flush_batch({.now = at(26ms)})->has_value());
    CHECK_FALSE(queue.take_flush_batch({.now = at(100ms)})->has_value());
    REQUIRE(queue.grant_credit({.scope = scope(), .serial = 1, .bytes = 6}));
    CHECK_FALSE(queue.take_flush_batch({.now = at(100ms), .force = true})->has_value());
    REQUIRE(queue.grant_credit({.scope = scope(), .serial = 2, .events = 2}));
    publish(queue, packet(2, 4), at(20ms));
    CHECK(queue.status().flush_deadline == at(26ms));
    auto output = take(queue, {.now = at(100ms)});
    CHECK(output.packets().size() == 1);
    CHECK(queue.status().flush_deadline == at(36ms));
    REQUIRE(output.finish());
    CHECK_FALSE(queue.take_flush_batch({.now = at(100ms)})->has_value());
    REQUIRE(queue.grant_credit({.scope = scope(), .serial = 3, .bytes = 3}));
    CHECK_FALSE(queue.take_flush_batch({.now = at(100ms)})->has_value());
    REQUIRE(queue.grant_credit({.scope = scope(), .serial = 4, .bytes = 1}));
    auto suffix = take(queue, {.now = at(100ms)});
    CHECK(suffix.packets().front()->clock().sequence == 2);
    REQUIRE(suffix.finish());
    CHECK_FALSE(queue.status().flush_deadline);
    CHECK_FALSE(queue.take_flush_batch({.now = at(100ms)})->has_value());
    CHECK(queue.reserve_delivery(packet(3), at(19ms)).error() == delivery_error::bad_clock);
}

TEST_CASE(
    "delivery flush triggers are exact and never exceed negotiated batch bounds",
    "[engine_client_delivery]") {
    auto queue = make_queue();
    SECTION("exact deadline") {
        credit(queue);
        publish(queue, packet(1), at(10ms));
        CHECK_FALSE(queue.take_flush_batch({.now = at(25ms)})->has_value());
        auto output = take(queue, {.now = at(26ms)});
        REQUIRE(output.finish());
    }
    SECTION("force before deadline") {
        credit(queue);
        publish(queue, packet(1), at(10ms));
        auto output = take(queue, {.now = at(10ms), .force = true});
        REQUIRE(output.finish());
    }
    SECTION("exact byte boundary") {
        credit(queue);
        publish(queue, packet(1, 10));
        auto output = take(queue, {.now = at()});
        CHECK(output.packets().size() == 1);
        REQUIRE(output.finish());
    }
    SECTION("exact count boundary") {
        credit(queue);
        publish(queue, packet(1, 1));
        publish(queue, packet(2, 1));
        auto output = take(queue, {.now = at()});
        CHECK(output.packets().size() == 2);
        REQUIRE(output.finish());
    }
    SECTION("partial batch does not skip a large required head") {
        credit(queue);
        publish(queue, packet(1, 6));
        publish(queue, packet(2, 5));
        auto output = take(queue, {.now = at()});
        CHECK(output.packets().size() == 1);
        CHECK(output.packets().front()->clock().sequence == 1);
        REQUIRE(output.finish());
        auto suffix = take(queue, {.now = at(), .force = true});
        CHECK(suffix.packets().front()->clock().sequence == 2);
        REQUIRE(suffix.finish());
    }
}

TEST_CASE(
    "delivery history reports actual complete or gapped coverage and bounded pages",
    "[engine_client_delivery]") {
    auto opts = options();
    SECTION("count eviction") { opts.history_events = 3; }
    SECTION("byte eviction") { opts.history_bytes = 15; }
    auto queue = make_queue(opts);
    auto empty = queue.resume_from({.scope = scope(), .sequence = 0});
    REQUIRE(empty);
    CHECK(empty->coverage == delivery_coverage::complete);
    CHECK_FALSE(empty->earliest_retained);
    CHECK_FALSE(empty->latest_retained);
    publish(queue, packet(1));
    publish(queue, packet(2));
    credit(queue);
    auto output = take(queue);
    REQUIRE(output.finish());
    publish(queue, packet(3));
    publish(queue, packet(4));
    CHECK(queue.status().earliest_retained == 2);
    CHECK(queue.status().latest_retained == 4);
    CHECK(queue.status().history_events == 3);
    CHECK(queue.status().history_bytes == 15);
    auto gap = queue.resume_from({.scope = scope(), .sequence = 0});
    REQUIRE(gap);
    CHECK(gap->coverage == delivery_coverage::gapped);
    CHECK(gap->packets.empty());
    CHECK(gap->earliest_retained == 2);
    CHECK(gap->latest_retained == 4);
    auto complete = queue.resume_from({.scope = scope(), .sequence = 1});
    REQUIRE(complete);
    CHECK(complete->coverage == delivery_coverage::complete);
    CHECK(complete->packets.size() == 2);
    CHECK(complete->packets.front()->clock().sequence == 2);
    CHECK(complete->next_sequence == 3);
    CHECK(complete->more);
    auto tail = queue.resume_from({.scope = scope(), .sequence = complete->next_sequence});
    REQUIRE(tail);
    CHECK(tail->packets.size() == 1);
    CHECK(tail->packets.front()->clock().sequence == 4);
    CHECK_FALSE(tail->more);
    CHECK(queue.resume_from({.scope = scope(), .sequence = 4})->packets.empty());
    CHECK(queue.resume_from({.scope = scope(), .sequence = 5}).error()
          == delivery_error::invalid_ack);
    CHECK(queue.resume_from({.scope = {.epoch = "other"}, .sequence = 0}).error()
          == delivery_error::wrong_scope);
    queue.disconnect();
    CHECK(queue.resume_from({.scope = scope(), .sequence = 1})->coverage
          == delivery_coverage::complete);
    CHECK(queue.status().history_events == 3);
}

TEST_CASE(
    "delivery history byte-page boundaries and repeated ring eviction are bounded",
    "[engine_client_delivery]") {
    auto opts = options();
    opts.history_events = 2;
    opts.history_bytes = 12;
    auto queue = make_queue(opts);
    auto oldest = packet(1, 6);
    auto weak = std::weak_ptr<const encoded_delivery_packet>(oldest);
    publish(queue, oldest);
    oldest.reset();
    publish(queue, packet(2, 6));
    credit(queue);
    auto first = take(queue);
    REQUIRE(first.finish());
    auto second = take(queue);
    REQUIRE(second.finish());
    for (auto sequence = std::uint64_t{3}; sequence <= 20; ++sequence) {
        publish(queue, packet(sequence, 6));
        CHECK(queue.status().history_events == 2);
        CHECK(queue.status().history_bytes == 12);
        CHECK(queue.status().earliest_retained == sequence - 1);
        REQUIRE(queue.grant_credit(
            {.scope = scope(), .serial = sequence - 1, .events = 1, .bytes = 6}));
        auto output = take(queue);
        REQUIRE(output.finish());
    }
    CHECK(weak.expired());
    auto page = queue.resume_from({.scope = scope(), .sequence = 18});
    REQUIRE(page);
    CHECK(page->coverage == delivery_coverage::complete);
    CHECK(page->packets.size() == 1);
    CHECK(page->next_sequence == 19);
    CHECK(page->more);
}

TEST_CASE(
    "delivery disconnect and output failure cannot become completion", "[engine_client_delivery]") {
    auto queue = make_queue();
    SECTION("disconnect before commit") {
        auto pending = queue.reserve_delivery(packet(1), at());
        REQUIRE(pending);
        queue.disconnect();
        CHECK(pending->commit().error() == delivery_error::disconnected);
        CHECK(queue.status().latest_sequence == 0);
        CHECK(queue.status().history_events == 0);
    }
    SECTION("explicit write failure") {
        publish(queue, packet(1));
        credit(queue);
        auto output = take(queue);
        output.fail();
        CHECK(output.finish().error() == delivery_error::invalid_packet);
        CHECK(queue.status().delivered_sequence == 0);
        CHECK(queue.resume_from({.scope = scope(), .sequence = 0})->packets.size() == 1);
    }
    SECTION("abandoned partial write") {
        publish(queue, packet(1));
        credit(queue);
        { auto output = take(queue); }
        CHECK(queue.status().delivered_sequence == 0);
    }
    SECTION("disconnect during partial write") {
        publish(queue, packet(1));
        credit(queue);
        auto output = take(queue);
        queue.disconnect();
        CHECK(output.packets().front()->encoded() == "x");
        CHECK(queue.status().queue_events == 1);
        CHECK(output.finish().error() == delivery_error::disconnected);
        CHECK(queue.status().delivered_sequence == 0);
    }
    CHECK_FALSE(queue.status().connected);
    CHECK(queue.status().queue_events == 0);
    CHECK(queue.reserve_delivery(packet(1), at()).error() == delivery_error::disconnected);
    CHECK(queue.take_flush_batch({.now = at(), .force = true}).error()
          == delivery_error::disconnected);
    CHECK(queue.grant_credit({.scope = scope(), .serial = 1, .events = 1}).error()
          == delivery_error::disconnected);
    CHECK(queue.acknowledge({.scope = scope(), .sequence = 1}).error()
          == delivery_error::disconnected);
    CHECK(queue.wait_for_capacity(1).error() == delivery_error::disconnected);
    queue.disconnect();
}

TEST_CASE(
    "delivery producer wait has no lost disconnect or cancellation wake",
    "[engine_client_delivery]") {
    auto opts = options();
    opts.batch_events = 1;
    opts.queue_events = 1;
    auto queue = make_queue(opts);
    auto pending = queue.reserve_delivery(packet(1), at());
    REQUIRE(pending);
    auto started = std::promise<void>{};
    auto ready = started.get_future();
    auto waiter = std::async(std::launch::async, [&] {
        started.set_value();
        return queue.wait_for_capacity(5);
    });
    // No sleep/timing assertion: both possible scheduling orders must make forward progress.
    ready.get();
    SECTION("disconnect") {
        queue.disconnect();
        CHECK(waiter.get().error() == delivery_error::disconnected);
    }
    SECTION("cancel") {
        pending->cancel();
        CHECK(waiter.get());
    }
}

TEST_CASE(
    "delivery clocks and deadlines resist exact integer overflow", "[engine_client_delivery]") {
    auto opts = options();
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    opts.initial_clock = {.sequence = maximum - 1, .revision = maximum};
    auto queue = make_queue(opts);
    auto prepared = encoded_delivery_packet::prepare(
        {.scope = scope(),
         .clock = {.sequence = maximum, .revision = maximum},
         .encoded = "x",
         .accounted_bytes = 5});
    REQUIRE(prepared);
    publish(queue, *prepared, delivery_time::max() - 1ms);
    CHECK(queue.status().flush_deadline == delivery_time::max());
    CHECK(queue.reserve_delivery(packet(0), delivery_time::max()).error()
          == delivery_error::bad_clock);
    credit(queue);
    CHECK_FALSE(queue.take_flush_batch({.now = delivery_time::max() - 1ms})->has_value());
    auto output = take(queue, {.now = delivery_time::max()});
    REQUIRE(output.finish());
    REQUIRE(queue.acknowledge({.scope = scope(), .sequence = maximum}));
    CHECK(queue.resume_from({.scope = scope(), .sequence = maximum})->coverage
          == delivery_coverage::complete);
    CHECK(queue.resume_from({.scope = scope(), .sequence = maximum - 1})->packets.size() == 1);
    CHECK(queue.resume_from({.scope = scope(), .sequence = maximum - 2})->coverage
          == delivery_coverage::gapped);
}

TEST_CASE(
    "delivery initial budgets bound gameplay and metadata through long streams",
    "[engine_client_delivery]") {
    const auto opts = delivery_options{.scope = scope()};
    CHECK(opts.batch_events == 8);
    CHECK(opts.batch_bytes == 262144);
    CHECK(opts.queue_events == 1024);
    CHECK(opts.queue_bytes == 8 * 1024 * 1024);
    CHECK(opts.history_events == 4096);
    CHECK(opts.history_bytes == 32 * 1024 * 1024);
    CHECK(opts.flush_interval == 16ms);
    auto queue = make_queue(opts);
    for (auto sequence = std::uint64_t{1}; sequence <= 1024; ++sequence) {
        publish(queue, packet(sequence, 8192));
    }
    CHECK(queue.status().queue_events == 1024);
    CHECK(queue.status().queue_bytes == opts.queue_bytes);
    CHECK(queue.reserve_delivery(packet(1025, 8192), at()).error() == delivery_error::full);
    REQUIRE(queue.grant_credit(
        {.scope = scope(), .serial = 1, .events = 1024, .bytes = opts.queue_bytes}));
    for (auto batch = 0; batch < 128; ++batch) {
        auto output = take(queue);
        CHECK(output.packets().size() == 8);
        REQUIRE(output.finish());
    }
    CHECK(queue.status().queue_bytes == 0);
    CHECK(queue.status().delivered_sequence == 1024);
    for (auto sequence = std::uint64_t{1025}; sequence <= 5000; ++sequence) {
        publish(queue, packet(sequence, 8192));
        REQUIRE(queue.grant_credit(
            {.scope = scope(), .serial = sequence - 1023, .events = 1, .bytes = 8192}));
        auto output = take(queue);
        REQUIRE(output.finish());
    }
    CHECK(queue.status().history_events == 4096);
    CHECK(queue.status().history_bytes == opts.history_bytes);
    CHECK(queue.status().earliest_retained == 905);
    CHECK(queue.status().latest_retained == 5000);
    CHECK(queue.resume_from({.scope = scope(), .sequence = 903})->coverage
          == delivery_coverage::gapped);
    CHECK(queue.resume_from({.scope = scope(), .sequence = 904})->coverage
          == delivery_coverage::complete);
}

TEST_CASE(
    "delivery reserved and output leases remain unique across moves", "[engine_client_delivery]") {
    auto left = make_queue();
    auto right = make_queue();
    SECTION("reservation move assignment cancels old reservation") {
        auto first = left.reserve_delivery(packet(1), at());
        auto second = right.reserve_delivery(packet(1), at());
        REQUIRE(first);
        REQUIRE(second);
        *first = std::move(*second);
        CHECK_FALSE(left.status().reserved);
        CHECK(left.status().latest_sequence == 0);
        CHECK(right.status().reserved);
        REQUIRE(first->commit());
        CHECK(second->commit().error() == delivery_error::invalid_packet);
        CHECK(right.status().latest_sequence == 1);
    }
    SECTION("output move constructor leaves source harmless") {
        publish(left, packet(1));
        credit(left);
        auto output = take(left);
        auto moved = std::move(output);
        output.fail();
        CHECK(left.status().connected);
        CHECK(left.status().output_pending);
        REQUIRE(moved.finish());
        CHECK(left.status().connected);
    }
    SECTION("output move assignment abandons old partial write") {
        publish(left, packet(1));
        publish(right, packet(1));
        credit(left);
        credit(right);
        auto first = take(left);
        auto second = take(right);
        first = std::move(second);
        CHECK_FALSE(left.status().connected);
        CHECK(left.status().delivered_sequence == 0);
        second.fail();
        CHECK(right.status().connected);
        REQUIRE(first.finish());
        CHECK(right.status().delivered_sequence == 1);
    }
}

TEST_CASE(
    "delivery producer wakes when successful output releases capacity",
    "[engine_client_delivery]") {
    auto opts = options();
    opts.batch_events = 1;
    opts.queue_events = 1;
    auto queue = make_queue(opts);
    publish(queue, packet(1));
    REQUIRE(queue.grant_credit({.scope = scope(), .serial = 1, .events = 1, .bytes = 5}));
    auto output = take(queue);
    auto started = std::promise<void>{};
    auto ready = started.get_future();
    auto waiter = std::async(std::launch::async, [&] {
        started.set_value();
        return queue.wait_for_capacity(5);
    });
    ready.get();
    REQUIRE(output.finish());
    REQUIRE(waiter.get());
    publish(queue, packet(2));
    CHECK(queue.status().latest_sequence == 2);
}

TEST_CASE(
    "delivery revision and initial snapshot endpoints do not invent history",
    "[engine_client_delivery]") {
    auto opts = options();
    opts.initial_clock = {.sequence = 5, .revision = 10};
    opts.flush_interval = 0ms;
    auto queue = make_queue(opts);
    CHECK(queue.resume_from({.scope = scope(), .sequence = 5})->coverage
          == delivery_coverage::complete);
    CHECK(queue.resume_from({.scope = scope(), .sequence = 4})->coverage
          == delivery_coverage::gapped);
    CHECK_FALSE(queue.status().earliest_retained);
    auto regressed = encoded_delivery_packet::prepare(
        {.scope = scope(),
         .clock = {.sequence = 6, .revision = 9},
         .encoded = "x",
         .accounted_bytes = 5});
    REQUIRE(regressed);
    CHECK(queue.reserve_delivery(*regressed, at()).error() == delivery_error::bad_clock);
    auto presentation = encoded_delivery_packet::prepare(
        {.scope = scope(),
         .clock = {.sequence = 6, .revision = 10},
         .encoded = "x",
         .accounted_bytes = 5});
    REQUIRE(presentation);
    publish(queue, *presentation);
    credit(queue);
    auto output = take(queue, {.now = at()});
    CHECK(output.packets().front()->clock().revision == 10);
    REQUIRE(output.finish());
    CHECK(queue.status().latest_sequence == 6);
    CHECK(queue.status().earliest_retained == 6);
    CHECK(queue.resume_from({.scope = scope(), .sequence = 4})->coverage
          == delivery_coverage::gapped);
    CHECK(queue.resume_from({.scope = scope(), .sequence = 5})->coverage
          == delivery_coverage::complete);
    CHECK(queue.wait_for_capacity(0).error() == delivery_error::too_large);
    CHECK(queue.wait_for_capacity(11).error() == delivery_error::too_large);
}

TEST_CASE(
    "delivery control rejection never consumes serial or changes credit",
    "[engine_client_delivery]") {
    auto queue = make_queue();
    CHECK(queue.grant_credit({.scope = scope(), .serial = 1}).error() == delivery_error::overgrant);
    REQUIRE(queue.grant_credit({.scope = scope(), .serial = 1, .events = 2, .bytes = 10}));
    CHECK(queue.grant_credit({.scope = scope(), .serial = 2, .events = 2, .bytes = 5}).error()
          == delivery_error::overgrant);
    CHECK(queue.status().credit_events == 2);
    CHECK(queue.status().credit_bytes == 10);
    CHECK(
        queue
            .grant_credit(
                {.scope = scope(),
                 .serial = 2,
                 .events = 1,
                 .bytes = std::numeric_limits<std::size_t>::max()})
            .error()
        == delivery_error::overgrant);
    REQUIRE(queue.grant_credit({.scope = scope(), .serial = 2, .events = 1, .bytes = 10}));
    CHECK(queue.status().credit_events == 3);
    CHECK(queue.status().credit_bytes == 20);
}

TEST_CASE(
    "delivery admitted packets cannot acquire mutable construction or assignment aliases",
    "[engine_client_delivery][engine_client_delivery_red]") {
    check_factory_only_packet<encoded_delivery_packet>();
}

TEST_CASE(
    "delivery accepts the full ordered epoch byte range through output and recovery",
    "[engine_client_delivery][engine_client_delivery_red]") {
    auto length = std::size_t{128};
    SECTION("128 byte positive control") { length = 128; }
    SECTION("129 byte ordered epoch") { length = 129; }
    SECTION("256 byte exact ordered maximum") { length = 256; }
    const auto identity = delivery_scope{.epoch = std::string(length, 'e'), .stream = 7};
    auto prepared = encoded_delivery_packet::prepare(
        {.scope = identity,
         .clock = {.sequence = 1, .revision = 1},
         .encoded = "x",
         .accounted_bytes = 5});
    CHECK(prepared.has_value());
    auto opts = options();
    opts.scope = identity;
    auto created = delivery_queue::create(opts);
    CHECK(created.has_value());
    if (!prepared || !created) { return; }
    auto& queue = *created;
    publish(queue, *prepared);
    REQUIRE(queue.grant_credit({.scope = identity, .serial = 1, .events = 1, .bytes = 5}));
    auto output = take(queue);
    CHECK(output.packets().front()->scope() == identity);
    REQUIRE(output.finish());
    REQUIRE(queue.acknowledge({.scope = identity, .sequence = 1}));
    auto replay = queue.resume_from({.scope = identity, .sequence = 0});
    REQUIRE(replay);
    CHECK(replay->coverage == delivery_coverage::complete);
    REQUIRE(replay->packets.size() == 1);
    CHECK(replay->packets.front()->scope() == identity);
}

TEST_CASE(
    "delivery paired zero history permits output without claiming lost transient recovery",
    "[engine_client_delivery][engine_client_delivery_red]") {
    auto opts = options();
    opts.history_events = 0;
    opts.history_bytes = 0;
    auto created = delivery_queue::create(opts);
    REQUIRE(created.has_value());
    auto& queue = *created;
    const auto empty = queue.resume_from({.scope = scope(), .sequence = 0});
    REQUIRE(empty);
    CHECK(empty->coverage == delivery_coverage::complete);
    CHECK(empty->packets.empty());
    CHECK_FALSE(empty->earliest_retained);
    CHECK_FALSE(empty->latest_retained);
    REQUIRE(queue.wait_for_capacity(5));
    for (auto sequence = std::uint64_t{1}; sequence <= 5; ++sequence) {
        publish(queue, packet(sequence));
        CHECK(queue.status().history_events == 0);
        CHECK(queue.status().history_bytes == 0);
        CHECK_FALSE(queue.status().earliest_retained);
        CHECK_FALSE(queue.status().latest_retained);
        REQUIRE(
            queue.grant_credit({.scope = scope(), .serial = sequence, .events = 1, .bytes = 5}));
        auto output = take(queue);
        CHECK(output.packets().front()->clock().sequence == sequence);
        REQUIRE(output.finish());
        REQUIRE(queue.acknowledge({.scope = scope(), .sequence = sequence}));
        const auto gap = queue.resume_from({.scope = scope(), .sequence = sequence - 1});
        REQUIRE(gap);
        CHECK(gap->coverage == delivery_coverage::gapped);
        CHECK(gap->packets.empty());
        CHECK_FALSE(gap->earliest_retained);
        CHECK_FALSE(gap->latest_retained);
        CHECK_FALSE(gap->more);
        const auto current = queue.resume_from({.scope = scope(), .sequence = sequence});
        REQUIRE(current);
        CHECK(current->coverage == delivery_coverage::complete);
        CHECK(current->packets.empty());
        CHECK_FALSE(current->earliest_retained);
        CHECK_FALSE(current->latest_retained);
    }
    auto pending = queue.reserve_delivery(packet(6), at());
    REQUIRE(pending);
    pending->cancel();
    CHECK(queue.status().latest_sequence == 5);
    queue.disconnect();
    const auto disconnected = queue.resume_from({.scope = scope(), .sequence = 0});
    REQUIRE(disconnected);
    CHECK(disconnected->coverage == delivery_coverage::gapped);
    CHECK(disconnected->packets.empty());
    CHECK_FALSE(disconnected->earliest_retained);
    CHECK_FALSE(disconnected->latest_retained);
}

TEST_CASE(
    "delivery batch event ceiling may exceed queue capacity without exceeding actual occupancy",
    "[engine_client_delivery][engine_client_delivery_red]") {
    auto opts = options();
    opts.batch_events = 8;
    opts.queue_events = 1;
    auto created = delivery_queue::create(opts);
    REQUIRE(created.has_value());
    auto& queue = *created;
    publish(queue, packet(1));
    CHECK(queue.reserve_delivery(packet(2), at()).error() == delivery_error::full);
    REQUIRE(queue.grant_credit({.scope = scope(), .serial = 1, .events = 1, .bytes = 5}));
    // Queue fullness flushes the one event even before the much larger batch ceiling/deadline.
    auto first = take(queue, {.now = at()});
    CHECK(first.packets().size() == 1);
    CHECK(first.packets().front()->clock().sequence == 1);
    CHECK(queue.status().queue_events == 1);
    CHECK(queue.status().queue_bytes == 5);
    CHECK(queue.reserve_delivery(packet(2), at()).error() == delivery_error::full);
    REQUIRE(first.finish());
    CHECK(queue.status().queue_events == 0);
    publish(queue, packet(2));
    REQUIRE(queue.grant_credit({.scope = scope(), .serial = 2, .events = 1, .bytes = 5}));
    auto second = take(queue, {.now = at()});
    CHECK(second.packets().size() == 1);
    CHECK(second.packets().front()->clock().sequence == 2);
    REQUIRE(second.finish());
    CHECK(queue.status().latest_sequence == 2);
    CHECK(queue.status().delivered_sequence == 2);
}

TEST_CASE(
    "delivery wrapped retained ring resumes all bounded pages without loss or duplication",
    "[engine_client_delivery][engine_client_delivery_paging]") {
    auto opts = options();
    opts.history_events = 17;
    opts.history_bytes = 85;
    auto queue = make_queue(opts);
    for (auto sequence = std::uint64_t{1}; sequence <= 35; ++sequence) {
        publish(queue, packet(sequence));
        REQUIRE(
            queue.grant_credit({.scope = scope(), .serial = sequence, .events = 1, .bytes = 5}));
        auto output = take(queue);
        REQUIRE(output.finish());
    }
    // 35 publications into 17 slots: retained [19,35], with the last value at physical slot zero.
    CHECK(queue.status().history_events == 17);
    CHECK(queue.status().history_bytes == 85);
    CHECK(queue.status().earliest_retained == 19);
    CHECK(queue.status().latest_retained == 35);
    auto cursor = std::uint64_t{18};
    auto pages = std::size_t{0};
    while (cursor < 35) {
        const auto page = queue.resume_from({.scope = scope(), .sequence = cursor});
        REQUIRE(page);
        REQUIRE(page->coverage == delivery_coverage::complete);
        CHECK(page->earliest_retained == 19);
        CHECK(page->latest_retained == 35);
        REQUIRE_FALSE(page->packets.empty());
        CHECK(page->packets.size() <= opts.batch_events);
        auto bytes = std::size_t{0};
        for (const auto& value : page->packets) {
            CHECK(value->clock().sequence == cursor + 1);
            cursor = value->clock().sequence;
            bytes += value->accounted_bytes();
        }
        CHECK(bytes <= opts.batch_bytes);
        CHECK(page->next_sequence == cursor);
        CHECK(page->more == (cursor < 35));
        ++pages;
    }
    CHECK(pages == 9);
    const auto gap = queue.resume_from({.scope = scope(), .sequence = 17});
    REQUIRE(gap);
    CHECK(gap->coverage == delivery_coverage::gapped);
    CHECK(gap->packets.empty());
    const auto last = queue.resume_from({.scope = scope(), .sequence = 34});
    REQUIRE(last);
    REQUIRE(last->packets.size() == 1);
    CHECK(last->packets.front()->clock().sequence == 35);
    CHECK_FALSE(last->more);
    const auto done = queue.resume_from({.scope = scope(), .sequence = 35});
    REQUIRE(done);
    CHECK(done->coverage == delivery_coverage::complete);
    CHECK(done->packets.empty());
}

TEST_CASE(
    "delivery oversized event ceiling bounds output and resume allocation by queue capacity",
    "[engine_client_delivery][engine_client_delivery_paging]") {
    auto opts = options();
    opts.batch_events = 8;
    SECTION("negotiated upper bound") { opts.batch_events = 8; }
    SECTION("arithmetic upper bound cannot become a reservation size") {
        opts.batch_events = std::numeric_limits<std::size_t>::max();
    }
    opts.queue_events = 2;
    opts.batch_bytes = 100;
    opts.queue_bytes = 100;
    opts.history_events = 7;
    opts.history_bytes = 100;
    auto queue = make_queue(opts);
    for (auto serial = std::uint64_t{1}; serial <= 5; ++serial) {
        publish(queue, packet(2 * serial - 1, 1));
        publish(queue, packet(2 * serial, 1));
        CHECK(queue.reserve_delivery(packet(2 * serial + 1, 1), at()).error()
              == delivery_error::full);
        REQUIRE(queue.grant_credit({.scope = scope(), .serial = serial, .events = 2, .bytes = 2}));
        auto output = take(queue, {.now = at()});
        CHECK(output.packets().size() == opts.queue_events);
        CHECK(queue.status().queue_events == opts.queue_events);
        REQUIRE(output.finish());
    }
    CHECK(queue.status().earliest_retained == 4);
    CHECK(queue.status().latest_retained == 10);
    auto cursor = std::uint64_t{3};
    auto pages = 0;
    while (cursor < 10) {
        const auto page = queue.resume_from({.scope = scope(), .sequence = cursor});
        REQUIRE(page);
        REQUIRE(page->coverage == delivery_coverage::complete);
        REQUIRE_FALSE(page->packets.empty());
        CHECK(page->packets.size() <= opts.queue_events);
        for (const auto& value : page->packets) {
            CHECK(value->clock().sequence == cursor + 1);
            cursor = value->clock().sequence;
        }
        CHECK(page->next_sequence == cursor);
        CHECK(page->more == (cursor < 10));
        ++pages;
    }
    CHECK(pages == 4);
}

TEST_CASE(
    "delivery disabled history keeps byte limits and failure recovery truthful",
    "[engine_client_delivery]") {
    auto opts = options();
    opts.history_events = 0;
    opts.history_bytes = 0;
    auto queue = make_queue(opts);
    CHECK(queue.reserve_delivery(packet(1, 11), at()).error() == delivery_error::too_large);
    publish(queue, packet(1, 10));
    publish(queue, packet(2, 10));
    CHECK(queue.status().queue_bytes == opts.queue_bytes);
    CHECK(queue.reserve_delivery(packet(3, 1), at()).error() == delivery_error::full);
    credit(queue);
    auto output = take(queue);
    REQUIRE(output.packets().size() == 1);
    SECTION("output failure is not completion or retention") {
        output.fail();
        CHECK(queue.status().delivered_sequence == 0);
    }
    SECTION("disconnect cancels reserved admission without history") {
        REQUIRE(output.finish());
        auto pending = queue.reserve_delivery(packet(3, 10), at());
        REQUIRE(pending);
        CHECK(queue.status().queue_bytes == opts.queue_bytes);
        queue.disconnect();
        CHECK(pending->commit().error() == delivery_error::disconnected);
        CHECK(queue.status().delivered_sequence == 1);
    }
    CHECK_FALSE(queue.status().connected);
    CHECK(queue.status().queue_events == 0);
    CHECK(queue.status().latest_sequence == 2);
    CHECK(queue.status().history_events == 0);
    CHECK(queue.status().history_bytes == 0);
    const auto gap = queue.resume_from({.scope = scope(), .sequence = 0});
    REQUIRE(gap);
    CHECK(gap->coverage == delivery_coverage::gapped);
    CHECK(gap->packets.empty());
    CHECK_FALSE(gap->earliest_retained);
    CHECK_FALSE(gap->latest_retained);
    CHECK_FALSE(gap->more);
}

TEST_CASE(
    "delivery direct resume indexing preserves wrapped suffix at the maximum sequence",
    "[engine_client_delivery][engine_client_delivery_paging]") {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    auto opts = options();
    opts.initial_clock = {.sequence = maximum - 7, .revision = maximum};
    opts.history_events = 3;
    auto queue = make_queue(opts);
    for (auto count = std::uint64_t{1}; count <= 7; ++count) {
        const auto prepared = encoded_delivery_packet::prepare(
            {.scope = scope(),
             .clock = {.sequence = maximum - 7 + count, .revision = maximum},
             .encoded = "x",
             .accounted_bytes = 5});
        REQUIRE(prepared);
        publish(queue, *prepared);
        REQUIRE(queue.grant_credit({.scope = scope(), .serial = count, .events = 1, .bytes = 5}));
        auto output = take(queue);
        REQUIRE(output.finish());
    }
    CHECK(queue.status().earliest_retained == maximum - 2);
    CHECK(queue.status().latest_retained == maximum);
    const auto first = queue.resume_from({.scope = scope(), .sequence = maximum - 3});
    REQUIRE(first);
    CHECK(first->coverage == delivery_coverage::complete);
    REQUIRE(first->packets.size() == 2);
    CHECK(first->packets[0]->clock().sequence == maximum - 2);
    CHECK(first->packets[1]->clock().sequence == maximum - 1);
    CHECK(first->next_sequence == maximum - 1);
    CHECK(first->more);
    const auto last = queue.resume_from({.scope = scope(), .sequence = first->next_sequence});
    REQUIRE(last);
    REQUIRE(last->packets.size() == 1);
    CHECK(last->packets.front()->clock().sequence == maximum);
    CHECK(last->next_sequence == maximum);
    CHECK_FALSE(last->more);
    const auto done = queue.resume_from({.scope = scope(), .sequence = maximum});
    REQUIRE(done);
    CHECK(done->coverage == delivery_coverage::complete);
    CHECK(done->packets.empty());
    const auto gap = queue.resume_from({.scope = scope(), .sequence = maximum - 4});
    REQUIRE(gap);
    CHECK(gap->coverage == delivery_coverage::gapped);
    CHECK(gap->packets.empty());
}
