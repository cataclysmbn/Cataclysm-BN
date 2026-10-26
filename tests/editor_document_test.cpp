#include "catch/catch.hpp"
#include "editor/document.h"
#include "json_formatter.h"

#include <filesystem>
#include <fstream>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

namespace {

using nlohmann::ordered_json;

auto read_file(const std::filesystem::path& path) -> std::string {
    auto in = std::ifstream(path, std::ios::binary);
    auto text = std::ostringstream{};
    text << in.rdbuf();
    return text.str();
}

auto write_file(const std::filesystem::path& path, const std::string& text) -> void {
    auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
    out << text;
}

auto data_files() -> std::vector<std::filesystem::path> {
    namespace ranges = std::ranges;
    using namespace std::views;
    const auto json = [](const std::filesystem::directory_entry& entry) {
        return entry.is_regular_file() && entry.path().extension() == ".json";
    };
    auto files =
        std::filesystem::recursive_directory_iterator("data") | filter(json)
        | transform(&std::filesystem::directory_entry::path) | ranges::to<std::vector>();
    ranges::sort(files);
    return files;
}

/// A scratch file holding `text`, removed when the test ends.
struct scratch_file {
    std::filesystem::path path;
    explicit scratch_file(const std::string& text)
        : path(std::filesystem::temp_directory_path() / "cata_editor_document_test.json") {
        write_file(path, text);
    }
    ~scratch_file() { std::filesystem::remove(path); }
};

const auto three_objects = std::string(R"([
  {
    "type": "terrain",
    "id": "t_one",
    "name": "a\tb"
  },
  {
    "type": "terrain",
    "id": "t_two",
    "weight": 1.50
  },
  {
    "type": "terrain",
    "id": "t_three"
  }
]
)");

} // namespace

TEST_CASE("every data file saves unchanged as the bytes it was opened from", "[editor_document]") {
    const auto files = data_files();
    REQUIRE(files.size() > 900);
    for (const auto& path : files) {
        CAPTURE(path.string());
        const auto opened = editor::document::open(path);
        REQUIRE(opened);
        CHECK_FALSE(opened->dirty());
        CHECK(opened->text() == read_file(path));
    }
}

TEST_CASE("an edit rewrites only its object, in the data style", "[editor_document]") {
    auto doc = editor::document::parse("three.json", three_objects);
    REQUIRE(doc);
    REQUIRE(doc->objects().size() == 3);
    const auto& second = doc->objects()[1];
    REQUIRE(
        doc->edit(1, ordered_json::parse(R"([{"op":"add","path":"/volume","value":0.00001}])")));
    const auto text = doc->text();
    CHECK(text.substr(0, second.begin) == three_objects.substr(0, second.begin));
    CHECK(text.substr(text.size() - (three_objects.size() - second.end))
          == three_objects.substr(second.end));
    CHECK(
        text
        == "[\n  {\n    \"type\": \"terrain\",\n    \"id\": \"t_one\",\n    \"name\": \"a\\tb\"\n  },\n"
           "  {\n    \"type\": \"terrain\",\n    \"id\": \"t_two\",\n    \"weight\": 1.5,\n"
           "    \"volume\": 0.00001\n  },\n  {\n    \"type\": \"terrain\",\n    \"id\": \"t_three\"\n  }\n]\n");
    CHECK(format_json(text) == text);
    CHECK(doc->dirty());
}

TEST_CASE("undo and redo restore the exact bytes", "[editor_document]") {
    auto doc = editor::document::parse("three.json", three_objects);
    REQUIRE(doc);
    REQUIRE(
        doc->edit(0, ordered_json::parse(R"([{"op":"replace","path":"/id","value":"t_first"}])")));
    REQUIRE(doc->edit(2, ordered_json::parse(R"([{"op":"remove","path":"/type"}])")));
    const auto edited = doc->text();
    CHECK(doc->undo());
    CHECK(doc->undo());
    CHECK_FALSE(doc->undo());
    CHECK(doc->text() == three_objects);
    CHECK_FALSE(doc->dirty());
    CHECK(doc->redo());
    CHECK(doc->redo());
    CHECK_FALSE(doc->redo());
    CHECK(doc->text() == edited);
}

TEST_CASE("edits of one object inside a group are one undo step", "[editor_document]") {
    auto doc = editor::document::parse("three.json", three_objects);
    REQUIRE(doc);
    const auto first = ordered_json::parse(R"([{"op":"replace","path":"/id","value":"t_first"}])");
    const auto second = ordered_json::parse(R"([{"op":"remove","path":"/type"}])");
    REQUIRE(doc->edit(0, first));
    doc->begin_group();
    REQUIRE(doc->edit(2, second));
    REQUIRE(doc->edit(2, ordered_json::parse(R"([{"op":"add","path":"/type","value":"x"}])")));
    // Another object ends the fold, and the group's next edit starts a step.
    REQUIRE(doc->edit(1, first));
    doc->end_group();
    REQUIRE(doc->edit(1, second));
    CHECK(doc->depth() == 4);
    const auto edited = doc->text();
    CHECK(doc->undo());
    CHECK(doc->undo());
    CHECK(doc->undo());
    CHECK(doc->undo());
    CHECK_FALSE(doc->undo());
    CHECK(doc->text() == three_objects);
    while (doc->redo()) {}
    CHECK(doc->text() == edited);
}

TEST_CASE(
    "a group folds a trail of edits into the one step, undone and redone whole",
    "[editor_document]") {
    auto doc = editor::document::parse("three.json", three_objects);
    REQUIRE(doc);
    doc->begin_group();
    REQUIRE(doc->edit(0, ordered_json::parse(R"([{"op":"replace","path":"/id","value":"a"}])")));
    REQUIRE(doc->edit(0, ordered_json::parse(R"([{"op":"replace","path":"/id","value":"b"}])")));
    REQUIRE(doc->edit(0, ordered_json::parse(R"([{"op":"replace","path":"/id","value":"c"}])")));
    doc->end_group();
    CHECK(doc->depth() == 1);
    CHECK(doc->history().front().edits.front().patch.size() == 3);
    const auto edited = doc->text();
    CHECK(doc->undo());
    CHECK(doc->text() == three_objects);
    CHECK(doc->redo());
    CHECK(doc->text() == edited);
    // A refused patch inside a group changes nothing.
    doc->begin_group();
    CHECK_FALSE(doc->edit(0, ordered_json::parse(R"([{"op":"remove","path":"/missing"}])")));
    doc->end_group();
    CHECK(doc->depth() == 1);
}

TEST_CASE("a refused patch changes nothing and leaves no undo step", "[editor_document]") {
    auto doc = editor::document::parse("three.json", three_objects);
    REQUIRE(doc);
    CHECK_FALSE(doc->edit(1, ordered_json::parse(R"([{"op":"remove","path":"/missing"}])")));
    CHECK_FALSE(doc->edit(3, ordered_json::parse(R"([])")));
    CHECK_FALSE(doc->undo());
    CHECK(doc->text() == three_objects);
}

TEST_CASE("an object naming a key twice is read-only", "[editor_document]") {
    auto doc = editor::document::
        parse("dup.json", "[\n  { \"id\": \"a\", \"id\": \"b\" },\n  { \"id\": \"c\" }\n]\n");
    REQUIRE(doc);
    CHECK(doc->objects()[0].duplicate_key == "id");
    CHECK_FALSE(doc->objects()[1].duplicate_key);
    const auto refused =
        doc->edit(0, ordered_json::parse(R"([{"op":"add","path":"/name","value":"x"}])"));
    REQUIRE_FALSE(refused);
    CHECK(refused.error().find("\"id\"") != std::string::npos);
    CHECK(doc->edit(1, ordered_json::parse(R"([{"op":"add","path":"/name","value":"x"}])")));
}

TEST_CASE("saving refuses a file changed on disk since it was opened", "[editor_document]") {
    const auto file = scratch_file(three_objects);
    auto doc = editor::document::open(file.path);
    REQUIRE(doc);
    REQUIRE(doc->edit(1, ordered_json::parse(R"([{"op":"replace","path":"/id","value":"t_2"}])")));
    REQUIRE(doc->save());
    CHECK(read_file(file.path) == doc->text());
    CHECK_FALSE(doc->dirty());

    const auto outside = std::string("[]\n");
    write_file(file.path, outside);
    REQUIRE(doc->edit(1, ordered_json::parse(R"([{"op":"replace","path":"/id","value":"t_3"}])")));
    const auto saved = doc->save();
    REQUIRE_FALSE(saved);
    CHECK(saved.error() == editor::save_error::changed_on_disk);
    CHECK(read_file(file.path) == outside);
}

TEST_CASE(
    "an object added in a group is written after the last one and undone with the group's edits",
    "[editor_document]") {
    auto doc = editor::document::parse("three.json", three_objects);
    REQUIRE(doc);
    doc->begin_group(true);
    REQUIRE(doc->edit(0, ordered_json::parse(R"([{"op":"replace","path":"/id","value":"t_1"}])")));
    const auto added = doc->add({{"type", "palette"}, {"id", "p"}});
    REQUIRE(added);
    CHECK(*added == 3);
    REQUIRE(doc->edit(2, ordered_json::parse(R"([{"op":"remove","path":"/type"}])")));
    doc->end_group();
    CHECK(doc->depth() == 1);
    CHECK(doc->history().front().edits.size() == 3);
    REQUIRE(doc->objects().size() == 4);
    CHECK(doc->objects()[3].value == ordered_json{{"type", "palette"}, {"id", "p"}});
    const auto edited = doc->text();
    // The bytes are what the data formatter writes, the unedited object as it was.
    CHECK(format_json(edited) == edited);
    CHECK(edited.find(R"("weight": 1.50)") != std::string::npos);
    CHECK(ordered_json::parse(edited)[3]["id"] == "p");
    CHECK(doc->undo());
    CHECK(doc->objects().size() == 3);
    CHECK(doc->text() == three_objects);
    CHECK(doc->redo());
    CHECK(doc->text() == edited);
}

TEST_CASE("an object is added to an empty file", "[editor_document]") {
    auto doc = editor::document::parse("empty.json", "[]\n");
    REQUIRE(doc);
    REQUIRE(doc->add({{"type", "palette"}, {"id", "p"}}));
    CHECK(doc->text() == format_json(doc->text()));
    CHECK(ordered_json::parse(doc->text())
          == ordered_json::parse(R"([{"type":"palette","id":"p"}])"));
    CHECK(doc->undo());
    CHECK(doc->text() == "[]\n");
}

TEST_CASE("a file of one value takes no other object", "[editor_document]") {
    auto doc = editor::document::parse("one.json", R"({"type": "terrain", "id": "t_one"})");
    REQUIRE(doc);
    CHECK_FALSE(doc->add({{"type", "palette"}, {"id", "p"}}));
    CHECK_FALSE(doc->can_undo());
}
