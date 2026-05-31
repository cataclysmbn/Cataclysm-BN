#include "item_search.h"

#include <map>
#include <utility>

#include "cata_utility.h"
#include "item.h"
#include "item_category.h"
#include "item_factory.h"
#include "material.h"
#include "requirements.h"
#include "skill.h"
#include "string_id.h"
#include "type_id.h"

rule::rule( const std::string &rule_str, bool active, bool exclude ) :
    sRule( rule_str ), bActive( active ), bExclude( exclude )
{
    ruleFunction = itype_id_filter_from_string_wildcard( rule_str );
}

void rule::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "rule", sRule );
    jsout.member( "active", bActive );
    jsout.member( "exclude", bExclude );
    jsout.end_object();
}
void rule::deserialize( JsonIn &jsin )
{
    JsonObject jo = jsin.get_object();
    set_rule_string( jo.get_string( "rule" ) );
    bActive = jo.get_bool( "active" );
    bExclude = jo.get_bool( "exclude" );
}

void rule::set_rule_string( const std::string &filter )
{
    sRule = filter;
    ruleFunction = itype_id_filter_from_string_wildcard( filter );
}

rule_state rule::operator()( const itype_id &item_type ) const
{
    if( bActive ) {
        bool in_filter = ruleFunction( item_type );
        if( !in_filter ) {
            return RULE_NONE;
        } else {
            if( bExclude ) {
                return RULE_BLACKLISTED;
            } else {
                return RULE_WHITELISTED;
            }
        }
    } else {
        return RULE_NONE;
    }
}

rule_state rule::operator()( const itype &item_type ) const
{
    return ( *this )( item_type.get_id() );
}

rule_state rule::operator()( const item &item ) const
{
    return ( *this )( item.typeId() );
}

void rule_list::serialize( JsonOut &jsout ) const
{
    jsout.start_array();
    for( const rule &elem : *this ) {
        elem.serialize( jsout );
    }
    jsout.end_array();
}

void rule_list::deserialize( JsonIn &jsin )
{
    clear();

    jsin.start_array();
    while( !jsin.end_array() ) {
        rule tmp;
        tmp.deserialize( jsin );
        push_back( tmp );
    }
}


rule_state rule_list::operator()( const itype_id &item_type ) const
{
    rule_state curr_state = RULE_NONE;
    for( size_t i = 0; i < size(); ++i ) {
        if( ( *this )[i].bActive ) {
            if( !( *this )[i].bExclude && ( *this )[i]( item_type ) ) {
                curr_state = RULE_WHITELISTED;
            } else if( ( *this )[i].bExclude && ( *this )[i]( item_type ) ) {
                curr_state = RULE_BLACKLISTED;
            }
        }
    }

    return curr_state;
}

rule_state rule_list::operator()( const itype &item_type ) const
{
    return ( *this )( item_type.get_id() );

}
rule_state rule_list::operator()( const item &item ) const
{
    return ( *this )( item.typeId() );
}

item_search_cache::item_search_cache()
{
    clear_items();
}


void item_search_cache::apply_rules( const rule_list &curr_rules )
{
    for( size_t i = 0; i < curr_rules.size(); ++i ) {
        apply_rules( curr_rules[i] );
    }
}

void item_search_cache::apply_rules( const rule &curr_rules )
{
    for( const itype *e : item_controller->all() ) {
        const itype_id &id = e->get_id();
        rule_state result = curr_rules( *e );
        if( result != RULE_NONE ) {
            ( *this )[id] = result;
        }
    }
}

void item_search_cache::clear_items()
{
    for( const itype *e : item_controller->all() ) {
        const itype_id &id = e->get_id();
        ( *this )[id] = RULE_BLACKLISTED;
    }
}


std::pair<std::string, std::string> get_both( const std::string &a );

std::function<bool( const item & )> basic_item_filter( std::string filter )
{
    size_t colon;
    char flag = '\0';
    if( ( colon = filter.find( ':' ) ) != std::string::npos ) {
        if( colon >= 1 ) {
            flag = filter[colon - 1];
            filter = filter.substr( colon + 1 );
        }
    }
    switch( flag ) {
        // category
        case 'c':
            return [filter]( const item & i ) {
                return lcmatch( i.get_category().name(), filter );
            };
        // material
        case 'm':
            return [filter]( const item & i ) {
                return std::any_of( i.made_of().begin(), i.made_of().end(),
                [&filter]( const material_id & mat ) {
                    return lcmatch( mat->name(), filter );
                } );
            };
        case 'M':
            return [filter]( const item & i ) {
                bool pure_material = true;
                for( auto &mat : i.made_of() ) {
                    if( !lcmatch( mat->name(), filter ) ) {
                        pure_material = false;
                    }
                }
                return pure_material;
            };
        // qualities
        case 'q':
            return [filter]( const item & i ) {
                return std::any_of( i.quality_of().begin(), i.quality_of().end(),
                [&filter]( const std::pair<quality_id, int> &e ) {
                    return lcmatch( e.first->name, filter );
                } );
            };
        // both
        case 'b':
            return [filter]( const item & i ) {
                auto pair = get_both( filter );
                return item_filter_from_string( pair.first )( i )
                       && item_filter_from_string( pair.second )( i );
            };
        // disassembled components
        case 'd':
            return [filter]( const item & i ) {
                const auto &components = i.get_uncraft_components();
                for( auto &component : components ) {
                    if( lcmatch( component.to_string(), filter ) ) {
                        return true;
                    }
                }
                return false;
            };
        // item notes
        case 'n':
            return [filter]( const item & i ) {
                const std::string note = i.get_var( "item_note" );
                return !note.empty() && lcmatch( note, filter );
            };
        // skill taught
        case 'k':
            return [filter]( const item & i ) {
                if( i.is_book() ) {
                    const islot_book &book = *i.type->book;
                    return lcmatch( book.skill->name(), filter );
                }
                return false;
            };
        // by name
        default:
            return [filter]( const item & a ) {
                return lcmatch( a.tname(), filter );
            };
    }
}

std::function<bool( const itype & )> basic_itype_filter( std::string filter )
{
    size_t colon;
    char flag = '\0';
    if( ( colon = filter.find( ':' ) ) != std::string::npos ) {
        if( colon >= 1 ) {
            flag = filter[colon - 1];
            filter = filter.substr( colon + 1 );
        }
    }
    switch( flag ) {
        // category
        case 'c':
            return [filter]( const itype & i ) {
                return lcmatch( i.category_force->name(), filter );
            };
        // material
        case 'm':
            return [filter]( const itype & i ) {
                return std::any_of( i.materials.begin(), i.materials.end(),
                [&filter]( const material_id & mat ) {
                    return lcmatch( mat->name(), filter );
                } );
            };
        case 'M':
            return [filter]( const itype & i ) {
                bool pure_material = true;
                for( auto &mat : i.materials ) {
                    if( !lcmatch( mat->name(), filter ) ) {
                        pure_material = false;
                    }
                }
                return pure_material;
            };
        // qualities
        case 'q':
            return [filter]( const itype & i ) {
                return std::any_of( i.qualities.begin(), i.qualities.end(),
                [&filter]( const std::pair<quality_id, int> &e ) {
                    return lcmatch( e.first->name, filter );
                } );
            };
        // both
        case 'b':
            return [filter]( const itype & i ) {
                auto pair = get_both( filter );
                return itype_filter_from_string( pair.first )( i )
                       && itype_filter_from_string( pair.second )( i );
            };
        // disassembled components
        // TODO: Move dissambled components into itype so we can implement this
        // case 'd':
        //     return [filter]( const item & i ) {
        //         const auto &components = i.get_uncraft_components();
        //         for( auto &component : components ) {
        //             if( lcmatch( component.to_string(), filter ) ) {
        //                 return true;
        //             }
        //         }
        //         return false;
        //     };
        // skill taught
        case 'k':
            return [filter]( const itype & i ) {
                if( i.book != NULL ) {
                    const islot_book &book = *i.book;
                    return lcmatch( book.skill->name(), filter );
                }
                return false;
            };
        // by name
        default:
            return [filter]( const itype & a ) {
                return lcmatch( a.nname( 1 ), filter );
            };
    }
}

std::function<bool( const itype_id & )> basic_itype_id_filter( const std::string &filter )
{
    return [filter]( const itype_id & id ) {
        return basic_itype_filter( filter )( id.obj() );
    };
}


std::function<bool( const itype & )> basic_itype_filter_wildcard( std::string filter )
{
    size_t colon;
    char flag = '\0';
    if( ( colon = filter.find( ':' ) ) != std::string::npos ) {
        if( colon >= 1 ) {
            flag = filter[colon - 1];
            filter = filter.substr( colon + 1 );
        }
    }
    switch( flag ) {
        // category
        case 'c':
            return [filter]( const itype & i ) {
                return lcmatch( i.category_force->name(), filter );
            };
        // material
        case 'm':
            return [filter]( const itype & i ) {
                return std::any_of( i.materials.begin(), i.materials.end(),
                [&filter]( const material_id & mat ) {
                    return lcmatch( mat->name(), filter );
                } );
            };
        // Only of 1 material
        case 'M':
            return [filter]( const itype & i ) {
                bool pure_material = true;
                for( auto &mat : i.materials ) {
                    if( !lcmatch( mat->name(), filter ) ) {
                        pure_material = false;
                    }
                }
                return pure_material;
            };
        // qualities
        case 'q':
            return [filter]( const itype & i ) {
                return std::any_of( i.qualities.begin(), i.qualities.end(),
                [&filter]( const std::pair<quality_id, int> &e ) {
                    return lcmatch( e.first->name, filter );
                } );
            };
        // both
        case 'b':
            return [filter]( const itype & i ) {
                auto pair = get_both( filter );
                return itype_filter_from_string_wildcard( pair.first )( i )
                       && itype_filter_from_string_wildcard( pair.second )( i );
            };
        // disassembled components
        // TODO: Move dissambled components into itype so we can implement this
        // case 'd':
        //     return [filter]( const item & i ) {
        //         const auto &components = i.get_uncraft_components();
        //         for( auto &component : components ) {
        //             if( lcmatch( component.to_string(), filter ) ) {
        //                 return true;
        //             }
        //         }
        //         return false;
        //     };
        // skill taught
        case 'k':
            return [filter]( const itype & i ) {
                if( i.book != NULL ) {
                    const islot_book &book = *i.book;
                    return lcmatch( book.skill->name(), filter );
                }
                return false;
            };
        // by name
        default:
            return [filter]( const itype & a ) {
                return wildcard_match( a.nname( 1 ), filter );
            };
    }
}

std::function<bool( const itype_id & )> basic_itype_id_filter_wildcard( std::string filter )
{
    return [filter]( const itype_id & id ) {
        return basic_itype_filter_wildcard( filter )( id.obj() );
    };
}

std::function<bool( const item & )> item_filter_from_string( const std::string &filter )
{
    return filter_from_string<item>( filter, basic_item_filter );
}

std::function<bool( const itype & )> itype_filter_from_string( const std::string &filter )
{
    return filter_from_string<itype>( filter, basic_itype_filter );
}
std::function<bool( const itype_id & )> itype_id_filter_from_string( const std::string &filter )
{
    return filter_from_string<itype_id>( filter, basic_itype_id_filter );
}

std::function<bool( const itype & )> itype_filter_from_string_wildcard( const std::string &filter )
{
    return filter_from_string<itype>( filter, basic_itype_filter_wildcard );
}

std::function<bool( const itype_id & )> itype_id_filter_from_string_wildcard(
    const std::string &filter )
{
    return filter_from_string<itype_id>( filter, basic_itype_id_filter_wildcard );
}
std::pair<std::string, std::string> get_both( const std::string &a )
{
    size_t split_mark = a.find( ';' );
    return std::make_pair( a.substr( 0, split_mark - 1 ),
                           a.substr( split_mark + 1 ) );
}
