#pragma once

#include <list>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "calendar.h"
#include "coordinates.h"
#include "itype.h"
#include "ret_val.h"
#include "type_id.h"

class item;
class player;

using seed_tuple = std::tuple<itype_id, std::string, int>;

namespace iexamine
{

void egg_sack_generic( player &p, const tripoint_bub_ms &examp, const mtype_id &montype );

void none( player &p, const tripoint_bub_ms &examp );

void gaspump( player &p, const tripoint_bub_ms &examp );
void atm( player &p, const tripoint_bub_ms &examp );
void vending( player &p, const tripoint_bub_ms &examp );
void toilet( player &p, const tripoint_bub_ms &examp );
auto fluid_grid_fixture( player &p, const tripoint_bub_ms &examp ) -> void;
void elevator( player &p, const tripoint_bub_ms &examp );
void nanofab( player &p, const tripoint_bub_ms &examp );
void nanoforge( player &p, const tripoint_bub_ms &examp );
void toggle_lights( player &p, const tripoint_bub_ms &examp );
void controls_gate( player &p, const tripoint_bub_ms &examp );
void cardreader( player &p, const tripoint_bub_ms &examp );
void cardreader_robofac( player &p, const tripoint_bub_ms &examp );
void cardreader_foodplace( player &p, const tripoint_bub_ms &examp );
void intercom( player &p, const tripoint_bub_ms &examp );
void cvdmachine( player &p, const tripoint_bub_ms &examp );
void rubble( player &p, const tripoint_bub_ms &examp );
void chainfence( player &p, const tripoint_bub_ms &examp );
void bars( player &p, const tripoint_bub_ms &examp );
void deployed_furniture( player &p, const tripoint_bub_ms &pos );
void portable_structure( player &p, const tripoint_bub_ms &examp );
void pit( player &p, const tripoint_bub_ms &examp );
void pit_covered( player &p, const tripoint_bub_ms &examp );
void slot_machine( player &p, const tripoint_bub_ms &examp );
void safe( player &p, const tripoint_bub_ms &examp );
void gunsafe_el( player &p, const tripoint_bub_ms &examp );
void harvest_furn_nectar( player &p, const tripoint_bub_ms &examp );
void harvest_furn( player &p, const tripoint_bub_ms &examp );
void harvest_ter_nectar( player &p, const tripoint_bub_ms &examp );
void harvest_ter( player &p, const tripoint_bub_ms &examp );
void harvested_plant( player &p, const tripoint_bub_ms &examp );
void locked_object( player &p, const tripoint_bub_ms &examp );
void locked_object_pickable( player &p, const tripoint_bub_ms &examp );
void fault( player &p, const tripoint_bub_ms &examp );
void notify( player &p, const tripoint_bub_ms &pos );
void transform( player &p, const tripoint_bub_ms &pos );
void pedestal_wyrm( player &p, const tripoint_bub_ms &examp );
void pedestal_temple( player &p, const tripoint_bub_ms &examp );
void door_peephole( player &p, const tripoint_bub_ms &examp );
void fswitch( player &p, const tripoint_bub_ms &examp );
void flower_tulip( player &p, const tripoint_bub_ms &examp );
void flower_spurge( player &p, const tripoint_bub_ms &examp );
void flower_poppy( player &p, const tripoint_bub_ms &examp );
void flower_cactus( player &p, const tripoint_bub_ms &examp );
void flower_bluebell( player &p, const tripoint_bub_ms &examp );
void flower_dahlia( player &p, const tripoint_bub_ms &examp );
void flower_marloss( player &p, const tripoint_bub_ms &examp );
void egg_sackbw( player &p, const tripoint_bub_ms &examp );
void egg_sackcs( player &p, const tripoint_bub_ms &examp );
void egg_sackws( player &p, const tripoint_bub_ms &examp );
void fungus( player &p, const tripoint_bub_ms &examp );
void dirtmound( player &p, const tripoint_bub_ms &examp );
void aggie_plant( player &p, const tripoint_bub_ms &examp );
void tree_hickory( player &p, const tripoint_bub_ms &examp );
void tree_maple( player &p, const tripoint_bub_ms &examp );
void tree_maple_tapped( player &p, const tripoint_bub_ms &examp );
void shrub_marloss( player &p, const tripoint_bub_ms &examp );
void tree_marloss( player &p, const tripoint_bub_ms &examp );
void shrub_wildveggies( player &p, const tripoint_bub_ms &examp );
void recycle_compactor( player &p, const tripoint_bub_ms &examp );
void trap( player &p, const tripoint_bub_ms &examp );
void water_source( player &p, const tripoint_bub_ms &examp );
void clean_water_source( player &, const tripoint_bub_ms &examp );
void liquid_source( player &p, const tripoint_bub_ms &examp );
void kiln_empty( player &p, const tripoint_bub_ms &examp );
void kiln_full( player &p, const tripoint_bub_ms &examp );
void arcfurnace_empty( player &p, const tripoint_bub_ms &examp );
void arcfurnace_full( player &p, const tripoint_bub_ms &examp );
void autoclave_empty( player &p, const tripoint_bub_ms & );
void autoclave_full( player &, const tripoint_bub_ms &examp );
void fireplace( player &p, const tripoint_bub_ms &examp );
void fvat_empty( player &p, const tripoint_bub_ms &examp );
void fvat_full( player &p, const tripoint_bub_ms &examp );
void keg( player &p, const tripoint_bub_ms &examp );
void reload_furniture( player &p, const tripoint_bub_ms &examp );
void use_furn_fake_item( player &p, const tripoint_bub_ms &examp );
void curtains( player &p, const tripoint_bub_ms &examp );
void sign( player &p, const tripoint_bub_ms &examp );
void pay_gas( player &p, const tripoint_bub_ms &examp );
void ledge( player &p, const tripoint_bub_ms &examp );
void autodoc( player &p, const tripoint_bub_ms &examp );
void translocator( player &p, const tripoint_bub_ms &examp );
void on_smoke_out( const tripoint_bub_ms &examp,
                   const time_point &start_time ); //activates end of smoking effects
void mill_finalize( player &, const tripoint_bub_ms &examp, const time_point &start_time );
void cloning_vat_finalize( const tripoint_bub_ms &examp, const time_point &start_time );
void cloning_vat_examine( player &p, const tripoint_bub_ms &examp );
void quern_examine( player &p, const tripoint_bub_ms &examp );
void smoker_options( player &p, const tripoint_bub_ms &examp );
void open_safe( player &p, const tripoint_bub_ms &examp );
void workbench( player &p, const tripoint_bub_ms &examp );
void dimensional_portal( player &p, const tripoint_bub_ms &examp );
void check_power( player &p, const tripoint_bub_ms &examp );
void migo_nerve_cluster( player &p, const tripoint_bub_ms &examp );
void cardreader_plutgen( player &p, const tripoint_bub_ms &examp );
void multicooker( player &p, const tripoint_bub_ms &pos );

detached_ptr<item> pour_into_keg( const tripoint_bub_ms &pos, detached_ptr<item> &&liquid );
std::optional<tripoint_bub_ms> getGasPumpByNumber( const tripoint_bub_ms &p, int number );
bool toPumpFuel( const tripoint_bub_ms &src, const tripoint_bub_ms &dst, int units );
std::optional<tripoint_bub_ms> getNearFilledGasTank( const tripoint_bub_ms &center,
        int &gas_units );

bool has_keg( const tripoint_bub_ms &pos );

std::vector<detached_ptr<item>> get_harvest_items( const itype &type, int plant_count,
                             int seed_count, bool byproducts );

// Planting functions
std::vector<seed_tuple> get_seed_entries( const std::vector<item *> &seed_inv );
int query_seed( const std::vector<seed_tuple> &seed_entries, int min_req = 1 );
void plant_seed( player &p, const tripoint_bub_ms &examp, const itype_id &seed_id );
void harvest_plant( player &p, const tripoint_bub_ms &examp, bool from_activity = false );
void fertilize_plant( player &p, const tripoint_bub_ms &tile, const itype_id &fertilizer );
itype_id choose_fertilizer( player &p, const std::string &pname, bool ask_player );
ret_val<bool> can_fertilize( player &p, const tripoint_bub_ms &tile, const itype_id &fertilizer );

// Skill training common functions
void practice_survival_while_foraging( player *p );

void power_portal( player &p, const tripoint_bub_ms &examp );
/** Generic dimensional portal (portal_tile active tile). */
void portal( player &p, const tripoint_bub_ms &examp );

} //namespace iexamine

using iexamine_function = void ( * )( player &, const tripoint_bub_ms & );
iexamine_function iexamine_function_from_string( const std::string &function_name );
