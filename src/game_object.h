#pragma once

#include <utility>

#include "detached_ptr.h"
#include "safe_reference.h"
#include "point.h"

class location_inventory;

template<typename T>
class location;
template<typename T>
class location_visitable;

class item;

template<typename T>
class game_object
{
    private:
        friend detached_ptr<T>;
        friend location_ptr<T, true>;
        friend location_ptr<T, false>;
        friend location_inventory;
        friend location_vector<T>;
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
        void set_location( location<T> *own );

        tripoint position( ) const;
        /** Returns the name that will be used when referring to the object in error messages */
        virtual std::string debug_name() const = 0;
};


