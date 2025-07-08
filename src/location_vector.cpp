#include "location_vector.h"
#include "item.h"
#include "locations.h"

template<typename T>
location_vector<T>::location_vector( location<T> *loc ) : loc( loc ) {};

template<typename T>
location_vector<T>::location_vector( location<T> *loc,
                                     std::vector<detached_ptr<T>> &from ) : loc( loc )
{
    for( detached_ptr<T> &obj : from ) {
        if( !obj ) {
            continue;
        }
        obj->set_location( &*loc );
        contents.push_back( obj.release() );
    }
    from.clear();
};

template<typename T>
location_vector<T> &location_vector<T>::operator=( location_vector<T> &&source )
noexcept
{
    for( item * const &it : source.contents ) {
        it->remove_location();
        it->set_location( &*loc );
    }

    contents = std::move( source.contents );
    return *this;
};

template<typename T>
location_vector<T>::~location_vector()
{
    on_destroy();
}

template<typename T>
location_vector<T>::iterator::iterator() = default;

template<typename T>
location_vector<T>::iterator::iterator( typename std::vector<T *>::iterator it,
                                        const location_vector<T> &home ) : it( it ), home( &home )
{};

template<typename T>
location_vector<T>::iterator::iterator( const location_vector<T>::iterator &source )
{
    this->it = source.it;
    this->home = source.home;
};

template<typename T>
location_vector<T>::iterator::iterator( location_vector<T>::iterator &&source )
noexcept
{
    this->it = source.it;
    this->home = source.home;
    source.home = nullptr;
    source.it = {};
};

template<typename T>
typename location_vector<T>::iterator &location_vector<T>::iterator::operator=( const
        location_vector<T>::iterator &source )
{
    this->it = source.it;
    this->home = source.home;
    return *this;
};

template<typename T>
typename location_vector<T>::iterator &location_vector<T>::iterator::operator=
( location_vector<T>::iterator &&source )
noexcept
{
    this->it = source.it;
    this->home = source.home;
    source.home = nullptr;
    source.it = {};
    return *this;
};


template<typename T>
location_vector<T>::const_iterator::const_iterator( const iterator &source )
{
    this->it = source.it;
    this->home = source.home;
}

template<typename T>
location_vector<T>::const_iterator::const_iterator( iterator &&source )
{
    this->it = source.it;
    this->home = source.home;
    source.home = nullptr;
    source.it = {};
}

template<typename T>
location_vector<T>::const_iterator::const_iterator() = default;

template<typename T>
location_vector<T>::const_iterator::const_iterator( typename std::vector<T *>::const_iterator it,
        const location_vector<T> &home ) : it( it ), home( &home )
{};

template<typename T>
location_vector<T>::const_iterator::const_iterator( const location_vector<T>::const_iterator
        &source )
{
    this->it = source.it;
    this->home = source.home;
};

template<typename T>
location_vector<T>::const_iterator::const_iterator( location_vector<T>::const_iterator &&source )
noexcept
{
    this->it = source.it;
    this->home = source.home;
    source.home = nullptr;
    source.it = {};
};

template<typename T>
typename location_vector<T>::const_iterator &location_vector<T>::const_iterator::operator=
( const location_vector<T>::const_iterator &source )
{
    this->it = source.it;
    this->home = source.home;
    return *this;
};

template<typename T>
typename location_vector<T>::const_iterator &location_vector<T>::const_iterator::operator=
( location_vector<T>::const_iterator &&source )
noexcept
{
    this->it = source.it;
    this->home = source.home;
    source.home = nullptr;
    source.it = {};
    return *this;
};




template<typename T>
void location_vector<T>::push_back( detached_ptr<T> &&obj )
{
    if( !obj ) {
        return;
    }

    T *raw = obj.release();

    //Skip adding it if it's already here
    if( &*loc != raw->saved_loc ) {
        raw->resolve_saved_loc();
        contents.push_back( raw );
        if( destroyed ) {
            raw->destroy_in_place();
        }
    } else {
        raw->saved_loc = nullptr;
    }
    raw->set_location( &*loc );
}

template<typename T>
size_t location_vector<T>::size() const
{
    return contents.size();
}

template<typename T>
bool location_vector<T>::empty() const
{
    return contents.empty();
}

template<typename T>
T *location_vector<T>::back() const
{
    if( contents.empty() ) {
        debugmsg( "Attempted to call back on an empty location vector" );
        return &null_item_reference();
    }
    return contents.back();
}

template<typename T>
detached_ptr<T> location_vector<T>::remove( T *obj )
{
    if( destroyed ) {
        debugmsg( "Attempted to remove something from a destroyed location." );
        return detached_ptr<T>();
    }

    for( auto iter = contents.begin(); iter != contents.end(); iter++ ) {
        if( *iter == obj ) {
            detached_ptr<T> ret;
            erase( location_vector<T>::iterator( iter, *this ), &ret );
            return ret;
        }
    }
    debugmsg( "Attempted to remove something from a location that wasn't there." );
    return detached_ptr<T>();
}

template<typename T>
const std::vector<T *> &location_vector<T>::as_vector() const
{
    return contents;
}

template<typename T>
typename location_vector<T>::iterator location_vector<T>::erase( typename
        location_vector<T>::const_iterator it,
        detached_ptr<T> *out )
{
    if( destroyed && out ) {
        debugmsg( "Attempted to erase something from a destroyed location." );
        return location_vector<T>::iterator( contents.end(), *this );
    }
    T *subject = *it;
    typename std::vector<T *>::iterator ret = contents.erase( it.it );
    subject->remove_location();

    detached_ptr<T> local;
    detached_ptr<T> *used = out ? out : &local;

    *used = detached_ptr<T>( subject );
    return location_vector<T>::iterator( ret, *this );
}

template<typename T>
typename location_vector<T>::iterator location_vector<T>::insert( typename
        location_vector<T>::iterator it,
        detached_ptr<T> &&obj )
{
    if( !obj ) {
        return it;
    }
    T *raw = obj.release();
    //Insert it if it's not already here, find it otherwise
    if( &*loc != raw->saved_loc ) {
        raw->resolve_saved_loc();
        raw->set_location( &*loc );
        if( destroyed ) {
            raw->destroy_in_place();
        }
        return location_vector<T>::iterator( contents.insert( it.it, raw ), *this );
    } else {
        raw->saved_loc = nullptr;
        raw->set_location( &*loc );
        return location_vector<T>::iterator( std::find( contents.begin(), contents.end(), raw ), *this );
    }
}

template<typename T>
typename location_vector<T>::iterator location_vector<T>::insert( typename
        location_vector<T>::iterator it,
        typename std::vector<detached_ptr<T>>::iterator start,
        typename std::vector<detached_ptr<T>>::iterator end )
{
    for( auto iter = start; iter != end; iter++ ) {
        if( !*iter ) {
            continue;
        }
        T *raw = iter->release();
        //Only insert it if it's not already here
        if( &*loc != raw->saved_loc ) {
            raw->saved_loc = nullptr;
            raw->set_location( &*loc );
            if( destroyed ) {
                raw->destroy_in_place();
            }
            it = location_vector<T>::iterator( contents.insert( it.it, raw ), *this );
        } else {
            raw->resolve_saved_loc();
            raw->set_location( &*loc );
        }
    }
    return it;
}

template<typename T>
typename location_vector<T>::const_iterator location_vector<T>::begin() const
{
    return location_vector<T>::const_iterator( contents.cbegin(), *this );
}

template<typename T>
typename location_vector<T>::const_iterator location_vector<T>::end() const
{
    return location_vector<T>::const_iterator( contents.cend(), *this );
}

template<typename T>
typename location_vector<T>::iterator location_vector<T>::begin()
{
    return location_vector<T>::iterator( contents.begin(), *this );
}

template<typename T>
typename location_vector<T>::iterator location_vector<T>::end()
{
    return location_vector<T>::iterator( contents.end(), *this );
}

template<typename T>
typename location_vector<T>::const_reverse_iterator location_vector<T>::rbegin() const
{
    return location_vector<T>::const_reverse_iterator( location_vector<T>::const_iterator(
                contents.end(), *this ) );
}

template<typename T>
typename location_vector<T>::const_reverse_iterator location_vector<T>::rend() const
{
    return location_vector<T>::const_reverse_iterator( location_vector<T>::const_iterator(
                contents.begin(), *this ) );
}

template<typename T>
typename location_vector<T>::reverse_iterator location_vector<T>::rbegin()
{
    return location_vector<T>::reverse_iterator( location_vector<T>::iterator( contents.end(),
            *this ) );
}

template<typename T>
typename location_vector<T>::reverse_iterator location_vector<T>::rend()
{
    return location_vector<T>::reverse_iterator( location_vector<T>::iterator( contents.begin(),
            *this ) );
}

template<typename T>
typename location_vector<T>::const_iterator location_vector<T>::cbegin() const
{
    return location_vector<T>::const_iterator( contents.cbegin(), *this );
}

template<typename T>
typename location_vector<T>::const_iterator location_vector<T>::cend() const
{
    return location_vector<T>::const_iterator( contents.cend(), *this );
}

template<typename T>
typename location_vector<T>::const_reverse_iterator location_vector<T>::crbegin() const
{
    return location_vector<T>::const_reverse_iterator( location_vector<T>::const_iterator(
                contents.cend(), *this ) );
}

template<typename T>
typename location_vector<T>::const_reverse_iterator location_vector<T>::crend() const
{
    return location_vector<T>::const_reverse_iterator( location_vector<T>::const_iterator(
                contents.cbegin(), *this ) );
}

template<typename T>
T *location_vector<T>::front() const
{
    if( contents.empty() ) {
        debugmsg( "Attempted to call front on an empty location vector" );
        return &null_item_reference();
    }
    return contents.front();
}

template<typename T>
location<T> *location_vector<T>::get_location() const
{
    return &*loc;
}

template<typename T>
typename std::vector<detached_ptr<T>> location_vector<T>::clear()
{
    if( destroyed ) {
        debugmsg( "Attempted to clear a destroyed location." );
        return std::vector<detached_ptr<T>>();
    }
    std::vector<detached_ptr<T>> ret;
    for( item * const &i : contents ) {
        i->remove_location();
        ret.push_back( detached_ptr( i ) );
    }
    contents.clear();
    return ret;
}

template<typename T>
void location_vector<T>::remove_with( std::function < detached_ptr<T>( detached_ptr<T> && ) > cb )
{
    if( destroyed ) {
        debugmsg( "Attempted to remove_with from a destroyed location." );
        return;
    }
    size_t i = 0;
    for( auto it = contents.begin(); it != contents.end(); ) {
        item &as_item = **it;
        location<T> *saved_loc = ( *it )->loc;
        ( *it )->remove_location();
        ( *it )->saved_loc = saved_loc;
        detached_ptr<T> original( *it );
        detached_ptr<T> n = cb( std::move( original ) );
        if( n ) {
            if( &*n == *it ) {
                ( *it )->loc = saved_loc;
                n.release();
            } else {
                debugmsg( "Returning a different item in remove_with is not currently supported" );
            }
            ( *it )->saved_loc = nullptr;
            it++;
            i++;
        } else {
            if( as_item.saved_loc == nullptr ) {
                if( i >= contents.size() ) {
                    break;
                }
                it = contents.begin() + i;
            } else {
                as_item.saved_loc = nullptr;
                it = contents.erase( it );
            }
        }
    }
}

template<typename T>
void location_vector<T>::move_by( tripoint offset )
{
    auto tile_loc = dynamic_cast<tile_item_location *>( &*loc );
    if( !tile_loc ) {
        debugmsg( "Tried to move_by a non-tile location" );
        return;
    }
    tile_loc->move_by( offset );
}

template<typename T>
void location_vector<T>::set_loc_hack( location<T> *new_loc )
{
    loc = std::unique_ptr<location<T>>( new_loc );
    for( item *&it : contents ) {
        it->remove_location();
        it->set_location( &*loc );
    }
}

template<typename T>
void location_vector<T>::on_destroy()
{
    if( destroyed ) {
        return;
    }
    for( item *&it : contents ) {
        it->destroy_in_place();
    }
    destroyed = true;
}

template<>
void std::swap<item>( location_vector<item> &lhs, location_vector<item> &rhs )
noexcept
{
    for( item *&it : lhs.contents ) {
        it->remove_location();
        it->set_location( &*rhs.loc );
    }
    for( item *&it : rhs.contents ) {
        it->remove_location();
        it->set_location( &*lhs.loc );
    }
    std::swap( lhs.contents, rhs.contents );
}

template
class location_vector<item>;
