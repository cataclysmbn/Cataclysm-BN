#include "catch/catch.hpp"
#include "json.h"
#include "magic/magic_teleporter_list.h"

#include <sstream>

namespace {

auto load_list(const std::string& text) -> teleporter_list {
    std::istringstream is(text);
    JsonIn jsin(is);
    teleporter_list list;
    list.deserialize(jsin);
    return list;
}

} // namespace

/// Checks that translocator gates keep their dimension (issue #10936).
/// A pocket dimension gate and an overworld gate can share OMT coordinates.
/// If the list forgets the dimension, the translocate menu lists the overworld gate
/// inside the pocket dimension, the teleport fails, and the gate gets deactivated.
/// Loads a save with one gate in "" and one in "pocket_a" at (1,2,0), and expects
/// each to be found only in its own dimension, also after a save and load.
TEST_CASE("translocator_gates_keep_their_dimension", "[magic][dimension]") {
    const auto pos = tripoint_abs_omt(1, 2, 0);
    const auto overworld = dimension_id();
    const auto pocket = dimension_id("pocket_a");
    const auto other = dimension_id("pocket_b");

    const auto list = load_list(R"({"known_teleporters":[
        {"position":[1,2,0],"name":"home"},
        {"dimension":"pocket_a","position":[1,2,0],"name":"cave"}
    ]})");

    const auto check = [&](const teleporter_list& l) {
        CHECK(l.knows_translocator({.dim = overworld, .pos = pos}));
        CHECK(l.knows_translocator({.dim = pocket, .pos = pos}));
        CHECK_FALSE(l.knows_translocator({.dim = other, .pos = pos}));
    };
    check(list);

    std::ostringstream os;
    JsonOut jsout(os);
    list.serialize(jsout);
    check(load_list(os.str()));
}
