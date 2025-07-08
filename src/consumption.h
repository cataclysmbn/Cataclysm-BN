#pragma once

#include <list>

#include "calendar.h"
#include "character.h"
#include "type_id.h"

class item;
class JsonOut;
class JsonIn;

struct consumption_event {
    time_point time;
    itype_id type_id;
    uint64_t component_hash;

    consumption_event() = default;
    consumption_event( const item &food );
    void serialize( JsonOut &json ) const;
    void deserialize( JsonIn &jsin );
};

struct consumption_history_t {
    std::list<consumption_event> elems;

    void serialize( JsonOut &json ) const;
    void deserialize( JsonIn &jsin );
};
bool query_consume_ownership( item &target, avatar &you );

void consume_poison( Character &consumer, item &food );
