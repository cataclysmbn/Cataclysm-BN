#include "dependency_tree.h"

#include <algorithm>
#include <array>
#include <ostream>
#include <set>
#include <utility>

#include "output.h"
#include "string_id.h"

std::array<std::string, static_cast<int>( node_error_type::num )> error_keyvals = {{
        translate_marker( "Missing Dependency(ies): " ),
        translate_marker( "Has conflicting dependencies: " ),
        translate_marker( "Has dependency cycle(s): " )
    }
};

// dependency_node
dependency_node::dependency_node(): index( -1 ), lowlink( -1 ), on_stack( false )
{
    availability = true;
}

dependency_node::dependency_node( mod_id _key ): index( -1 ), lowlink( -1 ), on_stack( false )
{
    key = _key;
    availability = true;
}

void dependency_node::add_parent( dependency_node *parent )
{
    parents.push_back( parent );
}

void dependency_node::add_child( dependency_node *child )
{
    children.push_back( child );
}

void dependency_node::add_conflict( const dependency_node *conflict )
{
    auto it = std::ranges::find( conflicts, conflict );
    if( it == conflicts.end() ) {
        conflicts.push_back( conflict );
    }
}

bool dependency_node::is_available() const
{
    return all_errors.empty();
}

std::map<node_error_type, std::vector<std::string > > dependency_node::errors()
{
    return all_errors;
}

std::string dependency_node::s_errors()
{
    std::string ret;
    for( auto &elem : all_errors ) {
        if( !ret.empty() ) {
            ret += "\n";
        }
        ret += _( error_keyvals[static_cast<size_t>( elem.first )] );
        ret += enumerate_as_string( elem.second, enumeration_conjunction::none );
    }
    return ret;
}

bool dependency_node::has_errors() const
{
    bool ret = false;
    for( const auto &elem : all_errors ) {
        if( !elem.second.empty() ) {
            ret = true;
            break;
        }
    }
    return ret;
}

void dependency_node::inherit_errors()
{
    std::stack<dependency_node * > nodes_to_check;
    std::set<mod_id> nodes_visited;

    for( auto &elem : parents ) {
        nodes_to_check.push( elem );
    }
    nodes_visited.insert( key );

    while( !nodes_to_check.empty() ) {
        dependency_node *check = nodes_to_check.top();
        nodes_to_check.pop();

        // add check errors
        if( !check->errors().empty() ) {
            std::map<node_error_type, std::vector<std::string > > cerrors = check->errors();
            for( auto &cerror : cerrors ) {
                std::vector<std::string> node_errors = cerror.second;
                node_error_type error_type = cerror.first;
                std::vector<std::string> cur_errors = all_errors[error_type];
                for( auto &node_error : node_errors ) {
                    if( std::ranges::find( cur_errors, node_error ) ==
                        cur_errors.end() ) {
                        all_errors[cerror.first].push_back( node_error );
                    }
                }
            }
        }
        if( nodes_visited.find( check->key ) != nodes_visited.end() ) {
            continue;
        }
        // add check parents, if exist, to stack
        if( !check->parents.empty() ) {
            for( auto &elem : check->parents ) {
                nodes_to_check.push( elem );
            }
        }
        nodes_visited.insert( check->key );
    }
}

std::vector<mod_id> dependency_node::get_dependencies_as_strings() const
{
    std::vector<mod_id> ret;

    std::vector<dependency_node *> as_nodes = get_dependencies_as_nodes();

    ret.reserve( as_nodes.size() );

    for( auto &as_node : as_nodes ) {
        ret.push_back( ( as_node )->key );
    }

    // returns dependencies in a guaranteed valid order
    return ret;
}

std::vector<dependency_node *> dependency_node::get_dependencies_as_nodes() const
{
    std::vector<dependency_node *> dependencies;
    std::vector<dependency_node *> ret;
    std::set<mod_id> found;

    std::stack<dependency_node *> nodes_to_check;
    for( auto &elem : parents ) {
        nodes_to_check.push( elem );
    }
    found.insert( key );

    while( !nodes_to_check.empty() ) {
        dependency_node *check = nodes_to_check.top();
        nodes_to_check.pop();

        // make sure that the one we are checking is not THIS one
        if( found.find( check->key ) != found.end() ) {
            continue; // just keep going, we aren't really caring about availability right now
        }

        // add check to dependencies
        dependencies.push_back( check );

        // add parents to check list
        if( !check->parents.empty() ) {
            for( auto &elem : check->parents ) {
                nodes_to_check.push( elem );
            }
        }
        found.insert( check->key );
    }

    // sort from the back!
    for( std::vector<dependency_node *>::reverse_iterator it =
             dependencies.rbegin();
         it != dependencies.rend(); ++it ) {
        if( std::ranges::find( ret, *it ) == ret.end() ) {
            ret.push_back( *it );
        }
    }

    return ret;
}

std::vector<mod_id> dependency_node::get_dependents_as_strings() const
{
    std::vector<mod_id> ret;

    std::vector<dependency_node *> as_nodes = get_dependents_as_nodes();

    ret.reserve( as_nodes.size() );

    for( auto &as_node : as_nodes ) {
        ret.push_back( ( as_node )->key );
    }

    // returns dependencies in a guaranteed valid order
    return ret;
}

std::vector<dependency_node *> dependency_node::get_dependents_as_nodes() const
{
    std::vector<dependency_node *> dependents;
    std::vector<dependency_node *> ret;
    std::set<mod_id> found;

    std::stack<dependency_node *> nodes_to_check;
    for( auto &elem : children ) {
        nodes_to_check.push( elem );
    }
    found.insert( key );

    while( !nodes_to_check.empty() ) {
        dependency_node *check = nodes_to_check.top();
        nodes_to_check.pop();

        if( found.find( check->key ) != found.end() ) {
            // skip it because we recursed for some reason
            continue;
        }
        dependents.push_back( check );

        if( !check->children.empty() ) {
            for( auto &elem : check->children ) {
                nodes_to_check.push( elem );
            }
        }
        found.insert( check->key );
    }

    // sort from front, keeping only one copy of the node
    for( auto &dependent : dependents ) {
        if( std::ranges::find( ret, dependent ) == ret.end() ) {
            ret.push_back( dependent );
        }
    }

    return ret;
}

dependency_tree::dependency_tree() = default;

void dependency_tree::init(
    const std::map<mod_id, std::vector<mod_id> > &key_dependency_map,
    const std::map<mod_id, std::vector<mod_id> > &key_conflict_map
)
{
    build_node_map( key_dependency_map );
    build_connections( key_dependency_map, key_conflict_map );
}

void dependency_tree::build_node_map(
    const std::map<mod_id, std::vector<mod_id > > &key_dependency_map )
{
    for( auto &elem : key_dependency_map ) {
        // check to see if the master node map knows the key
        if( master_node_map.find( elem.first ) == master_node_map.end() ) {
            master_node_map.emplace( elem.first, elem.first );
        }
    }
}

void dependency_tree::build_connections(
    const std::map<mod_id, std::vector<mod_id > > &key_dependency_map,
    const std::map<mod_id, std::vector<mod_id > > &key_conflict_map
)
{
    for( auto &elem : key_dependency_map ) {
        const auto iter = master_node_map.find( elem.first );
        if( iter != master_node_map.end() ) {
            dependency_node *knode = &iter->second;

            // apply parents list
            std::vector<mod_id> vnode_parents = elem.second;
            for( auto &vnode_parent : vnode_parents ) {
                const auto iter = master_node_map.find( vnode_parent );
                if( iter != master_node_map.end() ) {
                    dependency_node *vnode = &iter->second;

                    knode->add_parent( vnode );
                    vnode->add_child( knode );
                } else {
                    // missing dependency!
                    knode->all_errors[node_error_type::missing_dep].push_back( "[" + vnode_parent.str() + "]" );
                }
            }
        }
    }

    for( const std::pair<const mod_id, std::vector<mod_id>> &elem : key_conflict_map ) {
        const auto iter = master_node_map.find( elem.first );
        if( iter != master_node_map.end() ) {
            dependency_node *knode = &iter->second;

            const std::vector<mod_id> &vnode_conflicts = elem.second;
            for( const mod_id &vnode_conflict : vnode_conflicts ) {
                const auto iter = master_node_map.find( vnode_conflict );
                if( iter != master_node_map.end() ) {
                    dependency_node *vnode = &iter->second;

                    knode->add_conflict( vnode );
                    vnode->add_conflict( knode );
                }
            }
        }
    }

    // finalize and check for circular dependencies
    check_for_strongly_connected_components();

    for( auto &elem : master_node_map ) {
        elem.second.inherit_errors();
    }

    // check for conflicts between dependencies
    check_for_conflicting_dependencies();
}

std::vector<mod_id> dependency_tree::get_dependencies_of_X_as_strings( mod_id key ) const
{
    const auto iter = master_node_map.find( key );
    if( iter != master_node_map.end() ) {
        return iter->second.get_dependencies_as_strings();
    }
    return std::vector<mod_id>();
}

std::vector<dependency_node *> dependency_tree::get_dependencies_of_X_as_nodes( mod_id key ) const
{
    const auto iter = master_node_map.find( key );
    if( iter != master_node_map.end() ) {
        return iter->second.get_dependencies_as_nodes();
    }
    return std::vector<dependency_node *>();
}

std::vector<mod_id> dependency_tree::get_dependents_of_X_as_strings( mod_id key ) const
{
    const auto iter = master_node_map.find( key );
    if( iter != master_node_map.end() ) {
        return iter->second.get_dependents_as_strings();
    }
    return std::vector<mod_id>();
}

std::vector<dependency_node *> dependency_tree::get_dependents_of_X_as_nodes( mod_id key ) const
{
    const auto iter = master_node_map.find( key );
    if( iter != master_node_map.end() ) {
        return iter->second.get_dependents_as_nodes();
    }
    return std::vector<dependency_node *>();
}

bool dependency_tree::is_available( mod_id key ) const
{
    const auto iter = master_node_map.find( key );
    if( iter != master_node_map.end() ) {
        return iter->second.is_available();
    }

    return false;
}

void dependency_tree::clear()
{
    master_node_map.clear();
}

dependency_node *dependency_tree::get_node( mod_id key ) const
{
    const auto iter = master_node_map.find( key );
    if( iter != master_node_map.end() ) {
        return const_cast<dependency_node *>( &iter->second );
    }
    return nullptr;
}

// makes sure to set up Cycle errors properly!
void dependency_tree::check_for_strongly_connected_components()
{
    strongly_connected_components = std::vector<std::vector<dependency_node * > >();
    open_index = 0;
    connection_stack = std::stack<dependency_node *>();

    for( auto &elem : master_node_map ) {
        //nodes_on_stack = std::vector<dependency_node*>();
        // clear it for the next stack to run
        if( elem.second.index < 0 ) {
            strong_connect( &elem.second );
        }
    }

    for( std::vector<dependency_node *> &list : strongly_connected_components ) {
        if( list.size() <= 1 ) {
            continue;
        }

        std::string err_msg = "/";
        for( const dependency_node *node : list ) {
            err_msg += node->key.str();
            err_msg += "/";
        }
        for( dependency_node *elem : list ) {
            elem->all_errors[node_error_type::cyclic_dep].push_back( err_msg );
        }
    }
}

void dependency_tree::strong_connect( dependency_node *dnode )
{
    dnode->index = open_index;
    dnode->lowlink = open_index;
    ++open_index;
    connection_stack.push( dnode );
    dnode->on_stack = true;

    for( std::vector<dependency_node *>::iterator it = dnode->parents.begin();
         it != dnode->parents.end(); ++it ) {
        if( ( *it )->index < 0 ) {
            strong_connect( *it );
            dnode->lowlink = std::min( dnode->lowlink, ( *it )->lowlink );
        } else if( ( *it )->on_stack ) {
            dnode->lowlink = std::min( dnode->lowlink, ( *it )->index );
        }
    }

    if( dnode->lowlink == dnode->index ) {
        std::vector<dependency_node *> scc;

        dependency_node *d;
        do {
            d = connection_stack.top();
            scc.push_back( d );
            connection_stack.pop();
            d->on_stack = false;
        } while( dnode->key != d->key );

        strongly_connected_components.push_back( scc );
        dnode->on_stack = false;
    }
}

static bool depnode_comparator( const dependency_node *a, const dependency_node *b )
{
    return mod_id::LexCmp().operator()( a->key, b->key );
}

void dependency_tree::check_for_conflicting_dependencies()
{
    for( auto &node : master_node_map ) {
        dependency_node *this_node = &node.second;
        std::vector<dependency_node *> all_deps = get_dependencies_of_X_as_nodes( node.first );
        std::ranges::sort( all_deps, depnode_comparator );

        for( auto dep_it = all_deps.begin(); dep_it != all_deps.end(); dep_it++ ) {
            const dependency_node *this_dep = *dep_it;
            std::vector<const dependency_node *> this_dep_conflicts = this_dep->conflicts;
            std::ranges::sort( this_dep_conflicts, depnode_comparator );

            std::vector<const dependency_node *> both;
            std::set_intersection(
                std::next( dep_it ), all_deps.end(),
                this_dep_conflicts.begin(), this_dep_conflicts.end(),
                std::back_inserter( both ), depnode_comparator
            );

            for( const dependency_node *this_dep_conf : both ) {
                //~ Single entry in list of conflicting dependencies, both %s are mod ids.
                //~ Example of final string: "[aftershock] with [classic-zombies]"
                std::string msg = string_format( _( "[%1$s] with [%2$s]" ), this_dep->key, this_dep_conf->key );
                this_node->all_errors[node_error_type::conflicting_deps].push_back( msg );
            }
        }
    }
}
