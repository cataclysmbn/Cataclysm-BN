#pragma once

#include <memory>

namespace game_client::memory
{

/// Isolates the compositor and virtual screens, restoring their complete prior state on exit.
/// Like the compositor itself, this scope must be used on its single owning thread.
class scoped_state
{
    public:
        scoped_state();
        ~scoped_state() noexcept;
        scoped_state( const scoped_state & ) = delete;
        auto operator=( const scoped_state & ) -> scoped_state & = delete; // *NOPAD*
    private:
        struct saved_state;
        std::unique_ptr<saved_state> saved_;
};

} // namespace game_client::memory
