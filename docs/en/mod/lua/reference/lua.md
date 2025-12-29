---
edit: false
---

# Lua API reference

> [!NOTE]
>
> This page is auto-generated from [`data/raw/generate_docs.lua`][generate_docs]
> and should not be edited directly.

[generate_docs]: https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/data/raw/generate_docs.lua

## ActivityTypeId

### Bases

No base classes.

### Constructors

#### `ActivityTypeId.new()`

#### `ActivityTypeId.new( ActivityTypeId )`

#### `ActivityTypeId.new( string )`

### Members

#### obj

Function `( ActivityTypeId ) -> ActivityTypeRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( ActivityTypeId ) -> bool`

#### is_valid

Function `( ActivityTypeId ) -> bool`

#### str

Function `( ActivityTypeId ) -> string`

#### NULL_ID

Function `() -> ActivityTypeId`

#### __tostring

Function `( ActivityTypeId ) -> string`

#### serialize

Function `( ActivityTypeId, <cppval: 7JsonOut > )`

#### deserialize

Function `( ActivityTypeId, <cppval: 6JsonIn > )`

## Angle

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### from_radians

Function `( double ) -> Angle`

#### to_radians

Function `( Angle ) -> double`

#### from_degrees

Function `( double ) -> Angle`

#### to_degrees

Function `( Angle ) -> double`

#### from_arcmin

Function `( double ) -> Angle`

#### to_arcmin

Function `( Angle ) -> double`

#### __eq

Function `( Angle, Angle ) -> bool`

#### __lt

Function `( Angle, Angle ) -> bool`

#### __le

Function `( Angle, Angle ) -> bool`

## Avatar

### Bases

- `Player`
- `Character`
- `Creature`

### Constructors

No constructors.

### Members

#### get_active_missions

Function `( Avatar ) -> Vector( Mission )`

#### get_completed_missions

Function `( Avatar ) -> Vector( Mission )`

#### get_failed_missions

Function `( Avatar ) -> Vector( Mission )`

## BionicDataId

### Bases

No base classes.

### Constructors

#### `BionicDataId.new()`

#### `BionicDataId.new( BionicDataId )`

#### `BionicDataId.new( string )`

### Members

#### obj

Function `( BionicDataId ) -> BionicDataRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( BionicDataId ) -> bool`

#### is_valid

Function `( BionicDataId ) -> bool`

#### str

Function `( BionicDataId ) -> string`

#### NULL_ID

Function `() -> BionicDataId`

#### __tostring

Function `( BionicDataId ) -> string`

#### serialize

Function `( BionicDataId, <cppval: 7JsonOut > )`

#### deserialize

Function `( BionicDataId, <cppval: 6JsonIn > )`

## BodyPartTypeId

### Bases

No base classes.

### Constructors

#### `BodyPartTypeId.new()`

#### `BodyPartTypeId.new( BodyPartTypeId )`

#### `BodyPartTypeId.new( BodyPartTypeIntId )`

#### `BodyPartTypeId.new( string )`

### Members

#### obj

Function `( BodyPartTypeId ) -> BodyPartTypeRaw`

#### int_id

Function `( BodyPartTypeId ) -> BodyPartTypeIntId`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( BodyPartTypeId ) -> bool`

#### is_valid

Function `( BodyPartTypeId ) -> bool`

#### str

Function `( BodyPartTypeId ) -> string`

#### NULL_ID

Function `() -> BodyPartTypeId`

#### __tostring

Function `( BodyPartTypeId ) -> string`

#### serialize

Function `( BodyPartTypeId, <cppval: 7JsonOut > )`

#### deserialize

Function `( BodyPartTypeId, <cppval: 6JsonIn > )`

## BodyPartTypeIntId

### Bases

No base classes.

### Constructors

#### `BodyPartTypeIntId.new()`

#### `BodyPartTypeIntId.new( BodyPartTypeIntId )`

#### `BodyPartTypeIntId.new( BodyPartTypeId )`

### Members

#### obj

Function `( BodyPartTypeIntId ) -> BodyPartTypeRaw`

#### str_id

Function `( BodyPartTypeIntId ) -> BodyPartTypeId`

#### is_valid

Function `( BodyPartTypeIntId ) -> bool`

#### __tostring

Function `( BodyPartTypeIntId ) -> string`

## Character

### Bases

- `Creature`

### Constructors

No constructors.

### Members

#### name

Variable of type `string`

#### male

Variable of type `bool`

#### focus_pool

Variable of type `int`

#### cash

Variable of type `int`

#### follower_ids

Variable of type `Set( CharacterId )`

#### mutation_category_level

Variable of type `Map( MutationCategoryTraitId, int )`

#### getID

Function `( Character ) -> CharacterId`

#### setID

Function `( Character, CharacterId, bool )`

#### reset

Function `( Character )`

#### reset_encumbrance

Function `( Character )`

#### get_str

Function `( Character ) -> int`

#### get_dex

Function `( Character ) -> int`

#### get_per

Function `( Character ) -> int`

#### get_int

Function `( Character ) -> int`

#### get_str_base

Function `( Character ) -> int`

#### get_dex_base

Function `( Character ) -> int`

#### get_per_base

Function `( Character ) -> int`

#### get_int_base

Function `( Character ) -> int`

#### get_str_bonus

Function `( Character ) -> int`

#### get_dex_bonus

Function `( Character ) -> int`

#### get_per_bonus

Function `( Character ) -> int`

#### get_int_bonus

Function `( Character ) -> int`

#### set_str_bonus

Function `( Character, int )`

#### set_dex_bonus

Function `( Character, int )`

#### set_per_bonus

Function `( Character, int )`

#### set_int_bonus

Function `( Character, int )`

#### mod_str_bonus

Function `( Character, int )`

#### mod_dex_bonus

Function `( Character, int )`

#### mod_per_bonus

Function `( Character, int )`

#### mod_int_bonus

Function `( Character, int )`

#### mod_speed_bonus

Function `( Character, int )`

#### set_speed_bonus

Function `( Character, int )`

#### get_healthy

Function `( Character ) -> double`

#### get_healthy_mod

Function `( Character ) -> double`

#### mod_healthy

Function `( Character, double )`

#### mod_healthy_mod

Function `( Character, double, double )`

#### set_healthy

Function `( Character, double )`

#### set_healthy_mod

Function `( Character, double )`

#### get_stored_kcal

Function `( Character ) -> int`

#### max_stored_kcal

Function `( Character ) -> int`

#### get_kcal_percent

Function `( Character ) -> double`

#### get_thirst

Function `( Character ) -> int`

#### get_fatigue

Function `( Character ) -> int`

#### get_sleep_deprivation

Function `( Character ) -> int`

#### mod_stored_kcal

Function `( Character, int )`

#### mod_thirst

Function `( Character, int )`

#### mod_fatigue

Function `( Character, int )`

#### mod_sleep_deprivation

Function `( Character, int )`

#### set_stored_kcal

Function `( Character, int )`

#### set_thirst

Function `( Character, int )`

#### set_fatigue

Function `( Character, int )`

#### set_sleep_deprivation

Function `( Character, int )`

#### get_faction_id

Function `( Character ) -> FactionId`

#### set_faction_id

Function `( Character, FactionId )`

#### sight_impaired

Function `( Character ) -> bool`

#### has_alarm_clock

Function `( Character ) -> bool`

#### has_watch

Function `( Character ) -> bool`

#### get_part_temp_btu

Gets the current temperature of a specific body part (in Body Temperature Units).
Function `( Character, BodyPartTypeIntId ) -> int`

#### set_part_temp_btu

Sets a specific body part to a given temperature (in Body Temperature Units).
Function `( Character, BodyPartTypeIntId, int )`

#### get_temp_btu

Gets all bodyparts and their associated temperatures (in Body Temperature Units).
Function `( Character ) -> Map( BodyPartTypeIntId, int )`

#### set_temp_btu

Sets ALL body parts on a creature to the given temperature (in Body Temperature Units).
Function `( Character, int )`

#### blood_loss

Function `( Character, BodyPartTypeIntId ) -> int`

#### get_part_encumbrance

Function `( Character, BodyPartTypeId ) -> int`

#### is_wearing_power_armor

Function `( Character, bool ) -> bool`

#### is_wearing_active_power_armor

Function `( Character ) -> bool`

#### is_wearing_active_optcloak

Function `( Character ) -> bool`

#### in_climate_control

Function `( Character ) -> bool`

#### is_blind

Function `( Character ) -> bool`

#### is_invisible

Function `( Character ) -> bool`

#### get_movement_mode

Function `( Character ) -> CharacterMoveMode`

#### set_movement_mode

Function `( Character, CharacterMoveMode )`

#### expose_to_disease

Function `( Character, DiseaseTypeId )`

#### is_quiet

Function `( Character ) -> bool`

#### is_stealthy

Function `( Character ) -> bool`

#### uncanny_dodge

Function `( Character ) -> bool`

#### get_melee_stamina_cost

Function `( Character, Item ) -> int`

#### cough

Function `( Character, bool, int )`

#### bionic_armor_bonus

Function `( Character, BodyPartTypeIntId, DamageType ) -> double`

#### mabuff_armor_bonus

Function `( Character, DamageType ) -> int`

#### has_base_trait

Function `( Character, MutationBranchId ) -> bool`

#### has_trait_flag

Function `( Character, JsonTraitFlagId ) -> bool`

#### has_opposite_trait

Function `( Character, MutationBranchId ) -> bool`

#### set_mutation

Function `( Character, MutationBranchId )`

#### unset_mutation

Function `( Character, MutationBranchId )`

#### activate_mutation

Function `( Character, MutationBranchId )`

#### deactivate_mutation

Function `( Character, MutationBranchId )`

#### can_mount

Function `( Character, Monster ) -> bool`

#### mount_creature

Function `( Character, Monster )`

#### is_mounted

Function `( Character ) -> bool`

#### check_mount_will_move

Function `( Character, Tripoint ) -> bool`

#### check_mount_is_spooked

Function `( Character ) -> bool`

#### dismount

Function `( Character )`

#### forced_dismount

Function `( Character )`

#### is_deaf

Function `( Character ) -> bool`

#### has_two_arms

Function `( Character ) -> bool`

#### get_working_arm_count

Function `( Character ) -> int`

#### get_working_leg_count

Function `( Character ) -> int`

#### is_limb_disabled

Function `( Character, BodyPartTypeIntId ) -> bool`

#### is_limb_broken

Function `( Character, BodyPartTypeIntId ) -> bool`

#### can_run

Function `( Character ) -> bool`

#### hurtall

Function `( Character, int, Creature, bool )`

#### hitall

Function `( Character, int, int, Creature ) -> int`

#### heal

Function `( Character, BodyPartTypeIntId, int )`

#### healall

Function `( Character, int )`

#### global_square_location

Function `( Character ) -> Tripoint`

#### global_sm_location

Function `( Character ) -> Tripoint`

#### has_mabuff

Function `( Character, MartialArtsBuffId ) -> bool`

#### mabuff_tohit_bonus

Function `( Character ) -> double`

#### mabuff_dodge_bonus

Function `( Character ) -> double`

#### mabuff_block_bonus

Function `( Character ) -> int`

#### mabuff_speed_bonus

Function `( Character ) -> int`

#### mabuff_arpen_bonus

Function `( Character, DamageType ) -> int`

#### mabuff_damage_mult

Function `( Character, DamageType ) -> double`

#### mabuff_damage_bonus

Function `( Character, DamageType ) -> int`

#### mabuff_attack_cost_penalty

Function `( Character ) -> int`

#### mabuff_attack_cost_mult

Function `( Character ) -> double`

#### mutation_effect

Function `( Character, MutationBranchId )`

#### mutation_loss_effect

Function `( Character, MutationBranchId )`

#### has_active_mutation

Function `( Character, MutationBranchId ) -> bool`

#### mutate

Function `( Character )`

#### mutation_ok

Function `( Character, MutationBranchId, bool, bool ) -> bool`

#### mutate_category

Function `( Character, MutationCategoryTraitId )`

#### mutate_towards

Function `( Character, Vector( MutationBranchId ), int ) -> bool`

#### mutate_towards

Function `( Character, MutationBranchId ) -> bool`

#### mutate_towards

Function `( Character, Vector( MutationBranchId ), int ) -> bool`
Function `( Character, MutationBranchId ) -> bool`

#### remove_mutation

Function `( Character, MutationBranchId, bool )`

#### has_child_flag

Function `( Character, MutationBranchId ) -> bool`

#### remove_child_flag

Function `( Character, MutationBranchId )`

#### get_highest_category

Function `( Character ) -> MutationCategoryTraitId`

#### is_weak_to_water

Function `( Character ) -> bool`

#### mutation_armor

Function `( Character, BodyPartTypeIntId, DamageType ) -> double`

#### get_bionics

Function `( Character ) -> Vector( BionicDataId )`

#### has_bionic

Function `( Character, BionicDataId ) -> bool`

#### has_active_bionic

Function `( Character, BionicDataId ) -> bool`

#### has_any_bionic

Function `( Character ) -> bool`

#### has_bionics

Function `( Character ) -> bool`

#### clear_bionics

Function `( Character )`

#### get_used_bionics_slots

Function `( Character, BodyPartTypeIntId ) -> int`

#### get_total_bionics_slots

Function `( Character, BodyPartTypeIntId ) -> int`

#### get_free_bionics_slots

Function `( Character, BodyPartTypeIntId ) -> int`

#### remove_bionic

Function `( Character, BionicDataId )`

#### add_bionic

Function `( Character, BionicDataId )`

#### get_power_level

Function `( Character ) -> Energy`

#### get_max_power_level

Function `( Character ) -> Energy`

#### mod_power_level

Function `( Character, Energy )`

#### mod_max_power_level

Function `( Character, Energy )`

#### set_power_level

Function `( Character, Energy )`

#### set_max_power_level

Function `( Character, Energy )`

#### is_max_power

Function `( Character ) -> bool`

#### has_power

Function `( Character ) -> bool`

#### has_max_power

Function `( Character ) -> bool`

#### is_worn

Function `( Character, Item ) -> bool`

#### weight_carried

Function `( Character ) -> Mass`

#### volume_carried

Function `( Character ) -> Volume`

#### volume_capacity

Function `( Character ) -> Volume`

#### can_pick_volume

Function `( Character, Volume ) -> bool`

#### can_pick_weight

Function `( Character, Mass, bool ) -> bool`

#### is_armed

Function `( Character ) -> bool`

#### can_wield

Function `( Character, Item ) -> bool`

#### wield

Function `( Character, Item ) -> bool`

#### can_unwield

Function `( Character, Item ) -> bool`

#### unwield

Function `( Character ) -> bool`

#### is_wielding

Function `( Character, Item ) -> bool`

#### is_wearing

Function `( Character, Item ) -> bool`

#### is_wearing_on_bp

Function `( Character, ItypeId, BodyPartTypeIntId ) -> bool`

#### worn_with_flag

Function `( Character, JsonFlagId, BodyPartTypeIntId ) -> bool`

#### worn_with_id

Function `( Character, ItypeId, BodyPartTypeIntId ) -> bool`

#### item_worn_with_flag

Function `( Character, JsonFlagId, BodyPartTypeIntId ) -> Item`

#### item_worn_with_id

Function `( Character, ItypeId, BodyPartTypeIntId ) -> Item`

#### get_skill_level

Function `( Character, SkillId ) -> int`

#### get_all_skills

Function `( Character ) -> SkillLevelMap`

#### get_skill_level_object

Function `( Character, SkillId ) -> SkillLevel`

#### set_skill_level

Function `( Character, SkillId, int )`

#### mod_skill_level

Function `( Character, SkillId, int )`

#### rust_rate

Function `( Character ) -> int`

#### practice

Function `( Character, SkillId, int, int, bool )`

#### read_speed

Function `( Character, bool ) -> int`

#### get_time_died

Function `( Character ) -> TimePoint`

#### is_rad_immune

Function `( Character ) -> bool`

#### is_throw_immune

Function `( Character ) -> bool`

#### rest_quality

Function `( Character ) -> double`

#### healing_rate

Function `( Character, double ) -> double`

#### healing_rate_medicine

Function `( Character, double, BodyPartTypeIntId ) -> double`

#### mutation_value

Function `( Character, string ) -> double`

#### get_base_traits

Function `( Character ) -> Vector( MutationBranchId )`

#### get_mutations

Function `( Character, bool ) -> Vector( MutationBranchId )`

#### clear_skills

Function `( Character )`

#### clear_mutations

Function `( Character )`

#### crossed_threshold

Function `( Character ) -> bool`

#### add_addiction

Function `( Character, AddictionType, int )`

#### rem_addiction

Function `( Character, AddictionType )`

#### has_addiction

Function `( Character, AddictionType ) -> bool`

#### addiction_level

Function `( Character, AddictionType ) -> int`

#### is_hauling

Function `( Character ) -> bool`

#### add_item_with_id

Adds an item with the given id and amount
Function `( Character, ItypeId, int )`

#### has_item_with_id

Checks for an item with the given id
Function `( Character, ItypeId, bool ) -> bool`

#### get_item_with_id

Gets the first occurrence of an item with the given id
Function `( Character, ItypeId, bool ) -> Item`

#### has_item_with_flag

Checks for an item with the given flag
Function `( Character, JsonFlagId, bool ) -> bool`

#### all_items_with_flag

Gets all items with the given flag
Function `( Character, JsonFlagId, bool ) -> Vector( Item )`

#### all_items

Gets all items
Function `( Character, bool ) -> Vector( Item )`

#### inv_remove_item

Removes given `Item` from character's inventory. The `Item` must be in the inventory, neither wielded nor worn.
Function `( Character, Item )`

#### assign_activity

Function `( Character, ActivityTypeId, int, int, int, string )`

#### has_activity

Function `( Character, ActivityTypeId ) -> bool`

#### cancel_activity

Function `( Character )`

#### metabolic_rate

Function `( Character ) -> double`

#### base_age

Function `( Character ) -> int`

#### set_base_age

Function `( Character, int )`

#### mod_base_age

Function `( Character, int )`

#### age

Function `( Character ) -> int`

#### base_height

Function `( Character ) -> int`

#### set_base_height

Function `( Character, int )`

#### mod_base_height

Function `( Character, int )`

#### height

Function `( Character ) -> int`

#### bodyweight

Function `( Character ) -> Mass`

#### bionics_weight

Function `( Character ) -> Mass`

#### get_armor_acid

Function `( Character, BodyPartTypeIntId ) -> int`

#### get_stim

Function `( Character ) -> int`

#### set_stim

Function `( Character, int )`

#### mod_stim

Function `( Character, int )`

#### get_rad

Function `( Character ) -> int`

#### set_rad

Function `( Character, int )`

#### mod_rad

Function `( Character, int )`

#### get_stamina

Function `( Character ) -> int`

#### get_stamina_max

Function `( Character ) -> int`

#### set_stamina

Function `( Character, int )`

#### mod_stamina

Function `( Character, int )`

#### wake_up

Function `( Character )`

#### get_shout_volume

Function `( Character ) -> int`

#### shout

Function `( Character, string, bool )`

#### vomit

Function `( Character )`

#### restore_scent

Function `( Character )`

#### mod_painkiller

Function `( Character, int )`

#### set_painkiller

Function `( Character, int )`

#### get_painkiller

Function `( Character ) -> int`

#### spores

Function `( Character )`

#### blossoms

Function `( Character )`

#### rooted

Function `( Character )`

#### fall_asleep

Function `( Character )`
Function `( Character, TimeDuration )`

#### get_hostile_creatures

Function `( Character, int ) -> Vector( Creature )`

#### get_visible_creatures

Function `( Character, int ) -> Vector( Creature )`

#### wearing_something_on

Function `( Character, BodyPartTypeIntId ) -> bool`

#### is_wearing_helmet

Function `( Character ) -> bool`

#### get_morale_level

Function `( Character ) -> int`

#### add_morale

Function `( Character, MoraleTypeDataId, int, int, TimeDuration, TimeDuration, bool, ItypeRaw )`

#### has_morale

Function `( Character, MoraleTypeDataId ) -> bool`

#### get_morale

Function `( Character, MoraleTypeDataId ) -> int`

#### rem_morale

Function `( Character, MoraleTypeDataId )`

#### clear_morale

Function `( Character )`

#### has_morale_to_read

Function `( Character ) -> bool`

#### has_morale_to_craft

Function `( Character ) -> bool`

#### knows_recipe

Function `( Character, RecipeId ) -> bool`

#### learn_recipe

Function `( Character, RecipeId )`

#### suffer

Function `( Character )`

#### irradiate

Function `( Character, double, bool ) -> bool`

#### can_hear

Function `( Character, Tripoint, int ) -> bool`

#### hearing_ability

Function `( Character ) -> double`

#### get_lowest_hp

Function `( Character ) -> int`

#### drop_inv

Function `( Character, int )`

#### bodypart_exposure

Function `( Character ) -> Map( BodyPartTypeIntId, double )`

## CharacterId

### Bases

No base classes.

### Constructors

#### `CharacterId.new()`

#### `CharacterId.new( int )`

### Members

#### is_valid

Function `( CharacterId ) -> bool`

#### get_value

Function `( CharacterId ) -> int`

## Creature

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### get_name

Function `( Creature ) -> string`

#### disp_name

Function `( Creature, bool, bool ) -> string`

#### skin_name

Function `( Creature ) -> string`

#### get_grammatical_genders

Function `( Creature ) -> Vector( string )`

#### is_avatar

Function `( Creature ) -> bool`

#### is_npc

Function `( Creature ) -> bool`

#### is_monster

Function `( Creature ) -> bool`

#### as_monster

Function `( Creature ) -> Monster`

#### as_npc

Function `( Creature ) -> Npc`

#### as_character

Function `( Creature ) -> Character`

#### as_avatar

Function `( Creature ) -> Avatar`

#### dodge_roll

Function `( Creature ) -> double`

#### stability_roll

Function `( Creature ) -> double`

#### attitude_to

Function `( Creature, Creature ) -> Attitude`

#### sees

Function `( Creature, Creature ) -> bool`

#### sight_range

Function `( Creature, int ) -> int`

#### power_rating

Function `( Creature ) -> double`

#### speed_rating

Function `( Creature ) -> double`

#### ranged_target_size

Function `( Creature ) -> double`

#### knock_back_to

Function `( Creature, Tripoint )`

#### deal_damage

Function `( Creature, Creature, BodyPartTypeIntId, DamageInstance ) -> DealtDamageInstance`

#### apply_damage

Function `( Creature, Creature, BodyPartTypeIntId, int, bool )`

#### size_melee_penalty

Function `( Creature ) -> int`

#### digging

Function `( Creature ) -> bool`

#### is_on_ground

Function `( Creature ) -> bool`

#### is_underwater

Function `( Creature ) -> bool`

#### set_underwater

Function `( Creature, bool )`

#### is_warm

Function `( Creature ) -> bool`

#### in_species

Function `( Creature, SpeciesTypeId ) -> bool`

#### has_weapon

Function `( Creature ) -> bool`

#### is_hallucination

Function `( Creature ) -> bool`

#### is_dead

Function `( Creature ) -> bool`

#### is_elec_immune

Function `( Creature ) -> bool`

#### is_immune_effect

Function `( Creature, EffectTypeId ) -> bool`

#### is_immune_damage

Function `( Creature, DamageType ) -> bool`

#### get_pos_ms

Function `( Creature ) -> Tripoint`

#### set_pos_ms

Function `( Creature, Tripoint )`

#### has_effect

Function `( Creature, EffectTypeId, Opt( BodyPartTypeId ) ) -> bool`

#### has_effect_with_flag

Function `( Creature, JsonFlagId, Opt( BodyPartTypeId ) ) -> bool`

#### get_effect_dur

Function `( Creature, EffectTypeId, Opt( BodyPartTypeId ) ) -> TimeDuration`

#### get_effect_int

Function `( Creature, EffectTypeId, Opt( BodyPartTypeId ) ) -> int`

#### add_effect

Effect type, duration, bodypart and intensity
Function `( Creature, EffectTypeId, TimeDuration, Opt( BodyPartTypeId ), Opt( int ) )`

#### remove_effect

Function `( Creature, EffectTypeId, Opt( BodyPartTypeId ) ) -> bool`

#### clear_effects

Function `( Creature )`

#### set_value

Function `( Creature, string, string )`

#### remove_value

Function `( Creature, string )`

#### get_value

Function `( Creature, string ) -> string`

#### get_weight

Function `( Creature ) -> Mass`

#### has_trait

Function `( Creature, MutationBranchId ) -> bool`

#### mod_pain

Function `( Creature, int )`

#### mod_pain_noresist

Function `( Creature, int )`

#### set_pain

Function `( Creature, int )`

#### get_pain

Function `( Creature ) -> int`

#### get_perceived_pain

Function `( Creature ) -> int`

#### get_moves

Function `( Creature ) -> int`

#### mod_moves

Function `( Creature, int )`

#### set_moves

Function `( Creature, int )`

#### get_num_blocks

Function `( Creature ) -> int`

#### get_num_dodges

Function `( Creature ) -> int`

#### get_env_resist

Function `( Creature, BodyPartTypeIntId ) -> int`

#### get_armor_bash

Function `( Creature, BodyPartTypeIntId ) -> int`

#### get_armor_cut

Function `( Creature, BodyPartTypeIntId ) -> int`

#### get_armor_bullet

Function `( Creature, BodyPartTypeIntId ) -> int`

#### get_armor_bash_base

Function `( Creature, BodyPartTypeIntId ) -> int`

#### get_armor_cut_base

Function `( Creature, BodyPartTypeIntId ) -> int`

#### get_armor_bullet_base

Function `( Creature, BodyPartTypeIntId ) -> int`

#### get_armor_bash_bonus

Function `( Creature ) -> int`

#### get_armor_cut_bonus

Function `( Creature ) -> int`

#### get_armor_bullet_bonus

Function `( Creature ) -> int`

#### get_armor_type

Function `( Creature, DamageType, BodyPartTypeIntId ) -> int`

#### get_dodge

Function `( Creature ) -> double`

#### get_melee

Function `( Creature ) -> double`

#### get_hit

Function `( Creature ) -> double`

#### get_speed

Function `( Creature ) -> int`

#### get_size

Function `( Creature ) -> MonsterSize`

#### get_hp

Function `( Creature, Opt( BodyPartTypeIntId ) ) -> int`

#### get_hp_max

Function `( Creature, Opt( BodyPartTypeIntId ) ) -> int`

#### hp_percentage

Function `( Creature ) -> int`

#### has_flag

Function `( Creature, MonsterFlag ) -> bool`

#### get_part_hp_cur

Function `( Creature, BodyPartTypeIntId ) -> int`

#### get_part_hp_max

Function `( Creature, BodyPartTypeIntId ) -> int`

#### get_part_healed_total

Function `( Creature, BodyPartTypeIntId ) -> int`

#### set_part_hp_cur

Function `( Creature, BodyPartTypeIntId, int )`

#### set_part_hp_max

Function `( Creature, BodyPartTypeIntId, int )`

#### mod_part_hp_cur

Function `( Creature, BodyPartTypeIntId, int )`

#### mod_part_hp_max

Function `( Creature, BodyPartTypeIntId, int )`

#### set_all_parts_hp_cur

Function `( Creature, int )`

#### mod_all_parts_hp_cur

Function `( Creature, int )`

#### set_all_parts_hp_to_max

Function `( Creature )`

#### get_speed_base

Function `( Creature ) -> int`

#### get_speed_bonus

Function `( Creature ) -> int`

#### get_speed_mult

Function `( Creature ) -> double`

#### get_block_bonus

Function `( Creature ) -> int`

#### get_dodge_base

Function `( Creature ) -> double`

#### get_hit_base

Function `( Creature ) -> double`

#### get_dodge_bonus

Function `( Creature ) -> double`

#### get_hit_bonus

Function `( Creature ) -> double`

#### has_grab_break_tec

Function `( Creature ) -> bool`

#### get_weight_capacity

Function `( Creature ) -> int`

## DamageInstance

new(damageType, amount, armorPen, remainingArmorMultiplier, damageMultiplier)

### Bases

No base classes.

### Constructors

#### `DamageInstance.new()`

#### `DamageInstance.new( DamageType, double, double, double, double )`

### Members

#### damage_units

Variable of type `Vector( DamageUnit )`

#### mult_damage

Function `( DamageInstance, double, bool )`

#### type_damage

Function `( DamageInstance, DamageType ) -> double`

#### total_damage

Function `( DamageInstance ) -> double`

#### clear

Function `( DamageInstance )`

#### empty

Function `( DamageInstance ) -> bool`

#### add_damage

Function `( DamageInstance, DamageType, double, double, double, double )`

#### add

Function `( DamageInstance, DamageUnit )`

#### __eq

Function `( DamageInstance, DamageInstance ) -> bool`

## DamageUnit

new(damageType, amount, armorPen, remainingArmorMultiplier, damageMultiplier)

### Bases

No base classes.

### Constructors

#### `DamageUnit.new( DamageType, double, double, double, double )`

### Members

#### type

Variable of type `DamageType`

#### amount

Variable of type `double`

#### res_pen

Variable of type `double`

#### res_mult

Variable of type `double`

#### damage_multiplier

Variable of type `double`

#### __eq

Function `( DamageUnit, DamageUnit ) -> bool`

## DealtDamageInstance

Represents the final dealt damage

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### dealt_dams

Variable of type `Array( int, 14 )`

#### bp_hit

Variable of type `BodyPartTypeId`

#### type_damage

Function `( DealtDamageInstance, DamageType ) -> int`

#### total_damage

Function `( DealtDamageInstance ) -> int`

## DiseaseTypeId

### Bases

No base classes.

### Constructors

#### `DiseaseTypeId.new()`

#### `DiseaseTypeId.new( DiseaseTypeId )`

#### `DiseaseTypeId.new( string )`

### Members

#### obj

Function `( DiseaseTypeId ) -> DiseaseTypeRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( DiseaseTypeId ) -> bool`

#### is_valid

Function `( DiseaseTypeId ) -> bool`

#### str

Function `( DiseaseTypeId ) -> string`

#### NULL_ID

Function `() -> DiseaseTypeId`

#### __tostring

Function `( DiseaseTypeId ) -> string`

#### serialize

Function `( DiseaseTypeId, <cppval: 7JsonOut > )`

#### deserialize

Function `( DiseaseTypeId, <cppval: 6JsonIn > )`

## DistributionGrid

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### get_resource

Boolean argument controls recursive behavior
Function `( DistributionGrid, bool ) -> int`

#### mod_resource

Boolean argument controls recursive behavior
Function `( DistributionGrid, int, bool ) -> int`

## DistributionGridTracker

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### get_grid_at_abs_ms

Function `( DistributionGridTracker, Tripoint ) -> DistributionGrid`

## EffectTypeId

### Bases

No base classes.

### Constructors

#### `EffectTypeId.new()`

#### `EffectTypeId.new( EffectTypeId )`

#### `EffectTypeId.new( string )`

### Members

#### obj

Function `( EffectTypeId ) -> EffectTypeRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( EffectTypeId ) -> bool`

#### is_valid

Function `( EffectTypeId ) -> bool`

#### str

Function `( EffectTypeId ) -> string`

#### NULL_ID

Function `() -> EffectTypeId`

#### __tostring

Function `( EffectTypeId ) -> string`

#### serialize

Function `( EffectTypeId, <cppval: 7JsonOut > )`

#### deserialize

Function `( EffectTypeId, <cppval: 6JsonIn > )`

## Energy

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### from_joule

Function `( int ) -> Energy`

#### to_joule

Function `( Energy ) -> int`

#### from_kilojoule

Function `( int ) -> Energy`

#### to_kilojoule

Function `( Energy ) -> int`

#### __eq

Function `( Energy, Energy ) -> bool`

#### __lt

Function `( Energy, Energy ) -> bool`

#### __le

Function `( Energy, Energy ) -> bool`

## FactionId

### Bases

No base classes.

### Constructors

#### `FactionId.new()`

#### `FactionId.new( FactionId )`

#### `FactionId.new( string )`

### Members

#### obj

Function `( FactionId ) -> FactionRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( FactionId ) -> bool`

#### is_valid

Function `( FactionId ) -> bool`

#### str

Function `( FactionId ) -> string`

#### NULL_ID

Function `() -> FactionId`

#### __tostring

Function `( FactionId ) -> string`

#### serialize

Function `( FactionId, <cppval: 7JsonOut > )`

#### deserialize

Function `( FactionId, <cppval: 6JsonIn > )`

## FactionRaw

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### str_id

Function `( FactionRaw ) -> FactionId`

## FieldTypeId

### Bases

No base classes.

### Constructors

#### `FieldTypeId.new()`

#### `FieldTypeId.new( FieldTypeId )`

#### `FieldTypeId.new( FieldTypeIntId )`

#### `FieldTypeId.new( string )`

### Members

#### obj

Function `( FieldTypeId ) -> FieldTypeRaw`

#### int_id

Function `( FieldTypeId ) -> FieldTypeIntId`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( FieldTypeId ) -> bool`

#### is_valid

Function `( FieldTypeId ) -> bool`

#### str

Function `( FieldTypeId ) -> string`

#### NULL_ID

Function `() -> FieldTypeId`

#### __tostring

Function `( FieldTypeId ) -> string`

#### serialize

Function `( FieldTypeId, <cppval: 7JsonOut > )`

#### deserialize

Function `( FieldTypeId, <cppval: 6JsonIn > )`

## FieldTypeIntId

### Bases

No base classes.

### Constructors

#### `FieldTypeIntId.new()`

#### `FieldTypeIntId.new( FieldTypeIntId )`

#### `FieldTypeIntId.new( FieldTypeId )`

### Members

#### obj

Function `( FieldTypeIntId ) -> FieldTypeRaw`

#### str_id

Function `( FieldTypeIntId ) -> FieldTypeId`

#### is_valid

Function `( FieldTypeIntId ) -> bool`

#### __tostring

Function `( FieldTypeIntId ) -> string`

## FurnId

### Bases

No base classes.

### Constructors

#### `FurnId.new()`

#### `FurnId.new( FurnId )`

#### `FurnId.new( FurnIntId )`

#### `FurnId.new( string )`

### Members

#### obj

Function `( FurnId ) -> FurnRaw`

#### int_id

Function `( FurnId ) -> FurnIntId`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( FurnId ) -> bool`

#### is_valid

Function `( FurnId ) -> bool`

#### str

Function `( FurnId ) -> string`

#### NULL_ID

Function `() -> FurnId`

#### __tostring

Function `( FurnId ) -> string`

#### serialize

Function `( FurnId, <cppval: 7JsonOut > )`

#### deserialize

Function `( FurnId, <cppval: 6JsonIn > )`

## FurnIntId

### Bases

No base classes.

### Constructors

#### `FurnIntId.new()`

#### `FurnIntId.new( FurnIntId )`

#### `FurnIntId.new( FurnId )`

### Members

#### obj

Function `( FurnIntId ) -> FurnRaw`

#### str_id

Function `( FurnIntId ) -> FurnId`

#### is_valid

Function `( FurnIntId ) -> bool`

#### __tostring

Function `( FurnIntId ) -> string`

## FurnRaw

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### str_id

Function `( FurnRaw ) -> FurnId`

#### int_id

Function `( FurnRaw ) -> FurnIntId`

#### name

Function `( FurnRaw ) -> string`

#### get_flags

Function `( FurnRaw ) -> <cppval: St3setINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4lessIS5_ESaIS5_EE >`

#### has_flag

Function `( FurnRaw, string ) -> bool`

#### set_flag

Function `( FurnRaw, string )`

#### get_light_emitted

Function `( FurnRaw ) -> int`

#### set_light_emitted

Function `( FurnRaw, int )`

#### get_movecost

Function `( FurnRaw ) -> int`

#### set_movecost

Function `( FurnRaw, int )`

#### get_coverage

Function `( FurnRaw ) -> int`

#### set_coverage

Function `( FurnRaw, int )`

#### get_max_volume

Function `( FurnRaw ) -> Volume`

#### set_max_volume

Function `( FurnRaw, Volume )`

#### open

Variable of type `FurnId`

#### close

Variable of type `FurnId`

#### transforms_into

Variable of type `FurnId`

## Item

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### get_type

Function `( Item ) -> ItypeId`

#### get_mtype

Almost for a corpse.
Function `( Item ) -> MtypeId`

#### tname

Translated item name with prefixes
Function `( Item, int, bool, int ) -> string`

#### display_name

Display name with all bells and whistles like ammo and prefixes
Function `( Item, int ) -> string`

#### weight

Weight of the item. The first `bool` is whether including contents, second `bool` is whether it is `integral_weight`.
Function `( Item, Opt( bool ), Opt( bool ) ) -> Mass`

#### volume

Volume of the item. `bool` is whether it is `integral_volume`.
Function `( Item, Opt( bool ) ) -> Volume`

#### price

Cents of the item. `bool` is whether it is a post-cataclysm value.
Function `( Item, bool ) -> double`

#### has_var

Check for variable of any type
Function `( Item, string ) -> bool`

#### erase_var

Erase variable
Function `( Item, string )`

#### clear_vars

Erase all variables
Function `( Item )`

#### is_null

Function `( Item ) -> bool`

#### is_unarmed_weapon

Function `( Item ) -> bool`

#### is_sided

Function `( Item ) -> bool`

#### is_power_armor

Function `( Item ) -> bool`

#### is_money

Function `( Item ) -> bool`

#### is_gun

Function `( Item ) -> bool`

#### is_firearm

Function `( Item ) -> bool`

#### is_silent

Function `( Item ) -> bool`

#### is_gunmod

Function `( Item ) -> bool`

#### is_bionic

Function `( Item ) -> bool`

#### is_ammo_belt

Function `( Item ) -> bool`

#### is_bandolier

Function `( Item ) -> bool`

#### is_holster

Function `( Item ) -> bool`

#### is_ammo

Function `( Item ) -> bool`

#### is_comestible

Function `( Item ) -> bool`

#### is_food

Function `( Item ) -> bool`

#### is_medication

Function `( Item ) -> bool`

#### is_brewable

Function `( Item ) -> bool`

#### is_food_container

Function `( Item ) -> bool`

#### is_med_container

Function `( Item ) -> bool`

#### is_corpse

Function `( Item ) -> bool`

#### is_ammo_container

Function `( Item ) -> bool`

#### is_armor

Function `( Item ) -> bool`

#### is_book

Function `( Item ) -> bool`

#### is_map

Function `( Item ) -> bool`

#### is_container

Function `( Item ) -> bool`

#### is_watertight_container

Function `( Item ) -> bool`

#### is_non_resealable_container

Function `( Item ) -> bool`

#### is_bucket

Function `( Item ) -> bool`

#### is_bucket_nonempty

Function `( Item ) -> bool`

#### is_engine

Function `( Item ) -> bool`

#### is_wheel

Function `( Item ) -> bool`

#### is_fuel

Function `( Item ) -> bool`

#### is_toolmod

Function `( Item ) -> bool`

#### is_faulty

Function `( Item ) -> bool`

#### is_irremovable

Function `( Item ) -> bool`

#### is_container_empty

Function `( Item ) -> bool`

#### is_salvageable

Function `( Item, bool ) -> bool`

#### is_craft

Function `( Item ) -> bool`

#### is_emissive

Function `( Item ) -> bool`

#### is_deployable

Function `( Item ) -> bool`

#### is_tool

Function `( Item ) -> bool`

#### is_transformable

Function `( Item ) -> bool`

#### is_artifact

Function `( Item ) -> bool`

#### is_relic

Function `( Item ) -> bool`

#### is_seed

Function `( Item ) -> bool`

#### is_dangerous

Function `( Item ) -> bool`

#### is_tainted

Function `( Item ) -> bool`

#### is_soft

Function `( Item ) -> bool`

#### is_reloadable

Function `( Item ) -> bool`

#### is_filthy

DEPRECATED: Items are no longer filthy
Function `() -> bool`

#### is_active

Function `( Item ) -> bool`

#### is_upgrade

Function `( Item ) -> bool`

#### activate

Function `( Item )`

#### deactivate

Function `( Item )`

#### is_melee

Is this item an effective melee weapon for the given damage type?
Function `( Item, DamageType ) -> bool`

#### is_magazine

Is this a magazine? (batteries are magazines)
Function `( Item ) -> bool`

#### is_battery

DEPRECATED: Is this a battery? (spoiler: it isn't)
Function `( Item ) -> bool`

#### conductive

Function `( Item ) -> bool`

#### is_stackable

Function `( Item ) -> bool`

#### charges

Variable of type `int`

#### energy_remaining

Function `( Item ) -> Energy`

#### has_infinite_charges

Function `( Item ) -> bool`

#### mod_charges

Function `( Item, int )`

#### made_of

Function `( Item ) -> <cppval: St6vectorI9string_idI13material_typeESaIS2_EE >`

#### is_made_of

Function `( Item, MaterialTypeId ) -> bool`

#### get_kcal

Function `( Item ) -> int`

#### get_quench

Function `( Item ) -> int`

#### get_comestible_fun

Function `( Item ) -> int`

#### get_rot

Gets the TimeDuration until this item rots
Function `( Item ) -> TimeDuration`

#### get_category_id

Gets the category id this item is in
Function `( Item ) -> string`

#### get_owner

Gets the faction id that owns this item
Function `( Item ) -> FactionId`

#### set_owner

Sets the ownership of this item to a faction
Function `( Item, FactionId )`

#### set_owner

Sets the ownership of this item to a character
Function `( Item, Character )`

#### get_owner_name

Function `( Item ) -> string`

#### is_owned_by

Checks if this item owned by a character
Function `( Item, Character, bool ) -> bool`

#### has_technique

Checks if this item has the technique as an addition. Doesn't check original techniques.
Function `( Item, MartialArtsTechniqueId ) -> bool`

#### get_techniques

Gets all techniques. Including original techniques.
Function `( Item ) -> Set( MartialArtsTechniqueId )`

#### add_technique

Adds the technique. It isn't treated original, but additional.
Function `( Item, MartialArtsTechniqueId )`

#### remove_technique

Removes the additional technique. Doesn't affect originial techniques.
Function `( Item, MartialArtsTechniqueId )`

#### can_contain

Checks if this item can contain another
Function `( Item, Item ) -> bool`

#### remaining_capacity_for_id

Gets the remaining space available for a type of liquid
Function `( Item, ItypeId, bool ) -> int`

#### total_capacity

Gets maximum volume this item can hold (liquids, ammo, etc)
Function `( Item ) -> Volume`

#### current_magazine

Gets the current magazine
Function `( Item ) -> Item`

#### ammo_capacity

Gets the maximum capacity of a magazine
Function `( Item, bool ) -> int`

#### ammo_remaining

Get remaining ammo, works with batteries & stuff too
Function `( Item ) -> int`

#### ammo_data

Function `( Item ) -> ItypeRaw`

#### ammo_required

Function `( Item ) -> int`

#### ammo_current

Function `( Item ) -> ItypeId`

#### ammo_consume

Function `( Item, int, Tripoint ) -> int`

#### ammo_set

Function `( Item, ItypeId, int )`

#### ammo_unset

Function `( Item )`

#### get_reload_time

Function `( Item ) -> int`

#### add_item_with_id

Adds an item(s) to contents
Function `( Item, ItypeId, int )`

#### has_item_with_id

Checks item contents for a given item id
Function `( Item, ItypeId ) -> bool`

#### covers

Checks if the item covers a bodypart
Function `( Item, BodyPartTypeIntId ) -> bool`

#### set_flag

Function `( Item, JsonFlagId )`

#### unset_flag

Function `( Item, JsonFlagId )`

#### has_flag

Function `( Item, JsonFlagId ) -> bool`

#### has_own_flag

Function `( Item, JsonFlagId ) -> bool`

#### set_flag_recursive

Function `( Item, JsonFlagId )`

#### unset_flags

Function `( Item )`

#### convert

Converts the item as given `ItypeId`.
Function `( Item, ItypeId )`

#### get_var_str

Get variable as string
Function `( Item, string, string ) -> string`

#### get_var_num

Get variable as float number
Function `( Item, string, double ) -> double`

#### get_var_tri

Get variable as tripoint
Function `( Item, string, Tripoint ) -> Tripoint`

#### set_var_str

Function `( Item, string, string )`

#### set_var_num

Function `( Item, string, double )`

#### set_var_tri

Function `( Item, string, Tripoint )`

#### attack_cost

Function `( Item ) -> int`

#### stamina_cost

Function `( Item ) -> int`

## ItemStack

Iterate over this using pairs()

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### __pairs

Function `( ItemStack ) -> ( <cppval: FSt5tupleIJN3sol12basic_objectINS0_15basic_referenceILb0EEEEES4_EENS0_4userIR23item_stack_lua_it_stateEENS0_10this_stateEE >, <cppval: N3sol4userI23item_stack_lua_it_stateEE >, nil )`

## ItypeId

### Bases

No base classes.

### Constructors

#### `ItypeId.new()`

#### `ItypeId.new( ItypeId )`

#### `ItypeId.new( string )`

### Members

#### obj

Function `( ItypeId ) -> ItypeRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( ItypeId ) -> bool`

#### is_valid

Function `( ItypeId ) -> bool`

#### str

Function `( ItypeId ) -> string`

#### NULL_ID

Function `() -> ItypeId`

#### __tostring

Function `( ItypeId ) -> string`

#### serialize

Function `( ItypeId, <cppval: 7JsonOut > )`

#### deserialize

Function `( ItypeId, <cppval: 6JsonIn > )`

## JsonFlagId

### Bases

No base classes.

### Constructors

#### `JsonFlagId.new()`

#### `JsonFlagId.new( JsonFlagId )`

#### `JsonFlagId.new( string )`

### Members

#### obj

Function `( JsonFlagId ) -> JsonFlagRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( JsonFlagId ) -> bool`

#### is_valid

Function `( JsonFlagId ) -> bool`

#### str

Function `( JsonFlagId ) -> string`

#### NULL_ID

Function `() -> JsonFlagId`

#### __tostring

Function `( JsonFlagId ) -> string`

#### serialize

Function `( JsonFlagId, <cppval: 7JsonOut > )`

#### deserialize

Function `( JsonFlagId, <cppval: 6JsonIn > )`

## JsonTraitFlagId

### Bases

No base classes.

### Constructors

#### `JsonTraitFlagId.new()`

#### `JsonTraitFlagId.new( JsonTraitFlagId )`

#### `JsonTraitFlagId.new( string )`

### Members

#### obj

Function `( JsonTraitFlagId ) -> JsonTraitFlagRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( JsonTraitFlagId ) -> bool`

#### is_valid

Function `( JsonTraitFlagId ) -> bool`

#### str

Function `( JsonTraitFlagId ) -> string`

#### NULL_ID

Function `() -> JsonTraitFlagId`

#### __tostring

Function `( JsonTraitFlagId ) -> string`

#### serialize

Function `( JsonTraitFlagId, <cppval: 7JsonOut > )`

#### deserialize

Function `( JsonTraitFlagId, <cppval: 6JsonIn > )`

## Map

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### get_abs_ms

Convert local ms -> absolute ms
Function `( Map, Tripoint ) -> Tripoint`

#### get_local_ms

Convert absolute ms -> local ms
Function `( Map, Tripoint ) -> Tripoint`

#### get_map_size_in_submaps

Function `( Map ) -> int`

#### get_map_size

In map squares
Function `( Map ) -> int`

#### create_item_at

Creates a new item(s) at a position on the map.
Function `( Map, Tripoint, ItypeId, int )`

#### create_corpse_at

Creates a new corpse at a position on the map. You can skip `Opt` ones by omitting them or passing `nil`. `MtypeId` specifies which monster's body it is, `TimePoint` indicates when it died, `string` gives it a custom name, and `int` determines the revival time if the monster has the `REVIVES` flag.
Function `( Map, Tripoint, Opt( MtypeId ), Opt( TimePoint ), Opt( string ), Opt( int ) )`

#### has_items_at

Function `( Map, Tripoint ) -> bool`

#### get_items_at

Function `( Map, Tripoint ) -> <cppval: St10unique_ptrI9map_stackSt14default_deleteIS0_EE >`

#### remove_item_at

Function `( Map, Tripoint, Item )`

#### clear_items_at

Function `( Map, Tripoint )`

#### get_ter_at

Function `( Map, Tripoint ) -> TerIntId`

#### set_ter_at

Function `( Map, Tripoint, TerIntId ) -> bool`

#### get_furn_at

Function `( Map, Tripoint ) -> FurnIntId`

#### set_furn_at

Function `( Map, Tripoint, FurnIntId )`

#### has_field_at

Function `( Map, Tripoint, FieldTypeIntId ) -> bool`

#### get_field_int_at

Function `( Map, Tripoint, FieldTypeIntId ) -> int`

#### get_field_age_at

Function `( Map, Tripoint, FieldTypeIntId ) -> TimeDuration`

#### mod_field_int_at

Function `( Map, Tripoint, FieldTypeIntId, int ) -> int`

#### mod_field_age_at

Function `( Map, Tripoint, FieldTypeIntId, TimeDuration ) -> TimeDuration`

#### set_field_int_at

Function `( Map, Tripoint, FieldTypeIntId, int, bool ) -> int`

#### set_field_age_at

Function `( Map, Tripoint, FieldTypeIntId, TimeDuration, bool ) -> TimeDuration`

#### add_field_at

Function `( Map, Tripoint, FieldTypeIntId, int, TimeDuration ) -> bool`

#### remove_field_at

Function `( Map, Tripoint, FieldTypeIntId )`

#### get_trap_at

Function `( Map, Tripoint ) -> TrapIntId`

#### set_trap_at

Set a trap at a position on the map. It can also replace existing trap, even with `trap_null`.
Function `( Map, Tripoint, TrapIntId )`

#### disarm_trap_at

Disarms a trap using your skills and stats, with consequences depending on success or failure.
Function `( Map, Tripoint )`

#### remove_trap_at

Simpler version of `set_trap_at` with `trap_null`.
Function `( Map, Tripoint )`

## MapStack

### Bases

- `ItemStack`

### Constructors

No constructors.

### Members

#### as_item_stack

Function `( MapStack ) -> ItemStack`

## MartialArtsBuffId

### Bases

No base classes.

### Constructors

#### `MartialArtsBuffId.new()`

#### `MartialArtsBuffId.new( MartialArtsBuffId )`

#### `MartialArtsBuffId.new( string )`

### Members

#### obj

Function `( MartialArtsBuffId ) -> MartialArtsBuffRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( MartialArtsBuffId ) -> bool`

#### is_valid

Function `( MartialArtsBuffId ) -> bool`

#### str

Function `( MartialArtsBuffId ) -> string`

#### NULL_ID

Function `() -> MartialArtsBuffId`

#### __tostring

Function `( MartialArtsBuffId ) -> string`

#### serialize

Function `( MartialArtsBuffId, <cppval: 7JsonOut > )`

#### deserialize

Function `( MartialArtsBuffId, <cppval: 6JsonIn > )`

## MartialArtsTechniqueId

### Bases

No base classes.

### Constructors

#### `MartialArtsTechniqueId.new()`

#### `MartialArtsTechniqueId.new( MartialArtsTechniqueId )`

#### `MartialArtsTechniqueId.new( string )`

### Members

#### obj

Function `( MartialArtsTechniqueId ) -> MartialArtsTechniqueRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( MartialArtsTechniqueId ) -> bool`

#### is_valid

Function `( MartialArtsTechniqueId ) -> bool`

#### str

Function `( MartialArtsTechniqueId ) -> string`

#### NULL_ID

Function `() -> MartialArtsTechniqueId`

#### __tostring

Function `( MartialArtsTechniqueId ) -> string`

#### serialize

Function `( MartialArtsTechniqueId, <cppval: 7JsonOut > )`

#### deserialize

Function `( MartialArtsTechniqueId, <cppval: 6JsonIn > )`

## MartialArtsTechniqueRaw

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### name

Function `()`

#### get_description

Function `( MartialArtsTechniqueRaw ) -> string`

#### avatar_message

Function `()`

#### npc_message

Function `()`

#### defensive

Function `()`

#### side_switch

Function `()`

#### down_dur

Function `()`

#### stun_dur

Function `()`

#### knockback_dist

Function `()`

#### knockback_spread

Function `()`

#### powerful_knockback

Function `()`

#### crit_tec

Function `()`

#### crit_ok

Function `()`

#### knockback_follow

Function `()`

#### disarms

Function `()`

#### take_weapon

Function `()`

#### dodge_counter

Function `()`

#### block_counter

Function `()`

#### miss_recovery

Function `()`

#### grab_break

Function `()`

## Mass

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### from_milligram

Function `( int ) -> Mass`

#### to_milligram

Function `( Mass ) -> int`

#### from_gram

Function `( int ) -> Mass`

#### to_gram

Function `( Mass ) -> int`

#### from_kilogram

Function `( int ) -> Mass`

#### to_kilogram

Function `( Mass ) -> int`

#### from_newton

Function `( int ) -> Mass`

#### to_newton

Function `( Mass ) -> int`

#### __eq

Function `( Mass, Mass ) -> bool`

#### __lt

Function `( Mass, Mass ) -> bool`

#### __le

Function `( Mass, Mass ) -> bool`

## MaterialTypeId

### Bases

No base classes.

### Constructors

#### `MaterialTypeId.new()`

#### `MaterialTypeId.new( MaterialTypeId )`

#### `MaterialTypeId.new( string )`

### Members

#### obj

Function `( MaterialTypeId ) -> MaterialTypeRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( MaterialTypeId ) -> bool`

#### is_valid

Function `( MaterialTypeId ) -> bool`

#### str

Function `( MaterialTypeId ) -> string`

#### NULL_ID

Function `() -> MaterialTypeId`

#### __tostring

Function `( MaterialTypeId ) -> string`

#### serialize

Function `( MaterialTypeId, <cppval: 7JsonOut > )`

#### deserialize

Function `( MaterialTypeId, <cppval: 6JsonIn > )`

## MaterialTypeRaw

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### str_id

Function `( MaterialTypeRaw ) -> MaterialTypeId`

#### name

Function `( MaterialTypeRaw ) -> string`

## Mission

### Bases

No base classes.

### Constructors

#### `Mission.new()`

### Members

#### name

Returns the mission's name as a string.
Function `( Mission ) -> string`

#### mission_id

Returns the mission type ID of this mission.
Function `( Mission ) -> MissionTypeIdRaw`

#### has_deadline

Returns true if the mission has a deadline.
Function `( Mission ) -> bool`

#### get_deadline

Returns the mission's deadline as a time_point.
Function `( Mission ) -> TimePoint`

#### get_description

Returns the mission description.
Function `( Mission ) -> string`

#### has_target

Returns true if the mission has a target.
Function `( Mission ) -> bool`

#### get_target_point

Returns the target of the mission (pointer to tripoint_abs_omt).
Function `( Mission ) -> Tripoint`

#### get_type

Returns the mission type of the target (pointer to mission_type).
Function `( Mission ) -> MissionType`

#### has_follow_up

Returns true if the mission has a follow-up mission.
Function `( Mission ) -> bool`

#### get_follow_up

Returns the follow-up mission type ID.
Function `( Mission ) -> MissionTypeIdRaw`

#### get_value

Returns the mission's value as an integer.
Function `( Mission ) -> int`

#### get_id

Returns the mission's unique ID.
Function `( Mission ) -> int`

#### get_item_id

Returns the item ID associated with the mission.
Function `( Mission ) -> ItypeId`

#### get_npc_id

Returns the NPC character ID associated with the mission.
Function `( Mission ) -> CharacterId`

#### get_likely_rewards

Returns the likely rewards of the mission (vector of (int chance, itype_id) pairs).
Function `( Mission ) -> <cppval: St6vectorISt4pairIi9string_idI5itypeEESaIS4_EE >`

#### has_generic_rewards

Returns true if the mission has generic rewards.
Function `( Mission ) -> bool`

#### is_assigned

Returns true if the mission is currently assigned.
Function `( Mission ) -> bool`

#### fail

Fails the mission.
Function `( Mission )`

#### wrap_up

Wraps up the mission successfully.
Function `( Mission )`

#### has_failed

Returns true if the mission has failed.
Function `( Mission ) -> bool`

#### in_progress

Returns true if the mission is currently in progress.
Function `( Mission ) -> bool`

#### step_complete

Marks a mission step as complete, taking an integer step index.
Function `( Mission, int )`

#### is_complete

Returns true if the mission goal has been completed (optionally checked against given NPC ID).
Function `( Mission, Opt( CharacterId ) ) -> bool`

#### assign

Assigns this mission to the given avatar.
Function `( Mission, Avatar )`

#### reserve_new

Reserves a new mission of the given type for the specified NPC. Returns the new mission.
Function `( MissionTypeIdRaw, CharacterId ) -> Mission`

#### reserve_random

Reserves a random mission at the specified origin and position for the given NPC. Returns the new mission.
Function `( MissionOrigin, Tripoint, CharacterId ) -> Mission`

#### serialize

Function `( Mission, <cppval: 7JsonOut > )`

#### deserialize

Function `( Mission, <cppval: 6JsonIn > )`

## MissionType

### Bases

No base classes.

### Constructors

#### `MissionType.new()`

### Members

#### description

Returns the mission's description as a string.
Variable of type `<cppval: 11translation >`

#### goal

Returns the mission's goal text.
Variable of type `MissionGoal`

#### difficulty

Returns the mission's difficulty as an integer.
Variable of type `int`

#### value

Returns the mission's reward value as an integer.
Variable of type `int`

#### deadline_low

Returns the minimum allowed deadline for the mission.
Variable of type `TimeDuration`

#### deadline_high

Returns the maximum allowed deadline for the mission.
Variable of type `TimeDuration`

#### urgent

Returns true if the mission is marked as urgent.
Variable of type `bool`

#### has_generic_rewards

Returns true if the mission has generic rewards.
Variable of type `bool`

#### likely_rewards

Returns a vector of likely rewards (chance, itype_id pairs).
Variable of type `Vector( <cppval: St4pairIi9string_idI5itypeEE > )`

#### origins

Returns a list of origins from which this mission can be generated.
Variable of type `Vector( MissionOrigin )`

#### item_id

Returns the ID of the mission's main item target, if applicable.
Variable of type `ItypeId`

#### remove_container

Returns true if the mission requires removing a container.
Variable of type `bool`

#### empty_container

Returns true if the mission requires the container to be empty.
Variable of type `ItypeId`

#### item_count

Returns the count of items involved in the mission.
Variable of type `int`

#### target_npc_id

Returns the ID of the target NPC for the mission, if any.
Variable of type `CharacterId`

#### monster_type

Returns the monster type associated with the mission, if any.
Variable of type `MtypeId`

#### monster_kill_goal

Returns the number of monsters required to kill for this mission.
Variable of type `int`

#### follow_up

Returns any follow-up mission type ID.
Variable of type `MissionTypeIdRaw`

#### dialogue

Returns any associated dialogue for the mission.
Variable of type `Map( string, <cppval: 11translation > )`

#### get_all

Returns all available missions.
Function `() -> <cppval: St6vectorI12mission_typeSaIS0_EE >`

#### get_random_mission_id

Returns a random mission type ID at the specified origin and overmap tile position.
Function `( MissionOrigin, Tripoint ) -> MissionTypeIdRaw`

#### tname

Function `( MissionType ) -> string`

## MissionTypeIdRaw

### Bases

No base classes.

### Constructors

#### `MissionTypeIdRaw.new( string )`

### Members

No members.

## Monster

### Bases

- `Creature`

### Constructors

No constructors.

### Members

#### friendly

Variable of type `int`

#### anger

Variable of type `int`

#### morale

Variable of type `int`

#### faction

Variable of type `MonsterFactionIntId`

#### death_drops

Variable of type `bool`

#### unique_name

Variable of type `string`

#### get_type

Function `( Monster ) -> MtypeId`

#### can_upgrade

Function `( Monster ) -> bool`

#### hasten_upgrade

Function `( Monster )`

#### get_upgrade_time

Function `( Monster ) -> int`

#### try_upgrade

Function `( Monster, bool )`

#### try_reproduce

Function `( Monster )`

#### refill_udders

Function `( Monster )`

#### spawn

Function `( Monster, Tripoint )`

#### name

Function `( Monster, int ) -> string`

#### name_with_armor

Function `( Monster ) -> string`

#### can_see

Function `( Monster ) -> bool`

#### can_hear

Function `( Monster ) -> bool`

#### can_submerge

Function `( Monster ) -> bool`

#### can_drown

Function `( Monster ) -> bool`

#### can_climb

Function `( Monster ) -> bool`

#### can_dig

Function `( Monster ) -> bool`

#### digs

Function `( Monster ) -> bool`

#### flies

Function `( Monster ) -> bool`

#### climbs

Function `( Monster ) -> bool`

#### swims

Function `( Monster ) -> bool`

#### move_target

Function `( Monster ) -> Tripoint`

#### is_wandering

Function `( Monster ) -> bool`

#### wander_to

Function `( Monster, Tripoint, int )`

#### move_to

Function `( Monster, Tripoint, bool, bool, double ) -> bool`

#### attitude

Function `( Monster, Character ) -> MonsterAttitude`

#### heal

Function `( Monster, int, bool ) -> int`

#### set_hp

Function `( Monster, int )`

#### make_fungus

Function `( Monster ) -> bool`

#### make_friendly

Function `( Monster )`

#### make_ally

Function `( Monster, Monster )`

## MonsterFactionId

### Bases

No base classes.

### Constructors

#### `MonsterFactionId.new()`

#### `MonsterFactionId.new( MonsterFactionId )`

#### `MonsterFactionId.new( MonsterFactionIntId )`

#### `MonsterFactionId.new( string )`

### Members

#### obj

Function `( MonsterFactionId ) -> MonsterFactionRaw`

#### int_id

Function `( MonsterFactionId ) -> MonsterFactionIntId`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( MonsterFactionId ) -> bool`

#### is_valid

Function `( MonsterFactionId ) -> bool`

#### str

Function `( MonsterFactionId ) -> string`

#### NULL_ID

Function `() -> MonsterFactionId`

#### __tostring

Function `( MonsterFactionId ) -> string`

#### serialize

Function `( MonsterFactionId, <cppval: 7JsonOut > )`

#### deserialize

Function `( MonsterFactionId, <cppval: 6JsonIn > )`

## MonsterFactionIntId

### Bases

No base classes.

### Constructors

#### `MonsterFactionIntId.new()`

#### `MonsterFactionIntId.new( MonsterFactionIntId )`

#### `MonsterFactionIntId.new( MonsterFactionId )`

### Members

#### obj

Function `( MonsterFactionIntId ) -> MonsterFactionRaw`

#### str_id

Function `( MonsterFactionIntId ) -> MonsterFactionId`

#### is_valid

Function `( MonsterFactionIntId ) -> bool`

#### __tostring

Function `( MonsterFactionIntId ) -> string`

## MoraleTypeDataId

### Bases

No base classes.

### Constructors

#### `MoraleTypeDataId.new()`

#### `MoraleTypeDataId.new( MoraleTypeDataId )`

#### `MoraleTypeDataId.new( string )`

### Members

#### obj

Function `( MoraleTypeDataId ) -> MoraleTypeDataRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( MoraleTypeDataId ) -> bool`

#### is_valid

Function `( MoraleTypeDataId ) -> bool`

#### str

Function `( MoraleTypeDataId ) -> string`

#### NULL_ID

Function `() -> MoraleTypeDataId`

#### __tostring

Function `( MoraleTypeDataId ) -> string`

#### serialize

Function `( MoraleTypeDataId, <cppval: 7JsonOut > )`

#### deserialize

Function `( MoraleTypeDataId, <cppval: 6JsonIn > )`

## MtypeId

### Bases

No base classes.

### Constructors

#### `MtypeId.new()`

#### `MtypeId.new( MtypeId )`

#### `MtypeId.new( string )`

### Members

#### obj

Function `( MtypeId ) -> MtypeRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( MtypeId ) -> bool`

#### is_valid

Function `( MtypeId ) -> bool`

#### str

Function `( MtypeId ) -> string`

#### NULL_ID

Function `() -> MtypeId`

#### __tostring

Function `( MtypeId ) -> string`

#### serialize

Function `( MtypeId, <cppval: 7JsonOut > )`

#### deserialize

Function `( MtypeId, <cppval: 6JsonIn > )`

## MutationBranchId

### Bases

No base classes.

### Constructors

#### `MutationBranchId.new()`

#### `MutationBranchId.new( MutationBranchId )`

#### `MutationBranchId.new( string )`

### Members

#### obj

Function `( MutationBranchId ) -> MutationBranchRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( MutationBranchId ) -> bool`

#### is_valid

Function `( MutationBranchId ) -> bool`

#### str

Function `( MutationBranchId ) -> string`

#### NULL_ID

Function `() -> MutationBranchId`

#### __tostring

Function `( MutationBranchId ) -> string`

#### serialize

Function `( MutationBranchId, <cppval: 7JsonOut > )`

#### deserialize

Function `( MutationBranchId, <cppval: 6JsonIn > )`

## MutationBranchRaw

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### id

Variable of type `MutationBranchId`

#### valid

Whether this mutation is available through generic mutagen.
Variable of type `bool`

#### purifiable

Whether this mutation is possible to remove through Purifier. False for 'special' mutations.
Variable of type `bool`

#### threshold

Whether this is a Threshold mutation, and thus especially difficult to mutate. One per character.
Variable of type `bool`

#### profession

Whether this trait is ONLY gained through professional training/experience (and/or quests).
Variable of type `bool`

#### debug

Whether or not this mutation is limited to debug use.
Variable of type `bool`

#### player_display

Whether or not this mutation shows up in the status (`@`) menu.
Variable of type `bool`

#### mixed_effect

Whether this mutation has positive /and/ negative effects.
Variable of type `bool`

#### starting_trait

Whether this trait can normally be taken during character generation.
Variable of type `bool`

#### activated

Whether this mutation can be activated at will.
Variable of type `bool`

#### starts_active

Whether a mutation activates when granted.
Variable of type `bool`

#### allow_soft_gear

Mutation allows soft gear to be worn over otherwise-restricted parts.
Variable of type `bool`

#### fatigue

Mutation causes fatigue when used.
Variable of type `bool`

#### hunger

Mutation deducts calories when used.
Variable of type `bool`

#### thirst

Mutation dehydrates when used.
Variable of type `bool`

#### points

Point cost in character creation(?).
Variable of type `int`

#### visibility

How visible the mutation is to others.
Variable of type `int`

#### ugliness

How physically unappealing the mutation is. Can be negative.
Variable of type `int`

#### cost

Variable of type `int`

#### cooldown

Costs are incurred every 'cooldown' turns.
Variable of type `int`

#### bodytemp_min_btu

Variable of type `int`

#### bodytemp_max_btu

Variable of type `int`

#### bodytemp_sleep_btu

Variable of type `int`

#### pain_recovery

Pain recovery per turn from mutation.
Variable of type `double`

#### healing_awake

Healing per turn from mutation.
Variable of type `double`

#### healing_resting

Healing per turn from mutation, while asleep.
Variable of type `double`

#### mending_modifier

Multiplier applied to broken limb regeneration. Normally 0.25; clamped to 0.25..1.0.
Variable of type `double`

#### hp_modifier

Bonus HP multiplier. 1.0 doubles HP; -0.5 halves it.
Variable of type `double`

#### packmule_modifier

Packmule multiplier. 2.0 doubles backpack/container volume; 0.5 halves it.
Variable of type `double`

#### crafting_speed_modifier

Crafting speed multiplier. 2.0 doubles crafting speed; 0.5 halves it.
Variable of type `double`

#### construction_speed_modifier

Construction speed multiplier. 2.0 doubles construction speed; 0.5 halves it.
Variable of type `double`

#### hp_modifier_secondary

Secondary HP multiplier; stacks with the other one. 1.0 doubles HP; -0.5 halves it.
Variable of type `double`

#### hp_adjustment

Flat adjustment to HP.
Variable of type `double`

#### str_modifier

Adjustment to Strength that doesn't affect HP.
Variable of type `double`

#### dodge_modifier

Variable of type `double`

#### speed_modifier

Variable of type `double`

#### movecost_modifier

Variable of type `double`

#### movecost_flatground_modifier

Variable of type `double`

#### movecost_obstacle_modifier

Variable of type `double`

#### attackcost_modifier

Variable of type `double`

#### falling_damage_multiplier

Variable of type `double`

#### max_stamina_modifier

Variable of type `double`

#### weight_capacity_modifier

Variable of type `double`

#### hearing_modifier

Variable of type `double`

#### movecost_swim_modifier

Variable of type `double`

#### noise_modifier

Variable of type `double`

#### scent_modifier

Variable of type `double`

#### bleed_resist

Variable of type `double`

#### healthy_rate

How quickly health (not HP) trends toward healthy_mod.
Variable of type `double`

#### stealth_modifier

Variable of type `double`

#### night_vision_range

Variable of type `double`

#### temperature_speed_modifier

Variable of type `double`

#### metabolism_modifier

Variable of type `double`

#### thirst_modifier

Variable of type `double`

#### fatigue_modifier

Variable of type `double`

#### fatigue_regen_modifier

Variable of type `double`

#### stamina_regen_modifier

Variable of type `double`

#### overmap_sight

Variable of type `double`

#### overmap_multiplier

Variable of type `double`

#### reading_speed_multiplier

Variable of type `double`

#### skill_rust_multiplier

Variable of type `double`

#### name

Function `( MutationBranchRaw ) -> string`

#### desc

Function `( MutationBranchRaw ) -> string`

#### get_all

Returns a (long) list of every mutation in the game.
Function `() -> <cppval: St6vectorI15mutation_branchSaIS0_EE >`

#### __tostring

Function `( MutationBranchRaw ) -> string`

#### prerequisites

Lists the primary mutation(s) needed to gain this mutation.
Function `( MutationBranchRaw ) -> Vector( MutationBranchId )`

#### other_prerequisites

Lists the secondary mutation(s) needed to gain this mutation.
Function `( MutationBranchRaw ) -> Vector( MutationBranchId )`

#### thresh_requirements

Lists the threshold mutation(s) required to gain this mutation.
Function `( MutationBranchRaw ) -> Vector( MutationBranchId )`

#### mutation_types

Lists the type(s) of this mutation. Mutations of a given type are mutually exclusive.
Function `( MutationBranchRaw ) -> Set( string )`

#### conflicts_with

Lists conflicting mutations.
Function `( MutationBranchRaw ) -> Vector( MutationBranchId )`

#### replaced_by

Lists mutations that replace (e.g. evolve from) this one.
Function `( MutationBranchRaw ) -> Vector( MutationBranchId )`

#### addition_mutations

Function `( MutationBranchRaw ) -> Vector( MutationBranchId )`

#### categories

Lists the categories this mutation belongs to.
Function `( MutationBranchRaw ) -> Vector( MutationCategoryTraitId )`

## MutationCategoryTraitId

### Bases

No base classes.

### Constructors

#### `MutationCategoryTraitId.new()`

#### `MutationCategoryTraitId.new( MutationCategoryTraitId )`

#### `MutationCategoryTraitId.new( string )`

### Members

#### obj

Function `( MutationCategoryTraitId ) -> MutationCategoryTraitRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( MutationCategoryTraitId ) -> bool`

#### is_valid

Function `( MutationCategoryTraitId ) -> bool`

#### str

Function `( MutationCategoryTraitId ) -> string`

#### NULL_ID

Function `() -> MutationCategoryTraitId`

#### __tostring

Function `( MutationCategoryTraitId ) -> string`

#### serialize

Function `( MutationCategoryTraitId, <cppval: 7JsonOut > )`

#### deserialize

Function `( MutationCategoryTraitId, <cppval: 6JsonIn > )`

## Npc

### Bases

- `Player`
- `Character`
- `Creature`

### Constructors

No constructors.

### Members

#### current_activity_id

Variable of type `ActivityTypeId`

#### personality

Variable of type `NpcPersonality`

#### op_of_u

Variable of type `NpcOpinion`

#### patience

Variable of type `int`

#### marked_for_death

Variable of type `bool`

#### hit_by_player

Variable of type `bool`

#### needs

Variable of type `Vector( NpcNeed )`

#### set_faction_id

Function `( Npc, FactionId )`

#### turned_hostile

Function `( Npc ) -> bool`

#### hostile_anger_level

Function `( Npc ) -> int`

#### make_angry

Function `( Npc )`

#### is_enemy

Function `( Npc ) -> bool`

#### is_following

Function `( Npc ) -> bool`

#### is_obeying

Function `( Npc, Character ) -> bool`

#### is_friendly

Function `( Npc, Character ) -> bool`

#### is_leader

Function `( Npc ) -> bool`

#### is_walking_with

Function `( Npc ) -> bool`

#### is_ally

Function `( Npc, Character ) -> bool`

#### is_player_ally

Function `( Npc ) -> bool`

#### is_stationary

Function `( Npc, bool ) -> bool`

#### is_guarding

Function `( Npc ) -> bool`

#### is_patrolling

Function `( Npc ) -> bool`

#### has_player_activity

Function `( Npc ) -> bool`

#### is_travelling

Function `( Npc ) -> bool`

#### is_minion

Function `( Npc ) -> bool`

#### guaranteed_hostile

Function `( Npc ) -> bool`

#### mutiny

Function `( Npc )`

#### get_monster_faction

Function `( Npc ) -> MonsterFactionIntId`

#### follow_distance

Function `( Npc ) -> int`

#### current_target

Function `( Npc ) -> Creature`

#### current_ally

Function `( Npc ) -> Creature`

#### danger_assessment

Function `( Npc ) -> double`

#### say

Function `( Npc, string )`

#### smash_ability

Function `( Npc ) -> int`

#### complain_about

Function `( Npc, string, TimeDuration, string, Opt( bool ) ) -> bool`

#### warn_about

Function `( Npc, string, TimeDuration, string, int, Tripoint )`

#### complain

Function `( Npc ) -> bool`

#### evaluate_enemy

Function `( Npc, Creature ) -> double`

#### can_open_door

Function `( Npc, Tripoint, bool ) -> bool`

#### can_move_to

Function `( Npc, Tripoint, bool ) -> bool`

#### saw_player_recently

Function `( Npc ) -> bool`

#### has_omt_destination

Function `( Npc ) -> bool`

#### get_attitude

Function `( Npc ) -> NpcAttitude`

#### set_attitude

Function `( Npc, NpcAttitude )`

#### has_activity

Function `( Npc ) -> bool`

## NpcOpinion

### Bases

No base classes.

### Constructors

#### `NpcOpinion.new()`

#### `NpcOpinion.new( int, int, int, int, int )`

### Members

#### trust

Variable of type `int`

#### fear

Variable of type `int`

#### value

Variable of type `int`

#### anger

Variable of type `int`

#### owed

Variable of type `int`

## NpcPersonality

### Bases

No base classes.

### Constructors

#### `NpcPersonality.new()`

### Members

#### aggression

Variable of type `char`

#### bravery

Variable of type `char`

#### collector

Variable of type `char`

#### altruism

Variable of type `char`

## Player

### Bases

- `Character`
- `Creature`

### Constructors

No constructors.

### Members

No members.

## Point

### Bases

No base classes.

### Constructors

#### `Point.new()`

#### `Point.new( Point )`

#### `Point.new( int, int )`

### Members

#### x

Variable of type `int`

#### y

Variable of type `int`

#### abs

Function `( Point ) -> Point`

#### rotate

Function `( Point, int, Point ) -> Point`

#### serialize

Function `( Point, <cppval: 7JsonOut > )`

#### deserialize

Function `( Point, <cppval: 6JsonIn > )`

#### __tostring

Function `( Point ) -> string`

#### __eq

Function `( Point, Point ) -> bool`

#### __lt

Function `( Point, Point ) -> bool`

#### __add

Function `( Point, Point ) -> Point`

#### __sub

Function `( Point, Point ) -> Point`

#### __mul

Function `( Point, int ) -> Point`

#### __div

Function `( Point, int ) -> Point`

#### __idiv

Function `( Point, int ) -> Point`

#### __unm

Function `( Point ) -> Point`

## PopupInputStr

### Bases

No base classes.

### Constructors

#### `PopupInputStr.new()`

### Members

#### title

`title` is on the left of input field.
Function `( PopupInputStr, string )`

#### desc

`desc` is above input field.
Function `( PopupInputStr, string )`

#### query_str

Returns your input.
Function `( PopupInputStr ) -> string`

#### query_int

Returns your input, but allows numbers only.
Function `( PopupInputStr ) -> int`

## QueryPopup

### Bases

No base classes.

### Constructors

#### `QueryPopup.new()`

### Members

#### message

Function `( QueryPopup, ... )`

#### message_color

Function `( QueryPopup, Color )`

#### allow_any_key

Set whether to allow any key
Function `( QueryPopup, bool )`

#### query

Returns selected action
Function `( QueryPopup ) -> string`

#### query_yn

Returns `YES` or `NO`. If ESC pressed, returns `NO`.
Function `( QueryPopup ) -> string`

#### query_ynq

Returns `YES`, `NO` or `QUIT`. If ESC pressed, returns `QUIT`.
Function `( QueryPopup ) -> string`

## RecipeId

### Bases

No base classes.

### Constructors

#### `RecipeId.new()`

#### `RecipeId.new( RecipeId )`

#### `RecipeId.new( string )`

### Members

#### obj

Function `( RecipeId ) -> RecipeRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( RecipeId ) -> bool`

#### is_valid

Function `( RecipeId ) -> bool`

#### str

Function `( RecipeId ) -> string`

#### NULL_ID

Function `() -> RecipeId`

#### __tostring

Function `( RecipeId ) -> string`

#### serialize

Function `( RecipeId, <cppval: 7JsonOut > )`

#### deserialize

Function `( RecipeId, <cppval: 6JsonIn > )`

## RecipeRaw

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### category

Variable of type `string`

#### subcategory

Variable of type `string`

#### time

Variable of type `int`

#### skill_used

Variable of type `SkillId`

#### difficulty

Variable of type `int`

#### required_skills

Variable of type `Map( SkillId, int )`

#### learn_by_disassembly

Variable of type `Map( SkillId, int )`

#### booksets

Variable of type `Map( ItypeId, int )`

#### ident

Function `( RecipeRaw ) -> RecipeId`

#### result

Function `( RecipeRaw ) -> ItypeId`

#### result_name

Function `( RecipeRaw ) -> string`

#### has_flag

Function `( RecipeRaw, string ) -> bool`

#### get_from_skill_used

Function `( SkillId ) -> Vector( RecipeRaw )`

#### get_from_flag

Function `( string ) -> Vector( RecipeRaw )`

#### get_all

Function `() -> Vector( RecipeRaw )`

## SkillId

### Bases

No base classes.

### Constructors

#### `SkillId.new()`

#### `SkillId.new( SkillId )`

#### `SkillId.new( string )`

### Members

#### obj

Function `( SkillId ) -> SkillRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( SkillId ) -> bool`

#### is_valid

Function `( SkillId ) -> bool`

#### str

Function `( SkillId ) -> string`

#### NULL_ID

Function `() -> SkillId`

#### __tostring

Function `( SkillId ) -> string`

#### serialize

Function `( SkillId, <cppval: 7JsonOut > )`

#### deserialize

Function `( SkillId, <cppval: 6JsonIn > )`

## SkillLevel

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### is_training

Function `( SkillLevel ) -> bool`

#### level

Function `( SkillLevel ) -> int`

#### highest_level

Function `( SkillLevel ) -> int`

#### train

Function `( SkillLevel, int, bool )`

#### can_train

Function `( SkillLevel ) -> bool`

## SkillLevelMap

### Bases

- `Map( SkillId, SkillLevel )`

### Constructors

No constructors.

### Members

#### mod_skill_level

Function `( SkillLevelMap, SkillId, int )`

#### get_skill_level

Function `( SkillLevelMap, SkillId ) -> int`

#### get_skill_level_object

Function `( SkillLevelMap, SkillId ) -> SkillLevel`

## SpeciesTypeId

### Bases

No base classes.

### Constructors

#### `SpeciesTypeId.new()`

#### `SpeciesTypeId.new( SpeciesTypeId )`

#### `SpeciesTypeId.new( string )`

### Members

#### obj

Function `( SpeciesTypeId ) -> SpeciesTypeRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( SpeciesTypeId ) -> bool`

#### is_valid

Function `( SpeciesTypeId ) -> bool`

#### str

Function `( SpeciesTypeId ) -> string`

#### NULL_ID

Function `() -> SpeciesTypeId`

#### __tostring

Function `( SpeciesTypeId ) -> string`

#### serialize

Function `( SpeciesTypeId, <cppval: 7JsonOut > )`

#### deserialize

Function `( SpeciesTypeId, <cppval: 6JsonIn > )`

## Spell

The class used for spells that _a player_ knows, casts, and gains experience for using. If a given spell is not supposed to be directly cast by a player, consider using SpellSimple instead.

### Bases

No base classes.

### Constructors

#### `Spell.new( SpellTypeId, int )`

### Members

#### id

Variable of type `SpellTypeId`

#### xp

Function `( Spell ) -> int`

#### gain_exp

Function `( Spell, int )`

#### set_exp

Function `( Spell, int )`

#### gain_levels

Function `( Spell, int )`

#### set_level

Function `( Spell, int )`

#### get_level

Function `( Spell ) -> int`

#### name

Function `( Spell ) -> string`

#### desc

Function `( Spell ) -> string`

#### cast

Cast this spell, as well as any sub-spells.
Function `( Spell, Creature, Tripoint )`

#### cast_single_effect

Cast _only_ this spell's main effects. Generally, cast() should be used instead.
Function `( Spell, Creature, Tripoint )`

## SpellSimple

The type for basic spells. If you don't need to track XP from casting (e.g., if a spell is intended to be cast by anything _other than_ a player), this is likely the appropriate type. Otherwise, see the Spell type.

### Bases

No base classes.

### Constructors

#### `SpellSimple.new( SpellTypeId, bool )`

#### `SpellSimple.new( SpellTypeId, bool, int )`

### Members

#### __tostring

Function `( SpellSimple ) -> string`

#### id

Variable of type `SpellTypeId`

#### max_level

Returns the defined maximum level of this SpellSimple instance, if defined. Otherwise, returns 0.
Function `( SpellSimple ) -> int`

#### level

Variable of type `int`

#### force_target_source

Whether or not the target point is _locked_ to the source's location.
Variable of type `bool`

#### trigger_once_in

Used for enchantments; the spell's _chance_ to trigger every turn.
Variable of type `int`

#### cast

Function `( SpellSimple, Creature, Tripoint, Opt( int ) )`

#### prompt_cast

Static function: Creates and immediately casts a SimpleSpell, then returns the new spell for potential reuse. If the given tripoint is the player's location, the spell will be locked to the player. (This does not necessarily cause friendly fire!) If an integer is specified, the spell will be cast at that level.
Function `( SpellTypeId, Tripoint, Opt( int ) ) -> SpellSimple`

## SpellTypeId

### Bases

No base classes.

### Constructors

#### `SpellTypeId.new()`

#### `SpellTypeId.new( SpellTypeId )`

#### `SpellTypeId.new( string )`

### Members

#### obj

Function `( SpellTypeId ) -> SpellTypeRaw`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( SpellTypeId ) -> bool`

#### is_valid

Function `( SpellTypeId ) -> bool`

#### str

Function `( SpellTypeId ) -> string`

#### NULL_ID

Function `() -> SpellTypeId`

#### __tostring

Function `( SpellTypeId ) -> string`

#### serialize

Function `( SpellTypeId, <cppval: 7JsonOut > )`

#### deserialize

Function `( SpellTypeId, <cppval: 6JsonIn > )`

## SpellTypeRaw

The 'raw' type for storing the information defining every spell in the game. It's not possible to cast directly from this type; check SpellSimple and Spell.

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### __tostring

Function `( SpellTypeRaw ) -> string`

#### id

Variable of type `SpellTypeId`

#### effect_name

The name of the primary effect this spell will enact.
Variable of type `string`

#### effect_str

Specifics about the effect this spell will enact.
Variable of type `string`

#### field_chance

Variable of type `int`

#### min_field_intensity

Variable of type `int`

#### field_intensity_increment

Variable of type `double`

#### max_field_intensity

Variable of type `int`

#### field_intensity_variance

Variable of type `double`

#### min_damage

Variable of type `int`

#### damage_increment

Variable of type `double`

#### max_damage

Variable of type `int`

#### min_range

Variable of type `int`

#### range_increment

Variable of type `double`

#### max_range

Variable of type `int`

#### min_aoe

Variable of type `int`

#### aoe_increment

Variable of type `double`

#### max_aoe

Variable of type `int`

#### min_dot

Variable of type `int`

#### dot_increment

Variable of type `double`

#### max_dot

Variable of type `int`

#### min_duration

Variable of type `int`

#### duration_increment

Variable of type `int`

#### max_duration

Variable of type `int`

#### base_energy_cost

Variable of type `int`

#### energy_increment

Variable of type `double`

#### final_energy_cost

Variable of type `int`

#### difficulty

Variable of type `int`

#### max_level

Variable of type `int`

#### base_casting_time

Variable of type `int`

#### casting_time_increment

Variable of type `double`

#### final_casting_time

Variable of type `int`

#### additional_spells

Other spells cast by this spell.
Function `( SpellTypeRaw ) -> Vector( SpellSimple )`

#### get_all

Returns a (long) list of every spell in the game.
Function `() -> <cppval: St6vectorI10spell_typeSaIS0_EE >`

## TerId

### Bases

No base classes.

### Constructors

#### `TerId.new()`

#### `TerId.new( TerId )`

#### `TerId.new( TerIntId )`

#### `TerId.new( string )`

### Members

#### obj

Function `( TerId ) -> TerRaw`

#### int_id

Function `( TerId ) -> TerIntId`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( TerId ) -> bool`

#### is_valid

Function `( TerId ) -> bool`

#### str

Function `( TerId ) -> string`

#### NULL_ID

Function `() -> TerId`

#### __tostring

Function `( TerId ) -> string`

#### serialize

Function `( TerId, <cppval: 7JsonOut > )`

#### deserialize

Function `( TerId, <cppval: 6JsonIn > )`

## TerIntId

### Bases

No base classes.

### Constructors

#### `TerIntId.new()`

#### `TerIntId.new( TerIntId )`

#### `TerIntId.new( TerId )`

### Members

#### obj

Function `( TerIntId ) -> TerRaw`

#### str_id

Function `( TerIntId ) -> TerId`

#### is_valid

Function `( TerIntId ) -> bool`

#### __tostring

Function `( TerIntId ) -> string`

## TerRaw

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### str_id

Function `( TerRaw ) -> TerId`

#### int_id

Function `( TerRaw ) -> TerIntId`

#### name

Function `( TerRaw ) -> string`

#### get_flags

Function `( TerRaw ) -> <cppval: St3setINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt4lessIS5_ESaIS5_EE >`

#### has_flag

Function `( TerRaw, string ) -> bool`

#### set_flag

Function `( TerRaw, string )`

#### get_light_emitted

Function `( TerRaw ) -> int`

#### set_light_emitted

Function `( TerRaw, int )`

#### get_movecost

Function `( TerRaw ) -> int`

#### set_movecost

Function `( TerRaw, int )`

#### get_coverage

Function `( TerRaw ) -> int`

#### set_coverage

Function `( TerRaw, int )`

#### get_max_volume

Function `( TerRaw ) -> Volume`

#### set_max_volume

Function `( TerRaw, Volume )`

#### open

Variable of type `TerId`

#### close

Variable of type `TerId`

#### trap_id_str

Variable of type `string`

#### transforms_into

Variable of type `TerId`

#### roof

Variable of type `TerId`

#### heat_radiation

Variable of type `int`

## TimeDuration

Represent duration between 2 fixed points in time

### Bases

No base classes.

### Constructors

#### `TimeDuration.new()`

### Members

#### from_turns

Function `( int ) -> TimeDuration`

#### from_seconds

Function `( int ) -> TimeDuration`

#### from_minutes

Function `( int ) -> TimeDuration`

#### from_hours

Function `( int ) -> TimeDuration`

#### from_days

Function `( int ) -> TimeDuration`

#### from_weeks

Function `( int ) -> TimeDuration`

#### make_random

Function `( TimeDuration, TimeDuration ) -> TimeDuration`

#### to_turns

Function `( TimeDuration ) -> int`

#### to_seconds

Function `( TimeDuration ) -> int`

#### to_minutes

Function `( TimeDuration ) -> int`

#### to_hours

Function `( TimeDuration ) -> int`

#### to_days

Function `( TimeDuration ) -> int`

#### to_weeks

Function `( TimeDuration ) -> int`

#### serialize

Function `( TimeDuration, <cppval: 7JsonOut > )`

#### deserialize

Function `( TimeDuration, <cppval: 6JsonIn > )`

#### __tostring

Function `( TimeDuration ) -> string`

#### __add

Function `( TimeDuration, TimeDuration ) -> TimeDuration`

#### __sub

Function `( TimeDuration, TimeDuration ) -> TimeDuration`

#### __mul

Function `( TimeDuration, int ) -> TimeDuration`

#### __div

Function `( TimeDuration, int ) -> TimeDuration`

#### __unm

Function `( TimeDuration ) -> TimeDuration`

## TimePoint

Represent fixed point in time

### Bases

No base classes.

### Constructors

#### `TimePoint.new()`

### Members

#### from_turn

Function `( int ) -> TimePoint`

#### to_turn

Function `( TimePoint ) -> int`

#### is_night

Function `( TimePoint ) -> bool`

#### is_day

Function `( TimePoint ) -> bool`

#### is_dusk

Function `( TimePoint ) -> bool`

#### is_dawn

Function `( TimePoint ) -> bool`

#### second_of_minute

Function `( TimePoint ) -> int`

#### minute_of_hour

Function `( TimePoint ) -> int`

#### hour_of_day

Function `( TimePoint ) -> int`

#### serialize

Function `( TimePoint, <cppval: 7JsonOut > )`

#### deserialize

Function `( TimePoint, <cppval: 6JsonIn > )`

#### to_string_time_of_day

Function `( TimePoint ) -> string`

#### __tostring

Function `( TimePoint ) -> string`

#### __eq

Function `( TimePoint, TimePoint ) -> bool`

#### __lt

Function `( TimePoint, TimePoint ) -> bool`

#### __add

Function `( TimePoint, TimeDuration ) -> TimePoint`

#### __sub

Function `( TimePoint, TimePoint ) -> TimeDuration`
Function `( TimePoint, TimeDuration ) -> TimePoint`

## Tinymap

### Bases

- `Map`

### Constructors

No constructors.

### Members

No members.

## TrapId

### Bases

No base classes.

### Constructors

#### `TrapId.new()`

#### `TrapId.new( TrapId )`

#### `TrapId.new( TrapIntId )`

#### `TrapId.new( string )`

### Members

#### obj

Function `( TrapId ) -> TrapRaw`

#### int_id

Function `( TrapId ) -> TrapIntId`

#### implements_int_id

Function `() -> bool`

#### is_null

Function `( TrapId ) -> bool`

#### is_valid

Function `( TrapId ) -> bool`

#### str

Function `( TrapId ) -> string`

#### NULL_ID

Function `() -> TrapId`

#### __tostring

Function `( TrapId ) -> string`

#### serialize

Function `( TrapId, <cppval: 7JsonOut > )`

#### deserialize

Function `( TrapId, <cppval: 6JsonIn > )`

## TrapIntId

### Bases

No base classes.

### Constructors

#### `TrapIntId.new()`

#### `TrapIntId.new( TrapIntId )`

#### `TrapIntId.new( TrapId )`

### Members

#### obj

Function `( TrapIntId ) -> TrapRaw`

#### str_id

Function `( TrapIntId ) -> TrapId`

#### is_valid

Function `( TrapIntId ) -> bool`

#### __tostring

Function `( TrapIntId ) -> string`

## Tripoint

### Bases

No base classes.

### Constructors

#### `Tripoint.new()`

#### `Tripoint.new( Point, int )`

#### `Tripoint.new( Tripoint )`

#### `Tripoint.new( int, int, int )`

### Members

#### x

Variable of type `int`

#### y

Variable of type `int`

#### z

Variable of type `int`

#### abs

Function `( Tripoint ) -> Tripoint`

#### xy

Function `( Tripoint ) -> Point`

#### rotate_2d

Function `( Tripoint, int, Point ) -> Tripoint`

#### serialize

Function `( Tripoint, <cppval: 7JsonOut > )`

#### deserialize

Function `( Tripoint, <cppval: 6JsonIn > )`

#### __tostring

Function `( Tripoint ) -> string`

#### __eq

Function `( Tripoint, Tripoint ) -> bool`

#### __lt

Function `( Tripoint, Tripoint ) -> bool`

#### __add

Function `( Tripoint, Tripoint ) -> Tripoint`
Function `( Tripoint, Point ) -> Tripoint`

#### __sub

Function `( Tripoint, Tripoint ) -> Tripoint`
Function `( Tripoint, Point ) -> Tripoint`

#### __mul

Function `( Tripoint, int ) -> Tripoint`

#### __div

Function `( Tripoint, int ) -> Tripoint`

#### __idiv

Function `( Tripoint, int ) -> Tripoint`

#### __unm

Function `( Tripoint ) -> Tripoint`

## UiList

### Bases

No base classes.

### Constructors

#### `UiList.new()`

### Members

#### title

Sets title which is on the top line.
Function `( UiList, string )`

#### text

Sets text which is in upper box.
Function `( UiList, string )`

#### footer

Sets footer text which is in lower box. It overwrites descs of entries unless is empty.
Function `( UiList, string )`

#### desc_enabled

Puts a lower box. Footer or entry desc appears on it.
Function `( UiList, bool )`

#### add

Adds an entry. `string` is its name, and `int` is what it returns. If `int` is `-1`, the number is decided orderly.
Function `( UiList, int, string )`

#### add_w_desc

Adds an entry with desc(second `string`). `desc_enabled(true)` is required for showing desc.
Function `( UiList, int, string, string )`

#### add_w_col

Adds an entry with desc and col(third `string`). col is additional text on the right of the entry name.
Function `( UiList, int, string, string, string )`

#### entries

Entries from uilist. Remember, in lua, the first element of vector is `entries[1]`, not `entries[0]`.
Variable of type `Vector( UiListEntry )`

#### border_color

Changes the color. Default color is `c_magenta`.
Function `( UiList, Color )`

#### text_color

Changes the color. Default color is `c_light_gray`.
Function `( UiList, Color )`

#### title_color

Changes the color. Default color is `c_green`.
Function `( UiList, Color )`

#### hilight_color

Changes the color. Default color is `h_white`.
Function `( UiList, Color )`

#### hotkey_color

Changes the color. Default color is `c_light_green`.
Function `( UiList, Color )`

#### query

Returns retval for selected entry, or a negative number on fail/cancel
Function `( UiList ) -> int`

## UiListEntry

This type came from UiList.

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### enable

Entry whether it's enabled or not. Default is `true`.
Variable of type `bool`

#### txt

Entry text
Variable of type `string`

#### desc

Entry description
Variable of type `string`

#### ctxt

Entry text of column.
Variable of type `string`

#### txt_color

Entry text color. Its default color is `c_red_red`, which makes color of the entry same as what `uilist` decides. So if you want to make color different, choose one except `c_red_red`.
Function `( UiListEntry, Color )`

## Volume

### Bases

No base classes.

### Constructors

No constructors.

### Members

#### from_milliliter

Function `( int ) -> Volume`

#### from_liter

Function `( int ) -> Volume`

#### to_milliliter

Function `( Volume ) -> int`

#### to_liter

Function `( Volume ) -> double`

#### __eq

Function `( Volume, Volume ) -> bool`

#### __lt

Function `( Volume, Volume ) -> bool`

#### __le

Function `( Volume, Volume ) -> bool`

# Enums

## AddictionType

### Entries

- `NONE` = `0`
- `CAFFEINE` = `1`
- `ALCOHOL` = `2`
- `SLEEP` = `3`
- `PKILLER` = `4`
- `SPEED` = `5`
- `CIG` = `6`
- `COKE` = `7`
- `CRACK` = `8`
- `MUTAGEN` = `9`
- `DIAZEPAM` = `10`
- `MARLOSS_R` = `11`
- `MARLOSS_B` = `12`
- `MARLOSS_Y` = `13`

## Attitude

### Entries

- `Hostile` = `0`
- `Neutral` = `1`
- `Friendly` = `2`
- `Any` = `3`

## BodyPart

### Entries

- `TORSO` = `0`
- `HEAD` = `1`
- `EYES` = `2`
- `MOUTH` = `3`
- `ARM_L` = `4`
- `ARM_R` = `5`
- `HAND_L` = `6`
- `HAND_R` = `7`
- `LEG_L` = `8`
- `LEG_R` = `9`
- `FOOT_L` = `10`
- `FOOT_R` = `11`

## CharacterMoveMode

### Entries

- `walk` = `0`
- `run` = `1`
- `crouch` = `2`

## Color

### Entries

- `c_black` = `0`
- `c_white` = `1`
- `c_light_gray` = `2`
- `c_dark_gray` = `3`
- `c_red` = `4`
- `c_green` = `5`
- `c_blue` = `6`
- `c_cyan` = `7`
- `c_magenta` = `8`
- `c_brown` = `9`
- `c_light_red` = `10`
- `c_light_green` = `11`
- `c_light_blue` = `12`
- `c_light_cyan` = `13`
- `c_pink` = `14`
- `c_yellow` = `15`
- `h_black` = `16`
- `h_white` = `17`
- `h_light_gray` = `18`
- `h_dark_gray` = `19`
- `h_red` = `20`
- `h_green` = `21`
- `h_blue` = `22`
- `h_cyan` = `23`
- `h_magenta` = `24`
- `h_brown` = `25`
- `h_light_red` = `26`
- `h_light_green` = `27`
- `h_light_blue` = `28`
- `h_light_cyan` = `29`
- `h_pink` = `30`
- `h_yellow` = `31`
- `i_black` = `32`
- `i_white` = `33`
- `i_light_gray` = `34`
- `i_dark_gray` = `35`
- `i_red` = `36`
- `i_green` = `37`
- `i_blue` = `38`
- `i_cyan` = `39`
- `i_magenta` = `40`
- `i_brown` = `41`
- `i_light_red` = `42`
- `i_light_green` = `43`
- `i_light_blue` = `44`
- `i_light_cyan` = `45`
- `i_pink` = `46`
- `i_yellow` = `47`
- `c_unset` = `48`
- `c_black_red` = `49`
- `c_white_red` = `50`
- `c_light_gray_red` = `51`
- `c_dark_gray_red` = `52`
- `c_red_red` = `53`
- `c_green_red` = `54`
- `c_blue_red` = `55`
- `c_cyan_red` = `56`
- `c_magenta_red` = `57`
- `c_brown_red` = `58`
- `c_light_red_red` = `59`
- `c_light_green_red` = `60`
- `c_light_blue_red` = `61`
- `c_light_cyan_red` = `62`
- `c_pink_red` = `63`
- `c_yellow_red` = `64`
- `c_black_white` = `65`
- `c_dark_gray_white` = `66`
- `c_light_gray_white` = `67`
- `c_white_white` = `68`
- `c_red_white` = `69`
- `c_light_red_white` = `70`
- `c_green_white` = `71`
- `c_light_green_white` = `72`
- `c_brown_white` = `73`
- `c_yellow_white` = `74`
- `c_blue_white` = `75`
- `c_light_blue_white` = `76`
- `c_magenta_white` = `77`
- `c_pink_white` = `78`
- `c_cyan_white` = `79`
- `c_light_cyan_white` = `80`
- `c_black_green` = `81`
- `c_dark_gray_green` = `82`
- `c_light_gray_green` = `83`
- `c_white_green` = `84`
- `c_red_green` = `85`
- `c_light_red_green` = `86`
- `c_green_green` = `87`
- `c_light_green_green` = `88`
- `c_brown_green` = `89`
- `c_yellow_green` = `90`
- `c_blue_green` = `91`
- `c_light_blue_green` = `92`
- `c_magenta_green` = `93`
- `c_pink_green` = `94`
- `c_cyan_green` = `95`
- `c_light_cyan_green` = `96`
- `c_black_yellow` = `97`
- `c_dark_gray_yellow` = `98`
- `c_light_gray_yellow` = `99`
- `c_white_yellow` = `100`
- `c_red_yellow` = `101`
- `c_light_red_yellow` = `102`
- `c_green_yellow` = `103`
- `c_light_green_yellow` = `104`
- `c_brown_yellow` = `105`
- `c_yellow_yellow` = `106`
- `c_blue_yellow` = `107`
- `c_light_blue_yellow` = `108`
- `c_magenta_yellow` = `109`
- `c_pink_yellow` = `110`
- `c_cyan_yellow` = `111`
- `c_light_cyan_yellow` = `112`
- `c_black_magenta` = `113`
- `c_dark_gray_magenta` = `114`
- `c_light_gray_magenta` = `115`
- `c_white_magenta` = `116`
- `c_red_magenta` = `117`
- `c_light_red_magenta` = `118`
- `c_green_magenta` = `119`
- `c_light_green_magenta` = `120`
- `c_brown_magenta` = `121`
- `c_yellow_magenta` = `122`
- `c_blue_magenta` = `123`
- `c_light_blue_magenta` = `124`
- `c_magenta_magenta` = `125`
- `c_pink_magenta` = `126`
- `c_cyan_magenta` = `127`
- `c_light_cyan_magenta` = `128`
- `c_black_cyan` = `129`
- `c_dark_gray_cyan` = `130`
- `c_light_gray_cyan` = `131`
- `c_white_cyan` = `132`
- `c_red_cyan` = `133`
- `c_light_red_cyan` = `134`
- `c_green_cyan` = `135`
- `c_light_green_cyan` = `136`
- `c_brown_cyan` = `137`
- `c_yellow_cyan` = `138`
- `c_blue_cyan` = `139`
- `c_light_blue_cyan` = `140`
- `c_magenta_cyan` = `141`
- `c_pink_cyan` = `142`
- `c_cyan_cyan` = `143`
- `c_light_cyan_cyan` = `144`

## DamageType

### Entries

- `DT_NULL` = `0`
- `DT_TRUE` = `1`
- `DT_BIOLOGICAL` = `2`
- `DT_BASH` = `3`
- `DT_CUT` = `4`
- `DT_ACID` = `5`
- `DT_STAB` = `6`
- `DT_HEAT` = `7`
- `DT_COLD` = `8`
- `DT_DARK` = `9`
- `DT_LIGHT` = `10`
- `DT_PSI` = `11`
- `DT_ELECTRIC` = `12`
- `DT_BULLET` = `13`

## MissionGoal

### Entries

- `MGOAL_NULL` = `0`
- `MGOAL_GO_TO` = `1`
- `MGOAL_GO_TO_TYPE` = `2`
- `MGOAL_FIND_ITEM` = `3`
- `MGOAL_FIND_ANY_ITEM` = `4`
- `MGOAL_FIND_ITEM_GROUP` = `5`
- `MGOAL_FIND_MONSTER` = `6`
- `MGOAL_FIND_NPC` = `7`
- `MGOAL_ASSASSINATE` = `8`
- `MGOAL_KILL_MONSTER` = `9`
- `MGOAL_KILL_MONSTER_TYPE` = `10`
- `MGOAL_RECRUIT_NPC` = `11`
- `MGOAL_RECRUIT_NPC_CLASS` = `12`
- `MGOAL_COMPUTER_TOGGLE` = `13`
- `MGOAL_KILL_MONSTER_SPEC` = `14`
- `MGOAL_TALK_TO_NPC` = `15`
- `MGOAL_CONDITION` = `16`

## MissionOrigin

### Entries

- `ORIGIN_NULL` = `0`
- `ORIGIN_GAME_START` = `1`
- `ORIGIN_OPENER_NPC` = `2`
- `ORIGIN_ANY_NPC` = `3`
- `ORIGIN_SECONDARY` = `4`
- `ORIGIN_COMPUTER` = `5`

## MonsterAttitude

### Entries

- `MATT_NULL` = `0`
- `MATT_FRIEND` = `1`
- `MATT_FPASSIVE` = `2`
- `MATT_FLEE` = `3`
- `MATT_IGNORE` = `4`
- `MATT_FOLLOW` = `5`
- `MATT_ATTACK` = `6`
- `MATT_ZLAVE` = `7`

## MonsterFactionAttitude

### Entries

- `ByMood` = `0`
- `Neutral` = `1`
- `Friendly` = `2`
- `Hate` = `3`

## MonsterFlag

### Entries

- `SEES` = `0`
- `HEARS` = `1`
- `GOODHEARING` = `2`
- `SMELLS` = `3`
- `KEENNOSE` = `4`
- `STUMBLES` = `5`
- `WARM` = `6`
- `NOHEAD` = `7`
- `HARDTOSHOOT` = `8`
- `GRABS` = `9`
- `BASHES` = `10`
- `DESTROYS` = `11`
- `BORES` = `12`
- `POISON` = `13`
- `VENOM` = `14`
- `BADVENOM` = `15`
- `PARALYZEVENOM` = `16`
- `BLEED` = `17`
- `WEBWALK` = `18`
- `DIGS` = `19`
- `CAN_DIG` = `20`
- `FLIES` = `21`
- `AQUATIC` = `22`
- `SWIMS` = `23`
- `ATTACKMON` = `24`
- `ANIMAL` = `25`
- `PLASTIC` = `26`
- `SUNDEATH` = `27`
- `ELECTRIC` = `28`
- `ACIDPROOF` = `29`
- `ACIDTRAIL` = `30`
- `SHORTACIDTRAIL` = `31`
- `FIREPROOF` = `32`
- `SLUDGEPROOF` = `33`
- `SLUDGETRAIL` = `34`
- `COLDPROOF` = `35`
- `BIOPROOF` = `36`
- `DARKPROOF` = `37`
- `LIGHTPROOF` = `38`
- `PSIPROOF` = `39`
- `FIREY` = `40`
- `QUEEN` = `41`
- `ELECTRONIC` = `42`
- `FUR` = `43`
- `LEATHER` = `44`
- `WOOL` = `45`
- `FEATHER` = `46`
- `BONES` = `47`
- `FAT` = `48`
- `CONSOLE_DESPAWN` = `49`
- `IMMOBILE` = `50`
- `ID_CARD_DESPAWN` = `51`
- `RIDEABLE_MECH` = `52`
- `CARD_OVERRIDE` = `53`
- `MILITARY_MECH` = `54`
- `MECH_RECON_VISION` = `55`
- `MECH_DEFENSIVE` = `56`
- `HIT_AND_RUN` = `57`
- `GUILT` = `58`
- `PAY_BOT` = `59`
- `HUMAN` = `60`
- `NO_BREATHE` = `61`
- `FLAMMABLE` = `62`
- `REVIVES` = `63`
- `CHITIN` = `64`
- `VERMIN` = `65`
- `NOGIB` = `66`
- `LARVA` = `67`
- `ARTHROPOD_BLOOD` = `68`
- `ACID_BLOOD` = `69`
- `BILE_BLOOD` = `70`
- `ABSORBS` = `71`
- `ABSORBS_SPLITS` = `72`
- `CBM_CIV` = `73`
- `CBM_POWER` = `74`
- `CBM_SCI` = `75`
- `CBM_OP` = `76`
- `CBM_TECH` = `77`
- `CBM_SUBS` = `78`
- `UNUSED_76` = `79`
- `FISHABLE` = `80`
- `GROUP_BASH` = `81`
- `SWARMS` = `82`
- `GROUP_MORALE` = `83`
- `INTERIOR_AMMO` = `84`
- `CLIMBS` = `85`
- `PACIFIST` = `86`
- `PUSH_MON` = `87`
- `PUSH_VEH` = `88`
- `NIGHT_INVISIBILITY` = `89`
- `REVIVES_HEALTHY` = `90`
- `NO_NECRO` = `91`
- `PATH_AVOID_DANGER_1` = `92`
- `PATH_AVOID_DANGER_2` = `93`
- `PATH_AVOID_FIRE` = `94`
- `PATH_AVOID_FALL` = `95`
- `PRIORITIZE_TARGETS` = `96`
- `NOT_HALLUCINATION` = `97`
- `CANPLAY` = `98`
- `PET_MOUNTABLE` = `99`
- `PET_HARNESSABLE` = `100`
- `DOGFOOD` = `101`
- `MILKABLE` = `102`
- `SHEARABLE` = `103`
- `NO_BREED` = `104`
- `NO_FUNG_DMG` = `105`
- `PET_WONT_FOLLOW` = `106`
- `DRIPS_NAPALM` = `107`
- `DRIPS_GASOLINE` = `108`
- `ELECTRIC_FIELD` = `109`
- `LOUDMOVES` = `110`
- `CAN_OPEN_DOORS` = `111`
- `STUN_IMMUNE` = `112`
- `DROPS_AMMO` = `113`
- `CAN_BE_ORDERED` = `114`
- `SMALL_HEAD` = `115`
- `TINY_HEAD` = `116`
- `NO_HEAD_BONUS_CRIT` = `117`
- `HEAD_BONUS_CRIT_1` = `118`
- `HEAD_BONUS_CRIT_2` = `119`
- `TORSO_BONUS_CRIT_1` = `120`
- `TORSO_BONUS_CRIT_2` = `121`
- `PROJECTILE_RESISTANT_1` = `122`
- `PROJECTILE_RESISTANT_2` = `123`
- `PROJECTILE_RESISTANT_3` = `124`
- `PROJECTILE_RESISTANT_4` = `125`
- `VOLATILE` = `126`

## MonsterSize

### Entries

- `TINY` = `0`
- `SMALL` = `1`
- `MEDIUM` = `2`
- `LARGE` = `3`
- `HUGE` = `4`

## MsgType

### Entries

- `good` = `0`
- `bad` = `1`
- `mixed` = `2`
- `warning` = `3`
- `info` = `4`
- `neutral` = `5`
- `debug` = `6`
- `headshot` = `7`
- `critical` = `8`
- `grazing` = `9`

## NpcAttitude

### Entries

- `NPCATT_NULL` = `0`
- `NPCATT_TALK` = `1`
- `NPCATT_LEGACY_1` = `2`
- `NPCATT_FOLLOW` = `3`
- `NPCATT_LEGACY_2` = `4`
- `NPCATT_LEAD` = `5`
- `NPCATT_WAIT` = `6`
- `NPCATT_LEGACY_6` = `7`
- `NPCATT_MUG` = `8`
- `NPCATT_WAIT_FOR_LEAVE` = `9`
- `NPCATT_KILL` = `10`
- `NPCATT_FLEE` = `11`
- `NPCATT_LEGACY_3` = `12`
- `NPCATT_HEAL` = `13`
- `NPCATT_LEGACY_4` = `14`
- `NPCATT_LEGACY_5` = `15`
- `NPCATT_ACTIVITY` = `16`
- `NPCATT_FLEE_TEMP` = `17`
- `NPCATT_RECOVER_GOODS` = `18`

## NpcNeed

### Entries

- `need_none` = `0`
- `need_ammo` = `1`
- `need_weapon` = `2`
- `need_gun` = `3`
- `need_food` = `4`
- `need_drink` = `5`
- `need_safety` = `6`

## SfxChannel

### Entries

- `daytime_outdoors_env` = `0`
- `nighttime_outdoors_env` = `1`
- `underground_env` = `2`
- `indoors_env` = `3`
- `indoors_rain_env` = `4`
- `outdoors_snow_env` = `5`
- `outdoors_flurry_env` = `6`
- `outdoors_thunderstorm_env` = `7`
- `outdoors_rain_env` = `8`
- `outdoors_drizzle_env` = `9`
- `outdoor_blizzard` = `10`
- `deafness_tone` = `11`
- `danger_extreme_theme` = `12`
- `danger_high_theme` = `13`
- `danger_medium_theme` = `14`
- `danger_low_theme` = `15`
- `stamina_75` = `16`
- `stamina_50` = `17`
- `stamina_35` = `18`
- `idle_chainsaw` = `19`
- `chainsaw_theme` = `20`
- `player_activities` = `21`
- `exterior_engine_sound` = `22`
- `interior_engine_sound` = `23`
- `radio` = `24`

# Libraries

## const

Various game constants

### Members

#### OM_OMT_SIZE

Variable of type `int` value: `180`

#### OM_SM_SIZE

Variable of type `int` value: `360`

#### OM_MS_SIZE

Variable of type `int` value: `4320`

#### OMT_SM_SIZE

Variable of type `int` value: `2`

#### OMT_MS_SIZE

Variable of type `int` value: `24`

#### SM_MS_SIZE

Variable of type `int` value: `12`

## coords

Methods for manipulating coord systems and calculating distance

### Members

#### ms_to_sm

Function `( Tripoint ) -> ( Tripoint, Point )`

#### ms_to_omt

Function `( Tripoint ) -> ( Tripoint, Point )`

#### ms_to_om

Function `( Tripoint ) -> ( Point, Tripoint )`

#### sm_to_ms

Function `( Tripoint, Opt( Point ) ) -> Tripoint`

#### omt_to_ms

Function `( Tripoint, Opt( Point ) ) -> Tripoint`

#### om_to_ms

Function `( Point, Opt( Tripoint ) ) -> Tripoint`

#### rl_dist

Function `( Tripoint, Tripoint ) -> int`
Function `( Point, Point ) -> int`

#### trig_dist

Function `( Tripoint, Tripoint ) -> double`
Function `( Point, Point ) -> double`

#### square_dist

Function `( Tripoint, Tripoint ) -> int`
Function `( Point, Point ) -> int`

## gapi

Global game methods

### Members

#### get_avatar

Function `() -> Avatar`

#### get_map

Function `() -> Map`

#### get_distribution_grid_tracker

Function `() -> DistributionGridTracker`

#### add_msg

Function `( MsgType, ... )`
Function `( ... )`

#### place_player_overmap_at

Teleports player to absolute coordinate in overmap
Function `( Tripoint )`

#### place_player_local_at

Teleports player to local coordinates within active map
Function `( Tripoint )`

#### current_turn

Function `() -> TimePoint`

#### turn_zero

Function `() -> TimePoint`

#### before_time_starts

Function `() -> TimePoint`

#### rng

Function `( int, int ) -> int`

#### add_on_every_x_hook

Function `( TimeDuration, function )`

#### create_item

Function `( ItypeId, int ) -> <cppval: St10unique_ptrI4itemSt14default_deleteIS0_EE >`

#### get_creature_at

Function `( Tripoint, Opt( bool ) ) -> Creature`

#### get_monster_at

Function `( Tripoint, Opt( bool ) ) -> Monster`

#### place_monster_at

Function `( MtypeId, Tripoint ) -> Monster`

#### place_monster_around

Function `( MtypeId, Tripoint, int ) -> Monster`

#### get_character_at

Function `( Tripoint, Opt( bool ) ) -> Character`

#### get_npc_at

Function `( Tripoint, Opt( bool ) ) -> Npc`

#### choose_adjacent

Function `( string, Opt( bool ) ) -> Opt( Tripoint )`

#### choose_direction

Function `( string, Opt( bool ) ) -> Opt( Tripoint )`

#### look_around

Function `() -> Opt( Tripoint )`

#### play_variant_sound

Function `( string, string, int )`
Function `( string, string, int, Angle, double, double )`

#### play_ambient_variant_sound

Function `( string, string, int, SfxChannel, int, double, int )`

#### add_npc_follower

Function `( Npc )`

#### remove_npc_follower

Function `( Npc )`

## gdebug

Debugging and logging API.

### Members

#### log_info

Function `( ... )`

#### log_warn

Function `( ... )`

#### log_error

Function `( ... )`

#### debugmsg

Function `( ... )`

#### clear_lua_log

Function `()`

#### set_log_capacity

Function `( int )`

#### reload_lua_code

Function `()`

#### save_game

Function `() -> bool`

## hooks

Documentation for hooks

### Members

#### on_game_save

Called when game is about to save
Function `()`

#### on_game_load

Called right after game has loaded
Function `()`

#### on_game_started

Called when the game has first started
Function `()`

#### on_creature_dodged

Called when a character successfully dodges
Function `()`

#### on_creature_blocked

Called when a character successfully blocks
Function `()`

#### on_creature_performed_technique

Called when a character has performed technique
Function `()`

#### on_creature_melee_attacked

Called after a character has attacked in melee
Function `()`

#### on_character_reset_stats

Called when character stat gets reset
Function `()`

#### on_char_death

Called when a character is dead
Function `()`

#### on_mon_death

Called when a monster is dead
Function `()`

#### on_every_x

Called every in-game period
Function `()`

#### on_mapgen_postprocess

Called right after mapgen has completed. Map argument is the tinymap that represents 24x24 area (2x2 submaps, or 1x1 omt), tripoint is the absolute omt pos, and time_point is the current time (for time-based effects).
Function `( Map, Tripoint, TimePoint )`

## locale

Localization API.

### Members

#### gettext

Expects english source string, returns translated string.
Function `( string ) -> string`

#### vgettext

First is english singular string, second is english plural string. Number is amount to translate for.
Function `( string, string, <cppval: m > ) -> string`

#### pgettext

First is context string. Second is english source string.
Function `( string, string ) -> string`

#### vpgettext

First is context string. Second is english singular string. third is english plural. Number is amount to translate for.
Function `( string, string, string, <cppval: m > ) -> string`

## tests_lib

Library for testing purposes

### Members

#### my_awesome_lambda_1

Function `() -> int`

#### my_awesome_lambda_2

Function `() -> int`
