# World Types

World types are used to store data about dimensions. Including the default one

> [!Warning]
> Dimensions are still somewhat experimental, use at your own risk

### Fields

| Identifier           | Description                                                                                      |
| -------------------- | ------------------------------------------------------------------------------------------------ |
| id                   | (_mandatory_) Unique ID.                                                                         |
| name                 | (_mandatory_) In-game name displayed.                                                            |
| description          | (_optional_) Description of the dimension                                                        |
| region_settings      | (_optional_) Default is `default`, id of region settings to use.                                 |
| generate_overmap     | (_optional_) Default true, should connections lakes forests and swamps be generated.             |
| infinite_bounds      | (_optional_) Default true, is the dimension unbounded like the default dimension.                |
| boundary_terrain     | (_optional_) Default "boundary_terrain", what terrain to put around the bounds of the dimension. |
| save_prefix          | (_optional_) What string to prefix saves with.                                                   |
| allow_npc_travel     | (_optional_) Default false, weather to allow NPCs to travel to the dimension.                    |
| allow_vehicle_travel | (_optional_) Default false, weather to allow vehicles to travel to the dimension.                |
| scale_num            | (_optional_) Scale of travel in this dimension. 2 means 2x longer travel.                        |
| scale_den            | (_optional_) Scale of travel in this dimension. 2 means 2x shorter travel.                       |
| sunrise_summer       | (_optional_) Default -1 ( use game options ), hour of sunrise in summer                          |
| sunset_summer        | (_optional_) Default -1 ( use game options ), hour of sunset in summer                           |
| sunset_winter        | (_optional_) Default -1 ( use game options ), hour of sunset in winter                           |
| sunset_winter        | (_optional_) Default -1 ( use game options ), hour of sunset in winter                           |
| sunrise_equinox      | (_optional_) Default -1 ( use game options ), hour of sunrise in equinox                         |
| sunset_equinox       | (_optional_) Default -1 ( use game options ), hour of sunset in equinox                          |
| pernament_daylight   | (_optional_) Default -1 ( use game options ), sets weather it is always day                      |
| pernament_night      | (_optional_) Default -1 ( use game options ), sets weather it is always night                    |
