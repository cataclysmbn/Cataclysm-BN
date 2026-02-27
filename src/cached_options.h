#pragma once

// A collection of options which are accessed frequently enough that we don't
// want to pay the overhead of a string lookup each time one is tested.
// They should be updated when the corresponding option is changed (in options.cpp).

/**
 * Set to true when running in test mode (e.g. unit tests, checking mods).
 * Does not correspond to any game option, but still requires
 * caching due to heavy usage.
 */
extern bool test_mode;

/**
 * Extended debugging mode, can be toggled during game.
 * If enabled some debug messages in the normal player message log are shown,
 * and other windows might have verbose display (e.g. vehicle window).
 */
extern bool debug_mode;

/**
 * Report extra problems in JSONs.
 * Because either @ref test_mode or @ref json_report_unused_fields is set.
 */
extern bool json_report_strict;

/**
 * Use tiles for display. Always false for ncurses build,
 * but can be toggled in sdl build.
 */
extern bool use_tiles;

/**
 * Use tiles for 'm'ap display. Always false for ncurses build,
 * but can be toggled in sdl build.
 */
extern bool use_tiles_overmap;

/** Flow direction for the message log in the sidebar. */
extern bool log_from_top;
extern int message_ttl;
extern int message_cooldown;

/** Display mod source for items, furniture, terrain and monsters.*/
extern bool display_mod_source;
/** Display internal IDs for items, furniture, terrain and monsters.*/
extern bool display_object_ids;

/**
 * Circular distances.
 * If true, calculate distance in a realistic way [sqrt(dX^2 + dY^2)].
 * If false, use roguelike distance [maximum of dX and dY].
 */
extern bool trigdist;

/** 3D FoV enabled/disabled. */
extern bool fov_3d;

/** 3D FoV range, in Z levels, in both directions. */
extern int fov_3d_z_range;

/** Using isometric tileset. */
extern bool tile_iso;

/** Static z level effect. */
extern bool static_z_effect;

/** Render overmap air as transparent and render tiles that are below. */
extern bool overmap_transparency;

/**
 * Whether to show the pixel minimap. Always false for ncurses build,
 * but can be toggled during game in sdl build.
 */
extern bool pixel_minimap_option;

/**
 * Items on the map with at most this distance to the player are considered
 * available for crafting, see inventory::form_from_map
*/
extern int PICKUP_RANGE;

/**
 * If true, disables all debug messages. Only used for debugging "weird" saves.
 */
extern bool dont_debugmsg;


/* Options related to fungal activity */
struct FungalOptions {
    /**
    * Enables fungal stalk to be spawned on infested terrain from 'PLANT' attack function.
    */
    bool young_allowed;
    /**
    * Enables fungal infestation on FLAT tiles (indoors houses/asphalt/etc..).
    */
    bool spread_on_flat_tiles_allowed;
    /**
    * Base rate to spawn fungal stalk from 'PLANT' attack function.
    * Chance to spawn: 1 / (MON_FUNGALOID_YOUNG_SPAWN_BASE_RATE + num_creatures_in_bubble / MON_FUNGALOID_YOUNG_SPAWN_BUBBLE_CREATURES_DIVIDER)
    */
    int young_spawn_base_rate;
    /**
    * Number of creatures in bubble will be divided on that number to calculate chance to spawn fungal stalk from 'PLANT' attack function.
    * Chance to spawn: 1 / (MON_FUNGALOID_YOUNG_SPAWN_BASE_RATE + num_creatures_in_bubble / MON_FUNGALOID_YOUNG_SPAWN_BUBBLE_CREATURES_DIVIDER)
    */
    int young_spawn_bubble_creatures_divider;
    /**
    * Chance (0 to 1) to trigger spore creation on fungal_effects::fungalize method
    * (for character; infested furniture/terrain bash; FUNGUS death function, FUNGUS_ADVANCED attack function)
    * (note: change for FUNGUS_ADVANCED attack function will be affected by random radius change (1~2)
    * and will proportionally decrease as FUNGUS_ADVANCED_CREATURES_THRESHOLD is passed)",
    */
    float spore_chance;
    /**
    * If number of creatures nearby will pass this threshold - chance to spawn spore by FUNGUS_ADVANCED attack will be proportionally decreased.
    */
    int advanced_creatures_threshold;
    /**
    * Global value that affects ability of fungal_effects::fungalize to spawn spores.
    * If there is more creature nearby than specified value - spore will not spawn.
    */
    int spore_creatures_threshold;
};

extern FungalOptions fungal_opt;

enum class error_log_format_t {
    human_readable,
    // Output error messages in github action command format (currently json only)
    // See https://docs.github.com/en/free-pro-team@latest/actions/reference/workflow-commands-for-github-actions#setting-an-error-message
    github_action,
};
#ifndef CATA_IN_TOOL
extern error_log_format_t error_log_format;
#else
constexpr error_log_format_t error_log_format = error_log_format_t::human_readable;
#endif


