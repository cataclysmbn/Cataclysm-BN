# Adding Sound Hooks

How to thread a new gameplay event into the sound system so that both gameplay logic (NPC reaction, sound markers) and actual audio stay in sync.

## Pipeline in Brief

- C++ code calls `sounds::sound` with a map position, volume, category, and description. Those events drive monster AI, safe-mode checks, and the UI markers you see.
- If you pass a non-empty `id`/`variant`, `sounds::process_sound_markers` forwards the event to `sfx::play_variant_sound`, which looks up a matching `sound_effect` in the active soundpack.
- Soundpacks live under `data/sound/<pack>` and describe `sound_effect` entries (id + variant + file). The reference list of ids/variants is in `../../mod/json/reference/soundpacks.md`.
- For looping ambience or UI bleeps that do not need in-world propagation, call the `sfx::` helpers directly.
- Mods can ship `sound_effect` JSON too. File paths resolve relative to the JSON file that declared them (falling back to the mod root), so you can bundle audio with your mod without touching the base soundpack.

## Add a New Positional Sound

1. Pick (or add) a soundpack entry. Example:

   ```json
   [
     {
       "type": "sound_effect",
       "id": "generator",
       "variant": "start",
       "volume": 90,
       "files": ["generator_start.ogg"]
     }
   ]
   ```

2. Emit the sound from the gameplay code. Use the `sounds::sound` overload that carries an `id` and `variant` so the soundpack hook fires, and keep the description localized:

   ```cpp
   auto play_generator_start( const tripoint &src ) -> void
   {
       sounds::sound( src, 30, sounds::sound_t::activity,
                      _( "the generator rumbles to life" ),
                      false, "generator", "start" );
   }
   ```

3. Choose volume and category by matching nearby code: the heard volume is roughly `volume - distance` after weather/hearing modifiers, so 10-20 is subtle, 30-60 is normal actions, and 80+ is alarm-tier. Categories gate messaging and NPC reactions (for example `sound_t::alarm` is high-priority).

4. Use `ambient = true` for noises that should not interrupt activities (background hum, distant traffic).

## Direct `sfx::` Hooks

- For effects that should play only for the local listener (menus, UI toggles, one-off ambience) and should not alert monsters, call `sfx::play_variant_sound` directly. Use `sfx::get_heard_volume` and `sfx::get_heard_angle` when the source is on the map so panning and falloff match other sounds:

  ```cpp
  auto play_generator_loop( const tripoint &src ) -> void
  {
      const auto volume = sfx::get_heard_volume( src );
      const auto angle = sfx::get_heard_angle( src );
      sfx::play_variant_sound( "generator", "loop", volume, angle, 0.95, 1.05 );
  }
  ```

- For looping ambience, reserve a channel with `sfx::play_ambient_variant_sound` and stop/fade it via `fade_audio_channel` or `stop_sound_effect_fade`. Reuse existing channels in `sfx::channel` (for example `indoors_env` or `player_activities`) instead of inventing new integers.

## Testing Checklist

- Enable the target soundpack (Basic ships with only menu clicks; custom packs need the new `sound_effect` entry).
- Trigger the event in-game and confirm you see the expected log text/direction marker plus the audio. Debug builds log the `sound id`/`variant` when sound is enabled.
- If your sound effect is data-driven (JSON only), run `./build-scripts/lint-json.sh`. There is no extra C++ formatting needed beyond the normal style pass.
