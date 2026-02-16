#include "character_display.h" // IWYU pragma: associated

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>

#include "addiction.h"
#include "avatar.h"
#include "bionics.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "character_effects.h"
#include "character_encumbrance.h"
#include "debug.h"
#include "effect.h"
#include "game.h"
#include "input.h"
#include "melee.h"
#include "mutation.h"
#include "messages.h"
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
#include "ui_manager.h"
#include "units.h"
#include "units_utility.h"
#include "weather.h"

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

static int get_temp_conv( const Character &c, const bodypart_str_id &bp )
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

    int temp_conv = get_temp_conv( c, bp );
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
static int temperature_print_rescaling( int temp )
{
    return ( temp / 100.0 ) * 2 - 100;
}

static bool should_combine_bps( const Character &ch,
                                const bodypart_str_id &l, const bodypart_str_id &r,
                                const item *selected_clothing )
{
    const char_encumbrance_data enc_data = ch.get_encumbrance();
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

void character_display::print_encumbrance( ui_adaptor &ui, const catacurses::window &win,
        const Character &ch,
        const int line, const item *selected_clothing )
{
    // bool represents whether the part has been combined with its other half
    const std::vector<std::pair<bodypart_str_id, bool>> bps = list_and_combine_bps( ch,
            selected_clothing );

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
    const char_encumbrance_data enc_data = ch.get_encumbrance();
    for( int i = 0; i < height; ++i ) {
        int thisline = firstline + i;
        if( thisline < 0 ) {
            continue;
        }
        if( static_cast<size_t>( thisline ) >= bps.size() ) {
            break;
        }
        const bodypart_str_id &bp = bps[thisline].first;
        const bool combine = bps[thisline].second;
        const encumbrance_data &e = enc_data.elems.at( bp );
        const bool highlighted = selected_clothing ? selected_clothing->covers( bp.id() ) : false;
        std::string out = body_part_name_as_heading( bp, combine ? 2 : 1 );
        if( utf8_width( out ) > 7 ) {
            out = utf8_truncate( out, 7 );
        }

        // Two different highlighting schemes, highlight if the line is selected as per line being set.
        // Make the text green if this part is covered by the passed in item.
        const bool highlight_line = thisline == line;
        nc_color limb_color = highlight_line ?
                              ( highlighted ? h_green : h_light_gray ) :
                              ( highlighted ? c_green : c_light_gray );
        const int y_pos = 1 + i;
        if( highlight_line ) {
            ui.set_cursor( win, point( 1, y_pos ) );
        }
        mvwprintz( win, point( 1, y_pos ), limb_color, "%s", out );
        // accumulated encumbrance from clothing, plus extra encumbrance from layering
        mvwprintz( win, point( 8, 1 + i ), encumb_color( e.encumbrance ), "%3d",
                   e.encumbrance - e.layer_penalty );
        // separator in low toned color
        mvwprintz( win, point( 11, y_pos ), c_light_gray, "+" );
        // take into account the new encumbrance system for layers
        mvwprintz( win, point( 12, y_pos ), encumb_color( e.encumbrance ), "%-3d", e.layer_penalty );
        // print warmth, tethered to right hand side of the window

        mvwprintz( win, point( width - 6, y_pos ), warmth::bodytemp_color( ch, bp ), "(% 3d)",
                   temperature_print_rescaling( get_temp_conv( ch, bp ) ) );
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
    static const std::array<skill_id, 5> cqb_skills = { {
            skill_id( "melee" ), skill_id( "unarmed" ), skill_id( "cutting" ),
            skill_id( "bashing" ), skill_id( "stabbing" ),
        }
    };
    return std::ranges::find( cqb_skills, id ) != cqb_skills.end();
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
        nc_color cstatus;
        if( cur <= 0 ) {
            cstatus = c_dark_gray;
        } else if( cur < max / 2 ) {
            cstatus = c_red;
        } else if( cur < max ) {
            cstatus = c_light_red;
        } else if( cur == max ) {
            cstatus = c_white;
        } else if( cur < max * 1.5 ) {
            cstatus = c_light_green;
        } else {
            cstatus = c_green;
        }

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

static void draw_stats_info( const catacurses::window &w_info, const Character &you,
                             const unsigned line )
{
    werase( w_info );
    nc_color col_temp = c_light_gray;

    if( line == 0 ) {
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_magenta,
                        _( "Strength affects your melee damage, the amount of weight you can carry, your total HP, "
                           "your resistance to many diseases, and the effectiveness of actions which require brute force." ) );
        print_colored_text( w_info, point( 1, 3 ), col_temp, c_light_gray,
                            string_format( _( "Base HP: <color_white>%d</color>" ),
                                           you.get_part_hp_max( bodypart_id( "torso" ) ) ) );
        print_colored_text( w_info, point( 1, 4 ), col_temp, c_light_gray,
                            string_format( _( "Carry weight (%s): <color_white>%.1f</color>" ), weight_units(),
                                           convert_weight( you.weight_capacity() ) ) );
        print_colored_text( w_info, point( 1, 5 ), col_temp, c_light_gray,
                            string_format( _( "Melee damage: <color_white>%.1f</color>" ), you.bonus_damage( false ) ) );
    } else if( line == 1 ) {
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_magenta,
                        _( "Dexterity affects your chance to hit in melee combat, helps you steady your "
                           "gun for ranged combat, and enhances many actions that require finesse." ) );
        print_colored_text( w_info, point( 1, 3 ), col_temp, c_light_gray,
                            string_format( _( "Melee to-hit bonus: <color_white>%+.1lf</color>" ),
                                           you.get_melee_hit( you.used_weapon(), melee::default_attack( you.used_weapon() ) ) ) );
        print_colored_text( w_info, point( 1, 4 ), col_temp, c_light_gray,
                            string_format( _( "Ranged penalty: <color_white>%+d</color>" ),
                                           -std::abs( you.ranged_dex_mod() ) ) );
        print_colored_text( w_info, point( 1, 5 ), col_temp, c_light_gray,
                            string_format( _( "Throwing penalty per target's dodge: <color_white>%+d</color>" ),
                                           ranged::throw_dispersion_per_dodge( you, false ) ) );
    } else if( line == 2 ) {
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_magenta,
                        _( "Intelligence is less important in most situations, but it is vital for more complex tasks like "
                           "electronics crafting.  It also affects how much skill you can pick up from reading a book." ) );
        if( you.rust_rate() ) {
            print_colored_text( w_info, point( 1, 3 ), col_temp, c_light_gray,
                                string_format( _( "Skill rust: <color_white>%d%%</color>" ), you.rust_rate() ) );
        }
        print_colored_text( w_info, point( 1, 4 ), col_temp, c_light_gray,
                            string_format( _( "Read times: <color_white>%d%%</color>" ), you.read_speed( false ) ) );
        print_colored_text( w_info, point( 1, 5 ), col_temp, c_light_gray,
                            string_format( _( "Crafting bonus: <color_white>+%d%%</color>" ), you.get_int() ) );
    } else if( line == 3 ) {
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_magenta,
                        _( "Perception is the most important stat for ranged combat.  It's also used for "
                           "detecting traps and other things of interest." ) );
        print_colored_text( w_info, point( 1, 3 ), col_temp, c_light_gray,
                            string_format( _( "Base night vision range: <color_white>%.1f</color>" ),
                                           vision::nv_range_from_per( you.get_per() ) ) );
        print_colored_text( w_info, point( 1, 4 ), col_temp, c_light_gray,
                            string_format( _( "Trap detection level: <color_white>%d</color>" ), you.get_per() ) );
        if( you.ranged_per_mod() > 0 ) {
            print_colored_text( w_info, point( 1, 5 ), col_temp, c_light_gray,
                                string_format( _( "Aiming penalty: <color_white>%+d</color>" ), -you.ranged_per_mod() ) );
        }
    } else if( line == 4 ) {
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        const int lines = fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_magenta,
                                          _( "Your height.  Simply how tall you are." ) );
        fold_and_print( w_info, point( 1, 1 + lines ), FULL_SCREEN_WIDTH - 2, c_light_gray,
                        you.height_string() );
    } else if( line == 5 ) {
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        const int lines = fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_magenta,
                                          _( "This is how old you are." ) );
        fold_and_print( w_info, point( 1, 1 + lines ), FULL_SCREEN_WIDTH - 2, c_light_gray,
                        you.age_string() );
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
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        const auto fmtStr = cnt > 1 ? "%s\n\nYou have %s instances of this bionic installed." : "%s";
        fold_and_print(
            w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_light_gray, fmtStr, bio.info().description, cnt );
    }
    wnoutrefresh( w_info );
}

static void draw_effects_tab( ui_adaptor &ui, const catacurses::window &w_effects,
                              const unsigned line, const player_display_tab curtab,
                              const std::vector<std::pair<std::string, std::string>> &effect_name_and_text )
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
                        highlight_line ? h_light_gray : c_light_gray, effect_name_and_text[i].first );
    }
    if( do_draw_scrollbar ) {
        draw_scrollbar( w_effects, range.first, height, effect_name_and_text.size(), point( width + 1, 1 ),
                        c_white, true );
    }
    wnoutrefresh( w_effects );
}

static void draw_effects_info( const catacurses::window &w_info, const unsigned line,
                               const std::vector<std::pair<std::string, std::string>> &effect_name_and_text )
{
    werase( w_info );
    const size_t actual_size = effect_name_and_text.size();
    if( line < actual_size ) {
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_light_gray,
                        effect_name_and_text[line].second );
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
    int empty_hand_base_damage = you.get_skill_level( skill_unarmed );
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

static void draw_skills_tab( ui_adaptor &ui, const catacurses::window &w_skills,
                             Character &you, unsigned int line, const player_display_tab curtab,
                             std::vector<HeaderSkill> &skillslist,
                             const size_t skill_win_size_y )
{
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
        const SkillLevel &level = you.get_skill_level_object( aSkill->ident() );

        if( skillslist[i].is_header ) {
            const SkillDisplayType t = SkillDisplayType::get_skill_type( aSkill->display_category() );
            std::string type_name = t.display_string();
            mvwprintz( w_skills, point( 0, y_pos ), c_light_gray, header_spaces );
            center_print( w_skills, y_pos, c_yellow, type_name );
        } else {
            const bool can_train = level.can_train();
            const bool training = level.isTraining();
            const bool rusting = level.isRusting();
            int exercise = level.exercise();
            int level_num = level.level();
            bool locked = false;
            if( you.has_active_bionic( bionic_id( "bio_cqb" ) ) && is_cqb_skill( aSkill->ident() ) ) {
                level_num = 5;
                exercise = 0;
                locked = true;
            }
            if( is_current_tab && i == line ) {
                ui.set_cursor( w_skills, point( 1, y_pos ) );
                if( locked ) {
                    cstatus = h_yellow;
                } else if( !can_train ) {
                    cstatus = rusting ? h_light_red : h_white;
                } else if( exercise >= 100 ) {
                    cstatus = training ? h_pink : h_magenta;
                } else if( rusting ) {
                    cstatus = training ? h_light_red : h_red;
                } else {
                    cstatus = training ? h_light_blue : h_blue;
                }
                mvwprintz( w_skills, point( 1, y_pos ), cstatus, std::string( col_width, ' ' ) );
            } else {
                if( locked ) {
                    cstatus = c_yellow;
                } else if( rusting ) {
                    cstatus = training ? c_light_red : c_red;
                } else if( !can_train ) {
                    cstatus = c_white;
                } else {
                    cstatus = training ? c_light_blue : c_blue;
                }
                mvwprintz( w_skills, point( 1, y_pos ), c_light_gray, std::string( col_width, ' ' ) );
            }
            mvwprintz( w_skills, point( 1, y_pos ), cstatus, "%s:", aSkill->name() );
            if( aSkill->ident() == skill_id( "dodge" ) ) {
                mvwprintz( w_skills, point( 14, y_pos ), cstatus, "%4.1f/%-2d(%2d%%)",
                           you.get_dodge(), level_num, exercise < 0 ? 0 : exercise );
            }
            if( aSkill->ident() == skill_id( "unarmed" ) ) {
                mvwprintz( w_skills, point( 15, y_pos ), cstatus, "%3d/%-2d(%2d%%)",
                           character_display::display_empty_handed_base_damage( you ), level_num,
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

static void draw_skills_info( const catacurses::window &w_info, unsigned int line,
                              const std::vector<HeaderSkill> &skillslist )
{
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
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        fold_and_print( w_info, point( 1, 0 ), FULL_SCREEN_WIDTH - 2, c_light_gray,
                        selectedSkill->description() );
    }
    wnoutrefresh( w_info );
}

static void draw_speed_tab( const catacurses::window &w_speed,
                            const Character &you,
                            const std::map<std::string, int> &speed_effects )
{
    werase( w_speed );
    // Finally, draw speed.
    center_print( w_speed, 0, c_light_gray, _( title_SPEED ) );
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    mvwprintz( w_speed, point( 1, 1 ), c_light_gray, _( "Base Move Cost:" ) );
    mvwprintz( w_speed, point( 1, 2 ), c_light_gray, _( "Current Speed:" ) );
    int newmoves = you.get_speed();
    int pen = 0;
    unsigned int line = 3;
    if( you.weight_carried() > you.weight_capacity() ) {
        pen = 25 * ( you.weight_carried() - you.weight_capacity() ) / ( you.weight_capacity() );
        //~ %s: Overburdened (already left-justified), %2d%%: speed penalty
        mvwprintz( w_speed, point( 1, line ), c_red, pgettext( "speed penalty", "%s-%2d%%" ),
                   left_justify( _( "Overburdened" ), 20 ), pen );
        ++line;
    }
    pen = character_effects::get_pain_penalty( you ).speed;
    if( pen >= 1 ) {
        //~ %s: Pain (already left-justified), %2d%%: speed penalty
        mvwprintz( w_speed, point( 1, line ), c_red, pgettext( "speed penalty", "%s-%2d%%" ),
                   left_justify( _( "Pain" ), 20 ), pen );
        ++line;
    }
    if( you.get_thirst() > thirst_levels::very_thirsty ) {
        pen = std::abs( character_effects::get_thirst_speed_penalty( you.get_thirst() ) );
        //~ %s: Thirsty/Parched (already left-justified), %2d%%: speed penalty
        mvwprintz( w_speed, point( 1, line ), c_red, pgettext( "speed penalty", "%s-%2d%%" ),
                   left_justify( _( "Thirst" ), 20 ), pen );
        ++line;
    }
    if( character_effects::get_kcal_speed_penalty( you.get_kcal_percent() ) < 0 ) {
        pen = std::abs( character_effects::get_kcal_speed_penalty( you.get_kcal_percent() ) );
        //~ %s: Starving/Underfed (already left-justified), %2d%%: speed penalty
        mvwprintz( w_speed, point( 1, line ), c_red, pgettext( "speed penalty", "%s-%2d%%" ),
                   left_justify( _( "Starving" ), 20 ), pen );
        ++line;
    }
    if( you.has_trait( trait_id( "SUNLIGHT_DEPENDENT" ) ) && !g->is_in_sunlight( you.pos() ) ) {
        pen = ( g->light_level( you.posz() ) >= 12 ? 5 : 10 );
        //~ %s: Out of Sunlight (already left-justified), %2d%%: speed penalty
        mvwprintz( w_speed, point( 1, line ), c_red, pgettext( "speed penalty", "%s-%2d%%" ),
                   left_justify( _( "Out of Sunlight" ), 20 ), pen );
        ++line;
    }

    const float temperature_speed_modifier = you.mutation_value( "temperature_speed_modifier" );
    if( temperature_speed_modifier != 0 ) {
        nc_color pen_color;
        std::string pen_sign;
        const auto player_local_temp = units::to_fahrenheit( get_weather().get_temperature( you.pos() ) );
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
            mvwprintz( w_speed, point( 1, line ), pen_color, pgettext( "speed modifier", "%s%s%2d%%" ),
                       left_justify( _( "Cold-Blooded" ), 20 ), pen_sign, std::abs( pen ) );
            ++line;
        }
    }

    int quick_bonus = static_cast<int>( std::round( ( you.mutation_value( "speed_modifier" ) - 1 ) *
                                        100 ) );
    int bio_speed_bonus = 10;
    if( quick_bonus != 0 ) {
        std::string pen_sign = quick_bonus >= 0 ? "+" : "-";
        nc_color pen_color = quick_bonus >= 0 ? c_green : c_red;
        //~ %s: Mutations (already left-justified), %s: sign of bonus/penalty, %2d%%: speed modifier
        mvwprintz( w_speed, point( 1, line ), pen_color, pgettext( "speed bonus", "%s%s%2d%%" ),
                   left_justify( _( "Mutations" ), 20 ), pen_sign, std::abs( quick_bonus ) );
        ++line;
    }
    if( you.has_bionic( bionic_id( "bio_speed" ) ) ) {
        mvwprintz( w_speed, point( 1, line ), c_green,
                   pgettext( "speed bonus", "Bionic Speed        +%2d%%" ), bio_speed_bonus );
        ++line;
    }

    for( const std::pair<const std::string, int> &speed_effect : speed_effects ) {
        nc_color col = ( speed_effect.second > 0 ? c_green : c_red );
        mvwprintz( w_speed, point( 1, line ), col, "%s", speed_effect.first );
        mvwprintz( w_speed, point( 21, line ), col, ( speed_effect.second > 0 ? "+" : "-" ) );
        mvwprintz( w_speed, point( std::abs( speed_effect.second ) >= 10 ? 22 : 23, line ), col, "%d%%",
                   std::abs( speed_effect.second ) );
        ++line;
    }

    int runcost = you.run_cost( 100 );
    nc_color col = ( runcost <= 100 ? c_green : c_red );
    mvwprintz( w_speed, point( 21 + ( runcost >= 100 ? 0 : ( runcost < 10 ? 2 : 1 ) ), 1 ), col,
               "%d", runcost );
    col = ( newmoves >= 100 ? c_green : c_red );
    mvwprintz( w_speed, point( 21 + ( newmoves >= 100 ? 0 : ( newmoves < 10 ? 2 : 1 ) ), 2 ), col,
               "%d", newmoves );
    wnoutrefresh( w_speed );
}

static void draw_info_window( const catacurses::window &w_info, const Character &you,
                              const unsigned line, const player_display_tab curtab,
                              const std::vector<trait_id> &traitslist,
                              const std::vector<std::pair<bionic, int>> &bionicslist,
                              const std::vector<std::pair<std::string, std::string>> &effect_name_and_text,
                              const std::vector<HeaderSkill> &skillslist )
{
    switch( curtab ) {
        case player_display_tab::stats:
            draw_stats_info( w_info, you, line );
            break;
        case player_display_tab::encumbrance:
            draw_encumbrance_info( w_info, you, line );
            break;
        case player_display_tab::skills:
            draw_skills_info( w_info, line, skillslist );
            break;
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
}

static void draw_tip( const catacurses::window &w_tip, const Character &you,
                      const std::string &race, const input_context &ctxt )
{
    werase( w_tip );

    auto gender = you.male ? _( "Male" ) : _( "Female" );

    // Print name and header
    if( you.custom_profession.empty() ) {
        if( you.crossed_threshold() ) {
            //~ player info window: 1s - name, 2s - gender, 3s - Prof or Mutation name
            mvwprintz( w_tip, point_zero, c_white, _( " %1$s | %2$s | %3$s" ),
                       you.name, gender, race );
        } else if( !you.prof.is_valid() || you.prof == profession::generic() ) {
            // Regular person. Nothing interesting.
            //~ player info window: 1s - name, 2s - gender '|' - field separator.
            mvwprintz( w_tip, point_zero, c_white, _( " %1$s | %2$s" ),
                       you.name, gender );
        } else {
            mvwprintz( w_tip, point_zero, c_white, _( " %1$s | %2$s | %3$s" ),
                       you.name, gender, you.prof->gender_appropriate_name( you.male ) );
        }
    } else {
        mvwprintz( w_tip, point_zero, c_white, _( " %1$s | %2$s | %3$s" ),
                   you.name,  gender, you.custom_profession );
    }

    right_print( w_tip, 0, 1, c_white, string_format(
                     _( "[<color_yellow>%s</color>]" ),
                     ctxt.get_desc( "HELP_KEYBINDINGS" ) ) );

    wnoutrefresh( w_tip );
}

static bool handle_player_display_action( Character &you, unsigned int &line,
        player_display_tab &curtab, input_context &ctxt,
        const ui_adaptor &ui_tip, const ui_adaptor &ui_info,
        const ui_adaptor &ui_stats, const ui_adaptor &ui_encumb,
        const ui_adaptor &ui_traits, const ui_adaptor &ui_bionics,
        const ui_adaptor &ui_effects, const ui_adaptor &ui_skills,
        const std::vector<trait_id> &traitslist,
        const std::vector<std::pair<bionic, int>> &bionicslist,
        const std::vector<std::pair<std::string, std::string>> &effect_name_and_text,
        const std::vector<HeaderSkill> &skillslist )
{
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
    unsigned int line_end = 0;
    switch( curtab ) {
        case player_display_tab::stats:
            line_end = 6;
            break;
        case player_display_tab::encumbrance: {
            const std::vector<std::pair<bodypart_str_id, bool>> bps = list_and_combine_bps( you, nullptr );
            line_end = bps.size();
            break;
        }
        case player_display_tab::traits:
            line_end = traitslist.size();
            break;
        case player_display_tab::bionics:
            line_end = bionicslist.size();
            break;
        case player_display_tab::effects:
            line_end = effect_name_and_text.size();
            break;
        case player_display_tab::skills:
            line_beg = 1; // skip first header
            line_end = skillslist.size();
            break;
        case player_display_tab::num_tabs:
            abort();
    }
    if( line_beg >= line_end || line < line_beg ) {
        line = line_beg;
    } else if( line > line_end - 1 ) {
        line = line_end - 1;
    }

    bool done = false;
    std::string action = ctxt.handle_input();

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
                    you.get_skill_level_object( selectedSkill->ident() ).toggleTraining();
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

void character_display::disp_info( Character &ch )
{
    std::vector<std::pair<std::string, std::string>> effect_name_and_text;
    for( auto &elem : ch.get_all_effects() ) {
        for( auto &_effect_it : elem.second ) {
            const std::string tmp = _effect_it.second.disp_name();
            if( _effect_it.second.is_removed() || tmp.empty() ) {
                continue;
            }
            effect_name_and_text.emplace_back( tmp, _effect_it.second.disp_desc() );
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
        effect_name_and_text.emplace_back( _( "Pain" ), pain_text );
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

        effect_name_and_text.emplace_back( starvation_name, starvation_text );
    }

    if( ( ch.has_trait( trait_id( "TROGLO" ) ) && g->is_in_sunlight( ch.pos() ) &&
          get_weather().weather_id->sun_intensity >= sun_intensity_type::high ) ||
        ( ch.has_trait( trait_id( "TROGLO2" ) ) && g->is_in_sunlight( ch.pos() ) &&
          get_weather().weather_id->sun_intensity < sun_intensity_type::high )
      ) {
        effect_name_and_text.emplace_back( _( "In Sunlight" ),
                                           _( "The sunlight irritates you.\n"
                                              "Strength - 1;    Dexterity - 1;    Intelligence - 1;    Perception - 1" )
                                         );
    } else if( ch.has_trait( trait_id( "TROGLO2" ) ) && g->is_in_sunlight( ch.pos() ) ) {
        effect_name_and_text.emplace_back( _( "In Sunlight" ),
                                           _( "The sunlight irritates you badly.\n"
                                              "Strength - 2;    Dexterity - 2;    Intelligence - 2;    Perception - 2" )
                                         );
    } else if( ch.has_trait( trait_id( "TROGLO3" ) ) && g->is_in_sunlight( ch.pos() ) ) {
        effect_name_and_text.emplace_back( _( "In Sunlight" ),
                                           _( "The sunlight irritates you terribly.\n"
                                              "Strength - 4;    Dexterity - 4;    Intelligence - 4;    Perception - 4" )
                                         );
    }

    for( auto &elem : ch.addictions ) {
        if( elem.sated < 0_turns && elem.intensity >= MIN_ADDICTION_LEVEL ) {
            effect_name_and_text.emplace_back( addiction_name( elem ), addiction_text( elem ) );
        }
    }

    const unsigned int effect_win_size_y_max = 1 + static_cast<unsigned>( effect_name_and_text.size() );

    std::vector<trait_id> traitslist = ch.get_mutations( false );
    std::ranges::sort( traitslist, trait_display_sort );
    const unsigned int trait_win_size_y_max = 1 + static_cast<unsigned>( traitslist.size() );

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
    const unsigned int bionics_win_size_y_max = 2 + bionics_list.size();

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
    const unsigned int skill_win_size_y_max = 1 + skillslist.size();
    const unsigned int info_win_size_y = 6;

    const unsigned int grid_width = 26;
    const unsigned int grid_height = 9;

    const unsigned int infooffsetytop = grid_height + 2;
    unsigned int infooffsetybottom = infooffsetytop + 1 + info_win_size_y;

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

    input_context ctxt( "PLAYER_INFO" );
    ctxt.register_updown();
    ctxt.register_action( "NEXT_TAB", to_translation( "Cycle to next category" ) );
    ctxt.register_action( "PREV_TAB", to_translation( "Cycle to previous category" ) );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "CONFIRM", to_translation( "Toggle skill training / Upgrade stat" ) );
    ctxt.register_action( "CHANGE_PROFESSION_NAME", to_translation( "Change profession name" ) );
    ctxt.register_action( "CHANGE_NAME", to_translation( "Change name" ) );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    std::map<std::string, int> speed_effects;
    for( auto &elem : ch.get_all_effects() ) {
        for( std::pair<const bodypart_str_id, effect> &_effect_it : elem.second ) {
            effect &it = _effect_it.second;
            bool reduced = ch.resists_effect( it );
            int move_adjust = it.get_mod( "SPEED", reduced );
            if( move_adjust != 0 ) {
                const std::string dis_text = it.get_speed_name();
                speed_effects[dis_text] += move_adjust;
            }
        }
    }

    border_helper borders;

    player_display_tab curtab = player_display_tab::stats;
    unsigned int line = 0;

    catacurses::window w_tip;
    ui_adaptor ui_tip;
    ui_tip.on_screen_resize( [&]( ui_adaptor & ui_tip ) {
        w_tip = catacurses::newwin( 1, FULL_SCREEN_WIDTH + 1, point_zero );
        ui_tip.position_from_window( w_tip );
    } );
    ui_tip.mark_resize();
    ui_tip.on_redraw( [&]( ui_adaptor & ui_tip ) {
        ui_tip.disable_cursor();
        draw_tip( w_tip, ch, race, ctxt );
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
        draw_skills_tab( ui_skills, w_skills, ch, line, curtab, skillslist, skill_win_size_y );
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
        draw_info_window( w_info, ch, line, curtab,
                          traitslist, bionics_list, effect_name_and_text, skillslist );
    } );

    bool done = false;

    do {
        ui_manager::redraw_invalidated();

        done = handle_player_display_action( ch, line, curtab, ctxt, ui_tip, ui_info,
                                             ui_stats, ui_encumb, ui_traits, ui_bionics, ui_effects, ui_skills,
                                             traitslist, bionics_list, effect_name_and_text, skillslist );
    } while( !done );
}

void character_display::upgrade_stat_prompt( avatar &you, const character_stat &stat )
{
    const int free_points = you.free_upgrade_points();

    if( free_points <= 0 ) {
        std::optional<int> xp_remains = you.kill_xp_for_next_point();
        if( !xp_remains ) {
            popup( _( "You've already reached maximum level." ) );
        } else {
            popup( _( "Needs %d more experience to gain next level." ), *xp_remains );
        }
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
