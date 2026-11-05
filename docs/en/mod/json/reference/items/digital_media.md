# Digital media

Digital files are items stored inside USB drives, SD cards, hard drives, or a reader. The
e-ink tablet, laptops, wrist computers, and smartphones provide a **Download
data from storage media** action that copies files into internal storage.
**Browse digital files** offers information, study, and item actions for
installed files. Only options supported by the selected file are shown. Each
use action appears under its configured name; cooldown and availability details
appear in the menu description area.

**Copy files to storage media** copies all installed files onto an unencrypted
USB drive, SD card, or hard drive. For a single file, choose **Browse digital files → file →
Copy to storage media**. The
reader must be powered. Existing destination files are retained, and the original
files stay on the reader. Copies preserve their entry counts, metadata, and file
identities, including shared cooldowns. The media can then be imported onto
another reader. Encrypted media are excluded from the destination picker.

Readers connect to removable storage through `digital_reader_ports`, a comma-separated
list matched against the storage's `digital_storage_ports`. USB drives use `USB`,
SD cards use `SD`, and hard drives use `HARD_DRIVE`. Laptops support USB and hard
drives, smartphones support SD cards, and wrist computers support all three.
Storage without a port requirement remains compatible with any reader.

## Files and readers

Choose **Scan book** on a reader to digitize a physical book in your inventory.
Scanning takes one minute per 50 ml of book volume, with a minimum of one minute,
and consumes one device use per minute. Losing the book, reader, power, or enough
light to see fine details interrupts the scan without producing a file. The
physical book is preserved.

The resulting digital copy retains the original book's type, skills, recipes,
morale effects, and manual actions. Its instance flags mark it as `DIGITAL`,
`IRREMOVABLE`, and `NO_DROP`, and its volume is 1 ml. It can be studied, copied,
exported, and deleted like other installed files. Scanning the same title again
is unnecessary while a copy is installed on the reader.

- `DIGITAL`: the item is a file. It can only be placed in digital storage, and
  requires installation on a powered reader to study or activate it.
- `DIGITAL_STORAGE`: the item accepts digital contents and rejects physical
  contents. Dropping the media does not spill its files.
- `DIGITAL_READER`: installed files can supply recipe references and be used
  while the device has sufficient power for one use. Readers should also have
  `DIGITAL_STORAGE` and a tool battery configuration.
- `DIGITAL_MUSIC`: the file contains music tracks for the reader's playlist.

Derive instructional files from `digital_instruction`, a `BOOK` abstract with
zero mass, a minimum volume of 1 ml, and the `DIGITAL`, `IRREMOVABLE`, and `NO_DROP` flags:

```json
{
  "id": "my_cooking_lesson",
  "copy-from": "digital_instruction",
  "type": "BOOK",
  "name": { "str": "cooking tutorial" },
  "description": "A saved lesson in basic kitchen techniques.",
  "skill": "cooking",
  "required_level": 0,
  "max_level": 3,
  "time": "20 m",
  "fun": 1
}
```

Associate recipes through their existing `book_learn` field, using the digital
item ID as the book ID. Installed instructional files supply recipe references
from a powered reader without permanently teaching them. Studying uses the
normal book activity, including skill limits and the recipe-learning option, and
consumes one device use per minute. Losing the device or its power stops the
activity.

Non-book files can derive from `software`. Override `container` to `"null"` when
defining files intended for a media contents group; otherwise the software base
wraps spawned files in a USB drive. Existing JSON or Lua `use_action`s are
listed individually under their configured action names, including `menu_text`
overrides. Actions receive the file as their item and retain their normal cost
and behavior. The tablet additionally charges one device use
for its browse operation. Destructive actions can therefore remove their file;
actions intended to be repeatable should not consume it.

`digital_hello_world` (`hello_world.exe`) is a minimal executable example found
on personal SD cards. Copy it to a powered reader and choose **Browse digital
files → hello_world.exe → Print greeting** to print **Hello, world!**. Its JSON
`manualnoise` action uses `noise: 0` to display a message silently. The file is
preserved and can be run repeatedly.

All digital settings below are per-instance `item_vars`. JSON values must be
strings, including numeric settings. They are copied with the file and saved
with it. Lua can edit them through the existing item variable methods:

```lua
file:set_var_num("digital_music_morale_cap", 40)
file:set_var_str("digital_recipes", '[{"id":"3d_printer_basic","skill_level":7}]')
```

Recipe selections and media loot are generated once. Changing the settings does
not replace an already resolved selection or reroll initialized media. To
explicitly regenerate a recipe file, erase `DIGITAL_RECIPES` and reinstall it on
a reader. Older saves receive missing default digital settings while retaining
saved overrides and recipe selections.

## Recipe reference files

Reference files are `GENERIC` items, separate from instructional `BOOK` files.
They supply crafting recipes through a powered reader without teaching a skill
or memorizing a recipe. The 3D printing infographic archive is one example.
Choose **View recipes** in its file menu to inspect the available recipes.

Recipe definitions live directly on each item. `digital_recipes` is a
JSON-encoded array of recipe IDs and optional required skill levels:

```json
"item_vars": {
  "digital_recipes": "[{\"id\":\"3d_printer_basic\",\"skill_level\":7}]"
}
```

Omitting `skill_level` uses the recipe's difficulty. `digital_random_recipes`
contains JSON-encoded selection options and can include recipes added by mods:

```json
"item_vars": {
  "digital_random_recipes": "{\"count\":1,\"category\":\"CC_FOOD\",\"min_difficulty\":0,\"max_difficulty\":3,\"difficulty_chance_offset\":0}"
}
```

Both variables can be present on the same file. No separate recipe-file JSON
object type or registry is needed. An empty string disables a definition.

`category` is optional. Difficulty defaults to an unrestricted nonnegative
range, and `count` defaults to one. An optional `candidates` array restricts the
pool to specific recipe IDs. Recipes marked `never_learn` are excluded unless
`include_unlearnable` is true. Random selections do not repeat fixed entries or
other selections on the same file.

When `difficulty_chance_offset` is present, each matching recipe has a
one-in-`max(1, difficulty + offset)` chance to enter the pool before selection.
Omit this field to include every matching candidate. The standard cooking
archive uses food recipes of difficulty 0–3 with offset 0; the scientific
archive uses difficulty 3 and above with offset 1, matching the old random
recipe filters. Item-group entries control how often these files appear.

Selections are generated once when the file enters storage and are saved in the
file. Downloading, exporting, and loading the file retain that selection. An
empty result is retained too, rather than rerolled on every access.

## Digital maps

`digital_urbex_map` is a saved urban exploration forum guide, found among personal
files, encrypted card contents, and salvaged terminal files. Copy it onto a
powered reader, then select **Browse digital files → saved urbex forum guide
→ Read**. It inherits the paper urban explorer's map's reveal action,
including its coverage and location filters. A `reveal_map_center_omt` item
variable, when present, travels with the file when it is copied or saved.

`digital_evacuation_map` is a saved emergency evacuation guide found on personal
SD cards, encrypted cards, and salvaged hard drives. Open it on a powered reader
to mark refugee centers, evac shelters, and roads within 180 overmap tiles of
the map's saved center or your current location. It covers both refugee center
layouts and the standard, expanded, and vandalized evac shelters.

## Salvaged hard drives

Disassembling intact or broken computer terminals, including the furniture
versions, recovers a `hard_drive`. Each drive generates three to seven random
files from `digital_content_terminal` when first accessed. The pool includes
photos, music, instructional files, executables, and recipe archives. Files are
rolled once and retained when copied or saved.

Hard drives use the same removable-storage support as USB drives and SD cards:
download their files onto a powered reader, copy additional files back, or encrypt
them through the reader's storage menu. Genome workstations can also store
sample files on unencrypted hard drives.

## Generating content

Media can name an ordinary item group using the `digital_content` item variable:

```json
"item_vars": { "digital_content": "my_digital_content" }
```

The reader rolls this group on the first access to the media, then stores the
result as real item contents. Use collection probabilities, distributions, and
nested groups as usual. Generated items must be `DIGITAL`. A file packaged in a
single-file storage device, such as software's default USB drive, is extracted
from that wrapper. Already initialized legacy SD cards
keep their original contents rather than receiving another loot roll.

Saved text files reuse the newspaper, flier, character-sheet, radio-archive, and
lab-note snippet categories. Choose **Read saved text** in the file browser to
open a scrolling view. The selected text is retained when copied or saved.
Saved lab emails appear in scientific storage; personal files also include
fliers, character sheets, and radio transcripts.
Ordinary software can appear in the random file pools, with technical programs
more common in scientific storage.

`digital_encryption_chance` is a one-in-N chance to set `MC_ENCRYPTED` when
initializing a media content group. Encryption is an instance flag and does not
change the device's item type. Item groups can also set `MC_ENCRYPTED` through
`custom-flags`, including on USB drives.

The `sd_memory_card_personal`, `sd_memory_card_encrypted`, and
`sd_memory_card_science` loot groups all spawn `sd_memory_card` items. Their
flags select personal, encrypted, or science content, preserving the different
loot profiles without separate card item types. Personal cards have a one-in-eight
chance of encryption. Plain `sd_memory_card` items start empty.

For predetermined contents, use existing item-group modifiers instead:

```json
{ "item": "sd_memory_card", "contents-item": "digital_cooking_show" }
```

Files are copied without consuming the source. Identical copies are skipped;
file identity and item variables are preserved, so copying a photo album again
does not reset the viewer's cooldown. Encryption prevents downloading until the
SD card is decrypted. Formatting or triggering a card's data-erasure protection
deletes its files.

USB drives and SD cards keep the same item type whether empty, populated, or
encrypted. Encrypted media display an **(encrypted)** suffix and hide their
contents. **Encrypt storage media** encrypts either kind of media, and
**Decrypt storage media** uses the existing computer-skill check to unlock it.
Encrypted media cannot be copied to or downloaded from until unlocked.
Copying files preserves existing data on both media and readers. Old SD-card
variants migrate to `sd_memory_card`, preserving files and encryption.

## Music and photos

The `digital_collection` abstract supplies `CAN_HAVE_CHARGES`, which preserves
track and photo counts during saving and loading.

These collections are ordinary `GENERIC` digital items. Their item-group
`charges` field specifies the number of tracks, including random ranges:

```json
{
  "id": "my_media_files",
  "type": "item_group",
  "subtype": "collection",
  "entries": [
    { "item": "digital_music_collection", "prob": 35, "charges": [5, 15] }
  ]
}
```

Music collections set `digital_music_morale_cap` in their `item_vars`. The
playing collection's morale cap is limited by its own track count. The
`digital_builtin_music` group defines the preloaded collection on existing
reader devices. The `digital_content_reader` group combines it with random
personal files, including photos, lessons, executables, and recipe archives.
Readers retain these files and their menu when switched to music or light modes.
Music flavor text is in the existing `<music_description>` snippet category,
including its genre and instrument templates. `digital_music_cooldown_seconds`
sets the pause between listening sessions (five minutes for the standard
collection). Stopping playback preserves every track, and the reader menu
remains available during playback. Each collection has its own cooldown for each
listener. Listening to one does not block another or reduce its morale reward.
Use **Browse digital files → Listen** to choose a collection. The general music
action automatically picks an available collection. Copies of the same file
share a cooldown, even on another reader.

Camera photographs and monster collection entries are viewed through the
camera or their saved camera photo collection. Viewing either kind of photo can
improve morale, with a shared ten-minute cooldown per character. Synthetic
digital photo albums and their morale action have been removed.

## First-pass limits

Media capacity is not measured in bytes. The tablet imports USB drives, SD cards, and hard drives directly; format compatibility is not modeled yet. Other media,
such as DVDs, can later use the same flags and content groups without adding
media-specific file types. Files must be copied into a reader before use; an
adjacent USB or SD card alone supplies no recipe references.

Old `MC_MUSIC` and `EIPC_MUSIC` counters are converted into music collection
items once, preserving their counts. Synthetic `MC_PHOTOS` and `EIPC_PHOTOS`
counters are discarded. Legacy recipe
tokens (`MC_RECIPE`) become cooking or scientific reference files using the item-variable definitions above. Existing tablet recipe references remain readable for save
compatibility. Camera captures are stored in a `digital_camera_photo_collection`
file on the camera or phone. **Camera photos** and **Browse digital files →
Browse captured photos** reopen the individual photographs with their original
descriptions and quality. The computer's monster collection includes creatures
recorded in these files, including photographs taken with the same device.
Camera exports copy the collection onto an unencrypted USB drive or SD card.
Newer captures can be exported again without changing previously copied files. Legacy camera photos
migrate into a collection when the device is used. Older tablet and SD-card
photo metadata remains readable through its existing import path. Existing
mission-terminal downloads retain their original behavior, including overwriting
the chosen USB drive.

## Genome samples

USB drives and SD cards store `digital_genome_sample` files. These preserve the specimen
ID, name, and scan progress when downloaded to a reader or copied to an SD card.
The genome sampler and workstation can use samples carried inside digital
storage, including USB drives, SD cards, and readers. Scanning more specimens
updates a partial file without requiring another storage device; workstation
research updates the selected file. The workstation can copy a sample onto
another USB drive or unencrypted SD card without erasing existing files. Updated
samples can be copied again.

Old genome drives, including prefilled rat and dire wolf drives, migrate their
stored specimen variables into a sample file when loaded or accessed, and become
ordinary USB drives. Migration preserves partial progress and other drive data.
