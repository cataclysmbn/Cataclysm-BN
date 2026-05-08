# Starting locations

Starting locations are specified as JSON object with "type" member set to "start_location":

```json
{
    "type": "start_location",
    "id": "field",
    "name": "An empty field",
    "target": "field",
    ...
}
```

The id member should be the unique id of the location.

The following properties (mandatory, except if noted otherwise) are supported:

## `name`

(string)

The in-game name of the location.

## `target`

(string)

The id of an overmap terrain type (see overmap_terrain.json) of the starting location. The game will
chose a random place with that terrain.

## `flags`

(optional, array of strings)

Arbitrary flags. Mods can modify this via "add:flags" / "remove:flags". TODO: document them.
