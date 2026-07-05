#pragma once

#include <iterator>
#include <map>
#include <memory>
#include <vector>
#include "enums.h"
#include "json.h"
#include "string_id.h"
#include "units.h"
#include "generic_readers.h"
#include "item_group.h"

template <typename T1, typename T2>
class min_max_reader : generic_typed_reader<min_max_reader<T1, T2>>
{
    public:
        bool operator()( const JsonObject &jo, const std::string &member_name,
                         std::pair<T1, T2> &member, bool /* was_loaded */ ) const {
            bool result = false;
            if( jo.has_array( member_name ) ) {
                JsonArray arr = jo.get_array( member_name );
                bool inner_result = arr.read_next( member.first );
                inner_result &= arr.read_next( member.second );
                result = inner_result;
            } else if( jo.has_member( member_name + "-min" ) || jo.has_member( member_name + "-max" ) ) {
                bool inner_result = jo.read( member_name + "-min", member.first );
                inner_result &= jo.read( member_name + "-max", member.second );
                result = inner_result;
            } else {
                bool inner_result = jo.read( member_name, member.first );
                inner_result &= jo.read( member_name, member.second );
                result = inner_result;
            }
            return result;
        }
};

class spawn_data_reader : generic_typed_reader<spawn_data_reader>
{
    public:
        bool supports_arrays;
        int ammo;
        int magazine;
        spawn_data_reader( bool supports_arrays, int ammo, int magazine )
            : supports_arrays( supports_arrays ),
              ammo( ammo ),
              magazine( magazine ) { }

        bool operator()( const JsonObject &jo, const std::string &member_name,
                         std::shared_ptr<Item_spawn_data> &member, bool /* was_loaded */ ) const {
            const std::string iname( member_name + "-item" );
            const std::string gname( member_name + "-group" );
            // String: id
            // Bool: is_group
            std::vector< std::pair<const std::string, bool> > entries;
            // Default probability
            const int prob = 100;

            auto get_array = [&]( const std::string & arr_name, const bool isgroup ) {
                if( !jo.has_array( arr_name ) ) {
                    return;
                } else if( !supports_arrays ) {
                    jo.throw_error( string_format( "You can't use an array for '%s'", arr_name ) );
                }
                for( const std::string line : jo.get_array( arr_name ) ) {
                    entries.emplace_back( line, isgroup );
                }
            };
            get_array( iname, false );
            get_array( gname, true );

            if( jo.has_member( member_name ) ) {
                jo.throw_error( string_format( "This has been a TODO: since 2014.  Use '%s' and/or '%s' instead.",
                                               iname, gname ) );
                // TODO: !
                return false;
            }
            if( jo.has_string( iname ) ) {
                entries.emplace_back( jo.get_string( iname ), false );
            }
            if( jo.has_string( gname ) ) {
                entries.emplace_back( jo.get_string( gname ), true );
            }

            if( entries.size() > 1 && !supports_arrays ) {
                jo.throw_error( string_format( "You can only use one of '%s' and '%s'", iname, gname ) );
                return false;
            } else if( entries.size() == 1 ) {
                const auto type = entries.front().second ? Single_item_creator::Type::S_ITEM_GROUP :
                                  Single_item_creator::Type::S_ITEM;
                Single_item_creator *result = new Single_item_creator( entries.front().first, type, prob );
                result->inherit_ammo_mag_chances( ammo, magazine );
                member.reset( result );
                return true;
            } else if( entries.empty() ) {
                return false;
            }
            Item_group *result = new Item_group( Item_group::Type::G_COLLECTION, prob, ammo,
                                                 magazine );
            member.reset( result );
            for( const auto &elem : entries ) {
                if( elem.second ) {
                    result->add_group_entry( item_group_id( elem.first ), prob );
                } else {
                    result->add_item_entry( itype_id( elem.first ), prob );
                }
            }
            return true;
        }
};

class itemgroup_reader : generic_typed_reader<itemgroup_reader>
{
    public:
        const std::string default_subtype;
        static constexpr std::string random_group_id_prefix = "\u01F7";
        itemgroup_reader( std::string default_subtype ) : default_subtype( default_subtype ) {}

        // Mild little hack, when getting a single object, start some shennangans
        // When it is an array always add dont try to stack them
        bool read_normal_with_member( const JsonObject &jo, const std::string &name,
                                      item_group_id &member ) const {
            if( jo.has_member( name ) ) {
                const itemgroup_reader &derived = static_cast<const itemgroup_reader &>( *this );
                member = derived.get_next( *jo.get_raw( name ), member );
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
            return read_normal_with_member( jo, member_name, member ) ||
                   handle_proportional( jo, member_name, member ) ||
                   do_relative( jo, member_name, member );
        }

        item_group_id get_next( JsonIn &jin, item_group_id member = item_group_id::NULL_ID() ) const;
};
