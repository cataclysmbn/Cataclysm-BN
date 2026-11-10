#pragma once

#include <memory>

namespace debug_test_support
{

/// Single UI-thread test isolation. Restores private debug state, including pre-existing errors, and
/// shows each debug message as it comes instead of queueing it for startup.
/// Worker producers must be quiescent at entry/exit; join scoped worker work before exit.
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

} // namespace debug_test_support
