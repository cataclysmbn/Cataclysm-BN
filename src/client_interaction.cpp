#include "client_interaction.h"
#include "client_interaction_metadata.h"
#include "client_interaction_prepared.h"
#include "client_interaction_validation.h"

#include "catacharset.h"
#include "char_validity_check.h"
#include "client_command.h"
#include "client_input.h"
#include "input.h"
#include "json.h"
#include "wcwidth.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace game_client
{
namespace
{
thread_local auto work = interaction_work_counts {};

/// Sorting ordinals leaves output order intact and makes duplicate IDs first-occurrence wins.
auto index_for( const auto &values ) -> std::vector<std::size_t>
{
    namespace ranges = std::ranges;
    auto index = std::views::iota( std::size_t{0}, values.size() )
                 | ranges::to<std::vector>();
    ranges::sort( index, [&values]( const auto lhs, const auto rhs ) {
        const auto order = values[lhs].id.compare( values[rhs].id );
        return order < 0 || ( order == 0 && lhs < rhs );
    } );
    return index;
}
} // namespace

/// Defined only here: callers can retain const ownership but cannot access or mutate the model.
class prepared_interaction
{
    public:
        prepared_interaction( const input_context &context, interaction_snapshot snapshot,
                              std::optional<lazy_choice_descriptions> descriptions = std::nullopt )
            : context( &context ), snapshot( std::move( snapshot ) ),
              choices( index_for( this->snapshot.choices ) ),
              targets( this->snapshot.target ? index_for( this->snapshot.target->candidates ) :
                       std::vector<std::size_t> {} ), descriptions( std::move( descriptions ) ),
        description_cache( this->descriptions ? this->snapshot.choices.size() : 0 ) {}

        auto description( const std::size_t index ) const -> const std::string & { // *NOPAD*
            auto &cached = description_cache[index];
            if( !cached ) {
                ++work.description_calls;
                auto value = descriptions->render( index );
                work.description_bytes += value.size();
                cached = std::move( value );
            }
            return *cached;
        }

        const input_context *const context;
        const interaction_snapshot snapshot;
        const std::vector<std::size_t> choices;
        const std::vector<std::size_t> targets;
        const std::optional<lazy_choice_descriptions> descriptions;
        mutable std::vector<std::optional<std::string>> description_cache;
};

namespace
{

struct provider_entry {
    const input_context *context;
    std::shared_ptr<interaction_provider> provider;
    prepared_interaction_handle prepared;
    std::size_t token;
};

thread_local auto providers = std::vector<provider_entry> {};
thread_local auto next_provider_token = std::size_t {0};

constexpr auto maximum_page_size = std::size_t {200};

class stable_hash
{
    public:
        auto add( const std::string_view value ) -> void {
            for( const auto byte : value ) {
                value_ ^= static_cast<unsigned char>( byte );
                value_ *= 1099511628211ULL;
            }
            value_ ^= 0xffU;
            value_ *= 1099511628211ULL;
        }

        auto str() const -> std::string {
            auto output = std::ostringstream{};
            output.imbue( std::locale::classic() );
            output << std::hex << std::setfill( '0' ) << std::setw( 16 ) << value_;
            return output.str();
        }

    private:
        std::uint64_t value_ = 14695981039346656037ULL;
};

auto provider_for( const input_context &context ) -> const provider_entry *
{
    const auto provider =
    std::ranges::find_if( providers | std::views::reverse, [&context]( const auto & entry ) {
        return entry.context == &context;
    } );
    return provider == providers.rend() ? nullptr : &*provider;
}

auto add_position( stable_hash &hash, const interaction_position &position ) -> void
{
    hash.add( std::to_string( position.x ) );
    hash.add( std::to_string( position.y ) );
    hash.add( std::to_string( position.z ) );
}

struct normalization_options {
    bool count_work = true;
    const lazy_choice_descriptions *descriptions = nullptr;
};

auto schema_for( const interaction_snapshot &snapshot,
                 const normalization_options &options ) -> std::string
{
    const auto *descriptions = options.descriptions;
    if( options.count_work ) {
        ++work.schema_hashes;
        work.hashed_choices += snapshot.choices.size();
    }
    auto hash = stable_hash{};
    if( descriptions ) { hash.add( "owned-lazy-choice-descriptions-v1" ); }
    hash.add( interaction_kind_name( snapshot.kind ) );
    hash.add( snapshot.context );
    hash.add( snapshot.title );
    hash.add( snapshot.message );
    hash.add( snapshot.allow_cancel ? "cancel" : "no-cancel" );
    hash.add( snapshot.allow_set_count ? "set-count" : "no-set-count" );
    for( const auto &pane : snapshot.panes ) {
        hash.add( pane.id );
        hash.add( pane.label );
        hash.add( pane.role );
        hash.add( pane.area_id );
        hash.add( pane.area_label );
        hash.add( pane.area_description );
        hash.add( pane.filter );
        hash.add( pane.storage_kind );
    }
    auto ordinal = std::size_t{0};
    for( const auto &choice : snapshot.choices ) {
        hash.add( choice.id );
        hash.add( choice.label );
        hash.add( descriptions ? descriptions->dependency_keys[ordinal++] : choice.description );
        hash.add( choice.denial );
        hash.add( choice.pane_id.value_or( "no-pane" ) );
        hash.add( choice.area_id.value_or( "no-area" ) );
        hash.add( choice.storage_kind );
        hash.add( choice.enabled ? "enabled" : "disabled" );
        hash.add( choice.selectable ? "selectable" : "blocked" );
        for( const auto &column : choice.columns ) {
            hash.add( column.label );
            hash.add( column.value );
        }
        hash.add( choice.minimum_count ? std::to_string( *choice.minimum_count ) : "default-minimum" );
        hash.add( choice.available_count ? std::to_string( *choice.available_count ) : "no-limit" );
    }
    if( snapshot.field ) {
        hash.add( snapshot.field->id );
        hash.add( snapshot.field->label );
        hash.add( snapshot.field->description );
        hash.add( snapshot.field->type );
        hash.add( std::to_string( snapshot.field->max_length ) );
        hash.add( snapshot.field->printable ? "printable" : "controls-allowed" );
    }
    if( snapshot.target ) {
        hash.add( snapshot.target->coordinate_space );
        add_position( hash, snapshot.target->source );
        add_position( hash, snapshot.target->cursor );
        if( snapshot.target->minimum_position ) {
            add_position( hash, *snapshot.target->minimum_position );
        }
        if( snapshot.target->maximum_position ) {
            add_position( hash, *snapshot.target->maximum_position );
        }
        hash.add( std::to_string( snapshot.target->range ) );
        hash.add( snapshot.target->distance_metric );
        hash.add( snapshot.target->status );
        hash.add( snapshot.target->limit_to_reality_bubble ? "bubble-limited" : "map-limited" );
        for( const auto &candidate : snapshot.target->candidates ) {
            hash.add( candidate.id );
            hash.add( candidate.label );
            hash.add( candidate.description );
            add_position( hash, candidate.position );
            hash.add( candidate.creature ? "creature" : "tile" );
        }
    }
    return hash.str();
}

auto normalize( const input_context &context, interaction_snapshot snapshot,
const normalization_options &options = {} ) -> interaction_snapshot {
    snapshot.context = context.category_name();
    snapshot.structured = true;
    snapshot.actions_only = false;
    snapshot.choice_offset = 0;
    snapshot.choice_total = snapshot.choices.size();
    snapshot.schema_id = schema_for( snapshot, options );
    return snapshot;
}

struct acquired_interaction {
    prepared_interaction_handle prepared;
    interaction_snapshot legacy;
    bool count_work = true;

    auto snapshot() const -> const interaction_snapshot & { // *NOPAD*
        return prepared ? prepared->snapshot : legacy;
    }

    /// Prepared lookup is logarithmic; legacy callbacks still capture their complete live model.
    template<typename Values>
    auto find_id( const Values &values, const std::vector<std::size_t> *index,
                  const std::string &id ) const -> const typename Values::value_type * { // *NOPAD*
        if( index != nullptr ) {
            const auto found = std::ranges::lower_bound( *index, id, {}, [this, &values]( const auto ordinal )
            -> const std::string & { // *NOPAD*
                if( count_work ) { ++work.id_comparisons; }
                return values[ordinal].id;
            } );
            if( found == index->end() ) { return nullptr; }
            if( count_work ) { ++work.id_comparisons; }
            return values[*found].id == id ? &values[*found] : nullptr;
        }
        const auto found = std::ranges::find_if( values, [this, &id]( const auto & value ) {
            if( count_work ) { ++work.id_comparisons; }
            return value.id == id;
        } );
        return found == values.end() ? nullptr : &*found;
    }

    auto choice( const std::string &id ) const -> const interaction_choice * { // *NOPAD*
        return find_id( snapshot().choices, prepared ? &prepared->choices : nullptr, id );
    }

    auto target( const std::string &id ) const -> const interaction_target_candidate * { // *NOPAD*
        return find_id( snapshot().target->candidates, prepared ? &prepared->targets : nullptr, id );
    }
};

auto acquire( const input_context &context ) -> acquired_interaction
{
    const auto entry = provider_for( context );
    if( entry == nullptr ) {
        return { .legacy = {
                .schema_id = {},
                .context = context.category_name(),
                .kind = interaction_kind::custom,
                .structured = false,
                .actions_only = true,
            } };
    }
    if( entry->prepared ) { return { .prepared = entry->prepared }; }
    // Pin the original callable: nested registration can grow the registry, and mutable
    // legacy callbacks must retain their own state rather than run on a copied callable.
    const auto provider = entry->provider;
    return { .legacy = normalize( context, ( *provider )() ) };
}

auto acquire_active() -> acquired_interaction
{
    const auto active = active_input_context();
    if( active.context != nullptr ) { return acquire( *active.context ); }
    return { .legacy = { .context = std::string( active.category ) } };
}

auto materialize( acquired_interaction acquired,
                  interaction_page page ) -> interaction_snapshot
{
    const auto &full = acquired.snapshot();
    const auto total = full.choices.size();
    const auto offset = std::min( page.offset, total );
    const auto count = std::min( total - offset, std::min( page.limit, maximum_page_size ) );
    auto choices = std::vector<interaction_choice> {};
    choices.reserve( count );
    std::ranges::copy( full.choices | std::views::drop( offset ) | std::views::take( count ),
                       std::back_inserter( choices ) );
    // Immutable prepared metadata needs an owned copy.  Ephemeral legacy captures already
    // own their metadata: transfer it, including complete target candidate storage.
    // Never clone the full choices vector merely to discard off-page rows.
    auto result = interaction_snapshot{};
    if( acquired.prepared ) {
        result = {
            .schema_id = full.schema_id,
            .context = full.context,
            .kind = full.kind,
            .title = full.title,
            .message = full.message,
            .structured = full.structured,
            .actions_only = full.actions_only,
            .allow_cancel = full.allow_cancel,
            .panes = full.panes,
            .field = full.field,
            .target = full.target,
            .allow_set_count = full.allow_set_count,
        };
    } else {
        result = std::move( acquired.legacy );
    }
    result.choices = std::move( choices );
    if( acquired.prepared && acquired.prepared->descriptions ) {
        for( const auto index : std::views::iota( std::size_t{0}, count ) ) {
            result.choices[index].description = acquired.prepared->description( offset + index );
        }
    }
    result.input_id = current_input_id();
    result.choice_offset = offset;
    result.choice_total = total;
    work.materialized_choices += count;
    return result;
}

auto validate_field_value( const interaction_field &field, const std::string &value )
-> std::expected<void, std::string>
{
    const auto text = utf8_wrapper( value );
    if( field.max_length > 0 && text.display_width() > static_cast<std::size_t>( field.max_length ) ) {
        return std::unexpected( "invalid: field value exceeds max_length" );
    }
    auto remaining = static_cast<int>( value.size() );
    const auto *cursor = value.c_str();
    auto position = std::size_t{0};
    while( remaining > 0 ) {
        const auto codepoint = UTF8_getch( &cursor, &remaining );
        if( mk_wcwidth( codepoint ) < 0 || ( field.printable && !is_char_allowed( codepoint ) ) ) {
            return std::unexpected( "invalid: field value contains a disallowed character" );
        }
        if( field.type == "integer" ) {
            const auto is_sign = codepoint == '-' && position == 0;
            if( !is_sign && !isdigit( codepoint ) ) {
                return std::unexpected(
                           "invalid: integer field accepts an optional leading minus and digits" );
            }
        }
        ++position;
    }
    return {};
}

auto validate( const acquired_interaction &acquired, const interaction_event &event )
-> std::expected<void, std::string>
{
    const auto &snapshot = acquired.snapshot();
    if( !snapshot.structured ) {
        return std::unexpected( "unsupported: active view only exposes generic actions" );
    }
    if( event.schema_id != snapshot.schema_id ) {
        return std::unexpected(
                   "invalid: interaction schema changed (recorded=" + event.schema_id
                   + ", active=" + snapshot.schema_id + ", context=" + snapshot.context + ")" );
    }
    if( event.operation == interaction_operation::cancel ) {
        if( !event.target_id.empty() || !event.value.empty() || event.submit || event.count
            || event.position ) {
            return std::unexpected(
                       "invalid: cancel does not accept a target, value, submit, count, or position" );
        }
        if( !snapshot.allow_cancel ) {
            return std::unexpected( "unsupported: active interaction cannot be canceled" );
        }
        return {};
    }
    if( event.operation == interaction_operation::choose ) {
        if( event.target_id.empty() || !event.value.empty() || event.submit || event.count
            || event.position ) {
            return std::unexpected( "invalid: choose requires only a choice_id" );
        }
        const auto choice = acquired.choice( event.target_id );
        if( choice == nullptr ) {
            return std::unexpected( "invalid: choice_id is not in the active interaction" );
        }
        if( !choice->selectable ) { return std::unexpected( "disabled: choice is not selectable" ); }
        return {};
    }
    if( event.operation == interaction_operation::set_count ) {
        if( event.target_id.empty() || !event.value.empty() || event.submit || !event.count
            || event.position ) {
            return std::unexpected( "invalid: set_count requires only a choice_id and count" );
        }
        if( !snapshot.allow_set_count ) {
            return std::unexpected(
                       "unsupported: active interaction does not accept item quantities" );
        }
        const auto choice = acquired.choice( event.target_id );
        if( choice == nullptr ) {
            return std::unexpected( "invalid: choice_id is not in the active interaction" );
        }
        if( !choice->selectable ) { return std::unexpected( "disabled: choice is not selectable" ); }
        const auto minimum = choice->minimum_count.value_or( 0 );
        if( *event.count < minimum ) {
            return std::unexpected( "invalid: count is below the minimum quantity" );
        }
        if( !choice->available_count || *event.count > *choice->available_count ) {
            return std::unexpected( "invalid: count exceeds the available item quantity" );
        }
        return {};
    }
    if( event.operation == interaction_operation::set_target ) {
        if( !event.value.empty() || event.submit || event.count || !event.position ) {
            return std::unexpected(
                       "invalid: set_target requires a position and optional candidate_id" );
        }
        if( !snapshot.target ) {
            return std::unexpected( "unsupported: active interaction does not accept a target" );
        }
        if( !event.target_id.empty() ) {
            const auto candidate = acquired.target( event.target_id );
            if( candidate == nullptr
                || candidate->position != *event.position ) {
                return std::unexpected(
                           "invalid: candidate_id does not identify the requested target" );
            }
        }
        if( snapshot.target->minimum_position
            && ( event.position->x < snapshot.target->minimum_position->x
                 || event.position->y < snapshot.target->minimum_position->y
                 || event.position->z < snapshot.target->minimum_position->z ) ) {
            return std::unexpected(
                       "invalid: target position is below the active coordinate bounds" );
        }
        if( snapshot.target->maximum_position
            && ( event.position->x > snapshot.target->maximum_position->x
                 || event.position->y > snapshot.target->maximum_position->y
                 || event.position->z > snapshot.target->maximum_position->z ) ) {
            return std::unexpected(
                       "invalid: target position is above the active coordinate bounds" );
        }
        const auto dx = static_cast<double>( event.position->x ) - snapshot.target->source.x;
        const auto dy = static_cast<double>( event.position->y ) - snapshot.target->source.y;
        const auto dz = static_cast<double>( event.position->z ) - snapshot.target->source.z;
        const auto distance =
            snapshot.target->distance_metric == "trig"
            ? std::round( std::sqrt( dx * dx + dy * dy + dz * dz ) )
            : std::max( {std::abs( dx ), std::abs( dy ), std::abs( dz )} );
        if( distance > snapshot.target->range ) {
            return std::unexpected( "invalid: target position is outside the active range" );
        }
        return {};
    }
    if( event.target_id.empty() || !event.submit || event.count || event.position ) {
        return std::unexpected( "invalid: fill requires field_id, value, and explicit submit" );
    }
    if( !snapshot.field || snapshot.field->id != event.target_id ) {
        return std::unexpected( "invalid: field_id is not in the active interaction" );
    }
    return validate_field_value( *snapshot.field, event.value );
}

} // namespace

interaction_scope::interaction_scope( const input_context &context, interaction_provider provider )
    : token_( ++next_provider_token )
{
    providers.push_back( {.context = &context,
                          .provider = std::make_shared<interaction_provider>( std::move( provider ) ),
                          .token = token_} );
}

interaction_scope::~interaction_scope()
{
    const auto entry = std::ranges::find( providers, token_, &provider_entry::token );
    if( entry != providers.end() ) { providers.erase( entry ); }
}

auto normalize_interaction_metadata( const input_context &context, interaction_snapshot snapshot )
-> interaction_snapshot
{
    return normalize( context, std::move( snapshot ), { .count_work = false } );
}

auto validate_interaction_metadata( const interaction_snapshot &snapshot,
                                    const interaction_event &event )
-> std::expected<void, std::string>
{
    return validate( { .legacy = snapshot, .count_work = false }, event );
}

auto interaction_work() -> interaction_work_counts { return work; }
auto reset_interaction_work() -> void { work = {}; }

auto prepare_interaction( const input_context &context, interaction_snapshot snapshot )
-> prepared_interaction_handle
{
    return std::make_shared<const prepared_interaction>( context,
            normalize( context, std::move( snapshot ) ) );
}

auto prepare_interaction( const input_context &context, interaction_snapshot snapshot,
                          lazy_choice_descriptions descriptions ) -> prepared_interaction_handle
{
    if( !descriptions.render || descriptions.dependency_keys.size() != snapshot.choices.size() ||
    std::ranges::any_of( snapshot.choices, []( const auto & choice ) { return !choice.description.empty(); } ) ) {
        throw std::invalid_argument( "lazy descriptions require empty text and complete owned dependencies" );
    }
    auto normalized = normalize( context, std::move( snapshot ), { .descriptions = &descriptions } );
    return std::make_shared<const prepared_interaction>( context, std::move( normalized ),
            std::move( descriptions ) );
}

prepared_interaction_scope::prepared_interaction_scope( const input_context &context,
        prepared_interaction_handle prepared )
    : token_( ++next_provider_token )
{
    if( !prepared || prepared->context != &context ) {
        throw std::invalid_argument( "prepared interaction requires its original context" );
    }
    providers.push_back( { .context = &context, .prepared = std::move( prepared ), .token = token_ } );
}

prepared_interaction_scope::~prepared_interaction_scope()
{
    const auto entry = std::ranges::find( providers, token_, &provider_entry::token );
    if( entry != providers.end() ) { providers.erase( entry ); }
}

auto current_interaction( interaction_page page ) -> interaction_snapshot
{
    return materialize( acquire_active(), page );
}

auto paginated_interaction( interaction_page_state &state ) -> interaction_snapshot
{
    state.limit = std::clamp( state.limit, std::size_t{1}, maximum_page_size );
    auto acquired = acquire_active();
    const auto &full = acquired.snapshot();
    if( state.schema_id != full.schema_id ) {
        state.offset = 0;
        state.schema_id = full.schema_id;
    }
    const auto last_offset = full.choices.empty() ? std::size_t{0} :
                             ( ( full.choices.size() - 1 ) / state.limit ) * state.limit;
    state.offset = std::min( state.offset, last_offset );
    auto result = materialize( std::move( acquired ), { .offset = state.offset, .limit = state.limit } );
    state.offset = result.choice_offset;
    return result;
}

auto resolve_interaction_command( const interaction_command &command )
-> std::expected<interaction_event, std::string>
{
    auto result = resolve_checked_interaction( { .command = command } );
    if( !result ) { return std::unexpected( std::move( result.error().message ) ); }
    return std::move( *result );
}

auto resolve_checked_interaction( const interaction_validation_request &request )
-> std::expected<interaction_event, interaction_validation_error>
{
    const auto &command = request.command;
    if( command.input_id != current_input_id() ) {
        return std::unexpected( interaction_validation_error{
            .kind = interaction_rejection::stale_boundary,
            .message = "stale: input boundary changed; query bn.interaction again",
        } );
    }
    const auto active = active_input_context();
    if( active.context == nullptr ) {
        return std::unexpected( interaction_validation_error{
            .message = "unsupported: no active input interaction",
        } );
    }
    const auto acquired = acquire( *active.context );
    const auto &snapshot = acquired.snapshot();
    if( request.expected_schema && *request.expected_schema != snapshot.schema_id ) {
        return std::unexpected( interaction_validation_error{
            .kind = interaction_rejection::stale_schema,
            .message = "stale: interaction schema changed",
        } );
    }
    if( request.target_space && ( !snapshot.target ||
                                  *request.target_space != snapshot.target->coordinate_space ) ) {
        return std::unexpected( interaction_validation_error{
            .message = "invalid: target coordinate space changed",
        } );
    }
    auto event = interaction_event{
        .operation = command.operation,
        .schema_id = snapshot.schema_id,
        .target_id = command.target_id,
        .value = command.value,
        .submit = command.submit,
        .count = command.count,
        .position = command.position,
    };
    if( const auto valid = validate( acquired, event ); !valid ) {
        return std::unexpected( interaction_validation_error{ .message = valid.error() } );
    }
    return event;
}

auto validate_interaction_event( const input_context &context, const interaction_event &event )
-> std::expected<void, std::string>
{
    return validate( acquire( context ), event );
}

auto interaction_kind_name( const interaction_kind kind ) -> std::string
{
    switch( kind ) {
        case interaction_kind::choices:
            return "choices";
        case interaction_kind::field:
            return "field";
        case interaction_kind::inventory:
            return "inventory";
        case interaction_kind::target:
            return "target";
        case interaction_kind::custom:
            return "custom";
    }
    return "custom";
}

auto opaque_interaction_id( const std::string &prefix, const std::vector<std::string> &identity )
-> std::string
{
    auto hash = stable_hash{};
    for( const auto &part : identity ) { hash.add( part ); }
    return prefix + ":" + hash.str();
}

auto serialize_interaction( const interaction_snapshot &snapshot ) -> std::string
{
    auto output = std::ostringstream{};
    auto json = JsonOut( output );
    json.start_object();
    json.member( "input_id", snapshot.input_id );
    json.member( "schema_id", snapshot.schema_id );
    json.member( "context", snapshot.context );
    json.member( "kind", interaction_kind_name( snapshot.kind ) );
    json.member( "structured", snapshot.structured );
    json.member( "actions_only", snapshot.actions_only );
    json.member( "allow_cancel", snapshot.allow_cancel );
    json.member( "allow_set_count", snapshot.allow_set_count );
    json.member( "title", snapshot.title );
    json.member( "message", snapshot.message );
    json.member( "panes" );
    json.start_array();
    for( const auto &pane : snapshot.panes ) {
        json.start_object();
        json.member( "id", pane.id );
        json.member( "label", pane.label );
        json.member( "role", pane.role );
        json.member( "area_id", pane.area_id );
        json.member( "area_label", pane.area_label );
        json.member( "area_description", pane.area_description );
        json.member( "filter", pane.filter );
        json.member( "storage_kind", pane.storage_kind );
        json.end_object();
    }
    json.end_array();
    json.member( "choices" );
    json.start_array();
    for( const auto &choice : snapshot.choices ) {
        json.start_object();
        json.member( "id", choice.id );
        json.member( "label", choice.label );
        json.member( "description", choice.description );
        json.member( "denial", choice.denial );
        if( choice.pane_id ) { json.member( "pane_id", *choice.pane_id ); }
        if( choice.area_id ) { json.member( "area_id", *choice.area_id ); }
        if( !choice.storage_kind.empty() ) { json.member( "storage_kind", choice.storage_kind ); }
        json.member( "enabled", choice.enabled );
        json.member( "selectable", choice.selectable );
        json.member( "selected", choice.selected );
        json.member( "highlighted", choice.highlighted );
        json.member( "columns" );
        json.start_array();
        for( const auto &column : choice.columns ) {
            json.start_object();
            json.member( "label", column.label );
            json.member( "value", column.value );
            json.end_object();
        }
        json.end_array();
        if( choice.selected_count ) { json.member( "selected_count", *choice.selected_count ); }
        if( choice.minimum_count ) { json.member( "minimum_count", *choice.minimum_count ); }
        if( choice.available_count ) { json.member( "available_count", *choice.available_count ); }
        json.end_object();
    }
    json.end_array();
    json.member( "choice_page" );
    json.start_object();
    json.member( "offset", snapshot.choice_offset );
    json.member( "count", snapshot.choices.size() );
    json.member( "total", snapshot.choice_total );
    json.end_object();
    if( snapshot.field ) {
        json.member( "field" );
        json.start_object();
        json.member( "id", snapshot.field->id );
        json.member( "label", snapshot.field->label );
        json.member( "description", snapshot.field->description );
        json.member( "value", snapshot.field->value );
        json.member( "type", snapshot.field->type );
        json.member( "max_length", snapshot.field->max_length );
        json.member( "printable", snapshot.field->printable );
        json.end_object();
    }
    if( snapshot.target ) {
        const auto write_position = [&json]( const interaction_position & position ) {
            json.start_object();
            json.member( "x", position.x );
            json.member( "y", position.y );
            json.member( "z", position.z );
            json.end_object();
        };
        json.member( "target" );
        json.start_object();
        json.member( "coordinate_space", snapshot.target->coordinate_space );
        json.member( "source" );
        write_position( snapshot.target->source );
        json.member( "cursor" );
        write_position( snapshot.target->cursor );
        if( snapshot.target->minimum_position ) {
            json.member( "minimum_position" );
            write_position( *snapshot.target->minimum_position );
        }
        if( snapshot.target->maximum_position ) {
            json.member( "maximum_position" );
            write_position( *snapshot.target->maximum_position );
        }
        json.member( "range", snapshot.target->range );
        json.member( "distance_metric", snapshot.target->distance_metric );
        json.member( "status", snapshot.target->status );
        json.member( "limit_to_reality_bubble", snapshot.target->limit_to_reality_bubble );
        json.member( "candidates" );
        json.start_array();
        for( const auto &candidate : snapshot.target->candidates ) {
            json.start_object();
            json.member( "id", candidate.id );
            json.member( "label", candidate.label );
            json.member( "description", candidate.description );
            json.member( "position" );
            write_position( candidate.position );
            json.member( "creature", candidate.creature );
            json.end_object();
        }
        json.end_array();
        json.end_object();
    }
    json.end_object();
    return output.str();
}

} // namespace game_client
