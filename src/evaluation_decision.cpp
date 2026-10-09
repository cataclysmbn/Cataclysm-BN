#include "evaluation_decision.h"

#include <algorithm>
#include <limits>
#include <ranges>
#include <stdexcept>

namespace game_client::evaluation
{
namespace
{
thread_local auto current = static_cast<scope *>( nullptr );
#if defined(CATA_EVALUATION_DECISION_TESTING)
thread_local auto next_construction_fault = construction_fault::none;
#endif
constexpr auto maximum_requests = std::size_t { 128 };
constexpr auto maximum_bytes = std::size_t { 1048576 };
constexpr auto maximum_nesting = std::size_t { 32 };

// All additions are capped, so malicious sizes cannot wrap the retention budget.
auto add_size( std::size_t &size, const std::size_t amount ) -> void
{
    size += std::min( amount, maximum_bytes + 1 - size );
}

auto identity_size( const identity &value ) -> std::size_t
{
    auto size = sizeof( identity );
    for( const auto *text : {
             &value.engine_checkpoint, &value.runtime_checkpoint,
             &value.inputs, &value.operation
         } ) {
        add_size( size, text->size() );
    }
    return size;
}

auto add_array_size( std::size_t &size, const std::size_t count, const std::size_t width ) -> void
{
    if( count > ( maximum_bytes + 1 - size ) / width ) { size = maximum_bytes + 1; }
    else { size += count * width; }
}

// Measure owned storage without serializing/copying. Arrays are admitted before traversal.
auto snapshot_size( const interaction_snapshot &value ) -> std::size_t
{
    auto size = sizeof( value );
    const auto texts = [&size]( const auto & ...text ) { ( add_size( size, text.size() ), ... ); };
    texts( value.schema_id, value.context, value.title, value.message );
    add_array_size( size, value.panes.size(), sizeof( interaction_pane ) );
    add_array_size( size, value.choices.size(), sizeof( interaction_choice ) );
    if( size > maximum_bytes ) { return size; }
    for( const auto &pane : value.panes ) {
        texts( pane.id, pane.label, pane.role, pane.area_id, pane.area_label, pane.area_description,
               pane.filter, pane.storage_kind );
        if( size > maximum_bytes ) { return size; }
    }
    for( const auto &choice : value.choices ) {
        texts( choice.id, choice.label, choice.description, choice.denial, choice.storage_kind );
        if( choice.pane_id ) { texts( *choice.pane_id ); }
        if( choice.area_id ) { texts( *choice.area_id ); }
        add_array_size( size, choice.columns.size(), sizeof( interaction_column ) );
        if( size > maximum_bytes ) { return size; }
        for( const auto &column : choice.columns ) {
            texts( column.label, column.value );
            if( size > maximum_bytes ) { return size; }
        }
    }
    if( value.field ) {
        texts( value.field->id, value.field->label, value.field->description,
               value.field->value, value.field->type );
    }
    if( value.target ) {
        texts( value.target->coordinate_space, value.target->distance_metric, value.target->status );
        add_array_size( size, value.target->candidates.size(), sizeof( interaction_target_candidate ) );
        if( size > maximum_bytes ) { return size; }
        for( const auto &candidate : value.target->candidates ) {
            texts( candidate.id, candidate.label, candidate.description );
            if( size > maximum_bytes ) { return size; }
        }
    }
    return size;
}

auto request_size( const request &value ) -> std::size_t
{
    auto size = sizeof( request );
    add_size( size, identity_size( value.evaluation ) );
    add_size( size, value.popup_schema.size() );
    add_size( size, snapshot_size( value.interaction ) );
    add_array_size( size, value.callbacks.size(), sizeof( invocation ) );
    if( size > maximum_bytes ) { return size; }
    for( const auto &callback : value.callbacks ) {
        add_size( size, callback.callback.size() );
        if( size > maximum_bytes ) { return size; }
    }
    return size;
}

auto event_size( const input_event &value ) -> std::size_t
{
    auto size = sizeof( input_event );
    // Divide before multiplication to avoid overflow for untrusted event arrays.
    add_size( size, std::min( value.sequence.size(),
                              maximum_bytes / sizeof( int ) + 1 ) * sizeof( int ) );
    add_size( size, std::min( value.modifiers.size(),
                              maximum_bytes / sizeof( int ) + 1 ) * sizeof( int ) );
    add_size( size, value.text.size() );
    add_size( size, value.edit.size() );
    if( value.interaction ) {
        add_size( size, value.interaction->schema_id.size() );
        add_size( size, value.interaction->target_id.size() );
        add_size( size, value.interaction->value.size() );
    }
    return size;
}

template<typename T, typename Equal>
auto optional_equal( const std::optional<T> &lhs, const std::optional<T> &rhs, Equal equal ) -> bool
{
    return lhs.has_value() == rhs.has_value() && ( !lhs || equal( *lhs, *rhs ) );
}

auto same_snapshot( const interaction_snapshot &a, const interaction_snapshot &b ) -> bool
{
    namespace ranges = std::ranges;
    return a.input_id == b.input_id && a.schema_id == b.schema_id && a.context == b.context &&
           a.kind == b.kind && a.title == b.title && a.message == b.message &&
           a.structured == b.structured && a.actions_only == b.actions_only &&
           a.allow_cancel == b.allow_cancel && a.allow_set_count == b.allow_set_count &&
           a.choice_offset == b.choice_offset && a.choice_total == b.choice_total &&
    ranges::equal( a.panes, b.panes, []( const auto & x, const auto & y ) {
        return x.id == y.id && x.label == y.label && x.role == y.role && x.area_id == y.area_id &&
               x.area_label == y.area_label && x.area_description == y.area_description &&
               x.filter == y.filter && x.storage_kind == y.storage_kind;
    } ) &&ranges::equal( a.choices, b.choices, []( const auto & x, const auto & y ) {
        return x.id == y.id && x.label == y.label && x.description == y.description &&
               x.denial == y.denial && x.pane_id == y.pane_id && x.area_id == y.area_id &&
               x.storage_kind == y.storage_kind && x.enabled == y.enabled && x.selectable == y.selectable &&
               x.selected == y.selected && x.highlighted == y.highlighted &&
               x.selected_count == y.selected_count && x.minimum_count == y.minimum_count &&
               x.available_count == y.available_count &&
        std::ranges::equal( x.columns, y.columns, []( const auto & c, const auto & d ) {
            return c.label == d.label && c.value == d.value;
        } );
    } ) &&optional_equal( a.field, b.field, []( const auto & x, const auto & y ) {
        return x.id == y.id && x.label == y.label && x.description == y.description && x.value == y.value &&
               x.type == y.type && x.max_length == y.max_length && x.printable == y.printable;
    } ) &&optional_equal( a.target, b.target, []( const auto & x, const auto & y ) {
        return x.coordinate_space == y.coordinate_space && x.source == y.source && x.cursor == y.cursor &&
               x.minimum_position == y.minimum_position && x.maximum_position == y.maximum_position &&
               x.range == y.range && x.distance_metric == y.distance_metric && x.status == y.status &&
               x.limit_to_reality_bubble == y.limit_to_reality_bubble &&
        std::ranges::equal( x.candidates, y.candidates, []( const auto & c, const auto & d ) {
            return c.id == d.id && c.label == d.label && c.description == d.description &&
                   c.position == d.position && c.creature == d.creature;
        } );
    } );
}

auto same_request( const request &lhs, const request &rhs ) -> bool
{
    return lhs.evaluation == rhs.evaluation && lhs.callbacks == rhs.callbacks &&
           lhs.occurrence == rhs.occurrence && lhs.popup_schema == rhs.popup_schema &&
           same_snapshot( lhs.interaction, rhs.interaction );
}
} // namespace

struct scope::state {
    scope *parent = current;
    outcome terminal;
    identity evaluation;
    limits budget;
    std::vector<decision> transcript;
    std::vector<invocation> callbacks;
    std::size_t depth = 1;
    std::size_t retained = 0;
    std::size_t consumed = 0;
    std::uint64_t requests = 0;
    std::uint64_t invocations = 0;

    auto fail( const failure reason ) noexcept -> void {
        // Preserve an earlier pending/stale outcome; it must never become completion.
        if( terminal.state == status::completed ) {
            terminal.state = status::failed;
            terminal.reason = reason;
        }
        if( parent ) { parent->state_->fail( reason ); }
    }

    auto stop( const status value, const request &pending ) -> void {
        if( terminal.state == status::completed ) {
            const auto size = request_size( pending );
            if( size > budget.retained_bytes - retained ) { fail( failure::resource_limit ); return; }
            retained += size;
            terminal.state = value; // Latch before a potentially throwing owned copy.
            try {
                terminal.pending = pending;
                if( parent ) { parent->state_->stop( value, *terminal.pending ); }
            } catch( ... ) {
                terminal.pending.reset();
                terminal.state = status::failed;
                terminal.reason = failure::exception;
                if( parent ) { parent->state_->fail( failure::exception ); }
                throw;
            }
        }
    }
};

#if defined(CATA_EVALUATION_DECISION_TESTING)
auto inject_construction_failure( const construction_fault fault ) -> void
{
    next_construction_fault = fault;
}
#endif

auto scope::allocate_state() -> std::unique_ptr<state>
{
#if defined(CATA_EVALUATION_DECISION_TESTING)
    switch( std::exchange( next_construction_fault, construction_fault::none ) ) {
        case construction_fault::allocation:
            throw std::bad_alloc{};
        case construction_fault::interrupted:
            throw interrupted{};
        case construction_fault::none:
            break;
    }
#endif
    return std::make_unique<state>();
}

scope::scope( const options &opts ) : state_( allocate_state() )
{
    auto &state = *state_;
    if( state.parent ) { state.depth = state.parent->state_->depth + 1; }
    state.budget = opts.budget;
    if( !state.budget.requests || state.budget.requests > maximum_requests ||
        !state.budget.retained_bytes || state.budget.retained_bytes > maximum_bytes ||
        !state.budget.nesting || state.budget.nesting > maximum_nesting ||
        state.depth > state.budget.nesting || opts.transcript.size() > state.budget.requests ) {
        state.fail( failure::resource_limit );
    } else if( !opts.evaluation.session_epoch || opts.evaluation.engine_checkpoint.empty() ||
               opts.evaluation.runtime_checkpoint.empty() || opts.evaluation.inputs.empty() ||
               opts.evaluation.operation.empty() ) {
        state.fail( failure::invalid_identity );
    } else {
        try {
            auto retained = identity_size( opts.evaluation );
            for( const auto &entry : opts.transcript ) {
                add_size( retained, request_size( entry.expected ) );
                add_size( retained, event_size( entry.event ) );
                if( retained > state.budget.retained_bytes ) { break; }
            }
            if( retained > state.budget.retained_bytes ) {
                state.fail( failure::resource_limit );
            } else {
                auto evaluation = opts.evaluation;
                auto transcript = opts.transcript;
                state.evaluation = std::move( evaluation );
                state.transcript = std::move( transcript );
                state.retained = retained;
            }
        } catch( ... ) { state.fail( failure::exception ); }
    }
    // A nested fresh transcript cannot reset sticky ancestor state.
    if( state.parent && state.parent->state_->terminal.state != status::completed ) {
        const auto &ancestor = state.parent->state_->terminal;
        try {
            if( ancestor.pending ) { state.stop( ancestor.state, *ancestor.pending ); }
            else { state.fail( ancestor.reason ); }
        } catch( ... ) { state.fail( failure::exception ); }
    }
    current = this;
}

scope::~scope()
{
    if( state_->terminal.state == status::completed && state_->consumed != state_->transcript.size() ) {
        state_->fail( failure::unused_decisions );
    }
    if( std::uncaught_exceptions() ) { state_->fail( failure::exception ); }
    current = state_->parent;
}

auto scope::fail( const failure reason ) noexcept -> void { state_->fail( reason ); }

auto scope::finish() -> outcome
{
    if( state_->terminal.state == status::completed && state_->consumed != state_->transcript.size() ) {
        state_->fail( failure::unused_decisions );
    }
    state_->terminal.work.retained_bytes = state_->retained;
    auto terminal = std::move( state_->terminal );
    state_->terminal = { .work = terminal.work, .state = status::failed,
                         .reason = failure::already_finished
                       };
    return terminal;
}

auto active() -> bool { return current != nullptr; }

auto throw_if_incomplete() -> void
{
    if( current && current->state_->terminal.state != status::completed ) { throw interrupted{}; }
}

auto fail_current( const failure reason ) -> void
{
    if( current ) {
        current->state_->fail( reason );
        throw interrupted{};
    }
}

auto mark_failure( const failure reason ) noexcept -> void
{
    if( current ) { current->state_->fail( reason ); }
}

construction_budget::construction_budget()
{
    throw_if_incomplete();
    if( !current ) { throw std::logic_error( "construction requires evaluation" ); }
    auto &state = *current->state_;
    if( state.requests >= state.budget.requests ) { fail_current( failure::resource_limit ); }
    limit_ = state.budget.retained_bytes - state.retained;
    add_bytes( identity_size( state.evaluation ) );
    add_array( state.callbacks.size(), sizeof( invocation ) );
    for( const auto &callback : state.callbacks ) { add_bytes( callback.callback.size() ); }
}

auto construction_budget::add_bytes( const std::size_t amount ) -> void
{
    throw_if_incomplete();
    if( !current ) { throw std::logic_error( "construction requires evaluation" ); }
    ++current->state_->terminal.work.construction_checks;
    if( amount > limit_ - size_ ) { fail_current( failure::resource_limit ); }
    size_ += amount;
    auto &peak = current->state_->terminal.work.peak_construction_bytes;
    peak = std::max( peak, size_ );
}

auto construction_budget::add_array( const std::size_t count,
                                     const std::size_t element_bytes ) -> void
{
    if( element_bytes && count > ( limit_ - size_ ) / element_bytes ) {
        fail_current( failure::resource_limit );
    }
    add_bytes( count * element_bytes );
}

namespace
{
constexpr auto text_construction_multiplier = std::size_t { 32 };
} // namespace

auto construction_budget::remaining_text_capacity() const -> std::size_t
{
    throw_if_incomplete();
    if( !current ) { throw std::logic_error( "construction requires evaluation" ); }
    if( size_ > limit_ ) { fail_current( failure::resource_limit ); }
    return ( limit_ - size_ ) / text_construction_multiplier;
}

auto construction_budget::add_text( const std::string_view text ) -> void
{
    // JSON escaping uses at most six bytes per input byte. Include transient encoder
    // growth, its exported string and owned snapshot/context copies conservatively.
    add_array( text.size(), text_construction_multiplier );
}

auto construction_complete() -> void
{
    throw_if_incomplete();
    ++current->state_->terminal.work.constructed_requests;
}

auto consume( const request &source ) -> input_event
{
    throw_if_incomplete();
    if( !current ) { throw std::logic_error( "evaluation request requires a scope" ); }
    auto &state = *current->state_;
    if( state.requests >= state.budget.requests ) { fail_current( failure::resource_limit ); }
    const auto occurrence = ++state.requests;
    auto size = sizeof( request );
    add_size( size, snapshot_size( source.interaction ) );
    add_size( size, source.popup_schema.size() );
    add_size( size, identity_size( state.evaluation ) );
    add_array_size( size, state.callbacks.size(), sizeof( invocation ) );
    for( const auto &callback : state.callbacks ) { add_size( size, callback.callback.size() ); }
    if( size > state.budget.retained_bytes - state.retained ) { fail_current( failure::resource_limit ); }
    try {
        // Copy only after admission. input_event::interaction is an optional VALUE, not a
        // shared_ptr; both transcript admission and this response copy break caller aliases.
        auto value = request{
            .evaluation = state.evaluation,
            .callbacks = state.callbacks,
            .occurrence = occurrence,
            .interaction = source.interaction,
            .popup_schema = source.popup_schema,
        };
        if( state.consumed == state.transcript.size() ) {
            state.stop( status::requires_decision, value );
            throw interrupted{};
        }
        const auto &reply = state.transcript[state.consumed];
        if( !same_request( reply.expected, value ) ) {
            state.stop( status::stale, value );
            throw interrupted{};
        }
        // The transcript already owns one event. Admit its independent result copy as
        // well before allocating it; large payloads cannot evade the retention budget.
        add_size( size, event_size( reply.event ) );
        if( size > state.budget.retained_bytes - state.retained ) { fail_current( failure::resource_limit ); }
        // Advance before native validation/filter execution: a nested request is a new occurrence.
        ++state.consumed;
        return reply.event;
    } catch( const interrupted & ) { throw; }
    catch( ... ) { fail_current( failure::exception ); throw; }
}

callback_scope::callback_scope( const std::initializer_list<std::string_view> segments ) :
    exceptions_( std::uncaught_exceptions() )
{
    throw_if_incomplete();
    if( !current ) { return; }
    auto &state = *current->state_;
    if( state.callbacks.size() >= state.budget.nesting || segments.size() > maximum_nesting ||
        state.invocations == std::numeric_limits<std::uint64_t>::max() ||
        sizeof( invocation ) > state.budget.retained_bytes - state.retained ) {
        fail_current( failure::resource_limit );
    }
    auto size = sizeof( invocation );
    for( const auto segment : segments ) {
        if( segment.size() > state.budget.retained_bytes - state.retained - size ) {
            fail_current( failure::resource_limit );
        }
        size += segment.size();
    }
    try {
        auto callback = std::string{};
        callback.reserve( size - sizeof( invocation ) );
        for( const auto segment : segments ) { callback.append( segment ); }
        state.callbacks.push_back( { .callback = std::move( callback ), .occurrence = ++state.invocations } );
    } catch( ... ) { fail_current( failure::exception ); }
    state.retained += size;
    state_ = &state;
}

callback_scope::~callback_scope()
{
    if( state_ ) {
        if( std::uncaught_exceptions() > exceptions_ ) { state_->fail( failure::exception ); }
        state_->retained -= state_->callbacks.back().callback.size() + sizeof( invocation );
        state_->callbacks.pop_back();
    }
}

} // namespace game_client::evaluation
