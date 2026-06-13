local mon = gapi.place_monster_at(test_data["monster_id"], test_data["pos"])

test_data["monster_spawned"] = mon ~= nil

if mon then
  test_data["monster_type"] = mon:get_type():str()
  test_data["upgrade_time"] = mon:get_upgrade_time()
end
