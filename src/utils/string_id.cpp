#include "string_id.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

/// Interned strings and their ids.
/// Worker threads create and read string ids too (loading mapgen and submaps), so lookups share
/// a lock with insertions, and the id -> string table never moves, so it is read without one.
class intern_table {
public:
    auto get(const int id) const -> const std::string& { // *NOPAD*
        const auto index = static_cast<size_t>(id);
        const auto* const chunk = chunks_[index >> chunk_bits].load(std::memory_order_acquire);
        return *chunk[index & chunk_mask];
    }

    template <typename S> auto intern(S&& s) -> int {
        {
            const auto lock = std::shared_lock(mutex_);
            if (const auto found = ids_.find(s); found != ids_.end()) { return found->second; }
        }
        const auto lock = std::unique_lock(mutex_);
        const auto [iter, inserted] = ids_.emplace(std::forward<S>(s), size_);
        if (inserted) { append(iter->first); }
        return iter->second;
    }

private:
    static constexpr auto chunk_bits = size_t{12};
    static constexpr auto chunk_size = size_t{1} << chunk_bits;
    static constexpr auto chunk_mask = chunk_size - 1;
    static constexpr auto max_chunks = size_t{1} << 16;
    using chunk = std::array<const std::string*, chunk_size>;

    /// Must be called with `mutex_` held exclusively.
    auto append(const std::string& s) -> void {
        const auto index = static_cast<size_t>(size_);
        const auto chunk_index = index >> chunk_bits;
        if (chunk_index >= max_chunks) { throw std::length_error("too many interned strings"); }
        if (chunk_index == owned_chunks_.size()) {
            owned_chunks_.push_back(std::make_unique<chunk>());
            chunks_[chunk_index].store(owned_chunks_.back()->data(), std::memory_order_release);
        }
        (*owned_chunks_[chunk_index])[index & chunk_mask] = &s;
        ++size_;
    }

    std::shared_mutex mutex_;
    /// Keys stay where they are for the lifetime of the table, so `chunks_` can point at them.
    std::unordered_map<std::string, int> ids_;
    int size_ = 0;
    std::vector<std::unique_ptr<chunk>> owned_chunks_;
    std::array<std::atomic<const std::string* const*>, max_chunks> chunks_ = {};
};

auto get_intern_table() -> intern_table& // *NOPAD*
{
    static auto table = intern_table{};
    return table;
}

} // namespace

auto string_identity_static::string_id_intern(const std::string& s) -> int {
    return get_intern_table().intern(s);
}

auto string_identity_static::string_id_intern(std::string& s) -> int {
    return get_intern_table().intern(s);
}

auto string_identity_static::string_id_intern(std::string&& s) -> int {
    return get_intern_table().intern(std::move(s));
}

auto string_identity_static::get_interned_string(int id) -> const std::string& {
    return get_intern_table().get(id);
}

auto string_identity_static::empty_interned_string() -> int {
    static int empty_string_id = string_id_intern("");
    return empty_string_id;
}
