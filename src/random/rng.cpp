#include "random/rng.h"

#include "calendar.h"
#include "cata_utility.h"
#include "units.h"
#include "weighted_list.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <utility>

auto rng_bits() -> unsigned int {
    // Whole uint range.
    static std::uniform_int_distribution<unsigned int> rng_uint_dist;
    return rng_uint_dist(rng_get_engine());
}

int rng(int lo, int hi) {
    static std::uniform_int_distribution<int> rng_int_dist;
    if (lo > hi) { std::swap(lo, hi); }
    return rng_int_dist(rng_get_engine(), std::uniform_int_distribution<>::param_type(lo, hi));
}

double rng_float(double lo, double hi) {
    static std::uniform_real_distribution<double> rng_real_dist;
    if (lo > hi) { std::swap(lo, hi); }
    return rng_real_dist(rng_get_engine(), std::uniform_real_distribution<>::param_type(lo, hi));
}

units::angle random_direction() { return rng_float(0_pi_radians, 2_pi_radians); }

double normal_roll(double mean, double stddev) {
    auto rng_normal_dist = std::normal_distribution<double>{};
    return rng_normal_dist(rng_get_engine(), std::normal_distribution<>::param_type(mean, stddev));
}

double exponential_roll(double lambda) {
    static std::exponential_distribution<double> rng_exponential_dist;
    return rng_exponential_dist(
        rng_get_engine(), std::exponential_distribution<>::param_type(lambda));
}

double rng_exponential(double min, double mean) {
    const double adjusted_mean = mean - min;
    if (adjusted_mean <= 0.0) { return 0.0; }
    // lambda = 1 / mean
    return min + exponential_roll(1.0 / adjusted_mean);
}

bool one_in(int chance) { return (chance <= 1 || rng(0, chance - 1) == 0); }

bool one_turn_in(const time_duration& duration) { return one_in(to_turns<int>(duration)); }

bool x_in_y(double x, double y) { return rng_float(0.0, 1.0) <= x / y; }

bool check(units::probability p) { return rng(0, 1000000 - 1) < units::to_one_in_million(p); }

int dice(int number, int sides) {
    int ret = 0;
    for (int i = 0; i < number; i++) { ret += rng(1, sides); }
    return ret;
}

// probabilistically round a double to an int
// 1.3 has a 70% chance of rounding to 1, 30% chance to 2.
int roll_remainder(double value) {
    double integ;
    double frac = modf(value, &integ);
    if (value > 0.0 && value > integ && x_in_y(frac, 1.0)) {
        integ++;
    } else if (value < 0.0 && value < integ && x_in_y(-frac, 1.0)) {
        integ--;
    }

    return integ;
}

// http://www.cse.yorku.ca/~oz/hash.html
// for world seeding.
int djb2_hash(const unsigned char* input) {
    unsigned int hash = 5381;
    unsigned char c = *input++;
    while (c != '\0') {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        c = *input++;
    }
    return hash;
}

double rng_normal(double lo, double hi) {
    if (lo > hi) { std::swap(lo, hi); }

    const double stddev = (hi - lo) / 4;
    if (stddev == 0.0) { return hi; }
    double val = normal_roll((hi + lo) / 2, stddev);
    return clamp(val, lo, hi);
}

namespace {

constexpr auto child_seed_stream = std::uint64_t{0x6368696c645f5f5f};

// Main-thread global engine (default, time-seeded).
auto main_rng_engine() -> cata_default_random_engine& // *NOPAD*
{
    // NOLINTNEXTLINE(cata-determinism)
    static auto eng = cata_default_random_engine(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return eng;
}

auto has_deterministic_seed() -> std::atomic_bool& // *NOPAD*
{
    static auto value = std::atomic_bool{false};
    return value;
}

auto deterministic_seed() -> std::atomic_uint& // *NOPAD*
{
    static auto value = std::atomic_uint{1u};
    return value;
}

auto deterministic_task_counter() -> std::atomic_uint64_t& // *NOPAD*
{
    static auto value = std::atomic_uint64_t{0};
    return value;
}

auto saved_main_rng_engine() -> std::optional<cata_default_random_engine>& // *NOPAD*
{
    static auto value = std::optional<cata_default_random_engine>{};
    return value;
}

auto splitmix64(std::uint64_t value) -> std::uint64_t {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

auto non_zero_seed(const std::uint64_t value) -> unsigned int {
    const auto seed = static_cast<unsigned int>(value & 0x7fffffffu);
    return seed == 0 ? 1u : seed;
}

// Per-worker-thread engine.  Inactive (flag=false) on the main thread.
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_is_worker = false;
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_worker_engine = cata_default_random_engine{};
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_has_task_engine = false;
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_task_engine = cata_default_random_engine{};
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_has_task_context = false;
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_task_context_seed = std::uint64_t{0};
// NOLINTNEXTLINE(cata-determinism)
thread_local auto tl_task_child_counter = std::uint64_t{0};

} // namespace

auto rng_get_engine() -> cata_default_random_engine& // *NOPAD*
{
    if (tl_has_task_engine) { return tl_task_engine; }
    if (tl_is_worker) { return tl_worker_engine; }
    return main_rng_engine();
}

auto rng_set_engine_seed(unsigned int seed) -> void {
    if (seed != 0) { main_rng_engine().seed(seed); }
}

auto rng_set_deterministic_seed(unsigned int seed) -> void {
    if (seed == 0) { seed = 1; }
    if (!has_deterministic_seed().load(std::memory_order_acquire)) {
        saved_main_rng_engine() = main_rng_engine();
    }
    deterministic_seed().store(seed, std::memory_order_relaxed);
    deterministic_task_counter().store(0, std::memory_order_relaxed);
    has_deterministic_seed().store(true, std::memory_order_release);
    main_rng_engine().seed(seed);
}

auto rng_clear_deterministic_seed() -> void {
    if (auto& saved_engine = saved_main_rng_engine(); saved_engine) {
        main_rng_engine() = *saved_engine;
        saved_engine.reset();
    }
    has_deterministic_seed().store(false, std::memory_order_release);
    deterministic_task_counter().store(0, std::memory_order_relaxed);
}

auto rng_deterministic_seed_active() -> bool {
    return has_deterministic_seed().load(std::memory_order_acquire);
}

auto rng_deterministic_seed_for(const rng_deterministic_key& key) -> unsigned int {
    auto value = static_cast<std::uint64_t>(deterministic_seed().load(std::memory_order_relaxed));
    value = splitmix64(value ^ key.stream);
    value = splitmix64(value ^ key.id);
    return non_zero_seed(value);
}

auto rng_deterministic_child_seed(const unsigned int parent_seed, const rng_deterministic_key& key)
    -> unsigned int {
    auto value = static_cast<std::uint64_t>(parent_seed);
    value = splitmix64(value ^ child_seed_stream ^ key.stream);
    value = splitmix64(value ^ key.id);
    return non_zero_seed(value);
}

auto rng_deterministic_seed_for_current_context(const rng_deterministic_key& key)
    -> std::optional<unsigned int> {
    if (!rng_deterministic_seed_active()) { return std::nullopt; }
    if (tl_has_task_context) {
        return rng_deterministic_child_seed(static_cast<unsigned int>(tl_task_context_seed), key);
    }
    return rng_deterministic_seed_for(key);
}

auto rng_next_deterministic_call_seed(const std::uint64_t stream) -> std::optional<unsigned int> {
    if (!rng_deterministic_seed_active()) { return std::nullopt; }
    if (tl_has_task_context) {
        const auto child_index = tl_task_child_counter++;
        return rng_deterministic_child_seed(
            static_cast<unsigned int>(tl_task_context_seed), {.stream = stream, .id = child_index});
    }
    const auto task_index = deterministic_task_counter().fetch_add(1, std::memory_order_relaxed);
    return rng_deterministic_seed_for({.stream = child_seed_stream ^ stream, .id = task_index});
}

rng_deterministic_task_scope::rng_deterministic_task_scope(const unsigned int seed)
    : old_has_task_engine_(tl_has_task_engine),
      old_task_engine_(tl_task_engine),
      old_has_task_context_(tl_has_task_context),
      old_task_context_seed_(tl_task_context_seed),
      old_task_child_counter_(tl_task_child_counter) {
    tl_has_task_engine = true;
    tl_task_engine.seed(seed == 0 ? 1u : seed);
    tl_has_task_context = true;
    tl_task_context_seed = seed;
    tl_task_child_counter = 0;
}

rng_deterministic_task_scope::~rng_deterministic_task_scope() {
    tl_has_task_engine = old_has_task_engine_;
    tl_task_engine = old_task_engine_;
    tl_has_task_context = old_has_task_context_;
    tl_task_context_seed = old_task_context_seed_;
    tl_task_child_counter = old_task_child_counter_;
}

auto rng_set_worker_seed(unsigned int seed) -> void {
    tl_is_worker = true;
    tl_worker_engine.seed(seed);
}

namespace weighted_list_detail {
unsigned int gen_rand_i() { return rng_bits(); }
} // namespace weighted_list_detail
