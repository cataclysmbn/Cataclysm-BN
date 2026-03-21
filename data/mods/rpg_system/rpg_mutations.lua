gdebug.log_info("RPG System: Loading Mutations")

local Mutation = {}
Mutation.__index = Mutation

function Mutation.new(config)
  local self = setmetatable({}, Mutation)
  self.id = config.id
  self.type = config.type -- "class" or "trait"
  self.symbol = config.symbol or ""
  self.is_prestige = config.is_prestige or false
  self.requirements = config.requirements or {} -- { stats = { STR = 8 }, skills = { melee = 6 }, level = 10 }
  self.stat_bonuses = config.stat_bonuses or {} -- { str = 0.55, dex = 0.2, int = 0.1, per = 0.15, speed = 1 }
  self.periodic_bonuses = config.periodic_bonuses or {} -- { fatigue = -0.5, stamina = 20, thirst = -1.5, rad = -0.5, healthy_mod = 0.1, power_level = 1 }
  self.kill_monster_bonuses = config.kill_monster_bonuses or {} -- { heal_percent = 0.5 }
  self.base_class = config.base_class -- for prestige classes (the class to remove when selecting this)
  return self
end

function Mutation:get_mutation_id() return MutationBranchId.new(self.id) end

local MUTATIONS = {
  -- Base Classes
  RPG_WARRIOR = Mutation.new({
    id = "RPG_WARRIOR",
    type = "class",
    symbol = "⚔",
    requirements = { stats = { STR = 8 } },
    stat_bonuses = { str = 0.55, dex = 0.2, int = 0.1, per = 0.15 },
    periodic_bonuses = { stamina = 150 },
  }),

  RPG_MAGE = Mutation.new({
    id = "RPG_MAGE",
    type = "class",
    symbol = "✦",
    requirements = { stats = { INT = 8 } },
    stat_bonuses = { str = 0.1, dex = 0.05, int = 0.55, per = 0.3 },
  }),

  RPG_ROGUE = Mutation.new({
    id = "RPG_ROGUE",
    type = "class",
    symbol = "◈",
    requirements = { stats = { DEX = 8 } },
    stat_bonuses = { str = 0.1, dex = 0.55, int = 0.15, per = 0.2 },
  }),

  RPG_SCOUT = Mutation.new({
    id = "RPG_SCOUT",
    type = "class",
    symbol = "➳",
    requirements = { stats = { PER = 8 } },
    stat_bonuses = { str = 0.15, dex = 0.3, int = 0.05, per = 0.5, speed = 1 },
  }),

  -- Warrior Prestige Classes
  RPG_BERSERKER = Mutation.new({
    id = "RPG_BERSERKER",
    type = "class",
    symbol = "★",
    is_prestige = true,
    base_class = "RPG_WARRIOR",
    requirements = {
      level = 10,
      stats = { STR = 12, DEX = 8 },
      skills = { melee = 6, unarmed = 5 },
    },
    stat_bonuses = { str = 0.75, dex = 0.45, int = 0.1, per = 0.2 },
    periodic_bonuses = { stamina = 150 },
  }),

  RPG_GUARDIAN = Mutation.new({
    id = "RPG_GUARDIAN",
    type = "class",
    symbol = "★",
    is_prestige = true,
    base_class = "RPG_WARRIOR",
    requirements = {
      level = 10,
      stats = { STR = 12, PER = 8 },
      skills = { dodge = 6, firstaid = 4 },
    },
    stat_bonuses = { str = 0.6, dex = 0.15, int = 0.15, per = 0.6 },
    periodic_bonuses = { stamina = 150 },
  }),

  -- Mage Prestige Classes
  RPG_MYSTIC = Mutation.new({
    id = "RPG_MYSTIC",
    type = "class",
    symbol = "★",
    is_prestige = true,
    base_class = "RPG_MAGE",
    requirements = {
      level = 10,
      stats = { INT = 12, PER = 8 },
      skills = { spellcraft = 6 },
    },
    stat_bonuses = { str = 0.1, dex = 0.2, int = 0.75, per = 0.45 },
  }),

  RPG_SCHOLAR = Mutation.new({
    id = "RPG_SCHOLAR",
    type = "class",
    symbol = "★",
    is_prestige = true,
    base_class = "RPG_MAGE",
    requirements = {
      level = 10,
      stats = { INT = 12, PER = 8 },
      skills = { computer = 6 },
    },
    stat_bonuses = { str = 0.05, dex = 0.15, int = 0.8, per = 0.5 },
  }),

  -- Rogue Prestige Classes
  RPG_ACROBAT = Mutation.new({
    id = "RPG_ACROBAT",
    type = "class",
    symbol = "★",
    is_prestige = true,
    base_class = "RPG_ROGUE",
    requirements = {
      level = 10,
      stats = { DEX = 12, PER = 8 },
      skills = { dodge = 6, melee = 5 },
    },
    stat_bonuses = { str = 0.1, dex = 0.8, int = 0.2, per = 0.4, speed = 1.5 },
  }),

  RPG_ASSASSIN = Mutation.new({
    id = "RPG_ASSASSIN",
    type = "class",
    symbol = "★",
    is_prestige = true,
    base_class = "RPG_ROGUE",
    requirements = {
      level = 10,
      stats = { DEX = 12, INT = 8 },
      skills = { throw = 5, cooking = 4 },
    },
    stat_bonuses = { str = 0.1, dex = 0.75, int = 0.35, per = 0.3, speed = 1.5 },
  }),

  -- Scout Prestige Classes
  RPG_RANGER = Mutation.new({
    id = "RPG_RANGER",
    type = "class",
    symbol = "★",
    is_prestige = true,
    base_class = "RPG_SCOUT",
    requirements = {
      level = 10,
      stats = { PER = 12, DEX = 10 },
      skills = { archery = 6, survival = 5 },
    },
    stat_bonuses = { str = 0.15, dex = 0.45, int = 0.1, per = 0.8, speed = 1 },
    periodic_bonuses = { thirst = -0.05 },
  }),

  RPG_CRAFTSMAN = Mutation.new({
    id = "RPG_CRAFTSMAN",
    type = "class",
    symbol = "★",
    is_prestige = true,
    base_class = "RPG_SCOUT",
    requirements = {
      level = 10,
      stats = { PER = 12, INT = 10 },
      skills = { fabrication = 5, mechanics = 5 },
    },
    stat_bonuses = { str = 0.2, dex = 0.3, int = 0.4, per = 0.6 },
  }),

  -- Traits
  RPG_TRAIT_BIONIC_SYMBIOTE = Mutation.new({
    id = "RPG_TRAIT_BIONIC_SYMBIOTE",
    type = "trait",
    requirements = { stats = { INT = 10 } },
    periodic_bonuses = { power_level = 2 },
  }),

  RPG_TRAIT_VITAL_ESSENCE = Mutation.new({
    id = "RPG_TRAIT_VITAL_ESSENCE",
    type = "trait",
    requirements = { level = 5, stats = { PER = 12 } },
    periodic_bonuses = { healthy_mod = 0.02 },
  }),

  RPG_TRAIT_RADIOACTIVE_BLOOD = Mutation.new({
    id = "RPG_TRAIT_RADIOACTIVE_BLOOD",
    type = "trait",
    requirements = {},
    periodic_bonuses = { rad = -0.5 },
  }),

  RPG_TRAIT_IRON_HIDE = Mutation.new({
    id = "RPG_TRAIT_IRON_HIDE",
    type = "trait",
    requirements = { level = 5, stats = { STR = 12 } },
  }),

  RPG_TRAIT_LIGHTWEIGHT = Mutation.new({
    id = "RPG_TRAIT_LIGHTWEIGHT",
    type = "trait",
    requirements = { level = 5, stats = { DEX = 12 } },
  }),

  RPG_TRAIT_PACK_MULE = Mutation.new({
    id = "RPG_TRAIT_PACK_MULE",
    type = "trait",
    requirements = { level = 5, stats = { STR = 14 } },
  }),

  RPG_TRAIT_TIRELESS = Mutation.new({
    id = "RPG_TRAIT_TIRELESS",
    type = "trait",
    requirements = { stats = { STR = 10 } },
  }),

  RPG_TRAIT_MANA_FONT = Mutation.new({
    id = "RPG_TRAIT_MANA_FONT",
    type = "trait",
    requirements = { level = 5, stats = { INT = 14 } },
  }),

  RPG_TRAIT_BLINK_STEP = Mutation.new({
    id = "RPG_TRAIT_BLINK_STEP",
    type = "trait",
    requirements = { level = 5, stats = { DEX = 14 } },
  }),

  RPG_TRAIT_NATURAL_HEALER = Mutation.new({
    id = "RPG_TRAIT_NATURAL_HEALER",
    type = "trait",
    requirements = { level = 5, stats = { PER = 12 } },
  }),

  RPG_TRAIT_ADAPTIVE_BIOLOGY = Mutation.new({
    id = "RPG_TRAIT_ADAPTIVE_BIOLOGY",
    type = "trait",
    requirements = {},
  }),

  RPG_TRAIT_GLASS_CANNON = Mutation.new({
    id = "RPG_TRAIT_GLASS_CANNON",
    type = "trait",
    requirements = { level = 10, stats = { DEX = 16 } },
  }),

  RPG_TRAIT_JUGGERNAUT = Mutation.new({
    id = "RPG_TRAIT_JUGGERNAUT",
    type = "trait",
    requirements = { level = 10, stats = { STR = 18 } },
  }),

  RPG_TRAIT_ARCANE_BATTERY = Mutation.new({
    id = "RPG_TRAIT_ARCANE_BATTERY",
    type = "trait",
    requirements = { level = 10, stats = { INT = 16 } },
  }),

  RPG_TRAIT_IRON_FISTS = Mutation.new({
    id = "RPG_TRAIT_IRON_FISTS",
    type = "trait",
    requirements = { level = 5, stats = { STR = 12 } },
  }),

  RPG_TRAIT_EFFICIENT_METABOLISM = Mutation.new({
    id = "RPG_TRAIT_EFFICIENT_METABOLISM",
    type = "trait",
    requirements = {},
  }),

  RPG_TRAIT_COMBAT_REFLEXES = Mutation.new({
    id = "RPG_TRAIT_COMBAT_REFLEXES",
    type = "trait",
    requirements = { level = 5, stats = { DEX = 12 } },
  }),

  RPG_TRAIT_ACROBAT = Mutation.new({
    id = "RPG_TRAIT_ACROBAT",
    type = "trait",
    requirements = { level = 5, stats = { DEX = 14 } },
  }),

  RPG_TRAIT_TIRELESS_WORKER = Mutation.new({
    id = "RPG_TRAIT_TIRELESS_WORKER",
    type = "trait",
    requirements = { stats = { STR = 10 } },
  }),

  RPG_TRAIT_RAPID_METABOLISM = Mutation.new({
    id = "RPG_TRAIT_RAPID_METABOLISM",
    type = "trait",
    requirements = { level = 5, stats = { STR = 12 } },
  }),

  RPG_TRAIT_SCENTLESS = Mutation.new({
    id = "RPG_TRAIT_SCENTLESS",
    type = "trait",
    requirements = { stats = { PER = 10 } },
  }),

  RPG_TRAIT_CLOTTING_FACTOR = Mutation.new({
    id = "RPG_TRAIT_CLOTTING_FACTOR",
    type = "trait",
    requirements = { stats = { STR = 10 } },
  }),

  RPG_TRAIT_REGENERATOR = Mutation.new({
    id = "RPG_TRAIT_REGENERATOR",
    type = "trait",
    requirements = { level = 5, stats = { STR = 14, PER = 10 } },
  }),

  RPG_TRAIT_MASTER_CRAFTSMAN = Mutation.new({
    id = "RPG_TRAIT_MASTER_CRAFTSMAN",
    type = "trait",
    requirements = { level = 5, stats = { INT = 12, DEX = 12 } },
  }),

  RPG_TRAIT_PACK_RAT = Mutation.new({
    id = "RPG_TRAIT_PACK_RAT",
    type = "trait",
    requirements = { stats = { STR = 10 } },
  }),

  RPG_TRAIT_NATURAL_BUTCHER = Mutation.new({
    id = "RPG_TRAIT_NATURAL_BUTCHER",
    type = "trait",
    requirements = { stats = { DEX = 10, PER = 10 }, skills = { survival = 2 } },
  }),

  RPG_TRAIT_VAMPIRIC = Mutation.new({
    id = "RPG_TRAIT_VAMPIRIC",
    type = "trait",
    requirements = { level = 10, stats = { PER = 16 } },
    kill_monster_bonuses = { heal_percent = 0.5 },
  }),
}

-- Derived lists
local ALL_CLASS_IDS = {}
local ALL_TRAIT_IDS = {}
local BASE_CLASS_IDS = {}
local PRESTIGE_CLASS_IDS = {}
local STAT_BONUS_IDS = {}
local PERIODIC_BONUS_IDS = {}
local KILL_MONSTER_BONUS_IDS = {}

-- Function to register a mutation into the appropriate tracking lists
local function register_mutation(mutation)
  local mutation_id = mutation:get_mutation_id()

  if mutation.type == "class" then
    table.insert(ALL_CLASS_IDS, mutation_id)
    if mutation.is_prestige then
      table.insert(PRESTIGE_CLASS_IDS, mutation_id)
    else
      table.insert(BASE_CLASS_IDS, mutation_id)
    end
  elseif mutation.type == "trait" then
    table.insert(ALL_TRAIT_IDS, mutation_id)
  end

  if next(mutation.stat_bonuses) ~= nil then table.insert(STAT_BONUS_IDS, mutation_id) end

  if next(mutation.periodic_bonuses) ~= nil then table.insert(PERIODIC_BONUS_IDS, mutation_id) end

  if next(mutation.kill_monster_bonuses) ~= nil then table.insert(KILL_MONSTER_BONUS_IDS, mutation_id) end
end

-- Register all built-in mutations
for id, mutation in pairs(MUTATIONS) do
  register_mutation(mutation)
end

return {
  Mutation = Mutation,
  MUTATIONS = MUTATIONS,
  ALL_CLASS_IDS = ALL_CLASS_IDS,
  ALL_TRAIT_IDS = ALL_TRAIT_IDS,
  BASE_CLASS_IDS = BASE_CLASS_IDS,
  PRESTIGE_CLASS_IDS = PRESTIGE_CLASS_IDS,
  STAT_BONUS_IDS = STAT_BONUS_IDS,
  PERIODIC_BONUS_IDS = PERIODIC_BONUS_IDS,
  KILL_MONSTER_BONUS_IDS = KILL_MONSTER_BONUS_IDS,
  register_mutation = register_mutation,
}
