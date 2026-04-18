# Music Channel Example

Shows how to glue a simple playlist to `SfxChannel.radio` by watching for the new Lua bindings.

## Features

- Calls `game.play_ambient_variant_sound` with the built-in radio variants.
- Registers a `game.on_sound_channel_end` listener so each track schedules the next one when the channel finishes.
- Logs each transition via `gdebug.log_info` so you can observe the behavior while testing.

## Usage

1. Enable this mod from the mod manager (the id is `music_channel_example`).
2. Start or load a world that includes the default soundpack (Basic or similar).
3. Open the console/log to see messages whenever the radio track changes.
