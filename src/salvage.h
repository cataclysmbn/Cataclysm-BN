#pragma once

#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include "coordinates.h"
#include "ret_val.h"
#include "type_id.h"
#include "units.h"

class Character;
class inventory;
class item;
class material_type;
class player;
class JsonIn;

struct quality;
struct tripoint;


namespace salvage
{

using quality_cache = std::map<quality_id, std::map<int, int>>;

enum class q_result {
    yes,
    ignore,
    abort,
    skip,
    fail
};

ret_val<bool> try_salvage( const item &, quality_cache & );

units::mass minimal_weight_to_cut( const item &it );

/// <summary>
/// Returns a map of salvage results for the given item.
/// The map contains resulting item types and their respective amounts.
/// </summary>
/// <param name="target">Salvaged item</param>
std::unordered_map<itype_id, float> salvage_results( const item &target );

void complete_salvage( Character &who, item &cut, tripoint_abs_ms pos );

int moves_to_salvage( const item &target );

bool has_salvage_tools( quality_cache &, const material_id &material );
bool has_salvage_tools( quality_cache &, const item &item, bool strict = false );
bool menu_salvage_single( player &you );
bool prompt_salvage_single( Character &who, item &target );
bool salvage_single( Character &who, item &target );
bool salvage_all( Character &who );

void populate_salvage_materials( quality &q );
};


