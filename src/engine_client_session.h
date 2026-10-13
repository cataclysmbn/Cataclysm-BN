#pragma once

#include "engine_client_event.h"
#include "engine_client_wire.h"
#include "message_feed.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace engine_client
{

/// Something the connection must push at its next flush, in publication order.
struct resync_notice {
    std::string epoch = {};
    std::string reason = {};
    counter lost_after = 0;
};
struct event_push {
    std::string epoch = {};
    public_event event;
};
using push_item = std::variant<event_push, command_result, resync_notice>;

/// What one connection reads of the session's publications: every item posted after the reader
/// was opened, in order. A reader that is dropped stops receiving.
class push_reader
{
    public:
        /// Everything published since the last call; the connection decides what to send.
        auto take() -> std::vector<push_item>;
        /// The BN language id of the text the reader gets from now on; empty: the engine's language.
        /// A language other than the engine's costs the session a capture of its screens, avatar and
        /// environment in that language at each publication, shared by the readers that use it.
        auto use_locale( std::string lang_id ) -> void;
    private:
        friend class session;
        struct inbox {
            std::vector<push_item> items;
            std::string locale;
        };
        std::shared_ptr<inbox> inbox_ = std::make_shared<inbox>();
};

/// World capture hook. Called on the game thread at every input boundary and returns
/// everything the avatar currently knows (coverage, cells, entities, avatar, environment).
/// The session diffs it against the previous capture; the hook never produces events and must
/// not change game state, RNG or memory. Unset means no world is known (menus).
using world_capture_fn = auto( * )() -> world_state;
/// The same capture as a patch over the state published last, naming only the cells that can differ.
/// `consecutive`: `previous` is the state the previous patch stood for. A patch must give exactly the
/// publication of `world_capture_fn` (the oracle checks it); without the hook every publication captures
/// the whole world.
using world_patch_fn = auto( * )( const snapshot &previous, bool consecutive ) -> world_patch;
/// The sections of the world capture that hold text (avatar, environment) and no other, captured again for a
/// language other than the engine's. Unset means the world has no text of its own to word.
using text_capture_fn = auto( * )() -> world_state;

/// Game-thread authority shared by connections. Reads and hello never reset its clock.
class session final : public command_authority
{
    public:
        session();
        auto epoch() const -> const std::string &; // *NOPAD*
        auto current_permissions() const -> command_permissions override;
        auto set_phase( std::string phase ) -> void;
        auto set_world_capture( world_capture_fn capture, world_patch_fn patch = nullptr,
                                text_capture_fn text = nullptr ) -> void;
        /// With the oracle on, every publication made from a patch is checked against the publication of
        /// a full capture: the same event and the same state, or the difference is recorded (and the
        /// process aborts when `BN_CAPTURE_ORACLE` is set). Costs a full capture and diff per publication.
        auto set_oracle( bool on ) -> void;
        auto oracle_mismatches() const -> const std::vector<std::string> &; // *NOPAD*
        /// World replacement interrupts the old receipt, queues `resync` and installs a new epoch.
        auto replace_world() -> void;
        /// Capture the live boundary and publish its change as one event. A step inside a running
        /// activity (`settles` false) is not an input boundary: the executing command stays open.
        auto publish_boundary( bool settles = true ) -> std::expected<void, error>;
        /// Atomic with respect to publication: the game thread is the only writer.
        auto current() const -> std::expected<snapshot, error>;
        /// The same snapshot with the screens, avatar and environment worded in `lang_id`; the engine's
        /// language, or an empty id, gives `current()`. Message lines stay in the engine's language.
        auto current( const std::string &lang_id ) -> std::expected<snapshot, error>;
        /// The boundary last published, which the client has seen; `not_ready` before the first.
        auto boundary() const -> std::expected<boundary_state, error>;
        auto at() const -> std::optional<clock_point>;
        auto submit( command_request request ) -> std::expected<receipt, error>;
        auto prepare_input( point screen_size ) -> std::expected<game_client::input_command, error>;
        /// Return the core-validated event at the actual backend delivery seam.
        auto delivered( input_event fallback ) -> input_event;
        auto interrupt( error reason ) -> void;
        auto has_received() const -> bool;
        auto result( const result_request &request ) const -> std::expected<command_result, error>;
        /// A reader of what is published from now on. Every connection has its own.
        auto open_push() -> push_reader;
    private:
        std::string epoch_;
        std::string phase_ = "starting";
        world_capture_fn world_capture_ = nullptr;
        world_patch_fn world_patch_ = nullptr;
        text_capture_fn text_capture_ = nullptr;
        /// Whether the stream's state is what the last patch stood for, which the next one builds on.
        bool patch_consecutive_ = false;
        bool oracle_ = false;
        bool oracle_aborts_ = false;
        std::vector<std::string> oracle_mismatches_;
        std::unique_ptr<command_lifecycle> lifecycle_;
        std::optional<command_result> active_;
        std::optional<command_result> retired_;
        std::optional<command_request> input_;
        std::optional<input_event> validated_input_;
        std::optional<event_stream> stream_;
        std::vector<std::weak_ptr<push_reader::inbox>> readers_;
        /// The text sections of a language: what the connections reading it have been sent, so that the next
        /// event holds only what changed in them. Absent until a snapshot is taken in the language.
        struct worded_state {
            std::optional<boundary_state> interaction;
            std::optional<avatar_value> avatar;
            std::optional<environment_value> environment;
        };
        /// By language id, for the languages other than the engine's that a reader uses.
        std::map<std::string, std::optional<worded_state>> groups_;
        /// What a publication posts in place of its item to the readers of a language.
        using locale_items = std::map<std::string, push_item>;
        /// What the stream has already announced of the native message log.
        Messages::feed_cursor message_cursor_;
        auto post( push_item item, const locale_items &variants = {} ) -> void;
        /// Forget the languages no reader uses any more.
        auto prune_groups() -> void;
        /// The screens, and the avatar and environment, as they are now, worded in a language.
        struct wanted {
            bool interaction = true;
            bool world = true;
        };
        auto word( const std::string &lang_id,
                   wanted sections ) const -> std::expected<worded_state, error>;
        /// The event as each language in use reads it, for an event that carries text sections.
        auto worded_variants( const public_event &event ) -> locale_items;
        auto capture() const -> std::expected<state_value, error>;
        auto capture_interaction() const -> std::expected<boundary_state, error>;
        auto check_oracle( const state_value &before, const std::optional<public_event> &event ) -> void;
        auto restart_epoch( std::string_view reason ) -> void;
        auto refresh_result() -> void;
        auto publish_presentation( const std::optional<std::string> &command ) -> void;
        auto publish_messages( const std::optional<std::string> &command ) -> void;
};

auto process_session() -> session &; // *NOPAD*
/// Build string and loaded mod IDs for `bn.hello`.
auto describe_engine() -> engine_info;

} // namespace engine_client
