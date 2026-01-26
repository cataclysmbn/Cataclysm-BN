gdebug.log_info("SPAWN HOOKS TEST")

game.add_hook("on_creature_spawn", function(params) return print("Creature Spawned") end)
game.add_hook("on_monster_spawn", function(params) return print(params.monster:get_type():str().." Spawned") end)
game.add_hook("on_npc_spawn", function(params) return print(params.npc.name.." Spawned") end)

game.add_hook("on_creature_loaded", function(params) return print("Creature Loaded") end)
game.add_hook("on_monster_loaded", function(params) print(params.monster:get_type():str().." Loaded") end)
game.add_hook("on_npc_loaded", function(params) return print(params.npc.name.." Loaded") end)



