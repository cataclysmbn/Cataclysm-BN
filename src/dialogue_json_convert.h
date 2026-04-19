#pragma once

#include <string>
#include <vector>

class JsonObject;
namespace yarn { struct yarn_node; }

// Converts TALK_TOPIC JSON objects to yarn nodes and registers them for
// inclusion in the __legacy yarn story.  Called from load_talk_topic().
//
// DEPRECATED: Remove when JSON-to-Yarn migration is complete.
namespace dialogue_convert
{

// Register a single talk_topic JSON object for yarn conversion.
void register_yarn_node( const std::string &id, const JsonObject &jo );

// Drain the pending node cache.  Called by build_legacy_yarn_stories().
auto flush_pending_nodes() -> std::vector<yarn::yarn_node>;

} // namespace dialogue_convert
