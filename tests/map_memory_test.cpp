#include "../src/map/map.h"
#include "cata_dynamic_bitset.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game_constants.h"
#include "json.h"
#include "lru_cache.h"
#include "map_memory.h"
#include "string_formatter.h"

#include <cstdio>
#include <sstream>
#include <string>

static constexpr tripoint_abs_ms p1{-SEEX - 2, -SEEY - 3, -1};
static constexpr tripoint_abs_ms p2{5, 7, -1};
static constexpr tripoint_abs_ms p3{SEEX * 2 + 5, SEEY + 7, -1};
static constexpr tripoint_abs_ms p4{SEEX * 3 + 2, SEEY * 7 + 1, -1};

TEST_CASE("map_memory_keeps_region", "[map_memory]") {
    map_memory memory;
    CHECK(memory.prepare_region(p1, p2));
    CHECK(!memory.prepare_region(p1, p2));
    CHECK(!memory.prepare_region(p1 + tripoint_east, p2 + tripoint_east));
    CHECK(memory.prepare_region(p2, p3));
    CHECK(memory.prepare_region(p1, p3));
    CHECK(!memory.prepare_region(p1, p3));
    CHECK(!memory.prepare_region(p2, p3));
    CHECK(memory.prepare_region(p1, p4));
    CHECK(!memory.prepare_region(p2, p3));
    CHECK(!memory.prepare_region(
        tripoint_abs_ms(p2.xy(), -p2.z()), tripoint_abs_ms(p3.xy(), -p3.z())));
}

TEST_CASE("map_memory_defaults", "[map_memory]") {
    map_memory memory;
    memory.prepare_region(p1, p2);
    CHECK(memory.get_symbol(p1) == 0);
    memorized_terrain_tile default_tile = memory.get_tile(p1);
    CHECK(default_tile.tile.empty());
    CHECK(default_tile.subtile == 0);
    CHECK(default_tile.rotation == 0);
}

TEST_CASE("map_memory_remembers", "[map_memory]") {
    map_memory memory;
    memory.prepare_region(p1, p2);
    memory.memorize_symbol(p1, 1);
    memory.memorize_symbol(p2, 2);
    CHECK(memory.get_symbol(p1) == 1);
    CHECK(memory.get_symbol(p2) == 2);
}

TEST_CASE("map_memory_overwrites", "[map_memory]") {
    map_memory memory;
    memory.prepare_region(p1, p2);
    memory.memorize_symbol(p1, 1);
    memory.memorize_symbol(p2, 2);
    memory.memorize_symbol(p2, 3);
    CHECK(memory.get_symbol(p1) == 1);
    CHECK(memory.get_symbol(p2) == 3);
}

TEST_CASE("map_memory_forgets", "[map_memory]") {
    map_memory memory;
    memory.memorize_symbol(tripoint_abs_ms::zero(), 1);
    memory.memorize_symbol(p3, 1);
}

// TODO: map memory save / load

#include <chrono>

TEST_CASE("lru_cache_perf", "[.]") {
    constexpr int max_size = 1000000;
    lru_cache<tripoint, int> symbol_cache;
    const auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; ++i) {
        for (int j = -60; j <= 60; ++j) { symbol_cache.insert(max_size, {i, j, 0}, 1); }
    }
    const auto end1 = std::chrono::high_resolution_clock::now();
    const long long diff1 =
        std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1).count();
    cata_printf("completed %d insertions in %lld microseconds.\n", max_size, diff1);
    /*
     * Original tripoint_bub_ms hash    completed 1000000 insertions in 96136925 microseconds.
     * Table based interleave v1 completed 1000000 insertions in 41435604 microseconds.
     * Table based interleave v2 completed 1000000 insertions in 40856530 microseconds.
     * Jbtw hash                 completed 1000000 insertions in 19049163 microseconds.
     *                                                     rerun 21152804
     * With 1024 batch           completed 1000000 insertions in 39902325 microseconds.
     * backed out batching       completed 1000000 insertions in 20332498 microseconds.
     * rerun                     completed 1000000 insertions in 21659107 microseconds.
     * simple batching, disabled completed 1000000 insertions in 18541486 microseconds.
     * simple batching, 1024     completed 1000000 insertions in 23102395 microseconds.
     * rerun                     completed 1000000 insertions in 31337290 microseconds.
     */
}

// There are 4 quadrants we want to check,
// 1 | 2
// -----
// 3 | 4
// The partitions are defined by partition.x and partition.y
// Each partition has an expected value, and should be homogenous.
static void check_quadrants(
    cata_dynamic_bitset& test_cache, const point& partition, bool first_val, bool second_val,
    bool third_val, bool fourth_val) {
    int y = 0;
    for (; y < partition.y; ++y) {
        size_t y_offset = y * g_mapsize_x;
        int x = 0;
        for (; x < partition.x; ++x) {
            INFO(x << " " << y);
            CHECK(first_val == test_cache[y_offset + x]);
        }
        for (; x < g_mapsize_x; ++x) {
            INFO(x << " " << y);
            CHECK(second_val == test_cache[y_offset + x]);
        }
    }
    for (; y < g_mapsize_y; ++y) {
        size_t y_offset = y * g_mapsize_x;
        int x = 0;
        for (; x < partition.x; ++x) {
            INFO(x << " " << y);
            CHECK(third_val == test_cache[y_offset + x]);
        }
        for (; x < g_mapsize_x; ++x) {
            INFO(x << " " << y);
            CHECK(fourth_val == test_cache[y_offset + x]);
        }
    }
}

static constexpr size_t first_twelve = SEEX;

TEST_CASE("shift_map_memory_seen_cache") {
    // last_twelve is runtime because g_mapsize_x depends on the active bubble size.
    const size_t last_twelve = g_mapsize_x - SEEX;
    cata_dynamic_bitset test_cache(g_mapsize_x * g_mapsize_y);

    GIVEN("all bits are set") {
        test_cache.set();
        WHEN("positive x shift") {
            shift_bitset_cache(test_cache, g_mapsize_x, SEEX, point_rel_sm::east());
            THEN("last 12 columns are 0, rest are 1") {
                check_quadrants(test_cache, point(last_twelve, 0), true, false, true, false);
            }
        }
        WHEN("negative x shift") {
            shift_bitset_cache(test_cache, g_mapsize_x, SEEX, point_rel_sm::west());
            THEN("first 12 columns are 0, rest are 1") {
                check_quadrants(test_cache, point(first_twelve, 0), false, true, false, true);
            }
        }
        WHEN("positive y shift") {
            shift_bitset_cache(test_cache, g_mapsize_x, SEEX, point_rel_sm::south());
            THEN("last 12 rows are 0, rest are 1") {
                check_quadrants(test_cache, point(0, last_twelve), true, true, false, false);
            }
        }
        WHEN("negative y shift") {
            shift_bitset_cache(test_cache, g_mapsize_x, SEEX, point_rel_sm::north());
            THEN("first 12 rows are 0, rest are 1") {
                check_quadrants(test_cache, point(0, first_twelve), false, false, true, true);
            }
        }
        WHEN("positive x, positive y shift") {
            shift_bitset_cache(test_cache, g_mapsize_x, SEEX, point_rel_sm::south_east());
            THEN("last 12 columns and rows are 0, rest are 1") {
                check_quadrants(
                    test_cache, point(last_twelve, last_twelve), true, false, false, false);
            }
        }
        WHEN("positive x, negative y shift") {
            shift_bitset_cache(test_cache, g_mapsize_x, SEEX, point_rel_sm::north_east());
            THEN("last 12 columns and first 12 rows are 0, rest are 1") {
                check_quadrants(
                    test_cache, point(last_twelve, first_twelve), false, false, true, false);
            }
        }
        WHEN("negative x, positive y shift") {
            shift_bitset_cache(test_cache, g_mapsize_x, SEEX, point_rel_sm::south_west());
            THEN("first 12 columns and last 12 rows are 0, rest are 1") {
                check_quadrants(
                    test_cache, point(first_twelve, last_twelve), false, true, false, false);
            }
        }
        WHEN("negative x, negative y shift") {
            shift_bitset_cache(test_cache, g_mapsize_x, SEEX, point_rel_sm::north_west());
            THEN("first 12 columns and rows are 0, rest are 1") {
                check_quadrants(
                    test_cache, point(first_twelve, first_twelve), false, false, false, true);
            }
        }
    }
}

TEST_CASE("map_memory_revision_changes_on_every_way_memory_changes", "[map_memory][revision]") {
    map_memory memory;
    const auto changes = [&](const char* what, const auto& change) {
        CAPTURE(what);
        const auto before = memory.revision();
        change();
        CHECK(memory.revision() != before);
    };
    const auto pos = p2;
    changes("memorize_tile", [&] { memory.memorize_tile(pos, "f_bench", 0, 0); });
    changes("memorize_terrain_tile", [&] { memory.memorize_terrain_tile(pos, "t_floor", 0, 0); });
    changes("memorize_symbol", [&] { memory.memorize_symbol(pos, 'x'); });
    changes("clear_memorized_overlay", [&] { memory.clear_memorized_overlay(pos); });
    changes("clear_memorized_tile", [&] { memory.clear_memorized_tile(pos); });
    changes("clear", [&] { memory.clear(); });
    changes("load", [&] { memory.load(pos); });
    changes("save", [&] { memory.save(pos); });
    CHECK(map_memory{}.revision() != map_memory{}.revision());
}

TEST_CASE("map_memory_revision_ignores_reads", "[map_memory][revision]") {
    map_memory memory;
    memory.memorize_tile(p2, "f_bench", 0, 0);
    const auto before = memory.revision();
    CHECK(memory.get_tile(p2).tile == "f_bench");
    CHECK(memory.get_symbol(p2) == 0);
    CHECK(memory.get_terrain_tile(p2).tile.empty());
    CHECK(memory.memorized_positions().size() == 1);
    CHECK(memory.has_memory_for_autodrive(p2));
    CHECK(memory.revision() == before);
}
