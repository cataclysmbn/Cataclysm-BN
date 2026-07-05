#include "item_group_readers.h"

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

item_group_id itemgroup_reader::get_next( JsonIn &jin, item_group_id member ) const
{
    if( jin.test_string() ) {
        if( member.is_valid() && member != item_group::empty ) {
            auto nested_group = Item_group( Item_group::G_COLLECTION, 100, 100, 100 );
            nested_group.add_group_entry( item_group_id( jin.get_string() ), 100 );
            nested_group.add_group_entry( member, 100 );
            nested_group.id = item_group::get_unique_group_id();
            nested_group.is_inline = true;
            item_group::insert_itemgroup( nested_group );
            return nested_group.id;
        }
        return item_group_id( jin.get_string() );
    } else if( jin.test_object() ) {
        const JsonObject &jo = jin.get_object();
        const std::string subtype = jo.get_string( "subtype", default_subtype );
        if( jo.has_bool( "purge" ) && jo.get_bool( "purge" ) ) {
            member = item_group_id::NULL_ID();
        }
        auto g = Item_group( Item_group::G_COLLECTION, 100, 100, 100 );
        item_group_id group_id = item_group::get_unique_group_id();
        bool need_nested = false;
        if( member.is_valid() && member != item_group::empty ) {
            // If the fine prefix is the prefix
            // We know this is an INLINE item group
            // And thus fine to be copied
            if( member->is_inline ) {
                g = Item_group( *member );
                g.probability = 100;
                g.id = group_id;
                g.is_inline = true;
            } else {
                // Minor issue here, we have something like an external item group
                // ( Which could be later extended ) and an inline item group
                // This means we have to make the added item group nested.
                need_nested = true;
            }
        } else {
            g.id = group_id;
            g.is_inline = true;
        }
        g.load( jo, "", subtype );
        if( !need_nested ) {
            item_group::insert_itemgroup( g );
            return g.id;
        } else {
            auto nested_group = Item_group( Item_group::G_COLLECTION, 100, 100, 100 );
            nested_group.add_entry( std::make_shared<Item_group>( g ) );
            nested_group.add_group_entry( member, 100 );
            nested_group.id = group_id;
            nested_group.is_inline = true;
            item_group::insert_itemgroup( nested_group );
            return nested_group.id;
        }
    } else if( jin.test_array() ) {
        auto g = Item_group( Item_group::G_COLLECTION, 100, 100, 100 );
        item_group_id group_id = item_group::get_unique_group_id();
        bool need_nested = false;
        if( member.is_valid() && member != item_group::empty ) {
            if( member->is_inline ) {
                g = Item_group( *member );
                g.probability = 100;
                g.id = group_id;
                g.is_inline = true;
            } else {
                need_nested = true;
            }
        } else {
            g.id = group_id;
            g.is_inline = true;
        }
        for( const JsonValue subobj : jin.get_array() ) {
            if( subobj.test_object() ) {
                g.add_entry( subobj.get_object() );
            } else if( subobj.test_string() ) {
                // Fallback on items
                // Used by profession.cpp
                g.add_item_entry( itype_id( subobj.get_string() ), 100 );
            } else {
                jin.error( "invalid array member. Must be string (itype id) or object (group data)" );
            }
        }
        if( need_nested ) {
            auto nested_group = Item_group( Item_group::G_COLLECTION, 100, 100, 100 );
            nested_group.add_entry( std::make_shared<Item_group>( g ) );
            nested_group.add_group_entry( member, 100 );
            nested_group.id = group_id;
            nested_group.is_inline = true;
            item_group::insert_itemgroup( nested_group );
            return nested_group.id;
        } else {
            item_group::insert_itemgroup( g );
            return g.id;
        }
    } else {
        jin.error( "invalid item group, must be string (group id) or object/array (the group data)" );
        // stream.error always throws, this is here to prevent a warning
        return item_group_id{};
    }

}
