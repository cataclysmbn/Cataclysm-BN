#include "catch/catch.hpp"
#include "json_formatter.h"

#include <filesystem>
#include <fstream>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

namespace {

auto read_file(const std::filesystem::path& path) -> std::string {
    auto in = std::ifstream(path, std::ios::binary);
    auto text = std::ostringstream{};
    text << in.rdbuf();
    return text.str();
}

/// The files `style-json` formats: every data file but the name lists.
auto styled_data_files() -> std::vector<std::filesystem::path> {
    namespace ranges = std::ranges;
    using namespace std::views;
    const auto styled = [](const std::filesystem::directory_entry& entry) {
        return entry.is_regular_file() && entry.path().extension() == ".json"
            && !entry.path().string().starts_with("data/names/");
    };
    auto files =
        std::filesystem::recursive_directory_iterator("data") | filter(styled)
        | transform(&std::filesystem::directory_entry::path) | ranges::to<std::vector>();
    ranges::sort(files);
    return files;
}

} // namespace

TEST_CASE("every data file is already in the formatter's style", "[editor_document]") {
    const auto files = styled_data_files();
    REQUIRE(files.size() > 900);
    for (const auto& path : files) {
        CAPTURE(path.string());
        const auto text = read_file(path);
        const auto formatted = format_json(text);
        REQUIRE(formatted);
        CHECK(*formatted == text);
    }
}

TEST_CASE("the formatter writes data style and is a fixed point", "[editor_document]") {
    const auto loose = std::string(
        R"([{"type":"terrain","id":"t_x","name":"x \u00e9","rows":["ab","cd"],"weight":1.50,"n":-3}])");
    const auto formatted = format_json(loose);
    REQUIRE(formatted);
    CHECK(
        *formatted
        == "[\n  {\n    \"type\": \"terrain\",\n    \"id\": \"t_x\",\n    \"name\": \"x \\u00e9\",\n"
           "    \"rows\": [\n      \"ab\",\n      \"cd\"\n    ],\n    \"weight\": 1.5,\n    \"n\": -3\n  }\n]\n");
    CHECK(format_json(*formatted) == formatted);
}

TEST_CASE("the formatter names what it cannot read", "[editor_document]") {
    const auto broken = format_json(R"({"type": )");
    REQUIRE_FALSE(broken);
    CHECK_FALSE(broken.error().empty());
}
