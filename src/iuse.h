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
struct tripoint;

// iuse methods returning a bool indicating whether to consume a charge of the item being used.
namespace iuse
{
// FOOD AND DRUGS (ADMINISTRATION)
int sewage( player *, item *, bool, const tripoint_bub_ms & );
int honeycomb( player *, item *, bool, const tripoint_bub_ms & );
int alcohol_weak( player *, item *, bool, const tripoint_bub_ms & );
int alcohol_medium( player *, item *, bool, const tripoint_bub_ms & );
int alcohol_strong( player *, item *, bool, const tripoint_bub_ms & );
int xanax( player *, item *, bool, const tripoint_bub_ms & );
int antibiotic( player *, item *, bool, const tripoint_bub_ms & );
int eyedrops( player *, item *, bool, const tripoint_bub_ms & );
int fungicide( player *, item *, bool, const tripoint_bub_ms & );
int antifungal( player *, item *, bool, const tripoint_bub_ms & );
int antiparasitic( player *, item *, bool, const tripoint_bub_ms & );
int anticonvulsant( player *, item *, bool, const tripoint_bub_ms & );
int meth( player *, item *, bool, const tripoint_bub_ms & );
int vaccine( player *, item *, bool, const tripoint_bub_ms & );
int poison( player *, item *, bool, const tripoint_bub_ms & );
int meditate( player *, item *, bool, const tripoint_bub_ms & );
int thorazine( player *, item *, bool, const tripoint_bub_ms & );
int prozac( player *, item *, bool, const tripoint_bub_ms & );
int sleep( player *, item *, bool, const tripoint_bub_ms & );
int datura( player *, item *, bool, const tripoint_bub_ms & );
int flumed( player *, item *, bool, const tripoint_bub_ms & );
int flusleep( player *, item *, bool, const tripoint_bub_ms & );
int inhaler( player *, item *, bool, const tripoint_bub_ms & );
int blech( player *, item *, bool, const tripoint_bub_ms & );
int blech_because_unclean( player *, item *, bool, const tripoint_bub_ms & );
int plantblech( player *, item *, bool, const tripoint_bub_ms & );
int purifier( player *, item *, bool, const tripoint_bub_ms & );
int purify_iv( player *, item *, bool, const tripoint_bub_ms & );
int purify_smart( player *, item *, bool, const tripoint_bub_ms & );
int marloss( player *, item *, bool, const tripoint_bub_ms & );
int marloss_seed( player *, item *, bool, const tripoint_bub_ms & );
int marloss_gel( player *, item *, bool, const tripoint_bub_ms & );
int mycus( player *, item *, bool, const tripoint_bub_ms & );
int petfood( player *, item *, bool, const tripoint_bub_ms & );
int antiasthmatic( player *, item *, bool, const tripoint_bub_ms & );
// TOOLS
int amputate( player *, item *, bool, const tripoint_bub_ms & );
int extinguisher( player *, item *, bool, const tripoint_bub_ms & );
int hammer( player *, item *, bool, const tripoint_bub_ms & );
int water_purifier( player *, item *, bool, const tripoint_bub_ms & );
int directional_antenna( player *, item *, bool, const tripoint_bub_ms & );
int radio_off( player *, item *, bool, const tripoint_bub_ms & );
int radio_on( player *, item *, bool, const tripoint_bub_ms & );
int noise_emitter_off( player *, item *, bool, const tripoint_bub_ms & );
int noise_emitter_on( player *, item *, bool, const tripoint_bub_ms & );
int note_bionics( player *, item *, bool, const tripoint_bub_ms & );
int ma_manual( player *, item *, bool, const tripoint_bub_ms & );
int crowbar( player *, item *, bool, const tripoint_bub_ms & );
int makemound( player *, item *, bool, const tripoint_bub_ms & );
int dig( player *, item *, bool, const tripoint_bub_ms & );
int dig_channel( player *, item *, bool, const tripoint_bub_ms & );
int fill_pit( player *, item *, bool, const tripoint_bub_ms & );
int clear_rubble( player *, item *, bool, const tripoint_bub_ms & );
int siphon( player *, item *, bool, const tripoint_bub_ms & );
int jackhammer( player *, item *, bool, const tripoint_bub_ms & );
int pickaxe( player *, item *, bool, const tripoint_bub_ms & );
int burrow( player *, item *, bool, const tripoint_bub_ms & );
int geiger( player *, item *, bool, const tripoint_bub_ms & );
int teleport( player *, item *, bool, const tripoint_bub_ms & );
int can_goo( player *, item *, bool, const tripoint_bub_ms & );
int throwable_extinguisher_act( player *, item *, bool, const tripoint_bub_ms & );
int directional_hologram( player *, item *, bool, const tripoint_bub_ms & );
int capture_monster_veh( player *, item *, bool, const tripoint_bub_ms & );
int capture_monster_act( player *, item *, bool, const tripoint_bub_ms & );
int debug_grenade( player *, item *, bool, const tripoint_bub_ms & );
int debug_grenade_act( player *, item *, bool, const tripoint_bub_ms & );
int c4( player *, item *, bool, const tripoint_bub_ms & );
int arrow_flammable( player *, item *, bool, const tripoint_bub_ms & );
int acidbomb_act( player *, item *, bool, const tripoint_bub_ms & );
int grenade_inc_act( player *, item *, bool, const tripoint_bub_ms & );
int molotov_lit( player *, item *, bool, const tripoint_bub_ms & );
int firecracker_pack( player *, item *, bool, const tripoint_bub_ms & );
int firecracker_pack_act( player *, item *, bool, const tripoint_bub_ms & );
int firecracker( player *, item *, bool, const tripoint_bub_ms & );
int firecracker_act( player *, item *, bool, const tripoint_bub_ms & );
int mininuke( player *, item *, bool, const tripoint_bub_ms & );
int pheromone( player *, item *, bool, const tripoint_bub_ms & );
int pick_lock( player *, item *, bool, const tripoint_bub_ms & );
int portal( player *, item *, bool, const tripoint_bub_ms & );
int tazer( player *, item *, bool, const tripoint_bub_ms & );
int tazer2( player *, item *, bool, const tripoint_bub_ms & );
int mp3_on( player *, item *, bool, const tripoint_bub_ms & );
int rpgdie( player *, item *, bool, const tripoint_bub_ms & );
int dive_tank( player *, item *, bool, const tripoint_bub_ms & );
int gasmask( player *, item *, bool, const tripoint_bub_ms & );
int portable_game( player *, item *, bool, const tripoint_bub_ms & );
int vibe( player *, item *, bool, const tripoint_bub_ms & );
int vortex( player *, item *, bool, const tripoint_bub_ms & );
int dog_whistle( player *, item *, bool, const tripoint_bub_ms & );
int call_of_tindalos( player *, item *, bool, const tripoint_bub_ms & );
int blood_draw( player *, item *, bool, const tripoint_bub_ms & );
int mind_splicer( player *, item *, bool, const tripoint_bub_ms & );
void cut_log_into_planks( player & );
int lumber( player *, item *, bool, const tripoint_bub_ms & );
int chop_tree( player *, item *, bool, const tripoint_bub_ms & );
int chop_logs( player *, item *, bool, const tripoint_bub_ms & );
int oxytorch( player *, item *, bool, const tripoint_bub_ms & );
int hacksaw( player *, item *, bool, const tripoint_bub_ms & );
int boltcutters( player *, item *, bool, const tripoint_bub_ms & );
int mop( player *, item *, bool, const tripoint_bub_ms & );
int spray_can( player *, item *, bool, const tripoint_bub_ms & );
int towel( player *, item *, bool, const tripoint_bub_ms & );
int unfold_generic( player *, item *, bool, const tripoint_bub_ms & );
int adrenaline_injector( player *, item *, bool, const tripoint_bub_ms & );
int jet_injector( player *, item *, bool, const tripoint_bub_ms & );
int stimpack( player *, item *, bool, const tripoint_bub_ms & );
int contacts( player *, item *, bool, const tripoint_bub_ms & );
int talking_doll( player *, item *, bool, const tripoint_bub_ms & );
int bell( player *, item *, bool, const tripoint_bub_ms & );
int seed( player *, item *, bool, const tripoint_bub_ms & );
int oxygen_bottle( player *, item *, bool, const tripoint_bub_ms & );
int radio_mod( player *, item *, bool, const tripoint_bub_ms & );
int remove_all_mods( player *, item *, bool, const tripoint_bub_ms & );
int good_fishing_spot( const tripoint_bub_ms & );
int fishing_rod( player *, item *, bool, const tripoint_bub_ms & );
int fish_trap( player *, item *, bool, const tripoint_bub_ms & );
int gun_clean( player *, item *, bool, const tripoint_bub_ms & );
int gun_repair( player *, item *, bool, const tripoint_bub_ms & );
int gunmod_attach( player *, item *, bool, const tripoint_bub_ms & );
int toolmod_attach( player *, item *, bool, const tripoint_bub_ms & );
int unpack_item( player *, item *, bool, const tripoint_bub_ms & );
int pack_cbm( player *p, item *it, bool, const tripoint_bub_ms & );
int pack_item( player *, item *, bool, const tripoint_bub_ms & );
int radglove( player *, item *, bool, const tripoint_bub_ms & );
int robotcontrol( player *, item *, bool, const tripoint_bub_ms & );
// Helper for validating a potential taget of robot control
bool robotcontrol_can_target( player *, const monster & );
int einktabletpc( player *, item *, bool, const tripoint_bub_ms & );
int camera( player *, item *, bool, const tripoint_bub_ms & );
int ehandcuffs( player *, item *, bool, const tripoint_bub_ms & );
int foodperson( player *, item *, bool, const tripoint_bub_ms & );
int tow_attach( player *, item *, bool, const tripoint_bub_ms & );
int cable_attach( player *, item *, bool, const tripoint_bub_ms & );
int shavekit( player *, item *, bool, const tripoint_bub_ms & );
int hairkit( player *, item *, bool, const tripoint_bub_ms & );
int weather_tool( player *, item *, bool, const tripoint_bub_ms & );
int ladder( player *, item *, bool, const tripoint_bub_ms & );
int solarpack( player *, item *, bool, const tripoint_bub_ms & );
int solarpack_off( player *, item *, bool, const tripoint_bub_ms & );
int weak_antibiotic( player *, item *, bool, const tripoint_bub_ms & );
int strong_antibiotic( player *, item *, bool, const tripoint_bub_ms & );
int melatonin_tablet( player *, item *, bool, const tripoint_bub_ms & );
int coin_flip( player *, item *, bool, const tripoint_bub_ms & );
int play_game( player *, item *, bool, const tripoint_bub_ms & );
int magic_8_ball( player *, item *, bool, const tripoint_bub_ms & );
int toggle_heats_food( player *, item *, bool, const tripoint_bub_ms & );
int toggle_ups_charging( player *, item *, bool, const tripoint_bub_ms & );
int report_grid_charge( player *, item *, bool, const tripoint_bub_ms & );
int report_grid_connections( player *, item *, bool, const tripoint_bub_ms & );
int modify_grid_connections( player *, item *, bool, const tripoint_bub_ms & );
int report_fluid_grid_connections( player *, item *, bool, const tripoint_bub_ms & );
int modify_fluid_grid_connections( player *, item *, bool, const tripoint_bub_ms & );
int bullet_vibe_on( player *, item *, bool, const tripoint_bub_ms & );

// MACGUFFINS

int radiocar( player *, item *, bool, const tripoint_bub_ms & );
int radiocaron( player *, item *, bool, const tripoint_bub_ms & );
int radiocontrol( player *, item *, bool, const tripoint_bub_ms & );

int autoclave( player *, item *, bool, const tripoint_bub_ms & );

int remoteveh( player *, item *, bool, const tripoint_bub_ms & );

int craft( player *, item *, bool, const tripoint_bub_ms & );

int disassemble( player *, item *, bool, const tripoint_bub_ms & );

// ARTIFACTS
/* This function is used when an artifact is activated.
   It examines the item's artifact-specific properties.
   See artifact.h for a list.                        */
int artifact( player *, item *, bool, const tripoint_bub_ms & );

// Helper for listening to music, might deserve a better home, but not sure where.
void play_music( player &p, const tripoint_bub_ms &source, int volume, int max_morale );
int towel_common( player *, item *, bool );

// Helper for handling pesky wannabe-artists
int handle_ground_graffiti( player &p, item *it, const std::string &prefix,
                            const tripoint_bub_ms &where );

// Helper for wood chopping
int chop_moves( Character &ch, item &tool );

// LEGACY
int cauterize_hotplate( player *, item *, bool, const tripoint_bub_ms & );

} // namespace iuse

void remove_radio_mod( item &it, player &p );


using use_function_pointer = int ( * )( player *, item *, bool, const tripoint_bub_ms & );

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
        virtual int use( player &, item &, bool, const tripoint_bub_ms & ) const = 0;
        virtual ret_val<bool> can_use( const Character &, const item &, bool,
                                       const tripoint_bub_ms & ) const;
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
        virtual void on_placed( item &, const map &, const tripoint_bub_ms & ) const {}
};

struct use_function {
    protected:
        cata::clone_ptr<iuse_actor> actor;

    public:
        use_function() = default;
        use_function( const std::string &type, use_function_pointer f );
        use_function( std::unique_ptr<iuse_actor> f ) : actor( std::move( f ) ) {}

        int call( player &, item &, bool, const tripoint_bub_ms & ) const;
        ret_val<bool> can_call( const Character &, const item &, bool t, const tripoint_bub_ms &pos ) const;

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

