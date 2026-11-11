#pragma once

#include <cstddef>
#include <istream>
#include <streambuf>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

/// Where a JSON object or array that was already skipped ends; see JsonIn::skip_value.
struct skipped_json_value {
    int end = 0;
    bool ate_separator = false;
};

/// Read-only stream buffer over an owned string.
/// Exposes the unread bytes so parsers can scan runs of characters at once.
/// Header-only so tools that only compile json.cpp can use it too.
class memory_streambuf: public std::streambuf {
public:
    explicit memory_streambuf(std::string contents): data_(std::move(contents)) {
        setg(data_.data(), data_.data(), data_.data() + data_.size());
    }
    memory_streambuf(const memory_streambuf&) = delete;
    auto operator=(const memory_streambuf&) -> memory_streambuf& = delete; // *NOPAD*

    auto contents() const -> const std::string& { // *NOPAD*
        return data_;
    }
    auto unread() const -> std::string_view {
        return {gptr(), static_cast<size_t>(egptr() - gptr())};
    }
    /// Consumes `count` bytes, which must not exceed `unread().size()`.
    auto advance(const size_t count) -> void { setg(eback(), gptr() + count, egptr()); }
    /// Offset of the next unread byte.
    auto position() const -> size_t { return static_cast<size_t>(gptr() - eback()); }
    /// Moves to offset `pos`, which must not exceed `contents().size()`.
    auto seek_to(const size_t pos) -> void { setg(eback(), eback() + pos, egptr()); }
    /// JSON values skipped so far by start offset, shared by every JsonIn reading this buffer.
    auto skipped_json_values() -> std::unordered_map<int, skipped_json_value>& { // *NOPAD*
        return skipped_json_values_;
    }

protected:
    auto seekoff(
        const off_type off, const std::ios_base::seekdir dir, const std::ios_base::openmode which)
        -> pos_type override {
        if (!(which & std::ios_base::in)) { return pos_type(off_type(-1)); }
        const auto base = [&]() -> off_type {
            switch (dir) {
                case std::ios_base::beg:
                    return 0;
                case std::ios_base::cur:
                    return gptr() - eback();
                case std::ios_base::end:
                    return egptr() - eback();
                default:
                    return -1;
            }
        }();
        const auto target = base + off;
        if (base < 0 || target < 0 || target > egptr() - eback()) { return pos_type(off_type(-1)); }
        setg(eback(), eback() + target, egptr());
        return pos_type(target);
    }
    auto seekpos(const pos_type pos, const std::ios_base::openmode which) -> pos_type override {
        return seekoff(off_type(pos), std::ios_base::beg, which);
    }

private:
    std::string data_;
    std::unordered_map<int, skipped_json_value> skipped_json_values_;
};

/// Input stream reading from an owned string through memory_streambuf.
class memory_istream: public std::istream {
public:
    explicit memory_istream(std::string contents)
        : std::istream(nullptr),
          buffer_(std::move(contents)) {
        rdbuf(&buffer_);
    }

    auto contents() const -> const std::string& { // *NOPAD*
        return buffer_.contents();
    }

private:
    memory_streambuf buffer_;
};
