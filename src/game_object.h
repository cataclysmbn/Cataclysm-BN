#pragma once

#include <utility>

#include "coordinates.h"
#include "detached_ptr.h"
#include "safe_reference.h"
#include "type_id.h"

class location_inventory;

template<typename T>
class location;
template<typename T>
class location_visitable;

class item;
class mapbuffer;
struct vehicle_part;

template<typename T>
class game_object
{
    private:
        friend detached_ptr<T>;
        friend location_ptr<T, true>;
        friend location_ptr<T, false>;
        friend location_inventory;
        friend location_vector<T>;
        friend class mapbuffer;
        friend struct vehicle_part;
        friend location_visitable<location_inventory>;
        template<typename U>
        friend void ::std::swap( location_vector<U> &, location_vector<U> & ) noexcept ;
    protected:
        location<T> *saved_loc = nullptr;
        location<T> *loc = nullptr;

        game_object() = default;

        game_object( const game_object & ) {}

        void destroy();
        void destroy_in_place();

        void remove_location();

        void resolve_saved_loc();


    public:

        virtual ~game_object() = default;

        detached_ptr<T> detach();

        virtual bool attempt_detach( std::function < detached_ptr<T>( detached_ptr<T> && ) > cb );

        bool is_detached() const;
        bool is_loaded() const;
        bool has_position() const;
        const location<T> *get_location() const;
        void set_location( location<T> *own );

        tripoint_bub_ms bub_pos( ) const;
        tripoint_abs_ms abs_pos( ) const;
        dimension_id get_dimension_id( ) const;
        mapbuffer &get_mapbuffer( ) const;
        /** Returns the name that will be used when referring to the object in error messages */
        virtual std::string debug_name() const = 0;
};
