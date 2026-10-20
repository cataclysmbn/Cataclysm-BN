#include "character_display.h" // IWYU pragma: associated

#include "addiction.h"
#include "avatar.h"
#include "bionics.h"
#include "catalua_hooks.h"
#include "catalua_sol.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "character_effects.h"
#include "character_encumbrance.h"
#include "debug.h"
#include "effect.h"
#include "engine_client_component.h"
#include "engine_client_panels.h"
#include "engine_client_requirements.h"
#include "enum_conversions.h"
#include "game.h"
#include "input.h"
#include "melee.h"
#include "messages.h"
#include "mutation.h"
#include "options.h"
#include "output.h"
#include "pldata.h"
#include "profession.h"
#include "ranged.h"
#include "skill.h"
#include "string_formatter.h"
#include "string_id.h"
#include "string_input_popup.h"
#include "translations.h"
#include "type_id.h"
#include "ui_manager.h"
#include "units.h"
#include "units_utility.h"
#include "weather/weather.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <tuple>

static const skill_id skill_swimming( "swimming" );
static const skill_id skill_unarmed( "unarmed" );

static const std::string title_STATS = translate_marker( "STATS" );
static const std::string title_ENCUMB = translate_marker( "ENCUMBRANCE AND WARMTH" );
static const std::string title_EFFECTS = translate_marker( "EFFECTS" );
static const std::string title_SPEED = translate_marker( "SPEED" );
static const std::string title_SKILLS = translate_marker( "SKILLS" );
static const std::string title_BIONICS = translate_marker( "BIONICS" );
static const std::string title_TRAITS = translate_marker( "TRAITS" );

static const trait_flag_str_id trait_flag_NEED_ACTIVE_TO_MELEE( "NEED_ACTIVE_TO_MELEE" );
static const trait_flag_str_id trait_flag_UNARMED_BONUS( "UNARMED_BONUS" );

// use this instead of having to type out 26 spaces like before
static const std::string header_spaces( 26, ' ' );

static nc_color encumb_color( int level )
{
    if( level < 0 ) {
        return c_green;
    }
    if( level < 10 ) {
        return c_light_gray;
    }
    if( level < 40 ) {
        return c_yellow;
    }
    if( level < 70 ) {
        return c_light_red;
    }
    return c_red;
}

static auto get_temp_conv( const Character &c, const bodypart_str_id &bp ) -> units::temperature
{
    auto iter = c.get_body().find( bp );
    if( iter == c.get_body().end() ) {
        debugmsg( "Couldn't find bp %s on character %s", bp, c.disp_name() );
        return BODYTEMP_FREEZING;
    }

    return iter->second.get_temp_conv();
}

nc_color warmth::bodytemp_color( const Character &c, const bodypart_str_id &bp )
{
    if( bp == body_part_eyes ) {
        return c_light_gray;    // Eyes don't count towards warmth
    }

    const auto temp_conv = get_temp_conv( c, bp );
    if( temp_conv > BODYTEMP_SCORCHING ) {
        return c_red;
    } else if( temp_conv > BODYTEMP_VERY_HOT ) {
        return c_light_red;
    } else if( temp_conv > BODYTEMP_HOT ) {
        return c_yellow;
    } else if( temp_conv > BODYTEMP_COLD ) {
        return c_green;
    } else if( temp_conv > BODYTEMP_VERY_COLD ) {
        return c_light_blue;
    } else if( temp_conv > BODYTEMP_FREEZING ) {
        return c_cyan;
    } else if( temp_conv <= BODYTEMP_FREEZING ) {
        return c_blue;
    }
    return c_light_gray;
}

// Rescale temperature value to one that the player sees
static auto temperature_print_rescaling( units::temperature temp ) -> int
{
    const auto legacy_temp = units::to_legacy_bodypart_temp( temp );
    return ( legacy_temp / 100.0 ) * 2 - 100;
}

static bool should_combine_bps( const Character &ch,
                                const bodypart_str_id &l, const bodypart_str_id &r,
                                const item *selected_clothing )
{
    const auto &enc_data = ch.get_encumbrance();
    return l != r && // are different parts
           l == r->opposite_part && r == l->opposite_part && // are complementary parts
           // same encumberance & temperature
           // @todo Is ::at safe here?
           enc_data.elems.at( l ) == enc_data.elems.at( r ) &&
           temperature_print_rescaling( get_temp_conv( ch,
                                        l ) ) == temperature_print_rescaling( get_temp_conv( ch, r ) ) &&
           // selected_clothing covers both or neither parts
           ( !selected_clothing ||
             ( selected_clothing->covers( l.id() ) == selected_clothing->covers( r.id() ) ) );
}

static std::vector<std::pair<bodypart_str_id, bool>> list_and_combine_bps( const Character &ch,
        const item *selected_clothing )
{
    // bool represents whether the part has been combined with its other half
    std::vector<std::pair<bodypart_str_id, bool>> bps;
    for( auto bp : ch.get_all_body_parts() ) {
        // assuming that a body part has at most one other half
        if( bp->opposite_part->opposite_part != bp.id() ) {
            debugmsg( "Bodypart %s has more than one other half!", bp.id().c_str() );
        }
        if( should_combine_bps( ch, bp.id(), bp->opposite_part, selected_clothing ) ) {
            if( bp.to_i() < bp->opposite_part.id().to_i() ) {
                // only add the earlier one
                bps.emplace_back( bp, true );
            }
        } else {
            bps.emplace_back( bp, false );
        }
    }
    return bps;
}

static std::pair<int, int> subindex_around_cursor(
    const int num_entries, const int available_space, const int cursor_pos, const bool focused )
/**
 * Return indexes [start, end) that should be displayed from list long `num_entries`,
 * given that cursor is at position `cursor_pos` and we have `available_space` spaces.
 *
 * Example:
 * num_entries = 6, available_space = 3, cursor_pos = 2, focused = true;
 * so choose 3 from indexes [0, 1, 2, 3, 4, 5]
 * return {1, 4}
 */
{
    if( !focused || num_entries <= available_space ) {
        return std::make_pair( 0, std::min( available_space, num_entries ) );
    }
    int slice_start = std::min( std::max( 0, cursor_pos - available_space / 2 ),
                                num_entries - available_space );
    return std::make_pair( slice_start, slice_start + available_space );
}

auto character_display::encumbrance_lines( const Character &ch,
        const item *selected_clothing ) -> std::vector<encumbrance_line>
{
    using namespace std::views;
    const auto &enc_data = ch.get_encumbrance();
    return list_and_combine_bps( ch, selected_clothing ) | transform( [&]( const auto & entry ) {
        const auto &[bp, combine] = entry;
        const encumbrance_data &e = enc_data.elems.at( bp );
        return encumbrance_line{
            .part = bp, .combined = combine, .encumbrance = e.encumbrance - e.layer_penalty,
            .layering = e.layer_penalty, .warmth = temperature_print_rescaling( get_temp_conv( ch, bp ) ),
            .encumbrance_color = encumb_color( e.encumbrance ), .warmth_color = warmth::bodytemp_color( ch, bp ),
            .covered = selected_clothing ? selected_clothing->covers( bp.id() ) : false,
        };
    } ) | std::ranges::to<std::vector>();
}

void character_display::print_encumbrance( ui_adaptor &ui, const catacurses::window &win,
        const Character &ch,
        const int line, const item *selected_clothing )
{
    const auto bps = encumbrance_lines( ch, selected_clothing );

    // width/height excluding title & scrollbar
    const int height = getmaxy( win ) - 1;
    const bool do_draw_scrollbar = height < static_cast<int>( bps.size() );
    const int width = getmaxx( win ) - ( do_draw_scrollbar ? 1 : 0 );
    // index of the first printed bodypart from `bps`
    const int firstline = clamp( line - height / 2, 0, std::max( 0,
                                 static_cast<int>( bps.size() ) - height ) );

    /*** I chose to instead only display X+Y instead of X+Y=Z. More room was needed ***
     *** for displaying triple digit encumbrance, due to new encumbrance system.    ***
     *** If the player wants to see the total without having to do them maths, the  ***
     *** armor layers ui shows everything they want :-) -Davek                      ***/
    for( int i = 0; i < height; ++i ) {
        int thisline = firstline + i;
        if( thisline < 0 ) {
            continue;
        }
        if( static_cast<size_t>( thisline ) >= bps.size() ) {
            break;
        }
        const encumbrance_line &entry = bps[thisline];
        std::string out = body_part_name_as_heading( entry.part, entry.combined ? 2 : 1 );
        if( utf8_width( out ) > 7 ) {
            out = utf8_truncate( out, 7 );
        }

        // Two different highlighting schemes, highlight if the line is selected as per line being set.
        // Make the text green if this part is covered by the passed in item.
        const bool highlight_line = thisline == line;
        nc_color limb_color = highlight_line ?
                              ( entry.covered ? h_green : h_light_gray ) :
                              ( entry.covered ? c_green : c_light_gray );
        const int y_pos = 1 + i;
        if( highlight_line ) {
            ui.set_cursor( win, point( 1, y_pos ) );
        }
        mvwprintz( win, point( 1, y_pos ), limb_color, "%s", out );
        // accumulated encumbrance from clothing, plus extra encumbrance from layering
        mvwprintz( win, point( 8, 1 + i ), entry.encumbrance_color, "%3d", entry.encumbrance );
        // separator in low toned color
        mvwprintz( win, point( 11, y_pos ), c_light_gray, "+" );
        // take into account the new encumbrance system for layers
        mvwprintz( win, point( 12, y_pos ), entry.encumbrance_color, "%-3d", entry.layering );
        // print warmth, tethered to right hand side of the window

        mvwprintz( win, point( width - 6, y_pos ), entry.warmth_color, "(% 3d)", entry.warmth );
    }

    if( do_draw_scrollbar ) {
        draw_scrollbar( win, firstline, height, bps.size(), point( width, 1 ), c_white, true );
    }
}

static std::string swim_cost_text( int moves )
{
    return string_format( _( "Swimming movement point cost: <color_white>%+d</color>\n" ), moves );
}

static std::string run_cost_text( int moves )
{
    return string_format( _( "Movement point cost: <color_white>%+d</color>\n" ), moves );
}

static std::string reload_cost_text( int moves )
{
    return string_format( _( "Reloading movement point cost: <color_white>%+d</color>\n" ), moves );
}

static std::string melee_cost_text( int moves )
{
    return string_format(
               _( "Melee and thrown attack movement point cost: <color_white>%+d</color>\n" ), moves );
}
static std::string melee_stamina_cost_text( int cost )
{
    return string_format( _( "Melee stamina cost: <color_white>%+d</color>\n" ), cost );
}
static std::string mouth_stamina_cost_text( int cost )
{
    return string_format( _( "Stamina Regeneration: <color_white>%+d</color>\n" ), cost );
}
static std::string ranged_cost_text( double disp )
{
    return string_format( _( "Dispersion when using ranged attacks: <color_white>%+.1f</color>\n" ),
                          disp );
}
static std::string dodge_skill_text( double mod )
{
    return string_format( _( "Dodge skill: <color_white>%+.1f</color>\n" ), mod );
}

static int get_encumbrance( const Character &who, body_part bp, bool combine )
{
    // Body parts that can't combine with anything shouldn't print double values on combine
    // This shouldn't happen, but handle this, just in case
    const bool combines_with_other = static_cast<int>( bp_aiOther[bp] ) != bp;
    return who.encumb( convert_bp( bp ) ) * ( ( combine && combines_with_other ) ? 2 : 1 );
}

static std::string get_encumbrance_description( const Character &who, const bodypart_str_id &bp,
        bool combine )
{
    std::string s;

    const int eff_encumbrance = get_encumbrance( who, bp->token, combine );

    switch( bp->token ) {
        case bp_torso: {
            const int melee_roll_pen = std::max( -eff_encumbrance, -80 );
            s += string_format( _( "Melee attack rolls: <color_white>%+d%%</color>\n" ), melee_roll_pen );
            s += dodge_skill_text( -( eff_encumbrance / 10.0 ) );
            s += swim_cost_text( ( eff_encumbrance / 10.0 ) * ( 80 - who.get_skill_level(
                                     skill_swimming ) * 3 ) );
            s += melee_cost_text( eff_encumbrance );
            break;
        }
        case bp_head:
            s += _( "<color_magenta>Head encumbrance has no effect; it simply limits how much you can put on.</color>" );
            break;
        case bp_eyes:
            s += string_format(
                     _( "Perception when checking traps or firing ranged weapons: <color_white>%+d</color>\n"
                        "Dispersion when throwing items: <color_white>%+d</color>\n"
                        "Night vision range: <color_white>%+.1f</color>" ),
                     -( eff_encumbrance / 10 ),
                     eff_encumbrance * 10,
                     vision::nv_range_from_eye_encumbrance( eff_encumbrance ) );
            break;
        case bp_mouth:
            s += _( "<color_magenta>Covering your mouth will make it more difficult to breathe and catch your breath.</color>\n" );
            s += mouth_stamina_cost_text( -( eff_encumbrance / 5 ) );
            break;
        case bp_arm_l:
        case bp_arm_r:
            s += _( "<color_magenta>Arm encumbrance affects stamina cost of melee attacks and accuracy with ranged weapons.</color>\n" );
            s += melee_stamina_cost_text( eff_encumbrance * 2 );
            s += ranged_cost_text( eff_encumbrance / 5.0 );
            break;
        case bp_hand_l:
        case bp_hand_r:
            s += _( "<color_magenta>Reduces the speed at which you can handle or manipulate items.</color>\n\n" );
            s += reload_cost_text( ( eff_encumbrance / 10 ) * 15 );
            s += string_format( _( "Dexterity when throwing items: <color_white>%+.1f</color>\n" ),
                                -( eff_encumbrance / 10.0f ) );
            s += melee_cost_text( eff_encumbrance / 2 );
            s += string_format( _( "Reduced gun aim speed: <color_white>%.1f</color>" ),
                                ranged::aim_speed_encumbrance_modifier( who ) );
            break;
        case bp_leg_l:
        case bp_leg_r:
            s += run_cost_text( static_cast<int>( eff_encumbrance * 0.15 ) );
            s += swim_cost_text( ( eff_encumbrance / 10 ) * ( 50 - who.get_skill_level(
                                     skill_swimming ) * 2 ) / 2 );
            s += dodge_skill_text( -eff_encumbrance / 10.0 / 4.0 );
            break;
        case bp_foot_l:
        case bp_foot_r:
            s += run_cost_text( static_cast<int>( eff_encumbrance * 0.25 ) );
            break;
        case num_bp:
            break;
    }

    return s;
}

static bool is_cqb_skill( const skill_id &id )
{
    // TODO: this skill list here is used in other places as well. Useless redundancy and
    // dependency. Maybe change it into a flag of the skill that indicates it's a skill used
    // by the bionic?
    static const std::array<skill_id, 6> cqb_skills = { {
            skill_id( "melee" ), skill_id( "unarmed" ), skill_id( "cutting" ),
            skill_id( "bashing" ), skill_id( "stabbing" ), skill_id( "dodge" ),
        }
    };
    return std::ranges::contains( cqb_skills, id );
}

namespace
{
enum class player_display_tab {
    stats,
    encumbrance,
    skills,
    traits,
    bionics,
    effects,
    num_tabs,
};

/// One entry of the effects tab: what natively explains the player's state. The id comes from what
/// causes it, never from its text.
struct effect_line {
    std::string id;
    std::string name;
    std::string text;
};
} // namespace

static player_display_tab next_tab( const player_display_tab tab )
{
    if( static_cast<int>( tab ) + 1 < static_cast<int>( player_display_tab::num_tabs ) ) {
        return static_cast<player_display_tab>( static_cast<int>( tab ) + 1 );
    } else {
        return static_cast<player_display_tab>( 0 );
    }
}

static player_display_tab prev_tab( const player_display_tab tab )
{
    if( static_cast<int>( tab ) > 0 ) {
        return static_cast<player_display_tab>( static_cast<int>( tab ) - 1 );
    } else {
        return static_cast<player_display_tab>( static_cast<int>( player_display_tab::num_tabs ) - 1 );
    }
}

namespace
{
/// The color the stats tab prints a stat's current value in, against its base value.
auto stat_color( const int cur, const int max ) -> nc_color
{
    if( cur <= 0 ) {
        return c_dark_gray;
    } else if( cur < max / 2 ) {
        return c_red;
    } else if( cur < max ) {
        return c_light_red;
    } else if( cur == max ) {
        return c_white;
    } else if( cur < max * 1.5 ) {
        return c_light_green;
    }
    return c_green;
}
} // namespace

static void draw_stats_tab( ui_adaptor &ui, const catacurses::window &w_stats, const Character &you,
                            const unsigned line, const player_display_tab curtab )
{
    werase( w_stats );
    const bool is_current_tab = curtab == player_display_tab::stats;
    const nc_color title_col = is_current_tab ? h_light_gray : c_light_gray;
    if( is_current_tab ) {
        ui.set_cursor( w_stats, point_zero );
    }
    center_print( w_stats, 0, title_col, _( title_STATS ) );

    const auto highlight_line = [is_current_tab, line]( const unsigned line_to_draw ) {
        return is_current_tab && line == line_to_draw;
    };

    const auto line_color = [&highlight_line]( const unsigned line_to_draw ) {
        if( highlight_line( line_to_draw ) ) {
            return h_light_gray;
        } else {
            return c_light_gray;
        }
    };

    const auto set_highlight_cursor = [&highlight_line, &ui, &w_stats]
    ( const unsigned line_to_draw ) {
        if( highlight_line( line_to_draw ) ) {
            ui.set_cursor( w_stats, point( 1, line_to_draw + 1 ) );
        }
    };

    // Stats
    const auto display_stat = [&line_color, &set_highlight_cursor, &w_stats]
    ( const char *name, const int cur, const int max, const unsigned line_to_draw ) {
        const auto cstatus = stat_color( cur, max );

        set_highlight_cursor( line_to_draw );
        mvwprintz( w_stats, point( 1, line_to_draw + 1 ), line_color( line_to_draw ), name );
        mvwprintz( w_stats, point( 18, line_to_draw + 1 ), cstatus, "%2d", cur );
        mvwprintz( w_stats, point( 21, line_to_draw + 1 ), c_light_gray, "(%2d)", max );
    };

    display_stat( _( "Strength:" ), you.get_str(), you.get_str_base(), 0 );
    display_stat( _( "Dexterity:" ), you.get_dex(), you.get_dex_base(), 1 );
    display_stat( _( "Intelligence:" ), you.get_int(), you.get_int_base(), 2 );
    display_stat( _( "Perception:" ), you.get_per(), you.get_per_base(), 3 );

    set_highlight_cursor( 4 );
    mvwprintz( w_stats, point( 1, 5 ), line_color( 4 ), _( "Height:" ) );
    mvwprintz( w_stats, point( 25 - utf8_width( you.height_string() ), 5 ), line_color( 4 ),
               you.height_string() );

    set_highlight_cursor( 5 );
    mvwprintz( w_stats, point( 1, 6 ), line_color( 5 ), _( "Age:" ) );
    mvwprintz( w_stats, point( 25 - utf8_width( you.age_string() ), 6 ), line_color( 5 ),
               you.age_string() );

    wnoutrefresh( w_stats );
}

namespace
{
struct stat_info_text {
    std::string description;
    std::array<std::string, 3> values;
};

auto prepare_stat_info( const Character &you, const unsigned line ) -> stat_info_text
{
    switch( line ) {
        case 0:
            return {
                .description = _( "Strength affects your melee damage, the amount of weight you can carry, your total HP, "
                                  "your resistance to many diseases, and the effectiveness of actions which require brute force." ),
                .values = {
                    string_format( _( "Base HP: <color_white>%d</color>" ), you.get_part_hp_max( bodypart_id( "torso" ) ) ),
                    string_format( _( "Carry weight (%s): <color_white>%.1f</color>" ), weight_units(), convert_weight( you.weight_capacity() ) ),
                    string_format( _( "Melee damage: <color_white>%.1f</color>" ), you.bonus_damage( false ) )
                }
            };
        case 1:
            return {
                .description = _( "Dexterity affects your chance to hit in melee combat, helps you steady your "
                                  "gun for ranged combat, and enhances many actions that require finesse." ),
                .values = {
                    string_format( _( "Melee to-hit bonus: <color_white>%+.1lf</color>" ), you.get_melee_hit( you.used_weapon(), melee::default_attack( you.used_weapon() ) ) ),
                    string_format( _( "Ranged penalty: <color_white>%+d</color>" ), -std::abs( you.ranged_dex_mod() ) ),
                    string_format( _( "Throwing penalty per target's dodge: <color_white>%+d</color>" ), ranged::throw_dispersion_per_dodge( you, false ) )
                }
            };
        case 2:
            return {
                .description = _( "Intelligence is less important in most situations, but it is vital for more complex tasks like "
                                  "electronics crafting.  It also affects how much skill you can pick up from reading a book." ),
                .values = {
                    you.rust_rate() ? string_format( _( "Skill rust: <color_white>%d%%</color>" ), you.rust_rate() ) : "",
                    string_format( _( "Read times: <color_white>%d%%</color>" ), you.read_speed( false ) ),
                    string_format( _( "Crafting bonus: <color_white>+%d%%</color>" ), you.get_int() )
                }
            };
        case 3:
            return {
                .description = _( "Perception is the most important stat for ranged combat.  It's also used for "
                                  "detecting traps, camouflaged creatures, and other things of interest." ),
                .values = {
                    string_format( _( "Base night vision range: <color_white>%.1f</color>" ), vision::nv_range_from_per( you.get_per() ) ),
                    string_format( _( "Trap detection level: <color_white>%d</color>" ), you.get_per() ),
                    you.ranged_per_mod() > 0 ? string_format( _( "Aiming penalty: <color_white>%+d</color>" ), -you.ranged_per_mod() ) : ""
                }
            };
        case 4:
            return { .description = _( "Your height.  Simply how tall you are." ), .values = { you.height_string(), "", "" } };
        case 5:
            return { .description = _( "This is how old you are." ), .values = { you.age_string(), "", "" } };
        default:
            return {};
    }
}
} // namespace

static auto draw_stats_info( const catacurses::window &w_info, const Character &you,
                             const unsigned line ) -> void
{
    werase( w_info );
    const auto info = prepare_stat_info( you, line );
    auto color = c_light_gray;
    const auto lines = fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2,
                                       c_magenta, info.description );
    for( const auto i : std::views::iota( std::size_t{0}, info.values.size() ) ) {
        if( !info.values[i].empty() ) {
            if( line < 4 ) {
                print_colored_text( w_info, point( 1, 3 + i ), color, c_light_gray, info.values[i] );
            } else {
                fold_and_print( w_info, point( 1, 1 + lines ), FULL_SCREEN_WIDTH - 2,
                                c_light_gray, info.values[i] );
            }
        }
    }
    wnoutrefresh( w_info );
}

static void draw_encumbrance_tab( ui_adaptor &ui, const catacurses::window &w_encumb,
                                  const Character &you,
                                  const unsigned line, const player_display_tab curtab )
{
    werase( w_encumb );
    const bool is_current_tab = curtab == player_display_tab::encumbrance;
    const nc_color title_col = is_current_tab ? h_light_gray : c_light_gray;
    center_print( w_encumb, 0, title_col, _( title_ENCUMB ) );
    if( is_current_tab ) {
        ui.set_cursor( w_encumb, point_zero );
        character_display::print_encumbrance( ui, w_encumb, you, line );
    } else {
        character_display::print_encumbrance( ui, w_encumb, you );
    }
    wnoutrefresh( w_encumb );
}

static void draw_encumbrance_info( const catacurses::window &w_info, const Character &you,
                                   const unsigned line )
{
    const std::vector<std::pair<bodypart_str_id, bool>> bps = list_and_combine_bps( you, nullptr );

    werase( w_info );
    bodypart_str_id bp;
    bool combined_here = false;
    if( line < bps.size() ) {
        bp = bps[line].first;
        combined_here = bps[line].second;
    }
    const std::string s = get_encumbrance_description( you, bp, combined_here );
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_light_gray, s );
    wnoutrefresh( w_info );
}

static void draw_traits_tab( ui_adaptor &ui, const catacurses::window &w_traits,
                             const unsigned line, const player_display_tab curtab,
                             const std::vector<trait_id> &traitslist )
{
    werase( w_traits );
    const bool is_current_tab = curtab == player_display_tab::traits;
    const nc_color title_col = is_current_tab ? h_light_gray : c_light_gray;
    if( is_current_tab ) {
        ui.set_cursor( w_traits, point_zero );
    }
    center_print( w_traits, 0, title_col, _( title_TRAITS ) );

    const int height = getmaxy( w_traits ) - 1;
    const bool do_draw_scrollbar = height < static_cast<int>( traitslist.size() );
    const int width = getmaxx( w_traits ) - 1 - ( do_draw_scrollbar ? 1 : 0 );
    const std::pair<const int, const int> range = subindex_around_cursor( traitslist.size(), height,
            line, is_current_tab );

    for( size_t i = range.first; i < static_cast<size_t>( range.second ); ++i ) {
        const auto &mdata = traitslist[i].obj();
        const auto color = mdata.get_display_color();
        const bool highlight_line = is_current_tab && i == line;
        const point pos( 1, 1 + i - range.first );
        if( highlight_line ) {
            ui.set_cursor( w_traits, pos );
        }
        trim_and_print( w_traits, pos, width,
                        highlight_line ? hilite( color ) : color, mdata.name() );
    }
    if( do_draw_scrollbar ) {
        draw_scrollbar( w_traits, range.first, height, traitslist.size(), point( width + 1, 1 ), c_white,
                        true );
    }
    wnoutrefresh( w_traits );
}

static void draw_traits_info( const catacurses::window &w_info, const unsigned line,
                              const std::vector<trait_id> &traitslist )
{
    werase( w_info );
    if( line < traitslist.size() ) {
        const auto &mdata = traitslist[line].obj();
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_light_gray, string_format(
                            "%s: %s", colorize( mdata.name(), mdata.get_display_color() ), traitslist[line]->desc() ) );
    }
    wnoutrefresh( w_info );
}

static void draw_bionics_tab( ui_adaptor &ui, const catacurses::window &w_bionics,
                              const Character &you, const unsigned line,
                              const player_display_tab curtab,
                              const std::vector<std::pair<bionic, int>> &bionicslist )
{
    werase( w_bionics );
    const bool is_current_tab = curtab == player_display_tab::bionics;
    const nc_color title_col = is_current_tab ? h_light_gray : c_light_gray;
    center_print( w_bionics, 0, title_col, _( title_BIONICS ) );
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    const point pow_pos( 1, 1 );
    if( is_current_tab ) {
        ui.set_cursor( w_bionics, pow_pos );
    }
    trim_and_print( w_bionics, pow_pos, getmaxx( w_bionics ) - 1, c_white,
                    string_format( _( "Bionic Power: <color_light_blue>%1$d</color>"
                                      " / <color_light_blue>%2$d</color>" ),
                                   units::to_kilojoule( you.get_power_level() ), units::to_kilojoule( you.get_max_power_level() ) ) );

    const int height = getmaxy( w_bionics ) - 2;  // -2 for headline and power_level
    const bool do_draw_scrollbar = height < static_cast<int>( bionicslist.size() );
    const int width = getmaxx( w_bionics ) - 1 - ( do_draw_scrollbar ? 1 : 0 );
    const std::pair<const int, const int> range = subindex_around_cursor( bionicslist.size(), height,
            line, is_current_tab );

    for( size_t i = range.first; i < static_cast<size_t>( range.second ); ++i ) {
        const bool highlight_line = is_current_tab && i == line;
        const point pos( 1, 2 + i - range.first );
        if( highlight_line ) {
            ui.set_cursor( w_bionics, pos );
        }

        const auto& [bio, cnt] =  bionicslist[i];
        const auto fmtstr = cnt > 1 ? "%s (%d)" : "%s";
        const auto color = get_bionic_text_color( bio, highlight_line );
        trim_and_print( w_bionics, pos, width, highlight_line ? hilite( color ) : color, fmtstr,
                        bio.info().name, cnt );
    }
    if( do_draw_scrollbar ) {
        draw_scrollbar( w_bionics, range.first, height, bionicslist.size(), point( width + 1, 2 ), c_white,
                        true );
    }
    wnoutrefresh( w_bionics );
}

static void draw_bionics_info( const catacurses::window &w_info, const unsigned line,
                               const std::vector<std::pair<bionic, int>> &bionicslist )
{
    werase( w_info );
    if( line < bionicslist.size() ) {
        const auto& [bio, cnt] = bionicslist[line];
        const auto text = cnt > 1 ? string_format( "%s\n\n%s", bio.info().description,
                          string_format( _( "You have %s instances of this bionic installed." ), cnt ) ) :
                          bio.info().description.translated();
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_light_gray, text );
    }
    wnoutrefresh( w_info );
}

static void draw_effects_tab( ui_adaptor &ui, const catacurses::window &w_effects,
                              const unsigned line, const player_display_tab curtab,
                              const std::vector<effect_line> &effect_name_and_text )
{
    werase( w_effects );
    const bool is_current_tab = curtab == player_display_tab::effects;
    const nc_color title_col = is_current_tab ? h_light_gray : c_light_gray;
    if( is_current_tab ) {
        ui.set_cursor( w_effects, point_zero );
    }
    center_print( w_effects, 0, title_col, _( title_EFFECTS ) );

    const int height = getmaxy( w_effects ) - 1;
    const bool do_draw_scrollbar = height < static_cast<int>( effect_name_and_text.size() );
    const int width = getmaxx( w_effects ) - 1 - ( do_draw_scrollbar ? 1 : 0 );
    const std::pair<const int, const int> range = subindex_around_cursor( effect_name_and_text.size(),
            height, line, is_current_tab );

    for( size_t i = range.first; i < static_cast<size_t>( range.second ); ++i ) {
        const bool highlight_line = is_current_tab && i == line;
        const point pos( 1, 1 + i - range.first );
        if( highlight_line ) {
            ui.set_cursor( w_effects, pos );
        }
        trim_and_print( w_effects, pos, width,
                        highlight_line ? h_light_gray : c_light_gray, effect_name_and_text[i].name );
    }
    if( do_draw_scrollbar ) {
        draw_scrollbar( w_effects, range.first, height, effect_name_and_text.size(), point( width + 1, 1 ),
                        c_white, true );
    }
    wnoutrefresh( w_effects );
}

static void draw_effects_info( const catacurses::window &w_info, const unsigned line,
                               const std::vector<effect_line> &effect_name_and_text )
{
    werase( w_info );
    const size_t actual_size = effect_name_and_text.size();
    if( line < actual_size ) {
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_light_gray,
                        effect_name_and_text[line].text );
    }
    wnoutrefresh( w_info );
}

struct HeaderSkill {
    const Skill *skill;
    bool is_header;
    HeaderSkill( const Skill *skill, bool is_header ): skill( skill ), is_header( is_header ) {
    }
};

int character_display::display_empty_handed_base_damage( const Character &you )
{
    int empty_hand_base_damage = you.has_active_bionic( bionic_id( "bio_cqb" ) ) ? std::max(
                                     you.get_skill_level(
                                         skill_unarmed ), BIO_CQB_LEVEL ) : you.get_skill_level(
                                     skill_unarmed );
    const bool left_empty = !you.natural_attack_restricted_on( bodypart_id( "hand_l" ) );
    const bool right_empty = !you.natural_attack_restricted_on( bodypart_id( "hand_r" ) );

    if( !left_empty && !right_empty ) {
        // Mutation and bionic bonuses don't matter so just print unarmed bonus
        return empty_hand_base_damage;
    } else {

        // Mutation and bionic bonuses double if both hands are free
        int per_hand = 0;
        if( you.has_bionic( bionic_id( "bio_razors" ) ) ) {
            per_hand += 9;
        }
        for( const trait_id &mut : you.get_mutations() ) {
            if( mut->flags.contains( trait_flag_NEED_ACTIVE_TO_MELEE ) &&
                !you.has_active_mutation( mut ) ) {
                continue;
            }
            // Fixed bonuses are nice and simple
            per_hand += mut->bash_dmg_bonus + mut->cut_dmg_bonus + mut->pierce_dmg_bonus;

            // Random bonuses are more fiddly, since we want baseline numbers let's just report the minimum
            const std::pair<int, int> rand_bash = mut->rand_bash_bonus;
            const std::pair<int, int> rand_cut = mut->rand_cut_bonus;
            per_hand += rand_bash.first + rand_cut.first;

            // Extra skill bonus is also fairly simple, but each type of fixed bonus can trigger it separately
            if( mut->flags.contains( trait_flag_UNARMED_BONUS ) ) {
                if( mut->bash_dmg_bonus > 0 ) {
                    per_hand += std::min( you.get_skill_level( skill_unarmed ) / 2, 4 );
                }
                if( mut->cut_dmg_bonus > 0 ) {
                    per_hand += std::min( you.get_skill_level( skill_unarmed ) / 2, 4 );
                }
                if( mut->pierce_dmg_bonus > 0 ) {
                    per_hand += std::min( you.get_skill_level( skill_unarmed ) / 2, 4 );
                }
            }
        }
        empty_hand_base_damage += per_hand; // First hand
        if( left_empty && right_empty ) {
            // Second hand
            empty_hand_base_damage += per_hand;
        }
        return empty_hand_base_damage;
    }
}

namespace
{
struct skill_display_values {
    int number;
    int exercise;
    nc_color color;
    std::optional<double> dodge;
    std::optional<int> damage;
};

/// Share the exact native numbers/colors. Training under CQB or a white non-trainable row is
/// not disclosed by its color; do not publish the otherwise hidden underlying training flag.
auto prepare_skill_values( const Character &you, const Skill &skill, const bool highlighted )
-> skill_display_values
{
    const auto &level = you.get_skill_level_object( skill.ident() );
    const auto locked = you.has_active_bionic( bionic_id( "bio_cqb" ) ) &&
                        is_cqb_skill( skill.ident() );
    const auto number = locked ? std::max( int( you.get_skill_level( skill.ident() ) ),
                                           BIO_CQB_LEVEL ) : int( you.get_skill_level( skill.ident() ) );
    const auto exercise = locked ? 0 : level.exercise();
    auto color = c_white;
    if( locked ) {
        color = highlighted ? h_yellow : c_yellow;
    } else if( highlighted ) {
        if( !level.can_train() ) {
            color = level.isRusting() ? h_light_red : h_white;
        } else if( exercise >= 100 ) {
            color = level.isTraining() ? h_pink : h_magenta;
        } else if( level.isRusting() ) {
            color = level.isTraining() ? h_light_red : h_red;
        } else {
            color = level.isTraining() ? h_light_blue : h_blue;
        }
    } else if( level.isRusting() ) {
        color = level.isTraining() ? c_light_red : c_red;
    } else if( level.can_train() ) {
        color = level.isTraining() ? c_light_blue : c_blue;
    }
    return { .number = number, .exercise = std::max( 0, exercise ), .color = color,
             .dodge = skill.ident() == skill_id( "dodge" ) ? std::optional<double>{ you.get_dodge() } :
                      std::nullopt,
             .damage = skill.ident() == skill_unarmed ? std::optional<int>{ character_display::display_empty_handed_base_damage( you ) } :
                       std::nullopt };
}
} // namespace

namespace
{
struct skill_tab_options {
    ui_adaptor &ui;
    const catacurses::window &window;
    unsigned line;
    player_display_tab tab;
    const std::vector<HeaderSkill> &skills;
    std::size_t height;
    const std::vector<skill_display_values> &values;
};

auto draw_skills_tab( const skill_tab_options &opts ) -> void
{
    auto &ui = opts.ui;
    const auto &w_skills = opts.window;
    auto line = opts.line;
    const auto curtab = opts.tab;
    const auto &skillslist = opts.skills;
    const auto skill_win_size_y = opts.height;
    const int col_width = getmaxx( w_skills ) - 1;
    if( line == 0 ) { //can't point to a header;
        line = 1;
    }

    werase( w_skills );
    const bool is_current_tab = curtab == player_display_tab::skills;
    nc_color cstatus = is_current_tab ? h_light_gray : c_light_gray;
    if( is_current_tab ) {
        ui.set_cursor( w_skills, point_zero );
    }
    center_print( w_skills, 0, cstatus, _( title_SKILLS ) );

    size_t min = 0;
    size_t max = 0;

    const size_t half_y = ( skill_win_size_y - 1 ) / 2;

    if( !is_current_tab || line <= half_y ) {
        min = 0;
    } else if( line >= skillslist.size() - half_y ) {
        min = ( skillslist.size() < skill_win_size_y - 1 ? 0 : skillslist.size() -
                skill_win_size_y + 1 );
    } else {
        min = line - half_y;
    }
    max = std::min( min + skill_win_size_y - 1, skillslist.size() );

    int y_pos = 1;
    for( size_t i = min; i < max; ++i, ++y_pos ) {
        const Skill *aSkill = skillslist[i].skill;

        if( skillslist[i].is_header ) {
            const SkillDisplayType t = SkillDisplayType::get_skill_type( aSkill->display_category() );
            std::string type_name = t.display_string();
            mvwprintz( w_skills, point( 0, y_pos ), c_light_gray, header_spaces );
            center_print( w_skills, y_pos, c_yellow, type_name );
        } else {
            const auto &values = opts.values[i];
            const auto level_num = values.number;
            const auto exercise = values.exercise;
            cstatus = values.color;
            if( is_current_tab && i == line ) {
                ui.set_cursor( w_skills, point( 1, y_pos ) );
                mvwprintz( w_skills, point( 1, y_pos ), cstatus, std::string( col_width, ' ' ) );
            } else {
                mvwprintz( w_skills, point( 1, y_pos ), c_light_gray, std::string( col_width, ' ' ) );
            }
            mvwprintz( w_skills, point( 1, y_pos ), cstatus, "%s:", aSkill->name() );
            if( aSkill->ident() == skill_id( "dodge" ) ) {
                mvwprintz( w_skills, point( 14, y_pos ), cstatus, "%4.1f/%-2d(%2d%%)",
                           *values.dodge, level_num, exercise < 0 ? 0 : exercise );
            }
            if( aSkill->ident() == skill_id( "unarmed" ) ) {
                mvwprintz( w_skills, point( 15, y_pos ), cstatus, "%3d/%-2d(%2d%%)",
                           *values.damage, level_num,
                           exercise < 0 ? 0 : exercise );
            } else {
                mvwprintz( w_skills, point( 19, y_pos ), cstatus, "%-2d(%2d%%)",
                           level_num,
                           ( exercise < 0 ? 0 : exercise ) );
            }
        }
    }

    if( is_current_tab && skillslist.size() > skill_win_size_y - 1 ) {
        draw_scrollbar( w_skills, line - 1, skill_win_size_y - 1, skillslist.size() - 1,
                        // NOLINTNEXTLINE(cata-use-named-point-constants)
                        point( 0, 1 ) );
    }
    wnoutrefresh( w_skills );
}
} // namespace

namespace
{
struct skill_info_options {
    const catacurses::window &window;
    const Character &you;
    unsigned line;
    const std::vector<HeaderSkill> &skills;
};

auto draw_skills_info( const skill_info_options &opts ) -> std::string
{
    const auto &w_info = opts.window;
    const auto &you = opts.you;
    auto line = opts.line;
    const auto &skillslist = opts.skills;
    auto description = std::string{};
    werase( w_info );
    if( line < 1 ) {
        line = 1;
    }
    const Skill *selectedSkill = nullptr;
    if( line < skillslist.size() && !skillslist[line].is_header ) {
        selectedSkill = skillslist[line].skill;
    }

    werase( w_info );

    if( selectedSkill ) {
        description = selectedSkill->description();
        const auto hook_results = cata::run_hooks( "on_character_display_skill_info",
        [&]( sol::table & params ) {
            params["character"] = &you;
            params["skill"] = selectedSkill->ident();
        } );
        const auto extra_text = hook_results.get_or( "text", std::string() );
        if( !extra_text.empty() ) {
            description += "\n\n" + extra_text;
        }
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_light_gray,
                        description );
    }
    wnoutrefresh( w_info );
    return description;
}
} // namespace

namespace
{
/// What an effect adds to the speed: the effects of one name add up, and the first one names the row.
struct speed_effect {
    std::string id;
    int value = 0;
};

/// The label and value of one speed breakdown row, carried by the line that prints its label.
struct native_speed_row {
    /// What the row is about, never its text.
    std::string key;
    std::string label;
    std::string value;
    /// The value's color when it differs from the label line's.
    std::optional<nc_color> value_color;
};

struct native_speed_line {
    point position;
    nc_color color;
    std::string text;
    std::optional<native_speed_row> row;
};

auto prepare_speed_info( const Character &you,
                         const std::map<std::string, speed_effect> &speed_effects ) -> std::vector<native_speed_line>
{
    auto result = std::vector<native_speed_line> {};
    const auto runcost = you.run_cost( 100 );
    const auto newmoves = you.get_speed();
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    result.push_back( { .position = point( 1, 1 ), .color = c_light_gray, .text = string_format( _( "Base Move Cost:" ) ),
                        .row = native_speed_row{ .key = "base_move_cost", .label = _( "Base Move Cost:" ), .value = std::to_string( runcost ),
                                                 .value_color = runcost <= 100 ? c_green : c_red } } );
    result.push_back( { .position = point( 1, 2 ), .color = c_light_gray, .text = string_format( _( "Current Speed:" ) ),
                        .row = native_speed_row{ .key = "current_speed", .label = _( "Current Speed:" ), .value = std::to_string( newmoves ),
                                                 .value_color = newmoves >= 100 ? c_green : c_red } } );
    auto pen = 0;
    auto line = 3u;
    if( you.weight_carried() > you.weight_capacity() ) {
        pen = 25 * ( you.weight_carried() - you.weight_capacity() ) / ( you.weight_capacity() );
        //~ %s: Overburdened (already left-justified), %2d%%: speed penalty
        result.push_back( { .position = point( 1, line ), .color = c_red, .text = string_format( pgettext( "speed penalty", "%s-%2d%%" ), left_justify( _( "Overburdened" ), 20 ), pen ), .row = native_speed_row{ .key = "overburdened", .label = _( "Overburdened" ), .value = string_format( "-%d%%", pen ) } } );
        ++line;
    }
    pen = character_effects::get_pain_penalty( you ).speed;
    if( pen >= 1 ) {
        //~ %s: Pain (already left-justified), %2d%%: speed penalty
        result.push_back( { .position = point( 1, line ), .color = c_red, .text = string_format( pgettext( "speed penalty", "%s-%2d%%" ), left_justify( _( "Pain" ), 20 ), pen ), .row = native_speed_row{ .key = "pain", .label = _( "Pain" ), .value = string_format( "-%d%%", pen ) } } );
        ++line;
    }
    if( you.get_thirst() > thirst_levels::very_thirsty ) {
        pen = std::abs( character_effects::get_thirst_speed_penalty( you.get_thirst() ) );
        //~ %s: Thirsty/Parched (already left-justified), %2d%%: speed penalty
        result.push_back( { .position = point( 1, line ), .color = c_red, .text = string_format( pgettext( "speed penalty", "%s-%2d%%" ), left_justify( _( "Thirst" ), 20 ), pen ), .row = native_speed_row{ .key = "thirst", .label = _( "Thirst" ), .value = string_format( "-%d%%", pen ) } } );
        ++line;
    }
    if( character_effects::get_kcal_speed_penalty( you.get_kcal_percent() ) < 0 ) {
        pen = std::abs( character_effects::get_kcal_speed_penalty( you.get_kcal_percent() ) );
        //~ %s: Starving/Underfed (already left-justified), %2d%%: speed penalty
        result.push_back( { .position = point( 1, line ), .color = c_red, .text = string_format( pgettext( "speed penalty", "%s-%2d%%" ), left_justify( _( "Starving" ), 20 ), pen ), .row = native_speed_row{ .key = "starving", .label = _( "Starving" ), .value = string_format( "-%d%%", pen ) } } );
        ++line;
    }
    if( you.has_trait( trait_id( "SUNLIGHT_DEPENDENT" ) ) && !g->is_in_sunlight( you.bub_pos() ) ) {
        pen = ( g->light_level( you.bub_pos().z() ) >= 12 ? 5 : 10 );
        //~ %s: Out of Sunlight (already left-justified), %2d%%: speed penalty
        result.push_back( { .position = point( 1, line ), .color = c_red, .text = string_format( pgettext( "speed penalty", "%s-%2d%%" ), left_justify( _( "Out of Sunlight" ), 20 ), pen ), .row = native_speed_row{ .key = "sunlight", .label = _( "Out of Sunlight" ), .value = string_format( "-%d%%", pen ) } } );
        ++line;
    }

    auto temperature_speed_modifier = you.mutation_value( "temperature_speed_modifier" );
    temperature_speed_modifier += you.bonus_from_enchantments( temperature_speed_modifier,
                                  enchantment_value_id( "BODYTEMP_SPEED" ) );

    if( temperature_speed_modifier != 0 ) {
        auto pen_color = nc_color{};
        auto pen_sign = std::string{};
        const auto player_local_temp = units::to_fahrenheit( get_weather().get_temperature(
                                           you.abs_pos() ) );
        if( you.has_trait( trait_id( "COLDBLOOD4" ) ) && player_local_temp > 65 ) {
            pen_color = c_green;
            pen_sign = "+";
        } else if( player_local_temp < 65 ) {
            pen_color = c_red;
            pen_sign = "-";
        }
        if( !pen_sign.empty() ) {
            pen = ( player_local_temp - 65 ) * temperature_speed_modifier;
            //~ %s: Cold-Blooded (already left-justified), %s: sign of bonus/penalty, %2d%%: speed modifier
            result.push_back( { .position = point( 1, line ), .color = pen_color, .text = string_format( pgettext( "speed modifier", "%s%s%2d%%" ), left_justify( _( "Cold-Blooded" ), 20 ), pen_sign, std::abs( pen ) ),
                                .row = native_speed_row{ .key = "cold_blooded", .label = _( "Cold-Blooded" ), .value = string_format( "%s%d%%", pen_sign, std::abs( pen ) ) } } );
            ++line;
        }
    }

    const auto quick_bonus = static_cast<int>( std::round( ( you.mutation_value( "speed_modifier" ) -
                             1 ) *
                             100 ) );
    if( quick_bonus != 0 ) {
        const auto pen_sign = quick_bonus >= 0 ? "+" : "-";
        const auto pen_color = quick_bonus >= 0 ? c_green : c_red;
        //~ %s: Mutations (already left-justified), %s: sign of bonus/penalty, %2d%%: speed modifier
        result.push_back( { .position = point( 1, line ), .color = pen_color, .text = string_format( pgettext( "speed bonus", "%s%s%2d%%" ), left_justify( _( "Mutations" ), 20 ), pen_sign, std::abs( quick_bonus ) ),
                            .row = native_speed_row{ .key = "mutations", .label = _( "Mutations" ), .value = string_format( "%s%d%%", pen_sign, std::abs( quick_bonus ) ) } } );
        ++line;
    }
    const auto ench_speed = int( ceil( you.bonus_from_enchantments( 100,
                                       enchantment_value_id( "SPEED" ) ) ) );
    if( ench_speed > 0 ) {
        result.push_back( { .position = point( 1, line ), .color = c_green, .text = string_format( pgettext( "speed bonus", "Misc Speed        +%2d%%" ), ench_speed ),
                            .row = native_speed_row{ .key = "misc_speed", .label = _( "Misc Speed" ), .value = string_format( "+%d%%", ench_speed ) } } );
        ++line;
    } else if( ench_speed < 0 ) {
        result.push_back( { .position = point( 1, line ), .color = c_red, .text = string_format( pgettext( "speed bonus", "Misc Speed        -%2d%%" ), abs( ench_speed ) ),
                            .row = native_speed_row{ .key = "misc_speed", .label = _( "Misc Speed" ), .value = string_format( "-%d%%", abs( ench_speed ) ) } } );
        ++line;
    }

    for( const auto &speed_effect : speed_effects ) {
        auto col = ( speed_effect.second.value > 0 ? c_green : c_red );
        result.push_back( { .position = point( 1, line ), .color = col, .text = string_format( "%s", speed_effect.first ),
                            .row = native_speed_row{ .key = "speed:effect:" + speed_effect.second.id, .label = speed_effect.first,
                                                     .value = string_format( "%s%d%%", speed_effect.second.value > 0 ? "+" : "-", std::abs( speed_effect.second.value ) ) } } );
        result.push_back( { .position = point( 21, line ), .color = col, .text = string_format( ( speed_effect.second.value > 0 ? "+" : "-" ) ) } );
        result.push_back( { .position = point( std::abs( speed_effect.second.value ) >= 10 ? 22 : 23, line ), .color = col, .text = string_format( "%d%%", std::abs( speed_effect.second.value ) ) } );
        ++line;
    }

    nc_color col = ( runcost <= 100 ? c_green : c_red );
    result.push_back( { .position = point( 21 + ( runcost >= 100 ? 0 : ( runcost < 10 ? 2 : 1 ) ), 1 ), .color = col, .text = string_format( "%d", runcost ) } );
    col = ( newmoves >= 100 ? c_green : c_red );
    result.push_back( { .position = point( 21 + ( newmoves >= 100 ? 0 : ( newmoves < 10 ? 2 : 1 ) ), 2 ), .color = col, .text = string_format( "%d", newmoves ) } );
    return result;
}

} // namespace

static auto draw_speed_tab( const catacurses::window &w_speed, const Character &you,
                            const std::map<std::string, speed_effect> &speed_effects ) -> void
{
    const auto lines = prepare_speed_info( you, speed_effects );
    werase( w_speed );
    center_print( w_speed, 0, c_light_gray, _( title_SPEED ) );
    for( const auto &line : lines ) {
        mvwprintz( w_speed, line.position, line.color, "%s", line.text );
    }
    wnoutrefresh( w_speed );
}

namespace
{
struct info_window_options {
    const catacurses::window &window;
    const Character &you;
    unsigned line;
    player_display_tab tab;
    const std::vector<trait_id> &traits;
    const std::vector<std::pair<bionic, int>> &bionics;
    const std::vector<effect_line> &effects;
    const std::vector<HeaderSkill> &skills;
};

auto draw_info_window( const info_window_options &opts ) -> std::string
{
    const auto &w_info = opts.window;
    const auto &you = opts.you;
    const auto line = opts.line;
    const auto curtab = opts.tab;
    const auto &traitslist = opts.traits;
    const auto &bionicslist = opts.bionics;
    const auto &effect_name_and_text = opts.effects;
    const auto &skillslist = opts.skills;
    switch( curtab ) {
        case player_display_tab::stats:
            draw_stats_info( w_info, you, line );
            break;
        case player_display_tab::encumbrance:
            draw_encumbrance_info( w_info, you, line );
            break;
        case player_display_tab::skills:
            return draw_skills_info( { .window = w_info, .you = you, .line = line, .skills = skillslist } );
        case player_display_tab::traits:
            draw_traits_info( w_info, line, traitslist );
            break;
        case player_display_tab::bionics:
            draw_bionics_info( w_info, line, bionicslist );
            break;
        case player_display_tab::effects:
            draw_effects_info( w_info, line, effect_name_and_text );
            break;
        case player_display_tab::num_tabs:
            abort();
    }
    return {};
}
} // namespace

namespace
{
auto prepare_character_header( const Character &you, const std::string &race ) -> std::string
{
    auto gender = you.male ? _( "Male" ) : _( "Female" );

    // Print name and header
    if( you.custom_profession.empty() ) {
        if( you.crossed_threshold() ) {
            //~ player info window: 1s - name, 2s - gender, 3s - Prof or Mutation name
            return string_format( _( " %1$s | %2$s | %3$s" ),
                                  you.name, gender, race );
        } else if( !you.prof.is_valid() || you.prof == profession::generic() ) {
            // Regular person. Nothing interesting.
            //~ player info window: 1s - name, 2s - gender '|' - field separator.
            return string_format( _( " %1$s | %2$s" ),
                                  you.name, gender );
        } else {
            return string_format( _( " %1$s | %2$s | %3$s" ),
                                  you.name, gender, you.prof->gender_appropriate_name( you.male ) );
        }
    } else {
        return string_format( _( " %1$s | %2$s | %3$s" ),
                              you.name,  gender, you.custom_profession );
    }

}
} // namespace

static auto draw_tip( const catacurses::window &w_tip, const std::string &header,
                      const input_context &ctxt ) -> void
{
    werase( w_tip );
    mvwprintz( w_tip, point_zero, c_white, "%s", header );
    right_print( w_tip, 0, 1, c_white, string_format(
                     _( "[<color_yellow>%s</color>]" ),
                     ctxt.get_desc( "HELP_KEYBINDINGS" ) ) );

    wnoutrefresh( w_tip );
}

namespace
{
namespace ec = engine_client;

constexpr auto tabs_id = "character:tabs";
constexpr auto panes_id = "character:panes";
constexpr auto select_tab_id = "character:tab";

constexpr auto all_tabs = std::array
{
    player_display_tab::stats, player_display_tab::encumbrance, player_display_tab::skills,
                       player_display_tab::traits, player_display_tab::bionics, player_display_tab::effects,
};

/// The tab by its identity: its title is translated.
auto tab_name( const player_display_tab tab ) -> std::string
{
    constexpr auto names = std::array{ "stats", "encumbrance", "skills", "traits", "bionics", "effects" };
    return names[static_cast<std::size_t>( tab )];
}

auto tab_title( const player_display_tab tab ) -> std::string
{
    const auto titles = std::array{ &title_STATS, &title_ENCUMB, &title_SKILLS, &title_TRAITS, &title_BIONICS, &title_EFFECTS };
    return _( *titles[static_cast<std::size_t>( tab )] );
}

auto pane_id( const player_display_tab tab ) -> std::string { return "character:" + tab_name( tab ); }

/// One select affordance per pane: the client names the one whose row target holds its focus.
auto focus_row_id( const player_display_tab tab ) -> std::string { return "character:focus:" + tab_name( tab ); }

auto upgrade_denial( const avatar &you ) -> std::string
{
    if( you.free_upgrade_points() > 0 ) {
        return {};
    }
    const auto remaining = std::optional<int> { you.kill_xp_for_next_point() };
    if( !remaining ) {
        return _( "You've already reached maximum level." );
    }
    return string_format( _( "Needs %d more experience to gain next level." ), *remaining );
}

/// The label of a figure: what its native line says before the value.
auto figure_label( const std::string &format ) -> std::string
{
    return ec::caption( format.substr( 0, format.find_first_of( "<%" ) ) );
}

/// What the screen shows, as the loop keeps it: the focus is read when the screen is published.
struct sheet_model {
    const Character &you;
    const player_display_tab &tab;
    const unsigned &line;
    /// The profession or mutation the header names the character by.
    const std::string &race;
    const std::vector<trait_id> &traits;
    const std::vector<std::pair<bionic, int>> &bionics;
    const std::vector<effect_line> &effects;
    const std::vector<HeaderSkill> &skills;
    /// What the info renderer made of the focused skill, run by the native loop.
    const std::string &skill_description;
    const std::map<std::string, speed_effect> &speed_effects;
};

/// The skills tab never rests on a heading.
auto focused_line( const sheet_model &model ) -> unsigned
{
    return model.tab == player_display_tab::skills ? std::max( 1u, model.line ) : model.line;
}

/// The identities of the rows of `tab`, in order: the cause of the row, never its text or place.
auto row_ids( const sheet_model &model, const player_display_tab tab ) -> std::vector<std::string>
{
    using namespace std::views;
    switch( tab ) {
        case player_display_tab::stats:
            return { "stat:str", "stat:dex", "stat:int", "stat:per", "stat:height", "stat:age" };
        case player_display_tab::encumbrance:
            return list_and_combine_bps( model.you, nullptr ) | transform( []( const auto & part ) {
                return "body:" + part.first.str() + ( part.second ? ":pair" : "" );
            } ) | std::ranges::to<std::vector>();
        case player_display_tab::skills:
            return model.skills | transform( []( const auto & entry ) {
                return entry.is_header ? "skill_type:" + entry.skill->display_category().str() : "skill:" +
                       entry.skill->ident().str();
            } ) | std::ranges::to<std::vector>();
        case player_display_tab::traits:
            return model.traits | transform( []( const auto & trait ) { return "trait:" + trait.str(); } ) |
            std::ranges::to<std::vector>();
        case player_display_tab::bionics:
            return model.bionics | transform( []( const auto & entry ) { return "bionic:" + entry.first.id.str(); } )
            | std::ranges::to<std::vector>();
        case player_display_tab::effects:
            return model.effects | transform( &effect_line::id ) | std::ranges::to<std::vector>();
        case player_display_tab::num_tabs:
            break;
    }
    abort();
}

auto stat_rows( const Character &you, const std::vector<std::string> &ids ) -> std::vector<ec::row>
{
    const auto labels = std::array{ _( "Strength:" ), _( "Dexterity:" ), _( "Intelligence:" ), _( "Perception:" ), _( "Height:" ), _( "Age:" ) };
    const auto current = std::array{ you.get_str(), you.get_dex(), you.get_int(), you.get_per() };
    const auto base = std::array{ you.get_str_base(), you.get_dex_base(), you.get_int_base(), you.get_per_base() };
    auto rows = std::vector<ec::row> {};
    for( const auto i : std::views::iota( 0u, 6u ) ) {
        const auto info = prepare_stat_info( you, i );
        auto text = info.description;
        for( const auto &value : info.values ) {
            if( !value.empty() ) {
                text += "\n" + value;
            }
        }
        auto columns = std::vector<game_client::interaction_column> {};
        if( i < 4 ) {
            columns.push_back( { .key = "current", .label = _( "Current" ), .value = std::to_string( current[i] ),
                                 .color = string_from_color( stat_color( current[i], base[i] ) ) } );
            columns.push_back( { .key = "base", .label = _( "Base" ), .value = std::to_string( base[i] ) } );
        } else {
            columns.push_back( { .key = "current", .label = labels[i], .value = info.values[0] } );
        }
        rows.push_back( { .id = ids[i], .label = labels[i], .description = text, .columns = std::move( columns ) } );
    }
    return rows;
}

auto encumbrance_rows( const Character &you,
                       const std::vector<std::string> &ids ) -> std::vector<ec::row>
{
    const auto parts = list_and_combine_bps( you, nullptr );
    const auto &encumbrance = you.get_encumbrance();
    auto rows = std::vector<ec::row> {};
    for( const auto i : std::views::iota( std::size_t{0}, parts.size() ) ) {
        const auto &[bp, combined] = parts[i];
        const auto &data = encumbrance.elems.at( bp );
        rows.push_back( { .id = ids[i], .label = body_part_name_as_heading( bp, combined ? 2 : 1 ),
                          .description = get_encumbrance_description( you, bp, combined ),
        .columns = {
            {
                .key = "encumbrance", .label = _( "Encumbrance" ), .value = std::to_string( data.encumbrance - data.layer_penalty ),
                .color = string_from_color( encumb_color( data.encumbrance ) )
            },
            { .key = "layering", .label = _( "Layering" ), .value = std::to_string( data.layer_penalty ) },
            {
                .key = "warmth", .label = _( "Warmth" ), .value = std::to_string( temperature_print_rescaling( get_temp_conv( you, bp ) ) ),
                .color = string_from_color( warmth::bodytemp_color( you, bp ) )
            }
        } } );
    }
    return rows;
}

auto skill_rows( const sheet_model &model,
                 const std::vector<std::string> &ids ) -> std::vector<ec::row>
{
    auto rows = std::vector<ec::row> {};
    for( const auto i : std::views::iota( std::size_t{0}, model.skills.size() ) ) {
        const auto &entry = model.skills[i];
        if( entry.is_header ) {
            rows.push_back( { .id = ids[i], .label = SkillDisplayType::get_skill_type( entry.skill->display_category() ).display_string(),
                              .enabled = false, .selectable = false, .header = true } );
            continue;
        }
        const auto values = prepare_skill_values( model.you, *entry.skill, false );
        auto columns = std::vector<game_client::interaction_column> {
            { .key = "level", .label = _( "Level" ), .value = std::to_string( values.number ), .color = string_from_color( values.color ) },
            { .key = "exercise", .label = _( "Exercise" ), .value = string_format( "%d%%", values.exercise ) },
        };
        if( entry.skill->ident() == skill_unarmed ) {
            columns.push_back( { .key = "damage", .label = _( "Damage" ), .value = std::to_string( *values.damage ) } );
        } else if( entry.skill->ident() == skill_id( "dodge" ) ) {
            columns.push_back( { .key = "dodge", .label = _( "Dodge" ), .value = string_format( "%.1f", *values.dodge ) } );
        }
        // The focused skill is described as the info renderer made it, with what a hook adds.
        const auto focused = model.tab == player_display_tab::skills && i == focused_line( model );
        rows.push_back( { .id = ids[i], .label = entry.skill->name(), .description = focused ? model.skill_description : entry.skill->description(),
                          .columns = std::move( columns ) } );
    }
    return rows;
}

auto trait_rows( const sheet_model &model,
                 const std::vector<std::string> &ids ) -> std::vector<ec::row>
{
    auto rows = std::vector<ec::row> {};
    for( const auto i : std::views::iota( std::size_t{0}, model.traits.size() ) ) {
        const auto &trait = model.traits[i];
        rows.push_back( { .id = ids[i], .label = trait->name(),
                          .description = string_format( "%s: %s", colorize( trait->name(), trait->get_display_color() ), trait->desc() ),
                          .color = string_from_color( trait->get_display_color() ) } );
    }
    return rows;
}

auto bionic_rows( const sheet_model &model,
                  const std::vector<std::string> &ids ) -> std::vector<ec::row>
{
    auto rows = std::vector<ec::row> {};
    for( const auto i : std::views::iota( std::size_t{0}, model.bionics.size() ) ) {
        const auto &[bio, count] = model.bionics[i];
        rows.push_back( { .id = ids[i],
                          .label = count > 1 ? string_format( "%s (%d)", bio.info().name, count ) : bio.info().name.translated(),
                          .description = count > 1 ? string_format( "%s\n\n%s", bio.info().description, string_format( _( "You have %s instances of this bionic installed." ), count ) ) : bio.info().description.translated(),
                          .color = string_from_color( get_bionic_text_color( bio, false ) ) } );
    }
    return rows;
}

auto effect_rows( const sheet_model &model,
                  const std::vector<std::string> &ids ) -> std::vector<ec::row>
{
    auto rows = std::vector<ec::row> {};
    for( const auto i : std::views::iota( std::size_t{0}, model.effects.size() ) ) {
        rows.push_back( { .id = ids[i], .label = model.effects[i].name, .description = model.effects[i].text } );
    }
    return rows;
}

auto tab_rows( const sheet_model &model, const player_display_tab tab ) -> std::vector<ec::row>
{
    const auto ids = row_ids( model, tab );
    switch( tab ) {
        case player_display_tab::stats:
            return stat_rows( model.you, ids );
        case player_display_tab::encumbrance:
            return encumbrance_rows( model.you, ids );
        case player_display_tab::skills:
            return skill_rows( model, ids );
        case player_display_tab::traits:
            return trait_rows( model, ids );
        case player_display_tab::bionics:
            return bionic_rows( model, ids );
        case player_display_tab::effects:
            return effect_rows( model, ids );
        case player_display_tab::num_tabs:
            break;
    }
    abort();
}

/// The figures of the speed breakdown: its base and current cost, then what changes them.
auto speed_stats( const sheet_model &model ) -> std::vector<ec::panel_stat>
{
    using namespace std::views;
    return prepare_speed_info( model.you, model.speed_effects )
    | filter( []( const auto & line ) { return line.row.has_value(); } )
    | transform( []( const auto & line ) {
        const auto color = line.row->value_color.value_or( line.color );
        return ec::panel_stat{ .key = line.row->key, .label = ec::caption( line.row->label ), .value = line.row->value,
                               .tone = color == c_green ? "good" : color == c_red ? "bad" : "" };
    } ) | std::ranges::to<std::vector>();
}

/// The panes the native sheet shows at once, one of them focused.
auto sheet_view( const sheet_model &model ) -> ec::screen_view
{
    auto panes = std::vector<ec::component> {};
    auto entries = std::vector<ec::tab_entry> {};
    auto affordances = std::vector<ec::affordance> { ec::invoke_affordance( { .id = select_tab_id, .label = _( "Category" ), .args = { .row = ec::row_shape{ .component_id = tabs_id } } } ) };
    for( const auto tab : all_tabs ) {
        auto stats = std::vector<ec::panel_stat> {};
        if( tab == player_display_tab::bionics ) {
            const auto format =
                _( "Bionic Power: <color_light_blue>%1$d</color> / <color_light_blue>%2$d</color>" );
            stats.push_back( { .key = "power", .label = figure_label( format ),
                               .value = std::to_string( units::to_kilojoule( model.you.get_power_level() ) ),
                               .max = std::to_string( units::to_kilojoule( model.you.get_max_power_level() ) ) } );
        }
        panes.push_back( ec::panel( { .id = pane_id( tab ), .title = tab_title( tab ), .stats = std::move( stats ),
                                      .child = ec::list( { .id = pane_id( tab ) + ":rows", .rows = tab_rows( model, tab ),
                                               .selection = "single", .activate = focus_row_id( tab ) } ) } ) );
        affordances.push_back( ec::invoke_affordance( { .id = focus_row_id( tab ), .label = _( "Select" ), .args = { .row = ec::row_shape{ .component_id = pane_id( tab ) + ":rows" } }, .role = "select" } ) );
        entries.push_back( { .id = tab_name( tab ), .label = tab_title( tab ), .selected = tab == model.tab } );
    }
    panes.push_back( ec::panel( { .id = "character:speed", .title = _( title_SPEED ), .stats = speed_stats( model ) } ) );

    const auto focus = row_ids( model, model.tab );
    const auto line = focused_line( model );
    const auto shown = prepare_character_header( model.you, model.race );
    const auto first = shown.find_first_not_of( ' ' );
    auto denials = std::vector<ec::denial> {};
    if( model.tab == player_display_tab::stats && model.line < 4 &&
        get_option<bool>( "STATS_THROUGH_KILLS" ) &&
        model.you.is_avatar() ) {
        if( const auto reason = upgrade_denial( *model.you.as_avatar() ); !reason.empty() ) {
            denials.push_back( { .id = "CONFIRM", .reason = reason } );
        }
    }
    return {
.title = first == std::string::npos ? std::string{} :
        shown.substr( first ),
        .root = ec::tabs( {
            .id = tabs_id, .tabs = std::move( entries ), .select = select_tab_id,
            .child = ec::split( { .id = panes_id, .active = pane_id( model.tab ), .children = std::move( panes ) } ) } ),
        .affordances = std::move( affordances ),
        .denials = std::move( denials ),
        .focus_component = pane_id( model.tab ) + ":rows",
        .focus_row = line < focus.size() ? focus[line] : std::string{},
    };
}
} // namespace

namespace
{
struct display_action_options {
    Character &you;
    unsigned &line;
    player_display_tab &tab;
    input_context &context;
    const ui_adaptor &tip;
    const ui_adaptor &info;
    const ui_adaptor &stats;
    const ui_adaptor &encumbrance;
    const ui_adaptor &traits;
    const ui_adaptor &bionics;
    const ui_adaptor &effects;
    const ui_adaptor &skills;
    const sheet_model &model;
};

auto handle_player_display_action( const display_action_options &opts ) -> bool
{
    auto &you = opts.you;
    auto &line = opts.line;
    auto &curtab = opts.tab;
    auto &ctxt = opts.context;
    const auto &ui_tip = opts.tip;
    const auto &ui_info = opts.info;
    const auto &ui_stats = opts.stats;
    const auto &ui_encumb = opts.encumbrance;
    const auto &ui_traits = opts.traits;
    const auto &ui_bionics = opts.bionics;
    const auto &ui_effects = opts.effects;
    const auto &ui_skills = opts.skills;
    const auto &skillslist = opts.model.skills;
    const auto invalidate_tab = [&]( const player_display_tab tab ) {
        switch( tab ) {
            case player_display_tab::stats:
                ui_stats.invalidate_ui();
                break;
            case player_display_tab::encumbrance:
                ui_encumb.invalidate_ui();
                break;
            case player_display_tab::traits:
                ui_traits.invalidate_ui();
                break;
            case player_display_tab::bionics:
                ui_bionics.invalidate_ui();
                break;
            case player_display_tab::effects:
                ui_effects.invalidate_ui();
                break;
            case player_display_tab::skills:
                ui_skills.invalidate_ui();
                break;
            case player_display_tab::num_tabs:
                abort();
        }
    };

    unsigned int line_beg = 0;
    const auto line_end = static_cast<unsigned>( row_ids( opts.model, curtab ).size() );
    if( curtab == player_display_tab::skills ) {
        line_beg = 1; // skip first header
    }
    if( line_beg >= line_end || line < line_beg ) {
        line = line_beg;
    } else if( line > line_end - 1 ) {
        line = line_end - 1;
    }

    auto action = ctxt.handle_input();
    // A client picked a tab or a row: the cursor moves there, as a click would.
    if( const auto picked = ec::invoked( ctxt.get_raw_input() ); picked && picked->args.row ) {
        auto target = std::optional<std::pair<player_display_tab, unsigned>> {};
        if( picked->affordance_id == select_tab_id ) {
            const auto tab = std::ranges::find( all_tabs, *picked->args.row, tab_name );
            if( tab != all_tabs.end() ) {
                target = { *tab, *tab == player_display_tab::skills ? 1u : 0u };
            }
        } else if( picked->affordance_id.starts_with( "character:focus:" ) ) {
            for( const auto tab : all_tabs ) {
                const auto ids = row_ids( opts.model, tab );
                const auto found = std::ranges::find( ids, *picked->args.row );
                if( found == ids.end() ) { continue; }
                const auto selectable = tab != player_display_tab::skills ||
                                        !skillslist[found - ids.begin()].is_header;
                if( selectable ) {
                    target = { tab, static_cast<unsigned>( found - ids.begin() ) };
                }
            }
        }
        if( target ) {
            invalidate_tab( curtab );
            std::tie( curtab, line ) = *target;
            invalidate_tab( curtab );
            ui_info.invalidate_ui();
        }
        // Enter on a row is the native Enter on it.
        if( !target || !picked->activates ) { return false; }
        action = "CONFIRM";
    }
    auto done = false;

    if( action == "UP" ) {
        if( line > line_beg ) {
            --line;
        } else {
            line = line_end - 1;
        }
        if( curtab == player_display_tab::skills && skillslist[line].is_header ) {
            --line;
        }
        invalidate_tab( curtab );
        ui_info.invalidate_ui();
    } else if( action == "DOWN" ) {
        if( line + 1 < line_end ) {
            ++line;
        } else {
            line = line_beg;
        }
        if( curtab == player_display_tab::skills && skillslist[line].is_header ) {
            ++line;
        }
        invalidate_tab( curtab );
        ui_info.invalidate_ui();
    } else if( action == "NEXT_TAB" || action == "PREV_TAB" ) {
        line = 0;
        invalidate_tab( curtab );
        curtab = action == "NEXT_TAB" ? next_tab( curtab ) : prev_tab( curtab );
        invalidate_tab( curtab );
        ui_info.invalidate_ui();
    } else if( action == "QUIT" ) {
        done = true;
    } else if( action == "CONFIRM" ) {
        switch( curtab ) {
            default:
                break;
            case player_display_tab::stats:
                if( line < 4 && get_option<bool>( "STATS_THROUGH_KILLS" ) && you.is_avatar() ) {
                    character_display::upgrade_stat_prompt( *you.as_avatar(), static_cast<character_stat>( line ) );
                }
                invalidate_tab( curtab );
                break;
            case player_display_tab::skills: {
                const Skill *selectedSkill = nullptr;
                if( line < skillslist.size() && !skillslist[line].is_header ) {
                    selectedSkill = skillslist[line].skill;
                }
                if( selectedSkill ) {
                    const auto hook_results = cata::run_hooks( "on_character_display_skill_action",
                    [&]( sol::table & params ) {
                        params["character"] = &you;
                        params["skill"] = selectedSkill->ident();
                    } );
                    if( !hook_results.get_or( "handled", false ) ) {
                        you.get_skill_level_object( selectedSkill->ident() ).toggleTraining();
                    }
                }
                invalidate_tab( curtab );
                break;
            }
        }
    } else if( action == "CHANGE_PROFESSION_NAME" ) {
        string_input_popup popup;
        popup.title( _( "Profession Name: " ) )
        .width( 25 )
        .text( "" )
        .max_length( 25 )
        .query();

        you.custom_profession = popup.text();
        add_msg( "You now consider yourself to be a %s.", popup.text() );
        ui_tip.invalidate_ui();
    } else if( action == "CHANGE_NAME" ) {
        string_input_popup popup;
        popup.title( _( "Name: " ) )
        .width( 50 )
        .text( "" )
        .max_length( 50 )
        .query();

        you.name = popup.text();
        add_msg( "From now on, you will refer to yourself as '%s.'", popup.text() );
        ui_tip.invalidate_ui();
    }
    return done;
}
} // namespace

static std::pair<unsigned, unsigned> calculate_shared_column_win_height(
    const unsigned available_height, unsigned first_win_size_y_max, unsigned second_win_size_y_max )
/**
 * Calculate max allowed height of two windows sharing column space.
 */
{
    if( ( second_win_size_y_max + 1 + first_win_size_y_max ) > available_height ) {
        // maximum space for either window if they're both the same size
        unsigned max_shared_y = ( available_height - 1 ) / 2;
        if( std::min( second_win_size_y_max, first_win_size_y_max ) > max_shared_y ) {
            // both are larger than the shared size
            second_win_size_y_max = max_shared_y;
            first_win_size_y_max = available_height - 1 - second_win_size_y_max;
        } else if( first_win_size_y_max <= max_shared_y ) {
            // first window is less than the shared size, so give space to second window
            second_win_size_y_max = available_height - 1 - first_win_size_y_max;
        } else {
            // second window is less than the shared size
            first_win_size_y_max = available_height - 1 - second_win_size_y_max;
        }
    }
    return std::make_pair( first_win_size_y_max, second_win_size_y_max );
}

namespace
{
/// What the character sheet lists, gathered from `ch` as the native sheet opens; nothing is changed.
struct sheet_data {
    std::vector<effect_line> effects;
    std::vector<trait_id> traits;
    std::vector<std::pair<bionic, int>> bionics;
    std::vector<HeaderSkill> skills;
    std::string race;
    std::map<std::string, speed_effect> speed_effects;
};

auto gather_sheet( const Character &ch ) -> sheet_data
{
    std::vector<effect_line> effect_name_and_text;
    for( const auto &elem : ch.get_all_effects() ) {
        for( const auto &_effect_it : elem.second ) {
            const std::string tmp = _effect_it.second.disp_name();
            if( _effect_it.second.is_removed() || tmp.empty() ) {
                continue;
            }
            effect_name_and_text.push_back( { .id = "effect:" + elem.first.str() + ":" + _effect_it.first.str(),
                                              .name = tmp, .text = _effect_it.second.disp_desc() } );
        }
    }
    if( ch.get_perceived_pain() > 0 ) {
        const auto ppen = character_effects::get_pain_penalty( ch );
        std::string pain_text;
        const auto add_if = [&]( const int amount, const char *const name ) {
            if( amount > 0 ) {
                pain_text += string_format( name, amount ) + "   ";
            }
        };
        add_if( ppen.strength, _( "Strength -%d" ) );
        add_if( ppen.dexterity, _( "Dexterity -%d" ) );
        add_if( ppen.intelligence, _( "Intelligence -%d" ) );
        add_if( ppen.perception, _( "Perception -%d" ) );
        add_if( ppen.speed, _( "Speed -%d %%" ) );
        effect_name_and_text.push_back( { .id = "effect:pain", .name = _( "Pain" ), .text = pain_text } );
    }

    const float bmi = ch.bmi();

    if( bmi < character_weight_category::underweight ) {
        std::string starvation_name;
        std::string starvation_text;

        if( bmi < character_weight_category::emaciated ) {
            starvation_name = _( "Severely Malnourished" );
            starvation_text =
                _( "Your body is severely weakened by starvation.  You might die if you don't start eating regular meals!\n\n" );
        } else {
            starvation_name = _( "Malnourished" );
            starvation_text =
                _( "Your body is weakened by starvation.  Only time and regular meals will help you recover.\n\n" );
        }

        if( bmi < character_weight_category::underweight ) {
            const float str_penalty = 1.0f - ( ( bmi - 13.0f ) / 3.0f );
            starvation_text += std::string( _( "Strength" ) ) + " -" + string_format( "%2.0f%%\n",
                               str_penalty * 100.0f );
            starvation_text += std::string( _( "Dexterity" ) ) + " -" + string_format( "%2.0f%%\n",
                               str_penalty * 50.0f );
            starvation_text += std::string( _( "Intelligence" ) ) + " -" + string_format( "%2.0f%%",
                               str_penalty * 50.0f );
        }

        effect_name_and_text.push_back( { .id = "effect:starvation", .name = starvation_name, .text = starvation_text } );
    }

    if( ( ch.has_trait( trait_id( "TROGLO" ) ) && g->is_in_sunlight( ch.bub_pos() ) &&
          get_weather().weather_id->sun_intensity >= sun_intensity_type::high ) ||
        ( ch.has_trait( trait_id( "TROGLO2" ) ) && g->is_in_sunlight( ch.bub_pos() ) &&
          get_weather().weather_id->sun_intensity < sun_intensity_type::high )
      ) {
        effect_name_and_text.push_back( { .id = "effect:sunlight", .name = _( "In Sunlight" ),
                                          .text = _( "The sunlight irritates you.\n"
                                                  "Strength - 1;    Dexterity - 1;    Intelligence - 1;    Perception - 1" ) } );
    } else if( ch.has_trait( trait_id( "TROGLO2" ) ) && g->is_in_sunlight( ch.bub_pos() ) ) {
        effect_name_and_text.push_back( { .id = "effect:sunlight", .name = _( "In Sunlight" ),
                                          .text = _( "The sunlight irritates you badly.\n"
                                                  "Strength - 2;    Dexterity - 2;    Intelligence - 2;    Perception - 2" ) } );
    } else if( ch.has_trait( trait_id( "TROGLO3" ) ) && g->is_in_sunlight( ch.bub_pos() ) ) {
        effect_name_and_text.push_back( { .id = "effect:sunlight", .name = _( "In Sunlight" ),
                                          .text = _( "The sunlight irritates you terribly.\n"
                                                  "Strength - 4;    Dexterity - 4;    Intelligence - 4;    Perception - 4" ) } );
    }

    for( auto &elem : ch.addictions ) {
        if( elem.sated < 0_turns && elem.intensity >= MIN_ADDICTION_LEVEL ) {
            effect_name_and_text.push_back( { .id = "effect:addiction:" + io::enum_to_string( elem.type ),
                                              .name = addiction_name( elem ), .text = addiction_text( elem ) } );
        }
    }

    std::vector<trait_id> traitslist = ch.get_mutations( false );
    std::ranges::sort( traitslist, trait_display_sort );

    std::multimap<bionic_id, bionic> bionics_map;
    for( const auto &elem : *ch.my_bionics ) {
        bionics_map.emplace( elem.id, elem );
    }
    std::vector<std::pair<bionic, int>> bionics_list;
    for( auto it = bionics_map.begin(); it != bionics_map.end(); ) {
        const auto [k, v] = *it;
        int d = 0;
        do {
            ++it;
            ++d;
        } while( it != bionics_map.end() && k == it->first );
        bionics_list.push_back( std::make_pair( v,  d ) );
    }

    using bionic_pair = decltype( bionics_list )::value_type;
    std::ranges::sort( bionics_list, []( const bionic_pair & a, const bionic_pair & b ) -> bool {
        if( a.first.info().activated != b.first.info().activated )
        {
            return a.first.info().activated;
        }
        constexpr auto less = bionic_sort_less{ bionic_ui_sort_mode::NAME };
        return less( a.first, b.first );
    } );

    const std::vector<const Skill *> player_skill = Skill::get_skills_sorted_by(
    [&]( const Skill & a, const Skill & b ) {
        skill_displayType_id type_a = a.display_category();
        skill_displayType_id type_b = b.display_category();

        return localized_compare( std::make_pair( type_a, a.name() ),
                                  std::make_pair( type_b, b.name() ) );
    } );

    std::vector<HeaderSkill> skillslist;
    skill_displayType_id prev_type = skill_displayType_id::NULL_ID();
    for( auto &s : player_skill ) {
        if( s->display_category() != prev_type ) {
            prev_type = s->display_category();
            skillslist.emplace_back( s, true );
        }
        skillslist.emplace_back( s, false );
    }

    // Print name and header
    // Post-humanity trumps your pre-Cataclysm life
    // Unless you have a custom profession.
    std::string race;
    if( ch.custom_profession.empty() && ch.crossed_threshold() ) {
        for( const trait_id &mut : ch.get_mutations() ) {
            const mutation_branch &mdata = mut.obj();
            if( mdata.threshold ) {
                race = mdata.name();
                break;
            }
        }
    }

    std::map<std::string, speed_effect> speed_effects;
    for( const auto &elem : ch.get_all_effects() ) {
        for( const auto &_effect_it : elem.second ) {
            const effect &it = _effect_it.second;
            bool reduced = ch.resists_effect( it );
            int move_adjust = it.get_mod( "SPEED", reduced );
            if( move_adjust != 0 ) {
                const std::string dis_text = it.get_speed_name();
                auto &entry = speed_effects[dis_text];
                if( entry.id.empty() ) {
                    entry.id = elem.first.str();
                }
                entry.value += move_adjust;
            }
        }
    }

    return { .effects = std::move( effect_name_and_text ), .traits = std::move( traitslist ),
             .bionics = std::move( bionics_list ), .skills = std::move( skillslist ), .race = std::move( race ),
             .speed_effects = std::move( speed_effects ) };
}
} // namespace

auto character_display::passive_sheet( const Character &you,
                                       const std::string &row ) -> ec::screen_view
{
    const auto sheet = gather_sheet( you );
    auto tab = player_display_tab::stats;
    auto line = 0u;
    // A passive read runs no hook, so the focused skill is described by its own text.
    auto description = std::string{};
    const auto model = sheet_model{
        .you = you, .tab = tab, .line = line, .race = sheet.race, .traits = sheet.traits, .bionics = sheet.bionics,
        .effects = sheet.effects, .skills = sheet.skills, .skill_description = description,
        .speed_effects = sheet.speed_effects,
    };
    for( const auto shown : all_tabs ) {
        // A tab's own id puts it in front at the row it opens on.
        if( row == tab_name( shown ) ) {
            tab = shown;
            break;
        }
        const auto ids = row_ids( model, shown );
        if( const auto found = std::ranges::find( ids, row ); found != ids.end() ) {
            tab = shown;
            line = static_cast<unsigned>( found - ids.begin() );
            break;
        }
    }
    if( tab == player_display_tab::skills ) {
        description = sheet.skills[focused_line( model )].skill->description();
    }
    return sheet_view( model );
}

void character_display::disp_info( Character &ch )
{
    const auto sheet = gather_sheet( ch );
    const auto &effect_name_and_text = sheet.effects;
    const auto &traitslist = sheet.traits;
    const auto &bionics_list = sheet.bionics;
    const auto &skillslist = sheet.skills;
    const auto &race = sheet.race;
    const auto &speed_effects = sheet.speed_effects;
    const unsigned int effect_win_size_y_max = 1 + static_cast<unsigned>( effect_name_and_text.size() );
    const unsigned int trait_win_size_y_max = 1 + static_cast<unsigned>( traitslist.size() );
    const unsigned int bionics_win_size_y_max = 2 + bionics_list.size();
    const unsigned int skill_win_size_y_max = 1 + skillslist.size();
    const unsigned int info_win_size_y = 6;

    const unsigned int grid_width = 26;
    const unsigned int grid_height = 9;

    const unsigned int infooffsetytop = grid_height + 2;
    unsigned int infooffsetybottom = infooffsetytop + 1 + info_win_size_y;

    input_context ctxt( "PLAYER_INFO" );
    ctxt.register_updown();
    ctxt.register_action( "NEXT_TAB", to_translation( "Cycle to next category" ) );
    ctxt.register_action( "PREV_TAB", to_translation( "Cycle to previous category" ) );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "CONFIRM", to_translation( "Toggle skill training / Upgrade stat" ) );
    ctxt.register_action( "CHANGE_PROFESSION_NAME", to_translation( "Change profession name" ) );
    ctxt.register_action( "CHANGE_NAME", to_translation( "Change name" ) );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    border_helper borders;

    player_display_tab curtab = player_display_tab::stats;
    unsigned int line = 0;
    auto native_skill_description = std::string{};
    auto native_skill_values = std::vector<skill_display_values> {};
    const auto model = sheet_model{
        .you = ch, .tab = curtab, .line = line, .race = race, .traits = traitslist, .bionics = bionics_list,
        .effects = effect_name_and_text, .skills = skillslist, .skill_description = native_skill_description,
        .speed_effects = speed_effects,
    };

    catacurses::window w_tip;
    ui_adaptor ui_tip;
    ui_tip.on_screen_resize( [&]( ui_adaptor & ui_tip ) {
        w_tip = catacurses::newwin( 1, FULL_SCREEN_WIDTH + 1, point_zero );
        ui_tip.position_from_window( w_tip );
    } );
    ui_tip.mark_resize();
    ui_tip.on_redraw( [&]( ui_adaptor & ui_tip ) {
        ui_tip.disable_cursor();
        draw_tip( w_tip, prepare_character_header( ch, race ), ctxt );
    } );

    // STATS
    catacurses::window w_stats;
    catacurses::window w_stats_border;
    border_helper::border_info &border_stats = borders.add_border();
    ui_adaptor ui_stats;
    ui_stats.on_screen_resize( [&]( ui_adaptor & ui_stats ) {
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        w_stats = catacurses::newwin( grid_height, grid_width, point( 0, 1 ) );
        // Every grid draws the bottom and right borders. The top and left borders
        // are either not displayed or drawn by another grid.
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        w_stats_border = catacurses::newwin( grid_height + 1, grid_width + 1, point( 0, 1 ) );
        // But we need to specify the full border for border_helper to calculate the
        // border connection.
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        border_stats.set( point( -1, 0 ), point( grid_width + 2, grid_height + 2 ) );
        ui_stats.position_from_window( w_stats_border );
    } );
    ui_stats.mark_resize();
    ui_stats.on_redraw( [&]( ui_adaptor & ui_stats ) {
        borders.draw_border( w_stats_border );
        wnoutrefresh( w_stats_border );
        ui_stats.disable_cursor();
        draw_stats_tab( ui_stats, w_stats, ch, line, curtab );
    } );

    // TRAITS & BIONICS
    unsigned trait_win_size_y;
    unsigned bionics_win_size_y;
    // TRAITS
    catacurses::window w_traits;
    catacurses::window w_traits_border;
    border_helper::border_info &border_traits = borders.add_border();
    ui_adaptor ui_traits;
    ui_traits.on_screen_resize( [&]( ui_adaptor & ui_traits ) {
        std::tie( trait_win_size_y, bionics_win_size_y ) = calculate_shared_column_win_height(
                    static_cast<unsigned>( TERMY ) - infooffsetybottom, trait_win_size_y_max, bionics_win_size_y_max );
        w_traits = catacurses::newwin( trait_win_size_y, grid_width,
                                       point( grid_width + 1, infooffsetybottom ) );
        w_traits_border = catacurses::newwin( trait_win_size_y + 1, grid_width + 2,
                                              point( grid_width, infooffsetybottom ) );
        border_traits.set( point( grid_width, infooffsetybottom - 1 ),
                           point( grid_width + 2, trait_win_size_y + 2 ) );
        ui_traits.position_from_window( w_traits_border );
    } );
    ui_traits.mark_resize();
    ui_traits.on_redraw( [&]( ui_adaptor & ui_traits ) {
        borders.draw_border( w_traits_border );
        wnoutrefresh( w_traits_border );
        ui_traits.disable_cursor();
        draw_traits_tab( ui_traits, w_traits, line, curtab, traitslist );
    } );

    // BIONICS
    catacurses::window w_bionics;
    catacurses::window w_bionics_border;
    border_helper::border_info &border_bionics = borders.add_border();
    ui_adaptor ui_bionics;
    ui_bionics.on_screen_resize( [&]( ui_adaptor & ui_bionics ) {
        std::tie( trait_win_size_y, bionics_win_size_y ) = calculate_shared_column_win_height(
                    static_cast<unsigned>( TERMY ) - infooffsetybottom, trait_win_size_y_max, bionics_win_size_y_max );
        w_bionics = catacurses::newwin( bionics_win_size_y, grid_width,
                                        point( grid_width + 1,
                                               infooffsetybottom + trait_win_size_y + 1 ) );
        w_bionics_border = catacurses::newwin( bionics_win_size_y + 1, grid_width + 2,
                                               point( grid_width, infooffsetybottom + trait_win_size_y + 1 ) );
        border_bionics.set( point( grid_width, infooffsetybottom + trait_win_size_y ),
                            point( grid_width + 2, bionics_win_size_y + 2 ) );
        ui_bionics.position_from_window( w_bionics_border );
    } );
    ui_bionics.mark_resize();
    ui_bionics.on_redraw( [&]( ui_adaptor & ui_bionics ) {
        borders.draw_border( w_bionics_border );
        wnoutrefresh( w_bionics_border );
        ui_bionics.disable_cursor();
        draw_bionics_tab( ui_bionics, w_bionics, ch, line, curtab, bionics_list );
    } );

    // ENCUMBRANCE
    catacurses::window w_encumb;
    catacurses::window w_encumb_border;
    border_helper::border_info &border_encumb = borders.add_border();
    ui_adaptor ui_encumb;
    ui_encumb.on_screen_resize( [&]( ui_adaptor & ui_encumb ) {
        w_encumb = catacurses::newwin( grid_height, grid_width, point( grid_width + 1, 1 ) );
        w_encumb_border = catacurses::newwin( grid_height + 1, grid_width + 1, point( grid_width + 1, 1 ) );
        border_encumb.set( point( grid_width, 0 ), point( grid_width + 2, grid_height + 2 ) );
        ui_encumb.position_from_window( w_encumb_border );
    } );
    ui_encumb.mark_resize();
    ui_encumb.on_redraw( [&]( ui_adaptor & ui_encumb ) {
        borders.draw_border( w_encumb_border );
        wnoutrefresh( w_encumb_border );
        ui_encumb.disable_cursor();
        draw_encumbrance_tab( ui_encumb, w_encumb, ch, line, curtab );
    } );

    // EFFECTS
    unsigned int effect_win_size_y = 0;
    catacurses::window w_effects;
    catacurses::window w_effects_border;
    border_helper::border_info &border_effects = borders.add_border();
    ui_adaptor ui_effects;
    ui_effects.on_screen_resize( [&]( ui_adaptor & ui_effects ) {
        const unsigned int maxy = static_cast<unsigned>( TERMY );
        effect_win_size_y = effect_win_size_y_max;
        if( effect_win_size_y + infooffsetybottom > maxy ) {
            effect_win_size_y = maxy - infooffsetybottom;
        }
        w_effects = catacurses::newwin( effect_win_size_y, grid_width,
                                        point( grid_width * 2 + 2, infooffsetybottom ) );
        w_effects_border = catacurses::newwin( effect_win_size_y + 1, grid_width + 1,
                                               point( grid_width * 2 + 2, infooffsetybottom ) );
        border_effects.set( point( grid_width * 2 + 1, infooffsetybottom - 1 ),
                            point( grid_width + 2, effect_win_size_y + 2 ) );
        ui_effects.position_from_window( w_effects_border );
    } );
    ui_effects.mark_resize();
    ui_effects.on_redraw( [&]( ui_adaptor & ui_effects ) {
        borders.draw_border( w_effects_border );
        wnoutrefresh( w_effects_border );
        ui_effects.disable_cursor();
        draw_effects_tab( ui_effects, w_effects, line, curtab, effect_name_and_text );
    } );

    // SPEED
    catacurses::window w_speed;
    catacurses::window w_speed_border;
    border_helper::border_info &border_speed = borders.add_border();
    ui_adaptor ui_speed;
    ui_speed.on_screen_resize( [&]( ui_adaptor & ui_speed ) {
        w_speed = catacurses::newwin( grid_height, grid_width, point( grid_width * 2 + 2, 1 ) );
        w_speed_border = catacurses::newwin( grid_height + 1, grid_width + 1,
                                             point( grid_width * 2 + 2, 1 ) );
        border_speed.set( point( grid_width * 2 + 1, 0 ),
                          point( grid_width + 2, grid_height + 2 ) );
        ui_speed.position_from_window( w_speed_border );
    } );
    ui_speed.mark_resize();
    ui_speed.on_redraw( [&]( ui_adaptor & ui_speed ) {
        borders.draw_border( w_speed_border );
        wnoutrefresh( w_speed_border );
        ui_speed.disable_cursor();
        draw_speed_tab( w_speed, ch, speed_effects );
    } );

    // SKILLS
    unsigned int skill_win_size_y = 0;
    catacurses::window w_skills;
    catacurses::window w_skills_border;
    border_helper::border_info &border_skills = borders.add_border();
    ui_adaptor ui_skills;
    ui_skills.on_screen_resize( [&]( ui_adaptor & ui_skills ) {
        const unsigned int maxy = static_cast<unsigned>( TERMY );
        skill_win_size_y = skill_win_size_y_max;
        if( skill_win_size_y + infooffsetybottom > maxy ) {
            skill_win_size_y = maxy - infooffsetybottom;
        }
        w_skills = catacurses::newwin( skill_win_size_y, grid_width,
                                       point( 0, infooffsetybottom ) );
        w_skills_border = catacurses::newwin( skill_win_size_y + 1, grid_width + 1,
                                              point( 0, infooffsetybottom ) );
        border_skills.set( point( -1, infooffsetybottom - 1 ),
                           point( grid_width + 2, skill_win_size_y + 2 ) );
        ui_skills.position_from_window( w_skills_border );
    } );
    ui_skills.mark_resize();
    ui_skills.on_redraw( [&]( ui_adaptor & ui_skills ) {
        borders.draw_border( w_skills_border );
        wnoutrefresh( w_skills_border );
        ui_skills.disable_cursor();
        native_skill_values.clear();
        native_skill_values.reserve( skillslist.size() );
        for( const auto i : std::views::iota( std::size_t{0}, skillslist.size() ) ) {
            native_skill_values.push_back( skillslist[i].is_header ? skill_display_values{} :
                                           prepare_skill_values( ch, *skillslist[i].skill,
                                                   curtab == player_display_tab::skills && i == std::max( 1u, line ) ) );
        }
        draw_skills_tab( { .ui = ui_skills, .window = w_skills, .line = line, .tab = curtab,
                           .skills = skillslist, .height = skill_win_size_y, .values = native_skill_values } );
    } );

    // info panel
    catacurses::window w_info;
    catacurses::window w_info_border;
    border_helper::border_info &border_info = borders.add_border();
    ui_adaptor ui_info;
    ui_info.on_screen_resize( [&]( ui_adaptor & ui_info ) {
        w_info = catacurses::newwin( info_win_size_y, FULL_SCREEN_WIDTH,
                                     point( 0, infooffsetytop ) );
        w_info_border = catacurses::newwin( info_win_size_y + 1, FULL_SCREEN_WIDTH + 1,
                                            point( 0, infooffsetytop ) );
        border_info.set( point( -1, infooffsetytop - 1 ),
                         point( FULL_SCREEN_WIDTH + 2, info_win_size_y + 2 ) );
        ui_info.position_from_window( w_info_border );
    } );
    ui_info.mark_resize();
    ui_info.on_redraw( [&]( ui_adaptor & ui_info ) {
        borders.draw_border( w_info_border );
        wnoutrefresh( w_info_border );
        ui_info.disable_cursor();
        native_skill_description = draw_info_window( {
            .window = w_info, .you = ch, .line = line, .tab = curtab,
            .traits = traitslist, .bionics = bionics_list, .effects = effect_name_and_text,
            .skills = skillslist,
        } );
    } );

    const auto screen = ec::screen_scope( ctxt, "character_sheet", { .view = [&]( const ec::boundary_state & ) { return sheet_view( model ); } } );

    bool done = false;

    do {
        ui_manager::redraw_invalidated();

        done = handle_player_display_action( {
            .you = ch, .line = line, .tab = curtab, .context = ctxt,
            .tip = ui_tip, .info = ui_info, .stats = ui_stats, .encumbrance = ui_encumb,
            .traits = ui_traits, .bionics = ui_bionics, .effects = ui_effects, .skills = ui_skills,
            .model = model,
        } );
    } while( !done );
}

void character_display::upgrade_stat_prompt( avatar &you, const character_stat &stat )
{
    const int free_points = you.free_upgrade_points();

    if( free_points <= 0 ) {
        popup( "%s", upgrade_denial( you ) );
        return;
    }

    std::string stat_string;
    switch( stat ) {
        case character_stat::STRENGTH:
            stat_string = _( "strength" );
            break;
        case character_stat::DEXTERITY:
            stat_string = _( "dexterity" );
            break;
        case character_stat::INTELLIGENCE:
            stat_string = _( "intelligence" );
            break;
        case character_stat::PERCEPTION:
            stat_string = _( "perception" );
            break;
        case character_stat::DUMMY_STAT:
            stat_string = _( "invalid stat" );
            debugmsg( "Tried to use invalid stat" );
            break;
        default:
            return;
    }

    if( query_yn( _( "Are you sure you want to raise %s?  %d points available." ), stat_string,
                  free_points ) ) {
        you.upgrade_stat( stat );
    }
}
