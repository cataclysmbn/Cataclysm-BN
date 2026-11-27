#pragma once

#include "addiction_type.h"
#include "pldata.h"

#include <string>

class Character;

// Minimum intensity before effects are seen
constexpr int MIN_ADDICTION_LEVEL = 3;
constexpr int MAX_ADDICTION_LEVEL = 20;

// cancel_activity is called when the addiction effect wants to interrupt the player
// with an optional pre-translated message.
void addict_effect( Character &u, addiction &add );

std::string addiction_type_name( const addiction_type_id &cur );

std::string addiction_name( const addiction &cur );

morale_type addiction_craving( const addiction_type_id &cur );

// The JSON id of a legacy hardcoded addiction type (e.g. add_type::MARLOSS_R ->
// "marloss_r"). Returns a null id for add_type::NONE.
addiction_type_id addiction_id_from_legacy( add_type legacy );

// The legacy enum a JSON-defined addiction type aliases, add_type::NONE for
// types without one.
add_type legacy_add_type( const addiction_type_id &type );

std::string addiction_text( const addiction &cur );
