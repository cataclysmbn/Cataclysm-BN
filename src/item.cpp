#include "item.h"

#include <algorithm>
#include <numeric>
#include <array>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iterator>
#include <limits>
#include <locale>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_set>

#include "active_tile_data_def.h"
#include "ammo.h"
#include "ascii_art.h"
#include "avatar.h"
#include "bionics.h"
#include "bodypart.h"
#include "cached_item_options.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "character.h"
#include "character_encumbrance.h"
#include "character_functions.h"
#include "character_id.h"
#include "character_martial_arts.h"
#include "character_stat.h"
#include "clothing_mod.h"
#include "clzones.h"
#include "color.h"
#include "coordinate_conversions.h"
#include "craft_command.h"
#include "damage.h"
#include "debug.h"
#include "dispersion.h"
#include "drop_token.h"
#include "effect.h" // for weed_msg
#include "enums.h"
#include "explosion.h"
#include "faction.h"
#include "fault.h"
#include "field_type.h"
#include "fire.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "gun_mode.h"
#include "iexamine.h"
#include "int_id.h"
#include "inventory.h"
#include "item_category.h"
#include "item_factory.h"
#include "item_group.h"
#include "iteminfo_format_utils.h"
#include "iteminfo_query.h"
#include "itype.h"
#include "iuse.h"
#include "iuse_actor.h"
#include "line.h"
#include "locations.h"
#include "magic.h"
#include "map.h"
#include "martialarts.h"
#include "material.h"
#include "melee.h"
#include "messages.h"
#include "mod_manager.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "pimpl.h"
#include "player.h"
#include "player_activity.h"
#include "pldata.h"
#include "point.h"
#include "projectile.h"
#include "ranged.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "relic.h"
#include "requirements.h"
#include "ret_val.h"
#include "rng.h"
#include "rot.h"
#include "scores_ui.h"
#include "skill.h"
#include "stomach.h"
#include "string_formatter.h"
#include "string_id_utils.h"
#include "string_utils.h"
#include "text_snippets.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "units_energy.h"
#include "units_utility.h"
#include "value_ptr.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vitamin.h"
#include "vpart_position.h"
#include "weather.h"
#include "weather_gen.h"

static const std::string GUN_MODE_VAR_NAME( "item::mode" );
static const std::string CLOTHING_MOD_VAR_PREFIX( "clothing_mod_" );

static const ammo_effect_str_id ammo_effect_BLACKPOWDER( "BLACKPOWDER" );
static const ammo_effect_str_id ammo_effect_INCENDIARY( "INCENDIARY" );
static const ammo_effect_str_id ammo_effect_NEVER_MISFIRES( "NEVER_MISFIRES" );
static const ammo_effect_str_id ammo_effect_RECYCLED( "RECYCLED" );

static const ammotype ammo_battery( "battery" );
static const ammotype ammo_plutonium( "plutonium" );

static const item_category_id itemcat_drugs( "drugs" );
static const item_category_id itemcat_food( "food" );
static const item_category_id itemcat_maps( "maps" );

static const efftype_id effect_cig( "cig" );
static const efftype_id effect_shakes( "shakes" );
static const efftype_id effect_sleep( "sleep" );
static const efftype_id effect_weed_high( "weed_high" );

static const fault_id fault_gun_blackpowder( "fault_gun_blackpowder" );
static const fault_id fault_bionic_nonsterile( "fault_bionic_nonsterile" );

static const flag_id flag_MARIJUANA( "MARIJUANA" );

static const gun_mode_id gun_mode_REACH( "REACH" );

static const itype_id itype_barrel_small( "barrel_small" );
static const itype_id itype_cig_butt( "cig_butt" );
static const itype_id itype_cig_lit( "cig_lit" );
static const itype_id itype_cigar_butt( "cigar_butt" );
static const itype_id itype_cigar_lit( "cigar_lit" );
static const itype_id itype_joint_roach( "joint_roach" );
static const itype_id itype_plut_cell( "plut_cell" );
static const itype_id itype_rad_badge( "rad_badge" );
static const itype_id itype_tuned_mechanism( "tuned_mechanism" );
static const itype_id itype_stock_small( "stock_small" );
static const itype_id itype_UPS( "UPS" );
static const itype_id itype_genome_drive( "genome_drive" );
static const itype_id itype_bio_armor( "bio_armor" );
static const itype_id itype_waterproof_gunmod( "waterproof_gunmod" );
static const itype_id itype_water( "water" );
static const itype_id itype_water_acid( "water_acid" );
static const itype_id itype_water_acid_weak( "water_acid_weak" );

static const skill_id skill_survival( "survival" );
static const skill_id skill_throw( "throw" );
static const skill_id skill_unarmed( "unarmed" );
static const skill_id skill_weapon( "weapon" );

static const quality_id qual_JACK( "JACK" );
static const quality_id qual_LIFT( "LIFT" );
static const species_id ROBOT( "ROBOT" );

static const trait_flag_str_id trait_flag_CANNIBAL( "CANNIBAL" );

static const bionic_id bio_digestion( "bio_digestion" );

static const trait_id trait_CARNIVORE( "CARNIVORE" );
static const trait_id trait_ILLITERATE( "ILLITERATE" );
static const trait_id trait_LIGHTWEIGHT( "LIGHTWEIGHT" );
static const trait_id trait_SAPROVORE( "SAPROVORE" );
static const trait_id trait_TOLERANCE( "TOLERANCE" );
static const trait_id trait_WOOLALLERGY( "WOOLALLERGY" );

static const vitamin_id vitamin_human_flesh_vitamin( "human_flesh_vitamin" );

static const std::string flag_FLAMMABLE( "FLAMMABLE" );
static const std::string flag_FLAMMABLE_ASH( "FLAMMABLE_ASH" );
static const std::string flag_DEEP_WATER( "DEEP_WATER" );
static const std::string flag_LIQUID( "LIQUID" );
static const std::string flag_LIQUIDCONT( "LIQUIDCONT" );

static const std::string has_thievery_witness( "has_thievery_witness" );
static const activity_id ACT_PICKUP( "ACT_PICKUP" );

static const matec_id rapid_strike( "RAPID" );

class npc_class;

using npc_class_id = string_id<npc_class>;

std::string rad_badge_color( const int rad )
{
    using pair_t = std::pair<const int, const translation>;

    static const std::array<pair_t, 6> values = {{
            pair_t {  0, to_translation( "color", "green" ) },
            pair_t { 30, to_translation( "color", "blue" )  },
            pair_t { 60, to_translation( "color", "yellow" )},
            pair_t {120, to_translation( "color", "orange" )},
            pair_t {240, to_translation( "color", "red" )   },
            pair_t {500, to_translation( "color", "black" ) },
        }
    };

    for( const auto &i : values ) {
        if( rad <= i.first ) {
            return i.second.translated();
        }
    }

    return values.back().second.translated();
}

light_emission nolight = {0, 0, 0};

// Returns the default item type, used for the null item (default constructed),
// the returned pointer is always valid, it's never cleared by the @ref Item_factory.
static const itype *nullitem()
{
    static itype nullitem_m;
    return &nullitem_m;
}

item &null_item_reference()
{
    static item result{};
    // reset it, in case a previous caller has changed it
    result = item();
    return result;
}

namespace item_internal
{
bool goes_bad_temp_cache = false;
const item *goes_bad_temp_cache_for = nullptr;
inline bool goes_bad_cache_fetch()
{
    return goes_bad_temp_cache;
}
inline void goes_bad_cache_set( const item *i )
{
    goes_bad_temp_cache = i->goes_bad();
    goes_bad_temp_cache_for = i;
}
inline void goes_bad_cache_unset()
{
    goes_bad_temp_cache = false;
    goes_bad_temp_cache_for = nullptr;
}
inline bool goes_bad_cache_is_for( const item *i )
{
    return goes_bad_temp_cache_for == i;
}

struct scoped_goes_bad_cache {
    scoped_goes_bad_cache( item *i ) {
        goes_bad_cache_set( i );
    }
    ~scoped_goes_bad_cache() {
        goes_bad_cache_unset();
    }
};
} // namespace item_internal

const int item::INFINITE_CHARGES = INT_MAX;

item::item() : contents( this ),
    components( new component_item_location( this ) ),
    bday( calendar::start_of_cataclysm )
{
    type = nullitem();
    charges = 0;
}

item::item( const itype *type, time_point turn, int qty ) : type( type ),
    contents( this ),
    components( new component_item_location( this ) ), bday( turn )
{
    item_vars = type->item_vars;
    corpse = has_flag( flag_CORPSE ) ? &mtype_id::NULL_ID().obj() : nullptr;
    item_counter = type->countdown_interval;

    if( qty >= 0 ) {
        charges = qty;
    } else {
        if( type->tool && type->tool->rand_charges.size() > 1 ) {
            const int charge_roll = rng( 1, type->tool->rand_charges.size() - 1 );
            charges = rng( type->tool->rand_charges[charge_roll - 1], type->tool->rand_charges[charge_roll] );
        } else {
            charges = type->charges_default();
        }
    }

    if( has_flag( flag_NANOFAB_TEMPLATE ) ) {
        // Define all nanofab subgroups from nanofab_recipes.json
        auto all_groups = item_controller->get_all_group_names();

        // Prepare a vector to hold nanofab groups dynamically
        std::vector<item_group_id> nanofab_groups;

        // Populate it dynamically (this is probably pretty performance intensive, but allows for modded templates)
        for( const auto &group : all_groups ) {
            const std::string &name = group.str();
            if( name.starts_with( "nanofab_template_" ) ) {
                nanofab_groups.push_back( group );
            }
        }

        // Pick one subgroup randomly
        const item_group_id &chosen_group = random_entry( nanofab_groups );

        // Store which subgroup we picked
        set_var( "NANOFAB_GROUP_ID", chosen_group.str() );

        // Gather all possible items from this subgroup
        std::set<const itype *> all_items = item_group::every_possible_item_from( chosen_group );
        std::vector<const itype *> all_items_vec( all_items.begin(), all_items.end() );

        // Legacy compatibility: store the first item ID as fallback
        if( !all_items_vec.empty() ) {
            set_var( "NANOFAB_ITEM_ID", all_items_vec.front()->get_id().str() );
        }
    }


    if( type->gun ) {
        for( const itype_id &mod : type->gun->built_in_mods ) {
            detached_ptr<item> it = item::spawn( mod, turn, qty );
            it->set_flag( flag_IRREMOVABLE );
            put_in( std::move( it ) );
        }
        for( const itype_id &mod : type->gun->default_mods ) {
            put_in( item::spawn( mod, turn, qty ) );
        }

    } else if( type->magazine ) {
        if( type->magazine->count > 0 ) {
            put_in( item::spawn( type->magazine->default_ammo, calendar::turn, type->magazine->count ) );
        }

    } else if( goes_bad() ) {
        active = true;
        last_rot_check = bday;

    } else if( type->tool ) {
        if( ammo_remaining() && !ammo_types().empty() ) {
            ammo_set( ammo_default(), ammo_remaining() );
        }
    }

    if( ( type->gun || type->tool ) && !magazine_integral() ) {
        set_var( "magazine_converted", 1 );
    }

    if( !type->snippet_category.empty() ) {
        snip_id = SNIPPET.random_id_from_category( type->snippet_category );
    }

    // item always has any relic properties from itype.
    if( type->relic_data ) {
        relic_data = type->relic_data;
    }
}

item::item( const itype_id &id, time_point turn, int qty )
    : item( & * id, turn, qty ) {}

item::item( const itype *type, time_point turn, default_charges_tag )
    : item( type, turn, type->charges_default() ) {}

item::item( const itype_id &id, time_point turn, default_charges_tag tag )
    : item( & * id, turn, tag ) {}

item::item( const itype *type, time_point turn, solitary_tag )
    : item( type, turn, type->count_by_charges() ? 1 : -1 ) {}

item::item( const itype_id &id, time_point turn, solitary_tag tag )
    : item( & * id, turn, tag ) {}

static const item *get_most_rotten_component( const item &craft )
{
    const item *most_rotten = nullptr;
    for( const item * const &it : craft.get_components() ) {
        if( it->goes_bad() ) {
            if( !most_rotten || it->get_relative_rot() > most_rotten->get_relative_rot() ) {
                most_rotten = it;
            }
        }
    }
    return most_rotten;
}

item::item( const recipe *rec, int qty, std::vector<detached_ptr<item>> &&items,
            std::vector<item_comp> &&selections )
    : item( "craft", calendar::turn, qty )
{
    craft_data_ = cata::make_value<craft_data>();
    craft_data_->making = rec;
    for( detached_ptr<item> &it : items ) {
        components.push_back( std::move( it ) );
    }
    craft_data_->comps_used = std::move( selections );

    if( is_food() ) {
        activate();
        last_rot_check = bday;
        if( goes_bad() ) {
            const item *most_rotten = get_most_rotten_component( *this );
            if( most_rotten ) {
                set_relative_rot( most_rotten->get_relative_rot() );
            }
        }
    }

    for( item * const &component : components ) {
        for( const flag_id &f : component->item_tags ) {
            if( f->craft_inherit() ) {
                set_flag( f );
            }
        }
        for( const flag_id &f : component->type->get_flags() ) {
            if( f->craft_inherit() ) {
                set_flag( f );
            }
        }
    }
    // this extra section is so that in-progress crafts will correctly display expected flags.
    for( const flag_id &flag : rec->flags_to_delete ) {
        unset_flag( flag );
    }
}

item::item( const item &source ) : game_object<item>( source ), contents( this ),
    components( new component_item_location( this ) )
{
    //TODO!: back to defaults
    //Awful copy block, this can be avoided with equally awful inheritance shenanigans but...
    type = source.type;
    faults = source.faults;
    item_tags = source.item_tags;
    curammo = source.curammo;
    item_vars = source.item_vars;
    corpse = source.corpse;
    corpse_name = source.corpse_name;
    techniques = source.techniques;
    craft_data_ = source.craft_data_;
    relic_data = source.relic_data;
    charges = source.charges;
    energy = source.energy;
    recipe_charges = source.recipe_charges;
    burnt = source.burnt;
    poison = source.poison;
    frequency = source.frequency;
    snip_id = source.snip_id;
    irradiation = source.irradiation;
    item_counter = source.item_counter;
    mission_id = source.mission_id;
    player_id = source.player_id;
    encumbrance_update_ = source.encumbrance_update_;
    rot = source.rot;
    last_rot_check = source.last_rot_check;
    bday = source.bday;
    owner = source.owner;
    old_owner = source.old_owner;
    damage_ = source.damage_;
    light = source.light;
    invlet = source.invlet;
    active = source.active;
    activated_by = source.activated_by;
    is_favorite = source.is_favorite;

    for( item * const &it : source.contents.all_items_top() ) {
        contents.insert_item( item::spawn( *it ) );
    }

    for( item * const &it : source.components ) {
        components.push_back( item::spawn( *it ) );
    }
}

item &item::operator=( const item &source )
{
    type = source.type;
    faults = source.faults;
    item_tags = source.item_tags;
    curammo = source.curammo;
    item_vars = source.item_vars;
    corpse = source.corpse;
    corpse_name = source.corpse_name;
    techniques = source.techniques;
    craft_data_ = source.craft_data_;
    relic_data = source.relic_data;
    charges = source.charges;
    energy = source.energy;
    recipe_charges = source.recipe_charges;
    burnt = source.burnt;
    poison = source.poison;
    frequency = source.frequency;
    snip_id = source.snip_id;
    irradiation = source.irradiation;
    item_counter = source.item_counter;
    mission_id = source.mission_id;
    player_id = source.player_id;
    encumbrance_update_ = source.encumbrance_update_;
    rot = source.rot;
    last_rot_check = source.last_rot_check;
    bday = source.bday;
    owner = source.owner;
    old_owner = source.old_owner;
    damage_ = source.damage_;
    light = source.light;
    invlet = source.invlet;
    active = source.active;
    activated_by = source.activated_by;
    is_favorite = source.is_favorite;

    contents.clear_items();

    for( item * const &it : source.contents.all_items_top() ) {
        contents.insert_item( item::spawn( *it ) );
    }

    components.clear();

    for( item * const &it : source.components ) {
        components.push_back( item::spawn( *it ) );
    }
    return *this;
}

void item::on_destroy()
{
    //These are getting left out until it can be deferred better
    //components.on_destroy();
    //contents.on_destroy();
}


item::~item() = default;

detached_ptr<item> item::make_corpse( const mtype_id &mt, time_point turn, const std::string &name,
                                      const int upgrade_time )
{
    if( !mt.is_valid() ) {
        debugmsg( "tried to make a corpse with an invalid mtype id" );
    }

    std::string corpse_type = mt == mtype_id::NULL_ID() ? "corpse_generic_human" : "corpse";

    detached_ptr<item> result = item::spawn( corpse_type, turn );

    result->corpse = &mt.obj();

    if( result->corpse->has_flag( MF_REVIVES ) ) {
        if( one_in( 20 ) ) {
            result->set_flag( flag_REVIVE_SPECIAL );
        }
        result->set_var( "upgrade_time", std::to_string( upgrade_time ) );
    }

    // This is unconditional because the const itemructor above sets result.name to
    // "human corpse".
    result->corpse_name = name;

    return  result;
}

void item::convert( const itype_id &new_type )
{
    type = &*new_type;
    relic_data = type->relic_data;
}

void item::deactivate()
{
    if( !is_active() ) {
        return; // no-op
    }

    active = false;
    if( is_tool() ) {
        type->tool->turns_active = 0;
    }

    // Is not placed in the world, so either a template of some kind or a temporary item.
    if( !has_position() ) {
        return;
    }
    switch( where() ) {
        case item_location_type::map:
            get_map().make_inactive( *this );
            break;
        case item_location_type::vehicle:
            get_map().veh_at( position() )->vehicle().make_inactive( *this );
            break;
        default:
            break;
    }

}

void item::activate()
{
    if( is_active() ) {
        return; // no-op
    }

    if( type->countdown_interval > 0 ) {
        set_counter( type->countdown_interval );
    }

    active = true;

    // Is not placed in the world, so either a template of some kind or a temporary item.
    if( !has_position() ) {
        return;
    }
    switch( where() ) {
        case item_location_type::map:
            get_map().make_active( *this );
            break;
        case item_location_type::vehicle:
            get_map().veh_at( position() )->vehicle().make_active( *this );
            break;
        default:
            break;
    }
}

bool item::revert( const Character *ch, bool alert )
{
    const auto &tooldata = type->tool;
    // Can't be reverted, prevents destruction of irrevertable items.
    if( !tooldata->revert_to.has_value() ) {
        return false;
    }
    if( ch && alert && !tooldata->revert_msg.empty() ) {
        ch->add_msg_if_player( m_info, _( tooldata->revert_msg ), tname() );
    }
    convert( *tooldata->revert_to );
    return true;
}

units::energy item::mod_energy( const units::energy &qty )
{
    if( !is_battery() ) {
        debugmsg( "Tried to set energy of non-battery item" );
        return 0_J;
    }

    units::energy val = energy_remaining() + qty;
    if( val < 0_J ) {
        return val;
    } else if( val > type->battery->max_capacity ) {
        energy = type->battery->max_capacity;
    } else {
        energy = val;
    }
    return 0_J;
}

void item::ammo_set( const itype_id &ammo, int qty )
{
    if( qty < 0 ) {
        // completely fill an integral or existing magazine
        if( magazine_integral() || magazine_current() ) {
            qty = ammo_capacity();

            // else try to add a magazine using default ammo count property if set
        } else if( !magazine_default().is_null() ) {
            item mag( magazine_default() );
            if( mag.type->magazine->count > 0 ) {
                qty = mag.type->magazine->count;
            } else {
                qty = mag.ammo_capacity();
            }
        }
    }

    if( qty <= 0 ) {
        ammo_unset();
        return;
    }

    // handle reloadable tools and guns with no specific ammo type as special case
    if( ( ammo.is_null() && ammo_types().empty() ) || is_money() ) {
        if( ( is_tool() || is_gun() ) && magazine_integral() ) {
            curammo = nullptr;
            charges = std::min( qty, ammo_capacity() );
        }
        return;
    }

    // check ammo is valid for the item
    const itype *atype = &*ammo;
    if( !atype->ammo || !ammo_types().contains( atype->ammo->type ) ) {
        debugmsg( "Tried to set invalid ammo %s[%d] for %s", atype->get_id(), qty, typeId() );
        return;
    }

    if( is_magazine() ) {
        ammo_unset();
        detached_ptr<item> set_ammo = item::spawn( ammo, calendar::turn, std::min( qty,
                                      ammo_capacity() ) );
        if( has_flag( flag_NO_UNLOAD ) ) {
            set_ammo->set_flag( flag_NO_DROP );
            set_ammo->set_flag( flag_IRREMOVABLE );
        }
        put_in( std::move( set_ammo ) );

    } else if( magazine_integral() ) {
        curammo = atype;
        charges = std::min( qty, ammo_capacity() );

    } else {
        if( !magazine_current() ) {
            itype_id mag = magazine_default();
            if( !mag->magazine ) {
                debugmsg( "Tried to set ammo %s[%d] without suitable magazine for %s",
                          atype->get_id(), qty, typeId() );
                return;
            }

            // if default magazine too small fetch instead closest available match
            if( mag->magazine->capacity < qty ) {
                // as above call to magazine_default successful can infer minimum one option exists
                auto iter = type->magazines.find( atype->ammo->type );
                if( iter == type->magazines.end() ) {
                    debugmsg( "%s doesn't have a magazine for %s",
                              typeId(), ammo );
                    return;
                }
                std::vector<itype_id> opts( iter->second.begin(), iter->second.end() );
                std::ranges::sort( opts, []( const itype_id & lhs, const itype_id & rhs ) {
                    return lhs->magazine->capacity < rhs->magazine->capacity;
                } );
                mag = opts.back();
                for( const itype_id &e : opts ) {
                    if( e->magazine->capacity >= qty ) {
                        mag = e;
                        break;
                    }
                }
            }
            put_in( item::spawn( mag ) );
        }
        magazine_current()->ammo_set( ammo, qty );
    }

}

void item::ammo_unset()
{
    if( !is_tool() && !is_gun() && !is_magazine() ) {
        // do nothing
    } else if( is_magazine() ) {
        contents.clear_items();
    } else if( magazine_integral() ) {
        curammo = nullptr;
        charges = 0;
    } else if( magazine_current() ) {
        magazine_current()->ammo_unset();
    }

}

int item::damage() const
{
    return damage_;
}

int item::damage_level( int max ) const
{
    if( damage_ == 0 || max <= 0 ) {
        return 0;
    } else if( max_damage() <= 1 ) {
        return damage_ > 0 ? max : damage_;
    } else if( damage_ < 0 ) {
        return -( ( max - 1 ) * ( -damage_ - 1 ) / ( max_damage() - 1 ) + 1 );
    } else {
        return ( max - 1 ) * ( damage_ - 1 ) / ( max_damage() - 1 ) + 1;
    }
}

void item::set_damage( int qty )
{
    on_damage( qty - damage_, DT_TRUE );
    damage_ = std::max( std::min( qty, max_damage() ), min_damage() );
}

detached_ptr<item> item::split( int qty )
{
    if( qty <= 0 || !count_by_charges() || qty >= charges ) {
        return detach();
    }
    detached_ptr<item> res = item::spawn( *this );
    res->charges = qty;
    charges -= qty;
    return res;
}

detached_ptr<item> item::unsafe_split( int qty )
{
    if( !count_by_charges() ) {
        debugmsg( "Attempted to unsafe_split a non-count by charges item." );
        return detached_ptr<item>();
    }
    if( qty == 0 || qty >= charges ) {
        qty = charges;
    }
    detached_ptr<item> res = item::spawn( *this );
    res->charges = qty;
    charges -= qty;
    return res;
}

void item::unsafe_rejoin( item &old )
{
    if( old.charges != 0 ) {
        return;
    }

    merge_charges( old.detach(), true );
}

bool item::attempt_detach( std::function < detached_ptr<item>( detached_ptr<item> && ) > cb )
{
    if( is_null() ) {
        return false;
    }
    if( count_by_charges() ) {
        return attempt_split( 0, cb );
    }
    return game_object<item>::attempt_detach( cb );
}

bool item::attempt_split( int qty,
                          const std::function < detached_ptr<item>( detached_ptr<item> && ) > & cb )
{
    detached_ptr<item> det = unsafe_split( qty );
    if( !det ) {
        return false;
    }
    item &after_split = *det;
    int starting_charges = after_split.charges;
    det = cb( std::move( det ) );
    bool ret = true;
    bool changed = false;
    if( det ) {
        if( det->type->get_id() != type->get_id() ) {
            debugmsg( "attempt_split returned the wrong item type" );
        } else {
            changed |= det->charges != starting_charges;
            //Copy any changed properties from the new item, except the charges
            int old_charges = charges;
            *this = *det;
            charges = old_charges;
            merge_charges( std::move( det ), true );
        }
        ret = false;
    } else {
        changed = true;
    }
    if( changed ) {
        contents_item_location *contents_loc = dynamic_cast<contents_item_location *>( &*loc );
        if( contents_loc ) {
            contents_loc->on_changed( this );
        }
    }
    after_split.unsafe_rejoin( *this );
    return ret;
}

bool item::is_null() const
{
    // Actually, type should never by null at all.
    return ( type == nullptr || type == nullitem() || typeId().is_null() );
}

bool item::is_unarmed_weapon() const
{
    return has_flag( flag_UNARMED_WEAPON ) || is_null();
}

bool item::covers( const bodypart_id &bp ) const
{
    return get_covered_body_parts().test( bp.id() );
}

body_part_set item::get_covered_body_parts() const
{
    return get_covered_body_parts( get_side() );
}

body_part_set item::get_covered_body_parts( const side s ) const
{
    body_part_set res;

    if( is_gun() ) {
        // Currently only used for guns with the should strap mod, other guns might
        // go on another bodypart.
        res.set( bodypart_str_id( "torso" ) );
    }

    const islot_armor *armor = find_armor_data();
    if( armor == nullptr ) {
        return res;
    }

    for( const armor_portion_data &data : armor->data ) {
        res.unify_set( data.covers );
    }

    if( !armor->sided ) {
        return res; // Just ignore the side.
    }

    switch( s ) {
        case side::BOTH:
        case side::num_sides:
            break;

        case side::LEFT:
            res.reset( bodypart_str_id( "arm_r" ) );
            res.reset( bodypart_str_id( "hand_r" ) );
            res.reset( bodypart_str_id( "leg_r" ) );
            res.reset( bodypart_str_id( "foot_r" ) );
            break;

        case side::RIGHT:
            res.reset( bodypart_str_id( "arm_l" ) );
            res.reset( bodypart_str_id( "hand_l" ) );
            res.reset( bodypart_str_id( "leg_l" ) );
            res.reset( bodypart_str_id( "foot_l" ) );
            break;
    }

    return res;
}

bool item::is_sided() const
{
    const islot_armor *armor = find_armor_data();
    return armor ? armor->sided : false;
}

side item::get_side() const
{
    // MSVC complains if directly cast double to enum
    return static_cast<side>( static_cast<int>( get_var( "lateral",
                              static_cast<int>( side::BOTH ) ) ) );
}

bool item::set_side( side s )
{
    if( !is_sided() ) {
        return false;
    }

    if( s == side::BOTH ) {
        erase_var( "lateral" );
    } else {
        set_var( "lateral", static_cast<int>( s ) );
    }

    return true;
}

bool item::swap_side()
{
    return set_side( opposite_side( get_side() ) );
}

bool item::is_worn_only_with( const item &it ) const
{
    return( ( has_flag( flag_POWERARMOR_EXTERNAL ) || has_flag( flag_POWERARMOR_MOD ) ) &&
            it.has_flag( flag_POWERARMOR_EXO ) );
}

detached_ptr<item> item::in_its_container( detached_ptr<item> &&self )
{
    return item::in_container( self->type->default_container.value_or( itype_id::NULL_ID() ),
                               std::move( self ) );
}

detached_ptr<item> item::in_container( const itype_id &cont, detached_ptr<item> &&self )
{
    if( !cont.is_null() ) {
        detached_ptr<item> ret = item::spawn( cont, self->birthday() );
        ret->invlet = self->invlet;
        item &obj = *self;
        ret->put_in( std::move( self ) );

        if( obj.made_of( LIQUID ) && ret->is_container() ) {
            // Note: we can't use any of the normal container functions as they check the
            // container being suitable (seals, watertight etc.)
            ret->contents.back().charges = obj.charges_per_volume( ret->get_container_capacity() );
        }
        return ret;
    } else {
        return std::move( self );
    }
}

int item::charges_per_volume( const units::volume &vol ) const
{
    if( count_by_charges() ) {
        if( type->volume == 0_ml ) {
            debugmsg( "Item '%s' with zero volume", tname() );
            return INFINITE_CHARGES;
        }
        // Type cast to prevent integer overflow with large volume containers like the cargo
        // dimension
        return vol * static_cast<int64_t>( type->stack_size ) / type->volume;
    } else {
        units::volume my_volume = volume();
        if( my_volume == 0_ml ) {
            debugmsg( "Item '%s' with zero volume", tname() );
            return INFINITE_CHARGES;
        }
        return vol / my_volume;
    }
}

bool item::display_stacked_with( const item &rhs, bool check_components ) const
{
    return !count_by_charges() && stacks_with( rhs, check_components );
}

bool item::stacks_with( const item &rhs, bool check_components, bool skip_type_check ) const
{
    if( !skip_type_check && type != rhs.type ) {
        return false;
    }
    if( is_relic() && rhs.is_relic() && !( *relic_data == *rhs.relic_data ) ) {
        return false;
    }
    if( is_money() && charges != 0 && rhs.charges != 0 ) {
        // Dealing with nonempty cash cards
        return true;
    }
    // This function is also used to test whether items counted by charges should be merged, for that
    // check the, the charges must be ignored. In all other cases (tools/guns), the charges are important.
    if( !count_by_charges() && charges != rhs.charges ) {
        return false;
    }
    if( is_favorite != rhs.is_favorite ) {
        return false;
    }

    if( is_corpse() || rhs.is_corpse() ) {
        return this->is_corpse() && rhs.is_corpse() && ( *this->get_mtype() == *rhs.get_mtype() );
    }

    if( damage_ != rhs.damage_ ) {
        return false;
    }
    if( burnt != rhs.burnt ) {
        return false;
    }
    if( is_active() != rhs.is_active() ) {
        return false;
    }
    if( item_tags != rhs.item_tags ) {
        return false;
    }
    if( faults != rhs.faults ) {
        return false;
    }
    if( techniques != rhs.techniques ) {
        return false;
    }
    if( item_vars != rhs.item_vars ) {
        return false;
    }

    if( craft_data_ || rhs.craft_data_ ) {
        // In-progress crafts are always distinct items. Easier to handle for the player,
        // and there shouldn't be that many items of this type around anyway.
        return false;
    }
    if( check_components || is_comestible() || is_craft() ) {
        //Only check if at least one item isn't using the default recipe or is comestible
        if( !components.empty() || !rhs.components.empty() ) {
            if( get_uncraft_components() != rhs.get_uncraft_components() ) {
                return false;
            }
        }
    }
    if( contents.num_item_stacks() != rhs.contents.num_item_stacks() ) {
        return false;
    }

    if( ammo_current() != rhs.ammo_current() ) {
        return false;
    }

    if( goes_bad() && rhs.goes_bad() ) {
        // Stack items that fall into the same "bucket" of freshness.
        // Distant buckets are larger than near ones.

        switch( merge_comestible_mode ) {
            case merge_comestible_t::merge_legacy: {
                std::pair<int, clipped_unit> my_clipped_time_to_rot =
                    clipped_time( get_shelf_life() - rot );
                std::pair<int, clipped_unit> other_clipped_time_to_rot =
                    clipped_time( rhs.get_shelf_life() - rhs.rot );
                if( my_clipped_time_to_rot != other_clipped_time_to_rot ) {
                    return false;
                }
            }
            break;
            case merge_comestible_t::merge_liquid: {
                if( !made_of( LIQUID ) || !rhs.made_of( LIQUID ) ) {
                    return false;
                }
            }
            [[fallthrough]];
            default:
                return std::abs( get_relative_rot() - rhs.get_relative_rot() ) <= similarity_threshold;
        }

        if( rotten() != rhs.rotten() ) {
            // just to be safe that rotten and unrotten food is *never* stacked.
            return false;
        }
    }

    return contents.stacks_with( rhs.contents );
}

namespace
{

time_duration weighted_averaged_rot( const item *a, const item *b )
{
    const int base_charges = a->charges + b->charges;

    return base_charges > 0
           ? ( a->get_rot() * a->charges + b->get_rot() * b->charges ) / base_charges
           : 0_seconds;
}

} // namespace

bool item::merge_charges( detached_ptr<item> &&rhs, bool force )
{
    if( this == &*rhs ) {
        debugmsg( "Attempted to merge %s with itself.", debug_name() );
        return false;
    }
    if( !count_by_charges() || ( !stacks_with( *rhs ) && !force ) ) {
        return false;
    }
    item &obj = *rhs;
    safe_reference<item>::merge( this, &*rhs );
    detached_ptr<item> del = std::move( rhs );

    const auto new_rot = weighted_averaged_rot( this, &obj );

    // Prevent overflow when either item has "near infinite" charges.
    if( charges >= INFINITE_CHARGES / 2 || obj.charges >= INFINITE_CHARGES / 2 ) {
        charges = INFINITE_CHARGES;
        return true;
    }
    // We'll just hope that the item counter represents the same thing for both items
    if( item_counter > 0 || obj.item_counter > 0 ) {
        item_counter = ( static_cast<double>( item_counter ) * charges + static_cast<double>
                         ( obj.item_counter ) * obj.charges ) / ( charges + obj.charges );
    }
    charges += obj.charges;

    rot = new_rot;
    set_age( std::max( age(), obj.age() ) );

    return true;
}

void item::put_in( detached_ptr<item> &&payload )
{
    if( !payload || payload->typeId() == itype_id::NULL_ID() ) {
        debugmsg( "Tried to insert non-item into %s", debug_name() );
        return;
    }
    if( &*payload == this ) {
        debugmsg( "Tried to put %s inside itself", debug_name().c_str() );
        return;
    }
    contents.insert_item( std::move( payload ) );
}

void item::set_var( const std::string &name, const int value )
{
    std::ostringstream tmpstream;
    tmpstream.imbue( std::locale::classic() );
    tmpstream << value;
    item_vars[name] = tmpstream.str();
}

void item::set_var( const std::string &name, const long long value )
{
    std::ostringstream tmpstream;
    tmpstream.imbue( std::locale::classic() );
    tmpstream << value;
    item_vars[name] = tmpstream.str();
}

// NOLINTNEXTLINE(cata-no-long)
void item::set_var( const std::string &name, const long value )
{
    std::ostringstream tmpstream;
    tmpstream.imbue( std::locale::classic() );
    tmpstream << value;
    item_vars[name] = tmpstream.str();
}

void item::set_var( const std::string &name, const double value )
{
    item_vars[name] = string_format( "%f", value );
}

double item::get_var( const std::string &name, const double default_value ) const
{
    const auto it = item_vars.find( name );
    if( it == item_vars.end() ) {
        return default_value;
    }
    return atof( it->second.c_str() );
}

void item::set_var( const std::string &name, const tripoint &value )
{
    item_vars[name] = string_format( "%d,%d,%d", value.x, value.y, value.z );
}

tripoint item::get_var( const std::string &name, const tripoint &default_value ) const
{
    const auto it = item_vars.find( name );
    if( it == item_vars.end() ) {
        return default_value;
    }
    std::vector<std::string> values = string_split( it->second, ',' );
    return tripoint( atoi( values[0].c_str() ),
                     atoi( values[1].c_str() ),
                     atoi( values[2].c_str() ) );
}

void item::set_var( const std::string &name, const std::string &value )
{
    item_vars[name] = value;
}

std::string item::get_var( const std::string &name, const std::string &default_value ) const
{
    const auto it = item_vars.find( name );
    if( it == item_vars.end() ) {
        return default_value;
    }
    return it->second;
}

std::string item::get_var( const std::string &name ) const
{
    return get_var( name, "" );
}

bool item::has_var( const std::string &name ) const
{
    return item_vars.contains( name );
}

void item::erase_var( const std::string &name )
{
    item_vars.erase( name );
}

void item::clear_vars()
{
    item_vars.clear();
}

void item::add_item_with_id( const itype_id &itype, int count )
{
    detached_ptr<item> new_item = item::spawn( itype, calendar::turn, count );
    contents.insert_item( std::move( new_item ) );
}

bool item::has_item_with_id( const itype_id &itype ) const
{
    // shouldn't need to check any deeper than top-level
    std::vector<item *> item_contents = contents.all_items_top();
    for( item *itm : item_contents ) {
        if( itm->typeId() == itype ) {
            return true;
        }
    }
    return false;
}

// TODO: Get rid of, handle multiple types gracefully
static int get_ranged_pierce( const common_ranged_data &ranged )
{
    if( ranged.damage.empty() ) {
        return 0;
    }

    return ranged.damage.damage_units.front().res_pen;
}

static float get_ranged_armor_mult( const common_ranged_data &ranged )
{
    if( ranged.damage.empty() ) {
        return 0.0f;
    }

    return ranged.damage.damage_units.front().res_mult;
}

// Generates a long-form description of the freshness of the given rottable food item.
// NB: Doesn't check for non-rottable!
static std::string get_freshness_description( const item &food_item )
{
    // So, skilled characters looking at food that is neither super-fresh nor about to rot
    // can guess its age as one of {quite fresh,midlife,past midlife,old soon}, and also
    // guess about how long until it spoils.
    const double rot_progress = food_item.get_relative_rot();
    const time_duration shelf_life = food_item.get_shelf_life();
    time_duration time_left = shelf_life - ( shelf_life * rot_progress );

    // Correct for an estimate that exceeds shelf life -- this happens especially with
    // fresh items.
    if( time_left > shelf_life ) {
        time_left = shelf_life;
    }
    avatar &you = get_avatar();
    if( food_item.is_fresh() ) {
        // Fresh food is assumed to be obviously so regardless of skill.
        if( you.can_estimate_rot() ) {
            return string_format( _( "* This food looks as <good>fresh</good> as it can be.  "
                                     "It still has <info>%s</info> until it spoils." ),
                                  to_string_approx( time_left ) );
        } else {
            return _( "* This food looks as <good>fresh</good> as it can be." );
        }
    } else if( food_item.is_going_bad() ) {
        // Old food likewise is assumed to be fairly obvious.
        if( you.can_estimate_rot() ) {
            return string_format( _( "* This food looks <bad>old</bad>.  "
                                     "It's just <info>%s</info> from becoming inedible." ),
                                  to_string_approx( time_left ) );
        } else {
            return _( "* This food looks <bad>old</bad>.  "
                      "It's on the brink of becoming inedible." );
        }
    }

    if( !you.can_estimate_rot() ) {
        // Unskilled characters only get a hint that more information exists...
        return _( "* This food looks <info>fine</info>.  If you were more skilled in "
                  "cooking or survival, you might be able to make a better estimation." );
    }

    // Otherwise, a skilled character can determine the below options:
    if( rot_progress < 0.3 ) {
        //~ here, %s is an approximate time span, e.g., "over 2 weeks" or "about 1 season"
        return string_format( _( "* This food looks <good>quite fresh</good>.  "
                                 "It has <info>%s</info> until it spoils." ),
                              to_string_approx( time_left ) );
    } else if( rot_progress < 0.5 ) {
        //~ here, %s is an approximate time span, e.g., "over 2 weeks" or "about 1 season"
        return string_format( _( "* This food looks like it is reaching its <neutral>midlife</neutral>.  "
                                 "There's <info>%s</info> before it spoils." ),
                              to_string_approx( time_left ) );
    } else if( rot_progress < 0.7 ) {
        //~ here, %s is an approximate time span, e.g., "over 2 weeks" or "about 1 season"
        return string_format( _( "* This food looks like it has <neutral>passed its midlife</neutral>.  "
                                 "Edible, but will go bad in <info>%s</info>." ),
                              to_string_approx( time_left ) );
    } else {
        //~ here, %s is an approximate time span, e.g., "over 2 weeks" or "about 1 season"
        return string_format( _( "* This food looks like it <bad>will be old soon</bad>.  "
                                 "It has <info>%s</info>, so if you plan to use it, it's now or never." ),
                              to_string_approx( time_left ) );
    }
}

item::sizing item::get_sizing( const Character &who ) const
{
    const islot_armor *armor_data = find_armor_data();
    if( !armor_data ) {
        return sizing::ignore;
    }
    bool to_ignore = true;
    for( const armor_portion_data &piece : armor_data->data ) {
        if( piece.encumber != 0 ) {
            to_ignore = false;
        }
    }
    if( to_ignore ) {
        return sizing::ignore;
    } else {
        const bool small = who.get_size() == creature_size::tiny;
        const bool big = who.get_size() == creature_size::huge;

        // due to the iterative nature of these features, something can fit and be undersized/oversized
        // but that is fine because we have separate logic to adjust encumberance per each. One day we
        // may want to have fit be a flag that only applies if a piece of clothing is sized for you as there
        // is a bit of cognitive dissonance when something 'fits' and is 'oversized' and the same time
        const bool undersize = has_flag( flag_UNDERSIZE ) || has_flag( flag_resized_small );
        const bool oversize = has_flag( flag_OVERSIZE ) || has_flag( flag_resized_large );

        if( undersize ) {
            if( small ) {
                return sizing::small_sized_small_char;
            } else if( big ) {
                return sizing::small_sized_big_char;
            } else {
                return sizing::small_sized_human_char;
            }
        } else if( oversize ) {
            if( big ) {
                return sizing::big_sized_big_char;
            } else if( small ) {
                return sizing::big_sized_small_char;
            } else {
                return sizing::big_sized_human_char;
            }
        } else {
            if( big ) {
                return sizing::human_sized_big_char;
            } else if( small ) {
                return sizing::human_sized_small_char;
            } else {
                return sizing::human_sized_human_char;
            }
        }
    }
}

static int get_base_env_resist( const item &it )
{
    const islot_armor *armor = it.find_armor_data();
    if( armor == nullptr ) {
        if( it.is_pet_armor() ) {
            return it.type->pet_armor->env_resist * it.get_relative_health();
        } else {
            return 0;
        }
    }

    return armor->env_resist * it.get_relative_health();
}

bool item::is_owned_by( const Character &c, bool available_to_take ) const
{
    // owner.is_null() implies faction_id( "no_faction" ) which shouldn't happen, or no owner at all.
    // either way, certain situations this means the thing is available to take.
    // in other scenarios we actually really want to check for id == id, even for no_faction
    if( get_owner().is_null() ) {
        return available_to_take;
    }
    if( !c.get_faction() ) {
        debugmsg( "Character %s has no faction", c.disp_name() );
        return false;
    }
    return c.get_faction()->id == get_owner();
}

bool item::is_old_owner( const Character &c, bool available_to_take ) const
{
    if( get_old_owner().is_null() ) {
        return available_to_take;
    }
    if( !c.get_faction() ) {
        debugmsg( "Character %s has no faction.", c.disp_name() );
        return false;
    }
    return c.get_faction()->id == get_old_owner();
}

std::string item::get_owner_name() const
{
    if( !g->faction_manager_ptr->get( get_owner() ) ) {
        debugmsg( "item::get_owner_name() item %s has no valid nor null faction id ", tname() );
        return "no owner";
    }
    return g->faction_manager_ptr->get( get_owner() )->name;
}

void item::set_owner( const Character &c )
{
    if( !c.get_faction() ) {
        debugmsg( "item::set_owner() Character %s has no valid faction", c.disp_name() );
        return;
    }
    owner = c.get_faction()->id;
}

faction_id item::get_owner() const
{
    validate_ownership();
    return owner;
}

faction_id item::get_old_owner() const
{
    validate_ownership();
    return old_owner;
}

void item::validate_ownership() const
{
    if( !old_owner.is_null() && !g->faction_manager_ptr->get( old_owner, false ) ) {
        remove_old_owner();
    }
    if( !owner.is_null() && !g->faction_manager_ptr->get( owner, false ) ) {
        remove_owner();
    }
}

/*
 * 0 based lookup table of accuracy - monster defense converted into number of hits per 10000
 * attacks
 * data painstakingly looked up at http://onlinestatbook.com/2/calculators/normal_dist.html
 */
static const double hits_by_accuracy[41] = {
    0,    1,   2,   3,   7, // -20 to -16
    13,   26,  47,   82,  139, // -15 to -11
    228,   359,  548,  808, 1151, // -10 to -6
    1587, 2119, 2743, 3446, 4207, // -5 to -1
    5000,  // 0
    5793, 6554, 7257, 7881, 8413, // 1 to 5
    8849, 9192, 9452, 9641, 9772, // 6 to 10
    9861, 9918, 9953, 9974, 9987, // 11 to 15
    9993, 9997, 9998, 9999, 10000 // 16 to 20
};

double item::effective_dps( const player &guy, const monster &mon ) const
{
    return effective_dps( guy, mon, melee::default_attack( *this ) );
}

double item::effective_dps( const player &guy, const monster &mon,
                            const attack_statblock &attack ) const
{
    const float mon_dodge = mon.get_dodge();
    // TODO: Handle multiple attacks
    float base_hit = guy.get_dex() / 4.0f + guy.get_hit_weapon( *this, attack );
    base_hit *= std::max( 0.25f, 1.0f - guy.encumb( body_part_torso ) / 100.0f );
    float mon_defense = mon_dodge + mon.size_melee_penalty() / 5.0;
    constexpr double hit_trials = 10000.0;
    const int rng_mean = std::max( std::min( static_cast<int>( base_hit - mon_defense ), 20 ),
                                   -20 ) + 20;
    double num_all_hits = hits_by_accuracy[ rng_mean ];
    /* critical hits have two chances to occur: triple critical hits happen much less frequently,
     * and double critical hits can only occur if a hit roll is more than 1.5 * monster dodge.
     * Not the hit roll used to determine the attack, another one.
     * the way the math works, some percentage of the total hits are eligible to be double
     * critical hits, and the rest are eligible to be triple critical hits, but in each case,
     * only some small percent of them actually become critical hits.
     */
    const int rng_high_mean = std::max( std::min( static_cast<int>( base_hit - 1.5 * mon_dodge ),
                                        20 ), -20 ) + 20;
    double num_high_hits = hits_by_accuracy[ rng_high_mean ] * num_all_hits / hit_trials;
    double double_crit_chance = guy.crit_chance( 4, 0, *this, attack );
    double crit_chance = guy.crit_chance( 0, 0, *this, attack );
    double num_low_hits = std::max( 0.0, num_all_hits - num_high_hits );

    double moves_per_attack = guy.attack_cost( *this );
    // attacks that miss do no damage but take time
    double total_moves = ( hit_trials - num_all_hits ) * moves_per_attack;
    double total_damage = 0.0;
    double num_crits = std::min( num_low_hits * crit_chance + num_high_hits * double_crit_chance,
                                 num_all_hits );
    // critical hits are counted separately
    double num_hits = num_all_hits - num_crits;
    // sum average damage past armor and return the number of moves required to achieve
    // that damage
    // @todo Update for attack_statblock
    const attack_statblock default_attack = melee::default_attack( *this );
    const auto calc_effective_damage = [ &, moves_per_attack]( const double num_strikes,
    const bool crit, const player & guy, const monster & mon ) {
        monster temp_mon( mon );
        double subtotal_damage = 0;
        damage_instance base_damage;
        melee::roll_all_damage( guy, crit, base_damage, true, *this, default_attack );
        damage_instance dealt_damage = base_damage;
        temp_mon.absorb_hit( bodypart_id( "torso" ), dealt_damage );
        dealt_damage_instance dealt_dams;
        for( const damage_unit &dmg_unit : dealt_damage.damage_units ) {
            int cur_damage = 0;
            int total_pain = 0;
            temp_mon.deal_damage_handle_type( dmg_unit, bodypart_id( "torso" ), cur_damage, total_pain );
            if( cur_damage > 0 ) {
                dealt_dams.dealt_dams[ dmg_unit.type ] += cur_damage;
            }
        }
        double damage_per_hit = dealt_dams.total_damage();
        subtotal_damage = damage_per_hit * num_strikes;
        double subtotal_moves = moves_per_attack * num_strikes;

        if( has_technique( rapid_strike ) ) {
            monster temp_rs_mon( mon );
            damage_instance rs_base_damage;
            melee::roll_all_damage( guy, crit, rs_base_damage, true, *this, default_attack );
            damage_instance dealt_rs_damage = rs_base_damage;
            for( damage_unit &dmg_unit : dealt_rs_damage.damage_units ) {
                dmg_unit.damage_multiplier *= 0.66;
            }
            temp_rs_mon.absorb_hit( bodypart_id( "torso" ), dealt_rs_damage );
            dealt_damage_instance rs_dealt_dams;
            for( const damage_unit &dmg_unit : dealt_rs_damage.damage_units ) {
                int cur_damage = 0;
                int total_pain = 0;
                temp_rs_mon.deal_damage_handle_type( dmg_unit, bodypart_id( "torso" ), cur_damage, total_pain );
                if( cur_damage > 0 ) {
                    rs_dealt_dams.dealt_dams[ dmg_unit.type ] += cur_damage;
                }
            }
            double rs_damage_per_hit = rs_dealt_dams.total_damage();
            subtotal_moves *= 0.5;
            subtotal_damage *= 0.5;
            subtotal_moves += moves_per_attack * num_strikes * 0.33;
            subtotal_damage += rs_damage_per_hit * num_strikes * 0.5;
        }
        return std::make_pair( subtotal_moves, subtotal_damage );
    };
    std::pair<double, double> crit_summary = calc_effective_damage( num_crits, true, guy, mon );
    total_moves += crit_summary.first;
    total_damage += crit_summary.second;
    std::pair<double, double> summary = calc_effective_damage( num_hits, false, guy, mon );
    total_moves += summary.first;
    total_damage += summary.second;
    return total_damage * to_moves<double>( 1_seconds ) / total_moves;
}

struct dps_comp_data {
    mtype_id mon_id;
    bool display;
    bool evaluate;
};

static const std::vector<std::pair<translation, dps_comp_data>> dps_comp_monsters = {
    { to_translation( "Best" ), { mtype_id( "debug_mon" ), true, false } },
    { to_translation( "Vs. Agile" ), { mtype_id( "debug_mon_agile" ), true, true } },
    { to_translation( "Vs. Armored" ), { mtype_id( "debug_mon_armored" ), true, true } },
    { to_translation( "Vs. Mixed" ), { mtype_id( "debug_mon_mixed" ), false, true } },
};

std::map<std::string, double> item::dps( const bool for_display, const bool for_calc,
        const player &guy, const attack_statblock &attack ) const
{
    std::map<std::string, double> results;
    for( const std::pair<translation, dps_comp_data> &comp_mon : dps_comp_monsters ) {
        if( ( comp_mon.second.display != for_display ) &&
            ( comp_mon.second.evaluate != for_calc ) ) {
            continue;
        }
        monster test_mon = monster( comp_mon.second.mon_id );
        results[ comp_mon.first.translated() ] = effective_dps( guy, test_mon, attack );
    }
    return results;
}

std::map<std::string, double> item::dps( const bool for_display, const bool for_calc,
        const attack_statblock &attack ) const
{
    return dps( for_display, for_calc, get_avatar(), attack );
}

double item::average_dps( const player &guy, const attack_statblock &attack ) const
{
    const auto &dps_data = dps( false, true, guy, attack );
    const double sum = std::transform_reduce( dps_data.begin(), dps_data.end(),
                       0.0, std::plus{},
                       []( const auto & entry ) -> double { return entry.second; } );

    return sum / dps_data.size();
}

void item::basic_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int batch,
                       bool debug /* debug */ ) const
{
    if( display_mod_source && parts->test( iteminfo_parts::BASE_MOD_SRC ) ) {
        info.emplace_back( "BASE", string_format( _( "<stat>Origin: %s</stat>" ),
                           enumerate_as_string( type->src.begin(),
        type->src.end(), []( const std::pair<itype_id, mod_id> &source ) {
            return string_format( "'%s'", source.second->name() );
        }, enumeration_conjunction::arrow ) ) );
        insert_separation_line( info );
    }
    if( display_object_ids && parts->test( iteminfo_parts::BASE_ID ) ) {
        info.emplace_back( "BASE", colorize( string_format( "[%s]", type->get_id() ), c_light_blue ) );
        insert_separation_line( info );
    }

    const std::string space = "  ";
    if( parts->test( iteminfo_parts::BASE_MATERIAL ) ) {
        const std::vector<const material_type *> mat_types = made_of_types();
        if( !mat_types.empty() ) {
            const std::string material_list = enumerate_as_string( mat_types.begin(), mat_types.end(),
            []( const material_type * material ) {
                return string_format( "<stat>%s</stat>", material->name() );
            }, enumeration_conjunction::none );
            info.emplace_back( "BASE", string_format( _( "Material: %s" ), material_list ) );
        }
    }
    if( parts->test( iteminfo_parts::BASE_VOLUME ) ) {
        int converted_volume_scale = 0;
        const double converted_volume = round_up( convert_volume( volume().value(),
                                        &converted_volume_scale ) * batch, 3 );
        iteminfo::flags f = iteminfo::lower_is_better | iteminfo::no_newline;
        if( converted_volume_scale != 0 ) {
            f |= iteminfo::is_three_decimal;
        }
        info.emplace_back( "BASE", _( "Volume: " ),
                           string_format( "<num> %s", volume_units_abbr() ),
                           f, converted_volume );
    }
    if( parts->test( iteminfo_parts::BASE_WEIGHT ) ) {
        info.emplace_back( "BASE", space + _( "Weight: " ),
                           string_format( "<num> %s", weight_units() ),
                           iteminfo::lower_is_better | iteminfo::is_decimal,
                           convert_weight( weight() ) * batch );
    }
    if( !owner.is_null() ) {
        info.emplace_back( "BASE", string_format( _( "Owner: %s" ),
                           _( get_owner_name() ) ) );
    }
    if( parts->test( iteminfo_parts::BASE_CATEGORY ) ) {
        info.emplace_back( "BASE", _( "Category: " ),
                           "<header>" + get_category().name() + "</header>" );
    }
    if( !type->weapon_category.empty() && parts->test( iteminfo_parts::WEAPON_CATEGORY ) ) {
        const std::string weapon_categories = enumerate_as_string( type->weapon_category.begin(),
        type->weapon_category.end(), [&]( const weapon_category_id & elem ) {
            return elem->name().translated();
        }, enumeration_conjunction::none );
        info.emplace_back( "BASE", _( "Weapon Category: " ),
                           "<header>" + weapon_categories + "</header>" );
    }

    if( parts->test( iteminfo_parts::DESCRIPTION ) ) {
        insert_separation_line( info );
        const std::map<std::string, std::string>::const_iterator idescription =
            item_vars.find( "description" );
        const std::optional<translation> snippet = SNIPPET.get_snippet_by_id( snip_id );
        if( snippet.has_value() && ( !get_avatar().has_trait( trait_ILLITERATE ) ||
                                     !has_flag( flag_SNIPPET_NEEDS_LITERACY ) ) ) {
            // Just use the dynamic description
            info.emplace_back( "DESCRIPTION", snippet.value().translated() );
            // only ever do the effect for a snippet the first time you see it
            if( !get_avatar().has_seen_snippet( snip_id ) ) {
                //note that you have seen the snippet
                get_avatar().add_snippet( snip_id );
            }
        } else if( idescription != item_vars.end() ) {
            info.emplace_back( "DESCRIPTION", idescription->second );
        } else {
            if( is_craft() ) {
                const std::string desc = _( "This is an in progress %s.  "
                                            "It is %d percent complete." );
                const int percent_progress = item_counter / 100000;
                info.emplace_back( "DESCRIPTION", string_format( desc,
                                   craft_data_->making->result_name(),
                                   percent_progress ) );
            } else {
                info.emplace_back( "DESCRIPTION", type->description.translated() );
            }
        }
        std::map<std::string, std::string>::const_iterator item_note = item_vars.find( "item_note" );
        std::map<std::string, std::string>::const_iterator item_note_tool =
            item_vars.find( "item_note_tool" );

        if( item_note != item_vars.end() && parts->test( iteminfo_parts::DESCRIPTION_NOTES ) ) {
            std::string ntext;
            const inscribe_actor *use_actor = nullptr;
            if( item_note_tool != item_vars.end() ) {
                const use_function *use_func = itype_id( item_note_tool->second )->get_use( "inscribe" );
                use_actor = dynamic_cast<const inscribe_actor *>( use_func->get_actor_ptr() );
            }
            if( use_actor ) {
                //~ %1$s: gerund (e.g. carved), %2$s: item name, %3$s: inscription text
                ntext = string_format( pgettext( "carving", "<info>%1$s on the %2$s is:</info> %3$s" ),
                                       use_actor->gerund, tname(), item_note->second );
            } else {
                //~ %1$s: inscription text
                ntext = string_format( pgettext( "carving", "Note: %1$s" ), item_note->second );
            }
            info.emplace_back( "DESCRIPTION", ntext );
        }
        insert_separation_line( info );
    }

    insert_separation_line( info );

    if( parts->test( iteminfo_parts::BASE_REQUIREMENTS ) ) {
        // Display any minimal stat or skill requirements for the item
        std::vector<std::string> req;
        if( get_min_str() > 0 ) {
            avatar &viewer = get_avatar();
            if( has_flag( flag_STR_DRAW ) && ranged::get_str_draw_penalty( *this, viewer ) < 1.0f ) {
                if( ranged::get_str_draw_penalty( *this, viewer ) < 0.5f ) {
                    req.push_back( string_format( _( "%s %d <color_magenta>(Can't use!)</color>" ), _( "strength" ),
                                                  get_min_str() ) );
                } else if( ranged::get_str_draw_penalty( *this, viewer ) < 0.75f ) {
                    req.push_back( string_format( "%s %d <color_red>(Damage/Range 0.5x, Dispersion 2.0x)</color>",
                                                  _( "strength" ), get_min_str() ) );
                } else {
                    req.push_back( string_format( "%s %d <color_yellow>(Damage/Range 0.75x)</color>", _( "strength" ),
                                                  get_min_str() ) );
                }
            } else {
                req.push_back( string_format( "%s %d", _( "strength" ), get_min_str() ) );
            }
        }
        if( type->min_dex > 0 ) {
            req.push_back( string_format( "%s %d", _( "dexterity" ), type->min_dex ) );
        }
        if( type->min_int > 0 ) {
            req.push_back( string_format( "%s %d", _( "intelligence" ), type->min_int ) );
        }
        if( type->min_per > 0 ) {
            req.push_back( string_format( "%s %d", _( "perception" ), type->min_per ) );
        }
        for( const std::pair<const skill_id, int> sk : sorted_lex( type->min_skills ) ) {
            req.push_back( string_format( "%s %d", skill_id( sk.first )->name(), sk.second ) );
        }
        if( !req.empty() ) {
            info.emplace_back( "BASE", _( "<bold>Minimum requirements</bold>:" ) );
            info.emplace_back( "BASE", enumerate_as_string( req ) );
            insert_separation_line( info );
        }
    }

    if( has_var( "contained_name" ) && parts->test( iteminfo_parts::BASE_CONTENTS ) ) {
        info.emplace_back( "BASE", string_format( _( "Contains: %s" ),
                           get_var( "contained_name" ) ) );
    }
    if( count_by_charges() && !is_food() && !is_medication() &&
        parts->test( iteminfo_parts::BASE_AMOUNT ) ) {
        info.emplace_back( "BASE", _( "Amount: " ), "<num>", iteminfo::no_flags,
                           charges * batch );
    }
    if( debug && parts->test( iteminfo_parts::BASE_DEBUG ) ) {
        if( g != nullptr ) {
            info.emplace_back( "BASE", string_format( "itype_id: %s",
                               typeId().c_str() ) );
            info.emplace_back( "BASE", _( "age (hours): " ), "", iteminfo::lower_is_better,
                               to_hours<int>( age() ) );
            info.emplace_back( "BASE", _( "charges: " ), "", iteminfo::lower_is_better,
                               charges );
            info.emplace_back( "BASE", _( "damage: " ), "", iteminfo::lower_is_better,
                               damage_ );
            info.emplace_back( "BASE", _( "active: " ), "", iteminfo::lower_is_better,
                               is_active() );
            info.emplace_back( "BASE", _( "burn: " ), "", iteminfo::lower_is_better,
                               burnt );

            static const auto f = []( const flag_id & f ) -> std::string { return f.str(); };
            const std::string itype_tags_listed = enumerate_as_string( type->item_tags, f,
                                                  enumeration_conjunction::none );
            info.emplace_back( "BASE", string_format( _( "itype tags: %s" ), itype_tags_listed ) );

            const std::string tags_listed = enumerate_as_string( item_tags, f, enumeration_conjunction::none );
            info.emplace_back( "BASE", string_format( _( "tags: %s" ), tags_listed ) );

            for( auto const &imap : item_vars ) {
                info.emplace_back( "BASE",
                                   string_format( _( "item var: %s, %s" ), imap.first,
                                                  imap.second ) );
            }

            const item *food = get_food();
            if( food && food->goes_bad() ) {
                info.emplace_back( "BASE", _( "age (turns): " ),
                                   "", iteminfo::lower_is_better,
                                   to_turns<int>( food->age() ) );
                info.emplace_back( "BASE", _( "rot (turns): " ),
                                   "", iteminfo::lower_is_better,
                                   to_turns<int>( food->rot ) );
                info.emplace_back( "BASE", space + _( "shelf life (turns): " ),
                                   "", iteminfo::lower_is_better,
                                   to_turns<int>( food->get_shelf_life() ) );
                info.emplace_back( "BASE", _( "last rot: " ),
                                   "", iteminfo::lower_is_better,
                                   to_turn<int>( food->last_rot_check ) );
            }
        }
    }
}

void item::med_info( const item *med_item, std::vector<iteminfo> &info, const iteminfo_query *parts,
                     int batch, bool ) const
{
    const cata::value_ptr<islot_comestible> &med_com = med_item->get_comestible();
    if( med_com->quench != 0 && parts->test( iteminfo_parts::MED_QUENCH ) ) {
        info.emplace_back( "MED", _( "Quench: " ), med_com->quench );
    }

    if( med_item->get_comestible_fun() != 0 && parts->test( iteminfo_parts::MED_JOY ) ) {
        info.emplace_back( "MED", _( "Enjoyability: " ),
                           get_avatar().fun_for( *med_item ).first );
    }

    if( med_com->stim != 0 && parts->test( iteminfo_parts::MED_STIMULATION ) ) {
        std::string name = string_format( "%s <stat>%s</stat>", _( "Stimulation:" ),
                                          med_com->stim > 0 ? _( "Upper" ) : _( "Downer" ) );
        info.emplace_back( "MED", name );
    }

    if( parts->test( iteminfo_parts::MED_PORTIONS ) ) {
        info.emplace_back( "MED", _( "Portions: " ),
                           std::abs( static_cast<int>( med_item->charges ) * batch ) );
    }

    if( med_com->addict && parts->test( iteminfo_parts::DESCRIPTION_MED_ADDICTING ) ) {
        info.emplace_back( "DESCRIPTION", _( "* Consuming this item is <bad>addicting</bad>." ) );
    }
}

void item::food_info( const item *food_item, std::vector<iteminfo> &info,
                      const iteminfo_query *parts, int batch, bool debug,
                      temperature_flag temperature ) const
{
    nutrients min_nutr;
    nutrients max_nutr;
    avatar &you = get_avatar();

    std::string recipe_exemplar = get_var( "recipe_exemplar", "" );
    if( recipe_exemplar.empty() ) {
        min_nutr = max_nutr = you.compute_effective_nutrients( *food_item );
    } else {
        std::tie( min_nutr, max_nutr ) =
            you.compute_nutrient_range( *food_item, recipe_id( recipe_exemplar ) );
    }

    bool show_nutr = parts->test( iteminfo_parts::FOOD_NUTRITION ) ||
                     parts->test( iteminfo_parts::FOOD_VITAMINS );
    if( show_nutr && !food_item->has_flag( flag_NUTRIENT_OVERRIDE ) ) {
        info.emplace_back(
            "FOOD", _( "Nutrition will <color_cyan>vary with available ingredients</color>." ) );
        if( recipe_dict.is_item_on_loop( food_item->typeId() ) ) {
            info.emplace_back(
                "FOOD", _( "Nutrition range cannot be calculated accurately due to "
                           "<color_red>recipe loops</color>." ) );
        }
    }

    const std::string space = "  ";
    if( max_nutr.kcal != 0 || food_item->get_comestible()->quench != 0 ) {
        if( parts->test( iteminfo_parts::FOOD_NUTRITION ) ) {
            info.emplace_back( "FOOD", _( "<bold>Calories (kcal)</bold>: " ),
                               "", iteminfo::no_newline, you.compute_effective_nutrients( *food_item ).kcal );
            if( max_nutr.kcal != min_nutr.kcal ) {
                info.emplace_back( "FOOD", _( "-" ),
                                   "", iteminfo::no_newline, you.compute_effective_nutrients( *food_item ).kcal );
            }
        }
        if( parts->test( iteminfo_parts::FOOD_QUENCH ) ) {
            info.emplace_back( "FOOD", space + _( "Quench: " ),
                               food_item->get_comestible()->quench );
        }
    }

    const std::pair<int, int> fun_for_food_item = you.fun_for( *food_item );
    if( fun_for_food_item.first != 0 && parts->test( iteminfo_parts::FOOD_JOY ) ) {
        info.emplace_back( "FOOD", _( "Enjoyability: " ), fun_for_food_item.first );
    }

    if( parts->test( iteminfo_parts::FOOD_PORTIONS ) ) {
        info.emplace_back( "FOOD", _( "Portions: " ),
                           std::abs( static_cast<int>( food_item->charges ) * batch ) );
    }
    if( food_item->corpse != nullptr && parts->test( iteminfo_parts::FOOD_SMELL ) &&
        ( debug || ( g != nullptr && ( you.has_trait( trait_CARNIVORE ) ||
                                       you.has_artifact_with( AEP_SUPER_CLAIRVOYANCE ) ) ) ) ) {
        info.emplace_back( "FOOD", _( "Smells like: " ) + food_item->corpse->nname() );
    }

    auto format_vitamin = [&]( const std::pair<vitamin_id, int> &v, bool display_vitamins ) {
        const bool is_vitamin = v.first->type() == vitamin_type::VITAMIN;
        // only display vitamins that we actually require
        if( you.vitamin_rate( v.first ) == 0_turns || v.second == 0 ||
            display_vitamins != is_vitamin || v.first->has_flag( "NO_DISPLAY" ) ) {
            return std::string();
        }
        const double multiplier = you.vitamin_rate( v.first ) / 1_days * 100;
        const int min_value = min_nutr.get_vitamin( v.first );
        const int max_value = v.second;
        const int min_rda = std::lround( min_value * multiplier );
        const int max_rda = std::lround( max_value * multiplier );
        const std::string format = min_rda == max_rda ? "%s (%i%%)" : "%s (%i-%i%%)";
        return string_format( format, v.first->name(), min_value, max_value );
    };

    const auto max_nutr_vitamins = sorted_lex( max_nutr.vitamins );
    const std::string required_vits = enumerate_as_string(
                                          max_nutr_vitamins.begin(),
                                          max_nutr_vitamins.end(),
    [&]( const std::pair<vitamin_id, int> &v ) {
        return format_vitamin( v, true );
    } );

    const std::string effect_vits = enumerate_as_string(
                                        max_nutr_vitamins.begin(),
                                        max_nutr_vitamins.end(),
    [&]( const std::pair<vitamin_id, int> &v ) {
        return format_vitamin( v, false );
    } );

    if( !required_vits.empty() && parts->test( iteminfo_parts::FOOD_VITAMINS ) ) {
        info.emplace_back( "FOOD", _( "Vitamins (RDA): " ), required_vits );
    }

    if( !effect_vits.empty() && parts->test( iteminfo_parts::FOOD_VIT_EFFECTS ) ) {
        info.emplace_back( "FOOD", _( "Other contents: " ), effect_vits );
    }

    insert_separation_line( info );

    if( you.allergy_type( *food_item ) != morale_type( "morale_null" ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "* This food will cause an <bad>allergic reaction</bad>." ) );
    }

    if( food_item->has_vitamin( vitamin_human_flesh_vitamin ) &&
        parts->test( iteminfo_parts::FOOD_CANNIBALISM ) ) {
        if( !you.has_trait_flag( trait_flag_CANNIBAL ) ) {
            info.emplace_back( "DESCRIPTION",
                               _( "* This food contains <bad>human flesh</bad>." ) );
        } else {
            info.emplace_back( "DESCRIPTION",
                               _( "* This food contains <good>human flesh</good>." ) );
        }
    }

    if( food_item->is_tainted() && parts->test( iteminfo_parts::FOOD_CANNIBALISM ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "* This food is <bad>tainted</bad> and will poison you." ) );
    }

    ///\EFFECT_SURVIVAL >=3 allows detection of poisonous food
    if( food_item->has_flag( flag_HIDDEN_POISON ) && you.get_skill_level( skill_survival ) >= 3 &&
        parts->test( iteminfo_parts::FOOD_POISON ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "* On closer inspection, this appears to be "
                              "<bad>poisonous</bad>." ) );
    }

    ///\EFFECT_SURVIVAL >=5 allows detection of hallucinogenic food
    if( food_item->has_flag( flag_HIDDEN_HALLU ) && you.get_skill_level( skill_survival ) >= 5 &&
        parts->test( iteminfo_parts::FOOD_HALLUCINOGENIC ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "* On closer inspection, this appears to be "
                              "<neutral>hallucinogenic</neutral>." ) );
    }

    if( food_item->goes_bad() && parts->test( iteminfo_parts::FOOD_ROT ) ) {
        const std::string rot_time = to_string_clipped( food_item->get_shelf_life() );
        info.emplace_back( "DESCRIPTION",
                           string_format( _( "* This food is <neutral>perishable</neutral>, "
                                             "and at room temperature has an estimated nominal "
                                             "shelf life of <info>%s</info>." ), rot_time ) );


        if( parts->test( iteminfo_parts::FOOD_ROT_STORAGE ) ) {
            const char *temperature_description;
            bool print_freshness_duration = false;
            // There should be a better way to do this...
            switch( temperature ) {
                case temperature_flag::TEMP_NORMAL:
                case temperature_flag::TEMP_HEATER: {
                    temperature_description = _( "* Current storage conditions <bad>do not</bad> "
                                                 "protect this item from rot." );
                }
                break;
                case temperature_flag::TEMP_FRIDGE:
                case temperature_flag::TEMP_ROOT_CELLAR: {
                    temperature_description = _( "* Current storage conditions <neutral>partially</neutral> "
                                                 "protect this item from rot.  It will stay fresh at least <info>%s</info>." );
                    print_freshness_duration = true;
                }
                break;
                case temperature_flag::TEMP_FREEZER: {
                    temperature_description = _( "* Current storage conditions <good>fully</good> "
                                                 "protect this item from rot.  It will stay fresh indefinitely." );
                }
                break;
                default: {
                    temperature_description = "BUGGED TEMPERATURE INFO";
                }
            }

            if( print_freshness_duration ) {
                time_duration remaining_fresh = food_item->minimum_freshness_duration( temperature );
                std::string time_string = to_string_clipped( remaining_fresh );
                info.emplace_back( "DESCRIPTION", string_format( temperature_description, time_string ) );
            } else {
                info.emplace_back( "DESCRIPTION", temperature_description );
            }
        }

        if( !food_item->rotten() ) {
            info.emplace_back( "DESCRIPTION", get_freshness_description( *food_item ) );
        }

        if( food_item->has_flag( flag_NO_PARASITES ) ) {
            info.emplace_back( "DESCRIPTION",
                               _( "* It seems that deep freezing <good>killed all "
                                  "parasites</good>." ) );
        }
        if( food_item->rotten() ) {
            if( you.has_bionic( bio_digestion ) ) {
                info.emplace_back( "DESCRIPTION",
                                   _( "This food has started to <neutral>rot</neutral>, "
                                      "but <info>your bionic digestion can tolerate "
                                      "it</info>." ) );
            } else if( you.has_trait( trait_SAPROVORE ) ) {
                info.emplace_back( "DESCRIPTION",
                                   _( "This food has started to <neutral>rot</neutral>, "
                                      "but <info>you can tolerate it</info>." ) );
            } else {
                info.emplace_back( "DESCRIPTION",
                                   _( "This food has started to <bad>rot</bad>. "
                                      "<info>Eating</info> it would be a <bad>very bad "
                                      "idea</bad>." ) );
            }
        }
    }
}

void item::magazine_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int /*batch*/,
                          bool /*debug*/ ) const
{
    if( !is_magazine() || has_flag( flag_NO_RELOAD ) ) {
        return;
    }

    if( parts->test( iteminfo_parts::MAGAZINE_CAPACITY ) ) {
        for( const ammotype &at : ammo_types() ) {
            const std::string fmt = string_format( vgettext( "<num> round of %s",
                                                   "<num> rounds of %s", ammo_capacity() ),
                                                   at->name() );
            info.emplace_back( "MAGAZINE", _( "Capacity: " ), fmt, iteminfo::no_flags,
                               ammo_capacity() );
        }
    }
    if( parts->test( iteminfo_parts::MAGAZINE_RELOAD ) ) {
        info.emplace_back( "MAGAZINE", _( "Reload time: " ), _( "<num> moves per round" ),
                           iteminfo::lower_is_better, type->magazine->reload_time );
    }
    insert_separation_line( info );
}

void item::ammo_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int /* batch */,
                      bool /* debug */ ) const
{
    if( is_gun() || !ammo_data() || !parts->test( iteminfo_parts::AMMO_REMAINING_OR_TYPES ) ) {
        return;
    }

    const std::string space = "  ";
    if( ammo_remaining() > 0 ) {
        info.emplace_back( "AMMO", _( "<bold>Ammunition</bold>: " ),
                           ammo_data()->nname( ammo_remaining() ) );
    } else if( is_ammo() ) {
        info.emplace_back( "AMMO", _( "<bold>Ammunition type</bold>: " ), ammo_type()->name() );
    }

    const islot_ammo &ammo = *ammo_data()->ammo;
    if( !ammo.damage.empty() || ammo.force_stat_display ) {
        bool has_flat_dmg = !ammo.damage.empty() && ammo.damage.damage_units.front().amount > 0;
        bool display_flat_dmg = parts->test( iteminfo_parts::AMMO_DAMAGE_VALUE );
        // TODO: Multiple units
        bool has_dmg_multiplier = ammo.damage.damage_units.front().damage_multiplier != 1.0;
        bool display_dmg_multiplier = parts->test( iteminfo_parts::AMMO_DAMAGE_PROPORTIONAL );
        bool didnt_print_dmg = false;

        // TODO: Deduplicate with damage display
        bool has_flat_arpen = get_ranged_pierce( ammo ) != 0;
        bool display_flat_arpen = parts->test( iteminfo_parts::AMMO_DAMAGE_AP );
        bool has_armor_mult = get_ranged_armor_mult( ammo ) != 1.0;
        bool display_armor_mult = parts->test( iteminfo_parts::AMMO_DAMAGE_AP_PROPORTIONAL );

        iteminfo::flags f = ( has_flat_arpen ||
                              has_armor_mult ) ? iteminfo::no_newline : iteminfo::no_flags;
        iteminfo::flags fd = ( has_flat_arpen ||
                               has_armor_mult ) ? iteminfo::no_newline | iteminfo::is_decimal : iteminfo::is_decimal;

        if( has_flat_dmg && has_dmg_multiplier
            && has_dmg_multiplier && display_dmg_multiplier ) {
            info.emplace_back( "AMMO", _( "Damage: " ), "",
                               iteminfo::no_newline, ammo.damage.total_damage() );
            info.emplace_back( "AMMO", "/", "",
                               fd,
                               ammo.damage.damage_units.front().damage_multiplier );
            // Messy ifs...
        } else if( display_dmg_multiplier && has_dmg_multiplier ) {
            info.emplace_back( "AMMO", _( "Damage multiplier: " ), "",
                               fd,
                               ammo.damage.damage_units.front().damage_multiplier );
        } else if( display_flat_dmg && has_flat_dmg ) {
            info.emplace_back( "AMMO", _( "Damage: " ), "",
                               f, ammo.damage.total_damage() );
        } else {
            didnt_print_dmg = true;
        }

        // Ugly, but handles edge cases better than mandatory space
        static const std::string no_space;
        const std::string &maybe_space = didnt_print_dmg ? no_space : space;

        if( has_flat_arpen && display_flat_arpen
            && has_armor_mult && display_armor_mult ) {
            info.emplace_back( "AMMO", maybe_space + _( "Armor-pierce: " ), "",
                               iteminfo::no_newline, get_ranged_pierce( ammo ) );
            info.emplace_back( "AMMO", "/", "",
                               iteminfo::is_decimal | iteminfo::lower_is_better,
                               get_ranged_armor_mult( ammo ) );
        } else if( has_armor_mult && display_armor_mult ) {
            info.emplace_back( "AMMO", maybe_space + _( "Armor multiplier: " ), "",
                               iteminfo::is_decimal | iteminfo::lower_is_better, get_ranged_armor_mult( ammo ) );
        } else if( display_flat_arpen && has_flat_arpen ) {
            info.emplace_back( "AMMO", maybe_space + _( "Armor-pierce: " ), get_ranged_pierce( ammo ) );
        }
        if( parts->test( iteminfo_parts::AMMO_DAMAGE_RANGE ) && ammo.range != 0 ) {
            info.emplace_back( "AMMO", _( "Range: " ), "", iteminfo::no_newline, ammo.shape
                               ? static_cast<int>( ammo.shape->get_range() )
                               : ammo.range );
        }
        if( parts->test( iteminfo_parts::AMMO_DAMAGE_DISPERSION ) && ammo.dispersion != 0 ) {
            info.emplace_back( "AMMO", space + _( "Dispersion: " ), "",
                               iteminfo::lower_is_better, ammo.dispersion );
        }
        if( parts->test( iteminfo_parts::AMMO_DAMAGE_RECOIL ) && ammo.recoil != 0 ) {
            info.emplace_back( "AMMO", _( "Recoil: " ), "",
                               iteminfo::lower_is_better | iteminfo::no_newline, ammo.recoil );
        }
    }

    // TODO: De-hardcode. Have it imply a flag? Just include description_on_item in ammo_effect struct?
    std::vector<std::string> fx;
    if( ammo.shape &&
        parts->test( iteminfo_parts::AMMO_SHAPE ) ) {
        fx.emplace_back( string_format(
                             _( "This ammo will produce effects with the following shape:\n<bold>%s</bold>" ),
                             ammo.shape->get_description() ) );
    }
    if( ammo.ammo_effects.contains( ammo_effect_RECYCLED ) &&
        parts->test( iteminfo_parts::AMMO_FX_RECYCLED ) ) {
        fx.emplace_back(
            _( "This ammo has been <info>hand-loaded</info> and has a <bad>small chance to misfire</bad>." ) );
    }
    if( ammo.ammo_effects.contains( ammo_effect_BLACKPOWDER ) &&
        parts->test( iteminfo_parts::AMMO_FX_BLACKPOWDER ) ) {
        fx.emplace_back(
            _( "This ammo has been loaded with <bad>blackpowder</bad>, and will quickly "
               "clog up most guns, and cause rust if the gun is not cleaned." ) );
    }
    if( ammo.ammo_effects.contains( ammo_effect_NEVER_MISFIRES ) &&
        parts->test( iteminfo_parts::AMMO_FX_CANTMISSFIRE ) ) {
        fx.emplace_back( _( "This ammo <good>never misfires</good>." ) );
    }
    if( ammo.ammo_effects.contains( ammo_effect_INCENDIARY ) &&
        parts->test( iteminfo_parts::AMMO_FX_INCENDIARY ) ) {
        fx.emplace_back( _( "This ammo <neutral>starts fires</neutral>." ) );
    }
    if( !fx.empty() ) {
        insert_separation_line( info );
        for( const std::string &e : fx ) {
            info.emplace_back( "AMMO", e );
        }
    }
}

namespace
{
auto nname( const itype_id &id ) -> std::string
{
    return item::nname( id );
}
} // namespace

void item::gun_info( const item *mod, std::vector<iteminfo> &info, const iteminfo_query *parts,
                     int /* batch */, bool /* debug */ ) const
{
    const islot_gun &gun = *mod->type->gun;
    const Skill &skill = *mod->gun_skill();
    avatar &viewer = get_avatar();

    // many statistics are dependent upon loaded ammo
    // if item is unloaded (or is RELOAD_AND_SHOOT) shows approximate stats using default ammo
    const item *loaded_mod = mod;
    if( mod->ammo_required() && !mod->ammo_remaining() ) {
        item &tmp = *item::spawn_temporary( *mod );
        tmp.ammo_set( mod->magazine_current() ? tmp.common_ammo_default() : tmp.ammo_default() );
        if( tmp.ammo_data() == nullptr ) {
            insert_separation_line( info );
            info.emplace_back( "GUN",
                               _( "Weapon <bad>can't be loaded in its current state</bad>." ) );
            return;
        }
        loaded_mod = &tmp;
        if( parts->test( iteminfo_parts::GUN_DEFAULT_AMMO ) ) {
            insert_separation_line( info );
            info.emplace_back( "GUN",
                               _( "Weapon is <bad>not loaded</bad>, so stats below assume the default ammo: " ),
                               string_format( "<stat>%s</stat>",
                                              loaded_mod->ammo_data()->nname( 1 ) ) );
        }
    }

    const itype *curammo = loaded_mod->ammo_data();
    if( mod->ammo_required() && !curammo ) {
        debugmsg( "curammo is nullptr in item::gun_info()" );
        return;
    }
    damage_unit gun_du = gun_damage( false ).damage_units.front();

    gun_du.damage_multiplier *= ranged::str_draw_damage_modifier( *mod, viewer );

    damage_unit thrown_du = damage_unit( DT_STAB, 0 );

    damage_unit ammo_du = curammo != nullptr
                          ? curammo->ammo->damage.damage_units.front()
                          : damage_unit( DT_STAB, 0 );

    if( skill.ident() == skill_throw && curammo != nullptr ) {
        item &tmp = *item::spawn_temporary( item( curammo ) );

        thrown_du.amount += ranged::throw_damage( tmp,
                            get_avatar().get_skill_level( skill_throw ),
                            get_avatar().get_str() );
    }

    if( parts->test( iteminfo_parts::GUN_DAMAGE ) ) {
        insert_separation_line( info );
        info.emplace_back( "GUN", _( "<bold>Ranged damage</bold>: " ), "", iteminfo::no_newline,
                           gun_du.amount );
    }

    if( mod->ammo_required() ) {
        // ammo_damage, sum_of_damage, and ammo_mult not shown so don't need to translate.
        if( parts->test( iteminfo_parts::GUN_DAMAGE_LOADEDAMMO ) ) {
            assert( curammo ); // Appease clang-tidy
            damage_instance ammo_dam = curammo->ammo->damage;
            info.emplace_back( "GUN", "ammo_damage", "",
                               iteminfo::no_newline | iteminfo::no_name |
                               iteminfo::show_plus, std::max( ammo_du.amount, thrown_du.amount ) );
        }

        if( parts->test( iteminfo_parts::GUN_DAMAGE_TOTAL ) ) {
            // Intentionally not using total_damage() as it applies multipliers
            info.emplace_back( "GUN", "sum_of_damage", _( " = <num>" ),
                               iteminfo::no_newline | iteminfo::no_name,
                               gun_du.amount + std::max( ammo_du.amount, thrown_du.amount ) );
        }
    }
    info.back().bNewLine = true;
    avatar &you = get_avatar();
    int max_gun_range = loaded_mod->gun_range( &you );
    if( max_gun_range > 0 && parts->test( iteminfo_parts::GUN_MAX_RANGE ) ) {
        info.emplace_back( "GUN", _( "Maximum range: " ), "<num>", iteminfo::no_flags,
                           max_gun_range );
    }

    // TODO: This doesn't cover multiple damage types
    if( parts->test( iteminfo_parts::GUN_ARMORPIERCE ) ) {
        info.emplace_back( "GUN", _( "Armor-pierce: " ), "",
                           iteminfo::no_newline, get_ranged_pierce( gun ) );
    }
    if( mod->ammo_required() ) {
        assert( curammo ); // Appease clang-tidy
        int ammo_pierce = get_ranged_pierce( *curammo->ammo );
        // ammo_armor_pierce and sum_of_armor_pierce don't need to translate.
        if( parts->test( iteminfo_parts::GUN_ARMORPIERCE_LOADEDAMMO ) ) {
            info.emplace_back( "GUN", "ammo_armor_pierce", "",
                               iteminfo::no_newline | iteminfo::no_name |
                               iteminfo::show_plus, ammo_pierce );
        }
        if( parts->test( iteminfo_parts::GUN_ARMORPIERCE_TOTAL ) ) {
            info.emplace_back( "GUN", "sum_of_armor_pierce", _( " = <num>" ),
                               iteminfo::no_name,
                               get_ranged_pierce( gun ) + ammo_pierce );
        }
    }
    info.back().bNewLine = true;

    if( gun_du.damage_multiplier != 1.0 || ammo_du.damage_multiplier != 1.0 ) {
        if( parts->test( iteminfo_parts::GUN_DAMAGEMULT ) ) {
            info.emplace_back( "GUN", _( "Damage multiplier: " ), "",
                               iteminfo::no_newline | iteminfo::is_decimal,
                               gun_du.damage_multiplier );
        }

        if( mod->ammo_required() ) {
            if( parts->test( iteminfo_parts::GUN_DAMAGEMULT_AMMO ) ) {
                info.emplace_back( "GUN", "ammo_mult", "*",
                                   iteminfo::no_newline | iteminfo::no_name | iteminfo::is_decimal,
                                   ammo_du.damage_multiplier );
            }

            if( parts->test( iteminfo_parts::GUN_DAMAGEMULT_TOTAL ) ) {
                info.emplace_back( "GUN", "sum_of_damage", _( " = <num>" ),
                                   iteminfo::no_newline | iteminfo::no_name | iteminfo::is_decimal,
                                   gun_du.damage_multiplier * ammo_du.damage_multiplier );
            }
        }
        info.back().bNewLine = true;
    }

    if( gun_du.res_mult != 1.0 || ammo_du.res_mult != 1.0 ) {
        if( parts->test( iteminfo_parts::GUN_ARMORMULT ) ) {
            info.emplace_back( "GUN", _( "Armor multiplier: " ), "",
                               iteminfo::no_newline | iteminfo::lower_is_better | iteminfo::is_decimal,
                               gun_du.res_mult );
        }
        if( mod->ammo_required() ) {
            if( parts->test( iteminfo_parts::GUN_ARMORMULT_LOADEDAMMO ) ) {
                info.emplace_back( "GUN", "ammo_armor_mult", _( "*<num>" ),
                                   iteminfo::no_newline | iteminfo::no_name |
                                   iteminfo::lower_is_better | iteminfo::is_decimal,
                                   ammo_du.res_mult );
            }
            if( parts->test( iteminfo_parts::GUN_ARMORMULT_TOTAL ) ) {
                info.emplace_back( "GUN", "final_armor_mult", _( " = <num>" ),
                                   iteminfo::no_name | iteminfo::lower_is_better | iteminfo::is_decimal,
                                   gun_du.res_mult * ammo_du.res_mult );
            }
        }
        info.back().bNewLine = true;
    }

    if( parts->test( iteminfo_parts::GUN_DISPERSION ) ) {
        info.emplace_back( "GUN", _( "Dispersion: " ), "",
                           iteminfo::no_newline | iteminfo::lower_is_better,
                           mod->gun_dispersion( false, false ) );
    }
    if( mod->ammo_required() ) {
        int ammo_dispersion = curammo->ammo->dispersion;
        // ammo_dispersion and sum_of_dispersion don't need to translate.
        if( parts->test( iteminfo_parts::GUN_DISPERSION_LOADEDAMMO ) ) {
            info.emplace_back( "GUN", "ammo_dispersion", "",
                               iteminfo::no_newline | iteminfo::lower_is_better |
                               iteminfo::no_name | iteminfo::show_plus,
                               ammo_dispersion );
        }
        if( parts->test( iteminfo_parts::GUN_DISPERSION_TOTAL ) ) {
            info.emplace_back( "GUN", "sum_of_dispersion", _( " = <num>" ),
                               iteminfo::lower_is_better | iteminfo::no_name | iteminfo::no_newline,
                               loaded_mod->gun_dispersion( true, false ) );
            info.emplace_back( "GUN", "eff_dispersion", _( " (effective: <num>)" ),
                               iteminfo::lower_is_better | iteminfo::no_name,
                               static_cast<int>( ranged::get_weapon_dispersion( you, *this ).max() ) );
        }
    }
    info.back().bNewLine = true;

    // if effective sight dispersion differs from actual sight dispersion display both
    int act_disp = mod->sight_dispersion();
    int eff_disp = ranged::effective_dispersion( you, act_disp );
    int adj_disp = eff_disp - act_disp;

    if( parts->test( iteminfo_parts::GUN_DISPERSION_SIGHT ) ) {
        info.emplace_back( "GUN", _( "Sight dispersion: " ), "",
                           iteminfo::no_newline | iteminfo::lower_is_better,
                           act_disp );

        if( adj_disp ) {
            info.emplace_back( "GUN", "sight_adj_disp", "",
                               iteminfo::no_newline | iteminfo::lower_is_better |
                               iteminfo::no_name | iteminfo::show_plus, adj_disp );
            info.emplace_back( "GUN", "sight_eff_disp", _( " = <num>" ),
                               iteminfo::lower_is_better | iteminfo::no_name,
                               eff_disp );
        }
    }

    bool bipod = mod->has_flag( flag_BIPOD );

    if( loaded_mod->gun_recoil() ) {
        if( parts->test( iteminfo_parts::GUN_RECOIL_PERCENTAGE ) ) {
            info.emplace_back( "GUN", _( "Recoil percentage: " ), "",
                               iteminfo::no_newline | iteminfo::lower_is_better,
                               std::ceil( 100 * loaded_mod->gun_recoil_multiplier() ) );
        }
        if( bipod && parts->test( iteminfo_parts::GUN_RECOIL_PERCENTAGE_BIPOD ) ) {
            info.emplace_back( "GUN", "bipod_recoil_percentage", _( " (with bipod <num>)" ),
                               iteminfo::lower_is_better | iteminfo::no_name,
                               std::ceil( 100 * loaded_mod->gun_recoil_multiplier( true ) ) );
        }
        info.back().bNewLine = true;

        if( parts->test( iteminfo_parts::GUN_RECOIL ) ) {
            info.emplace_back( "GUN", _( "Effective recoil: " ), "",
                               iteminfo::no_newline | iteminfo::lower_is_better,
                               loaded_mod->gun_recoil() );
        }
        if( bipod && parts->test( iteminfo_parts::GUN_RECOIL_BIPOD ) ) {
            info.emplace_back( "GUN", "bipod_recoil", _( " (with bipod <num>)" ),
                               iteminfo::lower_is_better | iteminfo::no_name,
                               loaded_mod->gun_recoil( true ) );
        }
    }
    info.back().bNewLine = true;

    std::map<gun_mode_id, gun_mode> fire_modes = mod->gun_all_modes();
    if( parts->test( iteminfo_parts::GUN_BURST_PENALTY ) ) {
        if( std::ranges::any_of( fire_modes,
        []( const std::pair<gun_mode_id, gun_mode> &e ) {
        return e.second.qty > 1 && !e.second.melee();
        } ) ) {
            info.emplace_back( "GUN", _( "Burst fire penalty: " ), "",
                               iteminfo::no_newline | iteminfo::lower_is_better,
                               ranged::burst_penalty( you, *this, loaded_mod->gun_recoil() ) );
            if( bipod ) {
                info.emplace_back( "GUN", "bipod_burst", _( " (with bipod <num>)" ),
                                   iteminfo::lower_is_better | iteminfo::no_name,
                                   ranged::burst_penalty( you, *this, loaded_mod->gun_recoil( true ) ) );
            }
        }
        info.back().bNewLine = true;
    }

    if( parts->test( iteminfo_parts::GUN_RELOAD_TIME ) ) {
        info.emplace_back( "GUN", _( "Reload time: " ),
                           has_flag( flag_RELOAD_ONE ) ? _( "<num> moves per round" ) :
                           _( "<num> moves " ),
                           iteminfo::lower_is_better,  mod->get_reload_time() );
    }

    if( parts->test( iteminfo_parts::GUN_USEDSKILL ) ) {
        info.emplace_back( "GUN", _( "Skill used: " ),
                           "<info>" + skill.name() + "</info>" );
    }

    if( mod->magazine_integral() || mod->magazine_current() ) {
        if( mod->magazine_current() && parts->test( iteminfo_parts::GUN_MAGAZINE ) ) {
            info.emplace_back( "GUN", _( "Magazine: " ),
                               string_format( "<stat>%s</stat>",
                                              mod->magazine_current()->tname() ) );
        }
        if( mod->ammo_capacity() && parts->test( iteminfo_parts::GUN_CAPACITY ) ) {
            for( const ammotype &at : mod->ammo_types() ) {
                const std::string fmt = string_format( vgettext( "<num> round of %s",
                                                       "<num> rounds of %s",
                                                       mod->ammo_capacity() ), at->name() );
                info.emplace_back( "GUN", _( "Capacity: " ), fmt, iteminfo::no_flags,
                                   mod->ammo_capacity() );
            }
        }
    } else if( parts->test( iteminfo_parts::GUN_TYPE ) ) {
        info.emplace_back( "GUN", _( "Type: " ), enumerate_as_string( mod->ammo_types().begin(),
        mod->ammo_types().end(), []( const ammotype & at ) {
            return at->name();
        }, enumeration_conjunction::none ) );
    }

    if( mod->ammo_data() && parts->test( iteminfo_parts::AMMO_REMAINING ) ) {
        info.emplace_back( "AMMO", _( "Ammunition: " ), string_format( "<stat>%s</stat>",
                           mod->ammo_data()->nname( mod->ammo_remaining() ) ) );
    }

    if( mod->ammo_required() > 1 ) {
        info.emplace_back( "AMMO", string_format( "Uses <stat>%i</stat> ammo per shot",
                           mod->ammo_required() ) );
    }

    if( mod->get_gun_ups_drain() && parts->test( iteminfo_parts::AMMO_UPSCOST ) ) {
        info.emplace_back( "AMMO",
                           string_format( vgettext( "Uses <stat>%i</stat> charge of UPS per shot",
                                          "Uses <stat>%i</stat> charges of UPS per shot",
                                          mod->get_gun_ups_drain() ),
                                          mod->get_gun_ups_drain() ) );
    }

    if( skill.ident() == skill_throw ) {
        info.emplace_back( "GUN",
                           _( "Damage/range will vary with <info>throwing skill and ammo.</info>" ) );
    }

    if( parts->test( iteminfo_parts::GUN_AIMING_STATS ) ) {
        insert_separation_line( info );
        info.emplace_back( "GUN", _( "<bold>Base aim speed</bold>: " ), "<num>", iteminfo::no_flags,
                           ranged::aim_per_move( you, *mod, MAX_RECOIL ) );
        for( const ranged::aim_type &type : ranged::get_aim_types( you, *mod ) ) {
            // Nameless aim levels don't get an entry.
            if( type.name.empty() ) {
                continue;
            }
            // For item comparison to work correctly each info object needs a
            // distinct tag per aim type.
            const std::string tag = "GUN_" + type.name;
            info.emplace_back( tag, string_format( "<info>%s</info>", type.name ) );
            int max_dispersion = ranged::get_weapon_dispersion( you, *loaded_mod ).max();
            int range = range_with_even_chance_of_good_hit( max_dispersion + type.threshold );
            info.emplace_back( tag, _( "Even chance of good hit at range: " ),
                               _( "<num>" ), iteminfo::no_flags, range );
            int aim_mv = ranged::gun_engagement_moves( you, *mod, type.threshold );
            info.emplace_back( tag, _( "Time to reach aim level: " ), _( "<num> moves " ),
                               iteminfo::lower_is_better, aim_mv );
        }
    }

    if( parts->test( iteminfo_parts::GUN_FIRE_MODES ) ) {
        std::vector<std::string> fm;
        for( const std::pair<const gun_mode_id, gun_mode> &e : fire_modes ) {
            if( e.second.target == this && !e.second.melee() ) {
                fm.emplace_back( string_format( "%s (%i)", e.second.tname(), e.second.qty ) );
            }
        }
        if( !fm.empty() ) {
            insert_separation_line( info );
            info.emplace_back( "GUN", _( "<bold>Fire modes</bold>: " ) +
                               enumerate_as_string( fm ) );
        }
    }

    if( !magazine_integral() && parts->test( iteminfo_parts::GUN_ALLOWED_MAGAZINES ) ) {
        const auto &compat = magazine_compatible();
        if( !compat.empty() ) {
            insert_separation_line( info );
            info.emplace_back( "DESCRIPTION", _( "<bold>Compatible magazines</bold>: " )
                               + enumerate_as_string( compat, ::nname ) );
        }
    }

    if( !gun.valid_mod_locations.empty() && parts->test( iteminfo_parts::DESCRIPTION_GUN_MODS ) ) {
        insert_separation_line( info );

        std::string mod_str = _( "<bold>Mods</bold>: " );

        std::map<gunmod_location, int> mod_locations = get_mod_locations();

        int iternum = 0;
        for( std::pair<const gunmod_location, int> &elem : mod_locations ) {
            if( iternum != 0 ) {
                mod_str += "; ";
            }
            const int free_slots = ( elem ).second - get_free_mod_locations( elem.first );
            mod_str += string_format( "<bold>%d/%d</bold> %s", free_slots,  elem.second,
                                      elem.first.name() );
            bool first_mods = true;
            for( const item *gmod : gunmods() ) {
                if( gmod->type->gunmod->location == ( elem ).first ) { // if mod for this location
                    if( first_mods ) {
                        mod_str += ": ";
                        first_mods = false;
                    } else {
                        mod_str += ", ";
                    }
                    mod_str += string_format( "<stat>%s</stat>", gmod->tname() );
                }
            }
            iternum++;
        }
        mod_str += ".";
        info.emplace_back( "DESCRIPTION", mod_str );
    }

    if( mod->casings_count() && parts->test( iteminfo_parts::DESCRIPTION_GUN_CASINGS ) ) {
        insert_separation_line( info );
        std::string tmp = vgettext( "Contains <stat>%i</stat> casing",
                                    "Contains <stat>%i</stat> casings", mod->casings_count() );
        info.emplace_back( "DESCRIPTION", string_format( tmp, mod->casings_count() ) );
    }
}

void item::gunmod_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int /* batch */,
                        bool /* debug */ ) const
{
    if( !is_gunmod() ) {
        return;
    }
    const islot_gunmod &mod = *type->gunmod;

    if( is_gun() && parts->test( iteminfo_parts::DESCRIPTION_GUNMOD ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "This mod <info>must be attached to a gun</info>, "
                              "it can not be fired separately." ) );
    }
    if( has_flag( flag_REACH_ATTACK ) && parts->test( iteminfo_parts::DESCRIPTION_GUNMOD_REACH ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "When attached to a gun, <good>allows</good> making "
                              "<info>reach melee attacks</info> with it." ) );
    }
    if( mod.dispersion != 0 && parts->test( iteminfo_parts::GUNMOD_DISPERSION ) ) {
        info.emplace_back( "GUNMOD", _( "Dispersion modifier: " ), "",
                           iteminfo::lower_is_better | iteminfo::show_plus,
                           mod.dispersion );
    }
    if( mod.sight_dispersion != -1 && parts->test( iteminfo_parts::GUNMOD_DISPERSION_SIGHT ) ) {
        info.emplace_back( "GUNMOD", _( "Sight dispersion: " ), "",
                           iteminfo::lower_is_better, mod.sight_dispersion );
    }
    if( mod.aim_speed >= 0 && parts->test( iteminfo_parts::GUNMOD_AIMSPEED ) ) {
        info.emplace_back( "GUNMOD", _( "Aim speed: " ), "",
                           iteminfo::lower_is_better, mod.aim_speed );
    }
    int total_damage = static_cast<int>( mod.damage.total_damage() );
    if( total_damage != 0 && parts->test( iteminfo_parts::GUNMOD_DAMAGE ) ) {
        info.emplace_back( "GUNMOD", _( "Damage: " ), "", iteminfo::show_plus,
                           total_damage );
    }
    int pierce = get_ranged_pierce( mod );
    if( get_ranged_pierce( mod ) != 0 && parts->test( iteminfo_parts::GUNMOD_ARMORPIERCE ) ) {
        info.emplace_back( "GUNMOD", _( "Armor-pierce: " ), "", iteminfo::show_plus,
                           pierce );
    }
    if( mod.handling != 0 && parts->test( iteminfo_parts::GUNMOD_HANDLING ) ) {
        info.emplace_back( "GUNMOD", _( "Handling modifier: " ), "",
                           iteminfo::show_plus, mod.handling );
    }
    if( mod.range != 0 && parts->test( iteminfo_parts::GUNMOD_RANGE ) ) {
        info.emplace_back( "GUNMOD", _( "Range modifier: " ), "",
                           iteminfo::show_plus, mod.range );
    }
    if( !type->mod->ammo_modifier.empty() && parts->test( iteminfo_parts::GUNMOD_AMMO ) ) {
        for( const ammotype &at : type->mod->ammo_modifier ) {
            info.emplace_back( "GUNMOD", string_format( _( "Ammo: <stat>%s</stat>" ),
                               at->name() ) );
        }
    }
    if( mod.reload_modifier != 0 && parts->test( iteminfo_parts::GUNMOD_RELOAD ) ) {
        info.emplace_back( "GUNMOD", _( "Reload modifier: " ), _( "<num>%" ),
                           iteminfo::lower_is_better, mod.reload_modifier );
    }
    if( mod.min_str_required_mod > 0 && parts->test( iteminfo_parts::GUNMOD_STRENGTH ) ) {
        info.emplace_back( "GUNMOD", _( "Minimum strength required modifier: " ),
                           mod.min_str_required_mod );
    }
    if( !mod.add_mod.empty() && parts->test( iteminfo_parts::GUNMOD_ADD_MOD ) ) {
        insert_separation_line( info );

        std::string mod_loc_str = _( "<bold>Adds mod locations: </bold> " );

        std::map<gunmod_location, int> mod_locations = mod.add_mod;

        int iternum = 0;
        for( std::pair<const gunmod_location, int> &elem : mod_locations ) {
            if( iternum != 0 ) {
                mod_loc_str += "; ";
            }
            mod_loc_str += string_format( "<bold>%s</bold> %s", elem.second, elem.first.name() );
            iternum++;
        }
        mod_loc_str += ".";
        info.emplace_back( "GUNMOD", mod_loc_str );
    }

    insert_separation_line( info );

    if( parts->test( iteminfo_parts::GUNMOD_USEDON ) ) {
        std::string used_on_str = _( "<bold>Used on:</bold>" );

        if( !mod.usable.empty() ) {
            used_on_str += _( "\n  Specific: " ) + enumerate_as_string( mod.usable.begin(),
            mod.usable.end(), []( const itype_id & used_on ) {
                return string_format( "<info>%s</info>", used_on->nname( 1 ) );
            } );
        }

        if( !mod.usable_category.empty() ) {
            used_on_str += _( "\n  Category: " );
            std::vector<std::string> combination;
            combination.reserve( mod.usable_category.size() );
            for( const std::unordered_set<weapon_category_id> &catgroup : mod.usable_category ) {
                combination.emplace_back( ( "[" ) + enumerate_as_string( catgroup.begin(),
                catgroup.end(), []( const weapon_category_id & wcid ) {
                    return string_format( "<info>%s</info>", wcid->name().translated() );
                }, enumeration_conjunction::none ) + ( "]" ) );
            }
            used_on_str += enumerate_as_string( combination, enumeration_conjunction::or_ );
        }

        info.emplace_back( "GUNMOD", used_on_str );
    }

    if( !( mod.exclusion.empty() && mod.exclusion_category.empty() ) &&
        parts->test( iteminfo_parts::GUNMOD_EXCLUSION ) ) {
        std::string exclusion_str = _( "<bold>Cannot be used on:</bold>" );

        if( !mod.exclusion.empty() ) {
            exclusion_str += _( "\n  Specific: " ) + enumerate_as_string( mod.exclusion.begin(),
            mod.exclusion.end(), []( const itype_id & excluded ) {
                return string_format( "<info>%s</info>", excluded->nname( 1 ) );
            } );
        }

        if( !mod.exclusion_category.empty() ) {
            exclusion_str += _( "\n  Category: " );
            std::vector<std::string> combination;
            combination.reserve( mod.exclusion_category.size() );
            for( const std::unordered_set<weapon_category_id> &catgroup : mod.exclusion_category ) {
                combination.emplace_back( ( "[" ) + enumerate_as_string( catgroup.begin(),
                catgroup.end(), []( const weapon_category_id & wcid ) {
                    return string_format( "<info>%s</info>", wcid->name().translated() );
                }, enumeration_conjunction::none ) + ( "]" ) );
            }
            exclusion_str += enumerate_as_string( combination, enumeration_conjunction::or_ );
        }

        info.emplace_back( "GUNMOD", exclusion_str );
    }

    if( parts->test( iteminfo_parts::GUNMOD_LOCATION ) ) {
        info.emplace_back( "GUNMOD", string_format( _( "Location: %s" ),
                           mod.location.name() ) );
    }

    if( !mod.blacklist_mod.empty() && parts->test( iteminfo_parts::GUNMOD_BLACKLIST_MOD ) ) {
        std::string mod_black_str = _( "<bold>Incompatible with mod location: </bold> " );

        int iternum = 0;
        for( const gunmod_location &black : mod.blacklist_mod ) {
            if( iternum != 0 ) {
                mod_black_str += ", ";
            }
            mod_black_str += string_format( "%s", black.name() );
            iternum++;
        }
        mod_black_str += ".";
        info.emplace_back( "GUNMOD", mod_black_str );
    }
}

void item::armor_protection_info( std::vector<iteminfo> &info, const iteminfo_query *parts,
                                  int /*batch*/,
                                  bool /*debug*/ ) const
{
    if( !is_armor() && !is_pet_armor() ) {
        return;
    }

    const std::string space = "  ";

    if( parts->test( iteminfo_parts::ARMOR_PROTECTION ) ) {
        info.emplace_back( "ARMOR", _( "<bold>Protection</bold>: Bash: " ), "",
                           iteminfo::no_newline, bash_resist() );
        info.emplace_back( "ARMOR", space + _( "Cut: " ), "", iteminfo::no_newline, cut_resist() );
        info.emplace_back( "ARMOR", space + _( "Ballistic: " ), bullet_resist() );
        info.emplace_back( "ARMOR", space + _( "Acid: " ), "",
                           iteminfo::no_newline, acid_resist() );
        info.emplace_back( "ARMOR", space + _( "Fire: " ), "",
                           iteminfo::no_newline, fire_resist() );
        info.emplace_back( "ARMOR", space + _( "Environmental: " ),
                           get_base_env_resist( *this ) );
        if( type->can_use( "GASMASK" ) || type->can_use( "DIVE_TANK" ) ) {
            info.emplace_back( "ARMOR",
                               _( "<bold>Protection when active</bold>: " ) );
            info.emplace_back( "ARMOR", space + _( "Acid: " ), "",
                               iteminfo::no_newline,
                               acid_resist( false, get_base_env_resist_w_filter() ) );
            info.emplace_back( "ARMOR", space + _( "Fire: " ), "",
                               iteminfo::no_newline,
                               fire_resist( false, get_base_env_resist_w_filter() ) );
            info.emplace_back( "ARMOR", space + _( "Environmental: " ),
                               get_env_resist( get_base_env_resist_w_filter() ) );
        }

        if( damage() > 0 ) {
            info.emplace_back( "ARMOR",
                               _( "Protection values are <bad>reduced by damage</bad> and "
                                  "you may be able to <info>improve them by repairing this "
                                  "item</info>." ) );
        }
    }
}

void item::animal_armor_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int batch,
                              bool debug ) const
{
    if( !is_pet_armor() ) {
        return;
    }

    const std::string space = "  ";

    int converted_storage_scale = 0;
    const double converted_storage = round_up( convert_volume( get_storage().value(),
                                     &converted_storage_scale ), 2 );
    if( parts->test( iteminfo_parts::ARMOR_STORAGE ) && converted_storage > 0 ) {
        const iteminfo::flags f = converted_storage_scale == 0 ? iteminfo::no_flags : iteminfo::is_decimal;
        info.emplace_back( "ARMOR", space + _( "Storage: " ),
                           string_format( "<num> %s", volume_units_abbr() ),
                           f, converted_storage );
    }

    // Whatever the last entry was, we want a newline at this point
    info.back().bNewLine = true;

    armor_protection_info( info, parts, batch, debug );
}

void item::armor_fit_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int /*batch*/,
                           bool /*debug*/ ) const
{
    if( !is_armor() ) {
        return;
    }

    avatar &you = get_avatar();
    const sizing sizing_level = get_sizing( you );

    if( has_flag( flag_HELMET_COMPAT ) &&
        parts->test( iteminfo_parts::DESCRIPTION_FLAGS_HELMETCOMPAT ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "* This item can be <info>worn with a "
                              "helmet</info>." ) );
    }

    if( parts->test( iteminfo_parts::DESCRIPTION_FLAGS_FITS ) ) {
        switch( sizing_level ) {
            case sizing::human_sized_human_char:
                if( has_flag( flag_FIT ) ) {
                    info.emplace_back( "DESCRIPTION",
                                       _( "* This clothing <info>fits</info> you perfectly." ) );
                }
                break;
            case sizing::big_sized_big_char:
                if( has_flag( flag_FIT ) ) {
                    info.emplace_back( "DESCRIPTION", _( "* This clothing <info>fits</info> "
                                                         "your large frame perfectly." ) );
                }
                break;
            case sizing::small_sized_small_char:
                if( has_flag( flag_FIT ) ) {
                    info.emplace_back( "DESCRIPTION", _( "* This clothing <info>fits</info> "
                                                         "your small frame perfectly." ) );
                }
                break;
            case sizing::big_sized_human_char:
                info.emplace_back( "DESCRIPTION", _( "* This clothing is <bad>oversized</bad> "
                                                     "and does <bad>not fit</bad> you." ) );
                break;
            case sizing::big_sized_small_char:
                info.emplace_back( "DESCRIPTION",
                                   _( "* This clothing is hilariously <bad>oversized</bad> "
                                      "and does <bad>not fit</bad> your <info>abnormally "
                                      "small mutated anatomy</info>." ) );
                break;
            case sizing::human_sized_big_char:
                info.emplace_back( "DESCRIPTION",
                                   _( "* This clothing is <bad>normal sized</bad> and does "
                                      "<bad>not fit</info> your <info>abnormally large "
                                      "mutated anatomy</info>." ) );
                break;
            case sizing::human_sized_small_char:
                info.emplace_back( "DESCRIPTION",
                                   _( "* This clothing is <bad>normal sized</bad> and does "
                                      "<bad>not fit</bad> your <info>abnormally small "
                                      "mutated anatomy</info>." ) );
                break;
            case sizing::small_sized_big_char:
                info.emplace_back( "DESCRIPTION",
                                   _( "* This clothing is hilariously <bad>undersized</bad> "
                                      "and does <bad>not fit</bad> your <info>abnormally "
                                      "large mutated anatomy</info>." ) );
                break;
            case sizing::small_sized_human_char:
                info.emplace_back( "DESCRIPTION", _( "* This clothing is <bad>undersized</bad> "
                                                     "and does <bad>not fit</bad> you." ) );
                break;
            default:
                break;
        }
    }

    if( parts->test( iteminfo_parts::DESCRIPTION_FLAGS_VARSIZE ) ) {
        if( has_flag( flag_VARSIZE ) ) {
            std::string resize_str;
            if( has_flag( flag_FIT ) ) {
                switch( sizing_level ) {
                    case sizing::small_sized_human_char:
                        resize_str = _( "<info>can be upsized</info>" );
                        break;
                    case sizing::human_sized_small_char:
                        resize_str = _( "<info>can be downsized</info>" );
                        break;
                    case sizing::big_sized_human_char:
                    case sizing::big_sized_small_char:
                        resize_str = _( "<bad>can not be downsized</bad>" );
                        break;
                    case sizing::small_sized_big_char:
                    case sizing::human_sized_big_char:
                        resize_str = _( "<bad>can not be upsized</bad> without drastically altering it" );
                        break;
                    default:
                        break;
                }
                if( !resize_str.empty() ) {
                    std::string info_str = string_format( _( "* This clothing %s." ), resize_str );
                    info.emplace_back( "DESCRIPTION", info_str );
                }
            } else {
                switch( sizing_level ) {
                    case sizing::small_sized_human_char:
                        resize_str = _( " and <info>upsized</info>" );
                        break;
                    case sizing::human_sized_small_char:
                        resize_str = _( " and <info>downsized</info>" );
                        break;
                    case sizing::big_sized_human_char:
                    case sizing::big_sized_small_char:
                        resize_str = _( " but <bad>not downsized</bad>" );
                        break;
                    case sizing::small_sized_big_char:
                    case sizing::human_sized_big_char:
                        resize_str = _( " but <bad>not upsized</bad>" );
                        break;
                    default:
                        break;
                }
                std::string info_str = string_format( _( "* This clothing <info>can be "
                                                      "refitted</info>%s." ), resize_str );
                info.emplace_back( "DESCRIPTION", info_str );
            }
        } else {
            info.emplace_back( "DESCRIPTION", _( "* This clothing <bad>can not be refitted, "
                                                 "upsized, or downsized</bad>." ) );
        }
    }

    if( is_sided() && parts->test( iteminfo_parts::DESCRIPTION_FLAGS_SIDED ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "* This item can be worn on <info>either side</info> of "
                              "the body." ) );
    }
    if( ( is_power_armor() ) &&
        parts->test( iteminfo_parts::DESCRIPTION_FLAGS_POWERARMOR ) ) {
        if( parts->test( iteminfo_parts::DESCRIPTION_FLAGS_POWERARMOR_RADIATIONHINT ) ) {
            if( covers( bodypart_id( "head" ) ) && has_flag( flag_POWERARMOR_EXTERNAL ) ) {
                info.emplace_back( "DESCRIPTION",
                                   _( "* When worn with a power armor suit, it will "
                                      "<good>fully protect</good> you from "
                                      "<info>radiation</info>." ) );
            } else if( has_flag( flag_POWERARMOR_EXO ) ) {
                info.emplace_back( "DESCRIPTION",
                                   _( "* When worn with a power armor helmet, it will "
                                      "<good>fully protect</good> you from " "<info>radiation</info>." ) );
            }
        }
    }
    if( typeId() == itype_rad_badge && parts->test( iteminfo_parts::DESCRIPTION_IRRADIATION ) ) {
        info.emplace_back( "DESCRIPTION",
                           string_format( _( "* The film strip on the badge is %s." ),
                                          rad_badge_color( irradiation ) ) );
    }
}

void item::book_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int /* batch */,
                      bool /* debug */ ) const
{
    if( !is_book() ) {
        return;
    }

    Character &character = get_player_character();

    insert_separation_line( info );
    const islot_book &book = *type->book;
    // Some things about a book you CAN tell by it's cover.
    if( !book.skill && !type->can_use( "MA_MANUAL" ) && parts->test( iteminfo_parts::BOOK_SUMMARY ) ) {
        info.emplace_back( "BOOK", _( "Just for fun." ) );
    }
    if( type->can_use( "MA_MANUAL" ) && parts->test( iteminfo_parts::BOOK_SUMMARY ) ) {
        info.emplace_back( "BOOK",
                           _( "Some sort of <info>martial arts training "
                              "manual</info>." ) );
        const matype_id style_to_learn = martial_art_learned_from( *type );
        info.emplace_back( "BOOK",
                           string_format( _( "You can learn <info>%s</info> style "
                                             "from it." ), style_to_learn->name ) );
        info.emplace_back( "BOOK",
                           string_format( _( "This fighting style is <info>%s</info> "
                                             "to learn." ),
                                          martialart_difficulty( style_to_learn ) ) );
        info.emplace_back( "BOOK",
                           string_format( _( "It'd be easier to master if you'd have "
                                             "skill expertise in <info>%s</info>." ),
                                          style_to_learn->primary_skill->name() ) );
    }
    if( book.req == 0 && parts->test( iteminfo_parts::BOOK_REQUIREMENTS_BEGINNER ) ) {
        info.emplace_back( "BOOK", _( "It can be <info>understood by "
                                      "beginners</info>." ) );
    }
    avatar &you = get_avatar();
    if( !you.has_identified( typeId() ) && parts->test( iteminfo_parts::BOOK_UNREAD ) ) {
        info.emplace_back( "BOOK",
                           _( "You have <info>never read</info> this book." ) );
    }
    if( book.skill ) {
        const SkillLevel &skill = you.get_skill_level_object( book.skill );
        if( parts->test( iteminfo_parts::BOOK_SKILLRANGE_MAX ) ) {
            const std::string skill_name = book.skill->name();
            const std::string fmt = string_format( _( "Can bring <info>%s skill to</info> "
                                                   "<num>." ), skill_name );
            info.emplace_back( "BOOK", "", skill.can_train() ? fmt : colorize( fmt, c_brown ),
                               iteminfo::no_flags, book.level );
            info.emplace_back( "BOOK", "",
                               string_format( _( "Your current <stat>%s skill</stat> is <num>." ), skill_name ),
                               iteminfo::no_flags, skill.level() );
        }

        if( book.req != 0 && parts->test( iteminfo_parts::BOOK_SKILLRANGE_MIN ) ) {
            const std::string fmt = string_format(
                                        _( "<info>Requires %s level</info> <num> to "
                                           "understand." ), book.skill.obj().name() );
            info.emplace_back( "BOOK", "", fmt,
                               iteminfo::lower_is_better, book.req );
        }
    }

    if( book.intel != 0 && parts->test( iteminfo_parts::BOOK_REQUIREMENTS_INT ) ) {
        info.emplace_back( "BOOK", "",
                           _( "Requires <info>intelligence of</info> <num> to easily "
                              "read." ), iteminfo::lower_is_better, book.intel );
    }
    if( character_funcs::get_book_fun_for( character, *this ) != 0 &&
        parts->test( iteminfo_parts::BOOK_MORALECHANGE ) ) {
        info.emplace_back( "BOOK", "",
                           _( "Reading this book affects your morale by <num>" ),
                           iteminfo::show_plus, character_funcs::get_book_fun_for( character, *this ) );
    }
    if( parts->test( iteminfo_parts::BOOK_TIMEPERCHAPTER ) ) {
        std::string fmt = vgettext(
                              "A chapter of this book takes <num> <info>minute to "
                              "read</info>.",
                              "A chapter of this book takes <num> <info>minutes to "
                              "read</info>.", book.time );
        if( type->use_methods.contains( "MA_MANUAL" ) ) {
            fmt = vgettext(
                      "<info>A training session</info> with this book takes "
                      "<num> <info>minute</info>.",
                      "<info>A training session</info> with this book takes "
                      "<num> <info>minutes</info>.", book.time );
        }
        info.emplace_back( "BOOK", "", fmt,
                           iteminfo::lower_is_better, book.time );
    }

    if( book.chapters > 0 && parts->test( iteminfo_parts::BOOK_NUMUNREADCHAPTERS ) ) {
        const int unread = get_remaining_chapters( you );
        std::string fmt = vgettext( "This book has <num> <info>unread chapter</info>.",
                                    "This book has <num> <info>unread chapters</info>.",
                                    unread );
        info.emplace_back( "BOOK", "", fmt, iteminfo::no_flags, unread );
    }

    std::vector<std::string> recipe_list;
    for( const book_recipe &elem : book.recipes ) {
        const bool knows_it = you.knows_recipe( elem.recipe );
        const bool can_learn = you.get_skill_level( elem.recipe->skill_used )  >= elem.skill_level;
        // If the player knows it, they recognize it even if it's not clearly stated.
        if( elem.is_hidden() && !knows_it ) {
            continue;
        }
        if( knows_it ) {
            // In case the recipe is known, but has a different name in the book, use the
            // real name to avoid confusing the player.
            const std::string name = elem.recipe->result_name( /*decorated=*/true );
            recipe_list.push_back( "<bold>" + name + "</bold>" );
        } else if( !can_learn ) {
            recipe_list.push_back( "<color_brown>" + elem.name.translated() + "</color>" );
        } else {
            recipe_list.push_back( "<dark>" + elem.name.translated() + "</dark>" );
        }
    }

    if( !recipe_list.empty() && parts->test( iteminfo_parts::DESCRIPTION_BOOK_RECIPES ) ) {
        std::string recipe_line =
            string_format( vgettext( "This book contains %1$d crafting recipe: %2$s",
                                     "This book contains %1$d crafting recipes: %2$s",
                                     recipe_list.size() ),
                           recipe_list.size(), enumerate_as_string( recipe_list ) );

        insert_separation_line( info );
        info.emplace_back( "DESCRIPTION", recipe_line );
    }

    if( recipe_list.size() != book.recipes.size() &&
        parts->test( iteminfo_parts::DESCRIPTION_BOOK_ADDITIONAL_RECIPES ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "It might help you figuring out some <good>more "
                              "recipes</good>." ) );
    }
}

void item::container_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int /*batch*/,
                           bool /*debug*/ ) const
{
    if( !is_container() || !parts->test( iteminfo_parts::CONTAINER_DETAILS ) ) {
        return;
    }

    insert_separation_line( info );
    const islot_container &c = *type->container;

    std::string container_str =  _( "This container " );

    if( c.seals ) {
        container_str += _( "can be <info>resealed</info>, " );
    }
    if( c.watertight ) {
        container_str += _( "is <info>watertight</info>, " );
    }
    if( c.preserves ) {
        container_str += _( "<good>prevents spoiling</good>, " );
    }

    container_str += string_format( _( "can store <info>%s %s</info>." ),
                                    format_volume( c.contains ), volume_units_long() );

    info.emplace_back( "CONTAINER", container_str );
}

void item::battery_info( std::vector<iteminfo> &info, const iteminfo_query * /*parts*/,
                         int /*batch*/, bool /*debug*/ ) const
{
    if( !is_battery() ) {
        return;
    }

    std::string info_string;
    if( type->battery->max_capacity < 1_kJ ) {
        info_string = string_format( _( "<bold>Capacity</bold>: %dJ" ),
                                     to_joule( type->battery->max_capacity ) );
    } else if( type->battery->max_capacity >= 1_kJ ) {
        info_string = string_format( _( "<bold>Capacity</bold>: %dkJ" ),
                                     to_kilojoule( type->battery->max_capacity ) );
    }
    insert_separation_line( info );
    info.emplace_back( "BATTERY", info_string );
}

void item::tool_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int /*batch*/,
                      bool /*debug*/ ) const
{
    if( !is_tool() ) {
        return;
    }

    insert_separation_line( info );
    if( ammo_capacity() != 0 && parts->test( iteminfo_parts::TOOL_CHARGES ) ) {
        info.emplace_back( "TOOL", string_format( _( "<bold>Charges</bold>: %d" ),
                           ammo_remaining() ) );
    }

    if( !magazine_integral() ) {
        if( magazine_current() && parts->test( iteminfo_parts::TOOL_MAGAZINE_CURRENT ) ) {
            info.emplace_back( "TOOL", _( "Magazine: " ),
                               string_format( "<stat>%s</stat>", magazine_current()->tname() ) );
        }

        if( parts->test( iteminfo_parts::TOOL_MAGAZINE_COMPATIBLE ) ) {
            const auto &compat = magazine_compatible();
            if( !compat.empty() ) {
                info.emplace_back( "TOOL", _( "Compatible magazines: " )
                                   + enumerate_as_string( compat, ::nname ) );
            }
        }
    } else if( ammo_capacity() != 0 && parts->test( iteminfo_parts::TOOL_CAPACITY ) ) {
        std::string tmp;
        bool bionic_tool = has_flag( flag_USES_BIONIC_POWER );
        if( !ammo_types().empty() ) {
            //~ "%s" is ammunition type. This types can't be plural.
            tmp = vgettext( "Maximum <num> charge of %s.", "Maximum <num> charges of %s.",
                            ammo_capacity() );
            tmp = string_format( tmp, enumerate_as_string( ammo_types().begin(),
            ammo_types().end(), []( const ammotype & at ) {
                return at->name();
            }, enumeration_conjunction::none ) );

            // No need to display max charges, since charges are always equal to bionic power
        } else if( !bionic_tool ) {
            tmp = vgettext( "Maximum <num> charge.", "Maximum <num> charges.", ammo_capacity() );
        }
        if( !bionic_tool ) {
            info.emplace_back( "TOOL", "", tmp, iteminfo::no_flags, ammo_capacity() );
        }
    }
    if( type->tool->ups_eff_mult != 1 ) {
        info.emplace_back( "TOOL", _( "UPS Efficiency Multiplier: " ),
                           string_format( "<stat>%s</stat>", type->tool->ups_eff_mult ) );

    }
}

void item::component_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int /*batch*/,
                           bool /*debug*/ ) const
{
    if( components.empty() || !parts->test( iteminfo_parts::DESCRIPTION_COMPONENTS_MADEFROM ) ) {
        return;
    }
    if( is_craft() ) {
        info.emplace_back( "DESCRIPTION", string_format( _( "Using: %s" ),
                           _( components_to_string() ) ) );
        // Ugly hack warning! Corpses have CBMs as their components
    } else if( !is_corpse() ) {
        info.emplace_back( "DESCRIPTION", string_format( _( "Made from: %s" ),
                           _( components_to_string() ) ) );
    } else if( get_var( "bionics_scanned_by", -1 ) == get_avatar().getID().get_value() ) {
        // TODO: Extract into a more proper place (function in namespace)
        std::string bionics_string = enumerate_as_string( components.begin(), components.end(),
        []( const item * const & entry ) -> std::string {
            return entry->is_bionic() ? entry->display_name() : "";
        }, enumeration_conjunction::none );
        info.emplace_back( "DESCRIPTION", string_format( _( "Contains: %s" ),
                           bionics_string ) );
    }
}

void item::repair_info( std::vector<iteminfo> &info, const iteminfo_query *parts,
                        int /*batch*/, bool /*debug*/ ) const
{
    if( !parts->test( iteminfo_parts::DESCRIPTION_REPAIREDWITH ) ) {
        return;
    }
    insert_separation_line( info );
    const std::vector<itype_id> &rep = sorted_lex( repaired_with() );
    if( !rep.empty() ) {
        info.emplace_back( "DESCRIPTION", string_format( _( "<bold>Repair</bold> using %s." ),
        enumerate_as_string( rep.begin(), rep.end(), []( const itype_id & e ) {
            return nname( e );
        }, enumeration_conjunction::or_ ) ) );
        if( reinforceable() ) {
            info.emplace_back( "DESCRIPTION", _( "* This item can be <good>reinforced</good>." ) );
        }
    } else {
        info.emplace_back( "DESCRIPTION", _( "* This item is <bad>not repairable</bad>." ) );
    }
}

void item::disassembly_info( std::vector<iteminfo> &info, const iteminfo_query *parts,
                             int /*batch*/, bool /*debug*/ ) const
{
    if( !components.empty() && parts->test( iteminfo_parts::DESCRIPTION_COMPONENTS_MADEFROM ) ) {
        return;
    }
    if( !parts->test( iteminfo_parts::DESCRIPTION_COMPONENTS_DISASSEMBLE ) ) {
        return;
    }

    const recipe &dis = recipe_dictionary::get_uncraft( typeId() );
    const requirement_data &req = dis.disassembly_requirements();
    if( !req.is_empty() ) {
        const std::string approx_time = to_string_approx( time_duration::from_turns( dis.time / 100 ) );

        const requirement_data::alter_item_comp_vector &comps_list = req.get_components();
        const std::string comps_str = enumerate_as_string( comps_list.begin(), comps_list.end(),
        []( const std::vector<item_comp> &comp_opts ) {
            return comp_opts.front().to_string();
        } );

        std::vector<std::string> reqs_list;
        const requirement_data::alter_tool_comp_vector &tools_list = req.get_tools();
        for( const std::vector<tool_comp> &it : tools_list ) {
            if( !it.empty() ) {
                reqs_list.push_back( it.front().to_string() );
            }
        }
        const requirement_data::alter_quali_req_vector &quals_list = req.get_qualities();
        for( const std::vector<quality_requirement> &it : quals_list ) {
            if( !it.empty() ) {
                reqs_list.push_back( it.front().to_colored_string() );
            }
        }

        std::string descr;
        if( reqs_list.empty() ) {
            //~ 1 is approx. time (e.g. 'about 5 minutes'), 2 is a list of items
            descr = string_format( _( "<bold>Disassembly</bold> takes %1$s and might yield: %2$s." ),
                                   approx_time, comps_str );
        } else {
            const std::string reqs_str = enumerate_as_string( reqs_list );
            descr = string_format(
                        //~ 1 is approx. time, 2 is a list of items and tools with qualities, 3 is a list of items.
                        //~ Bold text in the middle makes it easier to see where the second list starts.
                        _( "<bold>Disassembly</bold> takes %1$s, requires %2$s and <bold>might yield</bold>: %3$s." ),
                        approx_time, reqs_str, comps_str );
        }

        insert_separation_line( info );
        info.emplace_back( "DESCRIPTION", descr );
    }
}

void item::qualities_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int /*batch*/,
                           bool /*debug*/ ) const
{
    auto name_quality = [&info]( const std::pair<quality_id, int> &q ) {
        std::string str;
        if( q.first == qual_JACK || q.first == qual_LIFT ) {
            str = string_format( _( "Has level <info>%1$d %2$s</info> quality and "
                                    "is rated at <info>%3$d</info> %4$s" ),
                                 q.second, q.first.obj().name,
                                 static_cast<int>( convert_weight( q.second * TOOL_LIFT_FACTOR ) ),
                                 weight_units() );
        } else {
            str = string_format( _( "Has level <info>%1$d %2$s</info> quality." ),
                                 q.second, q.first.obj().name );
        }
        info.emplace_back( "QUALITIES", "", str );
    };

    if( parts->test( iteminfo_parts::QUALITIES ) ) {
        insert_separation_line( info );
        for( const std::pair<const quality_id, int> q : sorted_lex( type->qualities ) ) {
            name_quality( q );
        }
    }

    if( parts->test( iteminfo_parts::QUALITIES_CONTAINED ) &&
    contents.has_any_with( []( const item & e ) {
    return !e.type->qualities.empty();
    } ) ) {

        info.emplace_back( "QUALITIES", "", _( "Contains items with qualities:" ) );
        std::map<quality_id, int, quality_id::LexCmp> most_quality;
        for( const item *e : contents.all_items_top() ) {
            for( const std::pair<const quality_id, int> &q : e->type->qualities ) {
                auto emplace_result = most_quality.emplace( q );
                if( !emplace_result.second &&
                    most_quality.at( emplace_result.first->first ) < q.second ) {
                    most_quality[ q.first ] = q.second;
                }
            }
        }
        for( const std::pair<const quality_id, int> &q : most_quality ) {
            name_quality( q );
        }
    }
}

void item::bionic_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int /*batch*/,
                        bool /*debug*/ ) const
{
    if( !is_bionic() ) {
        return;
    }

    // TODO: Unhide when enforcing limits
    if( get_option < bool >( "CBM_SLOTS_ENABLED" )
        && parts->test( iteminfo_parts::DESCRIPTION_CBM_SLOTS ) ) {
        info.emplace_back( "DESCRIPTION", list_occupied_bps( type->bionic->id,
                           _( "This bionic is installed in the following body "
                              "part(s):" ) ) );
    }
    insert_separation_line( info );

    const bionic_id bid = type->bionic->id;
    const std::vector<itype_id> &fuels = bid->fuel_opts;
    if( !fuels.empty() ) {
        const int &fuel_numb = fuels.size();

        info.emplace_back( "DESCRIPTION",
                           vgettext( "* This bionic can produce power from the following fuel: ",
                                     "* This bionic can produce power from the following fuels: ",
                                     fuel_numb ) + enumerate_as_string( fuels.begin(),
                                             fuels.end(), []( const itype_id & id ) -> std::string { return "<info>" + id->nname( 1 ) + "</info>"; } ) );
    }

    insert_separation_line( info );

    if( bid->capacity > 0_J ) {
        info.emplace_back( "CBM", _( "<bold>Power Capacity</bold>:" ), _( " <num> J" ),
                           iteminfo::no_newline,
                           units::to_joule( bid->capacity ) );
    }

    insert_separation_line( info );

    if( !bid->required_bionics.empty() ) {
        for( const bionic_id &req_bid : bid->required_bionics ) {
            info.emplace_back( "CBM", string_format( "* This CBM requires another CBM to also be installed: %s",
                               req_bid->name ) );
        }
    }

    insert_separation_line( info );

    if( !bid->encumbrance.empty() ) {
        info.emplace_back( "DESCRIPTION", _( "<bold>Encumbrance</bold>: " ),
                           iteminfo::no_newline );
        for( const std::pair< const bodypart_str_id, int > element : sorted_lex( bid->encumbrance ) ) {
            info.emplace_back( "CBM", body_part_name_as_heading( element.first->token, 1 ),
                               " <num> ", iteminfo::no_newline, element.second );
        }
    }

    if( !bid->env_protec.empty() ) {
        info.emplace_back( "DESCRIPTION",
                           bid->activated ? _( "<bold>Environmental Protection (activated)</bold>: " ) :
                           _( "<bold>Environmental Protection</bold>: " ),
                           iteminfo::no_newline );
        for( const std::pair< const bodypart_str_id, int > element : sorted_lex( bid->env_protec ) ) {
            info.emplace_back( "CBM", body_part_name_as_heading( element.first->token, 1 ),
                               " <num> ", iteminfo::no_newline, element.second );
        }
    }

    if( !bid->bash_protec.empty() ) {
        info.emplace_back( "DESCRIPTION",
                           _( "<bold>Bash Protection</bold>: " ),
                           iteminfo::no_newline );
        for( const std::pair< const bodypart_str_id, int > element : sorted_lex( bid->bash_protec ) ) {
            info.emplace_back( "CBM", body_part_name_as_heading( element.first->token, 1 ),
                               " <num> ", iteminfo::no_newline, element.second );
        }
    }

    if( !bid->cut_protec.empty() ) {
        info.emplace_back( "DESCRIPTION",
                           _( "<bold>Cut Protection</bold>: " ),
                           iteminfo::no_newline );
        for( const std::pair< const bodypart_str_id, int > element : sorted_lex( bid->cut_protec ) ) {
            info.emplace_back( "CBM", body_part_name_as_heading( element.first->token, 1 ),
                               " <num> ", iteminfo::no_newline, element.second );
        }
    }

    if( !bid->bullet_protec.empty() ) {
        info.emplace_back( "DESCRIPTION", _( "<bold>Ballistic Protection</bold>: " ),
                           iteminfo::no_newline );
        for( const auto &element : bid->bullet_protec ) {
            info.emplace_back( "CBM", body_part_name_as_heading( element.first->token, 1 ),
                               " <num> ", iteminfo::no_newline, element.second );
        }
    }

    if( !bid->stat_bonus.empty() ) {
        info.emplace_back( "DESCRIPTION", _( "<bold>Stat Bonus</bold>: " ),
                           iteminfo::no_newline );
        for( const auto &element : bid->stat_bonus ) {
            info.emplace_back( "CBM", get_stat_name( element.first ), " <num> ",
                               iteminfo::no_newline, element.second );
        }
    }

    const units::mass weight_bonus = bid->weight_capacity_bonus;
    const float weight_modif = bid->weight_capacity_modifier;
    if( weight_modif != 1 ) {
        std::string modifier;
        if( weight_modif < 1 ) {
            modifier = "<num><bad>x</bad>";
        } else {
            modifier = "<num><color_light_green>x</color>";
        }
        info.emplace_back( "CBM",
                           _( "<bold>Weight capacity modifier</bold>: " ), modifier,
                           iteminfo::no_newline | iteminfo::is_decimal,
                           weight_modif );
    }
    if( weight_bonus != 0_gram ) {
        std::string bonus;
        if( weight_bonus < 0_gram ) {
            bonus = string_format( "<num> <bad>%s</bad>", weight_units() );
        } else {
            bonus = string_format( "<num> <color_light_green>%s</color>", weight_units() );
        }
        info.emplace_back( "CBM", _( "<bold>Weight capacity bonus</bold>: " ), bonus,
                           iteminfo::no_newline | iteminfo::is_decimal,
                           convert_weight( weight_bonus ) );
    }
}

void item::combat_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int /*batch*/,
                        bool /*debug*/ ) const
{
    const std::string space = "  ";
    const std::string newline = "\n";

    bool print_attacks = false;

    // Old behavior - default to it for now
    if( type->attacks.contains( "DEFAULT" ) ) {
        const auto &attack = melee::default_attack( *this );
        int dmg_bash = damage_melee( DT_BASH );
        int dmg_cut = damage_melee( DT_CUT );
        int dmg_stab = damage_melee( DT_STAB );
        if( dmg_bash || dmg_cut || dmg_stab || type->m_to_hit > 0 ) {
            print_attacks = true;
        }

        if( parts->test( iteminfo_parts::BASE_DAMAGE ) ) {
            insert_separation_line( info );
            std::string sep;
            if( dmg_bash || dmg_cut || dmg_stab ) {
                info.emplace_back( "BASE", _( "<bold>Melee damage</bold>: " ), "", iteminfo::no_newline );
            }
            if( dmg_bash ) {
                info.emplace_back( "BASE", _( "Bash: " ), "", iteminfo::no_newline, dmg_bash );
                sep = space;
            }
            if( dmg_cut ) {
                info.emplace_back( "BASE", sep + _( "Cut: " ), "", iteminfo::no_newline, dmg_cut );
                sep = space;
            }
            if( dmg_stab ) {
                info.emplace_back( "BASE", sep + _( "Pierce: " ), "", iteminfo::no_newline, dmg_stab );
            }
        }

        if( dmg_bash || dmg_cut || dmg_stab ) {
            if( parts->test( iteminfo_parts::BASE_TOHIT ) ) {
                info.emplace_back( "BASE", space + _( "To-hit bonus: " ), "",
                                   iteminfo::show_plus, type->m_to_hit + get_melee_hit_bonus() );
            }

            if( parts->test( iteminfo_parts::BASE_MOVES ) ) {
                info.emplace_back( "BASE", _( "Moves per attack: " ), "",
                                   iteminfo::lower_is_better, attack_cost() );
                // This would be a bar if iteminfo was not very insistent on numbers
                info.emplace_back( "BASE", _( "Stamina Cost: " ), "", iteminfo::lower_is_better, stamina_cost() );
                info.emplace_back( "BASE", _( "Typical damage per second:" ), "" );
                const std::map<std::string, double> &dps_data = dps( true, false, attack );
                std::string sep;
                for( const std::pair<const std::string, double> &dps_entry : dps_data ) {
                    info.emplace_back( "BASE", sep + dps_entry.first + ": ", "",
                                       iteminfo::no_newline | iteminfo::is_decimal,
                                       dps_entry.second );
                    sep = space;
                }
                info.emplace_back( "BASE", "" );
            }
        }
    } else {
        print_attacks = true;

        for( const auto &attack_pr : get_attacks() ) {
            const auto &attack = attack_pr.second;

            if( parts->test( iteminfo_parts::BASE_DAMAGE ) ) {
                insert_separation_line( info );
                info.emplace_back( "BASE", _( "<bold>Melee damage</bold>:" ), "", iteminfo::no_newline );
                // if we have any armour penetration numbers, put every damage type on its own line
                bool line_by_line = attack.damage.has_armor_piercing();
                if( line_by_line ) {
                    info.emplace_back( "BASE", newline, "", iteminfo::no_newline );
                } else {
                    info.emplace_back( "BASE", space, "", iteminfo::no_newline );
                }
                damage_statblock_info( info, attack.damage, line_by_line );
            }

            if( parts->test( iteminfo_parts::BASE_TOHIT ) ) {
                info.emplace_back( "BASE", _( "To-hit bonus: " ), "",
                                   iteminfo::show_plus, attack.to_hit + get_melee_hit_bonus() );
            }

            if( parts->test( iteminfo_parts::BASE_MOVES ) ) {
                info.emplace_back( "BASE", _( "Moves per attack: " ), "",
                                   iteminfo::lower_is_better, attack_cost() );
                // This would be a bar if iteminfo was not very insistent on numbers
                info.emplace_back( "BASE", _( "Stamina Cost: " ), "", iteminfo::lower_is_better, stamina_cost() );
                info.emplace_back( "BASE", _( "Typical damage per second:" ), "" );
                const std::map<std::string, double> &dps_data = dps( true, false, attack );
                std::string sep;
                for( const std::pair<const std::string, double> &dps_entry : dps_data ) {
                    info.emplace_back( "BASE", sep + dps_entry.first + ": ", "",
                                       iteminfo::no_newline | iteminfo::is_decimal,
                                       dps_entry.second );
                    sep = space;
                }
                info.emplace_back( "BASE", "" );
            }
        }
    }

    if( parts->test( iteminfo_parts::DESCRIPTION_TECHNIQUES ) ) {
        std::set<matec_id> all_techniques = type->techniques;
        all_techniques.insert( techniques.begin(), techniques.end() );

        if( !all_techniques.empty() ) {
            const std::vector<matec_id> all_tec_sorted = sorted_lex( all_techniques );
            insert_separation_line( info );
            info.emplace_back( "DESCRIPTION", _( "<bold>Techniques when wielded</bold>: " ) +
            enumerate_as_string( all_tec_sorted.begin(), all_tec_sorted.end(), []( const matec_id & tid ) {
                return string_format( "<stat>%s</stat>: <info>%s</info>", _( tid.obj().name ),
                                      _( tid.obj().description ) );
            } ) );
        }
    }

    avatar &you = get_avatar();
    // display which martial arts styles character can use with this weapon
    if( parts->test( iteminfo_parts::DESCRIPTION_APPLICABLEMARTIALARTS ) ) {
        const std::string valid_styles = you.martial_arts_data->enumerate_known_styles( typeId() );
        if( !valid_styles.empty() ) {
            insert_separation_line( info );
            info.emplace_back( "DESCRIPTION",
                               _( "You know how to use this with these martial arts "
                                  "styles: " ) + valid_styles );
        }
    }

    if( !is_gunmod() && has_flag( flag_REACH_ATTACK ) &&
        parts->test( iteminfo_parts::DESCRIPTION_GUNMOD_ADDREACHATTACK ) ) {
        insert_separation_line( info );
        if( has_flag( flag_REACH3 ) ) {
            info.emplace_back( "DESCRIPTION",
                               _( "* This item can be used to make <stat>long reach "
                                  "attacks</stat>." ) );
        } else {
            info.emplace_back( "DESCRIPTION",
                               _( "* This item can be used to make <stat>reach "
                                  "attacks</stat>." ) );
        }
    }

    if( print_attacks || debug_mode ) {
        // @todo Handle multiple attacks
        const attack_statblock &default_attack = melee::default_attack( *this );
        damage_instance non_crit;
        melee::roll_all_damage( you, false, non_crit, true, *this, default_attack );
        damage_instance crit;
        melee::roll_all_damage( you, true, crit, true, *this, default_attack );
        int attack_cost = you.attack_cost( *this );
        insert_separation_line( info );
        if( parts->test( iteminfo_parts::DESCRIPTION_MELEEDMG ) ) {
            info.emplace_back( "DESCRIPTION", _( "<bold>Average melee damage</bold>:" ) );
        }
        // Chance of critical hit
        if( parts->test( iteminfo_parts::DESCRIPTION_MELEEDMG_CRIT ) ) {
            const auto &attack = melee::default_attack( *this );
            info.emplace_back( "DESCRIPTION",
                               string_format( _( "Critical hit chance <neutral>%d%% - %d%%</neutral>" ),
                                              static_cast<int>( you.crit_chance( 0, 100, *this, attack ) *
                                                      100 ),
                                              static_cast<int>( you.crit_chance( 100, 0, *this, attack ) *
                                                      100 ) ) );
        }
        // Bash damage
        if( parts->test( iteminfo_parts::DESCRIPTION_MELEEDMG_BASH ) ) {
            // NOTE: Using "BASE" instead of "DESCRIPTION", so numerical formatting will work
            // (output.cpp:format_item_info does not interpolate <num> for DESCRIPTION info)
            info.emplace_back( "BASE", _( "Bashing: " ), "<num>", iteminfo::no_newline,
                               non_crit.type_damage( DT_BASH ) );
            info.emplace_back( "BASE", space + _( "Critical bash: " ), "<num>", iteminfo::no_flags,
                               crit.type_damage( DT_BASH ) );
        }
        // Cut damage
        if( ( non_crit.type_damage( DT_CUT ) > 0.0f || crit.type_damage( DT_CUT ) > 0.0f )
            && parts->test( iteminfo_parts::DESCRIPTION_MELEEDMG_CUT ) ) {

            info.emplace_back( "BASE", _( "Cutting: " ), "<num>", iteminfo::no_newline,
                               non_crit.type_damage( DT_CUT ) );
            info.emplace_back( "BASE", space + _( "Critical cut: " ), "<num>", iteminfo::no_flags,
                               crit.type_damage( DT_CUT ) );
        }
        // Pierce/stab damage
        if( ( non_crit.type_damage( DT_STAB ) > 0.0f || crit.type_damage( DT_STAB ) > 0.0f )
            && parts->test( iteminfo_parts::DESCRIPTION_MELEEDMG_PIERCE ) ) {

            info.emplace_back( "BASE", _( "Piercing: " ), "<num>", iteminfo::no_newline,
                               non_crit.type_damage( DT_STAB ) );
            info.emplace_back( "BASE", space + _( "Critical pierce: " ), "<num>", iteminfo::no_flags,
                               crit.type_damage( DT_STAB ) );
        }
        // Moves
        if( parts->test( iteminfo_parts::DESCRIPTION_MELEEDMG_MOVES ) ) {
            info.emplace_back( "BASE", _( "Moves per attack: " ), "<num>",
                               iteminfo::lower_is_better, attack_cost );
        }
        insert_separation_line( info );
    }
}

// TODO: Deduplicated with ammo_info()
void item::damage_statblock_info( std::vector<iteminfo> &info, damage_instance attack,
                                  bool line_by_line ) const
{
    const std::string space = "  ";
    const std::string newline = "\n";
    std::string sep;

    /* TODO: All damage types can be defined for an attack, and will be displayed in the item description.
             However, non-physical types will be ignored by the melee damage roll, so they don't actually do any damage.
       TODO: damage_instance isn't ordered, so the damage types will be displayed in whatever order they were defined.
             This will probably be fine, but it might be a good idea to sort the damage_units. */
    for( const auto damage : attack ) {
        if( damage.amount != 0.0 ) {
            info.emplace_back( "BASE", sep + damage.get_name() + _( ": " ), "", iteminfo::no_newline,
                               damage.amount );

            if( damage.res_pen != 0.0 && damage.res_mult != 1.0 ) {
                // Both flat AP and an armor multiplier
                info.emplace_back( "BASE", _( "  Armor-pierce: " ), "", iteminfo::no_newline, damage.res_pen );
                info.emplace_back( "BASE", _( "/" ), "",
                                   iteminfo::no_newline | iteminfo::is_decimal | iteminfo::lower_is_better, damage.res_mult );
            } else if( damage.res_mult != 1.0 ) {
                // Only armor multiplier
                info.emplace_back( "BASE", _( "  Armor multiplier: " ), "",
                                   iteminfo::no_newline | iteminfo::is_decimal | iteminfo::lower_is_better, damage.res_mult );
            } else if( damage.res_pen != 0.0 ) {
                // Only flat AP
                info.emplace_back( "BASE", _( "Armor-pierce: " ), "", iteminfo::no_newline, damage.res_pen );
            }
            sep = line_by_line ? newline : space;
        }
    }
    info.emplace_back( "BASE", sep, "", iteminfo::no_newline );
}

void item::contents_info( std::vector<iteminfo> &info, const iteminfo_query *parts, int batch,
                          bool debug ) const
{
    if( contents.empty() || !parts->test( iteminfo_parts::DESCRIPTION_CONTENTS ) ) {
        return;
    }
    const std::string space = "  ";

    for( const item *mod : is_gun() ? gunmods() : toolmods() ) {
        std::string mod_str;
        if( mod->type->gunmod ) {
            if( mod->is_irremovable() ) {
                mod_str = _( "Integrated mod: " );
            } else {
                mod_str = _( "Mod: " );
            }
            mod_str += string_format( "<bold>%s</bold> (%s) ", mod->tname(),
                                      mod->type->gunmod->location.name() );
        }
        insert_separation_line( info );
        info.emplace_back( "DESCRIPTION", mod_str );
        info.emplace_back( "DESCRIPTION", mod->type->description.translated() );
    }
    bool contents_header = false;
    for( const item *contents_item : contents.all_items_top() ) {
        if( !contents_item->type->mod ) {
            if( !contents_header ) {
                insert_separation_line( info );
                info.emplace_back( "DESCRIPTION", _( "<bold>Contents of this item</bold>:" ) );
                contents_header = true;
            } else {
                // Separate items with a blank line
                info.emplace_back( "DESCRIPTION", space );
            }

            const translation &description = contents_item->type->description;

            if( contents_item->made_of( LIQUID ) ) {
                units::volume contents_volume = contents_item->volume() * batch;
                int converted_volume_scale = 0;
                const double converted_volume =
                    round_up( convert_volume( contents_volume.value(),
                                              &converted_volume_scale ), 2 );
                info.emplace_back( "DESCRIPTION", contents_item->display_name() );
                iteminfo::flags f = iteminfo::no_newline;
                if( display_mod_source ) {
                    info.emplace_back( "DESCRIPTION", string_format( _( "<stat>Origin: %s</stat>" ),
                                       enumerate_as_string( contents_item->type->src.begin(),
                    contents_item->type->src.end(), []( const std::pair<itype_id, mod_id> &content_source ) {
                        return string_format( "'%s'", content_source.second->name() );
                    }, enumeration_conjunction::arrow ) ) );
                }
                if( display_object_ids ) {
                    info.emplace_back( "DESCRIPTION", colorize(
                                           string_format( "[%s]", contents_item->type->get_id() ),
                                           c_light_blue ) );
                }
                if( converted_volume_scale != 0 ) {
                    f |= iteminfo::is_decimal;
                }
                info.emplace_back( "CONTAINER", description + space,
                                   string_format( "<num> %s", volume_units_abbr() ), f,
                                   converted_volume );
            } else {
                info.emplace_back( "DESCRIPTION", contents_item->display_name() );
                if( display_mod_source ) {
                    info.emplace_back( "DESCRIPTION", string_format( _( "<stat>Origin: %s</stat>" ),
                                       enumerate_as_string( contents_item->type->src.begin(),
                    contents_item->type->src.end(), []( const std::pair<itype_id, mod_id> &content_source ) {
                        return string_format( "'%s'", content_source.second->name() );
                    }, enumeration_conjunction::arrow ) ) );
                }
                if( display_object_ids ) {
                    info.emplace_back( "DESCRIPTION", colorize(
                                           string_format( "[%s]", contents_item->type->get_id() ),
                                           c_light_blue ) );
                }
                info.emplace_back( "DESCRIPTION", description.translated() );
            }

            if( debug && contents_item && contents_item->goes_bad() ) {
                info.emplace_back( "CONTAINER", space );
                info.emplace_back( "CONTAINER", _( "age (turns): " ),
                                   "", iteminfo::lower_is_better,
                                   to_turns<int>( contents_item->age() ) );
                info.emplace_back( "CONTAINER", _( "rot (turns): " ),
                                   "", iteminfo::lower_is_better,
                                   to_turns<int>( contents_item->rot ) );
                info.emplace_back( "CONTAINER", space + _( "shelf life (turns): " ),
                                   "", iteminfo::lower_is_better,
                                   to_turns<int>( contents_item->get_shelf_life() ) );
                info.emplace_back( "CONTAINER", _( "last rot: " ),
                                   "", iteminfo::lower_is_better,
                                   to_turn<int>( contents_item->last_rot_check ) );
            }
        }
    }
}

void item::final_info( std::vector<iteminfo> &info, const iteminfo_query &parts_ref, int batch,
                       bool debug ) const
{
    if( is_null() ) {
        return;
    }

    // TODO: Remove
    const iteminfo_query *parts = &parts_ref;

    const std::string space = "  ";

    insert_separation_line( info );

    if( can_shatter() ) {
        info.emplace_back( "BASE",
                           _( "* This item will potentially <info>shatter</info> if used as a weapon"
                              " or thrown, instantly <bad>destroying it and spilling any contents</bad>." ) );
    }

    if( parts->test( iteminfo_parts::BASE_RIGIDITY ) ) {
        if( const islot_armor *armor = find_armor_data() ) {
            if( !type->rigid ) {
                info.emplace_back( "BASE",
                                   _( "* This item is <info>not rigid</info>.  Its"
                                      " volume and encumbrance increase with contents." ) );
            } else {
                bool any_encumb_increase = std::ranges::any_of( armor->data,
                []( armor_portion_data data ) {
                    return data.encumber != data.max_encumber;
                } );
                if( any_encumb_increase ) {
                    info.emplace_back( "BASE",
                                       _( "* This item is <info>not rigid</info>.  Its"
                                          " volume and encumbrance increase with contents." ) );
                }
            }
        }
    }

    if( parts->test( iteminfo_parts::DESCRIPTION_CONDUCTIVITY ) ) {
        if( !conductive() ) {
            info.emplace_back( "BASE", _( "* This item <good>does not "
                                          "conduct</good> electricity." ) );
        } else if( has_flag( flag_CONDUCTIVE ) ) {
            info.emplace_back( "BASE",
                               _( "* This item effectively <bad>conducts</bad> "
                                  "electricity, as it has no guard." ) );
        } else {
            info.emplace_back( "BASE", _( "* This item <bad>conducts</bad> electricity." ) );
        }
    }

    avatar &you = get_avatar();
    if( is_armor() && you.has_trait( trait_WOOLALLERGY ) &&
        ( made_of( material_id( "wool" ) ) || has_own_flag( flag_wooled ) ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "* This clothing will give you an <bad>allergic "
                              "reaction</bad>." ) );
    }

    if( parts->test( iteminfo_parts::DESCRIPTION_FLAGS ) ) {
        // concatenate base and acquired flags...
        std::vector<flag_id> flags;
        std::set_union( type->get_flags().begin(), type->get_flags().end(),
                        get_flags().begin(), get_flags().end(),
                        std::back_inserter( flags ) );

        // ...and display those which have an info description
        for( const flag_id &e : sorted_lex( flags ) ) {
            const json_flag &f = e.obj();
            if( !f.info().empty() ) {
                info.emplace_back( "DESCRIPTION", string_format( "* %s", _( f.info() ) ) );
            }
        }
    }

    armor_fit_info( info, parts, batch, debug );

    if( is_tool() ) {
        if( is_power_armor() && parts->test( iteminfo_parts::DESCRIPTION_BIONIC_ARMOR_INTERFACE ) ) {
            info.emplace_back( "DESCRIPTION",
                               _( "* This tool can draw power from a <info>Bionic Armor Interface</info>" ) );
        }
        if( has_flag( flag_USE_UPS ) && parts->test( iteminfo_parts::DESCRIPTION_RECHARGE_UPSMODDED ) ) {
            info.emplace_back( "DESCRIPTION",
                               _( "* This tool uses a <info>universal power supply</info> "
                                  "and is <neutral>not compatible</neutral> with "
                                  "<info>standard batteries</info>." ) );
        } else if( has_flag( flag_RECHARGE ) && has_flag( flag_NO_RELOAD ) &&
                   parts->test( iteminfo_parts::DESCRIPTION_RECHARGE_NORELOAD ) ) {
            info.emplace_back( "DESCRIPTION",
                               _( "* This tool has a <info>rechargeable power cell</info> "
                                  "and is <neutral>not compatible</neutral> with "
                                  "<info>standard batteries</info>." ) );
        } else if( has_flag( flag_RECHARGE ) &&
                   parts->test( iteminfo_parts::DESCRIPTION_RECHARGE_UPSCAPABLE ) ) {
            info.emplace_back( "DESCRIPTION",
                               _( "* This tool has a <info>rechargeable power cell</info> "
                                  "and can be recharged in any <neutral>UPS-compatible "
                                  "recharging station</neutral>. You could charge it with "
                                  "<info>standard batteries</info>, but unloading it is "
                                  "impossible." ) );
        } else if( has_flag( flag_USES_BIONIC_POWER ) ) {
            info.emplace_back( "DESCRIPTION",
                               _( "* This tool <info>runs on bionic power</info>." ) );
        }
    }

    if( has_flag( flag_RADIO_ACTIVATION ) &&
        parts->test( iteminfo_parts::DESCRIPTION_RADIO_ACTIVATION ) ) {
        if( has_flag( flag_RADIO_MOD ) ) {
            info.emplace_back( "DESCRIPTION",
                               _( "* This item has been modified to listen to <info>radio "
                                  "signals</info>.  It can still be activated manually." ) );
        } else {
            info.emplace_back( "DESCRIPTION",
                               _( "* This item can only be activated by a <info>radio "
                                  "signal</info>." ) );
        }

        std::string signame;
        if( has_flag( flag_RADIOSIGNAL_1 ) ) {
            signame = "<color_c_red>red</color> radio signal.";
        } else if( has_flag( flag_RADIOSIGNAL_2 ) ) {
            signame = "<color_c_blue>blue</color> radio signal.";
        } else if( has_flag( flag_RADIOSIGNAL_3 ) ) {
            signame = "<color_c_green>green</color> radio signal.";
        }
        if( parts->test( iteminfo_parts::DESCRIPTION_RADIO_ACTIVATION_CHANNEL ) ) {
            info.emplace_back( "DESCRIPTION",
                               string_format( _( "* It will be activated by the %s." ),
                                              signame ) );
        }

        if( has_flag( flag_RADIO_INVOKE_PROC ) &&
            parts->test( iteminfo_parts::DESCRIPTION_RADIO_ACTIVATION_PROC ) ) {
            info.emplace_back( "DESCRIPTION",
                               _( "* Activating this item with a <info>radio signal</info> will "
                                  "<neutral>detonate</neutral> it immediately." ) );
        }
    }

    bionic_info( info, parts, batch, debug );

    if( is_gun() && has_flag( flag_FIRE_TWOHAND ) &&
        parts->test( iteminfo_parts::DESCRIPTION_TWOHANDED ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "* This weapon needs <info>two free hands</info> "
                              "to fire." ) );
    }

    if( is_gunmod() && has_flag( flag_DISABLE_SIGHTS ) &&
        parts->test( iteminfo_parts::DESCRIPTION_GUNMOD_DISABLESSIGHTS ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "* This mod <bad>obscures sights</bad> of the "
                              "base weapon." ) );
    }

    if( is_gunmod() && has_flag( flag_CONSUMABLE ) &&
        parts->test( iteminfo_parts::DESCRIPTION_GUNMOD_CONSUMABLE ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "* This mod might <bad>suffer wear</bad> when firing "
                              "the base weapon." ) );
    }

    if( has_flag( flag_LEAK_DAM ) && has_flag( flag_RADIOACTIVE ) && damage() > 0
        && parts->test( iteminfo_parts::DESCRIPTION_RADIOACTIVITY_DAMAGED ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "* The casing of this item has <neutral>cracked</neutral>, "
                              "revealing an <info>ominous green glow</info>." ) );
    }

    if( has_flag( flag_LEAK_ALWAYS ) && has_flag( flag_RADIOACTIVE ) &&
        parts->test( iteminfo_parts::DESCRIPTION_RADIOACTIVITY_ALWAYS ) ) {
        info.emplace_back( "DESCRIPTION",
                           _( "* This object is <neutral>surrounded</neutral> by a "
                              "<info>sickly green glow</info>." ) );
    }

    if( is_brewable() || ( !contents.empty() && contents.front().is_brewable() ) ) {
        const item &brewed = !is_brewable() ? contents.front() : *this;
        if( parts->test( iteminfo_parts::DESCRIPTION_BREWABLE_DURATION ) ) {
            const time_duration btime = brewed.brewing_time();
            int btime_i = to_days<int>( btime );
            if( btime <= 2_days ) {
                btime_i = to_hours<int>( btime );
                info.emplace_back( "DESCRIPTION",
                                   string_format( vgettext( "* Once set in a vat, this "
                                                  "will ferment in around %d hour.",
                                                  "* Once set in a vat, this will ferment in "
                                                  "around %d hours.", btime_i ), btime_i ) );
            } else {
                info.emplace_back( "DESCRIPTION",
                                   string_format( vgettext( "* Once set in a vat, this "
                                                  "will ferment in around %d day.",
                                                  "* Once set in a vat, this will ferment in "
                                                  "around %d days.", btime_i ), btime_i ) );
            }
        }
        if( parts->test( iteminfo_parts::DESCRIPTION_BREWABLE_PRODUCTS ) ) {
            for( const itype_id &res : brewed.brewing_results() ) {
                info.emplace_back( "DESCRIPTION",
                                   string_format( _( "* Fermenting this will produce "
                                                     "<neutral>%s</neutral>." ),
                                                  nname( res, brewed.charges ) ) );
            }
        }
    }

    if( parts->test( iteminfo_parts::DESCRIPTION_FAULTS ) ) {
        for( const fault_id &e : faults ) {
            //~ %1$s is the name of a fault and %2$s is the description of the fault
            info.emplace_back( "DESCRIPTION", string_format( _( "* <bad>%1$s</bad>.  %2$s" ),
                               e.obj().name(), e.obj().description() ) );
        }
    }

    // does the item fit in any holsters?
    std::vector<const itype *> holsters = item_controller->find( [this]( const itype & e ) {
        if( !e.can_use( "holster" ) ) {
            return false;
        }
        const holster_actor *ptr = dynamic_cast<const holster_actor *>
                                   ( e.get_use( "holster" )->get_actor_ptr() );
        return ptr->can_holster( *this );
    } );

    if( !holsters.empty() && parts->test( iteminfo_parts::DESCRIPTION_HOLSTERS ) ) {
        insert_separation_line( info );
        info.emplace_back( "DESCRIPTION", _( "<bold>Can be stored in</bold>: " ) +
                           enumerate_as_string( holsters.begin(), holsters.end(),
        []( const itype * e ) {
            return e->nname( 1 );
        } ) );
    }

    if( parts->test( iteminfo_parts::DESCRIPTION_ACTIVATABLE_TRANSFORMATION ) ) {
        for( auto &u : type->use_methods ) {
            const delayed_transform_iuse *tt = dynamic_cast<const delayed_transform_iuse *>
                                               ( u.second.get_actor_ptr() );
            if( tt == nullptr ) {
                continue;
            }
            const int time_to_do = tt->time_to_do( *this );
            if( time_to_do <= 0 ) {
                info.emplace_back( "DESCRIPTION",
                                   _( "It's done and <info>can be activated</info>." ) );
            } else {
                const std::string time = to_string_clipped( time_duration::from_turns( time_to_do ) );
                info.emplace_back( "DESCRIPTION",
                                   string_format( _( "It will be done in %s." ),
                                                  time.c_str() ) );
            }
        }
    }

    if( this->get_var( "die_num_sides", 0 ) != 0 ) {
        info.emplace_back( "DESCRIPTION",
                           string_format( _( "* This item can be used as a <info>die</info>, "
                                             "and has <info>%d</info> sides." ),
                                          static_cast<int>( this->get_var( "die_num_sides",
                                                  0 ) ) ) );
    }

    // Price and barter value
    const int price_preapoc = price( false ) * batch;
    const int price_postapoc = price( true ) * batch;
    if( parts->test( iteminfo_parts::BASE_PRICE ) ) {
        insert_separation_line( info );
        info.emplace_back( "BASE", _( "Price: " ), _( "$<num>" ),
                           iteminfo::is_decimal | iteminfo::lower_is_better | iteminfo::no_newline,
                           static_cast<double>( price_preapoc ) / 100 );
    }
    if( price_preapoc != price_postapoc && parts->test( iteminfo_parts::BASE_BARTER ) ) {
        info.emplace_back( "BASE", space + _( "Barter value: " ), _( "$<num>" ),
                           iteminfo::is_decimal | iteminfo::lower_is_better,
                           static_cast<double>( price_postapoc ) / 100 );
    }

    // Recipes using this item as an ingredient
    if( parts->test( iteminfo_parts::DESCRIPTION_APPLICABLE_RECIPES ) ) {
        itype_id tid = contents.empty() ? typeId() : contents.front().typeId();
        const inventory &crafting_inv = you.crafting_inventory();

        const recipe_subset available_recipe_subset = you.get_available_recipes( crafting_inv, nullptr,
                recipe_filter_by_component( tid ) );
        const std::set<const recipe *> &item_recipes = available_recipe_subset.of_component( tid );

        if( item_recipes.empty() ) {
            insert_separation_line( info );
            info.emplace_back( "DESCRIPTION",
                               _( "You know of nothing you could craft with it." ) );
        } else {
            if( item_recipes.size() > 24 ) {
                insert_separation_line( info );
                info.emplace_back( "DESCRIPTION",
                                   _( "You know dozens of things you could craft with it." ) );
            } else if( item_recipes.size() > 12 ) {
                insert_separation_line( info );
                info.emplace_back( "DESCRIPTION",
                                   _( "You could use it to craft various other things." ) );
            } else {
                // Extract item names from recipes and sort them
                std::vector<std::pair<std::string, bool>> result_names;
                std::ranges::transform(
                    item_recipes,
                    std::back_inserter( result_names ),
                [&crafting_inv]( const recipe * r ) {
                    bool can_make = r->deduped_requirements().can_make_with_inventory(
                                        crafting_inv, r->get_component_filter() );
                    return std::make_pair( r->result_name( /*decorated=*/true ), can_make );
                } );
                std::ranges::sort( result_names, localized_compare );
                const std::string recipes =
                    enumerate_as_string( result_names.begin(), result_names.end(),
                []( const std::pair<std::string, bool> &p ) {
                    if( p.second ) {
                        return p.first;
                    } else {
                        return string_format( "<dark>%s</dark>", p.first );
                    }
                } );
                insert_separation_line( info );
                info.emplace_back( "DESCRIPTION",
                                   string_format( _( "You could use it to craft: %s" ),
                                                  recipes ) );
            }
        }
    }
    if( get_option<bool>( "ENABLE_ASCII_ART_ITEM" ) ) {
        const ascii_art_id art = type->picture_id;
        if( art.is_valid() ) {
            for( const std::string &line : art->picture ) {
                info.emplace_back( "DESCRIPTION", line );
            }
        }
    }
}

std::vector<iteminfo> item::info() const
{
    return info( iteminfo_query::no_conditions, 1, temperature_flag::TEMP_NORMAL );
}

std::vector<iteminfo> item::info( int batch ) const
{
    return info( iteminfo_query::no_conditions, batch, temperature_flag::TEMP_NORMAL );
}

std::vector<iteminfo> item::info( temperature_flag temperature ) const
{
    return info( iteminfo_query::all, 1, temperature );
}

std::vector<iteminfo> item::info( const iteminfo_query &parts_ref, int batch,
                                  temperature_flag temperature ) const
{
    const bool debug = g != nullptr && debug_mode;

    // TODO: Use reference properly
    const iteminfo_query *parts = &parts_ref;
    std::vector<iteminfo> info;

    if( !is_null() ) {
        basic_info( info, parts, batch, debug );
    }

    const item *med_item = nullptr;
    if( is_medication() ) {
        med_item = this;
    } else if( is_med_container() ) {
        med_item = &contents.front();
    }
    if( med_item != nullptr ) {
        med_info( med_item, info, parts, batch, debug );
    }

    if( const item *food_item = get_food() ) {
        food_info( food_item, info, parts, batch, debug, temperature );
    }

    container_info( info, parts, batch, debug );
    contents_info( info, parts, batch, debug );
    combat_info( info, parts, batch, debug );

    magazine_info( info, parts, batch, debug );
    ammo_info( info, parts, batch, debug );

    const item *gun = nullptr;
    if( is_gun() ) {
        gun = this;
        const gun_mode aux = gun_current_mode();
        // if we have an active auxiliary gunmod display stats for this instead
        if( aux && aux->is_gunmod() && aux->is_gun() &&
            parts->test( iteminfo_parts::DESCRIPTION_AUX_GUNMOD_HEADER ) ) {
            gun = &*aux;
            info.emplace_back( "DESCRIPTION",
                               string_format( _( "Stats of the active <info>gunmod (%s)</info> "
                                                 "are shown." ), gun->tname() ) );
        }
    }
    if( gun != nullptr ) {
        gun_info( gun, info, parts, batch, debug );
    }

    gunmod_info( info, parts, batch, debug );
    armor_info( info, parts, batch, debug );
    animal_armor_info( info, parts, batch, debug );
    book_info( info, parts, batch, debug );
    battery_info( info, parts, batch, debug );
    tool_info( info, parts, batch, debug );
    component_info( info, parts, batch, debug );
    qualities_info( info, parts, batch, debug );

    // Uses for item (bandaging quality, holster capacity, grenade activation)
    if( parts->test( iteminfo_parts::DESCRIPTION_USE_METHODS ) ) {
        for( const std::pair<const std::string, use_function> &method : type->use_methods ) {
            insert_separation_line( info );
            method.second.dump_info( *this, info );
        }
    }

    repair_info( info, parts, batch, debug );
    disassembly_info( info, parts, batch, debug );

    final_info( info, parts_ref, batch, debug );

    if( !info.empty() && info.back().sName == "--" ) {
        info.pop_back();
    }

    return info;
}

std::string item::info_string() const
{
    return info_string( iteminfo_query::all, 1 );
}

std::string item::info_string( const iteminfo_query &parts, int batch,
                               temperature_flag temperature ) const
{
    std::vector<iteminfo> item_info = info( parts, batch, temperature );
    return format_item_info( item_info, {} );
}

std::map<gunmod_location, int> item::get_mod_locations() const
{
    std::map<gunmod_location, int> mod_locations = type->gun->valid_mod_locations;

    for( const item *mod : gunmods() ) {
        if( !mod->type->gunmod->add_mod.empty() ) {
            std::map<gunmod_location, int> add_locations = mod->type->gunmod->add_mod;

            for( const std::pair<const gunmod_location, int> &add_location : add_locations ) {
                mod_locations[add_location.first] += add_location.second;
            }
        }
    }

    return mod_locations;
}

int item::get_free_mod_locations( const gunmod_location &location ) const
{
    if( !is_gun() ) {
        return 0;
    }

    std::map<gunmod_location, int> mod_locations = get_mod_locations();

    const auto loc = mod_locations.find( location );
    if( loc == mod_locations.end() ) {
        return 0;
    }
    int result = loc->second;
    for( const item *elem : contents.all_items_top() ) {
        const cata::value_ptr<islot_gunmod> &mod = elem->type->gunmod;
        if( mod && mod->location == location ) {
            result--;
        }
    }
    return result;
}

int item::engine_displacement() const
{
    return type->engine ? type->engine->displacement : 0;
}

const std::string &item::symbol() const
{
    return type->sym;
}

nc_color item::color_in_inventory() const
{
    return item::color_in_inventory( get_avatar() );
}

nc_color item::color_in_inventory( const player &p ) const
{
    // Only item not otherwise colored gets colored as favorite
    nc_color ret = is_favorite ? c_white : c_light_gray;
    if( type->can_use( "learn_spell" ) ) {
        const use_function *iuse = get_use( "learn_spell" );
        const learn_spell_actor *actor_ptr =
            static_cast<const learn_spell_actor *>( iuse->get_actor_ptr() );
        for( const std::string &spell_id_str : actor_ptr->spells ) {
            const spell_id sp_id( spell_id_str );
            if( p.magic->knows_spell( sp_id ) && !p.magic->get_spell( sp_id ).is_max_level() ) {
                ret = c_yellow;
            }
            if( !p.magic->knows_spell( sp_id ) && p.magic->can_learn_spell( p, sp_id ) ) {
                return c_light_blue;
            }
        }
    } else if( has_flag( flag_WET ) ) {
        ret = c_cyan;
    } else if( has_flag( flag_LITCIG ) ) {
        ret = c_red;
    } else if( is_armor() && p.has_trait( trait_WOOLALLERGY ) &&
               ( made_of( material_id( "wool" ) ) || has_own_flag( flag_wooled ) ) ) {
        ret = c_red;
    } else if( has_own_flag( flag_DIRTY ) ) {
        ret = c_brown;
    } else if( is_bionic() ) {
        if( ( !p.has_bionic( type->bionic->id ) &&
              !character_funcs::has_upgraded_bionic( p, type->bionic->id ) ) ||
            type->bionic->id->has_flag( flag_MULTIINSTALL ) ) {
            ret = p.bionic_installation_issues( type->bionic->id ).empty() ? c_green : c_red;
        } else if( !has_fault( fault_bionic_nonsterile ) ) {
            ret = c_dark_gray;
        }
    } else if( has_flag( flag_LEAK_DAM ) && has_flag( flag_RADIOACTIVE ) && damage() > 0 ) {
        ret = c_light_green;
    } else if( is_active() && !is_food() && !is_food_container() && !is_corpse() ) {
        // Active items show up as yellow
        ret = c_yellow;
    } else if( is_corpse() && ( can_revive() || corpse->zombify_into ) && !has_flag( flag_PULPED ) ) {
        // Only reviving corpses are yellow
        ret = c_yellow;
    } else if( const item *food = get_food() ) {
        const bool preserves = type->container && type->container->preserves;

        // Give color priority to allergy (allergy > inedible by freeze or other conditions)
        // TODO: refactor u.will_eat to let this section handle coloring priority without duplicating code.
        if( p.allergy_type( *food ) != morale_type( "morale_null" ) ) {
            return c_red;
        }

        // Default: permafood, drugs
        // Brown: rotten (for non-saprophages) or non-rotten (for saprophages)
        // Dark gray: inedible
        // Red: morale penalty
        // Yellow: will rot soon
        // Cyan: will rot eventually
        const ret_val<edible_rating> rating = p.will_eat( *food );
        // TODO: More colors
        switch( rating.value() ) {
            case edible_rating::edible:
            case edible_rating::too_full:
                if( preserves ) {
                    // Nothing, canned food won't rot
                } else if( food->is_going_bad() ) {
                    ret = c_yellow;
                } else if( food->goes_bad() ) {
                    ret = c_cyan;
                }
                break;
            case edible_rating::inedible:
            case edible_rating::inedible_mutation:
                ret = c_dark_gray;
                break;
            case edible_rating::allergy:
            case edible_rating::allergy_weak:
            case edible_rating::cannibalism:
                ret = c_red;
                break;
            case edible_rating::rotten:
                ret = c_brown;
                break;
            case edible_rating::nausea:
            case edible_rating::bloated:
                ret = c_pink;
                break;
            case edible_rating::no_tool:
                break;
        }
    } else if( is_gun() ) {
        // Guns are green if you are carrying ammo for them
        // ltred if you have ammo but no mags
        // Gun with integrated mag counts as both
        for( const ammotype &at : ammo_types() ) {
            // get_ammo finds uncontained ammo, find_ammo finds ammo in magazines
            bool has_ammo = !character_funcs::get_ammo_items( p, at ).empty() ||
                            !character_funcs::find_ammo_items_or_mags( p, *this, false, -1 ).empty();
            bool has_mag = magazine_integral() ||
                           !character_funcs::find_ammo_items_or_mags( p, *this, true, -1 ).empty();
            if( has_ammo && has_mag ) {
                ret = c_green;
                break;
            } else if( has_ammo || has_mag ) {
                ret = c_light_red;
                break;
            }
        }
    } else if( is_ammo() ) {
        // Likewise, ammo is green if you have guns that use it
        // ltred if you have the gun but no mags
        // Gun with integrated mag counts as both
        bool has_gun = p.has_item_with( [this]( const item & i ) {
            return i.is_gun() && i.ammo_types().contains( ammo_type() );
        } );
        bool has_mag = p.has_item_with( [this]( const item & i ) {
            return ( i.is_gun() && i.magazine_integral() && i.ammo_types().contains( ammo_type() ) ) ||
                   ( i.is_magazine() && i.ammo_types().contains( ammo_type() ) );
        } );
        if( has_gun && has_mag ) {
            ret = c_green;
        } else if( has_gun || has_mag ) {
            ret = c_light_red;
        }
    } else if( is_magazine() ) {
        // Magazines are green if you have guns and ammo for them
        // ltred if you have one but not the other
        bool has_gun = p.has_item_with( [this]( const item & it ) {
            return it.is_gun() && it.magazine_compatible().contains( typeId() );
        } );
        bool has_ammo = !character_funcs::find_ammo_items_or_mags( p, *this, false, -1 ).empty();
        if( has_gun && has_ammo ) {
            ret = c_green;
        } else if( has_gun || has_ammo ) {
            ret = c_light_red;
        }
    } else if( is_book() ) {
        const islot_book &tmp = *type->book;
        // Player doesn't actually interested if NPC has identified book yet.
        // So we check identification for human avatar.
        if( get_avatar().has_identified( typeId() ) ) {
            if( tmp.skill && // Book can improve skill: blue
                p.get_skill_level_object( tmp.skill ).can_train() &&
                p.get_skill_level( tmp.skill ) >= tmp.req &&
                p.get_skill_level( tmp.skill ) < tmp.level ) {
                ret = c_light_blue;
            } else if( type->can_use( "MA_MANUAL" ) &&
                       !p.martial_arts_data->has_martialart( martial_art_learned_from( *type ) ) ) {
                ret = c_light_blue;
            } else if( tmp.skill && // Book can't improve skill right now, but maybe later: pink
                       p.get_skill_level_object( tmp.skill ).can_train() &&
                       p.get_skill_level( tmp.skill ) < tmp.level ) {
                ret = c_pink;
            } else if( !p.studied_all_recipes(
                           *type ) ) { // Book can't improve skill anymore, but has more recipes: yellow
                ret = c_yellow;
            }
        } else if( tmp.skill || type->can_use( "MA_MANUAL" ) ) {
            // Book can teach you something and hasn't been identified yet
            ret = c_red;
        } else {
            // "just for fun" book that they haven't read yet
            ret = c_magenta;
        }
    }
    return ret;
}

void item::on_wear( Character &who )
{
    if( is_sided() && get_side() == side::BOTH ) {
        if( has_flag( flag_SPLINT ) ) {
            set_side( side::LEFT );
            if( ( covers( bodypart_id( "leg_l" ) ) && who.is_limb_broken( bodypart_id( "leg_r" ) ) &&
                  !who.worn_with_flag( flag_SPLINT, bodypart_id( "leg_r" ) ) ) ||
                ( covers( bodypart_id( "arm_l" ) ) && who.is_limb_broken( bodypart_id( "arm_r" ) ) &&
                  !who.worn_with_flag( flag_SPLINT, bodypart_id( "arm_r" ) ) ) ) {
                set_side( side::RIGHT );
            }
        } else if( has_flag( flag_POWERARMOR_MOD ) ) {
            // for power armor mods, wear on side with least mods
            std::vector< std::pair< bodypart_str_id, int > > mod_parts;
            int lhs = 0;
            int rhs = 0;
            const auto &all_bps = who.get_all_body_parts();
            for( const bodypart_id &bp : all_bps ) {
                if( get_covered_body_parts().test( bp.id() ) ) {
                    mod_parts.emplace_back( bp, 0 );
                }
            }
            for( auto &elem : who.worn ) {
                for( std::pair< bodypart_str_id, int > &mod_part : mod_parts ) {
                    const bodypart_str_id &bp = mod_part.first;
                    if( elem->get_covered_body_parts().test( bp ) &&
                        elem->has_flag( flag_POWERARMOR_MOD ) ) {
                        if( elem->is_sided() && elem->get_side() == bp->part_side ) {
                            mod_part.second++;
                            continue;
                        }
                        mod_part.second++;
                    }
                }
            }
            for( std::pair< bodypart_str_id, int > &mod_part : mod_parts ) {
                const bodypart_str_id &bp = mod_part.first;
                if( bp->part_side == side::LEFT && mod_part.second > lhs ) {
                    lhs = mod_part.second;
                } else if( bp->part_side == side::RIGHT && mod_part.second > rhs ) {
                    rhs = mod_part.second;
                }
            }
            set_side( ( lhs > rhs ) ? side::RIGHT : side::LEFT );
        } else {
            // for sided items wear the item on the side which results in least encumbrance
            const auto &all_bps = who.get_all_body_parts();
            int lhs = 0;
            int rhs = 0;
            set_side( side::LEFT );
            const char_encumbrance_data left_enc = who.get_encumbrance( *this );
            for( const bodypart_id &bp : all_bps ) {
                lhs += left_enc.elems.at( bp.id() ).encumbrance;
            }

            set_side( side::RIGHT );
            const char_encumbrance_data right_enc = who.get_encumbrance( *this );
            for( const bodypart_id &bp : all_bps ) {
                rhs += right_enc.elems.at( bp.id() ).encumbrance;
            }

            set_side( lhs <= rhs ? side::LEFT : side::RIGHT );
        }
    }

    if( type->can_use( "set_transformed" ) ) {
        bool transform = false;
        const set_transformed_iuse *actor = dynamic_cast<const set_transformed_iuse *>
                                            ( this->get_use( "set_transformed" )->get_actor_ptr() );
        if( actor == nullptr ) {
            debugmsg( "iuse_actor type descriptor and actual type mismatch" );
            return;
        }
        flag_id transform_flag( actor->dependencies );
        for( const auto &elem : who.worn ) {
            if( elem->has_flag( transform_flag ) && elem->is_active() != is_active() ) {
                transform = true;
            }
        }
        if( transform && actor->restricted ) {
            actor->bypass( *who.as_player(), *this, false, who.pos() );
        }
    }

    // TODO: artifacts currently only work with the player character
    if( &who == &get_avatar() && type->artifact ) {
        g->add_artifact_messages( type->artifact->effects_worn );
    }
    // if game is loaded - don't want ownership assigned during char creation
    if( get_avatar().getID().is_valid() ) {
        handle_pickup_ownership( who );
    }
    who.on_item_wear( *this );
}

void item::on_takeoff( Character &who )
{
    who.on_item_takeoff( *this );

    if( is_sided() ) {
        set_side( side::BOTH );
    }

    // if power armor, no power_draw and active, shut down.
    if( type->can_use( "set_transformed" ) && is_active() ) {
        const set_transformed_iuse *actor = dynamic_cast<const set_transformed_iuse *>
                                            ( this->get_use( "set_transformed" )->get_actor_ptr() );
        if( actor == nullptr ) {
            debugmsg( "iuse_actor type descriptor and actual type mismatch" );
            return;
        }
        actor->bypass( *who.as_player(), *this, false, who.pos() );
    }
}

void item::on_wield( player &p, int mv )
{
    // TODO: artifacts currently only work with the player character
    if( &p == &get_avatar() && type->artifact ) {
        g->add_artifact_messages( type->artifact->effects_wielded );
    }

    // weapons with bayonet/bipod or other generic "unhandiness"
    if( has_flag( flag_SLOW_WIELD ) && !is_gunmod() ) {
        float d = 32.0; // arbitrary linear scaling factor
        if( is_gun() ) {
            d /= std::max( p.get_skill_level( gun_skill() ), 1 );
        } else if( is_melee() ) {
            d /= std::max( p.get_skill_level( melee_skill() ), 1 );
        }

        int penalty = get_var( "volume", volume() / units::legacy_volume_factor ) * d;
        p.moves -= penalty;
        mv += penalty;
    }

    // firearms with a folding stock or tool/melee without collapse/retract iuse
    if( has_flag( flag_NEEDS_UNFOLD ) && !is_gunmod() ) {
        int penalty = 50; // 200-300 for guns, 50-150 for melee, 50 as fallback
        if( is_gun() ) {
            penalty = std::max( 0, 300 - p.get_skill_level( gun_skill() ) * 10 );
        } else if( is_melee() ) {
            penalty = std::max( 0, 150 - p.get_skill_level( melee_skill() ) * 10 );
        }

        p.moves -= penalty;
        mv += penalty;
    }

    std::string msg;

    if( mv > 500 ) {
        msg = _( "It takes you a long time to wield your %s." );
    } else if( mv > 250 ) {
        msg = _( "It takes you several seconds to wield your %s." );
    } else if( mv > 100 ) {
        msg = _( "It takes you a couple seconds to wield your %s." );
    } else if( mv > 50 ) {
        msg = _( "It takes you a moment to wield your %s." );
    } else {
        msg = _( "You wield your %s." );
    }
    // if game is loaded - don't want ownership assigned during char creation
    if( p.getID().is_valid() ) {
        handle_pickup_ownership( p );
    }
    p.add_msg_if_player( m_neutral, msg, tname() );

    if( !p.martial_arts_data->selected_is_none() ) {
        p.martial_arts_data->martialart_use_message( p );
    }

    // Update encumbrance in case we were wearing it
    p.flag_encumbrance();
}

void item::handle_pickup_ownership( Character &c )
{
    if( is_owned_by( c ) ) {
        return;
    }
    // Add ownership to item if unowned
    if( owner.is_null() ) {
        set_owner( c );
    } else {
        Character &you = get_player_character();
        if( !is_owned_by( c ) && &c == &you ) {
            std::vector<npc *> witnesses;
            for( npc &elem : g->all_npcs() ) {
                // If they already want to murder you, no point in confronting you about theft
                if( rl_dist( elem.pos(), you.pos() ) < MAX_VIEW_DISTANCE && elem.get_faction() &&
                    is_owned_by( elem ) && elem.sees( you.pos() ) && !elem.guaranteed_hostile() ) {
                    elem.say( "<witnessed_thievery>", 7 );
                    npc *npc_to_add = &elem;
                    witnesses.push_back( npc_to_add );
                }
            }
            if( !witnesses.empty() ) {
                set_old_owner( get_owner() );
                // Make sure there is only one witness
                for( npc &guy : g->all_npcs() ) {
                    if( guy.get_attitude() == NPCATT_RECOVER_GOODS ) {
                        guy.set_attitude( NPCATT_NULL );
                    }
                }
                random_entry( witnesses )->set_attitude( NPCATT_RECOVER_GOODS );
                // Notify the activity that we got a witness
                if( c.activity && !c.activity->is_null() && c.activity->id() == ACT_PICKUP ) {
                    c.activity->str_values.clear();
                    c.activity->str_values.emplace_back( has_thievery_witness );
                }
            }
            set_owner( c );
        }
    }
}

void item::on_pickup( Character &who )
{
    // Fake characters are used to determine pickup weight and volume
    if( who.is_fake() ) {
        return;
    }
    avatar &you = get_avatar();
    // TODO: artifacts currently only work with the player character
    if( &who == &you && type->artifact ) {
        g->add_artifact_messages( type->artifact->effects_carried );
    }
    // if game is loaded - don't want ownership assigned during char creation
    if( you.getID().is_valid() ) {
        handle_pickup_ownership( who );
    }
    if( is_bucket_nonempty() ) {
        contents.spill_contents( who.pos() );
    }

    who.flag_encumbrance();
}

void item::on_contents_changed()
{
    if( is_non_resealable_container() ) {
        convert( type->container->unseals_into );
    }

    encumbrance_update_ = true;
}

void item::on_damage( int qty, damage_type )
{
    if( is_corpse() && qty + damage_ >= max_damage() ) {
        set_flag( flag_PULPED );
    }
}

std::string item::tname( unsigned int quantity, bool with_prefix, unsigned int truncate ) const
{
    int dirt_level = get_var( "dirt", 0 ) / 2000;
    std::string dirt_symbol;
    // TODO: MATERIALS put this in json

    // these symbols are unicode square characeters of different heights, representing a rough
    // estimation of fouling in a gun. This appears instead of "faulty"
    // since most guns will have some level of fouling in them, and usually it is not a big deal.
    switch( dirt_level ) {
        case 0:
            dirt_symbol = "";
            break;
        case 1:
            dirt_symbol = "<color_white>\u2581</color>";
            break;
        case 2:
            dirt_symbol = "<color_light_gray>\u2583</color>";
            break;
        case 3:
            dirt_symbol = "<color_light_gray>\u2585</color>";
            break;
        case 4:
            dirt_symbol = "<color_dark_gray>\u2587</color>";
            break;
        case 5:
            dirt_symbol = "<color_brown>\u2588</color>";
            break;
        default:
            dirt_symbol = "";
    }
    std::string damtext;

    // for portions of string that have <color_ etc in them, this aims to truncate the whole string correctly
    unsigned int truncate_override = 0;

    if( ( damage() != 0 || ( get_option<bool>( "ITEM_HEALTH_BAR" ) && is_armor() ) ) && !is_null() &&
        with_prefix ) {
        damtext = durability_indicator();
        if( get_option<bool>( "ITEM_HEALTH_BAR" ) ) {
            // get the utf8 width of the tags
            truncate_override = utf8_width( damtext, false ) - utf8_width( damtext, true );
        }
    }

    if( !faults.empty() ) {
        const bool silent = std::ranges::any_of( faults, []( const fault_id & f ) -> bool { return f->has_flag( "SILENT" ); } );
        if( silent ) {
            damtext.insert( 0, dirt_symbol );
        } else {
            damtext.insert( 0, _( "faulty " ) + dirt_symbol );
        }
    }

    std::string vehtext;
    if( is_engine() && engine_displacement() > 0 ) {
        vehtext = string_format( pgettext( "vehicle adjective", "%2.1fL " ),
                                 engine_displacement() / 100.0f );

    } else if( is_wheel() && type->wheel->diameter > 0 ) {
        vehtext = string_format( pgettext( "vehicle adjective", "%d\" " ), type->wheel->diameter );
    }

    std::string burntext;
    if( with_prefix && !made_of( LIQUID ) ) {
        if( volume() >= 1_liter && burnt * 125_ml >= volume() ) {
            burntext = pgettext( "burnt adjective", "badly burnt " );
        } else if( burnt > 0 ) {
            burntext = pgettext( "burnt adjective", "burnt " );
        }
    }

    std::string maintext;
    if( is_corpse() || item_vars.contains( "name" ) ) {
        maintext = type_name( quantity );
    } else if( is_craft() ) {
        maintext = string_format( _( "in progress %s" ), craft_data_->making->result_name() );
        if( charges > 1 ) {
            maintext += string_format( " (%d)", charges );
        }
        const int percent_progress = item_counter / 100000;
        maintext += string_format( " (%d%%)", percent_progress );
    } else {
        std::string labeltext = label( quantity );

        int modamt = 0;
        if( is_tool() ) {
            modamt += toolmods().size();
        }
        if( is_gun() ) {
            for( const item *mod : gunmods() ) {
                if( !type->gun->built_in_mods.contains( mod->typeId() ) ) {
                    modamt++;
                }
            }
        }
        if( is_armor() && has_clothing_mod() ) {
            modamt++;
        }
        if( modamt ) {
            labeltext += string_format( "+%d", modamt );
        }

        if( is_gun() || is_tool() || is_magazine() ) {
            maintext = labeltext;
        } else if( contents.num_item_stacks() == 1 ) {
            const item &contents_item = contents.front();
            const unsigned contents_count =
                ( ( contents_item.made_of( LIQUID ) || contents_item.is_food() ) &&
                  contents_item.charges > 1 )
                ? contents_item.charges
                : quantity;
            maintext = string_format( pgettext( "item name", "%2$s (%1$s)" ), labeltext,
                                      contents_item.tname( contents_count, with_prefix ) );
        } else if( !contents.empty() ) {
            maintext = string_format( vpgettext( "item name",
                                                 //~ %1$s: item name, %2$zd: content size
                                                 "%1$s with %2$zd item",
                                                 "%1$s with %2$zd items", contents.num_item_stacks() ),
                                      labeltext, contents.num_item_stacks() );
        } else {
            maintext = labeltext;
        }
    }

    avatar &you = get_avatar();
    std::string tagtext;
    if( is_food() ) {
        if( has_flag( flag_HIDDEN_POISON ) && you.get_skill_level( skill_survival ) >= 3 ) {
            tagtext += _( " (poisonous)" );
        } else if( has_flag( flag_HIDDEN_HALLU ) && you.get_skill_level( skill_survival ) >= 5 ) {
            tagtext += _( " (hallucinogenic)" );
        }
    }
    if( is_book() ) {
        if( !you.has_identified( typeId() ) ) {
            tagtext += _( " (unread)" );
        }
    }
    if( has_var( "bionics_scanned_by" ) ) {
        if( has_flag( flag_CBM_SCANNED ) ) {
            tagtext += _( " (bionic detected)" );
        } else {
            tagtext += _( " (scanned)" );
        }
    }
    if( has_flag( flag_ETHEREAL_ITEM ) ) {
        tagtext += string_format( _( " (%s turns)" ), get_var( "ethereal" ) );
    } else if( goes_bad() || is_food() ) {
        if( has_own_flag( flag_DIRTY ) ) {
            tagtext += _( " (dirty)" );
        } else if( rotten() ) {
            tagtext += _( " (rotten)" );
        } else if( is_going_bad() ) {
            tagtext += _( " (old)" );
        } else if( is_fresh() ) {
            tagtext += _( " (fresh)" );
        }
        if( is_loaded() ) {
            const auto temp = rot::temperature_flag_for_location( get_map(), *this );
            if( temp == temperature_flag::TEMP_FREEZER ) {
                tagtext += _( " (very cold)" );
            } else if( temp == temperature_flag::TEMP_FRIDGE || temp == temperature_flag::TEMP_ROOT_CELLAR ) {
                tagtext += _( " (cold)" );
            }
        }
    }

    if( has_flag( flag_resized_large ) ) {
        tagtext += _( " (XL)" );
    } else if( has_flag( flag_resized_small ) ) {
        tagtext += _( " (XS)" );
    }
    const sizing sizing_level = get_sizing( you );

    if( sizing_level == sizing::human_sized_small_char ) {
        tagtext += _( " (too big)" );
    } else if( sizing_level == sizing::big_sized_small_char ) {
        tagtext += _( " (huge!)" );
    } else if( sizing_level == sizing::human_sized_big_char ||
               sizing_level == sizing::small_sized_human_char ) {
        tagtext += _( " (too small)" );
    } else if( sizing_level == sizing::small_sized_big_char ) {
        tagtext += _( " (tiny!)" );
    } else if( !has_flag( flag_FIT ) && has_flag( flag_VARSIZE ) ) {
        tagtext += _( " (poor fit)" );
    }

    if( is_bionic() && !has_fault( fault_bionic_nonsterile ) ) {
        tagtext += _( " (sterile)" );
    }

    if( is_tool() && has_flag( flag_USE_UPS ) && !has_flag( flag_NAT_UPS ) ) {
        tagtext += _( " (UPS)" );
    }
    if( is_tool() && has_flag( flag_HEATS_FOOD ) ) {
        tagtext += _( " (heats)" );
    }


    if( has_var( "specimen_sample" ) ) {
        const std::string specimen_name = get_var( "specimen_name" );
        const int progress = get_var( "specimen_sample_progress", 0 );
        const int size = get_var( "specimen_size", 0 );
        if( typeId() == itype_genome_drive && size > 0 && progress < size ) {
            tagtext += string_format( " (%s [%d/%d])", specimen_name, progress, size );
        } else {
            tagtext += string_format( " (%s)", specimen_name );
        }
    }

    if( has_var( "place_monster_override" ) ) {
        tagtext += string_format( " (%s)", get_var( "place_monster_override_name" ) );
    }

    if( has_var( "NANOFAB_GROUP_ID" ) ) {
        std::string group_id_str = get_var( "NANOFAB_GROUP_ID" );
        const std::string prefix = "nanofab_template_";

        // Remove prefix if it exists
        if( group_id_str.rfind( prefix, 0 ) == 0 ) {
            group_id_str = group_id_str.substr( prefix.size() );
        }

        // Replace underscores with spaces
        std::replace( group_id_str.begin(), group_id_str.end(), '_', ' ' );

        // Append to tag text
        tagtext += string_format( " (%s)", group_id_str );
    } else if( has_var( "NANOFAB_ITEM_ID" ) ) {
        itype_id item = itype_id( get_var( "NANOFAB_ITEM_ID" ) );
        tagtext += string_format( " (%s [%d])", nname( item ), std::max( 1, item->volume / 250_ml ) * 5 );
    }

    if( has_flag( flag_RADIO_MOD ) ) {
        tagtext += _( " (radio:" );
        if( has_flag( flag_RADIOSIGNAL_1 ) ) {
            tagtext += pgettext( "The radio mod is associated with the [R]ed button.", "R)" );
        } else if( has_flag( flag_RADIOSIGNAL_2 ) ) {
            tagtext += pgettext( "The radio mod is associated with the [B]lue button.", "B)" );
        } else if( has_flag( flag_RADIOSIGNAL_3 ) ) {
            tagtext += pgettext( "The radio mod is associated with the [G]reen button.", "G)" );
        } else {
            debugmsg( "Why is the radio neither red, blue, nor green?" );
            tagtext += "?)";
        }
    }

    if( has_flag( flag_WET ) ) {
        tagtext += _( " (wet)" );
    }
    if( already_used_by_player( you ) ) {
        tagtext += _( " (used)" );
    }
    if( has_flag( flag_IS_UPS ) && get_var( "cable" ) == "plugged_in" ) {
        tagtext += _( " (plugged in)" );
    }
    if( has_flag( flag_SPAWN_FRIENDLY ) ) {
        tagtext += _( " (friendly)" );
    }

    if( is_favorite ) {
        tagtext += _( " *" ); // Display asterisk for favorite items
    }

    std::string modtext;
    if( gunmod_find( itype_barrel_small ) ) {
        modtext += _( "sawn-off " );
    }
    if( gunmod_find( itype_stock_small ) ) {
        modtext += _( "pistol " );
    }
    if( has_flag( flag_DIAMOND ) ) {
        modtext += std::string( pgettext( "Adjective, as in diamond katana", "diamond" ) ) + " ";
    }

    //~ This is a string to construct the item name as it is displayed. This format string has been added for maximum flexibility. The strings are: %1$s: Damage text (e.g. "bruised"). %2$s: burn adjectives (e.g. "burnt"). %3$s: tool modifier text (e.g. "atomic"). %4$s: vehicle part text (e.g. "3.8-Liter"). $5$s: main item text (e.g. "apple"). %6s: tags (e.g. "(wet) (poor fit)").
    std::string ret = string_format( _( "%1$s%2$s%3$s%4$s%5$s%6$s" ), damtext, burntext, modtext,
                                     vehtext, maintext, tagtext );

    if( truncate != 0 ) {
        ret = utf8_truncate( ret, truncate + truncate_override );
    }

    if( item_vars.contains( "item_note" ) ) {
        //~ %s is an item name. This style is used to denote items with notes.
        return string_format( _( "*%s*" ), ret );
    } else {
        return ret;
    }
}

std::string item::display_money( unsigned int quantity, unsigned int total,
                                 const std::optional<unsigned int> &selected ) const
{
    if( selected ) {
        //~ This is a string to display the selected and total amount of money in a stack of cash cards.
        //~ %1$s is the display name of cash cards.
        //~ %2$s is the total amount of money.
        //~ %3$s is the selected amount of money.
        //~ Example: "cash cards $15.35 of $20.48"
        return string_format( pgettext( "cash card and money", "%1$s %3$s of %2$s" ), tname( quantity ),
                              format_money( total ), format_money( *selected ) );
    } else {
        //~ This is a string to display the total amount of money in a stack of cash cards.
        //~ %1$s is the display name of cash cards.
        //~ %2$s is the total amount of money on the cash cards.
        //~ Example: "cash cards $20.48"
        return string_format( pgettext( "cash card and money", "%1$s %2$s" ), tname( quantity ),
                              format_money( total ) );
    }
}

std::string item::display_name( unsigned int quantity ) const
{
    std::string name = tname( quantity );
    std::string sidetxt;
    std::string amt;

    switch( get_side() ) {
        case side::BOTH:
        case side::num_sides:
            break;
        case side::LEFT:
            sidetxt = string_format( " (%s)", _( "left" ) );
            break;
        case side::RIGHT:
            sidetxt = string_format( " (%s)", _( "right" ) );
            break;
    }
    avatar &you = get_avatar();
    static const itype_id itype_battery( "battery" );
    int amount = 0;
    int max_amount = 0;
    bool has_item = is_container() && contents.num_item_stacks() == 1;
    bool has_ammo = is_ammo_container() && contents.num_item_stacks() == 1;
    bool contains = has_item || has_ammo;
    bool show_amt = false;
    // We should handle infinite charges properly in all cases.
    if( contains ) {
        amount = contents.front().charges;
        max_amount = contents.front().charges_per_volume( get_container_capacity() );
    } else if( is_book() && get_chapters() > 0 ) {
        // a book which has remaining unread chapters
        amount = get_remaining_chapters( you );
    } else if( ammo_capacity() > 0 ) {
        // anything that can be reloaded including tools, magazines, guns and auxiliary gunmods
        // but excluding bows etc., which have ammo, but can't be reloaded
        amount = ammo_remaining();
        max_amount = ammo_capacity();
        show_amt = !has_flag( flag_RELOAD_AND_SHOOT );
    } else if( count_by_charges() && !has_infinite_charges() ) {
        // A chargeable item
        amount = charges;
        max_amount = ammo_capacity();
    } else if( is_battery() ) {
        show_amt = true;
        amount = to_joule( energy_remaining() );
        max_amount = to_joule( type->battery->max_capacity );
    }

    std::string ammotext;
    if( ( ( is_gun() && ammo_required() ) || is_magazine() ) && get_option<bool>( "AMMO_IN_NAMES" ) ) {
        if( !ammo_current().is_null() ) {
            ammotext = ammo_current()->nname( 1 );
        } else {
            ammotext = ammotype( *ammo_types().begin() )->name();
        }
    }

    if( amount || show_amt ) {
        if( is_money() ) {
            amt = string_format( " $%.2f", amount / 100.0 );
        } else {
            if( !ammotext.empty() ) {
                ammotext = " " + ammotext;
            }

            if( max_amount != 0 ) {
                if( !contains ) {
                    int percentage = ( static_cast<float>( amount ) / max_amount ) * 100;
                    nc_color color = c_green;
                    if( percentage >= 100 ) {
                        color = c_green;
                    } else if( percentage >= 75 ) {
                        color = c_light_green;
                    } else if( percentage >= 50 ) {
                        color = c_yellow;
                    } else if( percentage >= 25 ) {
                        color = c_light_red;
                    } else if( percentage > 0 ) {
                        color = c_red;
                    } else if( percentage <= 0 ) {
                        color = c_light_gray;
                    }
                    amt = colorize( string_format( " (%i/%i%s)", amount, max_amount, ammotext ),
                                    color );
                } else {
                    amt = string_format( " (%i/%i%s)", amount, max_amount, ammotext );
                }

            } else {
                amt = string_format( " (%i%s)", amount, ammotext );
            }
        }
    } else if( !ammotext.empty() ) {
        amt = " (" + ammotext + ")";
    }

    // HACK: This is a hack to prevent possible crashing when displaying maps as items during character creation
    if( is_map() && calendar::turn != calendar::turn_zero ) {
        // TODO: fix point types
        tripoint map_pos_omt =
            get_var( "reveal_map_center_omt", you.global_omt_location().raw() );
        tripoint_abs_sm map_pos =
            project_to<coords::sm>( tripoint_abs_omt( map_pos_omt ) );
        const city *c = overmap_buffer.closest_city( map_pos ).city;
        if( c != nullptr ) {
            name = string_format( "%s %s", c->name, name );
        }
    }

    return string_format( "%s%s%s", name, sidetxt, amt );
}

std::string item::debug_name() const
{
    return typeId().str();
}

nc_color item::color() const
{
    if( is_null() ) {
        return c_black;
    }
    if( is_corpse() ) {
        return corpse->color;
    }
    return type->color;
}

auto item::price( bool practical ) const -> float
{
    float res = 0;

    visit_items( [&res, practical]( const item * e ) {
        if( e->rotten() ) {
            // TODO: Special case things that stay useful when rotten
            return VisitResponse::NEXT;
        }

        float child = units::to_cent( practical ? e->type->price_post : e->type->price );
        if( e->damage() > 0 ) {
            // maximal damage level is 4, maximal reduction is 40% of the value.
            child -= child * static_cast<double>( e->damage_level( 4 ) ) / 10;
        }

        if( e->count_by_charges() || e->made_of( LIQUID ) ) {
            // price from json data is for default-sized stack
            child *= e->charges / static_cast<double>( e->type->stack_size );

        } else if( e->magazine_integral() && e->ammo_remaining() && e->ammo_data() ) {
            // items with integral magazines may contain ammunition which can affect the price
            child += item( e->ammo_data(), calendar::turn, e->charges ).price( practical );

        } else if( e->is_tool() && e->ammo_types().empty() && e->ammo_capacity() ) {
            // if tool has no ammo (e.g. spray can) reduce price proportional to remaining charges
            child *= e->ammo_remaining() / static_cast<double>( std::max( e->type->charges_default(), 1 ) );
        }

        res += child;
        return VisitResponse::NEXT;
    } );

    return res;
}

// TODO: MATERIALS add a density field to materials.json
units::mass item::weight( bool include_contents, bool integral ) const
{
    if( is_null() ) {
        return 0_gram;
    }

    // Items that don't drop aren't really there, they're items just for ease of implementation
    if( has_flag( flag_NO_DROP ) ) {
        return 0_gram;
    }

    if( is_craft() ) {
        units::mass ret = 0_gram;
        for( const item * const &it : components ) {
            ret += it->weight();
        }
        return ret;
    }

    units::mass ret;
    std::string local_str_mass = integral ? get_var( "integral_weight" ) : get_var( "weight" );
    if( local_str_mass.empty() ) {
        ret = integral ? type->integral_weight : type->weight;
    } else {
        ret = units::from_milligram( std::stoll( local_str_mass ) );
    }

    if( has_flag( flag_REDUCED_WEIGHT ) ) {
        ret *= 0.75;
    }

    // if this is a gun apply all of its gunmods' weight multipliers
    if( is_gun() ) {
        for( const item *mod : gunmods() ) {
            ret *= mod->type->gunmod->weight_multiplier;
        }
    }

    if( count_by_charges() ) {
        ret *= charges;

    } else if( is_corpse() ) {
        assert( corpse ); // To appease static analysis
        ret = corpse->weight;
        if( has_flag( flag_FIELD_DRESS ) || has_flag( flag_FIELD_DRESS_FAILED ) ) {
            ret *= 0.75;
        }
        if( has_flag( flag_QUARTERED ) ) {
            ret /= 4;
        }
        if( has_flag( flag_GIBBED ) ) {
            ret *= 0.85;
        }
        if( has_flag( flag_SKINNED ) ) {
            ret *= 0.85;
        }

    } else if( magazine_integral() && !is_magazine() ) {
        if( ammo_current() == itype_plut_cell ) {
            units::mass w = ( *ammo_types().begin() )->default_ammotype()->weight;
            ret += ammo_remaining() * w / PLUTONIUM_CHARGES;
        } else if( ammo_data() ) {
            ret += ammo_remaining() * ammo_data()->weight;
        }
    }

    // if this is an ammo belt add the weight of any implicitly contained linkages
    if( is_magazine() ) {
        const auto &linkage = type->magazine->linkage;
        if( linkage ) {
            item links( *linkage );
            links.charges = ammo_remaining();
            ret += links.weight();
        }
    }

    // reduce weight for sawn-off weapons capped to the apportioned weight of the barrel
    if( gunmod_find( itype_barrel_small ) ) {
        const units::volume b = type->gun->barrel_volume;
        const units::mass max_barrel_weight = units::from_gram( to_milliliter( b ) );
        const units::mass barrel_weight = units::from_gram( b.value() * type->weight.value() /
                                          type->volume.value() );
        ret -= std::min( max_barrel_weight, barrel_weight );
    }

    if( is_gun() ) {
        for( const item *elem : gunmods() ) {
            ret += elem->weight( true, true );
        }
        if( !magazine_integral() && magazine_current() ) {
            ret += std::max( magazine_current()->weight(), 0_gram );
        }
    } else if( include_contents ) {
        ret += contents.item_weight_modifier();
    }

    return ret;
}

units::volume item::corpse_volume( const mtype *corpse ) const
{
    units::volume corpse_volume = corpse->volume;
    if( has_flag( flag_QUARTERED ) ) {
        corpse_volume /= 4;
    }
    if( has_flag( flag_FIELD_DRESS ) || has_flag( flag_FIELD_DRESS_FAILED ) ) {
        corpse_volume *= 0.75;
    }
    if( has_flag( flag_GIBBED ) ) {
        corpse_volume *= 0.85;
    }
    if( has_flag( flag_SKINNED ) ) {
        corpse_volume *= 0.85;
    }
    if( corpse_volume > 0_ml ) {
        return corpse_volume;
    }
    debugmsg( "invalid monster volume for corpse" );
    return 0_ml;
}

units::volume item::base_volume() const
{
    if( is_null() ) {
        return 0_ml;
    }
    if( is_corpse() ) {
        return corpse_volume( corpse );
    }

    if( is_craft() ) {
        units::volume ret = 0_ml;
        for( const item * const &it : components ) {
            ret += it->base_volume();
        }
        return ret;
    }

    if( count_by_charges() ) {
        if( type->volume % type->stack_size == 0_ml ) {
            return type->volume / type->stack_size;
        } else {
            return type->volume / type->stack_size + 1_ml;
        }
    }

    return type->volume;
}

units::volume item::volume( bool integral ) const
{
    if( is_null() ) {
        return 0_ml;
    }

    if( is_corpse() ) {
        return corpse_volume( corpse );
    }

    if( is_craft() ) {
        units::volume ret = 0_ml;
        for( const item * const &it : components ) {
            ret += it->volume();
        }
        return ret;
    }

    const int local_volume = get_var( "volume", -1 );
    units::volume ret;
    if( local_volume >= 0 ) {
        ret = local_volume * units::legacy_volume_factor;
    } else if( integral ) {
        ret = type->integral_volume;
    } else {
        ret = type->volume;
    }

    if( count_by_charges() || made_of( LIQUID ) ) {
        units::quantity<int64_t, units::volume_in_milliliter_tag> num = ret * static_cast<int64_t>
                ( charges );
        if( type->stack_size <= 0 ) {
            debugmsg( "Item type %s has invalid stack_size %d", typeId().str(), type->stack_size );
            ret = num;
        } else {
            ret = num / type->stack_size;
            if( num % type->stack_size != 0_ml ) {
                ret += 1_ml;
            }
        }
    }

    // Non-rigid items add the volume of the content
    if( !type->rigid ) {
        // Disintegrating belts should exactly match contents volume, don't enforce the 1_ml minimum
        if( type->has_flag( flag_MAG_BELT ) && type->has_flag( flag_MAG_DESTROY ) ) {
            ret = 0_ml;
        }
        ret += contents.item_size_modifier();
    }

    // Some magazines sit (partly) flush with the item so add less extra volume
    if( magazine_current() != nullptr ) {
        ret += std::max( magazine_current()->volume() - type->magazine_well, 0_ml );
    }

    if( is_gun() ) {
        for( const item *elem : gunmods() ) {
            ret += elem->volume( true );
        }

        // TODO: implement stock_length property for guns
        if( has_flag( flag_COLLAPSIBLE_STOCK ) ) {
            // consider only the base size of the gun (without mods)
            ret -= ( type->volume / 3 );
        }

        if( gunmod_find( itype_barrel_small ) ) {
            ret -= type->gun->barrel_volume;
        }
    }

    return ret;
}

int item::lift_strength() const
{
    const int mass = units::to_gram( weight() );
    return std::max( mass / 10000, 1 );
}

int item::attack_cost() const
{
    int base = 65 + ( volume() / 62.5_ml + weight() / 60_gram ) / count();
    int bonus = bonus_from_enchantments_wielded( base, enchant_vals::mod::ITEM_ATTACK_COST, true );
    return std::max( 0, base + bonus );
}

int item::stamina_cost() const
{
    return get_avatar().get_melee_stamina_cost( *this );
}

int item::damage_melee( damage_type dt ) const
{
    return damage_melee( melee::default_attack( *this ), dt );
}

int item::damage_melee( const attack_statblock &attack, damage_type dt ) const
{
    assert( dt >= DT_NULL && dt < NUM_DT );
    if( is_null() ) {
        return 0;
    }

    // effectiveness is reduced by 10% per damage level
    int res = attack.damage.type_damage( dt );
    res -= res * std::max( damage_level( 4 ), 0 ) * 0.1;

    // apply type specific flags
    switch( dt ) {
        case DT_BASH:
            if( has_flag( flag_REDUCED_BASHING ) ) {
                res *= 0.5;
            }
            break;

        case DT_CUT:
        case DT_STAB:
            if( has_flag( flag_DIAMOND ) ) {
                res *= 1.3;
            }
            break;

        default:
            break;
    }

    // @todo: This probably breaks attack_statblock logic completely...
    // consider any melee gunmods
    if( is_gun() ) {
        const std::vector<const item *> &mods = gunmods();
        return std::accumulate( mods.begin(), mods.end(), res, [dt]( int last_max, const item * it ) {
            return it->has_flag( flag_MELEE_GUNMOD ) ? std::max( last_max, it->damage_melee( dt ) ) : last_max;
        } );

    }

    switch( dt ) {
        case DT_BASH:
            res += bonus_from_enchantments_wielded( res, enchant_vals::mod::ITEM_DAMAGE_BASH, true );
            break;
        case DT_CUT:
            res += bonus_from_enchantments_wielded( res, enchant_vals::mod::ITEM_DAMAGE_CUT, true );
            break;
        case DT_STAB:
            res += bonus_from_enchantments_wielded( res, enchant_vals::mod::ITEM_DAMAGE_STAB, true );
            break;
        case DT_BULLET:
            res += bonus_from_enchantments_wielded( res, enchant_vals::mod::ITEM_DAMAGE_BULLET, true );
            break;
        case DT_ACID:
            res += bonus_from_enchantments_wielded( res, enchant_vals::mod::ITEM_DAMAGE_ACID, true );
            break;
        case DT_BIOLOGICAL:
            res += bonus_from_enchantments_wielded( res, enchant_vals::mod::ITEM_DAMAGE_BIO, true );
            break;
        case DT_COLD:
            res += bonus_from_enchantments_wielded( res, enchant_vals::mod::ITEM_DAMAGE_COLD, true );
            break;
        case DT_DARK:
            res += bonus_from_enchantments_wielded( res, enchant_vals::mod::ITEM_DAMAGE_DARK, true );
            break;
        case DT_ELECTRIC:
            res += bonus_from_enchantments_wielded( res, enchant_vals::mod::ITEM_DAMAGE_ELECTRIC, true );
            break;
        case DT_HEAT:
            res += bonus_from_enchantments_wielded( res, enchant_vals::mod::ITEM_DAMAGE_FIRE, true );
            break;
        case DT_LIGHT:
            res += bonus_from_enchantments_wielded( res, enchant_vals::mod::ITEM_DAMAGE_LIGHT, true );
            break;
        case DT_PSI:
            res += bonus_from_enchantments_wielded( res, enchant_vals::mod::ITEM_DAMAGE_PSI, true );
            break;
        case DT_TRUE:
            res += bonus_from_enchantments_wielded( res, enchant_vals::mod::ITEM_DAMAGE_TRUE, true );
            break;
        default:
            break;
    }
    // Apply melee damage bonus
    const auto &bonus = get_melee_damage_bonus();
    res += bonus.type_damage( dt );

    return std::max( res, 0 );
}

std::map<std::string, attack_statblock> item::get_attacks() const
{
    if( is_null() ) {
        return {{"DEFAULT", attack_statblock{}}};
    }

    std::map<std::string, attack_statblock> result;

    // TODO: Cache
    for( const auto &attack : type->attacks ) {
        attack_statblock modified_attack = attack.second;
        const auto &bonus = get_melee_damage_bonus();
        for( damage_unit &du : modified_attack.damage.damage_units ) {
            // effectiveness is reduced by 10% per damage level
            du.amount -= du.amount * std::max( damage_level( 4 ), 0 ) * 0.1;
            // apply type specific flags
            switch( du.type ) {
                case DT_BASH:
                    if( has_flag( flag_REDUCED_BASHING ) ) {
                        du.amount *= 0.5;
                    }
                    break;

                case DT_CUT:
                case DT_STAB:
                    if( has_flag( flag_DIAMOND ) ) {
                        du.amount *= 1.3;
                    }
                    break;

                default:
                    break;
            }

            switch( du.type ) {
                case DT_BASH:
                    du.amount += bonus_from_enchantments_wielded( du.amount, enchant_vals::mod::ITEM_DAMAGE_BASH,
                                 true );
                    break;
                case DT_CUT:
                    du.amount += bonus_from_enchantments_wielded( du.amount, enchant_vals::mod::ITEM_DAMAGE_CUT,
                                 true );
                    break;
                case DT_STAB:
                    du.amount += bonus_from_enchantments_wielded( du.amount, enchant_vals::mod::ITEM_DAMAGE_STAB,
                                 true );
                    break;
                case DT_BULLET:
                    du.amount += bonus_from_enchantments_wielded( du.amount, enchant_vals::mod::ITEM_DAMAGE_BULLET,
                                 true );
                    break;
                case DT_ACID:
                    du.amount += bonus_from_enchantments_wielded( du.amount, enchant_vals::mod::ITEM_DAMAGE_ACID,
                                 true );
                    break;
                case DT_BIOLOGICAL:
                    du.amount += bonus_from_enchantments_wielded( du.amount, enchant_vals::mod::ITEM_DAMAGE_BIO,
                                 true );
                    break;
                case DT_COLD:
                    du.amount += bonus_from_enchantments_wielded( du.amount, enchant_vals::mod::ITEM_DAMAGE_COLD,
                                 true );
                    break;
                case DT_DARK:
                    du.amount += bonus_from_enchantments_wielded( du.amount, enchant_vals::mod::ITEM_DAMAGE_DARK,
                                 true );
                    break;
                case DT_ELECTRIC:
                    du.amount += bonus_from_enchantments_wielded( du.amount, enchant_vals::mod::ITEM_DAMAGE_ELECTRIC,
                                 true );
                    break;
                case DT_HEAT:
                    du.amount += bonus_from_enchantments_wielded( du.amount, enchant_vals::mod::ITEM_DAMAGE_FIRE,
                                 true );
                    break;
                case DT_LIGHT:
                    du.amount += bonus_from_enchantments_wielded( du.amount, enchant_vals::mod::ITEM_DAMAGE_LIGHT,
                                 true );
                    break;
                case DT_PSI:
                    du.amount += bonus_from_enchantments_wielded( du.amount, enchant_vals::mod::ITEM_DAMAGE_PSI,
                                 true );
                    break;
                case DT_TRUE:
                    du.amount += bonus_from_enchantments_wielded( du.amount, enchant_vals::mod::ITEM_DAMAGE_TRUE,
                                 true );
                    break;
                default:
                    break;
            }
            // Apply melee damage bonus
            du.amount += bonus.type_damage( du.type );
        }
        result[attack.first] = modified_attack;

    }

    // consider any melee gunmods
    if( is_gun() ) {
        if( get_option<bool>( "LIMITED_BAYONETS" ) ) {
            // TODO: Multiple bayonets with multiple attacks each - add all attacks, resolve id conflicts
            const std::vector<const item *> &mods = gunmods();
            float best_damage = 0.0f;
            const attack_statblock *best = nullptr;
            for( const item *gunmod_ptr : mods ) {
                const item &gunmod = *gunmod_ptr;
                if( gunmod.has_flag( flag_MELEE_GUNMOD ) ) {
                    // TODO: Handle multiple attacks here - add all of them as separate attacks
                    assert( !gunmod.type->attacks.empty() );
                    const attack_statblock &first_attack = gunmod.type->attacks.begin()->second;
                    float damage_sum = std::accumulate( first_attack.damage.begin(), first_attack.damage.end(),
                                                        0.0f, []( float amount_sum,
                    const damage_unit & du ) {
                        // Ignore multipliers for now because it's a temporary hack
                        return amount_sum + du.amount;
                    } );
                    if( damage_sum > best_damage ) {
                        best = &first_attack;
                        best_damage = damage_sum;
                    }
                }
            }
            if( best != nullptr ) {
                attack_statblock gunmod_attack = *best;
                gunmod_attack.to_hit = type->m_to_hit;
                result["BAYONET"] = gunmod_attack;
            }
        } else {
            // Old logic here - max dmg for each type
            const std::vector<const item *> &mods = gunmods();
            for( const item *it : mods ) {
                const attack_statblock &attack = melee::default_attack( *it );
                for( auto &dmg : attack.damage ) {
                    result["DEFAULT"].damage.add( dmg );
                }
            }
        }
    }

    return result;
}

auto item::get_melee_damage_bonus( ) const -> const damage_instance &
{
    return melee_damage_bonus;
}

auto item::set_melee_damage_bonus( const damage_instance &bonus ) -> void
{
    melee_damage_bonus = bonus;
}

auto item::get_melee_hit_bonus() const -> int
{
    return melee_hit_bonus;
}

auto item::set_melee_hit_bonus( int bonus ) -> void
{
    melee_hit_bonus = bonus;
}

auto item::get_ranged_damage_bonus( ) const -> const damage_instance &
{
    return ranged_damage_bonus;
}

auto item::set_ranged_damage_bonus( const damage_instance &damages ) -> void
{
    ranged_damage_bonus = damages;
}

auto item::get_range_bonus() const -> int
{
    return range_bonus;
}

auto item::set_range_bonus( int bonus ) -> void
{
    range_bonus = bonus;
}

auto item::get_dispersion_bonus() const -> int
{
    return dispersion_bonus;
}

auto item::set_dispersion_bonus( int bonus ) -> void
{
    dispersion_bonus = bonus;
}

auto item::get_recoil_bonus() const -> int
{
    return recoil_bonus;
}

auto item::set_recoil_bonus( int bonus ) -> void
{
    recoil_bonus = bonus;
}

damage_instance item::base_damage_melee() const
{
    // TODO: Caching
    damage_instance ret;
    for( size_t i = DT_NULL + 1; i < NUM_DT; i++ ) {
        damage_type dt = static_cast<damage_type>( i );
        int dam = damage_melee( dt );
        if( dam > 0 ) {
            ret.add_damage( dt, dam );
        }
    }

    return ret;
}

damage_instance item::base_damage_thrown() const
{
    // TODO: Create a separate cache for individual items (for modifiers like diamond etc.)
    return type->thrown_damage;
}

int item::reach_range( const Character &guy ) const
{
    int res = 1;

    if( has_flag( flag_REACH_ATTACK ) ) {
        res = has_flag( flag_REACH3 ) ? 3 : 2;
    }

    // for guns consider any attached gunmods
    if( is_gun() && !is_gunmod() ) {
        for( const std::pair<const gun_mode_id, gun_mode> &m : gun_all_modes() ) {
            if( guy.is_npc() && m.second.flags.contains( "NPC_AVOID" ) ) {
                continue;
            }
            if( m.second.melee() ) {
                res = std::max( res, m.second.qty );
            }
        }
    }

    return std::max( 1, res );
}

bool item::can_shatter() const
{
    static const std::set<material_id> is_glass{ material_id( "glass" ) };
    return only_made_of( is_glass ) || has_flag( flag_SHATTERS );
}

void item::unset_flags()
{
    item_tags.clear();
}

bool item::has_fault( const fault_id &fault ) const
{
    return faults.contains( fault );
}

bool item::has_own_flag( const flag_id &f ) const
{
    return item_tags.count( f );
}

bool item::has_flag( const flag_id &f ) const
{
    // Check if we have any gun/toolmods with the flag, and if we do
    // check if that flag should be inherited.
    // `json_flag::get` is pretty expensive so it's faster to do it
    // last as frequently there are no gun/toolmods with the flag f
    auto mods = is_gun() ? gunmods() : toolmods();

    const auto flag_in_mods = [&f]( const auto & mods ) -> bool {
        return std::any_of( mods.begin(), mods.end(), [&f]( const item * e )-> bool {
            return ( !e->is_gun() && e->has_flag( f ) );
        } );
    };

    if( f->inherit() && flag_in_mods( mods ) ) {
        return true;
    }

    // other item type flags
    if( type->has_flag( f ) ) {
        return true;
    }

    // now check for item specific flags
    return has_own_flag( f );
}

bool item::has_vitamin( const vitamin_id &v ) const
{
    if( !this->is_comestible() ) {
        return false;
    }
    // We need this function to get all vitamins including from inheritance.
    // But we don't care about calories, so we can just pass a dummy.
    npc dummy;
    const nutrients food_item = dummy.compute_effective_nutrients( *this );
    for( auto const& [vit_id, amount] : food_item.vitamins ) {
        if( vit_id == v ) {
            if( amount > 0 ) {
                return true;
            } else {
                break;
            }
        }
    }
    return false;
}

void item::set_flag( const flag_id &flag )
{
    if( flag.is_valid() ) {
        item_tags.insert( flag );
    } else {
        debugmsg( "Attempted to set invalid flag_id %s", flag.str() );
    }
}

void item::unset_flag( const flag_id &flag )
{
    item_tags.erase( flag );
}

void item::set_flag_recursive( const flag_id &flag )
{
    set_flag( flag );
    for( item * const &comp : components ) {
        comp->set_flag_recursive( flag );
    }
}

const item::FlagsSetType &item::get_flags() const
{
    return item_tags;
}

bool item::has_property( const std::string &prop ) const
{
    return type->properties.contains( prop );
}

std::string item::get_property_string( const std::string &prop, const std::string &def ) const
{
    const auto it = type->properties.find( prop );
    return it != type->properties.end() ? it->second : def;
}

int64_t item::get_property_int64_t( const std::string &prop, int64_t def ) const
{
    const auto it = type->properties.find( prop );
    if( it != type->properties.end() ) {
        char *e = nullptr;
        int64_t r = std::strtoll( it->second.c_str(), &e, 10 );
        if( !it->second.empty() && *e == '\0' ) {
            return r;
        }
        debugmsg( "invalid property '%s' for item '%s'", prop.c_str(), tname() );
    }
    return def;
}

int item::get_quality( const quality_id &id ) const
{
    int return_quality = INT_MIN;

    /**
     * EXCEPTION: Items with quality BOIL only count as such if they are empty,
     * excluding items of their ammo type if they are tools.
     */
    auto block_boil_filter = [this]( const item & itm ) {
        // We want to skip (do not block) only those : correct ammo, correct magazine, correct toolmod.Everything else should block.
        if( &itm == this ) {
            // Do not block if checking itself - we are checking only item contents not item itself.
            return false;
        } else if( itm.is_ammo() ) {
            return !ammo_types().contains( itm.ammo_type() );
        } else if( itm.is_magazine() ) {
            // we want to return "fine for boiling" if any of the ammo types match and "blocks boiling" if none match.
            for( const ammotype &at : ammo_types() ) {
                for( const ammotype &mag_at : itm.ammo_types() ) {
                    if( at == mag_at ) {
                        return false;
                    }
                }
            }
            return true;
        } else if( itm.is_toolmod() ) {
            return false;
        }
        return true;
    };
    // if it's has boil quality and it's empty, it's good to boil. If it's not empty and it's not a tool (it's probably a container), it's not good to boil. If it's a tool, it gets an extra chance: if it's only contents are mods or batteries, it's still good.
    // Also  we are using inverted filter, since we don't care about items that the filter likes, we only care if it find something it doesn't like.
    if( id == quality_id( "BOIL" ) && !contents.empty() &&
        ( !is_tool() || has_item_with( block_boil_filter ) ) ) {
        return INT_MIN;
    }

    for( const std::pair<const quality_id, int> &quality : type->qualities ) {
        if( quality.first == id ) {
            return_quality = quality.second;
        }
    }
    return_quality = std::max( return_quality, contents.best_quality( id ) );

    return return_quality;
}

std::map<quality_id, int> item::get_qualities() const
{
    std::map<quality_id, int> qualities;
    for( const auto &quality : type->qualities ) {
        qualities[quality.first] = get_quality( quality.first );
    }
    return qualities;
}

bool item::has_technique( const matec_id &tech ) const
{
    return type->techniques.contains( tech ) || techniques.contains( tech );
}

void item::add_technique( const matec_id &tech )
{
    techniques.insert( tech );
}

void item::remove_technique( const matec_id &tech )
{
    techniques.erase( tech );
}

std::vector<item *> item::toolmods()
{
    std::vector<item *> res;
    if( is_tool() ) {
        for( item *e : contents.all_items_top() ) {
            if( e->is_toolmod() ) {
                res.push_back( e );
            }
        }
    }
    return res;
}

std::vector<const item *> item::toolmods() const
{
    std::vector<const item *> res;
    if( is_tool() ) {
        for( const item *e : contents.all_items_top() ) {
            if( e->is_toolmod() ) {
                res.push_back( e );
            }
        }
    }
    return res;
}

std::set<matec_id> item::get_techniques() const
{
    std::set<matec_id> result = type->techniques;
    result.insert( techniques.begin(), techniques.end() );
    return result;
}

int item::get_comestible_fun() const
{
    if( !is_comestible() ) {
        return 0;
    }
    int fun = get_comestible()->fun;
    for( const flag_id &flag : item_tags ) {
        fun += flag->taste_mod();
    }
    for( const flag_id &flag : type->get_flags() ) {
        fun += flag->taste_mod();
    }

    return fun;
}

bool item::goes_bad() const
{
    if( item_internal::goes_bad_cache_is_for( this ) ) {
        return item_internal::goes_bad_cache_fetch();
    }
    if( has_flag( flag_PROCESSING ) ) {
        return false;
    }
    if( is_corpse() ) {
        // Corpses rot only if they are made of rotting materials
        return made_of_any( materials::get_rotting() );
    }
    return is_food() && get_comestible()->spoils != 0_turns;
}

bool item::goes_bad_after_opening( bool strict ) const
{
    // check if this item is explicitly a canning-type item: eg, it preserves contents
    if( strict ) {
        if( type->container && type->container->preserves &&
            !contents.empty() && contents.front().goes_bad() ) {
            return true;
        } else {
            return false;
        }
    }

    return goes_bad() || ( type->container && type->container->preserves &&
                           !contents.empty() && contents.front().goes_bad() );
}

time_duration item::get_shelf_life() const
{
    if( goes_bad() ) {
        if( is_food() ) {
            return get_comestible()->spoils;
        } else if( is_corpse() ) {
            return 24_hours;
        }
    }
    return 0_turns;
}

double item::get_relative_rot() const
{
    if( goes_bad() ) {
        return rot / get_shelf_life();
    }
    return 0;
}

void item::set_relative_rot( double val )
{
    if( goes_bad() ) {
        rot = get_shelf_life() * val;
        // calc_rot uses last_rot_check (when it's not turn_zero) instead of bday.
        // this makes sure the rotting starts from now, not from bday.
        // if this item is the result of smoking or milling don't do this, we want to start from bday.
        if( !has_flag( flag_PROCESSING_RESULT ) ) {
            last_rot_check = calendar::turn;
        }
    }
}

void item::set_rot( time_duration val )
{
    rot = val;
}

int item::spoilage_sort_order() const
{
    const item *subject;
    constexpr int bottom = std::numeric_limits<int>::max();

    if( type->container && !contents.empty() ) {
        if( type->container->preserves ) {
            return bottom - 3;
        }
        subject = &contents.front();
    } else {
        subject = this;
    }

    if( subject->goes_bad() ) {
        return to_turns<int>( subject->get_shelf_life() - subject->rot );
    }

    if( subject->get_comestible() ) {
        if( subject->get_category().get_id() == itemcat_food ) {
            return bottom - 3;
        } else if( subject->get_category().get_id() == itemcat_drugs ) {
            return bottom - 2;
        } else {
            return bottom - 1;
        }
    }
    return bottom;
}

namespace
{

/**
 * Hardcoded lookup table for food rots per hour calculation.
 *
 * IRL this tends to double every 10c a few degrees above freezing, but past a certain
 * point the rate decreases until even extremophiles find it too hot. Here we just stop
 * further acceleration at 40C.
 *
 * Original formula:
 * @see https://github.com/cataclysmbn/Cataclysm-BN/blob/033901af4b52ad0bfcfd6abfe06bca4e403d44b1/src/item.cpp#L5612-L5640
 */
constexpr auto rot_chart = std::array<int, 44>
{
    0, 372, 744, 1118, 1219, 1273, 1388, 1514, 1651, 1800,
    1880, 2050, 2235, 2438, 2658, 2776, 3027, 3301, 3600, 3926,
    4100, 4471, 4875, 5317, 5798, 6054, 6602, 7200, 7852, 8562,
    8941, 9751, 10633, 11595, 12645, 13205, 14400, 15703, 17125, 18674,
    19501,
};

} // namespace

/**
 * Get the hourly rot for a given temperature from the precomputed table.
 * @see rot_chart
 */
auto get_hourly_rotpoints_at_temp( const units::temperature temp ) -> int
{
    /**
     * Precomputed rot lookup table.
     */
    if( temp < temperatures::freezing ) {
        return 0;
    }
    if( temp > 40_c ) {
        return 21240;
    }
    // HACK: due to frequent fahrenheit <-> celsius conversion, 18C is actually 17.777C
    // remove rounding after most of temperatures passed around are in `units::temperature`
    const float temp_c = static_cast<float>( units::to_millidegree_celsius( temp ) ) / 1000;
    return rot_chart[std::round( temp_c )];
}

auto item::calc_rot( time_point time, const units::temperature temp ) const -> time_duration
{
    // Avoid needlessly calculating already rotten things.  Corpses should
    // always rot away and food rots away at twice the shelf life.  If the food
    // is in a sealed container they won't rot away, this avoids needlessly
    // calculating their rot in that case.
    if( !is_corpse() && get_relative_rot() > 2.0 ) {
        return 0_seconds;
    }

    // rot modifier
    float factor = 1.0;
    if( is_corpse() && has_flag( flag_FIELD_DRESS ) ) {
        factor = 0.75;
    }

    time_duration added_rot = 0_seconds;
    // simulation of different age of food at the start of the game and good/bad storage
    // conditions by applying starting variation bonus/penalty of +/- 20% of base shelf-life
    // positive = food was produced some time before calendar::start and/or bad storage
    // negative = food was stored in good conditions before calendar::start
    if( last_rot_check <= calendar::start_of_cataclysm ) {
        time_duration spoil_variation = get_shelf_life() * 0.2f;
        added_rot += rng( -spoil_variation, spoil_variation );
    }
    time_duration time_delta = time - last_rot_check;
    added_rot += factor * time_delta / 1_hours * get_hourly_rotpoints_at_temp( temp ) * 1_turns;
    return added_rot;
}

namespace
{

auto temperature_flag_to_highest_temperature( temperature_flag temperature ) -> units::temperature
{
    switch( temperature ) {
        case temperature_flag::TEMP_NORMAL:
        case temperature_flag::TEMP_HEATER:
            return units::temperature_max;
        case temperature_flag::TEMP_FRIDGE:
            return temperatures::fridge;
        case temperature_flag::TEMP_FREEZER:
            return temperatures::freezer;
        case temperature_flag::TEMP_ROOT_CELLAR:
            return temperatures::root_cellar;
    }

    return units::temperature_max;
}

} // namespace


time_duration item::minimum_freshness_duration( temperature_flag temperature ) const
{
    const units::temperature temp = temperature_flag_to_highest_temperature( temperature );
    unsigned long long rot_per_hour = get_hourly_rotpoints_at_temp( temp );

    if( rot_per_hour <= 0 || !type->comestible ) {
        return calendar::INDEFINITELY_LONG_DURATION;
    }

    time_duration remaining_rot = type->comestible->spoils - rot;
    // Has to be in int64 or it will overflow for long lasting food
    unsigned long long duration = to_turns<unsigned long long>( remaining_rot )
                                  * to_turns<unsigned long long>( 1_hours )
                                  / rot_per_hour;
    if( duration > to_turns<unsigned long long>( calendar::INDEFINITELY_LONG_DURATION ) ) {
        return calendar::INDEFINITELY_LONG_DURATION;
    }

    return time_duration::from_turns( static_cast<int>( duration ) );
}

void item::mod_last_rot_check( time_duration processing_duration )
{
    if( !has_own_flag( flag_PROCESSING ) ) {
        debugmsg( "mod_last_rot_check called on non smoking item: %s", tname() );
        return;
    }

    // Apply no rot while smoking
    last_rot_check += processing_duration;
}

units::volume item::get_storage() const
{
    const islot_armor *armor = find_armor_data();
    if( armor == nullptr ) {
        return is_pet_armor() ? type->pet_armor->storage : 0_ml;
    }
    units::volume storage = armor->storage;
    float mod = get_clothing_mod_val( clothing_mod_type_storage );
    storage += std::lround( mod ) * units::legacy_volume_factor;

    return storage;
}

float item::get_weight_capacity_modifier() const
{
    const islot_armor *armor = find_armor_data();
    if( armor == nullptr ) {
        return 1;
    }
    return armor->weight_capacity_modifier;
}

units::mass item::get_weight_capacity_bonus() const
{
    const islot_armor *armor = find_armor_data();
    if( armor == nullptr ) {
        return 0_gram;
    }
    return armor->weight_capacity_bonus;
}

int item::get_env_resist( int override_base_resist ) const
{
    const islot_armor *armor = find_armor_data();
    if( armor == nullptr ) {
        return is_pet_armor() ? type->pet_armor->env_resist : 0;
    }
    // modify if item is a gas mask and has filter
    int resist_base = armor->env_resist;
    int resist_filter = get_var( "overwrite_env_resist", 0 );
    int resist = std::max( { resist_base, resist_filter, override_base_resist } );

    return std::lround( resist * get_relative_health() );
}

int item::get_base_env_resist_w_filter() const
{
    const islot_armor *armor = find_armor_data();
    if( armor == nullptr ) {
        return is_pet_armor() ? type->pet_armor->env_resist_w_filter : 0;
    }
    return armor->env_resist_w_filter;
}

bool item::is_power_armor() const
{
    return ( has_flag( flag_POWERARMOR_EXO ) || has_flag( flag_POWERARMOR_EXTERNAL ) ||
             has_flag( flag_POWERARMOR_MOD ) );
}

int item::get_avg_encumber( const Character &who ) const
{
    const islot_armor *armor = find_armor_data();
    if( !armor ) {
        // handle wearable guns (e.g. shoulder strap) as special case
        return is_gun() ? volume() / 750_ml : 0;
    }

    int avg_encumber = 0;
    int avg_ctr = 0;

    for( const armor_portion_data &entry : armor->data ) {
        for( const bodypart_str_id &limb : entry.covers ) {
            int encumber = get_encumber( who, limb.id() );
            if( encumber ) {
                avg_encumber += encumber;
                ++avg_ctr;
            }
        }
    }
    if( avg_encumber == 0 ) {
        return 0;
    } else {
        return avg_encumber / avg_ctr;
    }
}

int item::get_encumber( const Character &who, const bodypart_id &bodypart ) const
{

    units::volume contents_volume( 0_ml );

    contents_volume += contents.item_size_modifier();

    if( who.is_worn( *this ) ) {
        const islot_armor *armor = find_armor_data();

        if( armor != nullptr ) {
            for( const armor_portion_data &entry : armor->data ) {
                if( entry.covers.test( bodypart.id() ) ) {
                    if( entry.max_encumber != 0 ) {
                        units::volume char_storage( 0_ml );

                        for( const item * const &e : who.worn ) {
                            char_storage += e->get_storage();
                        }

                        if( char_storage != 0_ml ) {
                            // Cast up to 64 to prevent overflow. Dividing before would prevent this but lose data.
                            contents_volume += units::from_milliliter( static_cast<int64_t>( armor->storage.value() ) *
                                               who.inv_volume().value() / char_storage.value() );
                        }
                    }
                }
            }
        }
    }

    return get_encumber_when_containing( who, contents_volume, bodypart );
}

int item::get_encumber_when_containing(
    const Character &who, const units::volume &contents_volume, const bodypart_id &bodypart ) const
{
    const islot_armor *armor = find_armor_data();
    if( armor == nullptr ) {
        // handle wearable guns (e.g. shoulder strap) as special case
        return is_gun() ? volume() / 750_ml : 0;
    }

    int encumber = 0;

    for( const armor_portion_data &entry : armor->data ) {
        if( entry.covers.test( bodypart.id() ) ) {
            encumber = entry.encumber;
            // Non-rigid items add additional encumbrance proportional to their volume
            bool any_encumb_increase = std::ranges::any_of( armor->data,
            []( armor_portion_data data ) {
                return data.encumber != data.max_encumber;
            } );
            if( !type->rigid || any_encumb_increase ) {
                const int capacity = get_total_capacity().value();
                if( entry.max_encumber == 0 ) {
                    encumber += contents_volume / 500_ml;
                } else {
                    if( capacity <= 0 ) {
                        debugmsg( "Non-rigid item (%s) without storage capacity.", tname() );
                    } else {
                        // Cast up to 64 to prevent overflow. Dividing before would prevent this but lose data.
                        encumber += static_cast<int64_t>( entry.max_encumber - entry.encumber ) * contents_volume.value() /
                                    capacity;
                    }
                }
            }
        }
    }

    // Fit checked before changes, fitting shouldn't reduce penalties from patching.
    if( has_flag( flag_FIT ) && has_flag( flag_VARSIZE ) ) {
        encumber = std::max( encumber / 2, encumber - 10 );
    }

    // TODO: Should probably have sizing affect coverage
    const sizing sizing_level = get_sizing( who );
    switch( sizing_level ) {
        case sizing::small_sized_human_char:
        case sizing::small_sized_big_char:
            // non small characters have a HARD time wearing undersized clothing
            encumber *= 3;
            break;
        case sizing::human_sized_small_char:
        case sizing::big_sized_small_char:
            // clothes bag up around smol characters and encumber them more
            encumber *= 2;
            break;
        default:
            break;
    }

    encumber += static_cast<int>( std::ceil( get_clothing_mod_val( clothing_mod_type_encumbrance ) ) );

    return encumber;
}

layer_level item::get_layer() const
{
    if( type->armor ) {
        // We assume that an item will never have per-item flags defining its
        // layer, so we can defer to the itype.
        return type->layer;
    }

    if( has_flag( flag_PERSONAL ) ) {
        return PERSONAL_LAYER;
    } else if( has_flag( flag_SKINTIGHT ) ) {
        return UNDERWEAR_LAYER;
    } else if( has_flag( flag_WAIST ) ) {
        return WAIST_LAYER;
    } else if( has_flag( flag_OUTER ) ) {
        return OUTER_LAYER;
    } else if( has_flag( flag_BELTED ) ) {
        return BELTED_LAYER;
    } else if( has_flag( flag_AURA ) ) {
        return AURA_LAYER;
    } else {
        return REGULAR_LAYER;
    }
}

int item::get_avg_coverage() const
{
    const islot_armor *armor = find_armor_data();
    if( !armor ) {
        // handle wearable guns (e.g. shoulder strap) as special case
        return is_gun() ? std::min( volume() / 500_ml, 100 ) : 0;
    }
    int avg_coverage = 0;
    int avg_ctr = 0;
    for( const armor_portion_data &entry : armor->data ) {
        for( const bodypart_str_id &limb : entry.covers ) {
            int coverage = get_coverage( limb );
            if( coverage ) {
                avg_coverage += coverage;
                ++avg_ctr;
            }
        }
    }
    if( avg_coverage == 0 ) {
        return 0;
    } else {
        avg_coverage /= avg_ctr;
        return avg_coverage;
    }
}

int item::get_coverage( const bodypart_id &bodypart ) const
{
    if( std::optional<armor_portion_data> portion_data = portion_for_bodypart( bodypart ) ) {
        return portion_data->coverage;
    }
    return 0;
}

std::optional<armor_portion_data> item::portion_for_bodypart( const bodypart_id &bodypart ) const
{
    const islot_armor *armor = find_armor_data();
    if( !armor ) {
        return std::optional<armor_portion_data>();
    }
    for( const armor_portion_data &entry : armor->data ) {
        if( entry.covers.test( bodypart.id() ) ) {
            return entry;
        }
    }
    return std::optional<armor_portion_data>();
}

int item::get_thickness() const
{
    const islot_armor *armor = find_armor_data();
    if( armor == nullptr ) {
        return is_pet_armor() ? type->pet_armor->thickness : 0;
    }
    return armor->thickness;
}

int item::get_warmth() const
{
    const islot_armor *armor = find_armor_data();
    if( armor == nullptr ) {
        return 0;
    }
    int result = armor->warmth;

    result += get_clothing_mod_val( clothing_mod_type_warmth );

    return result;
}

units::volume item::get_pet_armor_max_vol() const
{
    return is_pet_armor() ? type->pet_armor->max_vol : 0_ml;
}

units::volume item::get_pet_armor_min_vol() const
{
    return is_pet_armor() ? type->pet_armor->min_vol : 0_ml;
}

std::string item::get_pet_armor_bodytype() const
{
    return is_pet_armor() ? type->pet_armor->bodytype : "";
}

time_duration item::brewing_time() const
{
    return is_brewable() ? type->brewable->time : 0_turns;
}

const std::vector<itype_id> &item::brewing_results() const
{
    static const std::vector<itype_id> nulresult{};
    return is_brewable() ? type->brewable->results : nulresult;
}

bool item::can_revive() const
{
    return is_corpse() && corpse->has_flag( MF_REVIVES ) && damage() < max_damage() &&
           !( has_flag( flag_FIELD_DRESS ) || has_flag( flag_FIELD_DRESS_FAILED ) ||
              has_flag( flag_QUARTERED ) ||
              has_flag( flag_SKINNED ) || has_flag( flag_PULPED ) );
}

bool item::ready_to_revive( const tripoint &pos ) const
{
    if( !can_revive() ) {
        return false;
    }
    if( get_map().veh_at( pos ) ) {
        return false;
    }
    if( !calendar::once_every( 1_seconds ) ) {
        return false;
    }
    int age_in_hours = to_hours<int>( age() );
    age_in_hours -= static_cast<int>( static_cast<float>( burnt ) / ( volume() / 250_ml ) );
    if( damage_level( 4 ) > 0 ) {
        age_in_hours /= ( damage_level( 4 ) + 1 );
    }
    int rez_factor = 48 - age_in_hours;
    if( age_in_hours > 6 && ( rez_factor <= 0 || one_in( rez_factor ) ) ) {
        // If we're a special revival zombie, wait to get up until the player is nearby.
        const bool isReviveSpecial = has_flag( flag_REVIVE_SPECIAL );
        if( isReviveSpecial ) {
            const int distance = rl_dist( pos, get_player_character().pos() );
            if( distance > 3 ) {
                return false;
            }
            if( !one_in( distance + 1 ) ) {
                return false;
            }
        }

        return true;
    }
    return false;
}

bool item::is_money() const
{
    return ammo_types().contains( ammotype( "money" ) );
}

bool item::count_by_charges() const
{
    return type->count_by_charges();
}

int item::count() const
{
    return count_by_charges() ? charges : 1;
}

bool item::craft_has_charges()
{
    if( count_by_charges() ) {
        return true;
    } else if( ammo_types().empty() ) {
        return true;
    }

    return false;
}

#if defined(_MSC_VER)
// Deal with MSVC compiler bug (#17791, #17958)
#pragma optimize( "", off )
#endif

template<typename ResistGetter>
static int phys_resist( const item &it, damage_type dt, clothing_mod_type cmt,
                        ResistGetter resist_getter, bool to_self )
{
    if( it.is_null() ) {
        return 0;
    }

    float resist = 0;
    int eff_thickness = 1;

    // base resistance
    // Don't give reinforced items +armor, just more resistance to ripping
    const int dmg = it.damage_level( 4 );
    const int eff_damage = to_self ? std::min( dmg, 0 ) : std::max( dmg, 0 );
    eff_thickness = std::max( 1, it.get_thickness() - eff_damage );

    float mod = it.get_clothing_mod_val( cmt );

    std::optional<resistances> overriden_resistance = it.damage_resistance_override();
    if( overriden_resistance ) {
        float base_resistance = 0.0f;
        auto iter = overriden_resistance->flat.find( dt );
        if( iter != overriden_resistance->flat.end() ) {
            base_resistance = iter->second;
        }

        // We can have 0 thickness items, so need to check for it to ensure we don't get NaN in calcs
        const int thickness = it.get_thickness();
        const float damaged_resistance = ( thickness == 0 ) ? 0.0f : base_resistance * eff_thickness /
                                         thickness;

        return std::lround( damaged_resistance + mod );
    }

    const std::vector<const material_type *> mat_types = it.made_of_types();
    if( !mat_types.empty() ) {
        for( const material_type *mat : mat_types ) {
            resist += ( mat->*resist_getter )();
        }
        // Average based on number of materials.
        resist /= mat_types.size();
    }

    return std::lround( ( resist * eff_thickness ) + mod );
}

int item::bash_resist( bool to_self ) const
{
    return phys_resist( *this, DT_BASH, clothing_mod_type_bash, &material_type::bash_resist, to_self );
}

int item::cut_resist( bool to_self ) const
{
    return phys_resist( *this, DT_CUT, clothing_mod_type_cut, &material_type::cut_resist, to_self );
}

#if defined(_MSC_VER)
#pragma optimize( "", on )
#endif

int item::stab_resist( bool to_self ) const
{
    // Better than hardcoding it in multiple places
    return static_cast<int>( 0.8f * cut_resist( to_self ) );
}

int item::bullet_resist( bool to_self ) const
{
    return phys_resist( *this, DT_BULLET, clothing_mod_type_bullet, &material_type::bullet_resist,
                        to_self );
}

int item::acid_resist( bool to_self, int base_env_resist ) const
{
    if( to_self ) {
        // Currently no items are damaged by acid
        return INT_MAX;
    }

    if( is_null() ) {
        return 0.0;
    }

    float resist = 0.0;
    float mod = get_clothing_mod_val( clothing_mod_type_acid );

    std::optional<resistances> overriden_resistance = damage_resistance_override();
    if( overriden_resistance && overriden_resistance->flat.contains( DT_ACID ) ) {
        return std::lround( overriden_resistance->flat[DT_ACID] + mod );
    }

    const std::vector<const material_type *> mat_types = made_of_types();
    if( !mat_types.empty() ) {
        // Not sure why cut and bash get an armor thickness bonus but acid doesn't,
        // but such is the way of the code.

        for( const material_type *mat : mat_types ) {
            resist += mat->acid_resist();
        }
        // Average based on number of materials.
        resist /= mat_types.size();
    }

    const int env = get_env_resist( base_env_resist );
    if( env < 10 ) {
        // Low env protection means it doesn't prevent acid seeping in.
        resist *= env / 10.0f;
    }

    return std::lround( resist + mod );
}

int item::fire_resist( bool to_self, int base_env_resist ) const
{
    if( to_self ) {
        // Fire damages items in a different way
        return INT_MAX;
    }

    if( is_null() ) {
        return 0.0;
    }

    float mod = get_clothing_mod_val( clothing_mod_type_fire );

    std::optional<resistances> overriden_resistance = damage_resistance_override();
    if( overriden_resistance && overriden_resistance->flat.contains( DT_HEAT ) ) {
        return std::lround( overriden_resistance->flat[DT_HEAT] + mod );
    }

    float resist = 0.0;

    const std::vector<const material_type *> mat_types = made_of_types();
    if( !mat_types.empty() ) {
        for( const material_type *mat : mat_types ) {
            resist += mat->fire_resist();
        }
        // Average based on number of materials.
        resist /= mat_types.size();
    }

    const int env = get_env_resist( base_env_resist );
    if( env < 10 ) {
        // Iron resists immersion in magma, iron-clad knight won't.
        resist *= env / 10.0f;
    }

    return std::lround( resist + mod );
}

int item::chip_resistance( bool worst ) const
{
    int res = worst ? INT_MAX : INT_MIN;
    for( const material_type *mat : made_of_types() ) {
        const int val = mat->chip_resist();
        res = worst ? std::min( res, val ) : std::max( res, val );
    }

    if( res == INT_MAX || res == INT_MIN ) {
        return 2;
    }

    if( res <= 0 ) {
        return 0;
    }

    return res;
}

std::optional<resistances> item::damage_resistance_override() const
{
    if( is_null() || !type->armor ) {
        return std::optional<resistances>();
    }

    return type->armor->resistance;
}

int item::min_damage() const
{
    return type->damage_min();
}

int item::max_damage() const
{
    return type->damage_max();
}

float item::get_relative_health() const
{
    return ( max_damage() + 1.0f - damage() ) / ( max_damage() + 1.0f );
}

bool item::mod_damage( int qty, damage_type dt )
{
    bool destroy = false;

    if( count_by_charges() ) {
        charges -= std::min( type->stack_size * qty / itype::damage_scale, charges );
        destroy |= charges == 0;
    }

    if( qty > 0 ) {
        on_damage( qty, dt );
    }

    if( !count_by_charges() ) {
        destroy |= damage_ + qty > max_damage();

        damage_ = std::max( std::min( damage_ + qty, max_damage() ), min_damage() );
    }

    return destroy;
}

bool item::mod_damage( const int qty )
{
    return mod_damage( qty, DT_NULL );
}

bool item::inc_damage( const damage_type dt )
{
    return mod_damage( itype::damage_scale, dt );
}

bool item::inc_damage()
{
    return inc_damage( DT_NULL );
}

nc_color item::damage_color() const
{
    // TODO: unify with veh_interact::countDurability
    switch( damage_level( 4 ) ) {
        default:
            // reinforced
            if( damage() <= min_damage() ) {
                // fully reinforced
                return c_green;
            } else {
                return c_light_green;
            }
        case 0:
            return c_light_green;
        case 1:
            return c_yellow;
        case 2:
            return c_magenta;
        case 3:
            return c_light_red;
        case 4:
            if( damage() >= max_damage() ) {
                return c_dark_gray;
            } else {
                return c_red;
            }
    }
}

std::string item::damage_symbol() const
{
    switch( damage_level( 4 ) ) {
        default:
            // reinforced
            return _( R"(++)" );
        case 0:
            return _( R"(||)" );
        case 1:
            return _( R"(|\)" );
        case 2:
            return _( R"(|.)" );
        case 3:
            return _( R"(\.)" );
        case 4:
            if( damage() >= max_damage() ) {
                return _( R"(XX)" );
            } else {
                return _( R"(..)" );
            }

    }
}

std::string item::durability_indicator( bool include_intact ) const
{
    std::string outputstring;

    if( damage() < 0 )  {
        if( get_option<bool>( "ITEM_HEALTH_BAR" ) ) {
            outputstring = colorize( damage_symbol() + "\u00A0", damage_color() );
        } else if( is_gun() ) {
            outputstring = pgettext( "damage adjective", "accurized " );
        } else {
            outputstring = pgettext( "damage adjective", "reinforced " );
        }
    } else if( has_flag( flag_CORPSE ) ) {
        if( damage() > 0 ) {
            switch( damage_level( 4 ) ) {
                case 1:
                    outputstring = pgettext( "damage adjective", "bruised " );
                    break;
                case 2:
                    outputstring = pgettext( "damage adjective", "damaged " );
                    break;
                case 3:
                    outputstring = pgettext( "damage adjective", "mangled " );
                    break;
                default:
                    outputstring = pgettext( "damage adjective", "pulped " );
                    break;
            }
        }
    } else if( get_option<bool>( "ITEM_HEALTH_BAR" ) ) {
        outputstring = colorize( damage_symbol() + "\u00A0", damage_color() );
    } else {
        outputstring = string_format( "%s ", get_base_material().dmg_adj( damage_level( 4 ) ) );
        if( include_intact && outputstring == " " ) {
            outputstring = _( "fully intact " );
        }
    }

    return  outputstring;
}

const std::set<itype_id> &item::repaired_with() const
{
    static std::set<itype_id> no_repair;
    return has_flag( flag_NO_REPAIR )  ? no_repair : type->repair;
}

void item::mitigate_damage( damage_unit &du ) const
{
    const resistances res = resistances( *this );
    const float mitigation = res.get_effective_resist( du );
    // get_effective_resist subtracts the flat penetration value before multiplying the remaining armor.
    // therefore, res_pen is reduced by the full value of the item's armor value even though mitigation might be smaller (such as an attack with a 0.5 armor multiplier)
    du.res_pen = std::max( 0.0f, du.res_pen - res.type_resist( du.type ) );
    du.amount = std::max( 0.0f, du.amount - mitigation );
}

int item::damage_resist( damage_type dt, bool to_self ) const
{
    switch( dt ) {
        case DT_NULL:
        case NUM_DT:
            return 0;
        case DT_TRUE:
        case DT_BIOLOGICAL:
        case DT_ELECTRIC:
        case DT_COLD:
        case DT_DARK:
        case DT_LIGHT:
        case DT_PSI:
            // Currently hardcoded:
            // Items can never be damaged by those types
            // But they provide 0 protection from them
            return to_self ? INT_MAX : 0;
        case DT_BASH:
            return bash_resist( to_self );
        case DT_CUT:
            return cut_resist( to_self );
        case DT_ACID:
            return acid_resist( to_self );
        case DT_STAB:
            return stab_resist( to_self );
        case DT_HEAT:
            return fire_resist( to_self );
        case DT_BULLET:
            return bullet_resist( to_self );
        default:
            debugmsg( "Invalid damage type: %d", dt );
    }

    return 0;
}

bool item::is_two_handed( const Character &guy ) const
{
    // Big weapons are always two-handed if you're Medium or smaller.
    if( has_flag( flag_ALWAYS_TWOHAND ) && guy.get_size() <= creature_size::medium ) {
        return true;
    }
    // Large characters get a penalty when trying to wield two-handed weapons, Huge characters treat them like normal.
    const float str_factor = has_flag( flag_ALWAYS_TWOHAND ) &&
                             guy.get_size() != creature_size::huge ?  2.0f : 4.0f;
    ///\EFFECT_STR determines which weapons can be wielded with one hand
    return ( ( weight() / 113_gram ) > guy.str_cur * str_factor );
}

const std::vector<material_id> &item::made_of() const
{
    if( is_corpse() ) {
        return corpse->mat;
    }
    return type->materials;
}

const std::map<quality_id, int> &item::quality_of() const
{
    return type->qualities;
}

std::vector<const material_type *> item::made_of_types() const
{
    std::vector<const material_type *> material_types_composed_of;
    for( const material_id &mat_id : made_of() ) {
        material_types_composed_of.push_back( &mat_id.obj() );
    }
    return material_types_composed_of;
}

bool item::made_of_any( const std::set<material_id> &mat_idents ) const
{
    const std::vector<material_id> &mats = made_of();
    if( mats.empty() ) {
        return false;
    }

    return std::ranges::any_of( mats, [&mat_idents]( const material_id & e ) {
        return mat_idents.count( e );
    } );
}

bool item::only_made_of( const std::set<material_id> &mat_idents ) const
{
    const std::vector<material_id> &mats = made_of();
    if( mats.empty() ) {
        return false;
    }

    return std::ranges::all_of( mats, [&mat_idents]( const material_id & e ) {
        return mat_idents.count( e );
    } );
}

bool item::made_of( const material_id &mat_ident ) const
{
    const std::vector<material_id> &materials = made_of();
    return std::ranges::find( materials, mat_ident ) != materials.end();
}

bool item::contents_made_of( const phase_id phase ) const
{
    return !contents.empty() && contents.front().made_of( phase );
}

bool item::made_of( phase_id phase ) const
{
    if( is_null() ) {
        return false;
    }
    return type->phase == phase;
}

bool item::conductive() const
{
    if( is_null() ) {
        return false;
    }

    if( has_flag( flag_CONDUCTIVE ) ) {
        return true;
    }

    if( has_flag( flag_NONCONDUCTIVE ) ) {
        return false;
    }

    // If any material has electricity resistance equal to or lower than flesh (1) we are conductive.
    const std::vector<const material_type *> &mats = made_of_types();
    return std::ranges::any_of( mats, []( const material_type * mt ) {
        return mt->elec_resist() <= 1;
    } );
}

bool item::reinforceable() const
{
    if( is_null() || has_flag( flag_NO_REPAIR ) ) {
        return false;
    }

    // If a material is reinforceable, so are we
    const std::vector<const material_type *> &mats = made_of_types();
    return std::ranges::any_of( mats, []( const material_type * mt ) {
        return mt->reinforces();
    } );
}

bool item::destroyed_at_zero_charges() const
{
    return ( is_ammo() || is_food() );
}

bool item::is_gun() const
{
    return !!type->gun;
}

bool item::is_firearm() const
{
    return is_gun() && !has_flag( flag_PRIMITIVE_RANGED_WEAPON );
}

int item::get_reload_time() const
{
    if( !is_gun() && !is_magazine() ) {
        return 0;
    }

    int reload_time = is_gun() ? type->gun->reload_time : type->magazine->reload_time;
    for( const item *mod : gunmods() ) {
        reload_time = ( reload_time * ( 100 + mod->type->gunmod->reload_modifier ) / 100 );
    }

    return reload_time;
}

bool item::is_silent() const
{
    // Most guns with a suppressor installed will be under this value, also see item::gun_noise in ranged.cpp
    return gun_noise().volume < 50;
}

bool item::is_gunmod() const
{
    return !!type->gunmod;
}

bool item::is_bionic() const
{
    return !!type->bionic;
}

bool item::is_magazine() const
{
    return !!type->magazine;
}

bool item::is_battery() const
{
    return !!type->battery;
}

bool item::is_ammo_belt() const
{
    return is_magazine() && has_flag( flag_MAG_BELT );
}

bool item::is_bandolier() const
{
    return type->can_use( "bandolier" );
}

bool item::is_holster() const
{
    return type->can_use( "holster" );
}

bool item::is_ammo() const
{
    return !!type->ammo;
}

bool item::is_comestible() const
{
    return !!get_comestible();
}

bool item::is_food() const
{
    return is_comestible() && ( get_comestible()->comesttype == "FOOD" ||
                                get_comestible()->comesttype == "DRINK" );
}

bool item::is_medication() const
{
    return is_comestible() && get_comestible()->comesttype == "MED";
}

bool item::is_brewable() const
{
    return !!type->brewable;
}

bool item::is_food_container() const
{
    return ( !contents.empty() && contents.front().is_food() ) ||
           ( is_craft() &&
             craft_data_->making->create_result()->is_food_container() );
}

bool item::is_med_container() const
{
    return !contents.empty() && contents.front().is_medication();
}

bool item::is_corpse() const
{
    return corpse != nullptr && has_flag( flag_CORPSE );
}

const mtype *item::get_mtype() const
{
    return corpse;
}

template<typename Item>
static Item *get_food_impl( Item *it )
{
    if( it->is_food() ) {
        return it;
    } else if( it->is_food_container() && !it->contents.empty() ) {
        return &it->contents.front();
    } else {
        return nullptr;
    }
}

item *item::get_food()
{
    return get_food_impl( this );
}

const item *item::get_food() const
{
    return get_food_impl( this );
}

void item::set_mtype( const mtype *const m )
{
    // This is potentially dangerous, e.g. for corpse items, which *must* have a valid mtype pointer.
    if( m == nullptr ) {
        debugmsg( "setting item::corpse of %s to NULL", tname() );
        return;
    }
    corpse = m;
}

bool item::is_ammo_container() const
{
    return !is_magazine() && !contents.empty() && contents.front().is_ammo();
}

bool item::is_melee() const
{
    for( int idx = DT_NULL + 1; idx != NUM_DT; ++idx ) {
        if( is_melee( static_cast<damage_type>( idx ) ) ) {
            return true;
        }
    }
    return false;
}

bool item::is_melee( damage_type dt ) const
{
    return damage_melee( dt ) > MELEE_STAT;
}

const islot_armor *item::find_armor_data() const
{
    if( type->armor ) {
        return &*type->armor;
    }
    // Currently the only way to make a non-armor item into armor is to install a gun mod.
    // The gunmods are stored in the items contents, as are the contents of a container, and the
    // tools in a tool belt (a container actually), or the ammo in a quiver (container again).
    for( const item *mod : gunmods() ) {
        if( mod->type->armor ) {
            return &*mod->type->armor;
        }
    }
    return nullptr;
}

bool item::is_pet_armor( bool on_pet ) const
{
    bool is_worn = on_pet && !get_var( "pet_armor", "" ).empty();
    return has_flag( flag_IS_PET_ARMOR ) && ( is_worn || !on_pet );
}

bool item::is_armor() const
{
    return find_armor_data() != nullptr || has_flag( flag_IS_ARMOR );
}

bool item::is_book() const
{
    return !!type->book;
}

bool item::is_map() const
{
    return get_category().get_id() == itemcat_maps;
}

bool item::is_container() const
{
    return !!type->container;
}

bool item::is_watertight_container() const
{
    return type->container && type->container->watertight && type->container->seals;
}

bool item::is_non_resealable_container() const
{
    return type->container && !type->container->seals && type->container->unseals_into;
}

bool item::is_bucket() const
{
    // That "preserves" part is a hack:
    // Currently all non-empty cans are effectively sealed at all times
    // Making them buckets would cause weirdness
    return type->container &&
           type->container->watertight &&
           !type->container->seals &&
           !type->container->unseals_into;
}

bool item::is_bucket_nonempty() const
{
    return is_bucket() && !is_container_empty();
}

bool item::is_engine() const
{
    return !!type->engine;
}

bool item::is_wheel() const
{
    return !!type->wheel;
}

bool item::is_fuel() const
{
    return !!type->fuel;
}

bool item::is_toolmod() const
{
    return !is_gunmod() && type->mod;
}

bool item::is_faulty() const
{
    return is_engine() ? !faults.empty() : false;
}

bool item::is_irremovable() const
{
    return has_flag( flag_IRREMOVABLE );
}

int item::wind_resist() const
{
    std::vector<const material_type *> materials = made_of_types();
    if( materials.empty() ) {
        debugmsg( "Called item::wind_resist on an item (%s [%s]) made of nothing!", tname(), typeId() );
        return 99;
    }

    int best = -1;
    for( const material_type *mat : materials ) {
        std::optional<int> resistance = mat->wind_resist();
        if( resistance && *resistance > best ) {
            best = *resistance;
        }
    }

    // Default to 99% effective
    if( best == -1 ) {
        return 99;
    }

    return best;
}

std::set<fault_id> item::faults_potential() const
{
    std::set<fault_id> res;
    res.insert( type->faults.begin(), type->faults.end() );
    return res;
}

int item::wheel_area() const
{
    return is_wheel() ? type->wheel->diameter * type->wheel->width : 0;
}

float item::fuel_energy() const
{
    return is_fuel() ? type->fuel->energy : 0.0f;
}

ter_id item::fuel_pump_terrain() const
{
    return is_fuel() ? type->fuel->pump_terrain : t_null;
}

bool item::has_explosion_data() const
{
    return is_fuel() ? type->fuel->has_explode_data : false;
}

struct fuel_explosion item::get_explosion_data()
{
    static struct fuel_explosion null_data;
    return has_explosion_data() ? type->fuel->explosion_data : null_data;
}

float item::get_kcal_mult() const
{
    return get_var( "kcal_mult", 1.0 );
}
void item::set_kcal_mult( float val )
{
    set_var( "kcal_mult", val );
}
bool item::is_container_empty() const
{
    return contents.empty();
}

bool item::is_container_full( bool allow_bucket ) const
{
    if( is_container_empty() ) {
        return false;
    }
    return get_remaining_capacity_for_liquid( contents.front(), allow_bucket ) == 0;
}

bool item::can_unload_liquid() const
{
    if( is_container_empty() ) {
        return true;
    }

    const item &cts = contents.front();
    bool cts_is_frozen_liquid = cts.made_of( LIQUID ) && cts.made_of( SOLID );
    return is_bucket() || !cts_is_frozen_liquid;
}

bool item::can_reload_with( const ammotype &ammo ) const
{
    return is_reloadable_helper( ammo->default_ammotype(), false );
}

bool item::can_reload_with( const itype_id &ammo ) const
{
    return is_reloadable_helper( ammo, false );
}

bool item::is_reloadable_with( const itype_id &ammo ) const
{
    return is_reloadable_helper( ammo, true );
}

bool item::is_reloadable_helper( const itype_id &ammo, bool now ) const
{
    // empty ammo is passed for listing possible ammo apparently, so it needs to return true.
    if( !is_reloadable() ) {
        return false;
    } else if( is_watertight_container() ) {
        if( ammo.is_empty() ) {
            return now ? !is_container_full() : true;
        } else if( ammo->phase != LIQUID ) {
            return false;
        } else {
            return now ? ( is_container_empty() || contents.front().typeId() == ammo ) : true;
        }
    } else if( magazine_integral() ) {
        if( !ammo.is_empty() ) {
            if( now && ammo_data() ) {
                if( ammo_current() != ammo ) {
                    return false;
                }
            } else {
                const itype *at = &*ammo;
                if( ( !at->ammo || !ammo_types().contains( at->ammo->type ) ) &&
                    !magazine_compatible().contains( ammo ) ) {
                    return false;
                }
            }
        }
        return now ? ( ammo_remaining() < ammo_capacity() ) : true;
    } else {
        return ammo.is_empty() ? true : magazine_compatible().count( ammo );
    }
}

bool item::is_craft() const
{
    return craft_data_ != nullptr;
}

bool item::is_funnel_container( units::volume &bigger_than ) const
{
    if( !is_bucket() && !is_watertight_container() ) {
        return false;
    }
    // TODO: consider linking funnel to item or -making- it an active item
    if( get_container_capacity() <= bigger_than ) {
        return false; // skip contents check, performance
    }
    if(
        contents.empty() ||
        contents.front().typeId() == itype_water ||
        contents.front().typeId() == itype_water_acid ||
        contents.front().typeId() == itype_water_acid_weak ) {
        bigger_than = get_container_capacity();
        return true;
    }
    return false;
}

bool item::is_emissive() const
{
    return light.luminance > 0 || type->light_emission > 0;
}

bool item::is_deployable() const
{
    return type->can_use( "deploy_furn" );
}

bool item::is_tool() const
{
    return !!type->tool;
}

bool item::is_transformable() const
{
    return type->use_methods.contains( "transform" );
}

bool item::is_artifact() const
{
    return !!type->artifact;
}

bool item::is_relic() const
{
    return !!relic_data;
}

const std::vector<enchantment> &item::get_enchantments() const
{
    if( !is_relic() ) {
        static const std::vector<enchantment> fallback;
        return fallback;
    }
    return relic_data->get_enchantments();
}

double item::bonus_from_enchantments( const Character &owner, double base,
                                      enchant_vals::mod value, bool round ) const
{
    double add = 0.0;
    double mul = 0.0;
    for( const enchantment &ench : get_enchantments() ) {
        if( ench.is_active( owner, *this ) ) {
            add += ench.get_value_add( value );
            mul += ench.get_value_multiply( value );
        }
    }
    // TODO: this part duplicates enchantment::calc_bonus()
    double ret = add + base * mul;
    if( round ) {
        ret = trunc( ret );
    }
    return ret;
}

double item::bonus_from_enchantments_wielded( double base, enchant_vals::mod value,
        bool round ) const
{
    double add = 0.0;
    double mul = 0.0;
    for( const enchantment &ench : get_enchantments() ) {
        if( ench.is_active_when_wielded() ) {
            add += ench.get_value_add( value );
            mul += ench.get_value_multiply( value );
        }
    }
    // TODO: this part duplicates enchantment::calc_bonus()
    double ret = add + base * mul;
    if( round ) {
        ret = trunc( ret );
    }
    return ret;
}

const std::vector<relic_recharge> &item::get_relic_recharge_scheme() const
{
    return relic_data->get_recharge_scheme();
}

bool item::can_contain( const item &it ) const
{
    // TODO: Volume check
    return can_contain( *it.type );
}

bool item::can_contain( const itype &tp ) const
{
    if( !type->container ) {
        // TODO: Tools etc.
        return false;
    }

    if( tp.phase == LIQUID && !type->container->watertight ) {
        return false;
    }

    // TODO: Acid in waterskins
    return true;
}

const item &item::get_contained() const
{
    if( contents.empty() ) {
        return null_item_reference();
    }
    return contents.front();
}

bool item::spill_contents( Character &c )
{
    if( !is_container() || is_container_empty() ) {
        return true;
    }

    if( c.is_npc() ) {
        return spill_contents( c.pos() );
    }

    contents.handle_liquid_or_spill( c );
    on_contents_changed();

    return true;
}

bool item::spill_contents( const tripoint &pos )
{
    if( !is_container() || is_container_empty() ) {
        return true;
    }

    for( detached_ptr<item> &it : contents.clear_items() ) {
        get_map().add_item_or_charges( pos, std::move( it ) );
    }

    return true;
}

int item::get_chapters() const
{
    if( !type->book ) {
        return 0;
    }
    return type->book->chapters;
}

int item::get_remaining_chapters( const Character &ch ) const
{
    const std::string var = string_format( "remaining-chapters-%d", ch.getID().get_value() );
    return get_var( var, get_chapters() );
}

void item::mark_chapter_as_read( const Character &ch )
{
    const std::string var = string_format( "remaining-chapters-%d", ch.getID().get_value() );
    if( type->book && type->book->chapters == 0 ) {
        // books without chapters will always have remaining chapters == 0, so we don't need to store them
        erase_var( var );
        return;
    }
    const int remain = std::max( 0, get_remaining_chapters( ch ) - 1 );
    set_var( var, remain );
}

std::vector<std::pair<const recipe *, int>> item::get_available_recipes( const Character &u ) const
{
    std::vector<std::pair<const recipe *, int>> recipe_entries;
    if( is_book() ) {
        for( const book_recipe &elem : type->book->recipes ) {
            if( u.get_skill_level( elem.recipe->skill_used ) >= elem.skill_level ) {
                recipe_entries.emplace_back( elem.recipe, elem.skill_level );
            }
        }
    } else if( has_var( "EIPC_RECIPES" ) ) {
        // See einkpc_download_memory_card() in iuse.cpp where this is set.
        const std::string recipes = get_var( "EIPC_RECIPES" );
        // Capture the index one past the delimiter, i.e. start of target string.
        size_t first_string_index = recipes.find_first_of( ',' ) + 1;
        while( first_string_index != std::string::npos ) {
            size_t next_string_index = recipes.find_first_of( ',', first_string_index );
            if( next_string_index == std::string::npos ) {
                break;
            }
            std::string new_recipe = recipes.substr( first_string_index,
                                     next_string_index - first_string_index );
            const recipe *r = &recipe_id( new_recipe ).obj();
            if( u.get_skill_level( r->skill_used ) >= r->difficulty ) {
                recipe_entries.emplace_back( r, r->difficulty );
            }
            first_string_index = next_string_index + 1;
        }
    }
    return recipe_entries;
}

const material_type &item::get_random_material() const
{
    return random_entry( made_of(), material_id::NULL_ID() ).obj();
}

const material_type &item::get_base_material() const
{
    const std::vector<material_id> &mats = made_of();
    return mats.empty() ? material_id::NULL_ID().obj() : mats.front().obj();
}

bool item::operator<( const item &other ) const
{
    const item_category &cat_a = get_category();
    const item_category &cat_b = other.get_category();
    if( cat_a != cat_b ) {
        return cat_a < cat_b;
    } else {
        const item *me = is_container() && !contents.empty() ? &contents.front() : this;
        const item *rhs = other.is_container() &&
                          !other.contents.empty() ? &other.contents.front() : &other;

        const itype *me_type = me->type;
        const itype *rhs_type = rhs->type;
        if( !me_type || !rhs_type ) {
            return !!me_type;
        }

        if( me_type->get_id() == rhs_type->get_id() ) {
            if( me->is_money() ) {
                return me->charges > rhs->charges;
            }
            return me->charges < rhs->charges;
        } else {
            std::string n1 = me_type->nname( 1 );
            std::string n2 = rhs_type->nname( 1 );
            return localized_compare( n1, n2 );
        }
    }
}

skill_id item::gun_skill() const
{
    if( !is_gun() ) {
        return skill_id::NULL_ID();
    }
    return type->gun->skill_used;
}

skill_id item::melee_skill() const
{
    if( !is_melee() ) {
        return skill_id::NULL_ID();
    }

    if( has_flag( flag_UNARMED_WEAPON ) ) {
        return skill_unarmed;
    }

    int hi = 0;
    skill_id res = skill_id::NULL_ID();

    for( int idx = DT_NULL + 1; idx != NUM_DT; ++idx ) {
        const int val = damage_melee( static_cast<damage_type>( idx ) );
        const skill_id &sk  = skill_by_dt( static_cast<damage_type>( idx ) );
        if( val > hi && sk ) {
            hi = val;
            res = sk;
        }
    }

    return res;
}

int item::gun_dispersion( bool with_ammo, bool with_scaling ) const
{
    if( !is_gun() ) {
        return 0;
    }
    int dispersion_sum = type->gun->dispersion;
    for( const item *mod : gunmods() ) {
        dispersion_sum += mod->type->gunmod->dispersion;
    }
    dispersion_sum += get_dispersion_bonus();
    int dispPerDamage = get_option< int >( "DISPERSION_PER_GUN_DAMAGE" );
    dispersion_sum += damage_level( 4 ) * dispPerDamage;
    dispersion_sum = std::max( dispersion_sum, 0 );
    if( with_ammo && ammo_data() ) {
        dispersion_sum += ammo_data()->ammo->dispersion;
    }
    if( !with_scaling ) {
        return dispersion_sum;
    }

    // Dividing dispersion by 15 temporarily as a gross adjustment,
    // will bake that adjustment into individual gun definitions in the future.
    // Absolute minimum gun dispersion is 1.
    double divider = get_option< float >( "GUN_DISPERSION_DIVIDER" );
    dispersion_sum = std::max( static_cast<int>( std::round( dispersion_sum / divider ) ), 1 );

    return dispersion_sum;
}

int item::sight_dispersion() const
{
    if( !is_gun() ) {
        return 0;
    }

    int res = has_flag( flag_DISABLE_SIGHTS ) ? 90 : type->gun->sight_dispersion;

    for( const item *e : gunmods() ) {
        const islot_gunmod &mod = *e->type->gunmod;
        if( mod.sight_dispersion < 0 || mod.aim_speed < 0 ) {
            continue; // skip gunmods which don't provide a sight
        }
        res = std::min( res, mod.sight_dispersion );
    }

    return res;
}

damage_instance item::gun_damage( bool with_ammo ) const
{
    if( !is_gun() ) {
        return damage_instance();
    }
    damage_instance ret = type->gun->damage;

    for( const item *mod : gunmods() ) {
        ret.add( mod->type->gunmod->damage );
    }

    if( with_ammo && ammo_data() ) {
        ret.add( ammo_data()->ammo->damage );
    }

    ret.add( get_ranged_damage_bonus() );

    int item_damage = damage_level( 4 );
    if( item_damage > 0 ) {
        // TODO: This isn't a good solution for multi-damage guns/ammos
        for( damage_unit &du : ret ) {
            if( du.amount <= 1.0 ) {
                continue;
            }
            du.amount = std::max<float>( 1.0f, du.amount - item_damage * 2 );
        }
    }

    return ret;
}

double item::gun_recoil_multiplier( bool bipod ) const
{
    double handling = type->gun->handling;
    for( const item *mod : gunmods() ) {
        if( bipod || !mod->has_flag( flag_BIPOD ) ) {
            handling += mod->type->gunmod->handling;
        }
    }

    // Rescale from JSON units which are intentionally specified as integral values
    handling /= 10;

    // Handling will almost always be above 1.0
    if( handling > 1.0 ) {
        return 1.0 / handling;
    } else {
        return 2.0 - handling;
    }
}

int item::gun_recoil( bool bipod ) const
{
    if( !is_gun() || ( ammo_required() && !ammo_remaining() ) ) {
        return 0;
    }

    int qty = type->gun->recoil;
    if( ammo_data() ) {
        qty += ammo_data()->ammo->recoil;
    }

    qty += get_recoil_bonus();

    return qty * gun_recoil_multiplier( bipod );
}

int item::gun_range( bool with_ammo ) const
{
    if( !is_gun() ) {
        return 0;
    }
    int ret = type->gun->range;
    for( const item *mod : gunmods() ) {
        ret += mod->type->gunmod->range;
    }
    if( with_ammo && ammo_data() ) {
        const auto &ammo_shape = ammo_data()->ammo->shape;
        if( ammo_shape ) {
            ret = ammo_shape->get_range();
        } else {
            int ret_thrown = 0;
            if( gun_skill() == skill_throw && ammo_data() ) {
                const itype *curammo = ammo_data();
                item &tmp = *item::spawn_temporary( item( curammo ) );
                ret_thrown += get_avatar().throw_range( tmp );
            }
            ret += std::max( ammo_data()->ammo->range, ret_thrown );
        }
    }
    ret += get_range_bonus();
    return std::min( std::max( 0, ret ), RANGE_HARD_CAP );
}

int item::gun_range( const player *p ) const
{
    int ret = gun_range( true );
    if( p == nullptr ) {
        return ret;
    }
    if( !p->meets_requirements( *this ) ) {
        return 0;
    }

    // Reduce bow range if player has less than minimum strength.
    ret *= ranged::str_draw_range_modifier( *this, *p );

    return std::max( 0, ret );
}

int item::gun_speed( bool with_ammo ) const
{
    if( !is_gun() ) {
        return 10;
    }
    int ret = type->gun->speed;
    for( const item *mod : gunmods() ) {
        ret += mod->type->gunmod->speed;
    }
    if( with_ammo && ammo_data() ) {
        ret += ammo_data()->ammo->speed;
    }
    return std::max( 0, ret );
}

double item::gun_aimed_crit_bonus( bool with_ammo ) const
{
    if( !is_gun() ) {
        return 0;
    }
    int ret = type->gun->aimedcritbonus;
    for( const item *mod : gunmods() ) {
        ret += mod->type->gunmod->aimedcritbonus;
    }
    if( with_ammo && ammo_data() ) {
        ret += ammo_data()->ammo->aimedcritbonus;
    }
    return std::max( 0, ret );
}

double item::gun_aimed_crit_max_bonus( bool with_ammo ) const
{
    if( !is_gun() ) {
        return 0;
    }
    int ret = type->gun->aimedcritmaxbonus;
    for( const item *mod : gunmods() ) {
        ret += mod->type->gunmod->aimedcritmaxbonus;
    }
    if( with_ammo && ammo_data() ) {
        ret += ammo_data()->ammo->aimedcritmaxbonus;
    }
    return std::max( 0, ret );
}

units::energy item::energy_remaining() const
{
    if( is_battery() ) {
        return energy;
    }

    return 0_J;
}

int item::ammo_remaining() const
{
    const item *mag = magazine_current();
    if( mag ) {
        return mag->ammo_remaining();
    }

    if( is_tool() || is_gun() ) {
        // includes auxiliary gunmods
        if( has_flag( flag_USES_BIONIC_POWER ) ) {
            int power = units::to_kilojoule( get_avatar().get_power_level() );
            return power;
        }
        return charges;
    }

    if( is_magazine() || is_bandolier() ) {
        int res = 0;
        for( const item *e : contents.all_items_top() ) {
            res += e->charges;
        }
        return res;
    }

    return 0;
}

int item::ammo_capacity() const
{
    return ammo_capacity( false );
}

int item::ammo_capacity( bool potential_capacity ) const
{
    int res = 0;

    const item *mag = magazine_current();

    if( has_flag( flag_USES_BIONIC_POWER ) ) {
        avatar &you = get_avatar();
        return you.get_max_power_level() / 1_kJ;
    }
    if( mag ) {
        return mag->ammo_capacity();
    }

    if( is_tool() ) {
        res = type->tool->max_charges;
        if( res == 0 && magazine_default() && potential_capacity ) {
            res = magazine_default()->magazine->capacity;
        }
        for( const item *e : toolmods() ) {
            res *= e->type->mod->capacity_multiplier;
        }
    }

    if( is_gun() ) {
        res = type->gun->clip;
        for( const item *e : gunmods() ) {
            res *= e->type->mod->capacity_multiplier;
        }
    }

    if( is_magazine() ) {
        res = type->magazine->capacity;
    }

    if( is_bandolier() ) {
        return dynamic_cast<const bandolier_actor *>
               ( type->get_use( "bandolier" )->get_actor_ptr() )->capacity;
    }

    return res;
}

int item::ammo_required() const
{
    if( is_tool() ) {
        return std::max( type->charges_to_use(), 0 );
    }

    if( is_gun() ) {
        if( ammo_types().empty() ) {
            return 0;
        } else if( has_flag( flag_FIRE_100 ) ) {
            return 100;
        } else if( has_flag( flag_FIRE_50 ) ) {
            return 50;
        } else if( has_flag( flag_FIRE_20 ) ) {
            return 20;
        } else {
            int modifier = 0;
            float multiplier = 1.0f;
            for( const item *mod : gunmods() ) {
                modifier += mod->type->gunmod->ammo_to_fire_modifier;
                multiplier *= mod->type->gunmod->ammo_to_fire_multiplier;
            }
            return ( type->gun->ammo_to_fire * multiplier ) + modifier;
        }
    }

    return 0;
}

bool item::ammo_sufficient( int qty ) const
{
    return ammo_remaining() >= ammo_required() * qty;
}

int item::ammo_consume( int qty, const tripoint &pos )
{
    if( qty < 0 ) {
        debugmsg( "Cannot consume negative quantity of ammo for %s", tname() );
        return 0;
    }

    item *mag = magazine_current();
    if( mag ) {
        const int res = mag->ammo_consume( qty, pos );
        if( res && ammo_remaining() == 0 ) {
            if( mag->has_flag( flag_MAG_DESTROY ) ) {
                remove_item( *mag );
            } else if( mag->has_flag( flag_MAG_EJECT ) ) {
                get_map().add_item( pos, remove_item( *mag ) );
            }
        }
        return res;
    }

    if( is_magazine() ) {
        int need = qty;
        while( !contents.empty() ) {
            item &e = contents.front();
            if( need >= e.charges ) {
                need -= e.charges;
                remove_item( contents.front() );
                e.destroy();
            } else {
                e.charges -= need;
                need = 0;
                break;
            }
        }
        return qty - need;

    } else if( is_tool() || is_gun() ) {
        qty = std::min( qty, charges );
        if( has_flag( flag_USES_BIONIC_POWER ) ) {
            avatar &you = get_avatar();
            charges = units::to_kilojoule( you.get_power_level() );
            you.mod_power_level( units::from_kilojoule( -qty ) );
        }
        charges -= qty;
        if( charges == 0 ) {
            curammo = nullptr;
        }
        return qty;
    }

    return 0;
}

const itype *item::ammo_data() const
{
    const item *mag = magazine_current();
    if( mag ) {
        return mag->ammo_data();
    }

    if( is_ammo() ) {
        return type;
    }

    if( is_magazine() ) {
        return !contents.empty() ? contents.front().ammo_data() : nullptr;
    }

    auto mods = is_gun() ? gunmods() : toolmods();
    for( const item *e : mods ) {
        if( !e->type->mod->ammo_modifier.empty() && e->ammo_current() &&
            e->ammo_current().is_valid() ) {
            return &*e->ammo_current();
        }
    }

    return curammo;
}

itype_id item::ammo_current() const
{
    const itype *ammo = ammo_data();
    return ammo ? ammo->get_id() : itype_id::NULL_ID();
}

const std::set<ammotype> &item::ammo_types( bool conversion ) const
{
    if( conversion ) {
        const std::vector<const item *> &mods = is_gun() ? gunmods() : toolmods();
        for( const item *e : mods ) {
            if( !e->type->mod->ammo_modifier.empty() ) {
                return e->type->mod->ammo_modifier;
            }
        }
    }

    if( is_gun() ) {
        return type->gun->ammo;
    } else if( is_tool() ) {
        return type->tool->ammo_id;
    } else if( is_magazine() ) {
        return type->magazine->type;
    }

    static std::set<ammotype> atypes = {};
    return atypes;
}

ammotype item::ammo_type() const
{
    if( is_ammo() ) {
        return type->ammo->type;
    }
    return ammotype::NULL_ID();
}

itype_id item::ammo_default( bool conversion ) const
{
    if( is_magazine() ) {
        return type->magazine->default_ammo;
    } else if( is_tool() && type->tool->default_ammo != itype_id::NULL_ID() ) {
        return type->tool->default_ammo;
    }

    const std::set<ammotype> &atypes = ammo_types( conversion );
    if( !atypes.empty() ) {
        itype_id res = ammotype( *atypes.begin() )->default_ammotype();
        if( !res.is_empty() ) {
            return res;
        }
    }
    return itype_id::NULL_ID();
}

itype_id item::common_ammo_default( bool conversion ) const
{
    if( !ammo_types( conversion ).empty() ) {
        for( const ammotype &at : ammo_types( conversion ) ) {
            const item *mag = magazine_current();
            if( mag && mag->type->magazine->type.contains( at ) ) {
                itype_id res = at->default_ammotype();
                if( !res.is_empty() ) {
                    return res;
                }
            }
        }
    }
    return itype_id::NULL_ID();
}

std::set<ammo_effect_str_id> item::ammo_effects( bool with_ammo ) const
{
    if( !is_gun() ) {
        return std::set<ammo_effect_str_id>();
    }

    std::set<ammo_effect_str_id> res = type->gun->ammo_effects;
    if( with_ammo && ammo_data() ) {
        res.insert( ammo_data()->ammo->ammo_effects.begin(), ammo_data()->ammo->ammo_effects.end() );
    }

    for( const item *mod : gunmods() ) {
        res.insert( mod->type->gunmod->ammo_effects.begin(), mod->type->gunmod->ammo_effects.end() );
    }

    return res;
}

std::string item::ammo_sort_name() const
{
    if( is_magazine() || is_gun() || is_tool() ) {
        const std::set<ammotype> &types = ammo_types();
        if( !types.empty() ) {
            return ammotype( *types.begin() )->name();
        }
    }
    if( is_ammo() ) {
        return ammo_type()->name();
    }
    return "";
}

bool item::magazine_integral() const
{
    // If it has a default magazine, it can't have an integral magazine.
    if( magazine_default() ) {
        return false;
    } else if( is_gun() ) {
        // We have an integral magazine if we're a gun with an ammo capacity (clip)
        return type->gun->clip;
    } else if( is_tool() ) {
        // Or we are a tool with max_charges defined
        return type->tool->max_charges;
    }
    return true;
}

itype_id item::magazine_default( bool conversion ) const
{
    if( !ammo_types( conversion ).empty() ) {
        if( conversion ) {
            for( const item *m : is_gun() ? gunmods() : toolmods() ) {
                if( !m->type->mod->magazine_adaptor.empty() ) {
                    auto mags = m->type->mod->magazine_adaptor.find( ammotype( *ammo_types( conversion ).begin() ) );
                    if( mags != m->type->mod->magazine_adaptor.end() &&
                        !( *mags->second.begin() )->has_flag( flag_SPEEDLOADER ) ) {
                        return *( mags->second.begin() );
                    }
                }
            }
        }
        auto mag = type->magazine_default.find( ammotype( *ammo_types( conversion ).begin() ) );
        if( mag != type->magazine_default.end() && !mag->second->has_flag( flag_SPEEDLOADER ) ) {
            return mag->second;
        }
    }
    return itype_id::NULL_ID();
}

std::set<itype_id> item::magazine_compatible( bool conversion ) const
{
    std::set<itype_id> mags = {};
    // mods that define magazine_adaptor may override the items usual magazines
    const std::vector<const item *> &mods = is_gun() ? gunmods() : toolmods();
    for( const item *m : mods ) {
        if( !m->type->mod->magazine_adaptor.empty() ) {
            for( const ammotype &atype : ammo_types( conversion ) ) {
                if( m->type->mod->magazine_adaptor.contains( atype ) ) {
                    std::set<itype_id> magazines_for_atype = m->type->mod->magazine_adaptor.find( atype )->second;
                    mags.insert( magazines_for_atype.begin(), magazines_for_atype.end() );
                }
            }
            return mags;
        }
    }

    for( const ammotype &atype : ammo_types( conversion ) ) {
        if( type->magazines.contains( atype ) ) {
            std::set<itype_id> magazines_for_atype = type->magazines.find( atype )->second;
            mags.insert( magazines_for_atype.begin(), magazines_for_atype.end() );
        }
    }
    return mags;
}

item *item::magazine_current()
{
    return contents.get_item_with(
    []( const item & it ) {
        return it.is_magazine();
    } );
}

const item *item::magazine_current() const
{
    return const_cast<item *>( this )->magazine_current();
}

std::vector<item *> item::gunmods()
{
    return contents.gunmods();
}

std::vector<const item *> item::gunmods() const
{
    return contents.gunmods();
}

item *item::gunmod_find( const itype_id &mod )
{
    std::vector<item *> mods = gunmods();
    auto it = std::ranges::find_if( mods, [&mod]( item * e ) {
        return e->typeId() == mod;
    } );
    return it != mods.end() ? *it : nullptr;
}

const item *item::gunmod_find( const itype_id &mod ) const
{
    return const_cast<item *>( this )->gunmod_find( mod );
}

ret_val<bool> item::is_gunmod_compatible( const item &mod ) const
{
    if( !mod.is_gunmod() ) {
        debugmsg( "Tried checking compatibility of non-gunmod" );
        return ret_val<bool>::make_failure();
    }
    const islot_gunmod &g_mod = *mod.type->gunmod;

    if( !is_gun() ) {
        return ret_val<bool>::make_failure( _( "isn't a weapon" ) );

    } else if( is_gunmod() ) {
        return ret_val<bool>::make_failure( _( "is a gunmod and cannot be modded" ) );

    } else if( gunmod_find( mod.typeId() ) ) {
        return ret_val<bool>::make_failure( _( "already has a %s" ), mod.tname( 1 ) );

    } else if( !get_mod_locations().contains( g_mod.location ) ) {
        return ret_val<bool>::make_failure( _( "doesn't have a slot for this mod" ) );

    } else if( get_free_mod_locations( g_mod.location ) <= 0 ) {
        return ret_val<bool>::make_failure( _( "doesn't have enough room for another %s mod" ),
                                            mod.type->gunmod->location.name() );

    } else if( !g_mod.usable.empty() || !g_mod.usable_category.empty() || !g_mod.exclusion.empty() ||
               !g_mod.exclusion_category.empty() ) {
        // First check that it's not explicitly excluded by id.
        bool excluded = g_mod.exclusion.contains( this->typeId() );
        // Then check if it's excluded by category.
        for( const std::unordered_set<weapon_category_id> &mod_cat : g_mod.exclusion_category ) {
            if( excluded ) {
                break;
            }
            if( std::ranges::all_of( mod_cat, [this]( const weapon_category_id & wcid ) {
            return this->type->weapon_category.count( wcid );
            } ) ) {
                excluded = true;
            }
        }

        // Check that it's included by id, if so, override banned so it's allowed.
        // A check is already in item_factory so that explicit inclusion and exclusion of the same id throws errors.
        bool usable = g_mod.usable.contains( this->typeId() );
        if( usable ) {
            excluded = false;
        }
        // Then check that it's included by category. If banned is still true, skip, no point checking.
        for( const std::unordered_set<weapon_category_id> &mod_cat : g_mod.usable_category ) {
            if( usable || excluded ) {
                break;
            }
            if( std::ranges::all_of( mod_cat, [this]( const weapon_category_id & wcid ) {
            return this->type->weapon_category.count( wcid );
            } ) ) {
                usable = true;
            }
        }
        if( !usable || excluded ) {
            return ret_val<bool>::make_failure( _( "cannot have a %s" ), mod.tname() );
        }

    } else if( g_mod.location.str() == "underbarrel" &&
               !mod.has_flag( flag_PUMP_RAIL_COMPATIBLE ) && has_flag( flag_PUMP_ACTION ) ) {
        return ret_val<bool>::make_failure( _( "can only accept small mods on that slot" ) );

    } else if( !mod.type->mod->acceptable_ammo.empty() ) {
        bool compat_ammo = false;
        for( const ammotype &at : mod.type->mod->acceptable_ammo ) {
            if( ammo_types( false ).contains( at ) ) {
                compat_ammo = true;
            }
        }
        if( !compat_ammo ) {
            return ret_val<bool>::make_failure(
                       _( "%1$s cannot be used on item with no compatible ammo types" ), mod.tname( 1 ) );
        }
    } else if( mod.typeId() == itype_waterproof_gunmod && has_flag( flag_WATERPROOF_GUN ) ) {
        return ret_val<bool>::make_failure( _( "is already waterproof" ) );

    } else if( mod.typeId() == itype_tuned_mechanism && has_flag( flag_NEVER_JAMS ) ) {
        return ret_val<bool>::make_failure( _( "is already eminently reliable" ) );

    } else if( mod.has_flag( flag_BRASS_CATCHER ) && has_flag( flag_RELOAD_EJECT ) ) {
        return ret_val<bool>::make_failure( _( "cannot have a brass catcher" ) );

    } else if( ( !mod.type->mod->ammo_modifier.empty() || !mod.type->mod->magazine_adaptor.empty() )
               && ( ammo_remaining() > 0 || magazine_current() ) ) {
        return ret_val<bool>::make_failure( _( "must be unloaded before installing this mod" ) );
    }

    for( const gunmod_location &slot : mod.type->gunmod->blacklist_mod ) {
        if( get_mod_locations().contains( slot ) ) {
            return ret_val<bool>::make_failure( _( "cannot be installed on a weapon with \"%s\"" ),
                                                slot.name() );
        }
    }

    return ret_val<bool>::make_success();
}

std::map<gun_mode_id, gun_mode> item::gun_all_modes() const
{
    std::map<gun_mode_id, gun_mode> res;

    if( !is_gun() || is_gunmod() ) {
        return res;
    }

    std::vector<const item *> opts = gunmods();
    opts.push_back( this );

    for( const item *e : opts ) {

        // handle base item plus any auxiliary gunmods
        if( e->is_gun() ) {
            for( const std::pair<const gun_mode_id, gun_modifier_data> &m : e->type->gun->modes ) {
                // prefix attached gunmods, e.g. M203_DEFAULT to avoid index key collisions
                std::string prefix = e->is_gunmod() ? ( std::string( e->typeId() ) += "_" ) : "";
                std::ranges::transform( prefix, prefix.begin(),
                                        static_cast<int( * )( int )>( toupper ) );

                const int qty = m.second.qty();

                res.emplace( gun_mode_id( prefix + m.first.str() ), gun_mode( m.second.name(),
                             const_cast<item *>( e ),
                             qty, m.second.flags() ) );
            }

            // non-auxiliary gunmods may provide additional modes for the base item
        } else if( e->is_gunmod() ) {
            for( const std::pair<const gun_mode_id, gun_modifier_data> &m : e->type->gunmod->mode_modifier ) {
                //checks for melee gunmod, points to gunmod
                if( m.first == gun_mode_REACH ) {
                    res.emplace( m.first, gun_mode { m.second.name(), const_cast<item *>( e ),
                                                     m.second.qty(), m.second.flags() } );
                    //otherwise points to the parent gun, not the gunmod
                } else {
                    res.emplace( m.first, gun_mode { m.second.name(), const_cast<item *>( this ),
                                                     m.second.qty(), m.second.flags() } );
                }
            }
        }
    }

    return res;
}

gun_mode item::gun_get_mode( const gun_mode_id &mode ) const
{
    if( is_gun() ) {
        for( const std::pair<const gun_mode_id, gun_mode> &e : gun_all_modes() ) {
            if( e.first == mode ) {
                return e.second;
            }
        }
    }
    return gun_mode();
}

gun_mode item::gun_current_mode() const
{
    return gun_get_mode( gun_get_mode_id() );
}

gun_mode_id item::gun_get_mode_id() const
{
    if( !is_gun() || is_gunmod() ) {
        return gun_mode_id();
    }
    return gun_mode_id( get_var( GUN_MODE_VAR_NAME, "DEFAULT" ) );
}

bool item::gun_set_mode( const gun_mode_id &mode )
{
    if( !is_gun() || is_gunmod() || !gun_all_modes().contains( mode ) ) {
        return false;
    }
    set_var( GUN_MODE_VAR_NAME, mode.str() );
    return true;
}

void item::gun_cycle_mode()
{
    if( !is_gun() || is_gunmod() ) {
        return;
    }

    const gun_mode_id cur = gun_get_mode_id();
    const std::map<gun_mode_id, gun_mode> modes = gun_all_modes();

    for( auto iter = modes.begin(); iter != modes.end(); ++iter ) {
        if( iter->first == cur ) {
            if( std::next( iter ) == modes.end() ) {
                break;
            }
            gun_set_mode( std::next( iter )->first );
            return;
        }
    }
    gun_set_mode( modes.begin()->first );

    return;
}

bool item::has_use() const
{
    return type->has_use();
}

const use_function *item::get_use( const std::string &use_name ) const
{
    const use_function *fun = nullptr;
    visit_items(
    [&fun, &use_name]( const item * it ) {
        if( it == nullptr ) {
            return VisitResponse::SKIP;
        }
        fun = it->get_use_internal( use_name );
        if( fun != nullptr ) {
            return VisitResponse::ABORT;
        }
        return VisitResponse::NEXT;
    } );

    return fun;
}

const use_function *item::get_use_internal( const std::string &use_name ) const
{
    if( type != nullptr ) {
        return type->get_use( use_name );
    }
    return nullptr;
}

const item *item::get_usable_item( const std::string &use_name ) const
{
    const item *ret = nullptr;
    visit_items(
    [&ret, &use_name]( const item * it ) {
        if( it == nullptr ) {
            return VisitResponse::SKIP;
        }
        if( it->get_use_internal( use_name ) ) {
            ret = it;
            return VisitResponse::ABORT;
        }
        return VisitResponse::NEXT;
    } );

    return ret;
}

item *item::get_usable_item( const std::string &use_name )
{
    return const_cast<item *>( const_cast<const item *>( this )->get_usable_item( use_name ) );
}

int item::units_remaining( const Character &ch, int limit ) const
{
    if( count_by_charges() ) {
        return std::min( static_cast<int>( charges ), limit );
    }

    int res = ammo_remaining();
    if( res < limit && is_power_armor() ) {
        if( character_funcs::can_interface_armor( ch ) && has_flag( flag_USE_UPS ) ) {
            res += std::max( ch.charges_of( itype_UPS, limit - res ), ch.charges_of( itype_bio_armor,
                             limit - res ) );
        } else if( character_funcs::can_interface_armor( ch ) ) {
            res += ch.charges_of( itype_bio_armor, limit - res );
        } else {
            res += ch.charges_of( itype_UPS, limit - res );
        }
    } else if( res < limit && has_flag( flag_USE_UPS ) ) {
        res += ch.charges_of( itype_UPS, limit - res );
    }

    return std::min( res, limit );
}

bool item::units_sufficient( const Character &ch, int qty ) const
{
    if( qty < 0 ) {
        qty = count_by_charges() ? 1 : ammo_required();
    }

    return units_remaining( ch, qty ) == qty;
}

item_reload_option::item_reload_option( const item_reload_option & ) = default;

item_reload_option &item_reload_option::operator=( const item_reload_option & ) = default;

item_reload_option::item_reload_option( const player *who, item *target, const item *parent,
                                        item &ammo ) :
    who( who ), target( target ), ammo( &ammo ), parent( parent )
{
    if( this->target->is_ammo_belt() ) {
        const auto &linkage = this->target->type->magazine->linkage ;
        if( linkage ) {
            max_qty = this->who->charges_of( *linkage );
        }
    }
    qty( max_qty );
}

int item_reload_option::moves() const
{
    int mv = ammo->obtain_cost( *who, qty() ) + who->item_reload_cost( *target, *ammo, qty() );
    if( parent != target ) {
        if( parent->is_gun() ) {
            mv += parent->get_reload_time();
        } else if( parent->is_tool() ) {
            mv += 100;
        }
    }
    return mv;
}

void item_reload_option::qty( int val )
{
    bool ammo_in_container = ammo->is_ammo_container();
    bool ammo_in_liquid_container = ammo->is_watertight_container();
    item &ammo_obj = ( ammo_in_container || ammo_in_liquid_container ) ?
                     ammo->contents.front() : *ammo;

    if( ( ammo_in_container && !ammo_obj.is_ammo() ) ||
        ( ammo_in_liquid_container && !ammo_obj.made_of( LIQUID ) ) ) {
        debugmsg( "Invalid reload option: %s", ammo_obj.tname() );
        return;
    }

    // Checking ammo capacity implicitly limits guns with removable magazines to capacity 0.
    // This gets rounded up to 1 later.
    int remaining_capacity = target->is_watertight_container() ?
                             target->get_remaining_capacity_for_liquid( ammo_obj, true ) :
                             target->ammo_capacity() - target->ammo_remaining();
    if( target->has_flag( flag_RELOAD_ONE ) && !ammo->has_flag( flag_SPEEDLOADER ) ) {
        remaining_capacity = 1;
    }
    if( ammo_obj.type->ammo ) {
        if( ammo_obj.ammo_type() == ammo_plutonium ) {
            remaining_capacity = remaining_capacity / PLUTONIUM_CHARGES +
                                 ( remaining_capacity % PLUTONIUM_CHARGES != 0 );
        }
    }

    bool ammo_by_charges = ammo_obj.is_ammo() || ammo_in_liquid_container;
    int available_ammo = ammo_by_charges ? ammo_obj.charges : ammo_obj.ammo_remaining();
    // constrain by available ammo, target capacity and other external factors (max_qty)
    // @ref max_qty is currently set when reloading ammo belts and limits to available linkages
    qty_ = std::min( { val, available_ammo, remaining_capacity, max_qty } );

    // always expect to reload at least one charge
    qty_ = std::max( qty_, 1 );

}

int item::casings_count() const
{
    int res = 0;

    const_cast<item *>( this )->casings_handle( [&res]( detached_ptr<item> &&it ) {
        ++res;
        return std::move( it );
    } );

    return res ;
}

void item::casings_handle( const std::function < detached_ptr<item>( detached_ptr<item> && ) >
                           &func )
{
    if( !is_gun() ) {
        return;
    }

    contents.casings_handle( func );
}

bool item::reload( Character &who, item &loc, int qty )
{
    if( qty <= 0 ) {
        debugmsg( "Tried to reload zero or less charges" );
        return false;
    }
    item *ammo = &loc;
    if( ammo->is_null() ) {
        debugmsg( "Tried to reload using non-existent ammo" );
        return false;
    }

    item *container = nullptr;
    if( ammo->is_ammo_container() || ammo->is_container() ) {
        container = ammo;
        ammo = &ammo->contents.front();
    }

    if( !is_reloadable_with( ammo->typeId() ) ) {
        return false;
    }

    // limit quantity of ammo loaded to remaining capacity
    int limit = is_watertight_container()
                ? get_remaining_capacity_for_liquid( *ammo )
                : ammo_capacity() - ammo_remaining();

    if( ammo->ammo_type() == ammo_plutonium ) {
        limit = limit / PLUTONIUM_CHARGES + ( limit % PLUTONIUM_CHARGES != 0 );
    }

    qty = std::min( qty, limit );

    casings_handle( [&who]( detached_ptr<item> &&e ) {
        return who.i_add_or_drop( std::move( e ) );
    } );

    if( is_magazine() ) {
        qty = std::min( qty, ammo->charges );

        if( is_ammo_belt() ) {
            const auto &linkage = type->magazine->linkage;
            if( linkage && !who.use_charges_if_avail( *linkage, qty ) ) {
                debugmsg( "insufficient linkages available when reloading ammo belt" );
            }
        }

        detached_ptr<item> to_reload = ammo->split( qty );
        bool merged = false;
        for( item *it : contents.all_items_top() ) {
            if( it->merge_charges( std::move( to_reload ) ) ) {
                merged = true;
                break;
            }
        }
        if( !merged ) {
            // NOLINTNEXTLINE(bugprone-use-after-move)
            put_in( std::move( to_reload ) );
        }
    } else if( is_watertight_container() ) {
        if( !ammo->made_of( LIQUID ) ) {
            debugmsg( "Tried to reload liquid container with non-liquid." );
            return false;
        }
        if( container ) {
            container->on_contents_changed();
        }
        item &cur = *this;
        ammo->attempt_split( 0, [&cur, qty]( detached_ptr<item> &&it ) {
            return cur.fill_with( std::move( it ), qty );
        } );
    } else if( !magazine_integral() ) {
        // if we already have a magazine loaded prompt to eject it
        if( magazine_current() ) {
            //~ %1$s: magazine name, %2$s: weapon name
            std::string prompt = string_format( pgettext( "magazine", "Eject %1$s from %2$s?" ),
                                                magazine_current()->tname(), tname() );

            if( !who.dispose_item( *magazine_current(), prompt ) ) {
                return false;
            }
        }

        put_in( ammo->detach() );
        return true;

    } else {
        if( ammo->has_flag( flag_SPEEDLOADER ) ) {
            curammo = ammo->contents.front().type;
            qty = std::min( qty, ammo->ammo_remaining() );
            ammo->ammo_consume( qty, tripoint_zero );
            charges += qty;
        } else if( ammo->ammo_type() == ammo_plutonium ) {
            curammo = ammo->type;
            ammo->charges -= qty;

            // any excess is wasted rather than overfilling the item
            charges += qty * PLUTONIUM_CHARGES;
            charges = std::min( charges, ammo_capacity() );
        } else {
            curammo = ammo->type;
            qty = std::min( qty, ammo->charges );
            ammo->charges -= qty;
            charges += qty;
        }
        // we have transfered ammo from the container to the item
        // therefore, we erase the 0-charge item inside container
        // TODO: why don't we just remove 0-charge items?
        if( ammo->charges == 0 && !ammo->has_flag( flag_SPEEDLOADER ) ) {
            ammo->detach();
            if( container != nullptr ) {
                who.inv_restack();
            }
        }
    }

    return true;
}

float item::simulate_burn( fire_data &frd ) const
{
    const std::vector<material_id> &mats = made_of();
    float smoke_added = 0.0f;
    float time_added = 0.0f;
    float burn_added = 0.0f;
    const units::volume vol = base_volume();
    const int effective_intensity = frd.contained ? 3 : frd.fire_intensity;
    for( const material_id &m : mats ) {
        const mat_burn_data &bd = m.obj().burn_data( effective_intensity );
        if( bd.immune ) {
            // Made to protect from fire
            return 0.0f;
        }

        // If fire is contained, burn rate is independent of volume
        if( frd.contained || bd.volume_per_turn == 0_ml ) {
            time_added += bd.fuel;
            smoke_added += bd.smoke;
            burn_added += bd.burn;
        } else {
            double volume_burn_rate = to_liter( bd.volume_per_turn ) / to_liter( vol );
            time_added += bd.fuel * volume_burn_rate;
            smoke_added += bd.smoke * volume_burn_rate;
            burn_added += bd.burn * volume_burn_rate;
        }
    }

    // Liquids that don't burn well smother fire well instead
    if( made_of( LIQUID ) && time_added < 200 ) {
        time_added -= rng( 400.0 * to_liter( vol ), 1200.0 * to_liter( vol ) );
    } else if( mats.size() > 1 ) {
        // Average the materials
        time_added /= mats.size();
        smoke_added /= mats.size();
        burn_added /= mats.size();
    } else if( mats.empty() ) {
        // Non-liquid items with no specified materials will burn at moderate speed
        burn_added = 1;
    }

    if( count_by_charges() ) {
        const int stack_burnt = type->stack_size;
        time_added *= stack_burnt;
        smoke_added *= stack_burnt;
        burn_added *= stack_burnt;
    }

    frd.fuel_produced += time_added;
    frd.smoke_produced += smoke_added;
    return burn_added;
}

bool item::burn( fire_data &frd )
{
    float burn_added = simulate_burn( frd );

    if( burn_added <= 0 ) {
        return false;
    }

    if( count_by_charges() ) {
        if( type->volume == 0_ml ) {
            charges = 0;
        } else {
            charges -= roll_remainder( burn_added * units::legacy_volume_factor * type->stack_size /
                                       ( 3.0 * type->volume ) );
        }

        return charges <= 0;
    }

    if( is_corpse() ) {
        const mtype *mt = get_mtype();
        if( is_active() && mt != nullptr && burnt + burn_added > mt->hp &&
            !mt->burn_into.is_null() && mt->burn_into.is_valid() ) {
            corpse = &get_mtype()->burn_into.obj();
            // Delay rezing
            set_age( 0_turns );
            burnt = 0;
            return false;
        }
    }

    burnt += roll_remainder( burn_added );

    const int vol = base_volume() / units::legacy_volume_factor;
    return burnt >= vol * 3;
}

bool item::flammable( int threshold ) const
{
    const std::vector<const material_type *> &mats = made_of_types();
    if( mats.empty() ) {
        // Don't know how to burn down something made of nothing.
        return false;
    }

    int flammability = 0;
    units::volume volume_per_turn = 0_ml;
    for( const material_type *m : mats ) {
        const mat_burn_data &bd = m->burn_data( 1 );
        if( bd.immune ) {
            // Made to protect from fire
            return false;
        }

        flammability += bd.fuel;
        volume_per_turn += bd.volume_per_turn;
    }

    if( threshold == 0 || flammability <= 0 ) {
        return flammability > 0;
    }

    volume_per_turn /= mats.size();
    units::volume vol = base_volume();
    if( volume_per_turn > 0_ml && volume_per_turn < vol ) {
        flammability = flammability * volume_per_turn / vol;
    } else {
        // If it burns well, it provides a bonus here
        flammability *= vol / units::legacy_volume_factor;
    }

    return flammability > threshold;
}

const itype_id &item::typeId() const
{
    return type ? type->get_id() : itype_id::NULL_ID();
}

bool item::getlight( float &luminance, units::angle &width, units::angle &direction ) const
{
    luminance = 0;
    width = 0_degrees;
    direction = 0_degrees;
    if( light.luminance > 0 ) {
        luminance = static_cast<float>( light.luminance );
        if( light.width > 0 ) {  // width > 0 is a light arc
            width = units::from_degrees( light.width );
            direction = units::from_degrees( light.direction );
        }
        return true;
    } else {
        const int lumint = getlight_emit();
        if( lumint > 0 ) {
            luminance = static_cast<float>( lumint );
            return true;
        }
    }
    return false;
}

int item::getlight_emit() const
{
    float lumint = type->light_emission;

    if( lumint == 0 ) {
        return 0;
    }
    if( has_flag( flag_CHARGEDIM ) && is_tool() && !has_flag( flag_USE_UPS ) ) {
        // Falloff starts at 1/5 total charge and scales linearly from there to 0.
        if( ammo_capacity() && ammo_remaining() < ( ammo_capacity() / 5 ) ) {
            lumint *= ammo_remaining() * 5.0 / ammo_capacity();
        }
    }
    return lumint;
}

units::volume item::get_container_capacity() const
{
    if( !is_container() ) {
        return 0_ml;
    }
    return type->container->contains;
}

units::volume item::get_total_capacity() const
{
    units::volume result = get_storage() + get_container_capacity();

    // Consider various iuse_actors which add containing capability
    // Treating these two as special cases for now; if more appear in the
    // future then this probably warrants a new method on use_function to
    // access this information generically.
    if( is_bandolier() ) {
        result += dynamic_cast<const bandolier_actor *>
                  ( type->get_use( "bandolier" )->get_actor_ptr() )->max_stored_volume();
    }

    if( is_holster() ) {
        result += dynamic_cast<const holster_actor *>
                  ( type->get_use( "holster" )->get_actor_ptr() )->max_stored_volume();
    }

    return result;
}

int item::get_remaining_capacity_for_liquid( const item &liquid, bool allow_bucket,
        std::string *err ) const
{
    const auto error = [ &err ]( const std::string & message ) {
        if( err != nullptr ) {
            *err = message;
        }
        return 0;
    };

    int remaining_capacity = 0;

    // TODO: (sm) is_reloadable_with and this function call each other and can recurse for
    // watertight containers.
    if( !is_container() && is_reloadable_with( liquid.typeId() ) ) {
        if( ammo_remaining() != 0 && ammo_current() != liquid.typeId() ) {
            return error( string_format( _( "You can't mix loads in your %s." ), tname() ) );
        }
        remaining_capacity = ammo_capacity() - ammo_remaining();
    } else if( is_container() ) {
        if( !type->container->watertight ) {
            return error( string_format( _( "That %s isn't water-tight." ), tname() ) );
        } else if( !type->container->seals && ( !allow_bucket || !is_bucket() ) ) {
            return error( string_format( is_bucket() ?
                                         _( "That %s must be on the ground or held to hold contents!" )
                                         : _( "You can't seal that %s!" ), tname() ) );
        } else if( !contents.empty() && contents.front().typeId() != liquid.typeId() ) {
            return error( string_format( _( "You can't mix loads in your %s." ), tname() ) );
        }
        remaining_capacity = liquid.charges_per_volume( get_container_capacity() );
        if( !contents.empty() ) {
            remaining_capacity -= contents.front().charges;
        }
    } else {
        return error( string_format( _( "That %1$s won't hold %2$s." ), tname(),
                                     liquid.tname() ) );
    }

    if( remaining_capacity <= 0 ) {
        return error( string_format( _( "Your %1$s can't hold any more %2$s." ), tname(),
                                     liquid.tname() ) );
    }

    return remaining_capacity;
}

int item::get_remaining_capacity_for_liquid( const item &liquid, const Character &who,
        std::string *err ) const
{
    const bool allow_bucket = who.is_wielding( *this ) || !who.has_item( *this );
    int res = get_remaining_capacity_for_liquid( liquid, allow_bucket, err );

    if( res > 0 && !type->rigid && who.has_item( *this ) ) {
        const units::volume volume_to_expand = std::max( who.volume_capacity() - who.volume_carried(),
                                               0_ml );

        res = std::min( liquid.charges_per_volume( volume_to_expand ), res );

        if( res == 0 && err != nullptr ) {
            *err = string_format( _( "That %s doesn't have room to expand." ), tname() );
        }
    }

    return res;
}

int item::get_remaining_capacity_for_id( const itype_id &liquid, bool allow_buckets ) const
{
    int rem_cap = 0;
    itype obj = liquid.obj();
    if( !is_container() && is_reloadable_with( liquid ) ) {
        if( ammo_remaining() != 0 && ammo_current() != liquid ) {
            return 0;
        }
        rem_cap = ammo_capacity() - ammo_remaining();
    } else if( is_container() ) {
        if( !type->container->watertight ) {
            return 0;
        } else if( !type->container->seals && ( !allow_buckets || !is_bucket() ) ) {
            return 0;
        } else if( !contents.empty() && contents.front().typeId() != liquid ) {
            return 0;
        }
        rem_cap = obj.charges_per_volume( get_container_capacity() );
        if( !contents.empty() ) {
            rem_cap -= contents.front().charges;
        }
    }
    return rem_cap;
}

detached_ptr<item> item::use_amount( detached_ptr<item> &&self, const itype_id &it, int &quantity,
                                     std::vector<detached_ptr<item>> &used,
                                     const std::function<bool( const item & )> &filter )
{
    // Remember quantity so that we can unseal self
    int old_quantity = quantity;

    self->remove_items_with( [&]( detached_ptr<item> &&a ) {
        if( quantity > 0  && a->typeId() == it && filter( *a ) ) {
            used.push_back( std::move( a ) );
            quantity--;
            return VisitResponse::SKIP;
        }
        return VisitResponse::NEXT;
    } );

    if( quantity != old_quantity ) {
        self->on_contents_changed();
    }

    if( quantity > 0 && self->typeId() == it && filter( *self ) ) {
        used.push_back( std::move( self ) );
        quantity--;
        return detached_ptr<item>();
    }
    return std::move( self );
}


bool item::allow_crafting_component() const
{
    if( is_toolmod() && is_irremovable() ) {
        return false;
    }

    // vehicle batteries are implemented as magazines of charge
    if( is_magazine() && ammo_types().contains( ammo_battery ) ) {
        return true;
    }

    // fixes #18886 - turret installation may require items with irremovable mods
    if( is_gun() ) {
        bool valid = true;
        visit_items( [&]( const item * it ) {
            if( this == it ) {
                return VisitResponse::NEXT;
            }
            if( !( it->is_magazine() || ( it->is_gunmod() && it->is_irremovable() ) ) ) {
                valid = false;
                return VisitResponse::ABORT;
            }
            return VisitResponse::NEXT;
        } );
        return valid;
    }

    return contents.empty();
}

detached_ptr<item> item::fill_with( detached_ptr<item> &&liquid, int amount )
{
    if( amount == -1 ) {
        amount = INT_MAX;
    }
    amount = std::min( get_remaining_capacity_for_liquid( *liquid, true ),
                       std::min( amount, liquid->charges ) );
    if( amount <= 0 ) {
        return std::move( liquid );
    }

    if( !is_container() ) {
        if( !is_reloadable_with( liquid->typeId() ) ) {
            debugmsg( "Tried to fill %s which is not a container and can't be reloaded with %s.",
                      tname(), liquid->tname() );
            return std::move( liquid );
        }
        ammo_set( liquid->typeId(), ammo_remaining() + amount );
    } else if( is_food_container() ) {
        item &cts = contents.front();

        cts.set_rot( weighted_averaged_rot( &cts, &*liquid ) );
        cts.mod_charges( amount );
    } else if( !is_container_empty() ) {
        // if container already has liquid we need to set the amount
        item &cts = contents.front();
        cts.mod_charges( amount );
    } else {
        detached_ptr<item> liquid_copy = item::spawn( *liquid );
        liquid_copy->charges = amount;
        put_in( std::move( liquid_copy ) );
    }

    liquid->mod_charges( -amount );
    on_contents_changed();
    if( liquid->charges > 0 ) {
        return std::move( liquid );
    }
    return detached_ptr<item>();
}

void item::set_counter( const int value )
{
    item_counter = value;
}

int item::get_counter() const
{
    return item_counter;
}

void item::set_charges( int value )
{
    if( value < 0 ) {
        debugmsg( "Tried to set a negative charges value %d.", value );
        return;
    }
    if( !ammo_types().empty() ) {
        debugmsg( "Tried to set charges on an item with ammo." );
        return;
    }
    charges = value;
}

detached_ptr<item> item::use_charges( detached_ptr<item> &&self, const itype_id &what, int &qty,
                                      std::vector<detached_ptr<item>> &used,
                                      const tripoint &pos, const std::function<bool( const item & )> &filter )
{


    auto handle_item = [&qty, &used, &pos, &what]( detached_ptr<item> &&e ) {
        if( e->is_tool() ) {
            if( e->typeId() == what || ( what == itype_UPS && e->has_flag( flag_IS_UPS ) ) ) {
                int ups_eff_mult = e->type->tool->ups_eff_mult;
                int n = std::min( e->ammo_remaining() * ups_eff_mult, qty );
                int rand_increase = x_in_y( n % ups_eff_mult, ups_eff_mult );
                int really_used = ( n / ups_eff_mult ) + rand_increase;
                qty -= n;

                if( n == e->ammo_remaining() ) {
                    used.push_back( item::spawn( *e ) );
                    e->ammo_consume( really_used, pos );
                } else {
                    detached_ptr<item> split = item::spawn( *e );
                    split->ammo_set( e->ammo_current(), really_used );
                    e->ammo_consume( really_used, pos );
                    used.push_back( std::move( split ) );
                }
            }
        } else if( e->count_by_charges() ) {
            if( e->typeId() == what ) {
                if( e->charges > qty ) {
                    e->charges -= qty;
                    detached_ptr<item> split = item::spawn( *e );
                    split->charges = qty;
                    used.push_back( std::move( split ) );
                    qty = 0;
                } else {
                    qty -= e->charges;
                    used.push_back( std::move( e ) );
                    return detached_ptr<item>();
                }
            }
        }
        return std::move( e );
    };

    item &obj = *self;

    if( qty > 0 && filter( *self ) && self->typeId() == what ) {
        self = handle_item( std::move( self ) );
    }

    obj.remove_items_with( [&qty, &filter, &handle_item]( detached_ptr<item> &&e ) {
        if( qty == 0 ) {
            // found sufficient charges
            return VisitResponse::ABORT;
        }
        if( !filter( *e ) ) {
            return VisitResponse::NEXT;
        }
        item &obj = *e;
        e = handle_item( std::move( e ) );
        if( obj.is_tool() || obj.count_by_charges() ) {
            return VisitResponse::SKIP;
        }
        return VisitResponse::NEXT;
    } );
    return std::move( self );
}

void item::set_snippet( const snippet_id &id )
{
    if( is_null() ) {
        return;
    }
    if( !id.is_valid() ) {
        debugmsg( "there's no snippet with id %s", id.str() );
        return;
    }
    snip_id = id;
}

const std::string &item::get_category_id() const
{
    if( is_container() && !contents.empty() ) {
        return contents.front().get_category().get_id().str();
    }

    static item_category null_category;
    return type->category_force.is_valid() ? type->category_force.obj().get_id().str() :
           null_category.get_id().str();
}

const item_category &item::get_category() const
{
    if( is_container() && !contents.empty() ) {
        return contents.front().get_category();
    }

    static item_category null_category;
    return type->category_force.is_valid() ? type->category_force.obj() : null_category;
}

iteminfo::iteminfo( const std::string &Type, const std::string &Name, const std::string &Fmt,
                    flags Flags, double Value )
{
    sType = Type;
    sName = replace_colors( Name );
    sFmt = replace_colors( Fmt );
    is_int = !( Flags & is_decimal || Flags & is_three_decimal );
    three_decimal = ( Flags & is_three_decimal );
    dValue = Value;
    bShowPlus = static_cast<bool>( Flags & show_plus );
    std::stringstream convert;
    if( bShowPlus ) {
        convert << std::showpos;
    }
    if( is_int ) {
        convert << std::setprecision( 0 );
    } else if( three_decimal ) {
        convert << std::setprecision( 3 );
    } else {
        convert << std::setprecision( 2 );
    }
    convert << std::fixed << Value;
    sValue = convert.str();
    bNewLine = !( Flags & no_newline );
    bLowerIsBetter = static_cast<bool>( Flags & lower_is_better );
    bDrawName = !( Flags & no_name );
}

iteminfo::iteminfo( const std::string &Type, const std::string &Name, double Value )
    : iteminfo( Type, Name, "", no_flags, Value )
{
}

bool item::will_explode_in_fire() const
{
    if( type->explode_in_fire ) {
        return true;
    }

    if( type->ammo && ( type->ammo->special_cookoff || type->ammo->cookoff ) ) {
        return true;
    }

    // Most containers do nothing to protect the contents from fire
    if( !is_magazine() || !type->magazine->protects_contents ) {
        return has_item_with( [&]( const item & it ) {
            return this != &it && it.will_explode_in_fire();
        } );
    }

    return false;
}

detached_ptr<item> item::detonate( detached_ptr<item> &&self, const tripoint &p,
                                   std::vector<detached_ptr<item>> &drops )
{
    if( self->type->explosion ) {
        explosion_handler::explosion( p, self->type->explosion, self->activated_by );
        return detached_ptr<item>();
    } else if( self->type->ammo && ( self->type->ammo->special_cookoff ||
                                     self->type->ammo->cookoff ) ) {
        int charges_remaining = self->charges;
        const int rounds_exploded = rng( 1, charges_remaining );
        // Yank the exploding item off the map for the duration of the explosion
        // so it doesn't blow itself up.
        const islot_ammo &ammo_type = *self->type->ammo;

        if( ammo_type.special_cookoff ) {
            // If it has a special effect just trigger it.
            apply_ammo_effects( p, ammo_type.ammo_effects, self->activated_by );
        }
        charges_remaining -= rounds_exploded;
        if( charges_remaining > 0 ) {
            detached_ptr<item> temp_item = item::spawn( *self );
            temp_item->charges = charges_remaining;
            drops.push_back( std::move( temp_item ) );
        }

        return detached_ptr<item>();
    } else if( !self->contents.empty() && ( !self->type->magazine ||
                                            !self->type->magazine->protects_contents ) ) {
        bool detonated = false;
        self->contents.remove_top_items_with( [&p, &drops, &detonated]( detached_ptr<item> &&it ) {
            it = detonate( std::move( it ), p, drops );
            if( !it ) {
                detonated = true;
            }
            return std::move( it );
        } );
        if( detonated ) {
            return detached_ptr<item>();
        } else {
            return std::move( self );
        }
    }

    return std::move( self );
}
bool item::has_rotten_away() const
{
    if( is_corpse() && !can_revive() ) {
        return get_rot() > 10_days;
    } else {
        return is_food() && get_relative_rot() > 2.0;
    }
}

detached_ptr<item> item::actualize_rot( detached_ptr<item> &&self, const tripoint &pnt,
                                        temperature_flag temperature,
                                        const weather_manager &weather )
{
    if( self->goes_bad() ) {
        return process_rot( std::move( self ), false, pnt, nullptr, temperature, weather );
    } else if( self->type->container && self->type->container->preserves ) {
        // Containers like tin cans preserves all items inside, they do not rot at all.
        return std::move( self );
    } else if( self->type->container && self->type->container->seals ) {
        // Items inside rot but do not vanish as the container seals them in.
        self->contents.remove_top_items_with( [&pnt, &temperature, &weather]( detached_ptr<item> &&it ) {
            if( it->goes_bad() ) {
                it = process_rot( std::move( it ), true, pnt, nullptr, temperature, weather );
            }
            return std::move( it );
        } );
        return std::move( self );
    } else {
        // Check and remove rotten contents, but always keep the container.
        self->contents.remove_top_items_with( [&pnt, &temperature, &weather]( detached_ptr<item> &&it ) {
            return actualize_rot( std::move( it ), pnt, temperature, weather );
        } );
        return std::move( self );
    }
}

bool item_ptr_compare_by_charges( const item *left, const item *right )
{
    if( left->contents.empty() ) {
        return false;
    } else if( right->contents.empty() ) {
        return true;
    } else {
        return right->contents.front().charges < left->contents.front().charges;
    }
}

bool item_compare_by_charges( const item &left, const item &right )
{
    return item_ptr_compare_by_charges( &left, &right );
}

static const std::string USED_BY_IDS( "USED_BY_IDS" );
bool item::already_used_by_player( const player &p ) const
{
    const auto it = item_vars.find( USED_BY_IDS );
    if( it == item_vars.end() ) {
        return false;
    }
    // USED_BY_IDS always starts *and* ends with a ';', the search string
    // ';<id>;' matches at most one part of USED_BY_IDS, and only when exactly that
    // id has been added.
    const std::string needle = string_format( ";%d;", p.getID().get_value() );
    return it->second.find( needle ) != std::string::npos;
}

void item::mark_as_used_by_player( const player &p )
{
    std::string &used_by_ids = item_vars[ USED_BY_IDS ];
    if( used_by_ids.empty() ) {
        // *always* start with a ';'
        used_by_ids = ";";
    }
    // and always end with a ';'
    used_by_ids += string_format( "%d;", p.getID().get_value() );
}

bool item::can_holster( const item &obj, bool ignore ) const
{
    if( !type->can_use( "holster" ) ) {
        return false; // item is not a holster
    }

    const holster_actor *ptr = dynamic_cast<const holster_actor *>
                               ( type->get_use( "holster" )->get_actor_ptr() );
    if( !ptr->can_holster( obj ) ) {
        return false; // item is not a suitable holster for obj
    }

    if( !ignore && static_cast<int>( contents.num_item_stacks() ) >= ptr->multi ) {
        return false; // item is already full
    }

    return true;
}

std::string item::components_to_string() const
{
    using t_count_map = std::map<std::string, int>;
    t_count_map counts;
    for( const item * const &elem : components ) {
        if( !elem->has_flag( flag_BYPRODUCT ) ) {
            const std::string name = elem->display_name();
            counts[name]++;
        }
    }
    return enumerate_as_string( counts.begin(), counts.end(),
    []( const std::pair<std::string, int> &entry ) -> std::string {
        if( entry.second != 1 )
        {
            return string_format( pgettext( "components count", "%d x %s" ), entry.second, entry.first );
        } else
        {
            return entry.first;
        }
    }, enumeration_conjunction::none );
}

uint64_t item::make_component_hash() const
{
    // First we need to sort the IDs so that identical ingredients give identical hashes.
    std::multiset<std::string> id_set;
    for( const item * const &it : components ) {
        id_set.insert( it->typeId().str() );
    }

    std::string concatenated_ids;
    for( const std::string &id : id_set ) {
        concatenated_ids += id;
    }

    std::hash<std::string> hasher;
    return hasher( concatenated_ids );
}

bool item::needs_processing() const
{
    return is_active() || has_flag( flag_RADIO_ACTIVATION ) || has_flag( flag_ETHEREAL_ITEM ) ||
           ( !contents.empty() && is_container() && contents.front().needs_processing() ) ||
           ( magazine_current() && magazine_current()->needs_processing() ) ||
           is_artifact() || is_relic() || is_food();
}

int item::processing_speed() const
{
    if( is_corpse() || is_food() || is_food_container() ) {
        return to_turns<int>( 10_minutes );
    }
    // Unless otherwise indicated, update every turn.
    return 1;
}

detached_ptr<item> item::process_rot( detached_ptr<item> &&self, const tripoint &pos )
{
    return process_rot( std::move( self ), false, pos, nullptr, temperature_flag::TEMP_NORMAL,
                        get_weather() );
}

static units::temperature clip_by_temperature_flag( units::temperature temperature,
        temperature_flag flag )
{
    switch( flag ) {
        case temperature_flag::TEMP_NORMAL:
            // Just use the temperature normally
            return temperature;
        case temperature_flag::TEMP_FRIDGE:
            return std::min( temperature, temperatures::fridge );
        case temperature_flag::TEMP_FREEZER:
            return std::min( temperature, temperatures::freezer );
        case temperature_flag::TEMP_HEATER:
            return std::max( temperature, temperatures::normal );
        case temperature_flag::TEMP_ROOT_CELLAR:
            return temperatures::root_cellar;
        default:
            debugmsg( "Temperature flag enum not valid: %d.  Using current temperature.",
                      static_cast<int>( flag ) );
            break;
    }
    return temperature;
}

detached_ptr<item>  item::process_rot( detached_ptr<item> &&self, const bool seals,
                                       const tripoint &pos,
                                       player *carrier, const temperature_flag flag,
                                       const weather_manager &weather )
{
    if( !self ) {
        return std::move( self );
    }
    const time_point now = calendar::turn;

    // if player debug menu'd the time backward it breaks stuff, just reset the
    // last_temp_check and last_rot_check in this case
    if( now - self->last_rot_check < 0_turns ) {
        self->last_rot_check = now;
        return std::move( self );
    }

    // process rot at most once every 100_turns (10 min)
    // note we're also gated by item::processing_speed
    constexpr time_duration smallest_interval = 10_minutes;

    units::temperature temp = weather.get_temperature( pos );
    temp = clip_by_temperature_flag( temp, flag );

    time_point time = self->last_rot_check;
    item_internal::scoped_goes_bad_cache _cache( &*self );

    if( now - time > 1_hours ) {
        // This code is for items that were left out of reality bubble for long time

        const weather_generator &wgen = weather.get_cur_weather_gen();
        const unsigned int seed = g->get_seed();
        // It's a modifier, so we need to subtract 0_f
        units::temperature local_mod = units::from_fahrenheit( g->new_game
                                       ? 0
                                       : get_map().get_temperature( pos ) ) - 0_f;

        // Process the past of this item since the last time it was processed
        while( now - time > 1_hours ) {
            // Get the environment temperature
            time_duration time_delta = std::min( 1_hours, now - 1_hours - time );
            time += time_delta;

            //Use weather if above ground, use map temp if below
            units::temperature env_temperature_raw;
            if( pos.z >= 0 ) {
                tripoint_abs_ms location = tripoint_abs_ms( get_map().getabs( pos ) );
                units::temperature weather_temperature = wgen.get_weather_temperature( location, time,
                        calendar::config, seed );
                env_temperature_raw = weather_temperature + local_mod;
            } else {
                env_temperature_raw = temperatures::annual_average + local_mod;
            }

            units::temperature env_temperature_clipped = clip_by_temperature_flag( env_temperature_raw, flag );

            // Calculate item rot
            self->rot += self->calc_rot( time, env_temperature_clipped );
            self->last_rot_check = time;

            if( self->has_rotten_away() && carrier == nullptr && !seals ) {
                // No need to track item that will be gone
                return detached_ptr<item>();
            }
        }
    }

    // Remaining <1 h from above
    // and items that are held near the player
    if( now - time > smallest_interval ) {
        self->rot += self->calc_rot( now, temp );
        self->last_rot_check = now;

        if( self->has_rotten_away() && carrier == nullptr && !seals ) {
            return detached_ptr<item>();
        } else {
            return std::move( self );
        }
    }
    return std::move( self );
}

void item::process_artifact( player *carrier, const tripoint & /*pos*/ )
{
    if( !is_artifact() ) {
        return;
    }
    // Artifacts are currently only useful for the player character, the messages
    // don't consider npcs. Also they are not processed when laying on the ground.
    // TODO: change game::process_artifact to work with npcs,
    // TODO: consider moving game::process_artifact here.
    if( carrier == &get_avatar() ) {
        g->process_artifact( *this, *carrier );
    }
}

std::vector<trait_id> item::mutations_from_wearing( const Character &guy ) const
{
    if( !is_relic() ) {
        return std::vector<trait_id> {};
    }
    std::vector<trait_id> muts;

    for( const enchantment &ench : relic_data->get_enchantments() ) {
        for( const trait_id &mut : ench.get_mutations() ) {
            // this may not be perfectly accurate due to conditions
            muts.push_back( mut );
        }
    }

    for( const trait_id &char_mut : guy.get_mutations() ) {
        for( auto iter = muts.begin(); iter != muts.end(); ) {
            if( char_mut == *iter ) {
                iter = muts.erase( iter );
            } else {
                ++iter;
            }
        }
    }

    return muts;
}

void item::process_relic( Character *carrier )
{
    if( !is_relic() ) {
        return;
    }
    std::vector<enchantment> active_enchantments;

    if( carrier ) {
        for( const enchantment &ench : get_enchantments() ) {
            if( ench.is_active( *carrier, *this ) ) {
                active_enchantments.emplace_back( ench );
            }
        }
    }

    relic_funcs::process_recharge( *this, carrier );
}

detached_ptr<item> item::process_corpse( detached_ptr<item> &&self, player *carrier,
        const tripoint &pos )
{
    if( !self ) {
        return std::move( self );
    }
    // some corpses rez over time
    if( self->corpse == nullptr || self->damage() >= self->max_damage() ) {
        return std::move( self );
    }
    if( self->corpse->zombify_into && self->rotten() && !self->has_flag( flag_PULPED ) ) {
        self->rot -= self->get_shelf_life();
        self->corpse = &*self->corpse->zombify_into;
        return std::move( self );
    }
    if( !self->ready_to_revive( pos ) ) {
        return std::move( self );
    }
    if( rng( 0, self->volume() / units::legacy_volume_factor ) > self->burnt &&
        g->revive_corpse( pos, *self ) ) {
        if( carrier == nullptr ) {
            if( get_avatar().sees( pos ) ) {
                if( self->corpse->in_species( ROBOT ) ) {
                    add_msg( m_warning, _( "A nearby robot has repaired itself and stands up!" ) );
                } else {
                    add_msg( m_warning, _( "A nearby corpse rises and moves towards you!" ) );
                }
            }
        } else {
            if( self->corpse->in_species( ROBOT ) ) {
                carrier->add_msg_if_player( m_warning,
                                            _( "Oh dear god, a robot you're carrying has started moving!" ) );
            } else {
                carrier->add_msg_if_player( m_warning,
                                            _( "Oh dear god, a corpse you're carrying has started moving!" ) );
            }
        }
        // Destroy this corpse item
        return detached_ptr<item>();
    }

    return std::move( self );
}

detached_ptr<item> item::process_fake_mill( detached_ptr<item> &&self, player * /*carrier*/,
        const tripoint &pos )
{
    if( !self ) {
        return std::move( self );
    }
    map &here = get_map();
    if( here.furn( pos ) != furn_str_id( "f_wind_mill_active" ) &&
        here.furn( pos ) != furn_str_id( "f_water_mill_active" ) ) {
        self->set_counter( 0 );
        return detached_ptr<item>(); //destroy fake mill
    }
    if( self->age() >= 6_hours || self->item_counter == 0 ) {
        iexamine::mill_finalize( get_avatar(), pos,
                                 self->birthday() ); //activate effects when timers goes to zero
        return detached_ptr<item>(); //destroy fake mill item
    }

    return std::move( self );
}

detached_ptr<item> item::process_fake_cloning_vat( detached_ptr<item> &&self, player * /*carrier*/,
        const tripoint &pos )
{
    if( !self ) {
        return std::move( self );
    }
    map &here = get_map();
    if( here.furn( pos ) != furn_str_id( "f_cloning_vat_active" ) ) {
        self->item_counter = 0;
        return detached_ptr<item>(); //destroy fake smoke
    }

    if( self->item_counter == 0 ) {
        iexamine::cloning_vat_finalize( pos, self->birthday() ); //activate effects when timers goes to zero
        return detached_ptr<item>(); //destroy fake smoke when it 'burns out'
    }

    return std::move( self );
}

detached_ptr<item> item::process_fake_smoke( detached_ptr<item> &&self, player * /*carrier*/,
        const tripoint &pos )
{
    if( !self ) {
        return std::move( self );
    }
    map &here = get_map();
    if( here.furn( pos ) != furn_str_id( "f_smoking_rack_active" ) &&
        here.furn( pos ) != furn_str_id( "f_metal_smoking_rack_active" ) ) {
        self->set_counter( 0 );
        return detached_ptr<item>(); //destroy fake smoke
    }

    if( self->age() >= 6_hours || self->item_counter == 0 ) {
        iexamine::on_smoke_out( pos, self->birthday() ); //activate effects when timers goes to zero
        return detached_ptr<item>(); //destroy fake smoke when it 'burns out'
    }

    return std::move( self );
}

detached_ptr<item> item::process_litcig( detached_ptr<item> &&self, player *carrier,
        const tripoint &pos )
{
    if( !self ) {
        return std::move( self );
    }
    if( !one_in( 10 ) ) {
        return std::move( self );
    }
    self = self->process_extinguish( std::move( self ), carrier, pos );
    // process_extinguish might have extinguished the item already
    if( !self->is_active() ) {
        return std::move( self );
    }
    item &it = *self;
    map &here = get_map();
    // if carried by someone:
    if( carrier != nullptr ) {
        time_duration duration = 15_seconds;
        if( carrier->has_trait( trait_TOLERANCE ) ) {
            duration = 7_seconds;
        } else if( carrier->has_trait( trait_LIGHTWEIGHT ) ) {
            duration = 30_seconds;
        }
        carrier->add_msg_if_player( m_neutral, _( "You take a puff of your %s." ), it.tname() );

        // we need to figure out a way to get the item before this got converted,
        // but i don't think that's going to be very easy...
        if( it.has_flag( flag_TOBACCO ) ) {
            carrier->add_effect( effect_cig, duration );
        }
        if( it.has_flag( flag_MARIJUANA ) ) {
            carrier->add_effect( effect_weed_high, duration );
        }

        carrier->moves -= 15;

        if( ( carrier->has_effect( effect_shakes ) && one_in( 10 ) ) ) {
            carrier->add_msg_if_player( m_bad, _( "Your shaking hand causes you to drop your %s." ),
                                        it.tname() );
            here.add_item_or_charges( pos + point( rng( -1, 1 ), rng( -1, 1 ) ), std::move( self ) );
            return detached_ptr<item>(); // removes the item that has just been added to the map
        }

        if( carrier->has_effect( effect_sleep ) ) {
            carrier->add_msg_if_player( m_bad, _( "You fall asleep and drop your %s." ),
                                        it.tname() );
            here.add_item_or_charges( pos + point( rng( -1, 1 ), rng( -1, 1 ) ), std::move( self ) );
            self = detached_ptr<item>();
        }
    } else {
        // If not carried by someone, but laying on the ground:
        if( it.item_counter % 5 == 0 ) {
            // lit cigarette can start fires
            if( here.flammable_items_at( pos ) ||
                here.has_flag( flag_FLAMMABLE, pos ) ||
                here.has_flag( flag_FLAMMABLE_ASH, pos ) ) {
                here.add_field( pos, fd_fire, 1 );
            }
        }
    }

    // cig dies out
    if( it.item_counter == 0 ) {
        if( carrier != nullptr ) {
            carrier->add_msg_if_player( m_neutral, _( "You finish your %s." ), it.tname() );
        }
        it.convert( dynamic_cast<const iuse_transform *>
                    ( it.type->get_use( "transform" )->get_actor_ptr() )->target );
        if( it.has_flag( flag_MARIJUANA ) ) {
            if( carrier != nullptr ) {
                carrier->add_effect( effect_weed_high, 1_minutes ); // one last puff
                here.add_field( pos + point( rng( -1, 1 ), rng( -1, 1 ) ), fd_weedsmoke, 2 );
                weed_msg( *carrier );
            }
        }
        it.deactivate();
    }
    // Item remains
    return std::move( self );
}

detached_ptr<item> item::process_extinguish( detached_ptr<item> &&self, player *carrier,
        const tripoint &pos )
{
    if( !self ) {
        return std::move( self );
    }
    // checks for water
    bool extinguish = false;
    bool in_inv = carrier != nullptr && carrier->has_item( *self );
    bool submerged = false;
    bool precipitation = false;
    bool windtoostrong = false;
    bool in_veh = carrier != nullptr && carrier->in_vehicle;
    int windpower = get_weather().windspeed;
    switch( get_weather().weather_id->precip ) {
        case precip_class::very_light:
            precipitation = one_in( 100 );
            break;
        case precip_class::light:
            precipitation = one_in( 50 );
            break;
        case precip_class::medium:
            precipitation = one_in( 25 );
            break;
        case precip_class::heavy:
            precipitation = one_in( 10 );
            break;
        default:
            break;
    }
    map &here = get_map();
    if( in_inv && !in_veh && here.has_flag( flag_DEEP_WATER, pos ) ) {
        extinguish = true;
        submerged = true;
    }
    if( ( !in_inv && here.has_flag( flag_LIQUID, pos ) && !here.veh_at( pos ) ) ||
        ( precipitation && !g->is_sheltered( pos ) ) ) {
        extinguish = true;
    }
    if( in_inv && windpower > 5 && !g->is_sheltered( pos ) &&
        self->has_flag( flag_WIND_EXTINGUISH ) ) {
        windtoostrong = true;
        extinguish = true;
    }
    if( !extinguish ||
        ( in_inv && precipitation && carrier->primary_weapon().has_flag( flag_RAIN_PROTECT ) ) ) {
        return std::move( self ); //nothing happens
    }
    if( carrier != nullptr ) {
        if( submerged ) {
            carrier->add_msg_if_player( m_neutral, _( "Your %s is quenched by water." ), self->tname() );
        } else if( precipitation ) {
            carrier->add_msg_if_player( m_neutral, _( "Your %s is quenched by precipitation." ),
                                        self->tname() );
        } else if( windtoostrong ) {
            carrier->add_msg_if_player( m_neutral, _( "Your %s is blown out by the wind." ),
                                        self->tname() );
        }
    }

    // cig dies out
    if( self->has_flag( flag_LITCIG ) ) {
        self->convert( dynamic_cast<const iuse_transform *>
                       ( self->type->get_use( "transform" )->get_actor_ptr() )->target );
    } else { // transform (lit) items
        if( !self->revert( carrier ) ) {
            self->type->invoke( carrier != nullptr ? *carrier : get_avatar(), *self, pos, "transform" );
        }

    }
    self->deactivate();
    // Item remains
    return std::move( self );
}

detached_ptr<item> item::process_cable( detached_ptr<item> &&self, player *carrier,
                                        const tripoint &pos )
{
    if( !self ) {
        return std::move( self );
    }
    //No need to process cable if it' has no map connections's only connected to character
    auto data = cable_connection_data::make_data( self.ptr );
    if( !data || data->empty() ) {
        self->reset_cable( carrier );
        return std::move( self );
    }
    if( data->character_only() || data->intermap_connection() ) {
        return std::move( self );
    }
    if( !data->get_nonchar_connection() ) {
        self->reset_cable( carrier );
        return std::move( self );
    }

    int distance = 0;
    map &here = get_map();
    //At this point we are sure that non_char is not empty
    auto nonchar = *data->get_nonchar_connection();

    //Caharacter connected to smth
    if( data->complete() ) {
        if( !carrier ) {
            if( data->get_map_connection() ) {
                data->unset_other_con( self.get(), nonchar );
            } else {
                self->reset_cable( carrier );
            }
            return std::move( self );
        }
        switch( nonchar.state ) {
            case state_solar_pack:
                if( !carrier->has_item( *self ) || !carrier->worn_with_flag( flag_SOLARPACK_ON ) ) {
                    carrier->add_msg_if_player( m_bad, _( "You notice the cable has come loose!" ) );
                    self->reset_cable( carrier );
                }
                return std::move( self );
            case state_UPS: {
                static const item_filter used_ups = [&]( const item & itm ) {
                    return itm.get_var( "cable" ) == "plugged_in";
                };

                if( !carrier->has_item( *self ) || !carrier->has_item_with( used_ups ) ) {
                    for( item *used : carrier->items_with( used_ups ) ) {
                        used->erase_var( "cable" );
                    }
                    carrier->add_msg_if_player( m_bad, _( "You notice the cable has come loose!" ) );
                    self->reset_cable( carrier );
                }
                return std::move( self );
            }
            case state_vehicle: {
                if( !here.veh_at( nonchar.point ) ) {
                    carrier->add_msg_if_player( m_bad, _( "You notice the cable has disconnected from a vehicle!" ) );
                    data->unset_con( self.get(), nonchar );
                    return std::move( self );
                }
                //Sitting in vehicle shenenigans
                //If character is sitting at vehicle - no need(and no way) to figure out cable distance and if it's the same vehicle we previously connected
                //So we just ignore distance
                const auto vp_pos = here.veh_at( pos );
                if( vp_pos ) {
                    const auto seat = vp_pos.part_with_feature( "BOARDABLE", true );
                    if( seat && carrier == seat->vehicle().get_passenger( seat->part_index() ) ) {
                        return std::move( self );
                    }
                }
                break;
            }
            case state_grid: {
                auto s = nonchar.point;
                auto *grid_connector = active_tiles::furn_at<vehicle_connector_tile>( s );
                if( !grid_connector ) {
                    if( carrier->has_item( *self ) ) {
                        carrier->add_msg_if_player( m_bad, _( "You notice the cable has come loose!" ) );
                    }
                    self->reset_cable( carrier );
                    return std::move( self );
                }
                break;
            }
            case state_none:
            case state_self:
            default:
                debugmsg( "Unexpected cable state %s", nonchar.state );
                self->reset_cable( carrier );
                return std::move( self );
        }
    }
    if( nonchar.map_point() ) {
        distance = rl_dist( pos, here.getlocal( nonchar.point ) );
        self->charges = self->type->maximum_charges() - distance;
        if( self->charges < 1 ) {
            if( carrier ) {
                carrier->add_msg_if_player( m_bad, _( "The over-extended cable breaks loose!" ) );
            }
            self->reset_cable( carrier );
        }
    }

    return std::move( self );
}

void item::reset_cable( Character *who )
{
    int max_charges = type->maximum_charges();
    //erase legacy info
    erase_var( "state" );
    erase_var( "source_x" );
    erase_var( "source_y" );
    erase_var( "source_z" );
    //erase info
    cable_connection_data::unset_vars( this );
    deactivate();
    charges = max_charges;

    if( who != nullptr ) {
        who->add_msg_if_player( m_info, _( "You reel in the cable." ) );
        who->mod_moves( -10 * charges );
    }
}

detached_ptr<item> item::process_UPS( detached_ptr<item> &&self, player *carrier,
                                      const tripoint & /*pos*/ )
{
    if( !self ) {
        return std::move( self );
    }
    if( carrier == nullptr ) {
        self->erase_var( "cable" );
        self->deactivate();
        return std::move( self );
    }
    bool has_connected_cable = carrier->has_item_with( []( const item & it ) {
        return it.is_active() && cable_connection_data::ups_connected( &it );
    } );
    if( !has_connected_cable ) {
        self->erase_var( "cable" );
        self->deactivate();
    }
    return std::move( self );
}

bool item::process_wet( player * /*carrier*/, const tripoint & /*pos*/ )
{
    if( item_counter == 0 ) {
        if( is_tool() && type->tool->revert_to ) {
            convert( *type->tool->revert_to );
        }
        unset_flag( flag_WET );
        deactivate();
    }
    // Always return true so our caller will bail out instead of processing us as a tool.
    return true;
}

detached_ptr<item> item::process_tool( detached_ptr<item> &&self, player *carrier,
                                       const tripoint &pos )
{
    if( !self ) {
        return std::move( self );
    }
    avatar &you = get_avatar();
    // items with iuse set_transformed which are restricted turn off if not attached to their dependency.
    if( self->type->can_use( "set_transformed" ) ) {
        const set_transformed_iuse *actor = dynamic_cast<const set_transformed_iuse *>
                                            ( self->get_use( "set_transformed" )->get_actor_ptr() );
        if( actor == nullptr ) {
            debugmsg( "iuse_actor type descriptor and actual type mismatch" );
            return std::move( self );
        }
        if( actor->restricted ) {
            if( !carrier ) {
                actor->bypass( carrier != nullptr ? *carrier : you, *self, false, pos );
                return std::move( self );
            } else {
                bool active = false;
                flag_id transform_flag( actor->dependencies );
                for( const auto &elem : carrier->worn ) {
                    if( elem->is_active() && elem->has_flag( transform_flag ) ) {
                        active = true;
                        break;
                    }
                }
                if( !active ) {
                    actor->bypass( carrier != nullptr ? *carrier : you, *self, false, pos );
                    return std::move( self );
                }
            }
        }
    }

    int energy = 0;
    const bool uses_UPS = self->has_flag( flag_USE_UPS );
    bool revert_destroy = false;
    if( self->type->tool->turns_per_charge > 0 ) {
        if( self->type->tool->turns_active >= self->type->tool->turns_per_charge ) {
            energy = std::max( self->ammo_required(), 1 );
            self->type->tool->turns_active = 0;
        }
        self->type->tool->turns_active += 1;
    } else if( self->type->tool->power_draw > 0 ) {
        // power_draw in mW / 1000000 to give kJ (battery unit) per second
        energy = self->type->tool->power_draw / 1000000;
        // energy_bat remainder results in chance at additional charge/discharge
        energy += x_in_y( self->type->tool->power_draw % 1000000, 1000000 ) ? 1 : 0;
    }

    // If ammo_required is 0 we just skip over this and go to tick processing.
    if( energy || self->ammo_required() > 0 ) {
        // No need to look for charges if energy is 0
        if( energy ) {
            energy -= self->ammo_consume( energy, pos );

            // for power armor pieces, try to use power armor interface first.
            if( carrier && self->is_power_armor() && character_funcs::can_interface_armor( *carrier ) ) {
                if( carrier->use_charges_if_avail( itype_bio_armor, energy ) ) {
                    energy = 0;
                }
            }

            // for items in player possession if insufficient charges within tool try UPS
            if( carrier && uses_UPS ) {
                if( carrier->use_charges_if_avail( itype_UPS, energy ) ) {
                    energy = 0;
                }
            }
        }

        // HACK: this means that UPS items will last one more check longer than they should since they don't trigger when
        // their ammo_remaining is 0, since that doesn't check the UPS "stock" available (which is an expensive check)
        // It's done like this cause grenades must be destroyed when charge reaches 0, or it will linger an extra turn.
        if( ( self->ammo_remaining() == 0 && !uses_UPS ) || energy > 0 ) {
            revert_destroy = true;
            if( carrier ) {
                if( self->is_power_armor() ) {
                    if( uses_UPS ) {
                        carrier->add_msg_if_player( m_info, _( "You need a UPS or Bionic Power Interface to run the %s!" ),
                                                    self->tname() );
                    } else {

                    }
                } else if( uses_UPS ) {
                    carrier->add_msg_if_player( m_info, _( "You need a UPS to run the %s!" ), self->tname() );
                }
            }
            if( carrier && self->type->can_use( "set_transform" ) ) {
                const set_transform_iuse *actor = dynamic_cast<const set_transform_iuse *>
                                                  ( self->get_use( "set_transform" )->get_actor_ptr() );
                if( actor == nullptr ) {
                    debugmsg( "iuse_actor type descriptor and actual type mismatch." );
                    return std::move( self );
                }
                flag_id transformed_flag( actor->flag );
                for( auto &elem : carrier->worn ) {
                    if( elem->is_active() && elem->has_flag( transformed_flag ) ) {
                        if( !elem->type->can_use( "set_transformed" ) ) {
                            debugmsg( "Expected set_transformed function" );
                            return std::move( self );
                        }
                        const set_transformed_iuse *actor = dynamic_cast<const set_transformed_iuse *>
                                                            ( elem->get_use( "set_transformed" )->get_actor_ptr() );
                        if( actor == nullptr ) {
                            debugmsg( "iuse_actor type descriptor and actual type mismatch" );
                            return std::move( self );
                        }
                        actor->bypass( *carrier, *elem, false, pos );
                    }
                }
            }
        }
    }

    // Process tick even if it's to be destroyed/reverted later, more for grenades
    // It technically gives an extra turn of action, but before the rework items functioned at 0 charges for a bit anyway.
    // Calls all use functions if active
    if( ( self->get_use( "REMOTEVEH" ) || self->get_use( "RADIOCONTROL" ) ) && self->is_active() ) {
        const use_function *method = nullptr;
        if( g->remoteveh() != nullptr && self->get_use( "REMOTEVEH" ) ) {
            method = &self->type->use_methods.find( "REMOTEVEH" )->second;
        } else if( !g->u.get_value( "remote_controlling" ).empty() && self->get_use( "RADIOCONTROL" ) ) {
            method = &self->type->use_methods.find( "RADIOCONTROL" )->second;
        }
        if( method != nullptr ) {
            method->call( carrier != nullptr ? *carrier : you, *self, true, pos );
        } else {
            self->type->tick( carrier != nullptr ? *carrier : you, *self, pos );
        }
    } else {
        self->type->tick( carrier != nullptr ? *carrier : you, *self, pos );
    }

    if( revert_destroy ) {
        // If no revert is defined, destroy it (candles and the like).
        if( self->is_active() && self->revert( carrier ) ) {
            self->deactivate();
            return std::move( self );
        } else {
            return detached_ptr<item>();
        }
    }

    return std::move( self );
}

detached_ptr<item> item::process_blackpowder_fouling( detached_ptr<item> &&self, player *carrier )
{
    if( !self ) {
        return std::move( self );
    }
    if( self->damage() < self->max_damage() && one_in( 2000 ) ) {
        self->inc_damage( DT_ACID );
        if( carrier ) {
            carrier->add_msg_if_player( m_bad, _( "Your %s rusts due to blackpowder fouling." ),
                                        self->tname() );
        }
    }
    return std::move( self );
}

detached_ptr<item> item::process( detached_ptr<item> &&self, player *carrier, const tripoint &pos,
                                  bool activate,
                                  temperature_flag flag )
{
    return process( std::move( self ), carrier, pos, activate, flag, get_weather() );
}

detached_ptr<item> item::process( detached_ptr<item> &&self, player *carrier, const tripoint &pos,
                                  bool activate,
                                  temperature_flag flag, const weather_manager &weather_generator )
{
    if( !self ) {
        return std::move( self );
    }
    const bool preserves = self->type->container && self->type->container->preserves;
    const bool seals = self->type->container && self->type->container->seals;
    item &obj = *self;

    obj.remove_items_with( [&]( detached_ptr<item> &&it ) {
        if( preserves ) {
            it->last_rot_check = calendar::turn;
        }
        it = it->process_internal( std::move( it ), carrier, pos, activate, seals, flag,
                                   weather_generator );
        return VisitResponse::NEXT;
    } );
    detached_ptr<item> res = process_internal( std::move( self ), carrier, pos, activate, seals, flag,
                             weather_generator );
    return res;
}

detached_ptr<item> item::process_internal( detached_ptr<item> &&self, player *carrier,
        const tripoint &pos, bool activate,
        const bool seals, const temperature_flag flag,
        const weather_manager &weather_generator )
{
    if( !self ) {
        return std::move( self );
    }
    if( self->has_flag( flag_ETHEREAL_ITEM ) ) {
        if( !self->has_var( "ethereal" ) ) {
            return detached_ptr<item>();
        }
        self->set_var( "ethereal", std::stoi( self->get_var( "ethereal" ) ) - 1 );
        const bool processed = std::stoi( self->get_var( "ethereal" ) ) <= 0;
        if( processed && carrier != nullptr ) {
            carrier->add_msg_if_player( _( "Your %s disappears!" ), self->tname() );
        }
        if( processed ) {
            return detached_ptr<item>();
        } else {
            return std::move( self );
        }
    }

    self->process_artifact( carrier, pos );
    self->process_relic( carrier );

    if( self->faults.contains( fault_gun_blackpowder ) ) {
        return process_blackpowder_fouling( std::move( self ), carrier );
    }

    avatar &you = get_avatar();
    if( activate ) {
        if( self->type->invoke( carrier != nullptr ? *carrier : you, *self, pos ) > 0 ) {
            return detached_ptr<item>();
        }
        return std::move( self );
    }
    // How this works: it checks what kind of processing has to be done
    // (e.g. for food, for drying towels, lit cigars), and if that matches,
    // call the processing function. If that function returns true, the item
    // has been destroyed by the processing, so no further processing has to be
    // done.
    // Otherwise processing continues. This allows items that are processed as
    // food and as litcig and as ...

    // Remaining stuff is only done for active items.
    if( !self->is_active() ) {
        return std::move( self );
    }

    if( !self->is_food() && self->item_counter > 0 ) {
        self->item_counter--;
    }

    if( self->item_counter == 0 && self->type->countdown_action ) {
        self->type->countdown_action.call( carrier ? *carrier : you, *self, false, pos );
        if( self->type->countdown_destroy ) {
            return detached_ptr<item>();
        }
    }

    map &here = get_map();
    for( const emit_id &e : self->type->emits ) {
        here.emit_field( pos, e );
    }

    if( self->has_flag( flag_FAKE_SMOKE ) ) {
        self = process_fake_smoke( std::move( self ), carrier, pos );
        if( !self ) {
            return std::move( self );
        }
    }
    if( self->has_flag( flag_FAKE_CLONING_VAT ) ) {
        self = process_fake_cloning_vat( std::move( self ), carrier, pos );
        if( !self ) {
            return std::move( self );
        }
    }
    if( self->has_flag( flag_FAKE_MILL ) ) {
        self = process_fake_mill( std::move( self ), carrier, pos );
        if( !self ) {
            return std::move( self );
        }
    }
    if( self->is_corpse() ) {
        self = process_corpse( std::move( self ), carrier, pos );
        if( !self ) {
            return std::move( self );
        }
    }
    if( self->has_flag( flag_WET ) && self->process_wet( carrier, pos ) ) {
        // Drying items are never destroyed, but we want to exit so they don't get processed as tools.
        return std::move( self );
    }
    if( self->has_flag( flag_LITCIG ) ) {
        self = process_litcig( std::move( self ), carrier, pos );
        if( !self ) {
            return std::move( self );
        }
    }
    if( ( self->has_flag( flag_WATER_EXTINGUISH ) || self->has_flag( flag_WIND_EXTINGUISH ) ) ) {
        self = process_extinguish( std::move( self ), carrier, pos );
        if( !self ) {
            return std::move( self );
        }
    }
    if( self->has_flag( flag_WATER_DISABLE ) && carrier->is_underwater() ) {
        carrier->add_msg_if_player( "Your %s gurgles and splutters.", self->tname() );
        self->revert( carrier );
        self->deactivate();
        return std::move( self );
    }
    if( self->has_flag( flag_CABLE_SPOOL ) ) {
        // DO NOT process this as a tool! It really isn't!
        return process_cable( std::move( self ), carrier, pos );
    }
    if( self->has_flag( flag_IS_UPS ) ) {
        // DO NOT process this as a tool! It really isn't!
        return process_UPS( std::move( self ), carrier, pos );
    }
    if( self->is_tool() ) {
        return process_tool( std::move( self ), carrier, pos );
    }
    // All foods that go bad have temperature
    if( ( self->is_food() || self->is_corpse() ) ) {
        item &obj = *self;
        self = process_rot( std::move( self ), seals, pos, carrier, flag, weather_generator );
        // If the item has rotted away, then self becomes a null pointer.
        if( !self ) {
            if( obj.is_comestible() ) {
                here.rotten_item_spawn( obj, pos );
            } else if( obj.is_corpse() ) {
                here.handle_decayed_corpse( obj, pos );
            }
        }
    }
    return std::move( self );
}

void item::mod_charges( int mod )
{
    if( has_infinite_charges() ) {
        return;
    }

    if( !count_by_charges() ) {
        debugmsg( "Tried to remove %s by charges, but item is not counted by charges.", tname() );
    } else if( mod < 0 && charges + mod < 0 ) {
        debugmsg( "Tried to remove charges that do not exist, removing maximum available charges instead." );
        charges = 0;
    } else if( mod > 0 && charges >= INFINITE_CHARGES - mod ) {
        charges = INFINITE_CHARGES - 1; // Highly unlikely, but finite charges should not become infinite.
    } else {
        charges += mod;
    }
}

bool item::has_effect_when_wielded( art_effect_passive effect ) const
{
    if( !type->artifact ) {
        return false;
    }
    const std::vector<art_effect_passive> &ew = type->artifact->effects_wielded;
    return std::ranges::find( ew, effect ) != ew.end();
}

bool item::has_effect_when_worn( art_effect_passive effect ) const
{
    if( !type->artifact ) {
        return false;
    }
    const std::vector<art_effect_passive> &ew = type->artifact->effects_worn;
    return std::ranges::find( ew, effect ) != ew.end();
}

bool item::has_effect_when_carried( art_effect_passive effect ) const
{
    if( !type->artifact ) {
        return false;
    }
    const std::vector<art_effect_passive> &ec = type->artifact->effects_carried;
    if( std::ranges::find( ec, effect ) != ec.end() ) {
        return true;
    }
    for( const item *i : contents.all_items_top() ) {
        if( i->has_effect_when_carried( effect ) ) {
            return true;
        }
    }
    return false;
}

bool item::is_seed() const
{
    return type->is_seed();
}

time_duration item::get_plant_epoch() const
{
    if( !type->seed ) {
        return 0_turns;
    }
    return type->seed->get_plant_epoch();
}

std::string item::get_plant_name() const
{
    if( !type->seed ) {
        return std::string{};
    }
    return type->seed->plant_name.translated();
}

bool item::is_dangerous() const
{
    if( has_flag( flag_DANGEROUS ) ) {
        return true;
    }

    // Note: Item should be dangerous regardless of what type of a container is it
    // Visitable interface would skip some options
    for( const item *it : contents.all_items_top() ) {
        if( it->is_dangerous() ) {
            return true;
        }
    }
    return false;
}

bool item::is_tainted() const
{
    return corpse && corpse->has_flag( MF_POISON );
}

bool item::is_soft() const
{
    const std::vector<material_id> mats = made_of();
    return std::ranges::any_of( mats, []( const material_id & mid ) {
        return mid.obj().soft();
    } );
}

bool item::is_reloadable() const
{
    if( has_flag( flag_NO_RELOAD ) && !has_flag( flag_VEHICLE ) ) {
        return false; // turrets ignore NO_RELOAD flag

    } else if( is_bandolier() || is_holster() ) {
        return true;

    } else if( is_container() ) {
        return true;

    } else if( !is_gun() && !is_tool() && !is_magazine() ) {
        return false;

    } else if( ammo_types().empty() ) {
        return false;
    }

    return true;
}

std::string item::type_name( unsigned int quantity ) const
{
    const auto iter = item_vars.find( "name" );
    std::string ret_name;
    if( iter != item_vars.end() ) {
        return iter->second;
    } else {
        ret_name = type->nname( quantity );
    }

    // Apply conditional names, in order.
    for( const conditional_name &cname : type->conditional_names ) {
        // Lambda for recursively searching for a item ID among all components.
        std::function<bool ( std::vector<item *> )> component_id_contains =
        [&]( const std::vector<item *> &components ) {
            for( const item *component : components ) {
                if( component->typeId().str().find( cname.condition ) != std::string::npos ||
                    component_id_contains( component->components.as_vector() ) ) {
                    return true;
                }
            }
            return false;
        };
        switch( cname.type ) {
            case condition_type::FLAG:
                if( has_flag( flag_id( cname.condition ) ) ) {
                    ret_name = string_format( cname.name.translated( quantity ), ret_name );
                }
                break;
            case condition_type::VITAMIN:
                if( has_vitamin( vitamin_id( cname.condition ) ) ) {
                    ret_name = string_format( cname.name.translated( quantity ), ret_name );
                }
                break;
            case condition_type::COMPONENT_ID:
                if( component_id_contains( components.as_vector() ) ) {
                    ret_name = string_format( cname.name.translated( quantity ), ret_name );
                }
                break;
            case condition_type::num_condition_types:
                break;
        }
    }

    // Identify who this corpse belonged to, if applicable.
    if( corpse != nullptr && has_flag( flag_CORPSE ) ) {
        if( corpse_name.empty() ) {
            //~ %1$s: name of corpse with modifiers;  %2$s: species name
            ret_name = string_format( pgettext( "corpse ownership qualifier", "%1$s of a %2$s" ),
                                      ret_name, corpse->nname() );
        } else {
            //~ %1$s: name of corpse with modifiers;  %2$s: proper name;  %3$s: species name
            ret_name = string_format( pgettext( "corpse ownership qualifier", "%1$s of %2$s, %3$s" ),
                                      ret_name, corpse_name, corpse->nname() );
        }
    }

    return ret_name;
}

const mtype *item::get_corpse_mon() const
{
    return corpse;
}

std::string item::get_corpse_name()
{
    if( corpse_name.empty() ) {
        return std::string();
    }
    return corpse_name;
}

std::string item::nname( const itype_id &id, unsigned int quantity )
{
    return id->nname( quantity );
}

bool item::count_by_charges( const itype_id &id )
{
    return id->count_by_charges();
}

int item::get_gun_ups_drain() const
{
    int draincount = 0;
    if( type->gun ) {
        int modifier = 0;
        float multiplier = 1.0f;
        for( const item *mod : gunmods() ) {
            modifier += mod->type->gunmod->ups_charges_modifier;
            multiplier *= mod->type->gunmod->ups_charges_multiplier;
        }
        draincount = ( type->gun->ups_charges * multiplier ) + modifier;
    }
    return draincount;
}

bool item::has_label() const
{
    return has_var( "item_label" );
}

std::string item::label( unsigned int quantity ) const
{
    if( has_label() ) {
        return get_var( "item_label" );
    }

    return type_name( quantity );
}

bool item::has_infinite_charges() const
{
    return charges == INFINITE_CHARGES;
}

skill_id item::contextualize_skill( const skill_id &id ) const
{
    if( id->is_contextual_skill() ) {
        if( id == skill_weapon ) {
            if( is_gun() ) {
                return gun_skill();
            } else if( is_melee() ) {
                return melee_skill();
            }
        }
    }

    return id;
}


bool item::on_drop( const tripoint &pos )
{
    return on_drop( pos, get_map() );
}

bool item::on_drop( const tripoint &pos, map &m )
{
    // dropping liquids, even currently frozen ones, on the ground makes them
    // dirty
    if( made_of( LIQUID ) && !m.has_flag( flag_LIQUIDCONT, pos ) &&
        !has_own_flag( flag_DIRTY ) ) {
        set_flag( flag_DIRTY );
    }
    avatar &you = get_avatar();
    you.flag_encumbrance();
    return type->drop_action && type->drop_action.call( you, *this, false, pos );
}

time_duration item::age() const
{
    return calendar::turn - birthday();
}

void item::set_age( const time_duration &age )
{
    set_birthday( time_point( calendar::turn ) - age );
}

void item::legacy_fast_forward_time()
{
    const time_duration tmp_bday = ( bday - calendar::turn_zero ) * 6;
    bday = calendar::turn_zero + tmp_bday;

    rot *= 6;

    const time_duration tmp_rot = ( last_rot_check - calendar::turn_zero ) * 6;
    last_rot_check = calendar::turn_zero + tmp_rot;
}

bool item::is_active() const
{
    return active;
}

time_point item::birthday() const
{
    return bday;
}

void item::set_birthday( const time_point &bday )
{
    this->bday = std::max( calendar::turn_zero, bday );
}

bool item::is_upgrade() const
{
    if( !type->bionic ) {
        return false;
    }
    return type->bionic->is_upgrade;
}

int item::get_min_str() const
{
    if( type->gun ) {
        int min_str = type->min_str;
        for( const item *mod : gunmods() ) {
            min_str += mod->type->gunmod->min_str_required_mod;
        }
        return min_str > 0 ? min_str : 0;
    } else {
        return type->min_str;
    }
}

std::vector<item_comp> item::get_uncraft_components() const
{
    std::vector<item_comp> ret;
    if( components.empty() ) {
        //If item wasn't crafted with specific components use default recipe
        std::vector<std::vector<item_comp>> recipe = recipe_dictionary::get_uncraft(
                                             typeId() ).disassembly_requirements().get_components();
        for( std::vector<item_comp> &component : recipe ) {
            ret.push_back( component.front() );
        }
    } else {
        //Make a new vector of components from the registered components
        for( const item * const &component : components ) {
            auto iter = std::ranges::find_if( ret, [component]( item_comp & obj ) {
                return obj.type == component->typeId();
            } );

            if( iter != ret.end() ) {
                iter->count += component->count();
            } else {
                ret.emplace_back( component->typeId(), component->count() );
            }
        }
    }
    return ret;
}

void item::set_favorite( const bool favorite )
{
    is_favorite = favorite;
}

const recipe &item::get_making() const
{
    if( !craft_data_ ) {
        debugmsg( "'%s' is not a craft or has a null recipe", tname() );
        static const recipe dummy{};
        return dummy;
    }
    assert( craft_data_->making );
    return *craft_data_->making;
}

void item::set_tools_to_continue( bool value )
{
    assert( craft_data_ );
    craft_data_->tools_to_continue = value;
}

bool item::has_tools_to_continue() const
{
    assert( craft_data_ );
    return craft_data_->tools_to_continue;
}

void item::set_cached_tool_selections( const std::vector<comp_selection<tool_comp>> &selections )
{
    assert( craft_data_ );
    craft_data_->cached_tool_selections = selections;
}

const std::vector<comp_selection<tool_comp>> &item::get_cached_tool_selections() const
{
    assert( craft_data_ );
    return craft_data_->cached_tool_selections;
}

const cata::value_ptr<islot_comestible> &item::get_comestible() const
{
    if( is_craft() ) {
        return craft_data_->making->result()->comestible;
    } else {
        return type->comestible;
    }
}

bool item::has_clothing_mod() const
{
    for( const clothing_mod &cm : clothing_mods::get_all() ) {
        if( has_own_flag( cm.flag ) ) {
            return true;
        }
    }
    return false;
}

namespace
{
const std::string &get_clothing_mod_val_key( clothing_mod_type type )
{
    const static auto cache = ( []() {
        std::array<std::string, clothing_mods::all_clothing_mod_types.size()> res;
        for( const clothing_mod_type &type : clothing_mods::all_clothing_mod_types ) {
            res[type] = CLOTHING_MOD_VAR_PREFIX
                        + clothing_mods::string_from_clothing_mod_type( clothing_mods::all_clothing_mod_types[type] );
        }
        return res;
    } )();

    return cache[ type ];
}
} // namespace

float item::get_clothing_mod_val( clothing_mod_type type ) const
{
    return get_var( get_clothing_mod_val_key( type ), 0.0f );
}

void item::update_clothing_mod_val()
{
    for( const clothing_mod_type &type : clothing_mods::all_clothing_mod_types ) {
        float tmp = 0.0;
        for( const clothing_mod &cm : clothing_mods::get_all_with( type ) ) {
            if( has_own_flag( cm.flag ) ) {
                tmp += cm.get_mod_val( type, *this );
            }
        }
        set_var( get_clothing_mod_val_key( type ), tmp );
    }
}

item_location_type item::where() const
{
    if( !loc ) {
        if( !saved_loc ) {
            debugmsg( "Tried to find where of an item without a location" );
            return item_location_type::invalid;
        }
        return static_cast<item_location *>( &*saved_loc )->where();
    }
    return static_cast<item_location *>( &*loc )->where();
}

item &item::obtain( Character &ch, int qty, bool costs_moves )
{
    if( costs_moves ) {
        ch.moves -= obtain_cost( ch, qty );
    }
    if( ch.is_worn( *this ) || ch.is_wielding( *this ) ) {
        return *this;
    }
    return ch.i_add( split( qty ) );
}

int item::obtain_cost( const Character &ch, int qty ) const
{
    if( !loc ) {
        debugmsg( "Tried to find obtain cost of an item without a location" );
        return 0;
    }
    return static_cast<item_location *>( &*loc )->obtain_cost( ch, qty, this );
}

std::string item::describe_location( const Character *ch ) const
{
    if( !loc ) {
        if( !saved_loc ) {
            debugmsg( "Tried to describe the location of an item without a location" );
            return "nowhere";
        }
        return saved_loc->describe( ch, this );
    }
    return loc->describe( ch, this );
}

item *item::parent_item() const
{
    contents_item_location *cont = dynamic_cast<contents_item_location *>( &*loc );
    if( !cont ) {
        return nullptr;
    }
    return cont->parent();
}

std::vector<detached_ptr<item>> item::remove_components()
{
    return components.clear();
}

detached_ptr<item> item::remove_component( item &it )
{
    for( auto iter = components.begin(); iter != components.end(); iter++ ) {
        if( *iter == &it ) {
            detached_ptr<item> ret;
            components.erase( iter, &ret );
            return ret;
        }
    }
    debugmsg( "Could not find component for removal" );
    return detached_ptr<item>();
}

void item::add_component( detached_ptr<item> &&comp )
{
    components.push_back( std::move( comp ) );
}

const location_vector<item> &item::get_components() const
{
    return components;
}

location_vector<item> &item::get_components()
{
    return components;
}

bool item::init_kill_tracker()
{
    if( kills ) {
        return true;
    } else if( get_option<bool>( "ENABLE_EVENTS" ) ) {
        kills = std::make_unique<kill_tracker>( false );
        return true;
    } else {
        return false;
    }
}
void item::add_monster_kill( mtype_id mon )
{
    if( init_kill_tracker() ) {
        kills->add_monster( mon );
    }
}
void item::add_npc_kill( std::string npc )
{
    if( init_kill_tracker() ) {
        kills->add_npc( npc );
    }
}
void item::show_kill_list()
{
    if( !kills ) {
        debugmsg( "Tried to display empty kill list" );
        return;
    }
    show_kills( *kills );
}
int item::kill_count()
{
    if( !kills ) {
        return 0;
    } else {
        return kills->monster_kill_count() + kills->npc_kill_count();
    }
}

bool cable_connection_data::ups_connected( const item *const cable )
{
    return cable && cable->has_flag( flag_CABLE_SPOOL ) &&
           ( cable_state( cable->get_var( p1_name, 0.0 ) ) == state_UPS ||
             cable_state( cable->get_var( p2_name, 0.0 ) ) == state_UPS );
}

std::optional<cable_connection_data> cable_connection_data::make_data( const item &cable )
{
    if( cable.has_flag( flag_CABLE_SPOOL ) ) {
        return cable_connection_data( cable );
    } else {
        return std::nullopt;
    }
}
