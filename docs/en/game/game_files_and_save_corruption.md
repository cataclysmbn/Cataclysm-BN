# Game file and save corruption

Game files and save corruption falls into following categories:

1. Caused by your system
2. Caused by your launcher
3. Caused by the game
4. Caused by improper upgrade procedure
5. Caused by character death

## Corruption caused by your system

### Common symptoms

- Nonsensical error messages when trying to load a save (e.g. "expected comma", or "expected array")
- Problematic files, when opened with a decent text editor (Notepad++ for you Windows users), are
  partially filled with zero bytes or some other gibberish
  ![image](https://user-images.githubusercontent.com/60584843/186019683-36a59d3b-31ce-408c-96ee-42390975171c.png)

### Common causes and ways to avoid

1. Insufficient disk space (buy a bigger disk)
2. Faulty disk (buy a new disk)
3. Saving on a USB stick / external hard drive and pulling it out without "Safe Removal" (don't do
   this)
4. Blackout (buy a UPS for your computer)
5. Power loss due to hardware failure (call for a PC repair crew)
6. Power loss due to user actions (don't pull the power plug of your PC, don't shut down laptop by
   long-pressing power button)
7. Blue screen or a similar unrecoverable OS error (again, call PC repair crew)

### Fixing corruption

There's nothing we can do about it, but you can try your OS file backup functionality (or game
launcher backup functionality, if its backups didn't get corrupted as well).

## Corruption caused by your launcher

### Common symptoms

1. All (or some) saves are suddenly gone
2. All (or some) of your personal tweaks in `[game root]/data/` are suddenly undone
3. All (or some) of your mods/soundpacks are suddenly gone
4. All (or some) settings are reset to defaults

### Common causes and ways to avoid

1. The launcher is buggy. Bug the author(s)/maintainer(s) to fix it, or find a different launcher -
   maybe one that [tries to avoid touching user data](https://github.com/qrrk/Catapult).
2. Your mods/soundpacks/personal tweaks were inside `[game root]/data/` folder. That folder is for
   base game stuff only, so don't get surprised if it gets overwritten/erased/corrupted after an
   update. Specifically to avoid this situation the game allows placing these files into so-called
   "user" folders - on Windows, that would be folders named `[game root]/mods/` and
   `[game root]/sound/`. All personal tweaks should go into a dedicated mod placed into same
   `[game root]/mods/` folder, and when that's not possible due to limitations of modding system -
   kept in some backup form, preferably away from `[game root]/` to avoid launcher treating it as
   junk.
3. The launcher may have some quirks. For example, one of the recent updates of a popular launcher
   had a big warning on it that some people decided to ignore. Advice here is similar, bug the
   author(s)/maintainer(s) to solve these (add migration?) or just be more attentive.
   ![image](https://user-images.githubusercontent.com/60584843/186022055-0015f2cc-2549-4721-8a0d-8b7047b3d2b1.png)

### Fixing corruption

Not much we can do about this either. Try bugging launcher people for a fix or your OS for the
backups.

## Corruption caused by the game

### Common symptoms

1. Whenever you load a save, something weird but non-game-breaking happens (you die, or lose 10kg,
   or your NPC companion teleports around)
2. Whenever you load a save, the game screams at your about lost item locations

### Common causes and ways to avoid

These are game bugs and should be fixed, not avoided. We can't fix what we don't know about, so bugs
need to be reported - either on GitHub via creating a
[Bug Report](https://github.com/cataclysmbn/Cataclysm-BN/issues/new/choose) (you only need a
working email address to make a GitHub account, it's not _that_ hard) or by joining
[our Discord server](https://discord.gg/XW7XhXuZ89) and complaining in `#development` channel -
you'll most likely get a faster response there. If you know how to fix it (or want to take a stab at
it) - help is always appreciated.

### Fixing corruption

Saves in these cases can usually be fixed, though exact method varies on a case-by-case basis.

If the game is showing red-on-black messages that say something about `ACT_XXX lost target item` or
`item_location lost target during save/load cycle`, try making a backup of the save, then opening
your character's save file (usually `[game root]/save/[World Name]/#[gibberish].sav`) with any
decent text editor (Notepad++ for Windows is ok), finding all occurrences of sequences starting with
`ACT_` (e.g. `ACT_DROP`, or `ACT_WASH`) and replacing them with `ACT_NULL`.

## Corruption caused by improper upgrade procedure

### Common symptoms

1. After a manual update the game is showing weird errors noone you ask knows about, or experiences,
   even if you're not using any mods.
2. The errors go away after a clean reinstall of the game.

### Common causes and ways to avoid

You're most likely updating the game by downloading a fresh version and unpacking it over the
existing installation. **This is not supported and has never been supported.** It is generally a bad
idea to update programs in such way, unless they explicitly state they can handle this and it's
allowed. BN can't handle this. The mods you downloaded and upgraded in same way also most likely
can't handle this.

### Fixing corruption

Just do a clean reinstall, or use a launcher.

## Corruption caused by character death

### Common symptoms

Your character dies - it's a tragedy, and you wish you could turn back time to the good old days.
And then you realize you can do exactly that by pressing Alt+F4. But when loading the save next
time, you see your corpse nearby with all items duplicated and your vehicle is nowhere to be seen.

### Common causes and ways to avoid

You tried to cheat death, but failed, and ultimately was forgotten among the billions lost in the
cataclysm.

### Fixing corruption

You'll have to live with the reminder of your mistakes. Try backing up the save next time.
Technically it's a bug, but it's also an easy way out, so noone actually cares about fixing it.
