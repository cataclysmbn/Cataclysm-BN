#ifndef CATA_TOOLS_CLANG_TIDY_PLUGIN_STRINGLITERALITERATOR_H
#define CATA_TOOLS_CLANG_TIDY_PLUGIN_STRINGLITERALITERATOR_H

#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/TargetInfo.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>

namespace clang {
class LangOptions;
class SourceManager;
class StringLiteral;

namespace tidy {
namespace cata {

// Currently only supports utf8 becasue StringLiteral only has full support
// for single-byte ascii/utf8 strings
class StringLiteralIterator {
public:
    // This assumes that ind points to the start of a valid utf8 sequence
    StringLiteralIterator(const StringLiteral& str, size_t ind);

    // Get the source location corresponding to the character pointed to
    // by the iterator.
    // Do note that clang crashes when the string literal is a predefined
    // expression such as __func__, so you may want to exclude them using
    // the matcher `unless( hasAncestor( predefinedExpr() ) )` or something
    // to that effect.
    auto toSourceLocation(
        const SourceManager& SrcMgr, const LangOptions& LangOpts, const TargetInfo& Info) const
        -> SourceLocation;

    // All following operators assume that StringLiteral contains a valid
    // utf8 string
    auto operator*() const -> uint32_t;

    auto operator<(const StringLiteralIterator& rhs) const -> bool;
    auto operator>(const StringLiteralIterator& rhs) const -> bool;
    auto operator<=(const StringLiteralIterator& rhs) const -> bool;
    auto operator>=(const StringLiteralIterator& rhs) const -> bool;
    auto operator==(const StringLiteralIterator& rhs) const -> bool;
    auto operator!=(const StringLiteralIterator& rhs) const -> bool;

    auto operator+=(ptrdiff_t inc) -> StringLiteralIterator&; // *NOPAD*
    auto operator-=(ptrdiff_t dec) -> StringLiteralIterator&; // *NOPAD*
    auto operator+(ptrdiff_t inc) const -> StringLiteralIterator;
    auto operator-(ptrdiff_t dec) const -> StringLiteralIterator;
    auto operator++() -> StringLiteralIterator&; // *NOPAD*
    auto operator++(int) -> StringLiteralIterator;
    auto operator--() -> StringLiteralIterator&; // *NOPAD*
    auto operator--(int) -> StringLiteralIterator;

    static auto begin(const StringLiteral& str) -> StringLiteralIterator;
    static auto end(const StringLiteral& str) -> StringLiteralIterator;

    static auto distance(const StringLiteralIterator& beg, const StringLiteralIterator& end)
        -> ptrdiff_t;

private:
    std::reference_wrapper<const StringLiteral> str;
    size_t ind;
};

} // namespace cata
} // namespace tidy
} // namespace clang

namespace std {
template <> struct iterator_traits<clang::tidy::cata::StringLiteralIterator> {
    using difference_type = ptrdiff_t;
    using value_type = uint32_t;
    using pointer = const uint32_t*;
    using reference = const uint32_t&;
    // randome_access_iterator_tag requires constant increment/decrement time,
    // which StringLiteralIterator doesn't satisfy.
    using iterator_category = bidirectional_iterator_tag;
};
} // namespace std

#endif // CATA_TOOLS_CLANG_TIDY_PLUGIN_STRINGLITERALITERATOR_H
