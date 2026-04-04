local mod = game.mod_runtime[game.current_mod]

local playlist = {
  { id = "radio", variant = "inaudible_chatter" },
  { id = "radio", variant = "static" },
}
local next_track_index = 1
local radio_channel = SfxChannel.radio

local function get_next_track()
  local track = playlist[next_track_index]
  next_track_index = next_track_index % #playlist + 1
  return track
end

local function play_next_track()
  local track = get_next_track()
  gdebug.log_info("Music Channel Example: playing %s/%s on %s", track.id, track.variant, tostring(radio_channel))
  game.play_ambient_variant_sound(track.id, track.variant, 100, radio_channel, 0, -1.0, 0)
end

local function ensure_listener()
  if mod.music_channel_listener then return end

  game.on_sound_channel_end(radio_channel, function(ended_channel)
    if ended_channel ~= radio_channel then return true end

    gdebug.log_info("Music Channel Example: %s ended, queuing next track", tostring(radio_channel))
    play_next_track()
    return true
  end)

  mod.music_channel_listener = true
end

mod.on_game_loaded_hook = function()
  ensure_listener()
  if mod.music_started then return end
  mod.music_started = true
  play_next_track()
end
