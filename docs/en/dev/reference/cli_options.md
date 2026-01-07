---
edit: false
---

# CLI Options

> [!NOTE]
>
> This page is auto-generated from `tools/gen_cli_docs.ts` and should not be edited directly.

The game executable can not only run your favorite roguelike,
but also provides a number of command line options to help modders and developers.

---

## Info

### `--help

`

print this message and exit.

### `--version

`

print the version and exit.

### `--paths

`

print the paths used by the game and exit.

### `

`

Command line parameters:.

### `--seed <string of letters and or numbers>

`

Sets the random number generator's seed value.

### `--jsonverify

`

Checks the BN json files.

### `--check-mods [mods…]

`

Checks the json files belonging to BN mods.

### `--dump-stats <what> [mode = TSV] [opts…]

`

Dumps item stats.

### `--world <name>

`

Load world.

### `--basepath <path>

`

Base path for all game data subdirectories.

### `--dont-debugmsg

`

If set, no debug messages will be printed.

### `--lua-doc <output path>

`

Generate Lua docs to given path and exit.

### `--lua-types <output path>

`

Generate Lua types to given path and exit.

### `--datadir <directory name>

`

Sub directory from which game data is loaded.

### `--autopickupfile <filename>

`

Name of the autopickup options file within the configdir.

### `--motdfile <filename>

`

Name of the message of the day file within the motd directory.

### `

`

Map sharing.

### `--shared

`

Activates the map-sharing mode.

### `--username <name>

`

Instructs map-sharing code to use this name for your character..

### `--addadmin <username>

`

Instructs map-sharing code to use this name for your character and give you access to the cheat functions..

### `--adddebugger <username>

`

Informs map-sharing code that you're running inside a debugger.

### `--competitive

`

Instructs map-sharing code to disable access to the in-game cheat functions.

### `--worldmenu

`

Enables the world menu in the map-sharing code.

### `

`

User directories.

### `--userdir <path>

`

Base path for user-overrides to files from the ./data directory and named below.

### `--savedir <directory name>

`

Subdirectory for game saves.

### `--configdir <directory name>

`

Subdirectory for game configuration.

### `--memorialdir <directory name>

`

Subdirectory for memorials.

### `--optionfile <filename>

`

Name of the options file within the configdir.
