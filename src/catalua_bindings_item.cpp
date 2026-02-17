#include "catalua_bindings.h"

#include <ranges>

#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "itype.h"
#include "mtype.h"
#include "material.h"
#include "faction.h"
#include "character.h"
#include "martialarts.h"
#include "relic.h"
#include "vitamin.h"
#include "gun_mode.h"
#include "mod_manager.h"
#include "ammo_effect.h"
#include "mongroup.h"
#include "disease.h"
#include "skill.h"
#include "ammo.h"
#include "flag.h"
#include "emit.h"
#include "fault.h"
#include "recipe.h"
#include "explosion.h"

static void reg_explosion_data( sol::state &lua );
static void reg_islot( sol::state &lua );
static void reg_itype( sol::state &lua );
static void reg_item( sol::state &lua );

void cata::detail::reg_item( sol::state &lua )
{
    ::reg_explosion_data( lua );
    ::reg_itype( lua );
    ::reg_islot( lua );
    ::reg_item( lua );
}

void reg_item( sol::state &lua )
{
#define UT_CLASS item
    {
        sol::usertype<item> ut = luna::new_usertype<item>( lua, luna::no_bases, luna::no_constructor );

        luna::set_fx( ut, "get_type", &item::typeId );
        DOC( "Almost for a corpse." );
        luna::set_fx( ut, "get_mtype",
        []( const item & it ) { return it.get_mtype() ? it.get_mtype()->id : mtype_id::NULL_ID(); } );

        DOC( "Translated item name with prefixes" );
        SET_FX( tname );

        DOC( "Display name with all bells and whistles like ammo and prefixes" );
        SET_FX( display_name );

        DOC( "Weight of the item. The first `bool` is whether including contents, second `bool` is whether it is `integral_weight`." );
        luna::set_fx( ut, "weight", [](
                          item & it,
                          sol::optional<bool> include_contents,
                          sol::optional<bool> integral
        ) { return it.weight( include_contents.value_or( true ), integral.value_or( false ) ); } );

        DOC( "Volume of the item. `bool` is whether it is `integral_volume`." );
        luna::set_fx( ut, "volume",
        []( item & it, sol::optional<bool> integral ) { return it.volume( integral.value_or( false ) ); } );

        DOC( "Cents of the item. `bool` is whether it is a post-cataclysm value." );
        SET_FX( price );

        DOC( "Check for variable of any type" );
        SET_FX( has_var );
        DOC( "Erase variable" );
        SET_FX( erase_var );
        DOC( "Erase all variables" );
        SET_FX( clear_vars );

        DOC( "Spawns a new item. Same as gapi.create_item " );
        luna::set_fx( ut, "spawn", []( const itype_id & itype, int count )
        {
            return item::spawn( itype, calendar::turn, count );
        } );

        SET_FX( is_null );
        SET_FX( is_unarmed_weapon );
        SET_FX( is_sided );
        SET_FX( is_power_armor );
        SET_FX( is_money );
        SET_FX( is_gun );
        SET_FX( is_firearm );
        SET_FX( is_silent );
        SET_FX( is_gunmod );
        SET_FX( is_bionic );
        SET_FX( is_ammo_belt );
        SET_FX( is_bandolier );
        SET_FX( is_holster );
        SET_FX( is_ammo );
        SET_FX( is_comestible );
        SET_FX( is_food );
        SET_FX( is_medication );
        SET_FX( is_brewable );
        SET_FX( is_food_container );
        SET_FX( is_med_container );
        SET_FX( is_corpse );
        SET_FX( is_ammo_container );
        SET_FX( is_armor );
        SET_FX( is_book );
        SET_FX( is_map );
        SET_FX( is_container );
        SET_FX( is_watertight_container );
        SET_FX( is_non_resealable_container );
        SET_FX( is_bucket );
        SET_FX( is_bucket_nonempty );
        SET_FX( is_engine );
        SET_FX( is_wheel );
        SET_FX( is_fuel );
        SET_FX( is_toolmod );
        SET_FX( is_faulty );
        SET_FX( is_irremovable );
        SET_FX( is_container_empty );
        SET_FX( is_salvageable );
        SET_FX( is_craft );
        SET_FX( is_emissive );
        SET_FX( is_deployable );
        SET_FX( is_tool );
        SET_FX( is_transformable );
        SET_FX( is_artifact );
        SET_FX( is_relic );
        SET_FX( is_seed );
        SET_FX( is_dangerous );
        SET_FX( is_tainted );
        SET_FX( is_soft );
        SET_FX( is_reloadable );
        DOC( "DEPRECATED: Items are no longer filthy" );
        luna::set_fx( ut, "is_filthy", []() { return false; } );
        SET_FX( is_active );
        SET_FX( is_upgrade );

        SET_FX( activate );
        SET_FX( deactivate );
        SET_FX( set_charges );

        SET_FX( set_counter );
        SET_FX( get_counter );

        DOC( "Is this item an effective melee weapon for the given damage type?" );
        luna::set_fx( ut, "is_melee", sol::resolve<bool( damage_type ) const>
                      ( &item::is_melee ) );

        DOC( "Is this a magazine? (batteries are magazines)" );
        SET_FX( is_magazine );

        DOC( "DEPRECATED: Is this a battery? (spoiler: it isn't)" );
        SET_FX( is_battery );

        SET_FX( conductive );

        luna::set_fx( ut, "is_stackable", sol::resolve<bool() const> ( &item::count_by_charges ) );

        luna::set( ut, "charges", &item::charges );

        SET_FX( energy_remaining );

        SET_FX( has_infinite_charges );

        SET_FX( mod_charges );

        luna::set_fx( ut, "made_of", []( item & it )
        {
            return it.made_of();
        } );

        luna::set_fx( ut, "is_made_of",
                      sol::resolve < auto( const material_id & ) const -> bool > ( &item::made_of ) );

        luna::set_fx( ut, "get_kcal", []( item & it ) -> int { return it.is_comestible() ? it.get_comestible()->default_nutrition.kcal : 0; } );
        luna::set_fx( ut, "get_quench", []( item & it ) -> int { return it.is_comestible() ? it.get_comestible()->quench : 0; } );
        luna::set_fx( ut, "get_comestible_fun", []( item & it ) -> int { return it.get_comestible_fun(); } );
        DOC( "Gets the TimeDuration until this item rots" );
        SET_FX( get_rot );

        DOC( "Gets the category id this item is in" );
        SET_FX( get_category_id );

        DOC( "Gets the faction id that owns this item" );
        SET_FX( get_owner );

        DOC( "Sets the ownership of this item to a faction" );
        luna::set_fx( ut, "set_owner",
                      sol::resolve<void( const faction_id & )>
                      ( &item::set_owner ) );

        DOC( "Sets the ownership of this item to a character" );
        luna::set_fx( ut, "set_owner",
                      sol::resolve<void( const Character & )>
                      ( &item::set_owner ) );

        SET_FX( get_owner_name );

        DOC( "Checks if this item owned by a character" );
        SET_FX( is_owned_by );

        DOC( "Checks if this item has the technique as an addition. Doesn't check original techniques." );
        luna::set_fx( ut, "has_technique",
                      sol::resolve<bool( const matec_id & ) const> ( &item::has_technique ) );
        DOC( "Gets all techniques. Including original techniques." );
        luna::set_fx( ut, "get_techniques",
                      sol::resolve<std::set<matec_id>() const> ( &item::get_techniques ) );
        DOC( "Adds the technique. It isn't treated original, but additional." );
        luna::set_fx( ut, "add_technique",
                      sol::resolve<void( const matec_id & )> ( &item::add_technique ) );
        DOC( "Removes the additional technique. Doesn't affect originial techniques." );
        luna::set_fx( ut, "remove_technique",
                      sol::resolve<void( const matec_id & )> ( &item::remove_technique ) );

        DOC( "Checks if this item can contain another" );
        luna::set_fx( ut, "can_contain",
                      sol::resolve<bool( const item & ) const>
                      ( &item::can_contain ) );

        DOC( "Gets the remaining space available for a type of liquid" );
        luna::set_fx( ut, "remaining_capacity_for_id", &item::get_remaining_capacity_for_id );

        DOC( "Gets maximum volume this item can hold (liquids, ammo, etc)" );
        luna::set_fx( ut, "total_capacity", &item::get_total_capacity );

        DOC( "Gets the current magazine" );
        luna::set_fx( ut, "current_magazine",
                      sol::resolve<const item*() const> ( &item::magazine_current ) );

        DOC( "Gets the maximum capacity of a magazine" );
        luna::set_fx( ut, "ammo_capacity",
                      sol::resolve<int( const bool ) const>
                      ( &item::ammo_capacity ) );

        DOC( "Get remaining ammo, works with batteries & stuff too" );
        SET_FX( ammo_remaining );

        SET_FX( ammo_data );
        SET_FX( ammo_required );
        SET_FX( ammo_current );

        SET_FX( ammo_consume );
        SET_FX( ammo_set );
        SET_FX( ammo_unset );

        SET_FX( get_reload_time );

        DOC( "Adds an item(s) to contents" );
        SET_FX( add_item_with_id );

        DOC( "Checks item contents for a given item id" );
        SET_FX( has_item_with_id );

        DOC( "Checks if the item covers a bodypart" );
        SET_FX( covers );

        SET_FX( set_flag );
        SET_FX( unset_flag );
        SET_FX( has_flag );
        SET_FX( has_own_flag );
        SET_FX( set_flag_recursive );
        SET_FX( unset_flags );

        DOC( "Converts the item as given `ItypeId`." );
        SET_FX( convert );

        DOC( "Get variable as string" );
        luna::set_fx( ut, "get_var_str",
                      sol::resolve<std::string( const std::string &, const std::string & ) const>
                      ( &item::get_var ) );
        DOC( "Get variable as float number" );
        luna::set_fx( ut, "get_var_num",
                      sol::resolve<double( const std::string &, double ) const>( &item::get_var ) );
        DOC( "Get variable as tripoint" );
        luna::set_fx( ut, "get_var_tri",
                      sol::resolve<tripoint( const std::string &, const tripoint & ) const>
                      ( &item::get_var ) );

        luna::set_fx( ut, "set_var_str", sol::resolve<void( const std::string &, const std::string & )>
                      ( &item::set_var ) );
        luna::set_fx( ut, "set_var_num",
                      sol::resolve<void( const std::string &, double )>( &item::set_var ) );
        luna::set_fx( ut, "set_var_tri",
                      sol::resolve<void( const std::string &, const tripoint & )>( &item::set_var ) );

        SET_FX( attack_cost );
        SET_FX( stamina_cost );

        // Damage (breakage) related bindings
        DOC( "Get current item damage value (durability). Higher values mean more damaged. Default range is -1000 (min) to 4000 (max), configurable via 'damage_states' in JSON." );
        luna::set_fx( ut, "get_damage", &item::damage );

        DOC( "Get item damage as a level from 0 to max. Used for UI display and damage thresholds." );
        luna::set_fx( ut, "get_damage_level", sol::overload(
        []( const item & self ) -> int {
            return self.damage_level( 4 );
        },
        []( const item & self, int max ) -> int {
            return self.damage_level( max );
        }
                      ) );

        DOC( "Get minimum possible damage value (can be negative for reinforced items). Default is -1000, configurable via 'damage_states' in JSON." );
        luna::set_fx( ut, "get_min_damage", &item::min_damage );

        DOC( "Get maximum possible damage value before item is destroyed. Default is 4000, configurable via 'damage_states' in JSON." );
        luna::set_fx( ut, "get_max_damage", &item::max_damage );

        DOC( "Get relative health as ratio 0.0-1.0, where 1.0 is undamaged and 0.0 is destroyed" );
        luna::set_fx( ut, "get_relative_health", &item::get_relative_health );

        DOC( "Set item damage to specified value. Clamped between min_damage and max_damage." );
        luna::set_fx( ut, "set_damage", &item::set_damage );

        DOC( "Modify item damage by given amount. Returns true if item should be destroyed." );
        luna::set_fx( ut, "mod_damage", sol::overload(
                          static_cast<bool( item::* )( int )>( &item::mod_damage ),
                          static_cast<bool( item::* )( int, damage_type )>( &item::mod_damage )
                      ) );

        SET_FX( get_melee_damage_bonus );
        SET_FX( get_melee_hit_bonus );
        SET_FX( get_ranged_damage_bonus );
        SET_FX( get_range_bonus );
        SET_FX( get_dispersion_bonus );
        SET_FX( get_recoil_bonus );

        SET_FX( set_melee_damage_bonus );
        SET_FX( set_melee_hit_bonus );
        SET_FX( set_ranged_damage_bonus );
        SET_FX( set_range_bonus );
        SET_FX( set_dispersion_bonus );
        SET_FX( set_recoil_bonus );

    }
#undef UT_CLASS
}

#define VALUE_PTR_MEMB_N(prop_name, lua_name_str) luna::set_fx( ut, lua_name_str, [](const UT_CLASS& c) { return c.prop_name.get(); } )

void reg_itype( sol::state &lua )
{
#define UT_CLASS itype
    {
        DOC( "Slots for various item type properties. Each slot may contain a valid value or nil" );
        sol::usertype<itype> ut = luna::new_usertype<itype>( lua, luna::no_bases, luna::no_constructor );

        VALUE_PTR_MEMB_N( container, "slot_container" );
        VALUE_PTR_MEMB_N( tool, "slot_tool" );
        VALUE_PTR_MEMB_N( comestible, "slot_comestible" );
        VALUE_PTR_MEMB_N( brewable, "slot_brewable" );
        VALUE_PTR_MEMB_N( armor, "slot_armor" );
        VALUE_PTR_MEMB_N( pet_armor, "slot_pet_armor" );
        VALUE_PTR_MEMB_N( book, "slot_book" );
        VALUE_PTR_MEMB_N( mod, "slot_mod" );
        VALUE_PTR_MEMB_N( engine, "slot_engine" );
        VALUE_PTR_MEMB_N( wheel, "slot_wheel" );
        VALUE_PTR_MEMB_N( fuel, "slot_fuel" );
        VALUE_PTR_MEMB_N( gun, "slot_gun" );
        VALUE_PTR_MEMB_N( gunmod, "slot_gunmod" );
        VALUE_PTR_MEMB_N( magazine, "slot_magazine" );
        VALUE_PTR_MEMB_N( battery, "slot_battery" );
        VALUE_PTR_MEMB_N( bionic, "slot_bionic" );
        VALUE_PTR_MEMB_N( ammo, "slot_ammo" );
        VALUE_PTR_MEMB_N( seed, "slot_seed" );
        VALUE_PTR_MEMB_N( artifact, "slot_artifact" );
        VALUE_PTR_MEMB_N( relic_data, "slot_relic" );
        VALUE_PTR_MEMB_N( milling_data, "slot_milling" );

        //TODO: Check nothing important is missing below

        auto get_action = []( const use_function & c ) { return c.get_type();};
        auto get_uses = [ = ]( const UT_CLASS & c )
        {
            std::vector<std::string> rv {};
            std::ranges::copy( c.use_methods | std::views::values | std::views::transform( get_action ),
                               std::back_inserter( rv ) );
            return rv;
        };

        SET_FX_N( get_id, "type_id" );
        SET_FX( can_have_charges );
        SET_FX( can_use );
        SET_FX( charge_factor );
        SET_FX( charges_default );
        SET_FX( charges_per_volume );
        SET_FX( charges_to_use );
        SET_FX_N( count_by_charges, "is_stackable" );
        SET_FX( damage_max );
        SET_FX( damage_min );
        SET_FX( get_flags );
        SET_FX( has_flag );
        SET_FX( has_use );
        SET_FX( maximum_charges );
        SET_FX_N( nname, "get_name" );
        SET_MEMB_N_RO( explosion, "explosion_data" );
        SET_MEMB_N_RO( m_to_hit, "melee_to_hit" );
        luna::set_fx( ut, "source_mod", []( const UT_CLASS & c )
        {
            std::vector<mod_id> rv {};
            std::ranges::copy( c.src | std::views::transform( []( auto & p ) { return p.second; } ),
            std::back_inserter( rv ) );
            return rv;
        } );
        SET_MEMB_RO( attacks );
        SET_MEMB_RO( countdown_destroy );
        SET_MEMB_RO( countdown_interval );
        SET_MEMB_RO( default_container );
        SET_MEMB_RO( emits );
        SET_MEMB_RO( explode_in_fire );
        SET_MEMB_RO( faults );
        SET_MEMB_RO( integral_volume );
        SET_MEMB_RO( integral_weight );
        SET_MEMB_RO( item_tags );
        SET_MEMB_RO( layer );
        SET_MEMB_RO( light_emission );
        SET_MEMB_RO( looks_like );
        SET_MEMB_RO( materials );
        SET_MEMB_RO( min_dex );
        SET_MEMB_RO( min_int );
        SET_MEMB_RO( min_per );
        SET_MEMB_RO( min_skills );
        SET_MEMB_RO( min_str );
        SET_MEMB_RO( phase );
        luna::set_fx( ut, "price", []( const UT_CLASS & c ) { return c.price.value(); } );
        luna::set_fx( ut, "price_post", []( const UT_CLASS & c ) { return c.price.value(); } );
        SET_MEMB_RO( properties );
        SET_MEMB_RO( qualities );
        SET_MEMB_RO( recipes );
        SET_MEMB_RO( repair );
        SET_MEMB_RO( repairs_like );
        SET_MEMB_RO( rigid );
        SET_MEMB_RO( stack_size );
        SET_MEMB_RO( techniques );
        SET_MEMB_RO( thrown_damage );
        SET_MEMB_RO( volume );
        SET_MEMB_RO( weapon_category );
        SET_MEMB_RO( weight );
        luna::set_fx( ut, "get_countdown_action", [ = ]( const UT_CLASS & c ) { return get_action( c.countdown_action ); } );
        luna::set_fx( ut, "get_description", []( const UT_CLASS & c, const int num ) { return c.description.translated( num ); } );
        luna::set_fx( ut, "get_drop_action", [ = ]( const UT_CLASS & c ) { return get_action( c.drop_action ); } );
        luna::set_fx( ut, "get_uses", get_uses );

    }
#undef UT_CLASS
}

void reg_islot( sol::state &lua )
{
#define UT_CLASS islot_container
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "Inner volume of the container" );
        SET_MEMB_RO( contains );

        DOC( "Contents do not spoil" );
        SET_MEMB_RO( preserves );

        DOC( "Can be resealed" );
        SET_MEMB_RO( seals );

        DOC( "If this is set to anything but \"null\", changing this container's contents in any way will turn this item into that type" );
        SET_MEMB_RO( unseals_into );

        DOC( "Can hold liquids" );
        SET_MEMB_RO( watertight );
    }
#undef UT_CLASS

#define UT_CLASS islot_tool
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        SET_MEMB_RO( charge_factor );
        SET_MEMB_RO( ammo_id );
        SET_MEMB_RO( charges_per_use );
        SET_MEMB_RO( def_charges );
        SET_MEMB_RO( default_ammo );
        SET_MEMB_RO( max_charges );
        SET_MEMB_RO( power_draw );
        SET_MEMB_RO( rand_charges );
        SET_MEMB_RO( revert_msg );
        SET_MEMB_RO( revert_to );
        SET_MEMB_RO( subtype );
        SET_MEMB_RO( turns_active );
        SET_MEMB_RO( turns_per_charge );
        SET_MEMB_RO( ups_eff_mult );
        SET_MEMB_RO( ups_recharge_rate );
    }
#undef UT_CLASS

#define UT_CLASS islot_comestible
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "comestible subtype - eg. FOOD, DRINK, MED" );
        SET_MEMB_N_RO( comesttype, "comest_type" );

        DOC( "tool needed to consume (e.g. lighter for cigarettes)" );
        SET_MEMB_RO( tool );

        DOC( "Defaults # of charges (drugs, loaf of bread? etc)" );
        SET_MEMB_RO( def_charges );

        DOC( "effect on character thirst (may be negative)" );
        SET_MEMB_RO( quench );

        DOC( "Nutrition values to use for this type when they aren't calculated from components" );
        luna::set_fx( ut, "get_default_nutrition", []( const UT_CLASS & c ) -> auto& {
            return c.default_nutrition.vitamins;
        } );

        DOC( "Time until becomes rotten at standard temperature, or zero if never spoils" );
        SET_MEMB_RO( spoils );

        DOC( "addiction potential" );
        SET_MEMB_N_RO( addict, "addict_value" );

        DOC( "effects of addiction" );
        SET_MEMB_N_RO( add, "addict_type" );

        DOC( "stimulant effect" );
        SET_MEMB_N_RO( stim, "stimulant_type" );

        DOC( "fatigue altering effect" );
        SET_MEMB_RO( fatigue_mod );

        DOC( "Reference to other item that replaces this one as a component in recipe results" );
        SET_MEMB_RO( cooks_like );

        DOC( "Reference to item that will be received after smoking current item" );
        SET_MEMB_RO( smoking_result );

        //DOC("TODO: add documentation");
        SET_MEMB_RO( healthy );

        DOC( "chance (odds) of becoming parasitised when eating (zero if never occurs)" );
        SET_MEMB_RO( parasites );

        DOC( "Amount of radiation you get from this comestible" );
        SET_MEMB_RO( radiation );

        DOC( "pet food category" );
        SET_MEMB_RO( petfood );

        DOC( "freezing point in degrees Fahrenheit, below this temperature item can freeze" );
        SET_MEMB_RO( freeze_point );

        DOC( "List of diseases carried by this comestible and their associated probability" );
        SET_MEMB_RO( contamination );

        DOC( "specific heats in J/(g K) and latent heat in J/g" );
        SET_MEMB_RO( specific_heat_liquid );
        SET_MEMB_RO( specific_heat_solid );
        SET_MEMB_RO( latent_heat );

        DOC( "A penalty applied to fun for every time this food has been eaten in the last 48 hours" );
        SET_MEMB_RO( monotony_penalty );

        SET_FX( has_calories );

        SET_FX( get_default_nutr );

        DOC( "The monster group that is drawn from when the item rots away" );
        SET_MEMB_RO( rot_spawn );

        DOC( "Chance the above monster group spawns" );
        SET_MEMB_RO( rot_spawn_chance );

    }
#undef UT_CLASS

#define UT_CLASS islot_brewable
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "What are the results of fermenting this item" );
        SET_MEMB_RO( results );

        DOC( "How long for this brew to ferment" );
        SET_MEMB_RO( time );
    }
#undef UT_CLASS

#define UT_CLASS islot_armor
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "Layer, encumbrance and coverage information" );
        SET_MEMB_N_RO( data, "layer_data" );

        DOC( "Resistance to environmental effects" );
        SET_MEMB_RO( env_resist );

        DOC( "Environmental protection of a gas mask with installed filter" );
        SET_MEMB_RO( env_resist_w_filter );

        DOC( "Damage negated by this armor. Usually calculated from materials+thickness" );
        SET_MEMB_RO( resistance );

        DOC( "Whether this item can be worn on either side of the body" );
        SET_MEMB_RO( sided );

        DOC( "How much storage this items provides when worn" );
        SET_MEMB_RO( storage );

        DOC( "Multiplier on resistances provided by armor's materials. Damaged armors have lower effective thickness, low capped at 1. Note: 1 thickness means item retains full resistance when damaged." );
        SET_MEMB_RO( thickness );

        DOC( "Whitelisted clothing mods. Restricted clothing mods must be listed here by id to be compatible." );
        SET_MEMB_RO( valid_mods );

        DOC( "How much warmth this item provides" );
        SET_MEMB_RO( warmth );

        DOC( "Bonus to weight capacity" );
        SET_MEMB_RO( weight_capacity_bonus );

        DOC( "Factor modifying weight capacity" );
        SET_MEMB_RO( weight_capacity_modifier );
    }
#undef UT_CLASS

#define UT_CLASS islot_pet_armor
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "The minimum volume a pet can be and wear this armor" );
        SET_MEMB_RO( min_vol );

        DOC( "The maximum volume a pet can be and wear this armor" );
        SET_MEMB_RO( max_vol );

        DOC( "Resistance to environmental effects" );
        SET_MEMB_RO( env_resist );

        DOC( "Environmental protection of a gas mask with installed filter" );
        SET_MEMB_RO( env_resist_w_filter );

        DOC( " How much storage this items provides when worn" );
        SET_MEMB_RO( storage );

        DOC( "Multiplier on resistances provided by this armor" );
        SET_MEMB_RO( thickness );

        DOC( "What animal bodytype can wear this armor" );
        SET_MEMB_RO( bodytype );
    }
#undef UT_CLASS

#define UT_CLASS islot_book
    {
        auto ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "How long in minutes it takes to read. \"To read\" means getting 1 skill point, not all of them." );
        SET_MEMB_RO( time );

        DOC( "Fun books have chapters; after all are read, the book is less fun." );
        SET_MEMB_RO( chapters );

        DOC( "Which martial art it teaches.  Can be MartialArtsId.NULL_ID" );
        SET_MEMB_RO( martial_art );

        DOC( "How fun reading this is, can be negative" );
        SET_MEMB_RO( fun );

        DOC( "Intelligence required to read it" );
        SET_MEMB_N_RO( intel, "intelligence" );

        DOC( "Which skill it upgrades, if any. Can be SkillId.NULL_ID" );
        SET_MEMB_RO( skill );

        DOC( "The skill level required to understand it" );
        SET_MEMB_N_RO( req, "skill_min" );

        DOC( "The skill level the book provides" );
        SET_MEMB_N_RO( level, "skill_max" );

        DOC( "Recipes contained in this book" );
        SET_MEMB_RO( recipes );
    }
#undef UT_CLASS

#define UT_CLASS islot_mod
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "If non-empty restrict mod to items with those base (before modifiers) ammo types" );
        SET_MEMB_RO( acceptable_ammo );

        DOC( "If set modifies parent ammo to this type" );
        SET_MEMB_RO( ammo_modifier );

        DOC( "Proportional adjustment of parent item ammo capacity" );
        SET_MEMB_RO( capacity_multiplier );

        DOC( "If non-empty replaces the compatible magazines for the parent item" );
        SET_MEMB_RO( magazine_adaptor );
    }
#undef UT_CLASS

#define UT_CLASS islot_engine
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "For combustion engines, the displacement" );
        SET_MEMB_RO( displacement );
    }
#undef UT_CLASS

#define UT_CLASS islot_wheel
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "Diameter of wheel in inches" );
        SET_MEMB_RO( diameter );

        DOC( "Width of wheel in inches" );
        SET_MEMB_RO( width );
    }
#undef UT_CLASS

#define UT_CLASS islot_fuel
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "Energy of the fuel (kilojoules per charge)" );
        SET_MEMB_RO( energy );

        SET_MEMB_RO( explosion_data );
        SET_MEMB_N_RO( has_explode_data, "has_explosion_data" );
        SET_MEMB_RO( pump_terrain );
    }
#undef UT_CLASS

#define UT_CLASS islot_gun
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::bases<common_ranged_data>(), luna::no_constructor );

        DOC( "What skill this gun uses" );
        SET_MEMB_RO( skill_used );

        DOC( "What type of ammo this gun uses" );
        SET_MEMB_RO( ammo );

        DOC( "Gun durability, affects gun being damaged during shooting" );
        SET_MEMB_RO( durability );

        DOC( "For guns with an integral magazine what is the capacity?" );
        SET_MEMB_RO( clip );

        DOC( "Reload time, in moves" );
        SET_MEMB_RO( reload_time );

        DOC( "Noise displayed when reloading the weapon" );
        SET_MEMB_RO( reload_noise );

        DOC( "Volume of the noise made when reloading this weapon" );
        SET_MEMB_RO( reload_noise_volume );

        DOC( "Maximum aim achievable using base weapon sights" );
        SET_MEMB_RO( sight_dispersion );

        DOC( "Modifies base loudness as provided by the currently loaded ammo" );
        SET_MEMB_RO( loudness );

        DOC( "If this uses UPS charges, how many (per shoot), 0 for no UPS charges at all" );
        SET_MEMB_RO( ups_charges );

        DOC( "One in X chance for gun to require major cleanup after firing blackpowder shot" );
        SET_MEMB_RO( blackpowder_tolerance );

        DOC( "Minimum ammo recoil for gun to be able to fire more than once per attack" );
        SET_MEMB_RO( min_cycle_recoil );

        DOC( "Volume of material removed by sawing down the barrel, if left unspecified barrel can't be sawed down" );
        SET_MEMB_RO( barrel_volume );

        DOC( "Effects that are applied to the ammo when fired" );
        SET_MEMB_RO( ammo_effects );

        DOC( "Location for gun mods. Key is the location (untranslated!), value is the number of mods that the location can have. The value should be > 0" );
        luna::set_fx( ut, "get_gunmod_locations", []( const UT_CLASS & c )
        {
            std::map<std::string, int> rv{};
            for( const auto& [k, v] : c.valid_mod_locations ) {
                rv[k.str()] = v;
            }
            return rv;
        } );

        DOC( "Built in mods. string is id of mod. These mods will get the IRREMOVABLE flag set" );
        SET_MEMB_RO( built_in_mods );

        DOC( "Default mods, string is id of mod. These mods are removable but are default on the weapon" );
        SET_MEMB_RO( default_mods );

        DOC( "Firing modes are supported by the gun. Always contains at least DEFAULT mode" );
        luna::set_fx( ut, "get_modes", []( const UT_CLASS & c )
        {
            std::vector<std::string> rv{};
            std::ranges::copy( c.modes | std::views::keys | std::views::transform( []( auto & v ) { return v.str(); } ),
            std::back_inserter( rv ) );
            return rv;
        } );

        DOC( "Burst size for AUTO mode (legacy field for items not migrated to specify modes )" );
        SET_MEMB_RO( burst );

        DOC( "How easy is control of recoil? If unset value automatically derived from weapon type" );
        SET_MEMB_RO( handling );

        DOC( "Additional recoil applied per shot before effects of handling are considered, useful for adding recoil effect to guns which otherwise consume no ammo" );
        SET_MEMB_RO( recoil );

        DOC( "How much ammo is consumed per shot" );
        SET_MEMB_RO( ammo_to_fire );
    }
#undef UT_CLASS

#define UT_CLASS islot_gunmod
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::bases<common_ranged_data>(), luna::no_constructor );

        DOC( "Where is this gunmod installed (e.g. \"stock\", \"rail\")?" );
        luna::set_fx( ut, "get_location", []( const UT_CLASS & c )
        {
            return c.location.str();
        } );

        DOC( "What kind of weapons this gunmod can be used with" );
        SET_MEMB_RO( usable );

        DOC( "What category of weapons this gunmod can be used with" );
        SET_MEMB_RO( usable_category );

        DOC( "What kind of weapons this gunmod can't be used with" );
        SET_MEMB_RO( exclusion );

        DOC( "What category of weapons this gunmod can't be used with" );
        SET_MEMB_RO( exclusion_category );

        DOC( "If this value is set (non-negative), this gunmod functions as a sight. A sight is only usable to aim by a character whose current Character::recoil is at or below this value." );
        SET_MEMB_RO( sight_dispersion );

        DOC( "For sights (see @ref sight_dispersion), this value affects time cost of aiming." );
        DOC( "Higher is better. In case of multiple usable sights," );
        DOC( "the one with highest aim speed is used." );
        SET_MEMB_RO( aim_speed );

        DOC( "Modifies base loudness as provided by the currently loaded ammo" );
        SET_MEMB_RO( loudness );

        DOC( "How many moves does this gunmod take to install?" );
        SET_MEMB_RO( install_time );

        DOC( "Increases base gun UPS consumption by this many times per shot" );
        SET_MEMB_RO( ups_charges_multiplier );

        DOC( "Increases base gun UPS consumption by this value per shot" );
        SET_MEMB_RO( ups_charges_modifier );

        DOC( "Increases base gun ammo to fire by this many times per shot" );
        SET_MEMB_RO( ammo_to_fire_multiplier );

        DOC( "Increases base gun ammo to fire by this value per shot" );
        SET_MEMB_RO( ammo_to_fire_modifier );

        DOC( "Increases gun weight by this many times" );
        SET_MEMB_RO( weight_multiplier );

        DOC( "Firing modes added to or replacing those of the base gun" );
        luna::set_fx( ut, "get_mode_modifiers", []( const UT_CLASS & c )
        {
            std::set<std::string> ret{};
            for( const auto &k : c.mode_modifier | std::views::keys ) {
                ret.insert( k.str() );
            }
            return ret;
        } );

        SET_MEMB_RO( ammo_effects );

        DOC( "Relative adjustment to base gun handling" );
        SET_MEMB_RO( handling );

        DOC( "Percentage value change to the gun's loading time. Higher is slower" );
        SET_MEMB_RO( reload_modifier );

        DOC( "Percentage value change to the gun's loading time. Higher is less likely" );
        SET_MEMB_RO( consume_chance );

        DOC( "Divsor to scale back gunmod consumption damage. lower is more damaging. Affected by ammo loudness and recoil, see ranged.cpp for how much." );
        SET_MEMB_RO( consume_divisor );

        DOC( "Modifies base strength required" );
        SET_MEMB_RO( min_str_required_mod );

        DOC( "Additional gunmod slots to add to the gun" );
        luna::set_fx( ut, "get_added_slots", []( const UT_CLASS & c )
        {
            std::map<std::string, int> ret;
            for( const auto& [k, v] : c.add_mod ) {
                ret[k.str()] = v;
            }
            return ret;
        } );

        DOC( "Not compatible on weapons that have this mod slot" );
        luna::set_fx( ut, "get_mod_blacklist", []( const UT_CLASS & c )
        {
            std::set<std::string> ret{};
            for( const auto &v : c.blacklist_mod ) {
                ret.insert( v.str() );
            }
            return ret;
        } );

    }
#undef UT_CLASS

#define UT_CLASS islot_magazine
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "Default type of ammo contained by a magazine (often set for ammo belts)" );
        SET_MEMB_RO( default_ammo );

        DOC( "Capacity of magazine (in equivalent units to ammo charges)" );
        SET_MEMB_RO( capacity );

        DOC( "Default amount of ammo contained by a magazine (often set for ammo belts)" );
        SET_MEMB_RO( count );

        DOC( "For ammo belts one linkage (of given type) is dropped for each unit of ammo consumed" );
        SET_MEMB_RO( linkage );

        DOC( "If false, ammo will cook off if this mag is affected by fire" );
        SET_MEMB_RO( protects_contents );

        DOC( "How reliable this magazine on a range of 0 to 10?" );
        SET_MEMB_RO( reliability );

        DOC( "How long it takes to load each unit of ammo into the magazine" );
        SET_MEMB_RO( reload_time );

        DOC( "What type of ammo this magazine can be loaded with" );
        SET_MEMB_N_RO( type, "ammo_type" );
    }
#undef UT_CLASS

#define UT_CLASS islot_battery
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "Maximum energy the battery can store" );
        SET_MEMB_RO( max_capacity );
    }
#undef UT_CLASS

#define UT_CLASS islot_bionic
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "Id of the bionic" );
        SET_MEMB_N_RO( id, "bionic_id" );

        DOC( "Arbitrary difficulty scale" );
        SET_MEMB_RO( difficulty );

        DOC( "Item with installation data that can be used to provide almost guaranteed successful install of corresponding bionic" );
        SET_MEMB_RO( installation_data );

        DOC( "Whether this CBM is an upgrade of another" );
        SET_MEMB_RO( is_upgrade );
    }
#undef UT_CLASS

#define UT_CLASS islot_ammo
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::bases<common_ranged_data>(), luna::no_constructor );

        DOC( "Default charges" );
        SET_MEMB_RO( def_charges );

        DOC( "Ammo type, basically the \"form\" of the ammo that fits into the gun/tool" );
        SET_MEMB_N_RO( type, "ammo_id" );

        SET_MEMB_RO( ammo_effects );

        DOC( "Type id of casings, if any" );
        SET_MEMB_N_RO( casing, "casing_id" );

        DOC( "Should this ammo explode in fire?" );
        SET_MEMB_RO( cookoff );

        DOC( "Chance to fail to recover the ammo used." );
        SET_MEMB_RO( dont_recover_one_in );

        SET_MEMB_RO( drop );
        SET_MEMB_RO( drop_count );
        SET_MEMB_RO( drop_active );

        SET_MEMB_RO( force_stat_display );

        DOC( "Base loudness of ammo (possibly modified by gun/gunmods)" );
        SET_MEMB_RO( loudness );

        DOC( "Recoil (per shot), roughly equivalent to kinetic energy (in Joules)" );
        SET_MEMB_RO( recoil );

        // TODO: shape_factory doesn't expose its type string
        //DOC( "AoE shape or null if it's a projectile" );
        //SET_MEMB_RO( shape );

        DOC( "Should this ammo apply a special explosion effect when in fire?" );
        SET_MEMB_RO( special_cookoff );
    }
#undef UT_CLASS

#define UT_CLASS islot_seed
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "Name of the plant." );
        luna::set_fx( ut, "get_plant_name", []( const islot_seed & s, int num ) { return s.plant_name.translated( num ); } );

        DOC( "Type id of the fruit item." );
        SET_MEMB_RO( fruit_id );

        DOC( "Time it takes for a seed to grow (based of off a season length of 91 days)." );
        SET_MEMB_RO( grow );

        DOC( "Additionally items (a list of their item ids) that will spawn when harvesting the plant." );
        SET_MEMB_RO( byproducts );

        DOC( "Amount of harvested charges of fruits is divided by this number." );
        SET_MEMB_RO( fruit_div );
    }
#undef UT_CLASS

#define UT_CLASS islot_artifact
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        SET_MEMB_RO( charge_req );
        SET_MEMB_RO( charge_type );
        SET_MEMB_RO( dream_freq_met );
        SET_MEMB_RO( dream_freq_unmet );
        SET_MEMB_RO( dream_msg_met );
        SET_MEMB_RO( dream_msg_unmet );

        SET_MEMB_RO( effects_activated );
        SET_MEMB_RO( effects_carried );
        SET_MEMB_RO( effects_wielded );
        SET_MEMB_RO( effects_worn );
    }
#undef UT_CLASS

#define UT_CLASS islot_milling
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        SET_MEMB_N_RO( conversion_rate_, "conversion_rate" );
        SET_MEMB_N_RO( into_, "converts_into" );
    }
#undef UT_CLASS

#define UT_CLASS common_ranged_data
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        SET_MEMB_N_RO( aimedcritbonus, "aimed_crit_bonus" );
        SET_MEMB_N_RO( aimedcritmaxbonus, "aimed_crit_max_bonus" );
        SET_MEMB_RO( damage );
        SET_MEMB_RO( dispersion );
        SET_MEMB_RO( range );
        SET_MEMB_RO( speed );
    }
#undef UT_CLASS

#define UT_CLASS relic
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        SET_FX( name );
        SET_FX( get_enchantments );
        SET_FX( get_recharge_scheme );
        SET_FX( get_spells );

    }
#undef UT_CLASS
}

void reg_explosion_data( sol::state &lua )
{
#define UT_CLASS explosion_data
    {
        sol::usertype<UT_CLASS> ut = luna::new_usertype<UT_CLASS>( lua, luna::no_bases, luna::no_constructor );

        DOC( "Damage dealt by the explosion" );
        SET_MEMB_RO( damage );

        DOC( "Radius of the explosion" );
        SET_MEMB_RO( radius );

        DOC( "Whether the explosion creates fire" );
        SET_MEMB_RO( fire );

        DOC( "Returns the safe range of the explosion" );
        SET_FX( safe_range );
    }
#undef UT_CLASS
}
