local scen = gapi.get_scenario()
test_data["has_scenario"] = scen ~= nil
test_data["scenario_id"] = scen and scen.id or "nil"
test_data["point_cost"] = scen and scen.point_cost or 0
local forced = scen and scen:forced_traits() or {}
test_data["forced_count"] = #forced
test_data["has_nemesis"] = #forced == 1 and forced[1] == MutationBranchId.new("HAS_NEMESIS")
test_data["has_flag"] = scen and scen:has_flag("LONE_START")
