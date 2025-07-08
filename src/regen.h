#pragma once

#include "type_id.h"

class Character;

/// like heal, but actually takes account of
/// - whether limb suffers from being broken without splint
/// - `mending_modifier`
///
/// @return actually healed amount. used for `mod_part_healed_total`
///
/// TODO: merge into `Character::heal`?
auto heal_adjusted( Character &c, const bodypart_id &bp, const int heal ) -> int;


