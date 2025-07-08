#pragma once

/**
 *
 * Safe references are easy to use but very complex internally. Please refer to proper documentation
 * on how to use them. This comments covers their internal workings rather than their public
 * interface.
 *
 * Note that safe references require that the objects referenced have an is_loaded method, but more
 * generally that it's a game object in order for the persistance to work correct.
 *
 * Central to safe references is their records. Each record contains 2 counts, an id and a pointer.
 * The first count stores the number of live in-memory references. It can be assumed to be accurate.
 * The second count stores the number of in-json references. It can't be trusted due to save
 * scumming. The ID will often be ID_NONE and contains two flag bits, which will be discussed later.
 * When comparing IDs the flag bits are ignored. The pointer usually stores a pointer to the target
 * itself. If the target is in memory it reliably points to them and is nullptr otherwise. Note that
 * the pointer will not be made null until the end of the turn if the target is unloaded or
 * destroyed. It's important to check these things separately. In the case that the redirect ID bit
 * is set the pointer instead points to another record.
 *
 * Two in-memory global (really per GO type) unordered_maps are used to manage this. One that
 * contains object pointers -> record pointers and one that contains ids -> record pointers. There
 * are also two global json structures created when saving. These store the json counts of IDs and a
 * table of ID redirects. Both of these are cleaned when the json count for an ID hits 0. Objects
 * are not given a record until a safe reference to them is first created. These records can exist
 * in both, either or neither of the global lists during their life. IDs are not added to a record
 * until either the object itself or one of its references is saved. IDs only exist in records, not
 * in the objects themselves. Records are typically cleaned up when the counts indicate we can do
 * so, however we never forget an ID once one has been assigned and will keep that record loaded for
 * as long as the object is.
 *
 * The two flags stored in IDs represent destruction and redirection. They can't be trusted due to
 * save scumming. Either one or neither, but not both can be set and they aren't considered part of
 * the ID proper. They are still stored in the record and persistened to json when appropriate. Note
 * that in json references and object ids never have these bits set. Only in memory and the global
 * structures. References to a destroyed thing should instead use ID_NONE with the destroyed bit
 * set, references to a redirected thing should resolve the redirection before saving. Destroyed
 * objects shouldn't be saved and likewise resolve redirections before saving.
 *
 * Merging and redirects is the most complicated part but fortunately they're quite rare, it's only
 * very few merges result in a redirect flag being set. The big thing to remember is that a
 * redirected entry is kept around until its counts are both 0. However, redirected references are
 * replaced with the thing they redirect to at every opportunity. This updates the counts
 * appropriately and so the counts of a redirected reference should only ever go down.
 */

#include <memory>
#include <algorithm>
#include <unordered_map>

#include "debug.h"

class item;
class game;
class JsonIn;
class JsonArray;
class JsonOut;

template<typename T> class cata_arena;
template<typename T> class cache_reference;

void reset_save_ids( uint32_t prefix, bool quitting );

extern uint64_t save_id_prefix;
extern bool save_and_quit;

template<typename T>
class safe_reference
{
    protected:
        struct record;
    public:
        using id_type = uint64_t;

        friend void reset_save_ids( uint32_t, bool );
        friend T;
        friend game;
        friend cata_arena<T>;

    protected:
        using rbp_type = std::unordered_map<const T *, record *>;
        using rbi_type = std::unordered_map<id_type, record *>;
        using rbp_it = typename rbp_type::iterator;
        using rbi_it = typename rbi_type::iterator;

        constexpr static id_type ID_NONE = 0;
        constexpr static id_type DESTROYED_MASK  = 0x80000000;
        constexpr static id_type REDIRECTED_MASK = 0x40000000;

        struct record {
            record( T *p ) : id( ID_NONE ), mem_count( 0 ), json_count( 0 ) {
                target.p = p;
            };
            record( id_type id ) : id( id ), mem_count( 0 ), json_count( 0 ) {
                target.p = nullptr;
            };
            record( T *p, id_type id ) : id( id ), mem_count( 0 ), json_count( 0 ) {
                target.p = p;
            };
            union {
                record *redirect;
                T *p;
            } target;
            id_type id;
            uint32_t mem_count;
            uint32_t json_count;
        };
        mutable record *rec;

        inline static rbp_type records_by_pointer;
        inline static rbi_type records_by_id;
        inline static uint32_t next_id = 1;

        void fill( T *obj ) {
            rbp_it search = records_by_pointer.find( obj );
            if( search != records_by_pointer.end() ) {
                rec = search->second;
            } else {
                rec = new record( obj );
                records_by_pointer.insert( {obj, rec} );
            }
        }
        void fill( id_type id ) {
            rbi_it search = records_by_id.find( id );
            if( search != records_by_id.end() ) {
                rec = search->second;
            } else {
                //This is indicative of save scumming
                rec = new record( id );
                records_by_id.insert( {id, rec} );
            }
        }

        static bool id_is_destroyed( id_type id ) {
            return ( id & DESTROYED_MASK ) != 0;
        }

        static bool id_is_redirected( id_type id ) {
            return ( id & REDIRECTED_MASK ) != 0;
        }

        static id_type base_id( id_type id ) {
            return ( id & ~( DESTROYED_MASK | REDIRECTED_MASK ) );
        }

        void resolve_redirects() const {
            while( rec != nullptr && id_is_redirected( rec->id ) ) {
                if( rec->mem_count == 1 && rec->json_count == 0 ) {
                    record *old_rec = rec;
                    rec = rec->target.redirect;
                    delete old_rec;
                } else {
                    rec->mem_count--;
                    rec = rec->target.redirect;
                    rec->mem_count++;
                }
            }
        }

        void remove() {
            resolve_redirects();
            if( rec == nullptr ) {
                return;
            }
            //Check if we're the last in-memory reference
            if( rec->mem_count == 1 ) {
                if( base_id( rec->id ) == ID_NONE ) {
                    //If the record doesn't have an ID it's ok to just forget it
                    records_by_pointer.erase( rec->target.p );
                    delete rec;
                } else if( rec->json_count == 0 && id_is_destroyed( rec->id ) ) {
                    //If there are no more references and the object is destroyed, forget it
                    records_by_id.erase( rec->id );
                    if( rec->target.p != nullptr ) {
                        records_by_pointer.erase( rec->target.p );
                    }
                    delete rec;
                } else {
                    //We need to keep this record around, just set its mem count to 0
                    rec->mem_count--;
                }
            } else {
                //If we're not just decrease the count
                rec->mem_count--;
            }
        }

        static void register_load( T *obj, id_type id );

        static id_type lookup_id( const T *obj );

        static void mark_destroyed( T *obj );

        static void mark_deallocated( T *obj );
        static void serialize_global( JsonOut &json );
        static void deserialize_global( const JsonArray &jsin );

        static id_type generate_new_id() {
            return save_id_prefix | next_id++;
        }

    public:

        safe_reference();

        safe_reference( T *obj );
        safe_reference( T &obj );
        explicit safe_reference( id_type id );
        safe_reference( const safe_reference<T> &source );
        safe_reference( safe_reference<T> &&source ) noexcept ;

        safe_reference<T> &operator=( const safe_reference<T> &source );

        safe_reference<T> &operator=( safe_reference<T> &&source ) noexcept ;

        ~safe_reference();
        static void cleanup();

        bool is_unassigned() const {
            return rec == nullptr;
        }

        bool is_accessible() const {
            return rec != nullptr && rec->target.p != nullptr;
        }

        bool is_unloaded() const {
            resolve_redirects();
            if( is_unassigned() ) {
                return false;
            }
            if( is_destroyed() ) {
                return false;
            }
            return ( rec->target.p == nullptr || ( !rec->target.p->is_loaded() &&
                                                   !rec->target.p->is_detached() ) );
        }

        bool is_destroyed() const {
            resolve_redirects();
            if( is_unassigned() ) {
                return false;
            }
            return ( rec->id & DESTROYED_MASK ) != 0;
        }

        id_type serialize() const {
            if( rec == nullptr ) {
                return ID_NONE;
            }
            if( rec->id == ID_NONE && rec->target.p != nullptr ) {
                rec->id = generate_new_id();
            }
            // If this object is to remain loaded, we don't increase the json count
            // as we should be saving it again before we're done.
            if( save_and_quit || is_unloaded() ) {
                rec->json_count++;
            }
            return rec->id;
        }

        void deserialize( id_type id ) {
            fill( id );
            if( rec == nullptr ) {
                return;
            }
            if( rec->json_count != 0 ) {
                rec->json_count--;
            } // else { this is indicative of save scumming }
            rec->mem_count++;
        }

        const T *get_const() const {
            if( !rec || !rec->target.p ) {
                //TODO! more safety and proper error
                return nullptr;
            }
            return rec->target.p;
        }

        T *get() const {
            resolve_redirects();
            if( !*this ) {
                debugmsg( "Attempted to resolve invalid safe reference" );
                return nullptr;
            }
            return rec->target.p;
        }

        explicit operator bool() const {
            return !!*this;
        }

        bool operator!() const {
            return is_unassigned() || is_unloaded() || is_destroyed();
        }

        T &operator*() const {
            return *get();
        }

        T *operator->() const {
            return get();
        }

        bool operator==( const safe_reference<T> &against ) const {
            resolve_redirects();
            against.resolve_redirects();
            return rec == against.rec;
        }

        bool operator==( const T &against ) const {
            if( !rec ) {
                return false;
            }
            resolve_redirects();
            return rec->target.p == &against;
        }

        bool operator==( const T *against ) const {
            if( !rec ) {
                return against == nullptr;
            }
            resolve_redirects();
            return rec->target.p == against;
        }

        template <typename U>
        bool operator!=( const U against ) const {
            return !( *this == against );
        }

        /**
         * Merge the secondary object into the primary. This means all
         * references to the secondary will now point to the primary.
         * Typically you'll want to destroy the secondary shortly afterwards.
         */
        static void merge( T *primary, T *secondary ) {

            rbp_it sec_search = records_by_pointer.find( secondary );

            // The secondary doesn't have a record (i.e. there are no references
            // to it to redirect) so there's nothing to do
            if( sec_search == records_by_pointer.end() ) {
                return;
            }

            rbp_it pri_search = records_by_pointer.find( primary );

            //The primary doesn't have a record but the secondary does
            if( pri_search == records_by_pointer.end() ) {
                //change the secondary's record to point to the primary now
                record *rec = sec_search->second;
                rec->target.p = primary;
                records_by_pointer.erase( secondary );
                records_by_pointer.insert( {primary, rec} );
                return;
            }

            // They both have a record
            // Neither of these records should be a redirect as this would imply
            // that a secondary wasn't destroyed after being merged
            record *pri_rec = pri_search->second;
            record *sec_rec = sec_search->second;

            //If the secondary doesn't have an ID
            if( sec_rec->id == ID_NONE ) {
                sec_rec->id = REDIRECTED_MASK;
                sec_rec->target.redirect = pri_rec;
                pri_rec->mem_count++;
            } else {
                //This is the worse case, we actually need a redirect
                sec_rec->id = sec_rec->id | REDIRECTED_MASK;
                sec_rec->target.redirect = pri_rec;
                pri_rec->mem_count++;
            }
        }

};

template<typename T>
class cache_reference
{
    private:
        T *p;
    protected:
        using ref_list = std::vector<cache_reference<T>*>;
        using ref_map = std::unordered_map<const T *, ref_list>;

        using ref_map_it = typename ref_map::iterator;

        inline static ref_map reference_map;

        void invalidate() {
            p = nullptr;
        }

        void add_to_map() {
            if( !p ) {
                return;
            }
            ref_map_it search = reference_map.find( p );
            if( search != reference_map.end() ) {
                search->second.push_back( this );
            } else {
                reference_map.insert( {p, {this}} );
            }
        }

        void remove_from_map() {
            if( !p ) {
                return;
            }
            ref_map_it search = reference_map.find( p );
            if( search != reference_map.end() ) {
                ref_list &list = search->second;
                list.erase( std::remove( list.begin(), list.end(), this ), list.end() );
                if( list.empty() ) {
                    reference_map.erase( search );
                }
            } else {
                debugmsg( "Could not find cache reference in reference map" );
            }
        }

    public:

        static void mark_destroyed( T *obj ) {
            ref_map_it search = reference_map.find( obj );
            if( search == reference_map.end() ) {
                return;
            }
            for( cache_reference<T> *&ref : search->second ) {
                ref->invalidate();
            }
            reference_map.erase( search );
        }

        cache_reference(): p( nullptr ) {}

        cache_reference( T *obj ) {
            p = obj;
            add_to_map();
        }
        cache_reference( T &obj ) {
            p = &obj;
            add_to_map();
        }

        cache_reference( const cache_reference<T> &source ) {
            p = source.p;
            add_to_map();
        }

        cache_reference( cache_reference<T> &&source )  noexcept {
            p = source.p;
            source.p = nullptr;
            if( !p ) {
                return;
            }
            ref_map_it search = reference_map.find( p );
            if( search == reference_map.end() ) {
                debugmsg( "Couldn't find cached reference in reference map." );
                p = nullptr;
                return;
            }
            ref_list &list = search->second;
            std::replace( list.begin(), list.end(), &source, this );
        }

        cache_reference<T> &operator=( const cache_reference<T> &source ) {
            if( !p && !source.p ) {
                return *this;
            }
            if( p ) {
                if( source.p == p ) {
                    return *this;
                }
                remove_from_map();
            }
            p = source.p;
            add_to_map();
            return *this;
        }

        cache_reference<T> &operator=( cache_reference<T> &&source )  noexcept {
            if( !p && !source.p ) {
                return *this;
            }
            if( p ) {
                if( source.p == p ) {
                    source.remove_from_map();
                    source.p = nullptr;
                    return *this;
                }
                remove_from_map();
            }
            p = source.p;
            add_to_map();
            source.remove_from_map();
            source.p = nullptr;
            return *this;
        }

        ~cache_reference() {
            remove_from_map();
        }

        T *get() const {
            if( !*this ) {
                debugmsg( "Tried to access invalid safe_reference" );
                return nullptr;
            }
            return p;
        }

        explicit operator bool() const {
            return !!*this;
        }

        bool operator!() const {
            return p == nullptr;
        }

        T &operator*() const {
            return *get();
        }

        T *operator->() const {
            return get();
        }

        bool operator==( const cache_reference<T> &against ) const {
            return against.p == p;
        }

        bool operator==( const T &against ) const {
            return p == &against;
        }

        bool operator==( const T *against ) const {
            return p == against;
        }

        template <typename U>
        bool operator!=( const U against ) const {
            return !( *this == against );
        }
};

template<typename T>
void deserialize( safe_reference<T> &, JsonIn & );

template<typename T>
void serialize( const safe_reference<T> &, JsonOut & );

void cleanup_references();


