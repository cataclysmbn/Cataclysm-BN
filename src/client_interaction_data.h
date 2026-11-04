#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace game_client
{

enum class interaction_operation : int {
    choose,
    fill,
    set_count,
    set_target,
    cancel,
    /// An affordance a screen runs itself, named by `target_id` with its `arguments`.
    invoke,
};

struct interaction_position {
    int x = 0;
    int y = 0;
    int z = 0;

    auto operator<=>( const interaction_position & ) const = default; // *NOPAD*
};

/// A row of an invoke's `rows` argument and how much of it.
struct interaction_row_count {
    std::string id;
    std::optional<std::uint64_t> count;

    auto operator<=>( const interaction_row_count & ) const = default; // *NOPAD*
};

/// What an `invoke` carries beyond its affordance: the members the affordance's argument shape names.
/// `position` is bubble-relative, like every native position.
struct interaction_arguments {
    std::optional<std::string> row;
    std::optional<std::vector<interaction_row_count>> rows;
    std::optional<std::int64_t> count;
    std::optional<std::string> text;
    std::optional<interaction_position> position;
    std::optional<std::string> pane;

    auto operator<=>( const interaction_arguments & ) const = default; // *NOPAD*
};

/// Transport-safe semantic input.  IDs are opaque outside the current interaction snapshot.
struct interaction_command {
    std::uint64_t input_id = 0;
    interaction_operation operation = interaction_operation::cancel;
    std::string target_id;
    std::string value;
    std::optional<bool> submit;
    std::optional<std::uint64_t> count;
    std::optional<interaction_position> position;

    auto operator<=>( const interaction_command & ) const = default; // *NOPAD*
};

/// Replay-safe semantic input after transport identity has been validated.
struct interaction_event {
    interaction_operation operation = interaction_operation::cancel;
    std::string schema_id;
    std::string target_id;
    std::string value;
    std::optional<bool> submit;
    std::optional<std::uint64_t> count;
    std::optional<interaction_position> position;
    /// Present exactly on `invoke`.
    std::optional<interaction_arguments> arguments;

    auto operator<=>( const interaction_event & ) const = default; // *NOPAD*
};

inline auto interaction_operation_name( const interaction_operation operation ) -> std::string
{
    switch( operation ) {
        case interaction_operation::choose:
            return "choose";
        case interaction_operation::fill:
            return "fill";
        case interaction_operation::set_count:
            return "set_count";
        case interaction_operation::set_target:
            return "set_target";
        case interaction_operation::cancel:
            return "cancel";
        case interaction_operation::invoke:
            return "invoke";
    }
    return "cancel";
}

inline auto parse_interaction_operation( const std::string &operation )
-> std::optional<interaction_operation>
{
    if( operation == "choose" ) {
        return interaction_operation::choose;
    }
    if( operation == "fill" ) {
        return interaction_operation::fill;
    }
    if( operation == "set_count" ) {
        return interaction_operation::set_count;
    }
    if( operation == "set_target" ) {
        return interaction_operation::set_target;
    }
    if( operation == "cancel" ) {
        return interaction_operation::cancel;
    }
    if( operation == "invoke" ) {
        return interaction_operation::invoke;
    }
    return std::nullopt;
}

} // namespace game_client
