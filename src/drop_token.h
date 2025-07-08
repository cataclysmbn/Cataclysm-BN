#pragma once

#include <iostream>

#include "calendar.h"

class JsonOut;
class JsonIn;

class item_drop_token
{
    public:
        // TODO: private
        time_point turn = calendar::turn_zero;
        int drop_number = 0;
        int parent_number = 0;

        item_drop_token() : item_drop_token( time_point(), 0, 0 )
        {}
        item_drop_token( time_point turn, int drop_number, int parent_number )
            : turn( turn )
            , drop_number( drop_number )
            , parent_number( parent_number )
        {}

        bool operator==( const item_drop_token &other ) const {
            return turn == other.turn
                   && drop_number == other.drop_number
                   && parent_number == other.parent_number;
        }
        bool operator!=( const item_drop_token &other ) const {
            return !( *this == other );
        }
        bool is_child_of( const item_drop_token &other ) const {
            return turn == other.turn
                   && parent_number == other.drop_number
                   && drop_number != other.drop_number;
        }
        bool is_sibling_of( const item_drop_token &other ) const {
            return turn == other.turn
                   && parent_number == other.parent_number &&
                   !is_child_of( other );
        }

        void serialize( JsonOut &jsout ) const;
        void deserialize( JsonIn &jsin );
};

class drop_token_provider
{
    private:
        time_point last_turn = calendar::before_time_starts;
        int last_drop = 0;
    public:
        drop_token_provider() = default;

        item_drop_token make_next( time_point turn );
        void clear();

        void serialize( JsonOut &jsout ) const;
        void deserialize( JsonIn &jsin );
};

namespace drop_token
{

drop_token_provider &get_provider();

} // namespace drop_token

std::ostream &operator<<( std::ostream &os, const item_drop_token &dt );


