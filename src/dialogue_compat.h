#pragma once

// COMPAT SHIM — isolated compatibility layer for legacy TALK_TOPIC JSON dialogue.
// Remove this file when the ecosystem has fully migrated to Yarn Spinner .yarn files.

class dialogue_window;
class npc;
class player;
struct dialogue;

namespace dialogue_compat
{

// Run the legacy TALK_TOPIC dialogue loop for an NPC that has no yarn_story.
// `d` must already be configured (alpha/beta/topic_stack set by npc::talk_to_u).
// Returns true if the shim handled the conversation, false if the caller should
// fall through to the original dialogue loop.
auto try_legacy_dialogue( dialogue_window &d_win, npc &n, player &p, dialogue &d ) -> bool;

} // namespace dialogue_compat
