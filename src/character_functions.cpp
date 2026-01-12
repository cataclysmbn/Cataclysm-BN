#include "character_functions.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ammo.h"
#include "bionics.h"
#include "bodypart.h"
#include "calendar.h"
#include "character_martial_arts.h"
#include "character.h"
#include "creature.h"
#include "flag.h"
#include "game.h"
#include "handle_liquid.h"
#include "itype.h"
#include "iuse_actor.h"
#include "make_static.h"
#include "map_iterator.h"
#include "map_selector.h"
#include "messages.h"
#include "monster.h"
#include "npc.h"
#include "output.h"
#include "player.h"
#include "ranged.h"
#include "rng.h"
#include "skill.h"
#include "submap.h"
#include "trap.h"
#include "flag_trait.h"
#include "uistate.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vehicle_selector.h"
#include "vpart_position.h"
#include "weather_gen.h"
#include "weather.h"

static const trait_id trait_CHLOROMORPH( "CHLOROMORPH" );
static const trait_id trait_DEBUG_NODMG( "DEBUG_NODMG" );
static const trait_id trait_EASYSLEEPER( "EASYSLEEPER" );
static const trait_id trait_EASYSLEEPER2( "EASYSLEEPER2" );
static const trait_id trait_INSOMNIA( "INSOMNIA" );
static const trait_id trait_LOVES_BOOKS( "LOVES_BOOKS" );
static const trait_id trait_M_SKIN3( "M_SKIN3" );
static const trait_id trait_NOPAIN( "NOPAIN" );
static const trait_id trait_PER_SLIME_OK( "PER_SLIME_OK" );
static const trait_id trait_SHELL2( "SHELL2" );
static const trait_id trait_STRONGBACK( "STRONGBACK" );
static const trait_id trait_BADBACK( "BADBACK" );
static const trait_id trait_THRESH_SPIDER( "THRESH_SPIDER" );
static const trait_id trait_WATERSLEEP( "WATERSLEEP" );
static const trait_id trait_WEB_SPINNER( "WEB_SPINNER" );
static const trait_id trait_WEB_WALKER( "WEB_WALKER" );
static const trait_id trait_WEB_WEAVER( "WEB_WEAVER" );

static const trait_flag_str_id trait_flag_CANNIBAL( "CANNIBAL" );
static const trait_flag_str_id trait_flag_PSYCHOPATH( "PSYCHOPATH" );
static const trait_flag_str_id trait_flag_SAPIOVORE( "SAPIOVORE" );
static const trait_flag_str_id trait_flag_SPIRITUAL( "SPIRITUAL" );

static const trait_flag_str_id trait_flag_MUTATION_FLIGHT( "MUTATION_FLIGHT" );
static const trait_flag_str_id trait_flag_FLIGHT_ALWAYS_ACTIVE( "FLIGHT_ALWAYS_ACTIVE" );

const flag_id flag_ALLOWS_FLIGHT( "ALLOWS_FLIGHT" );
const flag_id flag_ALWAYS_ALLOWS_FLIGHT( "ALWAYS_ALLOWS_FLIGHT" );

static const efftype_id effect_boomered( "boomered" );
static const efftype_id effect_darkness( "darkness" );
static const efftype_id effect_meth( "meth" );

static const bionic_id bio_soporific( "bio_soporific" );
static const bionic_id bio_uncanny_dodge( "bio_uncanny_dodge" );

static const itype_id itype_battery( "battery" );
static const itype_id itype_UPS( "UPS" );

static const skill_id skill_throw( "throw" );

static const quality_id qual_SLEEP_AID( "SLEEP_AID" );

namespace character_funcs
{

time_duration estimate_effect_dur( int skill_lvl, const efftype_id &target_effect,
                                   const time_duration &error_magnitude, int threshold, const Creature &target )
{
    const time_duration zero_duration = 0_turns;

    time_duration estimate = std::max( zero_duration, target.get_effect_dur( target_effect ) +
                                       rng( -1, 1 ) * error_magnitude *
                                       rng( 0, std::max( 0, threshold - skill_lvl ) ) );
    return estimate;
}

void siphon( Character &ch, vehicle &veh, const itype_id &desired_liquid )
{
    if( !ch.is_avatar() ) {
        // TODO: implement for NPCs
        debugmsg( "Siphoning not implemented for NPCs." );
        return;
    }
    auto qty = veh.fuel_left( desired_liquid );
    if( qty <= 0 ) {
        ch.add_msg_if_player( m_bad, _( "There is not enough %s left to siphon it." ),
                              item::nname( desired_liquid ) );
        return;
    }

    detached_ptr<item> liquid = item::spawn( desired_liquid, calendar::turn, qty );
    liquid_handler::handle_liquid( std::move( liquid ) );
    // NOLINTNEXTLINE(bugprone-use-after-move)
    if( liquid ) {
        veh.drain( desired_liquid, qty - liquid->charges );
    } else {
        veh.drain( desired_liquid, qty );
    }
}

bool can_noclip( const Character &ch )
{
    return ch.has_trait( trait_id( "DEBUG_NOCLIP" ) );
}

bool can_fly( Character &ch )
{

    // if the player can noclip, flying is technically a part of that
    if( can_noclip( ch ) ) {
        return true;
    }

    for( const auto &w : ch.worn ) {
        if( ( w->is_active() && w->has_flag( flag_ALLOWS_FLIGHT ) ) ||
            w->has_flag( flag_ALWAYS_ALLOWS_FLIGHT ) ) {
            return true;
        }
    }

    Creature *mc = ch.mounted_creature.get();
    if( mc && mc->has_flag( MF_FLIES ) ) {
        return true;
    }

    for( const trait_id &mid : ch.get_mutations() ) {
        auto it = ch.my_mutations.find( mid->id );
        if( it != ch.my_mutations.end() ) {
            if( ( mid->flags.contains( trait_flag_MUTATION_FLIGHT ) && ( can_use_mutation( mid, ch ) &&
                    it->second.powered ) ) ||  mid->flags.contains( trait_flag_FLIGHT_ALWAYS_ACTIVE ) ) {
                return true;
            } else if( ( mid->flags.contains( trait_flag_MUTATION_FLIGHT ) && !can_use_mutation( mid, ch ) &&
                         it->second.powered ) ) {
                ch.deactivate_mutation( mid );
                return false;
            }
        }
    }
    for( const bionic &bio : *ch.my_bionics ) {
        if( bio.info().has_flag( flag_id( "BIONIC_FLIGHT" ) ) &&
            ch.get_power_level() > units::from_kilojoule( 0 ) && bio.powered ) {
            return true;
        }
    }

    return false;
}

auto is_driving( const Character &p ) -> bool
{
    const optional_vpart_position vp = get_map().veh_at( p.pos() );
    return vp && vp->vehicle().is_moving() && vp->vehicle().player_in_control( p );
}

bool is_book_morale_boosted( const Character &ch, const item &book )
{
    // If you don't have a problem with eating humans, To Serve Man becomes rewarding
    if( ( ch.has_trait_flag( trait_flag_CANNIBAL ) || ch.has_trait_flag( trait_flag_PSYCHOPATH ) ||
          ch.has_trait_flag( trait_flag_SAPIOVORE ) ) &&
        book.has_flag( flag_BOOK_CANNIBAL ) ) {
        return true;
    } else if( ch.has_trait_flag( trait_flag_SPIRITUAL ) && book.has_flag( flag_INSPIRATIONAL ) ) {
        return true;
    } else if( ch.has_trait_flag( trait_flag_PSYCHOPATH ) && book.has_flag( flag_MORBID ) ) {
        return true;
    } else {
        return false;
    }
}

bool is_fun_to_read( const Character &ch, const item &book )
{
    if( is_book_morale_boosted( ch, book ) ) {
        return true;
    } else {
        return get_book_fun_for( ch, book ) > 0;
    }
}

int get_book_fun_for( const Character &ch, const item &book )
{
    int fun_bonus = book.type->book->fun;
    if( !book.is_book() ) {
        debugmsg( "called avatar::book_fun_for with non-book" );
        return 0;
    }

    if( is_book_morale_boosted( ch, book ) ) {
        fun_bonus = std::abs( fun_bonus );
    }

    // Separate bonus for spiritual characters.
    if( ch.has_trait_flag( trait_flag_SPIRITUAL ) && book.has_flag( flag_INSPIRATIONAL ) ) {
        fun_bonus += 2;
    }

    if( ch.has_trait( trait_LOVES_BOOKS ) ) {
        fun_bonus++;
    }

    if( fun_bonus > 1 && book.get_chapters() > 0 && book.get_remaining_chapters( ch ) == 0 ) {
        fun_bonus /= 2;
    }

    return fun_bonus;
}

float fine_detail_vision_mod( const Character &who )
{
    return fine_detail_vision_mod( who, who.pos() );
}

float fine_detail_vision_mod( const Character &who, const tripoint &p )
{
    if( who.has_effect_with_flag( flag_EFFECT_SUPER_CLAIRVOYANCE )
        || who.has_effect_with_flag( flag_EFFECT_CLAIRVOYANCE )
        || who.has_effect_with_flag( flag_EFFECT_CLAIRVOYANCE_PLUS ) ) {
        return 1.0f;
    }

    // PER_SLIME_OK implies you can get enough eyes around the bile
    // that you can generally see.  There still will be the haze, but
    // it's annoying rather than limiting.
    if( who.is_blind() ||
        ( ( who.has_effect( effect_boomered ) || who.has_effect( effect_darkness ) ) &&
          !who.has_trait( trait_PER_SLIME_OK ) ) ) {
        return 11.0;
    }
    // Regular NV trait isn't enough to help at all, while Full Night Vision allows reading at a penalty
    float nvbonus = who.mutation_value( "night_vision_range" ) >= 8 ? 4 : 0;
    // Scale linearly as light level approaches LIGHT_AMBIENT_LIT.
    // If we're actually a source of light, assume we can direct it where we need it.
    // Therefore give a hefty bonus relative to ambient light.
    float own_light = std::max( 1.0f, LIGHT_AMBIENT_LIT - who.active_light() - 2.0f );

    // Same calculation as above, but with a result 3 lower, and night vision is allowed to affect it.
    float ambient_light = std::max( 1.0f,
                                    LIGHT_AMBIENT_LIT - get_map().ambient_light_at( p ) - nvbonus + 1.0f );

    return std::min( own_light, ambient_light );
}

bool can_see_fine_details( const Character &who )
{
    return can_see_fine_details( who, who.pos() );
}

bool can_see_fine_details( const Character &who, const tripoint &p )
{
    return fine_detail_vision_mod( who, p ) <= FINE_VISION_THRESHOLD;
}

comfort_response_t base_comfort_value( const Character &who, const tripoint &p )
{
    // Comfort of sleeping spots is "objective", while sleep_spot( p ) is "subjective"
    // As in the latter also checks for fatigue and other variables while this function
    // only looks at the base comfyness of something. It's still subjective, in a sense,
    // as arachnids who sleep in webs will find most places comfortable for instance.
    float comfort = 0.0;

    comfort_response_t comfort_response;

    bool plantsleep = who.has_trait( trait_CHLOROMORPH );
    bool fungaloid_cosplay = who.has_trait( trait_M_SKIN3 );
    bool websleep = who.has_trait( trait_WEB_WALKER );
    bool webforce = who.has_trait( trait_THRESH_SPIDER ) && ( who.has_trait( trait_WEB_SPINNER ) ||
                    ( who.has_trait( trait_WEB_WEAVER ) ) );
    bool in_shell = who.has_active_mutation( trait_SHELL2 );
    bool watersleep = who.has_trait( trait_WATERSLEEP );
    bool music = who.has_active_item_with_action( "MP3_ON" );

    map &here = get_map();
    const optional_vpart_position vp = here.veh_at( p );
    const maptile tile = here.maptile_at( p );
    const trap &trap_at_pos = tile.get_trap_t();
    const ter_id ter_at_pos = tile.get_ter();
    const furn_id furn_at_pos = tile.get_furn();

    int web = here.get_field_intensity( p, fd_web );

    // Some mutants have different comfort needs
    if( !plantsleep && !webforce ) {
        if( in_shell ) {
            comfort += 1 + static_cast<int>( comfort_level::slightly_comfortable );
            // Note: shelled individuals can still use sleeping aids!
        } else if( vp ) {
            const std::optional<vpart_reference> carg = vp.part_with_feature( "CARGO", false );
            const std::optional<vpart_reference> board = vp.part_with_feature( "BOARDABLE", true );
            if( carg ) {
                const vehicle_stack items = vp->vehicle().get_items( carg->part_index() );
                for( item *items_it : items ) {
                    int sleep_quality = items_it->get_quality( qual_SLEEP_AID );
                    if( sleep_quality >= 0 ) {
                        // uncomfortable = -7, neutral = 0, comfortable = 5+, very comfortable = 10
                        // Note: BED + LEVEL 2 SLEEP_AID = 7 pts, or 3 pt below very_comfortable
                        comfort += sleep_quality;
                        comfort_response.aid.push_back( items_it );
                    }
                }
            }
            if( board ) {
                comfort += board->info().comfort;
            } else {
                comfort -= here.move_cost( p );
            }
        }
        // Not in a vehicle, start checking furniture/terrain/traps at this point in decreasing order
        else if( furn_at_pos != f_null ) {
            comfort += 0 + furn_at_pos.obj().comfort;
        }
        // Web sleepers can use their webs if better furniture isn't available
        else if( websleep && web >= 3 ) {
            comfort += 1 + static_cast<int>( comfort_level::slightly_comfortable );
        } else if( ter_at_pos == t_improvised_shelter ) {
            comfort += 0 + static_cast<int>( comfort_level::slightly_comfortable );
        } else if( ter_at_pos == t_floor || ter_at_pos == t_floor_waxed ||
                   ter_at_pos == t_carpet_red || ter_at_pos == t_carpet_yellow ||
                   ter_at_pos == t_carpet_green || ter_at_pos == t_carpet_purple ) {
            comfort += 1 + static_cast<int>( comfort_level::neutral );
        } else if( !trap_at_pos.is_null() ) {
            comfort += 0 + trap_at_pos.comfort;
        } else {
            // Not a comfortable sleeping spot
            comfort -= here.move_cost( p );
        }

        if( comfort_response.aid.empty() ) {
            const map_stack items = here.i_at( p );
            for( item *items_it : items ) {
                int sleep_quality = items_it->get_quality( qual_SLEEP_AID );
                if( sleep_quality >= 0 ) {
                    // Note: BED + LEVEL 2 SLEEP_AID = 7 pts, or 3 pt below very_comfortable
                    comfort += sleep_quality;
                    comfort_response.aid.push_back( items_it );
                }
            }
        }
        bool skintight_or_naked = true;
        for( item *it : who.worn ) {
            if( !it->has_flag( flag_SKINTIGHT ) && !it->has_flag( flag_OVERSIZE ) ) {
                skintight_or_naked = false;
            }

            // check wearing bonus (sleep aid clothing is worth double when worn, similar to snuggling)
            int sleep_quality = it->get_quality( qual_SLEEP_AID );
            if( sleep_quality >= 0 ) {
                comfort += sleep_quality;
                comfort_response.aid.push_back( it );
            }
        }

        // bonus if player is wearing only skintight clothing (pajamas/boxers), oversized clothing (big hoodies, blankets, etc) or naked
        if( skintight_or_naked ) {
            comfort += 1;
        }

        // bonus if player is snuggling with a specific item
        for( item *it : who.wielded_items() ) {
            if( it->has_quality( qual_SLEEP_AID ) ) {
                // don't add to the aid array, because we print special mesage here
                comfort += it->get_quality( qual_SLEEP_AID ) * 2;
                comfort_response.aid.push_back( it );
            }
        }

        if( fungaloid_cosplay && here.has_flag_ter_or_furn( "FUNGUS", p ) ) {
            comfort += static_cast<int>( comfort_level::very_comfortable );
        } else if( watersleep && here.has_flag_ter( "SWIMMABLE", p ) ) {
            comfort += static_cast<int>( comfort_level::very_comfortable );
        }
    } else if( plantsleep ) {
        if( vp || furn_at_pos != f_null ) {
            // Sleep ain't happening in a vehicle or on furniture
            comfort = static_cast<int>( comfort_level::impossible );
        } else {
            // It's very easy for Chloromorphs to get to sleep on soil!
            if( ter_at_pos == t_dirt || ter_at_pos == t_pit || ter_at_pos == t_dirtmound ||
                ter_at_pos == t_pit_shallow ) {
                comfort += static_cast<int>( comfort_level::very_comfortable );
            }
            // Not as much if you have to dig through stuff first
            else if( ter_at_pos == t_grass ) {
                comfort += static_cast<int>( comfort_level::comfortable );
            }
            // Sleep ain't happening
            else {
                comfort = static_cast<int>( comfort_level::impossible );
            }
        }
        // Has webforce
    } else {
        if( web >= 3 ) {
            // Thick Web and you're good to go
            comfort += static_cast<int>( comfort_level::very_comfortable );
        } else {
            comfort = static_cast<int>( comfort_level::impossible );
        }
    }
    // If you are listening to music, give a small buff to comfort
    if( music ) {
        comfort += 2;
    }
    if( comfort >= static_cast<int>( comfort_level::very_comfortable ) ) {
        comfort_response.level = comfort_level::very_comfortable;
    } else if( comfort >= static_cast<int>( comfort_level::comfortable ) ) {
        comfort_response.level = comfort_level::comfortable;
    } else if( comfort >= static_cast<int>( comfort_level::slightly_comfortable ) ) {
        comfort_response.level = comfort_level::slightly_comfortable;
    } else if( comfort >= static_cast<int>( comfort_level::neutral ) ) {
        comfort_response.level = comfort_level::neutral;
    } else {
        comfort_response.level = comfort_level::uncomfortable;
    }
    return comfort_response;
}

int rate_sleep_spot( const Character &who, const tripoint &p )
{
    const int current_stim = who.get_stim();
    const comfort_response_t comfort_info = base_comfort_value( who, p );
    for( item *comfort_item : comfort_info.aid ) {
        who.add_msg_if_player( m_info, _( "You use your %s for comfort." ), comfort_item->tname() );
    }

    int sleepy = static_cast<int>( comfort_info.level );
    bool watersleep = who.has_trait( trait_WATERSLEEP );

    if( who.has_addiction( add_type::SLEEP ) ) {
        sleepy -= 4;
    }
    if( who.has_trait( trait_INSOMNIA ) ) {
        // 12.5 points is the difference between "tired" and "dead tired"
        sleepy -= 12;
    }
    if( who.has_trait( trait_EASYSLEEPER ) ) {
        // Low fatigue (being rested) has a much stronger effect than high fatigue
        // so it's OK for the value to be that much higher
        sleepy += 40;
    }
    if( who.has_active_bionic( bio_soporific ) ) {
        sleepy += 30;
    }
    if( who.has_trait( trait_EASYSLEEPER2 ) ) {
        // At this point, the only limit to sleep is tiredness
        sleepy += 100;
    }
    if( watersleep && get_map().has_flag_ter( "SWIMMABLE", p ) ) {
        sleepy += 10; //comfy water!
    }

    if( who.get_fatigue() < fatigue_levels::tired + 1 ) {
        sleepy -= ( ( fatigue_levels::tired + 1 - who.get_fatigue() ) / 4 );
    } else {
        sleepy += ( ( who.get_fatigue() - fatigue_levels::tired + 1 ) / 16 );
    }

    if( current_stim > 0 || !who.has_trait( trait_INSOMNIA ) ) {
        sleepy -= 2 * current_stim;
    } else {
        // Make it harder for insomniac to get around the trait
        sleepy -= current_stim;
    }

    if( one_in( 3 ) ) {
        if( comfort_info.level >= comfort_level::very_comfortable ) {
            who.add_msg_if_player( "You feel very comfortable." );
        } else if( comfort_info.level >= comfort_level::comfortable ) {
            who.add_msg_if_player( "You feel comfortable." );
        } else if( comfort_info.level >= comfort_level::slightly_comfortable ) {
            who.add_msg_if_player( "You feel slightly comfortable." );
        } else {
            who.add_msg_if_player( "You feel uncomfortable." );
        }
    }

    return sleepy;
}

bool roll_can_sleep( Character &who )
{
    if( who.has_effect( effect_meth ) ) {
        // Sleep ain't happening until that meth wears off completely.
        return false;
    }

    // Since there's a bit of randomness to falling asleep, we want to
    // prevent exploiting this if can_sleep() gets called over and over.
    // Only actually check if we can fall asleep no more frequently than
    // every 30 minutes.  We're assuming that if we return true, we'll
    // immediately be falling asleep after that.
    //
    // Also if player debug menu'd time backwards this breaks, just do the
    // check anyway, this will reset the timer if 'dur' is negative.
    const time_point now = calendar::turn;
    const time_duration dur = now - who.last_sleep_check;
    if( dur >= 0_turns && dur < 30_minutes ) {
        return false;
    }
    who.last_sleep_check = now;

    int sleepy = character_funcs::rate_sleep_spot( who, who.pos() );
    sleepy += rng( -8, 8 );
    bool result = sleepy > 0;

    if( who.has_active_bionic( bio_soporific ) ) {
        if( who.bio_soporific_powered_at_last_sleep_check && !who.has_power() ) {
            who.add_msg_if_player( m_bad, _( "Your soporific inducer runs out of power!" ) );
        } else if( !who.bio_soporific_powered_at_last_sleep_check && who.has_power() ) {
            who.add_msg_if_player( m_good, _( "Your soporific inducer starts back up." ) );
        }
        who.bio_soporific_powered_at_last_sleep_check = who.has_power();
    }

    return result;
}

bool can_interface_armor( const Character &who )
{
    bool okay = std::any_of( who.my_bionics->begin(), who.my_bionics->end(),
    []( const bionic & b ) {
        return b.powered && b.info().has_flag( STATIC( flag_id( "BIONIC_ARMOR_INTERFACE" ) ) );
    } );
    return okay;
}

std::string fmt_wielded_weapon( const Character &who )
{
    if( !who.is_armed() ) {
        return _( "fists" );
    }
    const item &weapon = who.primary_weapon();
    if( weapon.is_gun() ) {
        std::string str = weapon.is_gunmod()
                          ? string_format( "(%d) %s",
                                           weapon.ammo_remaining(), weapon.type_name() )
                          : string_format( "(%d) [%s] %s",
                                           weapon.gun_current_mode()->ammo_remaining(),
                                           weapon.gun_current_mode().tname(), weapon.type_name() );
        // Is either the base item or at least one auxiliary gunmod loaded (includes empty magazines)
        bool base = weapon.ammo_capacity() > 0 && !weapon.has_flag( flag_RELOAD_AND_SHOOT );

        const auto mods = weapon.gunmods();
        bool aux = std::ranges::any_of( mods, [&]( const item * e ) {
            return e->is_gun() && e->ammo_capacity() > 0 && !e->has_flag( flag_RELOAD_AND_SHOOT );
        } );

        if( base || aux ) {
            for( auto e : mods ) {
                if( e->is_gun() && e->ammo_capacity() > 0 && !e->has_flag( flag_RELOAD_AND_SHOOT ) ) {
                    str += " (" + std::to_string( e->ammo_remaining() );
                    if( e->magazine_integral() ) {
                        str += "/" + std::to_string( e->ammo_capacity() );
                    }
                    str += ")";
                }
            }
        }
        return str;

    } else if( ( weapon.is_container() && weapon.contents.num_item_stacks() == 1 ) ||
               weapon.ammo_capacity() > 0 ) {
        return string_format( "(%d) %s",
                              weapon.is_container() ? weapon.contents.front().charges : weapon.ammo_remaining(), weapon.tname() );
    } else {
        return weapon.tname();
    }
}

void add_pain_msg( const Character &who, int val, const bodypart_str_id &bp )
{
    if( who.has_trait( trait_NOPAIN ) ) {
        return;
    }
    if( !bp ) {
        if( val > 20 ) {
            who.add_msg_if_player( _( "Your body is wracked with excruciating pain!" ) );
        } else if( val > 10 ) {
            who.add_msg_if_player( _( "Your body is wracked with terrible pain!" ) );
        } else if( val > 5 ) {
            who.add_msg_if_player( _( "Your body is wracked with pain!" ) );
        } else if( val > 1 ) {
            who.add_msg_if_player( _( "Your body pains you!" ) );
        } else {
            who.add_msg_if_player( _( "Your body aches." ) );
        }
    } else {
        if( val > 20 ) {
            who.add_msg_if_player( _( "Your %s is wracked with excruciating pain!" ),
                                   body_part_name_accusative( bp.id() ) );
        } else if( val > 10 ) {
            who.add_msg_if_player( _( "Your %s is wracked with terrible pain!" ),
                                   body_part_name_accusative( bp.id() ) );
        } else if( val > 5 ) {
            who.add_msg_if_player( _( "Your %s is wracked with pain!" ),
                                   body_part_name_accusative( bp.id() ) );
        } else if( val > 1 ) {
            who.add_msg_if_player( _( "Your %s pains you!" ),
                                   body_part_name_accusative( bp.id() ) );
        } else {
            who.add_msg_if_player( _( "Your %s aches." ),
                                   body_part_name_accusative( bp.id() ) );
        }
    }
}

void normalize( Character &who )
{
    who.martial_arts_data->reset_style();
    who.remove_primary_weapon();

    who.set_body();
    who.recalc_hp();

    for( auto &pr : who.get_body() ) {
        pr.second.set_temp_cur( BODYTEMP_NORM );
        pr.second.set_temp_conv( BODYTEMP_NORM );
    }

    who.set_stamina( who.get_stamina_max() );
}

void store_in_container( Character &who, item &container, detached_ptr<item> &&put, bool penalties,
                         int base_cost )
{
    who.moves -= who.item_store_cost( *put, container, penalties, base_cost );
    container.put_in( std::move( put ) );
    who.reset_encumbrance();
}

bool try_wield_contents( Character &who, item &container, item *internal_item, bool penalties,
                         int base_cost )
{
    // if index not specified and container has multiple items then ask the player to choose one
    if( internal_item == nullptr ) {
        std::vector<std::string> opts;
        std::vector<item *> container_contents = container.contents.all_items_top();
        std::ranges::transform( container_contents,
        std::back_inserter( opts ), []( const item * elem ) {
            return elem->display_name();
        } );
        if( opts.size() > 1 ) {
            int pos = uilist( _( "Wield what?" ), opts );
            if( pos < 0 ) {
                return false;
            }
            internal_item = *std::next( container_contents.begin(), pos );
        } else {
            internal_item = &container.contents.front();
        }
    }

    if( !container.has_item( *internal_item ) ) {
        debugmsg( "Tried to wield non-existent item from container (player::wield_contents)" );
        return false;
    }

    const ret_val<bool> ret = who.can_wield( *internal_item );
    if( !ret.success() ) {
        who.add_msg_if_player( m_info, "%s", ret.c_str() );
        return false;
    }

    int mv = 0;

    if( who.is_armed() ) {
        if( !who.as_player()->unwield() ) {
            return false;
        }
        who.inv_unsort();
    }

    who.set_primary_weapon( internal_item->detach() );
    who.inv_update_invlet( *internal_item );
    who.inv_update_invlet_cache_with_item( *internal_item );
    who.last_item = internal_item->typeId();

    /**
     * @EFFECT_PISTOL decreases time taken to draw pistols from holsters
     * @EFFECT_SMG decreases time taken to draw smgs from holsters
     * @EFFECT_RIFLE decreases time taken to draw rifles from holsters
     * @EFFECT_SHOTGUN decreases time taken to draw shotguns from holsters
     * @EFFECT_LAUNCHER decreases time taken to draw launchers from holsters
     * @EFFECT_STABBING decreases time taken to draw stabbing weapons from sheathes
     * @EFFECT_CUTTING decreases time taken to draw cutting weapons from scabbards
     * @EFFECT_BASHING decreases time taken to draw bashing weapons from holsters
     */
    int lvl = who.get_skill_level( internal_item->is_gun() ? internal_item->gun_skill() :
                                   internal_item->melee_skill() );
    mv += who.item_handling_cost( *internal_item, penalties, base_cost ) / ( ( lvl + 10.0f ) / 10.0f );

    who.moves -= mv;

    internal_item->on_wield( *who.as_player(), mv );

    return true;
}

bool try_uncanny_dodge( Character &who )
{
    const units::energy trigger_cost = bio_uncanny_dodge->power_trigger;
    if( who.get_power_level() < trigger_cost || !who.has_active_bionic( bio_uncanny_dodge ) ) {
        return false;
    }
    who.mod_power_level( -trigger_cost );
    bool is_u = who.is_avatar();
    bool seen = is_u || get_player_character().sees( who );
    // If successful, dodge for free. If we already burned bonus dodges this turn then get_dodge fails and we're overwhelmed.
    if( x_in_y( who.get_dodge(), 10 ) ) {
        if( is_u ) {
            add_msg( m_good, _( "Time seems to slow down and you effortlessly dodge!" ) );
        } else if( seen ) {
            add_msg( m_good, _( "%s effortlessly dodges… so fast!" ), who.disp_name() );
        }
        return true;
        // Didn't get a free dodge, burn dodges_left instead. If this zeros them out and there's still more attacks coming this turn the next shot will hit.
    } else if( who.dodges_left > 0 ) {
        if( is_u ) {
            add_msg( m_mixed, _( "Time seems to slow down and you instinctively dodge!" ) );
        } else if( seen ) {
            add_msg( m_mixed, _( "%s dodges… so fast!" ), who.disp_name() );
        }
        who.dodges_left--;
        return true;
        // No dodges left, catch those hands.
    } else {
        if( is_u ) {
            add_msg( m_bad, _( "You try to dodge but fail!" ) );
        } else if( seen ) {
            add_msg( m_bad, _( "%s tries to dodge but fails!" ), who.disp_name() );
        }
        return false;
    }
}

std::optional<tripoint> pick_safe_adjacent_tile( const Character &who )
{
    std::vector<tripoint> ret;
    int dangerous_fields = 0;
    map &here = get_map();
    for( const tripoint &p : here.points_in_radius( who.pos(), 1 ) ) {
        if( p == who.pos() ) {
            // Don't consider player position
            continue;
        }
        const trap &curtrap = here.tr_at( p );
        if( g->critter_at( p ) == nullptr && here.passable( p ) &&
            ( curtrap.is_null() || curtrap.is_benign() ) ) {
            // Only consider tile if unoccupied, passable and has no traps
            dangerous_fields = 0;
            auto &tmpfld = here.field_at( p );
            for( auto &fld : tmpfld ) {
                const field_entry &cur = fld.second;
                if( cur.is_dangerous() ) {
                    dangerous_fields++;
                }
            }

            if( dangerous_fields == 0 && ! get_map().obstructed_by_vehicle_rotation( who.pos(), p ) ) {
                ret.push_back( p );
            }
        }
    }

    return random_entry_opt( ret );
}

bool is_bp_immune_to( const Character &who, body_part bp, damage_unit dam )
{
    if( who.has_trait( trait_DEBUG_NODMG ) || who.is_immune_damage( dam.type ) ) {
        return true;
    }

    who.passive_absorb_hit( convert_bp( bp ).id(), dam );

    for( const item *cloth : who.worn ) {
        if( cloth->get_coverage( convert_bp( bp ).id() ) == 100 && cloth->covers( convert_bp( bp ) ) ) {
            cloth->mitigate_damage( dam );
        }
    }

    return dam.amount <= 0;
}

std::vector<npc *> get_crafting_helpers( const Character &who, int max )
{
    if( max == 0 || !who.is_avatar() ) {
        // TODO: NPCs assisting other NPCs
        return {};
    }
    int n = 0;
    return g->get_npcs_if( [&]( const npc & guy ) {
        // NPCs can help craft if awake, taking orders, within pickup range and have clear path
        if( max > 0 && n >= max ) {
            return false;
        }
        bool ok = !guy.in_sleep_state() && guy.is_obeying( who ) &&
                  rl_dist( guy.pos(), who.pos() ) < PICKUP_RANGE &&
                  get_map().clear_path( who.pos(), guy.pos(), PICKUP_RANGE, 1, 100 );
        if( ok ) {
            n += 1;
        }
        return ok;
    } );
}

int get_lift_strength( const Character &who )
{
    int str = who.get_str();
    if( who.mounted_creature ) {
        auto mons = who.mounted_creature.get();
        str = mons->mech_str_addition() == 0 ? str : mons->mech_str_addition();
    }
    if( who.has_trait( trait_STRONGBACK ) ) {
        str *= 1.35;
    } else if( who.has_trait( trait_BADBACK ) ) {
        str /= 1.35;
    }
    return str;
}

int get_lift_strength_with_helpers( const Character &who )
{
    int result = get_lift_strength( who );
    const std::vector<npc *> helpers = get_crafting_helpers( who );
    for( const npc *np : helpers ) {
        result += get_lift_strength( *np );
    }
    return result;
}

bool can_lift_with_helpers( const Character &who, int lift_required )
{
    return get_lift_strength_with_helpers( who ) >= lift_required;
}

bool list_ammo( const Character &who, item &base, std::vector<item_reload_option> &ammo_list,
                bool include_empty_mags, bool include_potential )
{
    auto opts = base.gunmods();
    opts.push_back( &base );

    if( base.magazine_current() ) {
        opts.push_back( base.magazine_current() );
    }

    for( const auto mod : base.gunmods() ) {
        if( mod->magazine_current() ) {
            opts.push_back( mod->magazine_current() );
        }
    }

    bool ammo_match_found = false;
    int ammo_search_range = who.is_mounted() ? -1 : 1;
    for( item *e : opts ) {
        for( item *ammo : find_ammo_items_or_mags( who, *e, include_empty_mags,
                ammo_search_range ) ) {
            // don't try to unload frozen liquids
            if( ammo->is_watertight_container() && ammo->contents_made_of( SOLID ) ) {
                continue;
            }
            auto id = ( ammo->is_ammo_container() || ammo->is_container() )
                      ? ammo->contents.front().typeId()
                      : ammo->typeId();
            const bool can_reload_with = e->can_reload_with( id );
            if( can_reload_with ) {
                // Skip if is magazine inside gun/mod, but gun/mod can't fire it (e.g 300 Blackout on STANAG on AR-15)
                if( e->is_magazine() && e->parent_item() ) {
                    auto ammo_type = ( ammo->is_ammo_container() || ammo->is_container() )
                                     ? ammo->contents.front().ammo_type()
                                     : ammo->ammo_type();
                    const std::set<ammotype> &supported_ammo = e->parent_item()->ammo_types();
                    const bool gun_supports = std::ranges::any_of( supported_ammo, [&]( const ammotype & at ) {
                        return at == ammo_type;
                    } );
                    if( !gun_supports ) {
                        continue;
                    }
                }
                // Speedloaders require an empty target.
                if( include_potential || !ammo->has_flag( flag_SPEEDLOADER ) || e->ammo_remaining() < 1 ) {
                    ammo_match_found = true;
                }
            }
            if( ( include_potential && can_reload_with )
                || who.as_player()->can_reload( *e, id ) || e->has_flag( flag_RELOAD_AND_SHOOT ) ) {
                ammo_list.emplace_back( who.as_player(), e, &base, *ammo );
            }
        }
    }
    return ammo_match_found;
}

item_reload_option select_ammo( const player &who, item &base,
                                std::vector<item_reload_option> opts )
{
    if( opts.empty() ) {
        who.add_msg_if_player( m_info, _( "Never mind." ) );
        return item_reload_option();
    }

    if( who.is_npc() ) {
        return opts[ 0 ];
    }

    uilist menu;
    menu.text = string_format( base.is_watertight_container() ? _( "Refill %s" ) :
                               base.has_flag( flag_RELOAD_AND_SHOOT ) ? _( "Select ammo for %s" ) : _( "Reload %s" ),
                               base.tname() );

    // Construct item names
    std::vector<std::string> names;
    std::ranges::transform( opts,
    std::back_inserter( names ), [&]( const item_reload_option & e ) {
        const auto ammo_color = [&]( const std::string & name ) {
            return base.is_gun() && e.ammo->ammo_data() &&
                   !base.ammo_types().contains( e.ammo->ammo_data()->ammo->type ) ?
                   colorize( name, c_dark_gray ) : name;
        };
        if( e.ammo->is_magazine() && e.ammo->ammo_data() ) {
            if( e.ammo->ammo_current() == itype_battery ) {
                // This battery ammo is not a real object that can be recovered but pseudo-object that represents charge
                //~ battery storage (charges)
                return string_format( pgettext( "magazine", "%1$s (%2$d)" ), e.ammo->type_name(),
                                      e.ammo->ammo_remaining() );
            } else {
                //~ magazine with ammo (count)
                return ammo_color( string_format( pgettext( "magazine", "%1$s with %2$s (%3$d)" ),
                                                  e.ammo->type_name(), e.ammo->ammo_data()->nname( e.ammo->ammo_remaining() ),
                                                  e.ammo->ammo_remaining() ) );
            }
        } else if( e.ammo->is_watertight_container() ||
                   ( e.ammo->is_ammo_container() && who.is_worn( *e.ammo ) ) ) {
            // worn ammo containers should be named by their contents with their location also updated below
            return e.ammo->contents.front().display_name();
        } else {
            return ammo_color( ( who.ammo_location &&
                                 who.ammo_location == e.ammo ? "* " : "" ) + e.ammo->display_name() );
        }
    } );

    // Get location descriptions
    std::vector<std::string> where;
    std::ranges::transform( opts,
    std::back_inserter( where ), [&]( const item_reload_option & e ) {
        bool is_ammo_container = e.ammo->is_ammo_container();
        if( is_ammo_container || e.ammo->is_container() ) {
            if( is_ammo_container && who.is_worn( *e.ammo ) ) {
                return e.ammo->type_name();
            }
            return string_format( _( "%s, %s" ), e.ammo->type_name(),
                                  e.ammo->describe_location( who.as_player() ) );
        }
        return e.ammo->describe_location( who.as_player() );
    } );

    // Pads elements to match longest member and return length
    auto pad = []( std::vector<std::string> &vec, int n, int t ) -> int {
        for( const auto &e : vec )
        {
            n = std::max( n, utf8_width( e, true ) + t );
        }
        for( auto &e : vec )
        {
            e += std::string( n - utf8_width( e, true ), ' ' );
        }
        return n;
    };

    // Pad the first column including 4 trailing spaces
    int w = pad( names, utf8_width( menu.text, true ), 6 );
    menu.text.insert( 0, 2, ' ' ); // add space for UI hotkeys
    menu.text += std::string( w + 2 - utf8_width( menu.text, true ), ' ' );

    // Pad the location similarly (excludes leading "| " and trailing " ")
    w = pad( where, utf8_width( _( "| Location " ) ) - 3, 6 );
    menu.text += _( "| Location " );
    menu.text += std::string( w + 3 - utf8_width( _( "| Location " ) ), ' ' );

    menu.text += _( "| Amount  " );
    menu.text += _( "| Moves   " );

    // We only show ammo statistics for guns and magazines
    if( base.is_gun() || base.is_magazine() ) {
        menu.text += _( "| Damage   | Pierce   " );
    }

    auto draw_row = [&]( int idx ) {
        const auto &sel = opts[ idx ];
        std::string row = string_format( "%s| %s |", names[ idx ], where[ idx ] );
        row += string_format( ( sel.ammo->is_ammo() ||
                                sel.ammo->is_ammo_container() ) ? " %-7d |" : "         |", sel.qty() );
        row += string_format( " %-7d ", sel.moves() );

        if( base.is_gun() || base.is_magazine() ) {
            const itype *ammo = sel.ammo->is_ammo_container() ? sel.ammo->contents.front().ammo_data() :
                                sel.ammo->ammo_data();
            if( ammo ) {
                const damage_instance &dam = ammo->ammo->damage;
                const damage_unit &du = dam.damage_units.front();
                if( du.damage_multiplier != 1.0f ) {
                    float dam_amt = du.amount;
                    row += string_format( "| %-3d*%3d%% ", static_cast<int>( dam_amt ),
                                          clamp( static_cast<int>( du.damage_multiplier * 100 ), 0, 999 ) );
                } else {
                    float throw_amt = 0;
                    if( base.gun_skill() == skill_throw ) {
                        item &tmp = *item::spawn_temporary( item( ammo ) );
                        throw_amt += ranged::throw_damage( tmp,
                                                           who.get_skill_level( skill_throw ),
                                                           who.get_str() );
                    }
                    float dam_amt = std::max( dam.total_damage(), throw_amt );
                    row += string_format( "| %-8d ", static_cast<int>( dam_amt ) );
                }
                if( du.res_mult != 1.0f ) {
                    row += string_format( "| %-3d/%3d%%",
                                          static_cast<int>( du.res_pen ), static_cast<int>( 100 * du.res_mult ) );
                } else {
                    row += string_format( "| %-8d", static_cast<int>( du.res_pen ) );
                }
            } else {
                row += "|          |          ";
            }
        }
        return row;
    };

    const ammotype base_ammotype( base.ammo_default().str() );
    itype_id last = uistate.lastreload[ base_ammotype ];
    // We keep the last key so that pressing the key twice (for example, r-r for reload)
    // will always pick the first option on the list.
    int last_key = inp_mngr.get_previously_pressed_key();
    bool last_key_bound = false;
    // This is the entry that has out default
    int default_to = 0;

    // If last_key is RETURN, don't use that to override hotkey
    if( last_key == '\n' ) {
        last_key_bound = true;
        default_to = -1;
    }

    for( auto i = 0; i < static_cast<int>( opts.size() ); ++i ) {
        const item &ammo = opts[ i ].ammo->is_ammo_container() ? opts[ i ].ammo->contents.front() :
                           *opts[ i ].ammo;

        char hotkey = -1;
        if( who.has_item( ammo ) ) {
            // if ammo in player possession and either it or any container has a valid invlet use this
            if( ammo.invlet ) {
                hotkey = ammo.invlet;
            } else {
                for( const auto obj : who.parents( ammo ) ) {
                    if( obj->invlet ) {
                        hotkey = obj->invlet;
                        break;
                    }
                }
            }
        }
        if( last == ammo.typeId() ) {
            if( !last_key_bound && hotkey == -1 ) {
                // If this is the first occurrence of the most recently used type of ammo and the hotkey
                // was not already set above then set it to the keypress that opened this prompt
                hotkey = last_key;
                last_key_bound = true;
            }
            if( !last_key_bound ) {
                // Pressing the last key defaults to the first entry of compatible type
                default_to = i;
                last_key_bound = true;
            }
        }
        if( hotkey == last_key ) {
            last_key_bound = true;
            // Prevent the default from being used: key is bound to something already
            default_to = -1;
        }

        menu.addentry( i, true, hotkey, draw_row( i ) );
    }

    struct reload_callback : public uilist_callback {
        public:
            std::vector<item_reload_option> &opts;
            const std::function<std::string( int )> draw_row;
            int last_key;
            const int default_to;
            const bool can_partial_reload;

            reload_callback( std::vector<item_reload_option> &_opts,
                             std::function<std::string( int )> _draw_row,
                             int _last_key, int _default_to, bool _can_partial_reload ) :
                opts( _opts ), draw_row( std::move( _draw_row ) ),
                last_key( _last_key ), default_to( _default_to ),
                can_partial_reload( _can_partial_reload )
            {}

            bool key( const input_context &, const input_event &event, int idx, uilist *menu ) override {
                auto cur_key = event.get_first_input();
                if( default_to != -1 && cur_key == last_key ) {
                    // Select the first entry on the list
                    menu->ret = default_to;
                    return true;
                }
                if( idx < 0 || idx >= static_cast<int>( opts.size() ) ) {
                    return false;
                }
                auto &sel = opts[ idx ];
                switch( cur_key ) {
                    case KEY_LEFT:
                        if( can_partial_reload ) {
                            sel.qty( sel.qty() - 1 );
                            menu->entries[ idx ].txt = draw_row( idx );
                        }
                        return true;

                    case KEY_RIGHT:
                        if( can_partial_reload ) {
                            sel.qty( sel.qty() + 1 );
                            menu->entries[ idx ].txt = draw_row( idx );
                        }
                        return true;
                }
                return false;
            }
    } cb( opts, draw_row, last_key, default_to, !base.has_flag( flag_RELOAD_ONE ) );
    menu.callback = &cb;

    menu.query();
    if( menu.ret < 0 || static_cast<size_t>( menu.ret ) >= opts.size() ) {
        who.add_msg_if_player( m_info, _( "Never mind." ) );
        return item_reload_option();
    }

    const item *sel = opts[ menu.ret ].ammo;
    uistate.lastreload[ ammotype( base.ammo_default().str() ) ] = sel->is_ammo_container() ?
            sel->contents.front().typeId() :
            sel->typeId();
    return opts[ menu.ret ];
}

item_reload_option select_ammo( const player &who, item &base, bool prompt,
                                bool include_empty_mags, bool include_potential )
{
    std::vector<item_reload_option> ammo_list;
    const bool ammo_match_found = list_ammo( who, base, ammo_list, include_empty_mags,
                                  include_potential );

    if( ammo_list.empty() && !base.is_holster() ) {
        if( !who.is_npc() ) {
            if( !base.is_magazine() && !base.magazine_integral() && !base.magazine_current() ) {
                who.add_msg_if_player( m_info, _( "You need a compatible magazine to reload the %s!" ),
                                       base.tname() );

            } else if( ammo_match_found ) {
                who.add_msg_if_player( m_info, _( "Nothing to reload!" ) );
            } else {
                std::string name;
                if( base.ammo_data() ) {
                    name = base.ammo_data()->nname( 1 );
                } else if( base.is_watertight_container() ) {
                    name = base.is_container_empty() ? "liquid" : base.contents.front().tname();
                } else {
                    name = enumerate_as_string( base.ammo_types().begin(),
                    base.ammo_types().end(), []( const ammotype & at ) {
                        return at->name();
                    }, enumeration_conjunction::none );
                }
                who.add_msg_if_player( m_info, _( "You don't have any %s to reload your %s!" ),
                                       name, base.tname() );
            }
        }
        return item_reload_option();
    }

    // sort in order of move cost (ascending), then remaining ammo (descending) with empty magazines always last
    std::ranges::stable_sort( ammo_list, []( const item_reload_option & lhs,
    const item_reload_option & rhs ) {
        return lhs.ammo->ammo_remaining() > rhs.ammo->ammo_remaining();
    } );
    std::ranges::stable_sort( ammo_list, []( const item_reload_option & lhs,
    const item_reload_option & rhs ) {
        return lhs.moves() < rhs.moves();
    } );
    std::ranges::stable_sort( ammo_list, []( const item_reload_option & lhs,
    const item_reload_option & rhs ) {
        return ( lhs.ammo->ammo_remaining() != 0 ) > ( rhs.ammo->ammo_remaining() != 0 );
    } );

    if( !prompt && ammo_list.size() == 1 ) {
        // unconditionally suppress the prompt if there's only one option
        return ammo_list[ 0 ];
    }

    return select_ammo( who, base, std::move( ammo_list ) );
}

std::vector<item *> get_ammo_items( const Character &who, const ammotype &at )
{
    return who.items_with( [at]( const item & it ) {
        return it.ammo_type() == at;
    } );
}

template <typename T, typename Output>
void find_ammo_helper( T &src, const item &obj, bool empty, Output out, bool nested )
{
    if( obj.is_watertight_container() ) {
        if( !obj.is_container_empty() ) {
            auto contents_id = obj.contents.front().typeId();

            // Look for containers with the same type of liquid as that already in our container
            src.visit_items( [&nested, &out, &contents_id, &obj]( item * node ) {
                if( node == &obj ) {
                    // This stops containers and magazines counting *themselves* as ammo sources.
                    return VisitResponse::SKIP;
                }

                if( node->is_container() && !node->is_container_empty() &&
                    node->contents.front().typeId() == contents_id ) {
                    out = node;
                }
                return nested ? VisitResponse::NEXT : VisitResponse::SKIP;
            } );
        } else {
            // Look for containers with any liquid
            src.visit_items( [&nested, &out]( item * node ) {
                if( node->is_container() && node->contents_made_of( LIQUID ) ) {
                    out = node;
                }
                return nested ? VisitResponse::NEXT : VisitResponse::SKIP;
            } );
        }
    }
    if( obj.magazine_integral() ) {
        // find suitable ammo excluding that already loaded in magazines
        const std::set<ammotype> &ammo = obj.ammo_types();
        const auto mags = obj.magazine_compatible();

        src.visit_items( [&nested, &out, &mags, ammo]( item * node ) {
            if( node->is_gun() || node->is_tool() ) {
                // guns/tools never contain usable ammo so most efficient to skip them now
                return VisitResponse::SKIP;
            }
            if( !node->made_of( SOLID ) ) {
                // some liquids are ammo but we can't reload with them unless within a container or frozen
                return VisitResponse::SKIP;
            }
            if( node->is_ammo_container() && !node->contents.empty() &&
                !node->contents_made_of( SOLID ) ) {
                for( const ammotype &at : ammo ) {
                    if( node->contents.front().ammo_type() == at ) {
                        out = node;
                    }
                }
                return VisitResponse::SKIP;
            }

            for( const ammotype &at : ammo ) {
                if( node->ammo_type() == at ) {
                    out = node;
                }
            }
            if( node->is_magazine() && node->has_flag( flag_SPEEDLOADER ) ) {
                if( mags.contains( node->typeId() ) && node->ammo_remaining() ) {
                    out = node;
                }
            }
            return nested ? VisitResponse::NEXT : VisitResponse::SKIP;
        } );
    } else {
        // find compatible magazines excluding those already loaded in tools/guns
        const auto mags = obj.magazine_compatible();
        const std::set<ammotype> &ammo = obj.ammo_types();

        src.visit_items( [&nested, &out, mags, empty, &ammo]( item * node ) {
            if( node->is_gun() || node->is_tool() ) {
                return VisitResponse::SKIP;
            }
            if( node->is_magazine() ) {

                if( !node->contents.empty() ) {
                    const bool match = std::ranges::any_of( ammo, [&]( const ammotype & at ) {
                        return node->contents.front().ammo_type() == at;
                    } );
                    if( !match ) {
                        return VisitResponse::SKIP;
                    }
                }

                if( mags.contains( node->typeId() ) && ( node->ammo_remaining() || empty ) ) {
                    out = node;
                }
                return VisitResponse::SKIP;
            }
            return nested ? VisitResponse::NEXT : VisitResponse::SKIP;
        } );
    }
}

std::vector<item *> find_ammo_items_or_mags( const Character &who, const item &obj,
        bool empty, int radius )
{
    std::vector<item *> res;

    find_ammo_helper( const_cast<Character &>( who ), obj, empty, std::back_inserter( res ), true );

    if( radius >= 0 ) {
        for( auto &cursor : map_selector( who.pos(), radius ) ) {
            find_ammo_helper( cursor, obj, empty, std::back_inserter( res ), false );
        }
        for( auto &cursor : vehicle_selector( who.pos(), radius ) ) {
            find_ammo_helper( cursor, obj, empty, std::back_inserter( res ), false );
        }
    }

    return res;
}

std::vector<item *> find_reloadables( Character &who )
{
    std::vector<item *> reloadables;

    who.visit_items( [&]( item * node ) {
        bool reloadable = false;
        if( node->is_gun() && !node->magazine_compatible().empty() ) {
            reloadable = node->magazine_current() == nullptr ||
                         node->ammo_remaining() < node->ammo_capacity();
        } else {
            reloadable = ( node->is_magazine() || node->is_bandolier() ||
                           ( node->is_gun() && node->magazine_integral() ) ) &&
                         node->ammo_remaining() < node->ammo_capacity();
        }
        if( node->is_holster() ) {
            const holster_actor *ptr = dynamic_cast<const holster_actor *>
                                       ( node->get_use( "holster" )->get_actor_ptr() );
            if( static_cast<int>( node->contents.num_item_stacks() ) < ptr->multi ) {
                reloadable = true;
            }
        }
        if( reloadable ) {
            reloadables.push_back( node );
        }
        return VisitResponse::SKIP;
    } );
    return reloadables;
}

int ammo_count_for( const Character &who, const item &gun )
{
    if( !gun.is_gun() ) {
        return item::INFINITE_CHARGES;
    }
    int ammo_drain = gun.ammo_required();
    int energy_drain = gun.get_gun_ups_drain();

    units::energy power = units::from_kilojoule( who.charges_of( itype_UPS ) );
    int total_ammo = gun.ammo_remaining();
    const std::vector<item *> inv_ammo = find_ammo_items_or_mags( who, gun, true, -1 );

    bool has_mag = gun.magazine_integral();

    for( const item * const &it : inv_ammo ) {
        if( it->is_magazine() ) {
            total_ammo += it->ammo_remaining();
        } else if( has_mag && it->is_ammo() ) {
            // In combat, NPCs will only consider ammo "available" if it's in an applicable magazine
            // or if the gun has an integral magazine (whereupon the "mags" are usually speedloaders)
            total_ammo += it->count();
        }
    }

    if( ammo_drain > 0 && energy_drain > 0 ) {
        // Both UPS and ammo, lower is limiting.
        return std::min( total_ammo / ammo_drain, power / units::from_kilojoule( energy_drain ) );
    } else if( energy_drain > 0 ) {
        //Only one of the two, it is limiting.
        return power / units::from_kilojoule( energy_drain );
    } else if( ammo_drain > 0 ) {
        return total_ammo / ammo_drain;
    } else {
        // Effectively infinite ammo.
        return item::INFINITE_CHARGES;
    }
}

void show_skill_capped_notice( const Character &who, const skill_id &id )
{
    if( !who.is_avatar() ) {
        return;
    }
    const SkillLevel &level = who.get_skill_level_object( id );

    const Skill &skill = id.obj();
    std::string skill_name = skill.name();
    int curLevel = level.level();

    add_msg( m_info, _( "This task is too simple to train your %s beyond %d." ),
             skill_name, curLevel );
}

/// Returns true if the character has a bionic listed in the entered bionic id's available_upgrades field
auto has_upgraded_bionic( const Character &c, const bionic_id &b ) -> bool
{
    return std::ranges::any_of( b->available_upgrades, [&]( const auto & bio ) { return c.has_bionic( bio ); } );
}

} // namespace character_funcs
