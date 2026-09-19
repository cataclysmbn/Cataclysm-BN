# Field Types

```jsonc
{
  // Required data
  "type": "field_type", // this is a field type
  "id": "fd_gum_web", // id of the field
  // Array of intensities of the field.
  // The first one is intensity 1, they increase by one each time
  "intensity_levels": [
    {
      "name": "shadow", // name of this level of intensity
      "sym": "a", // Symbol for this field intensity level
      "color": "light_red", // Curses color for the field symbol
      // Visibility
      "transparent": false, // Weather the field can be seen through
      "light_override": 3.7, // The amount of light to be used, ignores ambient light. 100 is daylight 0 is 100% dark.
      "light_color": "#ffd08a", // Color of light to emit
      "light_emitted": 20, // Amount of light to emit
      // Movement
      "dangerous": true, // Weather this intensity is dangerous to go through
      "move_cost": 50, // Move cost of moving through the field
      // Radiation
      "extra_radiation_min": 10, // Minimum amount of extra radiation from entering field
      "extra_radiation_max": 100, // Maximum amount of extra radiation from entering field
      "radiation_hurt_damage_min": 1, // Minimum amount of pain to cause to all bodyparts
      "radiation_hurt_damage_max": 3, // Maximum amount of pain to cause to all bodyparts
      "radiation_hurt_message": "The radioactive gas burns!", // Message to print when getting pain from it
      // Intensity manipulation
      "intensity_upgrade_chance": 10, // One in x chance to upgrade intensity every few ticks
      "intensity_upgrade_duration": 1, // Number of ticks to wait before upgrading intensity
      // Monster Group spawning
      "monster_spawn_chance": 10, // Chance to spawn monsters
      "monster_spawn_count": 10, // Number of monsters to spawn
      "monster_spawn_radius": 10, // Radius to spawn monsters in
      "monster_spawn_group": "GROUP_ZOMBIE", // Group to spawn
      // Misc
      "convection_temperature_mod": 10, // Amount of temperature to add
      // Effects
      "effects": [{
        // Basic information
        "effect_id": "sap", // Effect id to apply
        "body_part": "torso", // Bodypart with the stated id
        "intensity": 1, // Intensity of the effect
        // Duration
        "max_duration": "2 seconds", // Maximum duration of the effect
        "min_duration": "2 seconds", // Minimum duration of the effect
        // Immunity and application chances
        "is_environmental": true, // Will environmental protection protect
        "immune_in_vehicle": true, // Immune when on any vehicle part
        "chance_in_vehicle": 10, // One in x chance to get the effect when meeting above condition
        "immune_inside_vehicle": true, // Immune when on a vehicle part that qualifies as inside
        "chance_inside_vehicle": 10, // One in x chance to get the effect when meeting above condition
        "immune_outside_vehicle": true, // Immune when not on a vehicle part that qualifies as inside
        "chance_outside_vehicle": 10, // On in x chance to get the effect when meeting above condition
        // Messages
        "message": "The sap is sticky!", // Message to print when recieving the effect
        "message_npc": "The sap sticks to <npcname>!", // Message to print when NPC gets effect
        "message_type": "bad", // What type of message is this?
      }],
    },
  ],
  // Graphical
  "display_items": true, // Display the items on the files or not?
  "display_field": true, // Display the field or not?
  "priority": 1, // Priority to display the field's symbol / sprite
  // Immunities
  "immunity_data": {
    "traits": ["M_IMMUNE"], // Array of traits that prevent effects from the field
    "body_part_env_resistance": [["mouth", 15], ["eyes", 15]], // Environemental resistance on bodyparts required to protect from the field
  },
  "immune_mtypes": ["mon_zombie"], // Monster Types immune to the field
  "has_fire": true, // Does fire immunity make you immune to this field
  "has_acid": true, // Does acid immunity?
  "has_elec": true, // What about electricity?
  "has_fume": true, // Shall we change the background color to white on curses?
  // Decaying and spreading
  "underwater_age_speedup": "1 minute", // This many more turns pass on it's age in water
  "outdoor_age_speedup": "1 minute", // This many more turns pass on it's age while in water
  "decay_amount_factor": 5, // Yet another turn increment to the field's age
  "percent_spread": 10, // Percent chance to spread each turn
  "half_life": "10 minutes", // Number of turns til half of it is gone
  "stacking_type": "duration", // Does more field add `intensity` or `duration`
  "wandering_field": "fd_toxic_gas", // Field that this field produces
  "accelerated_decay": true, // Does this field go downward with terrain shifts?
  // Misc
  "gas_absorbption_factor": 10, // Number of charges to eat from the gas mask
  "dirty_transparency_cache": true, // Weather to forcefully recalc the transparency cache. It is automatically set with transparent being false
  "description_affix": "covered in", // Puts this in front of every intensity level name
  "phase": "solid", // Is it solid liquid gas or plasma? Mainly gas has effect on propagation
  "is_splattering": true, // Does this apply blood to vehicle parts?
  "apply_slime_factor": 2, // Applies scent of factor * intensity
  "scent_neutralization": 2, // Reduces scent by this value
  // NPC Complain
  "npc_complain": {
    "chance": 20, // One out of x chance to complain
    "issue": "crack_smoke", // Talk tag to complain about
    "duration": "30 minutes", // How long will they be complaining
    "speech": "<crack_smoke>", // Once again the talk tag to have spoken
  },
  // Monster spawning
  "monster_spawn_chance": 2, // One in x chance to spawn monster
  "monster_spawn_count": 3, // How many monsters to spawn
  "monster_spawn_group": "GROUP_ZOMBIE", // What monster group to spawn
  "monster_spawn_radius": 5, // Tile radius of monster spawn locations
  // Bash data
  "bash": {
    "str_min": 1, // lower bracket of bashing damage required to bash
    "str_max": 3, // higher bracket
    "sound_vol": 2, // noise made when succesfully bashing the field
    "sound_fail_vol": 2, // noise made when failing to bash the field
    "sound": "shwip", // sound on success
    "sound_fail": "shwomp", // sound on failure
    "msg_success": "You brush the gum web aside.", // message on success
    "move_cost": 120, // how many moves it costs to succesfully bash that field (default: 100)
    "items": [ // item dropped upon succesful bashing
      { "item": "2x4", "count": [5, 8] },
      { "item": "nail", "charges": [6, 8] },
      {
        "item": "splinter",
        "count": [3, 6],
      },
      { "item": "rag", "count": [40, 55] },
      { "item": "scrap", "count": [10, 20] },
    ],
  },
}
```
