#include "relic.h"

#include <algorithm>
#include <cmath>

#include "cata_unreachable.h"
#include "creature.h"
#include "character.h"
#include "field.h"
#include "game.h"
#include "json.h"
#include "magic.h"
#include "magic_enchantment.h"
#include "map.h"
#include "map_iterator.h"
#include "messages.h"
#include "rng.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "weather.h"

static const efftype_id effect_sleep( "sleep" );

namespace io
{
template<>
std::string enum_to_string<relic_recharge_type>( relic_recharge_type data )
{
    switch( data ) {
        // *INDENT-OFF*
        case relic_recharge_type::time: return "time";
        case relic_recharge_type::solar: return "solar";
        case relic_recharge_type::pain: return "pain";
        case relic_recharge_type::hp: return "hp";
        case relic_recharge_type::fatigue: return "fatigue";
        case relic_recharge_type::field: return "field";
        case relic_recharge_type::trap: return "trap";
        case relic_recharge_type::num:
            break;
        // *INDENT-ON*
    }
    debugmsg( "Invalid relic_recharge_type" );
    abort();
}

template<>
std::string enum_to_string<relic_recharge_req>( relic_recharge_req data )
{
    switch( data ) {
        // *INDENT-OFF*
        case relic_recharge_req::none: return "none";
        case relic_recharge_req::equipped: return "equipped";
        case relic_recharge_req::close_to_skin: return "close_to_skin";
        case relic_recharge_req::sleep: return "sleep";
        case relic_recharge_req::rad: return "rad";
        case relic_recharge_req::wet: return "wet";
        case relic_recharge_req::sky: return "sky";
        case relic_recharge_req::num:
            break;
        // *INDENT-ON*
    }
    debugmsg( "Invalid relic_recharge_req" );
    abort();
}
} // namespace io

bool relic_recharge::operator==( const relic_recharge &rhs ) const
{
    return type == rhs.type &&
           req == rhs.req &&
           field_type == rhs.field_type &&
           trap_type == rhs.trap_type &&
           interval == rhs.interval &&
           intensity_min == rhs.intensity_min &&
           intensity_max == rhs.intensity_max &&
           rate == rhs.rate &&
           message == rhs.message;
}

void relic_recharge::load( const JsonObject &jo )
{
    try {
        src_loc = jo.get_source_location();
        // NOLINTNEXTLINE(bugprone-empty-catch): Savefiles don't specify source, so ignore error
    } catch( const std::exception & ) {
    }

    jo.read( "type", type );
    jo.read( "req", req );
    jo.read( "field", field_type );
    jo.read( "trap", trap_type );
    jo.read( "interval", interval );
    jo.read( "int_min", intensity_min );
    jo.read( "int_max", intensity_max );
    jo.read( "rate", rate );
    jo.read( "message", message );
}

void relic_recharge::serialize( JsonOut &json ) const
{
    json.start_object();

    json.member( "type", type );
    json.member( "req", req );
    json.member( "field", field_type );
    json.member( "trap", trap_type );
    json.member( "interval", interval );
    json.member( "int_min", intensity_min );
    json.member( "int_max", intensity_max );
    json.member( "rate", rate );
    json.member( "message", message );

    json.end_object();
}

void relic_recharge::deserialize( JsonIn &jsin )
{
    JsonObject data = jsin.get_object();
    load( data );
}

void relic_recharge::check() const
{
    if( type == relic_recharge_type::field && !field_type.is_valid() ) {
        show_warning_at_json_loc( src_loc,
                                  string_format( "Relic specifies invalid field type '%s' to recharge from.", field_type )
                                );
    }
    if( type == relic_recharge_type::trap && !trap_type.is_valid() ) {
        show_warning_at_json_loc( src_loc,
                                  string_format( "Relic specifies invalid trap type '%s' to recharge from.", trap_type )
                                );
    }
    if( to_seconds<int>( interval ) <= 0 ) {
        show_warning_at_json_loc( src_loc, "Relic specifies invalid recharge interval." );
    }
    if( intensity_min < 0 || intensity_max < 0 ) {
        show_warning_at_json_loc( src_loc, "Relic specifies invalid recharge intensity." );
    }
}

void relic::add_active_effect( const fake_spell &sp )
{
    active_effects.emplace_back( sp );
}

void relic::add_passive_effect( const enchantment &nench )
{
    for( enchantment &ench : passive_effects ) {
        if( ench.add( nench ) ) {
            return;
        }
    }
    passive_effects.emplace_back( nench );
}

void relic::add_recharge_scheme( const relic_recharge &r )
{
    recharge_scheme.emplace_back( r );
}

void relic::load( const JsonObject &jo )
{
    if( jo.has_array( "active_effects" ) ) {
        for( JsonObject jobj : jo.get_array( "active_effects" ) ) {
            fake_spell sp;
            sp.load( jobj );
            add_active_effect( sp );
        }
    }
    if( jo.has_array( "passive_effects" ) ) {
        for( JsonObject jobj : jo.get_array( "passive_effects" ) ) {
            enchantment ench;
            ench.load( jobj );
            if( !ench.id.is_empty() ) {
                ench = ench.id.obj();
            }
            add_passive_effect( ench );
        }
    }
    if( jo.has_array( "recharge_scheme" ) ) {
        for( JsonObject jobj : jo.get_array( "recharge_scheme" ) ) {
            relic_recharge recharge;
            recharge.load( jobj );
            add_recharge_scheme( recharge );
        }
    }
    jo.read( "name", item_name_override );
    charges_per_activation = jo.get_int( "charges_per_activation", 1 );
    moves = jo.get_int( "moves", 100 );
}

void relic::deserialize( JsonIn &jsin )
{
    JsonObject jobj = jsin.get_object();
    load( jobj );
}

void relic::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "moves", moves );
    jsout.member( "charges_per_activation", charges_per_activation );
    // item_name_override is not saved, in case the original json text changes:
    // in such case names read back from a save wouold no longer be properly translated.

    if( !passive_effects.empty() ) {
        jsout.member( "passive_effects" );
        jsout.start_array();
        for( const enchantment &ench : passive_effects ) {
            ench.serialize( jsout );
        }
        jsout.end_array();
    }

    if( !active_effects.empty() ) {
        jsout.member( "active_effects" );
        jsout.start_array();
        for( const fake_spell &sp : active_effects ) {
            sp.serialize( jsout );
        }
        jsout.end_array();
    }

    if( !recharge_scheme.empty() ) {
        jsout.member( "recharge_scheme" );
        jsout.start_array();
        for( const relic_recharge &sp : recharge_scheme ) {
            sp.serialize( jsout );
        }
        jsout.end_array();
    }

    jsout.end_object();
}

int relic::activate( Creature &caster, const tripoint &target ) const
{
    caster.moves -= moves;
    for( const fake_spell &sp : active_effects ) {
        sp.get_spell( sp.level ).cast_all_effects( caster, target );
    }
    return charges_per_activation;
}

bool relic::operator==( const relic &rhs ) const
{
    return charges_per_activation == rhs.charges_per_activation &&
           moves == rhs.moves &&
           item_name_override == rhs.item_name_override &&
           active_effects == rhs.active_effects &&
           passive_effects == rhs.passive_effects &&
           recharge_scheme == rhs.recharge_scheme;
}

std::string relic::name() const
{
    return item_name_override.translated();
}

void relic::check() const
{
    for( const enchantment &ench : passive_effects ) {
        ench.check();
    }
    for( const relic_recharge &rech : recharge_scheme ) {
        rech.check();
    }
}

namespace relic_funcs
{

bool check_recharge_reqs( const item &itm, const relic_recharge &rech, const Character *carrier )
{
    bool possess = carrier;

    switch( rech.req ) {
        case relic_recharge_req::none: {
            return true;
        }
        case relic_recharge_req::equipped: {
            if( !possess ) {
                return false;
            }
            if( itm.is_armor() ) {
                return carrier->is_wearing( itm );
            } else {
                return carrier->is_wielding( itm );
            }
        }
        case relic_recharge_req::close_to_skin: {
            if( !possess ) {
                return false;
            }
            if( itm.is_armor() ) {
                if( !carrier->is_wearing( itm ) ) {
                    return false;
                }
                for( const bodypart_id &bp : carrier->get_all_body_parts() ) {
                    if( !itm.covers( bp ) ) {
                        continue;
                    }
                    bool this_bp_good = true;
                    for( const item *wi : carrier->worn ) {
                        if( wi->get_coverage( bp ) == 0 ) {
                            continue;
                        }
                        if( wi == &itm ) {
                            break;
                        } else if( wi->covers( bp ) ) {
                            this_bp_good = false;
                            break;
                        }
                    }
                    if( this_bp_good ) {
                        return true;
                    }
                }
                return false;
            } else {
                if( !carrier->is_wielding( itm ) ) {
                    return false;
                }
                bool hand_l_ok = true;
                bool hand_r_ok = true;
                for( const item *wi : carrier->worn ) {
                    if( wi->get_coverage( body_part_hand_l ) == 0 && wi->get_coverage( body_part_hand_r ) == 0 ) {
                        continue;
                    }
                    if( wi->covers( body_part_hand_l ) ) {
                        hand_l_ok = false;
                    }
                    if( wi->covers( body_part_hand_r ) ) {
                        hand_r_ok = false;
                    }
                }
                return hand_l_ok || hand_r_ok;
            }
        }
        case relic_recharge_req::sleep: {
            if( !possess ) {
                return false;
            }
            return carrier->has_effect( effect_sleep );
        }
        case relic_recharge_req::rad: {
            return get_map().get_radiation( itm.position() ) > 0 || ( possess && carrier->get_rad() > 0 );
        }
        case relic_recharge_req::wet: {
            bool soaked = false;
            if( possess ) {
                soaked = std::any_of( carrier->get_body().begin(), carrier->get_body().end(),
                []( const std::pair<const bodypart_str_id, bodypart> &elem ) {
                    return elem.second.get_wetness() != 0;
                } );
            }
            if( !soaked ) {
                const weather_type &wt = get_weather().weather_id.obj();
                if( !wt.rains || wt.acidic || wt.precip == precip_class::none ) {
                    return false;
                }
                soaked = get_map().is_outside( itm.position() );
            }
            return soaked;
        }
        case relic_recharge_req::sky: {
            return itm.position().z > 0;
        }
        default: {
            std::abort();
        }
    }
    cata::unreachable();
}

bool process_recharge_entry( item &itm, const relic_recharge &rech, Character *carrier )
{
    if( !calendar::once_every( rech.interval ) ) {
        return false;
    }
    if( !check_recharge_reqs( itm, rech, carrier ) ) {
        return false;
    }
    map &here = get_map();
    switch( rech.type ) {
        case relic_recharge_type::time: {
            break;
        }
        case relic_recharge_type::solar: {
            if( !g->is_in_sunlight( itm.position() ) ) {
                return false;
            }
            break;
        }
        case relic_recharge_type::pain: {
            if( carrier ) {
                carrier->add_msg_if_player( m_bad, _( "You suddenly feel sharp pain for no reason." ) );
                carrier->mod_pain_noresist( rng( rech.intensity_min, rech.intensity_max ) );
            } else {
                return false;
            }
            break;
        }
        case relic_recharge_type::hp: {
            if( carrier ) {
                carrier->add_msg_if_player( m_bad, _( "You feel your body decaying." ) );
                carrier->hurtall( rng( rech.intensity_min, rech.intensity_max ), nullptr );
            } else {
                return false;
            }
            break;
        }
        case relic_recharge_type::fatigue: {
            if( carrier ) {
                carrier->add_msg_if_player( m_bad, _( "You feel fatigue seeping into your body." ) );
                carrier->mod_fatigue( rng( rech.intensity_min, rech.intensity_max ) );
                carrier->mod_stamina( -100 * rng( rech.intensity_min, rech.intensity_max ) );
            } else {
                return false;
            }
            break;
        }
        case relic_recharge_type::field: {
            bool consumed = false;
            for( const tripoint &dest : here.points_in_radius( itm.position(), 1 ) ) {
                field_entry *field_at = here.field_at( dest ).find_field( rech.field_type );
                if( !field_at ) {
                    continue;
                }
                int int_here = field_at->get_field_intensity();
                if( int_here >= rech.intensity_min && int_here <= rech.intensity_max ) {
                    here.remove_field( dest, rech.field_type );
                    consumed = true;
                    break;
                }
            }
            if( !consumed ) {
                return false;
            }
            break;
        }
        case relic_recharge_type::trap: {
            bool consumed = false;
            for( const tripoint &dest : here.points_in_radius( itm.position(), 1 ) ) {
                if( here.tr_at( dest ).id == rech.trap_type ) {
                    here.remove_trap( dest );
                    consumed = true;
                }
            }
            if( !consumed ) {
                return false;
            }
            break;
        }
        default: {
            std::abort();
        }
    }
    // If it already has ammo, increment charges of ammo inside.
    if( itm.ammo_data() ) {
        int ammo_charge = clamp( itm.ammo_remaining() + rech.rate, 0, itm.ammo_capacity() );
        itm.ammo_set( itm.ammo_current(), ammo_charge );
    } else {
        // If not, either give it default ammo, or increment charges directly.
        if( !itm.ammo_types().empty() ) {
            itm.ammo_set( itm.ammo_default(), clamp( itm.ammo_remaining() + rech.rate, 0,
                          itm.ammo_capacity() ) );
        } else {
            itm.charges = clamp( itm.charges + rech.rate, 0, itm.ammo_capacity() );
        }
    }
    if( rech.message ) {
        if( carrier ) {
            carrier->add_msg_if_player( _( *rech.message ) );
        }
    }
    return true;
}

void process_recharge( item &itm, Character *carrier )
{
    if( itm.is_tool() || itm.is_gun() || itm.is_magazine() ) {
        if( itm.ammo_remaining() >= itm.ammo_capacity() ) {
            return;
        }
        for( const relic_recharge &rech : itm.get_relic_recharge_scheme() ) {
            process_recharge_entry( itm, rech, carrier );
        }
    }
    return;
}
} // namespace relic_funcs
