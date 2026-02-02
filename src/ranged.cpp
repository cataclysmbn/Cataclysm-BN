#include "ranged.h"

#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "activity_actor_definitions.h"
#include "avatar.h"
#include "ballistics.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "catalua_hooks.h"
#include "catalua_sol.h"
#include "character.h"
#include "character_functions.h"
#include "color.h"
#include "creature.h"
#include "cursesdef.h"
#include "damage.h"
#include "debug.h"
#include "dispersion.h"
#include "enums.h"
#include "event.h"
#include "event_bus.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "gun_mode.h"
#include "input.h"
#include "item.h"
#include "item_functions.h"
#include "itype.h"
#include "line.h"
#include "magic.h"
#include "map.h"
#include "material.h"
#include "math_defines.h"
#include "messages.h"
#include "monster.h"
#include "morale_types.h"
#include "mtype.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "panels.h"
#include "player.h"
#include "player_activity.h"
#include "point.h"
#include "projectile.h"
#include "rng.h"
#include "skill.h"
#include "sounds.h"
#include "string_formatter.h"
#include "string_id.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "ui_manager.h"
#include "units.h"
#include "units_angle.h"
#include "units_utility.h"
#include "value_ptr.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

struct ammo_effect;

using ammo_effect_str_id = string_id<ammo_effect>;

static const weapon_category_id weapon_cat_GRENADE_LAUNCHERS( "GRENADE_LAUNCHERS" );
static const weapon_category_id weapon_cat_MAGNETIC( "MAGNETIC" );
static const weapon_category_id weapon_cat_PNEUMATIC( "PNEUMATIC" );
static const weapon_category_id weapon_cat_FLAMETHROWERS( "FLAMETHROWERS" );
static const weapon_category_id weapon_cat_SPRAY_GUNS( "SPRAY_GUNS" );
static const weapon_category_id weapon_cat_WATER_CANNONS( "WATER_CANNONS" );
static const weapon_category_id weapon_cat_ROCKET_LAUNCHERS( "ROCKET_LAUNCHERS" );
static const weapon_category_id weapon_cat_ELASTIC( "ELASTIC" );
static const weapon_category_id weapon_cat_S_XBOWS( "S_XBOWS" );
static const weapon_category_id weapon_cat_M_XBOWS( "M_XBOWS" );
static const weapon_category_id weapon_cat_ENERGY_WEAPONS( "ENERGY_WEAPONS" );

static const ammo_effect_str_id ammo_effect_ACT_ON_RANGED_HIT( "ACT_ON_RANGED_HIT" );
static const ammo_effect_str_id ammo_effect_BLACKPOWDER( "BLACKPOWDER" );
static const ammo_effect_str_id ammo_effect_BOUNCE( "BOUNCE" );
static const ammo_effect_str_id ammo_effect_BURST( "BURST" );
static const ammo_effect_str_id ammo_effect_CUSTOM_EXPLOSION( "CUSTOM_EXPLOSION" );
static const ammo_effect_str_id ammo_effect_EMP( "EMP" );
static const ammo_effect_str_id ammo_effect_EXPLOSIVE( "EXPLOSIVE" );
static const ammo_effect_str_id ammo_effect_HEAVY_HIT( "HEAVY_HIT" );
static const ammo_effect_str_id ammo_effect_IGNITE( "IGNITE" );
static const ammo_effect_str_id ammo_effect_LASER( "LASER" );
static const ammo_effect_str_id ammo_effect_LIGHTNING( "LIGHTNING" );
static const ammo_effect_str_id ammo_effect_NO_CRIT( "NO_CRIT" );
static const ammo_effect_str_id ammo_effect_NO_DAMAGE( "NO_DAMAGE" );
static const ammo_effect_str_id ammo_effect_NO_EMBED( "NO_EMBED" );
static const ammo_effect_str_id ammo_effect_NO_ITEM_DAMAGE( "NO_ITEM_DAMAGE" );
static const ammo_effect_str_id ammo_effect_NON_FOULING( "NON_FOULING" );
static const ammo_effect_str_id ammo_effect_PLASMA( "PLASMA" );
static const ammo_effect_str_id ammo_effect_RECYCLED( "RECYCLED" );
static const ammo_effect_str_id ammo_effect_SHATTER_SELF( "SHATTER_SELF" );
static const ammo_effect_str_id ammo_effect_SHOT( "SHOT" );
static const ammo_effect_str_id ammo_effect_TANGLE( "TANGLE" );
static const ammo_effect_str_id ammo_effect_WIDE( "WIDE" );
static const ammo_effect_str_id ammo_effect_THROWN( "THROWN" );

static const efftype_id effect_downed( "downed" );
static const efftype_id effect_hit_by_player( "hit_by_player" );
static const efftype_id effect_on_roof( "on_roof" );

static const itype_id itype_UPS( "UPS" );

static const trap_str_id tr_practice_target( "tr_practice_target" );

static const fault_id fault_gun_blackpowder( "fault_gun_blackpowder" );
static const fault_id fault_gun_chamber_spent( "fault_gun_chamber_spent" );
static const fault_id fault_gun_dirt( "fault_gun_dirt" );
static const fault_id fault_gun_unlubricated( "fault_gun_unlubricated" );

static const skill_id skill_driving( "driving" );
static const skill_id skill_gun( "gun" );
static const skill_id skill_launcher( "launcher" );
static const skill_id skill_pistol( "pistol" );
static const skill_id skill_rifle( "rifle" );
static const skill_id skill_throw( "throw" );

static const bionic_id bio_railgun( "bio_railgun" );
static const bionic_id bio_targeting( "bio_targeting" );
static const bionic_id bio_ups( "bio_ups" );

static const trait_id trait_PYROMANIA( "PYROMANIA" );
static const trait_id trait_NORANGEDCRIT( "NO_RANGED_CRIT" );

// not to confuse with item flags (json_flag)
static const std::string flag_SHOOT_ME( "SHOOT_ME" );

// Maximum duration of aim-and-fire loop, in turns
static constexpr int AIF_DURATION_LIMIT = 10;

static projectile make_gun_projectile( const item &gun );
static void cycle_action( item &weap, const tripoint &pos );
static dispersion_sources calculate_dispersion( const map &m, const Character &who, const item &gun,
        int at_recoil, bool burst );

namespace
{

/// more generic version of `item::gunmod_find`
auto gunmod_find_with(
    item &it, std::function < auto( const item * ) -> bool > pred
) -> item *
{
    std::vector<item *> gunmods = it.gunmods();
    auto res = std::ranges::find_if( gunmods, pred );
    return res != gunmods.end() ? *res : nullptr;
}

} // namespace

class target_ui
{
    public:
        /* None of the public members (except range) should be modified during execution */

        enum class TargetMode : int {
            Fire,
            Throw,
            ThrowBlind,
            Turrets,
            TurretManual,
            Reach,
            Spell,
            Shape
        };

        // Avatar
        avatar *you;
        // Interface mode
        TargetMode mode = TargetMode::Fire;
        // Weapon being fired/thrown
        item *relevant = nullptr;
        // Cached selection range from player's position
        int range = 0;
        // Turret being manually fired
        turret_data *turret = nullptr;
        // Turrets being fired (via vehicle controls)
        const std::vector<vehicle_part *> *vturrets = nullptr;
        // Vehicle that turrets belong to
        vehicle *veh = nullptr;
        // Spell being cast
        spell *casting = nullptr;
        // Spell cannot fail
        bool no_fail = false;
        // Spell does not require mana
        bool no_mana = false;
        // Relevant activity
        aim_activity_actor *activity = nullptr;
        // Generator of AoE shapes
        std::optional<shape_factory> shape_gen;

        // Initialize UI and run the event loop
        target_handler::trajectory run();

    private:
        enum class ExitCode : int {
            Abort,
            Fire,
            Timeout,
            Reload
        };

        enum class Status : int {
            Good, // All UI elements are enabled
            BadTarget, // Bad 'dst' selected; forbid aiming/firing
            OutOfAmmo, // Selected gun mode is out of ammo; forbid moving cursor,aiming and firing
            OutOfRange // Selected target is out of range of current gun mode; forbid aiming/firing
        };

        // Ui status (affects which UI controls are temporarily disabled)
        Status status = Status::Good;

        // Cached current ammo to display
        const itype *ammo = nullptr;
        // Current trajectory
        std::vector<tripoint> traj;
        // Aiming source (player's position)
        tripoint src;
        // Aiming destination (cursor position)
        // Use set_cursor_pos() to modify
        tripoint dst;
        // Creature currently under cursor. nullptr if aiming at empty tile,
        // yourself or a creature you cannot see
        Creature *dst_critter = nullptr;
        // List of visible hostile targets
        std::vector<Creature *> targets;

        // 'true' if map has z levels and 3D fov is on
        bool allow_zlevel_shift = false;
        // Snap camera to cursor. Can be permanently toggled in settings
        // or temporarily in this window
        bool snap_to_target = false;
        // If true, LEVEL_UP, LEVEL_DOWN and directional keys
        // responsible for moving cursor will shift view instead.
        bool shifting_view = false;

        // Compact layout
        bool compact = false;
        // Tiny layout - when extremely short on space
        bool tiny = false;
        // Narrow layout - to keep in theme with
        // "compact" and "labels-narrow" sidebar styles.
        bool narrow = false;
        // Window
        catacurses::window w_target;
        // Input context
        input_context ctxt;

        /* These members are relevant for TargetMode::Fire */
        // Weapon sight dispersion
        int sight_dispersion = 0;
        // List of available weapon aim types
        std::vector<ranged::aim_type> aim_types;
        // Currently selected aim mode
        std::vector<ranged::aim_type>::iterator aim_mode;
        // 'Recoil' value the player will reach if they
        // start aiming at cursor position. Equals player's
        // 'recoil' while they are actively spending moves to aim,
        // but increases the further away the new aim point will be
        // relative to the current one.
        double predicted_recoil = 0;

        // For AOE spells, list of tiles affected by the spell
        // relevant for TargetMode::Spell
        std::set<tripoint> spell_aoe;

        // For shaped attacks, we want both points and coverage
        std::map<tripoint, double> shape_coverage;

        // Represents a turret and a straight line from that turret to target
        struct turret_with_lof {
            vehicle_part *turret;
            std::vector<tripoint> line;
        };

        // List of vehicle turrets in range (out of those listed in 'vturrets')
        std::vector<turret_with_lof> turrets_in_range;

        // If true, draws turret lines
        // relevant for TargetMode::Turrets
        bool draw_turret_lines = false;

        // Create window and set up input context
        void init_window_and_input();

        // Handle input related to cursor movement.
        // Returns 'true' if action was recognized and processed.
        // 'skip_redraw' is set to 'true' if there is no need to redraw the UI.
        bool handle_cursor_movement( const std::string &action, bool &skip_redraw );

        // Set cursor position. If new position is out of range,
        // selects closest position in range.
        // Returns 'false' if cursor position did not change
        bool set_cursor_pos( const tripoint &new_pos );

        // Called when range/ammo changes (or may have changed)
        void on_range_ammo_changed();

        // Updates 'targets' for current range
        void update_target_list();

        // Choose where to position the cursor when opening the ui
        tripoint choose_initial_target();

        /**
         * Try to re-acquire target for aim-and-fire.
         * @param critter whether were aiming at a critter, or a tile
         * @param new_dst where to move aim cursor (if e.g. critter moved)
         * @returns true on success
         */
        bool try_reacquire_target( bool critter, tripoint &new_dst );

        // Update 'status' variable
        void update_status();

        // Calculates distance from 'src'. For consistency, prefer using this over rl_dist.
        int dist_fn( const tripoint &p );

        // Set creature (or tile) under cursor as player's last target
        void set_last_target();

        // Prompts player to confirm attack on neutral NPC
        // Returns 'true' if attack should proceed
        bool confirm_non_enemy_target();

        // Prompts player to re-confirm an ongoing attack if
        // a non-hostile NPC / friendly creatures enters line of fire.
        // Returns 'true' if attack should proceed
        bool prompt_friendlies_in_lof();

        // List friendly creatures currently occupying line of fire.
        std::vector<weak_ptr_fast<Creature>> list_friendlies_in_lof();

        // Toggle snap-to-target
        void toggle_snap_to_target();

        // Cycle targets. 'direction' is either 1 or -1
        void cycle_targets( int direction );

        // Set new view offset. Updates map cache if necessary
        void set_view_offset( const tripoint &new_offset );

        // Updates 'turrets_in_range'
        void update_turrets_in_range();

        // Recalculate 'recoil' penalty. This should be called if
        // avatar's 'recoil' value has been modified
        // Relevant for TargetMode::Fire
        void recalc_aim_turning_penalty();

        // Apply penalty to avatar's 'recoil' value based on
        // how much they moved their aim point.
        // Relevant for TargetMode::Fire
        void apply_aim_turning_penalty();

        // Switch firing mode.
        void action_switch_mode();

        // Ensure we're using ranged gun mode.
        void ensure_ranged_gun_mode();

        // Update range & ammo from current gun mode
        void update_ammo_range_from_gun_mode();

        // Switch ammo. Returns 'false' if requires a reloading UI.
        bool action_switch_ammo();

        // Aim for 10 turns. Returns 'false' if ran out of moves
        bool action_aim();

        // Aim and shoot. Returns 'false' if ran out of moves
        bool action_aim_and_shoot( const std::string &action );

        // Draw UI-specific terrain overlays
        void draw_terrain_overlay();

        // Draw aiming window
        void draw_ui_window();

        // Generate ui window title
        std::string uitext_title();

        // Generate flavor text for 'Fire!' key
        std::string uitext_fire();

        void draw_window_title();
        void draw_help_notice();

        // Draw list of available controls at the bottom of the window.
        // text_y - first free line counting from the top
        void draw_controls_list( int text_y );

        void panel_cursor_info( int &text_y );
        void panel_gun_info( int &text_y );
        void panel_recoil( int &text_y );
        void panel_spell_info( int &text_y );
        void panel_target_info( int &text_y, bool fill_with_blank_if_no_target );
        void panel_fire_mode_aim( int &text_y );
        void panel_turret_list( int &text_y );

        // On-selected-as-target checks that act as if they are on-hit checks.
        // `harmful` is `false` if using a non-damaging spell
        void on_target_accepted( bool harmful );
};

target_handler::trajectory target_handler::mode_fire( avatar &you, aim_activity_actor &activity )
{
    target_ui ui = target_ui();
    ui.you = &you;
    ui.mode = target_ui::TargetMode::Fire;
    ui.activity = &activity;
    ui.relevant = activity.get_weapon();

    return ui.run();
}

target_handler::trajectory target_handler::mode_throw( avatar &you, item &relevant,
        bool blind_throwing )
{
    target_ui ui = target_ui();
    ui.you = &you;
    ui.mode = blind_throwing ? target_ui::TargetMode::ThrowBlind : target_ui::TargetMode::Throw;
    ui.relevant = &relevant;
    ui.range = you.throw_range( relevant );

    restore_on_out_of_scope<tripoint> view_offset_prev( you.view_offset );
    return ui.run();
}

target_handler::trajectory target_handler::mode_reach( avatar &you, item &weapon )
{
    target_ui ui = target_ui();
    ui.you = &you;
    ui.mode = target_ui::TargetMode::Reach;
    ui.relevant = &weapon;
    ui.range = weapon.reach_range( you );

    restore_on_out_of_scope<tripoint> view_offset_prev( you.view_offset );
    return ui.run();
}

target_handler::trajectory target_handler::mode_turret_manual( avatar &you, turret_data &turret )
{
    target_ui ui = target_ui();
    ui.you = &you;
    ui.mode = target_ui::TargetMode::TurretManual;
    ui.turret = &turret;
    ui.relevant = &turret.base();

    restore_on_out_of_scope<tripoint> view_offset_prev( you.view_offset );
    return ui.run();
}

target_handler::trajectory target_handler::mode_turrets( avatar &you, vehicle &veh,
        const std::vector<vehicle_part *> &turrets )
{
    // Find radius of a circle centered at u encompassing all points turrets can aim at
    // FIXME: this calculation is fine for square distances, but results in an underestimation
    //        when used with real circles
    int range_total = 0;
    for( vehicle_part *t : turrets ) {
        int range = veh.turret_query( *t ).range();
        tripoint pos = veh.global_part_pos3( *t );

        int res = 0;
        res = std::max( res, rl_dist( you.pos(), pos + point( range, 0 ) ) );
        res = std::max( res, rl_dist( you.pos(), pos + point( -range, 0 ) ) );
        res = std::max( res, rl_dist( you.pos(), pos + point( 0, range ) ) );
        res = std::max( res, rl_dist( you.pos(), pos + point( 0, -range ) ) );
        range_total = std::max( range_total, res );
    }

    target_ui ui = target_ui();
    ui.you = &you;
    ui.mode = target_ui::TargetMode::Turrets;
    ui.veh = &veh;
    ui.vturrets = &turrets;
    ui.range = range_total;

    restore_on_out_of_scope<tripoint> view_offset_prev( you.view_offset );
    return ui.run();
}

target_handler::trajectory target_handler::mode_spell( avatar &you, spell &casting, bool no_fail,
        bool no_mana )
{
    target_ui ui = target_ui();
    ui.you = &you;
    ui.mode = target_ui::TargetMode::Spell;
    ui.casting = &casting;
    ui.range = casting.range();
    ui.no_fail = no_fail;
    ui.no_mana = no_mana;

    restore_on_out_of_scope<tripoint> view_offset_prev( you.view_offset );
    return ui.run();
}

target_handler::trajectory target_handler::mode_shaped( avatar &you, shape_factory &shape_fac,
        aim_activity_actor &activity )
{
    target_ui ui = target_ui();
    ui.you = &you;
    ui.mode = target_ui::TargetMode::Shape;
    ui.shape_gen = shape_fac;
    ui.range = shape_fac.get_range();
    ui.activity = &activity;
    ui.relevant = activity.get_weapon();

    return ui.run();
}

static double occupied_tile_fraction( creature_size target_size )
{
    switch( target_size ) {
        case creature_size::tiny:
            return 0.1;
        case creature_size::small:
            return 0.25;
        case creature_size::medium:
            return 0.5;
        case creature_size::large:
            return 0.75;
        case creature_size::huge:
            return 1.0;
        default:
            break;
    }

    return 0.5;
}

double Creature::ranged_target_size() const
{
    if( const_cast<Creature &>( *this ).uncanny_dodge() ) {
        return 0.0;
    }
    bool is_crouched = false;
    if( Character *ch = const_cast<Creature &>( *this ).as_character() ) {
        if( ch->movement_mode_is( CMM_CROUCH ) ) {
            is_crouched = true;
        }
    }
    if( has_flag( MF_HARDTOSHOOT ) || is_crouched ) {
        switch( get_size() ) {
            case creature_size::tiny:
                // We can't be smaller than tiny, but we can make the hit rate lower.
                return 0.05;
            case creature_size::small:
                return occupied_tile_fraction( creature_size::tiny );
            case creature_size::medium:
                return occupied_tile_fraction( creature_size::small );
            case creature_size::large:
                return occupied_tile_fraction( creature_size::medium );
            case creature_size::huge:
                return occupied_tile_fraction( creature_size::large );
            default:
                break;
        }
    }
    return occupied_tile_fraction( get_size() );
}

int range_with_even_chance_of_good_hit( int dispersion )
{
    int even_chance_range = 0;
    while( static_cast<unsigned>( even_chance_range ) <
           Creature::dispersion_for_even_chance_of_good_hit.size() &&
           dispersion < Creature::dispersion_for_even_chance_of_good_hit[ even_chance_range ] ) {
        even_chance_range++;
    }
    return even_chance_range;
}

int ranged::gun_engagement_moves( const Character &who, const item &gun, int target, int start )
{
    int mv = 0;
    double penalty = start;

    while( penalty > target ) {
        double adj = ranged::aim_per_move( who, gun, penalty );
        if( adj <= 0 ) {
            break;
        }
        penalty -= adj;
        mv++;
    }

    return mv;
}

bool ranged::handle_gun_damage( Character &shooter, item &it )
{
    // Below item (maximum dirt possible) should be greater than or equal to dirt range in item_group.cpp. Also keep in mind that monster drops can have specific ranges and these should be below the max!
    const double dirt_max_dbl = 10000;
    if( !it.is_gun() ) {
        debugmsg( "Tried to handle_gun_damage of a non-gun %s", it.tname() );
        return false;
    }

    int dirt = it.get_var( "dirt", 0 );
    int dirtadder = 0;
    double dirt_dbl = static_cast<double>( dirt );
    if( it.faults.contains( fault_gun_chamber_spent ) ) {
        return false;
    }

    const auto &curammo_effects = it.ammo_effects();
    // If using an aux magazine for a gun, make the parent gun accumulate and track fouling instead of the gunmod that's doing the firing.
    item &parent = ( it.parent_item() != nullptr &&
                     it.has_flag( flag_USE_PARENT_GUN ) ) ? *it.parent_item() : it;
    const islot_gun &firing = *parent.type->gun;
    // misfire chance based on dirt accumulation. Formula is designed to make chance of jam highly unlikely at low dirt levels, but levels increase geometrically as the dirt level reaches max (10,000). The number used is just a figure I found reasonable after plugging the number into excel and changing it until the probability made sense at high, medium, and low levels of dirt.
    if( !parent.has_flag( flag_NEVER_JAMS ) &&
        x_in_y( dirt_dbl * dirt_dbl * dirt_dbl,
                1000000000000.0 ) ) {
        shooter.add_msg_player_or_npc(
            _( "Your %s misfires with a muffled click!" ),
            _( "<npcname>'s %s misfires with a muffled click!" ),
            it.tname() );
        // at high dirt levels the chance to misfire gets to significant levels. 10,000 is max and 7800 is quite high so above that the player gets some relief in the form of exchanging time for some dirt reduction. Basically jiggling the parts loose to remove some dirt and get a few more shots out.
        if( dirt_dbl >
            7800 ) {
            shooter.add_msg_player_or_npc(
                _( "Perhaps taking the ammo out of your %s and reloading will help." ),
                _( "Perhaps taking the ammo out of <npcname>'s %s and reloading will help." ),
                it.tname() );
        }
        return false;
    }

    // Here we check if we're underwater and whether we should misfire.
    // As a result this causes no damage to the firearm, note that some guns are waterproof
    // and so are immune to this effect, note also that WATERPROOF_GUN status does not
    // mean the gun will actually be accurate underwater.
    int effective_durability = firing.durability;
    if( shooter.is_underwater() && !parent.has_flag( flag_WATERPROOF_GUN ) &&
        one_in( effective_durability ) ) {
        shooter.add_msg_player_or_npc( _( "Your %s misfires with a wet click!" ),
                                       _( "<npcname>'s %s misfires with a wet click!" ),
                                       it.tname() );
        return false;
        // Here we check for a chance for the weapon to suffer a mechanical malfunction.
        // Note that some weapons never jam up 'NEVER_JAMS' and thus are immune to this
        // effect as current guns have a durability between 5 and 9 this results in
        // a chance of mechanical failure between 1/(64*3) and 1/(1024*3) on any given shot.
        // the malfunction can't cause damage
    } else if( one_in( ( 2 << effective_durability ) * 3 ) && !it.has_flag( flag_NEVER_JAMS ) ) {
        shooter.add_msg_player_or_npc( _( "Your %s malfunctions!" ),
                                       _( "<npcname>'s %s malfunctions!" ),
                                       it.tname() );
        return false;
        // Here we check for a chance for the weapon to suffer a misfire due to
        // using player-made 'RECYCLED' bullets. Note that not all forms of
        // player-made ammunition have this effect.
    } else if( curammo_effects.contains( ammo_effect_RECYCLED ) && one_in( 256 ) ) {
        shooter.add_msg_player_or_npc( _( "Your %s misfires with a muffled click!" ),
                                       _( "<npcname>'s %s misfires with a muffled click!" ),
                                       it.tname() );
        return false;
        // Here we check for a chance for attached mods to get damaged if they are flagged as 'CONSUMABLE'.
        // This is mostly for crappy handmade expedient stuff  or things that rarely receive damage during normal usage.
        // Default chance is 1/10000 unless set via json, damage is proportional to caliber(see below).
        // Can be toned down with 'consume_divisor.'

    } else if( parent.has_flag( flag_CONSUMABLE ) && !curammo_effects.contains( ammo_effect_LASER ) &&
               !curammo_effects.contains( ammo_effect_PLASMA ) && !curammo_effects.contains( ammo_effect_EMP ) ) {
        int uncork = ( ( 10 * it.ammo_data()->ammo->loudness )
                       + ( it.ammo_data()->ammo->recoil / 2 ) ) / 100;
        uncork = std::pow( uncork, 3 ) * 6.5;
        for( auto mod : parent.gunmods() ) {
            if( mod->has_flag( flag_CONSUMABLE ) ) {
                int dmgamt = uncork / mod->type->gunmod->consume_divisor;
                int modconsume = mod->type->gunmod->consume_chance;
                int initstate = parent.damage();
                // fuzz damage if it's small
                if( dmgamt < 1000 ) {
                    dmgamt = rng( dmgamt, dmgamt + 200 );
                    // ignore damage if inconsequential.
                }
                if( dmgamt < 800 ) {
                    dmgamt = 0;
                }
                if( one_in( modconsume ) ) {
                    if( mod->mod_damage( dmgamt ) ) {
                        shooter.add_msg_player_or_npc( m_bad, _( "Your attached %s is destroyed by your shot!" ),
                                                       _( "<npcname>'s attached %s is destroyed by their shot!" ),
                                                       mod->tname() );
                        mod->detach();
                    } else if( parent.damage() > initstate ) {
                        shooter.add_msg_player_or_npc( m_bad, _( "Your attached %s is damaged by your shot!" ),
                                                       _( "<npcname>'s %s is damaged by their shot!" ),
                                                       mod->tname() );
                    }
                }
            }
        }
    }
    if( parent.has_fault( fault_gun_unlubricated ) &&
        x_in_y( dirt_dbl, dirt_max_dbl ) ) {
        shooter.add_msg_player_or_npc( m_bad, _( "Your %s emits a grimace-inducing screech!" ),
                                       _( "<npcname>'s %s emits a grimace-inducing screech!" ),
                                       it.tname() );
        parent.inc_damage();
    }
    if( ( ( !curammo_effects.contains( ammo_effect_NON_FOULING ) &&
            !parent.has_flag( flag_NON_FOULING ) ) ||
          ( parent.has_fault( fault_gun_unlubricated ) ) ) &&
        !parent.has_flag( flag_PRIMITIVE_RANGED_WEAPON ) ) {
        if( curammo_effects.contains( ammo_effect_BLACKPOWDER ) ||
            parent.has_fault( fault_gun_unlubricated ) ) {
            if( ( ( it.ammo_data()->ammo->recoil < firing.min_cycle_recoil ) ||
                  ( parent.has_fault( fault_gun_unlubricated ) && one_in( 16 ) ) ) &&
                parent.faults_potential().contains( fault_gun_chamber_spent ) ) {
                shooter.add_msg_player_or_npc( m_bad, _( "Your %s fails to cycle!" ),
                                               _( "<npcname>'s %s fails to cycle!" ),
                                               it.tname() );
                parent.faults.insert( fault_gun_chamber_spent );
                // Don't return false in this case; this shot happens, follow-up ones won't.
            }
        }
        // These are the dirtying/fouling mechanics
        if( !curammo_effects.contains( ammo_effect_NON_FOULING ) && !parent.has_flag( flag_NON_FOULING ) ) {
            if( dirt < static_cast<int>( dirt_max_dbl ) ) {
                dirtadder = curammo_effects.count( ammo_effect_BLACKPOWDER ) * ( 200 -
                            ( firing.blackpowder_tolerance *
                              2 ) );
                // dirtadder is the dirt-increasing number for shots fired with gunpowder-based ammo. Usually dirt level increases by 1, unless it's blackpowder, in which case it increases by a higher number, but there is a reduction for blackpowder resistance of a weapon.
                if( dirtadder < 0 ) {
                    dirtadder = 0;
                }
                // in addition to increasing dirt level faster, regular gunpowder fouling is also capped at 7,150, not 10,000. So firing with regular gunpowder can never make the gun quite as bad as firing it with black gunpowder. At 7,150 the chance to jam is significantly lower (though still significant) than it is at 10,000, the absolute cap.
                if( curammo_effects.contains( ammo_effect_BLACKPOWDER ) ||
                    dirt < 7150 ) {
                    parent.set_var( "dirt", std::min( static_cast<int>( dirt_max_dbl ), dirt + dirtadder + 1 ) );
                }
            }
            dirt = parent.get_var( "dirt", 0 );
            dirt_dbl = static_cast<double>( dirt );
            if( dirt > 0 && !parent.faults.contains( fault_gun_blackpowder ) ) {
                parent.faults.insert( fault_gun_dirt );
            }
            if( dirt > 0 && curammo_effects.contains( ammo_effect_BLACKPOWDER ) ) {
                parent.faults.erase( fault_gun_dirt );
                parent.faults.insert( fault_gun_blackpowder );
            }
            // end fouling mechanics
        }
    }
    // chance to damage gun due to high levels of dirt. Very unlikely, especially at lower levels and impossible below 5,000. Lower than the chance of a jam at the same levels. 555555... is an arbitrary number that I came up with after playing with the formula in excel. It makes sense at low, medium, and high levels of dirt.
    if( dirt_dbl > 5000 &&
        x_in_y( dirt_dbl * dirt_dbl * dirt_dbl,
                5555555555555 ) ) {
        shooter.add_msg_player_or_npc( m_bad, _( "Your %s is damaged by the high pressure!" ),
                                       _( "<npcname>'s %s is damaged by the high pressure!" ),
                                       it.tname() );
        // Don't increment until after the message
        parent.inc_damage();
    }
    return true;
}

void npc::pretend_fire( npc *source, int shots, item &gun )
{
    int curshot = 0;
    avatar &you = get_avatar();
    if( you.sees( *source ) && one_in( 50 ) ) {
        add_msg( m_info, _( "%s shoots something." ), source->disp_name() );
    }
    while( curshot != shots ) {
        if( gun.ammo_consume( gun.ammo_required(), pos() ) != gun.ammo_required() ) {
            debugmsg( "Unexpected shortage of ammo whilst firing %s", gun.tname().c_str() );
            break;
        }

        item *weapon = &gun;
        const auto data = weapon->gun_noise( shots > 1 );

        if( you.sees( *source ) ) {
            add_msg( m_warning, _( "You hear %s." ), data.sound );
        }
        curshot++;
        moves -= 100;
    }
}


namespace
{

auto is_mountable( const map &m, const tripoint &pos ) -> bool
{
    // usage of any attached bipod is dependent upon terrain
    // only allow mounting passable OR climable terrain
    // example: sandbag barricades are impassable but climbable
    if( ( m.climb_difficulty( pos ) <= 5 || m.passable( pos ) ) &&
        m.has_flag_ter_or_furn( "MOUNTABLE", pos ) ) {
        return true;
    }

    if( const optional_vpart_position vp = m.veh_at( pos ) ) {
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        return m.passable( pos ) && vp->vehicle().has_part( pos, "MOUNTABLE" );
    }
    return false;
}

auto is_mountable_nearby( const map &m, const tripoint &pos ) -> bool
{
    const auto &xs = closest_points_first( pos, 1 );
    return std::any_of( xs.begin(), xs.end(),
                        [&m]( const tripoint & x ) -> bool { return is_mountable( m, x ); } );
}

auto can_use_heavy_weapon( const Character &who, const map &m, const tripoint &pos ) -> bool
{
    if( who.is_mounted() && who.mounted_creature->has_flag( MF_RIDEABLE_MECH ) ) {
        return true;
    }
    return is_mountable_nearby( m, pos );
}

} // namespace


dispersion_sources calculate_dispersion( const map &m, const Character &who, const item &gun,
        int at_recoil, bool burst )
{
    const bool bipod = can_use_heavy_weapon( who, m, who.pos() );

    const int gun_recoil = gun.gun_recoil( bipod );
    const int eff_recoil = at_recoil + ( burst ? ranged::burst_penalty( who, gun, gun_recoil ) : 0 );
    dispersion_sources dispersion( ranged::get_weapon_dispersion( who, gun ) );
    dispersion.add_range( eff_recoil );
    return dispersion;
}

static int calc_gun_volume( const item &gun )
{
    // Inherit suppressor modifiers if relevant (e.g. KSG second mag) but still use current ammo
    const item &parent = ( gun.parent_item() != nullptr &&
                           gun.has_flag( flag_USE_PARENT_GUN ) ) ? *gun.parent_item() : gun;
    int noise = parent.type->gun->loudness;
    for( const auto mod : parent.gunmods() ) {
        noise += mod->type->gunmod->loudness;
    }
    if( gun.ammo_data() ) {
        noise += gun.ammo_data()->ammo->loudness;
    }

    noise = std::max( noise, 0 );
    return noise;
}

int ranged::fire_gun( Character &who, const tripoint &target, int shots )
{
    return fire_gun( who, target, shots, who.primary_weapon(), nullptr );
}

int ranged::fire_gun( Character &who, const tripoint &target, int max_shots, item &gun,
                      item *ammo )
{
    int attack_moves = time_to_attack( who, gun, ammo );

    if( !gun.is_gun() ) {
        debugmsg( "%s tried to fire non-gun (%s).", who.name, gun.tname() );
        return 0;
    }

    if( gun.ammo_required() > 0 && !gun.ammo_remaining() && !ammo ) {
        debugmsg( "%s's gun %s is empty and has no ammo for reloading.", who.name, gun.tname() );
        return 0;
    }

    bool is_mech_weapon = false;
    if( who.is_mounted() && who.mounted_creature->has_flag( MF_RIDEABLE_MECH ) ) {
        is_mech_weapon = true;
    }

    int shots = max_shots;
    // Number of shots to fire is limited by the amount of remaining ammo
    if( gun.ammo_required() ) {
        const int ammo_left = ammo ? ammo->count() : gun.ammo_remaining();
        shots = std::min( shots, ammo_left / gun.ammo_required() );
    }

    // cap our maximum burst size by the amount of UPS power left
    if( !gun.has_flag( flag_VEHICLE ) && gun.get_gun_ups_drain() > 0 ) {
        shots = std::min( shots, ( who.charges_of( itype_UPS ) /
                                   gun.get_gun_ups_drain() ) );
    }

    if( shots <= 0 ) {
        debugmsg( "Attempted to fire zero or negative shots using %s", gun.tname() );
    }

    std::optional<shape_factory> shape = ranged::get_shape_factory( gun );

    map &here = get_map();
    // Shaped attacks don't allow aiming, so they don't suffer from lack of aim either
    int character_recoil = shape ? recoil_vehicle( who ) : recoil_total( who );
    // Penalty is (intentionally) based off mode shots, not ammo-limited.
    dispersion_sources dispersion = calculate_dispersion( here, who, gun, character_recoil,
                                    max_shots > 1 );

    bool aoe_attack = gun.gun_skill() == skill_launcher || shape;
    tripoint aim = target;
    int curshot = 0;
    int hits = 0; // total shots on target
    while( curshot != shots ) {
        if( !!ammo && !gun.ammo_remaining() ) {
            gun.reload( get_avatar(), *ammo, 1 );
        }
        if( gun.faults.contains( fault_gun_chamber_spent ) && curshot == 0 ) {
            who.moves -= 50;
            gun.faults.erase( fault_gun_chamber_spent );
            who.add_msg_if_player( _( "You cycle your %s manually." ), gun.tname() );
        }

        if( !ranged::handle_gun_damage( who, gun ) ) {
            break;
        }

        // If this is a vehicle mounted turret, which vehicle is it mounted on?
        const vehicle *in_veh = who.has_effect( effect_on_roof )
                                ? veh_pointer_or_null( here.veh_at( who.pos() ) )
                                : nullptr;
        projectile projectile = make_gun_projectile( gun );

        // Slings use ammo damage or damage from throwing the ammo, whichever is higher
        if( gun.gun_skill() == skill_throw && !who.is_fake() && gun.ammo_data() ) {
            item &tmp = *item::spawn_temporary( item( gun.ammo_data() ) );
            if( throw_damage( tmp, who.get_skill_level( skill_throw ),
                              who.get_str() ) > gun.ammo_data()->ammo->damage.damage_units.front().amount ) {
                projectile.impact = gun.gun_damage( false );
                auto &impact = projectile.impact;
                impact.add_damage( DT_BASH,
                                   throw_damage( tmp, who.get_skill_level( skill_throw ), who.get_str() ) );
            }
        }
        // Damage reduction from insufficient strength, if using a STR_DRAW weapon.
        projectile.impact.mult_damage( ranged::str_draw_damage_modifier( gun, who ) );

        if( who.has_trait( trait_NORANGEDCRIT ) ) {
            projectile.add_effect( ammo_effect_NO_CRIT );
        }
        if( !shape ) {
            auto shot = projectile_attack( projectile, who.pos(), aim, dispersion, &who, &gun, in_veh );
            if( shot.missed_by <= .1 ) {
                // TODO: check head existence for headshot
                g->events().send<event_type::character_gets_headshot>( who.getID() );
            }

            if( shot.hit_critter ) {
                hits++;
            }
        } else {
            // 30 degree cap, like for projectiles
            double angle_offset_arcmin = std::min( dispersion.roll(), 1800.0 ) * ( one_in( 2 ) ? 1 : -1 );
            double angle_offset = units::to_radians( units::from_arcmin( angle_offset_arcmin ) );
            double dx = aim.x - who.posx();
            double dy = aim.y - who.posy();
            double new_angle = atan2( dy, dx ) + angle_offset;
            // Always using trig here, rotations in maximum metric are weird
            double length = trig_dist( who.pos(), aim );
            rl_vec3d vec_pos( who.pos() );
            rl_vec3d new_aim = vec_pos + rl_vec3d( length, 0, 0 ).rotated( new_angle );
            ranged::execute_shaped_attack( *shape->create( vec_pos, new_aim ), projectile, who, &gun, in_veh );
        }
        curshot++;

        int noise = calc_gun_volume( gun );
        if( !who.is_deaf() && noise > 0 ) {
            who.add_msg_if_player( m_warning, _( "You fire your %s, %s" ),
                                   gun.tname(), gun.gun_noise( shots > 1 ).sound );
        } else {
            who.add_msg_if_player( m_warning, _( "You fire your %s!" ),
                                   gun.tname() );
        }
        ranged::make_gun_sound_effect( who, shots > 1, gun );

        cycle_action( gun, who.pos() );

        if( who.has_trait( trait_PYROMANIA ) &&
            !who.has_morale( MORALE_PYROMANIA_STARTFIRE ) &&
            gun.has_flag( flag_PYROMANIAC_WEAPON ) ) {
            who.add_msg_if_player( m_good, _( "You feel a surge of euphoria as flames roar out of the %s!" ),
                                   gun.tname() );
            who.add_morale( MORALE_PYROMANIA_STARTFIRE, 15, 15, 8_hours, 6_hours );
            who.rem_morale( MORALE_PYROMANIA_NOFIRE );
        }

        if( gun.ammo_consume( gun.ammo_required(), who.pos() ) != gun.ammo_required() ) {
            debugmsg( "Unexpected shortage of ammo whilst firing %s", gun.tname() );
            break;
        }

        if( !gun.has_flag( flag_VEHICLE ) ) {
            who.use_charges( itype_UPS, gun.get_gun_ups_drain() );
        }

        if( aoe_attack ) {
            continue; // skip retargeting for launchers
        }
    }

    if( gun.has_flag( flag_RELOAD_AND_SHOOT ) ) {
        // Reset aim for bows and other reload-and-shoot weapons.
        who.recoil = MAX_RECOIL;
    } else {
        // Hack alert: nearly every other use of can_use_heavy_weapon uses const character, except this one and gunmode_checks_weapon.
        const Character &shooter = who;
        // Now actually apply recoil for the future shots
        // But only for one shot, because bursts kinda suck
        int gun_recoil = gun.gun_recoil( can_use_heavy_weapon( shooter, here, shooter.pos() ) );

        // If user is currently able to fire a mounted gun freely, penalize dispersion
        // HEAVY_WEAPON_SUPPORT flag has highest penalty, Large mutants lower penalty, no penalty for Huge mutants.
        if( gun.has_flag( flag_MOUNTED_GUN ) && !can_use_heavy_weapon( shooter, here, shooter.pos() ) ) {
            if( who.get_size() == creature_size::large ) {
                gun_recoil = gun_recoil * 2;
            } else if( who.worn_with_flag( flag_HEAVY_WEAPON_SUPPORT ) &&
                       ( who.get_size() <= creature_size::medium ) ) {
                gun_recoil = gun_recoil * 3;
            }
        }

        who.recoil += gun_recoil;
        if( is_mech_weapon ) {
            // mechs can handle recoil far better. they are built around their main gun.
            who.recoil = who.recoil / 2;
        }
        who.recoil = std::min( MAX_RECOIL, who.recoil );
    }

    // Use different amounts of time depending on the type of gun and our skill
    who.moves -= attack_moves;

    // Practice the base gun skill proportionally to number of hits, but always by one.
    who.as_player()->practice( skill_gun, ( hits + 1 ) * 5 );
    // launchers train weapon skill for both hits and misses.
    int practice_units = aoe_attack ? curshot : hits;
    who.as_player()->practice( gun.gun_skill(), ( practice_units + 1 ) * 5 );

    cata::run_hooks( "on_shoot", [ & ]( auto & params ) {
        params["shooter"] = &who;
        params["target_pos"] = &target;
        params["shots"] = curshot;
        params["gun"] = &gun;
        params["ammo"] = ammo;
    } );
    return curshot;
}

namespace ranged
{

int throw_cost( const Character &c, const item &to_throw )
{
    // Very similar to player::attack_cost
    // TODO: Extract into a function?
    // Differences:
    // Dex is more (2x) important for throwing speed
    // At 10 skill, the cost is down to 0.75%, not 0.66%
    const int base_move_cost = to_throw.attack_cost() / 2;
    const int throw_skill = std::min( MAX_SKILL, c.get_skill_level( skill_throw ) );
    ///\EFFECT_THROW increases throwing speed
    const int skill_cost = ( base_move_cost * ( 20 - throw_skill ) / 20 );
    ///\EFFECT_DEX increases throwing speed
    const int dexbonus = c.get_dex();
    const int encumbrance_penalty = c.encumb( body_part_torso ) +
                                    ( c.encumb( body_part_hand_l ) + c.encumb( body_part_hand_r ) ) / 2;
    const float stamina_ratio = static_cast<float>( c.get_stamina() ) / c.get_stamina_max();
    const float stamina_penalty = 1.0 + std::max( ( 0.25f - stamina_ratio ) * 4.0f, 0.0f );

    int move_cost = base_move_cost;
    // Stamina penalty only affects base/2 and encumbrance parts of the cost
    move_cost += encumbrance_penalty;
    move_cost *= stamina_penalty;
    move_cost += skill_cost;
    move_cost -= dexbonus;
    move_cost *= c.mutation_value( "attackcost_modifier" );

    return std::max( 25, move_cost );
}

float get_str_draw_penalty( const item &it, const Character &p )
{
    // Grab the archer's skill for purposes of strength requirment reduction, if we are using a "gun"
    // Improvement intervals at 4/8/10, max reduction of 3 strength
    const int charskill = p.get_skill_level( it.gun_skill() );
    int str_adjust = 0;
    if( charskill >= 4 ) {
        if( charskill >= 10 ) {
            str_adjust = 3;
        } else if( charskill >= 8 ) {
            str_adjust = 2;
        } else {
            str_adjust = 1;
        }
    }
    // We only care if weapon has STR_DRAW, and that the user is weaker than required strength.
    // Also avoid dividing by zero, and skip if we'd just get a result of 1 anyway.
    if( !it.has_flag( flag_STR_DRAW ) || p.get_str() + str_adjust >= it.get_min_str() ||
        it.get_min_str() <= 1 ) {
        return 1.0f;
    }
    // We also don't want to actually reduce values to zero, even if user is debuffed to zero strength.
    float archer_str = std::max( 1, p.get_str() + str_adjust );
    return ( archer_str / it.get_min_str() );
}

float str_draw_damage_modifier( const item &it, const Character &p )
{
    // Grab the archer's skill for purposes of strength requirment reduction, if we are using a "gun"
    // Improvement intervals at 4/8/10, max reduction of 3 strength
    const int charskill = p.get_skill_level( it.gun_skill() );
    int str_adjust = 0;
    if( charskill >= 4 ) {
        if( charskill >= 10 ) {
            str_adjust = 3;
        } else if( charskill >= 8 ) {
            str_adjust = 2;
        } else {
            str_adjust = 1;
        }
    }
    if( !it.has_flag( flag_STR_DRAW ) || p.get_str() + str_adjust >= it.get_min_str() ||
        it.get_min_str() <= 1 ) {
        return 1.0f;
    }
    if( ranged::get_str_draw_penalty( it, p ) < 0.75f ) {
        return 0.5f;
    } else if( ranged::get_str_draw_penalty( it, p ) < 1.0f ) {
        return 0.75f;
    } else {
        return 1.0f;
    }
}

float str_draw_dispersion_modifier( const item &it, const Character &p )
{
    // Grab the archer's skill for purposes of strength requirment reduction, if we are using a "gun"
    // Improvement intervals at 4/8/10, max reduction of 3 strength
    const int charskill = p.get_skill_level( it.gun_skill() );
    int str_adjust = 0;
    if( charskill >= 4 ) {
        if( charskill >= 10 ) {
            str_adjust = 3;
        } else if( charskill >= 8 ) {
            str_adjust = 2;
        } else {
            str_adjust = 1;
        }
    }
    if( !it.has_flag( flag_STR_DRAW ) || p.get_str() + str_adjust >= it.get_min_str() ||
        it.get_min_str() <= 1 ) {
        return 1.0f;
    }
    if( ranged::get_str_draw_penalty( it, p ) < 0.75f ) {
        return 0.5f;
    } else {
        return 1.0f;
    }
}

float str_draw_range_modifier( const item &it, const Character &p )
{
    // Grab the archer's skill for purposes of strength requirment reduction, if we are using a "gun"
    // Improvement intervals at 4/8/10, max reduction of 3 strength
    const int charskill = p.get_skill_level( it.gun_skill() );
    int str_adjust = 0;
    if( charskill >= 4 ) {
        if( charskill >= 10 ) {
            str_adjust = 3;
        } else if( charskill >= 8 ) {
            str_adjust = 2;
        } else {
            str_adjust = 1;
        }
    }
    if( !it.has_flag( flag_STR_DRAW ) || p.get_str() + str_adjust >= it.get_min_str() ||
        it.get_min_str() <= 1 ) {
        return 1.0f;
    }
    if( ranged::get_str_draw_penalty( it, p ) < 0.75f ) {
        return 0.5f;
    } else if( ranged::get_str_draw_penalty( it, p ) < 1.0f ) {
        return 0.75f;
    } else {
        return 1.0f;
    }
}

int throw_dispersion_per_dodge( const Character &c, bool add_encumbrance )
{
    // +200 per dodge point at 0 dexterity
    // +100 at 8, +80 at 12, +66.6 at 16, +57 at 20, +50 at 24
    // Each 10 encumbrance on either hand is like -1 dex (can bring penalty to +400 per dodge)
    // Maybe TODO: Only use one hand
    const int encumbrance = add_encumbrance ? c.encumb( body_part_hand_l ) + c.encumb(
                                body_part_hand_r ) : 0;
    ///\EFFECT_DEX increases throwing accuracy against targets with good dodge stat
    float effective_dex = 2 + c.get_dex() / 4.0f - ( encumbrance ) / 40.0f;
    return static_cast<int>( 100.0f / std::max( 1.0f, effective_dex ) );
}

// Perfect situation gives us 1000 dispersion at lvl 0
// This goes down linearly to 200  dispersion at lvl 10
int throwing_dispersion( const Character &c, const item &to_throw, Creature *critter,
                         bool is_blind_throw )
{
    units::mass weight = to_throw.weight();
    units::volume volume = to_throw.volume();
    if( to_throw.count_by_charges() && to_throw.charges > 1 ) {
        weight /= to_throw.charges;
        volume /= to_throw.charges;
    }

    int throw_difficulty = 1000;
    // 1000 penalty for every liter after the first
    // TODO: Except javelin type items
    throw_difficulty += std::max<int>( 0, units::to_milliliter( volume - 1_liter ) );
    // 1 penalty for gram above str*100 grams (at 0 skill)
    ///\EFFECT_STR decreases throwing dispersion when throwing heavy objects
    const int weight_in_gram = units::to_gram( weight );
    throw_difficulty += std::max( 0, weight_in_gram - c.get_str() * 100 );

    // Dispersion from difficult throws goes from 100% at lvl 0 to 20% at lvl 10
    ///\EFFECT_THROW increases throwing accuracy
    const int throw_skill = std::min( MAX_SKILL, c.get_skill_level( skill_throw ) );
    int dispersion = 10 * throw_difficulty / ( 6 * throw_skill + 20 );
    // If the target is a creature, it moves around and ruins aim
    // TODO: Inform projectile functions if the attacker actually aims for the critter or just the tile
    if( critter != nullptr ) {
        // It's easier to dodge at close range (thrower needs to adjust more)
        // Dodge x10 at point blank, x5 at 1 dist, then flat
        float effective_dodge = critter->get_dodge() * std::max( 1, 10 - 5 * rl_dist( c.pos(),
                                critter->pos() ) );
        dispersion += throw_dispersion_per_dodge( c, true ) * effective_dodge;
    }
    // 1 perception per 1 eye encumbrance
    ///\EFFECT_PER decreases throwing accuracy penalty from eye encumbrance
    dispersion += std::max( 0, ( c.encumb( body_part_eyes ) - c.get_per() ) * 10 );

    // If throwing blind, we're assuming they mechanically can't achieve the
    // accuracy of a normal throw.
    if( is_blind_throw ) {
        dispersion *= 4;
    }

    return std::max( 0, dispersion );
}

namespace
{
auto throw_damage_projectile( const item &it, const int skill, const int str ) -> projectile
{
    const units::mass weight = it.weight();

    projectile proj;
    proj.impact = it.base_damage_thrown();
    proj.speed = std::log2( std::max( 1, skill ) * std::max( 1, str ) );

    const int damage = 0.5 * ( weight / 1_gram / 1000.0 ) * std::pow( proj.speed, 2 );

    proj.impact.add_damage( DT_BASH, damage );

    // add_msg( m_info, "skill_level is %s", skill );
    // add_msg( m_info, "effective_strength is %s", str );
    // add_msg( m_info, "Thrown item weight is %s grams", to_gram( weight ) );

    // add_msg( m_info, "Calculated damage is %s", damage );

    return proj;
}
} // namespace

auto throw_damage( const item &it, const int skill, const int str ) -> int
{
    return throw_damage_projectile( it, skill, str ).impact.total_damage();
}

dealt_projectile_attack throw_item( Character &who, const tripoint &target,
                                    detached_ptr<item> &&to_throw,
                                    std::optional<tripoint> blind_throw_from_pos )
{
    item &thrown = *to_throw;

    const int move_cost = ranged::throw_cost( who, thrown );
    who.mod_moves( -move_cost );

    const int throwing_skill = who.get_skill_level( skill_throw );
    const units::volume volume = thrown.volume();
    const units::mass weight = thrown.weight();

    // Previously calculated as 2_gram * std::max( 1, str_cur )
    // using 16_gram normalizes it to 8 str. Same effort expenditure
    // for being able to throw farther.
    const int weight_cost = weight / ( 16_gram );
    const int encumbrance_cost = roll_remainder( ( who.encumb( body_part_arm_l ) + who.encumb(
                                     body_part_arm_r ) ) * 2.0f );
    const int stamina_cost = ( weight_cost + encumbrance_cost - throwing_skill + 50 ) * -1;

    bool throw_assist = false;
    int throw_assist_str = 0;
    if( who.is_mounted() ) {
        monster *mons = who.mounted_creature.get();
        if( mons->mech_str_addition() != 0 ) {
            throw_assist = true;
            throw_assist_str = mons->mech_str_addition();
            mons->use_mech_power( -3 );
        }
    }
    if( !throw_assist ) {
        who.mod_stamina( stamina_cost );
    }

    const skill_id &skill_used = skill_throw;
    int skill_level = std::min( MAX_SKILL, who.get_skill_level( skill_throw ) );
    // if you are lying on the floor, you can't really throw that well
    if( who.has_effect( effect_downed ) ) {
        skill_level = std::max( 0, skill_level - 5 );
    }

    static const std::set<material_id> ferric = { material_id( "iron" ), material_id( "steel" ) };
    const bool do_railgun = who.has_active_bionic( bio_railgun ) && thrown.made_of_any( ferric ) &&
                            !throw_assist;
    const int effective_strength =
        throw_assist ? throw_assist_str : do_railgun ? who.get_str() * 2 : who.get_str();

    // We'll be constructing a projectile
    projectile proj = throw_damage_projectile( thrown, skill_level, effective_strength );

    if( thrown.has_flag( flag_ACT_ON_RANGED_HIT ) ) {
        proj.add_effect( ammo_effect_ACT_ON_RANGED_HIT );
        thrown.activate();
    }
    // This is just to indicate something is a thrown item
    // Checking with other methods downstream breaks other projectile attacks.
    proj.add_effect( ammo_effect_THROWN );

    // Item will shatter upon landing, destroying the item, dealing damage, and making noise
    /** @EFFECT_STR increases chance of shattering thrown glass items (NEGATIVE) */
    const bool shatter = !thrown.is_active() && thrown.can_shatter() &&
                         rng( 0, units::to_milliliter( 2_liter - volume ) ) < who.get_str() * 100;

    // Item will burst upon landing, destroying the item, and spilling its contents
    const bool burst = thrown.has_property( "burst_when_filled" ) && thrown.is_container() &&
                       thrown.get_property_int64_t( "burst_when_filled" ) <= static_cast<double>
                       ( thrown.get_contained().volume().value() ) / thrown.get_container_capacity().value() * 100;

    // Add some flags to the projectile
    if( weight > 500_gram ) {
        proj.add_effect( ammo_effect_HEAVY_HIT );
    }

    proj.add_effect( ammo_effect_NO_ITEM_DAMAGE );

    if( thrown.is_active() ) {
        // Can't have Molotovs embed into monsters
        // Monsters don't have inventory processing
        proj.add_effect( ammo_effect_NO_EMBED );
    }

    if( do_railgun ) {
        proj.add_effect( ammo_effect_LIGHTNING );
    }

    if( volume > 500_ml ) {
        proj.add_effect( ammo_effect_WIDE );
    }

    // Deal extra cut damage if the item breaks
    if( shatter ) {
        proj.impact.add_damage( DT_CUT, units::to_milliliter( volume ) / 500.0f );
        proj.add_effect( ammo_effect_SHATTER_SELF );
    }

    // TODO: Add wet effect if other things care about that
    if( burst ) {
        proj.add_effect( ammo_effect_BURST );
    }

    // Some minor (skill/2) armor piercing for skillful throws
    // Not as much as in melee, though
    for( damage_unit &du : proj.impact.damage_units ) {
        du.res_pen += skill_level / 2.0f;
    }
    // handling for tangling thrown items
    if( thrown.has_flag( flag_TANGLE ) ) {
        proj.add_effect( ammo_effect_TANGLE );
    }

    if( thrown.has_flag( flag_NO_DAMAGE ) ) {
        proj.add_effect( ammo_effect_NO_DAMAGE );
    }

    Creature *critter = g->critter_at( target, true );
    const dispersion_sources dispersion( ranged::throwing_dispersion( who, thrown, critter,
                                         blind_throw_from_pos.has_value() ) );
    const itype *thrown_type = thrown.type;

    // Put the item into the projectile
    proj.set_drop( std::move( to_throw ) );
    if( thrown_type->has_flag( flag_CUSTOM_EXPLOSION ) ) {
        proj.set_custom_explosion( thrown_type->explosion );
    }

    // Throw from the player's position, unless we're blind throwing, in which case
    // throw from the the blind throw position instead.
    const tripoint throw_from = blind_throw_from_pos.value_or( who.pos() );

    float range = rl_dist( throw_from, target );
    proj.range = range;
    int skill_lvl = who.get_skill_level( skill_used );
    // Avoid awarding tons of xp for lucky throws against hard to hit targets
    const float range_factor = std::min<float>( range, skill_lvl + 3 );
    // We're aiming to get a damaging hit, not just an accurate one - reward proper weapons
    const float damage_factor = 5.0f * std::sqrt( proj.impact.total_damage() / 5.0f );
    // This should generally have values below ~20*sqrt(skill_lvl)
    const float final_xp_mult = range_factor * damage_factor;

    auto dealt_attack = projectile_attack( proj, throw_from, target, dispersion, &who );

    const double missed_by = dealt_attack.missed_by;
    if( missed_by <= 0.1 && dealt_attack.hit_critter != nullptr ) {
        who.as_player()->practice( skill_used, final_xp_mult, MAX_SKILL );
        // TODO: Check target for existence of head
        g->events().send<event_type::character_gets_headshot>( who.getID() );
    } else if( dealt_attack.hit_critter != nullptr && missed_by > 0.0f ) {
        who.as_player()->practice( skill_used, final_xp_mult / ( 1.0f + missed_by ), MAX_SKILL );
    } else {
        // Pure grindy practice - cap gain at lvl 2
        who.as_player()->practice( skill_used, 5, 2 );
    }
    // Reset last target pos
    who.last_target_pos = std::nullopt;
    who.recoil = MAX_RECOIL;

    cata::run_hooks( "on_throw", [ & ]( auto & params ) {
        params["thrower"] = &who;
        params["target_pos"] = &target;
        params["throw_from_pos"] = &throw_from;
        params["thrown"] = &thrown;
    } );
    return dealt_attack;
}

} // namespace ranged

static void do_aim( avatar &you, const item &relevant, const double min_recoil )
{
    const double aim_amount = ranged::aim_per_move( you, relevant, you.recoil );
    if( aim_amount > 0 && you.recoil > min_recoil ) {
        // Increase aim at the cost of moves
        you.mod_moves( -1 );
        you.recoil = std::max( min_recoil, you.recoil - aim_amount );
    } else {
        // If aim is already maxed, we're just waiting, so pass the turn.
        you.set_moves( 0 );
    }
}

struct confidence_rating {
    double aim_level;
    char symbol;
    std::string color;
    std::string label;
};

static int print_steadiness( const catacurses::window &w, int line_number, double steadiness )
{
    const int window_width = getmaxx( w ) - 2; // Window width minus borders.

    if( get_option<std::string>( "ACCURACY_DISPLAY" ) == "numbers" ) {
        std::string steadiness_s = string_format( "%s: %d%%", _( "Steadiness" ),
                                   static_cast<int>( 100.0 * steadiness ) );
        mvwprintw( w, point( 1, line_number++ ), steadiness_s );
    } else {
        const std::string &steadiness_bar = get_labeled_bar( steadiness, window_width,
                                            _( "Steadiness" ), '*' );
        mvwprintw( w, point( 1, line_number++ ), steadiness_bar );
    }

    return line_number;
}

static double confidence_estimate( int range, double target_size,
                                   const dispersion_sources &dispersion )
{
    // This is a rough estimate of accuracy based on a linear distribution across min and max
    // dispersion.  It is highly inaccurate probability-wise.  The result gives the player
    // correct relative measures of chance to hit, and corresponds with the actual distribution at
    // min, max, and mean.
    if( range == 0 ) {
        return 2 * target_size;
    }
    const double max_lateral_offset =
        iso_tangent( range, units::from_arcmin( dispersion.max() ) );
    return 1 / ( max_lateral_offset / target_size );
}

static std::vector<ranged::aim_type> get_default_aim_type()
{
    std::vector<ranged::aim_type> aim_types;
    aim_types.push_back( ranged::aim_type { "", "", "", false, 0 } ); // dummy aim type for unaimed shots
    return aim_types;
}

using RatingVector = std::vector<std::tuple<double, char, std::string>>;
static std::string get_colored_bar( const double val, const int width, const std::string &label,
                                    RatingVector::iterator begin, RatingVector::iterator end )
{
    std::string result;

    result.reserve( width );
    if( !label.empty() ) {
        result += label;
        result += ' ';
    }
    const int bar_width = width - utf8_width( result ) - 2; // - 2 for the brackets

    result += '[';
    if( bar_width > 0 ) {
        int used_width = 0;
        for( auto it( begin ); it != end; ++it ) {
            const double factor = std::min( 1.0, std::max( 0.0, std::get<0>( *it ) * val ) );
            const int seg_width = static_cast<int>( factor * bar_width ) - used_width;

            if( seg_width <= 0 ) {
                continue;
            }
            used_width += seg_width;
            result += string_format( "<color_%s>", std::get<2>( *it ) );
            result.insert( result.end(), seg_width, std::get<1>( *it ) );
            result += "</color>";
        }
        result.insert( result.end(), bar_width - used_width, ' ' );
    }
    result += ']';

    return result;
}

static int print_ranged_chance( const catacurses::window &w, int line_number,
                                input_context &ctxt, const item &,
                                const std::vector<ranged::aim_type> &aim_types,
                                const std::function<dispersion_sources( const ranged::aim_type & )> &dispersion_fun,
                                const std::function<int( const ranged::aim_type & )> &cost_fun,
                                const std::vector<confidence_rating> &confidence_config,
                                double range, double target_size )
{
    int window_width = getmaxx( w ) - 2; // Window width minus borders.
    std::string display_type = get_option<std::string>( "ACCURACY_DISPLAY" );
    std::string panel_type = panel_manager::get_manager().get_current_layout_id();
    const int bars_pad = 3; // Padding for "bars" to fit moves_to_fire value.
    if( ( panel_type == "compact" || panel_type == "labels-narrow" ) && display_type != "numbers" ) {
        window_width -= bars_pad;
    }

    std::string label_m = _( "Moves" );
    std::vector<std::string> t_aims( 4 ), t_confidence( 16 );
    int aim_iter = 0, conf_iter = 0;

    nc_color col = c_dark_gray;

    if( display_type != "numbers" ) {
        std::string symbols;
        int column_number = 1;
        if( !( panel_type == "compact" || panel_type == "labels-narrow" ) ) {
            std::string label = _( "Symbols:" );
            mvwprintw( w, point( column_number, line_number ), label );
            column_number += utf8_width( label ) + 1; // 1 for whitespace after 'Symbols:'
        }
        for( const confidence_rating &cr : confidence_config ) {
            std::string label = pgettext( "aim_confidence", cr.label.c_str() );
            std::string symbols = string_format( "<color_%s>%s</color> = %s", cr.color, cr.symbol,
                                                 label );
            int line_len = utf8_width( label ) + 5; // 5 for '# = ' and whitespace at end
            if( ( window_width + bars_pad - column_number ) < line_len ) {
                column_number = 1;
                line_number++;
            }
            print_colored_text( w, point( column_number, line_number ), col, col, symbols );
            column_number += line_len;
        }
        line_number++;
    }
    if( ( panel_type == "compact" || panel_type == "labels-narrow" ) && display_type == "numbers" ) {
        std::string symbols = _( " <color_green>Great</color> - <color_light_gray>Normal</color>"
                                 " - <color_magenta>Graze</color> - <color_light_blue>Moves</color>" );
        fold_and_print( w, point( 1, line_number++ ), window_width + bars_pad,
                        c_dark_gray, symbols );
        int len = utf8_width( symbols ) - 96; // 96 to subtract color codes
        if( len > window_width + bars_pad ) {
            line_number++;
        }
        for( int i = 0; i < window_width; i++ ) {
            mvwprintw( w, point( i + 1, line_number ), "-" );
        }
    }

    const auto front_or = [&]( const std::string & s, const char fallback ) {
        const auto keys = ctxt.keys_bound_to( s );
        return keys.empty() ? fallback : keys.front();
    };

    for( const ranged::aim_type &type : aim_types ) {
        dispersion_sources current_dispersion = dispersion_fun( type );
        std::string label = _( "Current" );
        std::string aim_l = _( "Aim" );
        if( type.has_threshold ) {
            label = type.name;
        }

        int moves_to_fire = cost_fun( type );

        auto hotkey = front_or( type.action.empty() ? "FIRE" : type.action, ' ' );
        if( ( panel_type == "compact" || panel_type == "labels-narrow" ) ) {
            if( display_type == "numbers" ) {
                t_aims[aim_iter] = string_format( "<color_dark_gray>%s:</color>", label );
                t_confidence[( aim_iter * 4 ) + 3] = string_format( "<color_light_blue>%d</color>", moves_to_fire );
            } else {
                print_colored_text( w, point( 1, line_number ), col, col, string_format( _( "%s %s:" ), label,
                                    aim_l ) );
                right_print( w, line_number++, 1, c_light_blue, _( "Moves" ) );
                right_print( w, line_number, 1, c_light_blue, string_format( "%d", moves_to_fire ) );
            }
        } else {
            print_colored_text( w, point( 1, line_number++ ), col, col,
                                string_format( _( "<color_white>[%s]</color> %s %s: Moves to fire: "
                                                  "<color_light_blue>%d</color>" ),
                                               hotkey, label, aim_l, moves_to_fire ) );
        }

        double confidence = confidence_estimate( range, target_size, current_dispersion );

        if( display_type == "numbers" ) {
            if( panel_type == "compact" || panel_type == "labels-narrow" ) {
                int last_chance = 0;
                for( const confidence_rating &cr : confidence_config ) {
                    int chance = std::min<int>( 100, 100.0 * ( cr.aim_level ) * confidence ) - last_chance;
                    last_chance += chance;
                    t_confidence[conf_iter] = string_format( "<color_%s>%3d%%</color>", cr.color, chance );
                    conf_iter++;
                    if( conf_iter == ( aim_iter * 4 ) + 3 ) {
                        conf_iter++;
                    }
                }
                aim_iter++;
            } else {
                int last_chance = 0;
                std::string confidence_s = enumerate_as_string( confidence_config.begin(), confidence_config.end(),
                [&]( const confidence_rating & config ) {
                    // TODO: Consider not printing 0 chances, but only if you can print something (at least miss 100% or so)
                    int chance = std::min<int>( 100, 100.0 * ( config.aim_level * confidence ) ) - last_chance;
                    last_chance += chance;
                    return string_format( "%s: <color_%s>%3d%%</color>", pgettext( "aim_confidence",
                                          config.label.c_str() ), config.color, chance );
                }, enumeration_conjunction::none );
                line_number += fold_and_print_from( w, point( 1, line_number ), window_width, 0,
                                                    c_dark_gray, confidence_s );
            }
        } else {
            std::vector<std::tuple<double, char, std::string>> confidence_ratings;
            std::transform( confidence_config.begin(), confidence_config.end(),
                            std::back_inserter( confidence_ratings ),
            [&]( const confidence_rating & config ) {
                return std::make_tuple( config.aim_level, config.symbol, config.color );
            }
                          );
            const std::string &confidence_bar = get_colored_bar( confidence, window_width, "",
                                                confidence_ratings.begin(),
                                                confidence_ratings.end() );

            print_colored_text( w, point( 1, line_number++ ), col, col, confidence_bar );
        }
    }

    // Draw tables for compact Numbers display
    if( ( panel_type == "compact" || panel_type == "labels-narrow" )
        && display_type == "numbers" ) {
        const std::string divider = "|";
        int left_pad = 10, columns = 4;
        insert_table( w, left_pad, ++line_number, columns, c_light_gray, divider, true, t_confidence );
        insert_table( w, 0, line_number, 1, c_light_gray, "", false, t_aims );
        line_number = line_number + 4; // 4 to account for the tables
    }
    return line_number;
}

// Whether player character knows creature's position and can roughly track it with the aim cursor
static bool pl_sees( const Creature &cr )
{
    Character &u = get_player_character();
    return u.sees( cr ) || u.sees_with_infrared( cr ) || u.sees_with_specials( cr );
}

// Handle capping aim level when the player cannot see the target tile or there is nothing to aim at.
static double calculate_aim_cap( const Character &p, const tripoint &target )
{
    double min_recoil = 0.0;
    const Creature *victim = g->critter_at( target, true );
    // No p.sees_with_specials() here because special senses are not precise enough
    // to give creature's exact size & position, only which tile it occupies
    if( victim == nullptr || ( !p.sees( *victim ) && !p.sees_with_infrared( *victim ) ) ) {
        const int range = rl_dist( p.pos(), target );
        // Get angle of triangle that spans the target square.
        const double angle = atan2( 1, range );
        // Convert from radians to arcmin.
        min_recoil = 60 * 180 * angle / M_PI;
    }
    return min_recoil;
}

static int print_aim( const Character &p, const catacurses::window &w, int line_number,
                      input_context &ctxt, item &weapon,
                      const double target_size, const tripoint &pos, double predicted_recoil,
                      item *load_loc )
{
    // This is absolute accuracy for the player.
    // TODO: push the calculations duplicated from Creature::deal_projectile_attack() and
    // Creature::projectile_attack() into shared methods.
    // Dodge doesn't affect gun attacks

    dispersion_sources dispersion = ranged::get_weapon_dispersion( p, weapon );
    dispersion.add_range( ranged::recoil_vehicle( p ) );

    const double min_recoil = calculate_aim_cap( p, pos );
    const double effective_recoil = ranged::effective_dispersion( p,
                                    p.primary_weapon().sight_dispersion() );
    const double min_dispersion = std::max( min_recoil, effective_recoil );
    const double steadiness_range = MAX_RECOIL - min_dispersion;
    // This is a relative measure of how steady the player's aim is,
    // 0 is the best the player can do.
    const double steady_score = std::max( 0.0, predicted_recoil - min_dispersion );
    // Fairly arbitrary cap on steadiness...
    const double steadiness = 1.0 - ( steady_score / steadiness_range );

    // This could be extracted, to allow more/less verbose displays
    static const std::vector<confidence_rating> confidence_config = {{
            { accuracy_critical, '*', "green", translate_marker_context( "aim_confidence", "Great" ) },
            { accuracy_standard, '+', "light_gray", translate_marker_context( "aim_confidence", "Normal" ) },
            { accuracy_grazing, '|', "magenta", translate_marker_context( "aim_confidence", "Graze" ) }
        }
    };

    int shots = std::max( 1, weapon.gun_current_mode().qty );
    const auto dispersion_fun = [&]( const ranged::aim_type & at ) {
        int at_recoil = at.has_threshold ? at.threshold : static_cast<int>( predicted_recoil );
        return calculate_dispersion( get_map(), p, weapon, at_recoil, shots > 1 );
    };
    const auto cost_fun = [&]( const ranged::aim_type & at ) {
        int at_recoil = at.has_threshold ? at.threshold : static_cast<int>( predicted_recoil );
        return ranged::gun_engagement_moves( p, weapon, at_recoil, p.recoil ) +
               ranged::time_to_attack( p, weapon, load_loc );
    };
    const double range = rl_dist( p.pos(), pos );
    line_number = print_steadiness( w, line_number, steadiness );
    return print_ranged_chance( w, line_number, ctxt, weapon, ranged::get_aim_types( p, weapon ),
                                dispersion_fun, cost_fun, confidence_config, range, target_size );
}

static int draw_throw_aim( const player &p, const catacurses::window &w, int line_number,
                           input_context &ctxt,
                           const item &weapon, const tripoint &target_pos, bool is_blind_throw )
{
    Creature *target = g->critter_at( target_pos, true );
    if( target != nullptr && !p.sees( *target ) ) {
        target = nullptr;
    }

    const dispersion_sources dispersion(
        ranged::throwing_dispersion( p, weapon, target, is_blind_throw ) );
    const double range = rl_dist( p.pos(), target_pos );

    const double target_size = target != nullptr ? target->ranged_target_size() : 1.0f;

    static const std::vector<confidence_rating> confidence_config_critter = {{
            { accuracy_critical, '*', "green", translate_marker_context( "aim_confidence", "Great" ) },
            { accuracy_standard, '+', "light_gray", translate_marker_context( "aim_confidence", "Normal" ) },
            { accuracy_grazing, '|', "magenta", translate_marker_context( "aim_confidence", "Graze" ) }
        }
    };
    static const std::vector<confidence_rating> confidence_config_object = {{
            { accuracy_grazing, '*', "white", translate_marker_context( "aim_confidence", "Hit" ) }
        }
    };
    const auto &confidence_config = target != nullptr ?
                                    confidence_config_critter : confidence_config_object;

    const auto dispersion_fun = [&]( const ranged::aim_type & ) {
        return dispersion;
    };
    const auto cost_fun = [&]( const ranged::aim_type & ) {
        return ranged::throw_cost( p, weapon );
    };
    return print_ranged_chance( w, line_number, ctxt, weapon, get_default_aim_type(),
                                dispersion_fun, cost_fun, confidence_config, range, target_size );
}

std::vector<ranged::aim_type> ranged::get_aim_types( const Character &who, const item &gun )
{
    std::vector<aim_type> aim_types = get_default_aim_type();
    if( !gun.is_gun() ) {
        return aim_types;
    }
    int sight_dispersion = effective_dispersion( who, gun.sight_dispersion() );
    // Aiming thresholds are dependent on weapon sight dispersion, attempting to place thresholds
    // at 10%, 5% and 0% of the difference between MAX_RECOIL and sight dispersion.
    std::vector<int> thresholds = {
        static_cast<int>( ( ( MAX_RECOIL - sight_dispersion ) / 10.0 ) + sight_dispersion ),
        static_cast<int>( ( ( MAX_RECOIL - sight_dispersion ) / 20.0 ) + sight_dispersion ),
        sight_dispersion
    };
    // Remove duplicate thresholds.
    std::vector<int>::iterator thresholds_it = std::adjacent_find( thresholds.begin(),
            thresholds.end() );
    while( thresholds_it != thresholds.end() ) {
        thresholds.erase( thresholds_it );
        thresholds_it = std::adjacent_find( thresholds.begin(), thresholds.end() );
    }
    thresholds_it = thresholds.begin();
    aim_types.push_back( aim_type { _( "Regular" ), "AIMED_SHOT", _( "[%c] to aim and fire." ),
                                    true, *thresholds_it } );
    thresholds_it++;
    if( thresholds_it != thresholds.end() ) {
        aim_types.push_back( aim_type { _( "Careful" ), "CAREFUL_SHOT",
                                        _( "[%c] to take careful aim and fire." ), true,
                                        *thresholds_it } );
        thresholds_it++;
    }
    if( thresholds_it != thresholds.end() ) {
        aim_types.push_back( aim_type { _( "Precise" ), "PRECISE_SHOT",
                                        _( "[%c] to take precise aim and fire." ), true,
                                        *thresholds_it } );
    }
    return aim_types;
}

static projectile make_gun_projectile( const item &gun )
{
    projectile proj;
    proj.speed = gun.gun_speed();
    proj.impact = gun.gun_damage();
    proj.range = gun.gun_range();
    proj.aimedcritbonus = gun.gun_aimed_crit_bonus();
    proj.aimedcritmaxbonus = gun.gun_aimed_crit_max_bonus();
    for( const ammo_effect_str_id &ae_id : gun.ammo_effects() ) {
        proj.add_effect( ae_id );
    }

    auto &fx = proj;

    if( ( gun.ammo_data() && gun.ammo_data()->phase == LIQUID ) ||
        fx.has_effect( ammo_effect_SHOT ) || fx.has_effect( ammo_effect_BOUNCE ) ) {
        fx.add_effect( ammo_effect_WIDE );
    }

    if( gun.ammo_data() ) {
        assert( gun.ammo_data()->ammo );
        const islot_ammo &ammo = *gun.ammo_data()->ammo;
        // Some projectiles have a chance of being recoverable
        bool recover = !one_in( ammo.dont_recover_one_in );
        // Some weapons can override this
        recover = recover && !gun.has_flag( flag_NO_RECOVER_AMMO );

        if( recover && !fx.has_effect( ammo_effect_IGNITE ) && !fx.has_effect( ammo_effect_EXPLOSIVE ) ) {
            detached_ptr<item> drop = item::spawn( gun.ammo_current(), calendar::turn, 1 );
            if( fx.has_effect( ammo_effect_ACT_ON_RANGED_HIT ) ) {
                drop->activate();
            }
            proj.set_drop( std::move( drop ) );
        }

        if( ammo.drop ) {
            detached_ptr<item> drop = item::spawn( ammo.drop, calendar::turn, ammo.drop_count );
            if( ammo.drop_active ) {
                drop->activate();
            }
            proj.set_drop( std::move( drop ) );
        }

        if( fx.has_effect( ammo_effect_CUSTOM_EXPLOSION ) ) {
            proj.set_custom_explosion( gun.ammo_data()->explosion );
        }
    }

    return proj;
}

int ranged::time_to_attack( const Character &p, const item &firing, const item *loc )
{
    const skill_id &skill_used = firing.type->gun->skill_used;
    const time_info_t &info = skill_used->time_to_attack();
    int RAS_time = 0;
    if( !loc ) {
        RAS_time = 0;
    } else {
        // RAS Weapon, calculate move cost.
        // At low stamina levels, firing starts getting slow.
        const int sta_percent = ( 100 * p.get_stamina() ) / p.get_stamina_max();
        const int reload_stamina_penalty = ( sta_percent < 25 ) ? ( ( 25 - sta_percent ) * 2 ) : 0;
        item_reload_option opt = item_reload_option( p.as_player(), const_cast<item *>( &firing ), &firing,
                                 *const_cast<item *>( loc ) );
        RAS_time = opt.moves() + reload_stamina_penalty;
    }
    return std::max( info.min_time,
                     info.base_time - info.time_reduction_per_level * p.get_skill_level( skill_used ) + RAS_time );
}

static void cycle_action( item &weap, const tripoint &pos )
{
    map &here = get_map();
    // eject casings and linkages in random direction avoiding walls using player position as fallback
    std::vector<tripoint> tiles = closest_points_first( pos, 1 );
    tiles.erase( tiles.begin() );
    tiles.erase( std::remove_if( tiles.begin(), tiles.end(), [&pos, &here]( const tripoint & e ) {
        return !here.passable( e ) || here.obstructed_by_vehicle_rotation( pos, e );
    } ), tiles.end() );
    tripoint eject = tiles.empty() ? pos : random_entry( tiles );

    // for turrets try and drop casings or linkages directly to any CARGO part on the same tile
    const optional_vpart_position vp = here.veh_at( pos );
    std::vector<vehicle_part *> cargo;
    if( vp && weap.has_flag( flag_VEHICLE ) ) {
        cargo = vp->vehicle().get_parts_at( pos, "CARGO", part_status_flag::any );
    }

    item &parent = ( weap.parent_item() != nullptr &&
                     weap.has_flag( flag_USE_PARENT_GUN ) ) ? *weap.parent_item() : weap;
    if( weap.ammo_data() && weap.ammo_data()->ammo->casing ) {
        const itype_id casing = *weap.ammo_data()->ammo->casing;
        if( parent.has_flag( flag_RELOAD_EJECT )
            || gunmod_find_with( parent, []( auto * e ) -> bool { return e->has_flag( flag_BRASS_CATCHER ); } ) ) {
            detached_ptr<item> det = item::spawn( casing );
            det->set_flag( flag_CASING );
            parent.put_in( std::move( det ) );
        } else {
            if( cargo.empty() ) {
                here.add_item_or_charges( eject, item::spawn( casing ) );
            } else {
                vp->vehicle().add_item( *cargo.front(), item::spawn( casing ) );
            }

            sfx::play_variant_sound( "fire_gun", "brass_eject", sfx::get_heard_volume( eject ),
                                     sfx::get_heard_angle( eject ) );
        }
    }

    // some magazines also eject disintegrating linkages
    const auto mag = weap.magazine_current();
    if( mag && mag->type->magazine->linkage ) {
        detached_ptr<item> linkage = item::spawn( *mag->type->magazine->linkage, calendar::turn, 1 );
        if( gunmod_find_with( parent, []( auto * e ) -> bool { return e->has_flag( flag_BRASS_CATCHER ); } ) ) {
            linkage->set_flag( flag_CASING );
            parent.put_in( std::move( linkage ) );
        } else if( cargo.empty() ) {
            here.add_item_or_charges( eject, std::move( linkage ) );
        } else {
            vp->vehicle().add_item( *cargo.front(), std::move( linkage ) );
        }
    }
}

void ranged::make_gun_sound_effect( const Character &who, bool burst, const item &gun )
{
    const item::sound_data data = gun.gun_noise( burst );
    if( data.volume > 0 ) {
        sounds::sound( who.pos(), data.volume, sounds::sound_t::combat,
                       data.sound.empty() ? _( "Bang!" ) : data.sound );
    }
    sfx::generate_gun_sound( who.pos(), gun );
}

item::sound_data item::gun_noise( const bool burst ) const
{
    if( !is_gun() ) {
        return { 0, "" };
    }

    int noise = calc_gun_volume( *this );

    if( type->weapon_category.contains( weapon_cat_WATER_CANNONS ) ) {
        return { noise, _( "Splash!" ) };

    } else if( type->weapon_category.contains( weapon_cat_MAGNETIC ) ) {
        if( noise < 20 ) {
            return { noise, burst ? _( "tz-tz-tzk!" ) : _( "tzk!" ) };
        } else if( noise < 80 ) {
            return { noise, burst ? _( "Brzzip!" ) : _( "tz-Zing!" ) };
        } else if( noise < 200 ) {
            return { noise, burst ? _( "tzz-CR-CR-CRAck!" ) : _( "tz-CRACKck!" ) };
        } else {
            return { noise, burst ? _( "tzz-BOOOM!" ) : _( "tzk-BLAM!" ) };
        }

    } else if( type->weapon_category.contains( weapon_cat_PNEUMATIC ) ) {
        if( noise < 10 ) {
            return { noise, burst ? _( "P-p-p-pft!" ) : _( "pft!" ) };
        } else if( noise < 20 ) {
            return { noise, burst ? _( "F-F-Foomp!" ) : _( "Foomp!" ) };
        } else if( noise < 40 ) {
            return { noise, burst ? _( "Thk-Thk-Thunk!" ) : _( "Thunk!" ) };
        } else {
            return { noise, burst ? _( "Chuk-chunk!" ) : _( "Chunk!" ) };
        }

    } else if( type->weapon_category.contains( weapon_cat_ROCKET_LAUNCHERS ) ) {
        return { noise, _( "Fwsss!" ) };
    } else if( type->weapon_category.contains( weapon_cat_GRENADE_LAUNCHERS ) ) {
        return { noise, _( "Thump!" ) };
    } else if( type->weapon_category.contains( weapon_cat_FLAMETHROWERS ) ||
               type->weapon_category.contains( weapon_cat_SPRAY_GUNS ) ) {
        return { noise, _( "Fwoosh!" ) };
    } else if( type->weapon_category.contains( weapon_cat_S_XBOWS ) ||
               type->weapon_category.contains( weapon_cat_M_XBOWS ) ) {
        return { noise, _( "thonk!" ) };
    } else if( type->weapon_category.contains( weapon_cat_ELASTIC ) ) {
        return { noise, _( "whizz!" ) };
    }

    if( type->weapon_category.contains( weapon_cat_ENERGY_WEAPONS ) ) {
        // Lasers and plasma
        if( noise < 20 ) {
            return { noise, _( "Fzzt!" ) };
        } else if( noise < 40 ) {
            return { noise, _( "Pew!" ) };
        } else if( noise < 60 ) {
            return { noise, _( "Tsewww!" ) };
        } else {
            return { noise, _( "Kra-kow!" ) };
        }

        // Default behavior for normal guns without sound class defined.
    } else if( noise > 0 ) {
        if( noise < 50 ) {
            return { noise, burst ? _( "Brrrip!" ) : _( "plink!" ) };
        } else if( noise < 150 ) {
            return { noise, burst ? _( "Brrrap!" ) : _( "bang!" ) };
        } else if( noise < 175 ) {
            return { noise, burst ? _( "P-p-p-pow!" ) : _( "blam!" ) };
        } else {
            return { noise, burst ? _( "Kaboom!" ) : _( "kerblam!" ) };
        }
    }

    return { 0, "" }; // silent weapons
}

static double dispersion_from_skill( double skill, double weapon_dispersion )
{
    if( skill >= MAX_SKILL ) {
        return 0.0;
    }
    double skill_shortfall = double( MAX_SKILL ) - skill;
    double dispersion_penalty = 3 * skill_shortfall;
    double skill_threshold = 5;
    if( skill >= skill_threshold ) {
        double post_threshold_skill_shortfall = double( MAX_SKILL ) - skill;
        // Lack of mastery multiplies the dispersion of the weapon.
        return dispersion_penalty + ( weapon_dispersion * post_threshold_skill_shortfall * 1.25 ) /
               ( double( MAX_SKILL ) - skill_threshold );
    }
    // Unskilled shooters suffer greater penalties, still scaling with weapon penalties.
    double pre_threshold_skill_shortfall = skill_threshold - skill;
    dispersion_penalty += weapon_dispersion *
                          ( 1.25 + pre_threshold_skill_shortfall * 3.75 / skill_threshold );

    return dispersion_penalty;
}

// utility functions for projectile_attack
dispersion_sources ranged::get_weapon_dispersion( const Character &who, const item &obj )
{
    int weapon_dispersion = obj.gun_dispersion();
    dispersion_sources dispersion( weapon_dispersion );
    dispersion.add_range( who.ranged_dex_mod() );

    dispersion.add_range( ( who.encumb( body_part_arm_l ) + who.encumb( body_part_arm_r ) ) / 5.0 );

    if( character_funcs::is_driving( who ) ) {
        // get volume of gun (or for auxiliary gunmods the parent gun)
        const item *parent = who.has_item( obj ) ? who.find_parent( obj ) : nullptr;
        const int vol = ( parent ? parent->volume() : obj.volume() ) / 250_ml;

        /** @EFFECT_DRIVING reduces the inaccuracy penalty when using guns whilst driving */
        dispersion.add_range( std::max( vol - who.get_skill_level( skill_driving ), 1 ) * 20 );
    }

    /** @EFFECT_GUN improves usage of accurate weapons and sights */
    double avgSkill = static_cast<double>( who.get_skill_level( skill_gun ) +
                                           who.get_skill_level( obj.gun_skill() ) ) / 2.0;
    avgSkill = std::min( avgSkill, static_cast<double>( MAX_SKILL ) );

    dispersion.add_range( dispersion_from_skill( avgSkill, weapon_dispersion ) );

    if( who.has_bionic( bio_targeting ) ) {
        dispersion.add_multiplier( 0.75 );
    }
    // If we're crouched, it's easier to steady our aim.
    if( who.movement_mode_is( CMM_CROUCH ) ) {
        dispersion.add_multiplier( 0.75 );
    }

    // If using a bow you lack the strength for, increase based on how much weaker shooter is.
    dispersion.add_multiplier( 1 / ranged::str_draw_dispersion_modifier( obj, who ) );

    // Range is effectively four times longer when shooting unflagged/flagged guns underwater/out of water.
    if( who.is_underwater() != obj.has_flag( flag_UNDERWATER_GUN ) ) {
        // Adding dispersion for additional debuff
        dispersion.add_range( 150 );
        dispersion.add_multiplier( 4 );
    }

    // If user is currently able to fire a mounted gun freely, penalize dispersion
    // HEAVY_WEAPON_SUPPORT flag has highest penalty, Large mutants lower penalty, no penalty for Huge mutants.
    if( obj.has_flag( flag_MOUNTED_GUN ) && !can_use_heavy_weapon( who, get_map(), who.pos() ) ) {
        if( who.get_size() == creature_size::large ) {
            dispersion.add_range( 500 );
        } else if( who.worn_with_flag( flag_HEAVY_WEAPON_SUPPORT ) &&
                   ( who.get_size() <= creature_size::medium ) ) {
            dispersion.add_range( 1000 );
        }
    }

    return dispersion;
}

auto npc_ai::best_mode_for_range(
    const Character &who, const item &firing, int dist
) -> std::pair<gun_mode_id, std::optional<gun_mode>>
{
    const int shots = who.is_wielding( firing )
                      ? character_funcs::ammo_count_for( who, firing )
                      : item_funcs::shots_remaining( who, firing );

    if( !firing.is_gun() || firing.is_gunmod() || shots == 0 ) {
        return std::make_pair( gun_mode_id(), std::nullopt );
    }
    int min_recoil = MAX_RECOIL;
    min_recoil = ranged::get_most_accurate_sight( who, firing );
    int range = static_cast<const npc *>( &who )->confident_shoot_range( firing, min_recoil );

    if( dist > range ) {
        return  std::make_pair( gun_mode_id(), std::nullopt );
    }

    const auto gun_mode_cmp = []( const std::pair<gun_mode_id, gun_mode> &lhs,
    const std::pair<gun_mode_id, gun_mode> &rhs ) {
        return lhs.second.qty < rhs.second.qty;
    };

    const std::map<gun_mode_id, gun_mode> &to_add = firing.gun_all_modes();
    std::vector<std::pair<gun_mode_id, gun_mode>> modes;
    std::copy( to_add.begin(), to_add.end(), std::back_inserter( modes ) );

    erase_if( modes, [&]( const std::pair<gun_mode_id, gun_mode> &e ) {
        const auto &m = e.second;
        return m.melee() || !who.can_use( *m.target ) || m.flags.contains( "NPC_AVOID" ) || shots < m.qty ||
               ( dist > 0 && static_cast<const npc *>( &who )->confident_gun_mode_range( m, min_recoil ) < dist );
    } );

    if( modes.empty() ) {
        return  std::make_pair( gun_mode_id(), std::nullopt );
    }

    const auto g_mode = std::max_element( modes.begin(), modes.end(), gun_mode_cmp );
    return *g_mode;
}

double npc_ai::gun_value( const Character &who, const item &weap, int ammo )
{
    // TODO: Mods
    // TODO: Allow using a specified type of ammo rather than default or current
    if( !weap.type->gun ) {
        add_msg( m_debug, "%s is not a gun, gun_value set to 0", weap.type->get_id().str() );
        return 0.0;
    }

    if( ammo <= 0 ) {
        add_msg( m_debug, "%s has no ammo, gun_value set to 0", weap.type->get_id().str() );
        return 0.0;
    }

    item &ideal_weapon = *item::spawn_temporary( weap );
    if( !ideal_weapon.ammo_default().is_null() ) {
        ideal_weapon.ammo_set( ideal_weapon.ammo_default(), -1 );
    }

    auto [mode_id, mode_] = npc_ai::best_mode_for_range( who, ideal_weapon, -1 );

    // Doesn't use calculate_dispersion because that requires a map
    // TODO: Turn this into a common function.
    dispersion_sources mode_disp = ranged::get_weapon_dispersion( who, ideal_weapon );
    double total_dispersion = mode_disp.max();
    if( ideal_weapon.ammo_current() ) {
        total_dispersion += ideal_weapon.ammo_current()->ammo->dispersion;
    }
    // Necessary so that ideal_ranged_dps will include the reload cost in calc.
    ideal_weapon.ammo_unset();
    double dps = ideal_weapon.ideal_ranged_dps( who, mode_ );

    // "Medium range" below means 9 tiles, "short range" means 4
    // Those are guarantees (assuming maximum time spent aiming)
    static const std::vector<std::pair<float, float>> dispersion_thresholds = {
        {
            // Headshots all the time
            { 0.0f, 2.0f },
            // Critical at medium range
            { 100.0f, 1.8f },
            // Critical at short range or good hit at medium
            { 200.0f, 1.6f },
            // OK hits at medium
            { 300.0f, 1.45f },
            // Point blank headshots
            { 450.0f, 1.30f },
            // OK hits at short
            { 700.0f, 1.15f },
            // Glances at medium, criticals at point blank
            { 1000.0f, 1.0f },
            // Nothing guaranteed, pure gamble
            { 2000.0f, 0.1f },
        }
    };

    float dispersion_factor = multi_lerp( dispersion_thresholds, total_dispersion );

    double gun_value = dps * dispersion_factor;

    add_msg( m_debug, "%s as gun: %.1f total, %.1f dispersion, %.1f dps",
             weap.type->get_id().str(), gun_value, dispersion_factor, dps );
    return std::max( 0.0, gun_value );
}

double ranged::recoil_vehicle( const Character &who )
{
    // TODO: vary penalty dependent upon vehicle part on which player is boarded

    if( who.in_vehicle ) {
        if( const optional_vpart_position vp = get_map().veh_at( who.pos() ) ) {
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            return static_cast<double>( std::abs( vp->vehicle().velocity ) ) * 3 / 100;
        }
    }
    return 0;
}

double ranged::recoil_total( const Character &who )
{
    return who.recoil + recoil_vehicle( who );
}

namespace ranged
{

std::vector<Creature *> targetable_creatures( const Character &c, const int range )
{
    return targetable_creatures( c, range, turret_data() );
}

std::vector<Creature *> targetable_creatures( const Character &c, const int range,
        const turret_data &turret )
{
    const vehicle *veh_from_turret = turret ? turret.get_veh() : nullptr;
    return g->get_creatures_if( [&c, range, veh_from_turret]( const Creature & critter ) -> bool {
        if( std::round( rl_dist_exact( c.pos(), critter.pos() ) ) > range )
        {
            return false;
        }

        // Special case: if range is 1, it's a melee attack.
        // Melee attacks can only target on same z-level or directly up/down, not "z-diagonally".
        if( range <= 1 && c.posz() != critter.posz() && c.pos().xy() != critter.pos().xy() )
        {
            return false;
        }

        if( !c.sees( critter ) && !c.sees_with_infrared( critter ) )
        {
            return false;
        }

        // TODO: get rid of fake npcs (pos() check)
        if( &c == &critter || c.pos() == critter.pos() || c.attitude_to( critter ) == Attitude::A_FRIENDLY )
        {
            return false;
        }

        map &here = get_map();

        // TODO: It should use projectile passability checks when finding path, not vision checks.
        std::vector<tripoint> path = here.find_clear_path( c.pos(), critter.pos() );
        tripoint prev_point = c.pos();
        for( const tripoint &point : path )
        {
            if( here.obstructed_by_vehicle_rotation( prev_point, point ) ) {
                //Blocked by a rotated vehicle's walls
                return false;
            }

            prev_point = point;

            if( here.passable( point ) ) {
                // If it's passable, it doesn't block bullets
                continue;
            }

            const vehicle *veh_at_point = veh_pointer_or_null( here.veh_at( point ) );
            if( veh_at_point && veh_at_point != veh_from_turret ) {
                // Vehicles don't have impassable-but-shootable-through parts
                return false;
            }
            if( !here.has_flag_ter( TFLAG_TRANSPARENT, point ) ) {
                // If it's transparent, it's either glass (fine) or reinforced glass (not fine)
                // Hack it with the more common case for now
                // TODO: Handle armored glass
                return false;
            }
        }

        return true;
    } );
}

int burst_penalty( const Character &p, const item &gun, int gun_recoil )
{
    ///\EFFECT_DEX reduces burst penalty by flat amount
    int dex_effect = p.get_dex() * 10;
    ///\EFFECT_STR reduces burst fire penalty
    float str_effect = p.get_str() * 0.5f;

    /** @EFFECT_PISTOL reduces burst fire penalty */
    /** @EFFECT_SMG reduces burst fire penalty */
    /** @EFFECT_RIFLE reduces burst fire penalty */
    /** @EFFECT_SHOTGUN reduces burst fire penalty */
    int skill_lvl = std::min( p.get_skill_level( gun.gun_skill() ), MAX_SKILL );

    return std::max<int>( 0, 3 * ( gun_recoil - dex_effect ) / std::max( 1.0f,
                          str_effect + skill_lvl ) );
}

} // namespace ranged

target_handler::trajectory target_ui::run()
{
    if( mode == TargetMode::Spell && !no_mana && !casting->can_cast( *you ) ) {
        you->add_msg_if_player( m_bad, _( "You don't have enough %s to cast this spell" ),
                                casting->energy_string() );
    }
    if( mode == TargetMode::Fire || mode == TargetMode::TurretManual || mode == TargetMode::Shape ) {
        ensure_ranged_gun_mode();
        update_ammo_range_from_gun_mode();
        if( mode == TargetMode::Fire ) {
            sight_dispersion = ranged::effective_dispersion( *you, relevant->sight_dispersion() );
        }
    }

    map &here = get_map();
    // Load settings
    allow_zlevel_shift = here.has_zlevels() && get_option<bool>( "FOV_3D" );
    snap_to_target = get_option<bool>( "SNAP_TO_TARGET" );
    if( mode == TargetMode::Turrets ) {
        // Due to how cluttered the display would become, disable it by default
        // unless aiming a single turret.
        draw_turret_lines = vturrets->size() == 1;
    }

    avatar &player_character = *you;
    on_out_of_scope cleanup( [&here, &player_character]() {
        here.invalidate_map_cache( player_character.pos().z + player_character.view_offset.z );
    } );

    shared_ptr_fast<game::draw_callback_t> target_ui_cb = make_shared_fast<game::draw_callback_t>(
    [&]() {
        draw_terrain_overlay();
    } );
    g->add_draw_callback( target_ui_cb );

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        init_window_and_input();
        ui.position_from_window( w_target );
    } );
    ui.mark_resize();

    ui.on_redraw( [&]( const ui_adaptor & ) {
        draw_ui_window();
    } );

    // Handle multi-turn aiming
    std::string action;
    bool attack_was_confirmed = false;
    bool reentered = false;
    bool resume_critter = false;
    if( mode == TargetMode::Fire && !activity->action.empty() ) {
        // We were in this UI during previous turn...
        reentered = true;
        std::string act_data = activity->action;
        if( act_data == "AIM" ) {
            // ...and ran out of moves while aiming.
        } else {
            // ...and selected 'aim and shoot', but ran out of moves.
            // So, skip retrieving input and go straight to the action.
            action = act_data;
            attack_was_confirmed = true;
        }
        // Load state to keep the ui consistent across turns
        snap_to_target = activity->snap_to_target;
        shifting_view = activity->shifting_view;
        resume_critter = activity->aiming_at_critter;
    }

    // Initialize cursor position
    src = you->pos();
    update_target_list();

    if( activity && activity->abort_if_no_targets && targets.empty() ) {
        // this branch is taken when already shot once and re-entered
        // aiming, if no targets are available we want to abort so
        // players don't arrive at aiming ui with nothing to shoot at.
        activity->aborted = true;
        traj.clear();
        return traj;
    }
    tripoint initial_dst = src;
    if( reentered ) {
        if( !try_reacquire_target( resume_critter, initial_dst ) ) {
            // Target lost
            action.clear();
            attack_was_confirmed = false;
        }
    } else {
        initial_dst = choose_initial_target();
    }
    set_cursor_pos( initial_dst );
    if( dst != initial_dst ) {
        // Our target moved out of range
        action.clear();
        attack_was_confirmed = false;
    }
    if( mode == TargetMode::Fire ) {
        if( activity->aif_duration > AIF_DURATION_LIMIT ) {
            // Break long (potentially infinite) aim-and-fire loop.
            // May happen if e.g. avatar tries to get 'precise' shot while being
            // attacked by multiple zombies, which triggers dodges and corresponding aim loss.
            action.clear();
            attack_was_confirmed = false;
        }
        if( !action.empty() && !prompt_friendlies_in_lof() ) {
            // A friendly creature moved into line of fire during aim-and-shoot,
            // and player decided to stop aiming
            action.clear();
            attack_was_confirmed = false;
        }
        activity->acceptable_losses.clear();
        if( action.empty() ) {
            activity->aif_duration = 0;
        } else {
            activity->aif_duration += 1;
        }
    }

    // Event loop!
    ExitCode loop_exit_code;
    std::string timed_out_action;
    bool skip_redraw = false;
    for( ;; action.clear() ) {
        if( !skip_redraw ) {
            g->invalidate_main_ui_adaptor();
            ui_manager::redraw();
        }
        skip_redraw = false;

        // Wait for user input (or use value retrieved from activity)
        if( action.empty() ) {
            int timeout = get_option<int>( "EDGE_SCROLL" );
            action = ctxt.handle_input( timeout );
        }

        // If an aiming mode is selected, use "*_SHOT" instead of "FIRE"
        if( mode == TargetMode::Fire && action == "FIRE" && aim_mode->has_threshold ) {
            action = aim_mode->action;
        }

        // Handle received input
        if( handle_cursor_movement( action, skip_redraw ) ) {
            continue;
        } else if( action == "TOGGLE_SNAP_TO_TARGET" ) {
            toggle_snap_to_target();
        } else if( action == "TOGGLE_TURRET_LINES" ) {
            draw_turret_lines = !draw_turret_lines;
        } else if( action == "TOGGLE_MOVE_CURSOR_VIEW" ) {
            if( snap_to_target ) {
                toggle_snap_to_target();
            }
            shifting_view = !shifting_view;
        } else if( action == "zoom_in" ) {
            g->zoom_in();
            g->mark_main_ui_adaptor_resize();
        } else if( action == "zoom_out" ) {
            g->zoom_out();
            g->mark_main_ui_adaptor_resize();
        } else if( action == "QUIT" ) {
            loop_exit_code = ExitCode::Abort;
            break;
        } else if( action == "SWITCH_MODE" ) {
            action_switch_mode();
        } else if( action == "SWITCH_AMMO" ) {
            if( !action_switch_ammo() ) {
                loop_exit_code = ExitCode::Reload;
                break;
            }
        } else if( action == "FIRE" ) {
            if( status != Status::Good ) {
                continue;
            }
            bool can_skip_confirm = ( mode == TargetMode::Spell && casting->damage() <= 0 );
            if( !can_skip_confirm && !confirm_non_enemy_target() ) {
                continue;
            }
            set_last_target();
            loop_exit_code = ExitCode::Fire;
            break;
        } else if( action == "AIM" ) {
            if( status != Status::Good ) {
                continue;
            }

            // No confirm_non_enemy_target here because we have not initiated the firing.
            // Aiming can be stopped / aborted at any time.

            if( !action_aim() ) {
                timed_out_action = "AIM";
                loop_exit_code = ExitCode::Timeout;
                break;
            }
        } else if( action == "AIMED_SHOT" || action == "CAREFUL_SHOT" || action == "PRECISE_SHOT" ) {
            if( status != Status::Good ) {
                continue;
            }

            // This action basically means "Fire" as well; the actual firing may be delayed
            // through aiming, but there is usually no means to abort it. Therefore we query now
            if( !attack_was_confirmed && !confirm_non_enemy_target() ) {
                continue;
            }

            if( action_aim_and_shoot( action ) ) {
                loop_exit_code = ExitCode::Fire;
            } else {
                timed_out_action = action;
                loop_exit_code = ExitCode::Timeout;
            }
            break;
        }
    } // for(;;)

    switch( loop_exit_code ) {
        case ExitCode::Abort: {
            traj.clear();
            if( mode == TargetMode::Fire || mode == TargetMode::Shape ) {
                activity->aborted = true;
            }
            break;
        }
        case ExitCode::Fire: {
            bool harmful = !( mode == TargetMode::Spell && casting->damage() <= 0 );
            on_target_accepted( harmful );
            break;
        }
        case ExitCode::Timeout: {
            // We've ran out of moves, save UI state
            activity->acceptable_losses = list_friendlies_in_lof();
            traj.clear();
            activity->action = timed_out_action;
            activity->snap_to_target = snap_to_target;
            activity->shifting_view = shifting_view;
            activity->aiming_at_critter = !!dst_critter;
            break;
        }
        case ExitCode::Reload: {
            traj.clear();
            activity->aborted = true;
            activity->reload_requested = true;
            break;
        }
    }

    return traj;
}

void target_ui::init_window_and_input()
{
    std::string display_type = get_option<std::string>( "ACCURACY_DISPLAY" );
    std::string panel_type = panel_manager::get_manager().get_current_layout_id();
    narrow = ( panel_type == "compact" || panel_type == "labels-narrow" );

    int top = 0;
    int width;
    int height;
    if( narrow ) {
        // Narrow layout removes the list of controls. This allows us
        // to have small window size and not suffer from it.
        width = 34;
        height = 24;
        compact = true;
    } else {
        width = 55;
        compact = TERMY < 41;
        tiny = TERMY < 28;
        bool use_whole_sidebar = TERMY < 32;
        if( use_whole_sidebar ) {
            // If we're extremely short on space, use the whole sidebar.
            height = TERMY;
        } else if( compact ) {
            // Cover up more low-value ui elements if we're tight on space.
            height = 28;
        } else {
            // Go all out
            height = 32;
        }
    }

    w_target = catacurses::newwin( height, width, point( TERMX - width, top ) );

    ctxt = input_context( "TARGET" );
    ctxt.set_iso( true );
    ctxt.register_directions();
    ctxt.register_action( "COORDINATE" );
    ctxt.register_action( "SELECT" );
    ctxt.register_action( "FIRE" );
    ctxt.register_action( "NEXT_TARGET" );
    ctxt.register_action( "PREV_TARGET" );
    ctxt.register_action( "CENTER" );
    ctxt.register_action( "TOGGLE_SNAP_TO_TARGET" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "MOUSE_MOVE" );
    ctxt.register_action( "zoom_out" );
    ctxt.register_action( "zoom_in" );
    ctxt.register_action( "TOGGLE_MOVE_CURSOR_VIEW" );
    if( allow_zlevel_shift ) {
        ctxt.register_action( "LEVEL_UP" );
        ctxt.register_action( "LEVEL_DOWN" );
    }
    if( mode == TargetMode::Fire || mode == TargetMode::TurretManual || ( mode == TargetMode::Shape &&
            relevant->is_gun() ) ) {
        ctxt.register_action( "SWITCH_MODE" );
        if( mode == TargetMode::TurretManual || relevant->has_flag( flag_RELOAD_AND_SHOOT ) ) {
            // Turrets may support multiple ammo types.
            // RELOAD_AND_SHOOT weapons use whatever ammo is favorite.
            ctxt.register_action( "SWITCH_AMMO" );
        }
    }
    if( mode == TargetMode::Fire ) {
        ctxt.register_action( "AIM" );

        aim_types = ranged::get_aim_types( *you, *relevant );
        for( ranged::aim_type &type : aim_types ) {
            if( type.has_threshold ) {
                ctxt.register_action( type.action );
            }
        }
        aim_mode = aim_types.begin();
        for( auto it = aim_types.begin(); it != aim_types.end(); ++it ) {
            if( you->preferred_aiming_mode == it->action ) {
                aim_mode = it; // default to persisted mode if possible
            }
        }
    }
    if( mode == TargetMode::Turrets ) {
        ctxt.register_action( "TOGGLE_TURRET_LINES" );
    }
}

bool target_ui::handle_cursor_movement( const std::string &action, bool &skip_redraw )
{
    std::optional<tripoint> mouse_pos;
    const auto shift_view_or_cursor = [this]( const tripoint & delta ) {
        if( this->shifting_view ) {
            this->set_view_offset( this->you->view_offset + delta );
        } else {
            this->set_cursor_pos( dst + delta );
        }
    };

    if( action == "MOUSE_MOVE" || action == "TIMEOUT" ) {
        // Shift pos and/or view via edge scrolling
        tripoint edge_scroll = g->mouse_edge_scrolling_terrain( ctxt );
        if( edge_scroll == tripoint_zero ) {
            skip_redraw = true;
        } else {
            if( action == "MOUSE_MOVE" ) {
                edge_scroll *= 2;
            }
            if( snap_to_target ) {
                set_cursor_pos( dst + edge_scroll );
            } else {
                set_view_offset( you->view_offset + edge_scroll );
            }
        }
    } else if( const std::optional<tripoint> delta = ctxt.get_direction( action ) ) {
        // Shift view/cursor with directional keys
        shift_view_or_cursor( *delta );
    } else if( action == "SELECT" && ( mouse_pos = ctxt.get_coordinates( g->w_terrain ) ) ) {
        // Set pos by clicking with mouse
        mouse_pos->z = you->pos().z + you->view_offset.z;
        set_cursor_pos( *mouse_pos );
    } else if( action == "LEVEL_UP" || action == "LEVEL_DOWN" ) {
        // Shift view/cursor up/down one z level
        tripoint delta = tripoint(
                             0,
                             0,
                             action == "LEVEL_UP" ? 1 : -1
                         );
        shift_view_or_cursor( delta );
    } else if( action == "NEXT_TARGET" ) {
        cycle_targets( 1 );
    } else if( action == "PREV_TARGET" ) {
        cycle_targets( -1 );
    } else if( action == "CENTER" ) {
        if( shifting_view ) {
            set_view_offset( tripoint_zero );
        } else {
            set_cursor_pos( src );
        }
    } else {
        return false;
    }

    return true;
}

bool target_ui::set_cursor_pos( const tripoint &new_pos )
{
    if( dst == new_pos ) {
        return false;
    }
    if( status == Status::OutOfAmmo && new_pos != src ) {
        // range == 0, no sense in moving cursor
        return false;
    }

    // Make sure new position is valid or find a closest valid position
    std::vector<tripoint> new_traj;
    tripoint valid_pos = new_pos;
    map &here = get_map();
    if( new_pos != src ) {
        // On Z axis, make sure we do not exceed map boundaries
        valid_pos.z = clamp( valid_pos.z, -OVERMAP_DEPTH, OVERMAP_HEIGHT );
        // Or current view range
        valid_pos.z = clamp( valid_pos.z - src.z, -fov_3d_z_range, fov_3d_z_range ) + src.z;

        new_traj = here.find_clear_path( src, valid_pos );
        if( range == 1 ) {
            // We should always be able to hit adjacent squares
            if( square_dist( src, valid_pos ) > 1 ) {
                valid_pos = new_traj[0];
            }
        } else if( trigdist ) {
            if( dist_fn( valid_pos ) > range ) {
                // Find the farthest point that is still in range
                for( size_t i = new_traj.size(); i > 0; i-- ) {
                    if( dist_fn( new_traj[i - 1] ) <= range ) {
                        valid_pos = new_traj[i - 1];
                        break;
                    }
                }
                // Sanity check
                if( dist_fn( valid_pos ) > range ) {
                    debugmsg( "Calculated trajectory exceeds allowed range!" );
                    valid_pos = src;
                }
            }
        } else {
            tripoint delta = valid_pos - src;
            valid_pos = src + tripoint(
                            clamp( delta.x, -range, range ),
                            clamp( delta.y, -range, range ),
                            clamp( delta.z, -range, range )
                        );
        }
    } else {
        new_traj.push_back( src );
    }

    if( valid_pos == dst ) {
        // We don't need to move the cursor after all
        return false;
    } else if( new_pos == valid_pos ) {
        // We can reuse new_traj
        dst = valid_pos;
        traj = new_traj;
    } else {
        dst = valid_pos;
        traj = here.find_clear_path( src, dst );
    }

    if( snap_to_target ) {
        set_view_offset( dst - src );
    }

    // Make player's sprite flip to face the current target
    point d( dst.xy() - src.xy() );
    if( !tile_iso ) {

        if( d.x > 0 ) {
            you->facing = FacingDirection::FD_RIGHT;
        } else if( d.x < 0 ) {
            you->facing = FacingDirection::FD_LEFT;
        }
    } else {
        if( d.x >= 0 && d.y >= 0 ) {
            you->facing = FacingDirection::FD_RIGHT;
        }
        if( d.y <= 0 && d.x <= 0 ) {
            you->facing = FacingDirection::FD_LEFT;
        }
    }

    // Cache creature under cursor
    if( src != dst ) {
        Creature *cr = g->critter_at( dst, true );
        if( cr && pl_sees( *cr ) ) {
            dst_critter = cr;
        } else {
            dst_critter = nullptr;
        }
    } else {
        dst_critter = nullptr;
    }

    // Update mode-specific stuff
    if( mode == TargetMode::Fire ) {
        recalc_aim_turning_penalty();
    } else if( mode == TargetMode::Spell ) {
        const std::string fx = casting->effect();
        if( fx == "target_attack" || fx == "projectile_attack" || fx == "ter_transform" ) {
            spell_aoe = spell_effect::spell_effect_blast( *casting, src, dst, casting->aoe(), true );
        } else if( fx == "cone_attack" ) {
            spell_aoe = spell_effect::spell_effect_cone( *casting, src, dst, casting->aoe(), true );
        } else if( fx == "line_attack" ) {
            spell_aoe = spell_effect::spell_effect_line( *casting, src, dst, casting->aoe(), true );
        } else {
            spell_aoe.clear();
        }
    } else if( mode == TargetMode::Turrets ) {
        update_turrets_in_range();
    } else if( mode == TargetMode::Shape ) {
        std::shared_ptr<shape> sh = shape_gen->create( src, dst );
        projectile proj = make_gun_projectile( *relevant );
        // Same as in map::shoot (should probably be a function!)
        int expected_bash_force = std::accumulate( proj.impact.begin(), proj.impact.end(), 0.0,
        []( double acc, const damage_unit & du ) {
            return acc + du.amount + du.res_pen;
        } );
        shape_coverage = ranged::expected_coverage( *sh, here, expected_bash_force );
    }

    // Update UI controls & colors
    update_status();

    return true;
}

void target_ui::on_range_ammo_changed()
{
    update_status();
    update_target_list();
}

void target_ui::update_target_list()
{
    if( range == 0 ) {
        targets.clear();
        return;
    }

    // Get targets in range and sort them by distance (targets[0] is the closest)

    if( mode == TargetMode::TurretManual ) {
        targets = ranged::targetable_creatures( *you, range, *turret );
    } else {
        targets = ranged::targetable_creatures( *you, range );
    }

    map &here = get_map();
    const auto player_pos = you->pos();

    std::ranges::sort( targets, {}, [&]( const Creature * c ) -> std::tuple<bool, bool, int> {
        const auto target_pos = c->pos();
        const bool same_z = player_pos.z == target_pos.z;
        const bool has_los = here.sees( player_pos, target_pos, range );
        return { !has_los, !same_z, rl_dist_exact( target_pos, player_pos ) };
    } );
}

tripoint target_ui::choose_initial_target()
{
    // If we're casting a spell, don't lock onto enemies if the spell is meant for using on friendlies.
    if( mode == TargetMode::Spell && !casting->is_valid_target( valid_target::target_hostile ) ) {
        return src;
    }

    // Try previously targeted creature
    shared_ptr_fast<Creature> cr = you->last_target.lock();
    if( cr && pl_sees( *cr ) && dist_fn( cr->pos() ) <= range ) {
        return cr->pos();
    }

    // Try closest creature
    if( !targets.empty() ) {
        return targets[0]->pos();
    }

    // Try closest practice target
    map &here = get_map();
    const std::vector<tripoint> nearby = closest_points_first( src, range );
    const auto target_spot = std::find_if( nearby.begin(), nearby.end(),
    [this, &here]( const tripoint & pt ) {
        return ( here.has_flag_ter_or_furn( flag_SHOOT_ME, pt ) ||
                 here.tr_at( pt ).id == tr_practice_target ) && this->you->sees( pt );
    } );
    if( target_spot != nearby.end() ) {
        return *target_spot;
    }

    // We've got nothing.
    return src;
}

bool target_ui::try_reacquire_target( bool critter, tripoint &new_dst )
{
    if( critter ) {
        // Try to re-acquire the creature
        shared_ptr_fast<Creature> cr = you->last_target.lock();
        if( cr && pl_sees( *cr ) && dist_fn( cr->pos() ) <= range ) {
            new_dst = cr->pos();
            return true;
        }
    }

    if( !you->last_target_pos.has_value() ) {
        // This shouldn't happen
        return false;
    }

    // Try to re-acquire target tile or tile where the target creature used to be
    tripoint local_lt = get_map().getlocal( *you->last_target_pos );
    if( dist_fn( local_lt ) <= range ) {
        new_dst = local_lt;
        // Abort aiming if a creature moved in
        return !critter && !g->critter_at( local_lt, true );
    }

    // We moved out of range
    return false;
}

void target_ui::update_status()
{
    std::vector<std::string> msgbuf;
    if( mode == TargetMode::Turrets && turrets_in_range.empty() ) {
        // None of the turrets are in range
        status = Status::OutOfRange;
    } else if( mode == TargetMode::Fire &&
               ( !ranged::gunmode_checks_common( *you, get_map(), msgbuf, relevant->gun_current_mode() ) ||
                 !ranged::gunmode_checks_weapon( *you, get_map(), msgbuf, relevant->gun_current_mode() ) ) ) {
        // Selected gun mode is empty
        // TODO: it might be some other error, but that's highly unlikely to happen, so a catch-all 'Out of ammo' is fine
        status = Status::OutOfAmmo;
    } else if( mode == TargetMode::TurretManual && ( turret->query() != turret_data::status::ready ||
               !ranged::gunmode_checks_common( *you, get_map(), msgbuf, relevant->gun_current_mode() ) ) ) {
        status = Status::OutOfAmmo;
    } else if( ( src == dst ) && !( mode == TargetMode::Spell &&
                                    casting->is_valid_target( valid_target::target_self ) ) ) {
        // TODO: consider allowing targeting yourself with turrets
        status = Status::BadTarget;
    } else if( dist_fn( dst ) > range ) {
        // We're out of range. This can happen if we switch from long-ranged
        // gun mode to short-ranged. We can, of course, move the cursor into range automatically,
        // but that would be rude. Instead, wait for directional keys/etc. and *then* move the cursor.
        status = Status::OutOfRange;
    } else {
        status = Status::Good;
    }
}

int target_ui::dist_fn( const tripoint &p )
{
    return static_cast<int>( std::round( rl_dist_exact( src, p ) ) );
}

void target_ui::set_last_target()
{
    you->last_target_pos = get_map().getabs( dst );
    if( dst_critter ) {
        you->last_target = g->shared_from( *dst_critter );
    } else {
        you->last_target.reset();
    }
}

bool target_ui::confirm_non_enemy_target()
{
    npc *const who = dynamic_cast<npc *>( dst_critter );
    if( who && !who->guaranteed_hostile() ) {
        return query_yn( _( "Really attack %s?" ), who->name.c_str() );
    }
    return true;
}

bool target_ui::prompt_friendlies_in_lof()
{
    if( mode != TargetMode::Fire ) {
        debugmsg( "Not implemented" );
        return true;
    }

    std::vector<weak_ptr_fast<Creature>> in_lof = list_friendlies_in_lof();
    std::vector<Creature *> new_in_lof;
    for( const weak_ptr_fast<Creature> &cr_ptr : in_lof ) {
        bool found = false;
        shared_ptr_fast<Creature> ptr_lock = cr_ptr.lock();
        Creature *cr = ptr_lock.get();
        for( const weak_ptr_fast<Creature> &cr2_ptr : activity->acceptable_losses ) {
            shared_ptr_fast<Creature> ptr2_lock = cr2_ptr.lock();
            Creature *cr2 = ptr2_lock.get();
            if( cr == cr2 ) {
                found = true;
                break;
            }
        }
        if( !found ) {
            new_in_lof.push_back( cr );
        }
    }

    if( new_in_lof.empty() ) {
        return true;
    }

    std::string msg = _( "There are friendly creatures in line of fire:\n" );
    for( Creature *cr : new_in_lof ) {
        msg += "  " + cr->disp_name() + "\n";
    }
    msg += _( "Proceed with the attack?" );
    return query_yn( msg );
}

std::vector<weak_ptr_fast<Creature>> target_ui::list_friendlies_in_lof()
{
    std::vector<weak_ptr_fast<Creature>> ret;
    if( mode == TargetMode::Turrets || mode == TargetMode::Spell ) {
        debugmsg( "Not implemented" );
        return ret;
    }
    for( const tripoint &p : traj ) {
        if( p != dst && p != src ) {
            Creature *cr = g->critter_at( p, true );
            if( cr && you->sees( *cr ) ) {
                Attitude a = cr->attitude_to( *this->you );
                if(
                    ( cr->is_npc() && a != Attitude::A_HOSTILE ) ||
                    ( !cr->is_npc() && a == Attitude::A_FRIENDLY )
                ) {
                    ret.emplace_back( g->shared_from( *cr ) );
                }
            }
        }
    }
    return ret;
}

void target_ui::toggle_snap_to_target()
{
    shifting_view = false;
    if( snap_to_target ) {
        // Keep current view offset
    } else {
        set_view_offset( dst - src );
    }
    snap_to_target = !snap_to_target;
}

void target_ui::cycle_targets( int direction )
{
    if( targets.empty() ) {
        // Nothing to cycle
        return;
    }

    if( dst_critter ) {
        auto t = std::find( targets.begin(), targets.end(), dst_critter );
        size_t new_target = 0;
        if( t != targets.end() ) {
            size_t idx = std::distance( targets.begin(), t );
            new_target = ( idx + targets.size() + direction ) % targets.size();
            set_cursor_pos( targets[new_target]->pos() );
            return;
        }
    }

    // There is either no creature under the cursor or the player can't see it.
    // Use the closest/farthest target in this case
    if( direction == 1 ) {
        set_cursor_pos( targets.front()->pos() );
    } else {
        set_cursor_pos( targets.back()->pos() );
    }
}

void target_ui::set_view_offset( const tripoint &new_offset )
{
    tripoint new_( new_offset.xy(), clamp( new_offset.z, -fov_3d_z_range, fov_3d_z_range ) );
    new_.z = clamp( new_.z + src.z, -OVERMAP_DEPTH, OVERMAP_HEIGHT ) - src.z;

    bool changed_z = you->view_offset.z != new_.z;
    you->view_offset = new_;
    if( changed_z ) {
        // We need to do a bunch of cache updates since we're
        // looking at a different z-level.
        get_map().invalidate_map_cache( new_.z );
    }
}

void target_ui::update_turrets_in_range()
{
    turrets_in_range.clear();
    for( vehicle_part *t : *vturrets ) {
        turret_data td = veh->turret_query( *t );
        if( td.in_range( dst ) ) {
            tripoint src = veh->global_part_pos3( *t );
            turrets_in_range.push_back( { t, line_to( src, dst ) } );
        }
    }
}

void target_ui::recalc_aim_turning_penalty()
{
    if( status != Status::Good ) {
        // We don't care about invalid situations
        predicted_recoil = MAX_RECOIL;
        return;
    }

    double curr_recoil = you->recoil;
    tripoint curr_recoil_pos;
    shared_ptr_fast<Creature> ptr_lock = you->last_target.lock();
    const Creature *lt_ptr = ptr_lock.get();
    if( lt_ptr ) {
        curr_recoil_pos = lt_ptr->pos();
    } else if( you->last_target_pos ) {
        curr_recoil_pos = get_map().getlocal( *you->last_target_pos );
    } else {
        curr_recoil_pos = src;
    }

    if( curr_recoil_pos == dst ) {
        // We're aiming at that point right now, no penalty
        predicted_recoil = curr_recoil;
    } else if( curr_recoil_pos == src ) {
        // The player wasn't aiming anywhere, max it out
        predicted_recoil = MAX_RECOIL;
    } else {
        // Raise it proportionally to how much
        // the player has to turn from previous aiming point
        const double recoil_per_degree = MAX_RECOIL / 180.0;
        const units::angle angle_curr = coord_to_angle( src, curr_recoil_pos );
        const units::angle angle_desired = coord_to_angle( src, dst );
        const units::angle phi = normalize( angle_curr - angle_desired );
        const units::angle angle = std::min( phi, 360.0_degrees - phi );
        predicted_recoil =
            std::min( MAX_RECOIL, curr_recoil + to_degrees( angle ) * recoil_per_degree );
    }
}

void target_ui::apply_aim_turning_penalty()
{
    you->recoil = predicted_recoil;
}

void target_ui::action_switch_mode()
{
    uilist menu;
    menu.settext( _( "Select preferences" ) );

    std::vector<std::function<void()>> on_select;

    if( !aim_types.empty() ) {
        menu.addentry( -1, false, 0, "  " + std::string( _( "Default aiming mode" ) ) );
        menu.entries.back().force_color = true;
        menu.entries.back().text_color = c_cyan;

        for( auto it = aim_types.begin(); it != aim_types.end(); ++it ) {
            const bool is_active_aim_mode = aim_mode == it;
            const std::string text = ( it->name.empty() ? _( "Immediate" ) : it->name ) +
                                     ( is_active_aim_mode ? _( " (active)" ) : "" );

            menu.addentry( on_select.size(), true, MENU_AUTOASSIGN, text );
            on_select.emplace_back( [it, this]() {
                aim_mode = it;
                you->preferred_aiming_mode = it->action;
            } );
            if( is_active_aim_mode ) {
                menu.entries.back().text_color = c_light_green;
            }
        }
    }

    const std::map<gun_mode_id, gun_mode> &all_gun_modes = relevant->gun_all_modes();
    if( !all_gun_modes.empty() ) {
        menu.addentry( -1, false, 0, "  " + std::string( _( "Firing mode" ) ) );
        menu.entries.back().force_color = true;
        menu.entries.back().text_color = c_cyan;

        for( const auto &mode : all_gun_modes ) {
            if( mode.second.melee() ) {
                continue;
            }
            const bool active_gun_mode = relevant->gun_get_mode_id() == mode.first;

            // If gun mode is from a gunmod use gunmod's name, pay attention to the "->" on tname
            std::string text = ( mode.second.target == relevant )
                               ? mode.second.tname()
                               : mode.second->tname() + " (" + std::to_string( mode.second.qty ) + ")";

            text += ( active_gun_mode ? _( " (active)" ) : "" );

            menu.entries.emplace_back( static_cast<int>( on_select.size() ), true, MENU_AUTOASSIGN, text );
            on_select.emplace_back( [mode, this]() {
                relevant->gun_set_mode( mode.first );
            } );
            if( active_gun_mode ) {
                menu.entries.back().text_color = c_light_green;
                if( menu.selected == 0 ) {
                    menu.selected = menu.entries.size() - 1;
                }
            }
        }
    }

    menu.query();
    if( menu.ret >= 0 && menu.ret < static_cast<int>( on_select.size() ) ) {
        size_t i = static_cast<size_t>( menu.ret );
        on_select[i]();
    } // else - just refresh

    ensure_ranged_gun_mode();
    update_ammo_range_from_gun_mode();
    on_range_ammo_changed();
}

void target_ui::ensure_ranged_gun_mode()
{
    while( relevant->gun_current_mode().melee() ) {
        relevant->gun_cycle_mode();
    }
}

void target_ui::update_ammo_range_from_gun_mode()
{
    if( mode == TargetMode::TurretManual ) {
        itype_id ammo_current = turret->ammo_current();
        // Test no-ammo and not a UPS weapon
        if( !ammo_current && ( relevant->get_gun_ups_drain() == 0 ) ) {
            ammo = nullptr;
            range = 0;
        } else {
            ammo = &*ammo_current;
            range = turret->range();
        }
    } else {
        ammo = activity->reload_loc ? activity->reload_loc->type :
               relevant->gun_current_mode().target->ammo_data();
        if( activity->reload_loc ) {
            item *temp_weapon = item::spawn_temporary( *relevant );
            temp_weapon->ammo_set( ammo->get_id() );
            range = temp_weapon->gun_current_mode().target->gun_range( you );
        } else {
            range = relevant->gun_current_mode().target->gun_range( you );
        }
    }
}

bool target_ui::action_switch_ammo()
{
    if( mode == TargetMode::TurretManual ) {
        // For turrets that use vehicle tanks & can fire multiple liquids
        if( turret->ammo_options().size() > 1 ) {
            const auto opts = turret->ammo_options();
            auto iter = opts.find( turret->ammo_current() );
            turret->ammo_select( ++iter != opts.end() ? *iter : *opts.begin() );
            ammo = &*turret->ammo_current();
            range = turret->range();
        }
    } else {
        // Leave aiming UI and open reloading UI since
        // reloading annihilates our aim anyway
        return false;
    }
    on_range_ammo_changed();
    return true;
}

bool target_ui::action_aim()
{
    set_last_target();
    apply_aim_turning_penalty();
    const double min_recoil = calculate_aim_cap( *you, dst );
    for( int i = 0; i < 10; ++i ) {
        do_aim( *you, *relevant, min_recoil );
    }

    // We've changed pc.recoil, update penalty
    recalc_aim_turning_penalty();

    return you->moves > 0;
}

bool target_ui::action_aim_and_shoot( const std::string &action )
{
    std::vector<ranged::aim_type>::iterator it;
    for( it = aim_types.begin(); it != aim_types.end(); it++ ) {
        if( action == it->action ) {
            break;
        }
    }
    if( it == aim_types.end() ) {
        debugmsg( "Could not find a valid aim_type for %s", action.c_str() );
        aim_mode = aim_types.begin();
    }
    int aim_threshold = it->threshold;
    set_last_target();
    apply_aim_turning_penalty();
    const double min_recoil = calculate_aim_cap( *you, dst );
    do {
        do_aim( *you, relevant ? *relevant : null_item_reference(), min_recoil );
    } while( you->moves > 0 && you->recoil > aim_threshold &&
             you->recoil - sight_dispersion > min_recoil );

    // If we made it under the aim threshold, go ahead and fire.
    // Also fire if we're at our best aim level already.
    // If no critter is at dst then sight dispersion does not apply,
    // so it would lock into an infinite loop.
    bool done_aiming = you->recoil <= aim_threshold || you->recoil - sight_dispersion == min_recoil ||
                       ( !g->critter_at( dst ) && you->recoil == min_recoil );
    return done_aiming;
}

void target_ui::draw_terrain_overlay()
{
    tripoint center = you->pos() + you->view_offset;

    // Removes parts that don't belong to currently visible Z level
    const auto filter_this_z = [&center]( const std::vector<tripoint> &traj ) {
        std::vector<tripoint> this_z = traj;
        this_z.erase( std::remove_if( this_z.begin(), this_z.end(),
        [&center]( const tripoint & p ) {
            return p.z != center.z;
        } ), this_z.end() );
        return this_z;
    };

    // FIXME: TILES version of g->draw_line helpfully draws a cursor at last point.
    //        This creates a fake cursor if 'dst' is on a z-level we cannot see.

    // Draw approximate line of fire for each turret in range
    if( mode == TargetMode::Turrets && draw_turret_lines ) {
        // TODO: TILES version doesn't know how to draw more than 1 line at a time.
        //       We merge all lines together and draw them as a big malformed one
        std::set<tripoint> points;
        for( const turret_with_lof &it : turrets_in_range ) {
            std::vector<tripoint> this_z = filter_this_z( it.line );
            for( const tripoint &p : this_z ) {
                points.insert( p );
            }
        }
        // Since "trajectory" for each turret is just a straight line,
        // we can draw it even if the player can't see some parts
        points.erase( dst ); // Workaround for fake cursor on TILES
        std::vector<tripoint> l( points.begin(), points.end() );
        if( dst.z == center.z ) {
            // Workaround for fake cursor bug on TILES
            l.push_back( dst );
        }
        g->draw_line( src, center, l, true );
    }

    // Draw trajectory
    if( mode != TargetMode::Turrets && dst != src ) {
        std::vector<tripoint> this_z = filter_this_z( traj );

        // Draw a highlighted trajectory only if we can see the endpoint.
        // Provides feedback to the player, but avoids leaking information
        // about tiles they can't see.
        g->draw_line( dst, center, this_z );
    }

    // Since draw_line does nothing if destination is not visible,
    // cursor also disappears. Draw it explicitly.
    if( dst.z == center.z ) {
        g->draw_cursor( dst );
    }

    // Draw spell AOE
    if( mode == TargetMode::Spell ) {
        drawsq_params params = drawsq_params().highlight( true ).center( center );
        for( const tripoint &tile : spell_aoe ) {
            if( tile.z != center.z ) {
                continue;
            }
#ifdef TILES
            if( use_tiles ) {
                g->draw_highlight( tile );
            } else {
#endif
                get_map().drawsq( g->w_terrain, tile, params );
#ifdef TILES
            }
#endif
        }
    } else if( mode == TargetMode::Shape ) {
        drawsq_params params = drawsq_params().highlight( true ).center( center );
        for( const std::pair<const tripoint, double> &pr : shape_coverage ) {
            const tripoint &tile = pr.first;
#ifdef TILES
            if( use_tiles ) {
                g->draw_highlight( tile );
            } else {
#endif
                get_map().drawsq( g->w_terrain, tile, params );
                Creature *critter = g->critter_at( tile );
                if( critter != nullptr ) {
                    g->draw_critter_highlighted( *critter, center );
                }
#ifdef TILES
            }
#endif
        }
    }
}

void target_ui::draw_ui_window()
{
    // Clear target window and make it non-transparent.
    int width = getmaxx( w_target );
    int height = getmaxy( w_target );
    for( int y = 0; y < height; y++ ) {
        for( int x = 0; x < width; x++ ) {
            mvwputch( w_target, point( x, y ), c_white, ' ' );
        }
    }

    draw_border( w_target );
    draw_window_title();
    draw_help_notice();

    int text_y = 1; // Skip top border

    panel_cursor_info( text_y );
    text_y += compact ? 0 : 1;

    if( mode == TargetMode::Fire || mode == TargetMode::TurretManual || ( mode == TargetMode::Shape &&
            relevant->is_gun() ) ) {
        panel_gun_info( text_y );
        panel_recoil( text_y );
        text_y += compact ? 0 : 1;
    } else if( mode == TargetMode::Spell ) {
        panel_spell_info( text_y );
        text_y += compact ? 0 : 1;
    }

    bool fill_with_blank_if_no_target = !tiny;
    panel_target_info( text_y, fill_with_blank_if_no_target );
    text_y += compact ? 0 : 1;

    if( mode == TargetMode::Turrets ) {
        panel_turret_list( text_y );
    } else if( status == Status::Good ) {
        // TODO: these are old, consider refactoring
        if( mode == TargetMode::Fire ) {
            panel_fire_mode_aim( text_y );
        } else if( mode == TargetMode::Throw || mode == TargetMode::ThrowBlind ) {
            bool blind = ( mode == TargetMode::ThrowBlind );
            draw_throw_aim( *you, w_target, text_y, ctxt, *relevant, dst, blind );
        }
    }

    if( !narrow ) {
        draw_controls_list( text_y );
    }

    wnoutrefresh( w_target );
}

std::string target_ui::uitext_title()
{
    switch( mode ) {
        case TargetMode::Fire:
        case TargetMode::TurretManual:
            return string_format( _( "Firing %s" ), relevant->tname() );
        case TargetMode::Throw:
            return string_format( _( "Throwing %s" ), relevant->tname() );
        case TargetMode::ThrowBlind:
            return string_format( _( "Blind throwing %s" ), relevant->tname() );
        default:
            return _( "Set target" );
    }
}

std::string target_ui::uitext_fire()
{
    if( mode == TargetMode::Throw || mode == TargetMode::ThrowBlind ) {
        return to_translation( "[Hotkey] to throw", "to throw" ).translated();
    } else if( mode == TargetMode::Reach ) {
        return to_translation( "[Hotkey] to attack", "to attack" ).translated();
    } else if( mode == TargetMode::Spell ) {
        return to_translation( "[Hotkey] to cast the spell", "to cast" ).translated();
    } else {
        return to_translation( "[Hotkey] to fire", "to fire" ).translated();
    }
}

void target_ui::draw_window_title()
{
    mvwprintz( w_target, point( 2, 0 ), c_white, "< " );
    trim_and_print( w_target, point( 4, 0 ), getmaxx( w_target ) - 7, c_red, uitext_title() );
    wprintz( w_target, c_white, " >" );
}

void target_ui::draw_help_notice()
{
    int text_y = getmaxy( w_target ) - 1;
    int width = getmaxx( w_target );
    const std::string label_help = string_format(
                                       narrow ? _( "[%s] show help" ) : _( "[%s] show all controls" ),
                                       ctxt.get_desc( "HELP_KEYBINDINGS", 1 ) );
    int label_width = std::min( utf8_width( label_help ), width - 6 ); // 6 for borders and "< " + " >"
    int text_x = width - label_width - 6;
    mvwprintz( w_target, point( text_x + 1, text_y ), c_white, "< " );
    trim_and_print( w_target, point( text_x + 3, text_y ), label_width, c_white, label_help );
    wprintz( w_target, c_white, " >" );
}

void target_ui::draw_controls_list( int text_y )
{
    // Change UI colors for visual feedback
    // TODO: Colorize keys inside brackets to be consistent with other UI windows
    nc_color col_enabled = c_white;
    nc_color col_disabled = c_light_gray;
    nc_color col_move = ( status != Status::OutOfAmmo ? col_enabled : col_disabled );
    nc_color col_fire = ( status == Status::Good ? col_enabled : col_disabled );

    // Get first key bound to given action OR ' ' if there are none.
    const auto bound_key = [this]( const std::string & s ) {
        const std::vector<char> keys = this->ctxt.keys_bound_to( s );
        return keys.empty() ? ' ' : keys.front();
    };
    const auto colored = [col_enabled]( nc_color color, const std::string & s ) {
        if( color == col_enabled ) {
            // col_enabled is the default one when printing
            return s;
        } else {
            return colorize( s, color );
        }
    };

    struct line {
        size_t order; // Lines with highest 'order' are removed first
        std::string str;
    };
    std::vector<line> lines;

    // Compile full list
    if( shifting_view ) {
        lines.push_back( { 8, colored( col_move, _( "Shift view with directional keys" ) ) } );
    } else {
        lines.push_back( { 8, colored( col_move, _( "Move cursor with directional keys" ) ) } );
    }
    if( is_mouse_enabled() ) {
        std::string move = _( "Mouse: LMB: Target, Wheel: Cycle," );
        std::string fire = _( "RMB: Fire" );
        lines.push_back( { 7, colored( col_move, move ) + " " + colored( col_fire, fire ) } );
    }
    {
        std::string cycle = string_format( _( "[%s] Cycle targets;" ), ctxt.get_desc( "NEXT_TARGET", 1 ) );
        std::string fire = string_format( _( "[%c] %s." ), bound_key( "FIRE" ), uitext_fire() );
        lines.push_back( { 0, colored( col_move, cycle ) + " " + colored( col_fire, fire ) } );
    }
    {
        std::string text = string_format( _( "[%c] target self; [%c] toggle snap-to-target" ),
                                          bound_key( "CENTER" ), bound_key( "TOGGLE_SNAP_TO_TARGET" ) );
        lines.push_back( { 3, colored( col_enabled, text ) } );
    }
    if( mode == TargetMode::Fire ) {
        std::string aim_and_fire;
        for( const auto &e : aim_types ) {
            if( e.has_threshold ) {
                aim_and_fire += string_format( "[%c] ", bound_key( e.action ) );
            }
        }
        aim_and_fire += _( "to aim and fire." );

        std::string aim = string_format( _( "[%c] to steady your aim.  (10 moves)" ),
                                         bound_key( "AIM" ) );

        lines.push_back( { 2, colored( col_fire, aim ) } );
        lines.push_back( { 4, colored( col_fire, aim_and_fire ) } );
    }
    if( mode == TargetMode::Fire || mode == TargetMode::TurretManual || ( mode == TargetMode::Shape &&
            relevant->is_gun() ) ) {
        lines.push_back( { 5, colored( col_enabled, string_format( _( "[%c] to switch firing modes." ),
                                       bound_key( "SWITCH_MODE" ) ) ) } );
        lines.push_back( { 6, colored( col_enabled, string_format( _( "[%c] to switch ammo." ),
                                       bound_key( "SWITCH_AMMO" ) ) ) } );
    }
    if( mode == TargetMode::Turrets ) {
        const std::string label = draw_turret_lines
                                  ? _( "[%c] Hide lines of fire" )
                                  : _( "[%c] Show lines of fire" );
        lines.push_back( { 1, colored( col_enabled, string_format( label, bound_key( "TOGGLE_TURRET_LINES" ) ) ) } );
    }

    // Shrink the list until it fits
    int height = getmaxy( w_target );
    int available_lines = height - text_y - 1; // 1 for bottom border
    if( available_lines <= 0 ) {
        return;
    }
    while( lines.size() > static_cast<size_t>( available_lines ) ) {
        lines.erase( std::max_element( lines.begin(), lines.end(), []( const line & l1, const line & l2 ) {
            return l1.order < l2.order;
        } ) );
    }

    text_y = height - lines.size() - 1;
    for( const line &l : lines ) {
        nc_color col = col_enabled;
        print_colored_text( w_target, point( 1, text_y++ ), col, col, l.str );
    }
}

void target_ui::panel_cursor_info( int &text_y )
{
    std::string label_range;
    if( src == dst ) {
        label_range = string_format( "Range: %d", range );
    } else {
        label_range = string_format( "Range: %d/%d", dist_fn( dst ), range );
    }
    if( status == Status::OutOfRange && mode != TargetMode::Turrets ) {
        // Since each turret has its own range, highlighting cursor
        // range with red would be misleading
        label_range = colorize( label_range, c_red );
    }

    std::vector<std::string> labels;
    labels.push_back( label_range );
    if( allow_zlevel_shift ) {
        labels.push_back( string_format( _( "Elevation: %d" ), dst.z - src.z ) );
    }
    labels.push_back( string_format( _( "Targets: %d" ), targets.size() ) );

    nc_color col = c_light_gray;
    int width = getmaxx( w_target );
    int text_x = 1;
    for( const std::string &s : labels ) {
        int x_left = width - text_x - 1;
        int len = utf8_width( s, true );
        if( len > x_left ) {
            text_x = 1;
            text_y++;
        }
        print_colored_text( w_target, point( text_x, text_y ), col, col, s );
        text_x += len + 1; // 1 for space
    }
    text_y++;
}

void target_ui::panel_gun_info( int &text_y )
{
    gun_mode m = relevant->gun_current_mode();
    std::string mode_name = m.tname();
    std::string gunmod_name;
    if( m.target != relevant ) {
        // Gun mode comes from a gunmod, not base gun. Add gunmod's name
        gunmod_name = m->tname() + " ";
    }
    std::string str = string_format( _( "Firing mode: <color_cyan>%s%s (%d)</color>" ),
                                     gunmod_name, mode_name, m.qty
                                   );
    nc_color clr = c_light_gray;
    print_colored_text( w_target, point( 1, text_y++ ), clr, clr, str );

    if( status == Status::OutOfAmmo ) {
        mvwprintz( w_target, point( 1, text_y++ ), c_red, _( "OUT OF AMMO" ) );
    } else if( ammo ) {
        str = string_format( m->ammo_remaining() ? _( "Ammo: %s (%d/%d)" ) : _( "Ammo: %s" ),
                             colorize( ammo->nname( std::max( m->ammo_remaining(), 1 ) ), ammo->color ), m->ammo_remaining(),
                             m->ammo_capacity() );
        print_colored_text( w_target, point( 1, text_y++ ), clr, clr, str );
    } else {
        // Weapon doesn't use ammunition
        text_y++;
    }
}

void target_ui::panel_recoil( int &text_y )
{
    const int val = ranged::recoil_total( *you );
    const int min_recoil = ranged::effective_dispersion( *you, relevant->sight_dispersion() );
    const int recoil_range = MAX_RECOIL - min_recoil;
    std::string str;
    if( val >= min_recoil + ( recoil_range * 2 / 3 ) ) {
        str = pgettext( "amount of backward momentum", "<color_red>High</color>" );
    } else if( val >= min_recoil + ( recoil_range / 2 ) ) {
        str = pgettext( "amount of backward momentum", "<color_yellow>Medium</color>" );
    } else if( val >= min_recoil + ( recoil_range / 4 ) ) {
        str = pgettext( "amount of backward momentum", "<color_light_green>Low</color>" );
    } else {
        str = pgettext( "amount of backward momentum", "<color_cyan>None</color>" );
    }
    str = string_format( _( "Recoil: %s" ), str );
    nc_color clr = c_light_gray;
    print_colored_text( w_target, point( 1, text_y++ ), clr, clr, str );
}

void target_ui::panel_spell_info( int &text_y )
{
    nc_color clr = c_light_gray;

    mvwprintz( w_target, point( 1, text_y++ ), c_light_green, _( "Casting: %s (Level %u)" ),
               casting->name(),
               casting->get_level() );
    if( !no_mana || casting->energy_source() == energy_type::none_energy ) {
        if( casting->energy_source() == energy_type::hp_energy ) {
            text_y += fold_and_print( w_target, point( 1, text_y ), getmaxx( w_target ) - 2,
                                      clr,
                                      _( "Cost: %s %s" ), casting->energy_cost_string( *you ), casting->energy_string() );
        } else {
            text_y += fold_and_print( w_target, point( 1, text_y ), getmaxx( w_target ) - 2,
                                      clr,
                                      _( "Cost: %s %s (Current: %s)" ), casting->energy_cost_string( *you ), casting->energy_string(),
                                      casting->energy_cur_string( *you ) );
        }
    }

    std::string fail_str;
    if( no_fail ) {
        fail_str = colorize( _( "0.0 % Failure Chance" ), c_light_green );
    } else {
        fail_str = casting->colorized_fail_percent( *you );
    }
    print_colored_text( w_target, point( 1, text_y++ ), clr, clr, fail_str );

    if( casting->aoe() > 0 ) {
        nc_color color = c_light_gray;
        const std::string fx = casting->effect();
        const std::string aoes = casting->aoe_string();
        if( fx == "projectile_attack" || fx == "target_attack" ||
            fx == "area_pull" || fx == "area_push" ||
            fx == "ter_transform" ) {
            text_y += fold_and_print( w_target, point( 1, text_y ), getmaxx( w_target ) - 2, color,
                                      _( "Effective Spell Radius: %s%s" ), aoes,
                                      casting->in_aoe( src, dst ) ? colorize( _( " WARNING!  IN RANGE" ), c_red ) : "" );
        } else if( fx == "cone_attack" ) {
            text_y += fold_and_print( w_target, point( 1, text_y ), getmaxx( w_target ) - 2, color,
                                      _( "Cone Arc: %s degrees" ), aoes );
        } else if( fx == "line_attack" ) {
            text_y += fold_and_print( w_target, point( 1, text_y ), getmaxx( w_target ) - 2, color,
                                      _( "Line width: %s" ), aoes );
        }
    }

    mvwprintz( w_target, point( 1, text_y++ ), c_light_red, _( "Damage: %s" ),
               casting->damage_string( *you ) );

    text_y += fold_and_print( w_target, point( 1, text_y ), getmaxx( w_target ) - 2, clr,
                              casting->description() );
}

void target_ui::panel_target_info( int &text_y, bool fill_with_blank_if_no_target )
{
    int max_lines = 4;
    if( dst_critter ) {
        if( you->sees( *dst_critter ) ) {
            // FIXME: print_info doesn't really care about line limit
            //        and can always occupy up to 4 of them (or even more?).
            //        To make things consistent, we ask it for 2 lines
            //        and somewhat reliably get 4.
            int fix_for_print_info = max_lines - 2;
            dst_critter->print_info( w_target, text_y, fix_for_print_info, 1 );
            text_y += max_lines;
        } else {
            std::vector<std::string> buf;
            if( you->sees_with_infrared( *dst_critter ) ) {
                dst_critter->describe_infrared( buf );
            } else if( you->sees_with_specials( *dst_critter ) ) {
                dst_critter->describe_specials( buf );
            }
            for( size_t i = 0; i < static_cast<size_t>( max_lines ); i++, text_y++ ) {
                if( i >= buf.size() ) {
                    continue;
                }
                mvwprintw( w_target, point( 1, text_y ), buf[i] );
            }
        }
    } else if( fill_with_blank_if_no_target ) {
        // Fill with blank lines to prevent other panels from jumping around
        // when the cursor moves.
        text_y += max_lines;
        // TODO: print info about tile?
    }
}

void target_ui::panel_fire_mode_aim( int &text_y )
{
    // TODO: saving & restoring pc.recoil may actually be unnecessary
    double saved_pc_recoil = you->recoil;
    you->recoil = predicted_recoil;
    double predicted_recoil = you->recoil;
    int predicted_delay = 0;
    if( aim_mode->has_threshold && aim_mode->threshold < you->recoil ) {
        do {
            const double aim_amount = ranged::aim_per_move( *you, *relevant, predicted_recoil );
            if( aim_amount > 0 ) {
                predicted_delay++;
                predicted_recoil = std::max( predicted_recoil - aim_amount, 0.0 );
            }
        } while( predicted_recoil > aim_mode->threshold &&
                 predicted_recoil - sight_dispersion > 0 );
    } else {
        predicted_recoil = you->recoil;
    }

    const double target_size = dst_critter ? dst_critter->ranged_target_size() :
                               occupied_tile_fraction( creature_size::medium );

    item *load_loc = activity->reload_loc ? &*activity->reload_loc : nullptr;
    text_y = print_aim( *you, w_target, text_y, ctxt, *relevant->gun_current_mode(),
                        target_size, dst, predicted_recoil, load_loc );

    if( aim_mode->has_threshold ) {
        mvwprintw( w_target, point( 1, text_y++ ), _( "%s Delay: %i" ), aim_mode->name,
                   predicted_delay );
    }

    you->recoil = saved_pc_recoil;
}

void target_ui::panel_turret_list( int &text_y )
{
    mvwprintw( w_target, point( 1, text_y++ ), _( "Turrets in range: %d/%d" ), turrets_in_range.size(),
               vturrets->size() );

    for( const turret_with_lof &it : turrets_in_range ) {
        std::string str = string_format( "* %s", it.turret->name() );
        nc_color clr = c_white;
        print_colored_text( w_target, point( 1, text_y++ ), clr, clr, str );
    }
}

void target_ui::on_target_accepted( bool harmful )
{
    // TODO: all of this should be moved into on-hit code
    const auto lt_ptr = you->last_target.lock();
    if( npc *const guy = dynamic_cast<npc *>( lt_ptr.get() ) ) {
        if( harmful ) {
            if( !guy->guaranteed_hostile() ) {
                // TODO: get rid of this. Or combine it with effect_hit_by_player
                guy->hit_by_player = true; // used for morale penalty
            }
            guy->make_angry();
        }
    } else if( monster *const mon = dynamic_cast<monster *>( lt_ptr.get() ) ) {
        mon->add_effect( effect_hit_by_player, 10_minutes );
    }
}

bool ranged::gunmode_checks_common( avatar &you, const map &m, std::vector<std::string> &messages,
                                    const gun_mode &gmode )
{
    bool result = true;

    // Check that passed gun mode is valid and we are able to use it
    if( !( gmode && you.can_use( *gmode ) ) ) {
        messages.push_back( string_format( _( "You can't currently fire your %s." ),
                                           gmode->tname() ) );
        result = false;
    }

    const optional_vpart_position vp = m.veh_at( you.pos() );
    if( vp && vp->vehicle().player_in_control( you ) && ( gmode->is_two_handed( you ) ||
            gmode->has_flag( flag_FIRE_TWOHAND ) ) ) {

        const auto vp_control = vp->part_with_feature( "CONTROLS", true );
        const bool ctrl_handsfree = vp_control && vp_control->has_feature( "CONTROL_WITHOUT_HANDS" );
        const bool using_arms = vp->vehicle().has_part( "MUSCLE_ARMS", true );
        const bool single_tile_veh = vp->vehicle().all_parts_at_location( "structure" ).size() < 2 ;

        if( ctrl_handsfree ) { // check this vehicle is steerable and able to be controlled without hands.
            if( single_tile_veh ) {
                if( using_arms ) {
                    messages.push_back( string_format(
                                            _( "You can't fire your %s while driving; this vehicle is hand-powered." ),
                                            gmode->tname() ) );
                    result = false;
                } else if( you.get_skill_level( skill_driving ) < 3 ) {
                    messages.push_back( string_format(
                                            _( "Your driving skill isn't high enough to fire your %s while driving." ), gmode->tname() ) );
                    result = false;
                } else {
                    result = true;
                }
            } else { // its ctrl is handsfree, but the vehicle is too big.
                messages.push_back( string_format(
                                        _( "You can't fire your %s while driving; this vehicle is too large to handle." ),
                                        gmode->tname() ) );
                result = false;
            }
        } else { // You are driving with your own hands!
            messages.push_back( string_format( _( "You can't fire your %s while driving." ),
                                               gmode->tname() ) );
            result = false;
        }
    }

    if( gmode->has_flag( flag_FIRE_TWOHAND ) && ( !you.has_two_arms() ||
            you.worn_with_flag( flag_RESTRICT_HANDS ) ) ) {
        messages.push_back( string_format( _( "You need two free hands to fire your %s." ),
                                           gmode->tname() ) );
        result = false;
    }

    if( ranged::get_str_draw_penalty( *gmode, you ) < 0.5f ) {
        messages.push_back( string_format( _( "You don't have enough strength to fire your %s." ),
                                           gmode->tname() ) );
        result = false;
    }

    return result;
}

auto ranged::gunmode_checks_weapon( avatar &you, const map &m, std::vector<std::string> &messages,
                                    const gun_mode &gmode ) -> bool
{
    bool result = true;

    if( !gmode->ammo_sufficient() && !gmode->has_flag( flag_RELOAD_AND_SHOOT ) ) {
        if( !gmode->ammo_remaining() ) {
            messages.push_back( string_format( _( "Your %s is empty!" ), gmode->tname() ) );
        } else {
            messages.push_back( string_format( _( "Your %s needs %i charges to fire!" ),
                                               gmode->tname(), gmode->ammo_required() ) );
        }
        result = false;
    }

    if( gmode->get_gun_ups_drain() > 0 ) {
        const int ups_drain = gmode->get_gun_ups_drain();
        const int adv_ups_drain = std::max( 1, ups_drain / 2 );
        bool is_mech_weapon = false;
        if( you.is_mounted() ) {
            monster *mons = get_player_character().mounted_creature.get();
            if( !mons->type->mech_weapon.is_empty() ) {
                is_mech_weapon = true;
            }
        }
        if( !is_mech_weapon ) {
            if( !( you.has_charges( itype_UPS, ups_drain )  ||
                   ( you.has_active_bionic( bio_ups ) &&
                     you.get_power_level() >= units::from_kilojoule( ups_drain ) ) ) ) {
                messages.push_back( string_format(
                                        _( "You need a UPS with at least %2$d charges or an advanced UPS with at least %3$d charges to fire the %1$s!" ),
                                        gmode->tname(), ups_drain, adv_ups_drain ) );
                result = false;
            }
        } else {
            if( !you.has_charges( itype_UPS, ups_drain ) ) {
                messages.push_back( string_format( _( "Your mech has an empty battery, its %s will not fire." ),
                                                   gmode->tname() ) );
                result = false;
            }
        }
    }

    if( gmode->has_flag( flag_MOUNTED_GUN ) ) {
        const Character &shooter = you;
        if( !can_use_heavy_weapon( shooter, m, shooter.pos() ) &&
            !( you.get_size() > creature_size::medium ) &&
            !you.worn_with_flag( flag_HEAVY_WEAPON_SUPPORT ) ) {
            messages.push_back( string_format(
                                    _( "You must stand near acceptable terrain or furniture to fire the %s.  A table, a mound of dirt, a broken window, etc." ),
                                    gmode->tname() ) );
            result = false;
        }
    }

    return result;
}

void ranged::prompt_select_default_ammo_for( avatar &u, item &w )
{
    item_reload_option opt = character_funcs::select_ammo( u, w, false, true, true );
    if( opt ) {
        if( u.ammo_location && opt.ammo == &*u.ammo_location ) {
            u.add_msg_if_player( _( "Cleared ammo preferences for %s." ), w.tname() );
            u.ammo_location = nullptr;
        } else if( u.has_item( *opt.ammo ) ) {
            u.add_msg_if_player( _( "Selected %s as default ammo for %s." ), opt.ammo->tname(), w.tname() );
            u.ammo_location = opt.ammo;
        } else {
            u.add_msg_if_player( _( "You don't have that ammo on you." ) );
        }
    }
}

int ranged::effective_dispersion( const Character &who, int dispersion )
{
    /** @EFFECT_PER penalizes sight dispersion when low. */
    dispersion += who.ranged_per_mod();

    dispersion += who.encumb( body_part_eyes ) / 2;

    return std::max( dispersion, 0 );
}

std::pair<int, int> ranged::get_fastest_sight( const Character &who, const item &gun,
        double recoil )
{
    // Get fastest sight that can be used to improve aim further below @ref recoil.
    int sight_speed_modifier = INT_MIN;
    int limit = 0;
    if( effective_dispersion( who, gun.type->gun->sight_dispersion ) < recoil ) {
        sight_speed_modifier = gun.has_flag( flag_DISABLE_SIGHTS ) ? 0 : 6;
        limit = effective_dispersion( who, gun.type->gun->sight_dispersion );
    }

    for( const auto e : gun.gunmods() ) {
        const islot_gunmod &mod = *e->type->gunmod;
        if( mod.sight_dispersion < 0 || mod.aim_speed < 0 ) {
            continue; // skip gunmods which don't provide a sight
        }
        if( effective_dispersion( who, mod.sight_dispersion ) < recoil &&
            mod.aim_speed > sight_speed_modifier ) {
            sight_speed_modifier = mod.aim_speed;
            limit = effective_dispersion( who, mod.sight_dispersion );
        }
    }
    return std::make_pair( sight_speed_modifier, limit );
}

int ranged::get_most_accurate_sight( const Character &who, const item &gun )
{
    if( !gun.is_gun() ) {
        return 0;
    }

    int limit = effective_dispersion( who, gun.type->gun->sight_dispersion );
    for( const auto e : gun.gunmods() ) {
        const islot_gunmod &mod = *e->type->gunmod;
        if( mod.aim_speed >= 0 ) {
            limit = std::min( limit, effective_dispersion( who, mod.sight_dispersion ) );
        }
    }

    return limit;
}

double ranged::aim_speed_skill_modifier( const Character &who, const skill_id &gun_skill )
{
    double skill_mult = 1.0;
    if( gun_skill == skill_pistol ) {
        skill_mult = 2.0;
    } else if( gun_skill == skill_rifle ) {
        skill_mult = 0.9;
    }
    /** @EFFECT_PISTOL increases aiming speed for pistols */
    /** @EFFECT_SMG increases aiming speed for SMGs */
    /** @EFFECT_RIFLE increases aiming speed for rifles */
    /** @EFFECT_SHOTGUN increases aiming speed for shotguns */
    /** @EFFECT_LAUNCHER increases aiming speed for launchers */
    return skill_mult * std::min( MAX_SKILL, who.get_skill_level( gun_skill ) );
}

double ranged::aim_speed_dex_modifier( const Character &who )
{
    return who.get_dex() - 8;
}

double ranged::aim_speed_encumbrance_modifier( const Character &who )
{
    return ( who.encumb( body_part_hand_l ) + who.encumb( body_part_hand_r ) ) / 10.0;
}

double ranged::aim_multiplier_from_volume( const item &gun )
{
    // Penalty scales linearly as volume increases
    // 1 liters = multiplier of 0.9
    // 5 liters = multiplier of 0.5
    // Return early with maximum penalty of 0.1 at 9 liters and above.
    if( gun.volume() >= 9_liter ) {
        return 0.1f;
    }
    double aim_mult = ( 10000.0 - static_cast<float>( gun.volume() / 1_ml ) ) / 10000.0;
    // TODO: also scale with skill level?
    return aim_mult;
}

double ranged::aim_per_move( const Character &who, const item &gun, double recoil )
{
    if( !gun.is_gun() ) {
        return 0.0;
    }

    std::pair<int, int> best_sight = get_fastest_sight( who, gun, recoil );
    int sight_speed_modifier = best_sight.first;
    int limit = best_sight.second;
    if( sight_speed_modifier == INT_MIN ) {
        // No suitable sights (already at maximum aim).
        return 0;
    }

    // Overall strategy for determining aim speed is to sum the factors that contribute to it,
    // then scale that speed by current recoil level.
    // Player capabilities make aiming faster, and aim speed slows down as it approaches 0.
    // Base speed is non-zero to prevent extreme rate changes as aim speed approaches 0.
    double aim_speed = 10.0;

    skill_id gun_skill = gun.gun_skill();
    // Ranges [0 - 10]
    aim_speed += aim_speed_skill_modifier( who, gun_skill );

    // Range [0 - 12]
    /** @EFFECT_DEX increases aiming speed */
    aim_speed += aim_speed_dex_modifier( who );

    // Range [0 - 10]
    aim_speed += sight_speed_modifier;

    // Each 5 points (combined) of hand encumbrance decreases aim speed by one unit.
    aim_speed -= aim_speed_encumbrance_modifier( who );

    aim_speed *= aim_multiplier_from_volume( gun );

    // Just a raw scaling factor.
    aim_speed *= 6.5;

    // Scale rate logistically as recoil goes from MAX_RECOIL to 0.
    aim_speed *= 1.0 - logarithmic_range( 0, MAX_RECOIL, recoil );

    // Minimum improvement is 5MoA.  This mostly puts a cap on how long aiming for sniping takes.
    aim_speed = std::max( aim_speed, 5.0 );

    // Never improve by more than the currently used sights permit.
    return std::min( aim_speed, recoil - limit );
}

std::optional<shape_factory> ranged::get_shape_factory( const item &gun )
{
    if( gun.ammo_current() && gun.ammo_current()->ammo ) {
        return gun.ammo_current()->ammo->shape;
    }

    return {};
}
