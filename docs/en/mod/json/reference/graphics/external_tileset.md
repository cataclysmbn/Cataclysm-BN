# External Tilesets

`data/json/external_tileset` is used for content specific to Bright Nights which are not presently
in primary tilesets such as UDP or Ultica, for which `looks_like` is inadequate to depict the
content to a satisfactory degree. It functions using the `mod_tileset` feature, applying any desired
new sprites or overriding sprites to the specified tileset.

The primary advantage of this method is to set aside sprites so they won't interfere with updating
the tilesets themselves (as their sources are in the repositories of the tileset authors), and
likewise ensuring anything specific to BN won't be accidentally erased, like it would if a tileset
update was submitted to BN after these editing these sprites our versions of the tilesets. It also
covers divergences in content were, by nessecity, a different sprite is a better fit.

Links to relevant pull requests, for content covered below:

- Patchwork Skin port from DDA: [#1298](https://github.com/cataclysmbn/Cataclysm-BN/pull/1298)
- Bullet animations: [#1861](https://github.com/cataclysmbn/Cataclysm-BN/pull/1681)
- Steam turbine: [#2815](https://github.com/cataclysmbn/Cataclysm-BN/pull/2815)
- Wooden shields, riot shields, and ballistic shields:
  [#2851](https://github.com/cataclysmbn/Cataclysm-BN/pull/2851)
- Lead sling bullet: [#2709](https://github.com/cataclysmbn/Cataclysm-BN/pull/2709)
- Leather and banded shields: [#2856](https://github.com/cataclysmbn/Cataclysm-BN/pull/2856)
- Edits to blueberry bushes, strawberry bushes, and cherry trees:
  [#2861](https://github.com/cataclysmbn/Cataclysm-BN/pull/2861)
- De-obsoleted wooden greatbow, reflavored compound/composite greatbow:
  [#2862](https://github.com/cataclysmbn/Cataclysm-BN/pull/2862)
- Katar and sai: [#2715](https://github.com/cataclysmbn/Cataclysm-BN/pull/2715)
- Utility light can be switched off:
  [#1003](https://github.com/cataclysmbn/Cataclysm-BN/pull/1003)
- Mi-go nerve cluster: [#1962](https://github.com/cataclysmbn/Cataclysm-BN/pull/1962)
- Scaled bear: [#1371](https://github.com/cataclysmbn/Cataclysm-BN/pull/1371)
- Buckler and welded shield: [#2878](https://github.com/cataclysmbn/Cataclysm-BN/pull/2878)
- Battle masks and bronze arm guards:
  [#3221](https://github.com/cataclysmbn/Cataclysm-BN/pull/3221)
- Rewired street lights: [#3273](https://github.com/cataclysmbn/Cataclysm-BN/pull/3273)
- Alternative ear/tail mutation: [#3340](https://github.com/cataclysmbn/Cataclysm-BN/pull/3340)
- Iron ore from bog iron: [#3506](https://github.com/cataclysmbn/Cataclysm-BN/pull/3506)
- New trees: [#3626](https://github.com/cataclysmbn/Cataclysm-BN/pull/3626)
- Alternative sign sprite: [#3670](https://github.com/cataclysmbn/Cataclysm-BN/pull/3670)
- M1874 Gatling gun: [#3815](https://github.com/cataclysmbn/Cataclysm-BN/pull/3815)
- New traps: [#3939](https://github.com/cataclysmbn/Cataclysm-BN/pull/3939)
- New monster: [#4182](https://github.com/cataclysmbn/Cataclysm-BN/pull/4182)
- Furniture form of utility light:
  [#4780](https://github.com/cataclysmbn/Cataclysm-BN/pull/4780)
- Knocked-down version of steel target:
  [#5361](https://github.com/cataclysmbn/Cataclysm-BN/pull/5361)
- Flagpoles: [#5363](https://github.com/cataclysmbn/Cataclysm-BN/pull/5363)
- Vehicle-mounted Flags: [#5372](https://github.com/cataclysmbn/Cataclysm-BN/pull/5372)
- Pirate Flag: [#5375](https://github.com/cataclysmbn/Cataclysm-BN/pull/5375)
- Makeshift cannons and canister shot:
  [#5398](https://github.com/cataclysmbn/Cataclysm-BN/pull/5398)
- Harvested cattails: [#5445](https://github.com/cataclysmbn/Cataclysm-BN/pull/5445)
- Niter beds: [#5446](https://github.com/cataclysmbn/Cataclysm-BN/pull/5446)
- Scaleskin armor, tooth and bone weapons:
  [#5466](https://github.com/cataclysmbn/Cataclysm-BN/pull/5466)
- Reimplemented bone armor: [#5646](https://github.com/cataclysmbn/Cataclysm-BN/pull/5646)
- More tooth and bone weapons, triffid weapons:
  [#5712](https://github.com/cataclysmbn/Cataclysm-BN/pull/5712)
- Treetops ported from DDA: [#5167](https://github.com/cataclysmbn/Cataclysm-BN/pull/5167)
- Skateboards ported from DDA: [#5849](hhttps://github.com/cataclysmbn/Cataclysm-BN/pull/5849)
- Ability to weld shut doors: [#6182](https://github.com/cataclysmbn/Cataclysm-BN/pull/6182)
- Scouting visor: [#6687](https://github.com/cataclysmbn/Cataclysm-BN/pull/6687)
- Skeletal lich and master ported from DDA: [#6831](https://github.com/cataclysmbn/Cataclysm-BN/pull/6831)
- New elite zombies: [#6854](https://github.com/cataclysmbn/Cataclysm-BN/pull/6854)
- Flipping tables/benches for cover: [#6857](https://github.com/cataclysmbn/Cataclysm-BN/pull/6857)
- Arrow slits/fortifications: [#6864](https://github.com/cataclysmbn/Cataclysm-BN/pull/6864)
- Adobe flooring: [#6864](https://github.com/cataclysmbn/Cataclysm-BN/pull/6885)
- Lakebed content: [#6903](https://github.com/cataclysmbn/Cataclysm-BN/pull/6903)

## Undead People

The following is a current list of sprites this folder adds to the UDP tileset, in which files and
for what purpose. This is currently the only tileset with external_tileset coverage, but sprites for
Ultica are planned for the future.

### External_Tileset_DP_Normal.png

- Furniture form of steam turbines, to be deconstructed for advanced steam engine. Content specific
  to BN.
- Animation effects for bullets in flight. Feature specific to BN.
- Wooden shield, including worn and wielded. Item specific to BN.
- Riot shield, including worn and wielded. Item specific to BN.
- Ballistic shield, including worn and wielded. Item specific to BN.
- Lead sling bullet. Item specific to BN, `looks_like` of `rock` did not seem fitting
- Leather-reinforced shield, including worn and wielded. Item specific to BN.
- Large leather-reinforced shield, including worn and wielded. Item specific to BN.
- Banded shield, including worn and wielded. Item specific to BN.
- Large banded shield, including worn and wielded. Item specific to BN.
- Fixed worn sprite for `woodgreatbow`, as it had a sprite error in its UDP version.
  `Note: this can be removed once UDP is updated in BN, as the fix has been published in the source repository.`
- Overriding sprite for `compgreatbow`, as its version in BN is modeled after a composite bow
  instead of a compound bow.
- New worn sprite for `compgreatbow`, as compound greatbow in DDA can't be worn thus UDP lacks a
  worn sprite for it (which we'd liklely need to edit to resemble composite bow anyway).
- Katar, including sprite for wielded state. Item specific to BN.
- Sai (weapon, not to be confused with telecoms location), including sprite for wielded state. Item
  specific to BN.
- Welded shield, including worn and wielded. Item specific to BN.
- Buckler, including worn and wielded. Item specific to BN.
- Battle masks, iron and bronze, including worn sprites. Items specific to BN.
- Bronze arm guards, including worn sprites. Item specific to BN.
- Cacao pods. Item specific to BN.
- Override for sign sprite. Removes lettering on the front specific to DDA.
- Makeshift perimeter alarm. Trap specific to BN.
- Lepidopterid. New monster specific to BN.
- Furniture version of utility light. Furniture specific to BN.
- Knocked-down steel target. Furniture specific to BN.
- Jolly Roger, item and worn sprite. Item specific to BN.
- Makeshift cannon, item and vehiclepart sprite. Item specific to BN.
- Explosive cannonshells and sprites for readied cannon ammo. Items specific to BN.
- Harvested state for cattails, including winter variation. Furniture specific to BN.
- Armor made from reptile scutes, weapons made from heavy bones, teeth, and insect stingers. Items
  specific to BN.
- Sprites for bone cuirass and greaves. Reimplementation of previously-obsoleted bone armor specific
  to BN.
- Additional tooth and stinger weapons, including triffid stinger weapons. Items specific to BN.
- Scouting visors, item specific to BN.
- Cadaver assembler, zombie supply sergeant, banshee, stormbringer, shroud weaver, and shock vortex. Monsters specific to BN.
- Flipped variants of benches and tables. Furniture specific to BN.
- Iron ore, item ported from DDA we lacked a suitable sprite for.
- M1874 Gatling gun, including wielded sprite and vehicle turret. Item specific to BN.
- Patchwork Skin, ported from DDA but our version of UDP lacked a sprite for it.
- Skateboard, item and vehicleparts. Port from DDA lacking sprites in UDP.
- Sea scooter, item and vehiclepart. Item unique to BN.
- Skeletal lich and skeletal master. Port from DDA that UDP lacked a sprite for.
- AR-10.

### External_Tileset_DP_terrain_normal.png

- Rice paddies. Terrain specific to BN.
- Niter beds. Terrain specific to BN.
- Swapped spring and summer sprites (the latter not depicting young berries) for blueberry bush, as
  harvest season has been moved in BN.
- Welded-shut metal doors including peephole, bulkhead, lab door, and bar doors. Terrain specific to BN.
- Fortifcations carved into wood, log, stone, and brick walls. Terrain specific to BN.
- Adobe floors, terrain specific to BN.
- Moss and tree trunks on lake bed surface. Terrain specific to BN.

### External_Tileset_DP_Tall.png

- Off state for utility light. Ability to switch on and off specific to BN.
- Alien nerve cluster, furniture added to mi-go locations in BN.
  `Note: this can be removed once UDP is updated in BN, as the sprite for this has been published in the source repository.`
- Scaled bear, including corpse. Monster specific to BN.
- Cherry tree uses summer sprite (plus cherry blossom coloration), summer sprite depicted without
  berries, due to harvest season being moved in BN.
- Rewired street light, including active state. Furniture specific to BN.
- Cocoa tree. Terrain specific to BN.
- Coca plant. Terrain specific to BN.
- Grid floodlights, furniture based off now-deconstructabble utility lights, furniture added to BN.
- Metal and wooden flagpoles (including with flag raised), furniture added to BN.
- American flag displayed as a vehiclepart, specific to BN.
- Flagpole furniture and vehiclepart for Jolly Roger, content specific to BN.
- Treetops. Terrain ported from DDA, but which had no sprites available in UDP at present.

### alternative_mutation_tileset.png

![](../../../../../../../assets/img/external_tileset/mutation/after.png)

<details><summary>Before</summary>

![](../../../../../../../assets/img/external_tileset/mutation/before.png)

</details>

contains alternative sprites for:

- `FELINE_EARS`
- `LUPINE_EARS`
- `MOUSE_EARS`
- `CANINE_EARS`
- `TAIL_FLUFFY`
- `TAIL_STUB`
