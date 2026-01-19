#include "character_state_provider.h"

#include "character.h"
#include "effect.h"
#include "type_id.h"

static const efftype_id effect_downed( "downed" );
static const efftype_id effect_lying_down( "lying_down" );

std::optional<std::string> get_character_state_for_group(
    const Character &ch,
    const std::string &group_id
)
{
    if( group_id == "movement_mode" ) {
        switch( ch.get_movement_mode() ) {
            case CMM_WALK:
                return "walk";
            case CMM_RUN:
                return "run";
            case CMM_CROUCH:
                return "crouch";
            default:
                return "walk";
        }
    }

    if( group_id == "downed" ) {
        return ch.has_effect( effect_downed ) ? "downed" : "normal";
    }

    if( group_id == "lying_down" ) {
        return ch.has_effect( effect_lying_down ) ? "lying" : "normal";
    }

    // Unknown group_id - return nullopt to indicate it's not supported
    return std::nullopt;
}

std::vector<std::string> get_supported_modifier_groups()
{
    return { "movement_mode", "downed", "lying_down" };
}
