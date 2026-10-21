#include "catch/catch.hpp"
#include "editor/document.h"
#include "editor/mapgen_check.h"
#include "editor/mapgen_preview.h"
#include "editor/mapgen_stroke.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr auto runs = 20;

/// Milliseconds `run` takes after `prepare`, which is not timed, over `runs` runs.
auto timed(const std::function<void()>& prepare, const std::function<void()>& run)
    -> std::vector<double> {
    auto times = std::vector<double>{};
    for (auto index = 0; index < runs; ++index) {
        prepare();
        const auto start = std::chrono::steady_clock::now();
        run();
        times.push_back(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count());
    }
    std::ranges::sort(times);
    return times;
}

auto report(const std::string& name, const std::vector<double>& times, const double budget)
    -> void {
    const auto p95 = times[static_cast<std::size_t>(0.95 * (times.size() - 1))];
    std::cout << "editor-timing\t" << name << "\tp50=" << times[times.size() / 2] << "\tp95=" << p95
              << "\tmax=" << times.back() << "\tbudget=" << budget << '\n';
}

/// A stroke over `side` x `side` squares of object `object` of `path`, with the reload of the
/// object that shows it.
auto stroke_timing(const std::string& path, const std::size_t object, const int side)
    -> std::vector<double> {
    auto doc = std::optional<editor::document>{};
    auto symbol = std::string("x");
    return timed(
        [&] { doc = editor::document::open(path).value(); },
        [&] {
            REQUIRE(editor::stroke(
                *doc,
                {.object = object,
                 .drawn = {.kind = editor::shape_kind::rect_filled,
                           .from = {0, 0},
                           .to = {side - 1, side - 1}},
                 .symbol = symbol}));
            editor::check_mapgen(doc->objects()[object].value.dump());
        });
}

} // namespace

TEST_CASE("editor operation times against their budgets", "[.][editor_timing]") {
    const auto largest = std::string("data/json/mapgen/mall/mall_ground.json");
    REQUIRE(std::filesystem::file_size(largest) < 1024 * 1024);
    report("open " + largest, timed([] {}, [&] { REQUIRE(editor::document::open(largest)); }), 200);

    report("stroke 24x24", stroke_timing("data/json/mapgen/house/2storymodern06.json", 1, 24), 16);
    report("stroke 72x72", stroke_timing("data/json/mapgen/cground3x3.json", 0, 72), 50);

    const auto house = editor::document::open("data/json/mapgen/house/2storymodern06.json").value();
    const auto entry = std::ranges::find_if(house.objects(), [](const auto& object) {
        return object.value.value("om_terrain", "") == "2StoryModern06_1";
    });
    REQUIRE(entry != house.objects().end());
    const auto text = entry->value.dump();
    auto seed = 0U;
    report("preview one OMT",
           timed([] {},
                 [&] {
                     REQUIRE(editor::preview_mapgen(
                         text, {.at = tripoint_abs_omt(40, 40, 0), .seed = ++seed}));
                 }),
           100);

    const auto scratch = std::filesystem::temp_directory_path() / "cata_editor_timing.json";
    auto doc = std::optional<editor::document>{};
    report(
        "save " + largest,
        timed(
            [&] {
                std::filesystem::
                    copy_file(largest, scratch, std::filesystem::copy_options::overwrite_existing);
                doc = editor::document::open(scratch).value();
                REQUIRE(editor::stroke(*doc, {.drawn = {.from = {0, 0}}, .symbol = "x"}));
            },
            [&] { REQUIRE(doc->save()); }),
        100);
    std::filesystem::remove(scratch);
}
