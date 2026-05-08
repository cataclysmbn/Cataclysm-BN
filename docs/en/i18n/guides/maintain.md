# Merging and managing translations for maintainers

Several steps need to be done in the correct order to correctly merge and maintain the translation
files.

There are scripts available for these, so usually the process will be as follows:

1. Download the translations in `.po` format.
2. Put them in `lang/incoming/`, ensuring they are named consistently with the files in `lang/po/`.
3. Run `lang/update_pot.sh` to update `lang/po/cataclysm-BN.pot` (requires python with `polib` and
   `luaparser` modules installed).
4. Run `lang/merge_po.sh` to update `lang/po/*.po`. (This is only used to test translations locally
   as the project now uses Transifex for translation)

   This will also merge the translations from `lang/incoming/`.

These steps should be enough to keep the translation files up-to-date.

To compile the .po files into `.mo` files for use, run `lang/compile_mo.sh`. It will create a
directory in `lang/mo/` for each language found.

Also note that both `lang/merge_po.sh` and `lang/compile_mo.sh` accept arguments specifying which
languages to merge or compile. So to compile only the translation for, say, Traditional Chinese
(zh_TW), one would run `lang/compile_mo.sh zh_TW`.

After compiling the appropriate .mo file, if the language has been selected in game settings, the
translations will be automatically used when you run cataclysm.

When `System language` is selected in settings, the game tries to use language that matches system
language based on language definitions file `data/raw/languages.json`.

If you're testing translations for a new language, or the language does not show up in settings,
make sure it has its own entry in the definitions file.
