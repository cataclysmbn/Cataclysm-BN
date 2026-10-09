#include "character_preview.h"

namespace game_client
{
namespace
{
auto factory = character_preview_factory {};
}

auto set_character_preview_factory( character_preview_factory replacement ) -> void
{
    factory = replacement;
}

auto make_character_preview() -> std::unique_ptr<character_preview_window>
{
    return factory ? factory() : nullptr;
}
} // namespace game_client
