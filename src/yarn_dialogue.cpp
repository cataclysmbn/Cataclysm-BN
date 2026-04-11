// yarn_dialogue.cpp — Yarn Spinner dialogue runtime for Cataclysm: Bright Nights

#include "yarn_dialogue.h"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <cctype>
#include <charconv>
#include <cmath>
#include <functional>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "avatar.h"
#include "calendar.h"
#include "dialogue_json_convert.h"
#include "color.h"
#include "cursesdef.h"
#include "debug.h"
#include "dialogue.h"
#include "dialogue_win.h"
#include "game.h"
#include "map.h"
#include "mapdata.h"
#include "mapgen_functions.h"
#include "mission.h"
#include "overmapbuffer.h"
#include "filesystem.h"
#include "input.h"
#include "character_effects.h"
#include "detached_ptr.h"
#include "enum_conversions.h"
#include "item.h"
#include "item_category.h"
#include "itype.h"
#include "npc.h"
#include "visitable.h"
#include "npctalk.h"
#include "overmap.h"
#include "overmapbuffer_registry.h"
#include "effect.h"
#include "messages.h"
#include "monster.h"
#include "npctrade.h"
#include "output.h"
#include "path_info.h"
#include "player.h"
#include "recipe_dictionary.h"
#include "rng.h"
#include "skill.h"
#include "translations.h"
#include "ui_manager.h"
#include "vehicle.h"
#include "vpart_position.h"

namespace yarn
{

// Static IDs used by built-in functions/commands.
static const trait_id    trait_DEBUG_MIND_CONTROL( "DEBUG_MIND_CONTROL" );
static const skill_id    skill_speech( "speech" );
static const efftype_id  effect_currently_busy( "currently_busy" );

// ============================================================
// Value system
// ============================================================

auto type_of( const value &v ) -> value_type
{
    return std::visit( []<typename T>( const T & ) -> value_type {
        if constexpr( std::is_same_v<T, bool> )
        {
            return value_type::boolean;
        } else if constexpr( std::is_same_v<T, double> )
        {
            return value_type::number;
        } else
        {
            return value_type::string;
        }
    }, v );
}

auto type_name( value_type t ) -> std::string_view
{
    switch( t ) {
        case value_type::boolean:
            return "bool";
        case value_type::number:
            return "number";
        case value_type::string:
            return "string";
    }
    return "unknown";
}

// ============================================================
// func_registry
// ============================================================

void func_registry::register_func( func_signature sig )
{
    funcs_.emplace( sig.name, std::move( sig ) );
}

void func_registry::add( std::string name,
                         std::initializer_list<value_type> params,
                         value_type ret,
                         std::function<value( const std::vector<value> & )> impl )
{
    register_func( { .name = std::move( name ), .param_types = std::vector<value_type>( params ),
                     .return_type = ret, .impl = std::move( impl ) } );
}

auto func_registry::has_func( const std::string &name ) const -> bool
{
    return funcs_.contains( name );
}

auto func_registry::get_func( const std::string &name ) const -> const func_signature &
{
    return funcs_.at( name );
}

auto func_registry::call( const std::string &name, const std::vector<value> &args ) const -> value
{
    return funcs_.at( name ).impl( args );
}

auto func_registry::global() -> func_registry &
{
    static func_registry instance;
    return instance;
}

// ============================================================
// command_registry
// ============================================================

void command_registry::add( std::string name, int min_args, int max_args, impl_fn impl )
{
    cmds_.emplace( name, entry{ min_args, max_args, std::move( impl ) } );
}

void command_registry::add( std::string name, int arg_count, impl_fn impl )
{
    add( std::move( name ), arg_count, arg_count, std::move( impl ) );
}

void command_registry::add( std::string name, impl_fn impl )
{
    add( std::move( name ), 0, 0, std::move( impl ) );
}


auto command_registry::has_command( const std::string &name ) const -> bool
{
    return cmds_.contains( name );
}

auto command_registry::call( const std::string &name, const std::vector<value> &args ) const
-> command_signal
{
    const auto &e = cmds_.at( name );
    auto count = static_cast<int>( args.size() );
    if( count < e.min_args || ( e.max_args != -1 && count > e.max_args ) ) {
        DebugLog( DL::Warn, DC::Main )
                << "yarn: command '" << name << "' called with " << count
                << " args (expected " << e.min_args
                << ( e.max_args == -1 ? "+" : "-" + std::to_string( e.max_args ) ) << ")";
        return command_signal::none;
    }
    return e.impl( args );
}

auto command_registry::global() -> command_registry &
{
    static command_registry instance;
    return instance;
}

// ============================================================
// Expression lexer and parser (anonymous namespace)
// ============================================================

namespace
{

enum class token_type : uint8_t {
    string_lit, number_lit, ident,
    lparen, rparen, comma,
    plus, minus, star, slash,
    eq, neq, lt, lte, gt, gte,
    end_of_input,
    lex_error,
};

struct token {
    token_type type;
    std::string text;
    double num_val = 0.0;
    int col = 0;
};

// Simple single-pass lexer over an expression string.
struct expr_lexer {
    std::string_view src;
    int pos = 0;

    auto at_end() const -> bool { return pos >= static_cast<int>( src.size() ); }
    auto cur() const -> char { return at_end() ? '\0' : src[pos]; }
    auto peek( int offset = 1 ) const -> char {
        auto i = pos + offset;
        return i < static_cast<int>( src.size() ) ? src[i] : '\0';
    }

    void skip_ws() {
        while( !at_end() && static_cast<unsigned char>( cur() ) <= ' ' ) {
            ++pos;
        }
    }

    auto next() -> token {
        skip_ws();
        if( at_end() ) {
            return { token_type::end_of_input, "", 0.0, pos };
        }

        int col = pos;
        char c = cur();

        // Quoted string
        if( c == '"' ) {
            ++pos;
            std::string s;
            while( !at_end() && cur() != '"' ) {
                if( cur() == '\\' && peek() != '\0' ) {
                    ++pos;
                    switch( cur() ) {
                        case '"':
                            s += '"';
                            break;
                        case '\\':
                            s += '\\';
                            break;
                        case 'n':
                            s += '\n';
                            break;
                        default:
                            s += cur();
                            break;
                    }
                } else {
                    s += cur();
                }
                ++pos;
            }
            if( !at_end() ) {
                ++pos; // consume closing "
            }
            return { token_type::string_lit, s, 0.0, col };
        }

        // Number (positive only; unary minus handled in parser)
        if( std::isdigit( static_cast<unsigned char>( c ) ) ) {
            std::string s;
            while( !at_end() && ( std::isdigit( static_cast<unsigned char>( cur() ) )
                                  || cur() == '.' ) ) {
                s += cur();
                ++pos;
            }
            return { token_type::number_lit, s, std::stod( s ), col };
        }

        // Identifier or keyword
        if( std::isalpha( static_cast<unsigned char>( c ) ) || c == '_' ) {
            std::string s;
            while( !at_end() && ( std::isalnum( static_cast<unsigned char>( cur() ) )
                                  || cur() == '_' ) ) {
                s += cur();
                ++pos;
            }
            return { token_type::ident, s, 0.0, col };
        }

        ++pos;
        switch( c ) {
            case '(':
                return { token_type::lparen,  "(", 0.0, col };
            case ')':
                return { token_type::rparen,  ")", 0.0, col };
            case ',':
                return { token_type::comma,   ",", 0.0, col };
            case '+':
                return { token_type::plus,    "+", 0.0, col };
            case '-':
                return { token_type::minus,   "-", 0.0, col };
            case '*':
                return { token_type::star,    "*", 0.0, col };
            case '/':
                return { token_type::slash,   "/", 0.0, col };
            case '<':
                if( cur() == '=' ) { ++pos; return { token_type::lte, "<=", 0.0, col }; }
                return { token_type::lt, "<", 0.0, col };
            case '>':
                if( cur() == '=' ) { ++pos; return { token_type::gte, ">=", 0.0, col }; }
                return { token_type::gt, ">", 0.0, col };
            case '=':
                if( cur() == '=' ) { ++pos; return { token_type::eq, "==", 0.0, col }; }
                return { token_type::lex_error, "=", 0.0, col };
            case '!':
                if( cur() == '=' ) { ++pos; return { token_type::neq, "!=", 0.0, col }; }
                return { token_type::lex_error, "!", 0.0, col };
            default:
                return { token_type::lex_error, std::string( 1, c ), 0.0, col };
        }
    }
};

// Recursive descent expression parser.
// All parse_* methods consume tokens and return an expr_node.
// On error, error_ is set and subsequent calls return default nodes.
struct expr_parser_state {
    std::vector<token> tokens;
    int pos = 0;
    const func_registry &reg;
    std::optional<parse_error> error_;

    auto cur() const -> const token & { return tokens[pos]; } // *NOPAD*

    void advance() {
        if( pos + 1 < static_cast<int>( tokens.size() ) ) {
            ++pos;
        }
    }

    auto ok() const -> bool { return !error_.has_value(); }

    auto fail( std::string msg ) -> expr_node {
        if( !error_ ) {
            error_ = parse_error{ std::move( msg ), cur().col };
        }
        return expr_node{};
    }

    auto expect_type( token_type t, std::string_view desc ) -> bool {
        if( cur().type != t ) {
            fail( std::string( "expected " ) + std::string( desc ) + ", got '" + cur().text + "'" );
            return false;
        }
        advance();
        return true;
    }

    // expr = or
    auto parse_expr() -> expr_node { return parse_or(); }

    // or = and ('or' and)*
    auto parse_or() -> expr_node {
        auto left = parse_and();
        while( ok() && cur().type == token_type::ident && cur().text == "or" ) {
            advance();
            auto right = parse_and();
            expr_node node;
            node.type = expr_node::kind::binary_op;
            node.binary_operation = expr_node::bin_op::logical_or;
            node.children = { std::move( left ), std::move( right ) };
            left = std::move( node );
        }
        return left;
    }

    // and = not ('and' not)*
    auto parse_and() -> expr_node {
        auto left = parse_not();
        while( ok() && cur().type == token_type::ident && cur().text == "and" ) {
            advance();
            auto right = parse_not();
            expr_node node;
            node.type = expr_node::kind::binary_op;
            node.binary_operation = expr_node::bin_op::logical_and;
            node.children = { std::move( left ), std::move( right ) };
            left = std::move( node );
        }
        return left;
    }

    // not = 'not' not | comparison
    auto parse_not() -> expr_node {
        if( ok() && cur().type == token_type::ident && cur().text == "not" ) {
            advance();
            auto operand = parse_not();
            expr_node node;
            node.type = expr_node::kind::unary_op;
            node.unary_operation = expr_node::un_op::logical_not;
            node.children = { std::move( operand ) };
            return node;
        }
        return parse_comparison();
    }

    // comparison = sum (('<'|'<='|'>'|'>='|'=='|'!=') sum)?
    auto parse_comparison() -> expr_node {
        auto left = parse_sum();
        if( !ok() ) {
            return left;
        }

        std::optional<expr_node::bin_op> op;
        switch( cur().type ) {
            case token_type::lt:
                op = expr_node::bin_op::lt;
                break;
            case token_type::lte:
                op = expr_node::bin_op::lte;
                break;
            case token_type::gt:
                op = expr_node::bin_op::gt;
                break;
            case token_type::gte:
                op = expr_node::bin_op::gte;
                break;
            case token_type::eq:
                op = expr_node::bin_op::eq;
                break;
            case token_type::neq:
                op = expr_node::bin_op::neq;
                break;
            default:
                break;
        }
        if( !op ) {
            return left;
        }

        advance();
        auto right = parse_sum();
        expr_node node;
        node.type = expr_node::kind::binary_op;
        node.binary_operation = *op;
        node.children = { std::move( left ), std::move( right ) };
        return node;
    }

    // sum = product (('+' | '-') product)*
    auto parse_sum() -> expr_node {
        auto left = parse_product();
        while( ok() && ( cur().type == token_type::plus || cur().type == token_type::minus ) ) {
            auto op = cur().type == token_type::plus ? expr_node::bin_op::add : expr_node::bin_op::sub;
            advance();
            auto right = parse_product();
            expr_node node;
            node.type = expr_node::kind::binary_op;
            node.binary_operation = op;
            node.children = { std::move( left ), std::move( right ) };
            left = std::move( node );
        }
        return left;
    }

    // product = unary (('*' | '/') unary)*
    auto parse_product() -> expr_node {
        auto left = parse_unary();
        while( ok() && ( cur().type == token_type::star || cur().type == token_type::slash ) ) {
            auto op = cur().type == token_type::star ? expr_node::bin_op::mul : expr_node::bin_op::div;
            advance();
            auto right = parse_unary();
            expr_node node;
            node.type = expr_node::kind::binary_op;
            node.binary_operation = op;
            node.children = { std::move( left ), std::move( right ) };
            left = std::move( node );
        }
        return left;
    }

    // unary = '-' unary | primary
    auto parse_unary() -> expr_node {
        if( ok() && cur().type == token_type::minus ) {
            advance();
            auto operand = parse_unary();
            expr_node node;
            node.type = expr_node::kind::unary_op;
            node.unary_operation = expr_node::un_op::negate;
            node.children = { std::move( operand ) };
            return node;
        }
        return parse_primary();
    }

    // primary = '(' expr ')' | func_call | string | number | 'true' | 'false'
    auto parse_primary() -> expr_node {
        if( !ok() ) {
            return expr_node{};
        }

        if( cur().type == token_type::lparen ) {
            advance();
            auto inner = parse_expr();
            expect_type( token_type::rparen, "')'" );
            return inner;
        }

        if( cur().type == token_type::string_lit ) {
            expr_node node;
            node.type = expr_node::kind::literal;
            node.literal_val = cur().text;
            advance();
            return node;
        }

        if( cur().type == token_type::number_lit ) {
            expr_node node;
            node.type = expr_node::kind::literal;
            node.literal_val = cur().num_val;
            advance();
            return node;
        }

        if( cur().type == token_type::ident ) {
            auto name = cur().text;

            // Boolean literals
            if( name == "true" ) {
                advance();
                expr_node node;
                node.type = expr_node::kind::literal;
                node.literal_val = true;
                return node;
            }
            if( name == "false" ) {
                advance();
                expr_node node;
                node.type = expr_node::kind::literal;
                node.literal_val = false;
                return node;
            }

            advance();

            // Must be a function call
            if( cur().type != token_type::lparen ) {
                return fail( "expected '(' after function name '" + name +
                             "' — bare identifiers are not valid" );
            }
            advance(); // consume '('

            // Validate the function exists
            if( !reg.has_func( name ) ) {
                return fail( "unknown function '" + name + "'" );
            }
            const auto &sig = reg.get_func( name );

            // Parse arguments
            std::vector<expr_node> args;
            if( cur().type != token_type::rparen ) {
                args.push_back( parse_expr() );
                while( ok() && cur().type == token_type::comma ) {
                    advance();
                    args.push_back( parse_expr() );
                }
            }
            if( !expect_type( token_type::rparen, "')'" ) ) {
                return expr_node{};
            }

            // Validate argument count
            const bool too_few  = args.size() < sig.param_types.size();
            const bool too_many = !sig.variadic && args.size() > sig.param_types.size();
            if( too_few || too_many ) {
                return fail( "function '" + name + "' expects " +
                             std::to_string( sig.param_types.size() ) +
                             ( sig.variadic ? "+" : "" ) +
                             " argument(s), got " + std::to_string( args.size() ) );
            }

            expr_node node;
            node.type = expr_node::kind::func_call;
            node.func_name = name;
            node.args = std::move( args );
            return node;
        }

        return fail( "unexpected token '" + cur().text + "'" );
    }
};

auto tokenize_expr( std::string_view src ) -> std::vector<token>
{
    expr_lexer lex{ src, 0 };
    std::vector<token> tokens;
    for( ;; ) {
        auto tok = lex.next();
        tokens.push_back( tok );
        if( tok.type == token_type::end_of_input || tok.type == token_type::lex_error ) {
            break;
        }
    }
    return tokens;
}

} // anonymous namespace

// ============================================================
// parse_expr (public)
// ============================================================

auto parse_expr( std::string_view source, const func_registry &registry )
-> std::variant<expr_node, parse_error>
{
    auto tokens = tokenize_expr( source );
    expr_parser_state p{ std::move( tokens ), 0, registry, std::nullopt };
    auto root = p.parse_expr();
    if( p.error_ ) {
        return *p.error_;
    }
    if( p.cur().type != token_type::end_of_input ) {
        return parse_error{
            "unexpected token '" + p.cur().text + "' after expression end",
            p.cur().col
        };
    }
    return root;
}

// ============================================================
// evaluate_expr
// ============================================================

auto evaluate_expr( const expr_node &node, const func_registry &registry ) -> value
{
    switch( node.type ) {
        case expr_node::kind::literal:
            return node.literal_val;

        case expr_node::kind::func_call: {
            std::vector<value> args;
            args.reserve( node.args.size() );
            std::ranges::transform( node.args, std::back_inserter( args ),
            [&]( const expr_node & a ) {
                return evaluate_expr( a, registry );
            } );
            return registry.call( node.func_name, args );
        }

        case expr_node::kind::unary_op: {
            auto operand = evaluate_expr( node.children[0], registry );
            if( node.unary_operation == expr_node::un_op::logical_not ) {
                if( !std::holds_alternative<bool>( operand ) ) {
                    throw std::runtime_error( "'not' requires a boolean operand" );
                }
                return !std::get<bool>( operand );
            } else { // negate
                if( !std::holds_alternative<double>( operand ) ) {
                    throw std::runtime_error( "unary '-' requires a numeric operand" );
                }
                return -std::get<double>( operand );
            }
        }

        case expr_node::kind::binary_op: {
            // Short-circuit evaluation for logical operators
            if( node.binary_operation == expr_node::bin_op::logical_and ) {
                auto left = evaluate_expr( node.children[0], registry );
                if( !std::holds_alternative<bool>( left ) ) {
                    throw std::runtime_error( "'and' requires boolean operands" );
                }
                if( !std::get<bool>( left ) ) {
                    return false;
                }
                auto right = evaluate_expr( node.children[1], registry );
                if( !std::holds_alternative<bool>( right ) ) {
                    throw std::runtime_error( "'and' requires boolean operands" );
                }
                return std::get<bool>( right );
            }

            if( node.binary_operation == expr_node::bin_op::logical_or ) {
                auto left = evaluate_expr( node.children[0], registry );
                if( !std::holds_alternative<bool>( left ) ) {
                    throw std::runtime_error( "'or' requires boolean operands" );
                }
                if( std::get<bool>( left ) ) {
                    return true;
                }
                auto right = evaluate_expr( node.children[1], registry );
                if( !std::holds_alternative<bool>( right ) ) {
                    throw std::runtime_error( "'or' requires boolean operands" );
                }
                return std::get<bool>( right );
            }

            auto lv = evaluate_expr( node.children[0], registry );
            auto rv = evaluate_expr( node.children[1], registry );

            if( type_of( lv ) != type_of( rv ) ) {
                throw std::runtime_error( "type mismatch in binary expression" );
            }

            using op = expr_node::bin_op;
            switch( node.binary_operation ) {
                case op::add:
                    if( std::holds_alternative<double>( lv ) ) {
                        return std::get<double>( lv ) + std::get<double>( rv );
                    }
                    if( std::holds_alternative<std::string>( lv ) ) {
                        return std::get<std::string>( lv ) + std::get<std::string>( rv );
                    }
                    throw std::runtime_error( "'+' not supported for booleans" );
                case op::sub:
                    return std::get<double>( lv ) - std::get<double>( rv );
                case op::mul:
                    return std::get<double>( lv ) * std::get<double>( rv );
                case op::div: {
                    auto divisor = std::get<double>( rv );
                    if( divisor == 0.0 ) {
                        throw std::runtime_error( "division by zero" );
                    }
                    return std::get<double>( lv ) / divisor;
                }
                case op::eq:
                    return lv == rv;
                case op::neq:
                    return lv != rv;
                case op::lt:
                    if( std::holds_alternative<double>( lv ) ) {
                        return std::get<double>( lv ) < std::get<double>( rv );
                    }
                    return std::get<std::string>( lv ) < std::get<std::string>( rv );
                case op::lte:
                    if( std::holds_alternative<double>( lv ) ) {
                        return std::get<double>( lv ) <= std::get<double>( rv );
                    }
                    return std::get<std::string>( lv ) <= std::get<std::string>( rv );
                case op::gt:
                    if( std::holds_alternative<double>( lv ) ) {
                        return std::get<double>( lv ) > std::get<double>( rv );
                    }
                    return std::get<std::string>( lv ) > std::get<std::string>( rv );
                case op::gte:
                    if( std::holds_alternative<double>( lv ) ) {
                        return std::get<double>( lv ) >= std::get<double>( rv );
                    }
                    return std::get<std::string>( lv ) >= std::get<std::string>( rv );
                default:
                    throw std::runtime_error( "unhandled binary op" );
            }
        }
    }
    throw std::runtime_error( "unhandled expr_node kind" );
}

// ============================================================
// interpolate_text
// ============================================================

auto interpolate_text( std::string_view text, const func_registry &registry ) -> std::string
{
    std::string result;
    result.reserve( text.size() );

    auto it = text.begin();
    while( it != text.end() ) {
        if( *it != '{' ) {
            result += *it++;
            continue;
        }
        // {{ → literal '{'
        if( std::next( it ) != text.end() && *std::next( it ) == '{' ) {
            result += '{';
            std::advance( it, 2 );
            continue;
        }
        // Find matching '}'
        auto start = std::next( it );
        auto close = std::find( start, text.end(), '}' );
        if( close == text.end() ) {
            // Unmatched '{' — emit literally
            result += *it++;
            continue;
        }

        std::string_view expr_src( &*start, std::distance( start, close ) );
        auto parse_result = parse_expr( expr_src, registry );
        if( std::holds_alternative<parse_error>( parse_result ) ) {
            result += "[ERR:" + std::get<parse_error>( parse_result ).message + "]";
        } else {
            try {
                auto val = evaluate_expr( std::get<expr_node>( parse_result ), registry );
                std::visit( [&]( const auto & v ) {
                    if constexpr( std::is_same_v<std::decay_t<decltype( v )>, bool> ) {
                        result += v ? "true" : "false";
                    } else if constexpr( std::is_same_v<std::decay_t<decltype( v )>, double> ) {
                        // Show as integer when whole, otherwise with decimals
                        if( v == std::floor( v ) ) {
                            result += std::to_string( static_cast<long long>( v ) );
                        } else {
                            std::ostringstream oss;
                            oss << v;
                            result += oss.str();
                        }
                    } else {
                        result += v;
                    }
                }, val );
            } catch( const std::exception &e ) {
                result += "[ERR:" + std::string( e.what() ) + "]";
            }
        }

        it = std::next( close );
    }
    return result;
}

// ============================================================
// Yarn file parser (anonymous namespace)
// ============================================================

namespace
{

// A raw line with its indent depth and source line number.
struct raw_line {
    int indent = 0;
    std::string content; // trimmed of leading/trailing whitespace
    int line_num = 0;
};

auto count_indent( std::string_view line ) -> int
{
    return static_cast<int>(
               std::ranges::distance( line.begin(),
    std::ranges::find_if( line, []( unsigned char c ) {
        return !std::isspace( c );
    } ) )
           );
}

auto trim_sv( std::string_view s ) -> std::string_view
{
    auto start = std::ranges::find_if( s, []( unsigned char c ) { return !std::isspace( c ); } );
    if( start == s.end() ) {
        return {};
    }
    auto end = std::ranges::find_if( s | std::views::reverse,
    []( unsigned char c ) { return !std::isspace( c ); } ).base();
    return std::string_view( &*start, std::distance( start, end ) );
}

auto split_lines( std::string_view src ) -> std::vector<raw_line>
{
    std::vector<raw_line> lines;
    int line_num = 1;
    std::string_view remaining = src;

    while( !remaining.empty() ) {
        auto newline = remaining.find( '\n' );
        std::string_view line;
        if( newline == std::string_view::npos ) {
            line = remaining;
            remaining = {};
        } else {
            line = remaining.substr( 0, newline );
            remaining = remaining.substr( newline + 1 );
        }
        // Strip Windows-style \r
        if( !line.empty() && line.back() == '\r' ) {
            line = line.substr( 0, line.size() - 1 );
        }
        int indent = count_indent( line );
        auto content = std::string( trim_sv( line ) );
        lines.push_back( { indent, std::move( content ), line_num++ } );
    }
    return lines;
}

// Checks if a string starts with a given prefix (case-sensitive).
auto starts_with( std::string_view s, std::string_view prefix ) -> bool
{
    return s.size() >= prefix.size() && s.substr( 0, prefix.size() ) == prefix;
}

// Extracts the inner content of <<...>>, or empty if not a command.
auto extract_command( std::string_view line ) -> std::string_view
{
    if( starts_with( line, "<<" ) && line.size() > 4 && line.back() == '>' &&
        line[line.size() - 2] == '>' ) {
        return trim_sv( line.substr( 2, line.size() - 4 ) );
    }
    return {};
}

// Split a command string into name + raw arg tokens.
// Quotes are retained in arg tokens so parse_expr can parse them as strings.
// The command name itself is never quoted.
struct parsed_command {
    std::string name;
    std::vector<std::string> raw_args;
};

auto parse_command_line( std::string_view cmd ) -> parsed_command
{
    parsed_command result;
    bool in_quote = false;
    std::string current;

    for( auto c : cmd ) {
        if( c == '"' ) {
            in_quote = !in_quote;
            if( !result.name.empty() ) {
                current += c;  // keep quotes in args for parse_expr
            }
        } else if( ( c == ' ' || c == ',' ) && !in_quote ) {
            // Commas and spaces both act as argument separators outside quotes.
            if( !current.empty() ) {
                if( result.name.empty() ) {
                    result.name = std::move( current );
                } else {
                    result.raw_args.push_back( std::move( current ) );
                }
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if( !current.empty() ) {
        if( result.name.empty() ) {
            result.name = std::move( current );
        } else {
            result.raw_args.push_back( std::move( current ) );
        }
    }
    return result;
}

// yarn_parser is defined before parse_elements so that helper functions
// (parse_choice_body etc.) can take a yarn_parser& without a class forward-decl,
// which MSVC rejects inside anonymous namespaces.
class yarn_parser
{
    public:
        explicit yarn_parser( std::string_view source_name )
            : source_name_( source_name ) {}

        std::vector<std::string> errors;    // fatal — structural; story cannot load
        std::vector<std::string> warnings;  // non-fatal — bad expr/arg; element skipped

        // Set before parsing each node body so <<once>> keys are node-scoped.
        std::string current_node;
        int once_counter = 0;

        void error( int line_num, std::string msg ) {
            errors.push_back( source_name_ + ":" + std::to_string( line_num ) + ": " + msg );
        }

        void warn( int line_num, std::string msg ) {
            warnings.push_back( source_name_ + ":" + std::to_string( line_num ) + ": " + msg );
        }

        auto parse_condition( std::string_view src, int line_num )
        -> std::optional<expr_node> {
            auto result = parse_expr( src, func_registry::global() );
            if( std::holds_alternative<parse_error>( result ) ) {
                error( line_num, std::get<parse_error>( result ).message );
                return std::nullopt;
            }
            return std::get<expr_node>( std::move( result ) );
        }

    private:
        std::string source_name_;
};

// Forward declarations — the helpers call each other before all are defined.
auto parse_elements( yarn_parser &p, const std::vector<raw_line> &lines,
                     int &i, int end, int min_indent )
-> std::vector<node_element>;

auto make_once_condition( yarn_parser &p, std::optional<expr_node> extra_cond )
-> expr_node;

auto parse_once_block( yarn_parser &p, const std::vector<raw_line> &lines,
                       int &i, int end, int block_indent,
                       expr_node condition )
-> node_element;

// Parse the body of a choice option, starting after its -> line.
// Collects all lines with indent strictly greater than choice_indent.
auto parse_choice_body( yarn_parser &p, const std::vector<raw_line> &lines,
                        int &i, int end, int choice_indent )
-> std::vector<node_element>
{
    // Find end of body: lines with indent > choice_indent
    int body_end = i;
    while( body_end < end && ( lines[body_end].content.empty() ||
                               lines[body_end].indent > choice_indent ) ) {
        ++body_end;
    }
    return parse_elements( p, lines, i, body_end, choice_indent + 1 );
}

// Parses a <<repeat_for_item/category ...>>...<< endrepeat>> block into a repeat_group.
// i must point to the first line AFTER the <<repeat_for_*>> header when called.
// Supported header forms:
//   <<repeat_for_item "id1" "id2" [#include_containers] [if expr]>>
//   <<repeat_for_category "cat" [#include_containers] [if expr]>>
//   <<npc_repeat_for_item ...>>  / <<npc_repeat_for_category ...>>
auto parse_repeat_group( yarn_parser &p, const std::vector<raw_line> &lines,
                         int &i, int end, int /*group_indent*/,
                         const parsed_command &pc, int cmd_line_num )
-> node_element::repeat_group
{
    node_element::repeat_group rg;
    rg.is_npc    = starts_with( pc.name, "npc_" );
    const bool is_cat = pc.name.find( "category" ) != std::string::npos;

    // Split raw_args into id tokens and condition tokens (separated by the bare "if" token)
    const auto if_it = std::ranges::find( pc.raw_args, std::string{ "if" } );
    for( const auto &arg : std::ranges::subrange( pc.raw_args.begin(), if_it ) ) {
        auto sv = std::string_view{ arg };
        if( sv == "#include_containers" ) {
            rg.include_containers = true;
            continue;
        }
        if( sv.size() >= 2 && sv.front() == '"' && sv.back() == '"' ) {
            sv = sv.substr( 1, sv.size() - 2 );
        }
        if( is_cat ) { rg.for_category.emplace_back( sv ); }
        else          { rg.for_item.emplace_back( sv ); }
    }
    if( if_it != pc.raw_args.end() ) {
        std::string cond_src;
        for( const auto &tok : std::ranges::subrange( std::next( if_it ), pc.raw_args.end() ) ) {
            if( !cond_src.empty() ) { cond_src += ' '; }
            cond_src += tok;
        }
        rg.condition = p.parse_condition( cond_src, cmd_line_num );
    }

    // Collect body: exactly one -> template line followed by its body, closed by <<endrepeat>>
    bool found_template = false;
    while( i < end ) {
        const auto &line = lines[i];
        if( line.content.empty() || starts_with( line.content, "//" ) ) {
            ++i;
            continue;
        }
        const auto inner = extract_command( line.content );
        if( !inner.empty() && inner == "endrepeat" ) {
            ++i;
            break;
        }
        if( starts_with( line.content, "->" ) ) {
            if( found_template ) {
                p.warn( line.line_num, "<<repeat_for_*>>: only one -> template line is allowed" );
                ++i;
                continue;
            }
            found_template      = true;
            rg.text_template    = std::string( trim_sv( line.content.substr( 2 ) ) );
            const int tmpl_indent = line.indent;
            ++i;
            rg.body = parse_choice_body( p, lines, i, end, tmpl_indent );
        } else {
            p.warn( line.line_num, "<<repeat_for_*>>: expected -> template or <<endrepeat>>" );
            ++i;
        }
    }
    if( !found_template ) {
        p.warn( cmd_line_num, "<<repeat_for_*>> block is missing a -> template line" );
    }
    return rg;
}

// Parses a run of -> lines (possibly interleaved with blank lines) into a choice_group.
auto parse_choice_group( yarn_parser &p, const std::vector<raw_line> &lines,
                         int &i, int end, int group_indent )
-> node_element
{
    node_element group;
    group.type = node_element::kind::choice_group;

    while( i < end ) {
        const auto &line = lines[i];
        // Skip blank lines between choices
        if( line.content.empty() ) {
            ++i;
            continue;
        }
        // Stop if indent leaves the group
        if( line.indent != group_indent ) {
            break;
        }
        // <<repeat_for_*>> / <<npc_repeat_for_*>> — inventory-driven choice block
        if( !starts_with( line.content, "->" ) ) {
            auto cmd = extract_command( line.content );
            if( !cmd.empty() ) {
                auto pc = parse_command_line( cmd );
                if( starts_with( pc.name, "repeat_for_" ) ||
                    starts_with( pc.name, "npc_repeat_for_" ) ) {
                    ++i; // advance past header
                    group.repeat_groups.push_back(
                        parse_repeat_group( p, lines, i, end, group_indent, pc, line.line_num ) );
                    continue;
                }
            }
            break;
        }

        // Parse the -> line: "-> Choice text" or "-> Choice text <<if condition>>"
        std::string_view choice_text = trim_sv( line.content.substr( 2 ) );
        std::optional<expr_node> condition;

        // Check for trailing <<if condition>>
        if( choice_text.ends_with( ">>" ) ) {
            auto if_pos = choice_text.rfind( "<<if " );
            if( if_pos != std::string_view::npos ) {
                auto cond_src = trim_sv( choice_text.substr( if_pos + 5,
                                         choice_text.size() - if_pos - 7 ) );
                condition = p.parse_condition( cond_src, line.line_num );
                choice_text = trim_sv( choice_text.substr( 0, if_pos ) );
            }
        }

        // Helper: strip a trailing tag (e.g. "#spoken", "#tail") from choice_text.
        // Returns true and trims the text if the tag is found as a standalone suffix.
        auto strip_tag = [&]( std::string_view tag ) -> bool {
            auto tag_pos = choice_text.rfind( tag );
            if( tag_pos == std::string_view::npos )
            {
                return false;
            }
            bool leading_space = tag_pos == 0 || std::isspace( choice_text[tag_pos - 1] );
            bool trailing_end  = tag_pos + tag.size() == choice_text.size();
            if( leading_space && trailing_end )
            {
                choice_text = trim_sv( choice_text.substr( 0, tag_pos ) );
                return true;
            }
            return false;
        };

        // Strip all known tags from the end in any order.
        // Loop because each strip exposes the next tag as the new trailing token.
        bool echo_speech = false;
        bool tail        = false;
        for( bool found = true; found; ) {
            found  = false;
            if( strip_tag( "#spoken" ) ) { echo_speech = true; found = true; }
            if( strip_tag( "#tail" ) ) { tail        = true; found = true; }
        }

        int choice_indent = line.indent;
        ++i;

        node_element::choice ch;
        ch.text        = std::string( choice_text );
        ch.condition   = std::move( condition );
        ch.echo_speech = echo_speech;
        ch.tail        = tail;
        ch.body = parse_choice_body( p, lines, i, end, choice_indent );
        group.choices.push_back( std::move( ch ) );
    }
    return group;
}

// Parse any body lines following a => line (continuation lines, commands, etc.).
// Collects lines with indent >= line_indent that don't start the next => option.
auto parse_line_group_body( yarn_parser &p, const std::vector<raw_line> &lines,
                            int &i, int end, int line_indent )
-> std::vector<node_element>
{
    int body_end = i;
    while( body_end < end && !starts_with( lines[body_end].content, "=>" ) &&
           ( lines[body_end].content.empty() || lines[body_end].indent >= line_indent ) ) {
        ++body_end;
    }
    return parse_elements( p, lines, i, body_end, line_indent );
}

// Parses => line groups
auto parse_line_group( yarn_parser &p, const std::vector<raw_line> &lines,
                       int &i, int end, int group_indent )
-> node_element
{
    node_element group;
    group.type = node_element::kind::line_group;

    while( i < end ) {
        const auto &line = lines[i];
        // Skip blank lines
        if( line.content.empty() ) {
            ++i;
            continue;
        }

        if( line.indent != group_indent || !starts_with( line.content, "=>" ) ) {
            break;
        }

        // Parse the => line: "=> [Speaker: ]text" or "=> text <<if condition>>"
        std::string_view line_group_text = trim_sv( line.content.substr( 2 ) );
        std::optional<expr_node> condition;

        // Check for trailing <<if condition>>
        if( line_group_text.ends_with( ">>" ) ) {
            auto if_pos = line_group_text.rfind( "<<if " );
            if( if_pos != std::string_view::npos ) {
                auto cond_src = trim_sv( line_group_text.substr( if_pos + 5,
                                         line_group_text.size() - if_pos - 7 ) );
                condition = p.parse_condition( cond_src, line.line_num );
                line_group_text = trim_sv( line_group_text.substr( 0, if_pos ) );
            }
        }

        int line_indent = line.indent;
        ++i;  // advance past the => line before parsing the body

        // Construct the text element, respecting "- narrator", "Speaker: text", or plain text.
        node_element text_elem;
        text_elem.type = node_element::kind::dialogue;
        if( line_group_text.size() >= 2 && line_group_text[0] == '-' && line_group_text[1] == ' ' ) {
            text_elem.speaker = "__narrator";
            text_elem.text    = std::string( trim_sv( line_group_text.substr( 2 ) ) );
        } else {
            auto colon = line_group_text.find( ':' );
            if( colon != std::string_view::npos && colon > 0 &&
                line_group_text.find( ' ' ) > colon ) {
                text_elem.speaker = std::string( trim_sv( line_group_text.substr( 0, colon ) ) );
                text_elem.text    = std::string( trim_sv( line_group_text.substr( colon + 1 ) ) );
            } else {
                text_elem.text = std::string( line_group_text );
            }
        }

        node_element::choice ch;
        ch.condition = std::move( condition );
        ch.body.push_back( std::move( text_elem ) );
        auto tail = parse_line_group_body( p, lines, i, end, line_indent );
        ch.body.insert( ch.body.end(), std::make_move_iterator( tail.begin() ),
                        std::make_move_iterator( tail.end() ) );
        group.choices.push_back( std::move( ch ) );
    }
    return group;
}

// Parses <<if>>...<<else>>...<<endif>> blocks.
auto parse_if_block( yarn_parser &p, const std::vector<raw_line> &lines,
                     int &i, int end, int block_indent,
                     std::string_view condition_src, int cond_line_num )
-> node_element
{
    node_element elem;
    elem.type = node_element::kind::if_block;
    elem.condition = p.parse_condition( condition_src, cond_line_num );

    // Collect if-body until <<else>> or <<endif>>
    int body_start = i;
    std::vector<node_element> if_body;
    std::vector<node_element> else_body;

    bool in_else = false;
    while( i < end ) {
        const auto &line = lines[i];
        if( line.content.empty() || starts_with( line.content, "//" ) ) {
            ++i;
            continue;
        }

        auto &target_body = in_else ? else_body : if_body;

        // Choice groups and nested <<if>> span multiple lines — handle them directly
        // here with the correct `end` boundary rather than delegating to parse_elements
        // with the i+1 limit used below for single-line elements.
        if( starts_with( line.content, "->" ) ) {
            target_body.push_back( parse_choice_group( p, lines, i, end, line.indent ) );
            continue;
        }

        auto cmd = extract_command( line.content );
        if( !cmd.empty() ) {
            if( cmd == "else" ) {
                if( in_else ) {
                    p.warn( line.line_num, "duplicate <<else>>" );
                }
                in_else = true;
                ++i;
                continue;
            }
            if( cmd == "endif" ) {
                ++i;
                break;
            }
            if( starts_with( cmd, "if " ) ) {
                auto cond_src = trim_sv( cmd.substr( 3 ) );
                ++i;
                target_body.push_back(
                    parse_if_block( p, lines, i, end, line.indent, cond_src, line.line_num ) );
                continue;
            }
            if( cmd == "once" || starts_with( cmd, "once if " ) ) {
                std::optional<expr_node> extra;
                if( starts_with( cmd, "once if " ) ) {
                    extra = p.parse_condition( trim_sv( cmd.substr( 8 ) ), line.line_num );
                }
                ++i;
                target_body.push_back(
                    parse_once_block( p, lines, i, end, line.indent,
                                      make_once_condition( p, std::move( extra ) ) ) );
                continue;
            }
        }

        // All other elements (dialogue, single-line commands) are single-line;
        // use i+1 so parse_elements never reaches <<else>> / <<endif>>.
        auto body_elems = parse_elements( p, lines, i, i + 1, block_indent );
        target_body.insert( target_body.end(),
                            std::make_move_iterator( body_elems.begin() ),
                            std::make_move_iterator( body_elems.end() ) );
    }

    elem.if_body = std::move( if_body );
    elem.else_body = std::move( else_body );
    return elem;
}

// Builds the condition expression for a <<once>> block.
// Generates a stable per-node key and composes: [extra_cond &&] once("key").
// extra_cond goes on the LEFT of && so short-circuit prevents consuming the once
// key when the extra condition is false.
auto make_once_condition( yarn_parser &p, std::optional<expr_node> extra_cond ) -> expr_node
{
    const auto key = "_yarn_once_" + p.current_node + "_" + std::to_string( p.once_counter++ );

    expr_node key_lit;
    key_lit.type          = expr_node::kind::literal;
    key_lit.literal_val   = value{ key };

    expr_node once_call;
    once_call.type      = expr_node::kind::func_call;
    once_call.func_name = "once";
    once_call.args.push_back( std::move( key_lit ) );

    if( !extra_cond ) {
        return once_call;
    }

    expr_node and_node;
    and_node.type             = expr_node::kind::binary_op;
    and_node.binary_operation = expr_node::bin_op::logical_and;
    and_node.children.push_back( std::move( *extra_cond ) );
    and_node.children.push_back( std::move( once_call ) );
    return and_node;
}

// Parses <<once>>...<<else>>...<<endonce>> blocks.
// Condition is already built by make_once_condition.
auto parse_once_block( yarn_parser &p, const std::vector<raw_line> &lines,
                       int &i, int end, int block_indent,
                       expr_node condition )
-> node_element
{
    node_element elem;
    elem.type      = node_element::kind::if_block;
    elem.condition = std::move( condition );

    std::vector<node_element> if_body;
    std::vector<node_element> else_body;
    bool in_else = false;

    while( i < end ) {
        const auto &line = lines[i];
        if( line.content.empty() || starts_with( line.content, "//" ) ) {
            ++i;
            continue;
        }

        auto &target_body = in_else ? else_body : if_body;

        if( starts_with( line.content, "->" ) ) {
            target_body.push_back( parse_choice_group( p, lines, i, end, line.indent ) );
            continue;
        }

        auto cmd = extract_command( line.content );
        if( !cmd.empty() ) {
            if( cmd == "else" ) {
                if( in_else ) {
                    p.warn( line.line_num, "duplicate <<else>>" );
                }
                in_else = true;
                ++i;
                continue;
            }
            if( cmd == "endonce" ) {
                ++i;
                break;
            }
            if( starts_with( cmd, "if " ) ) {
                auto cond_src = trim_sv( cmd.substr( 3 ) );
                ++i;
                target_body.push_back(
                    parse_if_block( p, lines, i, end, line.indent, cond_src, line.line_num ) );
                continue;
            }
            if( cmd == "once" || starts_with( cmd, "once if " ) ) {
                std::optional<expr_node> extra;
                if( starts_with( cmd, "once if " ) ) {
                    extra = p.parse_condition( trim_sv( cmd.substr( 8 ) ), line.line_num );
                }
                ++i;
                target_body.push_back(
                    parse_once_block( p, lines, i, end, line.indent,
                                      make_once_condition( p, std::move( extra ) ) ) );
                continue;
            }
        }

        auto body_elems = parse_elements( p, lines, i, i + 1, block_indent );
        target_body.insert( target_body.end(),
                            std::make_move_iterator( body_elems.begin() ),
                            std::make_move_iterator( body_elems.end() ) );
    }

    elem.if_body  = std::move( if_body );
    elem.else_body = std::move( else_body );
    return elem;
}

// Parse node elements from lines[i..end), requiring indent >= min_indent.
auto parse_elements( yarn_parser &p, const std::vector<raw_line> &lines,
                     int &i, int end, int min_indent )
-> std::vector<node_element>
{
    std::vector<node_element> elements;

    while( i < end ) {
        const auto &line = lines[i];

        // Skip blank lines and comments
        if( line.content.empty() || starts_with( line.content, "//" ) ) {
            ++i;
            continue;
        }

        // Stop if indentation drops below minimum
        if( line.indent < min_indent ) {
            break;
        }

        // Choice group
        if( starts_with( line.content, "->" ) ) {
            elements.push_back( parse_choice_group( p, lines, i, end, line.indent ) );
            continue;
        }

        // Line group
        if( starts_with( line.content, "=>" ) ) {
            elements.push_back( parse_line_group( p, lines, i, end, line.indent ) );
            continue;
        }

        // Commands
        auto cmd_inner = extract_command( line.content );
        if( !cmd_inner.empty() ) {
            // <<else>>, <<endif>>, and <<endonce>> are handled by their block parsers;
            // encountering them here is a structural error.
            if( cmd_inner == "else" || cmd_inner == "endif" ) {
                p.warn( line.line_num,
                        "<<" + std::string( cmd_inner ) + ">> without matching <<if>>" );
                ++i;
                continue;
            }
            if( cmd_inner == "endonce" ) {
                p.warn( line.line_num, "<<endonce>> without matching <<once>>" );
                ++i;
                continue;
            }
            if( cmd_inner == "endrepeat" ) {
                p.warn( line.line_num, "<<endrepeat>> without matching <<repeat_for_*>>" );
                ++i;
                continue;
            }

            // <<repeat_for_*>> / <<npc_repeat_for_*>> — start an inventory-driven choice group
            if( starts_with( cmd_inner, "repeat_for_" ) ||
                starts_with( cmd_inner, "npc_repeat_for_" ) ) {
                elements.push_back( parse_choice_group( p, lines, i, end, line.indent ) );
                continue;
            }

            if( starts_with( cmd_inner, "if " ) ) {
                auto cond_src = trim_sv( cmd_inner.substr( 3 ) );
                ++i;
                elements.push_back(
                    parse_if_block( p, lines, i, end, line.indent,
                                    cond_src, line.line_num ) );
                continue;
            }

            if( cmd_inner == "once" || starts_with( cmd_inner, "once if " ) ) {
                std::optional<expr_node> extra;
                if( starts_with( cmd_inner, "once if " ) ) {
                    extra = p.parse_condition( trim_sv( cmd_inner.substr( 8 ) ), line.line_num );
                }
                ++i;
                elements.push_back(
                    parse_once_block( p, lines, i, end, line.indent,
                                      make_once_condition( p, std::move( extra ) ) ) );
                continue;
            }

            auto pc = parse_command_line( cmd_inner );
            ++i;

            // <<player "text">> / <<npc "text">> — explicit speech lines.
            // Parsed at load time into kind::dialogue elements; never reach command_registry.
            // <<player>> emits with player (You) formatting.
            // <<npc>> emits with NPC name formatting, useful after a #silent choice or
            // to add additional NPC lines inside a complex choice body.
            if( pc.name == "player" || pc.name == "npc" ) {
                if( pc.raw_args.size() != 1 ) {
                    p.warn( line.line_num, "<<" + pc.name + ">> requires exactly one string argument" );
                } else {
                    // Strip surrounding quotes; the text is a literal string, not an expression.
                    auto raw = std::string_view( pc.raw_args[0] );
                    if( raw.size() >= 2 && raw.front() == '"' && raw.back() == '"' ) {
                        raw = raw.substr( 1, raw.size() - 2 );
                    }
                    node_element elem;
                    elem.type    = node_element::kind::dialogue;
                    elem.speaker = pc.name;  // "player" or "npc"
                    elem.text    = std::string( raw );
                    elements.push_back( std::move( elem ) );
                }
            } else if( pc.name == "jump" ) {
                if( pc.raw_args.empty() ) {
                    p.warn( line.line_num, "<<jump>> requires a node name" );
                } else {
                    node_element elem;
                    elem.type = node_element::kind::jump;
                    elem.jump_target = pc.raw_args[0];
                    elements.push_back( std::move( elem ) );
                }
            } else if( pc.name == "detour" ) {
                if( pc.raw_args.empty() ) {
                    p.warn( line.line_num, "<<detour>> requires a node name" );
                } else {
                    node_element elem;
                    elem.type = node_element::kind::goto_node;
                    elem.jump_target = pc.raw_args[0];
                    elements.push_back( std::move( elem ) );
                }
            } else if( pc.name == "cross_detour" ) {
                if( pc.raw_args.empty() ) {
                    p.warn( line.line_num, "<<cross_detour>> requires a story::node target" );
                } else {
                    node_element elem;
                    elem.type = node_element::kind::goto_node;
                    elem.jump_target = pc.raw_args[0];
                    elements.push_back( std::move( elem ) );
                }
            } else if( pc.name == "cross_jump" ) {
                if( pc.raw_args.empty() ) {
                    p.warn( line.line_num, "<<cross_jump>> requires a story::node target" );
                } else {
                    node_element elem;
                    elem.type = node_element::kind::jump;
                    elem.jump_target = pc.raw_args[0];
                    elements.push_back( std::move( elem ) );
                }
            } else if( pc.name == "stop" ) {
                node_element elem;
                elem.type = node_element::kind::stop;
                elements.push_back( std::move( elem ) );
            } else if( pc.name == "return" ) {
                node_element elem;
                elem.type = node_element::kind::yarn_return;
                elements.push_back( std::move( elem ) );
            } else {
                // Generic command — parse each arg as an expression.
                node_element elem;
                elem.type = node_element::kind::command;
                elem.command_name = pc.name;
                bool args_ok = true;
                for( const auto &raw : pc.raw_args ) {
                    auto result = parse_expr( raw, func_registry::global() );
                    if( std::holds_alternative<parse_error>( result ) ) {
                        p.warn( line.line_num,
                                "<<" + pc.name + ">>: bad argument '" + raw + "': " +
                                std::get<parse_error>( result ).message );
                        args_ok = false;
                        break;
                    }
                    elem.command_args.push_back( std::get<expr_node>( std::move( result ) ) );
                }
                if( args_ok ) {
                    elements.push_back( std::move( elem ) );
                }
            }
            continue;
        }

        // Dialogue line — split "- narrator text", "Speaker: text", or plain "text"
        {
            node_element elem;
            elem.type = node_element::kind::dialogue;

            if( line.content.size() >= 2 && line.content[0] == '-' && line.content[1] == ' ' ) {
                // Narrator line: "- text" — rendered gray, no speaker label.
                elem.speaker = "__narrator";
                elem.text    = std::string( trim_sv( line.content.substr( 2 ) ) );
            } else {
                auto colon = line.content.find( ':' );
                if( colon != std::string::npos && colon > 0 &&
                    line.content.find( ' ' ) > colon ) {
                    // Has "Speaker: " format
                    elem.speaker = std::string( trim_sv( line.content.substr( 0, colon ) ) );
                    elem.text    = std::string( trim_sv( line.content.substr( colon + 1 ) ) );
                } else {
                    elem.text = line.content;
                }
            }
            elements.push_back( std::move( elem ) );
            ++i;
        }
    }

    return elements;
}

} // anonymous namespace

// ============================================================
// yarn_story
// ============================================================

auto yarn_story::from_string( std::string_view content, std::string_view source_name )
-> std::pair<yarn_story, load_result>
{
    yarn_story story;
    story.source_name_ = std::string( source_name );
    load_result result;

    yarn_parser parser( source_name );
    auto all_lines = split_lines( content );

    int i = 0;
    int total = static_cast<int>( all_lines.size() );

    while( i < total ) {
        // Skip blank lines between nodes
        if( all_lines[i].content.empty() ) {
            ++i;
            continue;
        }

        // Parse node header (key: value lines until ---)
        yarn_node node;
        bool found_separator = false;
        std::vector<std::string> raw_inject_into;

        while( i < total ) {
            const auto &line = all_lines[i];
            if( line.content == "---" ) {
                found_separator = true;
                ++i;
                break;
            }
            // Parse header key: value
            auto colon = line.content.find( ':' );
            if( colon != std::string::npos ) {
                auto key = std::string( trim_sv( line.content.substr( 0, colon ) ) );
                auto val = std::string( trim_sv( line.content.substr( colon + 1 ) ) );
                if( key == "title" ) {
                    node.title = val;
                } else if( key == "tags" ) {
                    // Space-separated tags
                    std::istringstream ss( val );
                    std::string tag;
                    while( ss >> tag ) {
                        node.tags.push_back( tag );
                    }
                } else if( key == "shared_choices" ) {
                    // Space-separated list of node titles whose choices to append.
                    std::istringstream ss( val );
                    std::string name;
                    while( ss >> name ) {
                        node.shared_choices.push_back( std::move( name ) );
                    }
                } else if( key == "inject_into" ) {
                    // Deferred: raw values accumulated and resolved after all headers
                    // are parsed so inject_category is always available for #inject_category.
                    raw_inject_into.push_back( val );
                } else if( key == "inject_tags" ) {
                    std::istringstream ss( val );
                    std::string tok;
                    while( ss >> tok ) {
                        if( !tok.empty() && tok.front() == '#' ) { tok.erase( tok.begin() ); }
                        if( !tok.empty() ) { node.inject_tags.push_back( std::move( tok ) ); }
                    }
                } else if( key == "inject_category" ) {
                    std::istringstream ss( val );
                    std::string tok;
                    while( ss >> tok ) {
                        if( !tok.empty() && tok.front() == '#' ) { tok.erase( tok.begin() ); }
                        if( !tok.empty() ) { node.inject_category.push_back( std::move( tok ) ); }
                    }
                } else if( key == "inject_block" ) {
                    std::istringstream ss( val );
                    std::string tok;
                    while( ss >> tok ) {
                        if( !tok.empty() && tok.front() == '#' ) { tok.erase( tok.begin() ); }
                        if( !tok.empty() ) { node.inject_block.push_back( std::move( tok ) ); }
                    }
                } else if( key == "inject_priority" ) {
                    int prio = 0;
                    auto [ptr, ec] = std::from_chars( val.data(), val.data() + val.size(), prio );
                    if( ec != std::errc{} ) {
                        parser.warn( 0, "inject_priority on '" + node.title +
                                     "': invalid integer '" + val + "'" );
                    } else {
                        node.inject_priority = prio;
                    }
                }
            }
            ++i;
        }

        if( !found_separator ) {
            break; // Malformed or trailing content
        }
        if( node.title.empty() ) {
            parser.error( all_lines[i - 1].line_num, "node has no title" );
        }

        // Resolve accumulated inject_into values now that inject_category is fully parsed.
        // Each raw value is one OR-group:
        //   "#inject_category"        → tag-based, using own inject_category as AND-group
        //   "#tag1 #tag2, #tag3"      → tag-based; space/comma separators both accepted
        //   "story::NodeName" / "Node" → direct injection
        auto parse_hash_tags = []( std::string_view sv ) -> std::vector<std::string> {
            std::vector<std::string> result;
            std::string work( sv );
            std::ranges::replace( work, ',', ' ' );
            std::istringstream ss( work );
            std::string tok;
            while( ss >> tok )
            {
                if( !tok.empty() && tok.front() == '#' ) { tok.erase( tok.begin() ); }
                if( !tok.empty() ) { result.push_back( std::move( tok ) ); }
            }
            return result;
        };
        for( const auto &raw : raw_inject_into ) {
            yarn_node::inject_target tgt;
            auto sv = trim_sv( std::string_view( raw ) );
            if( !sv.empty() && sv.front() == '#' ) {
                tgt.is_direct = false;
                if( sv == "#inject_category" ) {
                    tgt.required_tags = node.inject_category;
                    if( tgt.required_tags.empty() ) {
                        parser.warn( 0, "inject_into: #inject_category on '" + node.title +
                                     "' but inject_category is empty" );
                    }
                } else {
                    tgt.required_tags = parse_hash_tags( sv );
                }
            } else {
                tgt.is_direct = true;
                if( const auto sep = sv.find( "::" ); sep != std::string_view::npos ) {
                    tgt.target_story = std::string( sv.substr( 0, sep ) );
                    tgt.target_node  = std::string( sv.substr( sep + 2 ) );
                } else {
                    tgt.target_node = std::string( sv );
                }
            }
            node.inject_into.push_back( std::move( tgt ) );
        }

        // Find the end of this node (=== marker)
        int node_end = i;
        while( node_end < total && all_lines[node_end].content != "===" ) {
            ++node_end;
        }

        // Parse body elements
        parser.current_node = node.title;
        parser.once_counter = 0;
        node.elements = parse_elements( parser, all_lines, i, node_end, 0 );

        // Advance past ===
        if( node_end < total ) {
            i = node_end + 1;
        } else {
            i = node_end;
        }

        if( !node.title.empty() ) {
            story.nodes_.emplace( node.title, std::move( node ) );
        }
    }

    // shared_choices resolution is deferred to resolve_shared_choices(), called
    // after all stories are loaded so cross-file "story::NodeName" references work.

    result.errors   = std::move( parser.errors );
    result.warnings = std::move( parser.warnings );
    return { std::move( story ), std::move( result ) };
}

auto yarn_story::from_file( const std::string &path )
-> std::pair<yarn_story, load_result>
{
    if( !file_exist( path ) ) {
        load_result result;
        result.errors.push_back( "file not found: " + path );
        return { yarn_story{}, std::move( result ) };
    }
    auto content = read_entire_file( path );
    return from_string( content, path );
}

auto yarn_story::has_node( const std::string &name ) const -> bool
{
    return nodes_.contains( name );
}

auto yarn_story::get_node( const std::string &name ) const -> const yarn_node &
{
    return nodes_.at( name );
}

auto yarn_story::all_nodes() const -> const std::unordered_map<std::string, yarn_node> &
{
    return nodes_;
}

void yarn_story::add_node( yarn_node node )
{
    nodes_.emplace( node.title, std::move( node ) );
}

auto yarn_story::get_node_mutable( const std::string &name ) -> yarn_node *
{
    auto it = nodes_.find( name );
    return it != nodes_.end() ? &it->second : nullptr;
}

// Recursively qualifies bare jump/goto targets in an element tree with a source story prefix.
// Bare names (no '::') become "source_story::name".  Already-qualified names are left unchanged.
// Applied when copying choice bodies from a shared/injection source into a target node so that
// authors can use bare node names within their own file without needing explicit cross-file syntax.
static void qualify_elements( std::vector<node_element> &elements, const std::string &source_story )
{
    auto qualify = [&]( std::string & target ) {
        if( target.find( "::" ) == std::string::npos ) {
            target = source_story + "::" + target;
        }
    };
    for( auto &elem : elements ) {
        switch( elem.type ) {
            case node_element::kind::jump:
            case node_element::kind::goto_node:
                qualify( elem.jump_target );
                break;
            case node_element::kind::if_block:
                qualify_elements( elem.if_body,   source_story );
                qualify_elements( elem.else_body,  source_story );
                break;
            case node_element::kind::choice_group:
            case node_element::kind::line_group:
                for( auto &ch : elem.choices ) {
                    qualify_elements( ch.body, source_story );
                }
                break;
            default:
                break;
        }
    }
}

void yarn_story::resolve_shared_choices( const node_lookup_fn &lookup,
        const std::string &own_name,
        std::vector<std::string> &out_errors )
{
    std::unordered_set<std::string> resolved;
    std::unordered_set<std::string> in_progress;

    auto merge_choices = [&]( node_element & dest_cg, const yarn_node & base,
    const std::string & source_story ) {
        auto base_cg_it = std::ranges::find_if( base.elements, []( const node_element & e ) {
            return e.type == node_element::kind::choice_group;
        } );
        if( base_cg_it != base.elements.end() ) {
            const auto insert_pos = static_cast<ptrdiff_t>( dest_cg.choices.size() );
            dest_cg.choices.insert( dest_cg.choices.end(),
                                    base_cg_it->choices.begin(),
                                    base_cg_it->choices.end() );
            for( auto it = dest_cg.choices.begin() + insert_pos;
                 it != dest_cg.choices.end(); ++it ) {
                qualify_elements( it->body, source_story );
            }
        }
    };

    std::function<void( const std::string &, int )> resolve_node;
    resolve_node = [&]( const std::string & node_title, int depth ) {
        if( resolved.contains( node_title ) ) {
            return;
        }
        if( in_progress.contains( node_title ) ) {
            // Caller already emitted the cycle error before recursing.
            return;
        }

        auto *node = get_node_mutable( node_title );
        if( !node || node->shared_choices.empty() ) {
            resolved.insert( node_title );
            return;
        }

        in_progress.insert( node_title );

        if( depth > 10 ) {
            DebugLog( DL::Debug, DC::Dialogue )
                    << "yarn shared_choices: chain depth exceeds 10 at node '" << node_title << "'";
        }

        auto cg_it = std::ranges::find_if( node->elements, []( const node_element & e ) {
            return e.type == node_element::kind::choice_group;
        } );
        if( cg_it == node->elements.end() ) {
            out_errors.push_back( "shared_choices on '" + node_title + "': node has no choice_group" );
            in_progress.erase( node_title );
            resolved.insert( node_title );
            return;
        }

        for( const auto &ref : node->shared_choices ) {
            if( ref == node_title ) {
                out_errors.push_back( "shared_choices on '" + node_title + "': node lists itself as base" );
                continue;
            }

            std::string base_story_name = own_name;
            std::string base_node_name  = ref;
            if( const auto sep = ref.find( "::" ); sep != std::string::npos ) {
                base_story_name = ref.substr( 0, sep );
                base_node_name  = ref.substr( sep + 2 );
            }

            if( base_story_name != own_name ) {
                // Cross-story: resolution order of other stories is not guaranteed,
                // so chaining is not supported across story files.
                const yarn_node *base = lookup( base_story_name, base_node_name );
                if( base == nullptr ) {
                    out_errors.push_back( "shared_choices on '" + node_title +
                                          "': base node '" + ref + "' not found" );
                    continue;
                }
                if( !base->shared_choices.empty() ) {
                    out_errors.push_back( "shared_choices on '" + node_title +
                                          "': cross-story base '" + ref +
                                          "' has its own shared_choices — chaining across stories is not supported" );
                    continue;
                }
                merge_choices( *cg_it, *base, base_story_name );
            } else {
                // Within-story: check for cycle before recursing.
                if( in_progress.contains( base_node_name ) ) {
                    out_errors.push_back( "shared_choices on '" + node_title +
                                          "': cycle detected involving '" + base_node_name + "'" );
                    continue;
                }
                // Resolve base first so its choices are fully merged before we copy them.
                resolve_node( base_node_name, depth + 1 );

                const auto *base = get_node_mutable( base_node_name );
                if( base == nullptr ) {
                    out_errors.push_back( "shared_choices on '" + node_title +
                                          "': base node '" + ref + "' not found" );
                    continue;
                }
                merge_choices( *cg_it, *base, own_name );
            }
        }

        in_progress.erase( node_title );
        resolved.insert( node_title );
    };

    for( const auto &[title, node_ref] : nodes_ ) {
        resolve_node( title, 0 );
    }
}

// ============================================================
// yarn_runtime helpers (anonymous namespace)
// ============================================================

namespace
{

// Mirrors TALK_SIZE_UP logic from npctalk.cpp::dynamic_line().
auto size_up_text( const npc &p, const player &you ) -> std::string
{
    int ability = you.per_cur * 3 + you.int_cur;
    if( ability <= 10 ) {
        return _( "You can't make anything out." );
    }

    if( p.is_player_ally() || ability > 100 ) {
        ability = 100;
    }

    std::string info;
    int str_range = 100 / ability;
    int str_min   = ( p.str_max / str_range ) * str_range;
    info += string_format( _( "Str %d - %d" ), str_min, str_min + str_range );

    if( ability >= 40 ) {
        int dex_range = 160 / ability;
        int dex_min   = ( p.dex_max / dex_range ) * dex_range;
        info += string_format( _( "  Dex %d - %d" ), dex_min, dex_min + dex_range );
    }
    if( ability >= 50 ) {
        int int_range = 200 / ability;
        int int_min   = ( p.int_max / int_range ) * int_range;
        info += string_format( _( "  Int %d - %d" ), int_min, int_min + int_range );
    }
    if( ability >= 60 ) {
        int per_range = 240 / ability;
        int per_min   = ( p.per_max / per_range ) * per_range;
        info += string_format( _( "  Per %d - %d" ), per_min, per_min + per_range );
    }

    needs_rates rates = p.calc_needs_rates();

    if( ability >= 100 - ( p.get_fatigue() / 10 ) ) {
        std::string how_tired;
        if( p.get_fatigue() > fatigue_levels::exhausted ) {
            how_tired = _( "Exhausted" );
        } else if( p.get_fatigue() > fatigue_levels::dead_tired ) {
            how_tired = _( "Dead tired" );
        } else if( p.get_fatigue() > fatigue_levels::tired ) {
            how_tired = _( "Tired" );
        } else {
            how_tired = _( "Not tired" );
            if( ability >= 100 ) {
                time_duration sleep_at = 5_minutes
                                         * ( fatigue_levels::tired - p.get_fatigue() )
                                         / rates.fatigue;
                how_tired += _( ".  Will need sleep in " ) + to_string_approx( sleep_at );
            }
        }
        info += "\n" + how_tired;
    }

    if( ability >= 100 ) {
        if( p.get_thirst() < thirst_levels::thirsty ) {
            time_duration thirst_at = 5_minutes
                                      * ( thirst_levels::thirsty - p.get_thirst() )
                                      / rates.thirst;
            if( thirst_at > 1_hours ) {
                info += _( "\nWill need water in " ) + to_string_approx( thirst_at );
            }
        } else {
            info += _( "\nThirsty" );
        }
        if( p.max_stored_kcal() - p.get_stored_kcal() > 500 ) {
            time_duration hunger_at = 5_minutes
                                      * ( 500 - p.max_stored_kcal() + p.get_stored_kcal() )
                                      / p.bmr();
            if( hunger_at > 1_hours ) {
                info += _( "\nWill need food in " ) + to_string_approx( hunger_at );
            }
        } else {
            info += _( "\nHungry" );
        }
    }

    return info;
}

} // anonymous namespace

// ============================================================
// Conversation context (thread-local so global registry lambdas can read it)
// ============================================================

namespace
{

struct yarn_conv_ctx {
    npc             *npc_ref           = nullptr;
    player          *player_ref        = nullptr;
    // DEPRECATED: Only used by the legacy JSON dialogue shim. Remove when migration complete.
    dialogue        *dialogue_ref      = nullptr;
    // Set at the start of run() so commands can emit narrator lines to the dialogue window.
    dialogue_window *d_win_ref         = nullptr;
    // Set by _set_current_item command during repeat_group choice execution.
    std::string      current_item_type;
    // Set by <<set_reason>> command; cleared at conversation start. Read by has_reason().
    std::string      reason;
};

thread_local yarn_conv_ctx g_conv_ctx;

static const efftype_id effect_pacified( "pacified" );
static const efftype_id effect_pet( "pet" );

struct conv_ctx_guard {
    ~conv_ctx_guard() { g_conv_ctx = {}; }
};

} // namespace

// ============================================================
// dynamic_choice_registry
// ============================================================

auto dynamic_choice_registry::global() -> dynamic_choice_registry &
{
    static dynamic_choice_registry inst;
    return inst;
}

void dynamic_choice_registry::register_generator( const std::string &node_name, generator_fn fn )
{
    generators_[node_name].push_back( std::move( fn ) );
}

auto dynamic_choice_registry::generate( const std::string &node_name ) const -> std::vector<entry>
{
    auto it = generators_.find( node_name );
    if( it == generators_.end() ) {
        return {};
    }
    std::vector<entry> result;
    for( const auto &gen : it->second ) {
        try {
            auto entries = gen();
            std::ranges::move( entries, std::back_inserter( result ) );
        } catch( const std::exception &e ) {
            DebugLog( DL::Error, DC::Main )
                    << "yarn: dynamic_choice generator threw: " << e.what();
        }
    }
    return result;
}

// ============================================================
// yarn_runtime
// ============================================================

yarn_runtime::yarn_runtime( options opts )
    : story_( opts.story )
    , registry_( opts.registry )
    , npc_( opts.npc_ref )
    , player_( opts.player_ref )
    , dialogue_ref_( opts.dialogue_ref )
{
    node_stack_.push_back( { &story_, std::move( opts.starting_node ) } );
}

void yarn_runtime::run( dialogue_window &d_win )
{
    g_conv_ctx.d_win_ref = &d_win;

    // Resolves a jump/goto target string to a stack_frame.
    // Bare names resolve within the current frame's story.
    // "story::NodeName" looks up the named story from the global registry.
    auto resolve_frame = [&]( const std::string & target ) -> stack_frame {
        if( const auto sep = target.find( "::" ); sep != std::string::npos )
        {
            const auto sname = target.substr( 0, sep );
            const auto nname = target.substr( sep + 2 );
            if( !has_yarn_story( sname ) ) {
                DebugLog( DL::Error, DC::Main )
                        << "yarn: cross-story target '" << target << "': story '" << sname
                        << "' not found — staying put";
                return node_stack_.back();
            }
            return { &get_yarn_story( sname ), nname };
        }
        // Bare name: inherit current frame's story.
        return { node_stack_.back().story, target };
    };

    while( !node_stack_.empty() ) {
        const auto &frame = node_stack_.back();
        if( !frame.story->has_node( frame.node ) ) {
            DebugLog( DL::Error, DC::Main )
                    << "yarn: node '" << frame.node << "' not found";
            break;
        }
        const auto &node = frame.story->get_node( frame.node );
        auto result = execute_elements( node.elements, d_win );

        switch( result.kind ) {
            case signal::jump:
                // Replace current frame — standard <<jump>>, no implicit return
                node_stack_.back() = resolve_frame( result.target );
                break;
            case signal::goto_node:
                // Push: target executes and falls off end → pop back to here
                node_stack_.push_back( resolve_frame( result.target ) );
                break;
            case signal::yarn_return:
                node_stack_.pop_back();
                break;
            case signal::stop:
                node_stack_.clear();
                break;
            case signal::ok:
                // Fell off the end of the node — implicit <<return>>
                node_stack_.pop_back();
                break;
        }
    }
}

// Converts Yarn-style markup [color_X]text[/color_X] to CDDA tags <color_X>text</color_X>.
// Passes through existing <color_X> tags and any unrecognised [...] sequences unchanged.
static auto apply_yarn_markup( std::string text ) -> std::string
{
    std::string result;
    result.reserve( text.size() );
    std::size_t pos = 0;
    while( pos < text.size() ) {
        if( text[pos] == '[' ) {
            auto close = text.find( ']', pos + 1 );
            if( close != std::string::npos ) {
                auto tag = std::string_view( text ).substr( pos + 1, close - pos - 1 );
                if( tag.starts_with( "color_" ) || tag.starts_with( "/color_" ) ) {
                    result += '<';
                    result += tag;
                    result += '>';
                    pos = close + 1;
                    continue;
                }
            }
        }
        result += text[pos++];
    }
    return result;
}

void yarn_runtime::parse_dialogue_text( const node_element &elem, dialogue_window &d_win )
{
    auto text = apply_yarn_markup( interpolate( _( elem.text.c_str() ) ) );
    if( player_ && npc_ ) {
        parse_tags( text, *player_, *npc_, itype_id( g_conv_ctx.current_item_type ) );
    }

    if( elem.speaker == "__narrator" ) {
        // Narrator line — gray, prefixed with "- ", no speaker label, no quotes.
        d_win.add_to_history( colorize( "- " + text, c_light_gray ) );
    } else if( elem.speaker.empty() ) {
        // Unattributed continuation — white, no label, tight spacing.
        d_win.add_to_history( text, /*continuation=*/true );
    } else if( elem.speaker == "player" || elem.speaker == "You" || elem.speaker == "Player" ) {
        d_win.add_to_history( string_format( "%s: \"%s\"",
                                             colorize( _( "You" ), c_green ), text ) );
    } else {
        auto speaker_name = ( elem.speaker == "npc" || elem.speaker == "NPC" )
                            ? ( npc_ ? npc_->name : "NPC" )
                            : elem.speaker;
        d_win.add_to_history( string_format( "%s: \"%s\"",
                                             colorize( speaker_name, c_light_green ), text ) );
    }
}

auto yarn_runtime::execute_elements( const std::vector<node_element> &elements,
                                     dialogue_window &d_win ) -> exec_result
{
    for( const auto &elem : elements ) {
        switch( elem.type ) {
            case node_element::kind::dialogue: {
                parse_dialogue_text( elem, d_win );
                break;
            }

            case node_element::kind::line_group: {
                std::vector<node_element::choice> valid_choices;
                for( const auto &c : elem.choices ) {
                    if( c.condition ) {
                        try {
                            auto val = eval( *c.condition );
                            if( !std::holds_alternative<bool>( val ) || !std::get<bool>( val ) ) {
                                continue;
                            }
                        } catch( const std::exception &e ) {
                            DebugLog( DL::Error, DC::Main )
                                    << "yarn: line_group condition error: " << e.what();
                            continue;
                        }
                    }
                    valid_choices.push_back( c );
                }
                if( valid_choices.empty() ) { continue; }
                auto result = execute_elements( valid_choices[rng( 0, valid_choices.size() - 1 )].body, d_win );
                if( result.kind != signal::ok ) {
                    return result;
                }
                break;
            }

            case node_element::kind::choice_group: {
                // Helper: expands repeat groups into concrete choices.
                // Declared here so it can be re-run on each loop iteration.
                auto build_static_choices = [&]() {
                    std::vector<node_element::choice> out;
                    for( const auto &rg : elem.repeat_groups ) {
                        if( rg.condition ) {
                            try {
                                auto val = eval( *rg.condition );
                                if( !std::holds_alternative<bool>( val ) || !std::get<bool>( val ) ) {
                                    continue;
                                }
                            } catch( const std::exception &e ) {
                                DebugLog( DL::Error, DC::Main )
                                        << "yarn: repeat_group condition error: " << e.what();
                                continue;
                            }
                        }

                        player *actor = rg.is_npc
                                        ? ( npc_ ? static_cast<player *>( npc_ ) : nullptr )
                                        : player_;
                        if( !actor ) { continue; }

                        std::vector<itype_id> item_ids;
                        for( const auto &item_str : rg.for_item ) {
                            itype_id id( item_str );
                            if( actor->charges_of( id ) > 0 || actor->has_amount( id, 1 ) ) {
                                item_ids.push_back( id );
                            }
                        }
                        for( const auto &cat_str : rg.for_category ) {
                            item_category_id cat( cat_str );
                            auto matches = actor->items_with( [&cat, &rg]( const item & it ) {
                                if( rg.include_containers ) {
                                    return it.get_category().get_id() == cat;
                                }
                                return it.type && it.type->category_force == cat;
                            } );
                            for( const auto *it : matches ) {
                                if( !std::ranges::contains( item_ids, it->typeId() ) ) {
                                    item_ids.push_back( it->typeId() );
                                }
                            }
                        }

                        for( const auto &iid : item_ids ) {
                            node_element::choice ch;
                            ch.text = rg.text_template;
                            auto item_name = item::nname( iid, 1 );
                            for( std::size_t pos = 0;
                                 ( pos = ch.text.find( "<topic_item>", pos ) ) != std::string::npos; ) {
                                ch.text.replace( pos, 12, item_name );
                                pos += item_name.size();
                            }
                            node_element set_item;
                            set_item.type         = node_element::kind::command;
                            set_item.command_name = "_set_current_item";
                            expr_node id_lit;
                            id_lit.type        = expr_node::kind::literal;
                            id_lit.literal_val = iid.str();
                            set_item.command_args.push_back( std::move( id_lit ) );
                            ch.body.push_back( std::move( set_item ) );
                            ch.body.insert( ch.body.end(), rg.body.begin(), rg.body.end() );
                            out.push_back( std::move( ch ) );
                        }
                    }
                    out.insert( out.end(), elem.choices.begin(), elem.choices.end() );
                    return out;
                };

                const auto &cur_node_name = node_stack_.back().node;

                bool should_loop = true;
                while( should_loop ) {
                    should_loop = false;

                    auto static_choices = build_static_choices();
                    auto dyn_entries    = dynamic_choice_registry::global().generate( cur_node_name );
                    const auto static_count = static_cast<int>( static_choices.size() );

                    // Build the unified flat list (static first, then dynamic).
                    // Dynamic entries have no condition — the generator pre-filters them.
                    auto all_choices = static_choices;
                    for( const auto &dyn : dyn_entries ) {
                        node_element::choice ch;
                        ch.text = dyn.text;
                        ch.tail = dyn.tail;
                        all_choices.push_back( std::move( ch ) );
                    }

                    // Partition a display-order index rather than the choices themselves,
                    // so the static/dynamic boundary (static_count) stays valid for execution.
                    // Non-tail choices first (stable), then tail choices (stable).
                    std::vector<int> display_order( all_choices.size() );
                    std::iota( display_order.begin(), display_order.end(), 0 );
                    std::ranges::stable_partition( display_order,
                    [&]( int i ) { return !all_choices[i].tail; } );

                    std::vector<node_element::choice> display_choices;
                    display_choices.reserve( display_order.size() );
                    for( int idx : display_order ) {
                        display_choices.push_back( all_choices[idx] );
                    }

                    const int chosen_display = present_choices( display_choices, d_win );
                    if( chosen_display < 0 ) {
                        return { signal::stop, {} };
                    }
                    // Map display index back to original flat index for execution.
                    const int chosen = display_order[chosen_display];

                    d_win.mark_turn_start();

                    if( chosen < static_count ) {
                        // Static choice
                        const auto &ch = static_choices[chosen];
                        if( ch.echo_speech ) {
                            auto choice_text = apply_yarn_markup( interpolate( _( ch.text.c_str() ) ) );
                            if( player_ && npc_ ) {
                                parse_tags( choice_text, *player_, *npc_,
                                            itype_id( g_conv_ctx.current_item_type ) );
                            }
                            d_win.add_to_history(
                                string_format( "%s: \"%s\"",
                                               colorize( _( "You" ), c_green ),
                                               choice_text ) );
                        }
                        auto result = execute_elements( ch.body, d_win );
                        if( result.kind != signal::ok ) {
                            return result;
                        }
                    } else {
                        // Dynamic choice
                        const auto &dyn = dyn_entries[chosen - static_count];
                        const auto sig  = dyn.body();
                        if( sig == "stop" ) {
                            return { signal::stop, {} };
                        }
                        if( sig == "loop" ) {
                            should_loop = true;
                            continue;
                        }
                        if( !sig.empty() ) {
                            return { signal::jump, sig };
                        }
                        // empty = continue after the choice group
                    }
                }
                // ok → fall through to next element after the choice group
                break;
            }

            case node_element::kind::command: {
                auto &creg = command_registry::global();
                if( !creg.has_command( elem.command_name ) ) {
                    DebugLog( DL::Warn, DC::Main )
                            << "yarn: unknown command '" << elem.command_name << "'";
                    break;
                }
                std::vector<value> args;
                args.reserve( elem.command_args.size() );
                bool eval_ok = true;
                for( const auto &arg_node : elem.command_args ) {
                    try {
                        args.push_back( eval( arg_node ) );
                    } catch( const std::exception &e ) {
                        DebugLog( DL::Error, DC::Main )
                                << "yarn: command '" << elem.command_name
                                << "' arg eval error: " << e.what();
                        eval_ok = false;
                        break;
                    }
                }
                if( eval_ok ) {
                    auto sig = creg.call( elem.command_name, args );
                    if( sig == command_signal::stop ) {
                        return { signal::stop, {} };
                    }
                }
                break;
            }

            case node_element::kind::jump:
                return { signal::jump, elem.jump_target };

            case node_element::kind::goto_node:
                return { signal::goto_node, elem.jump_target };

            case node_element::kind::legacy_topic: {
                // DEPRECATED: Delegates to the JSON dialogue engine for one topic.
                // Remove this case when the JSON-to-Yarn migration is complete.
                if( !dialogue_ref_ || !npc_ || !player_ ) {
                    DebugLog( DL::Error, DC::Main )
                            << "yarn: legacy_topic requires dialogue/npc/player context";
                    return { signal::stop, {} };
                }
                auto &chatbin = npc_->chatbin;
                // Loop through chained legacy topics internally to avoid re-triggering this handler.
                // When the next topic is also a legacy stub we loop rather than returning signal::jump
                // (replace), which would discard the current frame and lose context needed for
                // correct mission-selection refresh on each iteration.
                auto current_topic = talk_topic( elem.jump_target );
                while( true ) {
                    // Refresh mission selection each iteration (the selected mission may change
                    // between topics, e.g. after accepting a mission the chatbin updates).
                    if( chatbin.mission_selected != nullptr &&
                        chatbin.mission_selected->get_assigned_player_id() != player_->getID() ) {
                        chatbin.mission_selected = nullptr;
                    }
                    if( chatbin.mission_selected == nullptr ) {
                        if( !chatbin.missions.empty() ) {
                            chatbin.mission_selected = chatbin.missions.front();
                        } else if( !dialogue_ref_->missions_assigned.empty() ) {
                            chatbin.mission_selected = dialogue_ref_->missions_assigned.front();
                        }
                    }
                    auto next = dialogue_ref_->opt( d_win, npc_->name, current_topic );
                    if( next.id == "TALK_DONE" ) {
                        dialogue_ref_->done = true;
                        return { signal::stop, {} };
                    }
                    if( next.id == "TALK_NONE" ) {
                        // Natural pop — return to the calling topic node.
                        return { signal::ok, {} };
                    }
                    // Group 99 meta-topics (O/L/S/Y hotkeys): opt() already displayed the info;
                    // call opt() once more to let the player pick "Okay.", then loop back to
                    // current_topic without replacing it. This preserves the previous topic.
                    static const std::unordered_set<std::string> s_group99 = {
                        "TALK_OPINION", "TALK_SIZE_UP", "TALK_LOOK_AT", "TALK_SHOUT"
                    };
                    if( s_group99.contains( next.id ) ) {
                        dialogue_ref_->opt( d_win, npc_->name, next );
                        continue;
                    }
                    // If the next topic is a legacy stub node, loop rather than replace the
                    // current frame, so the mission-selection refresh runs on each transition.
                    if( story_.has_node( next.id ) ) {
                        const auto &next_node = story_.get_node( next.id );
                        if( !next_node.elements.empty() &&
                            next_node.elements.front().type == node_element::kind::legacy_topic ) {
                            current_topic = std::move( next );
                            continue;
                        }
                    }
                    return { signal::jump, next.id };
                }
            }

            case node_element::kind::stop:
                return { signal::stop, {} };

            case node_element::kind::yarn_return:
                return { signal::yarn_return, {} };

            case node_element::kind::if_block: {
                bool cond = false;
                if( elem.condition ) {
                    try {
                        auto val = eval( *elem.condition );
                        if( !std::holds_alternative<bool>( val ) ) {
                            DebugLog( DL::Error, DC::Main )
                                    << "yarn: <<if>> condition did not evaluate to bool";
                        } else {
                            cond = std::get<bool>( val );
                        }
                    } catch( const std::exception &e ) {
                        DebugLog( DL::Error, DC::Main )
                                << "yarn: condition error: " << e.what();
                    }
                }
                const auto &body = cond ? elem.if_body : elem.else_body;
                auto result = execute_elements( body, d_win );
                if( result.kind != signal::ok ) {
                    return result;
                }
                break;
            }
        }
    }
    return { signal::ok, {} };
}

auto yarn_runtime::present_choices( const std::vector<node_element::choice> &choices,
                                    dialogue_window &d_win ) -> int
{
    // Filter choices by condition and build the display list.
    // The returned index is into the ORIGINAL choices vector.
    std::vector<int> available;
    for( int idx = 0; idx < static_cast<int>( choices.size() ); ++idx ) {
        const auto &ch = choices[idx];
        if( !ch.condition ) {
            available.push_back( idx );
            continue;
        }
        try {
            auto val = eval( *ch.condition );
            if( std::holds_alternative<bool>( val ) && std::get<bool>( val ) ) {
                available.push_back( idx );
            }
        } catch( const std::exception &e ) {
            DebugLog( DL::Error, DC::Main ) << "yarn: choice condition error: " << e.what();
        }
    }

    if( available.empty() ) {
        return -1;
    }

    // Build talk_data entries for the window
    std::vector<talk_data> response_lines;
    for( int i = 0; i < static_cast<int>( available.size() ); ++i ) {
        response_lines.push_back( {
            static_cast<char>( 'a' + i ),
            c_white,
            apply_yarn_markup( interpolate( _( choices[available[i]].text.c_str() ) ) )
        } );
    }

    size_t selected = 0;
    const auto response_count = response_lines.size();

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor & ui ) {
        d_win.resize_dialogue( ui );
    } );
    ui.mark_resize();
    ui.on_redraw( [&]( const ui_adaptor & ) {
        d_win.display_responses( response_lines, selected );
    } );

    int ch;
    do {
        d_win.refresh_response_display();
        do {
            ui_manager::redraw();
            ch = inp_mngr.get_input_event().get_first_input();
            if( ch == KEY_UP ) {
                selected = ( selected > 0 ) ? selected - 1 : response_count - 1;
                continue;
            }
            if( ch == KEY_DOWN ) {
                selected = ( selected + 1 < response_count ) ? selected + 1 : 0;
                continue;
            }
            if( ch == KEY_PPAGE || ch == KEY_NPAGE ) {
                auto scroll = d_win.handle_scrolling( ch );
                if( scroll ) {
                    selected = *scroll;
                }
                continue;
            }
            // Special always-available actions (mirrors dialogue::opt behaviour)
            if( ch == 'L' ) {
                if( npc_ ) {
                    d_win.add_to_history( npc_->short_description() );
                }
                continue;
            }
            if( ch == 'O' ) {
                if( npc_ ) {
                    d_win.add_to_history( npc_->opinion_text() );
                }
                continue;
            }
            if( ch == 'Y' ) {
                if( player_ ) {
                    player_->shout();
                    d_win.add_to_history( player_->is_deaf()
                                          ? _( "You yell, but can't hear yourself." )
                                          : _( "You yell." ) );
                }
                continue;
            }
            if( ch == 'S' ) {
                if( npc_ && player_ ) {
                    d_win.add_to_history( size_up_text( *npc_, *player_ ) );
                }
                continue;
            }
            if( ch == KEY_ENTER || ch == '\n' || ch == '\r' ) {
                ch = static_cast<int>( selected );
            } else {
                ch -= 'a';
            }
        } while( ch < 0 || ch >= static_cast<int>( response_count ) );
    } while( false );

    return available[ch];
}

auto yarn_runtime::eval( const expr_node &node ) const -> value
{
    return evaluate_expr( node, registry_ );
}

auto yarn_runtime::interpolate( std::string_view text ) const -> std::string
{
    return interpolate_text( text, registry_ );
}

// ============================================================
// Global story registry
// ============================================================

namespace
{

std::unordered_map<std::string, yarn_story> &story_registry()
{
    static std::unordered_map<std::string, yarn_story> reg;
    return reg;
}

// Collect all inject_into nodes across the registry and merge their choices into
// their declared targets.  Called once from load_yarn_stories() after all stories
// and shared_choices are resolved.
//
// Ordering within a target's choice_group after injection:
//   [ priority < 0, ascending ]  [ native choices ]  [ priority >= 0, ascending ]
// Within the same priority value, declaration (load) order is preserved.
void apply_injections( std::unordered_map<std::string, yarn_story> &registry )
{
    struct pending_injection {
        std::vector<node_element::choice> choices;
        int priority;
        std::string target_story;
        std::string target_node;
    };

    // Clone base_choices, qualify bare targets with source_story, then append a <<jump target_node>>.
    auto make_choices_with_return = []( const std::vector<node_element::choice> &base,
                                        const std::string & source_story,
    const std::string & target_node ) {
        auto cloned = base;
        for( auto &ch : cloned ) {
            qualify_elements( ch.body, source_story );
        }
        for( auto &ch : cloned ) {
            node_element return_jump;
            return_jump.type        = node_element::kind::jump;
            return_jump.jump_target = target_node;
            ch.body.push_back( std::move( return_jump ) );
        }
        return cloned;
    };

    std::vector<pending_injection> injections;
    for( const auto &[story_name, story] : registry ) {
        for( const auto &[node_name, node] : story.all_nodes() ) {
            if( node.inject_into.empty() ) {
                continue;
            }
            auto cg_it = std::ranges::find_if( node.elements, []( const node_element & e ) {
                return e.type == node_element::kind::choice_group;
            } );
            if( cg_it == node.elements.end() ) {
                DebugLog( DL::Error, DC::Main )
                        << "yarn inject_into on '" << node_name << "': node has no choice_group";
                continue;
            }
            if( cg_it->choices.empty() ) {
                continue;
            }
            const auto &base_choices = cg_it->choices;

            for( const auto &tgt : node.inject_into ) {
                if( tgt.is_direct ) {
                    // Direct injection: resolve story, bypass inject_block.
                    const std::string &ts = tgt.target_story.empty() ? story_name : tgt.target_story;
                    const std::string &tn = tgt.target_node;
                    injections.push_back( { make_choices_with_return( base_choices, story_name, tn ),
                                            node.inject_priority, ts, tn } );
                } else {
                    // Tag-based injection: find all nodes whose inject_tags satisfy the AND group.
                    // Filter out nodes that block this source's inject_category.
                    for( const auto &[ts2, story2] : registry ) {
                        for( const auto &[tn2, node2] : story2.all_nodes() ) {
                            // All required_tags must be present in the target's inject_tags.
                            bool matches = std::ranges::all_of( tgt.required_tags,
                            [&]( const std::string & t ) {
                                return std::ranges::contains( node2.inject_tags, t );
                            } );
                            if( !matches ) {
                                continue;
                            }
                            // Reject if any of our inject_category intersects target's inject_block.
                            bool blocked = std::ranges::any_of( node.inject_category,
                            [&]( const std::string & cat ) {
                                return std::ranges::contains( node2.inject_block, cat );
                            } );
                            if( blocked ) {
                                continue;
                            }
                            injections.push_back( { make_choices_with_return( base_choices, story_name, tn2 ),
                                                    node.inject_priority, ts2, tn2 } );
                        }
                    }
                }
            }
        }
    }

    if( injections.empty() ) {
        return;
    }

    // Sort by (target_story, target_node, priority) so we can process groups in one pass.
    std::ranges::stable_sort( injections, []( const pending_injection & a,
    const pending_injection & b ) {
        if( a.target_story != b.target_story ) { return a.target_story < b.target_story; }
        if( a.target_node  != b.target_node ) { return a.target_node  < b.target_node;  }
        return a.priority < b.priority;
    } );

    auto it = injections.begin();
    while( it != injections.end() ) {
        const std::string &ts = it->target_story;
        const std::string &tn = it->target_node;

        // Find end of this (target_story, target_node) group.
        auto group_end = std::ranges::find_if( it, injections.end(),
        [&ts, &tn]( const pending_injection & p ) {
            return p.target_story != ts || p.target_node != tn;
        } );

        // Locate the target node (mutable).
        auto sit = registry.find( ts );
        if( sit == registry.end() ) {
            DebugLog( DL::Error, DC::Main )
                    << "yarn inject_into: story '" << ts << "' not found";
            it = group_end;
            continue;
        }
        auto *target = sit->second.get_node_mutable( tn );
        if( target == nullptr ) {
            DebugLog( DL::Error, DC::Main )
                    << "yarn inject_into: node '" << tn << "' not found in story '" << ts << "'";
            it = group_end;
            continue;
        }
        auto cg_it = std::ranges::find_if( target->elements, []( const node_element & e ) {
            return e.type == node_element::kind::choice_group;
        } );
        if( cg_it == target->elements.end() ) {
            DebugLog( DL::Error, DC::Main )
                    << "yarn inject_into: target '" << tn << "' in '" << ts << "' has no choice_group";
            it = group_end;
            continue;
        }

        // Already sorted ascending by priority.  Split at the first non-negative value.
        auto split = std::ranges::find_if( it, group_end, []( const pending_injection & p ) {
            return p.priority >= 0;
        } );

        // Collect prepend choices (priority < 0) in ascending order → most-negative first.
        std::vector<node_element::choice> prepend;
        for( auto jt = it; jt != split; ++jt ) {
            prepend.insert( prepend.end(), jt->choices.begin(), jt->choices.end() );
        }

        // Collect append choices (priority >= 0) in ascending order → lowest first.
        std::vector<node_element::choice> append;
        for( auto jt = split; jt != group_end; ++jt ) {
            append.insert( append.end(), jt->choices.begin(), jt->choices.end() );
        }

        if( !prepend.empty() ) {
            cg_it->choices.insert( cg_it->choices.begin(), prepend.begin(), prepend.end() );
        }
        if( !append.empty() ) {
            cg_it->choices.insert( cg_it->choices.end(), append.begin(), append.end() );
        }

        it = group_end;
    }
}

} // anonymous namespace

void load_yarn_stories()
{
    const auto dir = PATH_INFO::datadir() + "dialogue/";
    const auto files = get_files_from_path( ".yarn", dir, false, true );

    for( const auto &path : files ) {
        auto [story, result] = yarn_story::from_file( path );

        // Derive story stem here so it is available in all branches.
        auto slash = path.rfind( '/' );
        if( slash == std::string::npos ) {
            slash = path.rfind( '\\' );
        }
        auto stem_start = ( slash == std::string::npos ) ? 0 : slash + 1;
        auto dot = path.rfind( '.' );
        auto stem = path.substr( stem_start, dot - stem_start );

        if( result.ok() ) {
            story_registry().emplace( stem, std::move( story ) );
            DebugLog( DL::Info, DC::Dialogue ) << "yarn: loaded story '" << stem << "'";
            if( !result.warnings.empty() ) {
                std::string summary;
                for( const auto &w : result.warnings ) {
                    DebugLog( DL::Warn, DC::Main ) << "yarn: " << w;
                    summary += w + "\n";
                }
                debugmsg( "Yarn dialogue warnings in '%s' (affected elements skipped):\n\n%s",
                          path.c_str(), summary.c_str() );
            }
        } else {
            for( const auto &err : result.errors ) {
                DebugLog( DL::Error, DC::Main ) << "yarn: " << err;
            }
            DebugLog( DL::Error, DC::Main )
                    << "yarn: failed to load '" << path << "' (" << result.errors.size() << " errors)";
            std::string summary;
            for( const auto &err : result.errors ) {
                summary += err + "\n";
            }
            debugmsg( "Yarn dialogue failed to load '%s':\n\n%s", path.c_str(), summary.c_str() );
        }
    }

    // Resolve shared_choices across all loaded stories now that the full registry is populated.
    // Lookup function resolves both same-file bare names and cross-file "story::NodeName".
    // Base nodes must be leaf nodes (no shared_choices of their own); chaining is an error.
    yarn_story::node_lookup_fn lookup = []( const std::string & story_name,
    const std::string & node_name ) -> const yarn_node * {
        auto &reg = story_registry();
        auto sit = reg.find( story_name );
        if( sit == reg.end() )
        {
            return nullptr;
        }
        return sit->second.has_node( node_name ) ? &sit->second.get_node( node_name ) : nullptr;
    };

    for( auto &[name, story] : story_registry() ) {
        std::vector<std::string> errors;
        story.resolve_shared_choices( lookup, name, errors );
        for( const auto &err : errors ) {
            DebugLog( DL::Error, DC::Main ) << "yarn shared_choices: " << err;
        }
    }

    // Apply inject_into contributions after shared_choices are resolved, so injected
    // choices benefit from any shared_choices already merged into their source nodes.
    apply_injections( story_registry() );
}

auto has_yarn_story( const std::string &name ) -> bool
{
    return story_registry().contains( name );
}

auto get_yarn_story( const std::string &name ) -> const yarn_story &
{
    return story_registry().at( name );
}

void build_legacy_yarn_stories()
{
    // Build the __legacy story from converted TALK_TOPIC JSON nodes.
    // Each node is a real yarn AST (choice_group, if_block, commands, jumps).
    // Special topics that need old-system UI keep legacy_topic stub elements.
    // DEPRECATED: Remove when JSON-to-Yarn migration is complete.
    auto converted = dialogue_convert::flush_pending_nodes();
    if( converted.empty() ) {
        return;
    }

    yarn_story legacy;

    // Add synthetic nodes for the two most common special topics.
    {
        yarn_node done_node;
        done_node.title = "TALK_DONE";
        node_element bye;
        bye.type = node_element::kind::dialogue;
        bye.text = "Bye.";
        done_node.elements.push_back( std::move( bye ) );
        node_element stop_elem;
        stop_elem.type = node_element::kind::stop;
        done_node.elements.push_back( std::move( stop_elem ) );
        legacy.add_node( std::move( done_node ) );
    }
    {
        yarn_node none_node;
        none_node.title = "TALK_NONE";
        node_element ret;
        ret.type = node_element::kind::yarn_return;
        none_node.elements.push_back( std::move( ret ) );
        legacy.add_node( std::move( none_node ) );
    }

    for( auto &node : converted ) {
        legacy.add_node( std::move( node ) );
    }
    story_registry().emplace( "__legacy", std::move( legacy ) );
}

// Convert a yarn value to a string suitable for Creature::set_value storage.
// Numbers that are whole integers are stored without a decimal point.
static auto value_to_storage_string( const value &v ) -> std::string
{
    return std::visit( []( const auto & x ) -> std::string {
        using T = std::decay_t<decltype( x )>;
        if constexpr( std::is_same_v<T, bool> )
        {
            return x ? "true" : "false";
        } else if constexpr( std::is_same_v<T, double> )
        {
            if( x == std::floor( x ) && std::abs( x ) < 1e15 ) {
                return std::to_string( static_cast<long long>( x ) );
            }
            return std::to_string( x );
        } else
        {
            return x;
        }
    }, v );
}

// ============================================================
// Built-in functions
// ============================================================

void register_builtin_functions( func_registry &reg )
{
    using vt = value_type;

    // Player-side predicates and getters (u_ prefix)

    reg.add( "u_has_trait", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && p->has_trait( trait_id( std::get<std::string>( args[0] ) ) );
    } );

    reg.add( "u_has_effect", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && p->has_effect( efftype_id( std::get<std::string>( args[0] ) ) );
    } );

    reg.add( "u_has_bionic", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && p->has_bionic( bionic_id( std::get<std::string>( args[0] ) ) );
    } );

    reg.add( "u_has_item", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && p->has_item_with_id( itype_id( std::get<std::string>( args[0] ) ) );
    } );

    reg.add( "u_has_items", {vt::string, vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p )
        {
            return false;
        }
        auto id    = itype_id( std::get<std::string>( args[0] ) );
        auto count = static_cast<int>( std::get<double>( args[1] ) );
        return static_cast<int>( p->all_items_with_id( id ).size() ) >= count;
    } );

    reg.add( "u_has_skill", {vt::string, vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p )
        {
            return false;
        }
        auto level = static_cast<int>( std::get<double>( args[1] ) );
        return p->get_skill_level( skill_id( std::get<std::string>( args[0] ) ) ) >= level;
    } );

    reg.add( "u_get_str", {}, vt::number,
    []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p ? static_cast<double>( p->get_str() ) : 0.0;
    } );

    reg.add( "u_get_dex", {}, vt::number,
    []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p ? static_cast<double>( p->get_dex() ) : 0.0;
    } );

    reg.add( "u_get_int", {}, vt::number,
    []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p ? static_cast<double>( p->get_int() ) : 0.0;
    } );

    reg.add( "u_get_per", {}, vt::number,
    []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p ? static_cast<double>( p->get_per() ) : 0.0;
    } );

    reg.add( "u_name", {}, vt::string,
    []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p ? p->name : std::string{};
    } );

    // NPC-side predicates and getters (npc_ prefix)

    reg.add( "npc_has_trait", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->has_trait( trait_id( std::get<std::string>( args[0] ) ) );
    } );

    reg.add( "npc_is_following", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->is_following();
    } );

    reg.add( "npc_is_ally", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        auto att = n->get_attitude();
        return att == NPCATT_FOLLOW || att == NPCATT_LEAD || att == NPCATT_HEAL;
    } );

    reg.add( "npc_is_enemy", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        auto att = n->get_attitude();
        return att == NPCATT_KILL || att == NPCATT_MUG || att == NPCATT_WAIT_FOR_LEAVE;
    } );

    reg.add( "npc_name", {}, vt::string,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n ? n->name : std::string{};
    } );

    reg.add( "npc_trust", {}, vt::number,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n ? static_cast<double>( n->op_of_u.trust ) : 0.0;
    } );

    reg.add( "npc_get_str", {}, vt::number,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n ? static_cast<double>( n->get_str() ) : 0.0;
    } );

    reg.add( "npc_get_dex", {}, vt::number,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n ? static_cast<double>( n->get_dex() ) : 0.0;
    } );

    reg.add( "npc_get_int", {}, vt::number,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n ? static_cast<double>( n->get_int() ) : 0.0;
    } );

    reg.add( "npc_get_per", {}, vt::number,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n ? static_cast<double>( n->get_per() ) : 0.0;
    } );

    reg.add( "npc_has_effect", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->has_effect( efftype_id( std::get<std::string>( args[0] ) ) );
    } );

    reg.add( "npc_has_bionic", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->has_bionic( bionic_id( std::get<std::string>( args[0] ) ) );
    } );

    reg.add( "npc_has_skill", {vt::string, vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        auto level = static_cast<int>( std::get<double>( args[1] ) );
        return n->get_skill_level( skill_id( std::get<std::string>( args[0] ) ) ) >= level;
    } );

    reg.add( "npc_get_skill", {vt::string}, vt::number,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n ? static_cast<double>( n->get_skill_level( skill_id( std::get<std::string>( args[0] ) ) ) ) : 0.0;
    } );

    // Arbitrary character variables (stored via Creature::set_value)

    reg.add( "u_get_var", {vt::string}, vt::string,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p ? p->get_value( std::get<std::string>( args[0] ) ) : std::string{};
    } );

    reg.add( "u_get_var_num", {vt::string}, vt::number,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p )
        {
            return 0.0;
        }
        const auto &s = p->get_value( std::get<std::string>( args[0] ) );
        try { return s.empty() ? 0.0 : static_cast<double>( std::stoll( s ) ); }
        catch( const std::exception & ) { return 0.0; }
    } );

    reg.add( "npc_get_var", {vt::string}, vt::string,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n ? n->get_value( std::get<std::string>( args[0] ) ) : std::string{};
    } );

    reg.add( "npc_get_var_num", {vt::string}, vt::number,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return 0.0;
        }
        const auto &s = n->get_value( std::get<std::string>( args[0] ) );
        try { return s.empty() ? 0.0 : static_cast<double>( std::stoll( s ) ); }
        catch( const std::exception & ) { return 0.0; }
    } );

    // ============================================================
    // Gender
    // ============================================================

    reg.add( "u_male",   {}, vt::boolean, []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && p->male;
    } );
    reg.add( "u_female", {}, vt::boolean, []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && !p->male;
    } );
    reg.add( "npc_male",   {}, vt::boolean, []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->male;
    } );
    reg.add( "npc_female", {}, vt::boolean, []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && !n->male;
    } );

    // ============================================================
    // Equipment and inventory
    // ============================================================

    reg.add( "u_is_wearing", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && p->is_wearing( itype_id( std::get<std::string>( args[0] ) ) );
    } );

    reg.add( "npc_is_wearing", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->is_wearing( itype_id( std::get<std::string>( args[0] ) ) );
    } );

    reg.add( "u_has_weapon", {}, vt::boolean, []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && p->is_armed();
    } );

    reg.add( "npc_has_weapon", {}, vt::boolean, []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->is_armed();
    } );

    reg.add( "npc_has_item", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->has_item_with_id( itype_id( std::get<std::string>( args[0] ) ) );
    } );

    reg.add( "npc_has_items", {vt::string, vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        auto id    = itype_id( std::get<std::string>( args[0] ) );
        auto count = static_cast<int>( std::get<double>( args[1] ) );
        return static_cast<int>( n->all_items_with_id( id ).size() ) >= count;
    } );

    // ============================================================
    // Traits and mutations
    // ============================================================

    reg.add( "u_has_trait_flag", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && p->has_trait_flag( trait_flag_str_id( std::get<std::string>( args[0] ) ) );
    } );

    reg.add( "npc_has_trait_flag", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->has_trait_flag( trait_flag_str_id( std::get<std::string>( args[0] ) ) );
    } );

    // ============================================================
    // Needs (hunger / thirst / fatigue as numbers for threshold comparison)
    // ============================================================
    // hunger:  (max_stored_kcal - stored_kcal) / 10  — matches the legacy condition formula
    // thirst / fatigue: direct getters

    reg.add( "u_get_hunger", {}, vt::number, []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p ? static_cast<double>( ( p->max_stored_kcal() - p->get_stored_kcal() ) / 10 )
        : 0.0;
    } );

    reg.add( "u_get_thirst", {}, vt::number, []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p ? static_cast<double>( p->get_thirst() ) : 0.0;
    } );

    reg.add( "u_get_fatigue", {}, vt::number, []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p ? static_cast<double>( p->get_fatigue() ) : 0.0;
    } );

    reg.add( "npc_get_hunger", {}, vt::number, []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n ? static_cast<double>( ( n->max_stored_kcal() - n->get_stored_kcal() ) / 10 )
        : 0.0;
    } );

    reg.add( "npc_get_thirst", {}, vt::number, []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n ? static_cast<double>( n->get_thirst() ) : 0.0;
    } );

    reg.add( "npc_get_fatigue", {}, vt::number, []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n ? static_cast<double>( n->get_fatigue() ) : 0.0;
    } );

    // ============================================================
    // Economy
    // ============================================================

    reg.add( "u_get_ecash", {}, vt::number, []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p ? static_cast<double>( p->cash ) : 0.0;
    } );

    // How much the NPC owes the player (use: u_get_owed() >= 100)
    reg.add( "u_get_owed", {}, vt::number, []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n ? static_cast<double>( n->op_of_u.owed ) : 0.0;
    } );

    // ============================================================
    // NPC state
    // ============================================================

    reg.add( "npc_available", {}, vt::boolean, []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && !n->has_effect( efftype_id( "currently_busy" ) );
    } );

    reg.add( "npc_is_riding", {}, vt::boolean, []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->is_mounted();
    } );

    reg.add( "npc_has_activity", {}, vt::boolean, []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && static_cast<bool>( n->activity );
    } );

    reg.add( "npc_has_class", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->myclass == npc_class_id( std::get<std::string>( args[0] ) );
    } );

    // Convenience aliases matching the legacy simple-string condition names.
    reg.add( "npc_friend", {}, vt::boolean, []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->is_player_ally();
    } );
    reg.add( "npc_hostile", {}, vt::boolean, []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        auto att = n->get_attitude();
        return att == NPCATT_KILL || att == NPCATT_MUG || att == NPCATT_WAIT_FOR_LEAVE;
    } );

    // ============================================================
    // Mission state (read from npc.chatbin)
    // ============================================================

    reg.add( "has_no_assigned_mission", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->chatbin.missions_assigned.empty();
    } );

    reg.add( "has_assigned_mission", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->chatbin.missions_assigned.size() == 1;
    } );

    reg.add( "has_many_assigned_missions", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->chatbin.missions_assigned.size() > 1;
    } );

    reg.add( "has_no_available_mission", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->chatbin.missions.empty();
    } );

    reg.add( "has_available_mission", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->chatbin.missions.size() == 1;
    } );

    reg.add( "has_many_available_missions", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->chatbin.missions.size() > 1;
    } );

    reg.add( "mission_complete", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        auto *p = g_conv_ctx.player_ref;
        if( !n || !p || !n->chatbin.mission_selected )
        {
            return false;
        }
        return n->chatbin.mission_selected->is_complete( p->getID() );
    } );

    reg.add( "mission_incomplete", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        auto *p = g_conv_ctx.player_ref;
        if( !n || !p || !n->chatbin.mission_selected )
        {
            return false;
        }
        return !n->chatbin.mission_selected->is_complete( p->getID() );
    } );

    // ============================================================
    // Time and calendar
    // ============================================================

    // Returns the current season as a lowercase string: "spring", "summer", "autumn", "winter".
    reg.add( "get_season", {}, vt::string, []( const std::vector<value> & ) -> value {
        switch( season_of_year( calendar::turn ) )
        {
            case SPRING:
                return std::string( "spring" );
            case SUMMER:
                return std::string( "summer" );
            case AUTUMN:
                return std::string( "autumn" );
            case WINTER:
                return std::string( "winter" );
            default:
                return std::string( "spring" );
        }
    } );

    // True if the current season matches the given string ("spring", "summer", "autumn", "winter").
    reg.add( "is_season", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        const auto &s = std::get<std::string>( args[0] );
        switch( season_of_year( calendar::turn ) )
        {
            case SPRING:
                return s == "spring";
            case SUMMER:
                return s == "summer";
            case AUTUMN:
                return s == "autumn";
            case WINTER:
                return s == "winter";
            default:
                return false;
        }
    } );

    // Integer number of days elapsed since the start of the cataclysm.
    reg.add( "days_since_cataclysm", {}, vt::number,
    []( const std::vector<value> & ) -> value {
        return static_cast<double>(
            to_days<int>( calendar::turn - calendar::start_of_cataclysm ) );
    } );

    // True if it is currently daytime (sun above horizon).
    reg.add( "is_day", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        return is_day( calendar::turn );
    } );

    // ============================================================
    // Driving
    // ============================================================

    reg.add( "u_driving", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p )
        {
            return false;
        }
        if( const optional_vpart_position vp = get_map().veh_at( p->pos() ) )
        {
            return vp->vehicle().is_moving() && vp->vehicle().player_in_control( *p );
        }
        return false;
    } );

    reg.add( "npc_driving", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        if( const optional_vpart_position vp = get_map().veh_at( n->pos() ) )
        {
            return vp->vehicle().is_moving() && vp->vehicle().player_in_control( *n );
        }
        return false;
    } );

    // ============================================================
    // NPC rule flags (ally_rule)
    // ============================================================
    // npc_has_rule("use_guns")        — rule is active (considering overrides)
    // npc_has_override("use_guns")    — an override is set for this rule

    reg.add( "npc_has_rule", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        const auto &key = std::get<std::string>( args[0] );
        auto it = ally_rule_strs.find( key );
        if( it == ally_rule_strs.end() )
        {
            DebugLog( DL::Warn, DC::Main )
                    << "yarn: npc_has_rule: unknown rule '" << key << "'";
            return false;
        }
        return n->rules.has_flag( it->second.rule );
    } );

    reg.add( "npc_has_override", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        const auto &key = std::get<std::string>( args[0] );
        auto it = ally_rule_strs.find( key );
        if( it == ally_rule_strs.end() )
        {
            DebugLog( DL::Warn, DC::Main )
                    << "yarn: npc_has_override: unknown rule '" << key << "'";
            return false;
        }
        return n->rules.has_override_enable( it->second.rule );
    } );

    // ============================================================
    // NPC combat/behaviour rules (aim, engagement, CBM)
    // ============================================================
    // npc_aim_rule("AIM_PRECISE")             — checks rules.aim
    // npc_engagement_rule("ENGAGE_ALL")       — checks rules.engagement
    // npc_cbm_reserve_rule("CBM_RESERVE_ALL") — checks rules.cbm_reserve
    // npc_cbm_recharge_rule("CBM_RECHARGE_ALWAYS") — checks rules.cbm_recharge

    reg.add( "npc_aim_rule", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        const auto &key = std::get<std::string>( args[0] );
        auto it = aim_rule_strs.find( key );
        if( it == aim_rule_strs.end() )
        {
            DebugLog( DL::Warn, DC::Main )
                    << "yarn: npc_aim_rule: unknown rule '" << key << "'";
            return false;
        }
        return n->rules.aim == it->second;
    } );

    reg.add( "npc_engagement_rule", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        const auto &key = std::get<std::string>( args[0] );
        auto it = combat_engagement_strs.find( key );
        if( it == combat_engagement_strs.end() )
        {
            DebugLog( DL::Warn, DC::Main )
                    << "yarn: npc_engagement_rule: unknown rule '" << key << "'";
            return false;
        }
        return n->rules.engagement == it->second;
    } );

    reg.add( "npc_cbm_reserve_rule", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        const auto &key = std::get<std::string>( args[0] );
        auto it = cbm_reserve_strs.find( key );
        if( it == cbm_reserve_strs.end() )
        {
            DebugLog( DL::Warn, DC::Main )
                    << "yarn: npc_cbm_reserve_rule: unknown rule '" << key << "'";
            return false;
        }
        return n->rules.cbm_reserve == it->second;
    } );

    reg.add( "npc_cbm_recharge_rule", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        const auto &key = std::get<std::string>( args[0] );
        auto it = cbm_recharge_strs.find( key );
        if( it == cbm_recharge_strs.end() )
        {
            DebugLog( DL::Warn, DC::Main )
                    << "yarn: npc_cbm_recharge_rule: unknown rule '" << key << "'";
            return false;
        }
        return n->rules.cbm_recharge == it->second;
    } );

    // ============================================================
    // Location
    // ============================================================

    // True if the NPC (the "beta" in legacy terms) is outdoors.
    reg.add( "is_outside", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        map &here = get_map();
        return !here.has_flag( TFLAG_INDOORS, here.getabs( n->pos() ) );
    } );

    // True if the NPC is in a safe-space overmap tile.
    reg.add( "at_safe_space", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        return get_overmapbuffer( n->get_dimension() ).is_safe( n->global_omt_location() );
    } );

    // True if the player is standing on the given overmap terrain type.
    reg.add( "u_at_om_location", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p )
        {
            return false;
        }
        const auto &loc = std::get<std::string>( args[0] );
        const oter_id &ter = get_overmapbuffer( p->get_dimension() ).ter( p->global_omt_location() );
        return ter == oter_id( oter_no_dir( oter_id( loc ) ) );
    } );

    // True if the NPC is standing on the given overmap terrain type.
    reg.add( "npc_at_om_location", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n )
        {
            return false;
        }
        const auto &loc = std::get<std::string>( args[0] );
        const oter_id &ter = get_overmapbuffer( n->get_dimension() ).ter( n->global_omt_location() );
        return ter == oter_id( oter_no_dir( oter_id( loc ) ) );
    } );

    // ============================================================
    // World NPC queries (requires live game)
    // ============================================================

    // True if any nearby NPC has the given companion mission role.
    reg.add( "npc_role_nearby", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p || !g )
        {
            return false;
        }
        const auto &role = std::get<std::string>( args[0] );
        auto npcs = g->get_npcs_if( [&]( const npc & guy )
        {
            return p->posz() == guy.posz()
            && guy.companion_mission_role_id == role
            && rl_dist( p->pos(), guy.pos() ) <= 48;
        } );
        return !npcs.empty();
    } );

    // True if the player has at least the given number of NPC allies.
    reg.add( "npc_allies", {vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        if( !g )
        {
            return false;
        }
        auto min_allies = static_cast<std::size_t>( std::get<double>( args[0] ) );
        return g->allies().size() >= min_allies;
    } );

    // ============================================================
    // NPC training availability
    // ============================================================

    reg.add( "npc_train_skills", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        auto *p = g_conv_ctx.player_ref;
        return n && p && !n->skills_offered_to( *p ).empty();
    } );

    reg.add( "npc_train_styles", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        auto *p = g_conv_ctx.player_ref;
        return n && p && !n->styles_offered_to( *p ).empty();
    } );

    // ============================================================
    // Stolen items
    // ============================================================

    reg.add( "u_has_stolen_item", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        auto *n = g_conv_ctx.npc_ref;
        if( !p || !n )
        {
            return false;
        }
        return std::ranges::any_of( p->inv_dump(),
        [n]( const item * it ) { return it->is_old_owner( *n, true ); } );
    } );

    // ============================================================
    // Missions (player)
    // ============================================================

    // True if the player has an active mission of the given type ID.
    reg.add( "u_has_mission", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p )
        {
            return false;
        }
        auto *av = dynamic_cast<avatar *>( p );
        if( !av )
        {
            return false;
        }
        auto target = mission_type_id( std::get<std::string>( args[0] ) );
        return std::ranges::any_of( av->get_active_missions(),
        [&target]( mission * m ) { return m->mission_id() == target; } );
    } );

    // ============================================================
    // Recipe knowledge
    // ============================================================

    reg.add( "u_know_recipe", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p )
        {
            return false;
        }
        const recipe &r = recipe_id( std::get<std::string>( args[0] ) ).obj();
        return p->knows_recipe( &r );
    } );

    // ============================================================
    // NPC following / stow checks
    // ============================================================

    reg.add( "npc_following", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->is_following();
    } );

    reg.add( "u_can_stow_weapon", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && !p->unarmed_attack() && p->can_pick_volume( p->primary_weapon() );
    } );

    reg.add( "npc_can_stow_weapon", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && !n->unarmed_attack() && n->can_pick_volume( n->primary_weapon() );
    } );

    reg.add( "has_pickup_list", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && !n->rules.pickup_whitelist->empty();
    } );

    // ============================================================
    // Any-trait (variadic)
    // ============================================================

    reg.register_func( {
        .name        = "u_has_any_trait",
        .param_types = {},
        .return_type = vt::boolean,
        .variadic    = true,
        .impl        = []( const std::vector<value> &args ) -> value {
            auto *p = g_conv_ctx.player_ref;
            if( !p ) { return false; }
            return std::ranges::any_of( args, [p]( const value & v )
            {
                return p->has_trait( trait_id( std::get<std::string>( v ) ) );
            } );
        }
    } );

    reg.register_func( {
        .name        = "npc_has_any_trait",
        .param_types = {},
        .return_type = vt::boolean,
        .variadic    = true,
        .impl        = []( const std::vector<value> &args ) -> value {
            auto *n = g_conv_ctx.npc_ref;
            if( !n ) { return false; }
            return std::ranges::any_of( args, [n]( const value & v )
            {
                return n->has_trait( trait_id( std::get<std::string>( v ) ) );
            } );
        }
    } );

    // ============================================================
    // Attribute minimum checks
    // ============================================================

    reg.add( "u_has_strength", {vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && p->str_cur >= static_cast<int>( std::get<double>( args[0] ) );
    } );
    reg.add( "npc_has_strength", {vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->str_cur >= static_cast<int>( std::get<double>( args[0] ) );
    } );
    reg.add( "u_has_dexterity", {vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && p->dex_cur >= static_cast<int>( std::get<double>( args[0] ) );
    } );
    reg.add( "npc_has_dexterity", {vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->dex_cur >= static_cast<int>( std::get<double>( args[0] ) );
    } );
    reg.add( "u_has_intelligence", {vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && p->int_cur >= static_cast<int>( std::get<double>( args[0] ) );
    } );
    reg.add( "npc_has_intelligence", {vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->int_cur >= static_cast<int>( std::get<double>( args[0] ) );
    } );
    reg.add( "u_has_perception", {vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        return p && p->per_cur >= static_cast<int>( std::get<double>( args[0] ) );
    } );
    reg.add( "npc_has_perception", {vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && n->per_cur >= static_cast<int>( std::get<double>( args[0] ) );
    } );

    // ============================================================
    // Needs: u_need(type, amount) — fatigue/hunger/thirst threshold
    // ============================================================

    reg.add( "u_need", {vt::string, vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p ) { return false; }
        const auto &need   = std::get<std::string>( args[0] );
        auto        amount = static_cast<int>( std::get<double>( args[1] ) );
        int effective_hunger = ( p->max_stored_kcal() - p->get_stored_kcal() ) / 10;
        return ( need == "fatigue" && p->get_fatigue() > amount ) ||
        ( need == "hunger"  && effective_hunger > amount ) ||
        ( need == "thirst"  && p->get_thirst() > amount );
    } );

    reg.add( "npc_need", {vt::string, vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) { return false; }
        auto *actor        = static_cast<player *>( n );
        const auto &need   = std::get<std::string>( args[0] );
        auto        amount = static_cast<int>( std::get<double>( args[1] ) );
        int effective_hunger = ( actor->max_stored_kcal() - actor->get_stored_kcal() ) / 10;
        return ( need == "fatigue" && actor->get_fatigue() > amount ) ||
        ( need == "hunger"  && effective_hunger > amount ) ||
        ( need == "thirst"  && actor->get_thirst() > amount );
    } );

    // ============================================================
    // Item category checks
    // ============================================================

    reg.add( "u_has_item_category", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p ) { return false; }
        item_category_id cat( std::get<std::string>( args[0] ) );
        return !p->items_with( [cat]( const item & it )
        {
            return it.type && it.type->category_force == cat;
        } ).empty();
    } );

    reg.add( "npc_has_item_category", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) { return false; }
        item_category_id cat( std::get<std::string>( args[0] ) );
        return !n->items_with( [cat]( const item & it )
        {
            return it.type && it.type->category_force == cat;
        } ).empty();
    } );

    // ============================================================
    // NPC character variables
    // ============================================================

    // talk var name construction: "npctalk_var[_type][_context]_name"
    auto make_talk_varname = []( const std::string & name,
                                 const std::string & type,
    const std::string & context ) -> std::string {
        return "npctalk_var" +
        ( type.empty()    ? std::string{} : "_" + type ) +
        ( context.empty() ? std::string{} : "_" + context ) +
        "_" + name;
    };

    reg.add( "u_has_var", {vt::string, vt::string, vt::string, vt::string}, vt::boolean,
    [make_talk_varname]( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p ) { return false; }
        auto varname = make_talk_varname(
            std::get<std::string>( args[0] ),
            std::get<std::string>( args[1] ),
            std::get<std::string>( args[2] ) );
        return p->get_value( varname ) == std::get<std::string>( args[3] );
    } );

    reg.add( "npc_has_var", {vt::string, vt::string, vt::string, vt::string}, vt::boolean,
    [make_talk_varname]( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) { return false; }
        auto varname = make_talk_varname(
            std::get<std::string>( args[0] ),
            std::get<std::string>( args[1] ),
            std::get<std::string>( args[2] ) );
        return n->get_value( varname ) == std::get<std::string>( args[3] );
    } );

    reg.add( "u_compare_var", {vt::string, vt::string, vt::string, vt::string, vt::number}, vt::boolean,
    [make_talk_varname]( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p ) { return false; }
        auto varname = make_talk_varname(
            std::get<std::string>( args[0] ),
            std::get<std::string>( args[1] ),
            std::get<std::string>( args[2] ) );
        const auto &op      = std::get<std::string>( args[3] );
        auto        compare = static_cast<int>( std::get<double>( args[4] ) );
        const auto &stored  = p->get_value( varname );
        int current = 0;
        try { current = stored.empty() ? 0 : std::stoi( stored ); }
        catch( const std::exception & ) {}
        if( op == "==" ) { return current == compare; }
        if( op == "!=" ) { return current != compare; }
        if( op == "<" ) { return current <  compare; }
        if( op == ">" ) { return current >  compare; }
        if( op == "<=" ) { return current <= compare; }
        if( op == ">=" ) { return current >= compare; }
        return false;
    } );

    reg.add( "npc_compare_var", {vt::string, vt::string, vt::string, vt::string, vt::number},
             vt::boolean,
    [make_talk_varname]( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) { return false; }
        auto varname = make_talk_varname(
            std::get<std::string>( args[0] ),
            std::get<std::string>( args[1] ),
            std::get<std::string>( args[2] ) );
        const auto &op      = std::get<std::string>( args[3] );
        auto        compare = static_cast<int>( std::get<double>( args[4] ) );
        const auto &stored  = n->get_value( varname );
        int current = 0;
        try { current = stored.empty() ? 0 : std::stoi( stored ); }
        catch( const std::exception & ) {}
        if( op == "==" ) { return current == compare; }
        if( op == "!=" ) { return current != compare; }
        if( op == "<" ) { return current <  compare; }
        if( op == ">" ) { return current >  compare; }
        if( op == "<=" ) { return current <= compare; }
        if( op == ">=" ) { return current >= compare; }
        return false;
    } );

    // ============================================================
    // Mission queries
    // ============================================================

    reg.add( "mission_has_generic_rewards", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) { return false; }
        mission *miss = n->chatbin.mission_selected;
        return miss && miss->has_generic_rewards();
    } );

    reg.add( "mission_goal", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) { return false; }
        mission *miss = n->chatbin.mission_selected;
        if( !miss ) { return false; }
        const auto &goal_str = std::get<std::string>( args[0] );
        const mission_goal mgoal = io::string_to_enum<mission_goal>( goal_str );
        return miss->get_type().goal == mgoal;
    } );

    // ============================================================
    // Dialogue context queries
    // ============================================================

    reg.add( "is_by_radio", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        return get_avatar().dialogue_by_radio;
    } );

    // has_reason() — true if a <<set_reason>> command has been called this conversation
    reg.add( "has_reason", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        return !g_conv_ctx.reason.empty();
    } );

    // once("key") — internal runtime backing for <<once>> blocks.
    // Key is a fully-qualified string generated at parse time by make_once_condition.
    // Returns true the first time this key is seen; stores "1" in player vars to mark seen.
    reg.add( "once", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p ) { return false; }
        const auto &key = std::get<std::string>( args[0] );
        if( !p->get_value( key ).empty() )
        {
            return false;
        }
        p->set_value( key, "1" );
        return true;
    } );

    // Unimplemented in original code — condition.cpp returns false for these.
    reg.add( "mission_failed",      {}, vt::boolean,
             []( const std::vector<value> & ) -> value { return false; } );
    reg.add( "npc_has_destination", {}, vt::boolean,
             []( const std::vector<value> & ) -> value { return false; } );
    reg.add( "asked_for_item",      {}, vt::boolean,
             []( const std::vector<value> & ) -> value { return false; } );

    reg.add( "npc_service", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n && !n->has_effect( effect_currently_busy );
    } );

    // ============================================================
    // Trial roll — dice-based social check.
    // trial_roll(type_str, difficulty) → bool
    // Types: PERSUADE, LIE, INTIMIDATE
    // ============================================================

    reg.add( "trial_roll", {vt::string, vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        auto *n = g_conv_ctx.npc_ref;
        if( !p || !n ) { return false; }

        const auto &type_str = std::get<std::string>( args[0] );
        int difficulty = static_cast<int>( std::get<double>( args[1] ) );

        if( p->has_trait( trait_DEBUG_MIND_CONTROL ) ) { return true; }

        const social_modifiers &u_mods = p->get_mutation_social_mods();
        int chance = difficulty;

        static const bionic_id bio_armor_eyes_t( "bio_armor_eyes" );
        static const bionic_id bio_deformity_t( "bio_deformity" );
        static const bionic_id bio_face_mask_t( "bio_face_mask" );
        static const bionic_id bio_voice_t( "bio_voice" );

        if( type_str == "PERSUADE" )
        {
            chance += character_effects::talk_skill( *p ) -
            character_effects::talk_skill( *n ) / 2 +
            n->op_of_u.trust * 2 + n->op_of_u.value;
            chance += u_mods.persuade;
            if( p->has_bionic( bio_face_mask_t ) ) { chance += 10; }
            if( p->has_bionic( bio_deformity_t ) ) { chance -= 50; }
            if( p->has_bionic( bio_voice_t ) )     { chance -= 20; }
        } else if( type_str == "LIE" )
        {
            chance += character_effects::talk_skill( *p ) -
                      character_effects::talk_skill( *n ) +
                      n->op_of_u.trust * 3;
            chance += u_mods.lie;
            if( p->has_bionic( bio_voice_t ) )     { chance += 10; }
            if( p->has_bionic( bio_face_mask_t ) ) { chance += 20; }
        } else if( type_str == "INTIMIDATE" )
        {
            chance += character_effects::intimidation( *p ) -
                      character_effects::intimidation( *n ) +
                      n->op_of_u.fear * 2 - n->personality.bravery * 2;
            chance += u_mods.intimidate;
            if( p->has_bionic( bio_face_mask_t ) )  { chance += 10; }
            if( p->has_bionic( bio_armor_eyes_t ) ) { chance += 10; }
            if( p->has_bionic( bio_deformity_t ) )  { chance += 20; }
            if( p->has_bionic( bio_voice_t ) )      { chance += 20; }
        }
        chance = std::max( 0, std::min( 100, chance ) );
        bool success = rng( 0, 99 ) < chance;
        if( success )
        {
            p->practice( skill_speech, ( 100 - chance ) / 10 );
        } else
        {
            p->practice( skill_speech, ( 100 - chance ) / 7 );
        }
        return success;
    } );

    // ============================================================
    // Random line selection (variadic)
    // random_line("text1", "text2", ...) → string
    // ============================================================

    reg.register_func( {
        .name        = "random_line",
        .param_types = {},
        .return_type = vt::string,
        .variadic    = true,
        .impl        = []( const std::vector<value> &args ) -> value {
            if( args.empty() ) { return std::string{}; }
            auto idx = rng( 0, static_cast<int>( args.size() ) - 1 );
            const auto &picked = args[static_cast<std::size_t>( idx )];
            if( std::holds_alternative<std::string>( picked ) )
            {
                return picked;
            }
            return std::string{};
        }
    } );
}

// ============================================================
// Built-in commands
// ============================================================

void register_builtin_commands( command_registry &reg )
{
    // give_item "item_id" [count] ["silent"]
    // Default: emits a gray narrator line in the dialogue window confirming receipt.
    // Pass "silent" as the third argument to suppress the notification.
    reg.add( "give_item", 1, 3, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        if( p )
        {
            auto id    = itype_id( std::get<std::string>( args[0] ) );
            auto count = args.size() > 1 ? static_cast<int>( std::get<double>( args[1] ) ) : 1;
            bool silent = args.size() > 2
            && std::holds_alternative<std::string>( args[2] )
            && std::get<std::string>( args[2] ) == "silent";
            p->add_item_with_id( id, count );
            if( !silent && g_conv_ctx.d_win_ref ) {
                auto item_name = colorize( item::nname( id, count ), id.obj().color );
                g_conv_ctx.d_win_ref->add_to_history(
                    colorize( string_format( _( "- Received %d %s." ), count, item_name ),
                              c_light_gray ) );
            }
        }
        return command_signal::none;
    } );

    // take_item "item_id" [count]
    reg.add( "take_item", 1, 2, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        if( p )
        {
            auto id    = itype_id( std::get<std::string>( args[0] ) );
            auto count = args.size() > 1 ? static_cast<int>( std::get<double>( args[1] ) ) : 1;
            p->use_amount( id, count );
        }
        return command_signal::none;
    } );

    // add_effect "effect_id" [duration_turns]
    reg.add( "add_effect", 1, 2, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        if( p )
        {
            auto id  = efftype_id( std::get<std::string>( args[0] ) );
            auto dur = args.size() > 1
            ? time_duration::from_turns( static_cast<int>( std::get<double>( args[1] ) ) )
            : 1_turns;
            p->add_effect( id, dur );
        }
        return command_signal::none;
    } );

    // remove_effect "effect_id"
    reg.add( "remove_effect", 1, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        if( p )
        {
            p->remove_effect( efftype_id( std::get<std::string>( args[0] ) ) );
        }
        return command_signal::none;
    } );

    // npc_follow — NPC starts following the player
    reg.add( "npc_follow", []( const std::vector<value> & ) -> command_signal {
        auto *n = g_conv_ctx.npc_ref;
        if( n )
        {
            n->set_attitude( NPCATT_FOLLOW );
        }
        return command_signal::none;
    } );

    // npc_stop_follow — NPC stops following
    reg.add( "npc_stop_follow", []( const std::vector<value> & ) -> command_signal {
        auto *n = g_conv_ctx.npc_ref;
        if( n )
        {
            n->set_attitude( NPCATT_NULL );
        }
        return command_signal::none;
    } );

    // npc_set_attitude "attitude_id"
    // Accepts the same string IDs used by npc_attitude_id() (e.g. "NPCATT_KILL")
    reg.add( "npc_set_attitude", 1, []( const std::vector<value> &args ) -> command_signal {
        auto *n = g_conv_ctx.npc_ref;
        if( n )
        {
            const auto &att_str = std::get<std::string>( args[0] );
            for( int i = 0; i < NPCATT_END; ++i ) {
                auto att = static_cast<npc_attitude>( i );
                if( npc_attitude_id( att ) == att_str ) {
                    n->set_attitude( att );
                    return command_signal::none;
                }
            }
            DebugLog( DL::Warn, DC::Main )
                    << "yarn: npc_set_attitude: unknown attitude '" << att_str << "'";
        }
        return command_signal::none;
    } );

    // Arbitrary character variables

    // u_set_var "key" value  — stores any typed value as a string
    reg.add( "u_set_var", 2, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        if( p )
        {
            p->set_value( std::get<std::string>( args[0] ),
                          value_to_storage_string( args[1] ) );
        }
        return command_signal::none;
    } );

    // u_remove_var "key"
    reg.add( "u_remove_var", 1, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        if( p )
        {
            p->remove_value( std::get<std::string>( args[0] ) );
        }
        return command_signal::none;
    } );

    // u_adjust_var "key" amount  — adds amount to stored numeric variable
    reg.add( "u_adjust_var", 2, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        if( p )
        {
            const auto &key    = std::get<std::string>( args[0] );
            auto        amount = static_cast<long long>( std::get<double>( args[1] ) );
            const auto &stored = p->get_value( key );
            long long current = 0;
            try { current = stored.empty() ? 0LL : std::stoll( stored ); }
            catch( const std::exception & ) {}
            p->set_value( key, std::to_string( current + amount ) );
        }
        return command_signal::none;
    } );

    // npc_set_var "key" value
    reg.add( "npc_set_var", 2, []( const std::vector<value> &args ) -> command_signal {
        auto *n = g_conv_ctx.npc_ref;
        if( n )
        {
            n->set_value( std::get<std::string>( args[0] ),
                          value_to_storage_string( args[1] ) );
        }
        return command_signal::none;
    } );

    // npc_remove_var "key"
    reg.add( "npc_remove_var", 1, []( const std::vector<value> &args ) -> command_signal {
        auto *n = g_conv_ctx.npc_ref;
        if( n )
        {
            n->remove_value( std::get<std::string>( args[0] ) );
        }
        return command_signal::none;
    } );

    // npc_adjust_var "key" amount
    reg.add( "npc_adjust_var", 2, []( const std::vector<value> &args ) -> command_signal {
        auto *n = g_conv_ctx.npc_ref;
        if( n )
        {
            const auto &key    = std::get<std::string>( args[0] );
            auto        amount = static_cast<long long>( std::get<double>( args[1] ) );
            const auto &stored = n->get_value( key );
            long long current = 0;
            try { current = stored.empty() ? 0LL : std::stoll( stored ); }
            catch( const std::exception & ) {}
            n->set_value( key, std::to_string( current + amount ) );
        }
        return command_signal::none;
    } );

    // ============================================================
    // talk_function wrappers
    // ============================================================
    //
    // Helpers: wrap a talk_function into a registry entry.
    // npc_fn   — fires and returns command_signal::none (conversation continues).
    // npc_stop — fires and returns command_signal::stop (conversation ends automatically).
    //            Use for commands that assign player activities or otherwise make it
    //            impossible/meaningless to continue the dialogue.

    auto npc_fn = [&reg]( const char *name, void( *fn )( npc & ) ) {
        reg.add( name, [fn]( const std::vector<value> & ) -> command_signal {
            if( auto *n = g_conv_ctx.npc_ref )
            {
                fn( *n );
            }
            return command_signal::none;
        } );
    };

    auto npc_stop = [&reg]( const char *name, void( *fn )( npc & ) ) {
        reg.add( name, [fn]( const std::vector<value> & ) -> command_signal {
            if( auto *n = g_conv_ctx.npc_ref )
            {
                fn( *n );
            }
            return command_signal::stop;
        } );
    };

    // Conversation / social
    npc_fn( "nothing",              talk_function::nothing );
    npc_fn( "morale_chat",          talk_function::morale_chat );
    npc_stop( "morale_chat_activity", talk_function::morale_chat_activity );
    npc_fn( "reveal_stats",         talk_function::reveal_stats );
    npc_stop( "end_conversation",   talk_function::end_conversation );

    // Mission handling
    npc_fn( "npc_assign_selected_mission",  talk_function::assign_mission );
    npc_fn( "mission_success", talk_function::mission_success );
    npc_fn( "mission_failure", talk_function::mission_failure );
    npc_fn( "clear_mission",   talk_function::clear_mission );
    npc_fn( "mission_reward",  talk_function::mission_reward );
    npc_fn( "lead_to_safety",  talk_function::lead_to_safety );

    // Equipment / items
    npc_fn( "give_equipment",     talk_function::give_equipment );
    npc_fn( "drop_weapon",        talk_function::drop_weapon );
    npc_fn( "player_weapon_away", talk_function::player_weapon_away );
    npc_fn( "player_weapon_drop", talk_function::player_weapon_drop );
    npc_fn( "drop_stolen_item",   talk_function::drop_stolen_item );
    npc_fn( "remove_stolen_status", talk_function::remove_stolen_status );

    // Trading / services
    npc_fn( "start_trade",  talk_function::start_trade );
    npc_fn( "buy_10_logs",  talk_function::buy_10_logs );
    npc_fn( "buy_100_logs", talk_function::buy_100_logs );
    npc_fn( "buy_horse",    talk_function::buy_horse );
    npc_fn( "buy_cow",      talk_function::buy_cow );
    npc_fn( "buy_chicken",  talk_function::buy_chicken );

    // Aid / healing (assign activities — auto-stop)
    npc_stop( "give_aid",     talk_function::give_aid );
    npc_stop( "give_all_aid", talk_function::give_all_aid );

    // Grooming (UI-opening; buy_haircut / buy_shave assign activities — auto-stop)
    npc_fn( "barber_hair",  talk_function::barber_hair );
    npc_fn( "barber_beard", talk_function::barber_beard );
    npc_stop( "buy_haircut", talk_function::buy_haircut );
    npc_stop( "buy_shave",   talk_function::buy_shave );

    // Bionics (UI-opening; conversation continues after screen closes)
    npc_fn( "bionic_install", talk_function::bionic_install );
    npc_fn( "bionic_remove",  talk_function::bionic_remove );

    // Training (assigns ACT_TRAIN — auto-stop)
    npc_stop( "start_training", talk_function::start_training );

    // NPC follower management
    npc_fn( "follow",          talk_function::follow );
    npc_fn( "follow_only",     talk_function::follow_only );
    npc_fn( "stop_following",  talk_function::stop_following );
    npc_fn( "deny_follow",     talk_function::deny_follow );
    npc_fn( "deny_lead",       talk_function::deny_lead );
    npc_fn( "deny_equipment",  talk_function::deny_equipment );
    npc_fn( "deny_train",      talk_function::deny_train );
    npc_fn( "deny_personal_info", talk_function::deny_personal_info );
    npc_fn( "copy_npc_rules",  talk_function::copy_npc_rules );
    npc_fn( "set_npc_pickup",  talk_function::set_npc_pickup );
    npc_fn( "clear_overrides", talk_function::clear_overrides );

    // NPC activities / orders
    npc_fn( "sort_loot",             talk_function::sort_loot );
    npc_fn( "do_construction",       talk_function::do_construction );
    npc_fn( "do_mining",             talk_function::do_mining );
    npc_fn( "do_read",               talk_function::do_read );
    npc_fn( "do_chop_plank",         talk_function::do_chop_plank );
    npc_fn( "do_vehicle_deconstruct", talk_function::do_vehicle_deconstruct );
    npc_fn( "do_vehicle_repair",     talk_function::do_vehicle_repair );
    npc_fn( "do_chop_trees",         talk_function::do_chop_trees );
    npc_fn( "do_fishing",            talk_function::do_fishing );
    npc_fn( "do_farming",            talk_function::do_farming );
    npc_fn( "do_butcher",            talk_function::do_butcher );
    npc_fn( "revert_activity",       talk_function::revert_activity );
    npc_fn( "assign_guard",          talk_function::assign_guard );
    npc_fn( "stop_guard",            talk_function::stop_guard );
    npc_fn( "goto_location",         talk_function::goto_location );
    npc_fn( "find_mount",            talk_function::find_mount );
    npc_fn( "dismount",              talk_function::dismount );

    // Hostile / conflict
    npc_fn( "hostile",         talk_function::hostile );
    npc_fn( "flee",            talk_function::flee );
    npc_fn( "leave",           talk_function::leave );
    npc_fn( "insult_combat",   talk_function::insult_combat );
    npc_fn( "start_mugging",   talk_function::start_mugging );
    npc_fn( "player_leaving",  talk_function::player_leaving );
    npc_fn( "stranger_neutral", talk_function::stranger_neutral );

    // Misc NPC state
    npc_fn( "wake_up",       talk_function::wake_up );
    npc_fn( "npc_die",       talk_function::npc_die );
    npc_fn( "npc_thankful",  talk_function::npc_thankful );
    npc_stop( "control_npc", talk_function::control_npc );

    // ============================================================
    // Legacy conversion — effect commands
    // ============================================================

    // Traits
    reg.add( "u_add_trait", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *p = g_conv_ctx.player_ref )
        {
            p->set_mutation( trait_id( std::get<std::string>( args[0] ) ) );
        }
        return command_signal::none;
    } );
    reg.add( "npc_add_trait", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            n->set_mutation( trait_id( std::get<std::string>( args[0] ) ) );
        }
        return command_signal::none;
    } );
    reg.add( "u_lose_trait", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *p = g_conv_ctx.player_ref )
        {
            p->unset_mutation( trait_id( std::get<std::string>( args[0] ) ) );
        }
        return command_signal::none;
    } );
    reg.add( "npc_lose_trait", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            n->unset_mutation( trait_id( std::get<std::string>( args[0] ) ) );
        }
        return command_signal::none;
    } );

    // Effects (NPC-targeted)
    reg.add( "u_add_effect", 2, []( const std::vector<value> &args ) -> command_signal {
        if( auto *p = g_conv_ctx.player_ref )
        {
            auto id      = efftype_id( std::get<std::string>( args[0] ) );
            auto turns   = static_cast<int>( std::get<double>( args[1] ) );
            auto dur     = time_duration::from_turns( turns < 0 ? 1 : turns );
            p->add_effect( id, dur );
            if( turns < 0 ) { p->get_effect( id ).set_permanent(); }
        }
        return command_signal::none;
    } );
    reg.add( "npc_add_effect", 2, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto id      = efftype_id( std::get<std::string>( args[0] ) );
            auto turns   = static_cast<int>( std::get<double>( args[1] ) );
            auto dur     = time_duration::from_turns( turns < 0 ? 1 : turns );
            n->add_effect( id, dur );
            if( turns < 0 ) { n->get_effect( id ).set_permanent(); }
        }
        return command_signal::none;
    } );
    reg.add( "u_lose_effect", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *p = g_conv_ctx.player_ref )
        {
            p->remove_effect( efftype_id( std::get<std::string>( args[0] ) ) );
        }
        return command_signal::none;
    } );
    reg.add( "npc_lose_effect", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            n->remove_effect( efftype_id( std::get<std::string>( args[0] ) ) );
        }
        return command_signal::none;
    } );

    // Variables — talk var naming: "npctalk_var[_type][_context]_name"
    auto talk_varname = []( const std::vector<value> &args, std::size_t offset ) -> std::string {
        const auto &name    = std::get<std::string>( args[offset] );
        const auto &type    = std::get<std::string>( args[offset + 1] );
        const auto &context = std::get<std::string>( args[offset + 2] );
        return "npctalk_var" +
        ( type.empty()    ? std::string{} : "_" + type ) +
        ( context.empty() ? std::string{} : "_" + context ) +
        "_" + name;
    };

    reg.add( "u_add_var", 4, [talk_varname]( const std::vector<value> &args ) -> command_signal {
        if( auto *p = g_conv_ctx.player_ref )
        {
            p->set_value( talk_varname( args, 0 ), std::get<std::string>( args[3] ) );
        }
        return command_signal::none;
    } );
    reg.add( "npc_add_var", 4, [talk_varname]( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            n->set_value( talk_varname( args, 0 ), std::get<std::string>( args[3] ) );
        }
        return command_signal::none;
    } );
    reg.add( "u_lose_var", 3, [talk_varname]( const std::vector<value> &args ) -> command_signal {
        if( auto *p = g_conv_ctx.player_ref )
        {
            p->remove_value( talk_varname( args, 0 ) );
        }
        return command_signal::none;
    } );
    reg.add( "npc_lose_var", 3, [talk_varname]( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            n->remove_value( talk_varname( args, 0 ) );
        }
        return command_signal::none;
    } );
    // u_adjust_var_legacy: name, type, context, amount (uses talk var naming)
    reg.add( "u_adjust_var_legacy",
    4, [talk_varname]( const std::vector<value> &args ) -> command_signal {
        if( auto *p = g_conv_ctx.player_ref )
        {
            auto key    = talk_varname( args, 0 );
            auto amount = static_cast<long long>( std::get<double>( args[3] ) );
            const auto &stored = p->get_value( key );
            long long current = 0;
            try { current = stored.empty() ? 0LL : std::stoll( stored ); }
            catch( const std::exception & ) {}
            p->set_value( key, std::to_string( current + amount ) );
        }
        return command_signal::none;
    } );
    reg.add( "npc_adjust_var_legacy",
    4, [talk_varname]( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto key    = talk_varname( args, 0 );
            auto amount = static_cast<long long>( std::get<double>( args[3] ) );
            const auto &stored = n->get_value( key );
            long long current = 0;
            try { current = stored.empty() ? 0LL : std::stoll( stored ); }
            catch( const std::exception & ) {}
            n->set_value( key, std::to_string( current + amount ) );
        }
        return command_signal::none;
    } );

    // Economy
    reg.add( "u_spend_ecash", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *p = g_conv_ctx.player_ref )
        {
            p->cash -= static_cast<long long>( std::get<double>( args[0] ) );
        }
        return command_signal::none;
    } );

    // Item trade commands (simplified — no container/cost validation)
    reg.add( "u_buy_item", 3, []( const std::vector<value> &args ) -> command_signal {
        if( auto *p = g_conv_ctx.player_ref )
        {
            auto id    = itype_id( std::get<std::string>( args[0] ) );
            auto cost  = static_cast<long long>( std::get<double>( args[1] ) );
            auto count = static_cast<int>( std::get<double>( args[2] ) );
            p->cash -= cost;
            p->add_item_with_id( id, count );
        }
        return command_signal::none;
    } );
    reg.add( "u_sell_item", 3, []( const std::vector<value> &args ) -> command_signal {
        if( auto *p = g_conv_ctx.player_ref )
        {
            auto id    = itype_id( std::get<std::string>( args[0] ) );
            auto cost  = static_cast<long long>( std::get<double>( args[1] ) );
            auto count = static_cast<int>( std::get<double>( args[2] ) );
            p->cash += cost;
            p->use_amount( id, count );
        }
        return command_signal::none;
    } );
    reg.add( "u_consume_item", 2, []( const std::vector<value> &args ) -> command_signal {
        if( auto *p = g_conv_ctx.player_ref )
        {
            auto id    = itype_id( std::get<std::string>( args[0] ) );
            auto count = static_cast<int>( std::get<double>( args[1] ) );
            p->use_amount( id, count );
        }
        return command_signal::none;
    } );
    reg.add( "npc_consume_item", 2, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto id    = itype_id( std::get<std::string>( args[0] ) );
            auto count = static_cast<int>( std::get<double>( args[1] ) );
            n->use_amount( id, count );
        }
        return command_signal::none;
    } );
    reg.add( "u_remove_item_with", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *p = g_conv_ctx.player_ref )
        {
            auto id = itype_id( std::get<std::string>( args[0] ) );
            p->remove_items_with( [id]( detached_ptr<item> &&it ) {
                if( it->typeId() == id ) {
                    detached_ptr<item> del = std::move( it );
                }
                return VisitResponse::SKIP;
            } );
        }
        return command_signal::none;
    } );
    reg.add( "npc_remove_item_with", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto id = itype_id( std::get<std::string>( args[0] ) );
            n->remove_items_with( [id]( detached_ptr<item> &&it ) {
                if( it->typeId() == id ) {
                    detached_ptr<item> del = std::move( it );
                }
                return VisitResponse::SKIP;
            } );
        }
        return command_signal::none;
    } );

    // Topic switching (from npc_first_topic effect)
    reg.add( "npc_set_first_topic", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            n->chatbin.first_topic = std::get<std::string>( args[0] );
        }
        return command_signal::none;
    } );

    // NPC ally rules
    reg.add( "toggle_npc_rule", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto it = ally_rule_strs.find( std::get<std::string>( args[0] ) );
            if( it != ally_rule_strs.end() ) {
                n->rules.toggle_flag( it->second.rule );
                n->wield_better_weapon();
            }
        }
        return command_signal::none;
    } );
    reg.add( "set_npc_rule", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto it = ally_rule_strs.find( std::get<std::string>( args[0] ) );
            if( it != ally_rule_strs.end() ) {
                n->rules.set_flag( it->second.rule );
                n->wield_better_weapon();
            }
        }
        return command_signal::none;
    } );
    reg.add( "clear_npc_rule", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto it = ally_rule_strs.find( std::get<std::string>( args[0] ) );
            if( it != ally_rule_strs.end() ) {
                n->rules.clear_flag( it->second.rule );
                n->wield_better_weapon();
            }
        }
        return command_signal::none;
    } );
    npc_fn( "copy_npc_rules", talk_function::copy_npc_rules );

    reg.add( "set_npc_engagement_rule", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto it = combat_engagement_strs.find( std::get<std::string>( args[0] ) );
            if( it != combat_engagement_strs.end() ) {
                n->rules.engagement = it->second;
                n->invalidate_range_cache();
            }
        }
        return command_signal::none;
    } );
    reg.add( "set_npc_aim_rule", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto it = aim_rule_strs.find( std::get<std::string>( args[0] ) );
            if( it != aim_rule_strs.end() ) {
                n->rules.aim = it->second;
                n->invalidate_range_cache();
            }
        }
        return command_signal::none;
    } );
    reg.add( "set_npc_cbm_reserve_rule", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto it = cbm_reserve_strs.find( std::get<std::string>( args[0] ) );
            if( it != cbm_reserve_strs.end() ) {
                n->rules.cbm_reserve = it->second;
            }
        }
        return command_signal::none;
    } );
    reg.add( "set_npc_cbm_recharge_rule", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto it = cbm_recharge_strs.find( std::get<std::string>( args[0] ) );
            if( it != cbm_recharge_strs.end() ) {
                n->rules.cbm_recharge = it->second;
            }
        }
        return command_signal::none;
    } );

    // Internal: set current_item_type_ on the runtime for parse_tags() expansion.
    // Not intended for use in authored .yarn files.
    reg.add( "_set_current_item", 1, []( const std::vector<value> &args ) -> command_signal {
        g_conv_ctx.current_item_type = std::get<std::string>( args[0] );
        return command_signal::none;
    } );

    // Opinion adjustment
    reg.add( "npc_add_trust", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            n->op_of_u.trust += static_cast<int>( std::get<double>( args[0] ) );
        }
        return command_signal::none;
    } );
    reg.add( "npc_add_fear", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            n->op_of_u.fear += static_cast<int>( std::get<double>( args[0] ) );
        }
        return command_signal::none;
    } );
    reg.add( "npc_add_value", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            n->op_of_u.value += static_cast<int>( std::get<double>( args[0] ) );
        }
        return command_signal::none;
    } );
    reg.add( "npc_add_anger", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            n->op_of_u.anger += static_cast<int>( std::get<double>( args[0] ) );
        }
        return command_signal::none;
    } );

    // add_debt("TYPE", factor, ...) — mirrors set_add_debt / parse_mod logic
    reg.add( "add_debt", 0, -1, []( const std::vector<value> &args ) -> command_signal {
        auto *n = g_conv_ctx.npc_ref;
        auto *p = g_conv_ctx.player_ref;
        if( !n || !p ) { return command_signal::none; }
        int debt = 0;
        int total_mult = 1;
        for( std::size_t i = 0; i + 1 < args.size(); i += 2 )
        {
            const auto &type   = std::get<std::string>( args[i] );
            auto        factor = static_cast<int>( std::get<double>( args[i + 1] ) );
            if( type == "TOTAL" ) {
                total_mult = factor;
                continue;
            }
            int mod = 0;
            if( type == "ANGER" ) { mod = n->op_of_u.anger; }
            else if( type == "FEAR" ) { mod = n->op_of_u.fear; }
            else if( type == "TRUST" ) { mod = n->op_of_u.trust; }
            else if( type == "VALUE" ) { mod = n->op_of_u.value; }
            else if( type == "POS_FEAR" ) { mod = std::max( 0, n->op_of_u.fear ); }
            else if( type == "AGGRESSION" ) { mod = n->personality.aggression; }
            else if( type == "ALTRUISM" ) { mod = n->personality.altruism; }
            else if( type == "BRAVERY" ) { mod = n->personality.bravery; }
            else if( type == "COLLECTOR" ) { mod = n->personality.collector; }
            else if( type == "U_INTIMIDATE" ) {
                mod = character_effects::intimidation( *p );
            } else if( type == "NPC_INTIMIDATE" ) {
                mod = character_effects::intimidation( *n );
            }
            debt += mod * factor;
        }
        debt *= total_mult;
        n->op_of_u += npc_opinion( 0, 0, 0, 0, debt );
        return command_signal::none;
    } );

    reg.add( "u_faction_rep", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto *fac = n->get_faction();
            if( fac && fac->id != faction_id( "no_faction" ) ) {
                auto delta = static_cast<int>( std::get<double>( args[0] ) );
                fac->likes_u    += delta;
                fac->respects_u += delta;
            }
        }
        return command_signal::none;
    } );

    // Mission management
    // add_mission("id") — NPC-assigned mission added to NPC's missions_assigned list
    reg.add( "add_mission", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto *miss = mission::reserve_new( mission_type_id( std::get<std::string>( args[0] ) ),
                                               n->getID() );
            miss->assign( get_avatar() );
            n->chatbin.missions_assigned.push_back( miss );
        }
        return command_signal::none;
    } );
    // assign_mission("id") — player-facing mission (no specific NPC owner)
    reg.add( "assign_mission", 1, []( const std::vector<value> &args ) -> command_signal {
        auto *new_mission = mission::reserve_new( mission_type_id( std::get<std::string>( args[0] ) ),
                character_id() );
        new_mission->assign( get_avatar() );
        return command_signal::none;
    } );
    // finish_mission("id", success_bool)
    reg.add( "finish_mission", 2, []( const std::vector<value> &args ) -> command_signal {
        auto type    = mission_type_id( std::get<std::string>( args[0] ) );
        auto success = std::get<bool>( args[1] );
        for( auto *m : get_avatar().get_active_missions() )
        {
            if( m->mission_id() == type ) {
                if( success ) { m->wrap_up(); }
                else { m->fail(); }
                break;
            }
        }
        return command_signal::none;
    } );
    // npc_change_faction("faction_name")
    reg.add( "npc_change_faction", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            n->set_fac( faction_id( std::get<std::string>( args[0] ) ) );
        }
        return command_signal::none;
    } );
    // npc_change_class("class_id") — changes NPC class but does NOT re-initialize stats.
    // Follow with <<npc_randomize>> to re-roll stats/inventory for the new class.
    reg.add( "npc_change_class", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            n->myclass = npc_class_id( std::get<std::string>( args[0] ) );
        }
        return command_signal::none;
    } );
    // npc_randomize — re-rolls NPC stats and inventory using the NPC's current class
    reg.add( "npc_randomize", 0, []( const std::vector<value> & ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            n->randomize();
        }
        return command_signal::none;
    } );
    // u_learn_recipe("recipe_id") — teaches the player a crafting recipe
    reg.add( "u_learn_recipe", 1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *p = g_conv_ctx.player_ref )
        {
            const recipe &r = recipe_id( std::get<std::string>( args[0] ) ).obj();
            p->learn_recipe( &r );
            popup( _( "You learn how to craft %s." ), r.result_name() );
        }
        return command_signal::none;
    } );
    // set_reason("text") — sets a reason string read by has_reason(); cleared each conversation
    reg.add( "set_reason", 1, []( const std::vector<value> &args ) -> command_signal {
        g_conv_ctx.reason = std::get<std::string>( args[0] );
        return command_signal::none;
    } );
    // u_buy_monster("mon_id", cost, [count=1], [pacified=false], [name=""])
    // Charges the player via the NPC's trade ledger, then spawns count friendly monsters nearby.
    reg.add( "u_buy_monster", 2, 5, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        auto *n = g_conv_ctx.npc_ref;
        if( !p || !n )
        {
            return command_signal::none;
        }
        const auto  type_str = std::get<std::string>( args[0] );
        const int   cost     = static_cast<int>( std::get<double>( args[1] ) );
        const int   count    = args.size() >= 3 ? static_cast<int>( std::get<double>( args[2] ) ) : 1;
        const bool  pacified = args.size() >= 4 ? std::get<bool>( args[3] ) : false;
        const auto  name     = args.size() >= 5 ? std::get<std::string>( args[4] ) : std::string{};

        if( !npc_trading::pay_npc( *n, cost ) )
        {
            popup( _( "You can't afford it!" ) );
            return command_signal::none;
        }

        const mtype_id mtype( type_str );
        for( int idx = 0; idx < count; ++idx )
        {
            monster *const mon_ptr = g->place_critter_around( mtype, p->pos(), 3 );
            if( !mon_ptr ) {
                add_msg( m_debug, "u_buy_monster: no valid placement location for %s", type_str );
                break;
            }
            mon_ptr->friendly = -1;
            mon_ptr->add_effect( effect_pet, 1_turns, bodypart_str_id::NULL_ID() );
            if( pacified ) {
                mon_ptr->add_effect( effect_pacified, 1_turns, bodypart_str_id::NULL_ID() );
            }
            if( !name.empty() ) {
                mon_ptr->unique_name = name;
            }
        }

        if( name.empty() )
        {
            popup( _( "%1$s gives you %2$d %3$s." ), n->name, count, mtype.obj().nname( count ) );
        } else
        {
            popup( _( "%1$s gives you %2$s." ), n->name, name );
        }
        return command_signal::none;
    } );
    // mapgen_update("id", ...) — variadic; each string arg is a mapgen update id
    reg.add( "mapgen_update", 0, -1, []( const std::vector<value> &args ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            auto omt_pos = n->global_omt_location();
            for( const auto &arg : args ) {
                run_mapgen_update_func( std::get<std::string>( arg ), omt_pos,
                                        n->chatbin.mission_selected );
            }
        }
        return command_signal::none;
    } );
    // npc_gets_item — player selects an item from inventory to give to the NPC to carry
    reg.add( "npc_gets_item", 0, []( const std::vector<value> & ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            give_item_to( *n, false );
        }
        return command_signal::none;
    } );
    // npc_gets_item_to_use — player selects an item for the NPC to use (eat/wield/wear)
    reg.add( "npc_gets_item_to_use", 0, []( const std::vector<value> & ) -> command_signal {
        if( auto *n = g_conv_ctx.npc_ref )
        {
            give_item_to( *n, true );
        }
        return command_signal::none;
    } );

    // Bulk trade / donate — operate on the current item set by <<repeat_for_item/category>>.
    //   u_bulk_trade_accept   — player sells all charges of current item to NPC, NPC pays
    //   npc_bulk_trade_accept — NPC sells all charges of current item to player, player pays
    //   u_bulk_donate         — player gives all charges of current item to NPC, no payment
    //   npc_bulk_donate       — NPC gives all charges of current item to player, no payment
    auto bulk_trade_impl = []( bool is_trade, bool is_npc ) -> command_signal {
        auto *npc_ptr = g_conv_ctx.npc_ref;
        auto *player_ptr = g_conv_ctx.player_ref;
        if( !npc_ptr || !player_ptr )
        {
            return command_signal::none;
        }

        itype_id cur_item( g_conv_ctx.current_item_type );
        if( cur_item.is_empty() )
        {
            DebugLog( DL::Warn, DC::Main )
                    << "yarn: bulk trade command called with no current item set";
            return command_signal::none;
        }

        player *seller = player_ptr;
        player *buyer  = static_cast<player *>( npc_ptr );
        if( is_npc )
        {
            seller = static_cast<player *>( npc_ptr );
            buyer  = player_ptr;
        }

        int seller_has = seller->charges_of( cur_item );
        detached_ptr<item> tmp = item::spawn( cur_item );
        tmp->charges = seller_has;

        if( is_trade )
        {
            int price = tmp->price( true ) * ( is_npc ? -1 : 1 ) + npc_ptr->op_of_u.owed;
            if( npc_ptr->get_faction() && !npc_ptr->get_faction()->currency.is_empty() ) {
                const itype_id &pay_in = npc_ptr->get_faction()->currency;
                item *pay = item::spawn_temporary( pay_in );
                if( npc_ptr->value( *pay ) > 0 ) {
                    int buyer_has = price / npc_ptr->value( *pay );
                    if( is_npc ) {
                        buyer_has = std::min( buyer_has, buyer->charges_of( pay_in ) );
                        buyer->use_charges( pay_in, buyer_has );
                    } else {
                        if( buyer_has == 1 ) {
                            popup( _( "%1$s gives you a %2$s." ),
                                   npc_ptr->disp_name(), pay->tname() );
                        } else if( buyer_has > 1 ) {
                            popup( _( "%1$s gives you %2$d %3$s." ),
                                   npc_ptr->disp_name(), buyer_has, pay->tname() );
                        }
                    }
                    for( int i = 0; i < buyer_has; i++ ) {
                        seller->i_add( item::spawn( *pay ) );
                        price -= npc_ptr->value( *pay );
                    }
                }
                npc_ptr->op_of_u.owed = price;
            }
        }

        seller->use_charges( cur_item, seller_has );
        buyer->i_add( std::move( tmp ) );
        return command_signal::none;
    };

    reg.add( "u_bulk_trade_accept", 0,
    [bulk_trade_impl]( const std::vector<value> & ) -> command_signal {
        return bulk_trade_impl( true, false );
    } );
    reg.add( "npc_bulk_trade_accept", 0,
    [bulk_trade_impl]( const std::vector<value> & ) -> command_signal {
        return bulk_trade_impl( true, true );
    } );
    reg.add( "u_bulk_donate", 0,
    [bulk_trade_impl]( const std::vector<value> & ) -> command_signal {
        return bulk_trade_impl( false, false );
    } );
    reg.add( "npc_bulk_donate", 0,
    [bulk_trade_impl]( const std::vector<value> & ) -> command_signal {
        return bulk_trade_impl( false, true );
    } );
}

// ============================================================
// Integration
// ============================================================

auto run_npc_dialogue( dialogue_window &d_win, npc &n, player &p ) -> bool
{
    if( n.chatbin.yarn_story.empty() ) {
        DebugLog( DL::Error, DC::Main )
                << "yarn: NPC '" << n.name << "' has no yarn_story — using legacy path";
        return false;
    }
    if( !has_yarn_story( n.chatbin.yarn_story ) ) {
        debugmsg( "Error: dialogue story %d failed to load.", n.name, n.chatbin.yarn_story );
        return true;
    }
    DebugLog( DL::Info, DC::Dialogue )
            << "yarn: running story '" << n.chatbin.yarn_story << "' for NPC '" << n.name << "'";

    const auto &story = get_yarn_story( n.chatbin.yarn_story );
    d_win.print_header( n.name );

    g_conv_ctx = { &n, &p };
    conv_ctx_guard guard;

    // Entry node convention: use "Greeting" if present (shown once on first contact),
    // otherwise fall back to "Start" (the recurring choice hub).
    const std::string entry_node = story.has_node( "Greeting" ) ? "Greeting" : "Start";

    yarn_runtime::options opts{
        .story         = story,
        .registry      = func_registry::global(),
        .starting_node = entry_node,
        .npc_ref       = &n,
        .player_ref    = &p
    };
    yarn_runtime runtime( std::move( opts ) );
    runtime.run( d_win );

    return true;
}

auto try_legacy_yarn_dialogue( dialogue_window &d_win, npc &n, player &p, dialogue &d ) -> bool
{
    if( n.chatbin.first_topic.empty() ) {
        return false;
    }
    if( !has_yarn_story( "__legacy" ) ) {
        DebugLog( DL::Warn, DC::Main ) << "yarn: __legacy story not found";
        return false;
    }

    const auto &story = get_yarn_story( "__legacy" );

    // Use the dynamic topic from the dialogue stack, not the NPC's static first_topic.
    // By the time this is called, talk_to_u has already resolved the effective starting topic
    // (accounting for pick_talk_topic, missions, sleeping, etc.).
    const auto &starting_topic = d.topic_stack.empty()
                                 ? n.chatbin.first_topic
                                 : d.topic_stack.back().id;

    if( !story.has_node( starting_topic ) ) {
        DebugLog( DL::Warn, DC::Main )
                << "yarn: __legacy story has no node '" << starting_topic << "'";
        return false;
    }

    d_win.print_header( n.name );

    // Set the global context so built-in functions and commands can access NPC/player.
    g_conv_ctx = { &n, &p };
    conv_ctx_guard guard;

    yarn_runtime::options opts{
        .story         = story,
        .registry      = func_registry::global(),
        .starting_node = starting_topic,
        .npc_ref       = &n,
        .player_ref    = &p,
        .dialogue_ref  = &d
    };
    yarn_runtime runtime( std::move( opts ) );
    runtime.run( d_win );
    return true;
}

} // namespace yarn
