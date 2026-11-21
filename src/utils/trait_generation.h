#pragma once

/// Counter that changes whenever the traits of a character may change: a mutation is gained or
/// lost, or an enchantment grants or removes one. Lua caches of trait checks compare it.
auto character_trait_generation() -> int;
