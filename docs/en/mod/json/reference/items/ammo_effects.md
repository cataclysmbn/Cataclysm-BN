# Ammo Effects

There are two types of ammo_effects. Hardcoded ammo effects and jsonized ammo effects.

### Hardcoded Ammo Effects

| Identifier             | Description                                                                  |
| ---------------------- | ---------------------------------------------------------------------------- |
| THROWN                 | Used to check if it was thrown or not                                        |
| NO_PENETRATE_OBSTACLES | Cannot obstacles; such as penetrate fences or reinforced glass               |
| HEAVY_HIT              | Will trigger traps and make a louder sound on hitting walls                  |
| NO_ITEM_DAMAGE         | Items and traps the projectile hits will not be damaged                      |
| NOGIB                  | Ammo will not overkill and explode corpses                                   |
| COOKOFF                | Ammo will explode in fires                                                   |
| INCENDIARY             | Causes fire in terrain / targets                                             |
| NEVER_MISFIRES         | Will not misfire                                                             |
| ACT_ON_RANGED_HIT      | The item will trigger it's use action on hit                                 |
| APPLY_SAP              | Applies the sap effect for turns scaling off damage                          |
| BEANBAG                | Applies the stun effect for a short duration                                 |
| BLINDS_EYES            | Applies blind if hits eyes                                                   |
| BOUNCE                 | Bounces and hits other targets in a nearby area also gives the `WIDE` effect |
| BURST                  | Drops contents on impact, but is also destroyed                              |
| CUSTOM_EXPLOSION       | Uses the explosion data of the item                                          |
| DRAW_AS_LINE           | Drawn instantly without animation                                            |
| IGNITE                 | Ignites all hit creatures                                                    |
| JET                    | Draws the whole effect even if damage becomes 0                              |
| LARGE_BEANBAG          | Applies the BEANBAG stun effect for a longer duration                        |
| MUZZLE_SMOKE           | Produces a small smoke plume on firing                                       |
| NO_CRIT                | Cannot critically hit, can still graze                                       |
| NO_DAMAGE              | Does not apply any damage                                                    |
| NO_DAMAGE_SCALING      | Cannot critically hit, cannot graze                                          |
| NO_EMBED               | Cannot embed in creatures                                                    |
| NO_OVERSHOOT           | Trajectory cannot extend pass target, even when missing                      |
| NULL_SOURCE            | Used internally, origin is not a real critter                                |
| POISON                 | Applies poison on hit, worse poison on crit                                  |
| PARALYZEPOISON         | Applies paralyzing poison on hit                                             |
| SHATTER_SELF           | Thrown destoryed on hit, drops contents, produces noise                      |
| TANGLE                 | Applies tied effect to creature can be recovered                             |
| NET_TANGLE             | Applies tied effect to a group, irrecoverable                                |
| WHIP                   | Scares wildlife hit by it, will also apply disarm sometimes on reach attacks |
| BLACKPOWDER            | Will cause build up of damaging gunk                                         |
| RECYCLED               | Causes a 1 in 256 chance to fail to fire                                     |
| RECOVER_X              | Has a (X-1/X) chance to recover one charge of used ammo at the impact        |
| WIDE                   | Currently has no effect, suppost to make `HARDTOSHOOT` have no effect        |
| SHOT                   | Force applies the `WIDE` effect                                              |

### Fields

| Identifier   | Description                                                                                   |
| ------------ | --------------------------------------------------------------------------------------------- |
| id           | (_mandatory_) Unique ID.                                                                      |
| aoe          | (_optional_) Object, see [aoe](#aoe) for fields                                               |
| trail        | (_optional_) Object, see [trail](#trail) for fields                                           |
| explosion    | (_optional_) Object, see [explosions](/mod/json/reference/misc/explosions/#fields) for fields |
| do_flashbang | (_optional_) Do flashbang effect                                                              |
| do_emp_blast | (_optional_) Do Emp effect                                                                    |

#### Aoe

An effect centered on the target

| Identifier        | Description                                                                 |
| ----------------- | --------------------------------------------------------------------------- |
| field_type        | (_optional_) Id of field to use in the effect                               |
| intensity_min     | (_optional_) Minimum intensity of field                                     |
| intensity_max     | (_optional_) Maximum intensity of field                                     |
| radius            | (_optional_) Radius of effect                                               |
| radius_z          | (_optional_) Z axis radius of effect                                        |
| chance            | (_optional_) Percent chance for added field                                 |
| size              | (_optional_) Rough size of the aoe                                          |
| check_sees        | (_optional_) Source has to see location for effect, effects below condition |
| check_sees_radius | (_optional_) Source has to see any spot in the radius                       |
| check_passable    | (_optional_) Target must have to be on a passable tile                      |

#### Trail

All effects are applied in a trail following the projectile's path

| Identifier    | Description                                   |
| ------------- | --------------------------------------------- |
| field_type    | (_optional_) Id of field to use in the effect |
| intensity_min | (_optional_) Minimum intensity of field       |
| intensity_max | (_optional_) Maximum intensity of field       |
| chance        | (_optional_) Percent chance for added field   |

### Example

```json
{
  "id": "MININUKE_MOD",
  "type": "ammo_effect",
  "aoe": {
    "field_type": "fd_nuke_gas",
    "intensity_min": 3,
    "intensity_max": 3,
    "chance": 100,
    "radius": 24,
    "radius_z": 0,
    "size": 1,
    "check_passable": true,
    "check_sees": true,
    "check_sees_radius": 3
  },
  "trail": { "field_type": "fd_acid", "intensity_min": 1, "intensity_max": 3, "chance": 66 },
  "explosion": {
    "damage": 3000,
    "radius": 20,
    "fragment": {
      "impact": { "damage_type": "heat", "amount": 3000, "armor_multiplier": 2 },
      "range": 24
    }
  }
}
```
