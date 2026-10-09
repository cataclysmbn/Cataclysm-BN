#include "translations.h"

#include <algorithm>
#include <ranges>

#include "cached_options.h"
#include "cata_libintl.h"
#include "catacharset.h"
#include "json.h"
#include "language.h"
#include "rng.h"
#include "string_utils.h"

namespace localization
{
namespace
{
thread_local auto work = snapshot_work_counts {};
const auto empty_input = translation_input {};

auto input_identity( const translation_input &input ) -> std::uint64_t
{
    auto hash = content_hash{};
    hash.add( "translation-input-v1" );
    hash.add( input.needs_translation ? "translate" : "literal" );
    hash.add( input.context ? "context" : "no-context" );
    hash.add( input.context ? std::string_view( *input.context ) : std::string_view{} );
    hash.add( input.raw );
    hash.add( input.plural ? "plural" : "no-plural" );
    hash.add( input.plural ? std::string_view( *input.plural ) : std::string_view{} );
    return hash.value();
}
const auto empty_identity = input_identity( empty_input );
} // namespace

auto content_hash::add( const std::string_view value ) -> void
{
    // Length framing includes embedded NULs and prevents separator collisions.
    for( const auto shift : std::views::iota( 0, 8 ) ) {
        value_ ^= static_cast<unsigned char>( static_cast<std::uint64_t>( value.size() ) >> ( shift * 8 ) );
        value_ *= 1099511628211ULL;
    }
    for( const auto byte : value ) {
        value_ ^= static_cast<unsigned char>( byte );
        value_ *= 1099511628211ULL;
    }
}
auto snapshot_work() -> snapshot_work_counts { return work; }
auto reset_snapshot_work() -> void { work = {}; }
auto record_catalogue_hash( const std::size_t bytes ) -> void { work.catalogue_hashed_bytes += bytes; }

auto copy_input( const translation_input &input ) -> translation_input
{
    auto result = input;
    work.input_copied_bytes += input.raw.size() + ( input.context ? input.context->size() : 0 ) +
                               ( input.plural ? input.plural->size() : 0 );
    return result;
}
auto publish_input( translation_input input ) -> std::shared_ptr<const translation_input>
{
    input.fingerprint = input_identity( input );
    work.input_hashed_bytes += input.raw.size() + ( input.context ? input.context->size() : 0 ) +
                               ( input.plural ? input.plural->size() : 0 );
    auto result = std::make_shared<const translation_input>( std::move( input ) );
    ++work.input_publications;
    return result;
}
auto copied_string( const std::string &value ) -> std::string
{
    auto result = value;
    work.input_copied_bytes += result.size();
    return result;
}
auto automatic_plural( const std::string &raw ) -> std::string
{
    auto result = copied_string( raw );
    result += "s";
    return result;
}

struct locale_snapshot::publication {
    cata_libintl::trans_library library;
    std::uint64_t fingerprint;
};

auto locale_snapshot::from_library( cata_libintl::trans_library library,
                                    const std::uint64_t fingerprint ) -> locale_snapshot
{
    auto result = locale_snapshot{};
    result.publication_ = std::make_shared<const publication>( publication{
        .library = std::move( library ), .fingerprint = fingerprint } );
    ++work.catalogue_publications;
    return result;
}
auto locale_snapshot::from_catalogues( std::vector<std::string> catalogues ) -> locale_snapshot
{
    auto hash = content_hash{};
    hash.add( "locale-catalogues-v1" );
    auto parsed = std::vector<cata_libintl::trans_catalogue> {};
    for( auto &buffer : catalogues ) {
        auto buffer_hash = content_hash{};
        buffer_hash.add( "mo-buffer-v1" );
        buffer_hash.add( buffer );
        record_catalogue_hash( buffer.size() );
        parsed.push_back( cata_libintl::trans_catalogue::load_from_memory( std::move( buffer ) ) );
        hash.add( std::to_string( buffer_hash.value() ) );
    }
    return from_library( cata_libintl::trans_library::create( std::move( parsed ) ), hash.value() );
}
auto locale_snapshot::fingerprint() const -> std::uint64_t
{
    if( publication_ ) { return publication_->fingerprint; }
    static const auto empty = []() {
        auto hash = content_hash{};
        hash.add( "locale-catalogues-v1" );
        return hash.value();
    }
    ();
    return empty;
}
auto locale_snapshot::library() const -> const cata_libintl::trans_library & // *NOPAD*
{
    static const auto empty = cata_libintl::trans_library::create( {} );
    return publication_ ? publication_->library : empty;
}
auto translation_snapshot::fingerprint() const -> std::uint64_t
{
    return input_ ? input_->fingerprint : empty_identity;
}
auto translation_snapshot::raw_view() const -> std::string_view
{
    return input_ ? std::string_view( input_->raw ) : std::string_view{};
}
auto translation_snapshot::translated( const locale_snapshot &locale,
                                       const int num ) const -> std::string
{
    const auto &input = input_ ? *input_ : empty_input;
    if( !input.needs_translation || input.raw.empty() ) { return input.raw; }
    const auto &library = locale.library();
    if( input.context ) {
        return input.plural ? library.get_ctx_pl( input.context->c_str(), input.raw.c_str(),
                input.plural->c_str(), num ) : library.get_ctx( input.context->c_str(), input.raw.c_str() );
    }
    return input.plural ? library.get_pl( input.raw.c_str(), input.plural->c_str(), num ) :
           library.get( input.raw.c_str() );
}
} // namespace localization

// int version/generation that is incremented each time language is changed
// used to invalidate translation cache
static int current_language_version = INVALID_LANGUAGE_VERSION + 1;

int detail::get_current_language_version()
{
    return current_language_version;
}

void invalidate_translations()
{
    // increment version to invalidate translation cache
    do {
        current_language_version++;
    } while( current_language_version == INVALID_LANGUAGE_VERSION );
}

const char *detail::_translate_internal( const char *msg )
{
    return l10n_data::get_library().get( msg );
}

const char *vgettext( const char *msgid, const char *msgid_plural, size_t n )
{
    return l10n_data::get_library().get_pl( msgid, msgid_plural, n );
}

const char *pgettext( const char *context, const char *msgid )
{
    return l10n_data::get_library().get_ctx( context, msgid );
}

const char *vpgettext( const char *const context, const char *const msgid,
                       const char *const msgid_plural, const size_t n )
{
    return l10n_data::get_library().get_ctx_pl( context, msgid, msgid_plural, n );
}

std::string gettext_gendered( const GenderMap &genders, const std::string &msg )
{
    const std::vector<std::string> &language_genders = get_language().genders;

    std::vector<std::string> chosen_genders;
    for( const auto &subject_genders : genders ) {
        // default if no match
        std::string chosen_gender = language_genders[0];
        for( const std::string &gender : subject_genders.second ) {
            if( std::ranges::find( language_genders, gender ) !=
                language_genders.end() ) {
                chosen_gender = gender;
                break;
            }
        }
        chosen_genders.push_back( subject_genders.first + ":" + chosen_gender );
    }
    std::string context = join( chosen_genders, " " );
    return pgettext( context.c_str(), msg.c_str() );
}

auto translation::data() const -> const localization::translation_input & // *NOPAD*
{
    return input_ ? *input_ : localization::empty_input;
}
auto translation::snapshot() const -> localization::translation_snapshot
{
    ++localization::work.input_acquisitions;
    return localization::translation_snapshot{ input_ };
}

translation::translation( const plural_tag )
    : input_( localization::publish_input( { .plural = std::string{} } ) ) {}
translation::translation( const std::string &ctxt, const std::string &raw )
    : input_( localization::publish_input( { .context = localization::copied_string( ctxt ),
                                             .raw = localization::copied_string( raw ), .needs_translation = true } ) ) {}
translation::translation( const std::string &raw )
    : input_( localization::publish_input( { .raw = localization::copied_string( raw ),
                                             .needs_translation = true } ) ) {}
translation::translation( const std::string &raw, const std::string &raw_pl, const plural_tag )
    : input_( localization::publish_input( { .raw = localization::copied_string( raw ),
                                             .plural = localization::copied_string( raw_pl ), .needs_translation = true } ) ) {}
translation::translation( const std::string &ctxt, const std::string &raw,
                          const std::string &raw_pl, const plural_tag )
    : input_( localization::publish_input( { .context = localization::copied_string( ctxt ),
                                             .raw = localization::copied_string( raw ), .plural = localization::copied_string( raw_pl ),
                                             .needs_translation = true } ) ) {}
translation::translation( const std::string &str, const no_translation_tag )
    : input_( localization::publish_input( { .raw = localization::copied_string( str ) } ) ) {}

translation translation::to_translation( const std::string &raw )
{
    return { raw };
}

translation translation::to_translation( const std::string &ctxt, const std::string &raw )
{
    return { ctxt, raw };
}

translation translation::pl_translation( const std::string &raw, const std::string &raw_pl )
{
    return { raw, raw_pl, plural_tag() };
}

translation translation::pl_translation( const std::string &ctxt, const std::string &raw,
        const std::string &raw_pl )
{
    return { ctxt, raw, raw_pl, plural_tag() };
}

translation translation::no_translation( const std::string &str )
{
    return { str, no_translation_tag() };
}

void translation::make_plural()
{
    // Even an idempotent call invalidates the eager cache, as it did before snapshots.
    cached_language_version = INVALID_LANGUAGE_VERSION;
    cached_translation = nullptr;
    if( data().plural ) { return; }
    auto next = localization::copy_input( data() );
    auto &raw_pl = next.plural;
    const auto &raw = next.raw;
    const auto needs_translation = next.needs_translation;
    if( needs_translation ) {
        // if plural form has not been enabled yet
        if( !raw_pl ) {
            // copy the singular string without appending "s" to preserve the original behavior
            raw_pl = localization::copied_string( raw );
        }
    } else if( !raw_pl ) {
        // just mark plural form as enabled
        raw_pl = std::string{};
    }
    input_ = localization::publish_input( std::move( next ) );
}

void translation::add_context( const std::string &ctxt )
{
    auto next = localization::copy_input( data() );
    if( next.context ) {
        // if context already exists, add to it
        auto &ctxt_this = *next.context;
        ctxt_this += "|";
        ctxt_this += ctxt;
        localization::work.input_copied_bytes += ctxt.size();
    } else {
        next.context = localization::copied_string( ctxt );
    }
    input_ = localization::publish_input( std::move( next ) );
    // reset the cache
    cached_language_version = INVALID_LANGUAGE_VERSION;
    cached_translation = nullptr;
}

void translation::deserialize( JsonIn &jsin )
{
    auto next = localization::copy_input( data() );
    auto &ctxt = next.context;
    auto &raw = next.raw;
    auto &raw_pl = next.plural;
    auto &needs_translation = next.needs_translation;
    // reset the cache
    cached_language_version = INVALID_LANGUAGE_VERSION;
    cached_translation = nullptr;

#ifndef CATA_IN_TOOL
    bool check_style = false;
    std::function<void( const std::string &msg, int offset )> log_error;
#endif
    [[maybe_unused]] auto auto_plural = false;
    [[maybe_unused]] auto is_str_sp = false;
    try {
        if( jsin.test_string() ) {
#ifndef CATA_IN_TOOL
            if( test_mode ) {
                const int origin = jsin.tell();
                check_style = true;
                log_error = [&jsin, origin]( const std::string & msg, const int offset ) {
                    const int previous_pos = jsin.tell();
                    try {
                        jsin.seek( origin );
                        jsin.string_error( msg, offset );
                    } catch( const JsonError &e ) {
                        debugmsg( "(json-error)\n%s", e.what() );
                    }
                    // seek to previous pos (end of string) so subsequent json input
                    // can continue.
                    jsin.seek( previous_pos );
                };
            }
#endif
            ctxt.reset();
            raw = jsin.get_string();
            // if plural form is enabled
            if( raw_pl ) {
                raw_pl = localization::automatic_plural( raw );
                auto_plural = true;
            }
            needs_translation = true;
        } else {
            JsonObject jsobj = jsin.get_object();
            if( jsobj.has_string( "ctxt" ) ) {
                ctxt = jsobj.get_string( "ctxt" );
            } else {
                ctxt.reset();
            }
            if( jsobj.has_member( "str_sp" ) ) {
                // same singular and plural forms
                raw = jsobj.get_string( "str_sp" );
                is_str_sp = true;
                // if plural form is enabled
                if( raw_pl ) {
                    raw_pl = localization::copied_string( raw );
                } else {
                    try {
                        jsobj.throw_error( "str_sp not supported here", "str_sp" );
                    } catch( const JsonError &e ) {
                        debugmsg( "(json-error)\n%s", e.what() );
                    }
                }
            } else {
                raw = jsobj.get_string( "str" );
                // if plural form is enabled
                if( raw_pl ) {
                    if( jsobj.has_string( "str_pl" ) ) {
                        raw_pl = jsobj.get_string( "str_pl" );
                    } else {
                        raw_pl = localization::automatic_plural( raw );
                        auto_plural = true;
                    }
                } else if( jsobj.has_string( "str_pl" ) ) {
                    try {
                        jsobj.throw_error( "str_pl not supported here", "str_pl" );
                    } catch( const JsonError &e ) {
                        debugmsg( "(json-error)\n%s", e.what() );
                    }
                }
            }
            needs_translation = true;
#ifndef CATA_IN_TOOL
            if( test_mode ) {
                check_style = !jsobj.has_member( "//NOLINT(cata-text-style)" );
                // Copying jsobj to avoid use-after-free
                log_error = [jsobj]( const std::string & msg, const int offset ) {
                    try {
                        if( jsobj.has_member( "str" ) ) {
                            jsobj.get_raw( "str" )->string_error( msg, offset );
                        } else {
                            jsobj.get_raw( "str_sp" )->string_error( msg, offset );
                        }
                    } catch( const JsonError &e ) {
                        debugmsg( "(json-error)\n%s", e.what() );
                    }
                };
            }
#endif
        }
#ifndef CATA_IN_TOOL
        // Check text style in translatable json strings.
        if( test_mode && check_style ) {
            if( raw_pl && !auto_plural && *raw_pl == raw + "s" ) {
                log_error( "\"str_pl\" is not necessary here since the "
                           "plural form can be automatically generated.",
                           0 );
            }
            if( !is_str_sp && raw_pl && !auto_plural && raw == *raw_pl ) {
                log_error( "Please use \"str_sp\" instead of \"str\" and \"str_pl\" "
                           "for text with identical singular and plural forms",
                           0 );
            }
        }
#endif
    } catch( ... ) {
        input_ = localization::publish_input( std::move( next ) );
        throw;
    }
    input_ = localization::publish_input( std::move( next ) );
}

std::string translation::translated( const int num ) const
{
    const auto &raw = data().raw;
    const auto &ctxt = data().context;
    const auto &raw_pl = data().plural;
    if( !data().needs_translation || raw.empty() ) {
        return raw;
    }
    // Note1: `raw`, `raw_pl` and `ctxt` are effectively immutable for caching purposes:
    // in the places where they are changed, cache is explicitly invalidated
    // Note2: if `raw_pl` is defined, `num` becomes part of the "cache key"
    // otherwise `num` is ignored (for both translation and cache)
    if( cached_language_version != current_language_version ||
        ( raw_pl && cached_num != num ) || !cached_translation ) {
        cached_language_version = current_language_version;
        cached_num = num;
        ++localization::work.eager_cache_refreshes;

        if( !ctxt ) {
            if( !raw_pl ) {
                cached_translation = cata::make_value<std::string>( detail::_translate_internal( raw ) );
            } else {
                cached_translation = cata::make_value<std::string>(
                                         vgettext( raw.c_str(), raw_pl->c_str(), num ) );
            }
        } else {
            if( !raw_pl ) {
                cached_translation = cata::make_value<std::string>( pgettext( ctxt->c_str(), raw.c_str() ) );
            } else {
                cached_translation = cata::make_value<std::string>(
                                         vpgettext( ctxt->c_str(), raw.c_str(), raw_pl->c_str(), num ) );
            }
        }
    }
    return *cached_translation;
}

bool translation::empty() const
{
    return data().raw.empty();
}

bool translation::translated_lt( const translation &that ) const
{
    return localized_compare( translated(), that.translated() );
}

bool translation::translated_eq( const translation &that ) const
{
    return translated() == that.translated();
}

bool translation::translated_ne( const translation &that ) const
{
    return !translated_eq( that );
}

bool translation::operator==( const translation &that ) const
{
    return data().context == that.data().context && data().raw == that.data().raw &&
           data().plural == that.data().plural &&
           data().needs_translation == that.data().needs_translation;
}

bool translation::operator!=( const translation &that ) const
{
    return !operator==( that );
}

std::pair<bool, int> translation::legacy_hash() const
{
    const auto &raw = data().raw;
    const auto &ctxt = data().context;
    const auto &raw_pl = data().plural;
    const auto needs_translation = data().needs_translation;
    if( needs_translation && !ctxt && !raw_pl ) {
        return {true, djb2_hash( reinterpret_cast<const unsigned char *>( raw.c_str() ) )};
    }
    // Otherwise the translation must have been added after snippets were changed
    // to use string ids only, so the translation doesn't have a legacy hash value.
    return {false, 0};
}

translation to_translation( const std::string &raw )
{
    return translation::to_translation( raw );
}

translation to_translation( const std::string &ctxt, const std::string &raw )
{
    return translation::to_translation( ctxt, raw );
}

translation pl_translation( const std::string &raw, const std::string &raw_pl )
{
    return translation::pl_translation( raw, raw_pl );
}

translation pl_translation( const std::string &ctxt, const std::string &raw,
                            const std::string &raw_pl )
{
    return translation::pl_translation( ctxt, raw, raw_pl );
}

translation no_translation( const std::string &str )
{
    return translation::no_translation( str );
}

std::ostream &operator<<( std::ostream &out, const translation &t )
{
    return out << t.translated();
}

std::string operator+( const translation &lhs, const std::string &rhs )
{
    return lhs.translated() + rhs;
}

std::string operator+( const std::string &lhs, const translation &rhs )
{
    return lhs + rhs.translated();
}

std::string operator+( const translation &lhs, const translation &rhs )
{
    return lhs.translated() + rhs.translated();
}
