# Arcana and Magic Items mod for Cataclysm Bright Nights

Arcana and Magic Items mod, formerly an in-repo mod during 0.C Experimental, maintained as a third-party repository by its author, and now once again restore as an in-repo mod for Bright Nights. Continually updating and expanding on its mechanics, expect regular updates.

Focused principally on magical items as the name implies (the mod predating the current spell mechanics), with proper spells being a valuable, powerful tool that demands hunting monsters and learning of the Beyond moreso than simple scavenging. Magic is a primal, recently-developed resource tied to otherworldly monsters and unearthly phenomena, with those who came before leaving an impression on the region that can be uncovered through exploration, or via pursuing missions and interactions involving the handful of survivors still around by the time of the Cataclysm.

__OPTIONAL:__ If you are using Arcana with Cataclysm++, Magical Nights, or a few other mods with Bright Nights, the Patchmods folder has optional mods that allow the content of different mods to interact in more ways. These are NOT necessary for actual functionality even if you use both mods, and just add a few extra improvements. To use, copy the patchmod subfolder desired from the Patchmods folder into your data/mods folder. Then just include it during world generation, it will automatically be loaded after the two mods to be combined, due to requiring the relevant mods as prerequisites. For DDA, mod interactions for supported mods are handled by the mod_interactions folder in Arcana itself.

Links to Third-Party Mod Repos That Have Patchmods Available:
* Cataclysm++, the scifi and innawoods content megamod by Noctifer-de-Mortem: https://github.com/Noctifer-de-Mortem/nocts_cata_mod

## Arcana
* A new skill, which I'm surprised I got working. It's used as the primary crafting skill for magical items. Thanks to a PR by BevapDin, magic weapons can now use this skill.

## Essence
*  Regular *essence* drops from slaying several different sorts of anomalous creatures, the more otherworldly the better. Main use is for creating most enchanted items. Certain magical items are also fueled by this.
* *Blood essence* is drained from non-anomalous beings, using anomalous means. Some magical ranged weapons and items are fueled by this, and it can be used as a less powerful alternative to essence. Certain monsters, generally undead that are earthly but anomalous, also drop this instead of normal essence.
* *Dull essence* is obtained by dismantling magic items and essence using the correct tools. Used to craft and fuel certain items, generally specific to the *Cleansing Flame*.
* *Crystallized essence* is something you'll have to craft yourself, used for a handful of specialized, high-yield purposes...

## Scenarios and Professions

##### Seeker of The Arcane
A scenario added to allow players a good way to dive right into Arcana. In it you are a visitor to one of the forgotten locations where practitioners of the arcane once lived. Each has an assortment of items, monsters, a powerful enemy at the end, and a unique item to be retrieved.

* The *Apprentice* is a mere baseline class used as something with no special talents or point costs, but still starts with a bare minimum to get into the first level of arcana.

* The *Alchemist*, *Scribe*, and *Summoner* are mainly utility professions, designed to easily get into the basics of their chosen craft. None of them are especially combat-ready, though the summoner stands a reasonable chance with its initial stock of minions.

* The *Blood Mage*, *Mage Hunter*, and *Dark Priest* each are a remnant of a given group, and each has come relatively prepared for battle. One of the __Sanguine Order__, one of the __Cleansing Flame__, and one of the __Keepers of The Oath__.

##### Urban Awakening
The second scenario added, allowing players another method of getting into the arcane, in more urban scenario. It allows an assortment of professions in the game already, Apprentice, and a few professions.

* The *Arcane Researcher* stole away with a small collection of arcane books, with nothing else but the clothes on their back.

* The *Sanguine Shrike* is a leftover member of the __Sanguine Order__, serving a different role compared to the *Blood Mage* from Seeker of The Arcane, while the *Dragonblood Aspirant* is a novice within sanguinist altered by their study of research left behind by said Order's more transhumanist-leaning sects.

* The *Arcane Purifier* and *Arcane Operative* represent the urban contacts and splinter factions among the __Cleansing Flame__, combining technology and a few basic arcane tools of the trade.

* The *Keeper Menicant* is a wayward follower of the __Keepers of The Oath__, taking up arms and left adrift after the destruction of their order.


## Item Dismantling

* The *Hammer of the Hunter* is needed to dismantle most arcane items. Some unique items are even more complex and difficult to get rid of, while a few are simpler to dispose of.

* *Cleansing Flame* items can be taken apart for *blood essence* using the tools of the __Sanguine Order__.

## Stories Lost in Darkness: NPCs and Missions

In addition to discovering items and other hints as to what happened in the margins of society, in the days prior to and just after the Cataclysm, the player can uncover remnants of these near-extinct factions here and there. Some may be willing to work together with you, while others may be only one more threat to deal with.

In contact with the *Refugee Center*, a small holdout of the *Cleansing Flame* has established shelter in what was once a rural church. By seeking out the right people you can be led into contact with them, enabling missions to assist in their survival. And, perhaps, seeking out other allies along with getting a better understanding of what went on between these warring factions, along with a chance to finish unfinished business.

In addition, though the *Keepers of The Oath* had been driven nearly to extinction, at least one final member of their faith remains. Having since given up hope, it may be possible to encourage them to guide you along a path that may help humanity's chances of surviving. At least, that's what their order believes. Whether there's any truth to it is something no survivor of the cataclysm itself will be around to see, given the long-term machinations of those __Beyond__...

## Monster Tweaks
* A wide range of monsters have been edited to allow obtaining essence or blood essence. A few can also drop magic items.
* In addition, certain monsters will drop various small odds and ends usable in crafting magic items. These are relatively non-magic leftovers with a touch of the otherworldly to them, and hold the path to obtaining skill in the arcane without access to the work of those who came before.

## Books
##### Apprentice's Notes
* Entry-level 0-to-2 skillbook. Implied to discuss magical concepts using religious metaphors. Can additionally be used to get an earlier grasp on the few recipes that are autolearned.

##### History of Alchemy
* For crafting potions, and for getting from levels 0 to 3. A madman's interpretation of old alchemical texts, with results that would be profoundly useless without magical essence. You get a cookie if you notice the references to historical alchemical concepts, hidden underneath the stranger things the recipes involve.

##### The Six Pillars 
* Levels 2-5, requires a decent intellect to understand. Used to craft disposable magic scrolls. Conceals magic lessons behind myths concerning a pantheon of otherworldly gods.

##### Sanguine Codex
* Blood magic, for levels 3-6. Allows crafting the silver athame, and producing blood essence, and making the various items used the __Sanguine Order__.

##### The Cleansing Flame 
* Levels 4-7. Religious book, written by an order of witch-hunters, it allows crafting a select number of sanctioned items fueled by dull essence.

##### Oaths to the Chalice 
* Also a religious book, skill levels 5-8. Allows crafting the offering chalice, recipes for sacrificing items to obtain essence, and magic items related to __The Keepers of The Oath__. Also needed to extract blood essence from a used chalice.

##### To Master the Unknown 
* Levels 6-10. A book for summoning your very own pet monstrosities.

##### A Story in Shadow 
* Levels 6-10. Relatively rare book that combines some of the concepts from the three major factions. Translation: a few magic items with an assortment of useful effects.

##### Lab Journal-Quinn
* Levels 5-8. The end result of rare research combining cutting-edge science with a glimpse of the unknown and anomalous, providing acess to many recipes involving magitech.

##### Strange Map 
* Technically not a book but instead a mapping item, revealing most anomalous locations.

##### The Path of Sword And Hammer
* A rare book, rather than teaching magic it instead teaches a martial art, combining understanding of the arcane with cold steel.

##### The Path of Scourging Claws
* Another, alternative martial art that directs arcane knowledge to enhance claws and bladed weapons.

##### Z-07 Engraved Plate
* Technically a book, a late-game one tied to a mission at that. If you do make it that far along, it may prove useful.

## Spellcasting

One might happen to find scrolls of various sorts, or see hints of their recipes as your learn more about the arcane. If one hunts down monsters of otherworldly sort, you'll find various parts torn from their bodies, at first glance just exotic keepsakes to carve into morbid trophies. But with the right knowledge, each fragment carries anomalous properties associated with the __Beyond__ that can be exploited. At first just a single-use surge of power, later those tokens of power can be used to create *pattern scrolls.*

Activating these scrolls will grant the user the ability to cast the spell associated with the monsterpart used to make it. Such spells are reasonably potent at low level, designed so that it's properly feasible to level them up via casting, and putting in the effort will be well-rewarded. Very few of them are traditional offensive spells, instead most are tools, focused towards surmounting a specific obstacle or filling a certain niche. These fatigue body and mind rather than consuming mana, expending a burst of stamina at first, with a prolonged exertion wearing down fatigue to discourage overuse.

Some of these pattern scrolls can even be found, though rarely and only in places where arcanists once practiced. Arcana favors craftable sources of spells, rewarding effort and risk rather than simply happening upon a spellbook via good RNG. Some of these pattern scrolls are even crafted in a more unusual manner, via taking a different type of scroll that serves a different purpose, and altering the magic it represents into something new, unknown until after the Cataclysm.

Later in the game, a well-prepared player can explore two competing paths can lead to entirely different forms of magic to learn. By restoring a lost relic, one can earn a varied skillset of powerful spells that properly draw on mana, geared towards surviving and furthering the goals of their new benefactor. Alternatively, one can complete a half-finished transhumanist experiment, gaining powerful mutations and learning spells that cast from HP, each of which is geared towards the combat-oriented role their mutations will nudge them towards.

## Tools of The Trade

##### Offering Chalice
* Used for a few recipes from *Oaths to the Chalice*, as you could surely guess. Trying to take a swig from it is a bad idea, but needed to fulfill one path to crafting blood essence.

##### Silver Athame
* Used for a few recipes in *Sanguine Codex*. Only a mere 3 butchering quality. Still sharp enough, so don't play around with it. Or do play with it, that's exactly what the authors of the Codex want you to do.

##### Hammer of the Hunter
* Weapon of choice for magiphobes the world over. Required for uncrafting most magical items. Also can be used to blind enemies, and yourself if you aren't careful.

##### Blank scrolls
* Used to craft magic scrolls, obviously. With other books, certain other things can be crafted with them.

##### Silver Glyph
* On its own it's merely a shiny piece of silver. But with the right magic, quite a few monsters will be yours to command.

## Major Items

##### Sanguine Codex
* Can teach the reader to create items linked to the __Sanguine Order__. Aside from the athame itself this can include a *Hellfire Stave* that shoots fire, the *Veinreaver* as an explosive weapon, the *Revenant Crown* that protects against poisons and other effects, or *Wyrmskin Armor* capable of conjuring sprays of acid.

##### The Cleansing Flame
* This book focuses on the items used by the mage-hunting order of the same name. While the hammer itself is both a weapon and tool, there's also the *Incorruptible Sword* that can bring fire to enemies in close combat, the *Wraithslayer Crossbow* with potent bolts of piercing light, the *Gilded Aegis* for healing, or the *Mask of Insight* to gain clairvoyance and protection from the hammer's blinding flare.

##### Oaths to The Chalice 
* Focuses on the items associated with The __Keepers of The Oath__, in addition to its uses for the chalice itself. Items include the lightning-shooting *Symbol of Judgement*, invisibility-granting *Mantle of Shadows*, the *Spear of Pestilence* which can conjure a host of mutated vermin, and *Thunder Gauntlets* that let you conjure lightning.

## Other Magical Items
* Certain monsters drop specific items, as mentioned above. Initially the only use for them is to carve and craft these things into decorate items. While not very useful in and of themselves, working with trophies taken from the __Beyond__ can enlighten those with no formal arcane skill.
* With the base items themselves and a little arcana skill, magic seals can be crafted. These entail a little essence and some extra effort put in preparing the item, turning it into essentially a single-use spell. For all the items usable, each has a specific effect thematically linked to it.
* At higher levels, and with the right books, magic seals can be woven into an effect bound to a scroll, allowing the user a shot at learning the corresponding ability permanently. Doing so doesn't always succeed.
* Fire. Water. Earth, and Air. While study of the __Beyond__ unlocks abilities of often inhuman nature, a desperate novice arcanist can use their newfound experience for more earthy, primitive purposes. Using these essences, or the closest available effigy in times of desperation, one can work the most basic of miracles.

## Artifacts
* Certain locations offer unique items that are otherwise uncraftable. Most notably are those hidden away in the forgotten halls of arcanist groups, how have since succumbed to the cataclysm and its horrors. Two of them tie into the background of what led to their downfall, while two are tied to the monsters that infest the place.
* In addition, hidden away one can find a few other lesser artifacts, each having their own potential uses.
* Two notable unique artifacts can be crafted, leading towards what's effectively endgame content, unlocking ways to become something more than human. By obtaining an item hidden away in a temple over the sky, one can restore its true nature and gain its power. The second meanwhile entails a shrine surrounded by water, and finishing what the *Sanguine Order* started.

## Other Magic Items
* Using *History of Alchemy* an assortment of potions can be made, a mimicry of the Opus Magnum. Each step has its own use however, from numbing pain to enhancing strength. Later on, combinations of previous potions can be used to make items with more specific effects.
* Scrolls with an array of magical effects can be made using *The Six Pillars*. Each has a specific effect, though many are a double-edged sword in some way.
* *To Master The Unknown* allows binding summoning magic into silver glyphs, to summon an assortment of monsters of various strengths. Beware the consequences of the summoning attempt failing, though.
* With *A Story in Shadow* a few extra items are available. The *Druidic Stave* can be used to mend a selection of materials, including some that are otherwise not repairable. The *Orb of the Veil* is a tool that can enhance the mind and scry the surrounding terrain, while the *Satchel of Eternity* harnesses the power of the __Beyond__ for more mundane utility.

## Furniture
* An upright sword, currently serves as an easily-removed barrier and a Dwarf Fortress reference.
* The *Candle of Warding*, also serves as a barrier from the unholy when activated, which can be crafted.

## Misc
* Added some overrides to make new monsters mutually friendly to both factions of monster that spawn in its area.



# **Here there be spoilers.**



## The Structures and their artifacts:

##### Strange Grove
*Before the major factions of arcanists had gained the power they held as the Cataclysm loomed closer, there were the patient scholars of the Beyond. They used little more than glimpses through the cracks of the Veil, and slowly advanced their studies. Then one day, they were scattered to the four winds. The decaying, overgrown structure has left little evidence of what caused their downfall, and the monsters that infest it may well be the work of the instability the Cataclysm itself has caused. Some may have even returned to it at one point, picking through the ruin and leaving behind their own hidden knowledge. But the knowledge they brought with them allowed the other orders to flourish, and began the conflicts that would rage in the shadow of the imminent apocalypse.*

##### Saguine Ruin
*This was the sanctum of the Sanguine Order, where their most morbid rituals were practiced. There they sought the blood of humans, monsters, anything they could exploit to gain greater power and mastery over the arcane. Sometime before the cataclysm, they prepared their greatest ritual yet. They hunted for copies of a Summoner's Tome, comparing differences and gleaning info. They also stole away a sacred ring from some forgotten craftsman, forced to use it even though they shun service to any God or Demon. With it, they called upon a creature of great power from beyond the realms, to best it and use its power for their own. When it came through the portal, it was twisted and warped. An undead mockery of what they sought, it broke the back of the Sanguine Order by fire and claw.*

##### Impact Site
*A place sacred to The Keepers of The Oath. Over the crater of a meteor strike from long before the Cataclysm, they built a temple that floated, unbound by the rules of reality or earthy architecture. A few months before the Cataclysm, a ritual was prepared. What they needed was a blade sacred to their Order, the only surviving examples lost to various misfortunes. A few cultists of the Sanguine Order brought them the only ritual blade known to still be intact. But it was an act of sabotage, a concealed curse meant to destroy the Keepers so that the dwindling blood mages could claim a new home. It worked too well, as the monster called forth instead of their master proved too powerful, and its evil too disruptive to cleanse.*

##### Curious Structure
*This was once a sanctum of The Cleansing Flame, outwardly designed to resemble a church hidden away, far from civilization. On the very day of the Cataclysm, the lowermost level was engulfed in the fires of Hell. There came a living demon and a host of monsters from beyond, and those who survived the initial chaos were driven back and picked off. In its wake it lingered, drawing more abominations with it, and left an unearthly weapon there as a symbol of its conquest. Only the actions of the last few survivors allowed them to seal off the entrance to the lowermost levels.*

##### Basements
*Not all of the arcanists lurked in the shadows far from civilization. They still needed some means to ensure that their actions could be carried out unhindered, and that required people much closer to mundane life.*

##### Shrines
*Elsewhere, beyond the reach of civilization, a few places of otherworldly construction have been dragged into this world in the wake of the Cataclysm. Some retain evidence of human contact and exploitation, but most are wild and infested by otherworldly monsters.*
