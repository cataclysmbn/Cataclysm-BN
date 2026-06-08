# Balancing Concepts

This will summarize many of the things that govern gameplay decisions within Arcana and how certain properties are determined, especially for spellcasting.

## Energy

Essence types have specific values of energy associated with them, and spellcasting consumes resources based on certain assumptions about which equals what.

### Essence

* 1 unit of dull essence is assumed to possess 5 kJ of energy.
* 1 unit of blood essence is assumed to possess 50 kJ of energy, making it equivalent to 10 units of Dull Essence.
* 1 unit of regular essence is possesses 150 kJ of energy. It's equivalent to 3 Blood Essence, or 30 Dull Essence.
* 1 unit of crystallized essence is 1500 kJ. It equals 10 essence, 30 blood essence, or 300 Dull Essence.

Whenever possible, any recipe or other method that converts one type of essence into another, from crafting crystallized essence to consecrating magic items for dull essence, is done on a 1:1 basis. Converting essence into other types should never cause a gain or loss of energy. Converting other forms of energy, like electricity, into essence can be less efficient, the methods present in the mod are at 50% efficiency, but any method that converts essence into electrical energy will do so at a 1:1 ratio.

Power cost for crafting and fueling magic items is not exact and is generally balanced relative to how effective it should be balance-wise rather than trying to determine how many kJ of energy it should take up, but power draw of different essence types relative to comparable items is generally consistent. Many magic items, especially if they cast a spell on activation, will at least try to mimic a Magic Sign of about mid-level power based on however much energy the action consumes, but this is a recommendation for balance and not applied rigidly. When balanced in this manner, 1 unit of essence counts as enough power to cast a mid-leveled spell of Spell Rank (see below) 3, 2 essence would power a spell with an SR of 7, 3 would power a mid-level spell equivalent to SR 11, etc (energy costs are multiplied by Spell Rank plus 1).

Whenever multiple types of essence are allowed in a recipe however, they will always use consistent amounts of each based on how much energy they have relative to each other. Meaning, for example, if a recipe requires 2 units of essence, it will require 6 blood essence if that's allowed as an alternative, or likewise 60 dull essence.

Magic ranged weapons tend to have direct damage that equals 50% of the kJ their essence cost per shot represents, comparable to the average for most vanilla laser weapons.

### Spell Energy

The energy demands for spells are based on the following assumption:

```
1 HP = 2 fatigue = 10 mana = 10 kJ = 100 stamina
```

The source code contains an implied conversation rate for HP, mana, and stamina in one of the functions (specifically, how hand encumbrance affects energy cost of spells that lack the `NO_HANDS` spell flag). The equivalence between mana and kJ is directly evident due to how bionic power reserves are hardcoded to affect mana capacity.

Fatigue is trickier to pin down, and is largely an arbitrary assumption based off Tired status being a reasonable point where a player would probably cease spellcasting, with 200 being a usefully round number not that far past that point. Given Arcana no longer uses fatigue as a primary casting resource (as only Bright Nights retains compatibility with the feature), getting an exact and perfectly accurate value is not really essential, making it more of a guideline for the few cases where fatigue does show up as a factor in spells.

The exact cost of any given spell is affected by other factors such as what sort of spell it is (Magic Sign, Arcane Blessing, or Sanguine Mark), how much it's been leveled up, and the spell's Spell Rank (see below).

## Spellcasting

Every spell in the mod has what's known internally as a *Spell Rank*. This is an arbitrary measure of how powerful the spell is intended to be relative to other spells. While the number itself is selected based off what feels proper for the spell's intended role, its uses are more consistent, used to mathematically determine properties such as energy cost, damage, etc.

You can see Spell Rank present in every spell, as the spell's difficulty rating is always ten times that number. For Magic Signs, a type of spell generally tied to a specific trait, the point cost of the associated mutation will generally be half the spell's Spell Rank, rounded up if needed. Difficulty is so elevated to increase the experience points per cast to a reasonable amount, with the `NO_FAIL` spell flag used to make it have no impact on failure rate (spell failure also being unused in Arcana due to the tedium added by it).

The three different types of spells in Arcana have different multipliers that use Spell Rank, and different degrees to which they scale up from level zero up to their maximum spell level. Spells in Arcana are generally mid to late game content, requiring effort to obtain unless you spent points to start with them (and that's only an option in scenarios specific to Arcana, or certain professions).

Spells tend to be fairly powerful relative to other magic mods as a consequence, as they're expected to be useful immediately once obtained. Because the spellbook mechanic is hardcoded to level up the Spellcraft skill (which is only defined in the JSON for Magiclysm), book-grinding is not an option for spells in Arcana. This is why casting experience is elevated via difficulty, as it's the only option left to the player.

Notes on the properties listed for each type of spell:
* Energy Cost is self-explanatory, how much of its primary casting resource it consumes per cast. This will always be halved at maximum level, regardless of the type of spell.
* Spell Exertion is a secondary effect used by Magic Signs and Sanguine Marks. Basically it adds a secondary resource cost, as stamina-costing is otherwise something players could spam with impunity, and in the case of Sanguine Marks some characters can potentially render HP-casting trivial too. It's explained further below. The amount of Magic Sign Exertion or Draconic Exertion given per cast will always be consistent relative to the spell's energy cost, so it will be halved at max level.
* Casting Time is how long it takes to cast the spell in moves (effectively centiseconds). This will always be halved at max level.
* Total Damage concerns how much damage offensive spells deal. Most attack spells in Arcana are AoE rather than single-target. To counterbalance this, in BN the `DIVIDE_DAMAGE` flag is used for AoE spells, splitting total damage depending on how many targets were hit, while in DDA half the expected damage will instead be shifted to subspell with a smaller radius. Total Damage is also used for Magic Sign: Healing and Magic Sign: Capacitance, two spells that restore a given resource (HP and bionic power).
* Protective Buffs concern any status effect given by a spell designed to block certain damage types or negate status effects, concerning a range of fairly powerful and useful defensive spells. These are generally short-term buffs meant to whether a hazard briefly or tackle a particularly threatening enemy.
* Durations for other buffs are longer, and cover various other positive status effects that provide primarily a stat increase or some utility purpose.
* Debuff durations concern most status effects intended to affect monsters and hostile NPCs, rendering them less effective for a modest length of time.
* Short-term debuffs primarily include effects that directly deal constant damage to their target, like On Fire or Corroding. This also includes mod_moves spell effects, typically used to paralyze targets. Spell effects that give the caster free turns, i.e. a timestop effect, also use this method of duration scaling despite not being a debuff.
* Duration for summoned monsters is generally much longer than other durations, but never permanent. This also covers spell effects designed to create fields as their primary purpose rather than as a side effect, like Magic Sign: Light.

### Magic Signs

Property | At Minimum Level | At Max Level
--- | --- | ---
Energy Cost | 500 stamina * ( Spell Rank + 1 ) | 250 stamina * ( Spell Rank + 1 )
Spell Exertion | 50 sec * ( Spell Rank + 1 ) | 25 sec * ( Spell Rank + 1 )
Casting Time | 0.4 sec * ( Spell Rank + 1 ) | 0.2 sec * ( Spell Rank + 1 )
Total Damage | 20 per Spell Rank | 40 per Spell Rank
Duration, Protective Buffs | 2 min per Spell Rank | 4 min per Spell Rank
Duration, Other Buffs | 6 min per Spell Rank | 12 min per Spell Rank
Duration, Debuffs | 6 sec per Spell Rank | 12 sec per Spell Rank
Duration, DoT/Paralysis | 2 sec sec per Spell Rank | 4 sec per Spell Rank
Summoning Duration | 20 min per Spell Rank | 40 min per Spell Rank

Magic Signs are the most readily-available type of spell in Arcana and Magic Items mod. Their resource consumed is stamina, making them usable repeatedly over a sustained period of time, but harder to use in the middle of a fight. Per unit conversions specified above, it consumes the equivalent of `50 mana * ( Spell Rank + 1 )` at minimum level, and equivalent to `25 mana * ( Spell Rank + 1 )` at max level.

The way their stats scale up is considered the baseline the other two types of spell are compared to, with reasonable minimum stats that grow to double when leveled to maximum. All Magic Signs have a maximum spell level of 10, in between Arcane Blessings and Sanguine Marks.

One complication to consider: Stamina is a very short-term resource, recovering far faster than mana or HP. Because of this, casting Magic Signs also inflicts a secondary cost on the user. Magic Sign Exertion is a status effect that steadily increases fatigue, at a rate of 1 every 25 seconds.

Magic Signs give out this status effect for a number of seconds exactly equaling 10% its current stamina cost. It can also be seen as a number of seconds equaling how much mana that amount of stamina is worth, if 10 stamina is assumed to equal 1 mana. Because 1 fatigue is assumed to equal 5 mana, the net effect is that casting Magic Signs also costs 20% of its energy cost in fatigue, in terms of how much mana both resource costs equate to.

### Arcane Blessings

Property | At Minimum Level | At Max Level
--- | --- | ---
Energy Cost | 60 mana * ( Spell Rank + 1 ) | 30 mana * ( Spell Rank + 1 )
Casting Time | 0.5 sec * ( Spell Rank + 1 ) | 0.25 sec * ( Spell Rank + 1 )
Total Damage | 10 per Spell Rank | 50 per Spell Rank
Duration, Protective Buffs | 1 min per Spell Rank | 5 min per Spell Rank
Duration, Other Buffs | 3 min per Spell Rank | 15 min per Spell Rank
Duration, Debuffs | 3 sec per Spell Rank | 15 sec per Spell Rank
Duration, DoT/Paralysis | 1 sec sec per Spell Rank | 5 sec per Spell Rank
Summoning Duration | 10 min per Spell Rank | 50 min per Spell Rank

Arcane Blessings are a type of spell associated with the Paragon of The Veil threshold, a late-game option for players. It consumes mana, and is the only form of magic in Arcana to do so. Spell costs are slightly higher than Magic Signs in terms of how much mana the stamina costs equate to, but lack the secondary cost the Magic Sign Exertion induces.

The stats at minimum level are half that of equivalent Magic Signs, but in exchange scale up to become 25% more effective at maximum level. Their maximum level is 20, making them slower to level up than Magic Signs.

While Magic Signs have a primary cost that is much quicker to regenerate, Arcane Blessings can benefit from various ways to increase mana capacity and regeneration, and lack any physical side effects from casting. Their slower scaling is also rewarded, not just by being more effective at max level but also via most Arcane Blessings having some side benefit that gives it an advantage over its closest equivalent Magic Sign.

### Sanguine Marks

Property | At Minimum Level | At Max Level
--- | --- | ---
Energy Cost | 4 HP * ( Spell Rank + 1 ) | 2 HP * ( Spell Rank + 1 )
Spell Exertion | 40 sec * ( Spell Rank + 1 ) | 20 sec * ( Spell Rank + 1 )
Casting Time | 0.3 sec * ( Spell Rank + 1 ) | 0.15 sec * ( Spell Rank + 1 )
Total Damage | 20 per Spell Rank | 50 per Spell Rank
Duration, Protective Buffs | 2 min per Spell Rank | 5 min per Spell Rank
Duration, Other Buffs | 6 min per Spell Rank | 15 min per Spell Rank
Duration, Debuffs | 6 sec per Spell Rank | 15 sec per Spell Rank
Duration, DoT/Paralysis | 2 sec sec per Spell Rank | 5 sec per Spell Rank
Summoning Duration | 20 min per Spell Rank | 50 min per Spell Rank

Sanguine Marks are associated with the Dragonblood mutation category, a competing late-game option for players. Its casting resource is HP, which is slower to recover and thus potentially costly despite the fairly low amounts involved. In terms of energy equivalence, its cost equals `40 mana * ( Spell Rank + 1 )` at minimum level, and `20 mana * ( Spell Rank + 1 )` at max level.

Sanguine Marks use what is effectively the best possible scaling of both Magic Signs and Arcane Blessings, having the min-level stats of Magic Signs and the max-level stats of Arcane Blessings. On top of this they have a maximum spell level of only 5, making them level up faster on top of a lower effective mana cost.

HP obviously is a much more important resource than mana or fatigue, and to some extent moreso than stamina as well. It tends to be slower to recover than any of those, though less so with good First Aid skill and medical supplies, healing traits, etc. The available spells are generally geared towards playing towards the strengths Dragonblood has, with its healing rate increase making it more feasible to cast Sanguine Marks more often over a prolonged period of time.

Like Arcane Blessings, Sanguine Marks tend to have secondary effects that differentiate them from comparable Magic Signs. While this often makes them more interesting and offer different options than Magic Signs, this often includes side effects and drawbacks that add a counterbalance to how Dragonblood is already well-suited for HP-casting, and how the positives for each spell tend to compliment the playstyle that mutation category is suited for.

However, since there are many effects in Arcana that can make HP recovery trivial, that makes HP a resource that is normally a long-term resource resource like mana, but it can become a short-term resource in the right situations. As with Magic Signs, to discourage overuse Sanguine Marks inflict a debuff that steadily increases hunger, to the same extent that Magic Sign Exertion increases fatigue.

This effect is granted at a rate equal to 10 seconds per 1 HP consumed. Just as one can interpret Magic Sign Exertion giving a number of seconds equal to how much mana its stamina cost is worth, amounting to paying 20% of the stamina cost in fatigue, so too do you effectively pay 20% of the HP cost in hunger gained, if one assumes that 1 unit of hunger (roughly 8.7 kcal) also equals 5 mana.