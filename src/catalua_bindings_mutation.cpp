#include "catalua_bindings.h"

#include "catalua.h"
// Thx Almantuxas
#include "catalua_bindings_utils.h"
#include "catalua_impl.h"
#include "catalua_log.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "mutation.h"

void cata::detail::mod_mutation_branch( sol::state &lua )
{
#define UT_CLASS mutation_branch
    {
        /* NOTE: These changes are applied to the "MutationBranchRaw" Lua obj.
         * Because mutation_branch was bound as an ID, the actual object is
         * shoved into a 'Raw' binding.
         * The following code makes that useful, while also stopping the
         * related object from being messed with.
         */
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        // Lets us grab the MutationBranchId from a MutationBranchRaw object.
        SET_MEMB_RO( id );

        /* === General Mutation Statistics === */
        DOC( "Whether this mutation is available through generic mutagen." );
        SET_MEMB_RO( valid );
        DOC( "Whether this mutation is possible to remove through Purifier. False for 'special' mutations." );
        SET_MEMB_RO( purifiable );
        DOC( "Whether this is a Threshold mutation, and thus especially difficult to mutate. One per character." );
        SET_MEMB_RO( threshold );
        DOC( "Whether this trait is ONLY gained through professional training/experience (and/or quests)." );
        SET_MEMB_RO( profession );
        DOC( "Whether or not this mutation is limited to debug use." );
        SET_MEMB_RO( debug );
        DOC( "Whether or not this mutation shows up in the status (`@`) menu." );
        SET_MEMB_RO( player_display );
        DOC( "Whether this mutation has positive /and/ negative effects." );
        SET_MEMB_RO( mixed_effect );
        DOC( "Whether this trait can normally be taken during character generation." );
        SET_MEMB_N_RO( startingtrait, "starting_trait" );
        DOC( "Whether this mutation can be activated at will." );
        SET_MEMB_RO( activated );
        DOC( "Whether a mutation activates when granted." );
        SET_MEMB_RO( starts_active );
        DOC( "Mutation allows soft gear to be worn over otherwise-restricted parts." );
        SET_MEMB_RO( allow_soft_gear );
        DOC( "Mutation causes fatigue when used." );
        SET_MEMB_RO( fatigue );
        DOC( "Mutation deducts calories when used." );
        SET_MEMB_RO( hunger );
        DOC( "Mutation dehydrates when used." );
        SET_MEMB_RO( thirst );
        DOC( "Point cost in character creation(?)." );
        SET_MEMB_RO( points );
        DOC( "How visible the mutation is to others." );
        SET_MEMB_RO( visibility );
        DOC( "How physically unappealing the mutation is. Can be negative." );
        SET_MEMB_RO( ugliness );

        SET_MEMB_RO( cost );
        DOC( "Costs are incurred every 'cooldown' turns." );
        SET_MEMB_RO( cooldown );

        /* === Modifiers === */
        SET_MEMB_N_RO( bodytemp_min, "bodytemp_min_btu" );
        SET_MEMB_N_RO( bodytemp_max, "bodytemp_max_btu" );
        SET_MEMB_N_RO( bodytemp_sleep, "bodytemp_sleep_btu" );

        DOC( "Pain recovery per turn from mutation." );
        SET_MEMB_RO( pain_recovery );
        DOC( "Healing per turn from mutation." );
        SET_MEMB_RO( healing_awake );
        DOC( "Healing per turn from mutation, while asleep." );
        SET_MEMB_RO( healing_resting );
        DOC( "Multiplier applied to broken limb regeneration. Normally 0.25; clamped to 0.25..1.0." );
        SET_MEMB_RO( mending_modifier );
        DOC( "Bonus HP multiplier. 1.0 doubles HP; -0.5 halves it." );
        SET_MEMB_RO( hp_modifier );
        DOC( "Secondary HP multiplier; stacks with the other one. 1.0 doubles HP; -0.5 halves it." );
        SET_MEMB_RO( hp_modifier_secondary );
        DOC( "Flat adjustment to HP." );
        SET_MEMB_RO( hp_adjustment );
        DOC( "Adjustment to Strength that doesn't affect HP." );
        SET_MEMB_RO( str_modifier );

        SET_MEMB_RO( dodge_modifier );
        SET_MEMB_RO( speed_modifier );
        SET_MEMB_RO( movecost_modifier );
        SET_MEMB_RO( movecost_flatground_modifier );
        SET_MEMB_RO( movecost_obstacle_modifier );
        SET_MEMB_RO( attackcost_modifier );
        SET_MEMB_RO( falling_damage_multiplier );
        SET_MEMB_RO( max_stamina_modifier );
        SET_MEMB_RO( weight_capacity_modifier );
        SET_MEMB_RO( hearing_modifier );
        SET_MEMB_RO( movecost_swim_modifier );
        SET_MEMB_RO( noise_modifier );
        SET_MEMB_RO( scent_modifier );
        SET_MEMB_RO( bleed_resist );

        DOC( "How quickly health (not HP) trends toward healthy_mod." );
        SET_MEMB_RO( healthy_rate );
        SET_MEMB_RO( stealth_modifier );
        SET_MEMB_RO( night_vision_range );
        SET_MEMB_RO( temperature_speed_modifier );
        SET_MEMB_RO( metabolism_modifier );
        SET_MEMB_RO( thirst_modifier );
        SET_MEMB_RO( fatigue_modifier );
        SET_MEMB_RO( fatigue_regen_modifier );
        SET_MEMB_RO( stamina_regen_modifier );

        SET_MEMB_RO( overmap_sight );
        SET_MEMB_RO( overmap_multiplier );
        SET_MEMB_RO( reading_speed_multiplier );
        SET_MEMB_RO( skill_rust_multiplier );

        SET_FX_T( name, std::string() const );
        SET_FX_T( desc, std::string() const );

        DOC( "Returns a (long) list of every mutation in the game." );
        SET_FX_T( get_all, const std::vector<mutation_branch> &() );

        // The string conversion function references this object's str_id.
        luna::set_fx( ut, sol::meta_function::to_string,
        []( const UT_CLASS & id ) -> std::string {
            return string_format( "%s[%s]", luna::detail::luna_traits<UT_CLASS>::name, id.id.c_str() );
        } );

        // Copy the return values so they can't be modified in-place.
        DOC( "Lists the primary mutation(s) needed to gain this mutation." );
        luna::set_fx( ut, "prerequisites", []( const UT_CLASS & mut ) -> std::vector<trait_id> {
            std::vector<trait_id> rv = mut.prereqs; return rv;
        } );
        DOC( "Lists the secondary mutation(s) needed to gain this mutation." );
        luna::set_fx( ut, "other_prerequisites", []( const UT_CLASS & mut ) -> std::vector<trait_id> {
            std::vector<trait_id> rv = mut.prereqs2; return rv;
        } );
        DOC( "Lists the threshold mutation(s) required to gain this mutation." );
        luna::set_fx( ut, "thresh_requirements", []( const UT_CLASS & mut ) -> std::vector<trait_id> {
            std::vector<trait_id> rv = mut.threshreq; return rv;
        } );
        DOC( "Lists the type(s) of this mutation. Mutations of a given type are mutually exclusive." );
        luna::set_fx( ut, "mutation_types", []( const UT_CLASS & mut ) -> std::set<std::string> {
            std::set<std::string> rv = mut.types; return rv;
        } );
        DOC( "Lists conflicting mutations." );
        luna::set_fx( ut, "conflicts_with", []( const UT_CLASS & mut ) -> std::vector<trait_id> {
            std::vector<trait_id> rv = mut.cancels; return rv;
        } );
        DOC( "Lists mutations that replace (e.g. evolve from) this one." );
        luna::set_fx( ut, "replaced_by", []( const UT_CLASS & mut ) -> std::vector<trait_id> {
            std::vector<trait_id> rv = mut.replacements; return rv;
        } );
        luna::set_fx( ut, "addition_mutations", []( const UT_CLASS & mut ) -> std::vector<trait_id> {
            std::vector<trait_id> rv = mut.additions; return rv;
        } );
        DOC( "Lists the categories this mutation belongs to." );
        luna::set_fx( ut, "categories", []( const UT_CLASS & mut ) -> std::vector<mutation_category_id> {
            std::vector<mutation_category_id> rv = mut.category; return rv;
        } );
        /* Would bind .flags to .trait_flags(), but json_trait_flag does not
         * seem to have definitions for comparison to itself.
         */
    }
#undef UT_CLASS // #define UT_CLASS mutation_branch
}
