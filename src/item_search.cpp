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
#include "string_utils.h"

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

std::function<bool( const item & )> item_filter_from_string( const std::string &filter )
{
    return filter_from_string<item>( filter, basic_item_filter );
}

std::function<bool( const itype & )> itype_filter_from_string( const std::string &filter )
{
    return filter_from_string<itype>( filter, basic_itype_filter );
}

std::pair<std::string, std::string> get_both( const std::string &a )
{
    size_t split_mark = a.find( ';' );
    return std::make_pair( a.substr( 0, split_mark - 1 ),
                           a.substr( split_mark + 1 ) );
}

std::function<bool( const item & )> wildcard_item_filter( std::string filter )
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
                return wildcard_match( i.get_category().name(), filter );
            };
        // material
        case 'm':
            return [filter]( const item & i ) {
                return std::any_of( i.made_of().begin(), i.made_of().end(),
                [&filter]( const material_id & mat ) {
                    return wildcard_match( mat->name(), filter );
                } );
            };
        // qualities
        case 'q':
            return [filter]( const item & i ) {
                return std::any_of( i.quality_of().begin(), i.quality_of().end(),
                [&filter]( const std::pair<quality_id, int> &e ) {
                    return wildcard_match( e.first->name.translated(), filter );
                } );
            };
        // both
        case 'b':
            return [filter]( const item & i ) {
                auto pair = get_both( filter );
                auto func1 = filter_from_string<item>( pair.first, wildcard_item_filter );
                auto func2 = filter_from_string<item>( pair.second, wildcard_item_filter );

                return func1( i ) && func2( i );
            };
        // disassembled components
        case 'd':
            return [filter]( const item & i ) {
                const auto &components = i.get_uncraft_components();
                for( auto &component : components ) {
                    if( wildcard_match( component.to_string(), filter ) ) {
                        return true;
                    }
                }
                return false;
            };
        // item notes
        case 'n':
            return [filter]( const item & i ) {
                const std::string note = i.get_var( "item_note" );
                return !note.empty() && wildcard_match( note, filter );
            };
        // skill taught
        case 'k':
            return [filter]( const item & i ) {
                if( i.is_book() ) {
                    const islot_book &book = *i.type->book;
                    return wildcard_match( book.skill->name(), filter );
                }
                return false;
            };
        // by name
        default:
            return [filter]( const item & a ) {
                return wildcard_match( a.tname(), filter );
            };
    }
}

std::function<bool( const itype & )> wildcard_itype_filter( std::string filter )
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
                return wildcard_match( i.category_force->name(), filter );
            };
        // material
        case 'm':
            return [filter]( const itype & i ) {
                return std::any_of( i.materials.begin(), i.materials.end(),
                [&filter]( const material_id & mat ) {
                    return wildcard_match( mat->name(), filter );
                } );
            };
        case 'M':
            return [filter]( const itype & i ) {
                bool pure_material = true;
                for( auto &mat : i.materials ) {
                    if( !wildcard_match( mat->name(), filter ) ) {
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
                    return wildcard_match( e.first->name.translated(), filter );
                } );
            };
        // both
        case 'b':
            return [filter]( const itype & i ) {
                auto pair = get_both( filter );
                auto func1 = filter_from_string<itype>( pair.first, wildcard_itype_filter );
                auto func2 = filter_from_string<itype>( pair.second, wildcard_itype_filter );

                return func1( i ) && func2( i );
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
                    return wildcard_match( book.skill->name(), filter );
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