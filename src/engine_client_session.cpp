#include "engine_client_session.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <utility>

#include "color.h"
#include "game_session.h"
#include "get_version.h"
#include "language.h"
#include "locale_scope.h"
#include "message_types.h"
#include "options.h"
#include "output.h"
#include "profile.h"
#include "sdlsound.h"
#include "sounds.h"
#include "translations.h"
#include "world.h"
#include "worldfactory.h"

namespace engine_client
{
namespace
{
/// The sink conformance tests record into: with `BN_PRESENTATION_RECORD` set, every presentation event the
/// engine publishes is also written as one line, the `bn.events` params it was pushed as.
auto recording() -> std::ofstream * // *NOPAD*
{
    static auto out = []() {
        const auto *path = std::getenv( "BN_PRESENTATION_RECORD" );
        return path == nullptr ? std::ofstream() : std::ofstream( path, std::ios::app );
    }
    ();
    return out.is_open() ? &out : nullptr;
}
} // namespace

session::session()
{
    // Conformance runs set it for the whole process: any publication a patch gets wrong ends the game.
    oracle_aborts_ = oracle_ = std::getenv( "BN_CAPTURE_ORACLE" ) != nullptr;
    const auto created = new_session_epoch();
    if( !created ) { throw std::runtime_error( "Engine session unavailable" ); }
    epoch_ = *created;
    lifecycle_ = std::make_unique<command_lifecycle>( epoch_, *this );
}

auto session::epoch() const -> const std::string & { return epoch_; } // *NOPAD*

auto session::current_permissions() const -> command_permissions
{
    const auto stable = phase_ == "menu" || phase_ == "waiting_for_input" || phase_ == "loading";
    return { .accepts_interaction_commands = stable, .accepts_registered_actions = stable };
}

auto session::set_phase( std::string phase ) -> void { phase_ = std::move( phase ); }

auto session::set_world_capture( const world_capture_fn capture, const world_patch_fn patch,
                                 const text_capture_fn text ) -> void
{
    world_capture_ = capture;
    world_patch_ = patch;
    text_capture_ = text;
    patch_consecutive_ = false;
}

auto session::set_oracle( const bool on ) -> void { oracle_ = on; }

auto session::oracle_mismatches() const -> const std::vector<std::string> & // *NOPAD*
{
    return oracle_mismatches_;
}

auto session::refresh_result() -> void
{
    if( !active_ ) { return; }
    const auto current = lifecycle_->result( active_->command_id );
    if( !current ) { return; }
    if( current->stage != active_->stage ) { post( *current ); }
    active_ = *current;
    // A screen-scoped invoke that waited takes over once the running command has ended.
    if( auto next = lifecycle_->promote() ) {
        active_ = std::move( next->result );
        input_ = std::move( next->request );
        retired_.reset();
    }
}

auto session::interrupt( const error reason ) -> void
{
    if( active_ ) {
        // The waiting commands end first, so that none takes over from the running one.
        for( auto &ended : lifecycle_->drain( reason ) ) { post( std::move( ended ) ); }
        static_cast<void>( lifecycle_->interrupt( reason ) );
        refresh_result();
    }
    input_.reset();
    validated_input_.reset();
}

namespace
{
/// `message` and the messages nested in it without the native color tags.
auto strip_color_tags( localised_text &message ) -> void
{
    for( auto &value : message.args ) {
        if( auto *text = std::get_if<std::string>( &value ) ) { *text = remove_color_tags( *text ); }
        if( auto *nested = std::get_if<std::vector<localised_text>>( &value ) ) { std::ranges::for_each( *nested, strip_color_tags ); }
    }
}
auto to_message( const Messages::feed_entry &entry ) -> message_value
{
    const auto kind = std::ranges::find( msg_type_and_names(), entry.type,
                                         &std::pair<game_message_type, const char *>::first );
    auto text = remove_color_tags( entry.text );
    auto label = entry.count > 1 ?
                 //~ Message %s on the message log was repeated %d times, e.g. "You hear a whack! x 12"
                 string_format( _( "%s x %d" ), text, entry.count ) : text;
    auto localised = entry.localised;
    if( localised ) { strip_color_tags( *localised ); }
    return {
        .id = entry.id, .text = std::move( text ),
        .kind = kind == msg_type_and_names().end() ? "neutral" : kind->second,
        .color = get_all_colors().get_name( msgtype_to_color( entry.type ) ),
        .count = static_cast<counter>( entry.count ), .turn = static_cast<counter>( entry.turn ),
        .label = std::move( label ),
        .localised = std::move( localised )
    };
}
} // namespace

auto session::restart_epoch( const std::string_view reason ) -> void
{
    interrupt( error::stale_epoch );
    retired_ = active_;
    if( stream_ ) {
        post( resync_notice{ .epoch = epoch_, .reason = std::string{reason},
                             .lost_after = stream_->current().at.sequence } );
    }
    const auto created = new_session_epoch();
    if( !created ) { throw std::runtime_error( "Engine session unavailable" ); }
    epoch_ = *created;
    lifecycle_ = std::make_unique<command_lifecycle>( epoch_, *this );
    active_.reset();
    stream_.reset();
    groups_.clear();
    patch_consecutive_ = false;
}

auto session::replace_world() -> void { restart_epoch( "world_replaced" ); }

auto session::capture_interaction() const -> std::expected<boundary_state, error>
{
    const auto permissions = current_permissions();
    return capture_boundary( {
        .epoch = epoch_,
        .ready = {
            .phase = phase_, .game_ready = game_session::running(),
            .accepts_interaction_commands = permissions.accepts_interaction_commands,
            .accepts_registered_actions = permissions.accepts_registered_actions
        },
    } );
}

auto session::capture() const -> std::expected<state_value, error>
{
    ZoneScopedN( "engine_client_capture" );
    auto boundary = capture_interaction();
    if( !boundary ) { return std::unexpected( boundary.error() ); }
    return state_value{ .interaction = std::move( *boundary ),
                        .world = world_capture_ ? world_capture_() : world_state{} };
}

auto session::check_oracle( const state_value &before,
                            const std::vector<public_event> &events ) -> void
{
    const auto &now = stream_->current().value;
    const auto full = state_value{ .interaction = now.interaction,
                                   .world = world_capture_ ? world_capture_() : world_state{} };
    const auto expected = diff( before, full );
    // A change published as several events is the first one's with the cells of the others.
    auto actual = events.empty() ? changes{} :
                  events.front().value().delta;
    for( const auto &event : events | std::views::drop( 1 ) ) {
        std::ranges::copy( event.value().delta.cells, std::back_inserter( actual.cells ) );
    }
    auto differs = std::string{};
    const auto note = [&]( const char *section, const bool same ) { if( !same ) { differs += std::string( " " ) + section; } };
    note( "coverage", actual.coverage == expected.coverage );
    note( "view", actual.view == expected.view );
    note( "cells", actual.cells == expected.cells );
    note( "forgotten", actual.forgotten == expected.forgotten );
    note( "entities", actual.entities == expected.entities );
    note( "gone", actual.gone == expected.gone );
    note( "avatar", actual.avatar == expected.avatar && actual.avatar_kept == expected.avatar_kept );
    note( "environment", actual.environment == expected.environment );
    note( "route", actual.route == expected.route );
    note( "interaction", actual.interaction.has_value() == expected.interaction.has_value() &&
          ( !actual.interaction || same_boundary( *actual.interaction, *expected.interaction ) ) &&
          actual.actions_unchanged == expected.actions_unchanged );
    note( "state", same_state( now, full ) );
    if( differs.empty() ) { return; }
    oracle_mismatches_.push_back( "patch publication differs from the full capture in:" + differs );
    std::cerr << "engine_client: " << oracle_mismatches_.back() << std::endl;
    if( oracle_aborts_ ) { std::abort(); }
    // Go on from the full capture, so that one miss does not hide the next.
    static_cast<void>( stream_->rebase( full ) );
    groups_.clear();
    patch_consecutive_ = false;
}

auto session::publish_boundary( const bool settles ) -> std::expected<void, error>
{
    ZoneScopedN( "engine_client_publish_boundary" );
    // Startup language/debug prompts are genuine native input boundaries, not engine readiness.
    if( phase_ == "starting" ) { phase_ = "menu"; }
    if( !stream_ ) {
        auto candidate = capture();
        if( !candidate ) { return std::unexpected( candidate.error() ); }
        auto created = event_stream::create( epoch_, std::move( *candidate ) );
        if( !created ) { return std::unexpected( created.error() ); }
        stream_ = std::move( *created );
        groups_.clear();
        patch_consecutive_ = false;
        // The whole log is new to this stream: a loaded game's saved lines and a new game's first
        // line were written before it existed. They enter the stream's log, which a snapshot carries.
        message_cursor_ = {};
        auto seeded = std::vector<message_value> {};
        for( const auto &entry : Messages::feed_since( message_cursor_ ) ) {
            message_cursor_ = { .id = entry.id, .count = entry.count };
            seeded.push_back( to_message( entry ) );
        }
        stream_->seed_log( std::move( seeded ) );
        presentation::collect( true );
        fill_groups();
        return {};
    }
    auto interaction = capture_interaction();
    if( !interaction ) { return std::unexpected( interaction.error() ); }
    auto command = std::optional<std::string> {};
    if( active_ && active_->stage == command_stage::executing ) { command = active_->command_id; }
    // A patch names the cells that can differ from the stream's state, so what it costs is what changed.
    auto patch = world_patch_ ? std::optional( world_patch_( stream_->current(),
                 patch_consecutive_ ) ) :
                 std::nullopt;
    publish_presentation( command );
    auto before = oracle_ && patch ? std::optional( stream_->current().value ) : std::nullopt;
    auto candidate = std::optional<state_value> {};
    auto published = std::expected<std::vector<public_event>, error> {};
    if( patch ) {
        ZoneScopedN( "engine_client_stream_publish" );
        published = stream_->publish( patch_request{ .interaction = *interaction, .patch = std::move( *patch ),
                                      .command = command } );
    }
    patch_consecutive_ = patch && published;
    if( !patch_consecutive_ ) {
        // No patch, or one the stream cannot take: the whole capture, whose errors recover below.
        candidate = state_value{ .interaction = std::move( *interaction ),
                                 .world = world_capture_ ? world_capture_() : world_state{} };
        ZoneScopedN( "engine_client_stream_publish" );
        published = stream_->publish( publish_request{ .next = *candidate, .command = command } );
    }
    if( published ) {
        for( const auto &event : *published ) { post( event_push{ .epoch = epoch_, .event = event }, worded_variants( event ) ); }
        if( before && patch_consecutive_ ) { check_oracle( *before, *published ); }
    } else if( published.error() == error::resource_limit ||
               published.error() == error::resync_required ) {
        // A change no events can carry, or one that cannot be expressed: adopt the state and make
        // subscribers start over.
        post( resync_notice{ .epoch = epoch_,
                             .reason = published.error() == error::resource_limit ? "overflow" : "state_removed",
                             .lost_after = stream_->current().at.sequence } );
        // The languages start over from a snapshot, like every subscriber.
        groups_.clear();
        if( const auto rebased = stream_->rebase( std::move( *candidate ) ); !rebased ) { return rebased; }
    } else { return std::unexpected( published.error() ); }
    fill_groups();
    publish_messages( command );
    if( settles && active_ && active_->stage == command_stage::executing ) {
        const auto &now = stream_->current();
        const auto completed = lifecycle_->complete_at_boundary( now.at, now.value.interaction );
        if( !completed && completed.error() != error::invalid_lifecycle ) {
            interrupt( completed.error() );
            return std::unexpected( completed.error() );
        }
        refresh_result();
    }
    return {};
}

auto session::publish_presentation( const std::optional<std::string> &command ) -> void
{
    for( auto &fact : presentation::take() ) {
        // A fact that cannot be published is skipped; it changes no state.
        auto published = stream_->publish_presentation( { .fact = std::move( fact ), .command = command } );
        if( !published ) { continue; }
        if( auto *out = recording() ) { *out << published->wire() << std::endl; }
        post( event_push{ .epoch = epoch_, .event = std::move( *published ) } );
    }
}

auto session::publish_messages( const std::optional<std::string> &command ) -> void
{
    for( const auto &entry : Messages::feed_since( message_cursor_ ) ) {
        message_cursor_ = { .id = entry.id, .count = entry.count };
        auto published = stream_->publish_message( { .message = to_message( entry ), .command = command } );
        // A line that cannot be published is skipped; the log itself still holds it.
        if( published ) { post( event_push{ .epoch = epoch_, .event = std::move( *published ) } ); }
    }
}

auto session::current() const -> std::expected<snapshot, error>
{
    if( !stream_ ) { return std::unexpected( error::not_ready ); }
    return stream_->current();
}

auto session::current( const std::string &lang_id ) -> std::expected<snapshot, error>
{
    auto result = current();
    if( !result || lang_id.empty() || lang_id == get_language().id ) { return result; }
    // The state a language starts from is the one captured at a publication, which the stream holds too.
    const auto group = groups_.find( lang_id );
    if( group == groups_.end() || !group->second ) { return std::unexpected( error::not_ready ); }
    const auto &state = group->second;
    result->value.interaction = *state->interaction;
    // Without a text capture the world has no wording of its own, and the engine's stands.
    if( state->avatar ) { result->value.world.avatar = state->avatar; }
    if( state->environment ) { result->value.world.environment = state->environment; }
    return result;
}

auto session::prepare_locale( const std::string &lang_id ) -> void
{
    if( !stream_ || lang_id.empty() || lang_id == get_language().id ) { return; }
    auto &state = groups_[lang_id];
    if( state ) { return; }
    if( auto worded = word( lang_id, {} ) ) { state = std::move( *worded ); }
}

auto session::fill_groups() -> void
{
    prune_groups();
    for( const auto &reader : readers_ ) {
        const auto &lang_id = reader.lock()->locale;
        if( !lang_id.empty() && lang_id != get_language().id ) { prepare_locale( lang_id ); }
    }
}

auto session::word( const std::string &lang_id,
                    const wanted sections ) const -> std::expected<worded_state, error>
{
    ZoneScopedN( "engine_client_word" );
    const auto scope = localization::locale_scope{ lang_id };
    auto result = worded_state{};
    if( sections.interaction ) {
        auto interaction = capture_interaction();
        if( !interaction ) { return std::unexpected( interaction.error() ); }
        result.interaction = std::move( *interaction );
    }
    if( sections.world && text_capture_ ) {
        auto world = text_capture_();
        result.avatar = std::move( world.avatar );
        result.environment = std::move( world.environment );
    }
    return result;
}

auto session::prune_groups() -> void
{
    std::erase_if( readers_, []( const auto & reader ) { return reader.expired(); } );
    std::erase_if( groups_, [this]( const auto & group ) {
        return std::ranges::none_of( readers_, [&]( const auto & reader ) { return reader.lock()->locale == group.first; } );
    } );
}

auto session::worded_variants( const public_event &event ) -> locale_items
{
    auto variants = locale_items{};
    const auto &delta = event.value().delta;
    if( !delta.interaction && !delta.avatar && !delta.environment ) { return variants; }
    prune_groups();
    for( auto &[lang_id, state] : groups_ ) {
        if( !state ) { continue; }
        auto worded = word( lang_id, { .interaction = delta.interaction.has_value(), .world = delta.avatar || delta.environment } );
        auto value = event.value();
        auto &changed = value.delta;
        if( worded && changed.interaction ) {
            changed.actions_unchanged = state->interaction &&
                                        state->interaction->actions == worded->interaction->actions;
            changed.interaction = *worded->interaction;
        }
        if( worded && changed.avatar && worded->avatar ) {
            changed.avatar_kept = state->avatar ? kept_members( *state->avatar,
                                  *worded->avatar ) : std::vector<std::string> {};
            changed.avatar = *worded->avatar;
        }
        if( worded && changed.environment && worded->environment ) { changed.environment = worded->environment; }
        auto sealed = worded ? public_event::sealed( std::move( value ), epoch_ ) : std::nullopt;
        if( !sealed ) {
            // Its text does not fit an event or could not be captured: the readers start over from a snapshot.
            variants.emplace( lang_id, resync_notice{ .epoch = epoch_, .reason = "overflow", .lost_after = event.value().sequence - 1 } );
            state.reset();
            continue;
        }
        if( worded->interaction ) { state->interaction = std::move( worded->interaction ); }
        if( worded->avatar ) { state->avatar = std::move( worded->avatar ); }
        if( worded->environment ) { state->environment = std::move( worded->environment ); }
        variants.emplace( lang_id, event_push{ .epoch = epoch_, .event = std::move( *sealed ) } );
    }
    return variants;
}

auto session::boundary() const -> std::expected<boundary_state, error>
{
    if( !stream_ ) { return std::unexpected( error::not_ready ); }
    return stream_->current().value.interaction;
}

auto session::at() const -> std::optional<clock_point>
{
    if( !stream_ ) { return std::nullopt; }
    return stream_->current().at;
}

auto session::standing() const -> avatar_standing
{
    if( !stream_ ) { return {}; }
    const auto &world = stream_->current().value.world;
    return { .at = world.avatar ? std::optional{ world.avatar->at } :
                   std::nullopt,
             .turn = world.environment ? world.environment->turn : std::string{} };
}

auto session::submit( command_request request ) -> std::expected<receipt, error>
{
    if( lifecycle_->busy() ) {
        // Only a screen-scoped invoke may wait; it keeps its receipt and is validated when it runs.
        const auto waiting = lifecycle_->enqueue( std::move( request ) );
        if( !waiting ) { return std::unexpected( waiting.error() ); }
        return receipt{ .epoch = waiting->epoch, .command_id = waiting->command_id };
    }
    auto input = request;
    const auto received = lifecycle_->submit( std::move( request ) );
    if( !received ) { return std::unexpected( received.error() ); }
    active_ = *received;
    input_ = std::move( input );
    retired_.reset();
    return receipt{ .epoch = received->epoch, .command_id = received->command_id };
}

auto session::has_received() const -> bool
{
    return active_ && active_->stage == command_stage::received;
}

auto session::prepare_input( const point screen_size ) ->
std::expected<game_client::input_command, error>
{
    if( !stream_ || !input_ ) { return std::unexpected( error::not_ready ); }
    const auto &now = stream_->current();
    const auto validated = lifecycle_->validate( now.at, now.value.interaction, screen_size );
    // A rejected command is over, and the one that waited behind it takes its place.
    if( !validated ) { input_.reset(); }
    refresh_result();
    if( !validated ) { return std::unexpected( validated.error() ); }
    validated_input_ = *validated;
    auto native = game_client::input_command{ .input_id = game_client::current_input_id() };
    if( const auto semantic = std::get_if<semantic_operation>( &input_->operation ) ) {
        // Semantic commands carry their boundary inside the operation, not the raw-input field.
        native.input_id.reset();
        native.interaction = semantic->command;
        native.interaction->input_id = game_client::current_input_id();
        if( semantic->target ) {
            native.interaction->position = to_bubble( *semantic->target, current_bubble_frame() );
        }
    } else if( const auto travel = std::get_if<travel_operation>( &input_->operation ) ) {
        native = *travel_click( *travel );
    } else if( std::holds_alternative<item_operation>( input_->operation ) ||
               std::holds_alternative<tile_operation>( input_->operation ) ||
               std::holds_alternative<invoke_operation>( input_->operation ) ) {
        // The native input only wakes the read; the validated event takes its place at the delivery seam.
        native = item_operation_input();
    } else {
        native.action = std::get<registered_action>( input_->operation ).id;
    }
    input_.reset();
    return native;
}

auto session::delivered( input_event fallback ) -> input_event
{
    if( active_ && active_->stage == command_stage::validated && validated_input_ ) {
        if( lifecycle_->execution_started() ) {
            refresh_result();
            auto validated = std::exchange( validated_input_, std::nullopt );
            return std::move( *validated );
        }
        interrupt( error::not_ready );
    }
    return fallback;
}

auto session::result( const result_request &request ) const -> std::expected<command_result, error>
{
    if( retired_ && request.epoch == retired_->epoch &&
        request.command_id == retired_->command_id ) { return *retired_; }
    if( request.epoch != epoch_ ) { return std::unexpected( error::stale_epoch ); }
    return lifecycle_->result( request.command_id );
}

auto push_reader::take() -> std::vector<push_item> { return std::exchange( inbox_->items, {} ); }

auto push_reader::use_locale( std::string lang_id ) -> void { inbox_->locale = std::move( lang_id ); }

auto session::open_push() -> push_reader
{
    auto reader = push_reader{};
    readers_.push_back( reader.inbox_ );
    return reader;
}

auto session::post( push_item item, const locale_items &variants ) -> void
{
    std::erase_if( readers_, []( const auto & reader ) { return reader.expired(); } );
    // A reader of a language that has a variant gets it; the others get the item.
    const auto variant_for = [&]( const push_reader::inbox & inbox ) -> const push_item * { // *NOPAD*
        const auto found = variants.find( inbox.locale );
        return found == variants.end() ? nullptr :&found->second;
    };
    // The last reader takes the item itself; with one connection nothing is copied.
    for( const auto &reader : readers_ | std::views::take( readers_.size() - 1 ) ) {
        const auto inbox = reader.lock();
        const auto *variant = variant_for( *inbox );
        inbox->items.push_back( variant ? *variant : item );
    }
    if( readers_.empty() ) { return; }
    const auto inbox = readers_.back().lock();
    if( const auto *variant = variant_for( *inbox ) ) { inbox->items.push_back( *variant ); }
    else { inbox->items.push_back( std::move( item ) ); }
}

namespace
{
/// The folder the sound pack was loaded from, while sound is on.
auto soundpack_folder() -> std::string { return sounds::sound_enabled ? soundpack_path() : std::string{}; }
} // namespace

auto describe_engine() -> engine_info
{
    auto result = engine_info{
        .build = getVersionString(),
        .use_tiles = get_option<bool>( "USE_TILES" ),
        .tileset = get_option<std::string>( "TILES" ),
        .soundpack = soundpack_folder(),
        .locale = get_language().id,
    };
    if( world_generator && world_generator->active_world && world_generator->active_world->info ) {
        for( const auto &mod : world_generator->active_world->info->active_mod_order ) {
            result.mods.push_back( mod.str() );
        }
    }
    return result;
}

auto process_session() -> session & // *NOPAD*
{
    static auto value = session{};
    return value;
}

} // namespace engine_client
