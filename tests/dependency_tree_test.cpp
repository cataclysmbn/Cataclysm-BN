#include "catch/catch.hpp"
#include "dependency_tree.h"
#include "mod_manager.h"
#include "stringmaker.h"

using t_mod_id = const char*;
using t_mod_list = std::vector<t_mod_id>;
using t_key_dep_map = std::map<t_mod_id, t_mod_list>;
using t_map_entry = std::pair<t_mod_id, t_mod_list>;

static std::map<mod_id, std::vector<mod_id>> build_map(const t_key_dep_map& m) {
    std::map<mod_id, std::vector<mod_id>> ret;

    for (const auto& entry : m) {
        std::vector<mod_id> ids;
        for (t_mod_id elem : entry.second) { ids.emplace_back(elem); }
        ret.emplace(entry.first, ids);
    }

    return ret;
}

static dependency_tree make_tree(
    const t_key_dep_map& dependencies, const t_key_dep_map& conflicts) {
    dependency_tree tree;

    tree.init(build_map(dependencies), build_map(conflicts));

    return tree;
}

TEST_CASE("deptree_get_node", "[dependency_tree]") {
    t_key_dep_map dep_map = {{{"a", {}}, {"b", {{"c", "d"}}}}};

    t_key_dep_map conf_map = {{{"e", {}}, {"c", {{"e", "f"}}}}};

    dependency_tree tree = make_tree(dep_map, conf_map);

    REQUIRE(tree.get_node(mod_id("a")));
    REQUIRE(tree.get_node(mod_id("b")));
    REQUIRE_FALSE(tree.get_node(mod_id("c")));
    REQUIRE_FALSE(tree.get_node(mod_id("d")));
    REQUIRE_FALSE(tree.get_node(mod_id("e")));
    REQUIRE_FALSE(tree.get_node(mod_id("f")));
}

TEST_CASE("deptree_basic", "[dependency_tree]") {
    t_key_dep_map dep_map = {
        {{"a", {}},
         {"b", {}},
         {"c", {}},
         {"d", {{"c", "b", "a"}}},
         {"e", {{"b", "d"}}},
         {"f", {{"e", "d"}}}}};

    t_key_dep_map conf_map = {{{"e", {"f"}}}};

    dependency_tree tree = make_tree(dep_map, conf_map);

    dependency_node* node = tree.get_node(mod_id("d"));
    REQUIRE(node);

    CAPTURE(node->s_errors());
    CHECK_FALSE(node->has_errors());
    CHECK(node->is_available());

    CHECK(node->get_dependencies_as_strings()
          == mod_manager::t_mod_list{{mod_id("c"), mod_id("b"), mod_id("a")}});

    CHECK(node->get_dependents_as_strings() == mod_manager::t_mod_list{{mod_id("f"), mod_id("e")}});
}

TEST_CASE("deptree_missing_dependency", "[dependency_tree]") {
    t_key_dep_map dep_map = {{{"a", {}}, {"c", {"e"}}, {"d", {{"c", "b", "a"}}}}};

    t_key_dep_map conf_map = {};

    dependency_tree tree = make_tree(dep_map, conf_map);

    dependency_node* node = tree.get_node(mod_id("d"));
    REQUIRE(node);

    REQUIRE(node->has_errors());
    CHECK(node->s_errors() == "Missing Dependency(ies): [b], [e]");
    CHECK_FALSE(node->is_available());
    CHECK(
        node->get_dependencies_as_strings() == mod_manager::t_mod_list{{mod_id("c"), mod_id("a")}});
}

TEST_CASE("deptree_circular_dependency", "[dependency_tree]") {
    t_key_dep_map dep_map = {
        {{"a", {}}, {"b", {{"a", "d"}}}, {"c", {{"a", "d"}}}, {"d", {{"c", "b", "a"}}}}};

    t_key_dep_map conf_map = {};

    dependency_tree tree = make_tree(dep_map, conf_map);

    dependency_node* node = tree.get_node(mod_id("d"));
    REQUIRE(node);

    REQUIRE(node->has_errors());
    CHECK(node->s_errors() == "Has dependency cycle(s): /c/d/b/");
    CHECK_FALSE(node->is_available());
}

TEST_CASE("deptree_multiple_dep_cycles", "[dependency_tree]") {
    t_key_dep_map dep_map = {
        {{"a", {"b"}},
         {"b", {"a"}},
         {"c", {"d"}},
         {"d", {"c"}},
         {"e", {"f"}},
         {"f", {"e"}},
         {"g", {{"a", "c", "e"}}}}};

    t_key_dep_map conf_map = {};

    dependency_tree tree = make_tree(dep_map, conf_map);

    dependency_node* node = tree.get_node(mod_id("g"));
    REQUIRE(node);

    REQUIRE(node->has_errors());
    CHECK(node->s_errors() == "Has dependency cycle(s): /f/e/, /d/c/, /b/a/");
    CHECK_FALSE(node->is_available());
}

TEST_CASE("deptree_conflicting_dependency", "[dependency_tree]") {
    t_key_dep_map dep_map = {
        {{"a", {}}, {"b", {}}, {"c", {"b"}}, {"d", {{"a", "c"}}}, {"e", {"c"}}}};

    t_key_dep_map conf_map = {{{"a", {"b"}}, {"e", {"d"}}}};

    dependency_tree tree = make_tree(dep_map, conf_map);

    dependency_node* node = tree.get_node(mod_id("d"));
    REQUIRE(node);

    REQUIRE(node->has_errors());
    CHECK(node->s_errors() == "Has conflicting dependencies: [a] with [b]");
    CHECK_FALSE(node->is_available());
}

TEST_CASE("deptree_complex_conflict", "[dependency_tree]") {
    t_key_dep_map dep_map = {
        {{"a", {"b"}}, {"b", {"c"}}, {"c", {"d"}}, {"d", {"a"}}, {"e", {{"a", "b", "c", "d"}}}}};

    t_key_dep_map conf_map = {{{"a", {"c"}}, {"b", {"d"}}}};

    dependency_tree tree = make_tree(dep_map, conf_map);

    dependency_node* node = tree.get_node(mod_id("e"));
    REQUIRE(node);

    REQUIRE(node->has_errors());
    CHECK(node->s_errors()
          == "Has conflicting dependencies: [a] with [c], [b] with [d]\n"
             "Has dependency cycle(s): /d/c/b/a/");
    CHECK_FALSE(node->is_available());
}
