# MOD_INFO

Also see [MODDING.md](../tutorial/modding.md).

Object with `MOD_INFO` type describes the mod itself. Each mod must have exactly one `MOD_INFO`, and
unlike other types of objects from mods it is loaded on game launch, before the title screen shows
up. As such, any and all errors related to it will show up before the title screen shows up.

Current convention is to put your `MOD_INFO` in `mod_info.json` file within the root directory of
the mod.

Example:

```json
[
  {
    "type": "MOD_INFO",

    // Mod's unique identifier, prefer to use only ASCII letters, numbers and underscore for clarity.
    "id": "better_zeds",
    // Mod's category, see MODDING.md for list of supported values.
    "category": "content",
    // Mod's display name, in English.
    "name": "Better Zombies",
    // Mod's description, in English.
    "description": "Reworks all base game zombies and adds 100+ new variants.",
    // Mod's license. Non-FOSS should (usually) use "All Rights Reserved" or "Source Available"
    "license": "CC-BY-SA 4.0 Unported",
    // Original author(s) of the mod.
    "authors": ["That Guy", "His Friend"],
    // If the author(s) abandoned the mod for some reason, this entry lists current maintainers.
    "maintainers": ["Mr. BugFixer", "BugFixer Jr."],
    // Optional loading screen image paths. Entries may name an image file or a directory to search recursively.
    // Paths are resolved relative to the mod's "path" directory when present, and the modinfo.json directory is also searched for compatibility.
    // Supported image extensions are .png, .jpg, .jpeg, .bmp, .gif, and .webp.
    // The author credit is taken from the image filename before the first underscore, e.g. "foo_rest.png" displays "by foo" in white text with a black outline.
    "loading_images": ["load_1.png", "load_2.png", "some_directory"],
    // Mod version string. This is for users' and maintainers' convenience, so you can use whatever is most convenient here (e.g. date).
    "version": "2021-12-02",
    // List of mod's dependencies. Dependencies are guaranteed to be loaded before the mod is loaded.
    "dependencies": ["bn", "zed_templates"],
    // List of mods that are incompatible with this mod.
    "conflicts": ["worse_zeds"],
    // Special flag for core game data, can only be used by total overhaul mods. Only 1 core mod can be loaded at a time.
    "core": false,
    // Marks mod as obsolete. Obsolete mods don't show up in mod selection list by default, and have a warning on them.
    "obsolete": false,
    // Path of mod's files relative to the modinfo.json file. The game automatically loads all files from the folder with modinfo.json,
    // and all the subfolders, so this field is only useful when you for whatever reason want to stick your modinfo.json in a subfolder of your mod.
    "path": "../common-data/"
  }
]
```
