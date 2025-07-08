#pragma once

#include <map>
#include <stack>
#include <vector>
#include <string>

#include "type_id.h"

enum class node_error_type : int {
    missing_dep = 0,
    conflicting_deps,
    cyclic_dep,
    num
};

class dependency_node
{
    public:
        std::vector<dependency_node *> parents;
        std::vector<dependency_node *> children;
        std::vector<const dependency_node *> conflicts;
        std::map<node_error_type, std::vector<std::string> > all_errors;
        mod_id key;
        bool availability;

        // cyclic check variables
        int index;
        int lowlink;
        bool on_stack;

        dependency_node();
        dependency_node( mod_id _key );

        void add_parent( dependency_node *parent );
        void add_child( dependency_node *child );
        void add_conflict( const dependency_node *conflict );

        bool is_available() const;
        bool has_errors() const;
        std::map<node_error_type, std::vector<std::string > > errors();
        std::string s_errors();

        // Tree traversal
        // Upward towards head(s)
        std::vector<mod_id> get_dependencies_as_strings() const;
        std::vector<dependency_node * > get_dependencies_as_nodes() const;
        // Downward towards leaf(ves)
        std::vector<mod_id> get_dependents_as_strings() const;
        std::vector< dependency_node * > get_dependents_as_nodes() const;

        void inherit_errors();
};

class dependency_tree
{
    public:
        dependency_tree();

        void init(
            const std::map<mod_id, std::vector<mod_id> > &key_dependency_map,
            const std::map<mod_id, std::vector<mod_id> > &key_conflict_map
        );

        void clear();

        // tree traversal
        // Upward by key
        std::vector<mod_id> get_dependencies_of_X_as_strings( mod_id key ) const;
        std::vector<dependency_node * > get_dependencies_of_X_as_nodes( mod_id key ) const;
        // Downward by key
        std::vector< mod_id> get_dependents_of_X_as_strings( mod_id key ) const;
        std::vector< dependency_node * > get_dependents_of_X_as_nodes( mod_id key ) const;

        bool is_available( mod_id key ) const;
        dependency_node *get_node( mod_id key ) const;

        std::map<mod_id, dependency_node, mod_id::LexCmp> master_node_map;
    private:
        // Don't need to be called directly. Only reason to call these are during initialization phase.
        void build_node_map( const std::map<mod_id, std::vector<mod_id > > &key_dependency_map );
        void build_connections(
            const std::map<mod_id, std::vector<mod_id > > &key_dependency_map,
            const std::map<mod_id, std::vector<mod_id > > &key_conflict_map
        );

        /*
        Cyclic Dependency checks using Tarjan's Strongly Connected Components algorithm
        Order is O(N+E) and linearly increases with number of nodes and number of connections between them.
        References:
            http://en.wikipedia.org/wiki/Tarjan's_strongly_connected_components_algorithm
            http://www.cosc.canterbury.ac.nz/tad.takaoka/alg/graphalg/sc.txt
        */
        void check_for_strongly_connected_components();
        void strong_connect( dependency_node *dnode );

        std::vector<std::vector<dependency_node * > > strongly_connected_components;
        std::stack<dependency_node *> connection_stack;
        std::vector<dependency_node *> nodes_on_stack;
        int open_index;

        void check_for_conflicting_dependencies();
};


