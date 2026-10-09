#pragma once

#include "client_interaction_data.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class input_context;

namespace game_client
{

enum class interaction_kind : int {
    custom,
    choices,
    field,
    inventory,
    target,
};

struct interaction_column {
    std::string label;
    std::string value;
};

struct interaction_choice {
    std::string id;
    std::string label;
    std::string description;
    std::string denial;
    std::optional<std::string> pane_id;
    std::optional<std::string> area_id;
    std::string storage_kind;
    bool enabled = true;
    bool selectable = true;
    bool selected = false;
    bool highlighted = false;
    std::vector<interaction_column> columns;
    std::optional<std::uint64_t> selected_count;
    std::optional<std::uint64_t> minimum_count;
    std::optional<std::uint64_t> available_count;
};

struct interaction_pane {
    std::string id;
    std::string label;
    std::string role;
    std::string area_id;
    std::string area_label;
    std::string area_description;
    std::string filter;
    std::string storage_kind;
};

struct interaction_field {
    std::string id;
    std::string label;
    std::string description;
    std::string value;
    std::string type = "text";
    int max_length = -1;
    bool printable = false;
};

struct interaction_target_candidate {
    std::string id;
    std::string label;
    std::string description;
    interaction_position position;
    bool creature = false;
};

struct interaction_target {
    std::string coordinate_space = "bubble_ms";
    interaction_position source;
    interaction_position cursor;
    std::optional<interaction_position> minimum_position;
    std::optional<interaction_position> maximum_position;
    int range = 0;
    std::string distance_metric;
    std::string status;
    bool limit_to_reality_bubble = false;
    std::vector<interaction_target_candidate> candidates;
};

/// Pure data describing the interaction currently waiting at an input boundary.
struct interaction_snapshot {
    std::uint64_t input_id = 0;
    std::string schema_id;
    std::string context;
    interaction_kind kind = interaction_kind::custom;
    std::string title;
    std::string message;
    bool structured = false;
    bool actions_only = true;
    bool allow_cancel = false;
    std::vector<interaction_pane> panes;
    std::vector<interaction_choice> choices;
    std::optional<interaction_field> field;
    std::optional<interaction_target> target;
    bool allow_set_count = false;
    std::size_t choice_offset = 0;
    std::size_t choice_total = 0;
};

struct interaction_page {
    std::size_t offset = 0;
    std::size_t limit = 100;
};

/// Retains one client's bounded choice page across interaction state updates.
struct interaction_page_state {
    std::size_t offset = 0;
    std::size_t limit = 100;
    std::string schema_id;
};

using interaction_provider = std::function < auto()->interaction_snapshot >;

/// Binds a value-only provider to one concrete input context for this synchronous widget read.
class interaction_scope
{
    public:
        interaction_scope( const input_context &context, interaction_provider provider );
        ~interaction_scope();
        interaction_scope( const interaction_scope & ) = delete;
        auto operator=( const interaction_scope& ) -> interaction_scope& = delete; // *NOPAD*

    private:
        std::size_t token_ = 0;
};

auto current_interaction( interaction_page page = {} ) -> interaction_snapshot;
auto paginated_interaction( interaction_page_state &state ) -> interaction_snapshot;
auto resolve_interaction_command( const interaction_command &command )
-> std::expected<interaction_event, std::string>;

/// Validates a live or replayed semantic event against the widget bound to this context.
auto validate_interaction_event( const input_context &context, const interaction_event &event )
-> std::expected<void, std::string>;

auto interaction_kind_name( interaction_kind kind ) -> std::string;
auto opaque_interaction_id( const std::string &prefix, const std::vector<std::string> &identity )
-> std::string;
auto serialize_interaction( const interaction_snapshot &snapshot ) -> std::string;

} // namespace game_client
