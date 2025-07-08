#pragma once

#include <list>

#include "safe_reference.h"

class JsonIn;
class JsonOut;

struct iuse_location {
    iuse_location() = default;
    ~iuse_location() = default;
    iuse_location( item &loc, int count ) : loc( &loc ), count( count ) { }

    safe_reference<item> loc;
    int count = 0;

    void serialize( JsonOut &jsout ) const;
    void deserialize( JsonIn &jsin );
};
using iuse_locations = std::vector<iuse_location>;

using drop_location = iuse_location;
using drop_locations = std::vector<drop_location>;


