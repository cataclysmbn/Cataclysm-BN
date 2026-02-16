# RPG System

An RPG progression mod for Cataclysm: Bright Nights that adds a LitRPG-style experience system. Should be able to be added to existing saves without breaking things.

## Features

- **Experience & Leveling**: Gain XP from killing monsters, level up to 40
- **Classes**: Choose from 4 base classes (Warrior, Mage, Rogue, Scout) and 8 prestige classes at level 10
- **Traits**: Unlock powerful traits
- **Stat Points**: Assign free stat points

## Balance

Worth ~25 character creation points. Worth much less early game, more late game. If you want to play a "vanilla balanced" experience, create a character with 25 points left at the end of character creation.

Not designed to be used with "Stats through Kills" mod.

## Screenshots

![System menu](https://cdn.imgchest.com/files/70905114f237.png) ![Class menu](https://cdn.imgchest.com/files/de1caa6782c7.png) ![Trait menu](https://cdn.imgchest.com/files/ae723f297ad8.png)

## XP Curve

Note: 40 is the max level, but you should not expect to get that far beyond level 10. A normal zombie gives around 7 XP.


Formula: **XP = 2.2387 × level^3.65**

| Level | Cumulative XP | XP Needed for This Level |
|------:|--------------:|-------------------------:|
| 1 | 2 | 2 |
| 2 | 28 | 26 |
| 3 | 123 | 95 |
| 4 | 353 | 230 |
| 5 | 797 | 444 |
| 6 | 1,550 | 753 |
| 7 | 2,720 | 1,170 |
| 8 | 4,429 | 1,709 |
| 9 | 6,807 | 2,378 |
| 10 | 10,000 | 3,193 |
| 11 | 14,161 | 4,161 |
| 12 | 19,454 | 5,293 |
| 13 | 26,055 | 6,601 |
| 14 | 34,148 | 8,093 |
| 15 | 43,927 | 9,779 |
| 16 | 55,595 | 11,668 |
| 17 | 69,364 | 13,769 |
| 18 | 85,456 | 16,092 |
| 19 | 104,099 | 18,643 |
| 20 | 125,532 | 21,433 |
| 21 | 150,002 | 24,470 |
| 22 | 177,762 | 27,760 |
| 23 | 209,075 | 31,313 |
| 24 | 244,212 | 35,137 |
| 25 | 283,450 | 39,238 |
| 26 | 327,076 | 43,626 |
| 27 | 375,382 | 48,306 |
| 28 | 428,670 | 53,288 |
| 29 | 487,246 | 58,576 |
| 30 | 551,428 | 64,182 |
| 31 | 621,536 | 70,108 |
| 32 | 697,900 | 76,364 |
| 33 | 780,857 | 82,957 |
| 34 | 870,751 | 89,894 |
| 35 | 967,931 | 97,180 |
| 36 | 1,072,754 | 104,823 |
| 37 | 1,185,584 | 112,830 |
| 38 | 1,306,791 | 121,207 |
| 39 | 1,436,752 | 129,961 |
| 40 | 1,575,850 | 139,098 |

## Dev notes

When you add a class or trait, it must be added both to `traits.json` and to `rpg_mutations.lua`.

Each base class should give a total of 1 stat point per level, and each prestige class should give a total of 1.5 stat points per level. In addition to stat points, classes will give unique bonuses.

A guideline for trait level requirements is 5 if the highest required stat is 12, 10 if the highest required stat is 16, and no level requirement otherwise.

### Cross-mod compatibility

If you want to add a new mod, you can call `add_mutation` from your own mod to register a new class or trait. See [here](https://github.com/mamick2006/Cataclysm-BN/releases/tag/release) for an example.

### TODOs

Have more interesting and dynamic stats and traits. They are all currently implemented either using `mod_` functions in lua, or using static mutation JSON bonuses. We could definitely add some more BombasticPerk style traits. Also, probably much of this is unbalanced.

Anyone who is interested should feel free to make changes!