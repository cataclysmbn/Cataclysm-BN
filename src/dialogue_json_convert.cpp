// dialogue_json_convert.cpp — Convert TALK_TOPIC JSON to yarn AST nodes.
//
// DEPRECATED: This file is the legacy JSON → Yarn conversion bridge.
// Remove when the ecosystem has fully migrated to .yarn files.
//
// Called once per topic from load_talk_topic().  Converts the raw JSON
// into yarn node_element nodes (dialogue, choice_group, if_block, commands,
// jumps) and caches them for build_legacy_yarn_stories().

#include "dialogue_json_convert.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

#include "condition.h"
#include "debug.h"
#include "item.h"
#include "item_category.h"
#include "json.h"
#include "translations.h"
#include "yarn_dialogue.h"

namespace
{

// ============================================================
// expr_node builders
// ============================================================

auto lit( bool v ) -> yarn::expr_node
{
    yarn::expr_node n;
    n.type        = yarn::expr_node::kind::literal;
    n.literal_val = v;
    return n;
}

auto lit_str( std::string v ) -> yarn::expr_node
{
    yarn::expr_node n;
    n.type        = yarn::expr_node::kind::literal;
    n.literal_val = std::move( v );
    return n;
}

auto lit_num( double v ) -> yarn::expr_node
{
    yarn::expr_node n;
    n.type        = yarn::expr_node::kind::literal;
    n.literal_val = v;
    return n;
}

// func_call(name, {arg1, arg2, ...})
auto fn( std::string name, std::vector<yarn::expr_node> args = {} ) -> yarn::expr_node {
    yarn::expr_node n;
    n.type      = yarn::expr_node::kind::func_call;
    n.func_name = std::move( name );
    n.args      = std::move( args );
    return n;
}

// Binary logical AND of two expressions.
auto and2( yarn::expr_node lhs, yarn::expr_node rhs ) -> yarn::expr_node
{
    yarn::expr_node n;
    n.type             = yarn::expr_node::kind::binary_op;
    n.binary_operation = yarn::expr_node::bin_op::logical_and;
    n.children         = { std::move( lhs ), std::move( rhs ) };
    return n;
}

// Binary logical OR of two expressions.
auto or2( yarn::expr_node lhs, yarn::expr_node rhs ) -> yarn::expr_node
{
    yarn::expr_node n;
    n.type             = yarn::expr_node::kind::binary_op;
    n.binary_operation = yarn::expr_node::bin_op::logical_or;
    n.children         = { std::move( lhs ), std::move( rhs ) };
    return n;
}

// Logical NOT of an expression.
auto not1( yarn::expr_node e ) -> yarn::expr_node
{
    yarn::expr_node n;
    n.type            = yarn::expr_node::kind::unary_op;
    n.unary_operation = yarn::expr_node::un_op::logical_not;
    n.children        = { std::move( e ) };
    return n;
}

// Reduce a non-empty vector of exprs with a combiner (and2 or or2).
template<typename Combiner>
auto fold_exprs( std::vector<yarn::expr_node> exprs, Combiner combine ) -> yarn::expr_node
{
    auto it = exprs.begin();
    auto result = std::move( *it );
    ++it;
    while( it != exprs.end() ) {
        result = combine( std::move( result ), std::move( *it ) );
        ++it;
    }
    return result;
}

// ============================================================
// node_element builders
// ============================================================

auto make_dialogue( std::string text, std::string speaker = "" ) -> yarn::node_element
{
    yarn::node_element e;
    e.type    = yarn::node_element::kind::dialogue;
    e.text    = std::move( text );
    e.speaker = std::move( speaker );
    return e;
}

// JSON-converted topic transitions use goto_node (push) so TALK_NONE pops back to the caller.
auto make_goto( std::string target ) -> yarn::node_element
{
    yarn::node_element e;
    e.type        = yarn::node_element::kind::goto_node;
    e.jump_target = std::move( target );
    return e;
}

auto make_stop() -> yarn::node_element
{
    yarn::node_element e;
    e.type = yarn::node_element::kind::stop;
    return e;
}

auto make_return() -> yarn::node_element
{
    yarn::node_element e;
    e.type = yarn::node_element::kind::yarn_return;
    return e;
}

auto make_legacy( std::string topic ) -> yarn::node_element
{
    yarn::node_element e;
    e.type        = yarn::node_element::kind::legacy_topic;
    e.jump_target = std::move( topic );
    return e;
}

auto make_command( std::string name,
std::vector<yarn::expr_node> args = {} ) -> yarn::node_element {
    yarn::node_element e;
    e.type         = yarn::node_element::kind::command;
    e.command_name = std::move( name );
    e.command_args = std::move( args );
    return e;
}

auto make_if_block( yarn::expr_node condition,
                    std::vector<yarn::node_element> if_body,
std::vector<yarn::node_element> else_body = {} ) -> yarn::node_element {
    yarn::node_element e;
    e.type      = yarn::node_element::kind::if_block;
    e.condition = std::move( condition );
    e.if_body   = std::move( if_body );
    e.else_body = std::move( else_body );
    return e;
}

// ============================================================
// Special topic dispatch
// ============================================================

// Topics that get special yarn treatment instead of a <<jump>>.
// Returns the appropriate node_elements to end/navigate, or empty
// if the topic should be treated as a normal jump target.
auto special_topic_elements( const std::string &id ) -> std::vector<yarn::node_element>
{
    if( id == "TALK_DONE" ) {
        return { make_dialogue( "Bye." ), make_stop() };
    }
    if( id == "TALK_NONE" ) {
        return { make_return() };
    }
    return {};
}

// Topics that keep a legacy_topic stub because they require old-system UI.
static const std::unordered_set<std::string> s_legacy_ui_topics = {
    "TALK_TRAIN", "TALK_TRAIN_STYLE", "TALK_TRAIN_MAGIC",
    "TALK_MISSION_LIST", "TALK_MISSION_LIST_ASSIGNED",
    "TALK_MISSION_DESCRIBE", "TALK_MISSION_DESCRIBE_URGENT",
    "TALK_MISSION_OFFER", "TALK_MISSION_ACCEPTED", "TALK_MISSION_REJECTED",
    "TALK_MISSION_ADVICE", "TALK_MISSION_INQUIRE",
    "TALK_MISSION_SUCCESS", "TALK_MISSION_SUCCESS_LIE", "TALK_MISSION_FAILURE",
    "TALK_MISSION_REWARD",
    "TALK_SUGGEST_FOLLOW", "TALK_HOW_MUCH_FURTHER",
    "TALK_COMBAT_ENGAGEMENT",
    "TALK_MIND_CONTROL",   // dynamic_line runs talk_function::follow() as a side effect
    // Group 99: hotkey-activated info topics (O/L/S/Y); gen_responses adds only "Okay." → TALK_NONE.
    "TALK_OPINION", "TALK_SIZE_UP", "TALK_LOOK_AT", "TALK_SHOUT",
};

// ============================================================
// Condition JSON → expr_node
// ============================================================

// Forward declaration.
auto json_condition_to_expr( const JsonValue &jv ) -> yarn::expr_node;

// Mapping from simple_string_conds name to yarn function name.
// (Most are identical; include explicit mapping for any renames.)
static const std::unordered_map<std::string, std::string> s_simple_cond_map = {
    { "u_male",                    "u_male" },
    { "u_female",                  "u_female" },
    { "npc_male",                  "npc_male" },
    { "npc_female",                "npc_female" },
    { "has_no_assigned_mission",   "has_no_assigned_mission" },
    { "has_assigned_mission",      "has_assigned_mission" },
    { "has_many_assigned_missions", "has_many_assigned_missions" },
    { "has_no_available_mission",  "has_no_available_mission" },
    { "has_available_mission",     "has_available_mission" },
    { "has_many_available_missions", "has_many_available_missions" },
    { "npc_available",             "npc_available" },
    { "npc_following",             "npc_following" },
    { "npc_friend",                "npc_friend" },
    { "npc_hostile",               "npc_hostile" },
    { "npc_train_skills",          "npc_train_skills" },
    { "npc_train_styles",          "npc_train_styles" },
    { "at_safe_space",             "at_safe_space" },
    { "is_day",                    "is_day" },
    { "npc_has_activity",          "npc_has_activity" },
    { "is_outside",                "is_outside" },
    { "u_can_stow_weapon",         "u_can_stow_weapon" },
    { "npc_can_stow_weapon",       "npc_can_stow_weapon" },
    { "u_has_weapon",              "u_has_weapon" },
    { "npc_has_weapon",            "npc_has_weapon" },
    { "u_driving",                 "u_driving" },
    { "npc_driving",               "npc_driving" },
    { "has_pickup_list",           "has_pickup_list" },
    { "u_has_stolen_item",         "u_has_stolen_item" },
    { "mission_complete",          "mission_complete" },
    { "mission_incomplete",        "mission_incomplete" },
    { "mission_has_generic_rewards", "mission_has_generic_rewards" },
    { "npc_is_riding",             "npc_is_riding" },
    // Stubbed — require dialogue context unavailable in yarn:
    { "is_by_radio",               "is_by_radio" },
    { "has_reason",                "has_reason" },
    // Unimplemented in original code (condition.cpp returns false for these):
    { "mission_failed",            "mission_failed" },
    { "npc_has_destination",       "npc_has_destination" },
    { "asked_for_item",            "asked_for_item" },
};

auto json_condition_obj_to_expr( const JsonObject &jo ) -> yarn::expr_node
{
    jo.allow_omitted_members();
    // Compound conditions
    if( jo.has_array( "and" ) ) {
        std::vector<yarn::expr_node> parts;
        for( const JsonValue &entry : jo.get_array( "and" ) ) {
            parts.push_back( json_condition_to_expr( entry ) );
        }
        if( parts.empty() ) { return lit( true ); }
        return fold_exprs( std::move( parts ), and2 );
    }
    if( jo.has_array( "or" ) ) {
        std::vector<yarn::expr_node> parts;
        for( const JsonValue &entry : jo.get_array( "or" ) ) {
            parts.push_back( json_condition_to_expr( entry ) );
        }
        if( parts.empty() ) { return lit( false ); }
        return fold_exprs( std::move( parts ), or2 );
    }
    if( jo.has_member( "not" ) ) {
        return not1( json_condition_to_expr( jo.get_member( "not" ) ) );
    }

    // Simple (boolean-flag) conditions — key present with value true.
    for( const auto &[key, fn_name] : s_simple_cond_map ) {
        if( jo.has_bool( key ) || jo.has_string( key ) ) {
            // Some simple conds appear as { "key": "key" } (string-valued)
            // or { "key": true } (bool-valued); both mean the condition holds.
            return fn( fn_name );
        }
    }

    // mission_complete / mission_incomplete with string value
    if( jo.has_string( "mission_complete" ) ) {
        return fn( "mission_complete", { lit_str( jo.get_string( "mission_complete" ) ) } );
    }
    if( jo.has_string( "mission_incomplete" ) ) {
        return fn( "mission_incomplete", { lit_str( jo.get_string( "mission_incomplete" ) ) } );
    }

    // Complex conditions
    if( jo.has_member( "u_has_trait" ) ) {
        return fn( "u_has_trait", { lit_str( jo.get_string( "u_has_trait" ) ) } );
    }
    if( jo.has_member( "npc_has_trait" ) ) {
        return fn( "npc_has_trait", { lit_str( jo.get_string( "npc_has_trait" ) ) } );
    }
    if( jo.has_member( "u_has_trait_flag" ) ) {
        return fn( "u_has_trait_flag", { lit_str( jo.get_string( "u_has_trait_flag" ) ) } );
    }
    if( jo.has_member( "npc_has_trait_flag" ) ) {
        return fn( "npc_has_trait_flag", { lit_str( jo.get_string( "npc_has_trait_flag" ) ) } );
    }
    if( jo.has_member( "u_has_any_trait" ) ) {
        std::vector<yarn::expr_node> args;
        if( jo.has_string( "u_has_any_trait" ) ) {
            args.push_back( lit_str( jo.get_string( "u_has_any_trait" ) ) );
        } else if( jo.has_array( "u_has_any_trait" ) ) {
            for( const std::string &s : jo.get_string_array( "u_has_any_trait" ) ) {
                args.push_back( lit_str( s ) );
            }
        }
        return fn( "u_has_any_trait", std::move( args ) );
    }
    if( jo.has_member( "npc_has_any_trait" ) ) {
        std::vector<yarn::expr_node> args;
        if( jo.has_string( "npc_has_any_trait" ) ) {
            args.push_back( lit_str( jo.get_string( "npc_has_any_trait" ) ) );
        } else if( jo.has_array( "npc_has_any_trait" ) ) {
            for( const std::string &s : jo.get_string_array( "npc_has_any_trait" ) ) {
                args.push_back( lit_str( s ) );
            }
        }
        return fn( "npc_has_any_trait", std::move( args ) );
    }
    if( jo.has_member( "npc_has_class" ) ) {
        return fn( "npc_has_class", { lit_str( jo.get_string( "npc_has_class" ) ) } );
    }
    if( jo.has_member( "u_has_mission" ) ) {
        return fn( "u_has_mission", { lit_str( jo.get_string( "u_has_mission" ) ) } );
    }

    // Attribute minimum checks: value IS the minimum
    auto make_attr_check = []( const std::string & fn_name,
    const JsonObject & j, const std::string & key ) -> yarn::expr_node {
        return fn( fn_name, { lit_num( static_cast<double>( j.get_int( key ) ) ) } );
    };
    if( jo.has_int( "u_has_strength" ) ) {
        return make_attr_check( "u_has_strength", jo, "u_has_strength" );
    }
    if( jo.has_int( "npc_has_strength" ) ) {
        return make_attr_check( "npc_has_strength", jo, "npc_has_strength" );
    }
    if( jo.has_int( "u_has_dexterity" ) ) {
        return make_attr_check( "u_has_dexterity", jo, "u_has_dexterity" );
    }
    if( jo.has_int( "npc_has_dexterity" ) ) {
        return make_attr_check( "npc_has_dexterity", jo, "npc_has_dexterity" );
    }
    if( jo.has_int( "u_has_intelligence" ) ) {
        return make_attr_check( "u_has_intelligence", jo, "u_has_intelligence" );
    }
    if( jo.has_int( "npc_has_intelligence" ) ) {
        return make_attr_check( "npc_has_intelligence", jo, "npc_has_intelligence" );
    }
    if( jo.has_int( "u_has_perception" ) ) {
        return make_attr_check( "u_has_perception", jo, "u_has_perception" );
    }
    if( jo.has_int( "npc_has_perception" ) ) {
        return make_attr_check( "npc_has_perception", jo, "npc_has_perception" );
    }

    // Item / equipment conditions
    if( jo.has_string( "u_is_wearing" ) ) {
        return fn( "u_is_wearing", { lit_str( jo.get_string( "u_is_wearing" ) ) } );
    }
    if( jo.has_string( "npc_is_wearing" ) ) {
        return fn( "npc_is_wearing", { lit_str( jo.get_string( "npc_is_wearing" ) ) } );
    }
    if( jo.has_string( "u_has_item" ) ) {
        return fn( "u_has_item", { lit_str( jo.get_string( "u_has_item" ) ) } );
    }
    if( jo.has_string( "npc_has_item" ) ) {
        return fn( "npc_has_item", { lit_str( jo.get_string( "npc_has_item" ) ) } );
    }
    // u_has_items: { "u_has_items": { "item": "id", "count": N } }
    if( jo.has_member( "u_has_items" ) ) {
        JsonObject sub = jo.get_object( "u_has_items" );
        sub.allow_omitted_members();
        auto name  = sub.get_string( "item", "" );
        auto count = sub.get_int( "count", 1 );
        return fn( "u_has_items", { lit_str( name ), lit_num( static_cast<double>( count ) ) } );
    }
    if( jo.has_member( "npc_has_items" ) ) {
        JsonObject sub = jo.get_object( "npc_has_items" );
        sub.allow_omitted_members();
        auto name  = sub.get_string( "item", "" );
        auto count = sub.get_int( "count", 1 );
        return fn( "npc_has_items", { lit_str( name ), lit_num( static_cast<double>( count ) ) } );
    }
    if( jo.has_string( "u_has_item_category" ) ) {
        return fn( "u_has_item_category", { lit_str( jo.get_string( "u_has_item_category" ) ) } );
    }
    if( jo.has_string( "npc_has_item_category" ) ) {
        return fn( "npc_has_item_category", { lit_str( jo.get_string( "npc_has_item_category" ) ) } );
    }
    if( jo.has_string( "u_has_bionics" ) ) {
        // In the old system "u_has_bionics" checks for bionic presence; same as u_has_bionic.
        return fn( "u_has_bionic", { lit_str( jo.get_string( "u_has_bionics" ) ) } );
    }
    if( jo.has_string( "npc_has_bionics" ) ) {
        return fn( "npc_has_bionic", { lit_str( jo.get_string( "npc_has_bionics" ) ) } );
    }
    if( jo.has_string( "u_has_effect" ) ) {
        return fn( "u_has_effect", { lit_str( jo.get_string( "u_has_effect" ) ) } );
    }
    if( jo.has_string( "npc_has_effect" ) ) {
        return fn( "npc_has_effect", { lit_str( jo.get_string( "npc_has_effect" ) ) } );
    }

    // Needs: { "u_need": "hunger", "amount": 50 }  or  { "u_need": "fatigue", "level": "TIRED" }
    // The "level" string maps to the fatigue_levels enum values (integers).
    static const std::unordered_map<std::string, int> fatigue_level_ints = {
        { "TIRED",          191  },
        { "DEAD_TIRED",     383  },
        { "EXHAUSTED",      575  },
        { "MASSIVE_FATIGUE", 1000 },
    };
    auto resolve_need_amount = [&]() -> int {
        if( jo.has_int( "amount" ) )
        {
            return jo.get_int( "amount" );
        }
        if( jo.has_string( "level" ) )
        {
            auto it = fatigue_level_ints.find( jo.get_string( "level" ) );
            if( it != fatigue_level_ints.end() ) {
                return it->second;
            }
        }
        return 0;
    };
    if( jo.has_string( "u_need" ) ) {
        return fn( "u_need", { lit_str( jo.get_string( "u_need" ) ),
                               lit_num( static_cast<double>( resolve_need_amount() ) )
                             } );
    }
    if( jo.has_string( "npc_need" ) ) {
        return fn( "npc_need", { lit_str( jo.get_string( "npc_need" ) ),
                                 lit_num( static_cast<double>( resolve_need_amount() ) )
                               } );
    }

    // Economy
    if( jo.has_int( "u_has_ecash" ) ) {
        return fn( "u_get_ecash", {} ); // Compare at runtime via >= is complex; use simple threshold
        // TODO: convert to u_get_ecash() >= N comparison once binary ops on numbers are supported
    }
    if( jo.has_int( "u_are_owed" ) ) {
        return fn( "u_get_owed", {} );
    }

    // NPC rules
    if( jo.has_string( "npc_aim_rule" ) ) {
        return fn( "npc_aim_rule", { lit_str( jo.get_string( "npc_aim_rule" ) ) } );
    }
    if( jo.has_string( "npc_engagement_rule" ) ) {
        return fn( "npc_engagement_rule", { lit_str( jo.get_string( "npc_engagement_rule" ) ) } );
    }
    if( jo.has_string( "npc_cbm_reserve_rule" ) ) {
        return fn( "npc_cbm_reserve_rule", { lit_str( jo.get_string( "npc_cbm_reserve_rule" ) ) } );
    }
    if( jo.has_string( "npc_cbm_recharge_rule" ) ) {
        return fn( "npc_cbm_recharge_rule", { lit_str( jo.get_string( "npc_cbm_recharge_rule" ) ) } );
    }
    if( jo.has_string( "npc_rule" ) ) {
        return fn( "npc_has_rule", { lit_str( jo.get_string( "npc_rule" ) ) } );
    }
    if( jo.has_string( "npc_override" ) ) {
        return fn( "npc_has_override", { lit_str( jo.get_string( "npc_override" ) ) } );
    }
    if( jo.has_string( "npc_is_riding" ) ) {
        jo.get_string( "npc_is_riding" );  // consume value (unused by the condition)
        return fn( "npc_is_riding" );
    }
    if( jo.get_bool( "npc_service", false ) ) {
        return fn( "npc_available" );
    }

    // Location
    if( jo.has_string( "u_at_om_location" ) ) {
        return fn( "u_at_om_location", { lit_str( jo.get_string( "u_at_om_location" ) ) } );
    }
    if( jo.has_string( "npc_at_om_location" ) ) {
        return fn( "npc_at_om_location", { lit_str( jo.get_string( "npc_at_om_location" ) ) } );
    }

    // World queries
    if( jo.has_string( "npc_role_nearby" ) ) {
        return fn( "npc_role_nearby", { lit_str( jo.get_string( "npc_role_nearby" ) ) } );
    }
    if( jo.has_int( "npc_allies" ) ) {
        return fn( "npc_allies", { lit_num( static_cast<double>( jo.get_int( "npc_allies" ) ) ) } );
    }

    // Time
    if( jo.has_int( "days_since_cataclysm" ) ) {
        return fn( "days_since_cataclysm" ); // TODO: compare result
    }
    if( jo.has_string( "is_season" ) ) {
        return fn( "is_season", { lit_str( jo.get_string( "is_season" ) ) } );
    }

    // Missions
    if( jo.has_string( "mission_goal" ) ) {
        return fn( "mission_goal", { lit_str( jo.get_string( "mission_goal" ) ) } );
    }

    // Skills: { "u_has_skill": { "skill": "cooking", "level": 3 } }
    if( jo.has_member( "u_has_skill" ) ) {
        if( jo.has_object( "u_has_skill" ) ) {
            JsonObject sk = jo.get_object( "u_has_skill" );
            auto skill_name = sk.get_string( "skill", "" );
            auto level      = sk.get_int( "level", 1 );
            return fn( "u_has_skill", {
                lit_str( skill_name ),
                lit_num( static_cast<double>( level ) )
            } );
        }
        return fn( "u_has_skill", { lit_str( jo.get_string( "u_has_skill" ) ), lit_num( 1.0 ) } );
    }
    if( jo.has_member( "npc_has_skill" ) ) {
        if( jo.has_object( "npc_has_skill" ) ) {
            JsonObject sk = jo.get_object( "npc_has_skill" );
            auto skill_name = sk.get_string( "skill", "" );
            auto level      = sk.get_int( "level", 1 );
            return fn( "npc_has_skill", {
                lit_str( skill_name ),
                lit_num( static_cast<double>( level ) )
            } );
        }
        return fn( "npc_has_skill", { lit_str( jo.get_string( "npc_has_skill" ) ), lit_num( 1.0 ) } );
    }
    if( jo.has_member( "u_know_recipe" ) ) {
        return fn( "u_know_recipe", { lit_str( jo.get_string( "u_know_recipe" ) ) } );
    }

    // Variables: { "u_has_var": "varname", "type": "t", "context": "c", "value": "v" }
    if( jo.has_string( "u_has_var" ) ) {
        auto name    = jo.get_string( "u_has_var" );
        auto type    = jo.get_string( "type", "" );
        auto context = jo.get_string( "context", "" );
        auto value   = jo.get_string( "value", "" );
        return fn( "u_has_var", { lit_str( name ), lit_str( type ), lit_str( context ), lit_str( value ) } );
    }
    if( jo.has_string( "npc_has_var" ) ) {
        auto name    = jo.get_string( "npc_has_var" );
        auto type    = jo.get_string( "type", "" );
        auto context = jo.get_string( "context", "" );
        auto value   = jo.get_string( "value", "" );
        return fn( "npc_has_var", { lit_str( name ), lit_str( type ), lit_str( context ), lit_str( value ) } );
    }
    if( jo.has_string( "u_compare_var" ) ) {
        auto name    = jo.get_string( "u_compare_var" );
        auto type    = jo.get_string( "type", "" );
        auto context = jo.get_string( "context", "" );
        auto op      = jo.get_string( "op", "==" );
        auto value   = jo.get_int( "value", 0 );
        return fn( "u_compare_var", {
            lit_str( name ), lit_str( type ), lit_str( context ),
            lit_str( op ), lit_num( static_cast<double>( value ) )
        } );
    }
    if( jo.has_string( "npc_compare_var" ) ) {
        auto name    = jo.get_string( "npc_compare_var" );
        auto type    = jo.get_string( "type", "" );
        auto context = jo.get_string( "context", "" );
        auto op      = jo.get_string( "op", "==" );
        auto value   = jo.get_int( "value", 0 );
        return fn( "npc_compare_var", {
            lit_str( name ), lit_str( type ), lit_str( context ),
            lit_str( op ), lit_num( static_cast<double>( value ) )
        } );
    }

    debugmsg( "dialogue_convert: unrecognized condition object: %s", jo.str() );
    return lit( true );  // conservative fallback: assume condition passes
}

auto json_condition_to_expr( const JsonValue &jv ) -> yarn::expr_node
{
    if( jv.test_string() ) {
        // Simple condition string — must be a simple_string_conds member
        auto name = jv.get_string();
        auto it   = s_simple_cond_map.find( name );
        if( it != s_simple_cond_map.end() ) {
            return fn( it->second );
        }
        // Also accept mission_complete / mission_incomplete as raw strings
        if( name == "mission_complete" || name == "mission_incomplete" ) {
            return fn( name );
        }
        debugmsg( "dialogue_convert: unrecognized string condition '%s'", name );
        return lit( true );
    }
    if( jv.test_object() ) {
        return json_condition_obj_to_expr( jv.get_object() );
    }
    debugmsg( "dialogue_convert: unexpected condition type" );
    return lit( true );
}

// ============================================================
// Effect JSON → node_elements
// ============================================================

// Convert opinion adjustment to commands.
auto opinion_to_elements( const JsonObject &jo ) -> std::vector<yarn::node_element>
{
    jo.allow_omitted_members();
    std::vector<yarn::node_element> out;
    auto add_if = [&]( const char *key, const char *cmd ) {
        if( jo.has_int( key ) ) {
            auto delta = jo.get_int( key );
            if( delta != 0 ) {
                out.push_back( make_command( cmd, { lit_num( static_cast<double>( delta ) ) } ) );
            }
        }
    };
    add_if( "trust", "npc_add_trust" );
    add_if( "fear",  "npc_add_fear" );
    add_if( "value", "npc_add_value" );
    add_if( "anger", "npc_add_anger" );
    return out;
}

// Convert a single sub-effect object to node_elements.
auto json_sub_effect_to_elements( const JsonObject &jo ) -> std::vector<yarn::node_element>
{
    jo.allow_omitted_members();
    std::vector<yarn::node_element> out;

    // Duration may be an int or a numeric string (e.g. "25920"). Resolve to turns.
    auto read_duration = [&]() -> int {
        if( jo.has_string( "duration" ) )
        {
            const auto s = jo.get_string( "duration" );
            if( s == "PERMANENT" ) { return -1; }  // -1 sentinel → set_permanent() in runtime
            if( !s.empty() ) { return std::stoi( s ); }
        }
        return jo.get_int( "duration", 1 );
    };

    if( jo.has_string( "u_add_effect" ) ) {
        auto id  = jo.get_string( "u_add_effect" );
        auto dur = read_duration();
        out.push_back( make_command( "u_add_effect", { lit_str( id ), lit_num( static_cast<double>( dur ) ) } ) );
    } else if( jo.has_string( "npc_add_effect" ) ) {
        auto id  = jo.get_string( "npc_add_effect" );
        auto dur = read_duration();
        out.push_back( make_command( "npc_add_effect", { lit_str( id ), lit_num( static_cast<double>( dur ) ) } ) );
    } else if( jo.has_string( "u_lose_effect" ) ) {
        out.push_back( make_command( "u_lose_effect", { lit_str( jo.get_string( "u_lose_effect" ) ) } ) );
    } else if( jo.has_string( "npc_lose_effect" ) ) {
        out.push_back( make_command( "npc_lose_effect", { lit_str( jo.get_string( "npc_lose_effect" ) ) } ) );
    } else if( jo.has_string( "u_add_trait" ) ) {
        out.push_back( make_command( "u_add_trait", { lit_str( jo.get_string( "u_add_trait" ) ) } ) );
    } else if( jo.has_string( "npc_add_trait" ) ) {
        out.push_back( make_command( "npc_add_trait", { lit_str( jo.get_string( "npc_add_trait" ) ) } ) );
    } else if( jo.has_string( "u_lose_trait" ) ) {
        out.push_back( make_command( "u_lose_trait", { lit_str( jo.get_string( "u_lose_trait" ) ) } ) );
    } else if( jo.has_string( "npc_lose_trait" ) ) {
        out.push_back( make_command( "npc_lose_trait", { lit_str( jo.get_string( "npc_lose_trait" ) ) } ) );
    } else if( jo.has_string( "u_add_var" ) ) {
        auto name    = jo.get_string( "u_add_var" );
        auto type    = jo.get_string( "type", "" );
        auto context = jo.get_string( "context", "" );
        auto value   = jo.get_string( "value", "" );
        out.push_back( make_command( "u_add_var", { lit_str( name ), lit_str( type ), lit_str( context ), lit_str( value ) } ) );
    } else if( jo.has_string( "npc_add_var" ) ) {
        auto name    = jo.get_string( "npc_add_var" );
        auto type    = jo.get_string( "type", "" );
        auto context = jo.get_string( "context", "" );
        auto value   = jo.get_string( "value", "" );
        out.push_back( make_command( "npc_add_var", { lit_str( name ), lit_str( type ), lit_str( context ), lit_str( value ) } ) );
    } else if( jo.has_string( "u_lose_var" ) ) {
        auto name    = jo.get_string( "u_lose_var" );
        auto type    = jo.get_string( "type", "" );
        auto context = jo.get_string( "context", "" );
        out.push_back( make_command( "u_lose_var", { lit_str( name ), lit_str( type ), lit_str( context ) } ) );
    } else if( jo.has_string( "npc_lose_var" ) ) {
        auto name    = jo.get_string( "npc_lose_var" );
        auto type    = jo.get_string( "type", "" );
        auto context = jo.get_string( "context", "" );
        out.push_back( make_command( "npc_lose_var", { lit_str( name ), lit_str( type ), lit_str( context ) } ) );
    } else if( jo.has_string( "u_adjust_var" ) ) {
        auto name    = jo.get_string( "u_adjust_var" );
        auto type    = jo.get_string( "type", "" );
        auto context = jo.get_string( "context", "" );
        auto amount  = jo.get_int( "adjustment", 0 );
        out.push_back( make_command( "u_adjust_var_legacy", { lit_str( name ), lit_str( type ), lit_str( context ), lit_num( static_cast<double>( amount ) ) } ) );
    } else if( jo.has_string( "npc_adjust_var" ) ) {
        auto name    = jo.get_string( "npc_adjust_var" );
        auto type    = jo.get_string( "type", "" );
        auto context = jo.get_string( "context", "" );
        auto amount  = jo.get_int( "adjustment", 0 );
        out.push_back( make_command( "npc_adjust_var_legacy", { lit_str( name ), lit_str( type ), lit_str( context ), lit_num( static_cast<double>( amount ) ) } ) );
    } else if( jo.has_int( "u_spend_ecash" ) ) {
        out.push_back( make_command( "u_spend_ecash", { lit_num( static_cast<double>( jo.get_int( "u_spend_ecash" ) ) ) } ) );
    } else if( jo.has_string( "u_buy_item" ) ) {
        auto id    = jo.get_string( "u_buy_item" );
        auto cost  = jo.get_int( "cost", 0 );
        auto count = jo.get_int( "count", 1 );
        out.push_back( make_command( "u_buy_item", { lit_str( id ), lit_num( static_cast<double>( cost ) ), lit_num( static_cast<double>( count ) ) } ) );
    } else if( jo.has_string( "u_sell_item" ) ) {
        auto id    = jo.get_string( "u_sell_item" );
        auto cost  = jo.get_int( "cost", 0 );
        auto count = jo.get_int( "count", 1 );
        out.push_back( make_command( "u_sell_item", { lit_str( id ), lit_num( static_cast<double>( cost ) ), lit_num( static_cast<double>( count ) ) } ) );
    } else if( jo.has_string( "u_consume_item" ) ) {
        auto id    = jo.get_string( "u_consume_item" );
        auto count = jo.get_int( "count", 1 );
        out.push_back( make_command( "u_consume_item", { lit_str( id ), lit_num( static_cast<double>( count ) ) } ) );
    } else if( jo.has_string( "npc_consume_item" ) ) {
        auto id    = jo.get_string( "npc_consume_item" );
        auto count = jo.get_int( "count", 1 );
        out.push_back( make_command( "npc_consume_item", { lit_str( id ), lit_num( static_cast<double>( count ) ) } ) );
    } else if( jo.has_string( "u_remove_item_with" ) ) {
        out.push_back( make_command( "u_remove_item_with", { lit_str( jo.get_string( "u_remove_item_with" ) ) } ) );
    } else if( jo.has_string( "npc_remove_item_with" ) ) {
        out.push_back( make_command( "npc_remove_item_with", { lit_str( jo.get_string( "npc_remove_item_with" ) ) } ) );
    } else if( jo.has_string( "u_set_first_topic" ) ) {
        out.push_back( make_command( "u_set_first_topic", { lit_str( jo.get_string( "u_set_first_topic" ) ) } ) );
    } else if( jo.has_string( "npc_first_topic" ) ) {
        out.push_back( make_command( "npc_set_first_topic", { lit_str( jo.get_string( "npc_first_topic" ) ) } ) );
    } else if( jo.has_string( "toggle_npc_rule" ) ) {
        out.push_back( make_command( "toggle_npc_rule", { lit_str( jo.get_string( "toggle_npc_rule" ) ) } ) );
    } else if( jo.has_string( "set_npc_rule" ) ) {
        out.push_back( make_command( "set_npc_rule", { lit_str( jo.get_string( "set_npc_rule" ) ) } ) );
    } else if( jo.has_string( "clear_npc_rule" ) ) {
        out.push_back( make_command( "clear_npc_rule", { lit_str( jo.get_string( "clear_npc_rule" ) ) } ) );
    } else if( jo.has_string( "copy_npc_rules" ) ) {
        out.push_back( make_command( "copy_npc_rules", {} ) );
    } else if( jo.has_string( "set_npc_engagement_rule" ) ) {
        out.push_back( make_command( "set_npc_engagement_rule", { lit_str( jo.get_string( "set_npc_engagement_rule" ) ) } ) );
    } else if( jo.has_string( "set_npc_aim_rule" ) ) {
        out.push_back( make_command( "set_npc_aim_rule", { lit_str( jo.get_string( "set_npc_aim_rule" ) ) } ) );
    } else if( jo.has_string( "set_npc_cbm_reserve_rule" ) ) {
        out.push_back( make_command( "set_npc_cbm_reserve_rule", { lit_str( jo.get_string( "set_npc_cbm_reserve_rule" ) ) } ) );
    } else if( jo.has_string( "set_npc_cbm_recharge_rule" ) ) {
        out.push_back( make_command( "set_npc_cbm_recharge_rule", { lit_str( jo.get_string( "set_npc_cbm_recharge_rule" ) ) } ) );
    } else if( jo.has_string( "add_mission" ) ) {
        out.push_back( make_command( "add_mission", { lit_str( jo.get_string( "add_mission" ) ) } ) );
    } else if( jo.has_member( "assign_mission" ) ) {
        out.push_back( make_command( "assign_mission", { lit_str( jo.get_string( "assign_mission" ) ) } ) );
    } else if( jo.has_string( "finish_mission" ) ) {
        auto id      = jo.get_string( "finish_mission" );
        auto success = jo.get_bool( "success", true );
        out.push_back( make_command( "finish_mission", { lit_str( id ), lit( success ) } ) );
    } else if( jo.has_string( "npc_change_faction" ) ) {
        out.push_back( make_command( "npc_change_faction", { lit_str( jo.get_string( "npc_change_faction" ) ) } ) );
    } else if( jo.has_member( "mapgen_update" ) ) {
        // Emit update IDs; runtime runs them at NPC position (simplified targeting).
        std::vector<yarn::expr_node> upd_args;
        if( jo.has_string( "mapgen_update" ) ) {
            upd_args.push_back( lit_str( jo.get_string( "mapgen_update" ) ) );
        } else if( jo.has_array( "mapgen_update" ) ) {
            for( const std::string &s : jo.get_string_array( "mapgen_update" ) ) {
                upd_args.push_back( lit_str( s ) );
            }
        }
        out.push_back( make_command( "mapgen_update", std::move( upd_args ) ) );
    } else if( jo.has_int( "u_faction_rep" ) ) {
        out.push_back( make_command( "u_faction_rep", { lit_num( static_cast<double>( jo.get_int( "u_faction_rep" ) ) ) } ) );
    } else if( jo.has_array( "add_debt" ) ) {
        // Each entry is ["TYPE", factor]. Emit as add_debt("TYPE", factor, ...) variadic.
        std::vector<yarn::expr_node> debt_args;
        for( JsonArray jmod : jo.get_array( "add_debt" ) ) {
            auto type   = jmod.next_string();
            auto factor = jmod.next_int();
            debt_args.push_back( lit_str( type ) );
            debt_args.push_back( lit_num( static_cast<double>( factor ) ) );
        }
        out.push_back( make_command( "add_debt", std::move( debt_args ) ) );
    } else if( jo.has_member( "opinion" ) ) {
        auto opinion_jo = jo.get_object( "opinion" );
        auto op_elems   = opinion_to_elements( opinion_jo );
        out.insert( out.end(), op_elems.begin(), op_elems.end() );
    } else {
        debugmsg( "dialogue_convert: unrecognized sub-effect: %s", jo.str() );
    }

    return out;
}

// Convert the "effect" member + optional "opinion" of a talk_effect JSON to node_elements.
// Does NOT include the "topic" jump — that is handled separately.
auto json_effect_member_to_elements( const JsonObject &jo ) -> std::vector<yarn::node_element>
{
    jo.allow_omitted_members();
    std::vector<yarn::node_element> out;

    // Opinion at the top level of the effect object
    if( jo.has_member( "opinion" ) ) {
        auto opinion_jo = jo.get_object( "opinion" );
        auto op_elems   = opinion_to_elements( opinion_jo );
        out.insert( out.end(), op_elems.begin(), op_elems.end() );
    }

    if( !jo.has_member( "effect" ) ) {
        return out;
    }

    if( jo.has_string( "effect" ) ) {
        out.push_back( make_command( jo.get_string( "effect" ) ) );
    } else if( jo.has_object( "effect" ) ) {
        auto sub = json_sub_effect_to_elements( jo.get_object( "effect" ) );
        out.insert( out.end(), sub.begin(), sub.end() );
    } else if( jo.has_array( "effect" ) ) {
        for( const JsonValue &entry : jo.get_array( "effect" ) ) {
            if( entry.test_string() ) {
                out.push_back( make_command( entry.get_string() ) );
            } else if( entry.test_object() ) {
                auto sub = json_sub_effect_to_elements( entry.get_object() );
                out.insert( out.end(), sub.begin(), sub.end() );
            }
        }
    }

    return out;
}

// ============================================================
// Dynamic line JSON → node_elements
// ============================================================

// Forward declaration for recursive dynamic_line_t conversion.
auto json_dynamic_line_to_elements( const JsonValue &jv ) -> std::vector<yarn::node_element>;

auto json_dynamic_line_obj_to_elements( const JsonObject &jo ) -> std::vector<yarn::node_element>
{
    jo.allow_omitted_members();
    std::vector<yarn::node_element> out;

    // Concatenation: { "and": [...lines...] }
    if( jo.has_array( "and" ) ) {
        // Concatenate lines; simplest conversion: emit each as a separate dialogue line.
        for( const JsonValue &entry : jo.get_array( "and" ) ) {
            auto sub = json_dynamic_line_to_elements( entry );
            out.insert( out.end(), sub.begin(), sub.end() );
        }
        return out;
    }

    // Special lines with no direct yarn equivalent — skip gracefully.
    if( jo.get_bool( "give_hint", false ) ) {
        // Hints are minor; skip with no output.
        return out;
    }
    if( jo.get_bool( "use_reason", false ) ) {
        // "reason" context from dialogue not available in yarn; skip.
        return out;
    }
    if( jo.has_string( "gendered_line" ) ) {
        // Gendered line — use the base string without pronoun substitution.
        out.push_back( make_dialogue( jo.get_string( "gendered_line" ) ) );
        return out;
    }

    // Conditional: { "condition_key": YES_BRANCH, "no": NO_BRANCH }
    // Two forms:
    //   1. { "key": true, "yes": ..., "no": ... }  — explicit yes/no members
    //   2. { "key": VALUE, "no": ... }              — VALUE is the yes branch directly
    //
    // try_cond(expr, yes_key): builds an if_block.
    //   If "yes" member exists, that is the yes branch.
    //   Otherwise, yes_key names the member whose value is the yes branch.
    auto try_cond = [&]( yarn::expr_node cond_expr, const std::string &yes_key = {} ) {
        std::vector<yarn::node_element> yes_elem;
        if( jo.has_member( "yes" ) ) {
            yes_elem = json_dynamic_line_to_elements( jo.get_member( "yes" ) );
        } else if( !yes_key.empty() && jo.has_member( yes_key ) ) {
            yes_elem = json_dynamic_line_to_elements( jo.get_member( yes_key ) );
        }
        auto no_elem = jo.has_member( "no" )
                       ? json_dynamic_line_to_elements( jo.get_member( "no" ) )
                       : std::vector<yarn::node_element> {};
        out.push_back( make_if_block( std::move( cond_expr ),
                                      std::move( yes_elem ),
                                      std::move( no_elem ) ) );
    };

    // Check simple_string_conds.
    // Matches either { "key": true } or { "key": VALUE } where VALUE is the yes branch.
    for( const auto &[key, fn_name] : s_simple_cond_map ) {
        if( jo.has_member( key ) ) {
            try_cond( fn( fn_name ), key );
            return out;
        }
    }

    // Check complex_conds
    {
        yarn::expr_node cond_expr = json_condition_obj_to_expr( jo );
        // If cond was not recognized, it returns lit(true) and we can't do a useful if_block.
        // In that case just emit the "yes" branch unconditionally.
        auto yes_elems = jo.has_member( "yes" )
                         ? json_dynamic_line_to_elements( jo.get_member( "yes" ) )
                         : std::vector<yarn::node_element> {};
        auto no_elems  = jo.has_member( "no" )
                         ? json_dynamic_line_to_elements( jo.get_member( "no" ) )
                         : std::vector<yarn::node_element> {};
        if( !yes_elems.empty() || !no_elems.empty() ) {
            out.push_back( make_if_block( std::move( cond_expr ),
                                          std::move( yes_elems ),
                                          std::move( no_elems ) ) );
            return out;
        }
    }

    // Could not interpret; skip
    debugmsg( "dialogue_convert: unrecognized dynamic_line object: %s", jo.str() );
    return out;
}

auto json_dynamic_line_to_elements( const JsonValue &jv ) -> std::vector<yarn::node_element>
{
    std::vector<yarn::node_element> out;

    if( jv.test_string() ) {
        out.push_back( make_dialogue( jv.get_string() ) );
        return out;
    }

    if( jv.test_array() ) {
        // Random selection from array — register as <<random_line>> command if strings,
        // or just emit the first element as a fallback if they're complex.
        std::vector<std::string> strs;
        bool all_strings = true;
        for( const JsonValue &entry : jv.get_array() ) {
            if( entry.test_string() ) {
                strs.push_back( entry.get_string() );
            } else {
                all_strings = false;
                break;
            }
        }
        if( all_strings && !strs.empty() ) {
            // Emit as dialogue with {random_line("a", "b", ...)} interpolation
            std::string interp = "{random_line(";
            bool first = true;
            for( const auto &s : strs ) {
                if( !first ) { interp += ", "; }
                first = false;
                // Escape quotes in string
                interp += '"';
                for( char c : s ) {
                    if( c == '"' ) { interp += '\\'; }
                    interp += c;
                }
                interp += '"';
            }
            interp += ")}";
            out.push_back( make_dialogue( interp ) );
        } else if( !strs.empty() ) {
            // Complex entries — just use the first string we found (lossy but safe)
            out.push_back( make_dialogue( strs[0] ) );
        } else {
            // All complex; emit first element
            auto jv_arr = jv.get_array();
            if( jv_arr.test_object() ) {
                auto sub = json_dynamic_line_obj_to_elements( jv_arr.next_object() );
                out.insert( out.end(), sub.begin(), sub.end() );
            }
        }
        return out;
    }

    if( jv.test_object() ) {
        return json_dynamic_line_obj_to_elements( jv.get_object() );
    }

    return out;
}

// ============================================================
// Talk effect + topic → node_elements (including the terminal jump/stop/return)
// ============================================================

// Converts the "topic" from a talk_effect to the appropriate terminal element.
// Returns empty if the topic is a regular jump (handled by caller).
auto topic_to_terminal( const std::string &topic_id ) -> std::vector<yarn::node_element>
{
    auto special = special_topic_elements( topic_id );
    if( !special.empty() ) {
        return special;
    }
    if( s_legacy_ui_topics.contains( topic_id ) ) {
        return { make_legacy( topic_id ) };
    }
    return { make_goto( topic_id ) };
}

// Convert a talk_effect JSON object to node_elements + terminal.
// The terminal jump/stop/return is appended.
auto json_talk_effect_to_elements( const JsonObject &jo,
                                   std::vector<yarn::yarn_node> &out_nodes ) -> std::vector<yarn::node_element>;

// Forward declaration for inline topic handling.
auto json_topic_to_yarn_node( const std::string &id,
                              const JsonObject &jo,
                              std::vector<yarn::yarn_node> &out_nodes ) -> yarn::yarn_node;

auto json_talk_effect_to_elements( const JsonObject &jo,
                                   std::vector<yarn::yarn_node> &out_nodes ) -> std::vector<yarn::node_element>
{
    jo.allow_omitted_members();
    auto elements = json_effect_member_to_elements( jo );

    // Opinion at the talk_effect level
    if( jo.has_member( "opinion" ) && !jo.has_object( "effect" ) ) {
        // Already handled inside json_effect_member_to_elements via "opinion" key check
    }

    // Terminal: topic
    if( jo.has_string( "topic" ) ) {
        auto topic_id = jo.get_string( "topic" );
        auto terminal = topic_to_terminal( topic_id );
        elements.insert( elements.end(), terminal.begin(), terminal.end() );
    } else if( jo.has_object( "topic" ) ) {
        // Inline topic definition
        JsonObject inline_jo = jo.get_object( "topic" );
        auto inline_id = inline_jo.get_string( "id", "" );
        if( !inline_id.empty() ) {
            // Recursively convert the inline topic
            auto inline_node = json_topic_to_yarn_node( inline_id, inline_jo, out_nodes );
            out_nodes.push_back( std::move( inline_node ) );
            auto terminal = topic_to_terminal( inline_id );
            elements.insert( elements.end(), terminal.begin(), terminal.end() );
        }
    }

    return elements;
}

// ============================================================
// Speaker effects → command elements
// ============================================================

auto json_speaker_effect_to_elements( const JsonObject &jo ) -> std::vector<yarn::node_element>
{
    jo.allow_omitted_members();
    // Speaker effects have: optional "condition", optional "effect", optional "opinion"
    std::vector<yarn::node_element> body;

    if( jo.has_member( "opinion" ) ) {
        auto opinion_jo = jo.get_object( "opinion" );
        auto op_elems   = opinion_to_elements( opinion_jo );
        body.insert( body.end(), op_elems.begin(), op_elems.end() );
    }
    if( jo.has_member( "effect" ) ) {
        if( jo.has_string( "effect" ) ) {
            body.push_back( make_command( jo.get_string( "effect" ) ) );
        } else if( jo.has_object( "effect" ) ) {
            auto sub = json_sub_effect_to_elements( jo.get_object( "effect" ) );
            body.insert( body.end(), sub.begin(), sub.end() );
        } else if( jo.has_array( "effect" ) ) {
            for( const JsonValue &entry : jo.get_array( "effect" ) ) {
                if( entry.test_string() ) {
                    body.push_back( make_command( entry.get_string() ) );
                } else if( entry.test_object() ) {
                    auto sub = json_sub_effect_to_elements( entry.get_object() );
                    body.insert( body.end(), sub.begin(), sub.end() );
                }
            }
        }
    }

    if( !jo.has_member( "condition" ) ) {
        return body;
    }

    // Wrap in if_block if conditional
    yarn::expr_node cond_expr;
    if( jo.has_string( "condition" ) ) {
        cond_expr = json_condition_to_expr( jo.get_member( "condition" ) );
    } else if( jo.has_object( "condition" ) ) {
        cond_expr = json_condition_obj_to_expr( jo.get_object( "condition" ) );
    } else {
        return body;
    }
    return { make_if_block( std::move( cond_expr ), std::move( body ) ) };
}

// ============================================================
// Response JSON → choice
// ============================================================

// Returns 1 choice normally, or 2 choices for truefalsetext responses (one per text variant,
// with opposite conditions, sharing the same body).
auto json_response_to_choices( const JsonObject &jo,
                               std::vector<yarn::yarn_node> &out_nodes )
-> std::vector<yarn::node_element::choice>;

auto json_response_to_choice( const JsonObject &jo,
                              std::vector<yarn::yarn_node> &out_nodes ) -> yarn::node_element::choice
{
    jo.allow_omitted_members();
    yarn::node_element::choice ch;

    ch.text = jo.get_string( "text", "" );

    // Choice visibility condition
    if( jo.has_member( "condition" ) ) {
        ch.condition = json_condition_to_expr( jo.get_member( "condition" ) );
    }

    // Trial handling
    bool has_trial = jo.has_object( "trial" );
    if( has_trial ) {
        JsonObject trial_jo = jo.get_object( "trial" );
        trial_jo.allow_omitted_members();
        auto trial_type = trial_jo.get_string( "type", "NONE" );
        auto difficulty = trial_jo.get_int( "difficulty", 0 );

        // Success body
        std::vector<yarn::node_element> success_body;
        if( jo.has_object( "success" ) ) {
            success_body = json_talk_effect_to_elements( jo.get_object( "success" ), out_nodes );
        }

        // Failure body
        std::vector<yarn::node_element> failure_body;
        if( jo.has_object( "failure" ) ) {
            failure_body = json_talk_effect_to_elements( jo.get_object( "failure" ), out_nodes );
        }

        if( trial_type == "NONE" ) {
            // No roll — always succeeds
            ch.body = std::move( success_body );
        } else if( trial_type == "CONDITION" ) {
            // Check condition instead of rolling
            yarn::expr_node cond_expr;
            if( trial_jo.has_member( "condition" ) ) {
                cond_expr = json_condition_to_expr( trial_jo.get_member( "condition" ) );
            } else {
                cond_expr = lit( true );
            }
            ch.body = { make_if_block( std::move( cond_expr ),
                                       std::move( success_body ),
                                       std::move( failure_body ) )
                      };
        } else {
            // Dice roll: PERSUADE, LIE, INTIMIDATE
            yarn::expr_node trial_expr = fn( "trial_roll", {
                lit_str( trial_type ),
                lit_num( static_cast<double>( difficulty ) )
            } );
            ch.body = { make_if_block( std::move( trial_expr ),
                                       std::move( success_body ),
                                       std::move( failure_body ) )
                      };
        }
    } else {
        // No trial — single success path
        // Build body from inline success object or flat effect + topic
        if( jo.has_object( "success" ) ) {
            ch.body = json_talk_effect_to_elements( jo.get_object( "success" ), out_nodes );
        } else {
            // Flat format: effect + topic at the response level
            ch.body = json_talk_effect_to_elements( jo, out_nodes );
        }
    }

    return ch;
}

// ============================================================
// truefalsetext responses → two choices with opposite conditions
// ============================================================

auto json_response_to_choices( const JsonObject &jo,
                               std::vector<yarn::yarn_node> &out_nodes )
-> std::vector<yarn::node_element::choice>
{
    if( !jo.has_member( "truefalsetext" ) ) {
        return { json_response_to_choice( jo, out_nodes ) };
    }

    jo.allow_omitted_members();

    JsonObject tf = jo.get_object( "truefalsetext" );
    tf.allow_omitted_members();
    const auto text_true  = tf.get_string( "true",  "" );
    const auto text_false = tf.get_string( "false", "" );

    std::optional<yarn::expr_node> tf_cond;
    if( tf.has_member( "condition" ) ) {
        tf_cond = json_condition_to_expr( tf.get_member( "condition" ) );
    }

    // Build shared body (no trial support in truefalsetext — body is flat)
    std::vector<yarn::node_element> body;
    if( jo.has_object( "success" ) ) {
        body = json_talk_effect_to_elements( jo.get_object( "success" ), out_nodes );
    } else {
        body = json_talk_effect_to_elements( jo, out_nodes );
    }

    // True-text choice: shown when condition passes
    yarn::node_element::choice ch_true;
    ch_true.text = text_true;
    if( tf_cond ) {
        ch_true.condition = *tf_cond;
    }
    ch_true.body = body;

    // False-text choice: shown when condition fails
    yarn::node_element::choice ch_false;
    ch_false.text = text_false;
    if( tf_cond ) {
        ch_false.condition = not1( *tf_cond );
    }
    ch_false.body = std::move( body );

    return { std::move( ch_true ), std::move( ch_false ) };
}

// ============================================================
// Repeat response JSON → repeat_group
// ============================================================

auto json_repeat_response_to_group( const JsonObject &jo,
                                    std::vector<yarn::yarn_node> &out_nodes )
-> yarn::node_element::repeat_group
{
    jo.allow_omitted_members();
    yarn::node_element::repeat_group g;

    g.is_npc              = jo.get_bool( "is_npc", false );
    g.include_containers  = jo.get_bool( "include_containers", false );

    if( jo.has_string( "for_item" ) ) {
        g.for_item.push_back( jo.get_string( "for_item" ) );
    } else if( jo.has_array( "for_item" ) ) {
        g.for_item = jo.get_string_array( "for_item" );
    }
    if( jo.has_string( "for_category" ) ) {
        g.for_category.push_back( jo.get_string( "for_category" ) );
    } else if( jo.has_array( "for_category" ) ) {
        g.for_category = jo.get_string_array( "for_category" );
    }

    if( jo.has_object( "response" ) ) {
        JsonObject resp_jo = jo.get_object( "response" );

        g.text_template = resp_jo.get_string( "text", "" );

        // Condition for the repeat group as a whole
        if( resp_jo.has_member( "condition" ) ) {
            g.condition = json_condition_to_expr( resp_jo.get_member( "condition" ) );
        }

        // Body (effects + jump) for when any repeat choice is selected
        // The body is shared — the selected item type is set in the runtime context.
        if( resp_jo.has_object( "success" ) ) {
            g.body = json_talk_effect_to_elements( resp_jo.get_object( "success" ), out_nodes );
        } else {
            g.body = json_talk_effect_to_elements( resp_jo, out_nodes );
        }
    }

    return g;
}

// ============================================================
// Full topic conversion
// ============================================================

auto json_topic_to_yarn_node( const std::string &id,
                              const JsonObject &jo,
                              std::vector<yarn::yarn_node> &out_nodes ) -> yarn::yarn_node
{
    jo.allow_omitted_members();
    yarn::yarn_node node;
    node.title = id;

    // Speaker effects (pre-dialogue effects from "speaker_effect")
    auto add_speaker_effects = [&]( const JsonObject & ejo, const std::string & ) {
        auto elems = json_speaker_effect_to_elements( ejo );
        node.elements.insert( node.elements.end(), elems.begin(), elems.end() );
    };

    if( jo.has_object( "speaker_effect" ) ) {
        add_speaker_effects( jo.get_object( "speaker_effect" ), id );
    } else if( jo.has_array( "speaker_effect" ) ) {
        for( JsonObject se : jo.get_array( "speaker_effect" ) ) {
            add_speaker_effects( se, id );
        }
    }

    // Dynamic line (NPC speech)
    if( jo.has_member( "dynamic_line" ) ) {
        auto speech_elems = json_dynamic_line_to_elements( jo.get_member( "dynamic_line" ) );
        node.elements.insert( node.elements.end(), speech_elems.begin(), speech_elems.end() );
    }

    // Responses → choice_group
    yarn::node_element cg;
    cg.type = yarn::node_element::kind::choice_group;

    // switch:true responses form mutually exclusive groups: each successive switch choice
    // is only shown when none of the preceding switch choices' conditions passed.
    std::vector<yarn::expr_node> switch_group_conditions;
    bool has_unconditional_done = false;  // tracks whether a bye/done option already exists

    for( JsonObject resp_jo : jo.get_array( "responses" ) ) {
        resp_jo.allow_omitted_members();
        const bool is_switch = resp_jo.get_bool( "switch", false );
        // Track whether any unconditional response already exits the conversation.
        if( !resp_jo.has_member( "condition" ) &&
            !resp_jo.has_member( "truefalsetext" ) &&
            resp_jo.get_string( "topic", "" ) == "TALK_DONE" ) {
            has_unconditional_done = true;
        }

        if( !is_switch ) {
            switch_group_conditions.clear();
        }

        auto choices = json_response_to_choices( resp_jo, out_nodes );

        if( is_switch ) {
            // Track the original (pre-guard) condition for each choice so subsequent
            // switch choices get a guard based on the raw conditions, not compounded ones.
            for( const auto &ch : choices ) {
                switch_group_conditions.push_back(
                    ch.condition ? *ch.condition : lit( true ) );
            }

            if( switch_group_conditions.size() > choices.size() ) {
                // There are previous switch conditions — apply guard to these choices.
                // Guard = NOT( OR of all previous switch conditions ).
                auto prev_end = static_cast<std::ptrdiff_t>(
                                    switch_group_conditions.size() - choices.size() );
                std::vector<yarn::expr_node> prev( switch_group_conditions.begin(),
                                                   switch_group_conditions.begin() + prev_end );
                auto prev_matched = fold_exprs( std::move( prev ), or2 );
                for( auto &ch : choices ) {
                    auto guard = not1( prev_matched );
                    ch.condition = ch.condition
                                   ? std::optional<yarn::expr_node> { and2( std::move( guard ), std::move( *ch.condition ) ) }
                                   :
                                   std::optional<yarn::expr_node> { std::move( guard ) };
                }
            }
        }

        for( auto &ch : choices ) {
            cg.choices.push_back( std::move( ch ) );
        }
    }

    // Repeat responses → repeat_groups
    if( jo.has_object( "repeat_responses" ) ) {
        cg.repeat_groups.push_back(
            json_repeat_response_to_group( jo.get_object( "repeat_responses" ), out_nodes )
        );
    } else if( jo.has_array( "repeat_responses" ) ) {
        for( JsonObject rr_jo : jo.get_array( "repeat_responses" ) ) {
            cg.repeat_groups.push_back(
                json_repeat_response_to_group( rr_jo, out_nodes )
            );
        }
    }

    // "OBEY ME!" — available when player has DEBUG_MIND_CONTROL and NPC is not yet an ally.
    // Mirrors the built-in response gen_responses() used to add for every topic.
    {
        yarn::node_element::choice obey;
        obey.text      = "OBEY ME!";
        obey.condition = and2( fn( "u_has_trait", { lit_str( "DEBUG_MIND_CONTROL" ) } ),
                               not1( fn( "npc_friend", {} ) ) );
        obey.body      = topic_to_terminal( "TALK_MIND_CONTROL" );
        cg.choices.push_back( std::move( obey ) );
    }

    // Fallback "Bye." — mirrors the unconditional exit gen_responses() always appended.
    // Omitted when the JSON already defines an unconditional path to TALK_DONE.
    if( !has_unconditional_done ) {
        yarn::node_element::choice bye;
        bye.text = _( "Bye." );
        bye.body = topic_to_terminal( "TALK_DONE" );
        cg.choices.push_back( std::move( bye ) );
    }

    node.elements.push_back( std::move( cg ) );

    return node;
}

// ============================================================
// Pending node cache
// ============================================================

struct pending_entry {
    std::string             id;
    std::vector<yarn::yarn_node> nodes;  // may include inline topic nodes
};

static std::vector<pending_entry> s_pending;

} // anonymous namespace

namespace dialogue_convert
{

void register_yarn_node( const std::string &id, const JsonObject &jo )
{
    pending_entry entry;
    entry.id = id;

    std::vector<yarn::yarn_node> out_nodes;
    out_nodes.push_back( json_topic_to_yarn_node( id, jo, out_nodes ) );
    entry.nodes = std::move( out_nodes );

    // If the JSON has an array of IDs, the caller will call us once per ID.
    // We just accumulate all nodes.
    s_pending.push_back( std::move( entry ) );
}

} // namespace dialogue_convert

// Called by yarn_dialogue.cpp's build_legacy_yarn_stories() via friend-linkage
// through the pending node flusher below.

namespace dialogue_convert
{

// Merge choices from a secondary node's choice_group into a primary node.
// Only choices that appear BEFORE "OBEY ME!" in the secondary are inserted
// (OBEY ME and Bye. are universal — they already exist in the primary).
static auto merge_choice_groups( yarn::yarn_node &primary,
                                 yarn::yarn_node &secondary ) -> void
{
    // Find the choice_group in each node.
    auto primary_cg_it = std::ranges::find_if( primary.elements, []( const yarn::node_element & e ) {
        return e.type == yarn::node_element::kind::choice_group;
    } );
    auto secondary_cg_it = std::ranges::find_if( secondary.elements, []( const yarn::node_element &
    e ) {
        return e.type == yarn::node_element::kind::choice_group;
    } );

    if( primary_cg_it == primary.elements.end() || secondary_cg_it == secondary.elements.end() ) {
        return;
    }

    // If the primary has no speech/pre-choice elements but the secondary does,
    // prepend secondary's pre-choice elements (dynamic_line, speaker_effects) to primary.
    // This handles load-order dependency: TALK_COMMON_MISSION.json (no dynamic_line) may
    // be loaded before TALK_COMMON_OTHER.json (has dynamic_line), making MISSION the primary.
    const bool primary_has_pre_elements = primary_cg_it != primary.elements.begin();
    if( !primary_has_pre_elements && secondary_cg_it != secondary.elements.begin() ) {
        primary.elements.insert( primary.elements.begin(),
                                 secondary.elements.begin(),
                                 secondary_cg_it );
        // Recompute iterator after insert.
        primary_cg_it = std::ranges::find_if( primary.elements, []( const yarn::node_element & e ) {
            return e.type == yarn::node_element::kind::choice_group;
        } );
    }

    auto &primary_choices   = primary_cg_it->choices;
    auto &secondary_choices = secondary_cg_it->choices;

    // Find "OBEY ME!" in each — choices before it are the JSON-derived ones.
    auto primary_obey_it = std::ranges::find_if( primary_choices, []( const auto & ch ) {
        return ch.text == "OBEY ME!";
    } );
    auto secondary_obey_it = std::ranges::find_if( secondary_choices, []( const auto & ch ) {
        return ch.text == "OBEY ME!";
    } );

    // Check whether secondary contributes any unconditional stop (TALK_DONE).
    // If so, the primary's forced "Bye." fallback becomes redundant.
    const bool secondary_has_unconditional_stop = std::ranges::any_of(
                std::ranges::subrange( secondary_choices.begin(), secondary_obey_it ),
    []( const auto & ch ) {
        return !ch.condition &&
               !ch.body.empty() &&
               ch.body.back().type == yarn::node_element::kind::stop;
    } );

    // Insert secondary's JSON choices before primary's OBEY ME.
    auto insert_pos = primary_obey_it;
    for( auto it = secondary_choices.begin(); it != secondary_obey_it; ++it ) {
        insert_pos = primary_choices.insert( insert_pos, std::move( *it ) );
        ++insert_pos;
    }

    // Remove the forced "Bye." from the primary if secondary provided a real unconditional exit.
    if( secondary_has_unconditional_stop ) {
        std::erase_if( primary_choices, []( const auto & ch ) {
            return !ch.condition &&
                   ch.text == _( "Bye." ) &&
                   !ch.body.empty() &&
                   ch.body.back().type == yarn::node_element::kind::stop;
        } );
    }
}

auto flush_pending_nodes() -> std::vector<yarn::yarn_node>
{
    // Merge duplicate titles: topics defined across multiple JSON files
    // (e.g. TALK_SHELTER in TALK_COMMON_OTHER.json + TALK_COMMON_MISSION.json)
    // must have their choice_groups combined, mirroring json_talk_topic::load's append semantics.
    std::unordered_map<std::string, yarn::yarn_node> merged;
    std::vector<std::string> insertion_order; // preserve first-seen title order

    for( auto &entry : s_pending ) {
        for( auto &node : entry.nodes ) {
            if( s_legacy_ui_topics.contains( node.title ) ) {
                if( !merged.contains( node.title ) ) {
                    yarn::yarn_node stub;
                    stub.title = node.title;
                    yarn::node_element lt;
                    lt.type        = yarn::node_element::kind::legacy_topic;
                    lt.jump_target = node.title;
                    stub.elements.push_back( std::move( lt ) );
                    insertion_order.push_back( node.title );
                    merged.emplace( node.title, std::move( stub ) );
                }
            } else {
                auto it = merged.find( node.title );
                if( it == merged.end() ) {
                    insertion_order.push_back( node.title );
                    merged.emplace( node.title, std::move( node ) );
                } else {
                    // Duplicate title: merge this node's JSON choices into the existing one.
                    merge_choice_groups( it->second, node );
                }
            }
        }
    }

    // Add stubs for legacy UI topics that have no JSON entries at all
    // (e.g. TALK_MIND_CONTROL is handled entirely by C++ dynamic_line/gen_responses).
    // Without a stub, the while-loop in the legacy_topic handler cannot route to them.
    for( const auto &title : s_legacy_ui_topics ) {
        if( !merged.contains( title ) ) {
            yarn::yarn_node stub;
            stub.title = title;
            yarn::node_element lt;
            lt.type        = yarn::node_element::kind::legacy_topic;
            lt.jump_target = title;
            stub.elements.push_back( std::move( lt ) );
            merged.emplace( title, std::move( stub ) );
            insertion_order.push_back( title );
        }
    }

    std::vector<yarn::yarn_node> all;
    all.reserve( insertion_order.size() );
    for( const auto &title : insertion_order ) {
        auto it = merged.find( title );
        if( it != merged.end() ) {
            all.push_back( std::move( it->second ) );
        }
    }

    s_pending.clear();
    return all;
}

} // namespace dialogue_convert
