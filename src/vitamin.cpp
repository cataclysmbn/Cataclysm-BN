#include "vitamin.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>

#include "calendar.h"
#include "debug.h"
#include "effect.h"
#include "enum_conversions.h"
#include "generic_factory.h"
#include "json.h"
#include "string_id.h"
#include "type_id.h"
#include "units.h"
#include "units_serde.h"

namespace
{
generic_factory<vitamin> all_vitamins( "Vitamins" );
}

/** @relates string_id */
template<>
bool string_id<vitamin>::is_valid() const
{
    return all_vitamins.is_valid( *this );
}

/** @relates string_id */
template<>
const vitamin &string_id<vitamin>::obj() const
{
    return all_vitamins.obj( *this );
}

/** @relates string_id */
template<>
int_id<vitamin> string_id<vitamin>::id() const
{
    return all_vitamins.convert( *this, int_id<vitamin>( INVALID_CID ) );
}

void vitamin::load_vitamin( const JsonObject &jo, const std::string &src )
{
    // TODO: Make this default maybe?
    // This is here because otherwise game.cpp will segfault on start when trying to get
    // Values from the unprepared generic factories
    all_vitamins.load( jo, src );
}

void vitamin::load( const JsonObject &jo, const std::string &src )
{
    const bool strict = is_strict_enabled( src );
    mandatory( jo, was_loaded, "name", name_ );
    mandatory( jo, was_loaded, "vit_type", type_ );
    mandatory( jo, was_loaded, "rate", rate_ );
    deficiency_ = jo.has_string( "deficiency" ) ? efftype_id( jo.get_string( "deficiency" ) ) :
                  efftype_id::NULL_ID();
    excess_ = jo.has_string( "excess" ) ? efftype_id( jo.get_string( "excess" ) ) :
              efftype_id::NULL_ID();
    mandatory( jo, was_loaded, "min", min_ );
    assign( jo, "max", max_, strict, 0 );


    for( JsonArray e : jo.get_array( "disease" ) ) {
        disease_.emplace_back( e.get_int( 0 ), e.get_int( 1 ) );
    }

    for( JsonArray e : jo.get_array( "disease_excess" ) ) {
        disease_excess_.emplace_back( e.get_int( 0 ), e.get_int( 1 ) );
    }

    for( std::string e : jo.get_array( "flags" ) ) {
        flags_.insert( e );
    }
}

const std::vector<vitamin> &vitamin::all()
{
    return all_vitamins.get_all();
}

void vitamin::check_consistency()
{
    all_vitamins.check();
}

void vitamin::check() const
{
    if( rate_ < 0_turns ) {
        debugmsg( "vitamin %s has negative consumption rate", id.c_str() );
    }
    if( !( deficiency_.is_null() || deficiency_.is_valid() ) ) {
        debugmsg( "vitamin %s has unknown deficiency %s", id.c_str(),
                  deficiency_.c_str() );
    } else if( !deficiency_.is_null() && !deficiency_->is_permanent() ) {
        debugmsg( "vitamin %s has non-permanent deficiency effect %s", id.c_str(),
                  deficiency_.c_str() );
    }
    if( !( excess_.is_null() || excess_.is_valid() ) ) {
        debugmsg( "vitamin %s has unknown excess %s", id.c_str(), excess_.c_str() );
    } else if( !excess_.is_null() && !excess_->is_permanent() ) {
        debugmsg( "vitamin %s has non-permanent excess effect %s", id.c_str(),
                  excess_.c_str() );
    }
}

void vitamin::reset()
{
    all_vitamins.reset();
}

int vitamin::severity( int qty ) const
{
    for( int i = 0; i != static_cast<int>( disease_.size() ); ++i ) {
        if( ( qty >= disease_[ i ].first && qty <= disease_[ i ].second ) ||
            ( qty <= disease_[ i ].first && qty >= disease_[ i ].second ) ) {
            return i + 1;
        }
    }
    for( int i = 0; i != static_cast<int>( disease_excess_.size() ); ++i ) {
        if( ( qty >= disease_excess_[ i ].first && qty <= disease_excess_[ i ].second ) ||
            ( qty <= disease_excess_[ i ].first && qty >= disease_excess_[ i ].second ) ) {
            return -i - 1;
        }
    }
    return 0;
}


namespace io
{
template<>
std::string enum_to_string<vitamin_type>( vitamin_type data )
{
    switch( data ) {
        case vitamin_type::VITAMIN:
            return "vitamin";
        case vitamin_type::TOXIN:
            return "toxin";
        case vitamin_type::DRUG:
            return "drug";
        case vitamin_type::COUNTER:
            return "counter";
        case vitamin_type::num_vitamin_types:
            break;
    }
    debugmsg( "Invalid vitamin_type" );
    abort();
}
} // namespace io
