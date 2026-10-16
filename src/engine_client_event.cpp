#include "engine_client_event.h"

#include <algorithm>
#include <iterator>
#include <cstdint>
#include <limits>
#include <set>
#include <type_traits>
#include <utility>

#include "profile.h"

namespace engine_client
{
namespace
{
// The MSVC standard library's node containers do not promise noexcept move assignment.
#if !defined(_MSC_VER)
static_assert( std::is_nothrow_move_assignable_v<state_value> );
static_assert( std::is_nothrow_move_assignable_v<snapshot> );
#endif

/// One event must fit a frame with room for its siblings in the same notification.
constexpr auto maximum_event_bytes = maximum_frame_bytes / 2;

auto inside( const bounds &area, const position &at ) -> bool
{
    return at.dim == area.min.dim && at.x >= area.min.x && at.x <= area.max.x &&
           at.y >= area.min.y && at.y <= area.max.y && at.z >= area.min.z && at.z <= area.max.z;
}
auto valid_event( const event_value &value ) -> bool
{
    const auto message = value.type == "message.logged";
    const auto transient = message || value.presentation.has_value();
    return value.sequence != 0 && ( value.revision != 0 || transient ) && !value.type.empty() &&
           message == value.message.has_value() && ( !transient || value.delta.empty() ) &&
           ( !value.presentation || ( !message && value.presentation->type == value.type ) ) &&
           ( !value.message || ( value.message->id != 0 && !value.message->text.empty() &&
                                 value.message->count != 0 && !value.message->label.empty() ) ) &&
           ( !value.cause || ( *value.cause != 0 && *value.cause < value.sequence ) ) &&
           ( !value.command || ( !value.command->empty() && value.command->size() <= maximum_id_bytes ) );
}
/// Knowledge decides which facts a value may carry (design 3.3): remembered cells only their
/// memory, sensed cells and entities no appearance. A capture that violates it never publishes.
auto valid_cell( const cell &value ) -> bool
{
    const auto live = value.terrain || value.furniture || !value.fields.empty() ||
                      !value.traps.empty() ||
                      !value.items.empty() || value.vehicle || value.light || value.reviving ||
                      value.cargo;
    switch( value.known ) {
        case knowledge::remembered:
            return !live && value.memory;
        case knowledge::visible:
            return true;
        case knowledge::sensed:
            return !live && !value.memory;
    }
    return false;
}
auto valid_entity( const entity &value ) -> bool
{
    switch( value.known ) {
        case knowledge::visible:
            return !value.sense;
        case knowledge::sensed:
            return !value.appearance && !value.name && value.overlays.empty() && value.traits.empty() &&
                   !value.attitude &&
                   !value.aware &&
                   value.statuses.empty();
        case knowledge::remembered:
            return false;
    }
    return false;
}
auto valid_world( const world_state &world ) -> bool
{
    return std::ranges::all_of( world.cells, []( const auto & entry ) { return valid_cell( entry.second ); } )
    &&
    std::ranges::all_of( world.entities, []( const auto & entry ) { return valid_entity( entry.second ); } );
}
auto validate_state( const state_value &value ) -> std::expected<void, error>
{
    if( !valid_world( value.world ) ) { return std::unexpected( error::validation_failed ); }
    return validate_boundary( value.interaction );
}
/// The diff cannot say that a coverage, avatar or environment disappeared.
auto removes_value( const world_state &from, const world_state &to ) -> bool
{
    return ( from.coverage && !to.coverage ) || ( from.view && !to.view ) ||
           ( from.avatar && !to.avatar ) || ( from.environment && !to.environment );
}
template<typename Key, typename Value, typename Remove>
auto erase_where( std::map<Key, Value> &values, const Remove &remove ) -> void
{
    for( auto it = values.begin(); it != values.end(); ) {
        if( remove( *it ) ) { it = values.erase( it ); }
        else { ++it; }
    }
}
} // namespace

auto changes::empty() const -> bool
{
    return !coverage && !view && cells.empty() && forgotten.empty() && entities.empty() &&
           gone.empty() &&
           !avatar && !environment && !route && !interaction;
}
auto same_state( const state_value &left, const state_value &right ) -> bool
{
    return left.world == right.world && same_boundary( left.interaction, right.interaction );
}

namespace
{
/// Whether a cell or entity at `at` is still inside the coverage `delta` moves to, if it does.
auto survives( const changes &delta, const position &at ) -> bool
{
    return !delta.coverage || inside( *delta.coverage, at );
}
/// Everything of the diff but the cells.
auto diff_sections( const state_value &from, const world_state &world,
                    const boundary_state &interaction ) -> changes
{
    auto result = changes{};
    if( world.coverage && world.coverage != from.world.coverage ) { result.coverage = world.coverage; }
    if( world.view && world.view != from.world.view ) { result.view = world.view; }
    for( const auto &[id, value] : world.entities ) {
        const auto old = from.world.entities.find( id );
        if( old == from.world.entities.end() || old->second != value ) { result.entities.push_back( value ); }
    }
    for( const auto &[id, value] : from.world.entities ) {
        if( survives( result, value.at ) && !world.entities.contains( id ) ) { result.gone.push_back( {.id = id} ); }
    }
    if( world.avatar && world.avatar != from.world.avatar ) {
        result.avatar = world.avatar;
        if( from.world.avatar ) { result.avatar_kept = kept_members( *from.world.avatar, *world.avatar ); }
    }
    if( world.environment && world.environment != from.world.environment ) {
        result.environment = world.environment;
    }
    if( world.route != from.world.route ) { result.route = world.route; }
    if( !same_boundary( from.interaction, interaction ) ) {
        result.interaction = interaction;
        result.actions_unchanged = from.interaction.actions == interaction.actions;
    }
    return result;
}
} // namespace

auto kept_members( const avatar_value &before,
                   const avatar_value &after ) -> std::vector<std::string>
{
    auto kept = std::vector<std::string> {};
    const auto keep = [&]( const char *member, const bool same, const bool empty ) {
        if( same && !empty ) { kept.emplace_back( member ); }
    };
    keep( "look", before.appearance == after.appearance, !after.appearance );
    keep( "overlays", before.overlays == after.overlays, after.overlays.empty() );
    keep( "traits", before.traits == after.traits, after.traits.empty() );
    keep( "stats", before.stats == after.stats, after.stats.empty() );
    keep( "inventory", before.inventory == after.inventory, after.inventory.empty() );
    keep( "sidebar", before.sidebar == after.sidebar, false );
    keep( "compass", before.compass == after.compass, after.compass.empty() );
    keep( "ground", before.ground == after.ground, after.ground.empty() );
    return kept;
}

auto diff( const state_value &from, const state_value &to ) -> changes
{
    auto result = diff_sections( from, to.world, to.interaction );
    // Cells and entities outside a new coverage are dropped by the coverage itself.
    for( const auto &[at, value] : to.world.cells ) {
        const auto old = from.world.cells.find( at );
        if( old == from.world.cells.end() || old->second != value ) { result.cells.push_back( value ); }
    }
    for( const auto &[at, value] : from.world.cells ) {
        if( survives( result, at ) && !to.world.cells.contains( at ) ) { result.forgotten.push_back( at ); }
    }
    return result;
}

auto diff( const state_value &from, const boundary_state &interaction,
           const world_patch &patch ) -> changes
{
    auto result = diff_sections( from, patch.world, interaction );
    const auto &known = from.world.cells;
    for( const auto &[at, value] : patch.cells ) {
        const auto old = known.find( at );
        if( old == known.end() || old->second != value ) { result.cells.push_back( value ); }
    }
    if( patch.exhaustive ) {
        for( const auto &[at, value] : known ) {
            if( survives( result, at ) && !patch.cells.contains( at ) ) { result.forgotten.push_back( at ); }
        }
    } else {
        for( const auto &at : patch.unknown ) {
            if( known.contains( at ) && survives( result, at ) ) { result.forgotten.push_back( at ); }
        }
    }
    return result;
}

auto apply( state_value &value, const changes &delta ) -> std::expected<void, error>
{
    auto next = value;
    auto &world = next.world;
    if( delta.coverage ) {
        world.coverage = delta.coverage;
        erase_where( world.cells, [&]( const auto & entry ) { return !inside( *delta.coverage, entry.first ); } );
        erase_where( world.entities, [&]( const auto & entry ) { return !inside( *delta.coverage, entry.second.at ); } );
    }
    if( delta.view ) { world.view = delta.view; }
    auto seen_cells = std::set<position> {};
    for( const auto &entry : delta.cells ) {
        if( !world.coverage || !inside( *world.coverage, entry.at ) ||
            !seen_cells.insert( entry.at ).second ) {
            return std::unexpected( error::resync_required );
        }
        world.cells[entry.at] = entry;
    }
    for( const auto &at : delta.forgotten ) {
        if( world.cells.erase( at ) == 0 ) { return std::unexpected( error::resync_required ); }
    }
    auto seen_entities = std::set<std::string> {};
    for( const auto &entry : delta.entities ) {
        if( !seen_entities.insert( entry.id ).second ) { return std::unexpected( error::resync_required ); }
        world.entities[entry.id] = entry;
    }
    for( const auto &entry : delta.gone ) {
        if( world.entities.erase( entry.id ) == 0 ) { return std::unexpected( error::resync_required ); }
    }
    if( delta.avatar ) { world.avatar = delta.avatar; }
    if( delta.environment ) { world.environment = delta.environment; }
    if( delta.route ) { world.route = *delta.route; }
    if( delta.interaction ) { next.interaction = *delta.interaction; }
    value = std::move( next );
    return {};
}

auto read_cells( const cells_request &request, const snapshot &current )
-> std::expected<cells_page, error>
{
    if( request.epoch != current.at.epoch ) { return std::unexpected( error::stale_epoch ); }
    const auto box = bounds{ .min = request.min, .max = request.max };
    if( box.min.dim != box.max.dim || box.min.z != box.max.z || box.min.x > box.max.x ||
        box.min.y > box.max.y ) { return std::unexpected( error::validation_failed ); }
    const auto width = std::int64_t{box.max.x} - box.min.x + 1;
    const auto height = std::int64_t{box.max.y} - box.min.y + 1;
    if( width * height > static_cast<std::int64_t>( cells_per_query ) ) {
        return std::unexpected( error::resource_limit );
    }
    const auto &world = current.value.world;
    auto page = cells_page{ .at = current.at };
    for( auto y = box.min.y; y <= box.max.y; ++y ) {
        for( auto x = box.min.x; x <= box.max.x; ++x ) {
            const auto at = position{ .dim = box.min.dim, .x = x, .y = y, .z = box.min.z };
            if( const auto known = world.cells.find( at ); known != world.cells.end() ) {
                page.cells.push_back( known->second );
            } else if( world.coverage && inside( *world.coverage, at ) ) {
                page.forgotten.push_back( at );
            }
        }
    }
    // A page that cannot be sent is a recoverable error: the client asks for a smaller box.
    if( serialize_cells( page ).size() > maximum_inline_bytes ) {
        return std::unexpected( error::resource_limit );
    }
    return page;
}

auto classify( const changes &delta ) -> std::string
{
    if( delta.coverage ) { return "coverage.moved"; }
    if( !delta.cells.empty() || !delta.forgotten.empty() || !delta.entities.empty() ||
        !delta.gone.empty() || delta.avatar || delta.route || delta.view ) { return "cells.seen"; }
    if( delta.environment ) { return "turn.passed"; }
    return "interaction.changed";
}

auto remember( std::vector<message_value> &log, const message_value &line ) -> void
{
    const auto old = std::ranges::find( log, line.id, &message_value::id );
    if( old != log.end() ) {
        *old = line;
        return;
    }
    log.push_back( line );
    if( log.size() > maximum_log_lines ) { log.erase( log.begin() ); }
}

public_event::public_event( event_value value ) : value_( std::move( value ) ) {}
auto public_event::value() const -> const event_value & { return value_; } // *NOPAD*
auto public_event::wire() const -> const std::string & { return wire_; } // *NOPAD*
auto public_event::element() const -> std::string_view
{
    // The wire is the event inside `{"epoch":...,"events":[` and `]}`.
    return wire_.empty() ? std::string_view{} :
           std::string_view( wire_ ).substr( element_at_, wire_.size() - element_at_ - 2 );
}

event_stream::event_stream( snapshot initial ) : current_( std::move( initial ) ) {}
auto event_stream::create( std::string epoch,
                           state_value initial ) -> std::expected<event_stream, error>
{
    if( epoch.empty() || epoch.size() > maximum_id_bytes ) { return std::unexpected( error::validation_failed ); }
    if( const auto valid = validate_state( initial ); !valid ) { return std::unexpected( valid.error() ); }
    return event_stream{snapshot{.at = {.epoch = std::move( epoch )}, .value = std::move( initial )}};
}
auto event_stream::seed_log( std::vector<message_value> lines ) -> void
{
    for( const auto &line : lines ) { remember( current_.log, line ); }
}
auto event_stream::current() const -> const snapshot & { return current_; } // *NOPAD*

auto public_event::seal( const std::string &epoch ) -> bool
{
    ZoneScopedN( "engine_client_serialize_event" );
    wire_ = serialize_events( {.epoch = epoch, .events = {*this}} );
    // A quote inside the epoch is escaped, so the first `,"events":[` is the key's.
    constexpr auto events_key = std::string_view( R"(,"events":[)" );
    element_at_ = wire_.find( events_key ) + events_key.size();
    return wire_.size() <= maximum_event_bytes;
}
auto public_event::sealed( event_value value,
                           const std::string &epoch ) -> std::optional<public_event>
{
    auto event = public_event{ std::move( value ) };
    if( !event.seal( epoch ) ) { return std::nullopt; }
    return event;
}

auto event_stream::issue( changes delta, std::optional<std::string> command,
                          std::optional<counter> cause ) -> std::expected<std::vector<public_event>, error>
{
    if( delta.empty() ) { return std::vector<public_event> {}; }
    if( current_.at.revision == std::numeric_limits<counter>::max() ||
        current_.at.sequence == std::numeric_limits<counter>::max() ) {
        return std::unexpected( error::resource_limit );
    }
    auto type = classify( delta );
    auto event = public_event{{
            .sequence = current_.at.sequence + 1,
            .revision = current_.at.revision + 1,
            .type = std::move( type ),
            .cause = cause,
            .command = command,
            .delta = std::move( delta ),
        }};
    if( !valid_event( event.value() ) ) { return std::unexpected( error::validation_failed ); }
    if( event.seal( current_.at.epoch ) ) {
        current_.at.sequence = event.value().sequence;
        current_.at.revision = event.value().revision;
        return std::vector{ std::move( event ) };
    }
    // A delta too large for one event goes out as two, the latter half of its cells after the rest, so a
    // receiver applies the change without a snapshot.
    auto &whole = event.value_.delta;
    if( whole.cells.size() < 2 ) { return std::unexpected( error::resource_limit ); }
    const auto half = whole.cells.begin() + static_cast<std::ptrdiff_t>( whole.cells.size() / 2 );
    auto rest = changes{ .cells = std::vector<cell>( std::make_move_iterator( half ),
                                  std::make_move_iterator( whole.cells.end() ) ) };
    whole.cells.erase( half, whole.cells.end() );
    const auto at = current_.at;
    auto events = issue( std::move( whole ), command, cause );
    auto after = events ? issue( std::move( rest ), std::move( command ), cause ) : events;
    if( !after ) {
        current_.at = at;
        return after;
    }
    std::ranges::move( *after, std::back_inserter( *events ) );
    return events;
}

auto event_stream::publish( publish_request request ) ->
std::expected<std::vector<public_event>, error>
{
    if( request.decision == disclosure::withheld ) { return std::vector<public_event> {}; }
    {
        ZoneScopedN( "engine_client_validate_state" );
        if( const auto valid = validate_state( request.next ); !valid ) { return std::unexpected( valid.error() ); }
    }
    if( removes_value( current_.value.world, request.next.world ) ) { return std::unexpected( error::resync_required ); }
    if( request.cause && ( *request.cause == 0 || *request.cause > current_.at.sequence ) ) {
        return std::unexpected( error::validation_failed );
    }
    auto delta = [&] {
        ZoneScopedN( "engine_client_diff" );
        return diff( current_.value, request.next );
    }();
    auto events = issue( std::move( delta ), std::move( request.command ), request.cause );
    if( events && !events->empty() ) { current_.value = std::move( request.next ); }
    return events;
}

auto event_stream::publish( patch_request request ) ->
std::expected<std::vector<public_event>, error>
{
    auto &patch = request.patch;
    {
        ZoneScopedN( "engine_client_validate_state" );
        const auto valid = std::ranges::all_of( patch.cells, []( const auto & entry ) { return valid_cell( entry.second ); } )
        &&
        std::ranges::all_of( patch.world.entities, []( const auto & entry ) { return valid_entity( entry.second ); } );
        if( !valid ) { return std::unexpected( error::validation_failed ); }
        if( const auto boundary = validate_boundary( request.interaction ); !boundary ) { return std::unexpected( boundary.error() ); }
    }
    if( removes_value( current_.value.world, patch.world ) ) { return std::unexpected( error::resync_required ); }
    auto delta = [&] {
        ZoneScopedN( "engine_client_diff" );
        return diff( current_.value, request.interaction, patch );
    }();
    auto events = issue( std::move( delta ), std::move( request.command ), std::nullopt );
    if( !events || events->empty() ) { return events; }
    auto &world = current_.value.world;
    auto cells = std::move( world.cells );
    if( patch.exhaustive ) {
        cells = std::move( patch.cells );
    } else {
        for( const auto &event : *events ) {
            const auto &changed = event.value().delta;
            if( changed.coverage ) { erase_where( cells, [&]( const auto & entry ) { return !inside( *changed.coverage, entry.first ); } ); }
            for( const auto &at : changed.forgotten ) { cells.erase( at ); }
            for( const auto &entry : changed.cells ) { cells.insert_or_assign( entry.at, std::move( patch.cells.at( entry.at ) ) ); }
        }
    }
    world = std::move( patch.world );
    world.cells = std::move( cells );
    current_.value.interaction = std::move( request.interaction );
    return events;

}

auto event_stream::publish_message( message_request request ) -> std::expected<public_event, error>
{
    if( current_.at.sequence == std::numeric_limits<counter>::max() ) {
        return std::unexpected( error::resource_limit );
    }
    auto event = public_event{{
            .sequence = current_.at.sequence + 1,
            .revision = current_.at.revision,
            .type = "message.logged",
            .command = std::move( request.command ),
            .message = std::move( request.message ),
        }};
    if( !valid_event( event.value() ) ) { return std::unexpected( error::validation_failed ); }
    if( !event.seal( current_.at.epoch ) ) { return std::unexpected( error::resource_limit ); }
    current_.at.sequence = event.value().sequence;
    remember( current_.log, *event.value().message );
    return event;
}

auto event_stream::publish_presentation( presentation_request request ) ->
std::expected<public_event, error>
{
    if( current_.at.sequence == std::numeric_limits<counter>::max() ) {
        return std::unexpected( error::resource_limit );
    }
    auto event = public_event{{
            .sequence = current_.at.sequence + 1,
            .revision = current_.at.revision,
            .type = request.fact.type,
            .command = std::move( request.command ),
            .presentation = std::move( request.fact ),
        }};
    if( !valid_event( event.value() ) ) { return std::unexpected( error::validation_failed ); }
    if( !event.seal( current_.at.epoch ) ) { return std::unexpected( error::resource_limit ); }
    current_.at.sequence = event.value().sequence;
    return event;
}

auto event_stream::rebase( state_value next ) -> std::expected<void, error>
{
    if( const auto valid = validate_state( next ); !valid ) { return valid; }
    if( current_.at.revision == std::numeric_limits<counter>::max() ) {
        return std::unexpected( error::resource_limit );
    }
    current_.value = std::move( next );
    ++current_.at.revision;
    return {};
}

auto apply_event( snapshot &receiver, const std::string &epoch, const public_event &event )
-> std::expected<void, error>
{
    const auto &value = event.value();
    if( !valid_event( value ) ) { return std::unexpected( error::resync_required ); }
    if( epoch != receiver.at.epoch || receiver.at.sequence == std::numeric_limits<counter>::max() ||
        value.sequence != receiver.at.sequence + 1 ||
        value.revision != receiver.at.revision + ( value.delta.empty() ? 0 : 1 ) ) {
        return std::unexpected( error::resync_required );
    }
    auto next = receiver.value;
    if( const auto applied = apply( next, value.delta ); !applied ) { return applied; }
    receiver.value = std::move( next );
    if( value.message ) { remember( receiver.log, *value.message ); }
    receiver.at.sequence = value.sequence;
    receiver.at.revision = value.revision;
    return {};
}
auto apply_batch( snapshot &receiver, const event_batch &batch ) -> std::expected<void, error>
{
    auto candidate = receiver;
    for( const auto &event : batch.events ) {
        if( const auto applied = apply_event( candidate, batch.epoch, event ); !applied ) { return applied; }
    }
    receiver = std::move( candidate );
    return {};
}
} // namespace engine_client
