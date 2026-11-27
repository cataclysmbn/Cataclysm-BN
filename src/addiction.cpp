#include "addiction.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>

#include "action_time_scale.h"
#include "calendar.h"
#include "catalua.h"
#include "character.h"
#include "debug.h"
#include "enums.h"
#include "enum_conversions.h"
#include "generic_factory.h"
#include "json.h"
#include "morale_types.h"
#include "rng.h"
#include "translations.h"
#include "type_id_implement.h"

static const efftype_id effect_hallu( "hallu" );
static const efftype_id effect_meth( "meth" );
static const efftype_id effect_shakes( "shakes" );
static const efftype_id effect_took_antinarcoleptic( "took_antinarcoleptic" );

static const trait_id trait_MUT_JUNKIE( "MUT_JUNKIE" );

namespace io
{

template<>
std::string enum_to_string<add_type>( add_type data )
{
    switch( data ) {
        // *INDENT-OFF*
        case add_type::NONE: return "NONE";
        case add_type::CAFFEINE: return "CAFFEINE";
        case add_type::ALCOHOL: return "ALCOHOL";
        case add_type::SLEEP: return "SLEEP";
        case add_type::PKILLER: return "PKILLER";
        case add_type::SPEED: return "SPEED";
        case add_type::CIG: return "CIG";
        case add_type::COKE: return "COKE";
        case add_type::CRACK: return "CRACK";
        case add_type::MUTAGEN: return "MUTAGEN";
        case add_type::DIAZEPAM: return "DIAZEPAM";
        case add_type::MARLOSS_R: return "MARLOSS_R";
        case add_type::MARLOSS_B: return "MARLOSS_B";
        case add_type::MARLOSS_Y: return "MARLOSS_Y";
        // *INDENT-ON*
        case add_type::NUM_ADD_TYPES:
            break;
    }
    debugmsg( "Invalid add_type" );
    abort();
}

} // namespace io

namespace
{

generic_factory<addiction_type_data> addiction_type_data_factory( "addiction type" );

} // namespace

IMPLEMENT_STRING_AND_INT_IDS( addiction_type_data, addiction_type_data_factory )

void addiction_type_data::load_type( const JsonObject &jo, const std::string &src )
{
    addiction_type_data_factory.load( jo, src );
}

void addiction_type_data::check_all()
{
    addiction_type_data_factory.check();
}

void addiction_type_data::reset()
{
    addiction_type_data_factory.reset();
}

void addiction_type_data::load( const JsonObject &jo, const std::string & )
{
    mandatory( jo, was_loaded, "name", name );
    mandatory( jo, was_loaded, "description", description );

    optional( jo, was_loaded, "craving_morale", craving_morale, morale_type() );
    optional( jo, was_loaded, "legacy_enum", legacy_enum, add_type::NONE );
    optional( jo, was_loaded, "on_withdrawal", on_withdrawal, std::string() );
}

void addiction_type_data::check() const
{
    if( !craving_morale.is_empty() && !craving_morale.is_valid() ) {
        debugmsg( "Addiction type \"%s\" has unknown craving_morale \"%s\"",
                  id.str(), craving_morale.str() );
    }
    if( legacy_enum != add_type::NONE && addiction_id_from_legacy( legacy_enum ) != id ) {
        debugmsg( "Addiction type \"%s\" uses legacy_enum %s which maps to a different id",
                  id.str(), io::enum_to_string( legacy_enum ) );
    }
}

addiction_type_id addiction_id_from_legacy( add_type legacy )
{
    switch( legacy ) {
        // *INDENT-OFF*
        case add_type::NONE: break;
        case add_type::CAFFEINE: return addiction_type_id( "caffeine" );
        case add_type::ALCOHOL: return addiction_type_id( "alcohol" );
        case add_type::SLEEP: return addiction_type_id( "sleeping pill" );
        case add_type::PKILLER: return addiction_type_id( "opiate" );
        case add_type::SPEED: return addiction_type_id( "amphetamine" );
        case add_type::CIG: return addiction_type_id( "nicotine" );
        case add_type::COKE: return addiction_type_id( "cocaine" );
        case add_type::CRACK: return addiction_type_id( "crack" );
        case add_type::MUTAGEN: return addiction_type_id( "mutagen" );
        case add_type::DIAZEPAM: return addiction_type_id( "diazepam" );
        case add_type::MARLOSS_R: return addiction_type_id( "marloss_r" );
        case add_type::MARLOSS_B: return addiction_type_id( "marloss_b" );
        case add_type::MARLOSS_Y: return addiction_type_id( "marloss_y" );
        // *INDENT-ON*
        case add_type::NUM_ADD_TYPES:
            break;
    }
    return addiction_type_id();
}

add_type legacy_add_type( const addiction_type_id &type )
{
    if( type.is_empty() || !type.is_valid() ) {
        return add_type::NONE;
    }
    return type.obj().legacy_enum;
}

void marloss_add( Character &u, int in, const char *msg );

void addict_effect( Character &u, addiction &add )
{
    const addiction_type_data &type_data = add.type.obj();
    if( type_data.on_withdrawal.rfind( "lua:", 0 ) == 0 ) {
        // A Lua withdrawal callback fully replaces the hardcoded behavior.
        cata::run_lua_addiction_withdrawal( type_data.on_withdrawal.substr( 4 ), u, add );
        return;
    }

    const add_type type = type_data.legacy_enum;
    const int in = std::min( 20, add.intensity );
    const int current_stim = u.get_stim();

    switch( type ) {
        case add_type::CIG:
            u.mod_int_bonus( -1, true );
            if( !one_in( 2000 - 20 * in ) ) {
                break;
            }

            u.add_msg_if_player( rng( 0, 6 ) < in ?
                                 _( "You need some nicotine." ) :
                                 _( "You could use some nicotine." ) );
            u.add_morale( MORALE_CRAVING_NICOTINE, -15, -3 * in );
            if( one_in( 800 - 50 * in ) ) {
                u.mod_fatigue( 1 );
            }
            if( current_stim > -5 * in && one_in( 400 - 20 * in ) ) {
                u.mod_stim( -1 );
            }
            break;

        case add_type::CAFFEINE:
            if( !one_in( 2000 - 20 * in ) ) {
                break;
            }

            u.add_msg_if_player( m_warning, _( "You want some caffeine." ) );
            u.add_morale( MORALE_CRAVING_CAFFEINE, -5, -30 );
            if( current_stim > -10 * in && rng( 0, 10 ) < in ) {
                u.mod_stim( -1 );
            }
            if( rng( 8, 400 ) < in ) {
                u.add_msg_if_player( m_bad, _( "Your hands start shaking… you need it bad!" ) );
                u.add_effect( effect_shakes, 2_minutes );
            }
            break;

        case add_type::ALCOHOL:
        case add_type::DIAZEPAM: {
            const auto morale_type = type == add_type::ALCOHOL ? MORALE_CRAVING_ALCOHOL :
                                     MORALE_CRAVING_DIAZEPAM;

            u.mod_per_bonus( -1, true );
            u.mod_int_bonus( -1, true );
            if( x_in_y( in, to_turns<int>( 2_hours ) ) ) {
                u.mod_healthy_mod( -1, -in * 10 );
            }

            const bool is_alcohol = type == add_type::ALCOHOL;
            const bool on_ghb = is_alcohol && u.has_effect( effect_took_antinarcoleptic );

            // Hardcoded morale penalties depending on GHB effect
            const int morale_penalty_min = on_ghb ? -18 : -35;  // roughly half
            const int morale_penalty_max = on_ghb ? -5 * in : -10 * in;

            if( is_alcohol || type == add_type::DIAZEPAM ) {
                if( x_in_y( in, to_turns<int>( 2_minutes ) * 20 ) && ( !on_ghb || one_in( 2 ) ) ) {
                    const std::string msg_1 = is_alcohol ?
                                              _( "You could use a drink." ) :
                                              _( "You could use some diazepam." );
                    u.add_msg_if_player( m_warning, msg_1 );
                    u.add_morale( morale_type, morale_penalty_min, morale_penalty_max );
                    if( on_ghb && one_in( 5 ) ) {
                        u.add_msg_if_player( _( "The GHB makes it not so bad." ) );
                    }
                } else if( action_time_scale::once_every_this_tick( 1_minutes ) &&
                           rng( 8, 300 ) < in && ( !on_ghb || one_in( 2 ) ) ) {
                    const std::string msg_2 = is_alcohol ?
                                              _( "Your hands start shaking… you need a drink bad!" ) :
                                              _( "You're shaking… you need some diazepam!" );
                    u.add_msg_if_player( m_bad, msg_2 );
                    u.add_morale( morale_type, morale_penalty_min, morale_penalty_max );

                    const time_duration shake_duration = on_ghb ? 2_minutes : 5_minutes;
                    u.add_effect( effect_shakes, shake_duration );
                    if( on_ghb && one_in( 5 ) ) {
                        u.add_msg_if_player( _( "The GHB makes it not so bad." ) );
                    }
                }
            }
            break;
        }

        case add_type::SLEEP:
            // No effects here--just in player::can_sleep()
            // EXCEPT!  Prolong this addiction longer than usual.
            if( one_in( 2 ) && add.sated < 0_turns ) {
                add.sated += action_time_scale::calendar_duration_this_tick();
            }
            break;

        case add_type::PKILLER:
            if( action_time_scale::once_every_this_tick( time_duration::from_turns( 100 - in * 4 ) ) &&
                u.get_painkiller() > 20 - in ) {
                // Tolerance increases!
                u.mod_painkiller( -1 );
            }
            // No further effects if we're doped up.
            if( u.get_painkiller() >= 35 ) {
                add.sated = 0_turns;
                break;
            }

            u.mod_str_bonus( -1, true );
            u.mod_per_bonus( -1, true );
            u.mod_dex_bonus( -1, true );
            if( u.get_pain() < in * 2 ) {
                u.mod_pain( 1 );
            }
            if( one_in( 1200 - 30 * in ) ) {
                u.mod_healthy_mod( -1, -in * 30 );
            }
            if( one_in( 20 ) && dice( 2, 20 ) < in ) {
                u.add_msg_if_player( m_bad, _( "Your hands start shaking… you need some painkillers." ) );
                u.add_morale( MORALE_CRAVING_OPIATE, -40, -10 * in );
                u.add_effect( effect_shakes, 2_minutes + in * 30_seconds );
            } else if( one_in( 20 ) && dice( 2, 30 ) < in ) {
                u.add_msg_if_player( m_bad, _( "You feel anxious. You need your painkillers!" ) );
                u.add_morale( MORALE_CRAVING_OPIATE, -30, -10 * in );
            } else if( one_in( 50 ) && dice( 3, 50 ) < in ) {
                u.vomit();
            }
            break;

        case add_type::SPEED: {
            if( u.has_effect( effect_meth ) ) {
                break;
            }
            u.mod_int_bonus( -1, true );
            u.mod_str_bonus( -1, true );
            if( current_stim > -75 && action_time_scale::once_every_this_tick( 3_minutes ) ) {
                u.mod_stim( -1 );
            }
            if( rng( 0, 150 ) <= in ) {
                u.mod_healthy_mod( -1, -in );
            }
            if( dice( 2, 100 ) < in ) {
                u.add_msg_if_player( m_warning, _( "You feel depressed.  Speed would help." ) );
                u.add_morale( MORALE_CRAVING_SPEED, -25, -50 - 10 * in );
            } else if( one_in( 10 ) && dice( 2, 80 ) < in ) {
                u.add_msg_if_player( m_bad, _( "Your hands start shaking… you need a pick-me-up." ) );
                u.add_morale( MORALE_CRAVING_SPEED, -25, -50 - 10 * in );
                u.add_effect( effect_shakes, in * 2_minutes );
            } else if( one_in( 50 ) && dice( 2, 100 ) < in ) {
                u.add_msg_if_player( m_bad, _( "You stop suddenly, feeling bewildered." ) );
                u.moves -= 300;
            } else if( !u.has_effect( effect_hallu ) && one_in( 20 ) && 8 + dice( 2, 80 ) < in ) {
                u.add_effect( effect_hallu, 6_hours );
            }
        }
        break;

        case add_type::COKE:
        case add_type::CRACK: {
            const std::string &cur_msg = type == add_type::COKE ?
                                         _( "You feel like you need a bump." ) :
                                         _( "You're shivering, you need some crack." );
            const auto morale_type = type == add_type::COKE ? MORALE_CRAVING_COCAINE :
                                     MORALE_CRAVING_CRACK;
            u.mod_int_bonus( -1, true );
            u.mod_per_bonus( -1, true );
            if( one_in( 900 - 30 * in ) ) {
                u.add_msg_if_player( m_warning, cur_msg );
                u.add_morale( morale_type, -20, -50 - 10 * in );
            }
            if( dice( 2, 80 ) <= in ) {
                u.add_msg_if_player( m_warning, cur_msg );
                u.add_morale( morale_type, -20, -50 - 10 * in );
                if( current_stim > -150 ) {
                    u.mod_stim( -3 );
                }
            }
            break;
        }

        case add_type::MUTAGEN:
            if( u.has_trait( trait_MUT_JUNKIE ) ) {
                if( one_in( 600 - 50 * in ) ) {
                    u.add_msg_if_player( m_warning, rng( 0,
                                                         6 ) < in ? _( "You so miss the exquisite rainbow of post-humanity." ) :
                                         _( "Your body is SOO booorrrring.  Just a little sip to liven things up?" ) );
                    u.add_morale( MORALE_CRAVING_MUTAGEN, -20, -200 );
                }
                if( u.focus_pool > 40 && one_in( 800 - 20 * in ) ) {
                    u.focus_pool -= in;
                    u.add_msg_if_player( m_warning,
                                         _( "You daydream what it'd be like if you were *different*.  Different is good." ) );
                }
            } else if( one_in( 500 - 15 * in ) ) {
                u.add_msg_if_player( m_warning, rng( 0, 6 ) < in ? _( "You haven't had any mutagen lately." ) :
                                     _( "You could use some new parts…" ) );
                u.add_morale( MORALE_CRAVING_MUTAGEN, -5, -50 );
            }
            break;
        case add_type::MARLOSS_R:
            marloss_add( u, in, _( "You daydream about luscious pink berries as big as your fist." ) );
            break;
        case add_type::MARLOSS_B:
            marloss_add( u, in, _( "You daydream about nutty cyan seeds as big as your hand." ) );
            break;
        case add_type::MARLOSS_Y:
            marloss_add( u, in, _( "You daydream about succulent, pale golden gel, sweet but light." ) );
            break;
        case add_type::NONE:
        case add_type::NUM_ADD_TYPES:
            break;
    }
}

/**
 * Returns the name of an addiction. It should be able to finish the sentence
 * "Became addicted to ______".
 */
std::string addiction_type_name( const addiction_type_id &cur )
{
    if( cur.is_empty() || !cur.is_valid() ) {
        return "bugs in addiction.cpp";
    }
    return cur.obj().name.translated();
}

std::string addiction_name( const addiction &cur )
{
    return addiction_type_name( cur.type );
}

morale_type addiction_craving( const addiction_type_id &cur )
{
    if( cur.is_empty() || !cur.is_valid() ) {
        return MORALE_NULL;
    }
    return cur.obj().craving_morale;
}

std::string addiction_text( const addiction &cur )
{
    if( cur.type.is_empty() || !cur.type.is_valid() ) {
        return "You crave to report this bug.";
    }
    return cur.type.obj().description.translated();
}

void marloss_add( Character &u, int in, const char *msg )
{
    if( one_in( 800 - 20 * in ) ) {
        u.add_morale( MORALE_CRAVING_MARLOSS, -5, -25 );
        u.add_msg_if_player( m_info, msg );
        if( u.focus_pool > 40 ) {
            u.focus_pool -= in;
        }
    }
}
