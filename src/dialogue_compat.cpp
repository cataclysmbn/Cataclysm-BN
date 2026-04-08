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
    return yarn::try_legacy_yarn_dialogue( d_win, n, p, d );
}

} // namespace dialogue_compat
