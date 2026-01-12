# Lua scripting cookbook

Here are some snippets to help you get familiar with Lua APIs and learn how to use them.
To test these examples, paste the code into the in-game Lua console by pressing the `` ` `` (backtick) key.

## Items

### Getting list of all wielded and worn items in your inventory

```lua
local you = gapi.get_avatar()
local items = you:all_items(false)

for _, item in pairs(items) do
  local status = ""
  if you:is_wielding(item) then
    status = "wielded: "
  elseif you:is_wearing(item) then
    status = "worn: "
  end
  print(status .. item:tname(1, false, 0))
end
```

<details>
<summary>Example output</summary>

```
wielded: smartphone
worn: bra
worn: panties
worn: pair of socks
worn: jeans
worn: long-sleeved shirt
worn: pair of sneakers
worn: messenger bag
worn: wrist watch
pocket knife
matchbook
clean water (plastic bottle)
clean water
```

</details>

## Monsters

### Spawning a dog near the player

```lua
local avatar = gapi.get_avatar()
local coords = avatar:get_pos_ms()
local dog_mtype = MtypeId.new("mon_dog_bcollie")
local doggy = gapi.place_monster_around(dog_mtype, coords, 5)
if doggy == nil then
    gdebug.log_info("Could not spawn doggy :(")
else
    gdebug.log_info(string.format("Spawned Doggy at %s", doggy:get_pos_ms()))
end
```

## Combat

### Printing details about a combat technique when it is used

First, define the function.

```lua
on_creature_performed_technique = function(params)
  local char = params.char
  local technique = params.technique
  local target = params.target
  local damage_instance = params.damage_instance
  local move_cost = params.move_cost
  gdebug.log_info(
          string.format(
                  "%s performed %s on %s (DI: %s , MC: %s)",
                  char:get_name(),
                  technique.name,
                  target:get_name(),
                  damage_instance:total_damage(),
                  move_cost
          )
  )
end
```

Then connect the hook to the function ONLY ONCE.

```lua
table.insert(game.hooks.on_creature_performed_technique, function(...) return on_creature_performed_technique(...) end)
```

<details>
<summary>Example output</summary>

```
Ramiro Waters performed Power Hit on zombie (DI: 27.96 , MC: 58)
```

</details>

## Item Durability

### Checking and modifying item damage

```lua
local you = gapi.get_avatar()
local wielding = you:all_items(false)[1]
print(wielding:get_damage())
print(wielding:get_damage_level(4))
wielding:mod_damage(2000)
print(wielding:get_damage_level(4))
```

## Monsters

### Adding items to a monster's inventory

```lua
local target_monster = -- [[ your monster reference ]]
local scraps = gapi.create_item(ItypeId.new("scrap"), 3)
target_monster:as_monster():add_item(scraps)
```

## Weather Hooks

### Reacting to weather changes

First, set up the hook in your preload.lua:

```lua
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_weather_changed, function(...) mod.weather_changed_alert(...) end)
table.insert(game.hooks.on_weather_updated, function(...) mod.weather_report(...) end)
```

Then define the handlers in your main.lua:

```lua
local mod = game.mod_runtime[game.current_mod]

-- Called when weather changes (e.g., clear -> rain)
mod.weather_changed_alert = function(params)
    local msg = string.format(
        "Weather changed from %s to %s!",
        params.old_weather_id,
        params.weather_id
    )
    gdebug.log_info(msg)
end

-- Called every 5 minutes with current weather data
mod.weather_report = function(params)
    local msg = string.format(
        "Current Weather: %s, Temperature: %.1f°C, Wind: %d, Humidity: %d%%",
        params.weather_id,
        params.temperature,
        params.windspeed,
        params.humidity
    )
    gdebug.log_info(msg)
end
```

## Combat Ranged

### Reacting to shots fired and thrown items

First, set up the hooks in your preload.lua:

```lua
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_shoot, function(...) return mod.on_shoot_fun(...) end)
table.insert(game.hooks.on_throw, function(...) return mod.on_throw_fun(...) end)
```

Then define the handlers in your main.lua:

```lua
local mod = game.mod_runtime[game.current_mod]

mod.on_shoot_fun = function(params)
    ---@type Item
    local gun = params.gun
    ---@type Item
    local ammo_item = params.ammo
    local ammo = ItypeId.NULL_ID()
    if not ammo_item then
        ammo = gun:ammo_current()
    else
        ammo = ammo_item:get_type()
    end
    local shoot_noise = ammo:obj():slot_ammo().loudness
    gdebug.log_info(string.format("Gun sound: %d.", shoot_noise))
end

mod.on_throw_fun = function(params)
    ---@type Character
    local thrower = params.thrower
    ---@type Item
    local thrown = params.thrown
    if thrown:is_gun() then
        gdebug.log_info("Hey! Guns are not for throwing!")
    end
end
```

## Overmap Queries

### Finding and manipulating items on the overmap

```lua
-- Find all items on the overmap at a specific location
local om_pos = OmPos.new(0, 0, 0)
local items = gapi.overmap_find_items_around(om_pos, 0)

-- Get an item from the map and keep it in Lua even if the map unloads
local map_pos = MapPos.new(100, 100, 0)
local item = gapi.get_map():find_item_at(map_pos)
if item then
    local detached = gapi.create_detached_item(item)
    -- Later, you can reattach it to a location
    local reattached = gapi.reattach_item(detached, map_pos)
end

-- Teleport items within the same map
local source_pos = MapPos.new(100, 100, 0)
local dest_pos = MapPos.new(110, 110, 0)
gapi.get_map():move_item_at(source_pos, dest_pos)
```

## Death Hooks

### Tracking when monsters die

```lua
-- In preload.lua
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_mon_death, function(...) return mod.on_mon_death(...) end)
```

```lua
-- In main.lua
local mod = game.mod_runtime[game.current_mod]

mod.on_mon_death = function(params)
    ---@type Creature
    local monster = params.creature
    ---@type Character|nil
    local killer = params.killer

    local killer_name = killer and killer:get_name() or "Unknown"
    gdebug.log_info(string.format("%s was killed by %s", monster:get_name(), killer_name))
end
```

### Tracking character deaths

```lua
-- In preload.lua
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_char_death, function(...) return mod.on_char_death(...) end)
```

```lua
-- In main.lua
local mod = game.mod_runtime[game.current_mod]

mod.on_char_death = function(params)
    ---@type Character
    local char = params.char
    ---@type Character|nil
    local killer = params.killer

    if char == gapi.get_avatar() then
        gdebug.log_info("You have died!")
    end
end
```

## Character Combat Stats

### Getting attack and stamina costs

```lua
local you = gapi.get_avatar()
local items = you:all_items(false)

for _, item in pairs(items) do
    print(
        item:tname(1, false, 0) 
        .. " { attack cost: " .. item:attack_cost() 
        .. ", stamina cost: " .. item:stamina_cost()
        .. ", melee stamina cost: " .. you:get_melee_stamina_cost(item)
        .. " }"
    )
end

-- Check for special abilities
print("Uncanny dodge: " .. (you:uncanny_dodge() and "yes" or "no"))
```

## Character Magics

### Learn a new spell and forget it

learning a spell:

```lua
local u = gapi.get_avatar()
local km = u:get_magic()
local ex_sp = SpellTypeId.new("example_template")
km:learn_spell(ex_sp, u, true) -- learn forced
print( km:knows_spell(ex_sp) ) -- check
```

forgetting a spell:

```lua
local u = gapi.get_avatar()
local km = u:get_magic()
local ex_sp = SpellTypeId.new("example_template")
km:forget_spell(ex_sp)         -- forget
print( km:knows_spell(ex_sp) ) -- check again
```

## Dynamic Item Actions

### Creating custom item use functions in Lua

```lua
-- Define an item's use behavior with tick and can_use functions
game.iuse_functions["my_custom_item"] = {
    use = function(params)
        local user = params.user
        local item = params.item
        gdebug.log_info("Using: " .. item:tname(1))
        return 0  -- Return time cost in moves
    end,

    can_use = function(params)
        local user = params.user
        local item = params.item
        -- Return true to allow use, false to prevent
        return true
    end,

    tick = function(params)
        local user = params.user
        local item = params.item
        -- Called periodically while item is active
        if item:get_countdown() == 0 then
            gdebug.log_info("Item countdown finished!")
        end
    end
}

-- Set a countdown on an item to trigger periodic ticks
local item = gapi.create_item(ItypeId.new("some_item"), 1)
item:set_countdown(100)  -- Ticks for 100 turns
```

## More Combat Hooks

### Reacting to dodge, block, and technique events

```lua
-- In preload.lua
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_creature_dodged, function(...) return mod.on_creature_dodged(...) end)
table.insert(game.hooks.on_creature_blocked, function(...) return mod.on_creature_blocked(...) end)
table.insert(game.hooks.on_creature_performed_technique, function(...) return mod.on_creature_performed_technique(...) end)
table.insert(game.hooks.on_creature_melee_attacked, function(...) return mod.on_creature_melee_attacked(...) end)
```

```lua
-- In main.lua
local mod = game.mod_runtime[game.current_mod]

mod.on_creature_dodged = function(params)
    ---@type Character
    local char = params.char
    ---@type Creature
    local source = params.source
    local difficulty = params.difficulty
    gdebug.log_info(string.format("%s dodged %s (DC: %d)", char:get_name(), source:get_name(), difficulty))
end

mod.on_creature_blocked = function(params)
    ---@type Character
    local char = params.char
    ---@type Creature
    local source = params.source
    local bodypart_id = params.bodypart_id
    local damage_blocked = params.damage_blocked
    gdebug.log_info(string.format(
        "%s blocked %s on %s (blocked: %.1f damage)",
        char:get_name(),
        source:get_name(),
        bodypart_id,
        damage_blocked
    ))
end

mod.on_creature_melee_attacked = function(params)
    ---@type Character
    local char = params.char
    ---@type Creature
    local target = params.target
    if params.success then
        gdebug.log_info(string.format("%s hit %s", char:get_name(), target:get_name()))
    else
        gdebug.log_info(string.format("%s missed %s", char:get_name(), target:get_name()))
    end
end
```

## Item Type Information

### Querying item type properties via ItypeId

```lua
local item_type = ItypeId.new("9mm")

-- Get the item type object (ItypeRaw)
local itype_raw = item_type:obj()

-- Access item type specific data (e.g., for ammo)
if itype_raw:slot_ammo() then
    local ammo_data = itype_raw:slot_ammo()
    print("Ammo damage: " .. ammo_data.damage)
    print("Ammo range: " .. ammo_data.range)
end

-- For containers
if itype_raw:slot_container() then
    local container_data = itype_raw:slot_container()
    print("Capacity: " .. container_data.capacity)
end

-- For tools
if itype_raw:slot_tool() then
    local tool_data = itype_raw:slot_tool()
    print("Tool quality: " .. tool_data.quality)
end
```

## Time and Space

### Sun and moon, inside and outside

```lua
local u_pos = gapi.get_avatar():get_pos_ms()
local map = gapi.get_map()
local now = gapi.current_turn()

-- Found the key name from MoonPhase entries
local moon = ""
for name, num in pairs(MoonPhase) do
   if num == now:moon_phase() then
      moon = name
   end
end

print( "Are you outside?: " .. tostring(map:is_outside(u_pos)) )
print( "Are you sheltered?: " .. tostring(map:is_sheltered(u_pos)) )
print( "Today moon phase is: " .. moon )
print( "Sunset time is: " .. now:sunset():to_string_time_of_day() )
```
