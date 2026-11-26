local avatar = gapi.get_avatar()
local center = gapi.bub_to_abs(avatar:get_pos_ms()):to_omt()
local expires_at = gapi.current_turn() + TimeDuration.from_minutes(30)

gapi.set_omt_weather_override(center, 0, "lightning", expires_at)

test_data["center"] = center
test_data["has_before"] = gapi.has_omt_weather_override(center)
test_data["weather_before"] = tostring(gapi.get_omt_weather_override(center))
