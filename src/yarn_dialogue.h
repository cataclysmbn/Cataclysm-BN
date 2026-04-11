#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

class npc;
class player;
class dialogue_window;
struct dialogue;

namespace yarn
{

// ============================================================
// Value system
// ============================================================

// The three types a yarn expression can evaluate to.
using value = std::variant<bool, double, std::string>;

enum class value_type : uint8_t { boolean, number, string };

auto type_of( const value &v ) -> value_type;
auto type_name( value_type t ) -> std::string_view;

// ============================================================
// Expression AST
// ============================================================
//
// Conditions in <<if>> blocks and choices, plus {expr} text
// interpolation, all share this evaluator.
//
// Grammar (all expressions ultimately evaluate to a typed value;
// conditions require the result to be boolean):
//
//   expr       := or
//   or         := and ('or' and)*
//   and        := not ('and' not)*
//   not        := 'not' not | comparison
//   comparison := sum (('<'|'<='|'>'|'>='|'=='|'!=') sum)?
//   sum        := product (('+' | '-') product)*
//   product    := unary (('*' | '/') unary)*
//   unary      := '-' unary | primary
//   primary    := '(' expr ')' | func_call | string | number | 'true' | 'false'
//   func_call  := name '(' [expr (',' expr)*] ')'

struct expr_node {
    enum class kind : uint8_t {
        literal,
        func_call,
        binary_op,
        unary_op,
    };

    // Arithmetic and logical binary operators.
    enum class bin_op : uint8_t {
        add, sub, mul, div,
        eq, neq, lt, lte, gt, gte,
        logical_and, logical_or,
    };

    // Arithmetic negation and logical negation.
    enum class un_op : uint8_t {
        negate,
        logical_not,
    };

    kind type = kind::literal;

    // kind::literal
    value literal_val = false;

    // kind::func_call
    std::string func_name;
    std::vector<expr_node> args;  // recursive via std::vector (legal in C++17+)

    // kind::binary_op
    // children[0] = left operand, children[1] = right operand
    bin_op binary_operation = bin_op::add;
    std::vector<expr_node> children;

    // kind::unary_op
    // children[0] = operand
    un_op unary_operation = un_op::negate;
};

// ============================================================
// Function registry
// ============================================================
//
// All callables available to expressions are registered here with
// their parameter and return types. Unknown function names and
// type mismatches are caught at .yarn parse time, not at runtime.
//
// Mods register additional functions via:
//   game.dialogue.register_function(name, param_types, return_type, fn)

struct func_signature {
    std::string name;
    std::vector<value_type> param_types;
    value_type return_type;
    // If true, args beyond param_types are accepted without type validation.
    bool variadic = false;
    // Receives fully-evaluated argument values, returns result.
    std::function<value( const std::vector<value> & )> impl;
};

class func_registry
{
    public:
        void register_func( func_signature sig );

        // Convenience form: avoids explicit func_signature struct construction.
        // params is an initializer_list so callers can write {vt::string, vt::number}.
        void add( std::string name,
                  std::initializer_list<value_type> params,
                  value_type ret,
                  std::function<value( const std::vector<value> & )> impl );

        auto has_func( const std::string &name ) const -> bool;
        auto get_func( const std::string &name ) const -> const func_signature &;
        auto call( const std::string &name, const std::vector<value> &args ) const -> value;

        // The game-global registry. Built-in functions are registered at
        // startup via register_builtin_functions(). Mod Lua scripts add
        // to it at mod-load time.
        static auto global() -> func_registry &;

    private:
        std::unordered_map<std::string, func_signature> funcs_;
};

// ============================================================
// Command registry
// ============================================================
//
// Commands are effects fired by <<command_name arg1 arg2 ...>> lines.
// Arguments are pre-evaluated values (string, number, or bool).
// min_args / max_args are checked at call time; use -1 for no upper limit.
//
// Commands that must end the conversation (e.g. activity-assigning commands)
// return command_signal::stop so the runtime clears the node stack immediately
// without requiring the author to write <<stop>> afterward.

enum class command_signal : uint8_t {
    none,  // continue normally after this command
    stop,  // clear the node stack and end the conversation
};

class command_registry
{
    public:
        // impl must return command_signal::none to continue or command_signal::stop to end.
        using impl_fn = std::function<command_signal( const std::vector<value> & )>;

        void add( std::string name, int min_args, int max_args, impl_fn impl );
        void add( std::string name, int arg_count, impl_fn impl );
        void add( std::string name, impl_fn impl );

        auto has_command( const std::string &name ) const -> bool;
        // Executes the command and returns its signal.
        auto call( const std::string &name, const std::vector<value> &args ) const -> command_signal;

        static auto global() -> command_registry &;

    private:
        struct entry {
            int min_args = 0;
            int max_args = 0;  // -1 = no upper limit
            impl_fn impl;
        };
        std::unordered_map<std::string, entry> cmds_;
};

// ============================================================
// Expression parser and evaluator
// ============================================================

struct parse_error {
    std::string message;
    int column = -1;  // 0-based; -1 if unknown
};

// Parses a condition or expression string into an AST.
// Validates function names and argument types against the registry.
// Returns the AST or the first parse error encountered.
auto parse_expr( std::string_view source, const func_registry &registry )
-> std::variant<expr_node, parse_error>;

// Evaluates a parsed expression tree. The registry is used to call
// registered functions. Throws std::runtime_error on type errors.
auto evaluate_expr( const expr_node &node, const func_registry &registry ) -> value;

// Parses and resolves {expr} interpolation within a dialogue text string.
// Unknown function names produce a visible error marker in the output
// rather than a crash, so broken text doesn't silently abort conversations.
auto interpolate_text( std::string_view text, const func_registry &registry ) -> std::string;

// ============================================================
// Yarn story AST
// ============================================================
//
// node_element uses a tagged-struct layout. Fields not relevant to a
// given kind are left default-constructed and should not be read.
// std::vector<node_element> children are legal recursive containers
// per C++17 (vector<T> permits incomplete T at declaration).

struct node_element {
    enum class kind : uint16_t {
        dialogue,      // NPC or player speech line
        line_group,    // a set of => lines that are picked at random
        choice_group,  // a set of -> choices presented to the player
        command,       // <<command_name arg1 arg2 ...>>
        jump,          // <<jump NodeName>>  — replace current frame (standard Yarn Spinner navigate; no implicit return)
        goto_node,     // <<detour NodeName>>  — push frame; callee falls off end → pop (return to caller)
        stop,          // <<stop>>           — end conversation (clear entire stack)
        yarn_return,   // <<return>>         — explicit early pop of current frame
        if_block,      // <<if>> / <<else>> / <<endif>>
        legacy_topic,  // DEPRECATED: wraps a JSON TALK_TOPIC; removed when migration is complete
    };

    kind type = kind::dialogue;

    // kind::dialogue
    // speaker is empty for unattributed lines.
    // text may contain {expr} interpolation sequences.
    std::string speaker;
    std::string text;

    // kind::command
    // Arguments are parsed as expressions and evaluated at dispatch time,
    // so literals, numbers, and function calls all work uniformly.
    std::string command_name;
    std::vector<expr_node> command_args;

    // kind::jump / kind::goto_node / kind::legacy_topic
    std::string jump_target;

    // kind::if_block
    // condition is present when type == if_block.
    // else_body may be empty if there is no <<else>>.
    std::optional<expr_node> condition;
    std::vector<node_element> if_body;
    std::vector<node_element> else_body;

    // kind::choice_group
    struct choice {
        std::string text;  // display text; may contain {expr} interpolation
        std::optional<expr_node> condition;  // absent = always shown
        std::vector<node_element> body;
        // If true, the choice label is echoed to history as player speech when selected.
        // Default is false — the label is a UI-only button, not speech.
        // Add #spoken to the choice line to echo it.  Use You: in the body for
        // explicit player speech with different wording than the button label.
        bool echo_speech = false;
        // If true, this choice is sorted to the end of the menu after all non-tail
        // choices, regardless of where it was authored or injected.  Add #tail to
        // the choice line.  Relative order among tail choices is preserved.
        bool tail = false;
    };
    std::vector<choice> choices;

    // kind::choice_group (optional) — dynamic choices from inventory iteration.
    // Generated choices are prepended to choices at runtime.
    struct repeat_group {
        bool is_npc = false;
        bool include_containers = false;
        std::vector<std::string> for_item;      // itype_id strings
        std::vector<std::string> for_category;  // item_category_id strings
        std::optional<expr_node> condition;     // group-level filter; if absent, group always runs
        std::string text_template;              // <topic_item> is replaced with item name
        std::vector<node_element> body;         // executed when a repeat choice is selected
    };
    std::vector<repeat_group> repeat_groups;
};

struct yarn_node {
    std::string title;
    std::vector<std::string> tags;
    std::vector<node_element> elements;

    // "shared_choices: NodeA story::NodeB" — pull: append named nodes' choices to this node.
    // Resolved at load time via yarn_story::resolve_shared_choices().
    std::vector<std::string> shared_choices;

    // inject_into: each entry is one OR-group of targets for this injection source.
    // Multiple inject_into: header lines accumulate as OR entries.
    // Direct entries (is_direct == true) bypass inject_block on the target.
    // Tag-based entries match nodes whose inject_tags contain all required_tags (AND).
    struct inject_target {
        bool is_direct = false;
        // Direct injection: identifies a specific node.
        std::string target_story;  // empty means same story as source
        std::string target_node;
        // Tag-based injection: all tags must be present on the target (AND within this entry).
        std::vector<std::string> required_tags;
    };
    std::vector<inject_target> inject_into;

    // Tags this node exposes so tag-based injection sources can target it.
    std::vector<std::string> inject_tags;

    // Category tags for this injection source; checked against target's inject_block.
    // inject_into: #inject_category expands to these tags as a targeting AND-group.
    std::vector<std::string> inject_category;

    // Rejects tag-based injections whose inject_category intersects this list.
    // Direct injection (inject_into: story::Node) ignores this field.
    std::vector<std::string> inject_block;

    // Controls where injected choices land relative to the target's native choices.
    // priority < 0  → prepend (most-negative first)
    // priority >= 0 → append  (lowest first); default is 0
    int inject_priority = 0;
};

// ============================================================
// yarn_story — parsed story, ready for the runtime
// ============================================================

class yarn_story
{
    public:
        struct load_result {
            std::vector<std::string> errors;    // fatal — structural failures; story not loaded
            std::vector<std::string> warnings;  // non-fatal — bad exprs/args; story still loads
            auto ok() const -> bool { return errors.empty(); }
        };

        // Load from a file path. Source name is used in error messages.
        static auto from_file( const std::string &path )
        -> std::pair<yarn_story, load_result>;

        // Load from an in-memory string (useful for tests and mods that
        // ship .yarn content directly).
        static auto from_string( std::string_view content, std::string_view source_name )
        -> std::pair<yarn_story, load_result>;

        auto has_node( const std::string &name ) const -> bool;
        auto get_node( const std::string &name ) const -> const yarn_node &;
        auto all_nodes() const -> const std::unordered_map<std::string, yarn_node> &;

        // Insert or replace a node by title. Used by build_legacy_yarn_stories().
        void add_node( yarn_node node );

        // Returns a mutable pointer to the named node, or nullptr if not found.
        // Used by apply_injections() to modify target nodes.
        auto get_node_mutable( const std::string &name ) -> yarn_node *;

        // Resolve shared_choices references after all stories are loaded.
        // lookup_fn(story_name, node_name) returns the node or nullptr if not found.
        // Handles both same-file bare names and cross-file "story::NodeName" references.
        // Errors are appended to out_errors.
        using node_lookup_fn = std::function<const yarn_node *( const std::string &story,
                               const std::string &node )>;
        void resolve_shared_choices( const node_lookup_fn &lookup, const std::string &own_name,
                                     std::vector<std::string> &out_errors );

    private:
        std::unordered_map<std::string, yarn_node> nodes_;
        std::string source_name_;
};

// ============================================================
// Runtime
// ============================================================
//
// Navigation semantics:
//   <<jump X>>   Replace the current frame with X (standard Yarn Spinner behavior).
//                Creates graph edges in the VS Code Yarn Spinner extension.
//                Use for lateral navigation between peer topics.
//   <<detour X>> Push X onto the node stack. When X falls off its end, pop back to caller.
//                Use for sub-dialogs that must return to the calling node.
//                Standard Yarn Spinner behavior; VS Code extension draws detour edges.
//   <<return>>   Explicit early pop of the current frame. Returns to caller.
//   <<stop>>     Clear the entire node stack. Ends the conversation.

class yarn_runtime
{
    public:
        struct options {
            const yarn_story &story;
            const func_registry &registry;
            std::string starting_node;
            npc *npc_ref = nullptr;
            player *player_ref = nullptr;
            // DEPRECATED: Only used by the legacy JSON dialogue shim.
            // Remove this field once JSON-to-Yarn migration is complete.
            dialogue *dialogue_ref = nullptr;
        };

        explicit yarn_runtime( options opts );

        // Drive the full conversation to completion.
        // Reads input and updates d_win directly.
        void run( dialogue_window &d_win );

    private:
        enum class signal : uint8_t { ok, jump, goto_node, stop, yarn_return };

        struct exec_result {
            signal kind = signal::ok;
            std::string target;  // node name for jump / goto_node
        };

        void parse_dialogue_text( const node_element &elem, dialogue_window &d_win );

        auto execute_elements( const std::vector<node_element> &elements,
                               dialogue_window &d_win ) -> exec_result;

        // Present a choice_group to the player. Returns the index of the
        // chosen option within the filtered (condition-passing) choices.
        // Drives d_win input loop directly (mirrors dialogue::opt() input).
        auto present_choices( const std::vector<node_element::choice> &choices,
                              dialogue_window &d_win ) -> int;

        auto eval( const expr_node &node ) const -> value;
        auto interpolate( std::string_view text ) const -> std::string;

        // Each frame carries its own story pointer so <<detour story::X>> and
        // <<jump story::X>> can cross story boundaries without changing context
        // for the rest of the stack.
        struct stack_frame {
            const yarn_story *story;
            std::string node;
        };
        std::vector<stack_frame> node_stack_;
        const yarn_story &story_;  // starting story; used only for the initial push
        const func_registry &registry_;
        npc *npc_;
        player *player_;
        // DEPRECATED: Only used by the legacy JSON dialogue shim. Remove when migration complete.
        dialogue *dialogue_ref_ = nullptr;
};

// ============================================================
// Global story registry and game integration
// ============================================================

// ============================================================
// Dynamic choice registry
// ============================================================
//
// Lua mods register generators here to inject choices into a named node at
// render time. Multiple generators on the same node are all called; results
// are concatenated in registration order.
//
// Generator return contract:
//   body() return value:
//     ""       = continue after the choice group (one-shot / nil equivalent)
//     "stop"   = end the conversation
//     "loop"   = re-present this choice menu
//     other    = <<jump>> to the named Yarn node

class dynamic_choice_registry
{
    public:
        struct entry {
            std::string text;
            std::function<std::string()> body;
            bool tail = false;
        };
        using generator_fn = std::function<std::vector<entry>()>;

        void register_generator( const std::string &node_name, generator_fn fn );
        auto generate( const std::string &node_name ) const -> std::vector<entry>;

        static auto global() -> dynamic_choice_registry &;

    private:
        std::unordered_map<std::string, std::vector<generator_fn>> generators_;
};

// Scan data/dialogue/*.yarn and load all stories.
// Called during game init alongside other data loading.
void load_yarn_stories();

// Build the __legacy yarn_story from all loaded JSON TALK_TOPICs.
// Must be called after all JSON talk topics are loaded (i.e. after load_yarn_stories()).
// DEPRECATED: Remove when JSON-to-Yarn migration is complete.
void build_legacy_yarn_stories();

// Called by dialogue_json_convert to cache a converted yarn node.
// The node is added to the __legacy story during build_legacy_yarn_stories().
void add_pending_legacy_node( std::string id, yarn_node node );

auto has_yarn_story( const std::string &name ) -> bool;
auto get_yarn_story( const std::string &name ) -> const yarn_story &;

// Register all built-in game predicates and functions (has_trait, get_str,
// etc.) into the given registry. Called once on the global registry at startup.
void register_builtin_functions( func_registry &registry );

// Register all built-in effect commands (give_item, add_effect, npc_follow,
// etc.) into the given registry. Called once on the global registry at startup.
void register_builtin_commands( command_registry &registry );

// Primary dialogue entry point called from npc::talk_to_u().
// If the NPC has an assigned yarn_story, runs it and returns true.
// Otherwise returns false so the caller can try the legacy JSON path.
auto run_npc_dialogue( dialogue_window &d_win, npc &n, player &p ) -> bool;

// DEPRECATED: Run the __legacy yarn story built from JSON TALK_TOPICs.
// Called via dialogue_compat::try_legacy_dialogue. Remove when migration is complete.
auto try_legacy_yarn_dialogue( dialogue_window &d_win, npc &n, player &p, dialogue &d ) -> bool;

} // namespace yarn
