#pragma once

#include <algorithm>
#include <bitset>
#include <set>
#include <unordered_map>
#include <vector>

#include "assign.h"
#include "catacharset.h"
#include "debug.h"
#include "enum_bitset.h"
#include "generic_readers.h"
#include "init.h"
#include "int_id.h"
#include "json.h"
#include "mod_tracker.h"
#include "output.h"
#include "string_id.h"
#include "translations.h"
#include "units.h"
#include "wcwidth.h"

/**
A generic class to store objects identified by a `string_id`.

The class handles loading (including overriding / replacing existing objects) and
querying for specific objects. The class is designed to work with @ref string_id and
can be by it to implement its interface.

----

@tparam T The type of the managed objects. The type must have:
  - a default constructor,
  - a `load( JsonObject & )` function,
  - an `id` member of type `string_id<T>`,
  - a `was_loaded` member of type `bool`, which must have the value `false` before
    the first call to `load`.

  The type can also have:
  - a 'check()' function (to run `generic_factory::check()` on all objects)

  Those things must be visible from the factory, you may have to add this class as
  friend if necessary.

  `T::load` should load all the members of `T`, except `id` and `was_loaded` (they are
  set by the `generic_factory` before calling `load`). Failures should be reported by
  throwing an exception (e.g. via `JsonObject::throw_error`).

----

Usage:

- Create an instance, it can be static, or packed into another object.
- Implement `string_id::load` as simply forwarding to `generic_factory::load`.
  Register `string_id::load` in the @ref DynamicDataLoader (init.cpp) to be called when
  an object of the matching type is encountered.
- Similar: implement and register `string_id::reset` and let it call `generic_factory::reset`.

The functions can contain more code:
- `load` typically does nothing special beside forwarding to `generic_factory`.
- `reset` removes the loaded objects. It usually needs to remove the additional data that was set
  up in `finalize`. It must call `generic_factory::reset`.

Optional: implement the other functions used by the DynamicDataLoader: `finalize`,
`check_consistency`. There is no implementation of them in the generic factory.

`check_consistency` typically goes over all loaded items and checks them somehow.

`finalize` typically populates some other data (e.g. some cache) or sets up connection between
loaded objects of different type.

A sample implementation looks like this:
\code
class my_class { ... }

namespace {
generic_factory<my_class> my_class_factory( "my class" );
std::map<...> some_cache;
}

template<>
void string_id<my_class>::load( const JsonObject &jo ) {
    my_class_factory.load( jo );
}

template<>
void string_id<my_class>::reset() {
    some_cache.clear();
    my_class_factory.reset();
}

template<>
void string_id<my_class>::finalize() {
    for( auto &ptr : my_class_factory.all() )
        // populate a cache just as an example
        some_cache.insert( ... );
    }
}

// Implementation of the string_id functions:
template<>
const my_class &string_id<my_class>::obj() const
{
    return my_class_factory.obj( *this );
}
// ... more functions of string_id, similar to the above.

\endcode
*/

template<typename T>
class string_id_reader;

template<typename T>
class generic_factory
{

    public:
        virtual ~generic_factory() = default;

    private:
        DynamicDataLoader::deferred_json deferred;
        // generation or "modification count" of this factory
        // it's incremented when any changes to the inner id containers occur
        // version value corresponds to the string_id::_version,
        // so incrementing the version here effectively invalidates all cached string_id::_cid
        int64_t  version = 0;

        void inc_version() {
            do {
                version++;
            } while( version == INVALID_VERSION );
        }

        bool _is_finalized = false;

    protected:
        void set_finalized( bool val ) {
            _is_finalized = val;
        }

        std::vector<T> list;
        std::unordered_map<string_id<T>, int_id<T>> map;
        std::unordered_map<std::string, T> abstracts;

        std::string type_name;
        std::string id_member_name;
        std::string alias_member_name;
        // TEMPORARY until 0.G: Remove "ident" support
        const std::string legacy_id_member_name = "ident";

        bool find_id( const string_id<T> &id, int_id<T> &result ) const {
            if( id._version == version ) {
                result = int_id<T>( id._cid );
                return is_valid( result );
            }

            const auto iter = map.find( id );
            // map lookup happens at most once per string_id instance per generic_factory::version
            // id was not found, explicitly marking it as "invalid"
            if( iter == map.end() ) {
                id.set_cid_version( INVALID_CID, version );
                return false;
            }
            result = iter->second;
            id.set_cid_version( result.to_i(), version );
            return true;
        }

        void remove_aliases( const string_id<T> &id ) {
            int_id<T> i_id;
            if( !find_id( id, i_id ) ) {
                return;
            }
            auto iter = map.begin();
            const auto end = map.end();
            for( ; iter != end; ) {
                if( iter->second == i_id && iter->first != id ) {
                    map.erase( iter++ );
                } else {
                    ++iter;
                }
            }
        }

        const T dummy_obj;

    public:
        /**
         * @param type_name A string used in debug messages as the name of `T`,
         * for example "vehicle type".
         * @param id_member_name The name of the JSON member that contains the id(s) of the
         * loaded object(s).
         * @param alias_member_name Alternate names of the JSON member that contains the id(s) of the
         * loaded object alias(es).
         */
        generic_factory( const std::string &type_name, const std::string &id_member_name = "id",
                         const std::string &alias_member_name = "alias" )
            : type_name( type_name ),
              id_member_name( id_member_name ),
              alias_member_name( alias_member_name ),
              dummy_obj() {
        }

        /**
         * Returns whether the factory has been finalized.
         */
        bool is_finalized() const {
            return _is_finalized;
        }

        /**
        * Perform JSON inheritance handling for `T def` and returns true if JsonObject is real.
        *
        * If the object contains a "copy-from" member the corresponding abstract gets copied if found.
        *    If abstract is not found, object is added to deferred.
        * If the object is abstract, it is loaded via `T::load` and added to `abstracts`
        *
        * @return true if `jo` is loaded and false if loading is deferred.
        * @throws JsonError If `jo` is both abstract and real. (contains "abstract" and "id" members)
        */
        bool handle_inheritance( T &def, const JsonObject &jo, const std::string &src ) {
            static const std::string copy_from_member_name( "copy-from" );
            static const std::string abstract_member_name( "abstract" );
            if( jo.has_string( copy_from_member_name ) ) {
                const std::string source = jo.get_string( copy_from_member_name );
                auto base = map.find( string_id<T>( source ) );

                if( base != map.end() ) {
                    def = obj( base->second );
                } else {
                    auto ab = abstracts.find( source );

                    if( ab != abstracts.end() ) {
                        def = ab->second;
                    } else {
                        def.was_loaded = false;
                        deferred.emplace_back( jo.get_source_location(), src );
                        jo.allow_omitted_members();
                        return false;
                    }
                }
                def.was_loaded = true;
            }

            if( jo.has_string( abstract_member_name ) ) {
                if( jo.has_string( id_member_name ) || jo.has_string( legacy_id_member_name ) ) {
                    jo.throw_error( string_format( "cannot specify both '%s' and '%s'/'%s'",
                                                   abstract_member_name, id_member_name, legacy_id_member_name ) );
                }
                def.load( jo, src );
                abstracts[jo.get_string( abstract_member_name )] = def;
            }
            return true;
        }

        /**
         * Load an object of type T with the data from the given JSON object.
         *
         * The id of the object is taken from the JSON object. The object data is loaded by
         * calling `T::load(jo)` (either on a new object or on an existing object).
         * See class documentation for intended behavior of that function.
         *
         * @throws JsonError If loading fails for any reason (thrown by `T::load`).
         */
        void load( const JsonObject &jo, const std::string &src ) {
            const bool strict = is_strict_enabled( src );

            static const std::string abstract_member_name( "abstract" );

            T def;

            if( !handle_inheritance( def, jo, src ) ) {
                return;
            }
            if( jo.has_string( id_member_name ) ) {
                def.id = string_id<T>( jo.get_string( id_member_name ) );
                assign_src( def, src );
                def.load( jo, src );
                insert( def );

                if( jo.has_member( alias_member_name ) ) {
                    std::set<string_id<T>> aliases;
                    assign( jo, alias_member_name, aliases, strict );

                    const int_id<T> ref = map[def.id];
                    for( const auto &e : aliases ) {
                        map[e] = ref;
                    }
                }

            } else if( jo.has_array( id_member_name ) ) {
                for( JsonValue e : jo.get_array( id_member_name ) ) {
                    T def;
                    if( !handle_inheritance( def, jo, src ) ) {
                        break;
                    }
                    def.id = string_id<T>( e );
                    assign_src( def, src );
                    def.load( jo, src );
                    insert( def );
                }
                if( jo.has_member( alias_member_name ) ) {
                    jo.throw_error( string_format( "can not specify '%s' when '%s' is array",
                                                   alias_member_name, id_member_name ) );
                }

            } else if( jo.has_string( legacy_id_member_name ) ) {
                def.id = string_id<T>( jo.get_string( legacy_id_member_name ) );
                assign_src( def, src );
                def.load( jo, src );
                insert( def );

                if( jo.has_member( alias_member_name ) ) {
                    std::set<string_id<T>> aliases;
                    assign( jo, alias_member_name, aliases, strict );

                    const int_id<T> ref = map[def.id];
                    for( const auto &e : aliases ) {
                        map[e] = ref;
                    }
                }

            } else if( jo.has_array( legacy_id_member_name ) ) {
                for( const JsonValue e : jo.get_array( legacy_id_member_name ) ) {
                    T def;
                    if( !handle_inheritance( def, jo, src ) ) {
                        break;
                    }
                    def.id = string_id<T>( e );
                    assign_src( def, src );
                    def.load( jo, src );
                    insert( def );
                }
                if( jo.has_member( alias_member_name ) ) {
                    jo.throw_error( string_format( "can not specify '%s' when '%s' is array",
                                                   alias_member_name, legacy_id_member_name ) );
                }

            } else if( !jo.has_string( abstract_member_name ) ) {
                jo.throw_error( string_format( "must specify either '%s' or '%s'/'%s'",
                                               abstract_member_name, id_member_name, legacy_id_member_name ) );
            }
        }
        /**
         * Add an object to the factory, without loading from JSON.
         * The new object replaces any existing object of the same id.
         * The function returns the actual object reference.
         */
        T &insert( const T &obj ) {
            // this invalidates `_cid` cache for all previously added string_ids,
            // but! it's necessary to invalidate cache for all possibly cached "missed" lookups
            // (lookups for not-yet-inserted elements)
            // in the common scenario there is no loss of performance, as `finalize` will make cache
            // for all ids valid again
            inc_version();
            const auto iter = map.find( obj.id );
            if( iter != map.end() ) {
                T &result = list[iter->second.to_i()];
                result = obj;
                result.id.set_cid_version( iter->second.to_i(), version );
                return result;
            }

            const int_id<T> cid( list.size() );
            list.push_back( obj );

            T &result = list.back();
            result.id.set_cid_version( cid.to_i(), version );
            map[result.id] = cid;
            return result;
        }

        /** Finalize all entries (derived classes should chain to this method) */
        virtual void finalize() {
            if( _is_finalized ) {
                debugmsg( "Attempted to finalize %s factory multiple times.", type_name );
            }

            DynamicDataLoader::get_instance().load_deferred( deferred );
            abstracts.clear();

            inc_version();
            for( size_t i = 0; i < list.size(); i++ ) {
                list[i].id.set_cid_version( static_cast<int>( i ), version );
            }
            set_finalized( true );
        }

        /**
         * Checks loaded/inserted objects for consistency
         */
        void check() const {
            for( const T &obj : list ) {
                obj.check();
            }
        }
        /**
         * Returns the number of loaded objects.
         */
        size_t size() const {
            return list.size();
        }
        /**
         * Returns whether factory is empty.
         */
        bool empty() const {
            return list.empty();
        }
        /**
         * Removes all loaded objects.
         * Postcondition: `size() == 0`
         */
        void reset() {
            set_finalized( false );
            inc_version();
            list.clear();
            map.clear();
            abstracts.clear();
            deferred.clear();
        }
        /**
         * Returns all the loaded objects. It can be used to iterate over them.
         */
        const std::vector<T> &get_all() const {
            return list;
        }
        /**
         * @name `string_id/int_id` interface functions
         *
         * The functions here are supposed to be used by the id classes, they have the
         * same behavior as described in the id classes and can be used directly by
         * forwarding the parameters to them and returning their result.
         */
        /**@{*/
        /**
         * Returns the object with the given id.
         * The input id should be valid, otherwise a debug message is issued.
         * This function can be used to implement @ref int_id::obj().
         * Note: If the id was valid, the returned object can be modified (after
         * casting the const away).
         */
        const T &obj( const int_id<T> &id ) const {
            if( !is_valid( id ) ) {
                debugmsg( "invalid %s id \"%d\"", type_name, id.to_i() );
                return dummy_obj;
            }
            return list[id.to_i()];
        }
        /**
         * Returns the object with the given id.
         * The input id should be valid, otherwise a debug message is issued.
         * This function can be used to implement @ref string_id::obj().
         * Note: If the id was valid, the returned object can be modified (after
         * casting the const away).
         */
        const T &obj( const string_id<T> &id ) const {
            int_id<T> i_id;
            if( !find_id( id, i_id ) ) {
                debugmsg( "invalid %s id \"%s\"", type_name, id.c_str() );
                return dummy_obj;
            }
            return list[i_id.to_i()];
        }
        /**
         * Checks whether the factory contains an object with the given id.
         * This function can be used to implement @ref int_id::is_valid().
         */
        bool is_valid( const int_id<T> &id ) const {
            return static_cast<size_t>( id.to_i() ) < list.size();
        }
        /**
         * Checks whether the factory contains an object with the given id.
         * This function can be used to implement @ref string_id::is_valid().
         */
        bool is_valid( const string_id<T> &id ) const {
            int_id<T> dummy;
            return find_id( id, dummy );
        }
        /**
         * Converts string_id<T> to int_id<T>. Returns null_id on failure.
         */
        int_id<T> convert( const string_id<T> &id, const int_id<T> &null_id ) const {
            int_id<T> result;
            if( find_id( id, result ) ) {
                return result;
            }
            debugmsg( "invalid %s id \"%s\"", type_name, id.c_str() );
            return null_id;
        }
        /**
         * Converts int_id<T> to string_id<T>. Returns null_id on failure.
         */
        const string_id<T> &convert( const int_id<T> &id ) const {
            return obj( id ).id;
        }
        /**@}*/

        /**
         * Wrapper around generic_factory::version.
         * Allows to have local caches that invalidate when corresponding generic factory invalidates.
         * Note: when created using it's default constructor, Version is guaranteed to be invalid.
        */
        class Version
        {
                friend generic_factory<T>;
            public:
                Version() = default;
            private:
                Version( int64_t version ) : version( version ) {}
                int64_t  version = -1;
            public:
                bool operator==( const Version &rhs ) const {
                    return version == rhs.version;
                }
                bool operator!=( const Version &rhs ) const {
                    return !( rhs == *this );
                }
        };

        // current version of this generic_factory
        Version get_version() {
            return Version( version );
        }

        // checks whether given version is the same as current version of this generic_factory
        bool is_valid( const Version &v ) {
            return v.version == version;
        }
};

/**
@file
Helper for loading from JSON

Loading (inside a `T::load(JsonObject &jo)` function) can be done with two functions
(defined here):
- `mandatory` loads required data and throws an error if the JSON data does not contain
  the required data.
- `optional` is for optional data, it has the same parameters and an additional default
  value that will be used if the JSON data does not contain the requested data. It may
  throw an error if the existing data is not valid (e.g. string instead of requested int).

The functions are designed to work with the `generic_factory` and therefor support the
`was_loaded` parameter (set be `generic_factory::load`). If that parameter is `true`, it
is assumed the object has already been loaded and missing JSON data is simply ignored
(the default value is not applied and no error is thrown upon missing mandatory data).

The parameters are this:
- `JsonObject jo` the object to load from.
- `bool was_loaded` whether the object had already been loaded completely.
- `std::string member_name` the name of the JSON member to load from.
- `T &member` a reference to the C++ object to store the loaded data.
- (for `optional`) a default value of any type that can be assigned to `member`.

Both functions use the native `read` functions of `JsonIn` (see there) to load the value.

Example:
\code
class Dummy {
    bool was_loaded = false;
    int a;
    std::string b;
    void load(JsonObject &jo) {
        mandatory(jo, was_loaded, "a", a);
        optional(jo, was_loaded, "b", b, "default value of b");
    }
};
\endcode

This only works if there is function with the matching type defined in `JsonIn`. For other
types, or if the loaded value needs to be converted (e.g. to `nc_color`), one can use the
reader classes/functions. `mandatory` and `optional` have an overload that requires the same
parameters and an additional reference to such a reader object.

\code
class Dummy2 {
    bool was_loaded = false;
    int b;
    nc_color c;
    void load(JsonObject &jo) {
        mandatory(jo, was_loaded, "b", b); // uses JsonIn::read(int&)
    }
};
\endcode

Both versions of `optional` have yet another overload that does not require an explicit default
value, a default initialized object of the member type will be used instead.

----

Readers must provide the following function:
`bool operator()( const JsonObject &jo, const std::string &member_name, T &member, bool was_loaded ) const

(This can be implemented as free function or as operator in a class.)

The parameters are the same as the for the `mandatory` function (see above). The `function shall
return `true` if the loading was done, or `false` if the JSON data did
not contain the requested member. If loading fails because of invalid data (but not missing
data), it should throw.

*/

/** @name Implementation of `mandatory` and `optional`. */
/**@{*/
template<typename MemberType>
inline void mandatory( const JsonObject &jo, const bool was_loaded, const std::string &name,
                       MemberType &member )
{
    if( !jo.read( name, member ) ) {
        if( !was_loaded ) {
            if( jo.has_member( name ) ) {
                jo.throw_error( "failed to read mandatory member \"" + name + "\"" );
            } else {
                jo.throw_error( "missing mandatory member \"" + name + "\"" );
            }
        }
    }
}
template<typename MemberType, typename ReaderType>
inline void mandatory( const JsonObject &jo, const bool was_loaded, const std::string &name,
                       MemberType &member, const ReaderType &reader )
{
    if( !reader( jo, name, member, was_loaded ) ) {
        if( !was_loaded ) {
            if( jo.has_member( name ) ) {
                jo.throw_error( "failed to read mandatory member \"" + name + "\"" );
            } else {
                jo.throw_error( "missing mandatory member \"" + name + "\"" );
            }
        }
    }
}

/*
 * Template vodoo:
 * The compiler will construct the appropriate one of these based on if the
 * type can support the operations being done.
 * So, it defaults to the false_type, but if it can use the *= operator
 * against a float, it then supports proportional, and the handle_proportional
 * template that isn't just a dummy is constructed.
 * Similarly, if it can use a += operator against it's own type, the non-dummy
 * handle_relative template is constructed.
 */
template<typename T, typename = std::void_t<>>
struct supports_proportional : std::false_type { };

template<typename T>
struct supports_proportional<T, std::void_t<decltype( std::declval<T &>() *= std::declval<float>() )>> :
std::true_type {};

// Explicitly specialize these templates for a couple types
// So the compiler does not attempt to use a template that it should not
template<>
struct supports_proportional<bool> : std::false_type {};

template<typename T>
concept SupportsProportional = supports_proportional<T>::value;

// This checks that all units:: types will support relative and proportional
static_assert( SupportsRelative<units::energy>, "units should support relative" );
static_assert( SupportsProportional<units::energy>, "units should support proportional" );

static_assert( SupportsRelative<int>, "ints should support relative" );
static_assert( SupportsProportional<int>, "ints should support proportional" );

static_assert( !SupportsRelative<bool>, "bools should not support relative" );
static_assert( !SupportsProportional<bool>, "bools should not support proportional" );

// Using string ids with ints doesn't make sense in practice, but it doesn't matter here
// The type that it is templated with does not change it's behavior
static_assert( !SupportsRelative<string_id<int>>, "string ids should not support relative" );
static_assert( !SupportsProportional<string_id<int>>,
               "string ids should not support proportional" );

// Using int ids with ints doesn't make sense in practice, but it doesn't matter here
// The type that it is templated with does not change it's behavior
static_assert( !SupportsRelative<int_id<int>>, "int ids should not support relative" );
static_assert( !SupportsProportional<int_id<int>>, "int ids should not support proportional" );

static_assert( !SupportsRelative<std::string>, "strings should not support relative" );
static_assert( !SupportsProportional<std::string>, "strings should not support proportional" );

// Grab an enum class from debug.h
static_assert( !SupportsRelative<DebugOutput>, "enum classes should not support relative" );
static_assert( !SupportsProportional<DebugOutput>, "enum classes should not support proportional" );

// Grab a normal enum from there too
static_assert( !SupportsRelative<DL>, "enums should not support relative" );
static_assert( !SupportsProportional<DL>, "enums should not support relative" );

// Dummy template:
// Warn if it's trying to use proportional where it cannot, but otherwise just
// return.
template<typename MemberType> requires( !SupportsProportional<MemberType> )
inline bool handle_proportional( const JsonObject &jo, const std::string &name, MemberType & )
{
    if( jo.has_object( "proportional" ) ) {
        JsonObject proportional = jo.get_object( "proportional" );
        proportional.allow_omitted_members();
        if( proportional.has_member( name ) ) {
            debugmsg( "Member %s of type %s does not support proportional", name,
                      demangle( typeid( MemberType ).name() ) );
        }
    }
    return false;
}

// Real template:
// Copy-from makes it so the thing we're inheriting from is used to construct
// this, so member will contain the value of the thing we inherit from
// So, check if there is a proportional entry, check if it's got a valid value
// and if it does, multiply the member by it.
template<SupportsProportional MemberType>
inline bool handle_proportional( const JsonObject &jo, const std::string &name, MemberType &member )
{
    if( jo.has_object( "proportional" ) ) {
        JsonObject proportional = jo.get_object( "proportional" );
        proportional.allow_omitted_members();
        // We need to check this here, otherwise we get problems with unvisited members
        if( !proportional.has_member( name ) ) {
            return false;
        }
        if( proportional.has_float( name ) ) {
            double scalar = proportional.get_float( name );
            if( scalar <= 0 || scalar == 1 ) {
                debugmsg( "Invalid scalar %g for %s", scalar, name );
                return false;
            }
            member *= scalar;
            return true;
        } else {
            jo.throw_error( "Invalid scalar for %s", name );
        }
    }
    return false;
}

// Dummy template:
// Warn when trying to use relative when it's not supported, but otherwise,
// return
template<typename MemberType> requires( !SupportsRelative<MemberType> )
inline bool handle_relative( const JsonObject &jo, const std::string &name, MemberType & )
{
    if( jo.has_object( "relative" ) ) {
        JsonObject relative = jo.get_object( "relative" );
        relative.allow_omitted_members();
        if( !relative.has_member( name ) ) {
            return false;
        }
        debugmsg( "Member %s of type %s does not support relative", name,
                  demangle( typeid( MemberType ).name() ) );
    }
    return false;
}

// Real template:
// Copy-from makes it so the thing we're inheriting from is used to construct
// this, so member will contain the value of the thing we inherit from
// So, check if there is a relative entry, then add it to our member
template<SupportsRelative MemberType>
inline bool handle_relative( const JsonObject &jo, const std::string &name, MemberType &member )
{
    if( jo.has_object( "relative" ) ) {
        JsonObject relative = jo.get_object( "relative" );
        relative.allow_omitted_members();
        // This needs to happen here, otherwise we get unvisited members
        if( !relative.has_member( name ) ) {
            return false;
        }
        MemberType adder;
        if( relative.read( name, adder ) ) {
            member += adder;
            return true;
        } else {
            jo.throw_error( "Invalid adder for %s", name );
        }
    }
    return false;
}

// No template magic here, yay!
template<typename MemberType>
inline void optional( const JsonObject &jo, const bool was_loaded, const std::string &name,
                      MemberType &member )
{
    if( !jo.read( name, member ) && !handle_proportional( jo, name, member ) &&
        !handle_relative( jo, name, member ) ) {
        if( !was_loaded ) {
            member = MemberType();
        }
    }
}
/*
Template trickery, not for the faint of heart. It is required because there are two functions
with 5 parameters. The first 4 are always the same: JsonObject, bool, member name, member reference.
The last one is different: in one case it's the default value, in the other case it's the reader
and there is no explicit default value there.
The enable_if stuff assumes that a `MemberType` can not be constructed from a `ReaderType`, in other
words: `MemberType foo( ReaderType(...) );` does not work. This is what `is_constructible` checks.
If the 5. parameter can be used to construct a `MemberType`, it is assumed to be the default value,
otherwise it is assumed to be the reader.
*/
template<typename MemberType, typename DefaultType = MemberType>
requires( std::is_constructible_v<MemberType, const DefaultType &> )
inline void optional( const JsonObject &jo, const bool was_loaded, const std::string &name,
                      MemberType &member, const DefaultType &default_value )
{
    if( !jo.read( name, member ) && !handle_proportional( jo, name, member ) &&
        !handle_relative( jo, name, member ) ) {
        if( !was_loaded ) {
            member = default_value;
        }
    }
}

template<typename MemberType, typename ReaderType, typename DefaultType = MemberType>
requires( !std::is_constructible_v<MemberType, const ReaderType &> )
inline void optional( const JsonObject &jo, const bool was_loaded, const std::string &name,
                      MemberType &member, const ReaderType &reader )
{
    if( !reader( jo, name, member, was_loaded ) ) {
        if( !was_loaded ) {
            member = MemberType();
        }
    }
}

template<typename MemberType, typename ReaderType, typename DefaultType = MemberType>
inline void optional( const JsonObject &jo, const bool was_loaded, const std::string &name,
                      MemberType &member, const ReaderType &reader, const DefaultType &default_value )
{
    if( !reader( jo, name, member, was_loaded ) ) {
        if( !was_loaded ) {
            member = default_value;
        }
    }
}
/**@}*/

/**
 * Reads a string and stores the first byte of it in `sym`. Throws if the input contains more
 * or less than one byte.
 */
bool one_char_symbol_reader( const JsonObject &jo, const std::string &member_name, int &sym,
                             bool );

/**
 * Reads a UTF-8 string (or int as legacy fallback) and stores Unicode codepoint of it in `symbol`.
 * Throws if the inputs width is more than one console cell wide.
 */
bool unicode_codepoint_from_symbol_reader( const JsonObject &jo,
        const std::string &member_name, uint32_t &member, bool );


