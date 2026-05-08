#include "safe_reference.h"
#include "item.h"
#include "json.h"
#include "character.h"
#include "map.h"
#include "map_selector.h"
#include "avatar.h"
#include "game.h"
#include "vpart_position.h"
#include "vehicle.h"
#include "vehicle_selector.h"

uint64_t save_id_prefix = 0;
bool save_and_quit = false;

template<typename T>
void safe_reference<T>::serialize_global( JsonOut &json )
{
    json.start_array();
    for( auto &it : records_by_id ) {
        //TODO!: better format
        safe_reference<T>::id_type id = it.second->id;
        uint32_t count = it.second->json_count;
        if( count != 0 ) {
            json.write( id );
            json.write( count );
        }
    }
    json.end_array();
}

template<typename T>
void safe_reference<T>::deserialize_global( const JsonArray &jsin )
{
    bool pair = false;
    safe_reference<T>::id_type id;
    for( const JsonValue val : jsin ) {
        if( !pair ) {
            id = val.get_uint64();
            pair = true;
            continue;
        }

        pair = false;
        uint32_t count = val.get_uint();
        auto *rec = new record( id );
        rec->json_count = count;
        records_by_id.insert( {id, rec} );
    }
    if( pair ) {
        debugmsg( "Corrupt safe_references in save" );
    }
}

void reset_save_ids( uint32_t prefix, bool quitting )
{
    save_id_prefix = prefix;
    save_id_prefix <<= 32;
    save_and_quit = quitting;
    safe_reference<item>::next_id = 1;
}

template<>
void deserialize<item>( safe_reference<item> &out, JsonIn &js )
{
    if( !js.test_number() ) {
        //legacy item_location support.


        auto obj = js.get_object();
        auto type = obj.get_string( "type" );
        int idx = -1;
        tripoint pos = tripoint_min;

        obj.read( "idx", idx );
        obj.read( "pos", pos );

        item *it = nullptr;

        auto find_index = [&idx, &it]( const item * e ) {
            if( idx-- == 0 ) {
                it = const_cast<item *>( e );
                return VisitResponse::ABORT;
            }
            return VisitResponse::NEXT;
        };

        if( type == "character" ) {
            character_id who_id;
            if( obj.has_member( "character" ) ) {
                obj.read( "character", who_id );
            } else {
                // This is for migrating saves before npc item locations were supported and all
                // character item locations were assumed to be on g->u
                who_id = get_avatar().getID();
            }

            auto *who = g->critter_by_id<Character>( who_id );
            if( !who ) {
                debugmsg( "Could not find character for item location.  May not have been loaded yet." );
                return;
            }
            who->visit_items( find_index );
            out = safe_reference<item>( it );
        } else if( type == "map" ) {
            map_cursor map_cur( pos );
            map_cur.visit_items( find_index );
            out = safe_reference<item>( it );
        } else if( type == "vehicle" ) {
            vehicle *const veh = veh_pointer_or_null( get_map().veh_at( pos ) );
            int part = obj.get_int( "part" );
            if( veh && part >= 0 && part < veh->part_count() ) {
                vehicle_cursor veh_cur( *veh, part );
                veh_cur.visit_items( find_index );
                out = safe_reference<item>( it );
            }
        } else if( type == "in_container" ) {
            safe_reference<item> parent;
            obj.read( "parent", parent );
            if( !parent ) {
                return;
            }
            const std::vector<item *> parent_contents = parent->contents.all_items_top();
            auto iter = parent_contents.begin();
            std::advance( iter, idx );
            out = safe_reference<item>( *iter );
        }
        return;
    }
    uint64_t id = 0;
    if( !js.read( id, false ) ) {
        return;
    }
    out = safe_reference<item>( id );
}

template<typename T>
void deserialize( safe_reference<T> &out, JsonIn &js )
{
    uint64_t id = 0;
    if( !js.read( id, false ) ) {
        return;
    }
    out = safe_reference<T>( id );
}

template<typename T>
void serialize( const safe_reference<T> &val, JsonOut &js )
{
    js.write( val.serialize() );
}

template
void serialize<item>( const safe_reference<item> &val, JsonOut &js );


template<typename T>
void safe_reference<T>::cleanup()
{
    std::set<record *> records;
    for( auto &rec : records_by_pointer ) {
        records.insert( rec.second );
    }
    for( auto &rec : records_by_id ) {
        records.insert( rec.second );
    }
    for( record *rec : records ) {
        if( rec->mem_count > 0 ) {
            debugmsg( "Found a safe_reference entry with a mem_count.  It's advised to fully restart the game now in case of crashes." );
        }
        delete rec;
    }
    records_by_id.clear();
    records_by_pointer.clear();
}

template<typename T>
void safe_reference<T>::register_load( T *obj, id_type id )
{
    if( id == ID_NONE ) {
        return;
    }
    auto search = records_by_id.find( id );
    if( search != records_by_id.end() ) {
        search->second->target.p = obj;
    } else {
        auto *rec = new record( obj, id );
        records_by_id.insert( {id, rec} );
        records_by_pointer.insert( {obj, rec} );
    }
}

template<typename T>
auto safe_reference<T>::lookup_id( const T *obj ) -> safe_reference<T>::id_type
{
    auto search = records_by_pointer.find( obj );
    if( search != records_by_pointer.end() ) {
        if( search->second->id == ID_NONE ) {
            search->second->id = generate_new_id();
        }
        return search->second->id;
    }
    return ID_NONE;
}

template<typename T>
void safe_reference<T>::mark_destroyed( T *obj )
{
    auto search = records_by_pointer.find( obj );
    if( search == records_by_pointer.end() ) {
        return;
    }
    search->second->id |= DESTROYED_MASK;
}

template<typename T>
void safe_reference<T>::mark_deallocated( T *obj )
{
    auto search = records_by_pointer.find( obj );
    if( search == records_by_pointer.end() ) {
        return;
    }
    records_by_pointer.erase( search );
}

template<typename T>
safe_reference<T>::safe_reference(): rec( nullptr ) {}

template<typename T>
safe_reference<T>::safe_reference( T *obj )
{
    fill( obj );
    rec->mem_count++;
}

template<typename T>
safe_reference<T>::safe_reference( T &obj )
{
    fill( &obj );
    rec->mem_count++;
}

template<typename T>
safe_reference<T>::safe_reference( id_type id )
{
    if( id == ID_NONE || id_is_destroyed( id ) ) {
        //TODO!: add cannon destroyed record
        rec = nullptr;
    } else {
        fill( id );
        rec->mem_count++;
    }
}

template<typename T>
safe_reference<T>::safe_reference( const safe_reference<T> &source )
{
    rec = source.rec;
    if( rec ) {
        rec->mem_count++;
    }
}

template<typename T>
safe_reference<T>::safe_reference( safe_reference<T> &&source ) noexcept
{
    rec = source.rec;
    source.rec = nullptr;
}

template<typename T>
auto safe_reference<T>::operator=( const safe_reference<T> &source ) -> safe_reference<T> &
{
    if( &source == this ) {
        return *this;
    }
    remove();
    rec = source.rec;
    if( rec ) {
        rec->mem_count++;
    }
    return *this;
}

template<typename T>
auto safe_reference<T>::operator=( safe_reference<T> &&source ) noexcept -> safe_reference<T> &
{
    if( &source == this ) {
        return *this;
    }
    remove();
    rec = source.rec;
    source.rec = nullptr;
    return *this;
}

template<typename T>
safe_reference<T>::~safe_reference()
{
    remove();
}

void cleanup_references()
{
    safe_reference<item>::cleanup();
}

template
class safe_reference<item>;
