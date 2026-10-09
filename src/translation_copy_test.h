#pragma once

#if defined(CATA_TRANSLATION_COPY_TESTING)
#include <cstddef>
#include <string>

class translation;

namespace translation_testing
{

/// Observes completed native string copies for one real translation object, not malloc/RSS.
struct copy_work {
    std::size_t cache_fills = 0;
    std::size_t cache_fill_bytes = 0;
    std::size_t owned_returns = 0;
    std::size_t owned_return_bytes = 0;
};

/// Test-build-only observer; never substitutes translation lookup, cache or result contents.
class scoped_copy_observer
{
    public:
        explicit scoped_copy_observer( const translation &source );
        ~scoped_copy_observer();
        scoped_copy_observer( const scoped_copy_observer & ) = delete;
        auto operator=( const scoped_copy_observer & ) -> scoped_copy_observer & = delete; // *NOPAD*
        auto work() const -> copy_work { return work_; }
    private:
        const translation *source_;
        copy_work work_;
        scoped_copy_observer *previous_;
        friend auto observe_cache_fill( const translation &source, std::size_t bytes ) -> void;
        friend auto observe_owned_return( const translation &source,
                                          const std::string &value ) -> std::string;
};

enum class copy_fault { none, cache_fill, owned_return };
/// One-shot fault at a real shared owning-copy site, not a process allocator replacement.
auto inject_copy_failure( copy_fault fault ) -> void;
auto throw_on_copy_fault( copy_fault site ) -> void;

/// Called only after the native cache has been filled.
auto observe_cache_fill( const translation &source, std::size_t bytes ) -> void;
/// Makes the same native owned return copy, then records its completed work.
auto observe_owned_return( const translation &source, const std::string &value ) -> std::string;

} // namespace translation_testing
#endif
