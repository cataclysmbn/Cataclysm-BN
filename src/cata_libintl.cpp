#include "cata_libintl.h"

#include "fstream_utils.h"
#include "string_utils.h"
#include "string_formatter.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cassert>
#include <iostream>
#include <limits>
#include <sstream>

// ===============================================================================================
// Plural forms
// ===============================================================================================
//
// Plural forms expression evaluation is done by evaluating a simple AST built from the
// source string. The expression follows same operator precedence as C, and the parser here
// is based on same simplified grammar as the parser used by Poedit:
//
// Expr:                     // Expression
//     OrExpr ? Expr : Expr
//     OrExpr
// OrExpr:                   // Logical OR
//     AndExpr || OrExpr
//     AndExpr
// AndExpr:                  // Logical AND
//     EqExpr && AndExpr
//     EqExpr
// EqExpr:                   // Equality
//     CmpExpr == CmpExpr
//     CmpExpr != CmpExpr
//     CmpExpr
// CmpExpr:                  // Comparison
//     ModExpr >= ModExpr
//     ModExpr > ModExpr
//     ModExpr <= ModExpr
//     ModExpr < ModExpr
//     ModExpr
// ModExpr:                  // Modulo
//     Value % Value
//     Value
// Value:                    // Value
//     ( Expr )   // expression in brackets
//     [0..9]+    // unsigned integer
//     n          // the variable
//
// Note that this grammar does not cover all valid C expressions, or all valid C operators
// (e.g. "n % 10 % 3", or "(n+1) % 2"), but that doesn't seem to cause problems.

namespace cata_libintl
{
struct PlfToken {
    PlfOp kind;
    size_t num;
    size_t start;
    size_t len;
};

static const std::array<std::pair<std::string, PlfOp>, 14> simple_tokens = {{
        { "?", PlfOp::TerCond },
        { ":", PlfOp::TerDelim },
        { "(", PlfOp::BrOpen },
        { ")", PlfOp::BrClose },
        { "n", PlfOp::Variable },
        { "%", PlfOp::Mod },
        { "==", PlfOp::Eq },
        { "!=", PlfOp::NotEq },
        { ">=", PlfOp::GreaterEq },
        { ">", PlfOp::Greater },
        { "<=", PlfOp::LessEq },
        { "<", PlfOp::Less },
        { "&&", PlfOp::And },
        { "||", PlfOp::Or },
    }
};

struct PlfTStream {
    const std::string *s;
    size_t pos;
    size_t end;

    PlfTStream( const std::string *raw ) {
        s = raw;
        pos = 0;
        if( s ) {
            end = s->size();
        }
    }

    PlfTStream cloned() const {
        return *this;
    }

    PlfToken peek_internal() const {
        std::string tok;
        size_t pos = this->pos;
        while( pos < end ) {
            char c = ( *s )[pos];
            if( c == ' ' || c == '\t' ) {
                // skip whitespace
                pos++;
                continue;
            } else if( c >= '0' && c <= '9' ) {
                // number
                size_t start_pos = pos;
                while( pos < end && c >= '0' && c <= '9' ) {
                    tok.push_back( c );
                    pos++;
                    c = ( *s )[pos];
                }
                try {
                    size_t ul = std::stoul( tok );
                    if( ul > std::numeric_limits<uint32_t>::max() ) {
                        throw std::out_of_range( "stoul" );
                    }
                    return PlfToken{ PlfOp::Literal, ul, start_pos, pos - start_pos };
                } catch( const std::logic_error &err ) {
                    std::string e = string_format( "invalid number '%s' at pos %d", tok, start_pos );
                    throw std::runtime_error( e );
                }
            }
            std::string ss = s->substr( pos, end - pos );
            for( const auto &t : simple_tokens ) {
                if( ss.starts_with( t.first ) ) {
                    return PlfToken{ t.second, 0, pos, t.first.size() };
                }
            }
            pos++;
            std::string e = string_format( "unexpected character '%c' at pos %d", c, pos - 1 );
            throw std::runtime_error( e );
        }
        return PlfToken{ PlfOp::NumOps, 0, pos, 0 };
    }

    PlfToken get() {
        PlfToken t = peek_internal();
        pos = t.start + t.len;
        return t;
    }

    PlfToken peek() const {
        return peek_internal();
    }

    PlfTStream &skip() {
        get();
        return *this;
    }

    bool has_tokens() const {
        return peek().kind != PlfOp::NumOps;
    }
};

struct ParseRet {
    PlfNodePtr expr;
    PlfTStream ts;

    ParseRet( PlfNodePtr e, const PlfTStream &ts ) : expr( std::move( e ) ), ts( ts ) {}
};

using ParserPtr = ParseRet( * )( const PlfTStream &ts );

ParseRet plf_get_expr( const PlfTStream &ts );
ParseRet plf_get_or( const PlfTStream &ts );
ParseRet plf_get_and( const PlfTStream &ts );
ParseRet plf_get_eq( const PlfTStream &ts );
ParseRet plf_get_cmp( const PlfTStream &ts );
ParseRet plf_get_mod( const PlfTStream &ts );
ParseRet plf_get_value( const PlfTStream &ts );

PlfNodePtr parse_plural_rules( const std::string &s )
{
    PlfTStream tokstr( &s );
    ParseRet ret = plf_get_expr( tokstr );
    if( ret.ts.has_tokens() ) {
        PlfToken tok = ret.ts.peek();
        std::string e = string_format( "unexpected token at pos %d", tok.start );
        throw std::runtime_error( e );
    }
    return std::move( ret.expr );
}

static bool plf_try_binary_op( ParseRet &left, PlfOp op, ParserPtr parser )
{
    const PlfTStream &ts = left.ts;
    if( ts.peek().kind != op ) {
        // not found
        return false;
    }
    ParseRet e = parser( ts.cloned().skip() );
    left = ParseRet( std::make_unique<PlfNode>( PlfNode{
        0, std::move( left.expr ), std::move( e.expr ), nullptr, op,
    } ),
    e.ts );
    return true;
}

ParseRet plf_get_expr( const PlfTStream &ts )
{
    ParseRet e1 = plf_get_or( ts );
    if( e1.ts.peek().kind != PlfOp::TerCond ) {
        return e1;
    }
    ParseRet e2 = plf_get_expr( e1.ts.cloned().skip() );
    if( e2.ts.peek().kind != PlfOp::TerDelim ) {
        PlfToken tok = e2.ts.peek();
        std::string e = string_format( "expected ternary delimiter at pos %d", tok.start );
        throw std::runtime_error( e );
    }
    ParseRet e3 = plf_get_expr( e2.ts.cloned().skip() );
    return ParseRet( std::make_unique<PlfNode>( PlfNode{
        0, std::move( e1.expr ), std::move( e2.expr ), std::move( e3.expr ), PlfOp::TerCond,
    } ),
    e3.ts );
}

ParseRet plf_get_or( const PlfTStream &ts )
{
    ParseRet ret = plf_get_and( ts );
    plf_try_binary_op( ret, PlfOp::Or, plf_get_or );
    return ret;
}

ParseRet plf_get_and( const PlfTStream &ts )
{
    ParseRet ret = plf_get_eq( ts );
    plf_try_binary_op( ret, PlfOp::And, plf_get_and );
    return ret;
}

ParseRet plf_get_eq( const PlfTStream &ts )
{
    ParseRet ret = plf_get_cmp( ts );
    if( plf_try_binary_op( ret, PlfOp::Eq, plf_get_cmp ) ) {
        return ret;
    }
    plf_try_binary_op( ret, PlfOp::NotEq, plf_get_cmp );
    return ret;
}

ParseRet plf_get_cmp( const PlfTStream &ts )
{
    ParseRet ret = plf_get_mod( ts );
    if( plf_try_binary_op( ret, PlfOp::GreaterEq, plf_get_mod ) ) {
        return ret;
    }
    if( plf_try_binary_op( ret, PlfOp::Greater, plf_get_mod ) ) {
        return ret;
    }
    if( plf_try_binary_op( ret, PlfOp::LessEq, plf_get_mod ) ) {
        return ret;
    }
    plf_try_binary_op( ret, PlfOp::Less, plf_get_mod );
    return ret;
}

ParseRet plf_get_mod( const PlfTStream &ts )
{
    ParseRet ret = plf_get_value( ts );
    plf_try_binary_op( ret, PlfOp::Mod, plf_get_value );
    return ret;
}

ParseRet plf_get_value( const PlfTStream &ts )
{
    PlfToken next = ts.peek();
    if( next.kind == PlfOp::BrOpen ) {
        // '(' expr ')'
        ParseRet e = plf_get_expr( ts.cloned().skip() );
        if( e.ts.peek().kind != PlfOp::BrClose ) {
            PlfToken tok = e.ts.peek();
            std::string e = string_format( "expected closing bracket at pos %d", tok.start );
            throw std::runtime_error( e );
        }
        return ParseRet( std::move( e.expr ), e.ts.cloned().skip() );
    } else if( next.kind == PlfOp::Literal ) {
        // number
        return ParseRet( std::make_unique<PlfNode>( PlfNode{
            next.num, nullptr, nullptr, nullptr, PlfOp::Literal,
        } ), ts.cloned().skip() );
    } else if( next.kind == PlfOp::Variable ) {
        // the variable
        return ParseRet( std::make_unique<PlfNode>( PlfNode{
            0, nullptr, nullptr, nullptr, PlfOp::Variable,
        } ), ts.cloned().skip() );
    } else {
        std::string e = string_format( "expected expression at pos %d", next.start );
        throw std::runtime_error( e );
    }
}

size_t PlfNode::eval( size_t n ) const
{
    switch( op ) {
        case PlfOp::Mod: {
            size_t right = b->eval( n );
            if( right == 0 ) {
                std::string e = string_format( "DBZ in PlfNode::eval( %d ), node='%s'", n, debug_dump() );
                throw std::runtime_error( e );
            }
            return a->eval( n ) % right;
        }
        case PlfOp::Eq:
            return a->eval( n ) == b->eval( n );
        case PlfOp::NotEq:
            return a->eval( n ) != b->eval( n );
        case PlfOp::GreaterEq:
            return a->eval( n ) >= b->eval( n );
        case PlfOp::Greater:
            return a->eval( n ) > b->eval( n );
        case PlfOp::LessEq:
            return a->eval( n ) <= b->eval( n );
        case PlfOp::Less:
            return a->eval( n ) < b->eval( n );
        case PlfOp::And:
            return a->eval( n ) && b->eval( n );
        case PlfOp::Or:
            return a->eval( n ) || b->eval( n );
        case PlfOp::TerCond:
            return a->eval( n ) ? b->eval( n ) : c->eval( n );
        case PlfOp::Literal:
            return literal_val;
        case PlfOp::Variable:
            return n;
        default:
            // unreachable
            assert( false );
    }
    return 0;
}

std::string PlfNode::debug_dump() const
{
    switch( op ) {
        case PlfOp::TerCond:
            return string_format( "(%s?%s:%s)", a->debug_dump(), b->debug_dump(), c->debug_dump() );
        case PlfOp::Literal:
            return string_format( "%d", literal_val );
        case PlfOp::Variable:
            return "n";
        default: {
            std::string ops = "x";
            for( const auto &it : simple_tokens ) {
                if( it.second == op ) {
                    ops = it.first;
                    break;
                }
            }
            return string_format( "(%s%s%s)", a->debug_dump(), ops, b->debug_dump() );
        }
    }
}

// ===============================================================================================
// Translation catalogue
// ===============================================================================================

constexpr u32 MO_STRING_DESCR_SIZE = 8;
constexpr u8 PLF_SEPARATOR = 0;

trans_catalogue trans_catalogue::load_from_file( const std::string &file_path )
{
    std::stringstream buffer;
    cata_ifstream file = std::move( cata_ifstream().mode( cata_ios_mode::binary ).open( file_path ) );
    if( !file.is_open() ) {
        throw std::runtime_error( "failed to open file" );
    }
    buffer << file->rdbuf();
    if( file.fail() ) {
        throw std::runtime_error( "failed to read file" );
    }
    return load_from_memory( buffer.str() );
}

trans_catalogue trans_catalogue::load_from_memory( std::string mo_file )
{
    return trans_catalogue( std::move( mo_file ) );
}

u8 trans_catalogue::get_u8( u32 offs ) const
{
    if( offs + 1 > buf_size() ) {
        std::string e = string_format( "tried get_u8() at offs %#x with file size %#x", offs, buf_size() );
        throw std::runtime_error( e );
    }
    return get_u8_unsafe( offs );
}

u32 trans_catalogue::get_u32( u32 offs ) const
{
    if( offs + 4 > buf_size() ) {
        std::string e = string_format( "tried get_u32() at offs %#x with file size %#x", offs, buf_size() );
        throw std::runtime_error( e );
    }
    return get_u32_unsafe( offs );
}

trans_catalogue::string_descr trans_catalogue::get_string_descr( u32 offs ) const
{
    string_descr ret;
    ret.length = get_u32( offs );
    ret.offset = get_u32( offs + 4 );
    return ret;
}

trans_catalogue::string_descr trans_catalogue::get_string_descr_unsafe( u32 offs ) const
{
    string_descr ret;
    ret.length = get_u32_unsafe( offs );
    ret.offset = get_u32_unsafe( offs + 4 );
    return ret;
}

std::string trans_catalogue::get_metadata() const
{
    // We're looking for a string with empty msgid and absent msgctxt and msgid_pl.
    // Since the strings are sorted in lexicographical order, this will be the first string.
    constexpr u32 METADATA_STRING_LEN = 0;

    string_descr k = get_string_descr_unsafe( offs_orig_table );

    if( k.length != METADATA_STRING_LEN ) {
        std::string e = string_format(
                            "invalid metadata entry (expected length %#x, got %#x)",
                            METADATA_STRING_LEN, k.length
                        );
        throw std::runtime_error( e );
    }

    string_descr v = get_string_descr_unsafe( offs_trans_table );
    return std::string( offs_to_cstr( v.offset ) );
}

void trans_catalogue::process_file_header()
{
    constexpr u32 MO_MAGIC_NUMBER_LE = 0x950412de;
    constexpr u32 MO_MAGIC_NUMBER_BE = 0xde120495;
    constexpr u32 MO_SUPPORTED_REVISION = 0;

    constexpr u32 OFFS_MAGIC_NUMBER = 0;
    constexpr u32 OFFS_REVISION = 4;
    constexpr u32 OFFS_NUM_STRINGS = 8;
    constexpr u32 OFFS_ORIG_TABLE_BEGIN = 12;
    constexpr u32 OFFS_TRANS_TABLE_BEGIN = 16;

    u32 magic = this->buffer.size() > 4 ? get_u32( OFFS_MAGIC_NUMBER ) : 0;
    if( magic != MO_MAGIC_NUMBER_LE && magic != MO_MAGIC_NUMBER_BE ) {
        throw std::runtime_error( "not a MO file" );
    }
    this->is_little_endian = magic == MO_MAGIC_NUMBER_LE;
    if( get_u32( OFFS_REVISION ) != MO_SUPPORTED_REVISION ) {
        std::string e = string_format(
                            "expected revision %d, got %d",
                            MO_SUPPORTED_REVISION,
                            get_u32( OFFS_REVISION )
                        );
        throw std::runtime_error( e );
    }

    number_of_strings = get_u32( OFFS_NUM_STRINGS );
    offs_orig_table = get_u32( OFFS_ORIG_TABLE_BEGIN );
    offs_trans_table = get_u32( OFFS_TRANS_TABLE_BEGIN );
}

void trans_catalogue::check_string_terminators()
{
    const auto check_string = [this]( u32 offs ) {
        // Check that string with its null terminator (not included in string length)
        // does not extend beyond file boundaries.
        string_descr s = get_string_descr( offs );
        if( s.offset + s.length + 1 > buf_size() ) {
            std::string e = string_format(
                                "string_descr at offs %#x: extends beyond EOF (len:%#x offs:%#x fsize:%#x)",
                                offs, s.length, s.offset, buf_size()
                            );
            throw std::runtime_error( e );
        }
        // Also check for existence of the null byte.
        u8 terminator = get_u8( s.offset + s.length );
        if( terminator != 0 ) {
            std::string e = string_format(
                                "string_descr at offs %#x: missing null terminator",
                                offs
                            );
            throw std::runtime_error( e );
        }
    };
    for( u32 i = 0; i < number_of_strings; i++ ) {
        check_string( offs_orig_table + i * MO_STRING_DESCR_SIZE );
        check_string( offs_trans_table + i * MO_STRING_DESCR_SIZE );
    }
}

void trans_catalogue::check_string_plurals()
{
    // Skip metadata (the 0th entry)
    for( u32 i = 1; i < number_of_strings; i++ ) {
        string_descr info = get_string_descr_unsafe( offs_orig_table + i * MO_STRING_DESCR_SIZE );

        // Check for null byte - msgid/msgid_plural separator
        bool has_plurals = false;
        for( u32 j = info.offset; j < info.offset + info.length; j++ ) {
            if( get_u8_unsafe( j ) == 0 ) {
                has_plurals = true;
                break;
            }
        }

        if( !has_plurals ) {
            continue;
        }

        // Count null bytes - each plural form is a null-terminated string (including last one)
        u32 offs_tr = offs_trans_table + i * MO_STRING_DESCR_SIZE;
        string_descr info_tr = get_string_descr_unsafe( offs_tr );
        size_t plural_forms = 0;
        for( u32 j = info_tr.offset; j <= info_tr.offset + info_tr.length; j++ ) {
            if( get_u8_unsafe( j ) == PLF_SEPARATOR ) {
                plural_forms += 1;
            }
        }

        // Number of plural forms should match the number specified in metadata
        if( plural_forms != this->plurals.num ) {
            std::string e = string_format(
                                "string_descr at offs %#x: expected %d plural forms, got %d",
                                offs_tr, this->plurals.num, plural_forms
                            );
            throw std::runtime_error( e );
        }
    }
}

void trans_catalogue::check_encoding( const meta_headers &headers )
{
    // HACK: The checks here are rather crude and don't account for
    //       possible extra spaces or wrong capitalization.
    //       However, we don't expect people to manually configure
    //       these headers (there're tons of software for that),
    //       and the strings here match output of our own scripts and
    //       Poedit/xgettext/Transifex, so leaving as is should be fine.
    {
        bool found = false;
        for( const std::string &entry : headers ) {
            if( !entry.starts_with( "Content-Type:" ) ) {
                continue;
            }
            found = true;
            static const std::string expected = "Content-Type: text/plain; charset=UTF-8";
            if( entry != expected ) {
                std::string e =
                    string_format( "unrecognized value in Content-Type header (wrong charset?). Expected \"%s\"",
                                   expected );
                throw std::runtime_error( e );
            }
            break;
        }
        if( !found ) {
            throw std::runtime_error( "failed to find Content-Type header" );
        }
    }
    {
        bool found = false;
        for( const std::string &entry : headers ) {
            if( !entry.starts_with( "Content-Transfer-Encoding:" ) ) {
                continue;
            }
            found = true;
            static const std::string expected = "Content-Transfer-Encoding: 8bit";
            if( entry != expected ) {
                std::string e =
                    string_format( "unrecognized value in Content-Transfer-Encoding header.  Expected \"%s\"",
                                   expected );
                throw std::runtime_error( e );
            }
            break;
        }
        if( !found ) {
            throw std::runtime_error( "failed to find Content-Transfer-Encoding header" );
        }
    }
}

trans_catalogue::catalogue_plurals_info trans_catalogue::parse_plf_header(
    const meta_headers &headers )
{
    constexpr size_t MAX_PLURAL_FORMS = 8;

    // HACK: Same as with encoding headers, we expect this header to be
    //       automatically generated. This "parser" here succeeds with
    //       MO files compiled from PO created with Transifex/Poedit/msginit,
    //       and that'll probably extend to majority of other software.

    catalogue_plurals_info ret;

    // Parse Plural-Forms header.
    std::string plf_raw;
    {
        bool found = false;
        for( const std::string &entry : headers ) {
            if( !entry.starts_with( "Plural-Forms:" ) ) {
                continue;
            }
            found = true;
            plf_raw = entry.substr( 13 ); // length of "Plural-Forms:" string
            break;
        }
        if( !found ) {
            // Default to Germanic rules (English, German, Dutch, ...)
            ret.num = 2;
            ret.expr = parse_plural_rules( "n!=1" );
            return ret;
        }
    }
    std::vector<std::string> parts = string_split( plf_raw, ';' );
    if( parts.size() != 3 ) {
        throw std::runtime_error( "expected Plural-Forms header to have 2 ';' characters" );
    }

    // Parse & validate nplurals
    {
        std::string plf_n_raw = parts[0];
        if( !plf_n_raw.starts_with( " nplurals=" ) ) {
            throw std::runtime_error( "failed to parse Plural-Forms header" );
        }
        plf_n_raw = plf_n_raw.substr( 10 ); // 10 is length of " nplurals=" string
        try {
            ret.num = std::stoul( plf_n_raw );
        } catch( const std::runtime_error &err ) {
            std::string e = string_format( "failed to parse Plural-Forms nplurals number '%s': %s", plf_n_raw,
                                           err.what() );
            throw std::runtime_error( e );
        }
        if( ret.num == 0 || ret.num > MAX_PLURAL_FORMS ) {
            std::string e = string_format( "expected at most 1-%d plural forms, got %d", MAX_PLURAL_FORMS,
                                           ret.num );
            throw std::runtime_error( e );
        }
    }

    // Parse & validate plural formula
    {
        std::string plf_rules_raw = parts[1];
        if( !plf_rules_raw.starts_with( " plural=" ) ) {
            throw std::runtime_error( "failed to parse Plural-Forms header" );
        }
        plf_rules_raw = plf_rules_raw.substr( 8 ); // 8 is length of " plural=" string
        try {
            ret.expr = parse_plural_rules( plf_rules_raw );
        } catch( const std::runtime_error &err ) {
            std::string e = string_format( "failed to parse plural forms formula: %s", err.what() );
            throw std::runtime_error( e );
        }
    }

    return ret;
}

trans_catalogue::trans_catalogue( std::string buffer )
{
    set_buffer( std::move( buffer ) );
    process_file_header();
    check_string_terminators();

    meta_headers headers = string_split( get_metadata(), '\n' );

    check_encoding( headers );
    this->plurals = parse_plf_header( headers );

    check_string_plurals();
}

bool trans_catalogue::check_nth_translation_has_plf( u32 n ) const
{
    u32 descr_offs = offs_trans_table + n * MO_STRING_DESCR_SIZE;
    string_descr r = get_string_descr_unsafe( descr_offs );

    for( u32 offs = r.offset; offs < r.offset + r.length; offs++ ) {
        if( get_u8_unsafe( offs ) == PLF_SEPARATOR ) {
            return true;
        }
    }

    return false;
}

const char *trans_catalogue::get_nth_orig_string( u32 n ) const
{
    u32 descr_offs = offs_orig_table + n * MO_STRING_DESCR_SIZE;
    string_descr r = get_string_descr_unsafe( descr_offs );

    return offs_to_cstr( r.offset );
}

const char *trans_catalogue::get_nth_translation( u32 n ) const
{
    u32 descr_offs = offs_trans_table + n * MO_STRING_DESCR_SIZE;
    string_descr r = get_string_descr_unsafe( descr_offs );

    return offs_to_cstr( r.offset );
}

const char *trans_catalogue::get_nth_pl_translation( u32 n, size_t num ) const
{
    u32 descr_offs = offs_trans_table + n * MO_STRING_DESCR_SIZE;
    string_descr r = get_string_descr_unsafe( descr_offs );

    size_t plf = plurals.expr->eval( num );

    if( plf == 0 || plf >= plurals.num ) {
        return offs_to_cstr( r.offset );
    }
    size_t curr_plf = 0;
    for( u32 offs = r.offset; offs < r.offset + r.length; offs++ ) {
        if( get_u8_unsafe( offs ) == PLF_SEPARATOR ) {
            curr_plf += 1;
            if( plf == curr_plf ) {
                return offs_to_cstr( offs + 1 );
            }
        }
    }
    return nullptr;
}

// ===============================================================================================
// Translation library
// ===============================================================================================

std::vector<trans_library::library_string_descr>::const_iterator trans_library::find_entry(
    const char *id ) const
{
    auto it = std::lower_bound( strings.begin(), strings.end(),
    id, [this]( const library_string_descr & a_descr, const char *b ) -> bool {
        const char *a = this->catalogues[a_descr.catalogue].get_nth_orig_string( a_descr.entry );
        return strcmp( a, b ) < 0;
    } );

    if( it != strings.end() ) {
        const char *found = catalogues[it->catalogue].get_nth_orig_string( it->entry );
        if( strcmp( id, found ) == 0 ) {
            return it;
        }
    }

    return strings.end();
}

void trans_library::build_string_table()
{
    assert( strings.empty() );

    for( size_t i_cat = 0; i_cat < catalogues.size(); i_cat++ ) {
        const trans_catalogue &cat = catalogues[i_cat];
        u32 num = cat.get_num_strings();
        // 0th entry is the metadata, we skip it
        strings.reserve( num - 1 );
        for( u32 i = 1; i < num; i++ ) {
            const char *i_cstr = cat.get_nth_orig_string( i );

            auto it = std::lower_bound( strings.begin(), strings.end(),
            i_cstr, [this]( const library_string_descr & a_descr, const char *b ) -> bool {
                const char *a = this->catalogues[a_descr.catalogue].get_nth_orig_string( a_descr.entry );
                return strcmp( a, b ) < 0;
            } );

            library_string_descr desc = { static_cast<u32>( i_cat ), i };
            if( it == strings.end() ) {
                // Not found, or all elements are greater
                strings.push_back( desc );
            } else if( strcmp( catalogues[it->catalogue].get_nth_orig_string( it->entry ), i_cstr ) == 0 ) {
                // Overwrite existing string only if new string has plural form(s),
                // but existing one does not.
                if(
                    cat.check_nth_translation_has_plf( i ) &&
                    !catalogues[it->catalogue].check_nth_translation_has_plf( it->entry )
                ) {
                    *it = desc;
                }
                continue;
            } else {
                strings.insert( it, desc );
            }
        }
    }
}

trans_library trans_library::create( std::vector<trans_catalogue> catalogues )
{
    trans_library lib;
    lib.catalogues = std::move( catalogues );
    lib.build_string_table();
    return lib;
}

const char *trans_library::lookup_string( const char *id ) const
{
    auto it = find_entry( id );
    if( it == strings.end() ) {
        return nullptr;
    }
    return catalogues[it->catalogue].get_nth_translation( it->entry );
}

const char *trans_library::lookup_pl_string( const char *id, size_t n ) const
{
    auto it = find_entry( id );
    if( it == strings.end() ) {
        return nullptr;
    }
    return catalogues[it->catalogue].get_nth_pl_translation( it->entry, n );
}

const char *trans_library::get( const char *msgid ) const
{
    const char *ret = lookup_string( msgid );
    return ret ? ret : msgid;
}

const char *trans_library::get_pl( const char *msgid, const char *msgid_pl, size_t n ) const
{
    const char *ret = lookup_pl_string( msgid, n );
    return ret ? ret : ( n == 1  ? msgid : msgid_pl );
}

const char *trans_library::get_ctx( const char *msgctxt, const char *msgid ) const
{
    std::string buf;
    buf.reserve( strlen( msgctxt ) + 1 + strlen( msgid ) );
    buf += msgctxt;
    buf += '\4';
    buf += msgid;
    const char *ret = lookup_string( buf.c_str() );
    return ret ? ret : msgid;
}

const char *trans_library::get_ctx_pl( const char *msgctxt, const char *msgid, const char *msgid_pl,
                                       size_t n ) const
{
    std::string buf;
    buf.reserve( strlen( msgctxt ) + 1 + strlen( msgid ) );
    buf += msgctxt;
    buf += '\4';
    buf += msgid;
    const char *ret = lookup_pl_string( buf.c_str(), n );
    return ret ? ret : ( n == 1  ? msgid : msgid_pl );
}
} // namespace cata_libintl
