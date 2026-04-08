// COMPAT SHIM — isolated compatibility layer for legacy TALK_TOPIC JSON dialogue.
// Remove this file when the ecosystem has fully migrated to Yarn Spinner .yarn files.

// Set enable_legacy_shim = false to force the original dialogue loop in
// npc::talk_to_u() for regression testing against the old code path.
static constexpr bool enable_legacy_shim = true;

#include "dialogue_compat.h"

#include "dialogue.h"
#include "dialogue_win.h"
#include "npc.h"
#include "player.h"
#include "yarn_dialogue.h"

namespace dialogue_compat
{

auto try_legacy_dialogue( dialogue_window &d_win, npc &n, player &p, dialogue &d ) -> bool
{
    if( !enable_legacy_shim ) {
        return false;
    }
    if( n.chatbin.first_topic.empty() ) {
        return false;
    }
    if( !yarn::has_yarn_story( "__legacy" ) ) {
        return false;
    }

    const auto &story = yarn::get_yarn_story( "__legacy" );
    if( !story.has_node( n.chatbin.first_topic ) ) {
        return false;
    }

    d_win.print_header( n.name );

    yarn::yarn_runtime::options opts{
        .story         = story,
        .registry      = yarn::func_registry::global(),
        .starting_node = n.chatbin.first_topic,
        .npc_ref       = &n,
        .player_ref    = &p,
        .dialogue_ref  = &d
    };
    yarn::yarn_runtime runtime( std::move( opts ) );
    runtime.run( d_win );
    return true;
}

} // namespace dialogue_compat
