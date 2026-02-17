# Modding with Lua

## Useful links

- [Lua 5.3 Reference Manual](https://www.lua.org/manual/5.3/)
- [Sol2 documentation](https://sol2.readthedocs.io/en/latest/)
- [Programming in Lua (first edition)](https://www.lua.org/pil/contents.html)

## Example mods

There are a couple heavily-commented example mods in `data/mods/` that make use of Lua API described
here:

- `smart_house_remotes` - Add remotes for controlling garage doors and window curtains.
- `saveload_lua_test` - Mod for testing Lua save/load API.

## Ingame Lua console

In-game Lua console is available through the debug menu or via `Lua Console` hotkey (unbound by
default).

It is rather simple, but is capable of keeping input history, showing output and errors from Lua
scripts as well as running Lua snippets and printing the returned values.

You can adjust console log capacity by running `gdebug.set_log_capacity( num )` (default is 100
entries), or clear it by running `gdebug.clear_lua_log()`.

## Lua hot-reload

To speed up mod development process, BN supports Lua hot-reload functionality.

There is no filesystem watcher, so hot-reload must be triggered manually via a corresponding
`Reload Lua Code` hotkey (unbound by default). The hot-reload can also be triggered from console
window by pressing the corresponding hotkey, or by running `gdebug.reload_lua_code()` command.
Running the command from regular Lua scripts may have unintended consequences, use at your own risk!

Note that not all code can be hot-reloaded, it'll be explained in later sections.

## Game data loading

When a world is being loaded, game does it in roughly these steps:

1. Initializes world-related internal state, sets the world as active
2. Loads world's artifact item types (artifacts are hacky and will likely be removed soon in favor
   of relics + Lua)
3. Retrieves list of mods used by the world
4. Loads the mods according to the list
5. Initializes avatar-related internal state
6. Loads from save dir actual overmap data, avatar data and reality bubble data

What we care about here is the mod loading stage. It has a number of sub-steps:

1. Loading function receives list of world mods
2. It discards the missing ones and prints debug message for each
3. It checks remaining mods on the list, and throws error if a mod needs Lua, but the game build
   does NOT support Lua
4. It also throws a warning if game's Lua API version differs from the one used by the mod
5. For every mod on the list that uses Lua, it runs the mod's [`preload.lua`](#preloadlua) script
   (if present)
6. It goes over all mods in same order as in the list, and loads JSON definitions from each mod's
   folder
7. It finalizes loaded data (resolves copy-from, prepares some types with complex state for use)
8. For every mod on the list that uses Lua, it runs the mod's [`finalize.lua`](#finalizelua) script
   (if present)
9. It checks consistency of loaded data (validates values, warns about iffy combinations of values,
   etc.)
10. (R) For every mod on the list that uses Lua, it runs the mod's [`main.lua`](#mainlua) script (if
    present)

As such, we only have 3 scipts to place a mod's Lua code into: [`preload.lua`](#preloadlua),
[`finalize.lua`](#finalizelua) and [`main.lua`](#mainlua). The differences between the 3 and their
intended use cases will be explained below.

You can use only one script, two or all three, depending on your needs.

When executing hot-reload, the game repeats the step marked with (R). That means if you want the
code you're working on to be hot-reloadable, put it into [`main.lua`](#mainlua).

### `preload.lua`

This script is supposed to register event hooks and set up definitions that will then be referred by
game JSON loading system (e.g. item use actions). Note that you can registers them here, and define
them in some later stage (e.g. in [`main.lua`](#mainlua) to allow hot-reload to affect your hooks).

### `finalize.lua`

This script is supposed to allow mods to modify definitions loaded from JSON after copy-from has
been resolved, but there is no API for this yet.

TODO: api for finalization

### `main.lua`

This script is supposed to implement the main logic of the mod. This includes, but not limited, to:

1. Mod runtime state
2. Mod initialization on game start
3. Mod save/load code, if required
4. Implementation of hooks that were set up in [`preload.lua`](#preloadlua)

## Lua API details

While you can do a lot of interesting stuff with vanilla Lua, the integration imposes some limits to
prevent potential bugs:

- Loading packages (or Lua modules) is limited to `data/lua/` and `data/mods/<mod_id>/` directories.
- Your current mod id is stored in `game.current_mod` variable
- Your mod's runtime state should live in `game.mod_runtime[ game.current_mod ]` table. You can also
  interface with other mods if you know their id, by accessing their runtime state in a similar way
  with `game.mod_runtime[ that_other_mod_id ]`
- Changes to global state are not available between scripts. This is to prevent accidental
  collisions between function names and variable names. You still can define global variables and
  functions, but they will be visible to your mod only.

### Module loading with `require`

The `require` function supports multiple import patterns for organizing your mod code:

#### Relative imports

Load modules relative to the current file:

```lua
-- In data/mods/my_mod/main.lua
local utils = require("./lib/utils")          -- loads lib/utils.lua
local config = require("./config")            -- loads config.lua in same dir

-- In data/mods/my_mod/lib/foo.lua
local helper = require("./helper")            -- loads lib/helper.lua
local parent_mod = require("../parent")       -- loads parent.lua from mod root
```

#### Standard library imports

Load shared libraries from `data/lua/lib/` using `lib.` or `bn.lib.` prefix:

```lua
-- Assuming penlight is installed in data/lua/lib/pl/
local pl_utils = require("lib.pl.utils")      -- loads data/lua/lib/pl/utils.lua
local pl_path = require("bn.lib.pl.path")     -- same, bn.lib. prefix also works
```

#### Mod-local absolute imports

Load modules from your mod directory using dotted notation (no prefix):

```lua
-- In data/mods/my_mod/main.lua
require("foo.bar.baz")  -- searches for:
                        -- 1. <mod>/foo/bar/baz.lua
                        -- 2. <mod>/foo/bar/baz/init.lua
```

#### Search order

- **Relative (`./`, `../`)**: Current file's directory
- **Standard library (`lib.*`, `bn.lib.*`)**: `data/lua/lib/` only
- **Mod-local (no prefix)**: Current mod directory only

> [!CAUTION]
> Avoid naming mod modules with reserved prefixes (`lib`, `bn`) to prevent namespace conflicts.
> Use descriptive mod-specific names instead (e.g., `mymod.core`, `utils`).

#### Module structure

Modules should return a table:

```lua
-- lib/math_helper.lua
local M = {}

M.add = function(a, b)
  return a + b
end

return M
```

Then use in other files:

```lua
local math_helper = require("./lib/math_helper")
local result = math_helper.add(2, 3)
```

### Lua libraries and functions

When script is called, it comes with some standard Lua libraries pre-loaded:

| Library   | Description                                  |
| --------- | -------------------------------------------- |
| `base`    | print, assert, and other base functions      |
| `math`    | all things math                              |
| `string`  | string library                               |
| `table`   | the table manipulator and observer functions |
| `package` | load modules with `require`                  |

See `Standard Libraries` section in Lua manual for details.

Some of the functions here are overloaded by BN, see [Global overrides](#global-overrides) for
details.

### Global state

Most of necessary data and game runtime state is available through global `game` table. It has the
following members:

| Variable                    | Description                                                                           |
| --------------------------- | ------------------------------------------------------------------------------------- |
| `game.current_mod`          | Id of mod that's being loaded (available only when script is executed)                |
| `game.active_mods`          | List of active world mods, in load order                                              |
| `game.mod_runtime.<mod_id>` | Runtime data for mods (each mod gets its own table named after its id)                |
| `game.mod_storage.<mod_id>` | Per-mod storage that gets automatically saved/loaded on game save/load                |
| `game.cata_internal`        | For internal game purposes, please don't use this                                     |
| `game.hooks.<hook_id>`      | Hooks exposed to Lua scripts, will be called on corresponding events                  |
| `game.iuse.<iuse_id>`       | Item use functions that will be recognized by the item factory and called on item use |

### Game Bindings

The game exposes various functions, constants and types to Lua. Functions and constants are
organized into "libraries" for organizational purposes. Types are available globally, and may have
member functions and fields.

To see the full list of functions, constants and types, run the game with `--lua-doc` command line
argument. This will generate documentation file `lua_doc.md` that will be placed in your `config`
folder.

#### Global overrides

Some functions have been globally overriden to improve integration with the game.

| Function   | Description                                                                        |
| ---------- | ---------------------------------------------------------------------------------- |
| print      | Print as `INFO LUA` to debug.log (overrides default Lua print)                     |
| require    | Supports relative (`./foo`, `../bar`) and absolute (`pl.utils`) imports            |
| package    | Custom searcher loads from `data/lua/`, `data/mods/<mod_id>/` with security checks |
| dofile     | Disabled                                                                           |
| loadfile   | Disabled                                                                           |
| load       | Disabled                                                                           |
| loadstring | Disabled                                                                           |

TODO: alternatives for dofile and such

#### Hooks

To see the list of hooks, check `hooks_doc` section of the autogenerated documentation file. There,
you will see the list of hook ids as well as function signatures that they expect. You can register
new hooks by appending to the hooks table like so:

```lua
-- In preload.lua
local mod = game.mod_runtime[game.current_mod]

table.insert(game.hooks.on_game_save, function(...)
  -- This is essentially a forward declaration.
  -- We declare that the hook exists, it should be called on game_save event,
  -- but we will forward all possible arguments (even if there is none) to,
  -- and return value from, the function that we'll declare later on.
  return mod.my_awesome_hook(...)
end)

-- In main.lua
local mod = game.mod_runtime[game.current_mod]

mod.my_awesome_hook = function()
  -- Do actual work herede
end
```

#### Item use function

Item use functions use unique id to register themselves in item factory. On item activation, they
receive multiple arguments that will be described in the example below.

```lua
-- In preload.lua
local mod = game.mod_runtime[ game.current_mod ]
game.iuse_functions[ "SMART_HOUSE_REMOTE" ] = function(...)
  -- This is just a forward declaration,
  -- but it will allow us to use SMART_HOUSE_REMOTE iuse in JSONs.
  return mod.my_awesome_iuse_function(...)
end

-- In main.lua
local mod = game.mod_runtime[ game.current_mod ]
mod.my_awesome_iuse_function = function( who, item, pos )
  -- Do actual activation effect here.
  -- `who` is the character that activated the item
  -- `item` is the item itself
  -- `pos` is the position of the item (equal to character pos if character has it on them)
end
```

#### Translation functions

To make the mod translatable to other languages, get your text via functions bound in `locale`
library. See [translation API](../explanation/lua_integration.md) for detailed explanation of their
C++ counterparts.

Usage examples are shown below:

```lua
-- Simple string.
--
-- The "Experimental Lab" text will be extracted from this code by a script,
-- and will be available for translators.
-- When your Lua script runs, this function will search for translation of
-- "Experimental Lab" string and return either translated string, 
-- or the original string if there was no translation found.
local location_name_translated = locale.gettext( "Experimental Lab" )

-- ERROR: you must call `gettext` with a string literal.
-- Calling it like this will make it so "Experimental Lab" is NOT extracted,
-- and translators won't see it when they translate the text.
local location_name_original = "Experimental Lab"
local location_name_translated = locale.gettext( location_name_original )

-- ERROR: don't alias the function under different name.
-- Calling it like this will make it so "Experimental Lab" is NOT extracted,
-- and translators won't see it when they translate the text.
local gettext_alt = locale.gettext
local location_name_translated = gettext_alt( "Experimental Lab" )

-- This, however, is fine.
local gettext = locale.gettext
local location_name_translated = gettext( "Experimental Lab" )

-- String with possible plural form.
-- Many languages have more than 2 plural forms with complex rules related to which one to use.
local item_display_name = locale.vgettext( "X-37 Prototype", "X-37 Prototypes", num_of_prototypes )

-- String with context
local text_1 = locale.pgettext("the one made of metal", "Spring")
local text_2 = locale.pgettext("the one that makes water", "Spring")
local text_3 = locale.pgettext("time of the year", "Spring")

-- String with both context and plural forms.
local item_display_name = locale.vpgettext("the one made of metal", "Spring", "Springs", num_of_springs)

--[[
  When some text is tricky and requires explanation,
  it's common to place a special comment denoted with `~` to help translators.
  The comment MUST BE right above the function call.
]]

--~ This comment is good and will be visible for translators.
local ok = locale.gettext("Confusing text that needs explanation.")

--~ ERROR: This comment is too far from gettext call and won't be extracted!
local not_ok = locale.
                gettext("Confusing text that needs explanation.")

local not_ok = locale.gettext(
                  --~ ERROR: This comment is in wrong place and won't be extracted!
                  "Confusing text that needs explanation."
                )

--[[~
  ERROR: Multiline Lua comments can't be used as translator comments!
  This comment won't be extracted!
]] 
local ok = locale.gettext("Confusing text that needs explanation.")

--~ If you need a multiline translator comment,
--~ just use 2 or more single-line comments.
--~ They'll be concatenated and shown as a single multi-line comment.
local ok = locale.gettext("Confusing text that needs explanation.")
```
