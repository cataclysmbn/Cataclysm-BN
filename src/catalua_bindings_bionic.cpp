#include "catalua_bindings.h"

#include "catalua.h"
#include "catalua_bindings_utils.h"
#include "catalua_impl.h"
#include "catalua_log.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "bionics.h"

void cata::detail::mod_bionic_data( sol::state &lua )
{
#define UT_CLASS bionic_data
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        /* === General Bionic Properties === */
        DOC( "Whether this bionic can be activated." );
        luna::set_fx( ut, "activated", []( const UT_CLASS & c ) { return c.activated; } );
        DOC( "Whether this bionic is included with another." );
        luna::set_fx( ut, "included", []( const UT_CLASS & c ) { return c.included; } );
        DOC( "Point cost in character creation." );
        luna::set_fx( ut, "points", []( const UT_CLASS & c ) { return c.points; } );
        DOC( "Whether this bionic can be uninstalled." );
        luna::set_fx( ut, "can_uninstall", []( const UT_CLASS & c ) { return c.can_uninstall; } );
        DOC( "Whether this bionic is available as a starting bionic." );
        luna::set_fx( ut, "starting_bionic", []( const UT_CLASS & c ) { return c.starting_bionic; } );

        /* === Power === */
        DOC( "Power cost on activation (joules)." );
        luna::set_fx( ut, "power_activate", []( const UT_CLASS & c ) { return  units::to_joule( c.power_activate ); } );
        DOC( "Power cost on deactivation (joules)." );
        luna::set_fx( ut, "power_deactivate", []( const UT_CLASS & c ) { return  units::to_joule( c.power_deactivate ); } );
        DOC( "Power cost over time (joules)." );
        luna::set_fx( ut, "power_over_time", []( const UT_CLASS & c ) { return units::to_joule( c.power_over_time ); } );
        DOC( "Power cost when triggered (joules)." );
        luna::set_fx( ut, "power_trigger", []( const UT_CLASS & c ) { return  units::to_joule( c.power_trigger ); } );
        DOC( "How often bionic draws/produces power (turns)." );
        luna::set_fx( ut, "charge_time", []( const UT_CLASS & c ) { return c.charge_time; } );
        DOC( "Power bank capacity (joules)." );
        luna::set_fx( ut, "capacity", []( const UT_CLASS & c ) { return  units::to_joule( c.capacity ); } );

        /* === Weight Capacity === */
        DOC( "Weight capacity multiplier." );
        luna::set_fx( ut, "weight_capacity_modifier", []( const UT_CLASS & c ) { return c.weight_capacity_modifier; } );
        DOC( "Weight capacity flat bonus." );
        luna::set_fx( ut, "weight_capacity_bonus", []( const UT_CLASS & c ) { return c.weight_capacity_bonus; } );

        /* === Fuel === */
        DOC( "Fraction of fuel energy converted to bionic power." );
        luna::set_fx( ut, "fuel_efficiency", []( const UT_CLASS & c ) { return c.fuel_efficiency; } );
        DOC( "Fraction of fuel energy passively converted to bionic power." );
        luna::set_fx( ut, "passive_fuel_efficiency", []( const UT_CLASS & c ) { return c.passive_fuel_efficiency; } );
        DOC( "How much fuel this bionic can hold." );
        luna::set_fx( ut, "fuel_capacity", []( const UT_CLASS & c ) { return c.fuel_capacity; } );
        DOC( "Multiplies the amount of fuel when loading." );
        luna::set_fx( ut, "fuel_multiplier", []( const UT_CLASS & c ) { return c.fuel_multiplier; } );
        DOC( "Whether this bionic emits heat when producing power." );
        luna::set_fx( ut, "exothermic_power_gen", []( const UT_CLASS & c ) { return c.exothermic_power_gen; } );
        DOC( "Whether this bionic draws power through a cable." );
        luna::set_fx( ut, "is_remote_fueled", []( const UT_CLASS & c ) { return c.is_remote_fueled; } );

        DOC( "Returns a list of every bionic in the game." );
        SET_FX_T( get_all, std::vector<bionic_data>() );

        /* === Type Checking Helpers === */
        DOC( "Whether this bionic is a power source." );
        SET_FX_T( is_power_source, bool() const );
        DOC( "Whether this bionic can be toggled on/off." );
        SET_FX_T( is_toggled, bool() const );
        DOC( "Whether this bionic is a gun bionic." );
        SET_FX_T( is_gun, bool() const );
        DOC( "Whether this bionic is a weapon bionic." );
        SET_FX_T( is_weapon, bool() const );

        luna::set_fx( ut, sol::meta_function::to_string,
        []( const UT_CLASS & bd ) -> std::string {
            return string_format( "%s[%s]", luna::detail::luna_traits<UT_CLASS>::name, bd.id.c_str() );
        } );

        // Relationships
        DOC( "Lists bionics required to install this one." );
        luna::set_fx( ut, "required_bionics", []( const UT_CLASS & bio ) -> std::vector<bionic_id> {
            return bio.required_bionics;
        } );
        DOC( "Lists bionics that conflict with this one." );
        luna::set_fx( ut, "conflicting_bionics", []( const UT_CLASS & bio ) -> std::vector<bionic_id> {
            return bio.conflicting_bionics;
        } );
        DOC( "Lists bionics included when this one is installed." );
        luna::set_fx( ut, "included_bionics", []( const UT_CLASS & bio ) -> std::vector<bionic_id> {
            return bio.included_bionics;
        } );
        DOC( "Lists mutations/traits canceled by this bionic." );
        luna::set_fx( ut, "canceled_mutations", []( const UT_CLASS & bio ) -> std::vector<trait_id> {
            return bio.canceled_mutations;
        } );
    }
#undef UT_CLASS

#define UT_CLASS bionic_bonuses
    {
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        /* === Stat Modifiers === */
        luna::set_fx( ut, "str_modifier", []( const UT_CLASS & c ) { return c.str_modifier; } );
        luna::set_fx( ut, "dex_modifier", []( const UT_CLASS & c ) { return c.dex_modifier; } );
        luna::set_fx( ut, "per_modifier", []( const UT_CLASS & c ) { return c.per_modifier; } );
        luna::set_fx( ut, "int_modifier", []( const UT_CLASS & c ) { return c.int_modifier; } );

        /* === Health & Healing === */
        luna::set_fx( ut, "pain_recovery", []( const UT_CLASS & c ) { return c.pain_recovery; } );
        luna::set_fx( ut, "healing_awake", []( const UT_CLASS & c ) { return c.healing_awake; } );
        luna::set_fx( ut, "healing_resting", []( const UT_CLASS & c ) { return c.healing_resting; } );
        DOC( "Multiplier applied to broken limb regeneration." );
        luna::set_fx( ut, "mending_modifier", []( const UT_CLASS & c ) { return c.mending_modifier; } );
        DOC( "Bonus HP multiplier. 1.0 doubles HP; -0.5 halves it." );
        luna::set_fx( ut, "hp_modifier", []( const UT_CLASS & c ) { return c.hp_modifier; } );
        DOC( "Secondary HP multiplier; stacks with hp_modifier." );
        luna::set_fx( ut, "hp_modifier_secondary", []( const UT_CLASS & c ) { return c.hp_modifier_secondary; } );
        DOC( "Flat adjustment to HP." );
        luna::set_fx( ut, "hp_adjustment", []( const UT_CLASS & c ) { return c.hp_adjustment; } );
        DOC( "How quickly health trends toward healthy_mod." );
        luna::set_fx( ut, "healthy_rate", []( const UT_CLASS & c ) { return c.healthy_rate; } );
        luna::set_fx( ut, "bleed_resist", []( const UT_CLASS & c ) { return c.bleed_resist; } );

        /* === Combat Bonuses === */
        luna::set_fx( ut, "cut_dmg_bonus", []( const UT_CLASS & c ) { return c.cut_dmg_bonus; } );
        luna::set_fx( ut, "pierce_dmg_bonus", []( const UT_CLASS & c ) { return c.pierce_dmg_bonus; } );
        luna::set_fx( ut, "bash_dmg_bonus", []( const UT_CLASS & c ) { return c.bash_dmg_bonus; } );

        /* === Movement & Speed === */
        luna::set_fx( ut, "dodge_modifier", []( const UT_CLASS & c ) { return c.dodge_modifier; } );
        luna::set_fx( ut, "speed_modifier", []( const UT_CLASS & c ) { return c.speed_modifier; } );
        luna::set_fx( ut, "movecost_modifier", []( const UT_CLASS & c ) { return c.movecost_modifier; } );
        luna::set_fx( ut, "movecost_flatground_modifier", []( const UT_CLASS & c ) { return c.movecost_flatground_modifier; } );
        luna::set_fx( ut, "movecost_obstacle_modifier", []( const UT_CLASS & c ) { return c.movecost_obstacle_modifier; } );
        luna::set_fx( ut, "movecost_swim_modifier", []( const UT_CLASS & c ) { return c.movecost_swim_modifier; } );
        luna::set_fx( ut, "attackcost_modifier", []( const UT_CLASS & c ) { return c.attackcost_modifier; } );

        /* === Physical Capabilities === */
        luna::set_fx( ut, "falling_damage_multiplier", []( const UT_CLASS & c ) { return c.falling_damage_multiplier; } );
        luna::set_fx( ut, "max_stamina_modifier", []( const UT_CLASS & c ) { return c.max_stamina_modifier; } );
        luna::set_fx( ut, "stamina_regen_modifier", []( const UT_CLASS & c ) { return c.stamina_regen_modifier; } );
        luna::set_fx( ut, "weight_capacity_modifier", []( const UT_CLASS & c ) { return c.weight_capacity_modifier; } );
        luna::set_fx( ut, "packmule_modifier", []( const UT_CLASS & c ) { return c.packmule_modifier; } );

        /* === Perception & Stealth === */
        luna::set_fx( ut, "hearing_modifier", []( const UT_CLASS & c ) { return c.hearing_modifier; } );
        luna::set_fx( ut, "noise_modifier", []( const UT_CLASS & c ) { return c.noise_modifier; } );
        luna::set_fx( ut, "stealth_modifier", []( const UT_CLASS & c ) { return c.stealth_modifier; } );
        luna::set_fx( ut, "night_vision_range", []( const UT_CLASS & c ) { return c.night_vision_range; } );

        /* === Environment === */
        luna::set_fx( ut, "bodytemp_min", []( const UT_CLASS & c ) { return c.bodytemp_min; } );
        luna::set_fx( ut, "bodytemp_max", []( const UT_CLASS & c ) { return c.bodytemp_max; } );
        luna::set_fx( ut, "bodytemp_sleep", []( const UT_CLASS & c ) { return c.bodytemp_sleep; } );
        luna::set_fx( ut, "temperature_speed_modifier", []( const UT_CLASS & c ) { return c.temperature_speed_modifier; } );
        luna::set_fx( ut, "scent_modifier", []( const UT_CLASS & c ) { return c.scent_modifier; } );

        /* === Resource Consumption === */
        luna::set_fx( ut, "metabolism_modifier", []( const UT_CLASS & c ) { return c.metabolism_modifier; } );
        luna::set_fx( ut, "thirst_modifier", []( const UT_CLASS & c ) { return c.thirst_modifier; } );
        luna::set_fx( ut, "fatigue_modifier", []( const UT_CLASS & c ) { return c.fatigue_modifier; } );
        luna::set_fx( ut, "fatigue_regen_modifier", []( const UT_CLASS & c ) { return c.fatigue_regen_modifier; } );

        /* === Skills & Crafting === */
        luna::set_fx( ut, "reading_speed_multiplier", []( const UT_CLASS & c ) { return c.reading_speed_multiplier; } );
        luna::set_fx( ut, "skill_rust_multiplier", []( const UT_CLASS & c ) { return c.skill_rust_multiplier; } );
        luna::set_fx( ut, "crafting_speed_modifier", []( const UT_CLASS & c ) { return c.crafting_speed_modifier; } );
        luna::set_fx( ut, "construction_speed_modifier", []( const UT_CLASS & c ) { return c.construction_speed_modifier; } );

        /* === Overmap === */
        luna::set_fx( ut, "overmap_sight", []( const UT_CLASS & c ) { return c.overmap_sight; } );
        luna::set_fx( ut, "overmap_multiplier", []( const UT_CLASS & c ) { return c.overmap_multiplier; } );

        /* === Mana === */
        luna::set_fx( ut, "mana_modifier", []( const UT_CLASS & c ) { return c.mana_modifier; } );
        luna::set_fx( ut, "mana_multiplier", []( const UT_CLASS & c ) { return c.mana_multiplier; } );
        luna::set_fx( ut, "mana_regen_multiplier", []( const UT_CLASS & c ) { return c.mana_regen_multiplier; } );

        DOC( "Returns true if any field differs from its default value." );
        SET_FX_T( has_any, bool() const );
    }
#undef UT_CLASS
}
