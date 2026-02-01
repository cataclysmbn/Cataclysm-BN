#include "catalua_bindings.h"

#include "catalua.h"
// Thx Almantuxas
#include "catalua_bindings_utils.h"
#include "catalua_impl.h"
#include "catalua_log.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "avatar.h"
#include "creature.h"
#include "magic.h"

// IN WAITING: enchantment_id, enchantments in general
void cata::detail::reg_magic( sol::state &lua )
{
    reg_spell_type( lua );
    reg_spell_fake( lua );
    reg_spell( lua );
    reg_known_magic( lua );
}

void cata::detail::reg_spell_type( sol::state &lua )
{
#define UT_CLASS spell_type
    {
        /* NOTE: These changes are applied to the "SpellTypeRaw" Lua obj.
         * Because spell_type is bound as an ID, the actual object is
         * shoved into a 'Raw' binding.
         */
        DOC( "The 'raw' type for storing the information defining every spell in the game. It's not possible to cast directly from this type; check SpellSimple and Spell." );
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        // The string conversion function references this object's str_id.
        luna::set_fx( ut, sol::meta_function::to_string,
        []( const UT_CLASS & id ) -> std::string { return string_format( "%s[%s]", luna::detail::luna_traits<UT_CLASS>::name, id.id.c_str() ); } );

        SET_MEMB_RO( id );
        DOC( "The name of the primary effect this spell will enact." );
        SET_MEMB_RO( effect_name );
        DOC( "Specifics about the effect this spell will enact." );
        SET_MEMB_RO( effect_str );

        // Currently unclear on how to implement 'field'; it's std::optional,
        // while we want to return sol::optional.
        SET_MEMB_RO( field_chance );
        SET_MEMB_RO( min_field_intensity );
        SET_MEMB_RO( field_intensity_increment );
        SET_MEMB_RO( max_field_intensity );
        SET_MEMB_RO( field_intensity_variance );

        SET_MEMB_RO( min_damage );
        SET_MEMB_RO( damage_increment );
        SET_MEMB_RO( max_damage );

        SET_MEMB_RO( min_range );
        SET_MEMB_RO( range_increment );
        SET_MEMB_RO( max_range );

        SET_MEMB_RO( min_aoe );
        SET_MEMB_RO( aoe_increment );
        SET_MEMB_RO( max_aoe );

        SET_MEMB_RO( min_dot );
        SET_MEMB_RO( dot_increment );
        SET_MEMB_RO( max_dot );

        SET_MEMB_RO( min_duration );
        SET_MEMB_RO( duration_increment );
        SET_MEMB_RO( max_duration );

        // Ignoring pierce damage for now, amongst other things.

        SET_MEMB_RO( base_energy_cost );
        SET_MEMB_RO( energy_increment );
        SET_MEMB_RO( final_energy_cost );

        SET_MEMB_RO( difficulty );
        SET_MEMB_RO( max_level );
        SET_MEMB_RO( base_casting_time );
        SET_MEMB_RO( casting_time_increment );
        SET_MEMB_RO( final_casting_time );

        DOC( "Other spells cast by this spell." );
        luna::set_fx( ut, "additional_spells",
        []( const UT_CLASS & spid ) -> std::vector<fake_spell> { std::vector<fake_spell> rv = spid.additional_spells; return rv; } );

        DOC( "Returns a (long) list of every spell in the game." );
        SET_FX_T( get_all, const std::vector<spell_type> &() );

    }
#undef UT_CLASS // #define UT_CLASS spell_type
}

void cata::detail::reg_spell_fake( sol::state &lua )
{
#define UT_CLASS fake_spell
    {
        DOC( "The type for basic spells. If you don't need to track XP from casting (e.g., if a spell is intended to be cast by anything *other than* a player), this is likely the appropriate type. Otherwise, see the Spell type." );
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::constructors <
            UT_CLASS( spell_id sp, bool hit_self ),
            UT_CLASS( spell_id sp, bool hit_self, int max_level )
            > ()
        );

        luna::set_fx( ut, sol::meta_function::to_string,
        []( const UT_CLASS & id ) -> std::string { return string_format( "%s[%s]", luna::detail::luna_traits<UT_CLASS>::name, id.id.c_str() ); } );

        SET_MEMB_RO( id );

        DOC( "Returns the defined maximum level of this SpellSimple instance, if defined. Otherwise, returns 0." );
        luna::set_fx( ut, "max_level", []( UT_CLASS & sp ) -> int { return sp.max_level.has_value() ? *sp.max_level : 0; } );

        // Perhaps this should be writeable?
        SET_MEMB_RO( level );
        DOC( "Whether or not the target point is *locked* to the source's location." );
        SET_MEMB_N_RO( self, "force_target_source" );
        DOC( "Used for enchantments; the spell's *chance* to trigger every turn." );
        SET_MEMB_RO( trigger_once_in );

        // TODO: Support min_level_override
        luna::set_fx( ut, "cast",
                      []( UT_CLASS & sp,
                          Creature & source,
                          const tripoint & target,
                          sol::optional<int> min_lvl_override )
        {
            int mlo = min_lvl_override.has_value() ? *min_lvl_override : 0;
            sp.get_spell( mlo ).cast_all_effects( source, target );
        }
                    );

        DOC( "Static function: Creates and immediately casts a SimpleSpell, then returns the new spell for potential reuse. If the given tripoint is the player's location, the spell will be locked to the player. (This does not necessarily cause friendly fire!) If an integer is specified, the spell will be cast at that level." );
        luna::set_fx( ut, "prompt_cast",
                      []( spell_id spid,
                          tripoint & target,
                          sol::optional<int> level ) -> fake_spell
        {
            // This will be our return value, as well as the spell we cast.
            fake_spell sp;
            /* Without a specified Creature, we assume the player is the
             * source.
             * I'd prefer to call gapi.get_avatar, but this will do for now.
             */
            avatar &avvy = get_avatar();
            // If target is avatar's location, assume we want to hit self
            bool hit_self = avvy.pos() == target;
            sp = fake_spell( spid, hit_self );

            // If a level is given, forcefully clamp to that level.
            if( level.has_value() )
            {
                sp.level = *level;
                sp.max_level = *level;
            }

            // Now that the spell is configured, we cast it as usual...
            sp.get_spell().cast_all_effects( avvy, target );
            // ...and return the spell we made for reuse.
            return sp;
        }
                    );
    }
#undef UT_CLASS // #define UT_CLASS fake_spell
}

void cata::detail::reg_spell( sol::state &lua )
{
#define UT_CLASS spell
    {
        /* NOTE: This is the actual 'spell type', which is fully-featured and
         * intended for use with players. As such, it tracks things like xp
         * and level, which you may not care about.
         * This functionality isn't presently important, particularly since
         * there are currently no methods to access the spells in a player's
         * spellbook (known_magic).
         */
        DOC( "The class used for spells that *a player* knows, casts, and gains experience for using. If a given spell is not supposed to be directly cast by a player, consider using SpellSimple instead." );
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::constructors <
            UT_CLASS( spell_id, int )
            > ()
        );

        // Lets us grab the ID from the object.
        SET_MEMB_N_RO( type, "id" );

        SET_FX_T( xp, int() const );
        SET_FX_T( gain_exp, void( int ) );
        SET_FX_T( set_exp, void( int ) );
        SET_FX_T( gain_levels, void( int ) );
        SET_FX_T( set_level, void( int ) );
        SET_FX_T( get_level, int() const );

        SET_FX_T( name, std::string() const );
        SET_FX_N_T( description, "desc", std::string() const );

        // Present priority is basic functionality.

        DOC( "Cast this spell, as well as any sub-spells." );
        SET_FX_N_T( cast_all_effects, "cast", void( Creature & source, const tripoint & target ) const );
        DOC( "Cast *only* this spell's main effects. Generally, cast() should be used instead." );
        SET_FX_N_T( cast_spell_effect, "cast_single_effect", void( Creature & source, const tripoint & target ) const );
    }
#undef UT_CLASS // #define UT_CLASS spell
}

void cata::detail::reg_known_magic( sol::state &lua )
{
#define UT_CLASS known_magic
    {
        DOC( "Represents a character's spellbook and mana pool. Manages all spells known by a character, their mana, and spell learning/forgetting." );
        sol::usertype<UT_CLASS> ut =
        luna::new_usertype<UT_CLASS>(
            lua,
            luna::no_bases,
            luna::no_constructor
        );

        DOC( "Check if the character knows a specific spell by spell_id." );
        luna::set_fx( ut, "knows_spell",
                      sol::overload(
        []( const UT_CLASS & km, const spell_id & sp ) -> bool { return km.knows_spell( sp ); },
        []( const UT_CLASS & km ) -> bool { return km.knows_spell(); } ) );

        DOC( "Learn a new spell. Requires a Character reference and spell_id. Optional force(boolean) parameter bypasses restrictions." );
        luna::set_fx( ut, "learn_spell",
                      []( UT_CLASS & km, const spell_id & sp, Character & guy, sol::optional<bool> force )
        {
            km.learn_spell( sp, guy, force.value_or( false ) );
        } );

        DOC( "Forget a known spell by spell_id." );
        SET_FX_T( forget_spell, void( const spell_id & ) );

        DOC( "Check if the character can learn a specific spell, considering traits and other restrictions." );
        SET_FX_T( can_learn_spell, bool( const Character &, const spell_id & ) const );

        DOC( "Get a reference to a known spell for editing. Returns the spell associated with the given spell_id." );
        SET_FX_T( get_spell, spell & ( const spell_id & ) );

        DOC( "Get all known spells as a vector of spell pointers." );
        SET_FX_T( get_spells, std::vector<spell *>() );

        DOC( "Get a vector of all known spell IDs." );
        SET_FX_T( spells, std::vector<spell_id>() const );

        DOC( "Get the current available mana." );
        SET_FX_T( available_mana, int() const );

        DOC( "Get the maximum mana for the given character." );
        SET_FX_T( max_mana, int( const Character & ) const );

        DOC( "Set the current mana to a specific value." );
        SET_FX_T( set_mana, void( int ) );

        DOC( "Modify the current mana by adding or subtracting an amount." );
        SET_FX_T( mod_mana, void( const Character &, int ) );

        DOC( "Get the mana regeneration rate in units per turn for the given character." );
        SET_FX_T( mana_regen_rate, double( const Character & ) const );

        DOC( "Check if the character has enough energy (of the appropriate type) to cast the given spell." );
        SET_FX_T( has_enough_energy, bool( const Character &, spell & ) const );

        DOC( "Calculate the time in moves required for the character to memorize/learn a spell." );
        luna::set_fx( ut, "time_to_learn_spell",
        []( const UT_CLASS & km, const Character & guy, const spell_id & sp ) -> int { return km.time_to_learn_spell( guy, sp ); } );

        DOC( "Whether casting ignores all distractions. Can be read and written." );
        SET_MEMB( casting_ignore );

    }
#undef UT_CLASS // #define UT_CLASS known_magic
}
