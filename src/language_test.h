#pragma once

#if defined(CATA_LANGUAGE_TESTING)
#include "translation_snapshot.h"

namespace l10n_data::testing
{

/// Game-thread test scope: publish real MO buffers without mutating any pinned library.
/// Restores the exact owned publication and mod-loaded flag, invalidating eager caches on exit.
/// Does not change the language option, selected language, paths, or active mod order.
class scoped_catalogues
{
    public:
        explicit scoped_catalogues( std::vector<std::string> catalogues );
        ~scoped_catalogues() noexcept;
        scoped_catalogues( const scoped_catalogues & ) = delete;
        auto operator=( const scoped_catalogues & ) -> scoped_catalogues & = delete; // *NOPAD*
        /// Parse/hash completely before publication; failure leaves the active state unchanged.
        auto replace( std::vector<std::string> catalogues ) -> void;
    private:
        localization::locale_snapshot previous_;
        bool previous_mod_loaded_;
};

} // namespace l10n_data::testing
#endif
