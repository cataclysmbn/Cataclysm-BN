# data/lua

This directory contains shared Lua libraries that can be imported by any mod using `require()`.

## Usage

Place library files here to make them available to all mods:

```
data/lua/
├── ui.lua              # Shared UI utilities
├── pl/                 # Penlight library (example)
│   ├── utils.lua
│   ├── path.lua
│   └── init.lua
└── mylib/
    └── helper.lua
```

## Importing from mods

Mods can import these libraries using absolute paths:

```lua
-- In any mod's .lua file
local ui = require("ui")                  -- loads data/lua/ui.lua
local pl_utils = require("pl.utils")      -- loads data/lua/pl/utils.lua
local helper = require("mylib.helper")    -- loads data/lua/mylib/helper.lua
```

## Installing third-party libraries

You can install pure-Lua libraries (like Penlight) by copying them to this directory:

```bash
# Example: Installing Penlight
git clone https://github.com/lunarmodules/Penlight.git
cp -r Penlight/lua/pl data/lua/
```

Then use in your mods:

```lua
local stringx = require("pl.stringx")
local pretty = require("pl.pretty")
```
