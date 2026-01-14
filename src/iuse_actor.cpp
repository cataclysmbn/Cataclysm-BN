#include "iuse_actor.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <ret_val.h>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <ranges>

#include "action.h"
#include "activity_handlers.h"
#include "addiction.h"
#include "ammo.h"
#include "animation.h"
#include "assign.h"
#include "avatar.h"
#include "avatar_functions.h"
#include "bionics.h"
#include "bodypart.h"
#include "cached_options.h"
#include "calendar.h"
#include "cata_utility.h"
#include "character.h"
#include "character_functions.h"
#include "character_id.h"
#include "clothing_mod.h"
#include "crafting.h"
#include "creature.h"
#include "debug.h"
#include "effect.h"
#include "enum_conversions.h"
#include "enums.h"
#include "explosion.h"
#include "field_type.h"
#include "flag.h"
#include "flat_set.h"
#include "game.h"
#include "game_inventory.h"
#include "handle_liquid.h"
#include "iexamine.h"
#include "int_id.h"
#include "inventory.h"
#include "item.h"
#include "item_contents.h"
#include "item_factory.h"
#include "item_group.h"
#include "itype.h"
#include "json.h"
#include "line.h"
#include "locations.h"
#include "magic.h"
#include "map.h"
#include "map_iterator.h"
#include "map_selector.h"
#include "map_utils.h"
#include "mapdata.h"
#include "material.h"
#include "memory_fast.h"
#include "messages.h"
#include "monster.h"
#include "morale_types.h"
#include "mtype.h"
#include "mutation.h"
#include "options.h"
#include "output.h"
#include "overmap.h"
#include "overmap_ui.h"
#include "overmapbuffer.h"
#include "player.h"
#include "player_activity.h"
#include "pldata.h"
#include "popup.h"
#include "point.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"
#include "rng.h"
#include "skill.h"
#include "sounds.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "string_input_popup.h"
#include "text_snippets.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "ui.h"
#include "uistate.h"
#include "units_utility.h"
#include "value_ptr.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vehicle_selector.h"
#include "visitable.h"
#include "vitamin.h"
#include "vpart_position.h"
#include "vpart_range.h"
#include "weather.h"

static const activity_id ACT_FIRSTAID( "ACT_FIRSTAID" );
static const activity_id ACT_MAKE_ZLAVE( "ACT_MAKE_ZLAVE" );
static const activity_id ACT_RELOAD( "ACT_RELOAD" );
static const activity_id ACT_REPAIR_ITEM( "ACT_REPAIR_ITEM" );
static const activity_id ACT_SPELLCASTING( "ACT_SPELLCASTING" );
static const activity_id ACT_STUDY_SPELL( "ACT_STUDY_SPELL" );
static const activity_id ACT_START_FIRE( "ACT_START_FIRE" );
static const activity_id ACT_VIBE( "ACT_VIBE" );
static const activity_id ACT_TRAIN_SKILL( "ACT_TRAIN_SKILL" );

static const efftype_id effect_accumulated_mutagen( "accumulated_mutagen" );
static const efftype_id effect_asthma( "asthma" );
static const efftype_id effect_bandaged( "bandaged" );
static const efftype_id effect_bite( "bite" );
static const efftype_id effect_cig( "cig" );
static const efftype_id effect_bleed( "bleed" );
static const efftype_id effect_pet( "pet" );
static const efftype_id effect_disinfected( "disinfected" );
static const efftype_id effect_downed( "downed" );
static const efftype_id effect_infected( "infected" );
static const efftype_id effect_hallu( "hallu" );
static const efftype_id effect_music( "music" );
static const efftype_id effect_playing_instrument( "playing_instrument" );
static const efftype_id effect_recover( "recover" );
static const efftype_id effect_run( "run" );
static const efftype_id effect_sleep( "sleep" );
static const efftype_id effect_stunned( "stunned" );
static const efftype_id effect_visuals( "visuals" );

static const fault_id fault_bionic_nonsterile( "fault_bionic_nonsterile" );

static const bionic_id bio_syringe( "bio_syringe" );

static const itype_id itype_barrel_small( "barrel_small" );
static const itype_id itype_brazier( "brazier" );
static const itype_id itype_char_smoker( "char_smoker" );
static const itype_id itype_fire( "fire" );
static const itype_id itype_stock_small( "stock_small" );
static const itype_id itype_syringe( "syringe" );
static const itype_id itype_fertilizer( "fertilizer" );
static const itype_id itype_genome_drive( "genome_drive" );
static const itype_id itype_usb_drive( "usb_drive" );
static const itype_id itype_mutagen( "mutagen" );
static const itype_id itype_biomaterial( "biomaterial" );

static const skill_id skill_fabrication( "fabrication" );
static const skill_id skill_firstaid( "firstaid" );
static const skill_id skill_survival( "survival" );

static const species_id HUMAN( "HUMAN" );
static const species_id ZOMBIE( "ZOMBIE" );

static const trait_id trait_CENOBITE( "CENOBITE" );
static const trait_id trait_DEBUG_BIONICS( "DEBUG_BIONICS" );
static const trait_id trait_TOLERANCE( "TOLERANCE" );
static const trait_id trait_INFRESIST( "INFRESIST" );
static const trait_id trait_LIGHTWEIGHT( "LIGHTWEIGHT" );
static const trait_id trait_PACIFIST( "PACIFIST" );
static const trait_id trait_PSYCHOPATH( "PSYCHOPATH" );
static const trait_id trait_PYROMANIA( "PYROMANIA" );
static const trait_id trait_NOPAIN( "NOPAIN" );
static const trait_id trait_MASOCHIST( "MASOCHIST" );
static const trait_id trait_MASOCHIST_MED( "MASOCHIST_MED" );
static const trait_id trait_MUT_JUNKIE( "MUT_JUNKIE" );
static const trait_id trait_SAPIOVORE( "SAPIOVORE" );

static const trait_flag_str_id trait_flag_PRED1( "PRED1" );
static const trait_flag_str_id trait_flag_PRED2( "PRED2" );
static const trait_flag_str_id trait_flag_PRED3( "PRED3" );
static const trait_flag_str_id trait_flag_PRED4( "PRED4" );

static const itype_id itype_UPS( "UPS" );

static const mtype_id mon_hallu_multicooker( "mon_hallu_multicooker" );


static const species_id species_HALLUCINATION( "HALLUCINATION" );
static const species_id species_ROBOT( "ROBOT" );
static const species_id species_ZOMBIE( "ZOMBIE" );
static const species_id species_NETHER( "NETHER" );
static const species_id species_SKELETON( "SKELETON" );

class npc;

std::unique_ptr<iuse_actor> iuse_transform::clone() const
{
    return std::make_unique<iuse_transform>( *this );
}

void iuse_transform::load( const JsonObject &obj )
{
    obj.read( "target", target, true );

    obj.read( "msg", msg_transform );
    obj.read( "container", container );
    if( obj.has_member( "target_charges" ) && obj.has_member( "rand_target_charges" ) ) {
        obj.throw_error( "Transform actor specified both fixed and random target charges",
                         "target_charges" );
    }
    obj.read( "target_charges", ammo_qty );
    if( obj.has_array( "rand_target_charges" ) ) {
        for( const int charge : obj.get_array( "rand_target_charges" ) ) {
            random_ammo_qty.push_back( charge );
        }
        if( random_ammo_qty.size() < 2 ) {
            obj.throw_error( "You must specify two or more values to choose between", "rand_target_charges" );
        }
    }
    obj.read( "target_ammo", ammo_type );

    obj.read( "countdown", countdown );

    if( !ammo_type.is_empty() && !container.is_empty() ) {
        obj.throw_error( "Transform actor specified both ammo type and container type", "target_ammo" );
    }

    obj.read( "active", active );

    obj.read( "moves", moves );
    if( moves < 0 ) {
        obj.throw_error( "transform actor specified negative moves", "moves" );
    }

    obj.read( "need_fire", need_fire );
    need_fire = std::max( need_fire, 0 );
    if( !obj.read( "need_charges_msg", need_charges_msg ) ) {
        need_charges_msg = to_translation( "The %s is empty!" );
    }

    obj.read( "need_charges", need_charges );
    need_charges = std::max( need_charges, 0 );
    if( !obj.read( "need_fire_msg", need_fire_msg ) ) {
        need_fire_msg = to_translation( "You need a source of fire!" );
    }
    obj.read( "transform_charges", transform_charges );

    obj.read( "need_worn", need_worn );
    obj.read( "need_wielding", need_wielding );
    obj.read( "need_dry", need_dry );

    obj.read( "qualities_needed", qualities_needed );

    obj.read( "menu_text", menu_text );
}

int iuse_transform::use( player &p, item &it, bool t, const tripoint &pos ) const
{
    if( t ) {
        return 0; // invoked from active item processing, do nothing.
    }

    const bool possess = p.has_item( it ) ||
                         ( it.has_flag( flag_ALLOWS_REMOTE_USE ) && square_dist( p.pos(), pos ) == 1 );

    if( possess && need_worn && !p.is_worn( it ) ) {
        p.add_msg_if_player( m_info, _( "You need to wear the %1$s before activating it." ), it.tname() );
        return 0;
    }
    if( possess && need_wielding && !p.is_wielding( it ) ) {
        p.add_msg_if_player( m_info, _( "You need to wield the %1$s before activating it." ), it.tname() );
        return 0;
    }
    // No charge consumption at this point, there are still points of failure later.
    if( need_charges || transform_charges ) {
        if( it.has_flag( flag_POWERARMOR_MOD ) && character_funcs::can_interface_armor( p ) ) {
            if( possess ) {
                const int bio_power = units::to_kilojoule( p.get_power_level() );
                if( bio_power < need_charges || bio_power < transform_charges ) {
                    p.add_msg_if_player( m_info, need_charges_msg, it.tname() );
                    return 0;
                }
            } else {
                return 0;
            }
        } else {
            const int item_charges = it.units_remaining( p );
            if( item_charges < need_charges || item_charges < transform_charges ) {
                p.add_msg_if_player( m_info, need_charges_msg, it.tname() );
                return 0;
            }
        }
    }


    if( need_fire && possess ) {
        if( !p.use_charges_if_avail( itype_fire, need_fire ) ) {
            p.add_msg_if_player( m_info, need_fire_msg, it.tname() );
            return 0;
        }
        if( p.is_underwater() ) {
            p.add_msg_if_player( m_info, _( "You can't do that while underwater" ) );
            return 0;
        }
    }

    // All checks complete the damn thing can finally transform
    // Consume charges if necessary at this point.
    if( transform_charges ) {
        p.consume_charges( it, transform_charges );
    }

    if( possess && !msg_transform.empty() ) {
        p.add_msg_if_player( m_neutral, msg_transform, it.tname() );
    }
    // We want this separate and not if/else because the preceding statement will always return true if a transform message is defined.
    if( p.is_npc() && get_player_character().sees( p ) ) {
        if( !it.has_flag( flag_COMBAT_NPC_ON ) ) {
            add_msg( m_info, _( "%s activates their %s." ), p.disp_name(),
                     it.display_name() );
        } else {
            add_msg( m_info, _( "%s deactivates their %s." ), p.disp_name(),
                     it.display_name() );
        }
    }

    if( possess ) {
        p.moves -= moves;
    }

    // Update Luminosity as object is "removed"
    get_map().update_lum( it, false );

    if( p.is_worn( it ) ) {
        p.on_item_takeoff( it );
    }
    if( container.is_empty() ) {
        it.convert( target );
        if( ammo_qty >= 0 || !random_ammo_qty.empty() ) {
            int qty;
            if( !random_ammo_qty.empty() ) {
                const auto index = rng( 1, random_ammo_qty.size() - 1 );
                qty = rng( random_ammo_qty[index - 1], random_ammo_qty[index] );
            } else {
                qty = ammo_qty;
            }
            if( !ammo_type.is_empty() ) {
                it.ammo_set( ammo_type, qty );
            } else if( !it.ammo_current().is_null() ) {
                it.ammo_set( it.ammo_current(), qty );
            } else {
                it.set_charges( qty );
            }
            // If we're setting target charges then check for integral mods too.
            if( it.type->gun ) {
                for( const itype_id &mod : it.type->gun->built_in_mods ) {
                    detached_ptr<item> content = item::spawn( mod, calendar::turn, qty );
                    content->set_flag( flag_IRREMOVABLE );
                    it.put_in( std::move( content ) );
                }
                for( const itype_id &mod : it.type->gun->default_mods ) {
                    it.put_in( item::spawn( mod, calendar::turn, qty ) );
                }

            }
        }
    } else {
        it.convert( container );
        it.put_in( item::spawn( target, calendar::turn, std::max( ammo_qty, 1 ) ) );
    }
    if( p.is_worn( it ) ) {
        p.reset_encumbrance();
        // This is most likely wrong: it doubles temperature shift for the turn!
        p.update_bodytemp( get_map(), get_weather() );
        p.on_item_wear( it );
    }
    p.inv_update_invlet_cache_with_item( it );
    // Update luminosity as object is "added"
    get_map().update_lum( it, true );
    ( active || countdown ) ? it.activate() : it.deactivate();
    it.set_counter( countdown > 0 ? countdown : it.type->countdown_interval );
    // Check for gaining or losing night vision, eye encumbrance effects, clairvoyance from transforming relics, etc.
    p.recalc_sight_limits();

    return 0;
}

ret_val<bool> iuse_transform::can_use( const Character &p, const item &, bool,
                                       const tripoint & ) const
{
    if( need_dry && p.is_underwater() ) {
        return ret_val<bool>::make_failure( _( "This item cannot be used while underwater." ) );
    }
    if( qualities_needed.empty() ) {
        return ret_val<bool>::make_success();
    }

    std::map<quality_id, int> unmet_reqs;
    inventory inv;
    inv.form_from_map( p.pos(), 1, &p, true, true );
    for( const auto &quality : qualities_needed ) {
        if( !p.has_quality( quality.first, quality.second ) &&
            !inv.has_quality( quality.first, quality.second ) ) {
            unmet_reqs.insert( quality );
        }
    }
    if( unmet_reqs.empty() ) {
        return ret_val<bool>::make_success();
    }
    std::string unmet_reqs_string = enumerate_as_string( unmet_reqs.begin(), unmet_reqs.end(),
    [&]( const std::pair<quality_id, int> &unmet_req ) {
        return string_format( "%s %d", unmet_req.first.obj().name, unmet_req.second );
    } );
    return ret_val<bool>::make_failure( vgettext( "You need a tool with %s.", "You need tools with %s.",
                                        unmet_reqs.size() ), unmet_reqs_string );
}

std::string iuse_transform::get_name() const
{
    if( !menu_text.empty() ) {
        return menu_text.translated();
    }
    return iuse_actor::get_name();
}

void iuse_transform::finalize( const itype_id & )
{
    if( !target.is_valid() ) {
        debugmsg( "Invalid transform target: %s", target.c_str() );
    }

    if( !container.is_empty() ) {
        if( !container.is_valid() ) {
            debugmsg( "Invalid transform container: %s", container.c_str() );
        }

        item *dummy = item::spawn_temporary( target );
        if( ammo_qty > 1 && !dummy->count_by_charges() ) {
            debugmsg( "Transform target with container must be an item with charges, got non-charged: %s",
                      target.c_str() );
        }
    }
}

void iuse_transform::info( const item &it, std::vector<iteminfo> &dump ) const
{
    item &dummy = *item::spawn_temporary( target, calendar::turn, std::max( ammo_qty, 1 ) );
    if( it.has_flag( flag_FIT ) ) {
        dummy.set_flag( flag_FIT );
    }
    dump.emplace_back( "TOOL", string_format( _( "<bold>Turns into</bold>: %s" ),
                       dummy.tname() ) );
    if( countdown > 0 ) {
        dump.emplace_back( "TOOL", _( "Countdown: " ), countdown );
    }

    const auto *explosion_use = dummy.get_use( "explosion" );
    if( explosion_use != nullptr ) {
        explosion_use->get_actor_ptr()->info( it, dump );
    }
}

std::unique_ptr<iuse_actor> unpack_actor::clone() const
{
    return std::make_unique<unpack_actor>( *this );
}

void unpack_actor::load( const JsonObject &obj )
{
    obj.read( "group", unpack_group );
    obj.read( "items_fit", items_fit );
}

int unpack_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    std::vector<detached_ptr<item>> items = item_group::items_from( unpack_group, calendar::turn );
    item *last_armor = &null_item_reference();

    p.add_msg_if_player( _( "You unpack the %s." ), it.tname() );

    map &here = get_map();
    for( detached_ptr<item> &content : items ) {
        if( content->is_armor() ) {
            if( items_fit ) {
                content->set_flag( flag_FIT );
            } else if( content->typeId() == last_armor->typeId() ) {
                if( last_armor->has_flag( flag_FIT ) ) {
                    content->set_flag( flag_FIT );
                } else if( !last_armor->has_flag( flag_FIT ) ) {
                    content->unset_flag( flag_FIT );
                }
            }
            last_armor = &*content;
        }


        here.add_item_or_charges( p.pos(), std::move( content ) );
    }

    it.detach( );

    return 0;
}

void unpack_actor::info( const item &, std::vector<iteminfo> &dump ) const
{
    dump.emplace_back( "DESCRIPTION",
                       _( "This item could be unpacked to receive something." ) );
}

std::unique_ptr<iuse_actor> countdown_actor::clone() const
{
    return std::make_unique<countdown_actor>( *this );
}

void countdown_actor::load( const JsonObject &obj )
{
    obj.read( "name", name );
    obj.read( "interval", interval );
    obj.read( "message", message );
}

int countdown_actor::use( player &p, item &it, bool t, const tripoint &pos ) const
{
    if( t ) {
        return 0;
    }

    if( it.is_active() ) {
        return 0;
    }

    if( p.sees( pos ) && !message.empty() ) {
        p.add_msg_if_player( m_neutral, _( message ), it.tname() );
    }

    it.activate();
    it.set_counter( interval > 0 ? interval : it.type->countdown_interval );
    return 0;
}

ret_val<bool> countdown_actor::can_use( const Character &, const item &it, bool,
                                        const tripoint & ) const
{
    if( it.is_active() ) {
        return ret_val<bool>::make_failure( _( "It's already been triggered." ) );
    }

    return ret_val<bool>::make_success();
}

std::string countdown_actor::get_name() const
{
    if( !name.empty() ) {
        return name;
    }
    return iuse_actor::get_name();
}

void countdown_actor::info( const item &it, std::vector<iteminfo> &dump ) const
{
    dump.emplace_back( "TOOL", _( "Countdown: " ),
                       interval > 0 ? interval : it.type->countdown_interval );
    const auto countdown_actor = it.type->countdown_action.get_actor_ptr();
    if( countdown_actor != nullptr ) {
        countdown_actor->info( it, dump );
    }
}

std::unique_ptr<iuse_actor> explosion_iuse::clone() const
{
    return std::make_unique<explosion_iuse>( *this );
}

// For an explosion (which releases some kind of gas), this function
// calculates the points around that explosion where to create those
// gas fields.
// Those points must have a clear line of sight and a clear path to
// the center of the explosion.
// They must also be passable.
static std::vector<tripoint> points_for_gas_cloud( const tripoint &center, int radius )
{
    map &here = get_map();
    std::vector<tripoint> result;
    for( const auto &p : closest_points_first( center, radius ) ) {
        if( here.impassable( p ) ) {
            continue;
        }
        if( p != center ) {
            if( !here.clear_path( center, p, radius, 1, 100 ) ) {
                // Can not splatter gas from center to that point, something is in the way
                continue;
            }
        }
        result.push_back( p );
    }
    return result;
}

void explosion_iuse::load( const JsonObject &obj )
{
    if( obj.has_object( "explosion" ) ) {
        auto expl = obj.get_object( "explosion" );
        explosion = load_explosion_data( expl );
    }

    obj.read( "draw_explosion_radius", draw_explosion_radius );
    if( obj.has_member( "draw_explosion_color" ) ) {
        draw_explosion_color = color_from_string( obj.get_string( "draw_explosion_color" ) );
    }
    obj.read( "do_flashbang", do_flashbang );
    obj.read( "flashbang_player_immune", flashbang_player_immune );
    obj.read( "fields_radius", fields_radius );
    if( obj.has_member( "fields_type" ) || fields_radius > 0 ) {
        fields_type = field_type_id( obj.get_string( "fields_type" ) );
    }
    obj.read( "fields_min_intensity", fields_min_intensity );
    obj.read( "fields_max_intensity", fields_max_intensity );
    if( fields_max_intensity == 0 ) {
        fields_max_intensity = fields_type.obj().get_max_intensity();
    }
    obj.read( "emp_blast_radius", emp_blast_radius );
    obj.read( "scrambler_blast_radius", scrambler_blast_radius );
    obj.read( "sound_volume", sound_volume );
    obj.read( "sound_msg", sound_msg );
    obj.read( "no_deactivate_msg", no_deactivate_msg );
}

int explosion_iuse::use( player &p, item &it, bool t, const tripoint &pos ) const
{
    if( t ) {
        if( sound_volume >= 0 ) {
            sounds::sound( pos, sound_volume, sounds::sound_t::alarm,
                           sound_msg.empty() ? _( "Tick." ) : _( sound_msg ), true, "misc", "bomb_ticking" );
        }
    } else if( it.charges > 0 ) {
        if( p.has_item( it ) ) {
            if( no_deactivate_msg.empty() ) {
                p.add_msg_if_player( m_warning,
                                     _( "You've already set the %s's timer you might want to get away from it." ), it.tname() );
            } else {
                p.add_msg_if_player( m_info, _( no_deactivate_msg ), it.tname() );
            }
        }
        return 0;
    }
    if( it.charges == 0 ) {
        trigger_explosion( pos, it.activated_by );
    }
    return 1;
}

void explosion_iuse::trigger_explosion( const tripoint &pos, Creature *source ) const
{
    if( explosion ) {
        explosion_handler::explosion( pos, explosion, source );
    }

    if( draw_explosion_radius >= 0 ) {
        explosion_handler::draw_explosion( pos, draw_explosion_radius, draw_explosion_color, "explosion" );
    }
    if( do_flashbang ) {
        explosion_handler::flashbang( pos, flashbang_player_immune, "explosion" );
    }
    map &here = get_map();
    if( fields_radius >= 0 && fields_type.id() ) {
        std::vector<tripoint> gas_sources = points_for_gas_cloud( pos, fields_radius );
        for( auto &gas_source : gas_sources ) {
            const int field_intensity = rng( fields_min_intensity, fields_max_intensity );
            here.add_field( gas_source, fields_type, field_intensity, 1_turns );
        }
        if( source == &get_player_character() && source->has_trait( trait_PYROMANIA ) ) {
            if( fields_type == fd_fire || fd_incendiary ) {
                Character &p = get_player_character();
                p.add_morale( MORALE_PYROMANIA_STARTFIRE, 15, 15, 8_hours, 6_hours );
                p.rem_morale( MORALE_PYROMANIA_NOFIRE );
                p.add_msg_if_player( m_good, _( "Fire…  Good…" ) );
            }
        }
    }
    if( scrambler_blast_radius >= 0 ) {
        for( const tripoint &dest : here.points_in_radius( pos, scrambler_blast_radius ) ) {
            explosion_handler::scrambler_blast( dest );
        }
    }
    if( emp_blast_radius >= 0 ) {
        for( const tripoint &dest : here.points_in_radius( pos, emp_blast_radius ) ) {
            explosion_handler::emp_blast( dest );
        }
    }
}

void explosion_iuse::info( const item &, std::vector<iteminfo> &dump ) const
{
    if( explosion.damage > 0 ) {
        dump.emplace_back( "TOOL", _( "Blast damage at epicenter: " ),
                           explosion.damage );
        dump.emplace_back( "TOOL", _( "Blast radius: " ), static_cast<int>( explosion.radius ) );
    }
    const auto &sd = explosion.fragment;
    if( sd ) {
        dump.emplace_back( "TOOL", _( "Shrapnel damage: " ),
                           static_cast<int>( sd->impact.total_damage() ) );
        dump.emplace_back( "TOOL", _( "Shrapnel range: " ), sd->range );
    }

    // TODO: List other effects, like EMP and clouds
}

std::unique_ptr<iuse_actor> unfold_vehicle_iuse::clone() const
{
    return std::make_unique<unfold_vehicle_iuse>( *this );
}

void unfold_vehicle_iuse::load( const JsonObject &obj )
{
    vehicle_id = vproto_id( obj.get_string( "vehicle_name" ) );
    obj.read( "unfold_msg", unfold_msg );
    obj.read( "moves", moves );
    obj.read( "tools_needed", tools_needed );
}

int unfold_vehicle_iuse::use( player &p, item &it, bool, const tripoint & ) const
{
    if( p.is_underwater() ) {
        p.add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    if( p.is_mounted() ) {
        p.add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    for( const auto &tool : tools_needed ) {
        // Amount == -1 means need one, but don't consume it.
        if( !p.has_amount( tool.first, 1 ) ) {
            p.add_msg_if_player( _( "You need %s to do it!" ),
                                 item::nname( tool.first ) );
            return 0;
        }
    }

    vehicle *veh = get_map().add_vehicle( vehicle_id, p.pos(), 0_degrees, 0, 0, false, false, true );
    if( veh == nullptr ) {
        p.add_msg_if_player( m_info, _( "There's no room to unfold the %s." ), it.tname() );
        return 0;
    }
    veh->set_owner( p );

    // Mark the vehicle as foldable.
    veh->tags.insert( "convertible" );
    // Store the id of the item the vehicle is made of.
    veh->tags.insert( std::string( "convertible:" ) + it.typeId().str() );
    if( !unfold_msg.empty() ) {
        p.add_msg_if_player( _( unfold_msg ), it.tname() );
    }
    p.moves -= moves;
    // Restore HP of parts if we stashed them previously.
    if( it.has_var( "folding_bicycle_parts" ) ) {
        // Brand new, no HP stored
        return 1;
    }
    std::istringstream veh_data;
    const auto data = it.get_var( "folding_bicycle_parts" );
    veh_data.str( data );
    if( !data.empty() && data[0] >= '0' && data[0] <= '9' ) {
        // starts with a digit -> old format
        for( const vpart_reference &vpr : veh->get_all_parts() ) {
            int tmp;
            veh_data >> tmp;
            veh->set_hp( vpr.part(), tmp );
        }
    } else {
        try {
            JsonIn json( veh_data );
            // Load parts into a temporary vector to not override
            // cached values (like precalc, passenger_id, ...)
            std::vector<vehicle_part> parts;
            json.read( parts );
            for( size_t i = 0; i < parts.size() && i < static_cast<size_t>( veh->part_count() ); i++ ) {
                const vehicle_part &src = parts[i];
                vehicle_part &dst = veh->part( i );
                // and now only copy values, that are
                // expected to be consistent.
                veh->set_hp( dst, src.hp() );
                dst.blood = src.blood;
                // door state/amount of fuel/direction of headlight
                dst.ammo_set( src.ammo_current(), src.ammo_remaining() );
                dst.flags = src.flags;
            }
        } catch( const JsonError &e ) {
            debugmsg( "Error restoring vehicle: %s", e.c_str() );
        }
    }
    return 1;
}

std::unique_ptr<iuse_actor> consume_drug_iuse::clone() const
{
    return std::make_unique<consume_drug_iuse>( *this );
}

static effect_data load_effect_data( const JsonObject &e )
{
    time_duration time;
    if( e.has_string( "duration" ) ) {
        time = read_from_json_string<time_duration>( *e.get_raw( "duration" ), time_duration::units );
    } else {
        time = time_duration::from_turns( e.get_int( "duration", 0 ) );
    }
    if( e.get_bool( "permanent", false ) ) {
        effect_data ret( efftype_id( e.get_string( "id" ) ), time,
                         get_body_part_token( e.get_string( "bp", "NUM_BP" ) ) );
        ret.permanent = true;
        if( json_report_strict ) {
            try {
                e.throw_error( "Effect permanence has been moved to effect_type.  Set permanence there.",
                               "permanent" );
            } catch( const JsonError &ex ) {
                debugmsg( "\n%s", ex.what() );
            }
        }
        return ret;
    } else {
        return effect_data( efftype_id( e.get_string( "id" ) ), time,
                            get_body_part_token( e.get_string( "bp", "NUM_BP" ) ) );
    }
}

void consume_drug_iuse::load( const JsonObject &obj )
{
    obj.read( "activation_message", activation_message );
    obj.read( "charges_needed", charges_needed );
    obj.read( "tools_needed", tools_needed );

    if( obj.has_array( "effects" ) ) {
        for( const JsonObject e : obj.get_array( "effects" ) ) {
            effects.push_back( load_effect_data( e ) );
        }
    }
    obj.read( "stat_adjustments", stat_adjustments );
    obj.read( "fields_produced", fields_produced );
    obj.read( "moves", moves );
    obj.read( "fake_item", fake_item );
    obj.read( "lightweight_mod", lightweight_mod );
    obj.read( "tolerance_mod", tolerance_mod );
    obj.read( "tolerance_lightweight_effected", tolerance_lightweight_effected ); // default true
    lit_item = obj.get_string( "lit_item", lit_item );
    obj.read( "smoking_duration", smoking_duration );
    obj.read( "too_much_threshold", too_much_threshold );
    obj.read( "snippet_category", snippet_category );
    obj.read( "snippet_chance", snippet_chance );
    obj.read( "do_weed_msg",
              do_weed_msg ); // i wish i didn't have to do this, but the weed_msg function can't really be easily JSONified

    if( obj.has_array( "addiction_type_too_much" ) ) {
        for( const JsonArray pair : obj.get_array( "addiction_type_too_much" ) ) {
            if( pair.size() >= 2 ) {
                addiction_type_too_much.emplace_back( pair.get_string( 0 ), pair.get_string( 1 ) );
            }
        }
    }

    for( JsonArray vit : obj.get_array( "vitamins" ) ) {
        auto lo = vit.get_int( 1 );
        auto hi = vit.size() >= 3 ? vit.get_int( 2 ) : lo;
        vitamins.emplace( vitamin_id( vit.get_string( 0 ) ), std::make_pair( lo, hi ) );
    }

    used_up_item = obj.get_string( "used_up_item", used_up_item );

}

void consume_drug_iuse::info( const item &, std::vector<iteminfo> &dump ) const
{
    const std::string vits = enumerate_as_string( vitamins.begin(), vitamins.end(),
    []( const decltype( vitamins )::value_type & v ) {
        const time_duration rate = get_player_character().vitamin_rate( v.first );
        if( rate <= 0_turns ) {
            return std::string();
        }
        const int lo = static_cast<int>( v.second.first  * rate / 1_days * 100 );
        const int hi = static_cast<int>( v.second.second * rate / 1_days * 100 );

        return string_format( lo == hi ? "%s (%i%%)" : "%s (%i-%i%%)", v.first.obj().name(), lo,
                              hi );
    } );

    if( !vits.empty() ) {
        dump.emplace_back( "TOOL", _( "Vitamins (RDA): " ), vits );
    }

    if( tools_needed.contains( itype_syringe ) ) {
        dump.emplace_back( "TOOL", _( "You need a <info>syringe</info> to inject this drug." ) );
    }
}

int consume_drug_iuse::use( player &p, item &it, bool, const tripoint & ) const
{
    auto need_these = tools_needed;
    if( need_these.contains( itype_syringe ) && p.has_bionic( bio_syringe ) ) {
        need_these.erase( itype_syringe ); // no need for a syringe with bionics like these!
    }

    // Check prerequisites first.
    for( const auto &tool : need_these ) {
        // Amount == -1 means need one, but don't consume it.
        if( !p.has_amount( tool.first, 1 ) ) {
            p.add_msg_player_or_say( _( "You need %1$s to consume %2$s!" ),
                                     _( "I need a %1$s to consume %2$s!" ),
                                     item::nname( tool.first ),
                                     it.type_name( 1 ) );
            return 0;
        }
    }
    for( const auto &consumable : charges_needed ) {
        // Amount == -1 means need one, but don't consume it.
        if( !p.has_charges( consumable.first, ( consumable.second == -1 ) ?
                            1 : consumable.second ) ) {
            p.add_msg_player_or_say( _( "You need %1$s to consume %2$s!" ),
                                     _( "I need a %1$s to consume %2$s!" ),
                                     item::nname( consumable.first ),
                                     it.type_name( 1 ) );
            return 0;
        }
    }

    // this is a smokeable item, we need to make sure player isnt already smoking (ripped from iuse::smoking)
    if( !lit_item.empty() ) {
        // make sure we're not already smoking something
        auto cigs = p.items_with( []( const item & it ) {
            return it.is_active() && it.has_flag( flag_LITCIG );
        } );
        if( !cigs.empty() ) {
            p.add_msg_if_player( m_info, _( "You're already smoking a %s!" ), cigs[0]->tname() );
            return 0;
        }
    }

    // Output message.
    p.add_msg_if_player( _( activation_message ), it.type_name( 1 ) );

    if( smoking_duration ) {
        detached_ptr<item> cig;
        cig = item::spawn( lit_item, calendar::turn );
        time_duration converted_time = time_duration::from_minutes( smoking_duration );

        cig->activate();
        cig->set_counter( to_turns<int>( converted_time ) );

        p.i_add( std::move( cig ) );
    }

    if( do_weed_msg ) {
        if( one_in( snippet_chance ) ) {
            weed_msg( p );
        }
    }

    // item used to "fake" addiction (ripped from old ecig iuse)
    if( !fake_item.empty() ) {
        item *dummy_item = item::spawn_temporary( fake_item, calendar::turn );
        p.consume_effects( *dummy_item );
    }

    // Apply the various effects.
    for( const auto &eff : effects ) {
        time_duration dur = eff.duration;
        if( tolerance_lightweight_effected ) {
            if( p.has_trait( trait_TOLERANCE ) ) {
                dur *= tolerance_mod;
            } else if( p.has_trait( trait_LIGHTWEIGHT ) ) {
                dur *= lightweight_mod;
            }
        }

        // only way i could figure out how to do this
        std::unordered_map<std::string, efftype_id> effect_map = {
            {"cig", effect_cig}
            // Add other mappings as needed. I think cigs are the only thing this applies to at the moment.
        };

        // check if effect were applying is connected to an addiction type
        for( const auto &entry : addiction_type_too_much ) {
            const std::string &attm_effect = entry.first;
            const std::string &attm_addiction_type = entry.second;

            auto it = effect_map.find( attm_effect );
            if( it != effect_map.end() ) {
                const efftype_id &id = it->second;
                if( id.obj() == eff.id.obj() ) {
                    if( p.get_effect_dur( id ) > time_duration::from_minutes( too_much_threshold ) *
                        ( p.addiction_level(
                              addiction_type( attm_addiction_type ) ) + 1 ) ) {
                        p.add_msg_if_player( m_bad, _( "Ugh, too much %s… you feel nasty." ), attm_addiction_type );
                        break;
                    }
                }
            }
        }

        p.add_effect( eff.id, eff.duration, convert_bp( eff.bp ) );
        if( eff.permanent ) {
            p.get_effect( eff.id, convert_bp( eff.bp ) ).set_permanent();
        }
    }

    for( const auto &stat_adjustment : stat_adjustments ) {
        p.mod_stat( stat_adjustment.first, stat_adjustment.second );
    }
    map &here = get_map();
    for( const auto &field : fields_produced ) {
        const field_type_id fid = field_type_id( field.first );
        for( int i = 0; i < 3; i++ ) {
            here.add_field( {p.posx() + rng( -2, 2 ), p.posy() + rng( -2, 2 ), p.posz()},
                            fid,
                            field.second );
        }
    }

    // for vitamins that accumulate (max > 0) multivitamins risk causing hypervitaminosis
    for( const auto &v : vitamins ) {
        // players with mutations that remove the requirement for a vitamin cannot suffer accumulation of it
        p.vitamin_mod( v.first, rng( v.second.first, v.second.second ),
                       p.vitamin_rate( v.first ) <= 0_turns );
    }

    if( !snippet_category.empty() ) {
        std::string snippet_string = "";
        snippet_string = SNIPPET.random_from_category( snippet_category ).value_or(
                             translation() ).translated();
        if( one_in( snippet_chance ) ) {
            p.add_msg_if_player( _( "%s" ), snippet_string );
        }
    }

    // Consume charges.
    for( const auto &consumable : charges_needed ) {
        if( consumable.second != -1 ) {
            p.use_charges( consumable.first, consumable.second );
        }
    }

    if( !used_up_item.empty() ) {
        p.i_add_or_drop( item::spawn( used_up_item, it.birthday() ) );
    }

    p.moves -= moves;
    return it.type->charges_to_use();
}

std::unique_ptr<iuse_actor> delayed_transform_iuse::clone() const
{
    return std::make_unique<delayed_transform_iuse>( *this );
}

void delayed_transform_iuse::load( const JsonObject &obj )
{
    iuse_transform::load( obj );
    not_ready_msg = obj.get_string( "not_ready_msg" );
    transform_age = obj.get_int( "transform_age" );
}

int delayed_transform_iuse::time_to_do( const item &it ) const
{
    // TODO: change return type to time_duration
    return transform_age - to_turns<int>( it.age() );
}

int delayed_transform_iuse::use( player &p, item &it, bool t, const tripoint &pos ) const
{
    if( time_to_do( it ) > 0 ) {
        p.add_msg_if_player( m_info, _( not_ready_msg ) );
        return 0;
    }
    return iuse_transform::use( p, it, t, pos );
}

std::unique_ptr<iuse_actor> set_transform_iuse::clone() const
{
    return std::make_unique<set_transform_iuse>( *this );
}

void set_transform_iuse::load( const JsonObject &obj )
{
    iuse_transform::load( obj );
    obj.read( "turn_off", turn_off );
    obj.read( "flag", flag );
    if( !obj.read( "set_charges_msg", set_charges_msg ) ) {
        set_charges_msg = to_translation( "The %s is empty!" );
    }

    if( !obj.read( "set_charges", set_charges ) ) {
        set_charges = 0;
    }
    set_charges = std::max( set_charges, 0 );
}

int set_transform_iuse::use( player &p, item &it, bool t, const tripoint &pos ) const
{
    if( t ) {
        return 0; // invoked from active item processing, do nothing.
    }

    const bool possess = p.has_item( it ) ||
                         ( it.has_flag( flag_ALLOWS_REMOTE_USE ) && square_dist( p.pos(), pos ) == 1 );

    if( set_charges ) {
        if( it.is_power_armor() && character_funcs::can_interface_armor( p ) ) {
            if( !p.has_power() ) {
                if( possess ) {
                    p.add_msg_if_player( m_info, set_charges_msg, it.tname() );
                }
                return 0;
            }
        } else if( it.units_remaining( p ) < set_charges ) {
            if( possess ) {
                p.add_msg_if_player( m_info, set_charges_msg, it.tname() );
            }
            return 0;
        }
    }

    iuse_transform::use( p, it, t, pos );

    const flag_id f( flag );
    for( auto &elem : p.worn ) {
        if( elem->has_flag( f ) && elem->is_active() == turn_off ) {
            if( elem->type->can_use( "set_transformed" ) ) {
                const set_transformed_iuse *actor = dynamic_cast<const set_transformed_iuse *>
                                                    ( elem->get_use( "set_transformed" )->get_actor_ptr() );
                if( actor == nullptr ) {
                    debugmsg( "iuse_actor type descriptor and actual type mismatch" );
                } else {
                    actor->bypass( p, *elem, t, pos );
                }
            } else {
                debugmsg( "Expected set_transformed function" );
            }
        }
    }
    return 0;
}

std::unique_ptr<iuse_actor> set_transformed_iuse::clone() const
{
    return std::make_unique<set_transformed_iuse>( *this );
}

void set_transformed_iuse::load( const JsonObject &obj )
{
    iuse_transform::load( obj );
    obj.read( "restricted", restricted );
    obj.read( "dependencies", dependencies );
}

int set_transformed_iuse::use( player &p, item &it, bool t, const tripoint &pos ) const
{
    if( t ) {
        return 0; // invoked from active item processing, do nothing.
    }

    iuse_transform::use( p, it, t, pos );

    return 0;
}

int set_transformed_iuse::bypass( player &p, item &it, bool t, const tripoint &pos ) const
{
    return iuse_transform::use( p, it, t, pos );
}

ret_val<bool> set_transformed_iuse::can_use( const Character &, const item &, bool,
        const tripoint & ) const
{
    if( restricted ) {
        return ret_val<bool>::make_failure( _( "Activate via main piece." ) );
    }
    return ret_val<bool>::make_success();

}

std::unique_ptr<iuse_actor> place_monster_iuse::clone() const
{
    return std::make_unique<place_monster_iuse>( *this );
}

void place_monster_iuse::load( const JsonObject &obj )
{
    mtypeid = mtype_id( obj.get_string( "monster_id" ) );
    obj.read( "friendly_msg", friendly_msg );
    obj.read( "hostile_msg", hostile_msg );
    obj.read( "difficulty", difficulty );
    obj.read( "moves", moves );
    obj.read( "place_randomly", place_randomly );
    obj.read( "is_pet", is_pet );
    if( obj.has_array( "skills" ) ) {
        JsonArray skills_ja = obj.get_array( "skills" );
        for( JsonValue s : skills_ja ) {
            skills.emplace( s.get_string() );
        }
    }
}

int place_monster_iuse::use( player &p, item &it, bool, const tripoint &pos ) const
{
    mtype_id spawn_id = mtypeid;

    int diff_mod = 1;
    bool place_random = place_randomly;
    // ugly hack, sorry
    if( it.has_var( "place_monster_override" ) ) {
        spawn_id = mtype_id( it.get_var( "place_monster_override" ) );
        // currently cant use this to tame an otherwise untameable animal
        diff_mod = 999;
    }

    if( it.has_flag( flag_RADIO_MOD ) ) {
        place_random = true;
        it.activate();
    }

    shared_ptr_fast<monster> newmon_ptr = make_shared_fast<monster>( spawn_id );
    monster &newmon = *newmon_ptr;
    newmon.init_from_item( it );

    tripoint pnt = it.is_active() ? pos : p.pos();

    if( it.has_var( "place_monster_override" ) ) {
        newmon.no_extra_death_drops = true;
        it.deactivate();
    }
    if( place_random ) {
        // place_critter_around returns the same pointer as its parameter (or null)
        // Allow position to be different from the player for tossed or launched items
        if( !g->place_critter_around( newmon_ptr, pnt, 1 ) ) {
            p.add_msg_if_player( m_info, _( "There is no adjacent square to release the %s in!" ),
                                 newmon.name() );
            // If remotely triggered due to ACT_ON_RANGED_HIT, set it back to being inactive so it won't spawn infinitely
            it.deactivate();
            return 0;
        }
    } else {
        const std::string query = string_format( _( "Place the %s where?" ), newmon.name() );
        const std::optional<tripoint> pnt_ = choose_adjacent( query );
        if( !pnt_ ) {
            return 0;
        }
        // place_critter_at returns the same pointer as its parameter (or null)
        if( !g->place_critter_at( newmon_ptr, *pnt_ ) ) {
            p.add_msg_if_player( m_info, _( "You cannot place a %s there." ), newmon.name() );
            return 0;
        }
    }
    // If it's active then we know it was triggered by ACT_ON_RANGED_HIT and did not deactivate from lack of room earlier
    // If so, don't drain moves from remote deployment since it would trigger after the throw
    if( !it.is_active() ) {
        p.moves -= moves;
    }
    if( !newmon.has_flag( MF_INTERIOR_AMMO ) ) {
        for( auto &amdef : newmon.ammo ) {
            item &ammo_item = *item::spawn_temporary( amdef.first, calendar::start_of_cataclysm );
            const int available = p.charges_of( amdef.first );
            if( available == 0 ) {
                amdef.second = 0;
                p.add_msg_if_player( m_info,
                                     _( "If you had standard factory-built %1$s bullets, you could load the %2$s." ),
                                     ammo_item.type_name( 2 ), newmon.name() );
                continue;
            }
            // Don't load more than the default from the monster definition.
            ammo_item.charges = std::min( available, amdef.second );
            p.use_charges( amdef.first, ammo_item.charges );
            //~ First %s is the ammo item (with plural form and count included), second is the monster name
            p.add_msg_if_player( vgettext( "You load %1$d x %2$s round into the %3$s.",
                                           "You load %1$d x %2$s rounds into the %3$s.", ammo_item.charges ),
                                 ammo_item.charges, ammo_item.type_name( ammo_item.charges ),
                                 newmon.name() );
            amdef.second = ammo_item.charges;
        }
    }
    int skill_offset = 0;
    for( const skill_id &sk : skills ) {
        skill_offset += p.get_skill_level( sk );
    }
    /** @EFFECT_INT increases chance of a placed turret being friendly */
    /** Full-on pets also auto-succeed if we've already succeeded before deactivating it */
    if( rng( 0, p.int_cur ) + skill_offset < rng( 0, 2 * ( difficulty * diff_mod ) ) &&
        !it.has_flag( flag_SPAWN_FRIENDLY ) ) {
        if( hostile_msg.empty() ) {
            p.add_msg_if_player( m_bad, _( "The %s scans you and makes angry beeping noises!" ),
                                 newmon.name() );
        } else {
            p.add_msg_if_player( m_bad, "%s", _( hostile_msg ) );
        }
    } else {
        if( friendly_msg.empty() ) {
            p.add_msg_if_player( m_warning, _( "The %s emits an IFF beep as it scans you." ),
                                 newmon.name() );
        } else {
            p.add_msg_if_player( m_warning, "%s", _( friendly_msg ) );
        }
        newmon.friendly = -1;
        if( is_pet ) {
            newmon.add_effect( effect_pet, 1_turns );
        }
    }
    // mark artifical womb as dirty, and convert it
    if( it.has_var( "place_monster_override" ) ) {
        it.convert( itype_id( "embryo_empty" ) );
        it.clear_vars();
        it.faults.emplace( fault_bionic_nonsterile );
    }
    // Transfer label from the item to monster nickname
    if( it.has_var( "item_label" ) ) {
        newmon.unique_name = it.get_var( "item_label" );
    }
    // TODO: add a flag instead of monster id or something?
    if( newmon.type->id == mtype_id( "mon_laserturret" ) && !g->is_in_sunlight( newmon.pos() ) ) {
        p.add_msg_if_player( _( "A flashing LED on the laser turret appears to indicate low light." ) );
    }
    return 1;
}

std::unique_ptr<iuse_actor> place_npc_iuse::clone() const
{
    return std::make_unique<place_npc_iuse>( *this );
}

void place_npc_iuse::load( const JsonObject &obj )
{
    npc_class_id = string_id<npc_template>( obj.get_string( "npc_class_id" ) );
    obj.read( "summon_msg", summon_msg );
    obj.read( "moves", moves );
    obj.read( "place_randomly", place_randomly );
}

int place_npc_iuse::use( player &p, item &, bool, const tripoint & ) const
{
    map &here = get_map();
    std::optional<tripoint> target_pos;
    if( place_randomly ) {
        const tripoint_range<tripoint> target_range = points_in_radius( p.pos(), 1 );
        target_pos = random_point( target_range, []( const tripoint & t ) {
            return !get_map().passable( t );
        } );
    } else {
        const std::string query = _( "Place npc where?" );
        target_pos = choose_adjacent( _( "Place npc where?" ) );
    }
    if( !target_pos ) {
        return 0;
    }
    if( !here.passable( target_pos.value() ) ) {
        p.add_msg_if_player( m_info, _( "There is no square to spawn npc in!" ) );
        return 0;
    }

    here.place_npc( target_pos.value().xy(), npc_class_id );
    p.mod_moves( -moves );
    p.add_msg_if_player( m_info, "%s", _( summon_msg ) );
    return 1;
}

std::unique_ptr<iuse_actor> deploy_furn_actor::clone() const
{
    return std::make_unique<deploy_furn_actor>( *this );
}

void deploy_furn_actor::info( const item &, std::vector<iteminfo> &dump ) const
{
    std::vector<std::string> can_function_as;
    const furn_t &the_furn = furn_type.obj();
    const std::string furn_name = the_furn.name();
    const std::set<itype_id> &pseudo_list = the_furn.crafting_pseudo_items;

    if( the_furn.workbench ) {
        can_function_as.emplace_back( _( "a <info>crafting station</info>" ) );
    }
    if( the_furn.has_flag( "BUTCHER_EQ" ) ) {
        can_function_as.emplace_back(
            _( "a place to hang <info>corpses for butchering</info>" ) );
    }
    if( the_furn.has_flag( "FLAT_SURF" ) ) {
        can_function_as.emplace_back(
            _( "a flat surface to <info>butcher</info> onto or <info>eat meals</info> from" ) );
    }
    if( the_furn.has_flag( "CAN_SIT" ) ) {
        can_function_as.emplace_back( _( "a place to <info>sit</info>" ) );
    }
    if( the_furn.has_flag( "HIDE_PLACE" ) ) {
        can_function_as.emplace_back( _( "a place to <info>hide</info>" ) );
    }
    if( the_furn.has_flag( "FIRE_CONTAINER" ) ) {
        can_function_as.emplace_back( _( "a safe place to <info>contain a fire</info>" ) );
    }
    if( pseudo_list.contains( itype_char_smoker ) ) {
        can_function_as.emplace_back( _( "a place to <info>smoke or dry food</info> for preservation" ) );
    }

    if( can_function_as.empty() ) {
        dump.emplace_back( "DESCRIPTION",
                           string_format( _( "Can be <info>activated</info> to deploy as furniture (<stat>%s</stat>)." ),
                                          furn_name ) );
    } else {
        std::string furn_usages = enumerate_as_string( can_function_as, enumeration_conjunction::or_ );
        dump.emplace_back( "DESCRIPTION",
                           string_format(
                               _( "Can be <info>activated</info> to deploy as furniture (<stat>%s</stat>), which can then be used as %s." ),
                               furn_name, furn_usages ) );
    }
}

void deploy_furn_actor::load( const JsonObject &obj )
{
    furn_type = furn_str_id( obj.get_string( "furn_type" ) );
}

int deploy_furn_actor::use( player &p, item &it, bool t, const tripoint &pos ) const
{
    if( t ) {
        return 0;
    }

    if( p.is_mounted() ) {
        p.add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    tripoint pnt = pos;
    if( const std::optional<tripoint> pnt_ = choose_adjacent( _( "Deploy where?" ) ) ) {
        pnt = *pnt_;
    } else {
        return 0;
    }

    if( pnt == p.pos() ) {
        p.add_msg_if_player( m_info,
                             _( "You attempt to become one with the furniture.  It doesn't work." ) );
        return 0;
    }

    map &here = get_map();
    optional_vpart_position veh_there = here.veh_at( pnt );
    if( veh_there.has_value() ) {
        // TODO: check for protrusion+short furniture, wheels+tiny furniture, NOCOLLIDE flag, etc.
        // and/or integrate furniture deployment with construction (which already seems to perform these checks sometimes?)
        p.add_msg_if_player( m_info, _( "The space under %s is too cramped to deploy a %s in." ),
                             veh_there.value().vehicle().disp_name(), it.tname() );
        return 0;
    }

    // For example: dirt = 2, long grass = 3
    if( here.move_cost( pnt ) != 2 && here.move_cost( pnt ) != 3 ) {
        p.add_msg_if_player( m_info, _( "You can't deploy a %s there." ), it.tname() );
        return 0;
    }

    if( here.has_furn( pnt ) ) {
        p.add_msg_if_player( m_info, _( "There is already furniture at that location." ) );
        return 0;
    }

    here.furn_set( pnt, furn_type );
    p.mod_moves( to_turns<int>( 2_seconds ) );
    return 1;
}

std::unique_ptr<iuse_actor> reveal_map_actor::clone() const
{
    return std::make_unique<reveal_map_actor>( *this );
}

void reveal_map_actor::load( const JsonObject &obj )
{
    radius = obj.get_int( "radius" );
    message = obj.get_string( "message" );
    std::string ter;
    ot_match_type ter_match_type;
    for( const JsonValue entry : obj.get_array( "terrain" ) ) {
        if( entry.test_string() ) {
            ter = entry.get_string();
            ter_match_type = ot_match_type::contains;
        } else {
            JsonObject jo = entry.get_object();
            ter = jo.get_string( "om_terrain" );
            ter_match_type = jo.get_enum_value<ot_match_type>( "om_terrain_match_type",
                             ot_match_type::contains );
        }
        omt_types.emplace_back( ter, ter_match_type );
    }
    if( obj.has_array( "terrain_view" ) ) {
        for( const JsonValue entry : obj.get_array( "terrain_view" ) ) {
            if( entry.test_string() ) {
                ter = entry.get_string();
                ter_match_type = ot_match_type::contains;
            } else {
                JsonObject jo = entry.get_object();
                ter = jo.get_string( "om_terrain" );
                ter_match_type = jo.get_enum_value<ot_match_type>( "om_terrain_match_type",
                                 ot_match_type::contains );
            }
            omt_types_view.emplace_back( ter, ter_match_type );
        }
    } else {
        omt_types_view = omt_types;
    }
    if( obj.has_array( "terrain_view_exclude" ) ) {
        for( const JsonValue entry : obj.get_array( "terrain_view_exclude" ) ) {
            if( entry.test_string() ) {
                ter = entry.get_string();
                ter_match_type = ot_match_type::contains;
            } else {
                JsonObject jo = entry.get_object();
                ter = jo.get_string( "om_terrain" );
                ter_match_type = jo.get_enum_value<ot_match_type>( "om_terrain_match_type",
                                 ot_match_type::contains );
            }
            omt_types_view_exclude.emplace_back( ter, ter_match_type );
        }
    } else {
        omt_types_view_exclude.emplace_back( "subway", ot_match_type::contains );
        omt_types_view_exclude.emplace_back( "hiway", ot_match_type::contains );
        omt_types_view_exclude.emplace_back( "road", ot_match_type::contains );
        omt_types_view_exclude.emplace_back( "forest_trail", ot_match_type::contains );
        omt_types_view_exclude.emplace_back( "bridge", ot_match_type::contains );
        omt_types_view_exclude.emplace_back( "roof", ot_match_type::contains );
    };
}

void reveal_map_actor::reveal_targets( const tripoint_abs_omt &map ) const
{
    omt_find_params params{};
    params.search_range = { 0, radius };
    params.search_layers = omt_find_all_layers;
    params.types = omt_types;
    params.existing_only = false;
    params.popup = make_shared_fast<throbber_popup>( _( "Please wait…" ) );

    /*
    * Stagger parallel map generation starting from center (0), outwards
    * so the generated maps have a neighbor to latch onto when generating roads/rivers
    * 5 4 3 2 3 4 5
    * 4 3 2 1 2 3 4
    * 3 2 1 0 1 2 3
    * 4 3 2 1 2 3 4
    * 5 4 3 2 3 4 5
    */

    const point_abs_om origin_om_pos = project_to<coords::om>( map.xy() );

    // Generate a Square fitting the requested map radius
    const point_abs_omt omt_bb_min = map.xy() - point_rel_omt{ radius, radius };
    const point_abs_omt omt_bb_max = map.xy() + point_rel_omt{ radius, radius };

    // OM Corners of bounding box
    const point_abs_om om_bb_min = project_to<coords::om>( omt_bb_min );
    const point_abs_om om_bb_max = project_to<coords::om>( omt_bb_max );

    // Iterate through range [om_bb_min, om_bb_max] to get the OM we want, then sort by manhattan distance
    std::map<int, std::vector<point_abs_om>> om_to_generate;
    for( int x = om_bb_min.x(); x <= om_bb_max.x(); ++x ) {
        for( int y = om_bb_min.y(); y <= om_bb_max.y(); ++y ) {
            auto dist = manhattan_dist( origin_om_pos, { x, y } );
            auto &vec =
                om_to_generate[dist]; // if the vector for this distance doesn't exist it will be created empty
            vec.emplace_back( x, y );
        }
    }

    for( const auto& [_, to_gen] : om_to_generate ) {
        overmap_buffer.generate( to_gen );
    }

    const auto places = overmap_buffer.find_all( map, params );
    for( auto &place : places ) {
        overmap_buffer.reveal( place, 0 );
    }
}

void reveal_map_actor::show_revealed( player &p, item &item, const tripoint_abs_omt &center ) const
{
    uistate.overmap_highlighted_omts.clear();

    omt_find_params params{};
    params.search_range = { 0, radius };
    params.types = omt_types_view;
    params.exclude_types = omt_types_view_exclude;
    params.existing_only = true;
    // TODO: Add support for variable reveal z-range to reveal_map iuse_action JSON
    params.search_layers = omt_find_all_layers;
    params.explored = false;
    params.popup = make_shared_fast<throbber_popup>( _( "Please wait…" ) );

    const auto places = overmap_buffer.find_all( center, params );

    // Delete popup after search is done, before showing uilist
    params.popup = nullptr;

    // Group tiles by name
    std::multimap<std::string, tripoint_abs_omt> mm;
    std::set<std::string> utypes;
    for( auto &place : places ) {
        auto desc = overmap_buffer.ter( place ).id().obj().get_name();
        mm.insert( { desc, place } );
        utypes.insert( desc );
    }

    if( utypes.empty() ) {
        p.add_msg_if_player( _( "There isn't anything new on the %s." ), item.tname() );
        return;
    }

    // Show selector for each group
    std::vector<std::string> otypes( utypes.begin(), utypes.end() );
    uilist ui;
    for( uint64_t i = 0; i < otypes.size(); ++i ) {
        auto &desc = otypes[i];
        ui.addentry( i, true, MENU_AUTOASSIGN, string_format( "%s (%d)", desc, mm.count( desc ) ) );
    }
    ui.query();

    if( ui.ret < 0 ) {
        return;
    }

    const tripoint_abs_omt plrPos = p.global_omt_location();
    auto eqRange = mm.equal_range( otypes[ui.ret] );

    // TODO: Cluster tripoints to collapse direct neighbor tiles (helipads, etc)?

    const auto sz = std::distance( eqRange.first, eqRange.second );

    // Shouldn't ever be hit, since multimap shouldn't have an entry with no overmap tiles, but
    if( sz == 0 ) {
        return;
    }

    std::transform(
        eqRange.first, eqRange.second,
        std::inserter( uistate.overmap_highlighted_omts, uistate.overmap_highlighted_omts.end() ),
        []( const auto & e ) -> tripoint_abs_omt { return e.second; } );

    // Only one overmap tile of type
    if( sz == 1 ) {
        ui::omap::choose_point( eqRange.first->second );
        return;
    }

    ui.reset();
    ui.addentry( 0, true, 'c', _( "Closest" ) );
    ui.addentry( 1, true, 'r', _( "Random" ) );
    ui.query();

    if( ui.ret < 0 ) {
        return;
    }

    if( ui.ret == 1 ) {
        // Pick random
        auto it = eqRange.first;
        std::advance( it, rng( 0, sz - 1 ) );
        ui::omap::choose_point( it->second );
    } else {
        // Pick closest
        const auto pred_dist = [&]( const std::pair<std::string, tripoint_abs_omt> &a,
        const std::pair<std::string, tripoint_abs_omt> &b ) {
            auto da = trig_dist_squared( plrPos.raw(), a.second.raw() );
            auto db = trig_dist_squared( plrPos.raw(), b.second.raw() );
            return da < db;
        };
        const auto it = std::min_element( eqRange.first, eqRange.second, pred_dist );
        ui::omap::choose_point( it->second );
    }
}

int reveal_map_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    if( !it.already_used_by_player( p ) && g->get_levz() < 0 ) {
        p.add_msg_if_player( _( "You should read your %s when you get to the surface." ),
                             it.tname() );
        return 0;
    } else if( !character_funcs::can_see_fine_details( p ) ) {
        p.add_msg_if_player( _( "It's too dark to read." ) );
        return 0;
    }

    const tripoint_abs_omt plrPos = p.global_omt_location();
    const tripoint_abs_omt mapPos( it.get_var( "reveal_map_center_omt", plrPos.raw() ) );

    if( it.already_used_by_player( p ) ) {
        show_revealed( p, it, mapPos );
        return 0;
    }

    reveal_targets( mapPos );
    if( !message.empty() ) {
        p.add_msg_if_player( m_good, "%s", _( message ) );
    }
    it.mark_as_used_by_player( p );
    show_revealed( p, it, mapPos );
    return 0;
}

void firestarter_actor::load( const JsonObject &obj )
{
    moves_cost_fast = obj.get_int( "moves", moves_cost_fast );
    moves_cost_slow = obj.get_int( "moves_slow", moves_cost_fast * 10 );
    need_sunlight = obj.get_bool( "need_sunlight", false );
}

std::unique_ptr<iuse_actor> firestarter_actor::clone() const
{
    return std::make_unique<firestarter_actor>( *this );
}

bool firestarter_actor::prep_firestarter_use( const player &p, tripoint &pos )
{
    // checks for fuel are handled by use and the activity, not here
    if( pos == p.pos() ) {
        if( const std::optional<tripoint> pnt_ = choose_adjacent( _( "Light where?" ) ) ) {
            pos = *pnt_;
        } else {
            return false;
        }
    }
    if( pos == p.pos() ) {
        p.add_msg_if_player( m_info, _( "You would set yourself on fire." ) );
        p.add_msg_if_player( _( "But you're already smokin' hot." ) );
        return false;
    }
    map &here = get_map();
    if( here.get_field( pos, fd_fire ) ) {
        // check if there's already a fire
        p.add_msg_if_player( m_info, _( "There is already a fire." ) );
        return false;
    }
    // Check for a brazier.
    bool has_unactivated_brazier = false;
    for( const item * const &i : here.i_at( pos ) ) {
        if( i->typeId() == itype_brazier ) {
            has_unactivated_brazier = true;
        }
    }
    return !has_unactivated_brazier ||
           query_yn(
               _( "There's a brazier there but you haven't set it up to contain the fire.  Continue?" ) );
}

void firestarter_actor::resolve_firestarter_use( player &p, const tripoint &pos )
{
    if( get_map().add_field( pos, fd_fire, 1, 10_minutes ) ) {
        if( !p.has_trait( trait_PYROMANIA ) ) {
            p.add_msg_if_player( _( "You successfully light a fire." ) );
        } else {
            if( one_in( 4 ) ) {
                p.add_msg_if_player( m_mixed,
                                     _( "You light a fire, but it isn't enough.  You need to light more." ) );
            } else {
                p.add_msg_if_player( m_good, _( "You happily light a fire." ) );
                p.add_morale( MORALE_PYROMANIA_STARTFIRE, 5, 10, 6_hours, 4_hours );
                p.rem_morale( MORALE_PYROMANIA_NOFIRE );
            }
        }
    }
}

ret_val<bool> firestarter_actor::can_use( const Character &p, const item &it, bool,
        const tripoint & ) const
{
    if( p.is_underwater() ) {
        return ret_val<bool>::make_failure( _( "You can't do that while underwater." ) );
    }

    if( !( it.has_flag( flag_USE_UPS ) && p.has_charges( itype_UPS, it.ammo_required() ) ) &&
        ( it.ammo_remaining() < it.ammo_required() ) ) {
        return ret_val<bool>::make_failure( _( "This tool doesn't have enough charges." ) );
    }

    if( need_sunlight && light_mod( p.pos() ) <= 0.0f ) {
        return ret_val<bool>::make_failure( _( "You need direct sunlight to light a fire with this." ) );
    }

    return ret_val<bool>::make_success();
}

float firestarter_actor::light_mod( const tripoint &pos ) const
{
    if( !need_sunlight ) {
        return 1.0f;
    }

    const float light_level = g->natural_light_level( pos.z );
    if( get_weather().weather_id->sun_intensity >= sun_intensity_type::normal &&
        light_level >= 60.0f && weather::is_sheltered( get_map(), pos ) ) {
        return std::pow( light_level / 80.0f, 8 );
    }

    return 0.0f;
}

int firestarter_actor::moves_cost_by_fuel( const tripoint &pos ) const
{
    map &here = get_map();
    if( here.flammable_items_at( pos, 100 ) ) {
        return moves_cost_fast;
    }

    if( here.flammable_items_at( pos, 10 ) ) {
        return ( moves_cost_slow + moves_cost_fast ) / 2;
    }

    return moves_cost_slow;
}

int firestarter_actor::use( player &p, item &it, bool t, const tripoint &spos ) const
{
    if( t ) {
        return 0;
    }

    tripoint pos = spos;
    float light = light_mod( p.pos() );
    if( !prep_firestarter_use( p, pos ) ) {
        return 0;
    }

    double skill_level = p.get_skill_level( skill_survival );
    /** @EFFECT_SURVIVAL speeds up fire starting */
    float moves_modifier = std::pow( 0.8, std::min( 5.0, skill_level ) );
    const int moves_base = moves_cost_by_fuel( pos );
    const double moves_per_turn = to_moves<double>( 1_turns );
    const int min_moves = std::min<int>(
                              moves_base, std::sqrt( 1 + moves_base / moves_per_turn ) * moves_per_turn );
    const int moves = std::max<int>( min_moves, moves_base * moves_modifier ) / light;
    if( moves > to_moves<int>( 1_minutes ) ) {
        // If more than 1 minute, inform the player
        p.add_msg_if_player( m_info, need_sunlight ?
                             _( "If the current weather holds, it will take around %d minutes to light a fire." ) :
                             _( "At your skill level, it will take around %d minutes to light a fire." ),
                             moves / to_moves<int>( 1_minutes ) );
    } else if( moves < to_moves<int>( 2_turns ) && get_map().is_flammable( pos ) ) {
        // If less than 2 turns, don't start a long action
        resolve_firestarter_use( p, pos );
        p.mod_moves( -moves );
        return it.type->charges_to_use();
    }

    // skill gains are handled by the activity, but stored here in the index field
    const int potential_skill_gain =
        moves_modifier + moves_cost_fast / 100.0 + 2;
    p.assign_activity( ACT_START_FIRE, moves, potential_skill_gain,
                       0, it.tname() );
    p.activity->tools.emplace_back( &it );
    p.activity->values.push_back( g->natural_light_level( pos.z ) );
    p.activity->placement = pos;
    // charges to use are handled by the activity
    return 0;
}

void inscribe_actor::load( const JsonObject &obj )
{
    assign( obj, "cost", cost );
    assign( obj, "on_items", on_items );
    assign( obj, "on_terrain", on_terrain );
    assign( obj, "material_restricted", material_restricted );

    if( obj.has_array( "material_whitelist" ) ) {
        material_whitelist.clear();
        assign( obj, "material_whitelist", material_whitelist );
    }

    assign( obj, "verb", verb );
    assign( obj, "gerund", gerund );

    if( !on_items && !on_terrain ) {
        obj.throw_error(
            R"(Tried to create an useless inscribe_actor, at least on of "on_items" or "on_terrain" should be true)" );
    }
}

std::unique_ptr<iuse_actor> inscribe_actor::clone() const
{
    return std::make_unique<inscribe_actor>( *this );
}

bool inscribe_actor::item_inscription( item &tool, item &cut ) const
{
    if( !cut.made_of( SOLID ) ) {
        add_msg( m_info, _( "You can't inscribe an item that isn't solid!" ) );
        return false;
    }

    if( material_restricted && !cut.made_of_any( material_whitelist ) ) {
        std::string lower_verb = verb.translated();
        std::transform( lower_verb.begin(), lower_verb.end(), lower_verb.begin(), ::tolower );
        add_msg( m_info, _( "You can't %1$s %2$s because of the material it is made of." ),
                 lower_verb, cut.display_name() );
        return false;
    }

    enum inscription_type {
        INSCRIPTION_LABEL,
        INSCRIPTION_NOTE,
    };

    uilist menu;
    menu.text = string_format( _( "%s meaning?" ), verb );
    menu.addentry( INSCRIPTION_LABEL, true, -1, _( "It's a label" ) );
    menu.addentry( INSCRIPTION_NOTE, true, -1, _( "It's a note" ) );
    menu.query();

    std::string carving;
    std::string carving_tool;
    switch( menu.ret ) {
        case INSCRIPTION_LABEL:
            carving = "item_label";
            carving_tool = "item_label_tool";
            break;
        case INSCRIPTION_NOTE:
            carving = "item_note";
            carving_tool = "item_note_tool";
            break;
        default:
            return false;
    }

    const bool hasnote = cut.has_var( carving );
    std::string messageprefix = ( hasnote ? _( "(To delete, clear the text and confirm)\n" ) : "" ) +
                                //~ %1$s: gerund (e.g. carved), %2$s: item name
                                string_format( pgettext( "carving", "%1$s on the %2$s is: " ),
                                        gerund, cut.type_name() );

    string_input_popup popup;
    popup.title( string_format( _( "%s what?" ), verb ) )
    .width( 64 )
    .text( hasnote ? cut.get_var( carving ) : std::string() )
    .description( messageprefix )
    .identifier( "inscribe_item" )
    .max_length( 128 )
    .query();
    if( popup.canceled() ) {
        return false;
    }
    const std::string message = popup.text();
    if( message.empty() ) {
        cut.erase_var( carving );
        cut.erase_var( carving_tool );
    } else {
        cut.set_var( carving, message );
        cut.set_var( carving_tool, tool.typeId().str() );
    }

    return true;
}

int inscribe_actor::use( player &p, item &it, bool t, const tripoint & ) const
{
    if( t ) {
        return 0;
    }

    int choice = INT_MAX;
    if( on_terrain && on_items ) {
        uilist imenu;
        imenu.text = string_format( _( "%s on what?" ), verb );
        imenu.addentry( 0, true, MENU_AUTOASSIGN, _( "The terrain" ) );
        imenu.addentry( 1, true, MENU_AUTOASSIGN, _( "An item" ) );
        imenu.query();
        choice = imenu.ret;
    } else if( on_terrain ) {
        choice = 0;
    } else {
        choice = 1;
    }

    if( choice < 0 || choice > 1 ) {
        return 0;
    }

    if( choice == 0 ) {
        const std::optional<tripoint> dest_ = choose_adjacent( _( "Write where?" ) );
        if( !dest_ ) {
            return 0;
        }
        return iuse::handle_ground_graffiti( p, &it, string_format( _( "%s what?" ), verb ),
                                             dest_.value() );
    }

    item *loc = game_menus::inv::titled_menu( get_avatar(), _( "Inscribe which item?" ) );
    if( !loc ) {
        p.add_msg_if_player( m_info, _( "Never mind." ) );
        return 0;
    }
    item &cut = *loc;
    if( &cut == &it ) {
        p.add_msg_if_player( _( "You try to bend your %s, but fail." ), it.tname() );
        return 0;
    }
    // inscribe_item returns false if the action fails or is canceled somehow.

    if( item_inscription( it, cut ) ) {
        return cost >= 0 ? cost : it.ammo_required();
    }

    return 0;
}

void cauterize_actor::load( const JsonObject &obj )
{
    assign( obj, "cost", cost );
    assign( obj, "flame", flame );
}

std::unique_ptr<iuse_actor> cauterize_actor::clone() const
{
    return std::make_unique<cauterize_actor>( *this );
}

static heal_actor prepare_dummy()
{
    heal_actor dummy;
    dummy.limb_power = -2;
    dummy.head_power = -2;
    dummy.torso_power = -2;
    dummy.bleed = 1.0f;
    dummy.bite = 0.5f;
    dummy.move_cost = 100;
    return dummy;
}

bool cauterize_actor::cauterize_effect( player &p, item &it, bool force )
{
    // TODO: Make this less hacky
    static const heal_actor dummy = prepare_dummy();
    bodypart_str_id hpart = dummy.use_healing_item( p, p, it, force );
    if( hpart ) {
        p.add_msg_if_player( m_neutral, _( "You cauterize yourself." ) );
        if( !( p.has_trait( trait_NOPAIN ) ) ) {
            p.mod_pain( 15 );
            p.add_msg_if_player( m_bad, _( "It hurts like hell!" ) );
        } else {
            p.add_msg_if_player( m_neutral, _( "It itches a little." ) );
        }
        if( p.has_effect( effect_bite, hpart ) ) {
            p.add_effect( effect_bite, 260_minutes,  hpart );
        }

        p.moves = 0;
        return true;
    }

    return false;
}

int cauterize_actor::use( player &p, item &it, bool t, const tripoint & ) const
{
    if( t ) {
        return 0;
    }
    if( p.is_mounted() ) {
        p.add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    bool has_disease = p.has_effect( effect_bite ) || p.has_effect( effect_bleed );
    bool did_cauterize = false;

    if( has_disease ) {
        did_cauterize = cauterize_effect( p, it, false );
    } else {
        const bool can_have_fun = p.has_trait( trait_MASOCHIST ) || p.has_trait( trait_MASOCHIST_MED ) ||
                                  p.has_trait( trait_CENOBITE );

        if( can_have_fun && query_yn( _( "Cauterize yourself for fun?" ) ) ) {
            did_cauterize = cauterize_effect( p, it, true );
        }
    }

    if( !did_cauterize ) {
        return 0;
    }

    if( flame ) {
        p.use_charges( itype_fire, 4 );
        return 0;

    } else {
        return cost >= 0 ? cost : it.ammo_required();
    }
}

ret_val<bool> cauterize_actor::can_use( const Character &p, const item &it, bool,
                                        const tripoint & ) const
{
    if( !p.has_effect( effect_bite ) &&
        !p.has_effect( effect_bleed ) &&
        !p.has_trait( trait_MASOCHIST ) &&
        !p.has_trait( trait_MASOCHIST_MED ) &&
        !p.has_trait( trait_CENOBITE ) ) {

        return ret_val<bool>::make_failure(
                   _( "You are not bleeding or bitten, there is no need to cauterize yourself." ) );
    }
    if( p.is_mounted() ) {
        return ret_val<bool>::make_failure( _( "You cannot cauterize while mounted." ) );
    }

    if( flame ) {
        if( !p.has_charges( itype_fire, 4 ) ) {
            return ret_val<bool>::make_failure(
                       _( "You need a source of flame (4 charges worth) before you can cauterize yourself." ) );
        }
    } else {
        if( !it.units_sufficient( p ) ) {
            return ret_val<bool>::make_failure( _( "You need at least %d charges to cauterize wounds." ),
                                                it.ammo_required() );
        }
    }

    if( p.is_underwater() ) {
        return ret_val<bool>::make_failure( _( "You can't do that while underwater." ) );
    }

    return ret_val<bool>::make_success();
}

void enzlave_actor::load( const JsonObject &obj )
{
    assign( obj, "cost", cost );
}

std::unique_ptr<iuse_actor> enzlave_actor::clone() const
{
    return std::make_unique<enzlave_actor>( *this );
}

int enzlave_actor::use( player &p, item &it, bool t, const tripoint & ) const
{
    if( t ) {
        return 0;
    }
    if( p.is_mounted() ) {
        p.add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    map_stack items = get_map().i_at( point( p.posx(), p.posy() ) );
    std::vector<const item *> corpses;

    for( item * const &corpse_candidate : items ) {
        const mtype *mt = corpse_candidate->get_mtype();
        if( corpse_candidate->is_corpse() && mt->in_species( ZOMBIE ) &&
            mt->made_of( material_id( "flesh" ) ) &&
            mt->in_species( HUMAN ) && corpse_candidate->is_active() &&
            !corpse_candidate->has_var( "zlave" ) ) {
            corpses.push_back( corpse_candidate );
        }
    }

    if( corpses.empty() ) {
        p.add_msg_if_player( _( "No suitable corpses" ) );
        return 0;
    }

    int tolerance_level = 9;
    if( p.has_trait( trait_PSYCHOPATH ) || p.has_trait( trait_SAPIOVORE ) ) {
        tolerance_level = 0;
    } else if( p.has_trait_flag( trait_flag_PRED4 ) ) {
        tolerance_level = 5;
    } else if( p.has_trait_flag( trait_flag_PRED3 ) ) {
        tolerance_level = 7;
    }

    // Survival skill increases your willingness to get things done,
    // but it doesn't make you feel any less bad about it.
    /** @EFFECT_SURVIVAL increases tolerance for enzlavement */
    if( p.get_morale_level() <= ( 15 * ( tolerance_level - p.get_skill_level(
            skill_survival ) ) ) - 150 ) {
        add_msg( m_neutral,
                 _( "The prospect of cutting up the corpse and letting it rise again as a slave is too much for you to deal with right now." ) );
        return 0;
    }

    uilist amenu;

    amenu.text = _( "Selectively butcher the downed zombie into a zombie slave?" );
    for( size_t i = 0; i < corpses.size(); i++ ) {
        amenu.addentry( i, true, -1, corpses[i]->display_name() );
    }

    amenu.query();

    if( amenu.ret < 0 ) {
        p.add_msg_if_player( _( "Make love, not zlave." ) );
        return 0;
    }

    if( tolerance_level == 0 ) {
        // You just don't care, no message.
    } else if( tolerance_level <= 5 ) {
        add_msg( m_neutral, _( "Well, it's more constructive than just chopping 'em into gooey meat…" ) );
    } else {
        add_msg( m_bad, _( "You feel horrible for mutilating and enslaving someone's corpse." ) );

        /** @EFFECT_SURVIVAL decreases moral penalty and duration for enzlavement */
        int moraleMalus = -50 * ( 5.0 / p.get_skill_level( skill_survival ) );
        int maxMalus = -250 * ( 5.0 / p.get_skill_level( skill_survival ) );
        time_duration duration = 30_minutes * ( 5.0 / p.get_skill_level( skill_survival ) );
        time_duration decayDelay = 3_minutes * ( 5.0 / p.get_skill_level( skill_survival ) );

        if( p.has_trait( trait_PACIFIST ) ) {
            moraleMalus *= 5;
            maxMalus *= 3;
        } else if( p.has_trait_flag( trait_flag_PRED1 ) ) {
            moraleMalus /= 4;
        } else if( p.has_trait_flag( trait_flag_PRED2 ) ) {
            moraleMalus /= 5;
        }

        p.add_morale( MORALE_MUTILATE_CORPSE, moraleMalus, maxMalus, duration, decayDelay );
    }

    const int selected_corpse = amenu.ret;

    const item *body = corpses[selected_corpse];
    const mtype *mt = body->get_mtype();

    // HP range for zombies is roughly 36 to 120, with the really big ones having 180 and 480 hp.
    // Speed range is 20 - 120 (for humanoids, dogs get way faster)
    // This gives us a difficulty ranging roughly from 10 - 40, with up to +25 for corpse damage.
    // An average zombie with an undamaged corpse is 0 + 8 + 14 = 22.
    int difficulty = ( body->damage_level( 4 ) * 5 ) + ( mt->hp / 10 ) + ( mt->speed / 5 );
    // 0 - 30
    /** @EFFECT_DEX increases chance of success for enzlavement */

    /** @EFFECT_SURVIVAL increases chance of success for enzlavement */

    /** @EFFECT_FIRSTAID increases chance of success for enzlavement */
    int skills = p.get_skill_level( skill_survival ) + p.get_skill_level( skill_firstaid ) +
                 ( p.dex_cur / 2 );
    skills *= 2;

    int success = rng( 0, skills ) - rng( 0, difficulty );

    /** @EFFECT_FIRSTAID speeds up enzlavement */
    const int moves = difficulty * to_moves<int>( 12_seconds ) / p.get_skill_level( skill_firstaid );

    p.assign_activity( ACT_MAKE_ZLAVE, moves );
    p.activity->values.push_back( success );
    p.activity->str_values.push_back( corpses[selected_corpse]->display_name() );

    return cost >= 0 ? cost : it.ammo_required();
}

ret_val<bool> enzlave_actor::can_use( const Character &p, const item &, bool,
                                      const tripoint & ) const
{
    /** @EFFECT_SURVIVAL >=1 allows enzlavement */

    /** @EFFECT_FIRSTAID >=1 allows enzlavement */

    // TODO: Extract such checks into some kind of 'stat_requirements' class.
    if( p.get_skill_level( skill_survival ) < 1 ) {
        //~ %s - name of the required skill.
        return ret_val<bool>::make_failure( _( "You need at least %s 1." ),
                                            skill_survival->name() );
    }
    if( p.is_mounted() ) {
        return ret_val<bool>::make_failure( _( "You cannot do that while mounted." ) );
    }
    if( p.get_skill_level( skill_firstaid ) < 1 ) {
        //~ %s - name of the required skill.
        return ret_val<bool>::make_failure( _( "You need at least %s 1." ),
                                            skill_firstaid->name() );
    }

    return ret_val<bool>::make_success();
}

void fireweapon_off_actor::load( const JsonObject &obj )
{
    obj.read( "target_id", target_id, true );
    success_message     = obj.get_string( "success_message", "hsss" );
    lacks_fuel_message  = obj.get_string( "lacks_fuel_message" );
    failure_message     = obj.get_string( "failure_message", "hsss" );
    noise               = obj.get_int( "noise", 0 );
    moves               = obj.get_int( "moves", 0 );
    success_chance      = obj.get_int( "success_chance", INT_MIN );
}

std::unique_ptr<iuse_actor> fireweapon_off_actor::clone() const
{
    return std::make_unique<fireweapon_off_actor>( *this );
}

int fireweapon_off_actor::use( player &p, item &it, bool t, const tripoint & ) const
{
    if( t ) {
        return 0;
    }

    if( it.charges <= 0 ) {
        p.add_msg_if_player( _( lacks_fuel_message ) );
        return 0;
    }

    p.moves -= moves;
    if( rng( 0, 10 ) - it.damage_level( 4 ) > success_chance && !p.is_underwater() ) {
        if( noise > 0 ) {
            sounds::sound( p.pos(), noise, sounds::sound_t::combat, _( success_message ) );
        }
        p.add_msg_if_player( _( success_message ) );
        if( p.is_npc() && get_player_character().sees( p ) ) {
            add_msg( m_info, _( "%s activates their %s." ), p.disp_name(),
                     it.display_name() );
        }
        it.convert( target_id );
        it.activate();
    } else if( !failure_message.empty() ) {
        p.add_msg_if_player( m_bad, _( failure_message ) );
    }

    return it.type->charges_to_use();
}

ret_val<bool> fireweapon_off_actor::can_use( const Character &p, const item &it, bool,
        const tripoint & ) const
{
    if( it.charges < it.type->charges_to_use() ) {
        return ret_val<bool>::make_failure( _( "This tool doesn't have enough charges." ) );
    }

    if( p.is_underwater() ) {
        return ret_val<bool>::make_failure( _( "You can't do that while underwater." ) );
    }

    return ret_val<bool>::make_success();
}

void fireweapon_on_actor::load( const JsonObject &obj )
{
    noise_message                   = obj.get_string( "noise_message", "hsss" );
    voluntary_extinguish_message    = obj.get_string( "voluntary_extinguish_message" );
    charges_extinguish_message      = obj.get_string( "charges_extinguish_message" );
    water_extinguish_message        = obj.get_string( "water_extinguish_message" );
    noise                           = obj.get_int( "noise", 0 );
    noise_chance                    = obj.get_int( "noise_chance", 1 );
    auto_extinguish_chance          = obj.get_int( "auto_extinguish_chance", 0 );
    if( auto_extinguish_chance > 0 ) {
        auto_extinguish_message         = obj.get_string( "auto_extinguish_message" );
    }
}

std::unique_ptr<iuse_actor> fireweapon_on_actor::clone() const
{
    return std::make_unique<fireweapon_on_actor>( *this );
}

int fireweapon_on_actor::use( player &p, item &it, bool t, const tripoint & ) const
{
    bool extinguish = true;
    if( it.charges == 0 ) {
        p.add_msg_if_player( m_bad, _( charges_extinguish_message ) );
        // Revert when it runs out of charges is handled in process_tool.
        extinguish = false;
    } else if( p.is_underwater() ) {
        p.add_msg_if_player( m_bad, _( water_extinguish_message ) );
    } else if( auto_extinguish_chance > 0 && one_in( auto_extinguish_chance ) ) {
        p.add_msg_if_player( m_bad, _( auto_extinguish_message ) );
    } else if( !t ) {
        p.add_msg_if_player( _( voluntary_extinguish_message ) );
    } else {
        extinguish = false;
    }

    if( extinguish ) {
        if( p.is_npc() && get_player_character().sees( p ) ) {
            add_msg( m_info, _( "%s deactivates their %s." ), p.disp_name(),
                     it.display_name() );
        }
        it.revert( &p, false );
        it.deactivate();
        return 0;
    } else if( one_in( noise_chance ) ) {
        if( noise > 0 ) {
            sounds::sound( p.pos(), noise, sounds::sound_t::combat, _( noise_message ) );
        }
        p.add_msg_if_player( _( noise_message ) );
    }

    return it.type->charges_to_use();
}

void manualnoise_actor::load( const JsonObject &obj )
{
    no_charges_message  = obj.get_string( "no_charges_message" );
    use_message         = obj.get_string( "use_message" );
    noise_message       = obj.get_string( "noise_message", "hsss" );
    noise_id            = obj.get_string( "noise_id", "misc" );
    noise_variant       = obj.get_string( "noise_variant", "default" );
    noise               = obj.get_int( "noise", 0 );
    moves               = obj.get_int( "moves", 0 );
}

std::unique_ptr<iuse_actor> manualnoise_actor::clone() const
{
    return std::make_unique<manualnoise_actor>( *this );
}

int manualnoise_actor::use( player &p, item &it, bool t, const tripoint & ) const
{
    if( t ) {
        return 0;
    }
    if( it.type->charges_to_use() != 0 && it.charges < it.type->charges_to_use() ) {
        p.add_msg_if_player( _( no_charges_message ) );
        return 0;
    }
    {
        p.moves -= moves;
        if( noise > 0 ) {
            sounds::sound( p.pos(), noise, sounds::sound_t::activity,
                           noise_message.empty() ? _( "Hsss" ) : _( noise_message ), true, noise_id, noise_variant );
        }
        p.add_msg_if_player( _( use_message ) );
    }
    return it.type->charges_to_use();
}

ret_val<bool> manualnoise_actor::can_use( const Character &, const item &it, bool,
        const tripoint & ) const
{
    if( it.charges < it.type->charges_to_use() ) {
        return ret_val<bool>::make_failure( _( "This tool doesn't have enough charges." ) );
    }

    return ret_val<bool>::make_success();
}

std::unique_ptr<iuse_actor> musical_instrument_actor::clone() const
{
    return std::make_unique<musical_instrument_actor>( *this );
}

void musical_instrument_actor::load( const JsonObject &obj )
{
    speed_penalty = obj.get_int( "speed_penalty", 10 );
    volume = obj.get_int( "volume" );
    fun = obj.get_int( "fun" );
    fun_bonus = obj.get_int( "fun_bonus", 0 );
    if( !obj.read( "description_frequency", description_frequency ) ) {
        obj.throw_error( "missing member \"description_frequency\"" );
    }
    player_descriptions = obj.get_string_array( "player_descriptions" );
    npc_descriptions = obj.get_string_array( "npc_descriptions" );
}

int musical_instrument_actor::use( player &p, item &it, bool t, const tripoint & ) const
{
    if( p.is_mounted() ) {
        p.add_msg_player_or_npc( m_bad, _( "You can't play music while mounted." ),
                                 _( "<npcname> can't play music while mounted." ) );
        it.deactivate();
        return 0;
    }
    if( p.is_underwater() ) {
        p.add_msg_player_or_npc( m_bad,
                                 _( "You can't play music underwater" ),
                                 _( "<npcname> can't play music underwater" ) );
        it.deactivate();
        return 0;
    }

    if( p.has_effect( effect_sleep ) || p.has_effect( effect_stunned ) ||
        p.has_effect( effect_asthma ) ) {
        p.add_msg_player_or_npc( m_bad,
                                 _( "You stop playing your %s" ),
                                 _( "<npcname> stops playing their %s" ),
                                 it.display_name() );
        it.deactivate();
        return 0;
    }

    if( !t && it.is_active() ) {
        p.add_msg_player_or_npc( _( "You stop playing your %s" ),
                                 _( "<npcname> stops playing their %s" ),
                                 it.display_name() );
        it.deactivate();
        return 0;
    }

    // Check for worn or wielded - no "floating"/bionic instruments for now
    // TODO: Distinguish instruments played with hands and with mouth, consider encumbrance
    const int inv_pos = p.get_item_position( &it );
    if( inv_pos >= 0 || inv_pos == INT_MIN ) {
        p.add_msg_player_or_npc( m_bad,
                                 _( "You need to hold or wear %s to play it" ),
                                 _( "<npcname> needs to hold or wear %s to play it" ),
                                 it.display_name() );
        it.deactivate();
        return 0;
    }

    // At speed this low you can't coordinate your actions well enough to play the instrument
    if( p.get_speed() <= 25 + speed_penalty ) {
        p.add_msg_player_or_npc( m_bad,
                                 _( "You feel too weak to play your %s" ),
                                 _( "<npcname> feels too weak to play their %s" ),
                                 it.display_name() );
        it.deactivate();
        return 0;
    }

    // We can play the music now
    if( !it.is_active() ) {
        p.add_msg_player_or_npc( m_good,
                                 _( "You start playing your %s" ),
                                 _( "<npcname> starts playing their %s" ),
                                 it.display_name() );
        it.activate();
    }

    if( p.get_effect_int( effect_playing_instrument ) <= speed_penalty ) {
        // Only re-apply the effect if it wouldn't lower the intensity
        p.add_effect( effect_playing_instrument, 2_turns, bodypart_str_id::NULL_ID(), speed_penalty );
    }

    std::string desc = "music";
    /** @EFFECT_PER increases morale bonus when playing an instrument */
    const int morale_effect = fun + fun_bonus * p.per_cur;
    if( morale_effect >= 0 && calendar::once_every( description_frequency ) ) {
        if( !player_descriptions.empty() && p.is_player() ) {
            desc = _( random_entry( player_descriptions ) );
        } else if( !npc_descriptions.empty() && p.is_npc() ) {
            desc = string_format( _( "%1$s %2$s" ), p.disp_name( false ),
                                  random_entry( npc_descriptions ) );
        }
    } else if( morale_effect < 0 && calendar::once_every( 1_minutes ) ) {
        // No musical skills = possible morale penalty
        if( p.is_player() ) {
            desc = _( "You produce an annoying sound" );
        } else {
            desc = string_format( _( "%s produces an annoying sound" ), p.disp_name( false ) );
        }
    }

    if( morale_effect >= 0 ) {
        sounds::sound( p.pos(), volume, sounds::sound_t::music, desc, true, "musical_instrument",
                       it.typeId().str() );
    } else {
        sounds::sound( p.pos(), volume, sounds::sound_t::music, desc, true, "musical_instrument_bad",
                       it.typeId().str() );
    }

    if( !p.has_effect( effect_music ) && p.can_hear( p.pos(), volume ) ) {
        // Sound code doesn't describe noises at the player position
        if( p.is_player() && desc != "music" ) {
            add_msg( m_info, desc );
        }
        p.add_effect( effect_music, 1_turns );
        const int sign = morale_effect > 0 ? 1 : -1;
        p.add_morale( MORALE_MUSIC, sign, morale_effect, 5_minutes, 2_minutes, true );
    }

    return 0;
}

ret_val<bool> musical_instrument_actor::can_use( const Character &p, const item &, bool,
        const tripoint & ) const
{
    // TODO: (maybe): Mouth encumbrance? Smoke? Lack of arms? Hand encumbrance?
    if( p.is_underwater() ) {
        return ret_val<bool>::make_failure( _( "You can't do that while underwater." ) );
    }
    if( p.is_mounted() ) {
        return ret_val<bool>::make_failure( _( "You can't do that while mounted." ) );
    }

    return ret_val<bool>::make_success();
}

std::unique_ptr<iuse_actor> learn_spell_actor::clone() const
{
    return std::make_unique<learn_spell_actor>( *this );
}

void learn_spell_actor::load( const JsonObject &obj )
{
    spells = obj.get_string_array( "spells" );
}

void learn_spell_actor::info( const item &, std::vector<iteminfo> &dump ) const
{
    std::string message;
    if( spells.size() == 1 ) {
        message = _( "This can teach you a spell." );
    } else {
        message = _( "This can teach you a number of spells." );
    }
    dump.emplace_back( "DESCRIPTION", message );
    dump.emplace_back( "DESCRIPTION", _( "Spells Contained:" ) );
    for( const std::string &sp : spells ) {
        dump.emplace_back( "SPELL", spell_id( sp ).obj().name.translated() );
    }
}

int learn_spell_actor::use( player &p, item &, bool, const tripoint & ) const
{
    if( !character_funcs::can_see_fine_details( p ) ) {
        p.add_msg_if_player( _( "It's too dark to read." ) );
        return 0;
    }
    std::vector<uilist_entry> uilist_initializer;
    uilist spellbook_uilist;
    spellbook_callback sp_cb;
    bool know_it_all = true;
    for( const std::string &sp_id_str : spells ) {
        const spell_id sp_id( sp_id_str );
        sp_cb.add_spell( sp_id );
        uilist_entry entry( sp_id.obj().name.translated() );
        if( p.magic->knows_spell( sp_id ) ) {
            const spell sp = p.magic->get_spell( sp_id );
            entry.ctxt = string_format( _( "Level %u" ), sp.get_level() );
            if( sp.is_max_level() ) {
                entry.ctxt += _( " (Max)" );
                entry.enabled = false;
            } else {
                know_it_all = false;
            }
        } else {
            if( p.magic->can_learn_spell( p, sp_id ) ) {
                entry.ctxt = _( "Study to Learn" );
                know_it_all = false;
            } else {
                entry.ctxt = _( "Can't learn!" );
                entry.enabled = false;
            }
        }
        uilist_initializer.emplace_back( entry );
    }

    if( know_it_all ) {
        add_msg( m_info, _( "You already know everything this could teach you." ) );
        return 0;
    }

    spellbook_uilist.entries = uilist_initializer;
    spellbook_uilist.w_height_setup = 24;
    spellbook_uilist.w_width_setup = 80;
    spellbook_uilist.callback = &sp_cb;
    spellbook_uilist.title = _( "Study a spell:" );
    spellbook_uilist.pad_left_setup = 38;
    spellbook_uilist.query();
    const int action = spellbook_uilist.ret;
    if( action < 0 ) {
        return 0;
    }
    const bool knows_spell = p.magic->knows_spell( spells[action] );
    std::unique_ptr<player_activity> study_spell = std::make_unique<player_activity>( ACT_STUDY_SPELL,
            p.magic->time_to_learn_spell( p, spells[action] ) );
    study_spell->str_values = {
        "", // reserved for "until you gain a spell level" option [0]
        "learn"
    }; // [1]
    study_spell->values = { 0, 0, 0 };
    if( knows_spell ) {
        study_spell->str_values[1] = "study";
        const int study_time = uilist( _( "Spend how long studying?" ), {
            { to_moves<int>( 30_minutes ), true, -1, _( "30 minutes" ) },
            { to_moves<int>( 1_hours ), true, -1, _( "1 hour" ) },
            { to_moves<int>( 2_hours ), true, -1, _( "2 hours" ) },
            { to_moves<int>( 4_hours ), true, -1, _( "4 hours" ) },
            { to_moves<int>( 8_hours ), true, -1, _( "8 hours" ) },
            { 10100, true, -1, _( "Until you gain a spell level" ) }
        } );
        if( study_time <= 0 ) {
            return 0;
        }
        study_spell->moves_total = study_time;
    }
    study_spell->moves_left = study_spell->moves_total;
    if( study_spell->moves_total == 10100 ) {
        study_spell->str_values[0] = "gain_level";
        study_spell->values[0] = 0; // reserved for xp
        study_spell->values[1] = 0; // reserved for levels
    }
    study_spell->name = spells[action];
    p.assign_activity( std::move( study_spell ), false );
    return 0;
}

std::unique_ptr<iuse_actor> cast_spell_actor::clone() const
{
    return std::make_unique<cast_spell_actor>( *this );
}

void cast_spell_actor::load( const JsonObject &obj )
{
    no_fail = obj.get_bool( "no_fail" );
    item_spell = spell_id( obj.get_string( "spell_id" ) );
    spell_level = obj.get_int( "level" );
    need_worn = obj.get_bool( "need_worn", false );
    need_wielding = obj.get_bool( "need_wielding", false );
}

void cast_spell_actor::info( const item &, std::vector<iteminfo> &dump ) const
{
    //~ %1$s: spell name, %2$i: spell level
    const std::string message = string_format( _( "This item casts %1$s at level %2$i." ),
                                item_spell->name, spell_level );
    dump.emplace_back( "DESCRIPTION", message );
    if( no_fail ) {
        dump.emplace_back( "DESCRIPTION", _( "This item never fails." ) );
    }
}

int cast_spell_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    if( need_worn && !p.is_worn( it ) ) {
        p.add_msg_if_player( m_info, _( "You need to wear the %1$s before activating it." ), it.tname() );
        return 0;
    }
    if( need_wielding && !p.is_wielding( it ) ) {
        p.add_msg_if_player( m_info, _( "You need to wield the %1$s before activating it." ), it.tname() );
        return 0;
    }

    spell casting = spell( spell_id( item_spell ) );

    std::unique_ptr<player_activity> cast_spell = std::make_unique<player_activity>( ACT_SPELLCASTING,
            casting.casting_time( p ) );
    // [0] this is used as a spell level override for items casting spells
    cast_spell->values.emplace_back( spell_level );
    if( no_fail ) {
        // [1] if this value is 1, the spell never fails
        cast_spell->values.emplace_back( 1 );
    } else {
        // [1]
        cast_spell->values.emplace_back( 0 );
    }
    cast_spell->name = casting.id().c_str();
    if( it.has_flag( flag_USE_PLAYER_ENERGY ) ) {
        // [2] this value overrides the mana cost if set to 0
        // Have to check whether the player actually has enough energy or not
        if( p.magic->has_enough_energy( p, casting ) ) {
            cast_spell->values.emplace_back( 1 );
        } else {
            p.add_msg_if_player( m_info, _( "You lack the energy to cast %s." ), casting.name() );
            return 0;
        }
    } else {
        // [2]
        cast_spell->values.emplace_back( 0 );
    }
    p.assign_activity( std::move( cast_spell ), false );
    p.activity->targets.emplace_back( &it );
    return 0;
}

std::unique_ptr<iuse_actor> holster_actor::clone() const
{
    return std::make_unique<holster_actor>( *this );
}

void holster_actor::load( const JsonObject &obj )
{
    holster_prompt = obj.get_string( "holster_prompt", "" );
    holster_msg = obj.get_string( "holster_msg", "" );
    assign( obj, "max_volume", max_volume );
    if( !assign( obj, "min_volume", min_volume ) ) {
        min_volume = max_volume / 3;
    }

    assign( obj, "max_weight", max_weight );
    multi      = obj.get_int( "multi",      multi );
    draw_cost  = obj.get_int( "draw_cost",  draw_cost );

    auto tmp = obj.get_string_array( "skills" );
    std::transform( tmp.begin(), tmp.end(), std::back_inserter( skills ),
    []( const std::string & elem ) {
        return skill_id( elem );
    } );

    flags = obj.get_string_array( "flags" );
}

bool holster_actor::can_holster( const item &obj ) const
{
    if( obj.volume() > max_volume || obj.volume() < min_volume ) {
        return false;
    }
    if( max_weight > 0_gram && obj.weight() > max_weight ) {
        return false;
    }
    if( obj.is_active() ) {
        return false;
    }
    return std::any_of( flags.begin(), flags.end(), [&]( const std::string & f ) {
        return obj.has_flag( flag_id( f ) );
    } ) ||
    std::find( skills.begin(), skills.end(), obj.gun_skill() ) != skills.end();
}

detached_ptr<item> holster_actor::store( player &p, item &holster, detached_ptr<item> &&obj ) const
{
    if( obj->is_null() || holster.is_null() ) {
        debugmsg( "Null item was passed to holster_actor" );
        return std::move( obj );
    }

    // if selected item is unsuitable inform the player why not
    if( obj->volume() > max_volume ) {
        p.add_msg_if_player( m_info, _( "Your %1$s is too big to fit in your %2$s" ),
                             obj->tname(), holster.tname() );
        return std::move( obj );
    }

    if( obj->volume() < min_volume ) {
        p.add_msg_if_player( m_info, _( "Your %1$s is too small to fit in your %2$s" ),
                             obj->tname(), holster.tname() );
        return std::move( obj );
    }

    if( max_weight > 0_gram && obj->weight() > max_weight ) {
        p.add_msg_if_player( m_info, _( "Your %1$s is too heavy to fit in your %2$s" ),
                             obj->tname(), holster.tname() );
        return std::move( obj );
    }

    if( obj->is_active() ) {
        p.add_msg_if_player( m_info, _( "You don't think putting your %1$s in your %2$s is a good idea" ),
                             obj->tname(), holster.tname() );
        return std::move( obj );
    }

    if( std::none_of( flags.begin(), flags.end(), [&]( const std::string & f ) {
    return obj->has_flag( flag_id( f ) );
    } ) &&
    std::find( skills.begin(), skills.end(), obj->gun_skill() ) == skills.end() ) {
        p.add_msg_if_player( m_info, _( "You can't put your %1$s in your %2$s" ),
                             obj->tname(), holster.tname() );
        return std::move( obj );
    }

    p.add_msg_if_player( holster_msg.empty() ? _( "You holster your %s" ) : _( holster_msg ),
                         obj->tname(), holster.tname() );

    // Holsters ignore penalty effects (e.g. GRABBED) when determining number of moves to consume
    character_funcs::store_in_container( p, holster, std::move( obj ), false, draw_cost );
    return detached_ptr<item>();
}

int holster_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    if( p.is_wielding( it ) ) {
        p.add_msg_if_player( _( "You need to unwield your %s before using it." ), it.tname() );
        return 0;
    }

    int pos = 0;
    std::vector<std::string> opts;

    if( static_cast<int>( it.contents.num_item_stacks() ) < multi ) {
        std::string prompt = holster_prompt.empty() ? _( "Holster item" ) : _( holster_prompt );
        opts.push_back( prompt );
        pos = -1;
    }

    std::vector<item *> top_contents{ it.contents.all_items_top() };
    std::transform( top_contents.begin(), top_contents.end(), std::back_inserter( opts ),
    []( const item * elem ) {
        return string_format( _( "Draw %s" ), elem->display_name() );
    } );

    item *internal_item = nullptr;
    if( opts.size() > 1 ) {
        int ret = uilist( string_format( _( "Use %s" ), it.tname() ), opts );
        if( ret < 0 ) {
            pos = -2;
        } else {
            pos += ret;
            if( opts.size() != it.contents.num_item_stacks() ) {
                ret--;
            }
            auto iter = std::next( top_contents.begin(), ret );
            internal_item = *iter;
        }
    } else if( !it.contents.empty() ) {
        internal_item = &it.contents.front();
    }

    if( pos < -1 ) {
        p.add_msg_if_player( _( "Never mind." ) );
        return 0;
    }

    if( pos >= 0 ) {
        // Worn holsters ignore penalty effects (e.g. GRABBED) when determining number of moves to consume
        bool penalties;
        int cost;
        if( p.is_worn( it ) ) {
            penalties = false;
            cost = draw_cost;
        } else {
            penalties = true;
            cost = INVENTORY_HANDLING_PENALTY;
        }
        character_funcs::try_wield_contents( p, it, internal_item, penalties, cost );

    } else {
        item *loc = game_menus::inv::holster( p, it );

        if( !loc ) {
            p.add_msg_if_player( _( "Never mind." ) );
            return 0;
        }
        store( p, it, loc->detach() );
    }

    return 0;
}

void holster_actor::info( const item &, std::vector<iteminfo> &dump ) const
{
    std::string message = vgettext( "Can be activated to store a suitable item.",
                                    "Can be activated to store suitable items.", multi );
    dump.emplace_back( "DESCRIPTION", message );
    dump.emplace_back( "TOOL", _( "Num items: " ), "<num>", iteminfo::no_flags, multi );
    dump.emplace_back( "TOOL", _( "Item volume: Min: " ),
                       string_format( "<num> %s", volume_units_abbr() ),
                       iteminfo::is_decimal | iteminfo::no_newline | iteminfo::lower_is_better,
                       convert_volume( min_volume.value() ) );
    dump.emplace_back( "TOOL", _( "  Max: " ),
                       string_format( "<num> %s", volume_units_abbr() ),
                       iteminfo::is_decimal,
                       convert_volume( max_volume.value() ) );

    if( max_weight > 0_gram ) {
        dump.emplace_back( "TOOL", _( "Max item weight: " ),
                           string_format( _( "<num> %s" ), weight_units() ),
                           iteminfo::is_decimal,
                           convert_weight( max_weight ) );
    }
}

units::volume holster_actor::max_stored_volume() const
{
    return max_volume * multi;
}

std::unique_ptr<iuse_actor> bandolier_actor::clone() const
{
    return std::make_unique<bandolier_actor>( *this );
}

void bandolier_actor::load( const JsonObject &obj )
{
    capacity = obj.get_int( "capacity", capacity );
    ammo.clear();
    for( auto &e : obj.get_tags( "ammo" ) ) {
        ammo.insert( ammotype( e ) );
    }

    draw_cost = obj.get_int( "draw_cost", draw_cost );
}

void bandolier_actor::info( const item &, std::vector<iteminfo> &dump ) const
{
    if( !ammo.empty() ) {
        auto str = enumerate_as_string( ammo.begin(), ammo.end(),
        [&]( const ammotype & a ) {
            return string_format( "<stat>%s</stat>", a->name() );
        }, enumeration_conjunction::or_ );

        dump.emplace_back( "TOOL", string_format(
                               vgettext( "Can be activated to store a single round of ",
                                         "Can be activated to store up to <stat>%i</stat> rounds of ", capacity ),
                               capacity ),
                           str );
    }
}

bool bandolier_actor::is_valid_ammo_type( const itype &t ) const
{
    if( !t.ammo ) {
        return false;
    }
    return ammo.contains( t.ammo->type );
}

bool bandolier_actor::can_store( const item &bandolier, const item &obj ) const
{
    if( !bandolier.contents.empty() && ( bandolier.contents.front().typeId() != obj.typeId() ||
                                         bandolier.contents.front().charges >= capacity ) ) {
        return false;
    }

    return is_valid_ammo_type( *obj.type );
}

bool bandolier_actor::reload( player &p, item &obj ) const
{
    if( !obj.is_bandolier() ) {
        debugmsg( "Invalid item passed to bandolier_actor" );
        return false;
    }
    // find all nearby compatible ammo (matching type currently contained if appropriate)
    auto found = p.nearby( [&]( const item * e, const item * parent ) {
        return parent != &obj && can_store( obj, *e );
    } );

    if( found.empty() ) {
        p.add_msg_if_player( m_bad, _( "No matching ammo for the %1$s" ), obj.type_name() );
        return false;
    }

    // convert these into reload options and display the selection prompt
    std::vector<item_reload_option> opts;
    std::transform( std::make_move_iterator( found.begin() ), std::make_move_iterator( found.end() ),
    std::back_inserter( opts ), [&]( item * e ) {
        return item_reload_option( &p, &obj, &obj, *e );
    } );

    item_reload_option sel = character_funcs::select_ammo( p, obj, std::move( opts ) );
    if( !sel ) {
        return false; // canceled menu
    }

    p.mod_moves( -sel.moves() );

    // add or stack the ammo dependent upon existing contents
    if( obj.contents.empty() ) {
        obj.put_in( sel.ammo->split( sel.qty() ) );
    } else {
        obj.contents.front().charges += sel.qty();
        if( sel.ammo->charges > sel.qty() ) {
            sel.ammo->charges -= sel.qty();
        } else {
            sel.ammo->detach();
        }
    }

    p.add_msg_if_player( _( "You store the %1$s in your %2$s" ),
                         obj.contents.front().tname( sel.qty() ),
                         obj.type_name() );

    return true;
}

int bandolier_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    if( p.is_wielding( it ) ) {
        p.add_msg_if_player( _( "You need to unwield your %s before using it." ),
                             it.type_name() );
        return 0;
    }

    uilist menu;
    menu.text = _( "Store ammo" );

    std::vector<std::function<void()>> actions;

    menu.addentry( -1, it.contents.empty() || it.contents.front().charges < capacity,
                   'r', _( "Store ammo in %s" ), it.type_name() );

    actions.emplace_back( [&] { reload( p, it ); } );

    menu.addentry( -1, !it.contents.empty(), 'u', _( "Unload %s" ), it.type_name() );

    actions.emplace_back( [&] {

        it.contents.front().attempt_detach( [&p]( detached_ptr<item> &&it )
        {
            it = p.i_add_or_drop( std::move( it ) );
            if( it ) {
                p.add_msg_if_player( _( "Never mind." ) );
            }
            return std::move( it );
        } );
    } );

    menu.query();
    if( menu.ret >= 0 ) {
        actions[ menu.ret ]();
    }

    return 0;
}

units::volume bandolier_actor::max_stored_volume() const
{
    // This is relevant only for bandoliers with the non-rigid flag

    // Find all valid ammo
    auto ammo_types = item_controller->find( [&]( const itype & t ) {
        return is_valid_ammo_type( t );
    } );
    // Figure out which has the greatest volume and calculate on that basis
    units::volume max_ammo_volume{};
    for( const auto *ammo_type : ammo_types ) {
        max_ammo_volume = std::max( max_ammo_volume, ammo_type->volume / ammo_type->stack_size );
    }
    return max_ammo_volume * capacity;
}

std::unique_ptr<iuse_actor> ammobelt_actor::clone() const
{
    return std::make_unique<ammobelt_actor>( *this );
}

void ammobelt_actor::load( const JsonObject &obj )
{
    belt = itype_id( obj.get_string( "belt" ) );
}

void ammobelt_actor::info( const item &, std::vector<iteminfo> &dump ) const
{
    dump.emplace_back( "AMMO", string_format( _( "Can be used to assemble: %s" ),
                       item::nname( belt ) ) );
}

int ammobelt_actor::use( player &p, item &, bool, const tripoint & ) const
{
    detached_ptr<item> mag = item::spawn( belt );
    mag->ammo_unset();

    if( !p.can_reload( *mag ) ) {
        p.add_msg_if_player( _( "Insufficient ammunition to assemble %s" ), mag->tname() );
        return 0;
    }

    item_reload_option opt = character_funcs::select_ammo( p, *mag, true );
    if( opt ) {
        p.assign_activity( ACT_RELOAD, opt.moves(), opt.qty() );
        p.activity->targets.emplace_back( &*mag );
        p.activity->targets.emplace_back( opt.ammo );
        p.i_add( std::move( mag ) );
    }

    return 0;
}

void repair_item_actor::load( const JsonObject &obj )
{
    // Mandatory:
    for( const std::string line : obj.get_array( "materials" ) ) {
        materials.emplace( line );
    }

    // TODO: Make skill non-mandatory while still erroring on invalid skill
    const std::string skill_string = obj.get_string( "skill" );
    used_skill = skill_id( skill_string );
    if( !used_skill.is_valid() ) {
        obj.throw_error( "Invalid skill", "skill" );
    }

    cost_scaling = obj.get_float( "cost_scaling" );

    // Optional
    tool_quality = obj.get_int( "tool_quality", 0 );
    move_cost    = obj.get_int( "move_cost", 500 );
    trains_skill_to = obj.get_int( "trains_skill_to", 5 ) - 1;
}

bool repair_item_actor::can_use_tool( const player &p, const item &tool, bool print_msg ) const
{
    if( p.is_underwater() ) {
        if( print_msg ) {
            p.add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        }
        return false;
    }
    if( p.is_mounted() ) {
        if( print_msg ) {
            p.add_msg_player_or_npc( m_bad, _( "You can't do that while mounted." ),
                                     _( "<npcname> can't do that while mounted." ) );
        }
        return false;
    }
    if( !character_funcs::can_see_fine_details( p ) ) {
        if( print_msg ) {
            p.add_msg_if_player( m_info, _( "You can't see to do that!" ) );
        }
        return false;
    }
    if( !tool.units_sufficient( p ) ) {
        if( print_msg ) {
            p.add_msg_if_player( m_info, _( "Your tool does not have enough charges to do that." ) );
        }
        return false;
    }

    return true;
}

int repair_item_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    if( !can_use_tool( p, it, true ) ) {
        return 0;
    }

    p.assign_activity( ACT_REPAIR_ITEM, 0, p.get_item_position( &it ), INT_MIN );
    // We also need to store the repair actor subtype in the activity
    p.activity->str_values.push_back( type );
    // storing of item_location to support repairs by tools on the ground
    p.activity->targets.emplace_back( &it );
    // All repairs are done in the activity, including charge cost and target item selection
    return 0;
}

std::unique_ptr<iuse_actor> repair_item_actor::clone() const
{
    return std::make_unique<repair_item_actor>( *this );
}

bool repair_item_actor::handle_components( player &pl, const item &fix,
        bool print_msg, bool just_check ) const
{
    // Entries valid for repaired items
    std::set<material_id> valid_entries;
    for( const auto &mat : materials ) {
        if( fix.made_of( mat ) ) {
            valid_entries.insert( mat );
        }
    }

    if( valid_entries.empty() ) {
        if( print_msg ) {
            pl.add_msg_if_player( m_info, _( "Your %s is not made of any of:" ),
                                  fix.tname() );
            for( const auto &mat_name : materials ) {
                const auto &mat = mat_name.obj();
                pl.add_msg_if_player( m_info, _( "%s (repaired using %s)" ), mat.name(),
                                      item::nname( mat.repaired_with(), 2 ) );
            }
        }

        return false;
    }

    const inventory &crafting_inv = pl.crafting_inventory();

    // Repairing or modifying items requires at least 1 repair item,
    //  otherwise number is related to size of item
    // Round up if checking, but roll if actually consuming
    // TODO: should 250_ml be part of the cost_scaling?
    const int items_needed = std::max<int>( 1, just_check ?
                                            std::ceil( fix.volume() / 250_ml * cost_scaling ) :
                                            roll_remainder( fix.volume() / 250_ml * cost_scaling ) );


    // Go through all discovered repair items and see if we have any of them available
    std::vector<item_comp> comps;
    for( const auto &entry : valid_entries ) {
        const itype_id &component_id = entry.obj().repaired_with();
        // Certain (different!) materials are repaired with the same components (steel, iron, hard steel use scrap metal).
        // This checks avoids adding the same component twice, which is annoying to the user.
        if( std::find_if( comps.begin(), comps.end(), [&]( const item_comp & ic ) {
        return ic.type == component_id;
    } ) != comps.end() ) {
            continue;
        }
        if( item::count_by_charges( component_id ) ) {
            if( crafting_inv.has_charges( component_id, items_needed ) ) {
                comps.emplace_back( component_id, items_needed );
            }
        } else if( crafting_inv.has_amount( component_id, items_needed, false, is_crafting_component ) ) {
            comps.emplace_back( component_id, items_needed );
        }
    }

    if( comps.empty() ) {
        if( print_msg ) {
            for( const auto &entry : valid_entries ) {
                const auto &mat_comp = entry.obj().repaired_with();
                pl.add_msg_if_player( m_info,
                                      _( "You don't have enough %s to do that.  Have: %d, need: %d" ),
                                      item::nname( mat_comp, 2 ),
                                      mat_comp->count_by_charges() ?
                                      crafting_inv.amount_of( mat_comp, false ) :
                                      crafting_inv.charges_of( mat_comp, items_needed ),
                                      items_needed );
            }
        }

        return false;
    }

    if( !just_check ) {
        if( comps.empty() ) {
            // This shouldn't happen - the check in can_repair_target should prevent it
            // But report it, just in case
            debugmsg( "Attempted repair with no components" );
        }

        pl.consume_items( comps, 1, is_crafting_component );
    }

    return true;
}

// Find the difficulty of the recipes that result in id
// If the recipe is not known by the player, +1 to difficulty
// If player doesn't meet the requirements of the recipe, +1 to difficulty
// Returns -1 if no recipe is found
static int find_repair_difficulty( const player &pl, const itype_id &id, bool training )
{
    // If the recipe is not found, this will remain unchanged
    int min = id->repair_difficulty;
    if( min != -1 ) {
        return min;
    }
    for( const auto &e : recipe_dict ) {
        const auto r = e.second;
        if( id != r.result() ) {
            continue;
        }
        // If this is the first time we found a recipe
        if( min == -1 ) {
            min = 5;
        }

        int cur_difficulty = r.difficulty;
        if( !training && !pl.knows_recipe( &r ) ) {
            cur_difficulty++;
        }

        if( !training && !pl.has_recipe_requirements( r ) ) {
            cur_difficulty++;
        }

        min = std::min( cur_difficulty, min );
    }

    return min;
}

// Returns the level of the lowest level recipe that results in item of `fix`'s type
// Or if it has a repairs_like, the lowest level recipe that results in that.
// If the recipe doesn't exist, difficulty is 10
int repair_item_actor::repair_recipe_difficulty( const player &pl,
        const item &fix, bool training ) const
{
    int diff = find_repair_difficulty( pl, fix.typeId(), training );

    // If we don't find a recipe, see if there's a repairs_like that has a recipe
    if( diff == -1 && !fix.type->repairs_like.is_empty() ) {
        diff = find_repair_difficulty( pl, fix.type->repairs_like, training );
    }

    // If we still don't find a recipe, difficulty is 10
    if( diff == -1 ) {
        diff = 10;
    }

    return diff;
}

bool repair_item_actor::can_repair_target( player &pl, const item &fix,
        bool print_msg ) const
{
    // In some rare cases (indices getting scrambled after inventory overflow)
    //  our `fix` can be a different item.
    if( fix.is_null() ) {
        if( print_msg ) {
            pl.add_msg_if_player( m_info, _( "You do not have that item!" ) );
        }
        return false;
    }
    if( fix.is_firearm() ) {
        if( print_msg ) {
            pl.add_msg_if_player( m_info, _( "That requires gunsmithing tools." ) );
        }
        return false;
    }
    if( fix.count_by_charges() || fix.has_flag( flag_NO_REPAIR ) ) {
        if( print_msg ) {
            pl.add_msg_if_player( m_info, _( "You cannot repair this type of item." ) );
        }
        return false;
    }

    if( any_of( materials.begin(), materials.end(), [&fix]( const material_id & mat ) {
    return mat.obj()
               .repaired_with() == fix.typeId();
    } ) ) {
        if( print_msg ) {
            pl.add_msg_if_player( m_info, _( "This can be used to repair other items, not itself." ) );
        }
        return false;
    }

    if( !handle_components( pl, fix, print_msg, true ) ) {
        return false;
    }

    const bool can_be_refitted = fix.has_flag( flag_VARSIZE );
    if( can_be_refitted && !fix.has_flag( flag_FIT ) ) {
        return true;
    }

    const bool resizing_matters = fix.get_sizing( pl ) != item::sizing::ignore;
    const bool small = pl.get_size() == creature_size::tiny;
    const bool can_resize = small != fix.has_flag( flag_UNDERSIZE );
    if( can_be_refitted && resizing_matters && can_resize ) {
        return true;
    }

    if( fix.damage() > 0 ) {
        return true;
    }

    if( fix.damage() <= fix.min_damage() ) {
        if( print_msg ) {
            pl.add_msg_if_player( m_info, _( "Your %s is already enhanced to its maximum potential." ),
                                  fix.tname() );
        }
        return false;
    }

    if( fix.has_flag( flag_PRIMITIVE_RANGED_WEAPON ) || !fix.reinforceable() ) {
        if( print_msg ) {
            pl.add_msg_if_player( m_info, _( "You cannot improve your %s any more this way." ),
                                  fix.tname() );
        }
        return false;
    }

    return true;
}

std::pair<float, float> repair_item_actor::repair_chance(
    const player &pl, const item &fix, repair_item_actor::repair_type action_type ) const
{
    /** @EFFECT_TAILOR randomly improves clothing repair efforts */
    /** @EFFECT_MECHANICS randomly improves metal repair efforts */
    const int skill = pl.get_skill_level( used_skill );
    const int recipe_difficulty = repair_recipe_difficulty( pl, fix );
    int action_difficulty = 0;
    switch( action_type ) {
        case RT_REPAIR:
            action_difficulty = fix.damage_level( 4 );
            break;
        case RT_REFIT:
            // Let's make refitting as hard as recovering an almost-wrecked item
            action_difficulty = fix.max_damage() / itype::damage_scale;
            break;
        case RT_REINFORCE:
            // Reinforcing is 50% harder than refitting
            action_difficulty = ( fix.max_damage() / itype::damage_scale ) + 2;
            break;
        case RT_PRACTICE:
            // Skill gain scales with recipe difficulty, so practice difficulty should too
            action_difficulty = recipe_difficulty;
        default:
            ;
    }

    const int difficulty = recipe_difficulty + action_difficulty;
    float success_chance = ( 10 + 2 * ( skill * ( 1 + tool_quality / 10.0f ) ) - 2 * difficulty ) /
                           100.0f;
    /** @EFFECT_DEX reduces the chances of damaging an item when repairing */
    float damage_chance = ( difficulty - ( skill * ( 1 + tool_quality / 10.0f ) ) - pl.dex_cur /
                            5.0f ) / 100.0f;

    damage_chance = std::max( 0.0f, std::min( 1.0f, damage_chance ) );
    success_chance = std::max( 0.0f, std::min( 1.0f - damage_chance, success_chance ) );

    return std::make_pair( success_chance, damage_chance );
}

repair_item_actor::repair_type repair_item_actor::default_action( const item &fix,
        int current_skill_level ) const
{
    if( fix.damage() > 0 ) {
        return RT_REPAIR;
    }

    const bool can_be_refitted = fix.has_flag( flag_VARSIZE );
    const bool doesnt_fit = !fix.has_flag( flag_FIT );
    if( doesnt_fit && can_be_refitted ) {
        return RT_REFIT;
    }

    Character &player_character = get_player_character();
    const bool smol = player_character.get_size() == creature_size::tiny;

    const bool is_undersized = fix.has_flag( flag_UNDERSIZE );
    const bool is_oversized = fix.has_flag( flag_OVERSIZE );
    const bool resizing_matters = fix.get_sizing( player_character ) != item::sizing::ignore;

    const bool too_big_while_smol = smol && !is_undersized && !is_oversized;
    if( too_big_while_smol && can_be_refitted && resizing_matters ) {
        return RT_DOWNSIZING;
    }

    const bool too_small_while_big = !smol && is_undersized && !is_oversized;
    if( too_small_while_big && can_be_refitted && resizing_matters ) {
        return RT_UPSIZING;
    }

    if( fix.damage() > fix.min_damage() ) {
        return RT_REINFORCE;
    }

    if( current_skill_level <= trains_skill_to ) {
        return RT_PRACTICE;
    }

    return RT_NOTHING;
}

static bool damage_item( player &pl, item *fix )
{
    const std::string startdurability = fix->durability_indicator( true );
    const auto destroyed = fix->inc_damage();
    const std::string resultdurability = fix->durability_indicator( true );
    pl.add_msg_if_player( m_bad, _( "You damage your %s!  ( %s-> %s)" ), fix->tname( 1, false ),
                          startdurability, resultdurability );
    if( destroyed ) {

        // Dump its contents on the ground
        // Destroy irremovable mods, if any
        fix->contents.remove_top_items_with( []( detached_ptr<item> &&mod ) {
            if( mod->is_gunmod() && !mod->is_irremovable() ) {
                return detached_ptr<item>();
            }
            return std::move( mod );
        } );

        fix->contents.spill_contents( fix->position() );

        pl.add_msg_if_player( m_bad, _( "You destroy it!" ) );
        if( fix->where() == item_location_type::character ) {
            pl.i_rem_keep_contents( pl.get_item_position( fix ) );
        } else {
            for( detached_ptr<item> &it : fix->contents.clear_items() ) {
                put_into_vehicle_or_drop( pl, item_drop_reason::deliberate, std::move( it ),
                                          fix->position() );
            }
            fix->detach();
        }

        return true;
    }

    return false;
}

repair_item_actor::attempt_hint repair_item_actor::repair( player &pl, item &tool,
        item &fix ) const
{
    if( !can_use_tool( pl, tool, true ) ) {
        return AS_CANT_USE_TOOL;
    }
    if( !can_repair_target( pl, fix, true ) ) {
        return AS_CANT;
    }

    const int current_skill_level = pl.get_skill_level( used_skill );
    const auto action = default_action( fix, current_skill_level );
    const auto chance = repair_chance( pl, fix, action );
    int practice_amount = std::max( repair_recipe_difficulty( pl, fix, true ), 1 );
    float roll_value = rng_float( 0.0, 1.0 );
    enum roll_result {
        SUCCESS,
        FAILURE,
        NEUTRAL
    } roll;

    if( roll_value > 1.0f - chance.second ) {
        roll = FAILURE;
    } else if( roll_value < chance.first ) {
        roll = SUCCESS;
    } else {
        roll = NEUTRAL;
    }

    if( action == RT_NOTHING ) {
        pl.add_msg_if_player( m_bad, _( "You won't learn anything more by doing that." ) );
        return AS_CANT;
    }

    // If not for this if, it would spam a lot
    if( current_skill_level > trains_skill_to ) {
        practice_amount = 0;
    }
    pl.practice( used_skill, practice_amount, trains_skill_to );

    if( roll == FAILURE ) {
        return damage_item( pl, &fix ) ? AS_DESTROYED : AS_FAILURE;
    }

    if( action == RT_PRACTICE ) {
        return AS_RETRY;
    }

    if( action == RT_REPAIR ) {
        if( roll == SUCCESS ) {
            const std::string startdurability = fix.durability_indicator( true );
            const auto damage = fix.damage();
            handle_components( pl, fix, false, false );
            fix.set_damage( std::max( damage - itype::damage_scale, 0 ) );
            const std::string resultdurability = fix.durability_indicator( true );
            if( damage > itype::damage_scale ) {
                pl.add_msg_if_player( m_good, _( "You repair your %s!  ( %s-> %s)" ), fix.tname( 1, false ),
                                      startdurability, resultdurability );
            } else {
                pl.add_msg_if_player( m_good, _( "You repair your %s completely!  ( %s-> %s)" ), fix.tname( 1,
                                      false ), startdurability, resultdurability );
            }
            return AS_SUCCESS;
        }

        return AS_RETRY;
    }

    if( action == RT_REFIT ) {
        if( roll == SUCCESS ) {
            if( !fix.has_flag( flag_FIT ) ) {
                pl.add_msg_if_player( m_good, _( "You take your %s in, improving the fit." ),
                                      fix.tname() );
                fix.set_flag( flag_FIT );
            }
            handle_components( pl, fix, false, false );
            return AS_SUCCESS;
        }

        return AS_RETRY;
    }

    if( action == RT_DOWNSIZING ) {
        //We don't need to check for smallness or undersize because DOWNSIZING already guarantees that
        if( roll == SUCCESS ) {
            pl.add_msg_if_player( m_good, _( "You resize the %s to accommodate your tiny build." ),
                                  fix.tname().c_str() );
            fix.set_flag( flag_UNDERSIZE );
            handle_components( pl, fix, false, false );
            return AS_SUCCESS;
        }
        return AS_RETRY;
    }

    if( action == RT_UPSIZING ) {
        //We don't need to check for smallness or undersize because UPSIZING already guarantees that
        if( roll == SUCCESS ) {
            pl.add_msg_if_player( m_good, _( "You adjust the %s back to its normal size." ),
                                  fix.tname().c_str() );
            fix.unset_flag( flag_UNDERSIZE );
            handle_components( pl, fix, false, false );
            return AS_SUCCESS;
        }
        return AS_RETRY;
    }

    if( action == RT_REINFORCE ) {
        if( fix.has_flag( flag_PRIMITIVE_RANGED_WEAPON ) || !fix.reinforceable() ) {
            pl.add_msg_if_player( m_info, _( "You cannot improve your %s any more this way." ),
                                  fix.tname() );
            return AS_CANT;
        }

        if( roll == SUCCESS ) {
            pl.add_msg_if_player( m_good, _( "You make your %s extra sturdy." ), fix.tname() );
            fix.mod_damage( -itype::damage_scale );
            handle_components( pl, fix, false, false );
            return AS_SUCCESS;
        }

        return AS_RETRY;
    }

    pl.add_msg_if_player( m_info, _( "Your %s is already enhanced." ), fix.tname() );
    return AS_CANT;
}

std::string repair_item_actor::action_description( repair_item_actor::repair_type rt )
{
    static const std::array<std::string, NUM_REPAIR_TYPES> arr = {{
            translate_marker( "Nothing" ),
            translate_marker( "Repairing" ),
            translate_marker( "Refitting" ),
            translate_marker( "Downsizing" ),
            translate_marker( "Upsizing" ),
            translate_marker( "Reinforcing" ),
            translate_marker( "Practicing" )
        }
    };

    return _( arr[rt] );
}

std::string repair_item_actor::get_name() const
{
    const std::string mats = enumerate_as_string( materials.begin(), materials.end(),
    []( const material_id & mid ) {
        return _( mid->name() );
    } );
    return string_format( _( "Repair %s" ), mats );
}

void heal_actor::load( const JsonObject &obj )
{
    // Mandatory
    move_cost = obj.get_int( "move_cost" );
    limb_power = obj.get_float( "limb_power", 0 );

    // Optional
    bandages_power = obj.get_float( "bandages_power", 0 );
    bandages_scaling = obj.get_float( "bandages_scaling", 0.25f * bandages_power );
    disinfectant_power = obj.get_float( "disinfectant_power", 0 );
    disinfectant_scaling = obj.get_float( "disinfectant_scaling", 0.25f * disinfectant_power );

    head_power = obj.get_float( "head_power", 0.8f * limb_power );
    torso_power = obj.get_float( "torso_power", 1.5f * limb_power );

    limb_scaling = obj.get_float( "limb_scaling", 0.25f * limb_power );
    double scaling_ratio = limb_power < 0.0001f ? 0.0 :
                           static_cast<double>( limb_scaling / limb_power );
    head_scaling = obj.get_float( "head_scaling", scaling_ratio * head_power );
    torso_scaling = obj.get_float( "torso_scaling", scaling_ratio * torso_power );

    bleed = obj.get_float( "bleed", 0.0f );
    bite = obj.get_float( "bite", 0.0f );
    infect = obj.get_float( "infect", 0.0f );

    long_action = obj.get_bool( "long_action", false );

    if( obj.has_array( "effects" ) ) {
        for( const JsonObject e : obj.get_array( "effects" ) ) {
            effects.push_back( load_effect_data( e ) );
        }
    }

    if( obj.has_string( "used_up_item" ) ) {
        obj.read( "used_up_item", used_up_item_id, true );
    } else if( obj.has_object( "used_up_item" ) ) {
        JsonObject u = obj.get_object( "used_up_item" );
        u.read( "id", used_up_item_id, true );
        used_up_item_quantity = u.get_int( "quantity", used_up_item_quantity );
        used_up_item_charges = u.get_int( "charges", used_up_item_charges );
        used_up_item_flags = u.get_tags<flag_id>( "flags" );
    }
}

static player &get_patient( player &healer, const tripoint &pos )
{
    if( healer.pos() == pos ) {
        return healer;
    }

    player *const person = g->critter_at<player>( pos );
    if( !person ) {
        // Default to heal self on failure not to break old functionality
        add_msg( m_debug, "No heal target at position %d,%d,%d", pos.x, pos.y, pos.z );
        return healer;
    }

    return *person;
}

int heal_actor::use( player &p, item &it, bool, const tripoint &pos ) const
{
    if( p.is_underwater() ) {
        p.add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    if( p.is_mounted() ) {
        p.add_msg_if_player( m_info, _( "You can't do that while mounted." ) );
        return 0;
    }

    player &patient = get_patient( p, pos );
    const bodypart_str_id hpp = use_healing_item( p, patient, it, false );
    if( !hpp ) {
        return 0;
    }

    int cost = move_cost;
    if( long_action ) {
        // A hack: long action healing on NPCs isn't done yet.
        // So just heal at start and paralyze the player for 5 minutes.
        cost /= std::min( 10, p.get_skill_level( skill_firstaid ) + 1 );
    }

    // NPCs can use first aid now, but they can't perform long actions
    if( long_action && &patient == &p && !p.is_npc() ) {
        // Assign first aid long action.
        /** @EFFECT_FIRSTAID speeds up firstaid activity */
        p.assign_activity( ACT_FIRSTAID, cost, 0, 0, it.tname() );
        p.activity->targets.emplace_back( &it );
        p.activity->str_values.push_back( hpp.str() );
        p.moves = 0;
        return 0;
    }

    p.moves -= cost;
    p.add_msg_if_player( m_good, _( "You use your %s." ), it.tname() );
    return it.type->charges_to_use();
}

std::unique_ptr<iuse_actor> heal_actor::clone() const
{
    return std::make_unique<heal_actor>( *this );
}

int heal_actor::get_heal_value( const Character &healer, const bodypart_str_id &healed ) const
{
    int heal_base;
    float bonus_mult;
    if( healed == body_part_head ) {
        heal_base = head_power;
        bonus_mult = head_scaling;
    } else if( healed == body_part_torso ) {
        heal_base = torso_power;
        bonus_mult = torso_scaling;
    } else {
        heal_base = limb_power;
        bonus_mult = limb_scaling;
    }

    if( heal_base > 0 ) {
        /** @EFFECT_FIRSTAID increases healing item effects */
        return heal_base + bonus_mult * healer.get_skill_level( skill_firstaid );
    }

    return heal_base;
}

int heal_actor::get_bandaged_level( const Character &healer ) const
{
    if( bandages_power > 0 ) {
        /** @EFFECT_FIRSTAID increases healing item effects */
        return bandages_power + bandages_scaling * healer.get_skill_level( skill_firstaid );
    }

    return bandages_power;
}

int heal_actor::get_disinfected_level( const Character &healer ) const
{
    if( disinfectant_power > 0 ) {
        /** @EFFECT_FIRSTAID increases healing item effects */
        return disinfectant_power + disinfectant_scaling * healer.get_skill_level( skill_firstaid );
    }

    return disinfectant_power;
}

int heal_actor::finish_using( player &healer, player &patient, item &it,
                              const bodypart_str_id &healed ) const
{
    float practice_amount = limb_power * 3.0f;
    const int dam = get_heal_value( healer, healed );

    const bodypart_id bp = healed.id();
    const int cur_hp = patient.get_part_hp_cur( bp );

    if( ( cur_hp >= 1 ) && ( dam > 0 ) ) { // Prevent first-aid from mending limbs
        patient.heal( bp, dam );
    } else if( ( cur_hp >= 1 ) && ( dam < 0 ) ) {
        patient.apply_damage( nullptr, bp, -dam ); //hurt takes + damage
    }

    const bodypart_str_id bp_healed = healed;

    Character &player_character = get_player_character();
    const bool u_see = healer.is_player() || patient.is_player() ||
                       player_character.sees( healer ) || player_character.sees( patient );
    const bool player_healing_player = healer.is_player() && patient.is_player();
    // Need a helper here - messages are from healer's point of view
    // but it would be cool if NPCs could use this function too
    const auto heal_msg = [&]( game_message_type msg_type,
    const char *player_player_msg, const char *other_msg ) {
        if( !u_see ) {
            return;
        }

        if( player_healing_player ) {
            add_msg( msg_type, player_player_msg );
        } else {
            add_msg( msg_type, other_msg );
        }
    };

    if( patient.has_effect( effect_bleed, bp_healed ) ) {
        if( x_in_y( bleed, 1.0f ) ) {
            patient.remove_effect( effect_bleed, bp_healed );
            heal_msg( m_good, _( "You stop the bleeding." ), _( "The bleeding is stopped." ) );
        } else {
            heal_msg( m_warning, _( "You fail to stop the bleeding." ), _( "The wound still bleeds." ) );
        }

        practice_amount += bleed * 3.0f;
    }
    if( patient.has_effect( effect_bite, bp_healed ) ) {
        if( x_in_y( bite, 1.0f ) ) {
            patient.remove_effect( effect_bite, bp_healed );
            heal_msg( m_good, _( "You clean the wound." ), _( "The wound is cleaned." ) );
        } else {
            heal_msg( m_warning, _( "Your wound still aches." ), _( "The wound still looks bad." ) );
        }

        practice_amount += bite * 3.0f;
    }
    if( patient.has_effect( effect_infected, bp_healed ) ) {
        if( x_in_y( infect, 1.0f ) ) {
            const time_duration infected_dur = patient.get_effect_dur( effect_infected, bp_healed );
            patient.remove_effect( effect_infected, bp_healed );
            patient.add_effect( effect_recover, infected_dur );
            heal_msg( m_good, _( "You disinfect the wound." ), _( "The wound is disinfected." ) );
        } else {
            heal_msg( m_warning, _( "Your wound still hurts." ), _( "The wound still looks nasty." ) );
        }

        practice_amount += infect * 10.0f;
    }

    if( long_action ) {
        healer.add_msg_if_player( _( "You finish using the %s." ), it.tname() );
    }

    for( const auto &eff : effects ) {
        patient.add_effect( eff.id, eff.duration, convert_bp( eff.bp ) );
        if( eff.permanent ) {
            patient.get_effect( eff.id, convert_bp( eff.bp ) ).set_permanent();
        }
    }

    const auto copy_flags = [&]( item & it ) {
        for( const auto &flag : used_up_item_flags ) {
            it.set_flag( flag );
        }
    };

    if( !used_up_item_id.is_empty() ) {
        item *used_up = item::spawn_temporary( used_up_item_id, it.birthday() );
        used_up->charges = used_up_item_charges;
        copy_flags( *used_up );
        for( int count = 0; count < used_up_item_quantity; count++ ) {
            healer.i_add_or_drop( item::spawn( *used_up ) );
        }
    }

    // apply healing over time effects
    if( bandages_power > 0 ) {
        int bandages_intensity = get_bandaged_level( healer );
        patient.add_effect( effect_bandaged, 1_turns, bp_healed );
        effect &e = patient.get_effect( effect_bandaged, bp_healed );
        e.set_duration( e.get_int_dur_factor() * bandages_intensity );
        patient.get_part( healed ).set_damage_bandaged( patient.get_part_hp_max(
                    bp ) - patient.get_part_hp_cur( bp ) );
        practice_amount += 3 * bandages_intensity;
    }
    if( disinfectant_power > 0 ) {
        int disinfectant_intensity = get_disinfected_level( healer );
        patient.add_effect( effect_disinfected, 1_turns, bp_healed );
        effect &e = patient.get_effect( effect_disinfected, bp_healed );
        e.set_duration( e.get_int_dur_factor() * disinfectant_intensity );
        patient.get_part( healed ).set_damage_disinfected( patient.get_part_hp_max(
                    bp ) - patient.get_part_hp_cur( bp ) );
        practice_amount += 3 * disinfectant_intensity;
    }
    practice_amount = std::max( 9.0f, practice_amount );

    healer.practice( skill_firstaid, static_cast<int>( practice_amount ) );
    return it.type->charges_to_use();
}

static bodypart_str_id pick_part_to_heal(
    const player &healer, const player &patient,
    const std::string &menu_header,
    int limb_power, int head_bonus, int torso_bonus,
    float bleed_chance, float bite_chance, float infect_chance,
    bool force, float bandage_power, float disinfectant_power )
{
    const bool bleed = bleed_chance > 0.0f;
    const bool bite = bite_chance > 0.0f;
    const bool infect = infect_chance > 0.0f;
    /** @EFFECT_PER slightly increases precision when using first aid */
    /** @EFFECT_FIRSTAID increases precision when using first aid */
    const bool precise = ( healer.get_skill_level( skill_firstaid ) * 4 + healer.per_cur >= 20 );

    while( true ) {
        bodypart_str_id healed_part = patient.body_window( menu_header, force, precise,
                                      limb_power, head_bonus, torso_bonus,
                                      bleed_chance, bite_chance, infect_chance, bandage_power, disinfectant_power );
        if( !healed_part ) {
            return bodypart_str_id::NULL_ID();
        }

        if( ( infect && patient.has_effect( effect_infected, healed_part ) ) ||
            ( bite && patient.has_effect( effect_bite, healed_part ) ) ||
            ( bleed && patient.has_effect( effect_bleed, healed_part ) ) ) {
            return healed_part;
        }

        if( force || patient.get_part_hp_cur( healed_part ) < patient.get_part_hp_max( healed_part ) ) {
            return healed_part;
        }
    }
}

bodypart_str_id heal_actor::use_healing_item( player &healer, player &patient, item &it,
        bool force ) const
{
    bodypart_str_id healed = bodypart_str_id::NULL_ID();
    const int head_bonus = get_heal_value( healer, body_part_head );
    const int limb_power = get_heal_value( healer, body_part_arm_l );
    const int torso_bonus = get_heal_value( healer, body_part_torso );

    if( !patient.can_use_heal_item( it ) ) {
        patient.add_msg_player_or_npc( m_bad,
                                       _( "Your biology is not compatible with that item." ),
                                       _( "<npcname>'s biology is not compatible with that item." ) );
        return bodypart_str_id::NULL_ID(); // canceled
    }

    if( healer.is_npc() ) {
        // NPCs heal whatever has sustained the most damaged that they can heal but never
        // rebandage parts
        int highest_damage = 0;
        for( const auto &part : patient.get_all_body_parts( true ) ) {
            const auto &bp = patient.get_part( part );
            int damage = 0;
            if( ( !patient.has_effect( effect_bandaged, part.id() ) && bandages_power > 0 ) ||
                ( !patient.has_effect( effect_disinfected, part.id() ) && disinfectant_power > 0 ) ) {
                damage += bp.get_hp_max() - bp.get_hp_cur();
                damage += damage > 0 ? bp.get_id()->essential * essential_value : 0;
                damage += bleed * patient.get_effect_dur( effect_bleed, part.id() ) / 5_minutes;
                damage += bite * patient.get_effect_dur( effect_bite, part.id() ) / 10_minutes;
                damage += infect * patient.get_effect_dur( effect_infected, part.id() ) / 10_minutes;
            }
            if( damage > highest_damage ) {
                highest_damage = damage;
                healed = part.id();
            }
        }
    } else if( patient.is_player() ) {
        // Player healing self - let player select
        if( healer.activity->id() != ACT_FIRSTAID ) {
            const std::string menu_header = _( "Select a body part for: " ) + it.tname();
            healed = pick_part_to_heal( healer, patient, menu_header,
                                        limb_power, head_bonus, torso_bonus,
                                        bleed, bite, infect, force,
                                        get_bandaged_level( healer ),
                                        get_disinfected_level( healer ) );
            if( !healed ) {
                add_msg( m_info, _( "Never mind." ) );
                return bodypart_str_id::NULL_ID(); // canceled
            }
        }
        // Brick healing if using a first aid kit for the first time.
        if( long_action && healer.activity->id() != ACT_FIRSTAID ) {
            // Cancel and wait for activity completion.
            return healed;
        } else if( healer.activity->id() == ACT_FIRSTAID ) {
            // Completed activity, extract body part from it.
            healed = bodypart_str_id( healer.activity->str_values[0] );
        }
    } else {
        // Player healing NPC
        // TODO: Remove this hack, allow using activities on NPCs
        const std::string menu_header = string_format( pgettext( "healing",
                                        //~ %1$s: patient name, %2$s: healing item name
                                        "Select a body part of %1$s for %2$s:" ),
                                        patient.disp_name(), it.tname() );
        healed = pick_part_to_heal( healer, patient, menu_header,
                                    limb_power, head_bonus, torso_bonus,
                                    bleed, bite, infect, force,
                                    get_bandaged_level( healer ),
                                    get_disinfected_level( healer ) );
    }

    if( healed ) {
        finish_using( healer, patient, it, healed );
    }

    return healed;
}

void heal_actor::info( const item &, std::vector<iteminfo> &dump ) const
{
    if( head_power > 0 || torso_power > 0 || limb_power > 0 || bandages_power > 0 ||
        disinfectant_power > 0 || bleed > 0.0f || bite > 0.0f || infect > 0.0f ) {
        dump.emplace_back( "HEAL", _( "<bold>Healing effects</bold> " ) );
    }

    Character &player_character = get_player_character();
    if( head_power > 0 || torso_power > 0 || limb_power > 0 ) {
        dump.emplace_back( "HEAL", _( "Base healing: " ) );
        dump.emplace_back( "HEAL_BASE", _( "Head: " ), "", iteminfo::no_newline, head_power );
        dump.emplace_back( "HEAL_BASE", _( "  Torso: " ), "", iteminfo::no_newline, torso_power );
        dump.emplace_back( "HEAL_BASE", _( "  Limbs: " ), limb_power );
        if( g != nullptr ) {
            dump.emplace_back( "HEAL", _( "Actual healing: " ) );
            dump.emplace_back( "HEAL_ACT", _( "Head: " ), "", iteminfo::no_newline,
                               get_heal_value( player_character, body_part_head ) );
            dump.emplace_back( "HEAL_ACT", _( "  Torso: " ), "", iteminfo::no_newline,
                               get_heal_value( player_character, body_part_torso ) );
            dump.emplace_back( "HEAL_ACT", _( "  Limbs: " ), get_heal_value( player_character,
                               body_part_arm_l ) );
        }
    }

    if( bandages_power > 0 ) {
        dump.emplace_back( "HEAL", _( "Base bandaging quality: " ),
                           texitify_base_healing_power( static_cast<int>( bandages_power ) ) );
        if( g != nullptr ) {
            dump.emplace_back( "HEAL", _( "Actual bandaging quality: " ),
                               texitify_healing_power( get_bandaged_level( player_character ) ) );
        }
    }

    if( disinfectant_power > 0 ) {
        dump.emplace_back( "HEAL", _( "Base disinfecting quality: " ),
                           texitify_base_healing_power( static_cast<int>( disinfectant_power ) ) );
        if( g != nullptr ) {
            dump.emplace_back( "HEAL", _( "Actual disinfecting quality: " ),
                               texitify_healing_power( get_disinfected_level( player_character ) ) );
        }
    }

    if( bleed > 0.0f || bite > 0.0f || infect > 0.0f ) {
        dump.emplace_back( "HEAL", _( "Chance to heal (percent): " ) );
        if( bleed > 0.0f ) {
            dump.emplace_back( "HEAL", _( "* Bleeding: " ),
                               static_cast<int>( bleed * 100 ) );
        }
        if( bite > 0.0f ) {
            dump.emplace_back( "HEAL", _( "* Bite: " ),
                               static_cast<int>( bite * 100 ) );
        }
        if( infect > 0.0f ) {
            dump.emplace_back( "HEAL", _( "* Infection: " ),
                               static_cast<int>( infect * 100 ) );
        }
    }

    dump.emplace_back( "HEAL", _( "Moves to use: " ), move_cost );
}

place_trap_actor::place_trap_actor( const std::string &type ) :
    iuse_actor( type ), needs_neighbor_terrain( ter_str_id::NULL_ID() ),
    outer_layer_trap( trap_str_id::NULL_ID() ) {}

place_trap_actor::data::data() : trap( trap_str_id::NULL_ID() ) {}

void place_trap_actor::data::load( const JsonObject &obj )
{
    assign( obj, "trap", trap );
    assign( obj, "done_message", done_message );
    assign( obj, "practice", practice );
    assign( obj, "moves", moves );
}

void place_trap_actor::load( const JsonObject &obj )
{
    assign( obj, "allow_underwater", allow_underwater );
    assign( obj, "allow_under_player", allow_under_player );
    assign( obj, "needs_solid_neighbor", needs_solid_neighbor );
    assign( obj, "needs_neighbor_terrain", needs_neighbor_terrain );
    assign( obj, "bury_question", bury_question );
    if( !bury_question.empty() ) {
        JsonObject buried_json = obj.get_object( "bury" );
        buried_data.load( buried_json );
    }
    unburied_data.load( obj );
    assign( obj, "outer_layer_trap", outer_layer_trap );
}

std::unique_ptr<iuse_actor> place_trap_actor::clone() const
{
    return std::make_unique<place_trap_actor>( *this );
}

static bool is_solid_neighbor( const tripoint &pos, point offset )
{
    map &here = get_map();
    const tripoint a = pos + offset;
    const tripoint b = pos - offset;
    return here.move_cost( a ) != 2 && here.move_cost( b ) != 2;
}

static bool has_neighbor( const tripoint &pos, const ter_id &terrain_id )
{
    map &here = get_map();
    for( const tripoint &t : here.points_in_radius( pos, 1, 0 ) ) {
        if( here.ter( t ) == terrain_id ) {
            return true;
        }
    }
    return false;
}

bool place_trap_actor::is_allowed( player &p, const tripoint &pos, const std::string &name ) const
{
    if( !allow_under_player && pos == p.pos() ) {
        p.add_msg_if_player( m_info, _( "Yeah.  Place the %s at your feet.  Real damn smart move." ),
                             name );
        return false;
    }
    map &here = get_map();
    if( here.move_cost( pos ) != 2 ) {
        p.add_msg_if_player( m_info, _( "You can't place a %s there." ), name );
        return false;
    }
    if( needs_solid_neighbor ) {
        if( !is_solid_neighbor( pos, point_east ) && !is_solid_neighbor( pos, point_south ) &&
            !is_solid_neighbor( pos, point_south_east ) && !is_solid_neighbor( pos, point_north_east ) ) {
            p.add_msg_if_player( m_info, _( "You must place the %s between two solid tiles." ), name );
            return false;
        }
    }
    if( needs_neighbor_terrain && !has_neighbor( pos, needs_neighbor_terrain ) ) {
        p.add_msg_if_player( m_info, _( "The %s needs a %s adjacent to it." ), name,
                             needs_neighbor_terrain.obj().name() );
        return false;
    }
    const trap &existing_trap = here.tr_at( pos );
    if( !existing_trap.is_null() ) {
        if( existing_trap.can_see( pos, p ) ) {
            p.add_msg_if_player( m_info, _( "You can't place a %s there.  It contains a trap already." ),
                                 name );
        } else {
            p.add_msg_if_player( m_bad, _( "You trigger a %s!" ), existing_trap.name() );
            existing_trap.trigger( pos, &p );
        }
        return false;
    }
    return true;
}

static void place_and_add_as_known( player &p, const tripoint &pos, const trap_str_id &id )
{
    map &here = get_map();
    here.trap_set( pos, id );
    const trap &tr = here.tr_at( pos );
    if( !tr.can_see( pos, p ) ) {
        p.add_known_trap( pos, tr );
    }
}

int place_trap_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    const bool could_bury = !bury_question.empty();
    if( !allow_underwater && p.is_underwater() ) {
        p.add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }
    if( p.is_mounted() ) {
        p.add_msg_if_player( m_info, _( "You can't do that while mounted." ) );
        return 0;
    }
    const std::optional<tripoint> pos_ = choose_adjacent( string_format( _( "Place %s where?" ),
                                         it.tname() ) );
    if( !pos_ ) {
        return 0;
    }
    tripoint pos = *pos_;

    if( !is_allowed( p, pos, it.tname() ) ) {
        return 0;
    }

    map &here = get_map();
    int distance_to_trap_center = unburied_data.trap.obj().get_trap_radius() +
                                  outer_layer_trap.obj().get_trap_radius() + 1;
    if( unburied_data.trap.obj().get_trap_radius() > 0 ) {
        // Math correction for multi-tile traps
        pos.x = ( pos.x - p.posx() ) * distance_to_trap_center + p.posx();
        pos.y = ( pos.y - p.posy() ) * distance_to_trap_center + p.posy();
        for( const tripoint &t : here.points_in_radius( pos, outer_layer_trap.obj().get_trap_radius(),
                0 ) ) {
            if( !is_allowed( p, t, it.tname() ) ) {
                p.add_msg_if_player( m_info,
                                     _( "That trap needs a space in %d tiles radius to be clear, centered %d tiles from you." ),
                                     outer_layer_trap.obj().get_trap_radius(), distance_to_trap_center );
                return 0;
            }
        }
    }

    const bool has_shovel = p.has_quality( quality_id( "DIG" ), 3 );
    const bool is_diggable = here.ter( pos )->is_diggable();
    bool bury = false;
    if( could_bury && has_shovel && is_diggable ) {
        bury = query_yn( _( bury_question ) );
    }
    const auto &data = bury ? buried_data : unburied_data;

    p.add_msg_if_player( m_info, _( data.done_message ), distance_to_trap_center );
    p.practice( skill_id( "traps" ), data.practice );
    p.mod_moves( -data.moves );

    place_and_add_as_known( p, pos, data.trap );
    for( const tripoint &t : here.points_in_radius( pos, data.trap.obj().get_trap_radius(), 0 ) ) {
        if( t != pos ) {
            place_and_add_as_known( p, t, outer_layer_trap );
        }
    }
    return 1;
}

void emit_actor::load( const JsonObject &obj )
{
    assign( obj, "emits", emits );
    assign( obj, "scale_qty", scale_qty );
}

int emit_actor::use( player &, item &it, bool, const tripoint &pos ) const
{
    map &here = get_map();
    const float scaling = scale_qty ? it.charges : 1;
    for( const auto &e : emits ) {
        here.emit_field( pos, e, scaling );
    }

    return 1;
}

std::unique_ptr<iuse_actor> emit_actor::clone() const
{
    return std::make_unique<emit_actor>( *this );
}

void emit_actor::finalize( const itype_id &my_item_type )
{
    /*
    // TODO: This must be called after all finalization
    for( const auto& e : emits ) {
        if( !e.is_valid() ) {
            debugmsg( "Item %s has unknown emit source %s", my_item_type.c_str(), e.c_str() );
        }
    }
    */

    if( scale_qty && !item::count_by_charges( my_item_type ) ) {
        debugmsg( "Item %s has emit_actor with scale_qty, but is not counted by charges",
                  my_item_type.c_str() );
        scale_qty = false;
    }
}

void saw_barrel_actor::load( const JsonObject &jo )
{
    assign( jo, "cost", cost );
}

int saw_barrel_actor::use( player &p, item &it, bool t, const tripoint & ) const
{
    if( t ) {
        return 0;
    }

    auto loc = game_menus::inv::saw_barrel( p, it );

    if( !loc ) {
        p.add_msg_if_player( _( "Never mind." ) );
        return 0;
    }

    loc->obtain( p );
    p.add_msg_if_player( _( "You saw down the barrel of your %s." ), loc->tname() );
    loc->put_in( item::spawn( "barrel_small", calendar::turn ) );

    return 0;
}

ret_val<bool> saw_barrel_actor::can_use_on( const player &, const item &, const item &target ) const
{
    if( !target.is_gun() ) {
        return ret_val<bool>::make_failure( _( "It's not a gun." ) );
    }

    if( target.type->gun->barrel_volume <= 0_ml ) {
        return ret_val<bool>::make_failure( _( "The barrel is too short." ) );
    }

    if( target.gunmod_find( itype_barrel_small ) ) {
        return ret_val<bool>::make_failure( _( "The barrel is already sawn-off." ) );
    }

    const auto gunmods = target.gunmods();
    const bool modified_barrel = std::any_of( gunmods.begin(), gunmods.end(),
    []( const item * mod ) {
        return mod->type->gunmod->location == gunmod_location( "barrel" );
    } );

    if( modified_barrel ) {
        return ret_val<bool>::make_failure( _( "Can't saw off modified barrels." ) );
    }

    return ret_val<bool>::make_success();
}

std::unique_ptr<iuse_actor> saw_barrel_actor::clone() const
{
    return std::make_unique<saw_barrel_actor>( *this );
}

void saw_stock_actor::load( const JsonObject &jo )
{
    assign( jo, "cost", cost );
}

int saw_stock_actor::use( player &p, item &it, bool t, const tripoint & ) const
{
    if( t ) {
        return 0;
    }

    auto loc = game_menus::inv::saw_stock( p, it );

    if( !loc ) {
        p.add_msg_if_player( _( "Never mind." ) );
        return 0;
    }

    loc->obtain( p );
    p.add_msg_if_player( _( "You saw down the stock of your %s." ), loc->tname() );
    loc->put_in( item::spawn( "stock_small", calendar::turn ) );

    return 0;
}

ret_val<bool> saw_stock_actor::can_use_on( const player &, const item &, const item &target ) const
{
    if( !target.is_gun() ) {
        return ret_val<bool>::make_failure( _( "It's not a gun." ) );
    }

    if( target.gunmod_find( itype_stock_small ) ) {
        return ret_val<bool>::make_failure( _( "The stock is already sawn-off." ) );
    }

    // Exclude pistols and the like that have had a stock mount bubba'd onto them.
    const auto gunmods = target.gunmods();
    const bool external_stock = std::any_of( gunmods.begin(), gunmods.end(),
    []( const item * mod ) {
        return mod->type->gunmod->location == gunmod_location( "stock mount" );
    } );

    if( external_stock ) {
        return ret_val<bool>::make_failure( _( "You can't saw anything off this." ) );
    }

    // Don't allow trying to stack stock mods.
    const bool modified_stock = std::any_of( gunmods.begin(), gunmods.end(),
    []( const item * mod ) {
        return mod->type->gunmod->location == gunmod_location( "stock" );
    } );

    if( modified_stock ) {
        return ret_val<bool>::make_failure( _( "Can't cut off modified stocks." ) );
    }

    // Also bail out if there's no unmodified stock to touch at all.
    if( target.get_free_mod_locations( gunmod_location( "stock" ) ) == 0 ||
        target.type->gun->skill_used == skill_id( "pistol" ) ) {
        return ret_val<bool>::make_failure(
                   _( "This doesn't have a stock." ) );
    }

    // Stock ideally should be made out of wood.
    if( !target.made_of( material_id( "wood" ) ) ) {
        return ret_val<bool>::make_failure(
                   _( "Can't cut off non-wooden stocks." ) );
    }

    return ret_val<bool>::make_success();
}

std::unique_ptr<iuse_actor> saw_stock_actor::clone() const
{
    return std::make_unique<saw_stock_actor>( *this );
}

int install_bionic_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    if( p.can_install_bionics( *it.type, p, false ) ) {
        return p.install_bionics( *it.type, p, false ) ? it.type->charges_to_use() : 0;
    } else {
        return 0;
    }
}

ret_val<bool> install_bionic_actor::can_use( const Character &p, const item &it, bool,
        const tripoint & ) const
{
    if( !it.is_bionic() ) {
        return ret_val<bool>::make_failure();
    }
    const bionic_id &bid = it.type->bionic->id;
    if( p.is_mounted() ) {
        return ret_val<bool>::make_failure( _( "You can't install bionics while mounted." ) );
    }
    if( !get_option<bool>( "MANUAL_BIONIC_INSTALLATION" ) &&
        !p.has_trait( trait_DEBUG_BIONICS ) ) {
        return ret_val<bool>::make_failure( _( "You can't self-install bionics." ) );
    } else if( !p.has_trait( trait_DEBUG_BIONICS ) ) {
        if( it.has_fault( fault_bionic_nonsterile ) && !p.has_trait( trait_INFRESIST ) ) {
            return ret_val<bool>::make_failure( _( "This CBM is not sterile, you can't install it." ) );
        } else if( units::energy_max - p.get_max_power_level() < bid->capacity ) {
            return ret_val<bool>::make_failure( _( "Max power capacity already reached" ) );
        }
    }

    if( !bid->has_flag( flag_MULTIINSTALL ) && p.has_bionic( bid ) ) {
        return ret_val<bool>::make_failure( _( "You have already installed this bionic." ) );
    } else if( bid->upgraded_bionic && !p.has_bionic( bid->upgraded_bionic ) ) {
        return ret_val<bool>::make_failure( _( "There is nothing to upgrade." ) );
    } else if( character_funcs::has_upgraded_bionic( p, bid ) ) {
        return ret_val<bool>::make_failure( _( "You have a superior version installed." ) );
    }

    return ret_val<bool>::make_success();
}

std::unique_ptr<iuse_actor> install_bionic_actor::clone() const
{
    return std::make_unique<install_bionic_actor>( *this );
}

void install_bionic_actor::finalize( const itype_id &my_item_type )
{
    if( !my_item_type->bionic ) {
        debugmsg( "Item %s has install_bionic actor, but it's not a bionic.", my_item_type.c_str() );
    }
}

int detach_gunmods_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    auto mods = it.gunmods();

    mods.erase( std::remove_if( mods.begin(), mods.end(), std::bind( &item::is_irremovable,
                                std::placeholders::_1 ) ), mods.end() );

    uilist prompt;
    prompt.text = _( "Remove which modification?" );

    for( size_t i = 0; i != mods.size(); ++i ) {
        prompt.addentry( i, true, -1, mods[ i ]->tname() );
    }

    prompt.query();

    if( prompt.ret >= 0 ) {
        item *gm = mods[ prompt.ret ];
        avatar_funcs::gunmod_remove( *p.as_avatar(), it, *gm );
    } else {
        p.add_msg_if_player( _( "Never mind." ) );
    }

    return 0;
}

ret_val<bool> detach_gunmods_actor::can_use( const Character &p, const item &it, bool,
        const tripoint & ) const
{
    const auto mods = it.gunmods();

    if( mods.empty() ) {
        return ret_val<bool>::make_failure( _( "Doesn't appear to be modded." ) );
    }

    const bool no_removables = std::all_of( mods.begin(), mods.end(), std::bind( &item::is_irremovable,
                                            std::placeholders::_1 ) );

    if( no_removables ) {
        return ret_val<bool>::make_failure( _( "None of the mods can be removed." ) );
    }

    if( p.is_worn(
            it ) ) { // Prevent removal of shoulder straps and thereby making the gun un-wearable again.
        return ret_val<bool>::make_failure( _( "Has to be taken off first." ) );
    }

    return ret_val<bool>::make_success();
}

std::unique_ptr<iuse_actor> detach_gunmods_actor::detach_gunmods_actor::clone() const
{
    return std::make_unique<detach_gunmods_actor>( *this );
}

void detach_gunmods_actor::finalize( const itype_id &my_item_type )
{
    if( !my_item_type->gun ) {
        debugmsg( "Item %s has detach_gunmods_actor actor, but it's a gun.", my_item_type.c_str() );
    }
}

std::unique_ptr<iuse_actor> mutagen_actor::clone() const
{
    return std::make_unique<mutagen_actor>( *this );
}

void mutagen_actor::load( const JsonObject &obj )
{
    mutation_category = mutation_category_id( obj.get_string( "mutation_category", "ANY" ) );
    is_weak = obj.get_bool( "is_weak", false );
    is_strong = obj.get_bool( "is_strong", false );
}

int mutagen_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    mutagen_attempt checks =
        mutagen_common_checks( p, it, false, mutagen_technique::consumed_mutagen );

    if( !checks.allowed ) {
        return checks.charges_used;
    }

    bool no_category = mutation_category == mutation_category_id( "ANY" );
    bool balanced = get_option<bool>( "BALANCED_MUTATIONS" );
    int accumulated_mutagen = p.get_effect_int( effect_accumulated_mutagen );
    if( balanced && !is_strong && is_weak && accumulated_mutagen < 2 && no_category && !p.query_yn(
            _( "Looking at it just makes you tired.  It probably won't work.  Do you want to try anyway?" )
        ) ) {
        return 0;
    }
    if( is_weak && !one_in( 3 ) && !balanced ) {
        // Nothing! Mutagenic flesh often just fails to work.
        return it.type->charges_to_use();
    }

    if( balanced && no_category ) {
        for( int i = ( is_strong ? 1 : 0 ) + ( is_weak ? 0 : 1 ); i > 0; i-- ) {
            p.add_effect( effect_accumulated_mutagen, 2_days, bodypart_str_id::NULL_ID() );
        }
    }
    const mutation_category_trait &m_category = mutation_category_trait::get_category(
                mutation_category );

    if( p.has_trait( trait_MUT_JUNKIE ) ) {
        p.add_msg_if_player( m_good, _( "You quiver with anticipation…" ) );
        p.add_morale( MORALE_MUTAGEN, 5, 50 );
    }

    p.add_msg_if_player( m_category.mutagen_message() );

    if( one_in( 6 ) ) {
        p.add_msg_player_or_npc( m_bad,
                                 _( "You suddenly feel dizzy, and collapse to the ground." ),
                                 _( "<npcname> suddenly collapses to the ground!" ) );
        p.add_effect( effect_downed, 20_turns, bodypart_str_id::NULL_ID(), 0 );
    }

    int mut_count = 1 + ( is_strong ? one_in( 3 ) : 0 );

    for( int i = 0; i < mut_count; i++ ) {
        p.mutate_category( m_category.id );
        p.mod_pain( m_category.mutagen_pain * rng( 1, 5 ) );
    }
    // burn calories directly
    p.mod_stored_nutr( m_category.mutagen_hunger * mut_count );
    p.mod_thirst( m_category.mutagen_thirst * mut_count );
    p.mod_fatigue( m_category.mutagen_fatigue * mut_count );

    return it.type->charges_to_use();
}

std::unique_ptr<iuse_actor> mutagen_iv_actor::clone() const
{
    return std::make_unique<mutagen_iv_actor>( *this );
}

void mutagen_iv_actor::load( const JsonObject &obj )
{
    mutation_category = mutation_category_id( obj.get_string( "mutation_category", "ANY" ) );
    tier = obj.get_int( "tier", 1 ); // fallback of 1 because IV mutagen usually is used for thresholds
}

int mutagen_iv_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    mutagen_attempt checks =
        mutagen_common_checks( p, it, false, mutagen_technique::injected_mutagen );

    if( !checks.allowed ) {
        return checks.charges_used;
    }

    const mutation_category_trait &m_category = mutation_category_trait::get_category(
                mutation_category );

    if( p.has_trait( trait_MUT_JUNKIE ) ) {
        p.add_msg_if_player( m_category.junkie_message() );
    } else {
        p.add_msg_if_player( m_category.iv_message() );
    }

    // try to cross the threshold to be able to get post-threshold mutations this iv.
    test_crossing_threshold( p, m_category, tier );

    // TODO: Remove the "is_player" part, implement NPC screams
    if( p.is_player() && !( p.has_trait( trait_NOPAIN ) ) && m_category.iv_sound ) {
        p.mod_pain( m_category.iv_pain );
        /** @EFFECT_STR increases volume of painful shouting when using IV mutagen */
        sounds::sound( p.pos(), m_category.iv_noise + p.str_cur, sounds::sound_t::alert,
                       m_category.iv_sound_message(), true, m_category.iv_sound_id(), m_category.iv_sound_variant() );
    }

    int mut_count = m_category.iv_min_mutations;
    for( int i = 0; i < m_category.iv_additional_mutations; ++i ) {
        if( !one_in( m_category.iv_additional_mutations_chance ) ) {
            ++mut_count;
        }
    }

    for( int i = 0; i < mut_count; i++ ) {
        p.mutate_category( m_category.id );
        p.mod_pain( m_category.iv_pain  * rng( 1, 5 ) );
    }

    p.mod_stored_kcal( -10 * m_category.iv_hunger * mut_count );
    p.mod_thirst( m_category.iv_thirst * mut_count );
    p.mod_fatigue( m_category.iv_fatigue * mut_count );

    if( m_category.id == mutation_category_id( "CHIMERA" ) ) {
        p.add_morale( MORALE_MUTAGEN_CHIMERA, m_category.iv_morale, m_category.iv_morale_max );
    } else if( m_category.id == mutation_category_id( "ELFA" ) ) {
        p.add_morale( MORALE_MUTAGEN_ELF, m_category.iv_morale, m_category.iv_morale_max );
    } else if( m_category.iv_morale > 0 ) {
        p.add_morale( MORALE_MUTAGEN_MUTATION, m_category.iv_morale, m_category.iv_morale_max );
    }

    if( m_category.iv_sleep && !one_in( 3 ) ) {
        p.add_msg_if_player( m_bad, m_category.iv_sleep_message() );
        /** @EFFECT_INT reduces sleep duration when using IV mutagen */
        p.fall_asleep( time_duration::from_turns( m_category.iv_sleep_dur - p.int_cur * 5 ) );
    }

    // try crossing again after getting new in-category mutations.
    test_crossing_threshold( p, m_category, tier );

    return it.type->charges_to_use();
}

std::unique_ptr<iuse_actor> deploy_tent_actor::clone() const
{
    return std::make_unique<deploy_tent_actor>( *this );
}

void deploy_tent_actor::load( const JsonObject &obj )
{
    assign( obj, "radius", radius );
    assign( obj, "wall", wall );
    assign( obj, "floor", floor );
    assign( obj, "floor_center", floor_center );
    assign( obj, "door_opened", door_opened );
    assign( obj, "door_closed", door_closed );
    assign( obj, "broken_type", broken_type );
}

int deploy_tent_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    int diam = 2 * radius + 1;
    if( p.is_mounted() ) {
        p.add_msg_if_player( _( "You cannot do that while mounted." ) );
        return 0;
    }
    const std::optional<tripoint> dir = choose_direction( string_format(
                                            _( "Put up the %s where (%dx%d clear area)?" ), it.tname(), diam, diam ) );
    if( !dir ) {
        return 0;
    }
    const tripoint direction = *dir;

    map &here = get_map();
    // We place the center of the structure (radius + 1)
    // spaces away from the player.
    // First check there's enough room.
    const tripoint center = p.pos() + tripoint( ( radius + 1 ) * direction.x,
                            ( radius + 1 ) * direction.y, 0 );
    for( const tripoint &dest : here.points_in_radius( center, radius ) ) {
        if( const auto vp = here.veh_at( dest ) ) {
            add_msg( m_info, _( "The %s is in the way." ), vp->vehicle().name );
            return 0;
        }
        if( const Creature *const c = g->critter_at( dest ) ) {
            add_msg( m_info, _( "%s is in the way." ), c->disp_name( false, true ) );
            return 0;
        }
        if( here.impassable( dest ) || !here.has_flag( "FLAT", dest ) ) {
            add_msg( m_info, _( "The %s in that direction isn't suitable for placing the %s." ),
                     here.name( dest ), it.tname() );
            return 0;
        }
        if( here.has_furn( dest ) ) {
            add_msg( m_info, _( "There is already furniture (%s) there." ), here.furnname( dest ) );
            return 0;
        }
    }
    // Make a square of floor surrounded by wall.
    for( const tripoint &dest : here.points_in_radius( center, radius ) ) {
        here.furn_set( dest, wall );
    }
    for( const tripoint &dest : here.points_in_radius( center, radius - 1 ) ) {
        here.furn_set( dest, floor );
    }
    // Place the center floor and the door.
    if( floor_center ) {
        here.furn_set( center, *floor_center );
    }
    here.furn_set( p.pos() + direction, door_closed );
    add_msg( m_info, _( "You set up the %s on the ground." ), it.tname() );
    add_msg( m_info, _( "Examine the center square to pack it up again." ) );
    return 1;
}

bool deploy_tent_actor::check_intact( const tripoint &center ) const
{
    map &here = get_map();
    for( const tripoint &dest : here.points_in_radius( center, radius ) ) {
        const furn_id fid = here.furn( dest );
        if( dest == center && floor_center ) {
            if( fid != *floor_center ) {
                return false;
            }
        } else if( square_dist( dest, center ) < radius ) {
            // So we are inside the tent
            if( fid != floor ) {
                return false;
            }
        } else {
            // We are on the border of the tent
            if( fid != wall && fid != door_opened && fid != door_closed ) {
                return false;
            }
        }
    }
    return true;
}

void weigh_self_actor::info( const item &, std::vector<iteminfo> &dump ) const
{
    dump.emplace_back( "DESCRIPTION",
                       _( "Use this item to weigh yourself.  Includes everything you are wearing." ) );
}

int weigh_self_actor::use( player &p, item &, bool, const tripoint & ) const
{
    if( p.is_mounted() ) {
        p.add_msg_if_player( m_info, _( "You cannot weigh yourself while mounted." ) );
        return 0;
    }
    // this is a weight, either in kgs or in lbs.
    double weight = convert_weight( p.get_weight() );
    if( weight > convert_weight( max_weight ) ) {
        popup( _( "ERROR: Max weight of %.0f %s exceeded" ), convert_weight( max_weight ), weight_units() );
    } else {
        popup( "%.0f %s", weight, weight_units() );
    }
    return 0;
}

void weigh_self_actor::load( const JsonObject &jo )
{
    assign( jo, "max_weight", max_weight );
}

std::unique_ptr<iuse_actor> weigh_self_actor::clone() const
{
    return std::make_unique<weigh_self_actor>( *this );
}

void gps_device_actor::info( const item &, std::vector<iteminfo> &dump ) const
{
    dump.emplace_back( "DESCRIPTION",
                       string_format( _( "This item uses up (%.2f) additional charges per tile revealed." ),
                                      additional_charges_per_tile ) );
}

int gps_device_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    float charges_built_up = 1.0;
    const tripoint_abs_omt center = p.global_omt_location();

    std::string query = string_input_popup()
                        .title( _( "Search for location:" ) )
                        .width( 40 )
                        .query_string();

    if( query.size() < 3 ) {
        p.add_msg_if_player( m_info, _( "Please enter at least 3 characters." ) );
        return 0;
    }

    // Exclude natural terrain types. This item should NOT obsolete other items, just be useful for the player.
    // This helps with that philosophy, since to actually properly survey the area you still need to get to high ground.
    static const std::vector<std::string> natural_terrains = {
        "air", "forest", "forest_thick", "forest_water", "field", "lake_surface", "lake_shore",
        "swamp", "stream", "stream_corner", "stream_end", "river_center", "river_shore", "river_bank", "deep_water", "shallow_water"
    };

    // Build list of matching terrain IDs whose display name matches the query
    std::vector<std::string> matching_ids;
    for( const oter_t &oter : overmap_terrains::get_all() ) {
        // get_name() returns the human‐readable display name
        if( lcmatch( oter.get_name(), query ) ) {
            matching_ids.push_back( oter.get_mapgen_id() );
        }
    }

    // Configure search to look only for those matching IDs
    omt_find_params params{};
    params.search_range = { 0, radius };
    params.types.clear();
    for( const auto &id_str : matching_ids ) {
        params.types.emplace_back( id_str, ot_match_type::type );
    }
    for( const std::string &nt : natural_terrains ) {
        params.exclude_types.emplace_back( nt, ot_match_type::type );
    }
    params.existing_only  = false;
    params.search_layers  = omt_find_above_ground_layer;
    params.explored       = false;
    if( it.has_flag( flag_USE_UPS ) ) {
        params.max_results = static_cast<size_t>( 1 + p.charges_of( itype_UPS ) /
                             additional_charges_per_tile );
    } else {
        params.max_results = static_cast<size_t>( 1 + it.ammo_remaining() / additional_charges_per_tile );
    }
    params.popup          = make_shared_fast<throbber_popup>( _( "Searching…" ) );

    const auto places = overmap_buffer.find_all( center, params );
    params.popup = nullptr;

    if( places.empty() ) {
        p.add_msg_if_player( m_info, _( "No locations found for \"%s\"." ), query );
        return 1;
    }

    // Group by display name
    std::multimap<std::string, tripoint_abs_omt> grouped;
    std::set<std::string> unique_names;
    for( const auto &pt : places ) {
        const std::string name = overmap_buffer.ter( pt ).obj().get_name();
        grouped.insert( { name, pt } );
        unique_names.insert( name );
        charges_built_up += additional_charges_per_tile;
    }
    if( it.has_flag( flag_USE_UPS ) ) {
        if( !p.has_charges( itype_UPS, charges_built_up ) ) {
            p.add_msg_if_player( m_info, _( "Requires %.1f charges, but only %d remaining." ),
                                 charges_built_up, p.charges_of( itype_UPS ) - 1 );
            return 1;
        }
    } else if( 1 + it.ammo_remaining() < charges_built_up ) {
        p.add_msg_if_player( m_info, _( "Requires %.1f charges, but only %d remaining." ),
                             charges_built_up, it.ammo_remaining() - 1 );
        return 1;
    }

    // I don't think this will actually ever be called, but we're leaving it here for now
    if( unique_names.empty() ) {
        p.add_msg_if_player( m_info, _( "Nothing new to display." ) );
        return 1;
    }

    p.add_msg_if_player( m_good, _( "You add the GPS results to your map." ) );
    // Device has enough charge and nothing has gone wrong, reveal on overmap the locations!
    for( const auto &pt : places ) {
        overmap_buffer.reveal( pt, 0 );
    }
    uistate.overmap_highlighted_omts.clear();

    // Let the player pick which name to highlight
    const std::vector<std::string> name_list( unique_names.begin(), unique_names.end() );
    uilist ui;
    for( size_t i = 0; i < name_list.size(); ++i ) {
        ui.addentry( i, true, MENU_AUTOASSIGN,
                     string_format( "%s (%d)", name_list[i], grouped.count( name_list[i] ) ) );
    }
    ui.query();
    if( ui.ret < 0 ) {
        return charges_built_up;
    }

    const tripoint_abs_omt plr_pos = p.global_omt_location();
    auto range = grouped.equal_range( name_list[ui.ret] );
    const int count = std::distance( range.first, range.second );
    if( count == 0 ) {
        return charges_built_up;
    }

    // Highlight all matching points
    std::transform( range.first, range.second,
                    std::inserter( uistate.overmap_highlighted_omts,
                                   uistate.overmap_highlighted_omts.end() ),
    []( const auto & e ) {
        return e.second;
    }
                  );

    if( count == 1 ) {
        ui::omap::choose_point( range.first->second );
        return charges_built_up;
    }

    // If there are multiple, ask for closest vs random
    ui.reset();
    ui.addentry( 0, true, 'c', _( "Closest" ) );
    ui.addentry( 1, true, 'r', _( "Random" ) );
    ui.query();
    if( ui.ret < 0 ) {
        return charges_built_up;
    }

    if( ui.ret == 1 ) {
        auto it = range.first;
        std::advance( it, rng( 0, count - 1 ) );
        ui::omap::choose_point( it->second );
    } else {
        const auto cmp = [&]( const auto & a, const auto & b ) {
            return trig_dist_squared( plr_pos.raw(), a.second.raw() ) <
                   trig_dist_squared( plr_pos.raw(), b.second.raw() );
        };
        const auto it = std::min_element( range.first, range.second, cmp );
        ui::omap::choose_point( it->second );
    }

    return charges_built_up;
}

void gps_device_actor::load( const JsonObject &jo )
{
    assign( jo, "radius", radius );
    assign( jo, "additional_charges_per_tile", additional_charges_per_tile );
}

std::unique_ptr<iuse_actor> gps_device_actor::clone() const
{
    return std::make_unique<gps_device_actor>( *this );
}

void sew_advanced_actor::load( const JsonObject &obj )
{
    // Mandatory:
    for( const std::string line : obj.get_array( "materials" ) ) {
        materials.emplace( line );
    }
    for( const std::string line : obj.get_array( "clothing_mods" ) ) {
        clothing_mods.emplace_back( line );
    }

    // TODO: Make skill non-mandatory while still erroring on invalid skill
    const std::string skill_string = obj.get_string( "skill" );
    used_skill = skill_id( skill_string );
    if( !used_skill.is_valid() ) {
        obj.throw_error( "Invalid skill", "skill" );
    }
}

int sew_advanced_actor::use( player &p, item &it, bool, const tripoint & ) const
{
    if( p.is_npc() ) {
        return 0;
    }
    if( p.is_mounted() ) {
        p.add_msg_if_player( m_info, _( "You cannot do that while mounted." ) );
        return 0;
    }
    if( p.is_underwater() ) {
        p.add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return 0;
    }

    if( !character_funcs::can_see_fine_details( p ) ) {
        add_msg( m_info, _( "You can't see to sew!" ) );
        return 0;
    }

    auto filter = [this]( const item & itm ) {
        return itm.is_armor() && !itm.is_firearm() && !itm.is_power_armor() &&
               itm.made_of_any( materials );
    };
    // note: if !p.is_npc() then p is avatar.
    item *loc = game_menus::inv::titled_filter_menu(
                    filter, *p.as_avatar(), _( "Enhance which clothing?" ) );
    if( !loc ) {
        p.add_msg_if_player( m_info, _( "You do not have that item!" ) );
        return 0;
    }
    item &mod = *loc;
    if( &mod == &it ) {
        p.add_msg_if_player( m_info,
                             _( "This can be used to repair or modify other items, not itself." ) );
        return 0;
    }

    // Gives us an item with the mod added or removed (toggled)
    const auto modded_copy = []( const item & proto, const flag_id & mod_type ) {
        item *mcopy = item::spawn_temporary( proto );
        if( mcopy->has_own_flag( mod_type ) == 0 ) {
            mcopy->set_flag( mod_type );
        } else {
            mcopy->unset_flag( mod_type );
        }

        return mcopy;
    };

    // Cache available materials
    std::map< itype_id, bool > has_enough;
    const int items_needed = mod.volume() / 750_ml + 1;
    const inventory &crafting_inv = p.crafting_inventory();
    // Go through all discovered repair items and see if we have any of them available
    for( auto &cm : clothing_mods::get_all() ) {
        has_enough[cm.item_string] =
            item::count_by_charges( cm.item_string )
            ? crafting_inv.has_charges( cm.item_string, items_needed )
            : crafting_inv.has_amount( cm.item_string, items_needed );
    }

    int mod_count = 0;
    for( auto &cm : clothing_mods::get_all() ) {
        mod_count += mod.has_own_flag( cm.flag );
    }

    // We need extra thread to lose it on bad rolls
    const int thread_needed = mod.volume() / 125_ml + 10;

    std::vector<std::string> valid_mods;
    if( mod.find_armor_data() ) {
        valid_mods = mod.find_armor_data()->valid_mods;
    }

    if( mod.has_flag( flag_VARSIZE ) && !mod.has_flag( flag_OVERSIZE ) ) {
        valid_mods.push_back( "resized_large" );
    }
    if( !mod.has_flag( flag_UNDERSIZE ) && mod.has_flag( flag_OVERSIZE ) ) {
        valid_mods.push_back( "resized_small" );
    }

    const auto get_compare_color = [&]( const int before, const int after,
    const bool higher_is_better ) {
        return before == after ? c_unset : ( ( after > before ) == higher_is_better ? c_light_green :
                                             c_red );
    };
    const auto get_volume_compare_color = [&]( const units::volume & before,
                                          const units::volume & after,
    const bool higher_is_better ) {
        return before == after ? c_unset : ( ( after > before ) == higher_is_better ? c_light_green :
                                             c_red );
    };
    const auto format_desc_string = [&]( const std::string & label, const int before, const int after,
    const bool higher_is_better ) {
        return colorize( string_format( "%s: %d->%d\n", label, before, after ), get_compare_color( before,
                         after, higher_is_better ) );
    };

    uilist tmenu;
    tmenu.text = _( "How do you want to modify it?" );

    int index = 0;
    for( auto cm : clothing_mods ) {
        auto obj = cm.obj();
        item &temp_item = *modded_copy( mod, obj.flag );
        temp_item.update_clothing_mod_val();

        bool enab = false;
        std::string prompt;
        // TODO: Fix for UTF-8 strings
        // TODO: find other places where this is used and make a global function for all
        static const auto tolower = []( std::string t ) {
            if( !t.empty() ) {
                t.front() = std::tolower( t.front() );
            }
            return t;
        };
        const bool already_resized = mod.has_flag( flag_resized_large ) ||
                                     mod.has_flag( flag_resized_small );
        if( !mod.has_own_flag( obj.flag ) ) {
            // Mod not already present, check if modification is possible
            if( obj.restricted &&
                std::find( valid_mods.begin(), valid_mods.end(), obj.flag.str() ) == valid_mods.end() ) {
                //~ %1$s: modification desc, %2$s: mod name
                prompt = string_format( _( "Can't %1$s (incompatible with %2$s)" ), tolower( obj.implement_prompt ),
                                        mod.tname( 1, false ) );
            } else if( ( obj.flag == flag_resized_large || obj.flag == flag_resized_small ) &&
                       already_resized ) {
                //~ %1$s: modification desc
                prompt = string_format( _( "Can't %1$s (already resized)" ),
                                        tolower( obj.implement_prompt ) );
            } else if( it.ammo_remaining() < thread_needed ) {
                //~ %1$s: modification desc, %2$d: number of charges needed
                prompt = string_format( _( "Can't %1$s (need %2$d charges loaded)" ),
                                        tolower( obj.implement_prompt ), thread_needed );
            } else if( !has_enough[obj.item_string] ) {
                //~ %1$s: modification desc, %2$d: number of items needed, %3$s: items needed
                prompt = string_format( _( "Can't %1$s (need %2$d %3$s)" ), tolower( obj.implement_prompt ),
                                        items_needed, item::nname( obj.item_string, items_needed ) );
            } else {
                // Modification is possible unless we're wearing it and doing so would make it not fit
                if( p.is_worn( mod ) && !p.can_wear( temp_item ).success() ) {
                    prompt = string_format( _( "Can't %s while wearing it" ), tolower( obj.implement_prompt ) );
                } else {
                    enab = true;
                    //~ %1$s: modification desc, %2$d: number of items needed, %3$s: items needed, %4$s: number of charges needed
                    prompt = string_format( _( "%1$s (%2$d %3$s and %4$d charges)" ), tolower( obj.implement_prompt ),
                                            items_needed, item::nname( obj.item_string, items_needed ), thread_needed );
                }
            }

        } else {
            // Mod already present, give option to destroy, unless we're wearing it and doing so would make it not fit
            if( p.is_worn( mod ) && !p.can_wear( temp_item ).success() ) {
                prompt = string_format( _( "Can't %s while wearing it" ), tolower( obj.destroy_prompt ) );
            } else {
                enab = true;
                prompt = _( obj.destroy_prompt );
            }
        }
        std::string desc;
        desc += format_desc_string( _( "Bash" ), mod.bash_resist(), temp_item.bash_resist(), true );
        desc += format_desc_string( _( "Cut" ), mod.cut_resist(), temp_item.cut_resist(), true );
        desc += format_desc_string( _( "Ballistic" ), mod.bullet_resist(), temp_item.bullet_resist(),
                                    true );
        desc += format_desc_string( _( "Acid" ), mod.acid_resist(), temp_item.acid_resist(), true );
        desc += format_desc_string( _( "Fire" ), mod.fire_resist(), temp_item.fire_resist(), true );
        desc += format_desc_string( _( "Warmth" ), mod.get_warmth(), temp_item.get_warmth(), true );
        desc += format_desc_string( _( "Encumbrance" ), mod.get_avg_encumber( p ),
                                    temp_item.get_avg_encumber( p ), false );
        auto before = mod.get_storage();
        auto after = temp_item.get_storage();
        desc += colorize( string_format( "%s: %s %s->%s %s\n", _( "Storage" ),
                                         format_volume( before ), volume_units_abbr(), format_volume( after ),
                                         volume_units_abbr() ), get_volume_compare_color( before, after, true ) );

        tmenu.addentry_desc( index++, enab, MENU_AUTOASSIGN, prompt, desc );
    }
    tmenu.textwidth = 80;
    tmenu.desc_enabled = true;
    tmenu.query();
    const int choice = tmenu.ret;

    if( choice < 0 || choice >= static_cast<int>( clothing_mods.size() ) ) {
        return 0;
    }

    // The mod player picked
    const flag_id &the_mod = clothing_mods[choice].obj().flag;

    // If the picked mod already exists, player wants to destroy it
    if( mod.has_own_flag( the_mod ) ) {
        if( query_yn( _( "Are you sure?  You will not gain any materials back." ) ) ) {
            mod.unset_flag( the_mod );
        }
        mod.update_clothing_mod_val();

        return 0;
    }

    // Get the id of the material used
    const auto &repair_item = clothing_mods[choice].obj().item_string;

    std::vector<item_comp> comps;
    comps.emplace_back( repair_item, items_needed );
    // TODO: this may take up to 2 minutes, and so should start an activity instead
    p.moves -= to_moves<int>( 30_seconds * character_funcs::fine_detail_vision_mod( p ) );
    p.practice( used_skill, items_needed * 3 + 3 );
    /** @EFFECT_TAILOR randomly improves clothing modification efforts */
    int rn = dice( 3, 2 + p.get_skill_level( used_skill ) ); // Skill
    /** @EFFECT_DEX randomly improves clothing modification efforts */
    rn += rng( 0, p.dex_cur / 2 );                    // Dexterity
    /** @EFFECT_PER randomly improves clothing modification efforts */
    rn += rng( 0, p.per_cur / 2 );                    // Perception
    rn -= mod_count * 10;                              // Other mods

    if( rn <= 8 ) {
        const std::string startdurability = mod.durability_indicator( true );
        const auto destroyed = mod.inc_damage();
        const std::string resultdurability = mod.durability_indicator( true );
        p.add_msg_if_player( m_bad, _( "You damage your %s trying to modify it!  ( %s-> %s)" ),
                             mod.tname( 1, false ), startdurability, resultdurability );
        if( destroyed ) {
            p.add_msg_if_player( m_bad, _( "You destroy it!" ) );
            p.i_rem_keep_contents( p.get_item_position( &mod ) );
        }
        return thread_needed / 2;
    } else if( rn <= 10 ) {
        p.add_msg_if_player( m_bad,
                             _( "You fail to modify the clothing, and you waste charges and materials." ) );
        p.consume_items( comps, 1, is_crafting_component );
        return thread_needed;
    } else if( rn <= 14 ) {
        p.add_msg_if_player( m_mixed, _( "You modify your %s, but waste a lot of charges." ),
                             mod.tname() );
        p.consume_items( comps, 1, is_crafting_component );
        mod.set_flag( the_mod );
        mod.update_clothing_mod_val();
        return thread_needed;
    }

    p.add_msg_if_player( m_good, _( "You modify your %s!" ), mod.tname() );
    mod.set_flag( the_mod );
    mod.update_clothing_mod_val();
    p.consume_items( comps, 1, is_crafting_component );
    return thread_needed / 2;
}

std::unique_ptr<iuse_actor> sew_advanced_actor::clone() const
{
    return std::make_unique<sew_advanced_actor>( *this );
}

void change_scent_iuse::load( const JsonObject &obj )
{
    scenttypeid = scenttype_id( obj.get_string( "scent_typeid" ) );
    if( !scenttypeid.is_valid() ) {
        obj.throw_error( "Invalid scent type id.", "scent_typeid" );
    }
    if( obj.has_array( "effects" ) ) {
        for( JsonObject e : obj.get_array( "effects" ) ) {
            effects.push_back( load_effect_data( e ) );
        }
    }
    assign( obj, "moves", moves );
    assign( obj, "charges_to_use", charges_to_use );
    assign( obj, "scent_mod", scent_mod );
    assign( obj, "duration", duration );
    assign( obj, "waterproof", waterproof );
}

int change_scent_iuse::use( player &p, item &it, bool, const tripoint & ) const
{
    p.set_value( "prev_scent", p.get_type_of_scent().c_str() );
    if( waterproof ) {
        p.set_value( "waterproof_scent", "true" );
    }
    p.add_effect( efftype_id( "masked_scent" ), duration, bodypart_str_id::NULL_ID(), scent_mod );
    p.set_type_of_scent( scenttypeid );
    p.mod_moves( -moves );
    add_msg( m_info, _( "You use the %s to mask your scent" ), it.tname() );

    // Apply the various effects.
    for( const auto &eff : effects ) {
        p.add_effect( eff.id, eff.duration, convert_bp( eff.bp ) );
        if( eff.permanent ) {
            p.get_effect( eff.id, convert_bp( eff.bp ) ).set_permanent();
        }
    }
    return charges_to_use;
}

std::unique_ptr<iuse_actor> change_scent_iuse::clone() const
{
    return std::make_unique<change_scent_iuse>( *this );
}

void cloning_syringe_iuse::load( const JsonObject &obj )
{
    assign( obj, "moves", moves );
    assign( obj, "charges_to_use", charges_to_use );
}

int cloning_syringe_iuse::use( player &p, item &it, bool, const tripoint &pos ) const
{
    const auto is_empty_usb = []( const item & drive ) {
        return drive.contents.empty();
    };

    if( !it.units_sufficient( p, charges_to_use ) ) {
        add_msg( m_info, _( "There's not enough charge left in the %s." ), it.display_name() );
        return 0;
    }
    if( !p.has_amount( itype_usb_drive, 1, true, is_empty_usb ) ) {
        add_msg( m_bad, "You need an empty USB drive to store genetic data." );
        return 0;
    }

    const std::string query = string_format( _( "Select which creature?" ) );
    const std::optional<tripoint> pnt_ = choose_adjacent( query );

    if( !pnt_ ) {
        // No valid point was chosen — handle this case, maybe just return
        return 0;
    }

    // Extract the tripoint from the optional
    const tripoint &pnt = *pnt_;
    const Creature *const critter = g->critter_at( pnt );
    if( !critter ) {
        add_msg( m_info, _( "There's no creature there." ) );
        return 0;
    }

    monster *const m = const_cast<monster *>( critter->as_monster() );
    if( !m ) {
        add_msg( m_info, _( "There's no creature there." ) );
        return 0;
    }

    const int fa_skill = p.get_skill_level( skill_firstaid );
    // Convert first aid skill into success chance.
    // Each skill level = +15% chance, but we clamp between 15–95%
    // so there is always a small chance to succeed (even unskilled)
    // and a small chance to fail (even at max skill).
    const int chance = clamp( fa_skill * 10, 15, 95 );

    // use moves and damage mon
    p.mod_moves( -moves );
    m->apply_damage( &p, bodypart_id( "torso" ), 1 );

    if( !x_in_y( chance, 100 ) ) {
        add_msg( m_bad, _( "The %s emits a loud error beep!  You failed to gather a sufficient sample." ),
                 it.display_name() );
        sounds::sound( pos, 8, sounds::sound_t::alarm, _( "beep!" ), true, "misc", "beep" );
        // add actual noise here
        return charges_to_use;
    }

    // we can only grow organic matter, and some species are invalid
    bool in_bad_species = m->in_species( species_HALLUCINATION ) || m->in_species( species_ROBOT ) ||
                          m->in_species( species_ZOMBIE ) || m->in_species( species_NETHER ) ||
                          m->in_species( species_SKELETON );
    if( m->has_flag( MF_CANT_CLONE ) || in_bad_species ) {
        add_msg( m_info,
                 _( "The %s emits two error beeps.  This creature can't provide a valid sample." ) );
        return 0;
    }

    // technically you can't use the same creature for two different scans, but you should be able to copy USB so doesn't matter
    if( m->get_value( "genome_scanned" ) == "true" ) {
        add_msg( m_info, _( "That creature's genome has already been scanned." ) );
        return 0;
    }

    m->set_value( "genome_scanned", "true" );

    const mtype_id &id = m->type->id;
    const std::string id_str = id.str();

    add_msg( m_good, _( "The %s beeps softly.  You successfully gathered a sample from the %s!" ),
             it.display_name(), m->name() );


    auto drives = p.all_items_with_id( itype_genome_drive );

    for( size_t z = 0; z < drives.size(); z++ ) {
        if( drives[z]->get_var( "specimen_sample" ) == id_str ) {
            int progress = drives[z]->get_var( "specimen_sample_progress", 0 );
            int size = drives[z]->get_var( "specimen_size" ).empty() ?
                       static_cast<int>( m->get_size() ) + 1 :
                       std::stoi( drives[z]->get_var( "specimen_size" ) );

            // Increment progress, but don't exceed size
            if( progress < size ) {
                progress++;
                drives[z]->set_var( "specimen_sample_progress", std::to_string( progress ) );
                add_msg( m_info, "Progress: %d/%d for genome sample.", progress, size );
                if( progress == size ) {
                    add_msg( m_good, "Sample is complete." );
                }
            } else {
                add_msg( "Sample is already complete." );
            }

            return charges_to_use;
        }
    }

    // Create new genome drive
    p.use_amount( itype_usb_drive, 1, is_empty_usb );
    detached_ptr<item> drive = item::spawn( itype_genome_drive, calendar::turn );
    int size = static_cast<int>( m->get_size() ) + 1;

    drive->set_var( "specimen_sample", id_str );
    drive->set_var( "specimen_sample_progress", "1" );  // First increment
    drive->set_var( "specimen_name", m->name() );
    drive->set_var( "specimen_size", std::to_string( size ) );

    if( size > 1 ) {
        add_msg( m_info, "Progress: 1/%d for genome sample.", size );
    } else {
        add_msg( m_good, "Sample is complete." );
    }
    p.i_add( std::move( drive ) );

    return charges_to_use;
}

std::unique_ptr<iuse_actor> cloning_syringe_iuse::clone() const
{
    return std::make_unique<cloning_syringe_iuse>( *this );
}

void dna_editor_iuse::load( const JsonObject &obj )
{
    assign( obj, "moves", moves );
    assign( obj, "charges_to_use", charges_to_use );
}

int dna_editor_iuse::use( player &p, item &it, bool, const tripoint & ) const
{
    const auto is_empty_usb = []( const item & drive ) {
        return drive.contents.empty();
    };

    if( !it.units_sufficient( p, charges_to_use ) ) {
        add_msg( m_info, _( "There's not enough charge left in the %s." ), it.display_name() );
        return 0;
    }
    auto genome_drives = p.all_items_with_id( itype_genome_drive );
    if( genome_drives.size() == 0 ) {
        popup( "You have no valid genome drives." );
        return 0;
    }

    uilist specimen_menu;
    specimen_menu.text = _( "Select specimen sample:" );
    bool has_complete_sample = false;
    for( size_t z = 0; z < genome_drives.size(); z++ ) {
        const int progress = genome_drives[z]->get_var( "specimen_sample_progress", 0 );
        const int size = genome_drives[z]->get_var( "specimen_size", 0 );
        if( progress >= size ) {
            has_complete_sample = true;
            specimen_menu.addentry( z, true, MENU_AUTOASSIGN, string_format( "%s",
                                    genome_drives[z]->display_name() ) );
        }
    }
    if( !has_complete_sample ) {
        popup( "You have no valid genome drives." );
        return 0;
    }
    specimen_menu.query();
    const int choice = specimen_menu.ret;
    if( choice < 0 ) {
        return 0;
    }

    auto &selected_drive = genome_drives[choice];  // reference to the original detached_ptr

    uilist menu;
    menu.text = string_format( _( "What to do with the %s?" ), selected_drive->display_name() );
    menu.addentry( 0, true, 'e', "Examine sample" );
    menu.addentry( 1, p.has_charges( itype_mutagen, 1 ) &&
                   p.has_charges( itype_biomaterial, 1 ), 'i', "Research upgrade" );
    menu.addentry( 2, p.has_amount( itype_usb_drive, 1, true, is_empty_usb ), 'c', "Clone drive" );
    menu.addentry( 3, p.has_charges( itype_biomaterial, 1 ), 'p', "Produce DNA" );
    menu.query();
    if( menu.ret < 0 ) {
        return 0;
    }

    if( menu.ret == 0 ) {
        // grab the monsters data from a fake copy
        const shared_ptr_fast<monster> newmon_ptr = make_shared_fast<monster>
                ( mtype_id( selected_drive->get_var( "specimen_sample" ) ) );
        const monster &newmon = *newmon_ptr;

        const int size_class = selected_drive->get_var( "specimen_size", 0 );

        static const char *creature_size_strings[] = {
            "TINY",
            "SMALL",
            "MEDIUM",
            "LARGE",
            "HUGE"
        };

        const char *size_str = "UNKNOWN";

        if( size_class >= 0 && size_class < std::ssize( creature_size_strings ) ) {
            size_str = creature_size_strings[size_class];
        }

        popup(
            _( "Examination Results:\n\nSample Name: %s\nSize Class: %s\nWeight: %.0fkg\nVolume: %.0fl" ),
            selected_drive->get_var( "specimen_name" ),
            size_str,
            static_cast<double>( to_kilogram( newmon.get_weight() ) ),
            static_cast<double>( to_liter( newmon.get_volume() ) )
        );

        return 0;
    } else if( menu.ret == 1 ) {
        const mtype_id id( selected_drive->get_var( "specimen_sample" ) );
        const mtype &type = id.obj();

        mongroup_id upgrade_group = mongroup_id::NULL_ID();
        upgrade_group = type.upgrade_group;
        const auto mons = upgrade_group.obj().monsters;

        if( mons.empty() ) {
            popup( "A message pops up on the genome editor indicating there are no further mutations possible for this sample." );
            return 0;
        }

        if( !query_yn( _( "This will use up 1 unit of mutagen and 1 unit of biomaterial.  Are you sure?" ),
                       selected_drive->display_name() ) ) {
            return 0;
        }

        int total_freq = 0;
        for( const MonsterGroupEntry &entry : mons ) {
            total_freq += entry.frequency;
        }
        int roll = rng( 1, total_freq );
        const MonsterGroupEntry *chosen = nullptr;
        for( const MonsterGroupEntry &entry : mons ) {
            roll -= entry.frequency;
            if( roll <= 0 ) {
                chosen = &entry;
                break;
            }
        }
        if( !chosen ) {
            return 0;
        }

        const shared_ptr_fast<monster> newmon_ptr = make_shared_fast<monster>
                ( mtype_id( chosen->name.str() ) );
        const monster &newmon = *newmon_ptr;

        p.use_charges( itype_mutagen, 1 );
        p.use_charges( itype_biomaterial, 1 );

        // chance of failure when converting DNA
        if( rng( 1, 100 ) < 80 ) {
            add_msg( m_bad, "The research produced no result." );
            return charges_to_use;
        }

        add_msg( m_info, _( "The research produced a viable %s sample!" ), newmon.name() );
        selected_drive->set_var( "specimen_sample", chosen->name.str() );
        selected_drive->set_var( "specimen_size", std::to_string( newmon.get_size() ) );
        selected_drive->set_var( "specimen_sample_progress", selected_drive->get_var( "specimen_size" ) );
        selected_drive->set_var( "specimen_name", newmon.name() );
    } else if( menu.ret == 2 ) {
        add_msg( "You clone a copy of the drive onto another USB." );
        p.use_amount( itype_usb_drive, 1, is_empty_usb );
        detached_ptr<item> drive_copy = item::spawn( itype_genome_drive, calendar::turn );

        drive_copy->set_var( "specimen_sample", selected_drive->get_var( "specimen_sample" ) );
        drive_copy->set_var( "specimen_sample_progress",
                             selected_drive->get_var( "specimen_sample_progress" ) );
        drive_copy->set_var( "specimen_size", selected_drive->get_var( "specimen_size" ) );
        drive_copy->set_var( "specimen_name", selected_drive->get_var( "specimen_name" ) );

        p.i_add( std::move( drive_copy ) );
    } else if( menu.ret == 3 ) {
        p.use_charges( itype_biomaterial, 1 );
        const std::string msg = string_format( _( "You produce a unit of %s DNA." ),
                                               selected_drive->get_var( "specimen_name" ) );
        add_msg( msg );

        detached_ptr<item> dna = item::spawn( itype_id( "dna" ), calendar::turn, 1 );

        dna->set_var( "specimen_sample", selected_drive->get_var( "specimen_sample" ) );
        dna->set_var( "specimen_size", selected_drive->get_var( "specimen_size" ) );
        dna->set_var( "specimen_name", selected_drive->get_var( "specimen_name" ) );

        liquid_handler::handle_all_liquid( std::move( dna ), PICKUP_RANGE );
    }

    return charges_to_use;
}

std::unique_ptr<iuse_actor> dna_editor_iuse::clone() const
{
    return std::make_unique<dna_editor_iuse>( *this );
}

void multicooker_iuse::load( const JsonObject &obj )
{
    assign( obj, "do_hallu", do_hallu );
    assign( obj, "charges_to_start", charges_to_start );
    assign( obj, "charges_per_minute", charges_per_minute );
    assign( obj, "time_mult", time_mult );
    for( const std::string line : obj.get_array( "recipes" ) ) {
        recipes.emplace( line );
    }
    for( const std::string line : obj.get_array( "subcategories" ) ) {
        subcategories.emplace( line );
    }
    for( const std::string line : obj.get_array( "temporary_tools" ) ) {
        temporary_tools.emplace( line );
    }
}

static bool multicooker_hallu( player &p )
{
    p.moves -= to_moves<int>( 2_seconds );
    const int random_hallu = rng( 1, 7 );
    switch( random_hallu ) {

        case 1:
            add_msg( m_info, _( "And when you gaze long into a screen, the screen also gazes into you." ) );
            return true;

        case 2:
            add_msg( m_bad, _( "The multi-cooker boiled your head!" ) );
            return true;

        case 3:
            add_msg( m_info, _( "The characters on the screen display an obscene joke.  Strange humor." ) );
            return true;

        case 4:
            //~ Single-spaced & lowercase are intentional, conveying hurried speech-KA101
            add_msg( m_warning, _( "Are you sure?!  the multi-cooker wants to poison your food!" ) );
            return true;

        case 5:
            add_msg( m_info,
                     _( "The multi-cooker argues with you about the taste preferences.  You don't want to deal with it." ) );
            return true;

        case 6:
            if( !one_in( 5 ) ) {
                add_msg( m_warning, _( "The multi-cooker runs away!" ) );
                if( monster *const m = g->place_critter_around( mon_hallu_multicooker, p.pos(), 1 ) ) {
                    m->hallucination = true;
                    m->add_effect( effect_run, 100_turns );
                }
            } else {
                p.add_msg_if_player( m_info, _( "You're surrounded by aggressive multi-cookers!" ) );

                for( const tripoint &pn : g->m.points_in_radius( p.pos(), 1 ) ) {
                    if( monster *const m = g->place_critter_at( mon_hallu_multicooker, pn ) ) {
                        m->hallucination = true;
                    }
                }
            }
            return true;

        default:
            return false;
    }

}

int multicooker_iuse::use( player &p, item &it, bool t, const tripoint &pos ) const
{
    if( t ) {
        if( !it.units_sufficient( p, charges_per_minute ) ) {
            it.deactivate();
            return 0;
        }

        int cooktime = it.get_var( "COOKTIME", 0 );
        cooktime -= 100;

        if( cooktime <= 0 ) {
            it.deactivate();
            it.erase_var( "COOKTIME" );
            it.put_in( item::spawn( it.get_var( "RESULT" ), calendar::turn, it.get_var( "BATCHCOUNT", 1 ) ) );
            it.erase_var( "BATCHCOUNT" );
            it.erase_var( "RESULT" );

            sounds::sound( pos, 8, sounds::sound_t::alarm, _( "ding!" ), true, "misc", "ding" );

            return 0;
        } else {
            if( calendar::once_every( 1_minutes ) ) {
                it.ammo_consume( charges_per_minute, pos );
            }
            it.set_var( "COOKTIME", cooktime );
            return 0;
        }
    } else {
        enum {
            mc_start, mc_stop, mc_take, mc_upgrade
        };

        if( p.is_underwater() ) {
            p.add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
            return 0;
        }

        if( do_hallu && ( p.has_effect( effect_hallu ) || p.has_effect( effect_visuals ) ) ) {
            if( multicooker_hallu( p ) ) {
                return 0;
            }
        }

        uilist menu;
        menu.text = _( "Choose option:" );

        item *dish_it = it.contents.get_item_with(
        []( const item & it ) {
            return !( it.is_toolmod() || it.is_magazine() );
        } );

        if( it.is_active() ) {
            menu.addentry( mc_stop, true, 's', _( "Stop crafting" ) );
        } else {

            if( dish_it == nullptr ) {
                if( it.ammo_remaining() < charges_to_start ) {
                    p.add_msg_if_player( _( "Batteries are low." ) );
                    return 0;
                }
                menu.addentry( mc_start, true, 's', _( "Start crafting " ) );
            } else {
                menu.addentry( mc_take, true, 't', _( "Remove Product" ) );
            }
        }

        menu.query();
        int choice = menu.ret;

        if( choice < 0 ) {
            return 0;
        }

        if( mc_stop == choice ) {
            if( query_yn( _( "Really stop?" ) ) ) {
                it.deactivate();
                it.erase_var( "RESULT" );
                it.erase_var( "COOKTIME" );
                it.erase_var( "BATCHCOUNT" );
                it.erase_var( "RECIPE" );
            }
            return 0;
        }

        if( mc_take == choice ) {

            detached_ptr<item> dish = it.remove_item( *dish_it );
            const std::string dish_name = dish->tname( dish->charges, false );
            if( dish->made_of( LIQUID ) ) {
                if( !p.check_eligible_containers_for_crafting( *recipe_id( it.get_var( "RECIPE" ) ), 1 ) ) {
                    p.add_msg_if_player( m_info, _( "You don't have a suitable container to store your %s." ),
                                         dish_name );

                    return 0;
                }
                liquid_handler::handle_all_liquid( std::move( dish ), PICKUP_RANGE );
            } else {
                p.i_add( std::move( dish ) );
            }

            it.erase_var( "RECIPE" );
            p.add_msg_if_player( m_good, _( "You got the %s from the %s." ),
                                 dish_name, it.tname() );

            return 0;
        }

        if( mc_start == choice ) {
            uilist dmenu;
            dmenu.text = _( "Choose desired recipe:" );

            std::vector<const recipe *> dishes;

            inventory crafting_inv = g->u.crafting_inventory();

            const time_point bday = calendar::start_of_cataclysm;
            for( const std::string &item : temporary_tools ) {
                crafting_inv.add_item( *item::spawn_temporary( item, bday ), false );
            }
            crafting_inv.update_quality_cache();

            int counter = 0;

            for( const auto &r : g->u.get_learned_recipes() ) {
                if( subcategories.contains( r->subcategory ) || recipes.contains( r->result() ) ) {
                    dishes.push_back( r );
                    const bool can_make = r->deduped_requirements().can_make_with_inventory(
                                              crafting_inv, r->get_component_filter() );
                    dmenu.addentry( counter++, can_make, -1, string_format( _( "%s (%1.f charges)" ), r->result_name(),
                                    r->time * time_mult / 6000 * charges_per_minute + charges_to_start ) );
                }
            }

            dmenu.query();

            int choice = dmenu.ret;

            if( choice < 0 ) {

                if( choice == -1024 ) {
                    p.add_msg_if_player( m_warning,
                                         _( "You don't know of anything you could craft with this." ) );
                }

                return 0;
            } else {
                const recipe *meal = dishes[choice];

                uilist batchmenu;
                batchmenu.text = _( "Choose batch count:" );
                int counter = 0;

                for( int i = 1; i < 51; i++ ) {
                    const bool can_make = meal->deduped_requirements().can_make_with_inventory(
                                              crafting_inv, meal->get_component_filter(), i );
                    batchmenu.addentry( counter++, can_make, -1, string_format( _( "%s batches (%1.f charges)" ), i,
                                        meal->batch_time( i, 1, 0 ) * time_mult / 6000 * charges_per_minute + charges_to_start ) );
                }

                batchmenu.query();

                int batchcount = batchmenu.ret;

                if( batchcount < 0 ) {
                    return 0;
                }
                batchcount++;

                int mealtime = meal->batch_time( batchcount, 1, 0 ) * time_mult;
                int all_charges = mealtime / 6000 * charges_per_minute + charges_to_start;

                if( it.ammo_remaining() < all_charges ) {

                    p.add_msg_if_player( m_warning,
                                         _( "The %s needs %d charges to create this." ),
                                         it.tname(), all_charges );

                    return 0;
                }

                const auto filter = is_crafting_component;
                const requirement_data *reqs =
                    meal->deduped_requirements().select_alternative( p, crafting_inv, filter, batchcount );
                if( !reqs ) {
                    return 0;
                }

                for( const auto &component : reqs->get_components() ) {
                    p.consume_items( component, batchcount, filter );
                }

                it.set_var( "RECIPE", meal->ident().str() );
                it.set_var( "RESULT", meal->result().str() );
                it.set_var( "COOKTIME", mealtime );
                it.set_var( "BATCHCOUNT", meal->makes_amount() * batchcount );

                p.add_msg_if_player( m_good,
                                     _( "The %s begins to hum." ), it.tname() );
                it.activate();

                return charges_to_start;
            }
        }
    }

    return 0;
}

std::unique_ptr<iuse_actor> multicooker_iuse::clone() const
{
    return std::make_unique<multicooker_iuse>( *this );
}

void sex_toy_actor::load( JsonObject const &obj )
{
    moves = obj.get_int( "moves", 60000 ); // default is 10 minutes
}

ret_val<bool> sex_toy_actor::can_use( const Character &c, const item &i, bool,
                                      const tripoint & ) const
{
    if( c.is_npc() ) {
        return ret_val<bool>::make_failure(); // Creepy, status quo
    }
    if( c.is_mounted() ) {
        return ret_val<bool>::make_failure( _( "You can't do *that* while mounted" ) );
    }
    if( ( c.is_underwater() ) && ( !( ( c.has_trait( trait_id( "GILLS" ) ) ) ||
                                      ( c.has_trait( trait_id( "GILLS_CEPH" ) ) ) ||
                                      ( c.is_wearing( itype_id( "rebreather_on" ) ) ) ||
                                      ( c.is_wearing( itype_id( "rebreather_xl_on" ) ) ) ||
                                      ( c.is_wearing( itype_id( "mask_h20survivor_on" ) ) ) ) ) ) {
        return ret_val<bool>::make_failure( _( "Are you trying to drown yourself?" ) );
    }
    if( !i.units_sufficient( c ) ) {
        return ret_val<bool>::make_failure( _( "The %s's batteries are dead." ), i.tname() );
    }
    if( c.get_fatigue() >= fatigue_levels::dead_tired ) {
        return ret_val<bool>::make_failure( _( "*Your* batteries are dead." ) );
    }
    return ret_val<bool>::make_success();
}

std::unique_ptr<iuse_actor> sex_toy_actor::clone() const
{
    return std::make_unique<sex_toy_actor>( *this );
}

void train_skill_actor::load( JsonObject const &obj )
{
    training_skill = obj.get_string( "training_skill" );
    training_skill_min_level = obj.get_int( "training_skill_min_level", 0 );
    training_skill_xp = obj.get_int( "training_skill_xp", 0 );
    training_skill_xp_chance = obj.get_int( "training_skill_xp_chance", 0 );
    training_skill_max_level = obj.get_int( "training_skill_max_level", 0 );
    training_skill_fatigue = obj.get_int( "training_skill_fatigue", 0 );
    training_skill_interval = obj.get_int( "training_skill_interval", 0 );
    training_msg = obj.get_string( "training_msg" );
}

int train_skill_actor::use( player &p, item &i, bool, const tripoint & ) const
{
    if( i.ammo_remaining() < i.ammo_required() ) {
        p.add_msg_if_player( _( "This tool doesn't have enough charges." ) );
        return 0;
    }
    if( p.get_skill_level( skill_id( training_skill ) ) < training_skill_min_level ) {
        p.add_msg_if_player( _( "Your skill isn't high enough yet to train using that (requires %s %s)." ),
                             training_skill_min_level, skill_id( training_skill )->name() );
        return 0;
    }
    if( p.get_skill_level( skill_id( training_skill ) ) >= training_skill_max_level ) {
        p.add_msg_if_player( _( "You can't train your %s beyond %s using that." ),
                             skill_id( training_skill )->name(), training_skill_max_level );
        return 0;
    }

    int hours = string_input_popup()
                .title( string_format( _( "Train %s for how many hours?" ),
                                       skill_id( training_skill )->name() ) )
                .width( 3 )
                .text( "" )
                .only_digits( true )
                .query_int();

    if( hours <= 0 ) {
        return 0;
    }

    p.add_msg_if_player( training_msg );
    // using metadata is the easiest way to transfer this over to the activity handler and also allow it to function as furniture
    p.set_value( "training_iuse_skill", training_skill );
    p.set_value( "training_iuse_skill_xp", std::to_string( training_skill_xp ) );
    p.set_value( "training_iuse_skill_xp_max_level", std::to_string( training_skill_max_level ) );
    p.set_value( "training_iuse_skill_fatigue", std::to_string( training_skill_fatigue ) );
    p.set_value( "training_iuse_skill_interval", std::to_string( training_skill_interval ) );
    p.set_value( "training_iuse_skill_xp_chance", std::to_string( training_skill_xp_chance ) );
    p.assign_activity( ACT_TRAIN_SKILL, hours * 360000, -1, 0, "training" );
    p.activity->str_values.emplace_back( i.typeId() );
    p.activity->tools.emplace_back( i );

    return 0;
}

std::unique_ptr<iuse_actor> train_skill_actor::clone() const
{
    return std::make_unique<train_skill_actor>( *this );
}

int sex_toy_actor::use( player &p, item &i, bool, const tripoint & ) const
{
    if( i.ammo_remaining() > 0 ) {
        p.add_msg_if_player( _( "You fire up your %s and start getting the tension out." ),
                             i.tname() );
    } else {
        p.add_msg_if_player( _( "You whip out your %s and start getting the tension out." ),
                             i.tname() );
    }
    p.assign_activity( ACT_VIBE, moves, -1, 0, "de-stressing" );
    p.activity->tools.emplace_back( i );

    return i.type->charges_to_use();
}

std::unique_ptr<iuse_actor> iuse_music_player::clone() const
{
    return std::make_unique<iuse_music_player>( *this );
}

void iuse_music_player::load( const JsonObject &obj )
{
    obj.read( "target", target, true );

    obj.read( "msg", msg_transform );

    obj.read( "moves", moves );
    if( moves < 0 ) {
        obj.throw_error( "transform actor specified negative moves", "moves" );
    }

    obj.read( "need_charges", need_charges );
    need_charges = std::max( need_charges, 0 );
    obj.read( "transform_charges", transform_charges );

    obj.read( "need_worn", need_worn );
    obj.read( "need_wielding", need_wielding );
}

int iuse_music_player::use( player &p, item &it, bool t, const tripoint &pos ) const
{
    if( t ) {
        return 0; // invoked from active item processing, do nothing.
    }

    const bool possess = p.has_item( it ) ||
                         ( it.has_flag( flag_ALLOWS_REMOTE_USE ) && square_dist( p.pos(), pos ) == 1 );

    if( possess && need_worn && !p.is_worn( it ) ) {
        p.add_msg_if_player( m_info, _( "You need to wear the %1$s before activating it." ), it.tname() );
        return 0;
    }
    if( possess && need_wielding && !p.is_wielding( it ) ) {
        p.add_msg_if_player( m_info, _( "You need to wield the %1$s before activating it." ), it.tname() );
        return 0;
    }
    // No charge consumption at this point, there are still points of failure later.
    if( need_charges || transform_charges ) {
        if( it.has_flag( flag_POWERARMOR_MOD ) && character_funcs::can_interface_armor( p ) ) {
            if( possess ) {
                const int bio_power = units::to_kilojoule( p.get_power_level() );
                if( bio_power < need_charges || bio_power < transform_charges ) {
                    p.add_msg_if_player( m_info, "Your %s doesn't have enough battery to do that", it.tname() );
                    return 0;
                }
            } else {
                return 0;
            }
        } else {
            const int item_charges = it.units_remaining( p );
            if( item_charges < need_charges || item_charges < transform_charges ) {
                p.add_msg_if_player( m_info, "Your %s doesn't have enough battery to do that", it.tname() );
                return 0;
            }
        }
    }

    // All checks complete the damn thing can finally transform
    // Consume charges if necessary at this point.
    if( transform_charges ) {
        p.consume_charges( it, transform_charges );
    }

    if( possess && !msg_transform.empty() ) {
        p.add_msg_if_player( m_neutral, msg_transform, it.tname() );
    }
    // We want this separate and not if/else because the preceding statement will always return true if a transform message is defined.
    if( p.is_npc() && get_player_character().sees( p ) ) {
        if( !it.has_flag( flag_COMBAT_NPC_ON ) ) {
            add_msg( m_info, _( "%s activates their %s." ), p.disp_name(),
                     it.display_name() );
        } else {
            add_msg( m_info, _( "%s deactivates their %s." ), p.disp_name(),
                     it.display_name() );
        }
    }

    if( possess ) {
        p.moves -= moves;
    }

    // Update Luminosity as object is "removed"
    get_map().update_lum( it, false );

    if( p.is_worn( it ) ) {
        p.on_item_takeoff( it );
    }
    it.convert( target );
    if( p.is_worn( it ) ) {
        p.reset_encumbrance();
        // This is most likely wrong: it doubles temperature shift for the turn!
        p.update_bodytemp( get_map(), get_weather() );
        p.on_item_wear( it );
    }
    p.inv_update_invlet_cache_with_item( it );
    // Update luminosity as object is "added"
    get_map().update_lum( it, true );
    it.activate();

    return 0;
}

ret_val<bool> iuse_music_player::can_use( const Character &p, const item &, bool,
        const tripoint & ) const
{
    if( p.has_effect( efftype_id( "music" ) ) ) {
        return ret_val<bool>::make_failure( _( "You can't listen to multiple music players at once!" ) );
    } else {
        return ret_val<bool>::make_success();
    }
}


ret_val<bool> iuse_prospect_pick::can_use( const Character &p, const item &, bool,
        const tripoint & ) const
{
    if( p.is_npc() ) {
        // Long action
        return ret_val<bool>::make_failure(
                   _( "Actually how. You're an NPC. You can't do long actions. No." ) );
    }
    if( p.is_mounted() ) {
        return ret_val<bool>::make_failure( _( "You can't prospect from a vehicle!" ) );
    }
    if( p.is_underwater() ) {
        return ret_val<bool>::make_failure( _( "You can't prospect underwater!" ) );
    }
    return ret_val<bool>::make_success();
}

void iuse_prospect_pick::load( const JsonObject &obj )
{
    range = obj.get_int( "radius", 3 );
}
//TODO: this should probably take some time to do when skill is implimented, for now though, it just does.
int iuse_prospect_pick::use( player &p, item &it, bool t,
                             const tripoint & ) const
{
    if( t ) {
        //we're doing it still hold on.
        return 0;
    }
    //* begin edited map code*/
    omt_find_params params{};
    params.search_range = {0, range};
    params.search_layers =
        omt_find_all_layers; // TODO: Find all levels -> find BELOW levels.

    params.types = { std::make_pair( "empty_rock", ot_match_type::type ) } ;
    params.existing_only = false;
    params.popup = make_shared_fast<throbber_popup>( _( "Please wait…" ) );
    params.seen = false;

    const point_abs_om origin_om_pos = project_to<coords::om>( p.global_omt_location().xy() );

    // Generate a Square fitting the requested map radius
    const point_abs_omt omt_bb_min = p.global_omt_location().xy() - point_rel_omt{ range, range };
    const point_abs_omt omt_bb_max = p.global_omt_location().xy() + point_rel_omt{ range, range };

    // OM Corners of bounding box
    const point_abs_om om_bb_min = project_to<coords::om>( omt_bb_min );
    const point_abs_om om_bb_max = project_to<coords::om>( omt_bb_max );

    // Iterate through range [om_bb_min, om_bb_max] to get the OM we want, then sort by manhattan distance
    std::map<int, std::vector<point_abs_om>> om_to_generate;
    for( int x = om_bb_min.x(); x <= om_bb_max.x(); ++x ) {
        for( int y = om_bb_min.y(); y <= om_bb_max.y(); ++y ) {
            auto dist = manhattan_dist( origin_om_pos, { x, y } );
            auto &vec =
                om_to_generate[dist]; // if the vector for this distance doesn't exist it will be created empty
            vec.emplace_back( x, y );
        }
    }

    for( const auto& [_, to_gen] : om_to_generate ) {
        overmap_buffer.generate( to_gen );
    }

    const auto places = overmap_buffer.find_all( p.global_omt_location(), params );
    for( auto &place : places ) {
        overmap_buffer.reveal( place, 0 );
    }
    //* end edited map code */
    p.add_msg_if_player( m_info,
                         _( "You use the %s to gather a few samples and gauge where minerals may lie nearby." ),
                         it.tname() );
    return 0;
}
std::unique_ptr<iuse_actor> iuse_prospect_pick::clone() const
{
    return std::make_unique<iuse_prospect_pick>( *this );
}

void iuse_reveal_contents::load( const JsonObject &obj )
{
    obj.read( "group", contents_group );
    if( obj.has_member( "open_message" ) ) {
        obj.read( "open_message", open_message );
    }
}
int iuse_reveal_contents::use( player &p, item &it, bool,
                               const tripoint & ) const
{
    std::vector<detached_ptr<item>> items = item_group::items_from( contents_group,
                                            calendar::turn );
    map &here = get_map();
    for( detached_ptr<item> &content : items ) {
        if( !open_message.empty() ) {
            p.add_msg_if_player( ( string_format( open_message,
                                                  it.tname() ) + content->tname() + "!" ) );
        }
        here.add_item_or_charges( p.pos(), std::move( content ) );
    }

    it.detach( );

    return 0;
}
std::unique_ptr<iuse_actor> iuse_reveal_contents::clone() const
{
    return std::make_unique<iuse_reveal_contents>( *this );
}

// -------------------

void iuse_flowerpot_plant::load( const JsonObject &jo )
{
    jo.read( "stages", stages );
    growth_rate = jo.get_float( "growth_rate", 1.0 );
    fert_boost = jo.get_float( "fert_boost", 1.5 );
    harvest_mult = jo.get_float( "harvest_mult", 1 );

    if( jo.has_array( "seeds_per_use" ) ) {
        auto arr = jo.get_int_array( "seeds_per_use" );
        seeds_per_use = std::make_pair( arr[0], arr[1] );
    } else if( jo.has_int( "seeds_per_use" ) ) {
        auto val = jo.get_int( "seeds_per_use" );
        seeds_per_use = std::make_pair( val, val );
    } else {
        seeds_per_use = std::make_pair( 1, 1 );
    }

    if( jo.has_array( "fert_per_use" ) ) {
        auto arr = jo.get_int_array( "fert_per_use" );
        fert_per_use = std::make_pair( arr[0], arr[1] );
    } else if( jo.has_int( "fert_per_use" ) ) {
        auto val = jo.get_int( "fert_per_use" );
        fert_per_use = std::make_pair( val, val );
    } else {
        fert_per_use = std::make_pair( 0, 1 );
    }

    if( jo.has_array( "terrain" ) ) {
        terrain = jo.get_tags<std::string>( "terrain" );
    }
}

auto iuse_flowerpot_plant::clone() const -> std::unique_ptr<iuse_actor>
{
    return std::make_unique<iuse_flowerpot_plant>( *this );
}

auto iuse_flowerpot_plant::growth_info::elapsed_time() const -> time_duration
{
    return calendar::turn - planted_time;
}

auto iuse_flowerpot_plant::growth_info::remaining_time() const -> time_duration
{
    return epoch - elapsed_time();
}

auto iuse_flowerpot_plant::growth_info::stage() const -> growth_stage
{
    if( epoch <= time_duration{} )
        return empty;

    const auto stage = to_turns<int>( elapsed_time() ) * 3 / to_turns<int>( epoch );
    switch( std::clamp( stage, 0, 3 ) ) {
        case 0:
            return seed;
        case 1:
            return seedling;
        case 2:
            return mature;
        case 3:
            return harvest;
        default:
            return empty;
    }
}

auto iuse_flowerpot_plant::growth_info::plant_name() const -> std::string
{
    return seed_id.obj().seed->plant_name.translated();
}

auto iuse_flowerpot_plant::growth_info::progress() const -> double
{
    return elapsed_time() / epoch;
}

auto iuse_flowerpot_plant::use( player &who, item &i, bool tick, const tripoint &pos ) const -> int
{
    if( tick ) {
        return on_tick( who, i, pos );
    }

    const auto info = get_info( i );
    switch( info.stage() ) {
        case seed:
        case seedling:
        case mature:
            return on_use_add_fertilizer( who, i, pos );
        case harvest:
            return on_use_harvest( who, i, pos );
        default:
            return on_use_plant( who, i, pos );
    }
}

auto iuse_flowerpot_plant::can_use( const Character &who, const item &i, bool,
                                    const tripoint & ) const -> ret_val<bool>
{

    const auto info = get_info( i );
    switch( info.stage() ) {
        case seed:
        case seedling:
        case mature: {
            const bool can_add_fert = info.fert_amt < fert_per_use.second;
            const bool has_fert = i.charges > 0;
            if( !can_add_fert ) {
                return ret_val<bool>::make_failure( _( "You need to wait for it to grow." ) );
            }
            if( !has_fert ) {
                return ret_val<bool>::make_failure( _( "You don't have enough fertilizer." ) );
            }
            return ret_val<bool>::make_success();
        }
        case harvest:
            return ret_val<bool>::make_success();
        default: {
            if( !who.has_item_with( []( const item & itm ) { return itm.is_seed(); } ) ) {
                return ret_val<bool>::make_failure( _( "You have no seeds to plant." ) );
            }
            if( i.charges < fert_per_use.first ) {
                return ret_val<bool>::make_failure( _( "You don't have enough fertilizer." ) );
            }
            return ret_val<bool>::make_success();
        }
    }
}

void iuse_flowerpot_plant::info( const item &i, std::vector<iteminfo> &inf ) const
{
    const auto info = get_info( i );
    if( !info.seed_id.is_valid() ) {
        return;
    }

    const auto plant_name = info.plant_name();

    inf.emplace_back( "TOOL", string_format( _( "<bold>Growing</bold>: %s" ), plant_name ) );
    switch( info.stage() ) {
        case seed:
            inf.emplace_back( "TOOL", string_format( _( "<bold>Stage</bold>: %s" ), _( "seed" ) ) );
            break;
        case seedling:
            inf.emplace_back( "TOOL", string_format( _( "<bold>Stage</bold>: %s" ), _( "seedling" ) ) );
            break;
        case mature:
            inf.emplace_back( "TOOL", string_format( _( "<bold>Stage</bold>: %s" ), _( "mature" ) ) );
            break;
        case harvest:
            inf.emplace_back( "TOOL", string_format( _( "<bold>Stage</bold>: %s" ), _( "harvest" ) ) );
            break;
        default:
            break;
    }
    if( i.is_active() ) {
        inf.emplace_back( "TOOL", string_format( _( "<bold>Progress</bold>: %d%%" ),
                          static_cast<int>( 100 * info.progress() ) ) );
        inf.emplace_back( "TOOL", string_format( _( "<bold>Harvestable in</bold>: %s" ),
                          to_string_approx( info.remaining_time() ) ) );
    }
    if( info.seed_id.is_valid() ) {
        inf.emplace_back( "TOOL", string_format( _( "<bold>Seeds</bold>: %d/%d" ),
                          info.seed_amt, seeds_per_use.second ) );
        inf.emplace_back( "TOOL", string_format( _( "<bold>Fertilizer</bold>: %d/%d" ),
                          info.fert_amt, fert_per_use.second ) );
        inf.emplace_back( "TOOL", string_format( _( "<bold>Growth</bold>: %d%%" ),
                          static_cast<int>( ( growth_rate + ( info.fert_amt * fert_boost ) ) * 100 ) ) );
        inf.emplace_back( "TOOL", string_format( _( "<bold>Yield</bold>: %d%%" ),
                          static_cast<int>( harvest_mult * 100 ) ) );
    }
}

auto iuse_flowerpot_plant::on_use_add_fertilizer( player &, item &i, const tripoint & ) const -> int
{

    const auto info = get_info( i );
    const int fert_to_add = std::min( i.charges,  fert_per_use.second - info.fert_amt );
    const auto new_fert_amt = info.fert_amt + fert_to_add;
    const auto old_prog = info.progress();
    const auto new_epoch = calculate_growth_time( info.seed_id, new_fert_amt );
    const auto new_age = new_epoch * old_prog;
    const auto new_date = calendar::turn - new_age;

    set_growing_plant( i, info.seed_id, new_date, info.seed_amt, new_fert_amt );

    return fert_to_add;
}


auto iuse_flowerpot_plant::on_use_plant( player &p, item &i, const tripoint & ) const -> int
{
    const std::vector<item *> seed_inv =
    p.items_with( []( const item & itm ) { return itm.is_seed(); } );

    const auto &[min_seed, max_seed] = seeds_per_use;
    const auto &[min_fert, max_fert] = fert_per_use;

    auto seed_entries = std::vector<seed_tuple> {};
    std::ranges::copy_if( iexamine::get_seed_entries( seed_inv ),
    std::back_inserter( seed_entries ), [&]( const seed_tuple & s ) {
        const auto &[type, name, cnt] = s;
        return terrain.contains( type->seed->required_terrain_flag );
    } );

    if( seed_entries.empty() ) {
        add_msg( _( "You don't have seeds to plant in this." ) );
        return 0;
    }

    const int seed_index = iexamine::query_seed( seed_entries, min_seed );

    if( seed_index < 0 || std::cmp_greater_equal( seed_index, seed_entries.size() ) ) {
        add_msg( _( "You saved your seeds for later." ) );
        return 0;
    }
    const auto &[seed_id, seed_name, seed_amt] = seed_entries[seed_index];

    const int used_fert = std::min( i.charges, max_fert );

    auto comps = p.use_charges( seed_id, max_seed );
    constexpr auto count_fn = []( const detached_ptr<item> &it ) { return it->count_by_charges() ? it->charges : 1;};
    const auto used_seeds = std::ranges::fold_left( comps | std::views::transform( count_fn ), 0,
                            std::plus<int> {} );
    set_growing_plant( i, seed_id, calendar::turn, used_seeds, used_fert );
    update( i );

    return used_fert;
}

auto iuse_flowerpot_plant::on_use_harvest( player &p, item &i, const tripoint & ) const -> int
{
    const auto info = get_info( i );
    clear_growing_plant( i );
    update( i );

    const int skillLevel = p.get_skill_level( skill_survival );
    const int max_harvest_count = get_option<int>( "MAX_HARVEST_COUNT" );

    // since a modded item could consume 10 seeds to produce 10 times the fruit
    // we roll n times the harvest
    std::vector<detached_ptr<item>> harvest;
    int practice = 0;

    for( int j = 0; j < info.seed_amt; j++ ) {
        int fruit_count = rng_float( skillLevel / 2.0, skillLevel ) * info.harvest_mult;
        fruit_count = std::clamp( fruit_count, 1, max_harvest_count );
        const int seed_count = std::max( 1, rng( fruit_count / 4, fruit_count / 2 ) );
        practice += fruit_count;

        auto tmp = iexamine::get_harvest_items( info.seed_id.obj(), fruit_count, seed_count, true );
        std::ranges::move( tmp, std::back_inserter( harvest ) );
    }

    for( auto &j : harvest ) {
        put_into_vehicle_or_drop( p, item_drop_reason::deliberate, std::move( j ), p.pos() );
    }

    p.moves -= to_moves<int>( 10_seconds * info.harvest_mult );
    p.practice( skill_survival, rng( 1,  practice ) );
    return 0;
}

auto iuse_flowerpot_plant::on_tick( player &, item &i, const tripoint & ) const -> int
{
    if( i.get_counter() != 0 ) {
        return 0;
    }

    update( i );
    return 0;
}

void iuse_flowerpot_plant::update( item &i ) const
{
    const auto info = get_info( i );
    if( !info.seed_id.is_valid() ) {
        clear_growing_plant( i );
        i.set_counter( 0 );
        i.convert( stages[0] );
        i.erase_var( "item_label" );
        i.deactivate();
        return;
    }

    i.convert( stages[info.stage()] );
    i.set_var( "item_label", string_format( "%s (%s)", stages[0]->nname( 1 ), info.plant_name() ) );
    switch( info.stage() ) {
        case 0:
            i.deactivate();
            i.set_counter( 0 );
            break;
        case 4:
            i.deactivate();
            i.set_counter( 0 );
            break;
        default:
            i.activate();
            i.set_counter( to_turns<int>( std::min( info.remaining_time(), 1_hours ) ) );
            break;
    }
}

void iuse_flowerpot_plant::set_growing_plant( item &i,
        const itype_id seed,
        const time_point planted_time,
        const int seeds,
        const int fertilizer )
{
    if( seed.is_valid() ) {
        i.set_var( VAR_SEED_TYPE, seed.str() );
        i.set_var( VAR_PLANTED_DATE, to_turn<int>( planted_time ) );
        i.set_var( VAR_SEED_AMT, seeds );
        i.set_var( VAR_FERT_AMT, fertilizer );
    } else {
        clear_growing_plant( i );
    }
}

void iuse_flowerpot_plant::clear_growing_plant( item &i )
{
    i.erase_var( VAR_SEED_TYPE );
    i.erase_var( VAR_PLANTED_DATE );
    i.erase_var( VAR_SEED_AMT );
    i.erase_var( VAR_FERT_AMT );
}

auto iuse_flowerpot_plant::query_adjacent_pot( const player &who,
        bool empty ) -> std::optional<item *>
{
    const auto selector_fn = empty ? empty_pot_selector : full_pot_selector;
    const auto p_selector_fn = [&]( const item * it ) { return selector_fn( *it ); };

    auto &map = get_map();
    const auto has_inv_pots = who.has_item_with( selector_fn );
    const auto has_map_pots = map.has_adjacent_item_with( who.pos(), selector_fn );

    if( !has_inv_pots && !has_map_pots ) {
        return std::nullopt;
    }

    std::optional<tripoint> pot_pos;
    if( has_map_pots ) {
        const auto fn = [&]( const tripoint & p ) {
            bool ok = false;
            ok |= map.has_item_with( p, selector_fn );
            ok |= ( who.pos() == p ) && who.has_item_with( selector_fn );
            return ok;
        };

        pot_pos =
            choose_adjacent_highlight(
                _( "Which planter?" ),
                _( "Never mind." ),
                fn
            );
    } else if( has_inv_pots ) {
        pot_pos = who.pos();
    }

    if( !pot_pos.has_value() ) {
        return std::nullopt;
    }


    std::vector<item *> choices{};
    const auto map_stack = map.i_at( pot_pos.value() );
    std::ranges::copy_if( map_stack, std::back_inserter( choices ), p_selector_fn );
    if( pot_pos.value() == who.pos() ) {
        std::ranges::copy( who.items_with( selector_fn ), std::back_inserter( choices ) );
    }

    if( choices.empty() ) {
        return std::nullopt;
    }

    if( choices.size() > 1 ) {
        uilist lst;
        for( const auto i : choices ) {
            lst.addentry( i->display_name() );
        }
        lst.query();

        if( lst.ret < 0 ) {
            return std::nullopt;
        }

        return choices[lst.ret];
    }

    return choices[0];
}

auto iuse_flowerpot_plant::get_info( const item &i ) const -> growth_info
{
    const auto seed_id = itype_id( i.get_var( VAR_SEED_TYPE, "" ) );
    if( !seed_id.is_valid() ) {
        return growth_info{};
    }

    const int num_seeds =  i.get_var( VAR_SEED_AMT, 1 );
    const int fert_amt = i.get_var( VAR_FERT_AMT, 1 );
    const auto planted_time = time_point::from_turn( i.get_var( VAR_PLANTED_DATE,
                              to_turn<int>( calendar::turn ) ) );

    const auto growth_time = calculate_growth_time( seed_id, fert_amt );

    return growth_info{seed_id, planted_time, growth_time, harvest_mult, fert_amt, num_seeds };
}

auto iuse_flowerpot_plant::calculate_growth_time( const itype_id &seed_id,
        const int used_fert ) const -> time_duration
{
    const auto epoch = seed_id->seed->get_plant_epoch() * 3;
    const auto rate = growth_rate + ( used_fert * fert_boost );
    const auto growth_time = epoch / rate;

    return growth_time;
}

auto iuse_flowerpot_plant::full_pot_selector( const item &it ) -> bool
{
    if( !it.type->can_use( IUSE_ACTOR ) ) {
        return false;
    }

    const auto actor =
        dynamic_cast<const iuse_flowerpot_plant *>( it.get_use( IUSE_ACTOR )->get_actor_ptr() );
    if( actor == nullptr ) {
        return false;
    }

    const auto info = actor->get_info( it );
    return info.stage() != empty;
}

auto iuse_flowerpot_plant::empty_pot_selector( const item &it ) -> bool
{
    if( !it.type->can_use( IUSE_ACTOR ) ) {
        return false;
    }

    const auto actor =
        dynamic_cast<const iuse_flowerpot_plant *>( it.get_use( IUSE_ACTOR )->get_actor_ptr() );
    if( actor == nullptr ) {
        return false;
    }

    const auto info = actor->get_info( it );
    return info.stage() ==        empty;
}

// -------------------

void iuse_flowerpot_collect::load( const JsonObject & )
{

}

auto iuse_flowerpot_collect::use( player &who, item &, bool, const tripoint & ) const -> int
{
    constexpr auto get_harvestable_furn = []( const tripoint & here ) {
        const auto &map = get_map();
        return map.has_flag( "PLANT", here );
    };

    const auto source_pos_opt =
        choose_adjacent_highlight(
            _( "Transplant what?" ),
            _( "There is nothing that can be collected nearby." ),
            get_harvestable_furn,
            false );

    if( !source_pos_opt.has_value() ) {
        return 0;
    }

    const auto source_pos = source_pos_opt.value();
    if( !source_pos_opt.has_value() ) {
        return 0;
    }

    const auto target_pot = iuse_flowerpot_plant::query_adjacent_pot( who, true );
    if( !target_pot.has_value() ) {
        return 0;
    }

    const auto actor = dynamic_cast<const iuse_flowerpot_plant *>( target_pot.value()->get_use(
                           iuse_flowerpot_plant::IUSE_ACTOR )->get_actor_ptr() );
    if( !actor ) {
        debugmsg( "Invalid iuse_actor" );
        return 0;
    }

    auto stack = get_map().i_at( source_pos );

    constexpr auto is_seed = []( const item * it ) { return it->is_seed(); };
    const auto seed_it = std::ranges::find_if( stack, is_seed );
    if( seed_it == stack.end() ) {
        debugmsg( "Missing seed" );
        return 0;
    }

    const item *seed = *seed_it;
    if( !actor->terrain.contains( seed->type->seed->required_terrain_flag ) ) {
        add_msg( "You can't collect that into this planter." );
        return 0;
    }

    // TODO: make an activity actor?
    who.moves -= to_turns<int>( 30_seconds );
    transfer_map_to_flowerpot( source_pos, *target_pot.value(), actor, seed->typeId() );

    return 0;
}

void iuse_flowerpot_collect::transfer_map_to_flowerpot(
    const tripoint &map_pos, item &flowerpot, const iuse_flowerpot_plant *actor,
    const itype_id &seed_type )
{
    auto &m = get_map();

    const auto furn_id = m.furn( map_pos );

    if( !furn_id->plant ) {
        debugmsg( "Invalid plant_data" );
        return;
    }

    auto stack = m.i_at( map_pos );

    const auto is_seed = [&]( const item * it ) { return it->typeId() == seed_type; };
    const auto seed_it = std::ranges::find_if( stack, is_seed );
    if( seed_it == stack.end() ) {
        debugmsg( "Missing seed" );
        return;
    }
    item *seed = *seed_it;

    auto max_seeds = actor->seeds_per_use.second;
    auto max_fert = actor->fert_per_use.second;

    std::vector<detached_ptr<item>> comps;
    stack.remove_top_items_with( [&]( detached_ptr<item> &&it ) {
        if( max_seeds > 0 && it->typeId() == seed_type ) {
            // Move the seeds
            return item::use_charges( std::move( it ), seed_type, max_seeds, comps, map_pos );
        }
        if( max_fert > 0 && it->typeId() == itype_fertilizer ) {
            // Clone the fertilizer
            auto tmp = item::spawn( *it );
            item::use_charges( std::move( tmp ), itype_fertilizer, max_fert, comps, map_pos );
        }
        return std::move( it );
    } );

    // Erase fertilizer and reset furniture if no more seeds
    if( std::ranges::find_if( stack, is_seed ) == stack.end() ) {
        m.furn_set( map_pos, furn_id->plant->base );
        stack.remove_top_items_with( []( detached_ptr<item> &&it ) {
            if( it->typeId() == itype_fertilizer )
                return detached_ptr<item> {};
            return std::move( it );
        } );
    }

    const auto fert_amt = actor->fert_per_use.second - max_fert;
    const auto seed_amt = actor->seeds_per_use.second - max_seeds;

    const auto old_epoch = seed->get_plant_epoch() * 3 * furn_id->plant->growth_multiplier;
    const auto old_pct = seed->age() / old_epoch;

    const auto new_epoch = actor->calculate_growth_time( seed->typeId(), fert_amt );
    const auto new_age = new_epoch * old_pct;
    seed->set_age( new_age );

    actor->set_growing_plant( flowerpot, seed->typeId(), seed->birthday(), seed_amt, fert_amt );
    actor->update( flowerpot );
}

auto iuse_flowerpot_collect::can_use( const Character &who, const item &, bool,
                                      const tripoint &pos ) const -> ret_val<bool>
{
    const bool has_empty_pot_inv = who.has_item_with( iuse_flowerpot_plant::empty_pot_selector );
    const bool has_empty_pot_near = get_map().has_adjacent_item_with( pos,
                                    iuse_flowerpot_plant::empty_pot_selector );
    const bool has_plant_furn = get_map().has_adjacent_furniture_with( pos, []( const furn_t &f ) {
        return f.has_flag( "PLANT" );
    } );

    if( ( has_empty_pot_inv || has_empty_pot_near ) && has_plant_furn ) {
        return ret_val<bool>::make_success();
    }

    /*
    const bool has_full_pot = who.has_item_with( iuse_flowerpot_plant::full_pot_selector );
    const bool has_empty_furn = get_map().has_adjacent_furniture_with( pos, []( const furn_t &f ) {
        return f.has_flag( "PLANTABLE" );
    } );
    const bool has_empty_ter = get_map().has_adjacent_terrain_with( pos, []( const ter_t & t ) {
        return t.has_flag( "PLANTABLE" );
    } );

    if( has_full_pot && ( has_empty_furn || has_empty_ter ) ) {
        return ret_val<bool>::make_success();
    }
    */

    return ret_val<bool>::make_failure();
}

auto iuse_flowerpot_collect::clone() const -> std::unique_ptr<iuse_actor>
{
    return std::make_unique<iuse_flowerpot_collect>( *this );
}
