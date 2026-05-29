#pragma once

#include <cstddef>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "string_utils.h"
#include "itype.h"

/**
 * A single entry in the list of auto pickup entries @ref rule_list.
 * The data contained can be edited by the player and determines what to pick/ignore.
 */
struct rule {
    std::string sRule;
    std::function<bool( const itype_id & )> ruleFunction = []( const itype_id &item ) { return false; };
    bool bActive = false;
    bool bExclude = false;

    rule() = default;
    rule( const std::string &rule_str, const bool active = true, const bool exclude = false );

    void serialize( JsonOut &jsout ) const;
    void deserialize( JsonIn &jsin );

    void set_rule_string( const std::string &filter );

    rule_state operator()( const itype_id &item_type ) const;
    rule_state operator()( const itype &item_type ) const;
    rule_state operator()( const item &item ) const;
};

/**
 * A list of rules. This is primarily a container with a few convenient functions (like saving/loading).
 */
struct rule_list : public std::vector<rule> {
    void serialize( JsonOut &jsout ) const;
    void deserialize( JsonIn &jsin );

    rule_state operator()( const itype_id &item_type ) const;
    rule_state operator()( const itype &item_type ) const;
    rule_state operator()( const item &item ) const;
};


/**
 * The currently-active set of rules, in a form that allows quick
 * lookup. After being filled up by apply_rules() allows constant
 * time access to the rule_state of the item
 */
struct item_search_cache : public std::unordered_map<itype_id, rule_state> {
    //Edits the cache according to the rules of curr_rules
    item_search_cache();
    void clear_items();
    void apply_rules( const rule_list &curr_rules );
    void apply_rules( const rule &curr_rules );
};
/**
 * Get a function that returns true if the value matches the query.
 */
template<typename T>
std::function<bool( const T & )> filter_from_string( std::string filter,
        std::function<std::function<bool( const T & )>( const std::string & )> basic_filter )
{
    if( filter.empty() ) {
        // Variable without name prevents unused parameter warning
        return []( const T & ) {
            return true;
        };
    }

    // remove curly braces (they only get in the way)
    if( filter.find( '{' ) != std::string::npos ) {
        filter.erase( std::remove( filter.begin(), filter.end(), '{' ), filter.end() );
    }
    if( filter.find( '}' ) != std::string::npos ) {
        filter.erase( std::remove( filter.begin(), filter.end(), '}' ), filter.end() );
    }
    if( filter.find( ',' ) != std::string::npos ) {
        // functions which only one of which must return true
        std::vector<std::function<bool( const T & )> > functions;
        // Functions that must all return true
        std::vector<std::function<bool( const T & )> > inv_functions;
        size_t comma = filter.find( ',' );
        while( !filter.empty() ) {
            const auto &current_filter = trim( filter.substr( 0, comma ) );
            if( !current_filter.empty() ) {
                auto current_func = filter_from_string( current_filter, basic_filter );
                if( current_filter[0] == '-' ) {
                    inv_functions.push_back( current_func );
                } else {
                    functions.push_back( current_func );
                }
            }
            if( comma != std::string::npos ) {
                filter = trim( filter.substr( comma + 1 ) );
                comma = filter.find( ',' );
            } else {
                break;
            }
        }

        return [functions, inv_functions]( const T & it ) {
            auto apply = [&]( const std::function<bool( const T & )> &func ) {
                return func( it );
            };
            const bool p_result = std::any_of( functions.begin(), functions.end(),
                                               apply );
            const bool n_result = std::all_of(
                                      inv_functions.begin(),
                                      inv_functions.end(),
                                      apply );
            if( !functions.empty() && inv_functions.empty() ) {
                return p_result;
            }
            if( functions.empty() && !inv_functions.empty() ) {
                return n_result;
            }
            return p_result && n_result;
        };
    }
    const bool exclude = filter[0] == '-';
    if( exclude ) {
        return [filter, basic_filter]( const T & i ) {
            return !filter_from_string( filter.substr( 1 ), basic_filter )( i );
        };
    }

    return basic_filter( filter );
}

class item;

/**
 * Get a function that returns true if the item matches the query.
 */
std::function<bool( const item & )> item_filter_from_string( const std::string &filter );
std::function<bool( const itype & )> itype_filter_from_string( const std::string &filter );
std::function<bool( const itype_id & )> itype_id_filter_from_string( const std::string &filter );

//Used for autopickup
std::function<bool( const itype & )> itype_filter_from_string_wildcard( const std::string &filter );
std::function<bool( const itype_id & )> itype_id_filter_from_string_wildcard(
    const std::string &filter );


/**
 * Get a function that returns true if the value matches the basic query (no commas or minuses).
 */
std::function<bool( const item & )> basic_item_filter( std::string filter );
std::function<bool( const itype & )> basic_itype_filter( std::string filter );
//This is the most efficient one
std::function<bool( const itype_id & )> basic_itype_id_filter( const std::string &filter );

//Used for autopickup with wildcards *
std::function<bool( const itype & )> basic_itype_filter_wildcard( std::string filter );
std::function<bool( const itype_id & )> basic_itype_id_filter_wildcard( std::string filter );

