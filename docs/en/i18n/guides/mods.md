# Translate third-party mods

## Intro

This document aims to give a brief explanation on how to set up and operate mod translation workflow
for Cataclysm: Bright Nights.

For mod localization the game uses custom localization system that is similar to
[GNU gettext](https://www.gnu.org/software/gettext/) and is compatible with GNU gettext MO files.

While it's possible to use Transifex or any other platform or software that supports gettext, this
document only gives examples on how to work with [Poedit](https://poedit.net/) and command-line
[GNU gettext utilities](https://www.gnu.org/software/gettext/).

If you desire an in-depth explanation on PO/POT/MO files or how to work with them using GNU gettext
utilities, see [GNU gettext manual](https://www.gnu.org/software/gettext/manual/gettext.html).

To get some generic tips on translating strings for Cataclysm: Bright Nights and its mods, see
[translation API](../reference/translation).

## A short glossary

### POT file

Portable Object Template file (`.pot`).

This is a text file that contains original (English) strings extracted from mod's JSON and Lua
source files. The POT file is a template used for creating empty or updating existing PO files of
any language.

### PO file

Portable Object file (`.po`).

This is a text file that contains translated strings for one language. The PO files are what
translators work with, and what will be compiled into a MO file.

### MO file

Machine Object file (`.mo`).

This is a binary file that contains translated strings for one language. The MO files are what the
game loads, and where it gets translated strings from.

## Workflow overview

The first translation workflow is as follows:

1. Extract strings from mod JSON and Lua source files into a POT file
2. Create PO file for target language from this POT
3. Fill the PO file with translated strings
4. Compile PO into MO
5. Put the MO into your mod files

As the mod changes with time, so may change its strings. Updating existing translations is done as
follows:

1. Extract strings from mod JSON and Lua source files into a new POT file
2. Update existing PO file from the new POT file
3. Add new or edit existing translated strings in the PO file
4. Compile PO into MO
5. Replace old MO in the mod files with new version

Step 1 in both workflows requires you to set up environment for string extraction (see below).

Steps 2-4 can be done using translation software either by the mod author/maintainer, or by the
translator.

## Setting up environment for string extraction

You'll need Python 3 with `polib` and `luaparser` modules installed (available via `pip`).

Scripts for string extraction can be found in the `lang` subdirectory of the repository:

- `extract_json_strings.py` - main string extraction routines
- `dedup_pot_file.py` - fixes errors in POT file produces by the 1st script
- `extract_mod_strings.bat` (`extract_mod_strings.sh` for Linux/MacOS) - to automate the other 2
  scripts

## Extracting strings

Copy these 3 scripts into the mod's folder and:

- on Windows, double-click `extract_mod_strings.bat`
- on Linux/MacOS, open terminal and run `./extract_mod_strings.sh`

If the process completed without errors, you'll see a new `lang` folder with `extracted_strings.pot`
file inside.

## Creating new PO

Before creating PO file, you need to choose language id.

Open `data/raw/languages.json` to see the list of languages supported by the game.

In this list, each entry has its own id in form of `ln_LN`, where `ln` determines language and
`LN` - dialect. You can either use full `ln_LN` for exact language+dialect match, or `ln` if you
want the game to use your MO regardless of dialect.

### Poedit

1. Open the POT file with Poedit
2. Press "Create new translation" button (should show up near the bottom)
3. In language selection dialog, enter language id you chose
4. Save the file as `path/to/mod/lang/LANG.po` where `LANG` is the same language id

### msginit

```bash
msginit -i lang/index.pot -o lang/LANG.po -l LANG.UTF-8 --no-translator
```

Where `LANG` is the language id you chose

## Updating existing PO

### Poedit

1. Open the PO file with Poedit
2. Choose `Catalog->Update from POT file...` and select the new POT file
3. Save the file

### msgmerge

```bash
msgmerge lang/LANG.po lang/index.pot
```

## Compiling PO into MO

### Poedit

1. Open the PO file with Poedit
2. Make sure MO file will be encoded using UTF-8 (it should do so by default, you can double check
   in `Catalog->Properties->"Translation properties" tab->Charset`).
3. By default, each time PO file is saved Poedit automatically compiles it into MO, but the same can
   also be done explicitly via `File->Compile to MO...`

### msgfmt

```
msgfmt -o lang/LANG.mo lang/LANG.po
```

## Adding MO file to the mod

Create `lang` directory in the mod files directory and put MOs there:

```
mods/
    YourMod/
        modinfo.json
        lang/
            es.mo
            pt_BR.mo
            zh_CN.mo
```

**Note:** Storing your POT/PO files in the same `lang` subdirectory may make it easier to keep track
of them. The game ignores these files, and your mod folder structure will look like this:

```
mods/
    YourMod/
        modinfo.json
        lang/
            extracted_strings.pot
            es.po
            es.mo
            pt_BR.po
            pt_BR.mo
            zh_CN.po
            zh_CN.mo
```

## Miscellaneous notes

### Is it possible to use arbitrary location or names for MO files, like with JSONs?

No. The game looks for MO files with specific names that are located in the `lang` subdirectory of
the mod's `path` directory specified in `modinfo.json` (if not specified, `path` matches the mod's
directory).

However, any mod will automatically try to use any other mod's translation files to translate its
strings. This makes it possible to create mods that are purely "translation packs" for other mods
(or mod collections).

### Reloading translations in a running game

Open debug menu and select `Info...->Reload translations`, and the game will reload all MO files
from disk.

This makes it easy to see how translated string looks in game, provided the translator has a way to
compile MO files.

Example workflow with Poedit:

1. Translate a string
2. Hit Ctrl+S
3. Alt+Tab into the game
4. Reload translation files via debug menu
5. The game now displays translated string

### MO load order

MO load order is as follows:

1. First and always goes base game's MO file, which contains translation strings for UI, hardcoded
   functionality, base "mod" (`data/json/`) and in-repo mods.
2. Then MO files of mods are loaded, in same order as the mod load order.

### Dialects

When loading MO files, the game first looks for the file with exact language and dialect match in
its name. If such file is absent, then it looks for a file with no dialect.

For example, when using `Español (España)` the selection order is

1. `es_ES.mo`
2. `es.mo`

And when using `Español (Argentina)` the selection order is

1. `es_AR.mo`
2. `es.mo`

Thus, `es.mo` would be loaded for either dialect of Spanish if the exact translation files are not
present.

### What if 2 or more mods provide different translations for same string?

Then the game selects which one to use according to this set of rules:

1. If string A's translation has plural forms, but string B's translation does not, then translation
   A is used for both single and plural forms.
2. If both translation A and B have (or both don't have) plural forms, then the first loaded
   translation is used (see MO load order).

If you want a different translation from the one in the base game, or don't want it to conflict with
a string from some other mod, add a translation context to the string in the corresponding JSON
object (see [here](../reference/translation) for which fields support translation context).

### PR that implements mod translations, for reference

https://github.com/cataclysmbn/Cataclysm-BN/pull/505

### [Example of the mod translation](https://github.com/Kenan2000/Bright-Nights-Kenan-Mod-Pack/pull/36)
