#pragma once

#include <memory>
#include <string>
#include <vector>

#include "clone_ptr.h"
#include "coordinates.h"
#include "type_id.h"
#include "units.h"

class map;
class Character;
class JsonObject;
class item;
class monster;
class player;
struct iteminfo;
template<typename T> class ret_val;
class mapbuffer;

// iuse methods returning a bool indicating whether to consume a charge of the item being used.
namespace iuse
{
// FOOD AND DRUGS (ADMINISTRATION)
int sewage( Character *, item *, bool, const tripoint_abs_ms * );
int honeycomb( Character *, item *, bool, const tripoint_abs_ms * );
int alcohol_weak( Character *, item *, bool, const tripoint_abs_ms * );
int alcohol_medium( Character *, item *, bool, const tripoint_abs_ms * );
int alcohol_strong( Character *, item *, bool, const tripoint_abs_ms * );
int xanax( Character *, item *, bool, const tripoint_abs_ms * );
int antibiotic( Character *, item *, bool, const tripoint_abs_ms * );
int eyedrops( Character *, item *, bool, const tripoint_abs_ms * );
int fungicide( Character *, item *, bool, const tripoint_abs_ms * );
int antifungal( Character *, item *, bool, const tripoint_abs_ms * );
int antiparasitic( Character *, item *, bool, const tripoint_abs_ms * );
int anticonvulsant( Character *, item *, bool, const tripoint_abs_ms * );
int meth( Character *, item *, bool, const tripoint_abs_ms * );
int vaccine( Character *, item *, bool, const tripoint_abs_ms * );
int poison( Character *, item *, bool, const tripoint_abs_ms * );
int meditate( Character *, item *, bool, const tripoint_abs_ms * );
int thorazine( Character *, item *, bool, const tripoint_abs_ms * );
int prozac( Character *, item *, bool, const tripoint_abs_ms * );
int sleep( Character *, item *, bool, const tripoint_abs_ms * );
int datura( Character *, item *, bool, const tripoint_abs_ms * );
int flumed( Character *, item *, bool, const tripoint_abs_ms * );
int flusleep( Character *, item *, bool, const tripoint_abs_ms * );
int inhaler( Character *, item *, bool, const tripoint_abs_ms * );
int blech( Character *, item *, bool, const tripoint_abs_ms * );
int blech_because_unclean( Character *, item *, bool, const tripoint_abs_ms * );
int plantblech( Character *, item *, bool, const tripoint_abs_ms * );
int purifier( Character *, item *, bool, const tripoint_abs_ms * );
int purify_iv( Character *, item *, bool, const tripoint_abs_ms * );
int purify_smart( Character *, item *, bool, const tripoint_abs_ms * );
int marloss( Character *, item *, bool, const tripoint_abs_ms * );
int marloss_seed( Character *, item *, bool, const tripoint_abs_ms * );
int marloss_gel( Character *, item *, bool, const tripoint_abs_ms * );
int mycus( Character *, item *, bool, const tripoint_abs_ms * );
int petfood( Character *, item *, bool, const tripoint_abs_ms * );
int antiasthmatic( Character *, item *, bool, const tripoint_abs_ms * );
// TOOLS
int amputate( Character *, item *, bool, const tripoint_abs_ms * );
int extinguisher( Character *, item *, bool, const tripoint_abs_ms * );
int hammer( Character *, item *, bool, const tripoint_abs_ms * );
int water_purifier( Character *, item *, bool, const tripoint_abs_ms * );
int directional_antenna( Character *, item *, bool, const tripoint_abs_ms * );
int radio_off( Character *, item *, bool, const tripoint_abs_ms * );
int radio_on( Character *, item *, bool, const tripoint_abs_ms * );
int noise_emitter_off( Character *, item *, bool, const tripoint_abs_ms * );
int noise_emitter_on( Character *, item *, bool, const tripoint_abs_ms * );
int note_bionics( Character *, item *, bool, const tripoint_abs_ms * );
int ma_manual( Character *, item *, bool, const tripoint_abs_ms * );
int crowbar( Character *, item *, bool, const tripoint_abs_ms * );
int makemound( Character *, item *, bool, const tripoint_abs_ms * );
int dig( Character *, item *, bool, const tripoint_abs_ms * );
int dig_channel( Character *, item *, bool, const tripoint_abs_ms * );
int fill_pit( Character *, item *, bool, const tripoint_abs_ms * );
int clear_rubble( Character *, item *, bool, const tripoint_abs_ms * );
int siphon( Character *, item *, bool, const tripoint_abs_ms * );
int jackhammer( Character *, item *, bool, const tripoint_abs_ms * );
int pickaxe( Character *, item *, bool, const tripoint_abs_ms * );
int burrow( Character *, item *, bool, const tripoint_abs_ms * );
int geiger( Character *, item *, bool, const tripoint_abs_ms * );
int teleport( Character *, item *, bool, const tripoint_abs_ms * );
int can_goo( Character *, item *, bool, const tripoint_abs_ms * );
int throwable_extinguisher_act( Character *, item *, bool, const tripoint_abs_ms * );
int directional_hologram( Character *, item *, bool, const tripoint_abs_ms * );
int capture_monster_veh( Character *, item *, bool, const tripoint_abs_ms * );
int capture_monster_act( Character *, item *, bool, const tripoint_abs_ms * );
int debug_grenade( Character *, item *, bool, const tripoint_abs_ms * );
int debug_grenade_act( Character *, item *, bool, const tripoint_abs_ms * );
int c4( Character *, item *, bool, const tripoint_abs_ms * );
int c4_breaching( Character *, item *, bool, const tripoint_abs_ms * );
int arrow_flammable( Character *, item *, bool, const tripoint_abs_ms * );
int acidbomb_act( Character *, item *, bool, const tripoint_abs_ms * );
int grenade_inc_act( Character *, item *, bool, const tripoint_abs_ms * );
int molotov_lit( Character *, item *, bool, const tripoint_abs_ms * );
int firecracker_pack( Character *, item *, bool, const tripoint_abs_ms * );
int firecracker_pack_act( Character *, item *, bool, const tripoint_abs_ms * );
int firecracker( Character *, item *, bool, const tripoint_abs_ms * );
int firecracker_act( Character *, item *, bool, const tripoint_abs_ms * );
int mininuke( Character *, item *, bool, const tripoint_abs_ms * );
int pheromone( Character *, item *, bool, const tripoint_abs_ms * );
int pick_lock( Character *, item *, bool, const tripoint_abs_ms * );
int portal( Character *, item *, bool, const tripoint_abs_ms * );
int tazer( Character *, item *, bool, const tripoint_abs_ms * );
int tazer2( Character *, item *, bool, const tripoint_abs_ms * );
int mp3_on( Character *, item *, bool, const tripoint_abs_ms * );
int rpgdie( Character *, item *, bool, const tripoint_abs_ms * );
int dive_tank( Character *, item *, bool, const tripoint_abs_ms * );
int gasmask( Character *, item *, bool, const tripoint_abs_ms * );
int portable_game( Character *, item *, bool, const tripoint_abs_ms * );
int vibe( Character *, item *, bool, const tripoint_abs_ms * );
int vortex( Character *, item *, bool, const tripoint_abs_ms * );
int dog_whistle( Character *, item *, bool, const tripoint_abs_ms * );
int call_of_tindalos( Character *, item *, bool, const tripoint_abs_ms * );
int blood_draw( Character *, item *, bool, const tripoint_abs_ms * );
int mind_splicer( Character *, item *, bool, const tripoint_abs_ms * );
void cut_log_into_planks( Character & );
int lumber( Character *, item *, bool, const tripoint_abs_ms * );
int chop_tree( Character *, item *, bool, const tripoint_abs_ms * );
int chop_logs( Character *, item *, bool, const tripoint_abs_ms * );
int oxytorch( Character *, item *, bool, const tripoint_abs_ms * );
int hacksaw( Character *, item *, bool, const tripoint_abs_ms * );
int boltcutters( Character *, item *, bool, const tripoint_abs_ms * );
int mop( Character *, item *, bool, const tripoint_abs_ms * );
int spray_can( Character *, item *, bool, const tripoint_abs_ms * );
int towel( Character *, item *, bool, const tripoint_abs_ms * );
int unfold_generic( Character *, item *, bool, const tripoint_abs_ms * );
int adrenaline_injector( Character *, item *, bool, const tripoint_abs_ms * );
int jet_injector( Character *, item *, bool, const tripoint_abs_ms * );
int stimpack( Character *, item *, bool, const tripoint_abs_ms * );
int contacts( Character *, item *, bool, const tripoint_abs_ms * );
int talking_doll( Character *, item *, bool, const tripoint_abs_ms * );
int bell( Character *, item *, bool, const tripoint_abs_ms * );
int seed( Character *, item *, bool, const tripoint_abs_ms * );
int oxygen_bottle( Character *, item *, bool, const tripoint_abs_ms * );
int radio_mod( Character *, item *, bool, const tripoint_abs_ms * );
int remove_all_mods( Character *, item *, bool, const tripoint_abs_ms * );
int fishing_rod( Character *, item *, bool, const tripoint_abs_ms * );
int fish_trap( Character *, item *, bool, const tripoint_abs_ms * );
int gun_clean( Character *, item *, bool, const tripoint_abs_ms * );
int gun_repair( Character *, item *, bool, const tripoint_abs_ms * );
int gunmod_attach( Character *, item *, bool, const tripoint_abs_ms * );
int toolmod_attach( Character *, item *, bool, const tripoint_abs_ms * );
int unpack_item( Character *, item *, bool, const tripoint_abs_ms * );
int pack_cbm( Character *, item *it, bool, const tripoint_abs_ms * );
int pack_item( Character *, item *, bool, const tripoint_abs_ms * );
int radglove( Character *, item *, bool, const tripoint_abs_ms * );
int robotcontrol( Character *, item *, bool, const tripoint_abs_ms * );
// Helper for validating a potential taget of robot control
bool robotcontrol_can_target( Character *, const monster & );
int einktabletpc( Character *, item *, bool, const tripoint_abs_ms * );
int camera( Character *, item *, bool, const tripoint_abs_ms * );
int ehandcuffs( Character *, item *, bool, const tripoint_abs_ms * );
int foodperson( Character *, item *, bool, const tripoint_abs_ms * );
int tow_attach( Character *, item *, bool, const tripoint_abs_ms * );
int cable_attach( Character *, item *, bool, const tripoint_abs_ms * );
int shavekit( Character *, item *, bool, const tripoint_abs_ms * );
int hairkit( Character *, item *, bool, const tripoint_abs_ms * );
int weather_tool( Character *, item *, bool, const tripoint_abs_ms * );
int ladder( Character *, item *, bool, const tripoint_abs_ms * );
int solarpack( Character *, item *, bool, const tripoint_abs_ms * );
int solarpack_off( Character *, item *, bool, const tripoint_abs_ms * );
int weak_antibiotic( Character *, item *, bool, const tripoint_abs_ms * );
int strong_antibiotic( Character *, item *, bool, const tripoint_abs_ms * );
int melatonin_tablet( Character *, item *, bool, const tripoint_abs_ms * );
int coin_flip( Character *, item *, bool, const tripoint_abs_ms * );
int play_game( Character *, item *, bool, const tripoint_abs_ms * );
int magic_8_ball( Character *, item *, bool, const tripoint_abs_ms * );
int toggle_heats_food( Character *, item *, bool, const tripoint_abs_ms * );
int toggle_ups_charging( Character *, item *, bool, const tripoint_abs_ms * );
int report_grid_charge( Character *, item *, bool, const tripoint_abs_ms * );
int report_grid_connections( Character *, item *, bool, const tripoint_abs_ms * );
int modify_grid_connections( Character *, item *, bool, const tripoint_abs_ms * );
int report_fluid_grid_connections( Character *, item *, bool, const tripoint_abs_ms * );
int modify_fluid_grid_connections( Character *, item *, bool, const tripoint_abs_ms * );
int bullet_vibe_on( Character *, item *, bool, const tripoint_abs_ms * );

// MACGUFFINS

int radiocar( Character *, item *, bool, const tripoint_abs_ms * );
int radiocaron( Character *, item *, bool, const tripoint_abs_ms * );
int radiocontrol( Character *, item *, bool, const tripoint_abs_ms * );

int autoclave( Character *, item *, bool, const tripoint_abs_ms * );

int remoteveh( Character *, item *, bool, const tripoint_abs_ms * );

int craft( Character *, item *, bool, const tripoint_abs_ms * );

int disassemble( Character *, item *, bool, const tripoint_abs_ms * );

// ARTIFACTS
/* This function is used when an artifact is activated.
   It examines the item's artifact-specific properties.
   See artifact.h for a list.                        */
int artifact( Character *, item *, bool, const tripoint_abs_ms * );

int towel_common( Character *, item *, bool, const tripoint_abs_ms * );

// Helper for handling pesky wannabe-artists
int handle_ground_graffiti( Character &p, item *it, const std::string &prefix,
                            const tripoint_abs_ms &where );

// Helper for wood chopping
int chop_moves( Character &ch, item &tool );

} // namespace iuse

void remove_radio_mod( item &it, Character &p );

// Helper for listening to music, might deserve a better home, but not sure where.
void play_music( Character &p, const tripoint_abs_ms &source, int volume, int max_morale );

// FEESH
int good_fishing_spot( mapbuffer &here, const tripoint_abs_ms &pos );


using use_function_pointer = int ( * )( Character *, item *, bool, const tripoint_abs_ms * );

class iuse_actor
{
    protected:
        iuse_actor( const std::string &type, int cost = -1 ) : type( type ), cost( cost ) {}

    public:
        /**
         * The type of the action. It's not translated. Different iuse_actor instances may have the
         * same type, but different data.
         */
        const std::string type;

        /** Units of ammo required per invocation (or use value from base item if negative) */
        int cost;

        virtual ~iuse_actor() = default;
        virtual void load( const JsonObject &jo ) = 0;
        virtual int use( Character &, item &, bool, const tripoint_abs_ms & ) const = 0;
        virtual ret_val<bool> can_use( const Character &, const item &, bool,
                                       const tripoint_abs_ms & ) const;
        virtual void info( const item &, std::vector<iteminfo> & ) const {}
        /**
         * Returns a deep copy of this object. Example implementation:
         * \code
         * class my_iuse_actor {
         *     std::unique_ptr<iuse_actor> clone() const override {
         *         return std::make_unique<my_iuse_actor>( *this );
         *     }
         * };
         * \endcode
         * The returned value should behave like the original item and must have the same type.
         */
        virtual std::unique_ptr<iuse_actor> clone() const = 0;
        /**
         * Returns whether the actor is valid (exists in the generator).
         */
        virtual bool is_valid() const;
        /**
         * Returns the translated name of the action. It is used for the item action menu.
         */
        virtual std::string get_name() const;
        /**
         * Finalizes the actor. Must be called after all items are loaded.
         */
        virtual void finalize( const itype_id &/*my_item_type*/ ) { }

        virtual void on_spawned( item & ) const {}
        virtual void on_placed( item & ) const {}
};

struct use_function {
    protected:
        cata::clone_ptr<iuse_actor> actor;

    public:
        use_function() = default;
        use_function( const std::string &type, use_function_pointer f );
        use_function( std::unique_ptr<iuse_actor> f ) : actor( std::move( f ) ) {}

        int call( Character &, item &, bool, const tripoint_abs_ms & ) const;
        ret_val<bool> can_call( const Character &, const item &, bool t, const tripoint_abs_ms &pos ) const;

        iuse_actor *get_actor_ptr() {
            return actor.get();
        }

        const iuse_actor *get_actor_ptr() const {
            return actor.get();
        }

        explicit operator bool() const {
            return actor != nullptr;
        }

        /** @return See @ref iuse_actor::type */
        std::string get_type() const;
        /** @return See @ref iuse_actor::get_name */
        std::string get_name() const;
        /** @return Used by @ref item::info to get description of the actor */
        void dump_info( const item &, std::vector<iteminfo> & ) const;
};
