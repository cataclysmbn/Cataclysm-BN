#pragma once

#include "client_interaction.h"
#include "input.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace game_client::evaluation
{

/// Trusted identities supplied by the future evaluator, NOT inferred world fingerprints.
struct identity {
    std::uint64_t session_epoch = 0;
    std::string engine_checkpoint;
    std::string runtime_checkpoint;
    std::string inputs;
    std::string operation;
    auto operator<=>( const identity & ) const = default; // *NOPAD*
};

struct invocation {
    std::string callback;
    std::uint64_t occurrence = 0;
    auto operator<=>( const invocation & ) const = default; // *NOPAD*
};

/// Owned actual popup description. Native filters remain authoritative at consumption;
/// the checkpoint owner must invalidate identity when their captured state changes.
struct request {
    identity evaluation;
    std::vector<invocation> callbacks;
    std::uint64_t occurrence = 0;
    interaction_snapshot interaction;
    /// Includes the complete native request, cursor, any-key mode and actual keybindings.
    std::string popup_schema;
};

/// Explicit input only: no synthesized YES/NO key, guessed cancellation or prompt-text lookup.
struct decision {
    request expected;
    input_event event;
};

struct limits {
    std::size_t requests = 64;
    std::size_t retained_bytes = 262144;
    std::size_t nesting = 16;
};

struct options {
    identity evaluation;
    std::vector<decision> transcript;
    limits budget;
};

enum class status {
    completed,
    requires_decision,
    stale,
    failed,
};

enum class failure {
    none,
    invalid_identity,
    resource_limit,
    invalid_response,
    exception,
    unused_decisions,
    already_finished,
};

/// Evaluation-local diagnostics, never live input/interaction work counters.
struct work_counts {
    std::size_t construction_checks = 0;
    std::size_t constructed_requests = 0;
    std::size_t peak_construction_bytes = 0;
    std::size_t retained_bytes = 0;
};

struct outcome {
    work_counts work;
    status state = status::completed;
    failure reason = failure::none;
    std::optional<request> pending;
};

/// Typed control flow, distinct from ordinary Lua/runtime errors and permissive catches.
class interrupted : public std::exception
{
    public:
        auto what() const noexcept -> const char * override { // *NOPAD*
            return "evaluation decision did not complete";
        }
};

/// Main-thread synchronous ownership. Nested scopes cannot clear an ancestor's failure;
/// a child's unresolved decision is sticky in every ancestor as well.
class scope
{
    public:
        explicit scope( const options &opts );
        ~scope();
        scope( const scope & ) = delete;
        auto operator=( const scope & ) -> scope & = delete; // *NOPAD*
        /// Extract exactly once; subsequent use is failed/already_finished, never completion.
        auto finish() -> outcome;
        auto fail( failure reason ) noexcept -> void;
    private:
        struct state;
        std::unique_ptr<state> state_;
        static auto allocate_state() -> std::unique_ptr<state>;
        friend auto consume( const request &value ) -> input_event;
        friend class callback_scope;
        friend auto active() -> bool;
        friend auto throw_if_incomplete() -> void;
        friend auto fail_current( failure reason ) -> void;
        friend auto mark_failure( failure reason ) noexcept -> void;
        friend class construction_budget;
        friend auto construction_complete() -> void;
};

/// Identifies each actual callback invocation without touching native input counters.
class callback_scope
{
    public:
        explicit callback_scope( std::initializer_list<std::string_view> segments );
        ~callback_scope();
        callback_scope( const callback_scope & ) = delete;
        auto operator=( const callback_scope & ) -> callback_scope & = delete; // *NOPAD*
    private:
        scope::state *state_ = nullptr;
        int exceptions_ = 0;
};

#if defined(CATA_EVALUATION_DECISION_TESTING)
enum class construction_fault { none, allocation, interrupted };
/// One-shot deterministic failure before optional engagement; absent in non-test builds.
auto inject_construction_failure( construction_fault fault ) -> void;
#endif

auto active() -> bool;
auto throw_if_incomplete() -> void;
/// Marks evaluation-local failure and interrupts, leaving ordinary native execution untouched.
auto fail_current( failure reason ) -> void;
/// Latch allocation/construction failure even before a nested scope is engaged.
auto mark_failure( failure reason ) noexcept -> void;

/// Conservative checked upper bound for popup snapshot/schema construction. Check every
/// input before its owned copy/encoding, including arrays before visiting their elements.
class construction_budget
{
    public:
        construction_budget();
        auto add_bytes( std::size_t amount ) -> void;
        auto add_array( std::size_t count, std::size_t element_bytes ) -> void;
        auto add_text( std::string_view text ) -> void;
        /// Maximum next text length under the same checked construction accounting.
        auto remaining_text_capacity() const -> std::size_t;
    private:
        std::size_t limit_ = 0;
        std::size_t size_ = 0;
};
auto construction_complete() -> void;
/// Called before all popup UI effects; admission precedes owned request/response copies.
auto consume( const request &value ) -> input_event;

template<typename T>
struct result {
    outcome evaluation;
    std::optional<T> value;
};

/// The sole completion gate. Callers must export owned presentation/result values, never
/// references to engine objects. This seam does NOT isolate arbitrary callback mutations.
template<typename Operation>
requires( !std::is_reference_v<std::invoke_result_t<Operation>> &&
          !std::is_void_v<std::invoke_result_t<Operation>> )
auto evaluate( const options &opts, Operation &&operation )
-> result<std::invoke_result_t<Operation>>
{
    auto evaluation = std::optional<scope> {};
    try {
        evaluation.emplace( opts );
        throw_if_incomplete();
        auto value = std::invoke( std::forward<Operation>( operation ) );
        auto terminal = evaluation->finish();
        if( terminal.state == status::completed ) {
            return { .evaluation = std::move( terminal ), .value = std::move( value ) };
        }
        return { .evaluation = std::move( terminal ) };
    } catch( const interrupted & ) {
        if( evaluation ) { return { .evaluation = evaluation->finish() }; }
        mark_failure( failure::exception );
        return { .evaluation = { .state = status::failed, .reason = failure::exception } };
    } catch( ... ) {
        if( evaluation ) {
            evaluation->fail( failure::exception );
            return { .evaluation = evaluation->finish() };
        }
        mark_failure( failure::exception );
        return { .evaluation = { .state = status::failed, .reason = failure::exception } };
    }
}

} // namespace game_client::evaluation
