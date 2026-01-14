# Updating the game

# Do

0. Get the game from https://github.com/cataclysmbn/Cataclysm-BN/releases
1. Unpack the game
2. Copy `save` and `config` from old game directory to the new directory
3. (Optional) Copy the `mods` from the old game dir

# Don't

### Unpack the new game over old directory

Unpacking the game over old directory can cause duplicate JSON entry errors. If you really want to
do it, make sure to delete the old `data` directory before unpacking the new version.

Sometimes data files are deleted in the core. New version of the game will not have those files, but
unpacking an archive doesn't delete the old files. Old files will still be read and loaded, which
can overwrite new entries.

### Have custom mods in `data/mods` directory

Put them directly into `mods` directory, on the same level as `data`. This directory doesn't exist
by default, but the game will read it if it's present.

This will allow you to copy the mods between game versions, while still allowing the "core mods" to
update properly.
