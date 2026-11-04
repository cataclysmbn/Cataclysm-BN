#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cata_libintl
{
class trans_library;
}
class translation;

namespace localization
{

/// Length-framed, deterministic content identity; computed at publication, never acquisition.
class content_hash
{
    public:
        auto add( std::string_view value ) -> void;
        auto value() const -> std::uint64_t { return value_; }
    private:
        std::uint64_t value_ = 14695981039346656037ULL;
};

struct snapshot_work_counts {
    std::size_t input_publications = 0;
    std::size_t input_copied_bytes = 0;
    std::size_t input_hashed_bytes = 0;
    std::size_t input_acquisitions = 0;
    std::size_t catalogue_publications = 0;
    std::size_t catalogue_hashed_bytes = 0;
    std::size_t eager_cache_refreshes = 0;
};
auto snapshot_work() -> snapshot_work_counts;
auto reset_snapshot_work() -> void;
/// Called by the catalogue producer at the actual complete-buffer hash site.
auto record_catalogue_hash( std::size_t bytes ) -> void;

/// Internal value payload. Published storage is immutable; translation mutations replace it.
struct translation_input {
    std::optional<std::string> context;
    std::string raw;
    std::optional<std::string> plural;
    bool needs_translation = false;
    std::uint64_t fingerprint = 0;
};

/// Owns exactly one library publication. No global history retains old publications.
class locale_snapshot
{
    public:
        locale_snapshot() = default;
        /// Takes the catalogue buffers by ownership; parsing/identity work occurs here.
        static auto from_catalogues( std::vector<std::string> catalogues ) -> locale_snapshot;
        /// Loader-only: fingerprint covers the ordered complete MO buffers used to create library.
        static auto from_library( cata_libintl::trans_library library,
                                  std::uint64_t fingerprint ) -> locale_snapshot;
        auto fingerprint() const -> std::uint64_t;
        auto library() const -> const cata_libintl::trans_library &; // *NOPAD*
    private:
        struct publication;
        std::shared_ptr<const publication> publication_;
};

class translation_snapshot
{
    public:
        translation_snapshot() = default;
        auto fingerprint() const -> std::uint64_t;
        auto raw_view() const -> std::string_view;
        auto translated( const locale_snapshot &locale, int num = 1 ) const -> std::string;
    private:
        friend class ::translation;
        explicit translation_snapshot( std::shared_ptr<const translation_input> input )
            : input_( std::move( input ) ) {}
        std::shared_ptr<const translation_input> input_;
};

} // namespace localization
