#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace editor {

/// One top-level value of a data file: an entry of its array, or the file's only value.
struct document_object {
    /// The bytes it was read from; written back as they were until it is edited.
    std::size_t begin = 0;
    std::size_t end = 0;
    nlohmann::ordered_json value;
    /// A key it names twice: such an object is read-only, since a parser keeps only one of the two.
    std::optional<std::string> duplicate_key = std::nullopt;
};

/// One step of the history: an RFC 6902 patch of one object, with the object before and after it.
/// Undo restores the value itself, since an inverse patch would put a removed member back at the
/// end.
struct document_edit {
    std::size_t object = 0;
    nlohmann::ordered_json patch;
    nlohmann::ordered_json before;
    nlohmann::ordered_json after;
};

enum class save_error {
    /// The file on disk is not what was opened, so saving would lose what changed it.
    changed_on_disk,
    /// It could not be written.
    unwritable,
    /// Saving as a file that already exists would overwrite it.
    exists,
};

/// A data file opened for editing. Saving an unchanged document writes the bytes it was opened
/// from; an edited object is written in the data style, and every other byte stays as it was.
class document {
public:
    static auto open(const std::filesystem::path& path) -> std::expected<document, std::string>;
    /// The same from text, as if read from `path`.
    static auto parse(const std::filesystem::path& path, std::string text)
        -> std::expected<document, std::string>;

    auto path() const -> const std::filesystem::path& { return path_; }              // *NOPAD*
    auto objects() const -> const std::vector<document_object>& { return objects_; } // *NOPAD*
    /// Whether the text differs from what was opened or last saved.
    auto dirty() const -> bool { return text() != saved_; }
    /// Whether the file on disk differs from what was opened or last saved.
    auto changed_on_disk() const -> bool;

    /// Applies an RFC 6902 patch to one object as one undo step; the error names what refused it.
    auto edit(std::size_t object, const nlohmann::ordered_json& patch)
        -> std::expected<void, std::string>;
    /// Until `end_group`, the edits of one object fold into one undo step.
    auto begin_group() -> void {
        grouping_ = true;
        grouped_ = false;
    }
    auto end_group() -> void { grouping_ = false; }
    auto undo() -> bool;
    auto redo() -> bool;
    auto can_undo() const -> bool { return !done_.empty(); }
    auto can_redo() const -> bool { return !undone_.empty(); }
    /// The steps done, oldest first, then the steps undone, the next redo first.
    auto history() const -> std::vector<document_edit>;
    /// How many steps are done now.
    auto depth() const -> std::size_t { return done_.size(); }
    /// The depth the file was opened or last saved at; none once an edit after an undo has left it.
    auto saved_depth() const -> std::optional<std::size_t> { return saved_depth_; }
    /// Undoes or redoes until `depth` steps are done; false for a depth the history does not reach.
    auto jump(std::size_t depth) -> bool;

    /// The bytes saving writes now.
    auto text() const -> std::string;
    auto save() -> std::expected<void, save_error>;
    /// Writes the bytes to a new file at `path`, which the document is from then on.
    auto save_as(const std::filesystem::path& path) -> std::expected<void, save_error>;

private:
    std::filesystem::path path_;
    /// The bytes opened, which the spans index.
    std::string original_;
    /// The bytes on disk as this document last read or wrote them.
    std::string saved_;
    /// Whether the file's values form an array, rather than being one value.
    bool listed_ = true;
    std::vector<document_object> objects_;
    /// The values the objects were opened with; an object equal to its own is written from its
    /// span.
    std::vector<nlohmann::ordered_json> opened_;
    std::vector<document_edit> done_;
    std::vector<document_edit> undone_;
    std::optional<std::size_t> saved_depth_ = 0;
    /// Whether a group is open, and whether it has made its step.
    bool grouping_ = false;
    bool grouped_ = false;
};

} // namespace editor
