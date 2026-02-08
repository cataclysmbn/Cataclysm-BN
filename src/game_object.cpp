#include "game_object.h"

#include <memory>
#include <type_traits>

#include "cata_arena.h"
#include "item.h"
#include "locations.h"

template<typename T>
void game_object<T>::destroy()
{
    if constexpr ( std::is_same_v<T, item> ) {
        if( static_cast<T *>( this ) == &null_item_reference() ) {
            return;
        }
    }
    if( loc != nullptr ) {
        debugmsg( "Attempted to destroy an item with a location." );
        remove_location();
    }
    destroy_in_place();
}

template<typename T>
void game_object<T>::destroy_in_place()
{
    T *self = static_cast<T *>( this );
    self->on_destroy();
    cata_arena<T>::mark_for_destruction( self );
}

template<typename T>
void game_object<T>::remove_location()
{
    loc = nullptr;
}

template<typename T>
void game_object<T>::set_location( location<T> *own )
{
    if( loc != nullptr ) {
        debugmsg( "Attempted to set the location of [%s] that already has one.", debug_name() );
        detach().release();
    }
    loc = own;
}

template<typename T>
detached_ptr<T> game_object<T>::detach()
{
    if( loc == nullptr ) {
        debugmsg( "Tried to detach [%s] not in a location.", debug_name() );
        return detached_ptr<T>();
    }
    if( saved_loc != nullptr ) {
        debugmsg( "Tried to detach [%s] that's already in an attempted detach.", debug_name() );
        return detached_ptr<T>();
    }
    detached_ptr<T> res = loc->detach( static_cast<T *>( this ) );
    remove_location();
    return  res;
}

template<typename T>
bool game_object<T>::attempt_detach( std::function < detached_ptr<T>
                                     ( detached_ptr<T> && ) > cb )
{
    /* Before changing this function be sure to consider the following edge case:
     * get_avatar().get_weapon().attempt_detach([](detached_ptr<item> &&e){
     *     get_avatar().set_weapon(std::move(e));
     *     get_avatar().get_weapon().attempt_detach([](detached_ptr<item> &&e){
     *         get_avatar().set_weapon(std::move(e));
     *         return detached_ptr<item>();
     *     }
     *     return detached_ptr<item>();
     * }
     *
     * Of course this is contrived but attempting to detach to somewhere the object already is or recursively detaching must work.
     * Most of the complexity in this function comes from handling these sorts of cases.
     */

    if( !loc ) {
        debugmsg( "Called attempt_detach on an object without a location" );
        return false;
    }

    assert( !saved_loc );

    //We keep our own copy of the location and also place one on the object.
    //If the object is added to location structures during the cb this property will be used and perhaps reset.
    location<T> *old_loc = loc;
    saved_loc = old_loc;
    remove_location();

    //Then we create a detached pointer of ourselves. Note that we haven't removed ourselves yet.
    T *self = static_cast<T *>( this );
    detached_ptr<T> orig( self );

    //Then run the callback.
    detached_ptr<T> n = cb( std::move( orig ) );


    //There are a bunch of awkwards cases here
    if( !n ) {
        //First the simplest one, we got a null detached_ptr back, we need to remove the object.
        //It might be that the saved_loc has been reset by a location structure (i.e. the object was placed somewhere else). It will have been removed at that point and we don't need to do anything.
        //Otherwise we need to remove it from its location without destroying it. It may have another valid detached_ptr somewhere at this point or orig may still be valid.
        resolve_saved_loc();
        return true;
    } else {
        //We got back a valid detached_ptr
        if( &*n == self ) {
            if( saved_loc ) {
                //It's the same as the original and hasn't been moved, i.e. we just reset our location as we didn't actually move
                self->set_location( old_loc );
                saved_loc = nullptr;
                n.release();
                return false;
            } else {
                //It's the same as the original, but it's been moved at some point, we need to reattach it.
                n->set_location( old_loc );
                old_loc->attach( std::move( n ) );
                return false;
            }
        } else {
            //We've been given something different.
            //If it hasn't been placed elsewhere then remove the old one now.
            resolve_saved_loc();
            n->set_location( old_loc );
            old_loc->attach( std::move( n ) );
            return false;
        }
    }
}

template<typename T>
void game_object<T>::resolve_saved_loc()
{
    if( saved_loc ) {
        saved_loc->detach( static_cast<T *>( this ) ).release();
        saved_loc = nullptr;
    }
}

template<typename T>
bool game_object<T>::is_detached() const
{
    return !!loc;
}

template<typename T>
bool game_object<T>::is_loaded() const
{
    if( !loc ) {
        return false;
    }
    return loc->is_loaded( static_cast<const T *>( this ) );
}

template<typename T>
bool game_object<T>::has_position() const
{
    return ( !!loc ) || ( !!saved_loc );
}

template<typename T>
tripoint game_object<T>::position( ) const
{
    if( !loc ) {
        if( !saved_loc ) {
            debugmsg( "position called on [%s] without a position", debug_name() );
            return tripoint_zero;
        }
        return saved_loc->position( static_cast<const T *>( this ) );
    }
    return loc->position( static_cast<const T *>( this ) );
};


template
class game_object<item>;
