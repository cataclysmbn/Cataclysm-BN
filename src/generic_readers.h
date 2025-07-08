#pragma once

#include <map>
#include <vector>
#include "json.h"
#include "units.h"

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
struct supports_relative : std::false_type { };

template<typename T>
struct supports_relative < T, std::void_t < decltype( std::declval<T &>() += std::declval<T &>() )
>> : std::true_type {};

// Explicitly specialize these templates for a couple types
// So the compiler does not attempt to use a template that it should not
template<>
struct supports_relative<bool> : std::false_type {};

template<>
struct supports_relative<std::string> : std::false_type {};

template<typename T> concept SupportsRelative = supports_relative<T>::value;

namespace reader_detail
{
template<typename T>
struct handler {
    static constexpr bool is_container = false;
};

template<typename T> concept Container = handler<T>::is_container;

template<typename T> concept RelativeContainer = Container<T> &&SupportsRelative<T>;

template<typename T>
struct handler<std::set<T>> {
    void clear( std::set<T> &container ) const {
        container.clear();
    }
    void insert( std::set<T> &container, const T &data ) const {
        container.insert( data );
    }
    void erase( std::set<T> &container, const T &data ) const {
        container.erase( data );
    }
    static constexpr bool is_container = true;
};

template<size_t N>
struct handler<std::bitset<N>> {
    void clear( std::bitset<N> &container ) const {
        container.reset();
    }
    template<typename T>
    void insert( std::bitset<N> &container, const T &data ) const {
        container.set( data );
    }
    template<typename T>
    void erase( std::bitset<N> &container, const T &data ) const {
        container.reset( data );
    }
    static constexpr bool is_container = true;
};

template<typename E>
struct handler<enum_bitset<E>> {
    void clear( enum_bitset<E> &container ) const {
        container.clear_all();
    }
    template<typename T>
    void insert( enum_bitset<E> &container, const T &data ) const {
        container.set( data );
    }
    template<typename T>
    void erase( enum_bitset<E> &container, const T &data ) const {
        container.clear( data );
    }
    static constexpr bool is_container = true;
};

template<typename T>
struct handler<std::vector<T>> {
    void clear( std::vector<T> &container ) const {
        container.clear();
    }
    void insert( std::vector<T> &container, const T &data ) const {
        container.push_back( data );
    }
    template<typename E>
    void erase( std::vector<T> &container, const E &data ) const {
        erase_if( container, [&data]( const T & e ) {
            return e == data;
        } );
    }
    template<typename P>
    void erase_if( std::vector<T> &container, const P &predicate ) const {
        const auto iter = std::find_if( container.begin(), container.end(), predicate );
        if( iter != container.end() ) {
            container.erase( iter );
        }
    }
    static constexpr bool is_container = true;
};
} // namespace reader_detail

/**
 * Base class for reading generic objects from JSON.
 * It can load members being certain containers or being a single value.
 * The function get_next() needs to be implemented to read and convert the data from JSON.
 * It uses the curiously recurring template pattern, you have to derive your new class
 * `MyReader` from `generic_typed_reader<MyReader>` and implement `get_next` and
 * optionally `erase_next`.
 * Most function calls here are done on a `Derived`, which means it can "override" them.
 * This even allows changing their signature and return type.
 *
 * - If the object is new (`was_loaded` is `false`), only the given JSON member is read
 *   and assigned, overriding any existing content of it.
 * - If the object is not new and the member exists, it is read and assigned as well.
 * - If the object is not new and the member does not exists, two further members are examined:
 *   entries from `"extend"` are added to the set and entries from `"delete"`
 *   are removed. This only works if the member is actually a container, not just a single value.
 *
 * Example:
 * The JSON `{ "f": ["a","b","c"] }` would be loaded as the set `{"a","b","c"}`.
 * Loading the set again from the JSON `{ "delete": { "f": ["c","x"] }, "extend": { "f": ["h"] } }`
 * would add the "h" flag and removes the "c" and the "x" flag, resulting in `{"a","b","h"}`.
 *
 * @tparam Derived The class that inherits from this. It must implement the following:
 *   - `Foo get_next( JsonIn & ) const`: reads the next value from JSON, converts it into some
 *      type `Foo` and returns it. The returned value is assigned to the loaded member (see reader
 *      interface above), or is inserted into the member (if it's a container). The type `Foo` must
 *      be compatible with those uses (read: it should be the same type).
 *   - (optional) `erase_next( JsonIn &jin, C &container ) const`, the default implementation here
 *      reads a value from JSON via `get_next` and removes the matching value in the container.
 *      The value in the container must match *exactly*. You may override this function to allow
 *      a different matching algorithm, e.g. reading a simple id from JSON and remove entries with
 *      the same id from the container.
 */
template<typename Derived>
class generic_typed_reader
{
    public:
        template<typename C>
        void insert_values_from( const JsonObject &jo, const std::string &member_name,
                                 C &container ) const {
            const Derived &derived = static_cast<const Derived &>( *this );
            if( !jo.has_member( member_name ) ) {
                return;
            }
            JsonIn &jin = *jo.get_raw( member_name );
            // We allow either a single value or an array of values. Note that this will not work
            // correctly if the thing we load from JSON is itself an array.
            if( jin.test_array() ) {
                jin.start_array();
                while( !jin.end_array() ) {
                    derived.insert_next( jin, container );
                }
            } else {
                derived.insert_next( jin, container );
            }
        }
        template<typename C>
        void insert_next( JsonIn &jin, C &container ) const {
            const Derived &derived = static_cast<const Derived &>( *this );
            reader_detail::handler<C>().insert( container, derived.get_next( jin ) );
        }

        template<typename C>
        void erase_values_from( const JsonObject &jo, const std::string &member_name, C &container ) const {
            const Derived &derived = static_cast<const Derived &>( *this );
            if( !jo.has_member( member_name ) ) {
                return;
            }
            JsonIn &jin = *jo.get_raw( member_name );
            // Same as for inserting: either an array or a single value, same caveat applies.
            if( jin.test_array() ) {
                jin.start_array();
                while( !jin.end_array() ) {
                    derived.erase_next( jin, container );
                }
            } else {
                derived.erase_next( jin, container );
            }
        }
        template<typename C>
        void erase_next( JsonIn &jin, C &container ) const {
            const Derived &derived = static_cast<const Derived &>( *this );
            reader_detail::handler<C>().erase( container, derived.get_next( jin ) );
        }

        /**
         * Implements the reader interface, handles members that are containers of flags.
         * The functions forwards the actual changes to assign(), insert()
         * and erase(), which are specialized for various container types.
         * The `enable_if` is here to prevent the compiler from considering it
         * when called on a simple data member, the other `operator()` will be used.
         */
        template<typename C>
        bool operator()( const JsonObject &jo, const std::string &member_name,
                         C &container, bool was_loaded ) const requires reader_detail::handler<C>::is_container {
            const Derived &derived = static_cast<const Derived &>( *this );
            // If you get an error about "incomplete type 'struct reader_detail::handler...",
            // you have to implement a specialization of your container type, so above for
            // existing specializations in namespace reader_detail.
            if( jo.has_member( member_name ) ) {
                reader_detail::handler<C>().clear( container );
                derived.insert_values_from( jo, member_name, container );
                return true;
            } else if( !was_loaded ) {
                return false;
            } else {
                if( jo.has_object( "extend" ) ) {
                    JsonObject tmp = jo.get_object( "extend" );
                    tmp.allow_omitted_members();
                    derived.insert_values_from( tmp, member_name, container );
                }
                if( jo.has_object( "delete" ) ) {
                    JsonObject tmp = jo.get_object( "delete" );
                    tmp.allow_omitted_members();
                    derived.erase_values_from( tmp, member_name, container );
                }
                return true;
            }
        }

        /*
         * These two functions are effectively handle_relative but they need to
         * use the reader, so they must be here.
         * proportional does not need these, because it's only reading a float
         * whereas these are reading values of the same type.
         */
        // Type does not support relative
        template<typename C> requires( !reader_detail::Container<C> || !SupportsRelative<C> )
        bool do_relative( const JsonObject &jo, const std::string &name, C & ) const {
            if( jo.has_object( "relative" ) ) {
                JsonObject relative = jo.get_object( "relative" );
                relative.allow_omitted_members();
                if( !relative.has_member( name ) ) {
                    return false;
                }
                debugmsg( "Member %s of type %s does not support relative", name, demangle( typeid( C ).name() ) );
            }
            return false;
        }

        // Type supports relative
        template<reader_detail::RelativeContainer C>
        bool do_relative( const JsonObject &jo, const std::string &name, C &member ) const {
            if( jo.has_object( "relative" ) ) {
                JsonObject relative = jo.get_object( "relative" );
                relative.allow_omitted_members();
                const Derived &derived = static_cast<const Derived &>( *this );
                // This needs to happen here, otherwise we get unvisited members
                if( !relative.has_member( name ) ) {
                    return false;
                }
                C adder = derived.get_next( *relative.get_raw( name ) );
                member += adder;
                return true;
            }
            return false;
        }

        template<typename C>
        bool read_normal( const JsonObject &jo, const std::string &name, C &member ) const {
            if( jo.has_member( name ) ) {
                const Derived &derived = static_cast<const Derived &>( *this );
                member = derived.get_next( *jo.get_raw( name ) );
                return true;
            }
            return false;
        }

        /**
         * Implements the reader interface, handles a simple data member.
         */
        // was_loaded is ignored here, if the value is not found in JSON, report to
        // the caller, which will take action on their own.
        template<typename C> requires( !reader_detail::Container<C> )
        bool operator()( const JsonObject &jo, const std::string &member_name,
                         C &member, bool /*was_loaded*/ ) const {
            return read_normal( jo, member_name, member ) ||
                   handle_proportional( jo, member_name, member ) ||
                   do_relative( jo, member_name, member );
        }
};

/**
 * Converts the JSON string to some type that must be construable from a `std::string`,
 * e.g. @ref string_id.
 * Example:
 * \code
 *   std::set<string_id<Foo>> set;
 *   mandatory( jo, was_loaded, "set", set, auto_flags_reader<string_id<Foo>>{} );
 *   // It also works for containers of simple strings:
 *   std::set<std::string> set2;
 *   mandatory( jo, was_loaded, "set2", set2, auto_flags_reader<>{} );
 * \endcode
 */
template<typename FlagType = std::string>
class auto_flags_reader : public generic_typed_reader<auto_flags_reader<FlagType>>
{
    public:
        FlagType get_next( JsonIn &jin ) const {
            return FlagType( jin.get_string() );
        }
};

using string_reader = auto_flags_reader<>;

template <typename T>
class unit_reader : generic_typed_reader<T>
{
    private:
        const std::vector<std::pair<std::string, T>> &type_units;
    protected:
        unit_reader( const std::vector<std::pair<std::string, T>> &type_units )
            : type_units( type_units )
        {}
    public:
        bool operator()( const JsonObject &jo, const std::string &member_name,
                         T &member, bool /* was_loaded */ ) const {
            if( !jo.has_member( member_name ) ) {
                return false;
            }
            member = read_from_json_string<T>( *jo.get_raw( member_name ), type_units );
            return true;
        }
        units::volume get_next( JsonIn &jin ) const {
            return read_from_json_string<units::volume>( jin, type_units );
        }
};

class volume_reader : public unit_reader<units::volume>
{
    public:
        volume_reader()
            : unit_reader( units::volume_units )
        {}
};

class mass_reader : public unit_reader<units::mass>
{
    public:
        mass_reader()
            : unit_reader( units::mass_units )
        {}
};

class temperature_reader : public unit_reader<units::temperature>
{
    public:
        temperature_reader()
            : unit_reader( units::temperature_units )
        {}
};

/**
 * Uses a map (unordered or standard) to convert strings from JSON to some other type
 * (the mapped type of the map: `C::mapped_type`). It works for all mapped types, not just enums.
 *
 * One can use this if the member is `std::set<some_enum>` or `some_enum` and a
 * map `std::map<std::string, some_enum>` with all the value enumeration values exists.
 *
 * The class can be conveniently instantiated for a given map `mapping` using
 * the helper function @ref make_flag_reader (see below).
 * The flag type (@ref flag_type) is used when the input contains invalid flags
 * (a string that is not contained in the map). It should sound something like
 * "my-enum-type".
 */
template<typename T>
class typed_flag_reader : public generic_typed_reader<typed_flag_reader<T>>
{
    private:
        using map_t = std::map<std::string, T>;

    private:
        const map_t &flag_map;
        const std::string flag_type;

    public:
        typed_flag_reader( const map_t &flag_map, const std::string &flag_type )
            : flag_map( flag_map )
            , flag_type( flag_type ) {
        }

        T get_next( JsonIn &jin ) const {
            const std::string flag = jin.get_string();
            const auto iter = flag_map.find( flag );

            if( iter == flag_map.cend() ) {
                jin.seek( jin.tell() );
                jin.error( string_format( "invalid %s: \"%s\"", flag_type, flag ) );
            }

            return iter->second;
        }
};

template<typename T>
typed_flag_reader<T> make_flag_reader( const std::map<std::string, T> &m, const std::string &e )
{
    return typed_flag_reader<T> { m, e };
}

/**
 * Uses @ref io::string_to_enum to convert the string from JSON to a C++ enum.
 */
template<typename E>
class enum_flags_reader : public generic_typed_reader<enum_flags_reader<E>>
{
    private:
        const std::string flag_type;

    public:
        enum_flags_reader( const std::string &flag_type ) : flag_type( flag_type ) {
        }

        E get_next( JsonIn &jin ) const {
            const auto position = jin.tell();
            const std::string flag = jin.get_string();
            try {
                return io::string_to_enum<E>( flag );
            } catch( const io::InvalidEnumString & ) {
                jin.seek( position );
                jin.error( string_format( "invalid %s: \"%s\"", flag_type, flag ) );
                throw; // ^^ throws already
            }
        }
};

/**
 * Loads string_id from JSON
 */
template<typename T>
class string_id_reader : public generic_typed_reader<string_id_reader<T>>
{
    public:
        string_id<T> get_next( JsonIn &jin ) const {
            return string_id<T>( jin.get_string() );
        }
};

/**
 * Reads a volume value from legacy format: JSON contains a integer which represents multiples
 * of `units::legacy_volume_factor` (250 ml).
 */
inline bool legacy_volume_reader( const JsonObject &jo, const std::string &member_name,
                                  units::volume &value, bool )
{
    int legacy_value;
    if( !jo.read( member_name, legacy_value ) ) {
        return false;
    }
    value = legacy_value * units::legacy_volume_factor;
    return true;
}


