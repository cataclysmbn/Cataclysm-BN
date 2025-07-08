#pragma once

#include <memory>
#include <string>
#include <vector>

/**
 * Runtime localization system for Cataclysm: Bright Nights.
 *
 * Implements translated string lookup within gettext MO files
 * with full support for integer plural forms.
 *
 * Key differences from GNU libintl:
 * 1. Does not support translation domains
 * 2. Does not support encoding conversion, operates on UTF-8 only
 * 3. Does not depend on locale / environment variables
 * 4. Supports loading MO files from arbitrary paths
 * 5. Supports loading multiple MO files into a single "domain"
 *
 * For MO file structure, see GNU gettext manual:
 * https://www.gnu.org/software/gettext/manual/
 */

namespace cata_libintl
{
using u8 = uint8_t;
using u32 = uint32_t;

/**
 * Plural forms AST node type.
 * Also used for token identification during parsing.
 */
enum class PlfOp : int {
    Mod,       // a % b
    Eq,        // a == b
    NotEq,     // a != b
    GreaterEq, // a >= b
    Greater,   // a > b
    LessEq,    // a <= b
    Less,      // a < b
    And,       // a && b
    Or,        // a || b
    TerCond,   // ?

    Literal,   // numeric literal
    Variable,  // the variable (n)

    BrOpen,    // (
    BrClose,   // )
    TerDelim,  // :

    NumOps,
};

struct PlfNode;
using PlfNodePtr = std::unique_ptr<PlfNode>;

/** Plural forms AST node */
struct PlfNode {
    size_t literal_val;
    PlfNodePtr a;
    PlfNodePtr b;
    PlfNodePtr c;
    PlfOp op = PlfOp::NumOps;

    size_t eval( size_t n ) const;
    std::string debug_dump() const;
};

/**
 * Parse plural rules expression and build AST.
 * @throws std::runtime_error on failure.
 */
PlfNodePtr parse_plural_rules( const std::string &s );

/**
 * Translation catalogue. Corresponds to single MO file.
 */
class trans_catalogue
{
    private:
        // Represents 1 entry in MO string table
        struct string_descr {
            u32 length;
            u32 offset;
        };
        struct catalogue_plurals_info {
            size_t num = 0;
            PlfNodePtr expr = nullptr;
        };
        using meta_headers = std::vector<std::string>;

        // =========== MEMBERS ===========

        bool is_little_endian = true; // File endianness
        std::string buffer; // Data buffer
        catalogue_plurals_info plurals; // Plural rules
        u32 number_of_strings = 0; // Number of strings (id-translation pairs)
        u32 offs_orig_table = 0; // Offset of table with original strings
        u32 offs_trans_table = 0; // Offset of table with translated strings

        // =========== METHODS ===========

        explicit trans_catalogue( std::string buffer );

        void set_buffer( std::string buffer ) {
            this->buffer = std::move( buffer );
        }
        u32 buf_size() const {
            return static_cast<u32>( buffer.size() );
        }

        u8 get_u8( u32 offs ) const;
        u8 get_u8_unsafe( u32 offs ) const {
            return static_cast<u8>( buffer[offs] );
        }

        u32 get_u32( u32 offs ) const;
        u32 get_u32_unsafe( u32 offs ) const {
            if( is_little_endian ) {
                return get_u8_unsafe( offs ) |
                       get_u8_unsafe( offs + 1 ) << 8 |
                       get_u8_unsafe( offs + 2 ) << 16 |
                       get_u8_unsafe( offs + 3 ) << 24;
            } else {
                return get_u8_unsafe( offs + 3 ) |
                       get_u8_unsafe( offs + 2 ) << 8 |
                       get_u8_unsafe( offs + 1 ) << 16 |
                       get_u8_unsafe( offs ) << 24;
            }
        }

        string_descr get_string_descr( u32 offs ) const;
        string_descr get_string_descr_unsafe( u32 offs ) const;

        const char *offs_to_cstr( u32 offs ) const {
            return &buffer[offs];
        }

        void process_file_header();
        void check_string_terminators();
        void check_string_plurals();
        std::string get_metadata() const;
        static void check_encoding( const meta_headers &headers );
        static catalogue_plurals_info parse_plf_header( const meta_headers &headers );

    public:
        /**
         * Load translation catalogue from given MO file.
         * @throws std::runtime_error on failure.
         */
        static trans_catalogue load_from_file( const std::string &file_path );
        /**
         * Load translation catalogue from given MO file in memory.
         * @throws std::runtime_error on failure.
         */
        static trans_catalogue load_from_memory( std::string mo_file );

        /** Number of entries in the catalogue. */
        u32 get_num_strings() const {
            return number_of_strings;
        }
        /** Get singular translated string of given entry. */
        const char *get_nth_translation( u32 n ) const;
        /** Get correct plural translated string of given entry for given number. */
        const char *get_nth_pl_translation( u32 n, size_t num ) const;
        /** Get original msgid (with msgctxt) of given entry. */
        const char *get_nth_orig_string( u32 n ) const;
        /** Check whether translated string contains plural forms. */
        bool check_nth_translation_has_plf( u32 n ) const;
};

/**
 * Translation library.
 * Represents collection of catalogues merged into a single pool ready for use.
 */
class trans_library
{
    private:
        // Describes which catalogue the string comes from
        struct library_string_descr {
            u32 catalogue;
            u32 entry;
        };

        // Full index of loaded strings
        std::vector<library_string_descr> strings;

        // Full index of loaded catalogues
        std::vector<trans_catalogue> catalogues;

        void build_string_table();
        std::vector<library_string_descr>::const_iterator find_entry( const char *id ) const;
        const char *lookup_string( const char *id ) const;
        const char *lookup_pl_string( const char *id, size_t n ) const;

    public:
        /**
         * Create new library from catalogues.
         *
         * If 2 or more catalogues have entries with same id (msgid + optional msgctxt),
         * only the entry from the first such catalogue is used.
         */
        static trans_library create( std::vector<trans_catalogue> catalogues );

        /**
         * @name Translation lookup
         * @param msgid singular original string
         * @param msgid_pl plural original string
         * @param msgctxt translation context
         * @param n number to choose plural form for
         * @returns If translation is found, returns translated string or correct plural form.
         *          Otherwise, returns original string or original plural string
         *          depending on n (n==1 ? msgid : msgid_pl).
         *
         * @{
         */
        const char *get( const char *msgid ) const;
        const char *get_pl( const char *msgid, const char *msgid_pl, size_t n ) const;
        const char *get_ctx( const char *msgctxt, const char *msgid ) const;
        const char *get_ctx_pl( const char *msgctxt, const char *msgid, const char *msgid_pl,
                                size_t n ) const;
        /** @} */
};
} // namespace cata_libintl


