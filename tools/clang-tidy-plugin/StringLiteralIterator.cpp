#include "StringLiteralIterator.h"

#include <algorithm>
#include <clang/AST/Expr.h>
#include <cstddef>
#include <cstdint>

namespace clang {
class LangOptions;
class SourceManager;

namespace tidy {
namespace cata {

StringLiteralIterator::StringLiteralIterator(const StringLiteral& str, const size_t ind)
    : str(str),
      ind(ind) {}

auto StringLiteralIterator::toSourceLocation(
    const SourceManager& SrcMgr, const LangOptions& LangOpts, const TargetInfo& Info) const
    -> SourceLocation {
    return str.get().getLocationOfByte(ind, SrcMgr, LangOpts, Info);
}

auto StringLiteralIterator::operator*() const -> uint32_t {
    uint32_t ch = str.get().getCodeUnit(ind);
    unsigned int n;
    if (ch >= 0xFC) {
        ch &= 0x01;
        n = 5;
    } else if (ch >= 0xF8) {
        ch &= 0x03;
        n = 4;
    } else if (ch >= 0xF0) {
        ch &= 0x07;
        n = 3;
    } else if (ch >= 0xE0) {
        ch &= 0x0F;
        n = 2;
    } else if (ch >= 0xC0) {
        ch &= 0x1F;
        n = 1;
    } else {
        n = 0;
    }
    for (size_t i = 1; i <= n; ++i) {
        if (ind + i >= str.get().getLength()) {
            return 0xFFFD; // unknown unicode
        }
        ch = (ch << 6) | (str.get().getCodeUnit(ind + i) & 0x3F);
    }
    return ch;
}

auto StringLiteralIterator::operator<(const StringLiteralIterator& rhs) const -> bool {
    return ind < rhs.ind;
}

auto StringLiteralIterator::operator>(const StringLiteralIterator& rhs) const -> bool {
    return rhs.operator<(*this);
}

auto StringLiteralIterator::operator<=(const StringLiteralIterator& rhs) const -> bool {
    return !rhs.operator<(*this);
}

auto StringLiteralIterator::operator>=(const StringLiteralIterator& rhs) const -> bool {
    return !operator<(rhs);
}

auto StringLiteralIterator::operator==(const StringLiteralIterator& rhs) const -> bool {
    return ind == rhs.ind;
}

auto StringLiteralIterator::operator!=(const StringLiteralIterator& rhs) const -> bool {
    return !operator==(rhs);
}

auto StringLiteralIterator::operator+=(ptrdiff_t inc) -> StringLiteralIterator& { // *NOPAD*
    if (inc == 0) {
        return *this;
    } else if (inc > 0) {
        while (inc > 0 && ind < str.get().getLength()) {
            uint32_t byte = str.get().getCodeUnit(ind);
            if (byte >= 0xFC) {
                ind += 6;
            } else if (byte >= 0xF8) {
                ind += 5;
            } else if (byte >= 0xF0) {
                ind += 4;
            } else if (byte >= 0xE0) {
                ind += 3;
            } else if (byte >= 0xC0) {
                ind += 2;
            } else {
                ++ind;
            }
            --inc;
        }
        ind = std::min<size_t>(ind, str.get().getLength());
    } else {
        while (inc < 0 && ind > 0) {
            uint32_t byte = str.get().getCodeUnit(ind - 1);
            if (byte < 0x80 || byte >= 0xC0) { ++inc; }
            --ind;
        }
    }
    return *this;
}

auto StringLiteralIterator::operator-=(ptrdiff_t dec) -> StringLiteralIterator& { // *NOPAD*
    return operator+=(-dec);
}

auto StringLiteralIterator::operator+(ptrdiff_t inc) const -> StringLiteralIterator {
    StringLiteralIterator ret = *this;
    ret.operator+=(inc);
    return ret;
}

auto StringLiteralIterator::operator-(ptrdiff_t dec) const -> StringLiteralIterator {
    return operator+(-dec);
}

auto StringLiteralIterator::operator++() -> StringLiteralIterator& {
    return operator+=(1);
} // *NOPAD*

auto StringLiteralIterator::operator++(int) -> StringLiteralIterator {
    StringLiteralIterator ret = *this;
    operator++();
    return ret;
}

auto StringLiteralIterator::operator--() -> StringLiteralIterator& {
    return operator-=(1);
} // *NOPAD*

auto StringLiteralIterator::operator--(int) -> StringLiteralIterator {
    StringLiteralIterator ret = *this;
    operator--();
    return ret;
}

auto StringLiteralIterator::begin(const StringLiteral& str) -> StringLiteralIterator {
    return StringLiteralIterator(str, 0);
}

auto StringLiteralIterator::end(const StringLiteral& str) -> StringLiteralIterator {
    return StringLiteralIterator(str, str.getLength());
}

auto StringLiteralIterator::distance(
    const StringLiteralIterator& beg, const StringLiteralIterator& end) -> ptrdiff_t {
    if (beg <= end) {
        ptrdiff_t dist = 0;
        for (auto it = beg; it < end; ++it, ++dist) {}
        return dist;
    } else {
        return -distance(end, beg);
    }
}

} // namespace cata
} // namespace tidy
} // namespace clang
