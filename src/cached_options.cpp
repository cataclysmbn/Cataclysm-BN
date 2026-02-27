#include "cached_options.h"

#include "options.h"

bool test_mode = false;
bool debug_mode = false;
bool json_report_strict = true;
bool use_tiles = false;
bool use_tiles_overmap = false;
bool log_from_top;
int message_ttl;
int message_cooldown;
bool display_mod_source;
bool display_object_ids;
bool trigdist;
bool fov_3d;
bool static_z_effect = false;
bool overmap_transparency = true;
int fov_3d_z_range;
bool tile_iso;
bool pixel_minimap_option = false;
int PICKUP_RANGE;

bool monster_lod_enabled = false;
int  lod_tier_full_dist = 20;
int  lod_tier_coarse_dist = 40;
int  lod_demotion_cooldown = 3;
int  lod_action_budget = 128;
int  lod_macro_interval = 3;
int  lod_coarse_scent_interval = 3;
int  lod_group_morale_max_tier = 0;

bool parallel_enabled = false;
bool parallel_monster_planning = true;
int  monster_plan_chunk_size = 8;
bool parallel_map_cache = true;
bool parallel_scent_update = true;

FungalOptions fungal_opt;

error_log_format_t error_log_format = error_log_format_t::human_readable;
