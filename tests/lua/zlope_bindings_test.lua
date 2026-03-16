test_data["brute_faction"] = gapi.monster_type_default_faction(MonsterTypeId.new("mon_zombie_brute"))
test_data["hulk_faction"] = gapi.monster_type_default_faction(MonsterTypeId.new("mon_zombie_hulk"))
test_data["scientist_faction"] = gapi.monster_type_default_faction(MonsterTypeId.new("mon_zombie_scientist"))
test_data["brute_has_bionic_group"] =
  gapi.monster_type_has_harvest_entry_type(MonsterTypeId.new("mon_zombie_brute"), "bionic_group")
test_data["hulk_has_bionic_group"] =
  gapi.monster_type_has_harvest_entry_type(MonsterTypeId.new("mon_zombie_hulk"), "bionic_group")
test_data["kevlar_has_bionic_group"] =
  gapi.monster_type_has_harvest_entry_type(MonsterTypeId.new("mon_zombie_kevlar_2"), "bionic_group")
test_data["zapper_has_bionic_group"] =
  gapi.monster_type_has_harvest_entry_type(MonsterTypeId.new("mon_zombie_electric"), "bionic_group")
