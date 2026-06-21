#include "effect.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "action_time_scale.h"
#include "assign.h"
#include "avatar.h"
#include "calendar.h"
#include "color.h"
#include "creature.h"
#include "debug.h"
#include "enums.h"
#include "game.h"
#include "item.h"
#include "json.h"
#include "messages.h"
#include "monster.h"
#include "numeric_interval.h"
#include "output.h"
#include "player.h"
#include "rng.h"
#include "sounds.h"
#include "string_formatter.h"
#include "string_id.h"
#include "type_id.h"
#include "units.h"
#include "units_serde.h"
#include "generic_factory.h"
#include "faction.h"
#include "flag.h"

static const efftype_id effect_bandaged( "bandaged" );
static const efftype_id effect_beartrap( "beartrap" );
static const efftype_id effect_crushed( "crushed" );
static const efftype_id effect_disinfected( "disinfected" );
static const efftype_id effect_downed( "downed" );
static const efftype_id effect_grabbed( "grabbed" );
static const efftype_id effect_heavysnare( "heavysnare" );
static const efftype_id effect_in_pit( "in_pit" );
static const efftype_id effect_lightsnare( "lightsnare" );
static const efftype_id effect_tied( "tied" );
static const efftype_id effect_webbed( "webbed" );
static const efftype_id effect_weed_high( "weed_high" );

static const itype_id itype_holybook_bible( "holybook_bible" );
static const itype_id itype_money_bundle( "money_bundle" );

static const trait_id trait_LACTOSE( "LACTOSE" );
static const trait_id trait_VEGETARIAN( "VEGETARIAN" );

namespace
{
std::map<efftype_id, effect_type> effect_types;
} // namespace

/** @relates string_id */
template<>
const effect_type &string_id<effect_type>::obj() const
{
    const auto iter = effect_types.find( *this );
    if( iter == effect_types.end() ) {
        debugmsg( "invalid effect type id %s", c_str() );
        static const effect_type dummy{};
        return dummy;
    }
    return iter->second;
}

/** @relates string_id */
template<>
bool string_id<effect_type>::is_valid() const
{
    return effect_types.contains( *this );
}

// The only reason this exists is because read_from_json_string cant actually handle being given a std::string
// It wants a JsonIn, which cant really handle a json string array
// Be Very Careful when using this.
template<typename T>
T string_from_json_to_typed_unit( const std::string &source, const std::vector<std::pair<std::string, T>> &units, const JsonObject &jo )
{
    const std::string s = source;
    size_t i = 0;
    const auto error = [&]( const char *const msg ) {
        jo.throw_error( msg, s );
    };
    
    // returns whether we are at the end of the string
    const auto skip_spaces = [&]() {
        while( i < s.size() && s[i] == ' ' ) {
            ++i;
        }
        return i >= s.size();
    };
    const auto get_unit = [&]() {
        if( skip_spaces() ) {
            error( R"("invalid quantity string [%1s]: missing unit.)" );
        }
        for( const auto &pair : units ) {
            const std::string &unit = pair.first;
            if( s.size() >= unit.size() + i && s.compare( i, unit.size(), unit ) == 0 ) {
                i += unit.size();
                return pair.second;
            }
        }
        error( R"("invalid quantity string [%1s]: unknown unit)" );
        // above always throws but lambdas cannot be marked [[noreturn]]
        throw;
    };

    if( skip_spaces() ) {
        error( R"("invalid quantity string [%1s]: empty string)" );
    }
    T result{};
    do {
        int sign_value = +1;
        if( s[i] == '-' ) {
            sign_value = -1;
            ++i;
        } else if( s[i] == '+' ) {
            ++i;
        }
        if( i >= s.size() || !isdigit( s[i] ) ) {
            error( R"("invalid quantity string [%1s]: number expected)" );
        }
        int value = 0;
        for( ; i < s.size() && isdigit( s[i] ); ++i ) {
            value = value * 10 + ( s[i] - '0' );
        }
        result += units::multiply_any_unit( get_unit(), sign_value * value );
    } while( !skip_spaces() );
    return result;
}

// If our output sound category = _LAST we dont actually add the category to the vector.
sounds::sound_t sound_category_from_string( const std::string &st ){
    if ( st == "background" ){
        return sounds::sound_t::background;
    } else if ( st == "weather" ){
        return sounds::sound_t::weather;
    } else if ( st == "music" ) {
        return sounds::sound_t::music;
    } else if ( st == "movement" ) {
        return sounds::sound_t::movement;
    } else if ( st == "speech" ) {
        return sounds::sound_t::speech;
    } else if ( st == "electronic_speech" ) {
        return sounds::sound_t::electronic_speech;
    } else if ( st == "activity" ) {
        return sounds::sound_t::activity;
    } else if ( st == "destructive_activity" ) {
        return sounds::sound_t::destructive_activity;
    } else if ( st == "alarm" ) {
        return sounds::sound_t::alarm;
    } else if ( st == "combat" ) {
        return sounds::sound_t::combat;
    } else if ( st == "alert" ) {
        return sounds::sound_t::alert;
    } else if ( st == "order" ) {
        return sounds::sound_t::order;
    } else {
        return sounds::sound_t::_LAST;
    }  

}

std::vector<efftype_id> find_all_effect_types()
{
    std::vector<efftype_id> all;
    all.reserve( effect_types.size() );
    std::ranges::transform( effect_types, std::back_inserter( all ),
    []( const std::pair<efftype_id, effect_type> &pr ) {
        return pr.first;
    } );
    return all;
}

void weed_msg( Character &who )
{
    const time_duration howhigh = who.get_effect_dur( effect_weed_high );
    ///\EFFECT_INT changes messages when smoking weed
    int smarts = who.get_int();
    if( howhigh > 12_minutes && one_in( 7 ) ) {
        int msg = rng( 0, 5 );
        switch( msg ) {
            case 0:
                // Freakazoid
                who.add_msg_if_player(
                    _( "The scariest thing in the world would be… if all the air in the world turned to WOOD!" ) );
                return;
            case 1:
                // Simpsons
                who.add_msg_if_player(
                    _( "Could Jesus microwave a burrito so hot, that he himself couldn't eat it?" ) );
                who.mod_stored_kcal( -20 );
                return;
            case 2:
                if( smarts > 8 ) {
                    // Timothy Leary
                    who.add_msg_if_player( _( "Science is all metaphor." ) );
                } else if( smarts < 3 ) {
                    // It's Always Sunny in Philadelphia
                    who.add_msg_if_player( _( "Science is a liar sometimes." ) );
                } else {
                    // Durr
                    who.add_msg_if_player( _( "Science is… wait, what was I talking about again?" ) );
                }
                return;
            case 3:
                // Dazed and Confused
                who.add_msg_if_player(
                    _( "Behind every good man there is a woman, and that woman was Martha Washington, man." ) );
                if( one_in( 2 ) ) {
                    who.add_msg_if_player(
                        _( "Every day, George would come home, and she would have a big fat bowl waiting for him when he came in the door, man." ) );
                    if( one_in( 2 ) ) {
                        who.add_msg_if_player( _( "She was a hip, hip, hip lady, man." ) );
                    }
                }
                return;
            case 4:
                if( who.has_amount( itype_money_bundle, 1 ) ) { // Half Baked
                    who.add_msg_if_player( _( "You ever see the back of a twenty dollar bill… on weed?" ) );
                    if( one_in( 2 ) ) {
                        who.add_msg_if_player(
                            _( "Oh, there's some crazy shit, man.  There's a dude in the bushes.  Has he got a gun?  I dunno!" ) );
                        if( one_in( 3 ) ) {
                            who.add_msg_if_player( _( "RED TEAM GO, RED TEAM GO!" ) );
                        }
                    }
                } else if( who.has_amount( itype_holybook_bible, 1 ) ) {
                    who.add_msg_if_player( _( "You have a sudden urge to flip your bible open to Genesis 1:29…" ) );
                } else { // Big Lebowski
                    who.add_msg_if_player( _( "That rug really tied the room together…" ) );
                }
                return;
            case 5:
                who.add_msg_if_player( _( "I used to do drugs…  I still do, but I used to, too." ) );
            default:
                return;
        }
    } else if( howhigh > 10_minutes && one_in( 5 ) ) {
        int msg = rng( 0, 5 );
        switch( msg ) {
            case 0:
                // Bob Marley
                who.add_msg_if_player( _( "The herb reveals you to yourself." ) );
                return;
            case 1:
                // Freakazoid
                who.add_msg_if_player(
                    _( "Okay, like, the scariest thing in the world would be… if like you went to grab something and it wasn't there!" ) );
                return;
            case 2:
                // Simpsons
                who.add_msg_if_player( _( "They call them fingers, but I never see them fing." ) );
                if( smarts > 2 && one_in( 2 ) ) {
                    who.add_msg_if_player( _( "…oh, there they go." ) );
                }
                return;
            case 3:
                // Bill Hicks
                who.add_msg_if_player(
                    _( "You suddenly realize that all matter is merely energy condensed to a slow vibration, and we are all one consciousness experiencing itself subjectively." ) );
                return;
            case 4:
                // Steve Martin
                who.add_msg_if_player( _( "I usually only smoke in the late evening." ) );
                if( one_in( 4 ) ) {
                    who.add_msg_if_player(
                        _( "Oh, occasionally the early evening, but usually the late evening, or the mid-evening." ) );
                }
                if( one_in( 4 ) ) {
                    who.add_msg_if_player( _( "Just the early evening, mid-evening and late evening." ) );
                }
                if( one_in( 4 ) ) {
                    who.add_msg_if_player(
                        _( "Occasionally, early afternoon, early mid-afternoon, or perhaps the late mid-afternoon." ) );
                }
                if( one_in( 4 ) ) {
                    who.add_msg_if_player( _( "Oh, sometimes the early-mid-late-early-morning." ) );
                }
                if( smarts > 2 ) {
                    who.add_msg_if_player( _( "…But never at dusk." ) );
                }
                return;
            case 5:
            default:
                return;
        }
    } else if( howhigh > 5_minutes && one_in( 3 ) ) {
        int msg = rng( 0, 5 );
        switch( msg ) {
            case 0:
                // Cheech and Chong
                who.add_msg_if_player( _( "Dave's not here, man." ) );
                return;
            case 1:
                // Real Life
                who.add_msg_if_player( _( "Man, a cheeseburger sounds SO awesome right now." ) );
                who.mod_stored_kcal( -40 );
                if( who.has_trait( trait_VEGETARIAN ) ) {
                    who.add_msg_if_player( _( "Eh… maybe not." ) );
                } else if( who.has_trait( trait_LACTOSE ) ) {
                    who.add_msg_if_player( _( "I guess, maybe, without the cheese… yeah." ) );
                }
                return;
            case 2:
                // Dazed and Confused
                who.add_msg_if_player( _( "Walkin' down the hall, by myself, smokin' a j with fifty elves." ) );
                return;
            case 3:
                // Half Baked
                who.add_msg_if_player( _( "That weed was the shiz-nittlebam snip-snap-sack." ) );
                return;
            case 4:
                // re-roll
                weed_msg( who );
            case 5:
            default:
                return;
        }
    }
}

static void extract_effect(
    const JsonObject &j,
    std::unordered_map<std::tuple<std::string, bool, std::string, std::string>, double,
    cata::tuple_hash> &data,
    const std::string &mod_type, const std::string &data_key, const std::string &type_key,
    const std::string &arg_key )
{
    double val = 0;
    double reduced_val = 0;
    if( j.has_member( mod_type ) ) {
        JsonArray jsarr = j.get_array( mod_type );
        val = jsarr.get_float( 0 );
        // If a second value exists use it, else reduced_val = val.
        if( jsarr.size() >= 2 ) {
            reduced_val = jsarr.get_float( 1 );
        } else {
            reduced_val = val;
        }
    }
    // Store values if they aren't zero.
    if( val != 0 ) {
        data[std::make_tuple( data_key, false, type_key, arg_key )] = val;
    }
    if( reduced_val != 0 ) {
        data[std::make_tuple( data_key, true, type_key, arg_key )] = reduced_val;
    }
}

bool effect_type::load_mod_data( const JsonObject &jo, const std::string &member )
{
    if( jo.has_object( member ) ) {
        JsonObject j = jo.get_object( member );

        // Stats first
        //                          json field                  type key    arg key
        extract_effect( j, mod_data, "str_mod",          member, "STR",      "min" );
        extract_effect( j, mod_data, "dex_mod",          member, "DEX",      "min" );
        extract_effect( j, mod_data, "per_mod",          member, "PER",      "min" );
        extract_effect( j, mod_data, "int_mod",          member, "INT",      "min" );
        extract_effect( j, mod_data, "speed_mod",        member, "SPEED",    "min" );

        // Then pain
        extract_effect( j, mod_data, "pain_amount",      member, "PAIN",     "amount" );
        extract_effect( j, mod_data, "pain_min",         member, "PAIN",     "min" );
        extract_effect( j, mod_data, "pain_max",         member, "PAIN",     "max" );
        extract_effect( j, mod_data, "pain_max_val",     member, "PAIN",     "max_val" );
        extract_effect( j, mod_data, "pain_chance",      member, "PAIN",     "chance_top" );
        extract_effect( j, mod_data, "pain_chance_bot",  member, "PAIN",     "chance_bot" );
        extract_effect( j, mod_data, "pain_tick",        member, "PAIN",     "tick" );

        // Then hurt
        extract_effect( j, mod_data, "hurt_amount",      member, "HURT",     "amount" );
        extract_effect( j, mod_data, "hurt_min",         member, "HURT",     "min" );
        extract_effect( j, mod_data, "hurt_max",         member, "HURT",     "max" );
        extract_effect( j, mod_data, "hurt_chance",      member, "HURT",     "chance_top" );
        extract_effect( j, mod_data, "hurt_chance_bot",  member, "HURT",     "chance_bot" );
        extract_effect( j, mod_data, "hurt_tick",        member, "HURT",     "tick" );

        // Then sleep
        extract_effect( j, mod_data, "sleep_amount",     member, "SLEEP",    "amount" );
        extract_effect( j, mod_data, "sleep_min",        member, "SLEEP",    "min" );
        extract_effect( j, mod_data, "sleep_max",        member, "SLEEP",    "max" );
        extract_effect( j, mod_data, "sleep_chance",     member, "SLEEP",    "chance_top" );
        extract_effect( j, mod_data, "sleep_chance_bot", member, "SLEEP",    "chance_bot" );
        extract_effect( j, mod_data, "sleep_tick",       member, "SLEEP",    "tick" );

        // Then pkill
        extract_effect( j, mod_data, "pkill_amount",     member, "PKILL",    "amount" );
        extract_effect( j, mod_data, "pkill_min",        member, "PKILL",    "min" );
        extract_effect( j, mod_data, "pkill_max",        member, "PKILL",    "max" );
        extract_effect( j, mod_data, "pkill_max_val",    member, "PKILL",    "max_val" );
        extract_effect( j, mod_data, "pkill_chance",     member, "PKILL",    "chance_top" );
        extract_effect( j, mod_data, "pkill_chance_bot", member, "PKILL",    "chance_bot" );
        extract_effect( j, mod_data, "pkill_tick",       member, "PKILL",    "tick" );

        // Then stim
        extract_effect( j, mod_data, "stim_amount",      member, "STIM",     "amount" );
        extract_effect( j, mod_data, "stim_min",         member, "STIM",     "min" );
        extract_effect( j, mod_data, "stim_max",         member, "STIM",     "max" );
        extract_effect( j, mod_data, "stim_min_val",     member, "STIM",     "min_val" );
        extract_effect( j, mod_data, "stim_max_val",     member, "STIM",     "max_val" );
        extract_effect( j, mod_data, "stim_chance",      member, "STIM",     "chance_top" );
        extract_effect( j, mod_data, "stim_chance_bot",  member, "STIM",     "chance_bot" );
        extract_effect( j, mod_data, "stim_tick",        member, "STIM",     "tick" );

        // Then health
        extract_effect( j, mod_data, "health_amount",    member, "HEALTH",   "amount" );
        extract_effect( j, mod_data, "health_min",       member, "HEALTH",   "min" );
        extract_effect( j, mod_data, "health_max",       member, "HEALTH",   "max" );
        extract_effect( j, mod_data, "health_min_val",   member, "HEALTH",   "min_val" );
        extract_effect( j, mod_data, "health_max_val",   member, "HEALTH",   "max_val" );
        extract_effect( j, mod_data, "health_chance",    member, "HEALTH",   "chance_top" );
        extract_effect( j, mod_data, "health_chance_bot", member, "HEALTH",   "chance_bot" );
        extract_effect( j, mod_data, "health_tick",      member, "HEALTH",   "tick" );

        // Then health mod
        extract_effect( j, mod_data, "h_mod_amount",     member, "H_MOD",    "amount" );
        extract_effect( j, mod_data, "h_mod_min",        member, "H_MOD",    "min" );
        extract_effect( j, mod_data, "h_mod_max",        member, "H_MOD",    "max" );
        extract_effect( j, mod_data, "h_mod_min_val",    member, "H_MOD",    "min_val" );
        extract_effect( j, mod_data, "h_mod_max_val",    member, "H_MOD",    "max_val" );
        extract_effect( j, mod_data, "h_mod_chance",     member, "H_MOD",    "chance_top" );
        extract_effect( j, mod_data, "h_mod_chance_bot", member, "H_MOD",    "chance_bot" );
        extract_effect( j, mod_data, "h_mod_tick",       member, "H_MOD",    "tick" );

        // Then radiation
        extract_effect( j, mod_data, "rad_amount",       member, "RAD",      "amount" );
        extract_effect( j, mod_data, "rad_min",          member, "RAD",      "min" );
        extract_effect( j, mod_data, "rad_max",          member, "RAD",      "max" );
        extract_effect( j, mod_data, "rad_max_val",      member, "RAD",      "max_val" );
        extract_effect( j, mod_data, "rad_chance",       member, "RAD",      "chance_top" );
        extract_effect( j, mod_data, "rad_chance_bot",   member, "RAD",      "chance_bot" );
        extract_effect( j, mod_data, "rad_tick",         member, "RAD",      "tick" );

        // Then hunger
        extract_effect( j, mod_data, "hunger_amount",    member, "HUNGER",   "amount" );
        extract_effect( j, mod_data, "hunger_min",       member, "HUNGER",   "min" );
        extract_effect( j, mod_data, "hunger_max",       member, "HUNGER",   "max" );
        extract_effect( j, mod_data, "hunger_min_val",   member, "HUNGER",   "min_val" );
        extract_effect( j, mod_data, "hunger_max_val",   member, "HUNGER",   "max_val" );
        extract_effect( j, mod_data, "hunger_chance",    member, "HUNGER",   "chance_top" );
        extract_effect( j, mod_data, "hunger_chance_bot", member, "HUNGER",   "chance_bot" );
        extract_effect( j, mod_data, "hunger_tick",      member, "HUNGER",   "tick" );

        // Then thirst
        extract_effect( j, mod_data, "thirst_amount",    member, "THIRST",   "amount" );
        extract_effect( j, mod_data, "thirst_min",       member, "THIRST",   "min" );
        extract_effect( j, mod_data, "thirst_max",       member, "THIRST",   "max" );
        extract_effect( j, mod_data, "thirst_min_val",   member, "THIRST",   "min_val" );
        extract_effect( j, mod_data, "thirst_max_val",   member, "THIRST",   "max_val" );
        extract_effect( j, mod_data, "thirst_chance",    member, "THIRST",   "chance_top" );
        extract_effect( j, mod_data, "thirst_chance_bot", member, "THIRST",   "chance_bot" );
        extract_effect( j, mod_data, "thirst_tick",      member, "THIRST",   "tick" );

        // Then fatigue
        extract_effect( j, mod_data, "fatigue_amount",    member, "FATIGUE",  "amount" );
        extract_effect( j, mod_data, "fatigue_min",       member, "FATIGUE",  "min" );
        extract_effect( j, mod_data, "fatigue_max",       member, "FATIGUE",  "max" );
        extract_effect( j, mod_data, "fatigue_min_val",   member, "FATIGUE",  "min_val" );
        extract_effect( j, mod_data, "fatigue_max_val",   member, "FATIGUE",  "max_val" );
        extract_effect( j, mod_data, "fatigue_chance",    member, "FATIGUE",  "chance_top" );
        extract_effect( j, mod_data, "fatigue_chance_bot", member, "FATIGUE",  "chance_bot" );
        extract_effect( j, mod_data, "fatigue_tick",      member, "FATIGUE",  "tick" );

        // Then sleep debt
        extract_effect( j, mod_data, "sleepdebt_amount",    member, "SLEEPDEBT",   "amount" );
        extract_effect( j, mod_data, "sleepdebt_min",       member, "SLEEPDEBT",   "min" );
        extract_effect( j, mod_data, "sleepdebt_max",       member, "SLEEPDEBT",   "max" );
        extract_effect( j, mod_data, "sleepdebt_min_val",   member, "SLEEPDEBT",   "min_val" );
        extract_effect( j, mod_data, "sleepdebt_max_val",   member, "SLEEPDEBT",   "max_val" );
        extract_effect( j, mod_data, "sleepdebt_chance",    member, "SLEEPDEBT",   "chance_top" );
        extract_effect( j, mod_data, "sleepdebt_chance_bot", member, "SLEEPDEBT",   "chance_bot" );
        extract_effect( j, mod_data, "sleepdebt_tick",      member, "SLEEPDEBT",   "tick" );

        // Then stamina
        extract_effect( j, mod_data, "stamina_amount",    member, "STAMINA",  "amount" );
        extract_effect( j, mod_data, "stamina_min",       member, "STAMINA",  "min" );
        extract_effect( j, mod_data, "stamina_max",       member, "STAMINA",  "max" );
        extract_effect( j, mod_data, "stamina_max_val",   member, "STAMINA",  "max_val" );
        extract_effect( j, mod_data, "stamina_chance",    member, "STAMINA",  "chance_top" );
        extract_effect( j, mod_data, "stamina_chance_bot", member, "STAMINA",  "chance_bot" );
        extract_effect( j, mod_data, "stamina_tick",      member, "STAMINA",  "tick" );

        // Then coughing
        extract_effect( j, mod_data, "cough_chance",     member, "COUGH",    "chance_top" );
        extract_effect( j, mod_data, "cough_chance_bot", member, "COUGH",    "chance_bot" );
        extract_effect( j, mod_data, "cough_tick",       member, "COUGH",    "tick" );

        // Then vomiting
        extract_effect( j, mod_data, "vomit_chance",     member, "VOMIT",    "chance_top" );
        extract_effect( j, mod_data, "vomit_chance_bot", member, "VOMIT",    "chance_bot" );
        extract_effect( j, mod_data, "vomit_tick",       member, "VOMIT",    "tick" );

        // Then healing effects
        extract_effect( j, mod_data, "healing_rate",    member, "HEAL_RATE",  "amount" );
        extract_effect( j, mod_data, "healing_head",    member, "HEAL_HEAD",  "amount" );
        extract_effect( j, mod_data, "healing_torso",   member, "HEAL_TORSO", "amount" );

        // Then morale
        extract_effect( j, mod_data, "morale",          member, "MORALE",     "amount" );

        // creature stats mod
        extract_effect( j, mod_data, "dodge_mod",    member, "DODGE",  "min" );
        extract_effect( j, mod_data, "hit_mod",    member, "HIT",  "min" );
        extract_effect( j, mod_data, "bash_mod",    member, "BASH",  "min" );
        extract_effect( j, mod_data, "cut_mod",    member, "CUT",  "min" );
        extract_effect( j, mod_data, "size_mod",    member, "SIZE",  "min" );

        return true;
    } else {
        return false;
    }
}

bool effect_type::has_flag( const flag_id &flag ) const
{
    return flags.contains( flag );
}

effect_rating effect_type::get_rating() const
{
    return rating;
}

bool effect_type::use_name_ints() const
{
    return name.size() > 1;
}

bool effect_type::use_desc_ints( bool reduced ) const
{
    if( reduced ) {
        return static_cast<size_t>( max_intensity ) <= reduced_desc.size();
    } else {
        return static_cast<size_t>( max_intensity ) <= desc.size();
    }
}

game_message_type effect_type::gain_game_message_type() const
{
    switch( rating ) {
        case e_good:
            return m_good;
        case e_bad:
            return m_bad;
        case e_neutral:
            return m_neutral;
        case e_mixed:
            return m_mixed;
        default:
            // Should never happen
            return m_neutral;
    }
}
game_message_type effect_type::lose_game_message_type() const
{
    switch( rating ) {
        case e_good:
            return m_bad;
        case e_bad:
            return m_good;
        case e_neutral:
            return m_neutral;
        case e_mixed:
            return m_mixed;
        default:
            // Should never happen
            return m_neutral;
    }
}
std::string effect_type::get_looks_like() const
{
    return looks_like;
}
std::string effect_type::get_apply_message() const
{
    return apply_message;
}
std::string effect_type::get_apply_memorial_log() const
{
    return apply_memorial_log;
}
std::string effect_type::get_remove_message() const
{
    return remove_message;
}
std::string effect_type::get_remove_memorial_log() const
{
    return remove_memorial_log;
}

std::string effect_type::get_blood_analysis_description() const
{
    return blood_analysis_description;
}

bool effect_type::get_main_parts() const
{
    return main_parts_only;
}
time_duration effect_type::get_max_duration() const
{
    return max_duration;
}
bool effect_type::is_permanent() const
{
    return permanent;
}
bool effect_type::is_show_in_info() const
{
    return show_in_info;
}
time_duration effect_type::get_int_dur_factor() const
{
    return int_dur_factor;
}
morale_type effect_type::get_morale_type() const
{
    return morale;
}
bool effect_type::load_miss_msgs( const JsonObject &jo, const std::string &member )
{
    if( jo.has_array( member ) ) {
        for( JsonArray inner : jo.get_array( member ) ) {
            miss_msgs.emplace_back( inner.get_string( 0 ), inner.get_int( 1 ) );
        }
        return true;
    }
    return false;
}
bool effect_type::load_decay_msgs( const JsonObject &jo, const std::string &member )
{
    if( jo.has_array( member ) ) {
        for( JsonArray inner : jo.get_array( member ) ) {
            std::string msg = inner.get_string( 0 );
            std::string r = inner.get_string( 1 );
            game_message_type rate = m_neutral;
            if( r == "good" ) {
                rate = m_good;
            } else if( r == "neutral" ) {
                rate = m_neutral;
            } else if( r == "bad" ) {
                rate = m_bad;
            } else if( r == "mixed" ) {
                rate = m_mixed;
            } else {
                rate = m_neutral;
            }
            decay_msgs.emplace_back( msg, rate );
        }
        return true;
    }
    return false;
}
void effect_type::check_consistency()
{
    for( const auto &pr : effect_types ) {
        const effect_type &et = pr.second;
        if( et.get_morale_type() && !et.get_morale_type().is_valid() ) {
            debugmsg( "Effect type %s has invalid morale type %s",
                      et.id.str(), et.get_morale_type().str() );
        }
    }
}

effect effect::null_effect;

bool effect::is_null() const
{
    return !eff_type;
}

std::string effect::disp_name() const
{
    if( eff_type->name.empty() ) {
        debugmsg( "No names for effect type, ID: %s", eff_type->id.c_str() );
        return "";
    }

    // End result should look like "name (l. arm)" or "name [intensity] (l. arm)"
    std::string ret;
    if( eff_type->use_name_ints() ) {
        const translation &d_name = eff_type->name[ std::min<size_t>( intensity,
                                                      eff_type->name.size() ) - 1 ];
        if( d_name.empty() ) {
            return std::string();
        }
        ret += d_name.translated();
    } else {
        if( eff_type->name[0].empty() ) {
            return std::string();
        }
        ret += eff_type->name[0].translated();
        if( intensity > 1 ) {
            if( eff_type->id == effect_bandaged || eff_type->id == effect_disinfected ) {
                ret += string_format( " [%s]", texitify_healing_power( intensity ) );
            } else {
                ret += string_format( " [%d]", intensity );
            }
        }
    }
    if( bp ) {
        ret += string_format( " (%s)", body_part_name( bp ) );
    }

    return ret;
}

// Used in disp_desc()
struct desc_freq {
    double chance;
    int val;
    std::string pos_string;
    std::string neg_string;

    desc_freq( double c, int v, const std::string &pos, const std::string &neg ) : chance( c ),
        val( v ), pos_string( pos ), neg_string( neg ) {}
};

std::string effect::disp_desc( bool reduced ) const
{
    std::string ret;
    // First print stat changes, adding + if value is positive
    int tmp = get_avg_mod( "STR", reduced );
    if( tmp > 0 ) {
        ret += string_format( _( "Strength <color_white>+%d</color>;  " ), tmp );
    } else if( tmp < 0 ) {
        ret += string_format( _( "Strength <color_white>%d</color>;  " ), tmp );
    }
    tmp = get_avg_mod( "DEX", reduced );
    if( tmp > 0 ) {
        ret += string_format( _( "Dexterity <color_white>+%d</color>;  " ), tmp );
    } else if( tmp < 0 ) {
        ret += string_format( _( "Dexterity <color_white>%d</color>;  " ), tmp );
    }
    tmp = get_avg_mod( "PER", reduced );
    if( tmp > 0 ) {
        ret += string_format( _( "Perception <color_white>+%d</color>;  " ), tmp );
    } else if( tmp < 0 ) {
        ret += string_format( _( "Perception <color_white>%d</color>;  " ), tmp );
    }
    tmp = get_avg_mod( "INT", reduced );
    if( tmp > 0 ) {
        ret += string_format( _( "Intelligence <color_white>+%d</color>;  " ), tmp );
    } else if( tmp < 0 ) {
        ret += string_format( _( "Intelligence <color_white>%d</color>;  " ), tmp );
    }
    tmp = get_avg_mod( "SPEED", reduced );
    if( tmp > 0 ) {
        ret += string_format( _( "Speed <color_white>+%d</color>;  " ), tmp );
    } else if( tmp < 0 ) {
        ret += string_format( _( "Speed <color_white>%d</color>;  " ), tmp );
    }
    // Newline if necessary
    if( !ret.empty() && ret.back() != '\n' ) {
        ret += "\n";
    }

    // Then print pain/damage/coughing/vomiting, we don't display pkill, health, or radiation
    std::vector<std::string> constant;
    std::vector<std::string> frequent;
    std::vector<std::string> uncommon;
    std::vector<std::string> rare;
    std::vector<desc_freq> values;
    // Add various desc_freq structs to values. If more effects wish to be placed in the descriptions this is the
    // place to add them.
    int val = 0;
    val = get_avg_mod( "PAIN", reduced );
    values.emplace_back( get_percentage( "PAIN", val, reduced ), val, _( "pain" ),
                         _( "pain" ) );
    val = get_avg_mod( "HURT", reduced );
    values.emplace_back( get_percentage( "HURT", val, reduced ), val, _( "damage" ),
                         _( "damage" ) );
    val = get_avg_mod( "STAMINA", reduced );
    values.emplace_back( get_percentage( "STAMINA", val, reduced ), val,
                         _( "stamina recovery" ), _( "fatigue" ) );
    val = get_avg_mod( "THIRST", reduced );
    values.emplace_back( get_percentage( "THIRST", val, reduced ), val, _( "thirst" ),
                         _( "quench" ) );
    val = get_avg_mod( "HUNGER", reduced );
    values.emplace_back( get_percentage( "HUNGER", val, reduced ), val, _( "hunger" ),
                         _( "sate" ) );
    val = get_avg_mod( "FATIGUE", reduced );
    values.emplace_back( get_percentage( "FATIGUE", val, reduced ), val, _( "sleepiness" ),
                         _( "rest" ) );
    val = get_avg_mod( "COUGH", reduced );
    values.emplace_back( get_percentage( "COUGH", val, reduced ), val, _( "coughing" ),
                         _( "coughing" ) );
    val = get_avg_mod( "VOMIT", reduced );
    values.emplace_back( get_percentage( "VOMIT", val, reduced ), val, _( "vomiting" ),
                         _( "vomiting" ) );
    val = get_avg_mod( "SLEEP", reduced );
    values.emplace_back( get_percentage( "SLEEP", val, reduced ), val, _( "blackouts" ),
                         _( "blackouts" ) );

    for( auto &i : values ) {
        if( i.val > 0 ) {
            // +50% chance, every other step
            if( i.chance >= 50.0 ) {
                constant.push_back( i.pos_string );
                // +1% chance, every 100 steps
            } else if( i.chance >= 1.0 ) {
                frequent.push_back( i.pos_string );
                // +.4% chance, every 250 steps
            } else if( i.chance >= .4 ) {
                uncommon.push_back( i.pos_string );
                // <.4% chance
            } else if( i.chance > 0 ) {
                rare.push_back( i.pos_string );
            }
        } else if( i.val < 0 ) {
            // +50% chance, every other step
            if( i.chance >= 50.0 ) {
                constant.push_back( i.neg_string );
                // +1% chance, every 100 steps
            } else if( i.chance >= 1.0 ) {
                frequent.push_back( i.neg_string );
                // +.4% chance, every 250 steps
            } else if( i.chance >= .4 ) {
                uncommon.push_back( i.neg_string );
                // <.4% chance
            } else if( i.chance > 0 ) {
                rare.push_back( i.neg_string );
            }
        }
    }
    if( !constant.empty() ) {
        ret += _( "Const: " ) + enumerate_as_string( constant ) + " ";
    }
    if( !frequent.empty() ) {
        ret += _( "Freq: " ) + enumerate_as_string( frequent ) + " ";
    }
    if( !uncommon.empty() ) {
        ret += _( "Unfreq: " ) + enumerate_as_string( uncommon ) + " ";
    }
    if( !rare.empty() ) {
        ret += _( "Rare: " ) + enumerate_as_string( rare ); // No space needed at the end
    }

    // Newline if necessary
    if( !ret.empty() && ret.back() != '\n' ) {
        ret += "\n";
    }

    std::string tmp_str;
    if( eff_type->use_desc_ints( reduced ) ) {
        if( reduced ) {
            tmp_str = eff_type->reduced_desc[intensity - 1];
        } else {
            tmp_str = eff_type->desc[intensity - 1];
        }
    } else {
        if( reduced ) {
            tmp_str = eff_type->reduced_desc[0];
        } else {
            tmp_str = eff_type->desc[0];
        }
    }
    // Then print the effect description
    if( use_part_descs() ) {
        ret += string_format( _( tmp_str ), body_part_name( bp ) );
    } else {
        if( !tmp_str.empty() ) {
            ret += _( tmp_str );
        }
    }

    return ret;
}

std::string effect::disp_short_desc( bool reduced ) const
{
    if( eff_type->use_desc_ints( reduced ) ) {
        if( reduced ) {
            return eff_type->reduced_desc[intensity - 1];
        } else {
            return eff_type->desc[intensity - 1];
        }
    } else {
        if( reduced ) {
            return eff_type->reduced_desc[0];
        } else {
            return eff_type->desc[0];
        }
    }
}

bool effect::decay( const time_point &time, const bool player )
{
    // Decay intensity if supposed to do so
    // TODO: Remove effects that would decay to 0 intensity?
    if( intensity > 1 && eff_type->int_decay_tick != 0 &&
        action_time_scale::calendar_ticks_crossed_this_tick( time,
                time_duration::from_turns( eff_type->int_decay_tick ) ) > 0 &&
        get_max_duration() > get_duration() ) {
        set_intensity( intensity + eff_type->int_decay_step, player );

    }

    if( duration <= 0_turns ) {
        return true;
    } else if( !is_permanent() ) {
        mod_duration( -action_time_scale::calendar_duration_this_tick(), player );
    }

    return false;
}

bool effect::use_part_descs() const
{
    return eff_type->part_descs;
}

time_duration effect::get_duration() const
{
    return duration;
}
time_duration effect::get_max_duration() const
{
    return eff_type->get_max_duration();
}
void effect::set_duration( const time_duration &dur, bool alert )
{
    duration = dur;
    // Cap to max_duration if it exists
    if( eff_type->max_duration > 0_turns && duration > eff_type->max_duration ) {
        duration = eff_type->max_duration;
    }

    // Force intensity if it is duration based
    if( eff_type->int_dur_factor != 0_turns ) {
        // + 1 here so that the lowest is intensity 1, not 0
        set_intensity( duration / eff_type->int_dur_factor + 1, alert );
    }

    add_msg( m_debug, "ID: %s, Duration %d", get_id().c_str(), to_turns<int>( duration ) );
}
void effect::mod_duration( const time_duration &dur, bool alert )
{
    set_duration( duration + dur, alert );
}
void effect::mult_duration( double dur, bool alert )
{
    set_duration( duration * dur, alert );
}

time_point effect::get_start_time() const
{
    return start_time;
}

const bodypart_str_id &effect::get_bp() const
{
    return bp;
}

bool effect::is_permanent() const
{
    return permanent || eff_type->is_permanent();
}
void effect::set_permanent()
{
    permanent = true;
}

int effect::get_intensity() const
{
    return intensity;
}
int effect::get_max_intensity() const
{
    return eff_type->max_intensity;
}

int effect::set_intensity( int val, bool alert )
{
    if( intensity < 1 ) {
        // Fix bad intensity
        add_msg( m_debug, "Bad intensity, ID: %s", get_id().c_str() );
        intensity = 1;
    }

    val = std::max( std::min( val, eff_type->max_intensity ), 1 );
    if( val == intensity ) {
        // Nothing to change
        return intensity;
    }

    if( alert && val < intensity && val - 1 < static_cast<int>( eff_type->decay_msgs.size() ) ) {
        add_msg( eff_type->decay_msgs[ val - 1 ].second,
                 eff_type->decay_msgs[ val - 1 ].first.c_str() );
    }

    int old_intensity = intensity;
    intensity = val;
    if( old_intensity != intensity ) {
        add_msg( m_debug, "%s intensity %d->%d", get_id().c_str(), old_intensity, intensity );
    }

    return intensity;
}

int effect::mod_intensity( int mod, bool alert )
{
    return set_intensity( intensity + mod, alert );
}

const std::vector<trait_id> &effect::get_resist_traits() const
{
    return eff_type->resist_traits;
}
const std::vector<efftype_id> &effect::get_resist_effects() const
{
    return eff_type->resist_effects;
}
const std::vector<efftype_id> &effect::get_removes_effects() const
{
    return eff_type->removes_effects;
}
std::vector<efftype_id> effect::get_blocks_effects() const
{
    std::vector<efftype_id> ret = eff_type->removes_effects;
    ret.insert( ret.end(), eff_type->blocks_effects.begin(), eff_type->blocks_effects.end() );
    return ret;
}

int effect::get_mod( const std::string &arg, bool reduced ) const
{
    auto &mod_data = eff_type->mod_data;
    double min = 0;
    double max = 0;
    // Get the minimum total
    auto found = mod_data.find( std::make_tuple( "base_mods", reduced, arg, "min" ) );
    if( found != mod_data.end() ) {
        min += found->second;
    }
    found = mod_data.find( std::make_tuple( "scaling_mods", reduced, arg, "min" ) );
    if( found != mod_data.end() ) {
        min += found->second * ( intensity - 1 );
    }
    // Get the maximum total
    found = mod_data.find( std::make_tuple( "base_mods", reduced, arg, "max" ) );
    if( found != mod_data.end() ) {
        max += found->second;
    }
    found = mod_data.find( std::make_tuple( "scaling_mods", reduced, arg, "max" ) );
    if( found != mod_data.end() ) {
        max += found->second * ( intensity - 1 );
    }
    if( static_cast<int>( max ) != 0 ) {
        // Return a random value between [min, max]
        return rng( min, max );
    } else {
        // Else return the minimum value
        return min;
    }
}

int effect::get_avg_mod( const std::string &arg, bool reduced ) const
{
    auto &mod_data = eff_type->mod_data;
    double min = 0;
    double max = 0;
    // Get the minimum total
    auto found = mod_data.find( std::make_tuple( "base_mods", reduced, arg, "min" ) );
    if( found != mod_data.end() ) {
        min += found->second;
    }
    found = mod_data.find( std::make_tuple( "scaling_mods", reduced, arg, "min" ) );
    if( found != mod_data.end() ) {
        min += found->second * ( intensity - 1 );
    }
    // Get the maximum total
    found = mod_data.find( std::make_tuple( "base_mods", reduced, arg, "max" ) );
    if( found != mod_data.end() ) {
        max += found->second;
    }
    found = mod_data.find( std::make_tuple( "scaling_mods", reduced, arg, "max" ) );
    if( found != mod_data.end() ) {
        max += found->second * ( intensity - 1 );
    }
    if( static_cast<int>( max ) != 0 ) {
        // Return an average of min and max
        return static_cast<int>( ( min + max ) / 2 );
    } else {
        // Else return the minimum value
        return min;
    }
}

int effect::get_amount( const std::string &arg, bool reduced ) const
{
    int intensity_capped = eff_type->max_effective_intensity > 0 ? std::min(
                               eff_type->max_effective_intensity, intensity ) : intensity;
    auto &mod_data = eff_type->mod_data;
    double ret = 0;
    auto found = mod_data.find( std::make_tuple( "base_mods", reduced, arg, "amount" ) );
    if( found != mod_data.end() ) {
        ret += found->second;
    }
    found = mod_data.find( std::make_tuple( "scaling_mods", reduced, arg, "amount" ) );
    if( found != mod_data.end() ) {
        ret += found->second * ( intensity_capped - 1 );
    }
    return static_cast<int>( ret );
}

int effect::get_min_val( const std::string &arg, bool reduced ) const
{
    auto &mod_data = eff_type->mod_data;
    double ret = 0;
    auto found = mod_data.find( std::make_tuple( "base_mods", reduced, arg, "min_val" ) );
    if( found != mod_data.end() ) {
        ret += found->second;
    }
    found = mod_data.find( std::make_tuple( "scaling_mods", reduced, arg, "min_val" ) );
    if( found != mod_data.end() ) {
        ret += found->second * ( intensity - 1 );
    }
    return static_cast<int>( ret );
}

int effect::get_max_val( const std::string &arg, bool reduced ) const
{
    auto &mod_data = eff_type->mod_data;
    double ret = 0;
    auto found = mod_data.find( std::make_tuple( "base_mods", reduced, arg, "max_val" ) );
    if( found != mod_data.end() ) {
        ret += found->second;
    }
    found = mod_data.find( std::make_tuple( "scaling_mods", reduced, arg, "max_val" ) );
    if( found != mod_data.end() ) {
        ret += found->second * ( intensity - 1 );
    }
    return static_cast<int>( ret );
}

bool effect::get_sizing( const std::string &arg ) const
{
    if( arg == "PAIN" ) {
        return eff_type->pain_sizing;
    } else if( arg == "HURT" ) {
        return eff_type->hurt_sizing;
    }
    return false;
}

double effect::get_percentage( const std::string &arg, int val, bool reduced ) const
{
    auto &mod_data = eff_type->mod_data;
    auto found_top_base = mod_data.find( std::make_tuple( "base_mods", reduced, arg, "chance_top" ) );
    auto found_top_scale = mod_data.find( std::make_tuple( "scaling_mods", reduced, arg,
                                          "chance_top" ) );
    // Convert to int or 0
    int top_base = 0;
    int top_scale = 0;
    if( found_top_base != mod_data.end() ) {
        top_base = found_top_base->second;
    }
    if( found_top_scale != mod_data.end() ) {
        top_scale = found_top_scale->second * ( intensity - 1 );
    }
    // Check chances if value is 0 (so we can check valueless effects like vomiting)
    // Else a nonzero value overrides a 0 chance for default purposes
    if( val == 0 ) {
        // If both top values <= 0 then it should never trigger
        if( top_base <= 0 && top_scale <= 0 ) {
            return 0;
        }
        // It will also never trigger if top_base + top_scale <= 0
        if( top_base + top_scale <= 0 ) {
            return 0;
        }
    }

    // We only need to calculate these if we haven't already returned
    int bot_base = 0;
    int bot_scale = 0;
    int tick = 0;
    auto found_bot_base = mod_data.find( std::make_tuple( "base_mods", reduced, arg, "chance_bot" ) );
    auto found_bot_scale = mod_data.find( std::make_tuple( "scaling_mods", reduced, arg,
                                          "chance_bot" ) );
    auto found_tick_base = mod_data.find( std::make_tuple( "base_mods", reduced, arg, "tick" ) );
    auto found_tick_scale = mod_data.find( std::make_tuple( "scaling_mods", reduced, arg, "tick" ) );
    if( found_bot_base != mod_data.end() ) {
        bot_base = found_bot_base->second;
    }
    if( found_bot_scale != mod_data.end() ) {
        bot_scale = found_bot_scale->second * ( intensity - 1 );
    }
    if( found_tick_base != mod_data.end() ) {
        tick += found_tick_base->second;
    }
    if( found_tick_scale != mod_data.end() ) {
        tick += found_tick_scale->second * ( intensity - 1 );
    }
    // Tick is the exception where tick = 0 means tick = 1
    if( tick == 0 ) {
        tick = 1;
    }

    double ret = 0;
    // If both bot values are zero the formula is one_in(top), else the formula is x_in_y(top, bot)
    if( bot_base != 0 && bot_scale != 0 ) {
        if( bot_base + bot_scale == 0 ) {
            // Special crash avoidance case, in most effect fields 0 = "nothing happens"
            // so assume false here for consistency
            ret = 0;
        } else {
            // Cast to double here to allow for partial percentages
            ret = 100 * static_cast<double>( top_base + top_scale ) / static_cast<double>
                  ( bot_base + bot_scale );
        }
    } else {
        // Cast to double here to allow for partial percentages
        ret = 100 / static_cast<double>( top_base + top_scale );
    }
    // Divide by ticks between rolls
    if( tick > 1 ) {
        ret = ret / tick;
    }
    return ret;
}

bool effect::activated( const time_point &when, const std::string &arg, int val, bool reduced,
                        double mod ) const
{
    auto &mod_data = eff_type->mod_data;
    auto found_top_base = mod_data.find( std::make_tuple( "base_mods", reduced, arg, "chance_top" ) );
    auto found_top_scale = mod_data.find( std::make_tuple( "scaling_mods", reduced, arg,
                                          "chance_top" ) );
    // Convert to int or 0
    int top_base = 0;
    int top_scale = 0;
    if( found_top_base != mod_data.end() ) {
        top_base = found_top_base->second;
    }
    if( found_top_scale != mod_data.end() ) {
        top_scale = found_top_scale->second * ( intensity - 1 );
    }
    // Check chances if value is 0 (so we can check valueless effects like vomiting)
    // Else a nonzero value overrides a 0 chance for default purposes
    if( val == 0 ) {
        // If both top values <= 0 then it should never trigger
        if( top_base <= 0 && top_scale <= 0 ) {
            return false;
        }
        // It will also never trigger if top_base + top_scale <= 0
        if( top_base + top_scale <= 0 ) {
            return false;
        }
    }

    // We only need to calculate these if we haven't already returned
    int bot_base = 0;
    int bot_scale = 0;
    int tick = 0;
    auto found_bot_base = mod_data.find( std::make_tuple( "base_mods", reduced, arg, "chance_bot" ) );
    auto found_bot_scale = mod_data.find( std::make_tuple( "scaling_mods", reduced, arg,
                                          "chance_bot" ) );
    auto found_tick_base = mod_data.find( std::make_tuple( "base_mods", reduced, arg, "tick" ) );
    auto found_tick_scale = mod_data.find( std::make_tuple( "scaling_mods", reduced, arg, "tick" ) );
    if( found_bot_base != mod_data.end() ) {
        bot_base = found_bot_base->second;
    }
    if( found_bot_scale != mod_data.end() ) {
        bot_scale = found_bot_scale->second * ( intensity - 1 );
    }
    if( found_tick_base != mod_data.end() ) {
        tick += found_tick_base->second;
    }
    if( found_tick_scale != mod_data.end() ) {
        tick += found_tick_scale->second * ( intensity - 1 );
    }
    // Tick is the exception where tick = 0 means tick = 1
    if( tick == 0 ) {
        tick = 1;
    }

    // Check if tick allows for triggering. If both bot values are zero the formula is
    // x_in_y(1, top) i.e. one_in(top), else the formula is x_in_y(top, bot),
    // mod multiplies the overall percentage chances

    // has to be an && here to avoid undefined behavior of turn % 0
    if( tick > 0 &&
        ( when - calendar::turn_zero ) % time_duration::from_turns( tick ) == 0_turns ) {
        if( bot_base != 0 && bot_scale != 0 ) {
            if( bot_base + bot_scale == 0 ) {
                // Special crash avoidance case, in most effect fields 0 = "nothing happens"
                // so assume false here for consistency
                return false;
            } else {
                return x_in_y( ( top_base + top_scale ) * mod, bot_base + bot_scale );
            }
        } else {
            return x_in_y( mod, top_base + top_scale );
        }
    }
    return false;
}

double effect::get_addict_mod( const std::string &arg, int addict_level ) const
{
    // TODO: convert this to JSON id's and values once we have JSON'ed addictions
    if( arg == "PKILL" ) {
        if( eff_type->pkill_addict_reduces ) {
            return 1.0 / std::max( static_cast<double>( addict_level ) * 2.0, 1.0 );
        } else {
            return 1.0;
        }
    } else {
        return 1.0;
    }
}

bool effect::get_harmful_cough() const
{
    return eff_type->harmful_cough;
}
int effect::get_dur_add_perc() const
{
    return eff_type->dur_add_perc;
}
time_duration effect::get_int_dur_factor() const
{
    return eff_type->get_int_dur_factor();
}
int effect::get_int_add_val() const
{
    return eff_type->int_add_val;
}

std::vector<std::pair<std::string, int>> effect::get_miss_msgs() const
{
    return eff_type->miss_msgs;
}
std::string effect::get_speed_name() const
{
    // USes the speed_mod_name if one exists, else defaults to the first entry in "name".
    // But make sure the name for this intensity actually exists!
    if( !eff_type->speed_mod_name.empty() ) {
        return _( eff_type->speed_mod_name );
    } else if( eff_type->use_name_ints() ) {
        return eff_type->name[ std::min<size_t>( intensity, eff_type->name.size() ) - 1 ].translated();
    } else if( !eff_type->name.empty() ) {
        return eff_type->name[0].translated();
    } else {
        return "";
    }
}

bool effect::impairs_movement() const
{
    return eff_type->impairs_movement;
}

const effect_type *effect::get_effect_type() const
{
    return eff_type;
}

// This contains all the effects checked in move_effects
// It's here and not in json because it is hardcoded anyway
static const std::unordered_set<efftype_id> hardcoded_movement_impairing = {{
        effect_beartrap,
        effect_crushed,
        effect_downed,
        effect_grabbed,
        effect_heavysnare,
        effect_in_pit,
        effect_lightsnare,
        effect_tied,
        effect_webbed,
    }
};

void load_effect_type( const JsonObject &jo )
{
    effect_type new_etype;
    new_etype.id = efftype_id( jo.get_string( "id" ) );

    if( jo.has_member( "name" ) ) {
        for( const JsonValue entry : jo.get_array( "name" ) ) {
            translation name;
            if( !entry.read( name ) ) {
                entry.throw_error( "Error reading effect names" );
            }
            new_etype.name.emplace_back( name );
        }
    } else {
        new_etype.name.emplace_back();
    }
    new_etype.speed_mod_name = jo.get_string( "speed_name", "" );

    if( jo.has_member( "desc" ) ) {
        for( const std::string line : jo.get_array( "desc" ) ) {
            new_etype.desc.push_back( line );
        }
    } else {
        new_etype.desc.emplace_back( "" );
    }
    if( jo.has_member( "reduced_desc" ) ) {
        for( const std::string line : jo.get_array( "reduced_desc" ) ) {
            new_etype.reduced_desc.push_back( line );
        }
    } else {
        new_etype.reduced_desc = new_etype.desc;
    }
    new_etype.looks_like = jo.get_string( "looks_like", "" );

    new_etype.part_descs = jo.get_bool( "part_descs", false );

    if( jo.has_member( "rating" ) ) {
        std::string r = jo.get_string( "rating" );
        if( r == "good" ) {
            new_etype.rating = e_good;
        } else if( r == "neutral" ) {
            new_etype.rating = e_neutral;
        } else if( r == "bad" ) {
            new_etype.rating = e_bad;
        } else if( r == "mixed" ) {
            new_etype.rating = e_mixed;
        } else {
            new_etype.rating = e_neutral;
        }
    } else {
        new_etype.rating = e_neutral;
    }
    new_etype.apply_message = jo.get_string( "apply_message", "" );
    new_etype.remove_message = jo.get_string( "remove_message", "" );
    new_etype.apply_memorial_log = jo.get_string( "apply_memorial_log", "" );
    new_etype.remove_memorial_log = jo.get_string( "remove_memorial_log", "" );

    new_etype.blood_analysis_description = jo.get_string( "blood_analysis_description", "" );

    for( auto &&f : jo.get_string_array( "resist_traits" ) ) { // *NOPAD*
        new_etype.resist_traits.emplace_back( f );
    }
    for( auto &&f : jo.get_string_array( "resist_effects" ) ) { // *NOPAD*
        new_etype.resist_effects.emplace_back( f );
    }
    for( auto &&f : jo.get_string_array( "removes_effects" ) ) { // *NOPAD*
        new_etype.removes_effects.emplace_back( f );
    }
    for( auto &&f : jo.get_string_array( "blocks_effects" ) ) { // *NOPAD*
        new_etype.blocks_effects.emplace_back( f );
    }

    if( jo.has_string( "max_duration" ) ) {
        new_etype.max_duration = read_from_json_string<time_duration>( *jo.get_raw( "max_duration" ),
                                 time_duration::units );
    } else {
        new_etype.max_duration = time_duration::from_turns( jo.get_int( "max_duration", 0 ) );
    }

    if( jo.has_string( "int_dur_factor" ) ) {
        new_etype.int_dur_factor = read_from_json_string<time_duration>( *jo.get_raw( "int_dur_factor" ),
                                   time_duration::units );
    } else {
        new_etype.int_dur_factor = time_duration::from_turns( jo.get_int( "int_dur_factor", 0 ) );
    }

    new_etype.max_intensity = jo.get_int( "max_intensity", 1 );
    new_etype.dur_add_perc = jo.get_int( "dur_add_perc", 100 );
    new_etype.int_add_val = jo.get_int( "int_add_val", 0 );
    new_etype.int_decay_step = jo.get_int( "int_decay_step", -1 );
    new_etype.int_decay_tick = jo.get_int( "int_decay_tick", 0 );

    new_etype.load_miss_msgs( jo, "miss_messages" );
    new_etype.load_decay_msgs( jo, "decay_messages" );

    new_etype.main_parts_only = jo.get_bool( "main_parts_only", false );
    new_etype.show_in_info = jo.get_bool( "show_in_info", false );
    new_etype.permanent = jo.get_bool( "permanent", false );
    new_etype.pkill_addict_reduces = jo.get_bool( "pkill_addict_reduces", false );

    new_etype.pain_sizing = jo.get_bool( "pain_sizing", false );
    new_etype.hurt_sizing = jo.get_bool( "hurt_sizing", false );
    new_etype.harmful_cough = jo.get_bool( "harmful_cough", false );

    new_etype.max_effective_intensity = jo.get_int( "max_effective_intensity", 0 );

    new_etype.load_mod_data( jo, "base_mods" );
    new_etype.load_mod_data( jo, "scaling_mods" );

    new_etype.impairs_movement = hardcoded_movement_impairing.contains( new_etype.id );

    new_etype.flags = jo.get_tags<flag_id>( "flags" );

    assign( jo, "morale", new_etype.morale );

    const auto morale_effect = std::ranges::find_if( new_etype.mod_data,
    []( decltype( *new_etype.mod_data.begin() ) & pr ) {
        return std::get<2>( pr.first ) == "MORALE";
    } );
    bool has_morale_effect = morale_effect != new_etype.mod_data.end();
    if( new_etype.morale && !has_morale_effect ) {
        jo.throw_error( "Morale type set, but no MORALE base/scaling effect", "morale" );
    } else if( !new_etype.morale && has_morale_effect ) {
        jo.throw_error( "MORALE base/scaling effect present, but no morale type set",
                        std::get<0>( morale_effect->first ) );
    }

    // TODO: Implement handling of reduced morale, remove this
    static const std::vector<std::string> mod_types = {{
            {"base_mods"}, {"scaling_mods"}
        }
    };
    if( has_morale_effect ) {
        for( const std::string &cur_mod : mod_types ) {
            auto reduced_tuple = std::make_tuple( cur_mod, true, "MORALE", "amount" );
            auto reduced = new_etype.mod_data.find( reduced_tuple );
            auto non_reduced_tuple = std::make_tuple( cur_mod, false, "MORALE", "amount" );
            auto non_reduced = new_etype.mod_data.find( non_reduced_tuple );
            bool has_reduced = reduced != new_etype.mod_data.end();
            bool has_non_reduced = non_reduced != new_etype.mod_data.end();
            if( ( has_reduced && has_non_reduced && reduced->second != non_reduced->second )
                || has_reduced != has_non_reduced ) {
                jo.throw_error( "MORALE doesn't support different amounts for resisted effects yet",
                                cur_mod );
            }
        }
    }

    if( jo.has_array( "effects_on_remove" ) ) {
        JsonArray jarr = jo.get_array( "effects_on_remove" );
        for( JsonObject jo_decay : jarr ) {
            new_etype.effects_on_remove.emplace_back();
            new_etype.effects_on_remove.back().load_decay( jo_decay );
        }
    }
    // Load up our potential effect caused sounds.
    if ( jo.has_array( "caused_sounds" ) ) {
        JsonArray jarr = jo.get_array( "caused_sounds" );
        if ( !jarr.empty() ){ 
            for ( const JsonObject &csound : jarr ) {
                caused_sound candidate;
                candidate.load( csound );
                // did our load actually produce a valid caused sound entry?
                if ( candidate ){
                    new_etype.caused_sounds.push_back( candidate );
                }
            }
        }
        if ( !new_etype.caused_sounds.empty() ){
            new_etype.flags.insert( flag_id( "EFFECT_CREATES_SOUND ") );
        }
    }
    // Load up our potential incoming sound modifiers.
    if ( jo.has_array( "incoming_sound_modifiers" ) ) {
        JsonArray jarr = jo.get_array( "incoming_sound_modifiers" );
        if ( !jarr.empty() ){ 
            for ( const JsonObject &inmod : jarr ) {
                heard_sound_modifiers candidate;
                candidate.load( inmod );
                // did our load actually produce a valid incoming sound modifiers entry?
                if ( candidate ){
                    new_etype.in_sound_modifiers.push_back( candidate );
                }
            }
        }
        if ( !new_etype.in_sound_modifiers.empty() ){
            new_etype.flags.insert( flag_id( "EFFECT_MODIFIES_INCOMING_SOUND" ) );
        }
    }
    // Load up our potential outgoing sound modifiers.
    if ( jo.has_array( "outgoing_sound_modifiers" ) ) {
        JsonArray jarr = jo.get_array( "outgoing_sound_modifiers" );
        if ( !jarr.empty() ){ 
            for ( const JsonObject &outmod : jarr ) {
                outgoing_sound_modifiers candidate;
                candidate.load( outmod );
                // did our load actually produce a valid outgoing sound modifiers entry?
                if ( candidate ){
                    new_etype.out_sound_modifiers.push_back( candidate );
                }
            }
        }
        if ( !new_etype.out_sound_modifiers.empty() ){
            new_etype.flags.insert( flag_id( "EFFECT_MODIFIES_OUTGOING_SOUND" ) );
        }
    }
    

    effect_types[new_etype.id] = new_etype;
}

bool effect::has_flag( const flag_id &flag ) const
{
    return eff_type->has_flag( flag );
}

void reset_effect_types()
{
    effect_types.clear();
}

void effect_type::register_ma_buff_effect( const effect_type &eff )
{
    if( eff.id.is_valid() ) {
        debugmsg( "effect id %s of a martial art buff is already used as id for an effect",
                  eff.id.c_str() );
        return;
    }
    effect_types.insert( std::make_pair( eff.id, eff ) );
}

void effect::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "eff_type", eff_type != nullptr ? eff_type->id.str() : "" );
    json.member( "duration", duration );
    json.member( "bp", bp );
    json.member( "intensity", intensity );
    json.member( "start_turn", start_time );
    // Legacy
    if( permanent && eff_type && !eff_type->is_permanent() ) {
        json.member( "permanent", true );
    }
    json.end_object();
}
void effect::deserialize( JsonIn &jsin )
{
    JsonObject jo = jsin.get_object();
    const efftype_id id( jo.get_string( "eff_type" ) );
    eff_type = &id.obj();
    jo.read( "duration", duration );
    // @todo Remove after stable
    if( jo.has_int( "bp" ) ) {
        bp = convert_bp( static_cast<body_part>( jo.get_int( "bp" ) ) );
    } else {
        bp = bodypart_str_id( jo.get_string( "bp" ) );

    }
    intensity = jo.get_int( "intensity" );
    start_time = calendar::turn_zero;
    jo.read( "start_turn", start_time );
    permanent = jo.get_bool( "permanent", false );
    // Removed effects should never be saved
    removed = false;
}

std::string texitify_base_healing_power( const int power )
{
    if( power == 1 ) {
        return colorize( _( "very poor" ), c_red );
    } else if( power == 2 ) {
        return colorize( _( "poor" ), c_light_red );
    } else if( power == 3 ) {
        return colorize( _( "average" ), c_yellow );
    } else if( power == 4 ) {
        return colorize( _( "good" ), c_light_green );
    } else if( power >= 5 ) {
        return colorize( _( "great" ), c_green );
    }
    if( power < 1 ) {
        debugmsg( "Tried to convert zero or negative value." );
    }
    return "";
}

std::string texitify_healing_power( const int power )
{
    if( power >= 1 && power <= 2 ) {
        return colorize( _( "very poor" ), c_red );
    } else if( power >= 3 && power <= 4 ) {
        return colorize( _( "poor" ), c_light_red );
    } else if( power >= 5 && power <= 6 ) {
        return colorize( _( "average" ), c_yellow );
    } else if( power >= 7 && power <= 8 ) {
        return colorize( _( "good" ), c_yellow );
    } else if( power >= 9 && power <= 10 ) {
        return colorize( _( "very good" ), c_light_green );
    } else if( power >= 11 && power <= 12 ) {
        return colorize( _( "great" ), c_light_green );
    } else if( power >= 13 && power <= 14 ) {
        return colorize( _( "outstanding" ), c_green );
    } else if( power >= 15 ) {
        return colorize( _( "perfect" ), c_green );
    }
    if( power < 1 ) {
        debugmsg( "Converted value out of bounds." );
    }
    return "";
}

void caused_effect::load_decay( const JsonObject &jo )
{
    assign( jo, "allow_on_decay", allow_on_decay );
    assign( jo, "allow_on_remove", allow_on_remove );
    load( jo );
}

void caused_effect::load( const JsonObject &jo )
{
    assign( jo, "effect_type", type );
    assign( jo, "intensity_requirement", intensity_requirement );

    if( assign( jo, "duration", duration ) ) {
        // In case of copy-from
        inherit_duration = false;
    }
    assign( jo, "inherit_duration", inherit_duration );
    if( jo.has_member( "duration" ) && jo.has_member( "inherit_duration" ) ) {
        jo.throw_error( R"("duration" and "inherit_duration" can't both be set at the same time.)" );
    }

    if( assign( jo, "intensity", intensity ) ) {
        inherit_intensity = false;
    }
    assign( jo, "inherit_intensity", inherit_intensity );
    if( jo.has_member( "intensity" ) && jo.has_member( "inherit_intensity" ) ) {
        jo.throw_error( R"("intensity" and "inherit_intensity" can't both be set at the same time.)" );
    }

    if( assign( jo, "body_part", bp ) ) {
        inherit_body_part = false;
    }
    assign( jo, "inherit_body_part", inherit_body_part );
    if( jo.has_member( "intensity" ) && jo.has_member( "inherit_intensity" ) ) {
        jo.throw_error( R"("body_part" and "inherit_body_part" can't both be set at the same time.)" );
    }
}

std::vector<effect> effect::create_decay_effects() const
{
    return create_child_effects( true );
}

std::vector<effect> effect::create_removal_effects() const
{
    return create_child_effects( false );
}

std::vector<effect> effect::create_child_effects( bool decay ) const
{
    std::vector<effect> ret;
    for( const auto &new_effect : eff_type->effects_on_remove ) {
        if( this->intensity < new_effect.intensity_requirement ||
            ( decay && !new_effect.allow_on_decay ) ||
            ( !decay && !new_effect.allow_on_remove ) ) {
            continue;
        }
        const effect_type *new_effect_type = &*new_effect.type;
        time_duration dur = new_effect.inherit_duration ? this->duration : new_effect.duration;
        int intensity = new_effect.inherit_intensity ? this->intensity : new_effect.intensity;
        bodypart_str_id bp = new_effect.inherit_body_part ? this->bp : new_effect.bp;
        effect e = effect( new_effect_type, dur, bp, intensity, calendar::turn );
        ret.emplace_back( e );
    }
    return ret;
}

void outgoing_sound_modifiers::load( const JsonObject &jo ){
    
    if( jo.has_member( "checked_categories" ) ) {
        if ( !jo.get_array("checked_categories").empty() ){
            for( const std::string line : jo.get_array( "checked_categories" ) ) {
                const auto cat = sound_category_from_string( line );
                if ( cat != sounds::sound_t::_LAST){
                    checked_categories.push_back( cat );
                } else {
                    jo.throw_error( R"("Heard sound modifiers cannot check against a invalid sound category.)" );
                }
            } 
        }
    }
    if( jo.has_member( "checked_sound_descriptions" ) ) {
        if ( !jo.get_array("checked_sound_descriptions").empty() ){
            for( const std::string line : jo.get_array( "checked_sound_descriptions" ) ) {
                if ( line != std::string( "" ) ){
                    checked_sound_descriptions.push_back( line );
                } else {
                    jo.throw_error( R"("Heard sound modifiers cannot check against a blank sound description.)" );
                }            
            }
        }
    }
    if( jo.has_member( "replace_with_sound_descriptions" ) ) {
        if ( !jo.get_array("replace_with_sound_descriptions").empty() ){
            for( const std::string line : jo.get_array( "replace_with_sound_descriptions" ) ) {
                if ( line != std::string( "" ) ){
                    replace_with_sound_descriptions.push_back( line );
                } else {
                    jo.throw_error( R"("Heard sound modifiers cannot use a blank sound description for replacement.)" );
                }            
            }
        }
        if ( !replace_with_sound_descriptions.empty() ){
            optional(jo, valid, "choose_random_desc", choose_random_desc );
        }
    }

    if ( jo.has_member( "replace_with_npc_faction" ) ){
        replace_with_npc_faction = faction_id( jo.get_member( "replace_with_npc_faction" ).get_string() );
        replace_npc_faction_attribution = true;
    } 
    if ( jo.has_member( "replace_with_monster_faction" ) ){
        replace_with_monfaction = mfaction_str_id( jo.get_member( "replace_with_monster_faction" ).get_string() );
        replace_monster_faction_attribution = true;
    } 

    optional( jo, valid, "intensity_min_requirment", intensity_min_requirment );

    if( jo.has_member("volume_mdB_adj") ){
        if ( !jo.get_array("volume_mdB_adj").empty() ){
            for( auto entry : jo.get_array("volume_mdB_adj") ){
                volume_mdB_adj.push_back( entry.get_int() );
            }
            bool has_nonzero = false;
            for( auto &entry : volume_mdB_adj ){
                if ( entry > 0 ){
                    has_nonzero = true;
                }
            }
            if ( !has_nonzero ){
                jo.throw_error( R"("Outgoing sound modifier volume_mdB_adj vector must contain atleast one non-zero entry.)" );
                // clear the vector, since there is nothing actually usable in there.
                volume_mdB_adj.clear();
            }
        }
    } // Adjusts the mdB volume of all heard sounds by this amount. Will influence deafening.
    if ( !volume_mdB_adj.empty() ){
        optional(jo, valid, "volume_mdB_adj_min_val" , volume_mdB_adj_min_val ); // Defaults to 0 
        optional(jo, valid, "volume_mdB_adj_max_val" , volume_mdB_adj_max_val ); // Defaults to 0, which means uncapped. 
        optional(jo, valid, "volume_mdB_adj_intensity_scaling" , volume_mdB_adj_intensity_scaling ); // Optional mult per intensity. Defaults to 1
    }

    if( jo.has_member("volume_mdB_floor") ){
        if ( !jo.get_array("volume_mdB_floor").empty() ){
            for( auto entry : jo.get_array("volume_mdB_floor") ){
                volume_mdB_floor.push_back( entry.get_int() );
            }
            bool has_nonzero = false;
            for( auto &entry : volume_mdB_floor ){
                if ( entry > 0 ){
                    has_nonzero = true;
                }
            }
            if ( !has_nonzero ){
                jo.throw_error( R"("Outgoing sound modifier volume_mdB_floor vector must contain atleast one non-zero entry.)" );
                // clear the vector, since there is nothing actually usable in there.
                volume_mdB_floor.clear();
            }
        }
    } // Adjusts the mdB volume of all heard sounds by this amount. Will influence deafening.
    if ( !volume_mdB_floor.empty() ){
        optional(jo, valid, "volume_mdB_floor_min_val" , volume_mdB_floor_min_val ); // Defaults to 0 
        optional(jo, valid, "volume_mdB_floor_max_val" , volume_mdB_floor_max_val ); // Defaults to 0, which means uncapped. 
        optional(jo, valid, "volume_mdB_floor_intensity_scaling" , volume_mdB_floor_intensity_scaling ); // Optional mult per intensity. Defaults to 1
    }

    if( jo.has_member("volume_mdB_ceiling") ){
        if ( !jo.get_array("volume_mdB_ceiling").empty() ){
            for( auto entry : jo.get_array("volume_mdB_ceiling") ){
                volume_mdB_ceiling.push_back( entry.get_int() );
            }
            bool has_nonzero = false;
            for( auto &entry : volume_mdB_ceiling ){
                if ( entry > 0 ){
                    has_nonzero = true;
                }
            }
            if ( !has_nonzero ){
                jo.throw_error( R"("Outgoing sound modifiers volume_mdB_ceiling vector must contain atleast one non-zero entry.)" );
                // clear the vector, since there is nothing actually usable in there.
                volume_mdB_ceiling.clear();
            }
        }
    } // Adjusts the mdB volume of all heard sounds by this amount. Will influence deafening.
    if ( !volume_mdB_ceiling.empty() ){
        optional(jo, valid, "volume_mdB_ceiling_min_val" , volume_mdB_ceiling_min_val ); // Defaults to 0 
        optional(jo, valid, "volume_mdB_ceiling_max_val" , volume_mdB_ceiling_max_val ); // Defaults to 0, which means uncapped. 
        optional(jo, valid, "volume_mdB_ceiling_intensity_scaling" , volume_mdB_ceiling_intensity_scaling ); // Optional mult per intensity. Defaults to 1
    }

    if ( !replace_with_sound_descriptions.empty() || replace_monster_faction_attribution || replace_npc_faction_attribution || !volume_mdB_ceiling.empty() || !volume_mdB_floor.empty() || !volume_mdB_adj.empty() ){
        // Make sure that we have atleast one usable modifier.
        valid = true;
    }  
}

void heard_sound_modifiers::load( const JsonObject &jo )
{

    if( jo.has_member( "checked_categories" ) ) {
        if ( !jo.get_array("checked_categories").empty() ){
            for( const std::string line : jo.get_array( "checked_categories" ) ) {
                const auto cat = sound_category_from_string( line );
                if ( cat != sounds::sound_t::_LAST){
                    checked_categories.push_back( cat );
                } else {
                    jo.throw_error( R"("Heard sound modifiers cannot check against a invalid sound category.)" );
                }
            } 
        }
    }
    if( jo.has_member( "checked_sound_descriptions" ) ) {
        if ( !jo.get_array("checked_sound_descriptions").empty() ){
            for( const std::string line : jo.get_array( "checked_sound_descriptions" ) ) {
                if ( line != std::string( "" ) ){
                    checked_sound_descriptions.push_back( line );
                } else {
                    jo.throw_error( R"("Heard sound modifiers cannot check against a blank sound description.)" );
                }            
            }
        }
    }
    if( jo.has_member( "replace_with_sound_descriptions" ) ) {
        if ( !jo.get_array("replace_with_sound_descriptions").empty() ){
            for( const std::string line : jo.get_array( "replace_with_sound_descriptions" ) ) {
                if ( line != std::string( "" ) ){
                    replace_with_sound_descriptions.push_back( line );
                } else {
                    jo.throw_error( R"("Heard sound modifiers cannot use a blank sound description for replacement.)" );
                }            
            }
        }
        if ( !replace_with_sound_descriptions.empty() ){
            optional(jo, valid, "choose_random_desc", choose_random_desc );
        }
    }

    if ( jo.has_member( "replace_with_npc_faction" ) ){
        replace_with_npc_faction = faction_id( jo.get_member( "replace_with_npc_faction" ).get_string() );
        replace_npc_faction_attribution = true;
    } 
    if ( jo.has_member( "replace_with_monster_faction" ) ){
        replace_with_monfaction = mfaction_str_id( jo.get_member( "replace_with_monster_faction" ).get_string() );
        replace_monster_faction_attribution = true;
    } 

    optional( jo, valid, "intensity_min_requirment", intensity_min_requirment );

    if( jo.has_member("base_mdB_volume_adj") ){
        if ( !jo.get_array("base_mdB_volume_adj").empty() ){
            for( auto entry : jo.get_array("base_mdB_volume_adj") ){
                base_mdb_adj.push_back( entry.get_int() );
            }
            bool has_nonzero = false;
            for( auto &entry : base_mdb_adj){
                if ( entry > 0 ){
                    has_nonzero = true;
                }
            }
            if ( !has_nonzero ){
                jo.throw_error( R"("Heard sound modifier base_mdB_volume_adj vector must contain atleast one non-zero entry.)" );
                // clear the vector, since there is nothing actually usable in there.
                base_mdb_adj.clear();
            }
        }
    } // Adjusts the base mdB volume of all incoming sounds. Will influence deafening.
    if ( !base_mdb_adj.empty() ){
        optional(jo, valid, "base_mdb_adj_min_val" , base_mdb_adj_min_val ); // Defaults to 0 
        optional(jo, valid, "base_mdb_adj_max_val" , base_mdb_adj_max_val ); // Defaults to 0, which means uncapped. 
        optional(jo, valid, "base_mdB_adj_intensity_scaling" , base_mdb_adj_intensity_scaling ); // Optional mult per intensity. Defaults to 1
    }
    
    if( jo.has_member("heard_vol_mdb_adj") ){
        if ( !jo.get_array("heard_vol_mdb_adj").empty() ){
            for( auto entry : jo.get_array("heard_vol_mdb_adj") ){
                heard_vol_mdb_adj.push_back( entry.get_int() );
            }
            bool has_nonzero = false;
            for( auto &entry : heard_vol_mdb_adj){
                if ( entry > 0 ){
                    has_nonzero = true;
                }
            }
            if ( !has_nonzero ){
                jo.throw_error( R"("Heard sound modifier heard_vol_mdb_adj vector must contain atleast one non-zero entry.)" );
                // clear the vector, since there is nothing actually usable in there.
                heard_vol_mdb_adj.clear();
            }
        }
    } // Adjusts the mdB volume of all heard sounds by this amount. Will influence deafening.
    if ( !heard_vol_mdb_adj.empty() ){
        optional(jo, valid, "heard_vol_mdb_adj_min_val" , heard_vol_mdb_adj_min_val ); // Defaults to 0 
        optional(jo, valid, "heard_vol_mdb_adj_max_val" , heard_vol_mdb_adj_max_val ); // Defaults to 0, which means uncapped. 
        optional(jo, valid, "heard_vol_mdB_adj_intensity_scaling" , heard_vol_mdb_adj_intensity_scaling ); // Optional mult per intensity. Defaults to 1
    }
    
    if( jo.has_member("perceived_vol_mdb_adj") ){
        if ( !jo.get_array("perceived_vol_mdb_adj").empty() ){
            for( auto entry : jo.get_array("perceived_vol_mdb_adj") ){
                perceived_vol_mdb_adj.push_back( entry.get_int() );
            }
            bool has_nonzero = false;
            for( auto &entry : perceived_vol_mdb_adj){
                if ( entry > 0 ){
                    has_nonzero = true;
                }
            }
            if ( !has_nonzero ){
                jo.throw_error( R"("Heard sound modifier perceived_vol_mdb_adj vector must contain atleast one non-zero entry.)" );
                // clear the vector, since there is nothing actually usable in there.
                perceived_vol_mdb_adj.clear();
            }
        }
    } // Adjusts the perceived mdB volume of all heard sounds by this amount. Does not influence deafening.
    if ( !perceived_vol_mdb_adj.empty() ){
        optional(jo, valid, "perceived_vol_mdb_adj_min_val" , perceived_vol_mdb_adj_min_val ); // Defaults to 0 
        optional(jo, valid, "perceived_vol_mdb_adj_max_val" , perceived_vol_mdb_adj_max_val ); // Defaults to 0, which means uncapped. 
        optional(jo, valid, "perceived_vol_mdB_adj_intensity_scaling" , perceived_vol_mdb_adj_intensity_scaling ); // Optional mult per intensity. Defaults to 1
    }
    
    if( jo.has_member("hearing_threshold_mdb_adj") ){
        if ( !jo.get_array("hearing_threshold_mdb_adj").empty() ){
            for( auto entry : jo.get_array("hearing_threshold_mdb_adj") ){
                hearing_threshold_mdb_adj.push_back( entry.get_int() );
            }
            bool has_nonzero = false;
            for( auto &entry : hearing_threshold_mdb_adj){
                if ( entry > 0 ){
                    has_nonzero = true;
                }
            }
            if ( !has_nonzero ){
                jo.throw_error( R"("Heard sound modifier hearing_threshold_mdb_adj vector must contain atleast one non-zero entry.)" );
                // clear the vector, since there is nothing actually usable in there.
                hearing_threshold_mdb_adj.clear();
            }
        }
    } // Adjusts the entities volume thresholds by this amount.
    if ( !hearing_threshold_mdb_adj.empty() ){
        optional(jo, valid, "hearing_threshold_mdb_adj_min_val" , hearing_threshold_mdb_adj_min_val ); // Defaults to 0
        optional(jo, valid, "hearing_threshold_mdb_adj_max_val" , hearing_threshold_mdb_adj_max_val ); // Defaults to 0, which means uncapped. 
        optional(jo, valid, "hearing_threshold_mdB_adj_intensity_scaling" , hearing_threshold_mdb_adj_intensity_scaling ); // Optional mult per intensity. Defaults to 1
    }
    
    if( jo.has_member("hearing_protection_mdb_adj") ){
        if ( !jo.get_array("hearing_protection_mdb_adj").empty() ){
            for( auto entry : jo.get_array("hearing_protection_mdb_adj") ){
                hearing_protection_mdb_adj.push_back( entry.get_int() );
            }
            bool has_nonzero = false;
            for( auto &entry : hearing_protection_mdb_adj){
                if ( entry > 0 ){
                    has_nonzero = true;
                }
            }
            if ( !has_nonzero ){
                jo.throw_error( R"("Heard sound modifier hearing_protection_mdb_adj vector must contain atleast one non-zero entry.)" );
                // clear the vector, since there is nothing actually usable in there.
                hearing_protection_mdb_adj.clear();
            }
        }
    }
    if ( !hearing_protection_mdb_adj.empty() ){
        optional(jo, valid, "hearing_protection_basic_mdb_adj_min_val" , hearing_protection_mdb_adj_min_val );// Defaults to 0
        optional(jo, valid, "hearing_protection_basic_mdb_adj_max_val" , hearing_protection_mdb_adj_max_val );// Defaults to 0, which means uncapped.  
        optional(jo, valid, "hearing_protection_basic_mdb_adj_intensity_scaling" , hearing_protection_mdb_adj_intensity_scaling ); // Optional mult per intensity. Defaults to 1
    }
    
    if( jo.has_member("hearing_protection_adv_mdb_adj") ){
        if ( !jo.get_array("hearing_protection_adv_mdb_adj").empty() ){
            for( auto entry : jo.get_array("hearing_protection_adv_mdb_adj") ){
                hearing_protection_adv_mdb_adj.push_back( entry.get_int() );
            }
            bool has_nonzero = false;
            for( auto &entry : hearing_protection_adv_mdb_adj){
                if ( entry > 0 ){
                    has_nonzero = true;
                }
            }
            if ( !has_nonzero ){
                jo.throw_error( R"("Heard sound modifier hearing_protection_adv_mdb_adj vector must contain atleast one non-zero entry.)" );
                // clear the vector, since there is nothing actually usable in there.
                hearing_protection_adv_mdb_adj.clear();
            }
        }
    }
    if ( !hearing_protection_adv_mdb_adj.empty() ){
        optional(jo, valid, "hearing_protection_adv_mdb_adj_min_val" , hearing_protection_adv_mdb_adj_min_val );// Defaults to 0
        optional(jo, valid, "hearing_protection_adv_mdb_adj_max_val" , hearing_protection_adv_mdb_adj_max_val );// Defaults to 0, which means uncapped. 
        optional(jo, valid, "hearing_protection_adv_mdb_adj_intensity_scaling" , hearing_protection_adv_mdb_adj_intensity_scaling ); // Optional mult per intensity. Defaults to 1
    }
    
    if( jo.has_member("permanant_hearing_loss_mdb_adj") ){
        if ( !jo.get_array("permanant_hearing_loss_mdb_adj").empty() ){
            for( auto entry : jo.get_array("permanant_hearing_loss_mdb_adj") ){
                permanant_hearing_loss_mdb_adj.push_back( entry.get_int() );
            }
            bool has_nonzero = false;
            for( auto &entry : permanant_hearing_loss_mdb_adj){
                if ( entry > 0 ){
                    has_nonzero = true;
                }
            }
            if ( !has_nonzero ){
                jo.throw_error( R"("Heard sound modifier permanant_hearing_loss_mdb_adj vector must contain atleast one non-zero entry.)" );
                // clear the vector, since there is nothing actually usable in there.
                permanant_hearing_loss_mdb_adj.clear();
            }
        }
    }
    if ( !permanant_hearing_loss_mdb_adj.empty() ){
        optional(jo, valid, "permanant_hearing_loss_mdb_adj_min_val" , permanant_hearing_loss_mdb_adj_min_val ); // Defaults to 0
        optional(jo, valid, "permanant_hearing_loss_mdb_adj_max_val" , permanant_hearing_loss_mdb_adj_max_val ); // Defaults to 0, which means uncapped. 
        optional(jo, valid, "permanant_hearing_loss_mdb_adj_intensity_scaling" , permanant_hearing_loss_mdb_adj_intensity_scaling ); // Optional scaling per intensity. Defaults to 1
    }
    
    if ( !replace_with_sound_descriptions.empty() || replace_monster_faction_attribution || replace_npc_faction_attribution || !permanant_hearing_loss_mdb_adj.empty() || !hearing_protection_adv_mdb_adj.empty() || !hearing_protection_mdb_adj.empty() || !hearing_threshold_mdb_adj.empty() || !perceived_vol_mdb_adj.empty() || !heard_vol_mdb_adj.empty() || !base_mdb_adj.empty() ){
        // Make sure that we have atleast one usable modifier.
        valid = true;
    }  
}

void caused_sound::load( const JsonObject &jo )
{
    optional( jo, valid, "allow_on_apply", allow_on_apply );
    optional( jo, valid, "allow_on_increment", allow_on_increment );
    optional( jo, valid, "allow_on_decay", allow_on_decay );
    optional( jo, valid, "allow_on_remove", allow_on_remove );
    optional( jo, valid, "time_based", time_based );
    optional( jo, valid, "random_time", random_time );
    
    bool valid_random_time_interval = false;
    bool valid_time_interval = false;

    optional( jo, valid, "intensity_min_requirment", intensity_min_requirment );

    if( jo.has_member( "base_time_intervals" ) ) {

        if ( !jo.get_array( "base_time_intervals" ).empty() ) {
            const auto &time_intervals = jo.get_array( "base_time_intervals" );

            for(const std::string &interval : time_intervals ) {
                // This is annoying but read_from_json_string is even more annoying.
                base_emission_intervals.push_back( string_from_json_to_typed_unit<time_duration>( interval, time_duration::units, jo ) );

            }
            if ( !base_emission_intervals.empty() ){
                valid_time_interval = true;
            }
        }

    }

    if ( valid_time_interval ){
        optional( jo, valid, "intensity_interval_scaling", intensity_interval_scaling );
    }

    if( jo.has_member( "random_time_intervals" ) ) {
        random_time = true;
        if ( !jo.get_array( "random_time_intervals" ).empty() ) {
            const auto &time_intervals = jo.get_array( "random_time_intervals" );

            for( JsonArray listing : time_intervals ) {
                // This is annoying but read_from_json_string is even more annoying.
                const auto &min = string_from_json_to_typed_unit<time_duration>( listing.get_string( 0 ), time_duration::units, jo );
                const auto &max = string_from_json_to_typed_unit<time_duration>( listing.get_string( 1 ), time_duration::units, jo );
                const std::pair<time_duration,time_duration> entry = { min, max};
                random_interval_minmax.push_back( entry );

            }
            if ( !random_interval_minmax.empty() ){
                valid_random_time_interval = true;
            }
        }
    }

    if ( valid_random_time_interval ){
        optional( jo, valid, "intensity_random_interval_min_scaling", intensity_random_interval_min_scaling );
        optional( jo, valid, "intensity_random_interval_max_scaling", intensity_random_interval_max_scaling );
    }
    
    optional(jo, valid, "sfx_only", sfx_only );

    // What volume should the sound be made at?
    mandatory(jo, valid, "base_db_volume", base_dB_volume );
    if ( base_dB_volume <= 0 || base_dB_volume > 191 ){
        jo.throw_error( R"("A effect caused sound must have a decibel volume between 1 and 191.)" );
        base_dB_volume = std::max( static_cast<short>(1), std::min( static_cast<short>(191), base_dB_volume) );
    }

    // Optional: How much should we increase the volume per parent effect intensity?
    optional(jo, valid, "intensity_db_volume_scaling", intensity_db_volume_scaling );

    if( jo.has_member( "categories" ) ) {
        bool valid_category = false;
        for( const std::string line : jo.get_array( "categories" ) ) {
            if ( sound_category_from_string( line ) != sounds::sound_t::_LAST ){
                categories.push_back( sound_category_from_string( line ) );
                valid_category = true;
            } 
        }
        // If a sound does not have a valid activity, give it a single activity entry.
        if ( !valid_category ){
             categories.push_back( sounds::sound_t::activity );
        }
    }

    bool valid_desc = false;
    if( jo.has_member( "sound_descriptions" ) ) {
        for( const std::string line : jo.get_array( "sound_descriptions" ) ) {
            sound_descriptions.push_back( line );
        }
        
        for (const std::string desc : sound_descriptions ){
            if ( desc != std::string( "" ) ){
                valid_desc = true;
            }
        }
        if ( !valid_desc ){
            jo.throw_error( R"("Sound must have atleast one valid description.)" );
        }
    }

    optional(jo, valid, "choose_random_desc", choose_random_desc );
    optional(jo, valid, "from_effected_creature", from_effected_creature );
    optional( jo, valid, "movement_noise", movement_noise );

    if ( !from_effected_creature ){
        optional( jo, valid, "from_player", from_player );
        optional( jo, valid, "from_npc", from_npc );
        optional( jo, valid, "from_monster", from_monster );
        if (jo.has_member( "npc_faction" ) ){
            faction = faction_id( jo.get_member( "npc_faction" ).get_string() );
        }
        if (jo.has_member( "monster_faction" ) ){
            monfaction = mfaction_str_id( jo.get_member( "monster_faction" ).get_string() );
        }
    }
    bool valid_sfx_pair = true;
    if( jo.has_member( "sfx_idvariant_pairs" ) ) {
        
        if ( !jo.get_array( "sfx_idvariant_pairs" ).empty() ) {
            valid_sfx_pair = false;
            // We dont want to keep the default sfx idvariant pair if we are actually loading specified sfx ids and variants.
            sfx_idvariant_pairs.clear();
            const auto &pairlist = jo.get_array( "sfx_idvariant_pairs" );

            for( JsonArray sfxpair : pairlist ) {
                // This is annoying but read_from_json_string is even more annoying.
                const auto &id = sfxpair.get_string( 0 );
                const auto &variant = sfxpair.get_string( 1 );
                const std::pair<std::string,std::string> entry = { id, variant};
                sfx_idvariant_pairs.push_back( entry );

            }
            if ( !sfx_idvariant_pairs.empty() ){
                valid_sfx_pair = true;
            }
        } 
        if (!valid_sfx_pair ) {
            jo.throw_error( R"("No valid sfx id and variant pair provided for caused sound.)" );
        }
    }
    // Run through all of the checks to make sure we actually have a valid sound without conflicting settings.
    if ( valid_sfx_pair && valid_desc && (base_dB_volume > 0 && base_dB_volume <= 191 ) ){

        if ( (from_monster + from_npc + from_player) > 1 ){
            jo.throw_error( R"("Effect caused sound can only have at most one from_monster/npc/player bool set.)" );

        } else if ( !allow_on_apply && !allow_on_increment && !allow_on_decay && !allow_on_remove && !time_based && !random_time ){
            jo.throw_error( R"("A caused sound must have atleast one emit condition bool set.)" );

        } else if ( time_based && !valid_time_interval ){
            jo.throw_error( R"("A time based effect caused sound must have atleast one valid time duration.)" );

        } else if (random_time && !valid_random_time_interval ){
            jo.throw_error( R"("A random time based effect caused sound must have atleast one valid pair of time durations.)" );

        } else {
            // We have at bare minimum the things we need. 
            // Setting valid to true actually lets this caused sound 
            // to be added to the caused sounds vector.
            valid = true;
        }
    }
}

std::vector<sound_event> effect::create_apply_sounds( const Creature *critter ) const{
    std::vector<sound_event> created_sounds;
    if ( has_apply_sounds() ){
        for( const caused_sound &cs : eff_type->get_caused_sounds() ){
            if ( cs.allow_on_apply && intensity >= cs.intensity_min_requirment ){
                created_sounds.push_back( create_sound_event( cs, critter ) );
            }
        }
    }
    return created_sounds;
}

std::vector<sound_event> effect::create_increment_sounds( const Creature *critter ) const{
    std::vector<sound_event> created_sounds;
    if ( has_increment_sounds() ){
        for( const caused_sound &cs : eff_type->get_caused_sounds() ){
            if ( cs.allow_on_increment && intensity >= cs.intensity_min_requirment ){
                created_sounds.push_back( create_sound_event( cs, critter ) );
            }
        }
    }
    return created_sounds;
}

std::vector<sound_event> effect::create_decay_sounds( const Creature *critter ) const{
    std::vector<sound_event> created_sounds;
    if ( has_decay_sounds() ){
        for( const caused_sound &cs : eff_type->get_caused_sounds() ){
            if ( cs.allow_on_decay && intensity >= cs.intensity_min_requirment ){
                created_sounds.push_back( create_sound_event( cs, critter ) );
            }
        }
    }
    return created_sounds;
}

std::vector<sound_event> effect::create_remove_sounds( const Creature *critter ) const{
    std::vector<sound_event> created_sounds;
    if ( has_remove_sounds() ){
        for( const caused_sound &cs : eff_type->get_caused_sounds() ){
            if ( cs.allow_on_remove && intensity >= cs.intensity_min_requirment ){
                created_sounds.push_back( create_sound_event( cs, critter ) );
            }
        }
    }
    return created_sounds;
}

std::vector<sound_event> effect::create_time_based_sounds( const Creature *critter, const time_point &time ) {

    std::vector<sound_event> created_sounds;
    for( auto &time_details : caused_sound_time_intervals ){

        const caused_sound &cs = eff_type->caused_sounds[time_details.index];
        const auto &emit = action_time_scale::calendar_ticks_crossed_this_tick( time, time_details.interval ) > 0;

        if ( intensity >= cs.intensity_min_requirment && emit ){  

            created_sounds.push_back( create_sound_event( cs, critter ) );
            if ( cs.random_time ) {
                time_details.dirty = true;
            }
        }
    }
    return created_sounds;
}

// Chose which sound description to use based on intensity etc.
std::string effect::select_sound_desc( const std::vector<std::string> &descs, const int &adj_intensity, const bool &random ) const{
    if ( descs.empty() ){
        debugmsg( "Effect type [%1s] attempted to get a sound description from an empty description vector", eff_type->id.str() );
        return std::string( "REPORT THIS SOUND" );
    } else if ( descs.size() == 1 ) {
        return descs[0];
    } else if ( random ) {
        const auto &iterator = rng( 1, descs.size() ) - 1;
        return descs[iterator];
    } else {
        const auto &iterator = std::max<size_t>( 0, (std::min<size_t>( adj_intensity, descs.size() ) - 1 ) );
        return descs[iterator];
    }
}

// Chose which sound category to use based on intensity etc.
sounds::sound_t effect::select_sound_cat( const std::vector<sounds::sound_t> &cats, const int &adj_intensity ) const{
    if ( cats.empty() ){
        return sounds::sound_t::activity;
    } else if ( cats.size() == 1 ) {
        return cats[0];
    } else {
        const auto &iterator = std::max<size_t>( 0, (std::min<size_t>( adj_intensity, cats.size() ) - 1 ) );
        return cats[iterator];
    }
}

// Chose which sound sfx pair to use based on intensity etc.
std::pair<std::string,std::string> effect::select_sound_sfx( const std::vector<std::pair<std::string,std::string>> &pairs, const int &adj_intensity ) const{
    if ( pairs.empty() ){
        debugmsg( "Effect type [%1s] attempted to get sound sfx from an empty sfx idvariant vector", eff_type->id.str() );
        return { std::string(""),std::string( "default" ) };
    } else if ( pairs.size() == 1 ) {
        return pairs[0];
    } else {
        const auto &iterator = std::max<size_t>( 0, (std::min<size_t>( adj_intensity, pairs.size() ) - 1 ) );
        return pairs[iterator];
    }
}

sound_event effect::create_sound_event( const caused_sound &cs, const Creature *critter ) const{
    bool valid_critter = false;
    if ( critter != nullptr){
        valid_critter = true;
    }
    sound_event sound;
    // We know that we have atleast our minimum intensity requirment satisfied.
    // But we need to adjust our intensity based on min intensity requirment.
    const int adj_intensity = ( cs.intensity_min_requirment != 0 ) ? intensity - ( cs.intensity_min_requirment - 1 ) : intensity;

    sound.description = select_sound_desc( cs.sound_descriptions, adj_intensity, cs.choose_random_desc );

    sound.category = select_sound_cat( cs.categories, adj_intensity );

    sound.volume = cs.base_dB_volume + std::round( ( adj_intensity - 1 ) * cs.intensity_db_volume_scaling );

    if ( !cs.sfx_only ){
        sound.volume = std::max<short>( 0, std::min<short>( 191, sound.volume ) );
    } else {
        // If we are only doing this for sfx, cap our volume to the sfx system limits.
        sound.volume = std::max<short>( 0, std::min<short>( 100, sound.volume ) );
    }

    if ( valid_critter ){
        sound.origin = critter->bub_pos();
        if ( cs.from_effected_creature) {

            if ( critter->is_avatar() ) {

                const player &p = get_avatar();
                sound.from_player = true;
                sound.faction = p.get_faction()->id;
                sound.monfaction = p.get_faction()->mon_faction;

            } else if (critter->is_npc() ) {

                // NPCs have some very strange and annoying abstraction.
                player *const npc = g->critter_at<player>( sound.origin, true );
                sound.from_npc = true;
                sound.faction = npc->get_faction()->id;
                sound.monfaction = npc->get_faction()->mon_faction;

            } else if (critter->is_monster() ) {

                const auto *mon = critter->as_monster();
                sound.from_monster = true;
                sound.monfaction = mon->faction.id();

            } else {
                // Well, something reaaaaaly funky has happened. Just read the caused sounds details and move on.
                sound.from_monster = cs.from_monster;
                sound.from_npc = cs.from_npc;
                sound.from_player = cs.from_player;
                sound.faction = cs.faction;
                sound.monfaction = cs.monfaction;
            }
        } else {

            sound.from_monster = cs.from_monster;
            sound.from_npc = cs.from_npc;
            sound.from_player = cs.from_player;
            sound.faction = cs.faction;
            sound.monfaction = cs.monfaction;

        }
    } else {

        // If we got handed a null pointer, put the location as at the player's position. 
        sound.origin = get_avatar().bub_pos();
        sound.from_monster = cs.from_monster;
        sound.from_npc = cs.from_npc;
        sound.from_player = cs.from_player;
        sound.faction = cs.faction;
        sound.monfaction = cs.monfaction;

    }

    sound.movement_noise = cs.movement_noise || sound.category == sounds::sound_t::movement;
    const auto idvarpair = select_sound_sfx(cs.sfx_idvariant_pairs, adj_intensity);

    sound.id = idvarpair.first;
    sound.variant = idvarpair.second;

    return sound;
}

void effect::update_sound_time_intervals( const bool &dirty_only )
{
    // Quickly check that we actually have any time based sounds 
    if ( !has_time_based_sounds() ){
        return;
    }

    auto select_time_interval = [&]( const caused_sound &ce, const int &adj_intensity ) -> time_duration{

        time_duration base_dur = 0_seconds;
        time_duration min_rand = 0_seconds;
        time_duration max_rand = 0_seconds;
        const time_duration &base_scaling = (ce.intensity_interval_scaling == 0_seconds ) ? 0_seconds : ce.intensity_interval_scaling * ( adj_intensity - 1);
        const time_duration &minr_scaling = (ce.intensity_random_interval_min_scaling == 0_seconds ) ? 0_seconds : ce.intensity_random_interval_min_scaling * ( adj_intensity - 1);
        const time_duration &maxr_scaling = (ce.intensity_random_interval_max_scaling == 0_seconds ) ? 0_seconds : ce.intensity_random_interval_max_scaling * ( adj_intensity - 1);
        
        if ( ce.random_time ) {
            const auto &pairs = ce.random_interval_minmax;
            if ( pairs.empty() ){
                debugmsg( "Effect type [%1s] attempted to get a random time interval from an empty random time intervals vector", eff_type->id.str() );
            } else if ( pairs.size() == 1 ) {
                min_rand = pairs[0].first + minr_scaling;
                max_rand = pairs[0].second + maxr_scaling;
            } else {
                const auto &iterator = std::max<size_t>( 0, (std::min<size_t>( adj_intensity, pairs.size() ) - 1 ) );
                min_rand = pairs[iterator].first + minr_scaling;
                max_rand = pairs[iterator].second + maxr_scaling;
            }
        }
        if ( ce.time_based ) {
            const auto &intervals = ce.base_emission_intervals;
            if ( intervals.empty() ){
                debugmsg( "Effect type [%1s] attempted to get a base time interval from an empty base time intervals vector", eff_type->id.str() );
            } else if ( intervals.size() == 1 ) {
                base_dur = intervals[0] + base_scaling;
            } else {
                const auto &iterator = std::max<size_t>( 0, (std::min<size_t>( adj_intensity, intervals.size() ) - 1 ) );
                base_dur = intervals[iterator] + base_scaling;
            }
        }

        const auto &minseconds = std::max( 0, to_seconds<int>( min_rand ) );
        const auto &maxseconds = std::max( 0, to_seconds<int>( max_rand ) );
        const auto &randresult = (maxseconds > minseconds ) ? rng( minseconds, maxseconds ) : ( minseconds == maxseconds ) ? maxseconds : rng( maxseconds, minseconds );

        if ( ce.random_time && !ce.time_based ){

            return time_duration::from_seconds( randresult );

        } else if ( ce.time_based && !ce.random_time ){
            return base_dur;
        }
        
        const auto &baseseconds = std::max( 1, to_seconds<int>( base_dur ) );

        const auto &resultseconds = std::max( 1, ( baseseconds + ( (one_in(2) ? randresult : - randresult ) ) ) );

        return time_duration::from_seconds( resultseconds );

    };
    // We should only encounter this the first time our effect is added to a creature.
    if ( caused_sound_time_intervals.empty() ){
        // Lets build our initial time intervals.
        const int num_sounds = eff_type->caused_sounds.size();
        // Step through our caused sounds vector.
        for ( int i = 0; i < num_sounds; i++ ){
            const auto &cs = eff_type->caused_sounds[i];
                const int adj_intensity = ( intensity <= cs.intensity_min_requirment ) ? 1 : intensity - cs.intensity_min_requirment;
                caused_sound_interval_details csdetails;
                csdetails.index = i;
                csdetails.interval = select_time_interval( cs, adj_intensity );
                csdetails.dirty = false;
                caused_sound_time_intervals.push_back( csdetails );
        }
    } else {

        for ( caused_sound_interval_details &csdet : caused_sound_time_intervals ){
            if ( dirty_only && !csdet.dirty ) {
                continue;
            }
            const auto &cs = eff_type->caused_sounds[csdet.index];
            const int adj_intensity = ( intensity <= cs.intensity_min_requirment ) ? 1 : intensity - cs.intensity_min_requirment;

            csdet.interval = select_time_interval( cs, adj_intensity );
            csdet.dirty = false;
        }
    }
}

sound_modifier_base::~sound_modifier_base() = default;

bool sound_modifier_base::can_modify( const sound_event &se ) const{
    if ( !status.test(6) ) {
        return false;
    }
    if ( !filter.test(0) ) {
        bool valid_cat = false;
        for ( const sounds::sound_t &cat : checked_categories ) {
            if ( se.category == cat ) {
                valid_cat = true;
            }
        }
        if ( !valid_cat ){
            return false;
        }
    }
    if ( !filter.test(1) ) {
        bool valid_desc = false;
        for ( const auto &desc : checked_sound_descriptions ) {
            if ( se.description == desc ) {
                valid_desc = true;
            }
        }
        if ( !valid_desc ){
            return false;
        }
    }
    return true;
}

outgoing_sound_modifier_instance::~outgoing_sound_modifier_instance() = default;

incoming_sound_modifier_instance::~incoming_sound_modifier_instance() = default;

// If ready() fails due to being unable to properly update/remove 
// ready() will trigger one full rebuild, and attempt the check again.
// If that also fails, we abort, send a debugwarning, and fully clear the controller and all its bits.
// if ready() fails due to the controller not being bound, we just abort. 
bool sound_modifiers_controller::ready() {

    // indirty      = status[0];
    // outdirty     = status[1];
    // inactive     = status[2];
    // outactive    = status[3];
    // inrmarked    = status[4];
    // outrmarked   = status[5];
    // rebuild      = status[6];
    // valid        = status[7];
    
    auto &in_global = inmods.global_mods;
    auto &in_conditional = inmods.conditional_mods;
    auto &out_global = outmods.global_mods;
    auto &out_conditional = outmods.conditional_mods;

    auto ready_check = [&]() -> bool {
        return !status.test(0) && !status.test(1) && !status.test(4) && !status.test(5) && !status.test(6);
    };
    
    if ( !is_bound() ){
        return false;
    } else if ( ready_check() ) {
        return true;
    }
    if ( !status[6] ){
        // this will set inmods and outmods to dirty,
        // but that is a fine precaution here as we should not ever find lingering mods marked for removal.
        if ( status[4] || status[5] ) {
            // Make sure that it was not just odd lingering values.
            status[4] = false;
            status[5] = false;
            auto mark_remove = [&]( sound_modifier_base &inst ) -> bool {
                return inst.marked_for_removal();
            };
            // Just loop through our 4 vectors and mark instanced.
            for( auto &inst : in_global ) {
                if( mark_remove(inst) ) {
                    status[4] = true;
                }
            }
            for( auto &inst : in_conditional ) {
                if( mark_remove(inst) ) {
                    status[4] = true;
                }
            }
            for( auto &inst : out_global ) {
                if( mark_remove(inst) ) {
                    status[5] = true;
                }
            }
            for( auto &inst : out_conditional ) {
                if( mark_remove(inst) ) {
                    status[5] = true;
                }
            }
            // If we found modifiers marked for removal, remove them.
            if ( status[4] || status[5] ) {
                remove_marked();
            }
            
        }
        if ( status[0] || status[1] ) {
            update();
        }
        if ( ready_check() ) {
            return true;
        } else {
            status.set(6);
        }
            
    }
    if ( rebuild() ){
        if ( ready_check() ) {
            return true;
        } else {
            const auto loc = parent->bub_pos();
            debugmsg( "Creature [%1s] at %i:%i:%i x:y:z has a non-valid sound_modifiers_controller after a full rebuild! dirty inmods/outmods %i:%i, remove marked inmods/outmods %i:%i "
                                    , parent->get_name(), loc.x(), loc.y(), loc.z(), status[0], status[1], status[4], status[5] );
        }
    }
    return false;
}

bool sound_modifiers_controller::active_in( const bool &only_global ) {
    if ( ready() && !only_global) {
        return status[2];
    } else {

        auto &in_global = inmods.global_mods;
        auto &in_conditional = inmods.conditional_mods;

        auto is_active = [&]( sound_modifier_base &inst ) -> bool {
            const auto &cv = inst.status;
            return !cv[0] && cv[6] && cv[7];
        };

        // Just loop through our 4 vectors and mark instances.
        for( auto &inst : in_global ) {
            if ( is_active( inst ) ){
                return true;
            }
        }
        if ( !only_global ){
            for( auto &inst : in_conditional ) {
                if ( is_active( inst ) ){
                    return true;
                }
            }
        }
        return false;
    }
}

bool sound_modifiers_controller::active_out( const bool &only_global ) {
    if ( ready() && only_global) {
        return status[3];
    } else {

        auto &out_global = outmods.global_mods;
        auto &out_conditional = outmods.conditional_mods;

        auto is_active = [&]( sound_modifier_base &inst ) -> bool {
            const auto &cv = inst.status;
            return !cv[0] && cv[6] && cv[7];
        };

        // Just loop through our 4 vectors and mark instances.
        for( auto &inst : out_global ) {
            if ( is_active( inst ) ){
                return true;
            }
        }
        if ( !only_global ){
            for( auto &inst : out_conditional ) {
                if ( is_active( inst ) ){
                    return true;
                }
            }
        }
        return false;
    }
}

bool sound_modifiers_controller::active_in( const sound_event &se, const bool &allow_global ) {
    if ( ready() && allow_global) {
        return status[2];
    } else {

        auto &in_global = inmods.global_mods;
        auto &in_conditional = inmods.conditional_mods;

        auto is_active = [&]( sound_modifier_base &inst ) -> bool {
            const auto &cv = inst.status;
            return !cv[0] && cv[6] && cv[7];
        };

        // Just loop through our 4 vectors and mark instances.
        if ( allow_global ) {
            for( auto &inst : in_global ) {
            if ( is_active( inst ) ){
                return true;
            }
        }}

        for( auto &inst : in_conditional ) {
            if ( is_active( inst ) && inst.can_modify( se ) ) {
                return true;
            }
        }
        return false;
    }
}

bool sound_modifiers_controller::active_out( const sound_event &se, const bool &allow_global ) {
    if ( ready() && allow_global) {
        return status[3];
    } else {

        auto &out_global = outmods.global_mods;
        auto &out_conditional = outmods.conditional_mods;

        auto is_active = [&]( sound_modifier_base &inst ) -> bool {
            const auto &cv = inst.status;
            return !cv[0] && cv[6] && cv[7];
        };

        // Just loop through our 4 vectors and mark instances.
        if ( allow_global ) {
            for( auto &inst : out_global ) {
                if ( is_active( inst ) ){
                    return true;
                }
            }
        }
        for( auto &inst : out_conditional ) {
            if ( is_active( inst ) && inst.can_modify( se ) ){
                return true;
            }
        }
        return false;
    }
}

bool sound_modifiers_controller::active() {
    
    if ( ready() ) {
        return status[2] || status[3];
    } 
    return active_in() || active_out();
}

void sound_modifiers_controller::mark_all_dirty() {
    status.set(0);
    status.set(1);

    auto &in_global = inmods.global_mods;
    auto &in_conditional = inmods.conditional_mods;
    auto &out_global = outmods.global_mods;
    auto &out_conditional = outmods.conditional_mods;    

    // Just loop through our 4 vectors and mark instances.
    for( auto &inst : in_global ) {
        inst.status.reset(0);
    }
    for( auto &inst : in_conditional ) {
        inst.status.reset(0);
    }
    for( auto &inst : out_global ) {
        inst.status.reset(0);
    }
    for( auto &inst : out_conditional ) {
        inst.status.reset(0);
    }
}

void sound_modifiers_controller::mark_dirty( const efftype_id &id ) {
    bool indirty = status[0];
    bool outdirty = status[1];

    auto &in_global = inmods.global_mods;
    auto &in_conditional = inmods.conditional_mods;
    auto &out_global = outmods.global_mods;
    auto &out_conditional = outmods.conditional_mods;    

    auto mark_dirty = [&]( sound_modifier_base &inst ) -> bool {
        if ( inst.parent_id == id ){
            inst.status.reset(0);
            return true;
        } else {
            return false;
        }
    };
    // Just loop through our 4 vectors and mark instances.
    for( auto &inst : in_global ) {
        if( mark_dirty(inst) ) {
            indirty = true;
        }
    }
    for( auto &inst : in_conditional ) {
        if( mark_dirty(inst) ) {
            indirty = true;
        }
    }
    for( auto &inst : out_global ) {
        if( mark_dirty(inst) ) {
            outdirty = true;
        }
    }
    for( auto &inst : out_conditional ) {
        if( mark_dirty(inst) ) {
            outdirty = true;
        }
    }
    status[0] = indirty; 
    status[1] = outdirty; 
}

void sound_modifiers_controller::mark_for_removal( const efftype_id &id ) {
    bool indirty = status[0];
    bool outdirty = status[1];
    bool inrmarked = status[4];
    bool outrmarked = status[5];

    auto &in_global = inmods.global_mods;
    auto &in_conditional = inmods.conditional_mods;
    auto &out_global = outmods.global_mods;
    auto &out_conditional = outmods.conditional_mods;    

    auto mark_remove = [&]( sound_modifier_base &inst ) -> bool {
        if ( inst.parent_id == id ){
            inst.filter.set();
            return true;
        } else {
            return false;
        }
    };
    // Just loop through our 4 vectors and mark instanced.
    for( auto &inst : in_global ) {
        if( mark_remove(inst) ) {
            indirty = true;
            inrmarked = true;
        }
    }
    for( auto &inst : in_conditional ) {
        if( mark_remove(inst) ) {
            indirty = true;
            inrmarked = true;
        }
    }
    for( auto &inst : out_global ) {
        if( mark_remove(inst) ) {
            outdirty = true;
            outrmarked = true;
        }
    }
    for( auto &inst : out_conditional ) {
        if( mark_remove(inst) ) {
            outdirty = true;
            outrmarked = true;
        }
    }

    status[0] = indirty; 
    status[1] = outdirty; 
    status[4] = inrmarked; 
    status[5] = outrmarked; 
    // Now remove all the sounds we marked.
    remove_marked();
}

void sound_modifiers_controller::remove_marked() {
    bool inrmarked = status[4];
    bool outrmarked = status[5];

    auto &in_global = inmods.global_mods;
    auto &in_conditional = inmods.conditional_mods;
    auto &out_global = outmods.global_mods;
    auto &out_conditional = outmods.conditional_mods;   

    do {
        std::erase_if( in_global, []( const auto & inst ) {
            return inst.marked_for_removal();
        } );
        std::erase_if( in_conditional, []( const auto & inst ) {
            return inst.marked_for_removal();
        } );
        bool found_marked = false;
        for( const auto &inst : in_global ){
            found_marked = found_marked || inst.marked_for_removal();
        }
        for( const auto &inst : in_conditional ){
            found_marked = found_marked || inst.marked_for_removal();
        }
        inrmarked = found_marked;
    } while ( inrmarked );

    do {
        std::erase_if( out_global, []( const auto & inst ) {
            return inst.marked_for_removal();
        } );
        std::erase_if( out_conditional, []( const auto & inst ) {
            return inst.marked_for_removal();
        } );
        bool found_marked = false;
        for( const auto &inst : out_global ){
            found_marked = found_marked || inst.marked_for_removal();
        }
        for( const auto &inst : out_conditional ){
            found_marked = found_marked || inst.marked_for_removal();
        }
        outrmarked = found_marked;
    } while ( outrmarked );
    
    status[2] = !in_global.empty() || !in_conditional.empty();
    status[3] = !out_global.empty() || !out_conditional.empty();  
    status[4] = inrmarked;  
    status[5] = outrmarked; 
}
// will update modifier details that change with intensity. 
void sound_modifiers_controller::update( const bool &dirty_only ) {
    
    if ( !dirty_only ) {
        mark_all_dirty();
    } 
    if ( status[4] || status[5] ) {
        // Remove the marked before we really get going so we can still properly update.
        remove_marked();
    }
    auto &in_global = inmods.global_mods;
    auto &in_conditional = inmods.conditional_mods;
    auto &out_global = outmods.global_mods;
    auto &out_conditional = outmods.conditional_mods;

    // Dont try to update if all the sound modifier vectors are empty. 
    // Clear the two checkvars and set the status bitset.
    if ( in_global.empty() && in_conditional.empty() && out_global.empty() && out_conditional.empty() ) {
        inmods.checkvars = incoming_sound_modifier_instance();
        outmods.checkvars = outgoing_sound_modifier_instance();
        inmods.previous_sound.clear();
        outmods.previous_sound.clear();
        status.reset(0);
        status.reset(1);
        status.reset(2);
        status.reset(3);
        status.reset(4);
        status.reset(5);
        status.reset(6);
        return;
    }
    
    auto short_vector_entry = [&]( const std::vector<short> &vect, const short &min, const short &max, const float &scaling, const int &adj_intensity ) -> short {
        if ( vect.empty() ) {
            return 0;
        }
        short ret = 0;
        if ( vect.size() == 1 ) {
            ret = vect[0];
        } else {
            const auto &iterator = std::max<size_t>( 0, (std::min<size_t>( adj_intensity, vect.size() ) - 1 ) );
            ret = vect[iterator];
        }
        ret += std::round<short>( scaling * ( adj_intensity - 1 ) );
        if ( min != 0 ) {
            ret = std::max<short>( min, ret );
        }
        if ( max != 0 ) {
            ret = std::min<short>( max, ret );
        }
        return ret;
    };
    
    auto calc_incoming_mods = [&]( incoming_sound_modifier_instance *mod, const effect &e ) -> void {
        const heard_sound_modifiers &sourcemods = e.get_heard_sound_mods()[mod->modifier_index];
        const auto &intensity = e.get_intensity();
        const bool active = intensity >= mod->intensity_minreq;
        // Dont go through all the calcs if the modifier is not active.
        if (active) {

            const int adj_intensity = ( mod->intensity_minreq > 1 ) ? intensity - ( mod->intensity_minreq - 1 ) : intensity;
            if ( !sourcemods.replace_with_sound_descriptions.empty() ){

                // clear our mod instances replace sound descriptions with vector.
                mod->replace_with_sound_descriptions.clear();

                if ( sourcemods.replace_with_sound_descriptions.size() == 1){
                    mod->replace_with_sound_descriptions.push_back( sourcemods.replace_with_sound_descriptions[0] );

                } else if ( sourcemods.choose_random_desc ){
                    // If its random, dump all of the descriptions in.
                    mod->replace_with_sound_descriptions = sourcemods.replace_with_sound_descriptions;

                } else {
                    const auto &iterator = std::max<size_t>( 0, (std::min<size_t>( adj_intensity, sourcemods.replace_with_sound_descriptions.size() ) - 1 ) );
                    mod->replace_with_sound_descriptions.push_back( sourcemods.replace_with_sound_descriptions[iterator] );

                }
            }
            // behold the short vector entry army
            mod->base_mdb_adj = short_vector_entry( sourcemods.base_mdb_adj, sourcemods.base_mdb_adj_min_val, sourcemods.base_mdb_adj_max_val, sourcemods.base_mdb_adj_intensity_scaling, adj_intensity);

            mod->heard_vol_mdb_adj = short_vector_entry( sourcemods.heard_vol_mdb_adj, sourcemods.heard_vol_mdb_adj_min_val, sourcemods.heard_vol_mdb_adj_max_val, sourcemods.heard_vol_mdb_adj_intensity_scaling, adj_intensity);

            mod->perceived_vol_mdb_adj = short_vector_entry( sourcemods.perceived_vol_mdb_adj, sourcemods.perceived_vol_mdb_adj_min_val, sourcemods.perceived_vol_mdb_adj_max_val, sourcemods.perceived_vol_mdb_adj_intensity_scaling, adj_intensity);

            mod->hearing_threshold_mdb_adj = short_vector_entry( sourcemods.hearing_threshold_mdb_adj, sourcemods.hearing_threshold_mdb_adj_min_val, sourcemods.hearing_threshold_mdb_adj_max_val, sourcemods.hearing_threshold_mdb_adj_intensity_scaling, adj_intensity);

            mod->hearing_protection_mdb_adj = short_vector_entry( sourcemods.hearing_protection_mdb_adj, sourcemods.hearing_protection_mdb_adj_min_val, sourcemods.hearing_protection_mdb_adj_max_val, sourcemods.hearing_protection_mdb_adj_intensity_scaling, adj_intensity);

            mod->hearing_protection_adv_mdb_adj = short_vector_entry( sourcemods.hearing_protection_adv_mdb_adj, sourcemods.hearing_protection_adv_mdb_adj_min_val, sourcemods.hearing_protection_adv_mdb_adj_max_val, sourcemods.hearing_protection_adv_mdb_adj_intensity_scaling, adj_intensity);

            mod->permanant_hearing_loss_mdb_adj = short_vector_entry( sourcemods.permanant_hearing_loss_mdb_adj, sourcemods.permanant_hearing_loss_mdb_adj_min_val, sourcemods.permanant_hearing_loss_mdb_adj_max_val, sourcemods.permanant_hearing_loss_mdb_adj_intensity_scaling, adj_intensity);
        
        } else {
            // If we were not loud enough, mark the modifier as not active
            mod->status.reset(6);
        }  
        // Last but not least, update the status and filter bitsets.
        mod->status[0] = false;        
        mod->status[1] = mod->intensity_minreq > 1;        
        mod->status[2] = false;    
        mod->status[3] = mod->hearing_protection_mdb_adj != 0;        
        mod->status[4] = mod->hearing_protection_adv_mdb_adj != 0;        
        mod->status[5] = mod->permanant_hearing_loss_mdb_adj != 0;  
        mod->status[6] = true;       
        mod->status[7] = true;        
        mod->filter[0] = mod->checked_categories.empty();        
        mod->filter[1] = mod->checked_sound_descriptions.empty();        
        mod->filter[2] = !mod->replace_with_sound_descriptions.empty();        
        mod->filter[3] = mod->replace_with_sound_descriptions.size() == 1;        
        mod->filter[4] = !mod->npc_faction.empty();       
        mod->filter[5] = mod->npc_faction.size() == 1;        
        mod->filter[6] = !mod->monfaction.empty();      
        mod->filter[7] = mod->monfaction.size() == 1;        

    };

    auto calc_outgoing_mods = [&]( outgoing_sound_modifier_instance *mod, const effect &e ) -> void {
        const outgoing_sound_modifiers &sourcemods = e.get_outgoing_sound_mods()[mod->modifier_index];
        const auto &intensity = e.get_intensity();
        const bool active = intensity >= mod->intensity_minreq;

        if ( active ) { 

            const int adj_intensity = ( mod->intensity_minreq > 1 ) ? intensity - ( mod->intensity_minreq - 1 ) : intensity;
            if ( !sourcemods.replace_with_sound_descriptions.empty() ){

                // clear our mod instances replace sound descriptions with vector.
                mod->replace_with_sound_descriptions.clear();

                if ( sourcemods.replace_with_sound_descriptions.size() == 1){
                    mod->replace_with_sound_descriptions.push_back( sourcemods.replace_with_sound_descriptions[0] );

                } else if ( sourcemods.choose_random_desc ){
                    // If its random, dump all of the descriptions in.
                    mod->replace_with_sound_descriptions = sourcemods.replace_with_sound_descriptions;

                } else {
                    const auto &iterator = std::max<size_t>( 0, (std::min<size_t>( adj_intensity, sourcemods.replace_with_sound_descriptions.size() ) - 1 ) );
                    mod->replace_with_sound_descriptions.push_back( sourcemods.replace_with_sound_descriptions[iterator] );

                }
            }

            mod->volume_mdB_adj = short_vector_entry(sourcemods.volume_mdB_adj, sourcemods.volume_mdB_adj_min_val, sourcemods.volume_mdB_adj_max_val, sourcemods.volume_mdB_adj_intensity_scaling, adj_intensity );

            mod->volume_mdB_floor = short_vector_entry(sourcemods.volume_mdB_floor, sourcemods.volume_mdB_floor_min_val, sourcemods.volume_mdB_floor_max_val, sourcemods.volume_mdB_floor_intensity_scaling, adj_intensity );

            mod->volume_mdB_ceiling = short_vector_entry(sourcemods.volume_mdB_ceiling, sourcemods.volume_mdB_ceiling_min_val, sourcemods.volume_mdB_ceiling_max_val, sourcemods.volume_mdB_ceiling_intensity_scaling, adj_intensity );
        } else {
            // If we were not loud enough, mark the modifier as not active
            mod->status.reset(6);
        } 
        // Last but not least, update the status and filter bitsets.
        mod->status[0] = false;        
        mod->status[1] = mod->intensity_minreq > 1;        
        mod->status[2] = true;    
        mod->status[3] = false;        
        mod->status[4] = false;        
        mod->status[5] = false;
        mod->status[6] = true;       
        mod->status[7] = true;        
        mod->filter[0] = mod->checked_categories.empty();        
        mod->filter[1] = mod->checked_sound_descriptions.empty();        
        mod->filter[2] = !mod->replace_with_sound_descriptions.empty();        
        mod->filter[3] = mod->replace_with_sound_descriptions.size() == 1;        
        mod->filter[4] = !mod->npc_faction.empty();       
        mod->filter[5] = mod->npc_faction.size() == 1;        
        mod->filter[6] = !mod->monfaction.empty();      
        mod->filter[7] = mod->monfaction.size() == 1;       

    };
    
    auto update_modifier = [&]( sound_modifier_base *mod ) -> void {
        // Take our easy outs.
        if ( dirty_only && !mod->status[0]) {
            return;
        } else if ( !parent->has_effect(mod->parent_id) ){
            mod->marked_for_removal();
            return;
        }
        // const bool outgoing = mod->is_outgoing();
        const effect &e = parent->get_effect(mod->parent_id);
        
        if ( mod->is_incoming() ) {
            incoming_sound_modifier_instance *inmod = mod->as_incoming();
            calc_incoming_mods( inmod, e );
            return;
        } else {
            outgoing_sound_modifier_instance *outmod = mod->as_outgoing();
            calc_outgoing_mods( outmod, e );
            return;
        }
    };

    for( incoming_sound_modifier_instance &inst : in_global ) {
        auto *mod = inst.as_incoming();
        update_modifier(mod);
    }
    for( incoming_sound_modifier_instance &inst : in_conditional ) {
        auto *mod = inst.as_incoming();
        update_modifier(mod);
    }
    for( outgoing_sound_modifier_instance &inst : out_global ) {
        auto *mod = inst.as_outgoing();
        update_modifier(mod);
    }
    for( outgoing_sound_modifier_instance &inst : out_conditional ) {
        auto *mod = inst.as_outgoing();
        update_modifier(mod);
    }
    // We can now set the controller's in/outdirty to false.
    status.reset(0);
    status.reset(1);
}
// Toss it all out and rebuild it.
// Returns true if successful, false if we had to abort the rebuild.
bool sound_modifiers_controller::rebuild() {

    // Dont actually do a rebuild if we are not marked for rebuild
    // Dont consider an abort
    if ( !status[6] ) {
        return true;
    }
    
    // First we need to clear everything.
    auto &in_global = inmods.global_mods;
    auto &in_conditional = inmods.conditional_mods;
    auto &out_global = outmods.global_mods;
    auto &out_conditional = outmods.conditional_mods;
    auto &in_checkvars = inmods.checkvars;
    auto &out_checkvars = outmods.checkvars;

    // Reset the checkvars
    in_checkvars = incoming_sound_modifier_instance();
    out_checkvars = outgoing_sound_modifier_instance();

    in_global.clear();
    in_conditional.clear();
    out_global.clear();
    out_conditional.clear();
    inmods.previous_sound.clear();
    outmods.previous_sound.clear();

    // Reset the controllers status bitset
    status[0] = true;
    status[1] = true;
    status[2] = false;
    status[3] = false;
    status[4] = false;
    status[5] = false;
    status[6] = false;

    // Also dont rebuild if we are not actually bound to a creature. 
    // We still want to clear everything if we are not bound to prevent potential wierdness.
    if ( parent == nullptr || !status[7] ) {
        // Quick paranoia check
        if ( parent == nullptr && status[7] ) {
            debugmsg( "A sound modifiers controller is marked as valid but was found to have a null parent pointer on rebuild! Stack trace may reveal the owning creature." );
            status.reset(7);   
            return false;
        } else if ( parent != nullptr  && !status[7] ) {
            const auto &loc = parent->bub_pos();
            debugmsg( "Creature [%1s] at %i:%i:%i x:y:z has an inproperly initialized sound modifiers controller that attempted a rebuild."
                                  , parent->get_name(), loc.x(), loc.y(), loc.z() );
            return false;
        }
        return false;
    }
    for( const auto &elem : parent->get_all_effects() ) {
        for( const auto &_it : elem.second ) {
            const auto &e = _it.second;
            if( !e.is_removed() && (e.has_flag( flag_EFFECT_MODIFIES_OUTGOING_SOUND ) || e.has_flag( flag_EFFECT_MODIFIES_INCOMING_SOUND ) ) ) {
                add_mods( e );
            }
        }
    }

    if ( !in_global.empty() && !in_conditional.empty() && !out_global.empty() && !out_conditional.empty() ) {
        update( false );
    }
    status.set(2, ( !in_global.empty() || !in_conditional.empty() ) );
    status.set(3, ( !out_global.empty() || !out_conditional.empty() ) );
    return true;
}

// We dont fully initialize the modifiers when they are added,
// We put in the details that dont change with intensity and mark them dirty.
void sound_modifiers_controller::add_mods( const effect &e ) {

    auto &in_global = inmods.global_mods;
    auto &in_conditional = inmods.conditional_mods;
    auto &out_global = outmods.global_mods;
    auto &out_conditional = outmods.conditional_mods;

    auto not_duplicate = [&]( const efftype_id &id, const uint8_t &mindex, const bool &outgoing ) -> bool {
        // Check to see if we already have a modifier with the same ID and vector index in the appropriate holder.
        if ( !outgoing ) {
            for( const auto &mod : in_global ) {
                if ( mod.parent_id == id && mod.modifier_index == mindex ){
                    return false;
                }
            }
            for( const auto &mod : in_conditional ) {
                if ( mod.parent_id == id && mod.modifier_index == mindex ){
                    return false;
                }
            }
        } else {
            for( const auto &mod : out_global ) {
                if ( mod.parent_id == id && mod.modifier_index == mindex ){
                    return false;
                }
            }
            for( const auto &mod : out_conditional ) {
                if ( mod.parent_id == id && mod.modifier_index == mindex ){
                    return false;
                }
            }
        }
        return true;
    };
    
    auto create_in_mod = [&]( const heard_sound_modifiers &sourcemod, const uint8_t &mindex ) -> void {
        incoming_sound_modifier_instance mod;
        mod.parent_id = e.get_id();
        mod.modifier_index = mindex;
        mod.intensity_minreq = sourcemod.intensity_min_requirment;
        mod.checked_categories = sourcemod.checked_categories;
        mod.checked_sound_descriptions = sourcemod.checked_sound_descriptions;
        if ( sourcemod.replace_npc_faction_attribution ){
            mod.npc_faction.push_back( sourcemod.replace_with_npc_faction );
        }
        if ( sourcemod.replace_monster_faction_attribution ){
            mod.monfaction.push_back( sourcemod.replace_with_monfaction );
        }
        if ( sourcemod.choose_random_desc ){
            mod.replace_with_sound_descriptions = sourcemod.replace_with_sound_descriptions;
        }
        mod.status[0] = true; //dirty        
        mod.status[1] = mod.intensity_minreq > 1;        
        mod.status[2] = false;         
        mod.status[7] = true; // valid     
        mod.filter[0] = mod.checked_categories.empty();        
        mod.filter[1] = mod.checked_sound_descriptions.empty();        
        mod.filter[2] = !mod.replace_with_sound_descriptions.empty();        
        mod.filter[3] = mod.replace_with_sound_descriptions.size() == 1;        
        mod.filter[4] = !mod.npc_faction.empty();       
        mod.filter[5] = mod.npc_faction.size() == 1;        
        mod.filter[6] = !mod.monfaction.empty();      
        mod.filter[7] = mod.monfaction.size() == 1;    

         if ( mod.filter[0] && mod.filter[1] ){
            in_global.push_back( mod );
        } else {
            in_conditional.push_back( mod );
        }
        // Mark that we have dirty incoming sound modifiers
        status[0] = true;
    };

    auto create_out_mod = [&]( const outgoing_sound_modifiers &sourcemod, const uint8_t &mindex ) -> void {
        outgoing_sound_modifier_instance mod;
        mod.parent_id = e.get_id();
        mod.modifier_index = mindex;
        mod.intensity_minreq = sourcemod.intensity_min_requirment;
        mod.checked_categories = sourcemod.checked_categories;
        mod.checked_sound_descriptions = sourcemod.checked_sound_descriptions;
        if ( sourcemod.replace_npc_faction_attribution ){
            mod.npc_faction.push_back( sourcemod.replace_with_npc_faction );
        }
        if ( sourcemod.replace_monster_faction_attribution ){
            mod.monfaction.push_back( sourcemod.replace_with_monfaction );
        }
        if ( sourcemod.choose_random_desc ){
            mod.replace_with_sound_descriptions = sourcemod.replace_with_sound_descriptions;
        }
        mod.status[0] = true; //dirty        
        mod.status[1] = mod.intensity_minreq > 1;        
        mod.status[2] = true;         
        mod.status[7] = true; // valid     
        mod.filter[0] = mod.checked_categories.empty();        
        mod.filter[1] = mod.checked_sound_descriptions.empty();        
        mod.filter[2] = !mod.replace_with_sound_descriptions.empty();        
        mod.filter[3] = mod.replace_with_sound_descriptions.size() == 1;        
        mod.filter[4] = !mod.npc_faction.empty();       
        mod.filter[5] = mod.npc_faction.size() == 1;        
        mod.filter[6] = !mod.monfaction.empty();      
        mod.filter[7] = mod.monfaction.size() == 1;    

        if ( mod.filter[0] && mod.filter[1] ){
            out_global.push_back( mod );
        } else {
            out_conditional.push_back( mod );
        }
        // Mark that we have dirty outgoing sound modifiers
        status[1] = true;
    };
    // Just step through the two possible vectors. 
    if ( e.has_heard_sound_mods() ){
        uint8_t length = e.get_heard_sound_mods().size();
        for( uint8_t i = 0; i < length; i++ ){
            if (not_duplicate(e.get_id(), i, false ) ) {
                const auto &sourcemods = e.get_heard_sound_mods()[i];
                create_in_mod( sourcemods, i );
            } 
        }
    }

    if ( e.has_outgoing_sound_mods() ){
        uint8_t length = e.get_outgoing_sound_mods().size();
        for( uint8_t i = 0; i < length; i++ ){
            if (not_duplicate(e.get_id(), i, true ) ) {
                const auto &sourcemods = e.get_outgoing_sound_mods()[i];
                create_out_mod( sourcemods, i );
            }
        }
    }       
}

void sound_modifiers_controller::config_in_checkvars( const sound_event &se ) {
    auto &cv = inmods.checkvars;
    // We only actually check the first sound.
    const auto &in_global = inmods.global_mods;
    const auto &in_conditional = inmods.conditional_mods;

    auto shortvect_sum = [&]( const std::vector<short> &vect ) -> short {
        int ret = 0;
        for( const auto &val : vect ) {
                ret += val;
        }
        ret = std::min<int>( MAXIMUM_VOLUME_ATMOSPHERE, std::max( -MAXIMUM_VOLUME_ATMOSPHERE, ret ) );
        return static_cast<short>(ret);
    };

    // We can garuntee that all of our modifiers are up to date by checking against ready.
    if ( ready() && status[2] ) {

        const bool global_only = in_conditional.empty();
        const bool cv_dirty = !cv.status[7];
        const bool has_oldse = !inmods.previous_sound.empty();

        // Always recalc if our checkvars are dirty
        if ( cv_dirty || !global_only ){
            bool equivalent = !cv_dirty && has_oldse;
            
            if ( !global_only && equivalent){
                const auto &prevse = inmods.previous_sound[0];
                for ( const auto &mod : in_conditional ) {
                    if ( mod.can_modify(se) != mod.can_modify(prevse) ) {
                        equivalent = false;
                    }
                }
            }
            if ( !equivalent ) {
                // If we only have one sound modifier, just grab that.
                if ( in_conditional.size() + in_global.size() == 1 ) {
                    if ( in_conditional.empty() ) {
                        cv = in_global[0];
                    } else {
                        cv = in_conditional[0];
                    }
                    inmods.previous_sound.clear();
                    inmods.previous_sound.push_back( se );
                    return;
                }

                // Only recalc if our sound is not equivalent.
                cv = incoming_sound_modifier_instance();
                dummy_incoming_sound_modifier_sums sum;
                
                auto proccess_mod = [&]( const incoming_sound_modifier_instance &mod ) -> void {

                    const auto &mf = mod.filter;
                    // Check if our modifier is active and if it can affect the sound event.
                    // can_modify already tests if the modifier is active.
                    if ( mod.can_modify( se ) ) {
                        if ( mf.test( 2 ) ) {
                            for ( const auto &desc : mod.replace_with_sound_descriptions ) {
                                sum.sound_descriptions.push_back( desc );
                            }
                        }
                        if ( mf.test( 4 ) ) {
                            for ( const auto &nfac : mod.npc_faction ) {
                                sum.npc_faction.push_back( nfac );
                            }
                        }
                        if ( mf.test( 6 ) ) {
                            for ( const auto &mfac : mod.monfaction ) {
                                sum.monfaction.push_back( mfac );
                            }
                        }
                    }
                    sum.base_mdb_adj.push_back( mod.base_mdb_adj );
                    sum.heard_vol_mdb_adj.push_back( mod.heard_vol_mdb_adj );
                    sum.perceived_vol_mdb_adj.push_back( mod.perceived_vol_mdb_adj );
                    sum.hearing_threshold_mdb_adj.push_back( mod.hearing_threshold_mdb_adj );
                    sum.hearing_protection_mdb_adj.push_back( mod.hearing_protection_mdb_adj );
                    sum.hearing_protection_adv_mdb_adj.push_back( mod.hearing_protection_adv_mdb_adj );
                    sum.permanant_hearing_loss_mdb_adj.push_back( mod.permanant_hearing_loss_mdb_adj );
                };

                for ( const auto &mod : in_global ) {
                    proccess_mod( mod );
                }
                for ( const auto &mod : in_conditional ) {
                    proccess_mod( mod );
                }
                // Now refill the checkvars.
                if ( !sum.sound_descriptions.empty() ){
                    for ( const auto &val : sum.sound_descriptions ) {
                        cv.replace_with_sound_descriptions.push_back( val );
                    }
                }
                if ( !sum.npc_faction.empty() ){
                    for ( const auto &val : sum.npc_faction ) {
                        cv.npc_faction.push_back( val );
                    }
                }
                if ( !sum.monfaction.empty() ){
                    for ( const auto &val : sum.monfaction ) {
                        cv.monfaction.push_back( val );
                    }
                }
                cv.base_mdb_adj = shortvect_sum( sum.base_mdb_adj );
                cv.heard_vol_mdb_adj = shortvect_sum( sum.heard_vol_mdb_adj );
                cv.perceived_vol_mdb_adj = shortvect_sum( sum.perceived_vol_mdb_adj );
                cv.hearing_threshold_mdb_adj = shortvect_sum( sum.hearing_threshold_mdb_adj );
                cv.hearing_protection_mdb_adj = shortvect_sum( sum.hearing_protection_mdb_adj );
                cv.hearing_protection_adv_mdb_adj = shortvect_sum( sum.hearing_protection_adv_mdb_adj );
                cv.permanant_hearing_loss_mdb_adj = shortvect_sum( sum.permanant_hearing_loss_mdb_adj );

                cv.status.set(0 ); 
                cv.status.reset(1 ); 
                cv.status.reset(2 ); 
                cv.status.set(3, cv.hearing_protection_mdb_adj != 0 ); 
                cv.status.set(4, cv.hearing_protection_adv_mdb_adj != 0 ); 
                cv.status.set(5, cv.permanant_hearing_loss_mdb_adj != 0 ); 
                cv.status.set(6 ); 
                cv.status.set(7 ); 
                cv.filter.set(0 ); 
                cv.filter.set(1 ); 
                cv.filter.set(2, !cv.replace_with_sound_descriptions.empty() ); 
                cv.filter.set(3, cv.replace_with_sound_descriptions.size() == 1 ); 
                cv.filter.set(4, !cv.npc_faction.empty() ); 
                cv.filter.set(5, cv.npc_faction.size() == 1 ); 
                cv.filter.set(6, !cv.monfaction.empty() ); 
                cv.filter.set(7, cv.monfaction.size() == 1 ); 

                inmods.previous_sound.clear();
                inmods.previous_sound.push_back( se );
                
                return;
            }
        }
    } else { 
        cv = incoming_sound_modifier_instance();
        // Make sure no previous sounds are hanging around.
        // We have no previous sound if we have no modifiers.
        inmods.previous_sound.clear();
        // Set our checkvar's status and filter.
        cv.status.set(7);
        cv.status.reset(2);
        cv.filter.set(0);
        cv.filter.set(1);
    }
}

void sound_modifiers_controller::config_out_checkvars( const sound_event &se ) {
    auto &cv = outmods.checkvars;
    // We only actually check the first sound.
    const auto &out_global = outmods.global_mods;
    const auto &out_conditional = outmods.conditional_mods;

    auto shortvect_sum = [&]( const std::vector<short> &vect ) -> short {
        int ret = 0;
        for( const auto &val : vect ) {
                ret += val;
        }
        ret = std::min<int>( MAXIMUM_VOLUME_ATMOSPHERE, std::max( -MAXIMUM_VOLUME_ATMOSPHERE, ret ) );
        return static_cast<short>(ret);
    };

    // We can garuntee that all of our modifiers are up to date by checking against ready.
    if ( ready() && status[3] ) {

        const bool global_only = out_conditional.empty();
        const bool cv_dirty = !cv.status[7];
        const bool has_oldse = !outmods.previous_sound.empty();

        // Always recalc if our checkvars are dirty
        if ( cv_dirty || !global_only ){
            bool equivalent = !cv_dirty && has_oldse;
            
            if ( !global_only && equivalent){
                const auto &prevse = outmods.previous_sound[0];
                for ( const auto &mod : out_conditional ) {
                    if ( mod.can_modify(se) != mod.can_modify(prevse) ) {
                        equivalent = false;
                    }
                }
            }
            if ( !equivalent ) {
                // If we only have one sound modifier, just grab that.
                if ( out_conditional.size() + out_global.size() == 1 ) {
                    if ( out_conditional.empty() ) {
                        cv = out_global[0];
                    } else {
                        cv = out_conditional[0];
                    }
                    outmods.previous_sound.clear();
                    outmods.previous_sound.push_back( se );
                    return;
                }

                // Only recalc if our sound is not equivalent.
                cv = outgoing_sound_modifier_instance();
                dummy_outgoing_sound_modifier_sums sum;
                
                auto proccess_mod = [&]( const outgoing_sound_modifier_instance &mod ) -> void {

                    const auto &mf = mod.filter;
                    // Check if our modifier is active and if it can affect the sound event.
                    // can_modify already tests if the modifier is active.
                    if ( mod.can_modify( se ) ) {
                        if ( mf.test( 2 ) ) {
                            for ( const auto &desc : mod.replace_with_sound_descriptions ) {
                                sum.sound_descriptions.push_back( desc );
                            }
                        }
                        if ( mf.test( 4 ) ) {
                            for ( const auto &nfac : mod.npc_faction ) {
                                sum.npc_faction.push_back( nfac );
                            }
                        }
                        if ( mf.test( 6 ) ) {
                            for ( const auto &mfac : mod.monfaction ) {
                                sum.monfaction.push_back( mfac );
                            }
                        }
                    }
                    sum.volume_mdB_adj.push_back( mod.volume_mdB_adj );
                    sum.volume_mdB_floor.push_back( mod.volume_mdB_floor );
                    sum.volume_mdB_ceiling.push_back( mod.volume_mdB_ceiling );

                };

                for ( const auto &mod : out_global ) {
                    proccess_mod( mod );
                }
                for ( const auto &mod : out_conditional ) {
                    proccess_mod( mod );
                }
                // Now refill the checkvars.
                if ( !sum.sound_descriptions.empty() ){
                    for ( const auto &val : sum.sound_descriptions ) {
                        cv.replace_with_sound_descriptions.push_back( val );
                    }
                }
                if ( !sum.npc_faction.empty() ){
                    for ( const auto &val : sum.npc_faction ) {
                        cv.npc_faction.push_back( val );
                    }
                }
                if ( !sum.monfaction.empty() ){
                    for ( const auto &val : sum.monfaction ) {
                        cv.monfaction.push_back( val );
                    }
                }
                cv.volume_mdB_adj = shortvect_sum( sum.volume_mdB_adj );
                cv.volume_mdB_floor = shortvect_sum( sum.volume_mdB_floor );
                cv.volume_mdB_ceiling = shortvect_sum( sum.volume_mdB_ceiling );

                cv.status.set(0 ); 
                cv.status.reset(1 ); 
                cv.status.set(2 ); 
                cv.status.reset(3); 
                cv.status.reset(4); 
                cv.status.reset(5); 
                cv.status.set(6 ); 
                cv.status.set(7 ); 

                cv.filter.set(0 ); 
                cv.filter.set(1 ); 
                cv.filter.set(2, !cv.replace_with_sound_descriptions.empty() ); 
                cv.filter.set(3, cv.replace_with_sound_descriptions.size() == 1 ); 
                cv.filter.set(4, !cv.npc_faction.empty() ); 
                cv.filter.set(5, cv.npc_faction.size() == 1 ); 
                cv.filter.set(6, !cv.monfaction.empty() ); 
                cv.filter.set(7, cv.monfaction.size() == 1 ); 

                outmods.previous_sound.clear();
                outmods.previous_sound.push_back( se );

                return;
            }
        }
    } else { 
        cv = outgoing_sound_modifier_instance();
        // Make sure no previous sounds are hanging around.
        // We have no previous sound if we have no modifiers.
        outmods.previous_sound.clear();
        // Set our checkvar's status and filter.
        cv.status.set(7);
        cv.status.reset(2);
        cv.filter.set(0);
        cv.filter.set(1);
    }
}

short sound_modifiers_controller::get_permanant_hearing_loss() {
    int ret = 0;
    if ( ready() && status[2] ){
        const auto &in_global = inmods.global_mods;
        const auto &in_conditional = inmods.conditional_mods;
        for( const auto &mod : in_global ) {
            if ( mod.status[5] && mod.status[6] ){
                ret += mod.permanant_hearing_loss_mdb_adj;
            }
        }
        for( const auto &mod : in_conditional ) {
            if ( mod.status[5] && mod.status[6] ){
                ret += mod.permanant_hearing_loss_mdb_adj;
            }
        }
    }
    ret = std::min<int>( MAXIMUM_VOLUME_ATMOSPHERE, std::max( -MAXIMUM_VOLUME_ATMOSPHERE, ret ) );
    return static_cast<short>(ret);
}



