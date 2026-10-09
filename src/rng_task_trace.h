#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "rng_observation.h"

#if defined(CATA_RNG_TRACE_TESTING)
#    include <functional>
#    include <iosfwd>
#endif

/// Diagnostic identities are parent-linked, not worker/thread identities. Record
/// indices are local references only, NOT deterministic inter-thread arrival order.
/// Match records by the recursive (parent, kind, stream, key, occurrence) path.
/// Root-counter samples and worker outer/restored contexts may reflect scheduling;
/// do not compare the raw vector as a deterministic cross-thread event sequence.
enum class rng_task_trace_kind { scope, keyed_task, parallel_call, parallel_index };
enum class rng_task_trace_phase { submitted, running, restored, cancelled };

struct rng_task_trace_record {
    std::optional<std::size_t> parent = std::nullopt;
    rng_task_trace_kind kind = rng_task_trace_kind::scope;
    std::uint64_t stream = 0;
    std::uint64_t key = 0;
    std::size_t occurrence = 0;
    unsigned int seed = 0;
    rng_task_trace_phase phase = rng_task_trace_phase::submitted;
    bool exception = false;
    std::optional<rng_observation> submission_context = std::nullopt;
    std::optional<rng_observation> outer_context = std::nullopt;
    std::optional<rng_observation> initial_context = std::nullopt;
    std::optional<rng_observation> final_context = std::nullopt;
    std::optional<rng_observation> restored_context = std::nullopt;
};

struct rng_task_trace_snapshot {
    /// New ROOT reservations are accepted; existing descendants remain attached
    /// even after stop().
    bool accepting = true;
    bool overflow = false;
    bool incomplete = false;
    /// Exact for tracked reservations in a covered window; only a LOWER BOUND
    /// after overflow/incomplete evidence. Never equate zero with pool idleness.
    std::size_t outstanding = 0;
    std::vector<rng_task_trace_record> records;

    /// Only proves restoration of scopes submitted within this window. The
    /// caller must establish an idle boundary BEFORE enabling the trace. This
    /// does not cover unscoped worker RNG, libc/Lua RNG or pending application.
    auto all_recorded_scopes_restored() const -> bool {
        return !accepting && !overflow && !incomplete && outstanding == 0;
    }
};

struct rng_task_trace_options {
    /// Hard capped at 65536 records AND reservation tickets. Overflow stops
    /// retaining new tasks and denies complete evidence; no unbounded history.
    std::size_t capacity = 4096;
};

class rng_task_trace;
struct rng_task_trace_ticket;
using rng_task_trace_token = std::shared_ptr<rng_task_trace_ticket>;

struct rng_task_trace_submission {
    rng_task_trace_kind kind = rng_task_trace_kind::scope;
    std::uint64_t stream = 0;
    std::uint64_t key = 0;
    unsigned int seed = 0;
    rng_task_trace_token parent = nullptr;
};

/// Owned, passive window. stop() disables new root submissions, but already
/// reserved work and its descendants continue acknowledging this same window.
/// Observations never wait for work, consume futures, or inspect foreign TLS.
class rng_task_trace
{
    public:
        ~rng_task_trace();
        rng_task_trace( const rng_task_trace & ) = delete;
        auto operator=( const rng_task_trace & ) -> rng_task_trace & = delete; // *NOPAD*
        auto observe() const -> rng_task_trace_snapshot;
        auto stop() -> void;

        struct state;
    private:
        std::shared_ptr<state> state_;
        explicit rng_task_trace( const rng_task_trace_options &options );
        friend auto begin_rng_task_trace( const rng_task_trace_options &options ) ->
        std::shared_ptr<rng_task_trace>; // *NOPAD*
};

/// Returns null when deterministic mode is off, a window already exists, or
/// the caller is inside an existing task context. Establish idle entry yourself.
auto begin_rng_task_trace( const rng_task_trace_options &options = {} ) ->
std::shared_ptr<rng_task_trace>; // *NOPAD*

/// Diagnostic-only reservations; these do not allocate gameplay RNG streams.
/// Used by pool submission and by parallel loops on the submitting thread.
auto reserve_rng_task_trace( const rng_task_trace_submission &submission ) -> rng_task_trace_token;
auto rng_task_trace_note_exception() -> void;

/// Owns a parallel call identity across dispatch (not an RNG scope).
class rng_task_trace_call
{
    public:
        explicit rng_task_trace_call( const rng_task_trace_submission &submission );
        ~rng_task_trace_call();
        rng_task_trace_call( const rng_task_trace_call & ) = delete;
        auto operator=( const rng_task_trace_call & ) -> rng_task_trace_call & = delete; // *NOPAD*
        auto token() const -> rng_task_trace_token { return token_; }
    private:
        rng_task_trace_token token_;
        rng_task_trace_token outer_;
        int exceptions_ = 0;
};

/// Binds a submission reservation to the next RNG task scope on this thread.
/// Null reservations remain null (work submitted before the window is excluded).
class rng_task_trace_binding
{
    public:
        explicit rng_task_trace_binding( const rng_task_trace_token &token );
        ~rng_task_trace_binding();
        rng_task_trace_binding( const rng_task_trace_binding & ) = delete;
        auto operator=( const rng_task_trace_binding & ) -> rng_task_trace_binding & = delete; // *NOPAD*
    private:
        rng_task_trace_token old_token_;
        rng_task_trace_token old_callable_;
        bool old_bound_ = false;
};

#if defined(CATA_RNG_TRACE_TESTING)
/// Test-only probes, compiled solely when BUILD_TESTING is enabled.
/// Installed on the submitting thread; never read another thread's probe/TLS.
namespace rng_task_trace_test
{
struct probe {
    std::function < auto() -> void > after_root_owner_acquired = nullptr;
    std::uint64_t occurrence_comparisons = 0;
    std::uint64_t occurrence_hashes = 0;
    std::size_t occurrence_entries = 0;
    std::size_t peak_occurrence_entries = 0;
    /// Runs under the ledger lock; fault injection must not observe/reenter it.
    std::function < auto() -> void > before_record_append = nullptr;
    std::function < auto() -> void > before_context_capture = nullptr;
    /// Route actual engine insertion through a test-owned stream buffer; never reenter capture.
    std::function < auto( std::ostream & ) -> void > configure_engine_output = nullptr;
};

class scoped_probe
{
    public:
        explicit scoped_probe( probe &value );
        ~scoped_probe();
        scoped_probe( const scoped_probe & ) = delete;
        auto operator=( const scoped_probe & ) -> scoped_probe & = delete; // *NOPAD*
    private:
        probe *previous_ = nullptr;
};
} // namespace rng_task_trace_test
#endif
