#pragma once

#include "units.h"

// Fixed window sizes.
static constexpr int HP_HEIGHT = 14;
static constexpr int HP_WIDTH = 7;
static constexpr int MINIMAP_HEIGHT = 7;
static constexpr int MINIMAP_WIDTH = MINIMAP_HEIGHT;
static constexpr int MONINFO_HEIGHT = 12;
static constexpr int MONINFO_WIDTH = 48;
static constexpr int MESSAGES_HEIGHT = 8;
static constexpr int MESSAGES_WIDTH = 48;
static constexpr int LOCATION_HEIGHT = 1;
static constexpr int LOCATION_WIDTH = 48;
static constexpr int STATUS_HEIGHT = 4;
static constexpr int STATUS_WIDTH = 55;

static constexpr int EXPLOSION_MULTIPLIER = 7;

// Really just a sanity check for functions not tested beyond this. in theory 4096 works (`InvletInvlet).
static constexpr int MAX_ITEM_IN_SQUARE = 4096;
// no reason to differ.
static constexpr int MAX_ITEM_IN_VEHICLE_STORAGE = MAX_ITEM_IN_SQUARE;
// only can wear a maximum of two of any type of clothing.
static constexpr int MAX_WORN_PER_TYPE = 2;

// Maximum configurable reality bubble size.  MAPSIZE is sized for this maximum
// so that all compile-time arrays (shadowcasting, pathfinding, scent) are large
// enough regardless of the runtime setting.  The option itself is capped to this.
static constexpr int REALITY_BUBBLE_SIZE_MAX = 4;

// MAPSIZE is the number of submaps in each direction.
// Formula: 4 * REALITY_BUBBLE_SIZE_MAX + 3 (e.g. size=2 → 11, size=4 → 19).
static constexpr int MAPSIZE = 4 * REALITY_BUBBLE_SIZE_MAX + 3;
static constexpr int HALF_MAPSIZE = static_cast<int>( MAPSIZE / 2 );

// SEEX/SEEY define the size of a nonant, or grid.
// All map segments will need to be at least this wide.
static constexpr int SEEX = 12;
static constexpr int SEEY = SEEX;

static constexpr int MAPSIZE_X = SEEX * MAPSIZE;
static constexpr int MAPSIZE_Y = SEEY * MAPSIZE;

static constexpr int HALF_MAPSIZE_X = SEEX * HALF_MAPSIZE;
static constexpr int HALF_MAPSIZE_Y = SEEY * HALF_MAPSIZE;

static constexpr int MAX_VIEW_DISTANCE = SEEX * HALF_MAPSIZE;

/**
 * Size of the overmap. This is the number of overmap terrain tiles per dimension in one overmap,
 * it's just like SEEX/SEEY for submaps.
*/
static constexpr int OMAPX = 180;
static constexpr int OMAPY = OMAPX;

// Size of a square unit of terrain saved to a directory.
static constexpr int SEG_SIZE = 32;

// Size of a square unit of tile memory saved in a single file, in mm_submaps.
static constexpr int MM_REG_SIZE = 8;

// Number of z-levels below 0 (not including 0).
static constexpr int OVERMAP_DEPTH = 10;
// Number of z-levels above 0 (not including 0).
static constexpr int OVERMAP_HEIGHT = 10;
// Total number of z-levels.
static constexpr int OVERMAP_LAYERS = 1 + OVERMAP_DEPTH + OVERMAP_HEIGHT;

// Maximum move cost when handling an item.
static constexpr int MAX_HANDLING_COST = 400;
// Move cost of accessing an item in inventory.
static constexpr int INVENTORY_HANDLING_PENALTY = 100;
// Move cost of accessing an item lying on the map. TODO: Less if player is crouching.
static constexpr int MAP_HANDLING_PENALTY = 80;
// Move cost of accessing an item lying on a vehicle.
static constexpr int VEHICLE_HANDLING_PENALTY = 80;

// Amount by which to charge an item for each unit of plutonium cell.
static constexpr int PLUTONIUM_CHARGES = 500;

// Temperature constants.
namespace temperatures
{

/// Average annual temperature used for climate, weather and temperature calculation.
constexpr units::temperature annual_average = 6_c;

// temperature at which something starts is considered HOT.
constexpr units::temperature hot = 38_c;

// the "normal" temperature midpoint between cold and hot.
constexpr units::temperature normal = 21_c;

// Temperature inside an active fridge
constexpr units::temperature fridge = 2_c;

// Temperature at which things are considered "cold".
constexpr units::temperature cold = 5_c;

// Temperature inside an active freezer.
constexpr units::temperature freezer = -5_c;

// Temperature in which water freezes.
constexpr units::temperature freezing = 0_c;

// Arbitrary constant for root cellar temperature
constexpr units::temperature root_cellar = annual_average;
} // namespace temperatures

// Weight per level of LIFT/JACK tool quality.
static constexpr units::mass TOOL_LIFT_FACTOR = 500_kilogram; // 500kg/level

// Cap JACK requirements to support arbitrarily large vehicles.
static constexpr units::mass JACK_LIMIT = 8500_kilogram;

// Slowest speed at which a gun can be aimed.
static constexpr int MAX_AIM_COST = 10;

// Maximum (effective) level for a skill.
static constexpr int MAX_SKILL = 10;

// Maximum (effective) level for a stat.
static constexpr int MAX_STAT = 20;

// Maximum range at which ranged attacks can be executed.
// Equals the compile-time maximum view distance; actual runtime cap uses g_max_view_distance.
static constexpr int RANGE_HARD_CAP = MAX_VIEW_DISTANCE;

// ---------------------------------------------------------------------------
// Runtime reality-bubble configuration (set from REALITY_BUBBLE_SIZE option).
// Defaults match size=2, which gives the original 11×11 submap grid.
// ---------------------------------------------------------------------------

/// Player-facing size setting (1–REALITY_BUBBLE_SIZE_MAX, default 2).
extern int g_reality_bubble_size;
/// Submap radius = 2*size+1 (half the loaded grid width).
extern int g_half_mapsize;
/// Total submaps per dimension = 4*size+3.
extern int g_mapsize;
/// Tile width of the loaded area = SEEX * g_mapsize.
extern int g_mapsize_x;
/// Tile height of the loaded area = SEEY * g_mapsize.
extern int g_mapsize_y;
/// Half tile width = SEEX * g_half_mapsize.
extern int g_half_mapsize_x;
/// Half tile height = SEEY * g_half_mapsize.
extern int g_half_mapsize_y;
/// Maximum player sight range = SEEX * g_half_mapsize.
extern int g_max_view_distance;
/// Visibility threshold at max view distance for "obstructed" determination.
/// Computed as 1/exp(LIGHT_TRANSPARENCY_OPEN_AIR * g_max_view_distance).
/// Scales with bubble size so vision cutoff matches g_max_view_distance.
extern float g_visible_threshold;

// Accuracy levels which a shots tangent must be below.
constexpr double accuracy_headshot = 0.1;
constexpr double accuracy_critical = 0.2;
constexpr double accuracy_goodhit  = 0.5;
constexpr double accuracy_standard = 0.8;
constexpr double accuracy_grazing  = 1.0;

// The maximum level recoil will ever reach.
// This corresponds to the level of accuracy of a "snap" or "hip" shot.
constexpr double MAX_RECOIL = 3000;

// Minimum item damage output of relevant type to allow using with relevant weapon skill.
static constexpr int MELEE_STAT = 5;

// Effective lower bound to combat skill levels when CQB bionic is active.
static constexpr int BIO_CQB_LEVEL = 5;

// Minimum size of a horde to show up on the minimap.
static constexpr int HORDE_VISIBILITY_SIZE = 3;

/**
 * Used to limit the random seed during noise calculation. A large value flattens the noise generator to zero.
 * Windows has a rand limit of 32768, other operating systems can have higher limits.
*/
constexpr int SIMPLEX_NOISE_RANDOM_SEED_LIMIT = 32768;

/**
 * activity levels, used for BMR.
 * these levels are normally used over the length of
 * days to weeks in order to calculate your total BMR
 * but we are making it more granular to be able to have
 * variable activity levels.
 * as such, when determining your activity level
 * in the json, think about what it would be if you
 * did this activity for a longer period of time.
*/
constexpr float NO_EXERCISE = 1.2f;
constexpr float LIGHT_EXERCISE = 1.375f;
constexpr float MODERATE_EXERCISE = 1.55f;
constexpr float ACTIVE_EXERCISE = 1.725f;
constexpr float EXTRA_EXERCISE = 1.9f;

// these are the lower bounds of each of the weight classes.
namespace character_weight_category
{
constexpr float emaciated = 14.0f;
constexpr float underweight = 16.0f;
constexpr float normal = 18.5f;
constexpr float overweight = 25.0f;
constexpr float obese = 30.0f;
constexpr float very_obese = 35.0f;
constexpr float morbidly_obese = 40.0f;
} // namespace character_weight_category



