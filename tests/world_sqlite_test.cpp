#include "avatar.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "filesystem.h"
#include "game.h"
#include "thread_pool.h"
#include "world.h"

#include <algorithm>
#include <atomic>
#include <istream>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <string>

namespace {

auto concurrent_test_omt(const int index) -> tripoint_abs_omt {
    return {10000 + index, 20000 + index, 0};
}

const auto dimension_save_omt = tripoint_abs_omt(31000, 32000, 0);
const auto dimension_save_om = point_abs_om(33000, 34000);
const auto dimension_save_mmr = tripoint_abs_mmr::zero();

auto write_empty_json(std::ostream& out) -> void { out << "[]"; }

auto read_empty_json(JsonIn& jsin) -> void {
    jsin.start_array();
    jsin.end_array();
}

auto write_text(std::ostream& out) -> void { out << "data"; }

auto read_text(std::istream& in) -> void {
    auto value = std::string{};
    in >> value;
}

auto dimension_data_file(const std::string& dim_id) -> std::string {
    return "dimension_data_" + dim_id + ".gsav";
}

auto assure_legacy_dimension_dirs(world& w, const std::string& dim_id) -> void {
    const auto segment_addr = project_to<coords::seg>(dimension_save_omt);
    const auto segment_dir =
        "dimensions/" + dim_id + "/maps/" + std::to_string(segment_addr.x()) + "."
        + std::to_string(segment_addr.y()) + "." + std::to_string(segment_addr.z());
    const auto save_id = base64_encode(g->u.get_save_id());

    REQUIRE(w.assure_dir_exist("dimensions"));
    REQUIRE(w.assure_dir_exist("dimensions/" + dim_id));
    REQUIRE(w.assure_dir_exist("dimensions/" + dim_id + "/maps"));
    REQUIRE(w.assure_dir_exist(segment_dir));
    REQUIRE(w.assure_dir_exist(save_id + "dimensions"));
    REQUIRE(w.assure_dir_exist(save_id + "dimensions/" + dim_id));
    REQUIRE(w.assure_dir_exist(save_id + ".mm1"));
    REQUIRE(w.assure_dir_exist(save_id + ".mm1/dimensions"));
    REQUIRE(w.assure_dir_exist(save_id + ".mm1/dimensions/" + dim_id));
}

auto write_player_dimension_records(world& w, const std::string& dim_id) -> void {
    REQUIRE(w.write_overmap_player_visibility(dim_id, dimension_save_om, write_text));
    REQUIRE(w.write_player_mm_omt(dim_id, dimension_save_mmr, write_empty_json));
}

auto write_dimension_save_records(world& w, const std::string& dim_id) -> void {
    if (w.info->world_save_format == save_format::V1) { assure_legacy_dimension_dirs(w, dim_id); }
    REQUIRE(w.write_map_omt(dim_id, dimension_save_omt, write_empty_json));
    REQUIRE(w.write_overmap(dim_id, dimension_save_om, write_text));
    write_player_dimension_records(w, dim_id);
    REQUIRE(w.write_to_file(dimension_data_file(dim_id), write_empty_json));
}

auto check_player_dimension_records_exist(world& w, const std::string& dim_id) -> void {
    CHECK(w.read_overmap_player_visibility(dim_id, dimension_save_om, read_text));
    CHECK(w.read_player_mm_omt(dim_id, dimension_save_mmr, read_empty_json));
}

auto check_player_dimension_records_missing(world& w, const std::string& dim_id) -> void {
    CHECK_FALSE(w.read_overmap_player_visibility(dim_id, dimension_save_om, read_text));
    CHECK_FALSE(w.read_player_mm_omt(dim_id, dimension_save_mmr, read_empty_json));
}

auto check_dimension_save_records_exist(world& w, const std::string& dim_id) -> void {
    CHECK(w.read_map_omt(dim_id, dimension_save_omt, read_empty_json));
    CHECK(w.read_overmap(dim_id, dimension_save_om, read_text));
    check_player_dimension_records_exist(w, dim_id);
    CHECK(w.file_exist(dimension_data_file(dim_id)));
}

auto check_dimension_save_records_missing(world& w, const std::string& dim_id) -> void {
    CHECK_FALSE(w.read_map_omt(dim_id, dimension_save_omt, read_empty_json));
    CHECK_FALSE(w.read_overmap(dim_id, dimension_save_om, read_text));
    check_player_dimension_records_missing(w, dim_id);
    CHECK_FALSE(w.file_exist(dimension_data_file(dim_id)));
}

} // namespace

TEST_CASE("sqlite map database accepts concurrent map writes", "[world][sqlite]") {
    auto* const w = g->get_active_world();
    REQUIRE(w != nullptr);
    REQUIRE(w->info->world_save_format == save_format::V2_COMPRESSED_SQLITE3);

    static constexpr auto write_count = 64;
    const auto dim_id = "sqlite_concurrent_" + get_pid_string();

    parallel_for(0, write_count, [&](const auto i) {
        const auto omt_addr = concurrent_test_omt(i);
        if (!w->write_map_omt(dim_id, omt_addr, [](std::ostream& out) { out << "[]"; })) {
            throw std::runtime_error("failed to write sqlite map omt");
        }
    });

    const auto all_written =
        std::ranges::all_of(std::views::iota(0, write_count), [&](const auto i) {
            const auto omt_addr = concurrent_test_omt(i);
            return w->read_map_omt(dim_id, omt_addr, [](JsonIn& jsin) {
                jsin.start_array();
                jsin.end_array();
            });
        });
    CHECK(all_written);
}

TEST_CASE("delete_dimension_data rejects unsafe dimension ids", "[world]") {
    auto* const w = g->get_active_world();
    REQUIRE(w != nullptr);

    CHECK_FALSE(w->delete_dimension_data(""));
    CHECK_FALSE(w->delete_dimension_data("."));
    CHECK_FALSE(w->delete_dimension_data(".."));
    CHECK_FALSE(w->delete_dimension_data("lua/test"));
    CHECK_FALSE(w->delete_dimension_data("lua\\test"));
}

TEST_CASE("delete_dimension_data removes sqlite dimension save data", "[world][sqlite]") {
    auto* const w = g->get_active_world();
    REQUIRE(w != nullptr);
    REQUIRE(w->info->world_save_format == save_format::V2_COMPRESSED_SQLITE3);
    const auto dim_id = "sqlite_delete_dimension_" + get_pid_string();
    const auto sibling_dim_id = dim_id + "_extra";
    const auto original_save_id = g->u.get_save_id();
    const auto other_save_id = "sqlite_dimension_other_player_" + get_pid_string();
    const auto original_world_saves = w->info->world_saves;
    const auto other_db_path =
        w->info->folder_path() + "/" + base64_encode(other_save_id) + ".sqlite3";
    const auto restore_player = on_out_of_scope([&]() {
        w->release_player_db();
        g->u.set_save_id(original_save_id);
        w->info->world_saves = original_world_saves;
        if (file_exist(other_db_path)) { remove_file(other_db_path); }
    });
    w->info->add_save(save_t::from_save_id(original_save_id));
    w->info->add_save(save_t::from_save_id(other_save_id));

    write_dimension_save_records(*w, dim_id);
    write_dimension_save_records(*w, sibling_dim_id);
    check_dimension_save_records_exist(*w, dim_id);
    check_dimension_save_records_exist(*w, sibling_dim_id);

    w->release_player_db();
    g->u.set_save_id(other_save_id);
    write_player_dimension_records(*w, dim_id);
    write_player_dimension_records(*w, sibling_dim_id);
    check_player_dimension_records_exist(*w, dim_id);
    check_player_dimension_records_exist(*w, sibling_dim_id);

    w->release_player_db();
    g->u.set_save_id(original_save_id);
    CHECK(w->has_dimension_data(dim_id));
    REQUIRE(w->delete_dimension_data(dim_id));
    check_dimension_save_records_missing(*w, dim_id);
    check_dimension_save_records_exist(*w, sibling_dim_id);

    w->release_player_db();
    g->u.set_save_id(other_save_id);
    check_player_dimension_records_missing(*w, dim_id);
    check_player_dimension_records_exist(*w, sibling_dim_id);

    w->release_player_db();
    g->u.set_save_id(original_save_id);
    REQUIRE(w->delete_dimension_data(sibling_dim_id));
}

TEST_CASE("delete_dimension_data removes legacy dimension save data", "[world]") {
    auto* const w = g->get_active_world();
    REQUIRE(w != nullptr);
    const auto original_format = w->info->world_save_format;
    const auto restore_format = on_out_of_scope([&]() {
        w->info->world_save_format = original_format;
    });
    w->info->world_save_format = save_format::V1;
    const auto dim_id = "legacy_delete_dimension_" + get_pid_string();
    const auto sibling_dim_id = dim_id + "_extra";

    write_dimension_save_records(*w, dim_id);
    write_dimension_save_records(*w, sibling_dim_id);
    check_dimension_save_records_exist(*w, dim_id);
    check_dimension_save_records_exist(*w, sibling_dim_id);

    CHECK(w->has_dimension_data(dim_id));
    REQUIRE(w->delete_dimension_data(dim_id));
    check_dimension_save_records_missing(*w, dim_id);
    check_dimension_save_records_exist(*w, sibling_dim_id);
    REQUIRE(w->delete_dimension_data(sibling_dim_id));
}

TEST_CASE("sqlite map database accepts concurrent map reads", "[world][sqlite]") {
    auto* const w = g->get_active_world();
    REQUIRE(w != nullptr);
    REQUIRE(w->info->world_save_format == save_format::V2_COMPRESSED_SQLITE3);

    static constexpr auto read_count = 64;
    const auto dim_id = "sqlite_concurrent_reads_" + get_pid_string();

    std::ranges::for_each(std::views::iota(0, read_count), [&](const auto i) {
        const auto omt_addr = concurrent_test_omt(i + read_count);
        REQUIRE(w->write_map_omt(dim_id, omt_addr, [](std::ostream& out) { out << "[]"; }));
    });

    auto all_read = std::atomic_bool{true};
    const auto read_empty_array = [](JsonIn& jsin) {
        jsin.start_array();
        jsin.end_array();
    };
    parallel_for(0, read_count, [&](const auto i) {
        const auto omt_addr = concurrent_test_omt(i + read_count);
        if (!w->read_map_omt(dim_id, omt_addr, read_empty_array)) {
            all_read.store(false, std::memory_order_relaxed);
        }
    });

    CHECK(all_read.load(std::memory_order_relaxed));
}
