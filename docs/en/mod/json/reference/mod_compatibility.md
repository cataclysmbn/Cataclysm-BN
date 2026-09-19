# Mod Compatability

## Explaination

The `mod_interactions` folder in the base of the data folder has special loading logic.
Within that folder, place a folder with a mod id.
Anything within that folder will only be loaded when the other mod is loaded.
All loading in that folder occurs after both mods have loaded, and the loading of interactions happens in standard load order.

## Example

For example
Mod 1: Arcana
Has the folder `mod_interactions` in root
Then the folder `crt_expansion` in mod interactions.
When `crt_expansion` is loaded, it adds all the present files, such as `professions.json` adding the C.R.I.T. Anomaly Investigator

## Notesi

Nested mod interactions are currently unsupported, you cannot have it load if mod A and B is present.

Nor can you do something like mod A or mod B being present load something

Mod source is the base mod
