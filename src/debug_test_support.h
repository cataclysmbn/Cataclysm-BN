#pragma once

#include <memory>
#include <string>

namespace debug_test_support
{

/// Owned arguments for invoking the real native prompt (not a substitute widget).
struct prompt_options {
    std::string filename = "client_debug_fixture.cpp";
    std::string line = "42";
    std::string function = "native_debug_fixture";
    std::string text = "Native debug fixture";
    bool force = false;
};

struct state_options {
    /// Exercise the real startup queue rather than immediate main-thread presentation.
    bool buffering = false;
};

/// Single UI-thread test isolation. Restores private debug state, including pre-existing errors.
/// Worker producers must be quiescent at entry/exit; join scoped worker work before exit.
class scoped_state
{
    public:
        explicit scoped_state( const state_options &options = {} );
        ~scoped_state() noexcept;
        scoped_state( const scoped_state & ) = delete;
        auto operator=( const scoped_state & ) -> scoped_state & = delete; // *NOPAD*
    private:
        struct saved_state;
        std::unique_ptr<saved_state> saved_;
};

auto prompt( const prompt_options &options ) -> void;
/// Primes the real repetition folder immediately before its threshold, with timeout pinned;
/// the next realDebugmsg still performs production routing/threshold checks and clock update.
/// Arguments must outlive that call.
auto prime_repetition( const prompt_options &options ) -> void;

} // namespace debug_test_support
