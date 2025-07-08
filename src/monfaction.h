#pragma once

#include <unordered_map>

#include "catalua_type_operators.h"
#include "enum_traits.h"
#include "type_id.h"

class JsonObject;

enum mf_attitude : int {
    MFA_BY_MOOD = 0,    // Hostile if angry
    MFA_NEUTRAL,        // Neutral even when angry
    MFA_FRIENDLY,       // Friendly
    MFA_HATE,           // Attacks on sight
    NUM_MFA             // Count of attitudes
};

template<>
struct enum_traits<mf_attitude> {
    static constexpr mf_attitude last = NUM_MFA;
};

using mfaction_att_map = std::unordered_map< mfaction_id, mf_attitude >;

namespace monfactions
{
void reset();
void finalize();
void load_monster_faction( const JsonObject &jo );
mfaction_id get_or_add_faction( const mfaction_str_id &id );
} // namespace monfactions

class monfaction
{
    public:
        mfaction_id loadid;
        mfaction_id base_faction;
        mfaction_str_id id;

        mfaction_att_map attitude_map;

        mf_attitude attitude( const mfaction_id &other ) const;

        LUA_TYPE_OPS( monfaction, id );
};


