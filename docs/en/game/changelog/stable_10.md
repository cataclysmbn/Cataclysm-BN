## Stable 0.10.0

This summary lists the most significant changes from this release. This release includes 188 changes, including features and bugfixes.

## Gameplay

- NPC trade and dialogue interfaces were reworked to improve response selection and interaction flow
- Added a world option for initial trait purification and cancellation
- Climbing trees can trigger slips and heavy impacts can knock survivors off unstable terrain
- Vehicle idle lift now reduces drag and supports floating at ground level

## New Content

- Added cargo ship overmap specials with non-square nested mapgen support
- Mainlined additional MST Extra content into the core game
- Added aluminum boat hulls and wooden wing tiles for vehicle construction
- Added GUNSHY and Nyctophobia traits
- Added gun enchantments and recreational furniture constructions

## Balance

- Mininuke damage was increased from 300 to 3000
- CBM body slot costs were rebalanced and microreactor behavior was adjusted into atomic battery workflow
- Plutonium cells were reworked to self-charge over time and battery compartments now accept military plutonium fuel cells
- Medieval armor materials were adjusted to increase protection and autoloader power draw was reduced from 50 kJ to 5 kJ
- Extended toolset was obsoleted and its repair and cauterize functions were moved to integrated toolset

## Infrastructure

- Android now uses a user-accessible mod directory
- Bullet and thrown item sprites are now customizable through JSON
- Item variables can now be defined in JSON
- Lua now supports proper relative and absolute require paths with expanded NPC and dialogue hooks
- Save format v2 is now current and v1 is marked legacy

## Bugfixes

- Fixed flying robots to be hackable again
- Fixed crashes in training activities
- Fixed Mycus fruit-eating crashes and progression blockers that could softlock Mycus characters
- Fixed items disappearing when loading smoking racks and charcoal kilns
- Fixed vehicle ladders so they function without install-order dependency
- Fixed turret autoloaders so internal magazines reload correctly

## Ported from DDA

- Ported the relentless hulk and NEMESIS content set including its scenario
- Ported directed trap disarm changes that remove base difficulty checks

See detailed changelog [https://github.com/cataclysmbn/Cataclysm-BN/releases/tag/v0.10.0 here]
