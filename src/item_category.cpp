#include "item_category.h"

#include <algorithm>
#include <set>

#include "generic_factory.h"
#include "item.h"
#include "json.h"
#include "string_id.h"
#include "type_id_implement.h"

namespace
{
generic_factory<item_category> item_category_factory( "item_category" );
} // namespace

IMPLEMENT_STRING_AND_INT_IDS( item_category, item_category_factory );

void zone_priority_data::deserialize( JsonIn &jsin )
{
    JsonObject data = jsin.get_object();
    load( data );
}

void zone_priority_data::load( JsonObject &jo )
{
    mandatory( jo, was_loaded, "id", id );
    optional( jo, was_loaded, "flags", flags );
}

void item_category::load_item_cat( const JsonObject &jo, const std::string &src )
{
    item_category_factory.load( jo, src );
}

void item_category::load( const JsonObject &jo, const std::string & )
{
    mandatory( jo, was_loaded, "id", id );
    mandatory( jo, was_loaded, "name", name_ );
    mandatory( jo, was_loaded, "sort_rank", sort_rank_ );
    optional( jo, was_loaded, "priority_zones", zone_priority_ );
    optional( jo, was_loaded, "zone", zone_, std::nullopt );
}

bool item_category::operator<( const item_category &rhs ) const
{
    if( sort_rank_ != rhs.sort_rank_ ) {
        return sort_rank_ < rhs.sort_rank_;
    }
    if( name_.translated_ne( rhs.name_ ) ) {
        return name_.translated_lt( rhs.name_ );
    }
    return id < rhs.id;
}

bool item_category::operator==( const item_category &rhs ) const
{
    return sort_rank_ == rhs.sort_rank_ && name_.translated_eq( rhs.name_ ) && id == rhs.id;
}

bool item_category::operator!=( const item_category &rhs ) const
{
    return !operator==( rhs );
}

std::string item_category::name() const
{
    return name_.translated();
}

item_category_id item_category::get_id() const
{
    return id;
}

std::optional<zone_type_id> item_category::zone() const
{
    return zone_;
}

std::optional<zone_type_id> item_category::priority_zone( const item &it ) const
{
    for( const zone_priority_data &zone_dat : zone_priority_ ) {
        if( it.has_any_flag( zone_dat.flags ) ) {
            return zone_dat.id;
        }
    }
    return std::nullopt;
}

int item_category::sort_rank() const
{
    return sort_rank_;
}

auto item_category::zone_types() -> std::vector<zone_type_id>
{
    auto result = std::vector<zone_type_id>();
    for( const auto &category : item_category_factory.get_all() ) {
        if( const auto id = category.zone(); id && !std::ranges::contains( result, *id ) ) {
            result.push_back( *id );
        }
        for( const auto &priority : category.zone_priority_ ) {
            if( !std::ranges::contains( result, priority.id ) ) {
                result.push_back( priority.id );
            }
        }
    }
    return result;
}
