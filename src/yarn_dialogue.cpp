// yarn_dialogue.cpp — Yarn Spinner dialogue runtime for Cataclysm: Bright Nights

#include "yarn_dialogue.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "avatar.h"
#include "calendar.h"
#include "color.h"
#include "cursesdef.h"
#include "debug.h"
#include "dialogue.h"
#include "dialogue_win.h"
#include "game.h"
#include "map.h"
#include "mapdata.h"
#include "mission.h"
#include "overmapbuffer.h"
#include "filesystem.h"
#include "input.h"
#include "npc.h"
#include "npctalk.h"
#include "overmap.h"
#include "overmapbuffer_registry.h"
#include "path_info.h"
#include "player.h"
#include "recipe_dictionary.h"
#include "skill.h"
#include "translations.h"
#include "ui_manager.h"
#include "vehicle.h"
#include "vpart_position.h"

namespace yarn {

// ============================================================
// Value system
// ============================================================

auto type_of( const value &v ) -> value_type
{
    return std::visit( []<typename T>( const T & ) -> value_type {
        if constexpr( std::is_same_v<T, bool> ) {
            return value_type::boolean;
        } else if constexpr( std::is_same_v<T, double> ) {
            return value_type::number;
        } else {
            return value_type::string;
        }
    }, v );
}

auto type_name( value_type t ) -> std::string_view
{
    switch( t ) {
        case value_type::boolean: return "bool";
        case value_type::number:  return "number";
        case value_type::string:  return "string";
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
    register_func( { std::move( name ), std::vector<value_type>( params ), ret, std::move( impl ) } );
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
        DebugLog( DL::Warn, DC::Dialogue )
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
                        case '"':  s += '"';  break;
                        case '\\': s += '\\'; break;
                        case 'n':  s += '\n'; break;
                        default:   s += cur(); break;
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
            case '(': return { token_type::lparen,  "(", 0.0, col };
            case ')': return { token_type::rparen,  ")", 0.0, col };
            case ',': return { token_type::comma,   ",", 0.0, col };
            case '+': return { token_type::plus,    "+", 0.0, col };
            case '-': return { token_type::minus,   "-", 0.0, col };
            case '*': return { token_type::star,    "*", 0.0, col };
            case '/': return { token_type::slash,   "/", 0.0, col };
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

    auto cur() const -> const token & { return tokens[pos]; }

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
            case token_type::lt:  op = expr_node::bin_op::lt;  break;
            case token_type::lte: op = expr_node::bin_op::lte; break;
            case token_type::gt:  op = expr_node::bin_op::gt;  break;
            case token_type::gte: op = expr_node::bin_op::gte; break;
            case token_type::eq:  op = expr_node::bin_op::eq;  break;
            case token_type::neq: op = expr_node::bin_op::neq; break;
            default: break;
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
            if( args.size() != sig.param_types.size() ) {
                return fail( "function '" + name + "' expects " +
                             std::to_string( sig.param_types.size() ) + " argument(s), got " +
                             std::to_string( args.size() ) );
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
                                    [&]( const expr_node &a ) {
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
                std::visit( [&]( const auto &v ) {
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
        } else if( c == ' ' && !in_quote ) {
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

        std::vector<std::string> errors;

        void error( int line_num, std::string msg ) {
            errors.push_back( source_name_ + ":" + std::to_string( line_num ) + ": " + msg );
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

// Forward declaration only for parse_elements — the helpers call it before it is defined.
auto parse_elements( yarn_parser &p, const std::vector<raw_line> &lines,
                     int &i, int end, int min_indent )
    -> std::vector<node_element>;

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
        // Stop if this line is not a -> at the group indent level
        if( line.indent != group_indent || !starts_with( line.content, "->" ) ) {
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

        int choice_indent = line.indent;
        ++i;

        node_element::choice ch;
        ch.text = std::string( choice_text );
        ch.condition = std::move( condition );
        ch.body = parse_choice_body( p, lines, i, end, choice_indent );
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
        if( line.content.empty() ) {
            ++i;
            continue;
        }

        auto cmd = extract_command( line.content );
        if( !cmd.empty() ) {
            if( cmd == "else" ) {
                if( in_else ) {
                    p.error( line.line_num, "duplicate <<else>>" );
                }
                in_else = true;
                ++i;
                continue;
            }
            if( cmd == "endif" ) {
                ++i;
                break;
            }
        }

        if( !in_else ) {
            auto body_elems = parse_elements( p, lines, i, i + 1, block_indent );
            if_body.insert( if_body.end(),
                            std::make_move_iterator( body_elems.begin() ),
                            std::make_move_iterator( body_elems.end() ) );
        } else {
            auto body_elems = parse_elements( p, lines, i, i + 1, block_indent );
            else_body.insert( else_body.end(),
                              std::make_move_iterator( body_elems.begin() ),
                              std::make_move_iterator( body_elems.end() ) );
        }
    }

    elem.if_body = std::move( if_body );
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

        // Commands
        auto cmd_inner = extract_command( line.content );
        if( !cmd_inner.empty() ) {
            // <<else>> and <<endif>> are handled by parse_if_block; if we see them here
            // it means there's a structural error.
            if( cmd_inner == "else" || cmd_inner == "endif" ) {
                p.error( line.line_num,
                         "<<" + std::string( cmd_inner ) + ">> without matching <<if>>" );
                ++i;
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

            auto pc = parse_command_line( cmd_inner );
            ++i;

            if( pc.name == "jump" ) {
                if( pc.raw_args.empty() ) {
                    p.error( line.line_num, "<<jump>> requires a node name" );
                } else {
                    node_element elem;
                    elem.type = node_element::kind::jump;
                    elem.jump_target = pc.raw_args[0];
                    elements.push_back( std::move( elem ) );
                }
            } else if( pc.name == "goto" ) {
                if( pc.raw_args.empty() ) {
                    p.error( line.line_num, "<<goto>> requires a node name" );
                } else {
                    node_element elem;
                    elem.type = node_element::kind::goto_node;
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
                        p.error( line.line_num,
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

        // Dialogue line — split "Speaker: text" or plain "text"
        {
            node_element elem;
            elem.type = node_element::kind::dialogue;

            auto colon = line.content.find( ':' );
            if( colon != std::string::npos && colon > 0 &&
                line.content.find( ' ' ) > colon ) {
                // Has "Speaker: " format
                elem.speaker = std::string( trim_sv( line.content.substr( 0, colon ) ) );
                elem.text = std::string( trim_sv( line.content.substr( colon + 1 ) ) );
            } else {
                elem.text = line.content;
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

        // Find the end of this node (=== marker)
        int node_end = i;
        while( node_end < total && all_lines[node_end].content != "===" ) {
            ++node_end;
        }

        // Parse body elements
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

    result.errors = std::move( parser.errors );
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
// yarn_runtime
// ============================================================

yarn_runtime::yarn_runtime( options opts )
    : story_( opts.story )
    , registry_( opts.registry )
    , npc_( opts.npc_ref )
    , player_( opts.player_ref )
    , dialogue_ref_( opts.dialogue_ref )
{
    node_stack_.push_back( std::move( opts.starting_node ) );
}

void yarn_runtime::run( dialogue_window &d_win )
{
    while( !node_stack_.empty() ) {
        const auto &node_name = node_stack_.back();
        if( !story_.has_node( node_name ) ) {
            DebugLog( DL::Error, DC::Dialogue ) << "yarn: node '" << node_name << "' not found";
            break;
        }
        const auto &node = story_.get_node( node_name );
        auto result = execute_elements( node.elements, d_win );

        switch( result.kind ) {
            case signal::jump:
                // Push: target executes and falls off end → pop back to here
                node_stack_.push_back( result.target );
                break;
            case signal::goto_node:
                // Replace current frame — no return to caller
                node_stack_.back() = result.target;
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

auto yarn_runtime::execute_elements( const std::vector<node_element> &elements,
                                     dialogue_window &d_win ) -> exec_result
{
    for( const auto &elem : elements ) {
        switch( elem.type ) {
            case node_element::kind::dialogue: {
                auto speaker = elem.speaker.empty() ? ( npc_ ? npc_->name : "NPC" )
                                                    : elem.speaker;
                auto text    = interpolate( elem.text );
                auto line    = string_format( "%s: \"%s\"",
                                              colorize( speaker, c_light_green ),
                                              text );
                d_win.add_to_history( line );
                break;
            }

            case node_element::kind::choice_group: {
                int chosen = present_choices( elem.choices, d_win );
                if( chosen < 0 ) {
                    // No available choices — treat as stop
                    return { signal::stop, {} };
                }
                const auto &ch = elem.choices[chosen];

                // Log the player's choice
                auto choice_text = interpolate( ch.text );
                d_win.add_to_history(
                    string_format( "%s: \"%s\"",
                                   colorize( _( "You" ), c_green ),
                                   choice_text ) );

                // Execute the choice body
                auto result = execute_elements( ch.body, d_win );
                if( result.kind != signal::ok ) {
                    return result;
                }
                // ok → fall through to next element after the choice group
                break;
            }

            case node_element::kind::command: {
                auto &creg = command_registry::global();
                if( !creg.has_command( elem.command_name ) ) {
                    DebugLog( DL::Warn, DC::Dialogue )
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
                        DebugLog( DL::Error, DC::Dialogue )
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
                    DebugLog( DL::Error, DC::Dialogue )
                        << "yarn: legacy_topic requires dialogue/npc/player context";
                    return { signal::stop, {} };
                }
                // Mirror the mission-selection logic from the original dialogue loop.
                auto &chatbin = npc_->chatbin;
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
                auto next = dialogue_ref_->opt( d_win, npc_->name, talk_topic( elem.jump_target ) );
                if( next.id == "TALK_DONE" ) {
                    npc_->say( _( "Bye." ) );
                    dialogue_ref_->done = true;
                    return { signal::stop, {} };
                }
                if( next.id == "TALK_NONE" ) {
                    // Natural pop — return to the calling topic node.
                    return { signal::ok, {} };
                }
                return { signal::jump, next.id };
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
                            DebugLog( DL::Error, DC::Dialogue )
                                << "yarn: <<if>> condition did not evaluate to bool";
                        } else {
                            cond = std::get<bool>( val );
                        }
                    } catch( const std::exception &e ) {
                        DebugLog( DL::Error, DC::Dialogue )
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
            DebugLog( DL::Error, DC::Dialogue ) << "yarn: choice condition error: " << e.what();
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
            interpolate( choices[available[i]].text )
        } );
    }

    size_t selected = 0;
    const auto response_count = response_lines.size();

    ui_adaptor ui;
    ui.on_screen_resize( [&]( ui_adaptor &ui ) {
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
            if( ch == 'L' || ch == 'l' ) {
                if( npc_ ) {
                    d_win.add_to_history( npc_->short_description() );
                }
                continue;
            }
            if( ch == 'O' || ch == 'o' ) {
                if( npc_ ) {
                    d_win.add_to_history( npc_->opinion_text() );
                }
                continue;
            }
            if( ch == 'Y' || ch == 'y' ) {
                if( player_ ) {
                    player_->shout();
                    d_win.add_to_history( player_->is_deaf()
                                          ? _( "You yell, but can't hear yourself." )
                                          : _( "You yell." ) );
                }
                continue;
            }
            if( ch == 'S' || ch == 's' ) {
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

} // anonymous namespace

void load_yarn_stories()
{
    const auto dir = PATH_INFO::datadir() + "dialogue/";
    const auto files = get_files_from_path( ".yarn", dir, false, true );

    for( const auto &path : files ) {
        auto [story, result] = yarn_story::from_file( path );
        for( const auto &err : result.errors ) {
            DebugLog( DL::Error, DC::Dialogue ) << "yarn: " << err;
        }
        if( result.ok() ) {
            // Derive story name from filename without extension
            auto slash = path.rfind( '/' );
            if( slash == std::string::npos ) {
                slash = path.rfind( '\\' );
            }
            auto stem_start = ( slash == std::string::npos ) ? 0 : slash + 1;
            auto dot = path.rfind( '.' );
            auto stem = path.substr( stem_start, dot - stem_start );
            story_registry().emplace( stem, std::move( story ) );
        }
    }
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
    // Build a synthetic __legacy story: one yarn_node per JSON TALK_TOPIC.
    // Each node has a single legacy_topic element that delegates to dialogue::opt()
    // at runtime, so the yarn runtime drives all navigation for legacy dialogue.
    // DEPRECATED: Remove this function when JSON-to-Yarn migration is complete.
    auto ids = get_all_talk_topic_ids();
    if( ids.empty() ) {
        return;
    }

    yarn_story legacy;
    for( const auto &id : ids ) {
        yarn_node node;
        node.title = id;
        node_element elem;
        elem.type = node_element::kind::legacy_topic;
        elem.jump_target = id;
        node.elements.push_back( std::move( elem ) );
        legacy.add_node( std::move( node ) );
    }
    story_registry().emplace( "__legacy", std::move( legacy ) );
}

// ============================================================
// Conversation context (thread-local so global registry lambdas can read it)
// ============================================================

namespace
{

struct yarn_conv_ctx {
    npc      *npc_ref      = nullptr;
    player   *player_ref   = nullptr;
    // DEPRECATED: Only used by the legacy JSON dialogue shim. Remove when migration complete.
    dialogue *dialogue_ref = nullptr;
};

thread_local yarn_conv_ctx g_conv_ctx;

struct conv_ctx_guard {
    ~conv_ctx_guard() { g_conv_ctx = {}; }
};

// Convert a yarn value to a string suitable for Creature::set_value storage.
// Numbers that are whole integers are stored without a decimal point.
auto value_to_storage_string( const value &v ) -> std::string
{
    return std::visit( []( const auto &x ) -> std::string {
        using T = std::decay_t<decltype( x )>;
        if constexpr( std::is_same_v<T, bool> ) {
            return x ? "true" : "false";
        } else if constexpr( std::is_same_v<T, double> ) {
            if( x == std::floor( x ) && std::abs( x ) < 1e15 ) {
                return std::to_string( static_cast<long long>( x ) );
            }
            return std::to_string( x );
        } else {
            return x;
        }
    }, v );
}

} // anonymous namespace

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
        if( !p ) {
            return false;
        }
        auto id    = itype_id( std::get<std::string>( args[0] ) );
        auto count = static_cast<int>( std::get<double>( args[1] ) );
        return static_cast<int>( p->all_items_with_id( id ).size() ) >= count;
    } );

    reg.add( "u_has_skill", {vt::string, vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p ) {
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
        if( !n ) {
            return false;
        }
        auto att = n->get_attitude();
        return att == NPCATT_FOLLOW || att == NPCATT_LEAD || att == NPCATT_HEAL;
    } );

    reg.add( "npc_is_enemy", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) {
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
        if( !n ) {
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
        if( !p ) {
            return 0.0;
        }
        const auto &s = p->get_value( std::get<std::string>( args[0] ) );
        return s.empty() ? 0.0 : static_cast<double>( std::stoll( s ) );
    } );

    reg.add( "npc_get_var", {vt::string}, vt::string,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        return n ? n->get_value( std::get<std::string>( args[0] ) ) : std::string{};
    } );

    reg.add( "npc_get_var_num", {vt::string}, vt::number,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) {
            return 0.0;
        }
        const auto &s = n->get_value( std::get<std::string>( args[0] ) );
        return s.empty() ? 0.0 : static_cast<double>( std::stoll( s ) );
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
        if( !n ) {
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
        if( !n ) {
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
        if( !n || !p || !n->chatbin.mission_selected ) {
            return false;
        }
        return n->chatbin.mission_selected->is_complete( p->getID() );
    } );

    reg.add( "mission_incomplete", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        auto *p = g_conv_ctx.player_ref;
        if( !n || !p || !n->chatbin.mission_selected ) {
            return false;
        }
        return !n->chatbin.mission_selected->is_complete( p->getID() );
    } );

    // ============================================================
    // Time and calendar
    // ============================================================

    // Returns the current season as a lowercase string: "spring", "summer", "autumn", "winter".
    reg.add( "get_season", {}, vt::string, []( const std::vector<value> & ) -> value {
        switch( season_of_year( calendar::turn ) ) {
            case SPRING: return std::string( "spring" );
            case SUMMER: return std::string( "summer" );
            case AUTUMN: return std::string( "autumn" );
            case WINTER: return std::string( "winter" );
            default:     return std::string( "spring" );
        }
    } );

    // True if the current season matches the given string ("spring", "summer", "autumn", "winter").
    reg.add( "is_season", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        const auto &s = std::get<std::string>( args[0] );
        switch( season_of_year( calendar::turn ) ) {
            case SPRING: return s == "spring";
            case SUMMER: return s == "summer";
            case AUTUMN: return s == "autumn";
            case WINTER: return s == "winter";
            default:     return false;
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
        if( !p ) {
            return false;
        }
        if( const optional_vpart_position vp = get_map().veh_at( p->pos() ) ) {
            return vp->vehicle().is_moving() && vp->vehicle().player_in_control( *p );
        }
        return false;
    } );

    reg.add( "npc_driving", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) {
            return false;
        }
        if( const optional_vpart_position vp = get_map().veh_at( n->pos() ) ) {
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
        if( !n ) {
            return false;
        }
        const auto &key = std::get<std::string>( args[0] );
        auto it = ally_rule_strs.find( key );
        if( it == ally_rule_strs.end() ) {
            DebugLog( DL::Warn, DC::Dialogue )
                << "yarn: npc_has_rule: unknown rule '" << key << "'";
            return false;
        }
        return n->rules.has_flag( it->second.rule );
    } );

    reg.add( "npc_has_override", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) {
            return false;
        }
        const auto &key = std::get<std::string>( args[0] );
        auto it = ally_rule_strs.find( key );
        if( it == ally_rule_strs.end() ) {
            DebugLog( DL::Warn, DC::Dialogue )
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
        if( !n ) {
            return false;
        }
        const auto &key = std::get<std::string>( args[0] );
        auto it = aim_rule_strs.find( key );
        if( it == aim_rule_strs.end() ) {
            DebugLog( DL::Warn, DC::Dialogue )
                << "yarn: npc_aim_rule: unknown rule '" << key << "'";
            return false;
        }
        return n->rules.aim == it->second;
    } );

    reg.add( "npc_engagement_rule", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) {
            return false;
        }
        const auto &key = std::get<std::string>( args[0] );
        auto it = combat_engagement_strs.find( key );
        if( it == combat_engagement_strs.end() ) {
            DebugLog( DL::Warn, DC::Dialogue )
                << "yarn: npc_engagement_rule: unknown rule '" << key << "'";
            return false;
        }
        return n->rules.engagement == it->second;
    } );

    reg.add( "npc_cbm_reserve_rule", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) {
            return false;
        }
        const auto &key = std::get<std::string>( args[0] );
        auto it = cbm_reserve_strs.find( key );
        if( it == cbm_reserve_strs.end() ) {
            DebugLog( DL::Warn, DC::Dialogue )
                << "yarn: npc_cbm_reserve_rule: unknown rule '" << key << "'";
            return false;
        }
        return n->rules.cbm_reserve == it->second;
    } );

    reg.add( "npc_cbm_recharge_rule", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) {
            return false;
        }
        const auto &key = std::get<std::string>( args[0] );
        auto it = cbm_recharge_strs.find( key );
        if( it == cbm_recharge_strs.end() ) {
            DebugLog( DL::Warn, DC::Dialogue )
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
        if( !n ) {
            return false;
        }
        map &here = get_map();
        return !here.has_flag( TFLAG_INDOORS, here.getabs( n->pos() ) );
    } );

    // True if the NPC is in a safe-space overmap tile.
    reg.add( "at_safe_space", {}, vt::boolean,
    []( const std::vector<value> & ) -> value {
        auto *n = g_conv_ctx.npc_ref;
        if( !n ) {
            return false;
        }
        return get_overmapbuffer( n->get_dimension() ).is_safe( n->global_omt_location() );
    } );

    // True if the player is standing on the given overmap terrain type.
    reg.add( "u_at_om_location", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p ) {
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
        if( !n ) {
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
        if( !p || !g ) {
            return false;
        }
        const auto &role = std::get<std::string>( args[0] );
        auto npcs = g->get_npcs_if( [&]( const npc &guy ) {
            return p->posz() == guy.posz()
                && guy.companion_mission_role_id == role
                && rl_dist( p->pos(), guy.pos() ) <= 48;
        } );
        return !npcs.empty();
    } );

    // True if the player has at least the given number of NPC allies.
    reg.add( "npc_allies", {vt::number}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        if( !g ) {
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
        if( !p || !n ) {
            return false;
        }
        return std::ranges::any_of( p->inv_dump(),
            [n]( const item *it ) { return it->is_old_owner( *n, true ); } );
    } );

    // ============================================================
    // Missions (player)
    // ============================================================

    // True if the player has an active mission of the given type ID.
    reg.add( "u_has_mission", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p ) {
            return false;
        }
        auto *av = dynamic_cast<avatar *>( p );
        if( !av ) {
            return false;
        }
        auto target = mission_type_id( std::get<std::string>( args[0] ) );
        return std::ranges::any_of( av->get_active_missions(),
            [&target]( mission *m ) { return m->mission_id() == target; } );
    } );

    // ============================================================
    // Recipe knowledge
    // ============================================================

    reg.add( "u_know_recipe", {vt::string}, vt::boolean,
    []( const std::vector<value> &args ) -> value {
        auto *p = g_conv_ctx.player_ref;
        if( !p ) {
            return false;
        }
        const recipe &r = recipe_id( std::get<std::string>( args[0] ) ).obj();
        return p->knows_recipe( &r );
    } );
}

// ============================================================
// Built-in commands
// ============================================================

void register_builtin_commands( command_registry &reg )
{
    // give_item "item_id" [count]
    reg.add( "give_item", 1, 2, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        if( p ) {
            auto id    = itype_id( std::get<std::string>( args[0] ) );
            auto count = args.size() > 1 ? static_cast<int>( std::get<double>( args[1] ) ) : 1;
            p->add_item_with_id( id, count );
        }
        return command_signal::none;
    } );

    // take_item "item_id" [count]
    reg.add( "take_item", 1, 2, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        if( p ) {
            auto id    = itype_id( std::get<std::string>( args[0] ) );
            auto count = args.size() > 1 ? static_cast<int>( std::get<double>( args[1] ) ) : 1;
            p->use_amount( id, count );
        }
        return command_signal::none;
    } );

    // add_effect "effect_id" [duration_turns]
    reg.add( "add_effect", 1, 2, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        if( p ) {
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
        if( p ) {
            p->remove_effect( efftype_id( std::get<std::string>( args[0] ) ) );
        }
        return command_signal::none;
    } );

    // npc_follow — NPC starts following the player
    reg.add( "npc_follow", []( const std::vector<value> & ) -> command_signal {
        auto *n = g_conv_ctx.npc_ref;
        if( n ) {
            n->set_attitude( NPCATT_FOLLOW );
        }
        return command_signal::none;
    } );

    // npc_stop_follow — NPC stops following
    reg.add( "npc_stop_follow", []( const std::vector<value> & ) -> command_signal {
        auto *n = g_conv_ctx.npc_ref;
        if( n ) {
            n->set_attitude( NPCATT_NULL );
        }
        return command_signal::none;
    } );

    // npc_set_attitude "attitude_id"
    // Accepts the same string IDs used by npc_attitude_id() (e.g. "NPCATT_KILL")
    reg.add( "npc_set_attitude", 1, []( const std::vector<value> &args ) -> command_signal {
        auto *n = g_conv_ctx.npc_ref;
        if( n ) {
            const auto &att_str = std::get<std::string>( args[0] );
            for( int i = 0; i < NPCATT_END; ++i ) {
                auto att = static_cast<npc_attitude>( i );
                if( npc_attitude_id( att ) == att_str ) {
                    n->set_attitude( att );
                    return command_signal::none;
                }
            }
            DebugLog( DL::Warn, DC::Dialogue )
                << "yarn: npc_set_attitude: unknown attitude '" << att_str << "'";
        }
        return command_signal::none;
    } );

    // Arbitrary character variables

    // u_set_var "key" value  — stores any typed value as a string
    reg.add( "u_set_var", 2, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        if( p ) {
            p->set_value( std::get<std::string>( args[0] ),
                          value_to_storage_string( args[1] ) );
        }
        return command_signal::none;
    } );

    // u_remove_var "key"
    reg.add( "u_remove_var", 1, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        if( p ) {
            p->remove_value( std::get<std::string>( args[0] ) );
        }
        return command_signal::none;
    } );

    // u_adjust_var "key" amount  — adds amount to stored numeric variable
    reg.add( "u_adjust_var", 2, []( const std::vector<value> &args ) -> command_signal {
        auto *p = g_conv_ctx.player_ref;
        if( p ) {
            const auto &key    = std::get<std::string>( args[0] );
            auto        amount = static_cast<long long>( std::get<double>( args[1] ) );
            const auto &stored = p->get_value( key );
            auto current = stored.empty() ? 0LL : std::stoll( stored );
            p->set_value( key, std::to_string( current + amount ) );
        }
        return command_signal::none;
    } );

    // npc_set_var "key" value
    reg.add( "npc_set_var", 2, []( const std::vector<value> &args ) -> command_signal {
        auto *n = g_conv_ctx.npc_ref;
        if( n ) {
            n->set_value( std::get<std::string>( args[0] ),
                          value_to_storage_string( args[1] ) );
        }
        return command_signal::none;
    } );

    // npc_remove_var "key"
    reg.add( "npc_remove_var", 1, []( const std::vector<value> &args ) -> command_signal {
        auto *n = g_conv_ctx.npc_ref;
        if( n ) {
            n->remove_value( std::get<std::string>( args[0] ) );
        }
        return command_signal::none;
    } );

    // npc_adjust_var "key" amount
    reg.add( "npc_adjust_var", 2, []( const std::vector<value> &args ) -> command_signal {
        auto *n = g_conv_ctx.npc_ref;
        if( n ) {
            const auto &key    = std::get<std::string>( args[0] );
            auto        amount = static_cast<long long>( std::get<double>( args[1] ) );
            const auto &stored = n->get_value( key );
            auto current = stored.empty() ? 0LL : std::stoll( stored );
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
            if( auto *n = g_conv_ctx.npc_ref ) {
                fn( *n );
            }
            return command_signal::none;
        } );
    };

    auto npc_stop = [&reg]( const char *name, void( *fn )( npc & ) ) {
        reg.add( name, [fn]( const std::vector<value> & ) -> command_signal {
            if( auto *n = g_conv_ctx.npc_ref ) {
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
    npc_fn( "assign_mission",  talk_function::assign_mission );
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
}

// ============================================================
// Integration
// ============================================================

auto try_yarn_dialogue( dialogue_window &d_win, npc &n, player &p ) -> bool
{
    if( n.chatbin.yarn_story.empty() ) {
        return false;
    }
    if( !has_yarn_story( n.chatbin.yarn_story ) ) {
        DebugLog( DL::Warn, DC::Dialogue )
            << "yarn: NPC '" << n.name << "' references unknown story '"
            << n.chatbin.yarn_story << "'";
        return false;
    }

    const auto &story = get_yarn_story( n.chatbin.yarn_story );
    d_win.print_header( n.name );

    g_conv_ctx = { &n, &p };
    conv_ctx_guard guard;

    yarn_runtime::options opts{
        .story        = story,
        .registry     = func_registry::global(),
        .starting_node = "Start",
        .npc_ref       = &n,
        .player_ref    = &p
    };
    yarn_runtime runtime( std::move( opts ) );
    runtime.run( d_win );

    return true;
}

} // namespace yarn
