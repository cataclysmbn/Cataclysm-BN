#pragma once

#include <optional>
#include <string>

#include "translation_snapshot.h"

struct language_info;

namespace localization
{

/// Whether the game can word its text in this BN language id (`lang/languages.json`): it is the language in
/// use, or it is defined and has a compiled catalogue, or is the source language, which needs none.
auto locale_available( const std::string &lang_id ) -> bool;

/// The catalogues of a language, the base game's and those of the active mods. Loaded once per id and
/// kept until the catalogues are reloaded, so that text taken from it stays valid in between.
/// `lang_id` must satisfy `locale_available`.
auto load_locale( const std::string &lang_id ) -> locale_snapshot;

/// Everything the game words while it lives, it words in `lang_id`: the catalogue and the grammatical
/// language are swapped, and every translation cache is invalidated on entry and on exit. A scope
/// for the language already in use, or for none, changes nothing. Game thread only, since the swap is process-wide.
class locale_scope
{
    public:
        explicit locale_scope( const std::string &lang_id );
        ~locale_scope();
        locale_scope( const locale_scope & ) = delete;
        auto operator=( const locale_scope & ) -> locale_scope & = delete; // *NOPAD*

    private:
        struct swapped {
            locale_snapshot library;
            const language_info *language;
        };
        std::optional<swapped> previous_;
};

} // namespace localization
