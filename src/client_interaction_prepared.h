#pragma once

#include "client_interaction.h"

#include <memory>

namespace game_client
{

class prepared_interaction;
using prepared_interaction_handle = std::shared_ptr<const prepared_interaction>;

/// Own a complete native boundary, normalizing and hashing it once.  Input IDs are stamped on reads.
/// Common metadata and target candidates remain complete; cold preparation is not page bounded.
auto prepare_interaction( const input_context &context, interaction_snapshot snapshot )
-> prepared_interaction_handle;

/// Complete frozen dependencies, cheap to capture/hash. Renderer must own its frozen inputs.
/// Empty metadata descriptions are required. Game-thread use only; no world access on I/O threads.
struct lazy_choice_descriptions {
    std::vector<std::string> dependency_keys;
    std::function < auto( std::size_t ) -> std::string > render;
};
auto prepare_interaction( const input_context &context, interaction_snapshot snapshot,
                          lazy_choice_descriptions descriptions ) -> prepared_interaction_handle;

/// Register immutable data for exactly one native read.  A retained handle is not command authority.
/// The handle must be non-null and prepared for this exact context.  Game-thread use only.
class prepared_interaction_scope
{
    public:
        prepared_interaction_scope( const input_context &context, prepared_interaction_handle prepared );
        ~prepared_interaction_scope();
        prepared_interaction_scope( const prepared_interaction_scope & ) = delete;
        auto operator=( const prepared_interaction_scope & ) -> prepared_interaction_scope & =
            delete; // *NOPAD*

    private:
        std::size_t token_ = 0;
};

/// Deterministic thread-local work diagnostics, not timing or wire state.  Includes legacy work.
struct interaction_work_counts {
    std::size_t schema_hashes = 0;
    std::size_t hashed_choices = 0;
    std::size_t materialized_choices = 0;
    std::size_t id_comparisons = 0;
    std::size_t description_calls = 0;
    std::size_t description_bytes = 0;
};
auto interaction_work() -> interaction_work_counts;
auto reset_interaction_work() -> void;

} // namespace game_client
