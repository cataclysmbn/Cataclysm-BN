#include "item_group.h"

#include <algorithm>
#include <cassert>
#include <set>
#include <vector>

#include "calendar.h"
#include "debug.h"
#include "detached_ptr.h"
#include "enums.h"
#include "flag.h"
#include "flat_set.h"
#include "game_constants.h"
#include "generic_factory.h"
#include "generic_readers.h"
#include "item.h"
#include "item_factory.h"
#include "item_group_readers.h"
#include "itype.h"
#include "iuse_actor.h"
#include "json.h"
#include "rng.h"
#include "string_id.h"
#include "type_id.h"
#include "type_id_implement.h"
#include "options.h"

namespace
{
// This might appear to be more complicated then normal, but this basically implements
// Purge and previous itemgroup copy logic in a generic factory
generic_factory<Item_group> all_itemgroups( "Item Groups", "id", "alias", "purge", true );
}

IMPLEMENT_STRING_AND_INT_IDS( Item_group, all_itemgroups );

std::vector<detached_ptr<item>> Item_spawn_data::create( const time_point &birthday ) const
{
    RecursionList rec;
    return create( birthday, rec );
}

detached_ptr<item> Item_spawn_data::create_single( const time_point &birthday ) const
{
    RecursionList rec;
    return create_single( birthday, rec );
}

Single_item_creator::Single_item_creator( const std::string &_id, Type _type, int _probability )
    : Item_spawn_data( _probability )
    , id( _id )
    , type( _type )
{
}

detached_ptr<item> Single_item_creator::create_single( const time_point &birthday,
        RecursionList &rec ) const
{
    detached_ptr<item> tmp;
    if( type == S_ITEM ) {
        if( id == "corpse" ) {
            tmp = item::make_corpse( mtype_id::NULL_ID(), birthday );
        } else {
            tmp = item::spawn( id, birthday );
        }
    } else if( type == S_ITEM_GROUP ) {
        if( std::ranges::contains( rec, id ) ) {
            debugmsg( "recursion in item spawn list %s", id.c_str() );
            return detached_ptr<item>();
        }
        rec.push_back( id );
        item_group_id igroup_id = item_group_id( id );
        if( !igroup_id.is_valid() ) {
            debugmsg( "unknown item spawn list %s", igroup_id.str() );
            return detached_ptr<item>();
        }
        tmp = igroup_id->create_single( birthday, rec );
        rec.erase( rec.end() - 1 );
    } else if( type == S_NONE ) {
        return detached_ptr<item>();
    }
    if( !tmp ) {
        return detached_ptr<item>();
    }
    if( one_in( 3 ) && tmp->has_flag( flag_VARSIZE ) ) {
        tmp->set_flag( flag_FIT );
    }
    if( modifier ) {
        tmp = modifier->modify( std::move( tmp ) );
    } else {
        // TODO: change the spawn lists to contain proper references to containers
        tmp = item::in_its_container( std::move( tmp ) );
    }
    return tmp;
}

std::vector<detached_ptr<item>> Single_item_creator::create( const time_point &birthday,
                             RecursionList &rec ) const
{
    std::vector<detached_ptr<item>> result;
    int cnt = 1;
    if( modifier ) {
        auto modifier_count = modifier->count;
        cnt = ( modifier_count.first == modifier_count.second ) ? modifier_count.first : rng(
                  modifier_count.first, modifier_count.second );
    }
    for( ; cnt > 0; cnt-- ) {
        if( type == S_ITEM ) {
            detached_ptr<item> itm = create_single( birthday, rec );
            if( itm && !itm->is_null() ) {
                result.push_back( std::move( itm ) );
            }
        } else {
            if( std::ranges::contains( rec, id ) ) {
                debugmsg( "recursion in item spawn list %s", id.c_str() );
                return result;
            }
            rec.push_back( id );
            const auto igroup_id = item_group_id( id );
            if( !igroup_id.is_valid() ) {
                debugmsg( "unknown item spawn list %s", igroup_id.str() );
                return result;
            }
            std::vector<detached_ptr<item>> tmplist = igroup_id->create( birthday, rec );
            rec.erase( rec.end() - 1 );
            if( modifier ) {
                for( auto &elem : tmplist ) {
                    elem = modifier->modify( std::move( elem ) );
                }
            }
            result.insert( result.end(), std::make_move_iterator( tmplist.begin() ),
                           std::make_move_iterator( tmplist.end() ) );
        }
    }
    return result;
}


bool Single_item_creator::remove_item( const itype_id &itemid )
{
    if( modifier ) {
        if( modifier->remove_item( itemid ) ) {
            type = S_NONE;
            return true;
        }
    }
    if( type == S_ITEM ) {
        if( itemid.str() == id ) {
            type = S_NONE;
            return true;
        }
    } else if( type == S_ITEM_GROUP ) {
        auto igroup_id = item_group_id( id );
        if( igroup_id.is_valid() ) {
            const_cast<Item_group &>( *igroup_id ).remove_item( itemid );
        }
    }
    return type == S_NONE;
}

bool Single_item_creator::replace_item( const itype_id &itemid, const itype_id &replacementid,
                                        const std::string &context )
{
    if( modifier ) {
        if( modifier->replace_item( itemid, replacementid, context ) ) {
            return true;
        }
    }
    if( type == S_ITEM ) {
        if( itemid.str() == id ) {
            if( get_option<bool>( "MIGRATION_CHECKS" ) ) {
                debugmsg( "Migrated item: %s in ( %s ), should be migrated to %s", itemid,
                          context, replacementid );
            }
            id = replacementid.str();
            return true;
        }
    } else if( type == S_ITEM_GROUP ) {
        auto igroup_id = item_group_id( id );
        if( igroup_id.is_valid() ) {
            const_cast<Item_group &>( *igroup_id ).replace_item( itemid, replacementid, "in itemgroup " + id );
        }
    }
    return type == S_NONE;
}

bool Single_item_creator::has_item( const itype_id &itemid ) const
{
    return type == S_ITEM && itemid.str() == id;
}

std::set<const itype *> Single_item_creator::every_item() const
{
    switch( type ) {
        case S_ITEM: {
            const itype *ptr = &*itype_id( id );
            return { ptr };
        }
        case S_ITEM_GROUP: {
            auto igroup_id = item_group_id( id );
            if( igroup_id.is_valid() ) {
                return igroup_id->every_item();
            }
            return {};
        }
        case S_NONE:
            return {};
    }
    assert( !"Unexpected type" );
    return {};
}

std::vector<detached_ptr<item>> Single_item_creator::every_item_modified( bool modify ) const
{
    std::vector<detached_ptr<item>> items;
    switch( type ) {
        case S_ITEM: {
            detached_ptr<item> itm = item::spawn( itype_id( id ) );
            if( modifier && modify && itm ) {
                items.push_back( modifier->modify( std::move( itm ) ) );
            } else if( itm ) {
                items.push_back( std::move( itm ) );
            }
            return items;
        }
        case S_ITEM_GROUP: {
            auto group_id = item_group_id( id );
            // Check consistency so we get the consistency errors not the
            // I failed to place it right errors
            if( !group_id.is_valid() ) {
                debugmsg( "Invalid item group %s", id );
                return {};
            }
            if( !group_id->checked ) {
                group_id->check_consistency( "" );
                const_cast<Item_group &>( *group_id ).checked = true;
            }
            std::vector<detached_ptr<item>> item_group_items = group_id->every_item_modified( true );
            items.reserve( items.size() + item_group_items.size() );
            for( auto &itm : item_group_items ) {
                if( modifier && modify ) {
                    items.push_back( modifier->modify( std::move( itm ) ) );
                } else {
                    items.push_back( std::move( itm ) );
                }
            }
            return items;
        }
        case S_NONE:
            return {};
    }
    assert( !"Unexpected type" );
    return {};
}

void Single_item_creator::inherit_ammo_mag_chances( const int ammo, const int mag )
{
    if( ammo != 0 || mag != 0 ) {
        if( !modifier ) {
            modifier.emplace();
        }
        modifier->with_ammo = ammo;
        modifier->with_magazine = mag;
    }
}

Item_modifier::Item_modifier()
    : damage( 0, 0 )
    , count( 1, 1 )
      // Dirt in guns is capped unless overwritten in the itemgroup
      // most guns should not be very dirty or dirty at all
    , dirt( 0, 500 )
    , charges( -1, -1 )
    , with_ammo( 0 )
    , with_magazine( 0 )
{
}

detached_ptr<item> Item_modifier::modify( detached_ptr<item> &&new_item ) const
{
    if( new_item->is_null() ) {
        return std::move( new_item );
    }

    new_item->set_damage( rng( damage.first, damage.second ) * itype::damage_scale );
    // no need for dirt if it's a bow
    if( new_item->is_gun() && !new_item->has_flag( flag_PRIMITIVE_RANGED_WEAPON ) &&
        !new_item->has_flag( flag_NON_FOULING ) ) {
        int random_dirt = rng( dirt.first, dirt.second );
        // if gun RNG is dirty, must add dirt fault to allow cleaning
        if( random_dirt > 0 ) {
            new_item->set_var( "dirt", random_dirt );
            new_item->faults.emplace( "fault_gun_dirt" );
            // chance to be unlubed, but only if it's not a laser or something
        } else if( one_in( 10 ) && !new_item->has_flag( flag_NEEDS_NO_LUBE ) ) {
            new_item->faults.emplace( "fault_gun_unlubricated" );
        }
    }

    // create container here from modifier or from default to get max charges later
    detached_ptr<item> cont;
    if( container != nullptr ) {
        cont = container->create_single( new_item->birthday() );
    }
    if( ( !cont || cont->is_null() ) && new_item->type->default_container.has_value() ) {
        const itype_id &cont_value = new_item->type->default_container.value_or( itype_id::NULL_ID() );
        if( !cont_value.is_null() ) {
            cont = item::spawn( cont_value, new_item->birthday() );
        }
    }

    int max_capacity = -1;
    if( charges.first != -1 && charges.second == -1 ) {
        const int max_ammo = new_item->ammo_capacity();
        if( max_ammo > 0 ) {
            max_capacity = max_ammo;
        }
    }

    if( max_capacity == -1 && cont != nullptr && !cont->is_null() && ( new_item->made_of( LIQUID ) ||
            ( !new_item->is_tool() && !new_item->is_gun() && !new_item->is_magazine() ) ) ) {
        max_capacity = new_item->charges_per_volume( cont->get_container_capacity() );
    }

    const bool charges_not_set = charges.first == -1 && charges.second == -1;
    int ch = -1;
    if( !charges_not_set ) {
        int charges_min = charges.first == -1 ? 0 : charges.first;
        int charges_max = charges.second == -1 ? max_capacity : charges.second;

        if( charges_min == -1 && charges_max != -1 ) {
            charges_min = 0;
        }

        if( max_capacity != -1 && ( charges_max > max_capacity || ( charges_min != 1 &&
                                    charges_max == -1 ) ) ) {
            charges_max = max_capacity;
        }

        if( charges_min > charges_max ) {
            charges_min = charges_max;
        }

        ch = charges_min == charges_max ? charges_min : rng( charges_min,
                charges_max );
    } else if( cont != nullptr && !cont->is_null() && new_item->made_of( LIQUID ) ) {
        new_item->charges = std::max( 1, max_capacity );
    }

    if( ch != -1 ) {
        if( new_item->count_by_charges() || new_item->made_of( LIQUID ) ) {
            // food, ammo
            // count_by_charges requires that charges is at least 1. It makes no sense to
            // spawn a "water (0)" item.
            new_item->charges = std::max( 1, ch );
        } else if( new_item->is_tool() ) {
            const int qty = std::min( ch, new_item->ammo_capacity() );
            new_item->charges = qty;
            if( !new_item->ammo_types().empty() && qty > 0 ) {
                new_item->ammo_set( new_item->ammo_default(), qty );
            }
        } else if( new_item->type->can_have_charges() ) {
            new_item->charges = ch;
        }
    }

    if( ch > 0 && ( new_item->is_gun() || new_item->is_magazine() ) ) {
        if( ammo == nullptr ) {
            // In case there is no explicit ammo item defined, use the default ammo
            if( !new_item->ammo_types().empty() ) {
                new_item->ammo_set( new_item->ammo_default(), ch );
            }
        } else {
            detached_ptr<item> am = ammo->create_single( new_item->birthday() );
            if( am ) {
                new_item->ammo_set( am->typeId(), ch );
            }
        }
        // Make sure the item is in valid state
        if( new_item->ammo_data() && new_item->magazine_integral() ) {
            new_item->charges = std::min( new_item->charges, new_item->ammo_capacity() );
        } else {
            new_item->charges = 0;
        }
    }

    if( new_item->is_tool() || new_item->is_gun() || new_item->is_magazine() ) {
        bool spawn_ammo = rng( 0, 99 ) < with_ammo && new_item->ammo_remaining() == 0 && ch == -1 &&
                          ( !new_item->is_tool() || new_item->type->tool->rand_charges.empty() );
        bool spawn_mag  = rng( 0, 99 ) < with_magazine && !new_item->magazine_current()
                          && new_item->magazine_default() != itype_id::NULL_ID();

        if( spawn_mag ) {
            new_item->put_in( item::spawn( new_item->magazine_default(), new_item->birthday() ) );
        }

        if( spawn_ammo ) {
            if( ammo ) {
                detached_ptr<item> am = ammo->create_single( new_item->birthday() );
                if( am ) {
                    new_item->ammo_set( am->typeId() );
                }
            } else {
                new_item->ammo_set( new_item->ammo_default() );
            }
        }
    }

    if( cont != nullptr && !cont->is_null() ) {
        cont->put_in( std::move( new_item ) );
        new_item = std::move( cont );
    }

    if( contents != nullptr ) {
        std::vector<detached_ptr<item>> contentitems = contents->create( new_item->birthday() );
        for( detached_ptr<item> &it : contentitems ) {
            new_item->put_in( std::move( it ) );
        }
    }

    for( const flag_id &flag : custom_flags ) {
        new_item->set_flag( flag );
    }

    for( const auto &fn : postprocess_fns ) {
        new_item = fn( std::move( new_item ) );
    }
    return std::move( new_item );
}

bool Item_modifier::remove_item( const itype_id &itemid )
{
    if( ammo != nullptr ) {
        if( ammo->remove_item( itemid ) ) {
            ammo.reset();
        }
    }
    if( container != nullptr ) {
        if( container->remove_item( itemid ) ) {
            container.reset();
            return true;
        }
    }
    return false;
}

bool Item_modifier::replace_item( const itype_id &itemid, const itype_id &replacementid,
                                  const std::string &context )
{
    if( ammo != nullptr ) {
        ammo->replace_item( itemid, replacementid, "ammo of " + context );
    }
    if( container != nullptr ) {
        if( container->replace_item( itemid, replacementid, "container of " + context ) ) {
            return true;
        }
    }
    return false;
}

Item_group::Item_group( Type t, int probability, int ammo_chance, int magazine_chance )
    : Item_spawn_data( probability )
    , type( t )
    , with_ammo( ammo_chance )
    , with_magazine( magazine_chance )
    , sum_prob( 0 )
{
    if( probability <= 0 || ( t != Type::G_DISTRIBUTION && probability > 100 ) ) {
        debugmsg( "Probability %d out of range", probability );
    }
    if( ammo_chance < 0 || ammo_chance > 100 ) {
        debugmsg( "Ammo chance %d is out of range.", ammo_chance );
    }
    if( magazine_chance < 0 || magazine_chance > 100 ) {
        debugmsg( "Magazine chance %d is out of range.", magazine_chance );
    }
}

void Item_group::load( const JsonObject &jo, const std::string &src )
{
    std::string subtype;

    optional( jo, was_loaded, "subtype", subtype, "distribution" );

    load( jo, src, subtype );
}

void Item_group::load( const JsonObject &jo, const std::string &src, const std::string &subtype )
{
    if( subtype == "distribution" ) {
        type = Item_group::G_DISTRIBUTION;
    } else {
        type = Item_group::G_COLLECTION;
    }

    optional( jo, was_loaded, "ammo", with_ammo, 0 );
    optional( jo, was_loaded, "magazine", with_magazine, 0 );

    // We can't use optional or such here due to the overall complexity
    if( jo.has_member( "entries" ) ) {
        for( const JsonObject subobj : jo.get_array( "entries" ) ) {
            add_entry( subobj );
        }
    }

    if( jo.has_member( "items" ) ) {
        for( const JsonValue entry : jo.get_array( "items" ) ) {
            if( entry.test_string() ) {
                // Strings: itype, 100 weight
                add_item_entry( itype_id( entry.get_string() ), 100 );
            } else if( entry.test_array() ) {
                // Arrays: 1st itype, 2cd weight, rest ignored
                JsonArray subitem = entry.get_array();
                add_item_entry( itype_id( subitem.get_string( 0 ) ), subitem.get_int( 1 ) );
            } else {
                // Fallback: Standard `entries` object
                add_entry( entry.get_object() );
            }
        }
    }

    if( jo.has_member( "groups" ) ) {
        for( const JsonValue entry : jo.get_array( "groups" ) ) {
            if( entry.test_string() ) {
                // Strings: item_group_id, 100 weight
                add_group_entry( item_group_id( entry.get_string() ), 100 );
            } else if( entry.test_array() ) {
                // Arrays: 1st item_group_id, 2cd weight, rest ignored
                JsonArray subitem = entry.get_array();
                add_group_entry( item_group_id( subitem.get_string( 0 ) ), subitem.get_int( 1 ) );
            } else {
                // Fallback: Standard `entries` object
                add_entry( entry.get_object() );
            }
        }
    }
    if( jo.has_member( "delete" ) ) {
        for( const JsonObject obj : jo.get_array( "delete" ) ) {
            if( obj.has_member( "item" ) ) {
                remove_specific_item( obj.get_string( "item" ) );
            } else if( obj.has_member( "group" ) ) {
                remove_specific_group( obj.get_string( "group" ) );
            }
        }
    }
}

void Item_group::add_entry( const JsonObject &obj )
{
    std::shared_ptr<Item_group> gptr;
    int probability = obj.get_int( "prob", 100 );
    JsonArray jarr;
    if( obj.has_member( "collection" ) ) {
        gptr = std::make_unique<Item_group>( Item_group::G_COLLECTION, probability, with_ammo,
                                             with_magazine );
        jarr = obj.get_array( "collection" );
    } else if( obj.has_member( "distribution" ) ) {
        gptr = std::make_unique<Item_group>( Item_group::G_COLLECTION, probability, with_ammo,
                                             with_magazine );
        jarr = obj.get_array( "distribution" );
    }
    if( gptr ) {
        for( const JsonObject job2 : jarr ) {
            gptr->add_entry( job2 );
        }
        add_entry( gptr );
        return;
    }

    std::shared_ptr<Single_item_creator> sptr;
    if( obj.has_member( "item" ) ) {
        sptr = std::make_shared<Single_item_creator>(
                   obj.get_string( "item" ), Single_item_creator::S_ITEM, probability );
    } else if( obj.has_member( "group" ) ) {
        sptr = std::make_shared<Single_item_creator>(
                   obj.get_string( "group" ), Single_item_creator::S_ITEM_GROUP, probability );
    }
    if( !sptr ) {
        return;
    }

    Item_modifier modifier;
    bool use_modifier = false;
    use_modifier |= optional( obj, false, "damage", modifier.damage, min_max_reader<int, int>() );
    use_modifier |= optional( obj, false, "dirt", modifier.dirt, min_max_reader<int, int>() );
    use_modifier |= optional( obj, false, "charges", modifier.charges, min_max_reader<int, int>() );
    use_modifier |= optional( obj, false, "count", modifier.count, min_max_reader<int, int>() );
    use_modifier |= optional( obj, false, "ammo", modifier.ammo, spawn_data_reader( false, with_ammo,
                              with_magazine ) );
    use_modifier |= optional( obj, false, "container", modifier.container, spawn_data_reader( false,
                              with_ammo, with_magazine ) );
    use_modifier |= optional( obj, false, "contents", modifier.contents, spawn_data_reader( true,
                              with_ammo,
                              with_magazine ) );
    use_modifier |= optional( obj, false, "custom-flags", modifier.custom_flags,
                              auto_flags_reader<flag_id>() );

    if( obj.has_bool( "active" ) && obj.get_bool( "active" ) ) {
        modifier.postprocess_fns.emplace_back( []( detached_ptr<item> &&it ) {
            it->activate();
            return std::move( it );
        } );
        use_modifier |= true;
    }

    if( use_modifier ) {
        sptr->modifier.emplace( std::move( modifier ) );
    }
    add_entry( std::move( sptr ) );
}

void Item_group::add_item_entry( const itype_id &itemid, int probability )
{
    add_entry( std::make_shared<Single_item_creator>(
                   itemid.str(), Single_item_creator::S_ITEM, probability ) );
}

void Item_group::add_group_entry( const item_group_id &groupid, int probability )
{
    add_entry( std::make_shared<Single_item_creator>(
                   groupid.str(), Single_item_creator::S_ITEM_GROUP, probability ) );
}

void Item_group::add_entry( std::shared_ptr<Item_spawn_data> ptr )
{
    assert( ptr.get() != nullptr );
    if( ptr->probability <= 0 ) {
        return;
    }
    if( type == G_COLLECTION ) {
        ptr->probability = std::min( 100, ptr->probability );
    }
    sum_prob += ptr->probability;

    // Make the ammo and magazine probabilities from the outer entity apply to the nested entity:
    // If ptr is an Item_group, it already inherited its parent's ammo/magazine chances in its constructor.
    Single_item_creator *sic = dynamic_cast<Single_item_creator *>( ptr.get() );
    if( sic ) {
        sic->inherit_ammo_mag_chances( with_ammo, with_magazine );
    }
    items.push_back( std::move( ptr ) );
}

std::vector<detached_ptr<item>> Item_group::create( const time_point &birthday,
                             RecursionList &rec ) const
{
    std::vector<detached_ptr<item>> result;
    if( type == G_COLLECTION ) {
        for( const auto &elem : items ) {
            if( rng( 0, 99 ) >= ( elem )->probability ) {
                continue;
            }
            std::vector<detached_ptr<item>> tmp = ( elem )->create( birthday, rec );
            result.insert( result.end(), std::make_move_iterator( tmp.begin() ),
                           std::make_move_iterator( tmp.end() ) );
        }
    } else if( type == G_DISTRIBUTION ) {
        int p = rng( 0, sum_prob - 1 );
        for( const auto &elem : items ) {
            p -= ( elem )->probability;
            if( p >= 0 ) {
                continue;
            }
            std::vector<detached_ptr<item>> tmp = ( elem )->create( birthday, rec );
            result.insert( result.end(), std::make_move_iterator( tmp.begin() ),
                           std::make_move_iterator( tmp.end() ) );
            break;
        }
    }

    return result;
}

detached_ptr<item> Item_group::create_single( const time_point &birthday, RecursionList &rec ) const
{
    if( type == G_COLLECTION ) {
        for( const auto &elem : items ) {
            if( rng( 0, 99 ) >= ( elem )->probability ) {
                continue;
            }
            return ( elem )->create_single( birthday, rec );
        }
    } else if( type == G_DISTRIBUTION ) {
        int p = rng( 0, sum_prob - 1 );
        for( const auto &elem : items ) {
            p -= ( elem )->probability;
            if( p >= 0 ) {
                continue;
            }
            return ( elem )->create_single( birthday, rec );
        }
    }
    return detached_ptr<item>();
}

bool Item_group::remove_item( const itype_id &itemid )
{
    for( prop_list::iterator a = items.begin(); a != items.end(); ) {
        if( ( *a )->remove_item( itemid ) ) {
            sum_prob -= ( *a )->probability;
            a = items.erase( a );
        } else {
            ++a;
        }
    }
    return items.empty();
}

bool Item_group::remove_specific_item( const std::string &itemid )
{
    for( prop_list::iterator a = items.begin(); a != items.end(); ) {
        auto b = dynamic_cast<Single_item_creator *>( ( &*a )->get() );
        if( b == nullptr ) {
            ++a;
        } else if( b->type == Single_item_creator::Type::S_ITEM ) {
            if( itemid == b->id ) {
                sum_prob -= b->probability;
                a = items.erase( a );
                return true;
            }
            ++a;
        } else {
            ++a;
        }
    }
    return items.empty();
}

bool Item_group::remove_specific_group( const std::string &itemid )
{
    for( prop_list::iterator a = items.begin(); a != items.end(); ) {
        auto b = dynamic_cast<Single_item_creator *>( ( &*a )->get() );
        if( b == nullptr ) {
            ++a;
        } else if( b->type == Single_item_creator::Type::S_ITEM_GROUP ) {
            if( itemid == b->id ) {
                sum_prob -= b->probability;
                a = items.erase( a );
                return true;
            }
            ++a;
        } else {
            ++a;
        }
    }
    return items.empty();
}

bool Item_group::replace_item( const itype_id &itemid, const itype_id &replacementid,
                               const std::string &context )
{
    for( const std::shared_ptr<Item_spawn_data> &elem : items ) {
        ( elem )->replace_item( itemid, replacementid, "item in " + context );
    }
    return items.empty();
}

bool Item_group::has_item( const itype_id &itemid ) const
{
    for( const std::shared_ptr<Item_spawn_data> &elem : items ) {
        if( ( elem )->has_item( itemid ) ) {
            return true;
        }
    }
    return false;
}

std::set<const itype *> Item_group::every_item() const
{
    std::set<const itype *> result;
    for( const auto &spawn_data : items ) {
        std::set<const itype *> these_items = spawn_data->every_item();
        result.insert( these_items.begin(), these_items.end() );
    }
    return result;
}

std::vector<detached_ptr<item>> Item_group::every_item_modified( bool /*modify*/ ) const
{
    std::vector<detached_ptr<item>> result;
    for( const auto &spawn_data : items ) {
        auto these_items = spawn_data->every_item_modified();
        result.reserve( result.size() + these_items.size() );
        for( auto &itm : these_items ) {
            result.push_back( std::move( itm ) );
        }
    }
    return result;
}

void item_group::insert_itemgroup( Item_group group )
{
    all_itemgroups.insert( group );
}

std::vector<detached_ptr<item>> item_group::items_from( const item_group_id &group_id,
                             const time_point &birthday )
{
    if( !group_id.is_valid() ) {
        return {};
    }
    std::vector<std::string> lst;
    return group_id->create( birthday, lst );
}

detached_ptr<item> item_group::item_from( const item_group_id &group_id,
        const time_point &birthday )
{
    if( !group_id.is_valid() ) {
        return detached_ptr<item>();
    }
    std::vector<std::string> lst;
    return group_id->create_single( birthday, lst );
}

bool item_group::group_contains_item( const item_group_id &group_id, const itype_id &type_id )
{
    if( !group_id.is_valid() ) {
        return false;
    }
    return group_id->has_item( type_id );
}

std::set<const itype *> item_group::every_possible_item_from( const item_group_id &group_id )
{
    if( !group_id.is_valid() ) {
        return {};
    }
    return group_id->every_item();
}

item_group_id item_group::get_unique_group_id()
{
    // This is just a hint what id to use next. Overflow of it is defined and if the group
    // name is already used, we simply go the next id.
    static unsigned int next_id = 0;
    // Prefixing with a character outside the ASCII range, so it is hopefully unique and
    // (if actually printed somewhere) stands out. Theoretically those auto-generated group
    // names should not be seen anywhere.
    static const std::string unique_prefix = "\u01F7 ";
    while( true ) {
        const item_group_id new_group = item_group_id( unique_prefix + std::to_string( next_id++ ) );
        if( !new_group.is_valid() ) {
            return new_group;
        }
    }
}

void Item_group::load_item_groups( const JsonObject &jo, const std::string &src )
{
    all_itemgroups.load( jo, src );
    // An empty dummy group, it will not spawn anything. However, it makes that item group
    // id valid, so it can be used all over the place without need to explicitly check for it.
    auto g = Item_group( Item_group::G_COLLECTION, 100, 0, 0 );
    g.id = item_group_id( "EMPTY_GROUP" );
    item_group::insert_itemgroup( g );
}

void Item_group::check_all()
{
    all_itemgroups.check();
}

void Item_group::check() const
{
    check_consistency( id.str() );
}

std::vector<Item_group> Item_group::get_all()
{
    return all_itemgroups.get_all();
}

void Item_group::reset()
{
    all_itemgroups.reset();
}

void Item_modifier::check_consistency( const std::string &context ) const
{
    if( ammo != nullptr ) {
        ammo->check_consistency( "ammo of " + context );
    }
    if( container != nullptr ) {
        container->check_consistency( "container of " + context );
    }
    if( contents != nullptr ) {
        contents->check_consistency( "contents of " + context );
    }
    if( with_ammo < 0 || with_ammo > 100 ) {
        debugmsg( "Item modifier's ammo chance %d is out of range", with_ammo );
    }
    if( with_magazine < 0 || with_magazine > 100 ) {
        debugmsg( "Item modifier's magazine chance %d is out of range", with_magazine );
    }
}

void Item_group::check_consistency( const std::string &context ) const
{
    for( const auto &elem : items ) {
        ( elem )->check_consistency( context );
    }
}

void Single_item_creator::check_consistency( const std::string &context ) const
{
    if( type == S_ITEM ) {
        if( !itype_id( id ).is_valid() ) {
            debugmsg( "item id %s is unknown (in %s)", id, context );
        }
    } else if( type == S_ITEM_GROUP ) {
        // TODO: figure out a way to check for itemgroup recursion here
        // Beyond the fact that your game wil ljust stall...
        if( !item_group_id( id ).is_valid() ) {
            debugmsg( "item group id %s is unknown (in %s)", id, context );
        }
    } else if( type == S_NONE ) {
        // this is okay, it will be ignored
    } else {
        debugmsg( "Unknown type of Single_item_creator: %d", static_cast<int>( type ) );
    }
    if( modifier ) {
        modifier->check_consistency( context );
        for( auto &item : every_item_modified( false ) ) {
            if( modifier && modifier->ammo != nullptr ) {
                for( auto &ammo : modifier->ammo->every_item_modified() ) {
                    if( item->is_tool() ) {
                        if( item->magazine_integral() ) {
                            if( !item->ammo_types().contains( ammo->ammo_type() ) &&
                                !item->ammo_types().contains( ammo->ammo_default()->ammo->type ) ) {
                                debugmsg( "Incompatible ammo: %s in ( %s %s )", ammo->type->get_id().str(),
                                          item->type->get_id().str(), context );
                            }
                        } else if( !item->ammo_types().empty() ) {
                            if( !item->ammo_types().contains( ammo->ammo_type() ) &&
                                !item->ammo_types().contains( ammo->ammo_default()->ammo->type ) ) {
                                debugmsg( "Incompatible ammo: %s in ( %s %s )", ammo->type->get_id().str(),
                                          item->type->get_id().str(), context );
                            }
                        } else if( item->ammo_default() != ammo->type->get_id() ) {
                            debugmsg( "Incompatible ammo: %s in ( %s %s )", ammo->type->get_id().str(),
                                      item->type->get_id().str(), context );
                        }
                    }
                    if( item->is_magazine() || item->is_gun() ) {
                        if( item->magazine_integral() ) {
                            if( !item->ammo_types().contains( ammo->ammo_type() ) ) {
                                debugmsg( "Incompatible ammo: %s in ( %s %s )", ammo->type->get_id().str(),
                                          item->type->get_id().str(), context );
                            }
                        } else if( !item->ammo_types().contains( ammo->ammo_type() ) ) {
                            debugmsg( "Incompatible ammo: %s in ( %s %s )", ammo->type->get_id().str(),
                                      item->type->get_id().str(), context );
                        }
                    }
                    // Exception as can_reload_with does not work for containers
                    // As it always returns true regardless of volume
                    else if( item->is_container() ) {
                        if( !item->is_reloadable_with( ammo->type->get_id() ) ) {
                            debugmsg( "Incompatible contained item: %s in ( %s %s )", ammo->type->get_id().str(),
                                      item->type->get_id().str(), context );
                        }
                        auto volume = ammo->volume();
                        if( ammo->count_by_charges() ) {
                            volume /= ammo->charges;
                        }
                        auto maxmult = std::max( modifier->charges.second, modifier->count.second );
                        // If autofill put to 1
                        if( maxmult == -1 ) {
                            maxmult = 1;
                        }
                        if( item->get_container_capacity() < volume * maxmult ) {
                            debugmsg( "Incompatible contained size: %s in ( %s %s )", ammo->type->get_id().str(),
                                      item->type->get_id().str(), context );
                        }
                    }
                }
            }
            if( modifier && modifier->container != nullptr ) {
                for( auto &container : modifier->container->every_item_modified() ) {
                    if( container->is_container() ) {
                        if( container->is_container() && !container->type->container->watertight &&
                            item->type->phase == LIQUID ) {
                            debugmsg( "Liquid %s in solid container ( %s %s )", item->type->get_id().str(),
                                      container->type->get_id().str(), context );
                        }
                        auto volume = item->volume();
                        if( item->count_by_charges() ) {
                            volume /= item->charges;
                        }
                        auto maxmult = std::max( modifier->charges.second, modifier->count.second );
                        // If autofill put to 1
                        if( maxmult == -1 ) {
                            maxmult = 1;
                        }
                        if( container->get_container_capacity() < volume * maxmult ) {
                            debugmsg( "Incompatible individual contents size: %s in ( %s %s )", item->type->get_id().str(),
                                      container->type->get_id().str(), context );
                        }
                    } else if( container->is_holster() && !container->can_holster( *item ) ) {
                        debugmsg( "Incorrect holstered object object: ( %s %s ) in %s", item->type->get_id().str(),
                                  context, container->type->get_id().str() );
                    } else if( container->is_bandolier() && !container->can_put_in_bandolier( *item ) ) {
                        debugmsg( "Incorrect bandoliered object object: ( %s %s ) in %s", item->type->get_id().str(),
                                  context, container->type->get_id().str() );
                    } else if( container->is_tool() ) {
                        if( container->magazine_integral() ) {
                            if( !container->ammo_types().contains( item->ammo_type() ) &&
                                !container->ammo_types().contains( item->ammo_default()->ammo->type ) ) {
                                debugmsg( "Incompatible ammo: %s in ( %s %s )", item->type->get_id().str(),
                                          container->type->get_id().str(), context );
                            }
                        } else if( !container->ammo_types().empty() ) {
                            if( !container->ammo_types().contains( item->ammo_type() ) &&
                                !container->ammo_types().contains( item->ammo_default()->ammo->type ) ) {
                                debugmsg( "Incompatible ammo: %s in ( %s %s )", item->type->get_id().str(),
                                          container->type->get_id().str(), context );
                            }
                        } else if( container->ammo_default() != item->type->get_id() ) {
                            debugmsg( "Incompatible ammo: %s in ( %s %s )", item->type->get_id().str(),
                                      container->type->get_id().str(), context );
                        }
                    }
                }
            }
            if( modifier && modifier->contents != nullptr ) {
                for( auto &contents : modifier->contents->every_item_modified() ) {
                    // Exception as can_reload_with does not work for containers
                    // As it always returns true regardless of volume
                    if( item->is_container() ) {
                        if( !item->is_reloadable_with( contents->type->get_id() ) ) {
                            // Okay if we cant contain it first try nesting it
                            // Then if still failing enter debugmsg
                            contents = item::in_its_container( std::move( contents ) );
                            if( !item->is_reloadable_with( contents->type->get_id() ) ) {
                                debugmsg( "Incompatible contained item: %s in ( %s %s )", contents->type->get_id().str(),
                                          item->type->get_id().str(), context );
                            }
                        }
                        auto volume = contents->volume();
                        if( contents->count_by_charges() ) {
                            volume /= contents->charges;
                        }
                        auto maxmult = std::max( modifier->charges.second, modifier->count.second );
                        // If autofill put to 1
                        if( maxmult == -1 ) {
                            maxmult = 1;
                        }
                        if( item->get_container_capacity() < volume * maxmult ) {
                            debugmsg( "Incompatible contained size: %s in ( %s %s )", contents->type->get_id().str(),
                                      item->type->get_id().str(), context );
                        }
                    } else if( item->is_holster() ) {
                        // Need to keep these if statements seperate for ordering purposes
                        if( !item->can_holster( *contents ) ) {
                            debugmsg( "Incorrect holstered object object: ( %s %s ) in %s", contents->type->get_id().str(),
                                      context, item->type->get_id().str() );
                        }
                    } else if( item->is_bandolier() ) {
                        if( !item->can_put_in_bandolier( *contents ) ) {
                            debugmsg( "Incorrect bandoliered object object: ( %s %s ) in %s", contents->type->get_id().str(),
                                      context, item->type->get_id().str() );
                        }
                    } else if( item->is_gun() ) {
                        if( !item->is_gunmod_compatible( *contents ).success() ) {
                            debugmsg( "Incompatible gunmod: %s in ( %s %s )", contents->type->get_id().str(),
                                      item->type->get_id().str(), context );
                        }
                    } else if( item->is_tool() ) {
                        // TODO: Make a unified check in item
                        // This is taken from iuse.cpp iuse::toolmod_attach
                        if( contents->is_toolmod() ) {
                            bool is_acceptable_ammo_mod = std::any_of( contents->type->mod->acceptable_ammo.begin(),
                            contents->type->mod->acceptable_ammo.end(), [&]( const ammotype & at ) {
                                return item->ammo_types( false ).count( at );
                            } );
                            if( ( contents->has_flag( flag_USE_UPS ) &&
                                  ( item->has_flag( flag_IS_UPS ) || item->has_flag( flag_USE_UPS ) ) ) ||
                                !item->toolmods().empty()  || !is_acceptable_ammo_mod ) {
                                debugmsg( "Incompatible toolmod: %s in ( %s %s )", contents->type->get_id().str(),
                                          item->type->get_id().str(), context );
                            }
                            if( item->magazine_current() ) {
                                debugmsg( "Tried to put a toolmod in a magazine containing item %s in ( %s %s )",
                                          contents->type->get_id().str(),
                                          item->type->get_id().str(), context );
                            }
                        } else {
                            debugmsg( "Tried to put %s in ( %s %s ) as a toolmod while it wasn't",
                                      contents->type->get_id().str(),
                                      item->type->get_id().str(), context );
                        }
                    }
                }
            }
        }
    }
}
