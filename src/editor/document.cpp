#include "editor/document.h"

#include "editor/mapgen_check.h"
#include "json.h"
#include "json_formatter.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <ranges>
#include <set>
#include <sstream>

namespace editor {
namespace {

using ordered_json = nlohmann::ordered_json;

/// The end of a value whose reading stopped at `stop`, past the blanks and the comma that follow
/// it.
auto value_end(const std::string& text, const std::size_t begin, std::size_t stop) -> std::size_t {
    while (stop > begin
           && (std::isspace(static_cast<unsigned char>(text[stop - 1])) || text[stop - 1] == ',')) {
        --stop;
    }
    return stop;
}

struct parsed_object {
    ordered_json value;
    std::optional<std::string> duplicate_key;
};

auto parse_object(const std::string_view text) -> parsed_object {
    auto keys = std::vector<std::set<std::string>>{};
    auto duplicate = std::optional<std::string>{};
    const auto note_key =
        [&](const int /*depth*/, const ordered_json::parse_event_t event, ordered_json& parsed) {
            if (event == ordered_json::parse_event_t::object_start) {
                keys.emplace_back();
            } else if (event == ordered_json::parse_event_t::object_end) {
                keys.pop_back();
            } else if (event == ordered_json::parse_event_t::key
                       && !keys.back().insert(parsed.get<std::string>()).second && !duplicate) {
                duplicate = parsed.get<std::string>();
            }
            return true;
        };
    auto value = ordered_json::parse(text, note_key);
    return {.value = std::move(value), .duplicate_key = std::move(duplicate)};
}

/// JSON text the data formatter reads as the value: a float always has a decimal point, never an
/// exponent.
auto write_plain(const ordered_json& value, std::string& out) -> void {
    if (value.is_object()) {
        out += '{';
        auto first = true;
        for (const auto& [key, member] : value.items()) {
            if (!first) { out += ','; }
            first = false;
            out += ordered_json(key).dump();
            out += ':';
            write_plain(member, out);
        }
        out += '}';
    } else if (value.is_array()) {
        out += '[';
        auto first = true;
        for (const auto& entry : value) {
            if (!first) { out += ','; }
            first = false;
            write_plain(entry, out);
        }
        out += ']';
    } else if (value.is_number_float()) {
        auto digits = std::array<char, 400>{};
        const auto written = std::to_chars(
            digits.data(), digits.data() + digits.size(), value.get<double>(),
            std::chars_format::fixed);
        auto number = std::string(digits.data(), written.ptr);
        if (number.find('.') == std::string::npos) { number += ".0"; }
        out += number;
    } else {
        out += value.dump();
    }
}

/// An edited value as it is written in place of its span: in the data style, at the depth of a
/// file's entry when `listed`, without the indent before its first line and the newline after its
/// last.
auto render(const ordered_json& value, const bool listed) -> std::string {
    auto plain = std::string{};
    if (listed) { plain += '['; }
    write_plain(value, plain);
    if (listed) { plain += ']'; }
    auto formatted = format_json(plain).value_or(value.dump());
    if (!listed) { return formatted.substr(0, formatted.size() - 1); }
    // "[\n  <value>\n]\n"
    constexpr auto opening = std::string_view("[\n  ");
    constexpr auto closing = std::string_view("\n]\n");
    return formatted.substr(opening.size(), formatted.size() - opening.size() - closing.size());
}

auto read_text(const std::filesystem::path& path) -> std::optional<std::string> {
    auto in = std::ifstream(path, std::ios::binary);
    if (!in) { return std::nullopt; }
    auto text = std::ostringstream{};
    text << in.rdbuf();
    return text.str();
}

/// Writes `bytes` through a temporary file beside `path`, so a failed write leaves the file as it
/// was.
auto write_text(const std::filesystem::path& path, const std::string& bytes) -> bool {
    const auto temporary = std::filesystem::path(path).concat(".editor-save");
    {
        auto out = std::ofstream(temporary, std::ios::binary | std::ios::trunc);
        out << bytes;
        if (!out.flush()) { return false; }
    }
    auto renamed = std::error_code{};
    std::filesystem::rename(temporary, path, renamed);
    return !renamed;
}

} // namespace

auto document::open(const std::filesystem::path& path) -> std::expected<document, std::string> {
    auto text = read_text(path);
    if (!text) { return std::unexpected("cannot read " + path.string()); }
    return parse(path, std::move(*text));
}

auto document::parse(const std::filesystem::path& path, std::string text)
    -> std::expected<document, std::string> {
    auto result = document{};
    result.path_ = path;
    result.original_ = std::move(text);
    result.saved_ = result.original_;
    const auto& original = result.original_;
    auto spans = std::vector<std::pair<std::size_t, std::size_t>>{};
    try {
        auto in = std::istringstream(original);
        auto jsin = JsonIn(in);
        jsin.eat_whitespace();
        result.listed_ = jsin.test_array();
        const auto span_of_next = [&] {
            jsin.eat_whitespace();
            const auto begin = static_cast<std::size_t>(jsin.tell());
            jsin.skip_value();
            spans.emplace_back(
                begin, value_end(original, begin, static_cast<std::size_t>(jsin.tell())));
        };
        if (result.listed_) {
            jsin.start_array();
            while (!jsin.end_array()) { span_of_next(); }
        } else {
            span_of_next();
        }
    } catch (const JsonError& e) { return std::unexpected(plain_loader_text(e.what())); }
    try {
        for (const auto& [begin, end] : spans) {
            auto parsed = parse_object(std::string_view(original).substr(begin, end - begin));
            result.opened_.push_back(parsed.value);
            result.objects_.push_back(
                {.begin = begin,
                 .end = end,
                 .value = std::move(parsed.value),
                 .duplicate_key = std::move(parsed.duplicate_key)});
        }
    } catch (const ordered_json::exception& e) { return std::unexpected(std::string(e.what())); }
    return result;
}

auto document::edit(const std::size_t object, const nlohmann::ordered_json& patch)
    -> std::expected<void, std::string> {
    if (object >= objects_.size()) {
        return std::unexpected("no object " + std::to_string(object));
    }
    auto& target = objects_[object];
    if (target.duplicate_key) {
        return std::unexpected("the key \"" + *target.duplicate_key + "\" appears twice");
    }
    try {
        auto next = target.value.patch(patch);
        record({.object = object, .patch = patch, .before = target.value, .after = next});
        target.value = std::move(next);
    } catch (const ordered_json::exception& e) { return std::unexpected(std::string(e.what())); }
    return {};
}

auto document::add(nlohmann::ordered_json value) -> std::expected<std::size_t, std::string> {
    if (!listed_) { return std::unexpected(std::string("the file holds one value, not a list")); }
    const auto index = objects_.size();
    record(
        {.object = index,
         .patch = ordered_json::array({{{"op", "add"}, {"path", ""}, {"value", value}}}),
         .before = nullptr,
         .after = value});
    objects_.push_back({.begin = std::string::npos, .end = std::string::npos, .value = value});
    return index;
}

auto document::record(document_edit edit) -> void {
    auto* const step = grouping_ && grouped_ ? &done_.back() : nullptr;
    auto* const last = step ? &step->edits.back() : nullptr;
    if (last && last->object == edit.object && !edit.before.is_null()) {
        last->patch.insert(last->patch.end(), edit.patch.begin(), edit.patch.end());
        last->after = std::move(edit.after);
    } else if (step && across_) {
        step->edits.push_back(std::move(edit));
    } else {
        done_.push_back({.edits = {std::move(edit)}});
        grouped_ = grouping_;
    }
    undone_.clear();
    if (saved_depth_ && *saved_depth_ >= done_.size()) { saved_depth_.reset(); }
}

auto document::undo() -> bool {
    if (done_.empty()) { return false; }
    // The step a group folds into is no longer the last done.
    end_group();
    auto step = std::move(done_.back());
    done_.pop_back();
    for (const auto& edit : step.edits | std::views::reverse) {
        if (edit.before.is_null()) {
            // An added object is the last one while its step is undone.
            objects_.pop_back();
        } else {
            objects_[edit.object].value = edit.before;
        }
    }
    undone_.push_back(std::move(step));
    return true;
}

auto document::redo() -> bool {
    if (undone_.empty()) { return false; }
    end_group();
    auto step = std::move(undone_.back());
    undone_.pop_back();
    for (const auto& edit : step.edits) {
        if (edit.before.is_null()) {
            objects_.push_back(
                {.begin = std::string::npos, .end = std::string::npos, .value = edit.after});
        } else {
            objects_[edit.object].value = edit.after;
        }
    }
    done_.push_back(std::move(step));
    return true;
}

auto document::history() const -> std::vector<document_step> {
    auto steps = done_;
    steps.insert(steps.end(), undone_.rbegin(), undone_.rend());
    return steps;
}

auto document::jump(const std::size_t depth) -> bool {
    if (depth > done_.size() + undone_.size()) { return false; }
    while (done_.size() > depth) { undo(); }
    while (done_.size() < depth) { redo(); }
    return true;
}

auto document::text() const -> std::string {
    const auto opened = opened_.size();
    const auto edited = [&](const std::size_t index) {
        return objects_[index].value != opened_[index];
    };
    if (objects_.size() == opened
        && std::ranges::none_of(std::views::iota(std::size_t{0}, opened), edited)) {
        return original_;
    }
    auto out = std::string{};
    auto copied = std::size_t{0};
    for (const auto index : std::views::iota(std::size_t{0}, opened)) {
        const auto& entry = objects_[index];
        out.append(original_, copied, entry.begin - copied);
        if (edited(index)) {
            out += render(entry.value, listed_);
        } else {
            out.append(original_, entry.begin, entry.end - entry.begin);
        }
        copied = entry.end;
    }
    if (objects_.size() > opened) {
        // Added objects follow the last one, or open the empty list, in the data style.
        if (opened == 0) {
            copied = original_.find('[') + 1;
            out.append(original_, 0, copied);
            copied = original_.find(']', copied);
        }
        for (const auto index : std::views::iota(opened, objects_.size())) {
            out += index == 0 ? "\n  " : ",\n  ";
            out += render(objects_[index].value, listed_);
        }
        if (opened == 0) { out += '\n'; }
    }
    out.append(original_, copied);
    return out;
}

auto document::changed_on_disk() const -> bool {
    return read_text(path_).value_or(std::string{}) != saved_;
}

auto document::save() -> std::expected<void, save_error> {
    if (changed_on_disk()) { return std::unexpected(save_error::changed_on_disk); }
    auto bytes = text();
    if (!write_text(path_, bytes)) { return std::unexpected(save_error::unwritable); }
    saved_ = std::move(bytes);
    saved_depth_ = done_.size();
    return {};
}

auto document::save_as(const std::filesystem::path& path) -> std::expected<void, save_error> {
    if (std::filesystem::exists(path)) { return std::unexpected(save_error::exists); }
    auto made = std::error_code{};
    std::filesystem::create_directories(path.parent_path(), made);
    auto bytes = text();
    if (made || !write_text(path, bytes)) { return std::unexpected(save_error::unwritable); }
    path_ = path;
    saved_ = std::move(bytes);
    saved_depth_ = done_.size();
    return {};
}

} // namespace editor
