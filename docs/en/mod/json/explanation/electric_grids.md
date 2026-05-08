# Electric grids

Electric grids are electric connections within a building, similar to how all parts of a vehicle
share battery power.

# Features

Currently, only the following furniture interacts with an electric grid:

- Mounted solar panel (like the one on top of the Evac Center) produces energy
- Mounted battery stores energy - when examining it, you receive information about power in a given
  battery and in the whole grid.
- Battery charger works like vehicle battery charger/recharging station. Place rechargeable
  batteries or items containing them into the recharger and power will be drained from mounted
  batteries to recharge those.
- You can use voltmeter to
  - tell electric grid connections
  - check amount of energy stored
  - extend electric grid connections
- Ovens work like hotplates, but with charges equal to charges of the grid
- Jumper cable connector connects a vehicle to an electric grid. To use, connect the jumper cable to
  the vehicle and then to the connector. Once connected, the vehicle and the grid will share power
  for most purposes, but will usually charge/discharge their own batteries before sending them
  through the connector
- Fridges
- Floor lamp
- Electric Kiln
- Food Dehydrator
- Food Processor
- Vaccuum Sealer
- Arc Welder
- Electric Forge
- Electrolysis Kit

# Grid size

For the purposes of an electric grid, a building is made out of overmap tiles (the tiles you see on
the map opened with 'M' key, on the local map they are 24x24 tiles). Most city buildings have
individual electric grids. For example, a house with a roof and a basement, with a solar panel on
the roof, oven on the floor and a battery in the basement, will allow using the oven with the energy
from the battery charged by the panel. Overmap tiles without buildings only have grids within their
own overmap tile. There is currently no way of connecting grids together, other than by use of two
connectors, two jumper cables, and a vehicle.

# Modding

To create a furniture that uses the grid to power a fake item, create the fake item with a flag
`"USES_GRID_POWER"` and then set furniture's `"crafting_pseudo_item"` to that item's ID. For
example, the oven item is:

```json
{
  "id": "fake_oven",
  "copy-from": "fake_item",
  "type": "TOOL",
  "name": { "str": "oven" },
  "sub": "hotplate",
  "max_charges": 1000,
  "flags": ["USES_GRID_POWER"]
}
```

And the oven furniture uses it as:

```json
"crafting_pseudo_item": "fake_oven"
```

Currently, the grid can only power fake items used as a part of a furniture.

# Manually Adding Grids (for map design)

**Warning: it is for map design. You can built grid in game without editing files. You just have to
build the solars and battery. Jumper cable and connector can connect grid with vehicles.** If you
build a new house, you'll find the new house doesn't have any electric grid. To use electric grid in
your new house, you should get and edit the overmap file, which is stored in your game's save
folder. It's name is o.0.1 or similar. What you need is:

```json
"electric_grid_connections":[[[0,0,0]], ... ... ... ]}
```

Add your coordinate in this place. If your home is at `0'113, 2'56`, then your overmap file is
`o.0.2` and your coordinate to add is `[113,56,0]` (or your z-coordinate) To connect different
floors, add coordinate as like this:

```json
"electric_grid_connections":[[[113,56,0],[0,0,1]],[[113,56,1],[0,0,-1]]... ... ... ]}
```

`[0,0,1]` means this coordinate shares electricity with the upper stair, and `[0,0,-1]` means the
opposite one. If your house is as big as an office tower or city hall, `[0,1,0]` or `[1,0,0]` will
also work. It's actually how the large building's electric grid works.
