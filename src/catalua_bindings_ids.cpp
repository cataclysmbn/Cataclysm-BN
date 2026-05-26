#include "catalua_bindings.h"

namespace cata::detail
{

auto reg_game_ids_primary( sol::state &lua ) -> void;
auto reg_game_ids_traits( sol::state &lua ) -> void;
auto reg_game_ids_world( sol::state &lua ) -> void;
auto reg_game_ids_misc( sol::state &lua ) -> void;

} // namespace cata::detail

auto cata::detail::reg_game_ids( sol::state &lua ) -> void
{
    reg_game_ids_primary( lua );
    reg_game_ids_traits( lua );
    reg_game_ids_world( lua );
    reg_game_ids_misc( lua );
}
