#!/bin/sh

# Extract JSON strings from base game and in-repo mods

lang/extract_json_strings.py \
    --project "cataclysm-bn" \
    --tracked-only \
    --warn-unused-types \
    \
    -i "data/raw" \
    -i "data/json" \
    -i "data/mods" \
    -i "data/help" \
    \
    -e "data/json/anatomy.json" \
    -e "data/mods/replacements.json" \
    -e "data/raw/color_templates/no_bright_background.json" \
    -E "data/mods/TEST_DATA/" \
    \
    -s "data/json/overmap_terrain.json" \
    -s "data/json/scores.json" \
    -s "data/json/traps.json" \
    -s "data/json/vehicleparts/" \
    -s "data/raw/keybindings.json" \
    -s "data/mods/alt_map_key/overmap_terrain.json" \
    -s "data/mods/DeoxyMod/Deoxy_vehicle_parts.json" \
    -s "data/mods/More_Survival_Tools/start_locations.json" \
    -s "data/mods/NPC_Traits/npc_classes.json" \
    -s "data/mods/Tanks/monsters.json" \
    \
    "$@"
