#include "avatar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "debug.h"
#include "game.h"
#include "item.h"
#include "itype.h"
#include "map.h"
#include "map_helpers.h"
#include "npc.h"
#include "overmapbuffer.h"
#include "overmapbuffer_registry.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "safe_reference.h"
#include "state_helpers.h"
#include "type_id.h"

#include <memory>
#include <string>
#include <utility>

static const activity_id ACT_READ("ACT_READ");


TEST_CASE("npc_book_activity_safe_reference_cleanup", "[npc][safe_reference]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([] { clear_all_state(); });

    // Spawn an NPC and give it a book to read.
    npc& reader = spawn_npc(tripoint_bub_ms(60, 60, 0), "test_talker");
    detached_ptr<item> det = item::spawn("novel_western");
    item& book = *det;
    reader.i_add(std::move(det));
    REQUIRE(book.type->book);

    // Assign an ACT_READ activity with the book as target.
    auto act = std::make_unique<player_activity>(ACT_READ, 100, 0,
                reader.getID().get_value());
    act->targets.emplace_back(book);
    reader.assign_activity(std::move(act));

    REQUIRE(reader.activity->id() == ACT_READ);
    REQUIRE(!reader.activity->targets.empty());
    REQUIRE(reader.activity->targets.front().is_accessible());

    // Clear overmapbuffers so the NPC is only held by active_npc.
    for_each_overmapbuffer([](const dimension_id&, overmapbuffer& buf) {
        buf.clear();
    });

    // Unload NPCs — reload_npcs() calls unload_npcs() (clearing active_npc,
    // which destroys the NPC and releases its safe_reference) then load_npcs()
    g->reload_npcs();

    const auto debug_msg = capture_debugmsg_during([]() {
        cleanup_references();
    });

    CHECK(debug_msg.find("mem_count") == std::string::npos);
}
