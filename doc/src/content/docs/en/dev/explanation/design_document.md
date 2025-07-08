---
title: Design document
---

See [Game Balance](../../mod/json/explanation/game_balance.md) and
[Lore](../../game/lore/factions.md) for more detailed discussion coresponding to particular elements
of the CDDA design document. This document covers the core philosphy of CBN.

One of the primary priorities of Cataclysm Bright Nights is fun.

Gameplay is more important than realism.

Realism isn't inherently wrong. But it often comes at the expense of the new player experience. So
the fundamentals of Game Design are a priority for Cataclysm Bright Nights.

Depth is preferable to complexity. But complexity is valuable as long as it isn't overwhelming. See
porting principles 3 and 5. Complexity generally becomes overwhelming when there are too many things
to track at the same time to be able to tell what is important. So then you have to put in more work
than you want to, to understand and filter the information down to what matters.

Generally a cap should be set for how many things are displayed or being dealt with simultaneously.

Complexity can be managed:

- By having the most important info visible at a glance.
- Making the most important menus more apparent and accessible.

## General rules

There are six principles applied for content in general:

1. Things that are useless should be either made useful or removed.

2. Weird/wonky mechanics should be either reworked or removed. If there is value in retaining them,
   they should be clearly communicated to the player through UI.

3. Each mechanic/item/monster/etc. should fill its own niche or have its own purpose, e.g. there
   shouldn't be many different effects that penalize the character in the same way, or a dozen of
   guns that differ only in name and +1 to damage value. If such variety is desired, it should be
   relegated to mods.

3.1 Clutter benefits nobody. If these additions are just more bloat, mainline mod it.
Examples are guns, books, food, drugs, morale items, furniture variants, clothes.
Opt-out is preferred for additional bloat, and Opt-in for things that change game theme.
e.g. Magical Nights, Aftershock, CRIT. When in doubt, ask.

4. The gameplay should be less about pressing buttons and spending time in submenus and more about
   interacting with the surrounding world and progressing (achieving results).

5. Different play styles should be balanced against each other, with their own upsides and
   downsides, so that you can actually choose a different play style and have it not suck just
   because "realistically, firearms are better than everything else", but actually be playable and
   have a different feel to it.

6. Rewards should be proportional to amount of effort spent on achieving them (e.g. weapon with
   harder craft recipe, or one that requires rarer components, or rarer ammo, should deal more
   damage).

### Reasoning

1. useless things only serve to clutter interfaces and item lists. increasing complexity for little
   value. Often strategically removing things can add more value than an addition.
2. weirdness is a sign that things are unintuitive. Somethings benefit from not being obvious. But
   things that are too far from people's expectations break the suspension of disbelief and require
   people to remember more disconnected elements.
3. More items means more interactions, more depth, but finding what you want or understanding
   what items are good when we have 30 AK pattern rifles and 30 AR-15 platforms.
   Distributions are also cluttered badly from increased variants. This is bad for gameplay.
   A more believable world has 10,000 firearm variants, a more playable world has 10.
   If in doubt, Add your new items as a mod package.
4. Sorting through complexity in menus isn't fun. menus are only fun when there are meaningful
   choices or tradeoffs being made there. (I.E. what to equip, what to carry, what to loot first,
   etc.)
5. and 6. are kind of common sense applications of prioritizing gameplay.

### Removals and re-adding

Removal of content is a contentious problem in the community. To clarify official stance:

- Content should not be kept inside the mainline on the basis of "it can be made good".
  If it isn't good now, it isn't good. Shift it to a mainline mod, or improve/obsolete it.
- Content should not be re-added to mainline on the basis of "it was present before".
  Re-added content should be treated the same way new content is.
- Content should not be "spared" from shifting/obsoletion by giving it forced updates that still
  don't make it good enough. It should be treated like new content is.
- This doesn't cover arguments regarding content being thematic, liked etc., content can still be
  given extra "points" based on those. We want a level of flavor to the world.

In short, if it doesn't feel like a good "default" experience, improve or shift to a mod.

## Depth

Depth occurs primarily from interactions between mechanics and different elements of the game. For
example, a common scenario is:

enemies spot you close, what do you do? The general options are: run away quickly.(Where to?) move
to a good spot and fight them. kite them at location.

But the exact details change the preferable answer. those factors bring depth into that decision and
interesting gameplay. As well as playing into resource management elements that are classic fun
elements of roguelikes.

Some factors here are: damage you deal vs. enemy resilience and numbers (How long to kill? Can I
kill?) How much cost do you take from a fight? How far/fast can I run vs. them (Speed + stamina)
terrain nearby.(terrain speed slowdowns. damage terrain, chokepoints, etc.) other clusters of
enemies. How much benefit is there to a fight? (zombie drops + nearby loot locations that are now
safe)

## Bloat

Bloat happens because we all want to add things that we personally like. But we can't add everything.

The rule on bloat is to save it for opt-out/opt-in mods, or third party mods.

Bloat is when there is no meaningful reason to have something in the game, A gun is a gun.
Clothes are clothes. We don't want 30 variations of the same T-Shirt but colored differently inside
the base game. Now you can't find what you want.

There is a difference between bloat, and flavor. Flavor is adding a missing cooking recipe.
We've historically not represented much European/African/Asian cooking. Some recipes exist,
Such as lutefisk and haggis, but a lot of recipes are generic or common American fare.

Bloat is adding rechambered versions of firearms, example being Vektor variants.
Another example would be adding 30 variations of the bible, or 30 novels. Put it in an expanded literature mod, or a religion mod in the case of religious books.
We don't need 20 types of beer, it just makes it harder to parse the comestibles menu. Put it in a (craft) brewery mod.
We don't need clothes whose only distinction is a different player sprite. Put it in a clothing/fashion mod.
