#pragma once

#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "input.h"
#    include "language.h"
#    include "locale_scope.h"
#    include "path_info.h"
#    include "translations.h"

#    include <algorithm>
#    include <cstdint>
#    include <filesystem>
#    include <fstream>
#    include <ranges>
#    include <string>
#    include <utility>
#    include <vector>

namespace locale_fixture {

constexpr auto korean = "ko_KR";

/// Real MO bytes: the catalogue of a language that has `entries` translated.
inline auto catalogue(std::vector<std::pair<std::string, std::string>> entries) -> std::string {
    entries.emplace_back(
        "", "Content-Type: text/plain; charset=UTF-8\nContent-Transfer-Encoding: 8bit\n"
            "Plural-Forms: nplurals=2; plural=n!=1;\n");
    std::ranges::sort(entries);
    auto result = std::string(28 + entries.size() * 16, '\0');
    const auto put = [&](const std::size_t offset, const std::uint32_t value) {
        for (const auto byte : std::views::iota(std::size_t{0}, std::size_t{4})) {
            result[offset + byte] = static_cast<char>(value >> (byte * 8));
        }
    };
    put(0, 0x950412de);
    put(8, entries.size());
    put(12, 28);
    put(16, 28 + entries.size() * 8);
    for (const auto ordinal : std::views::iota(std::size_t{0}, entries.size())) {
        const auto& [original, translated] = entries[ordinal];
        put(28 + ordinal * 8, original.size());
        put(32 + ordinal * 8, result.size());
        result += original;
        result += '\0';
        put(28 + entries.size() * 8 + ordinal * 8, translated.size());
        put(32 + entries.size() * 8 + ordinal * 8, result.size());
        result += translated;
        result += '\0';
    }
    return result;
}

/// A catalogue entry for a name that has a singular and a plural form.
inline auto plural(const std::string& singular, const std::string& many) -> std::string {
    return singular + '\0' + many;
}

/// The game finds the catalogue of `ko_KR` under its base path. Every word is the English one with
/// `-ko`.
struct korean_catalogue {
    std::string base_path = PATH_INFO::base_path();
    std::filesystem::path directory =
        std::filesystem::path{PATH_INFO::user_dir()} / "locale-scope-fixture";
    korean_catalogue() {
        std::filesystem::remove_all(directory);
        const auto path = directory / "lang/mo" / korean / "LC_MESSAGES/cataclysm-bn.mo";
        std::filesystem::create_directories(path.parent_path());
        auto output = std::ofstream{path, std::ios::binary};
        const auto bytes = catalogue({
            {"Yes", "Yes-ko"},
            {"No", "No-ko"},
            {"Confirm", "Confirm-ko"},
            {"Strength", "Strength-ko"},
            {"Dawn", "Dawn-ko"},
            {"North:", "North:-ko"},
            {"Focus", "Focus-ko"},
            {"Trading with", "Trading with-ko"},
            {"Paying", "Paying-ko"},
            {"Accepting a reward from", "Accepting a reward from-ko"},
            {plural("TEST 1kg cube", "TEST 1kg cubes"),
             plural("TEST 1kg cube-ko", "TEST 1kg cubes-ko")},
        });
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        REQUIRE(output.good());
        output.close();
        PATH_INFO::init_base_path(directory.string());
        l10n_data::reload_catalogues();
        REQUIRE(get_language().id != korean);
    }
    ~korean_catalogue() {
        PATH_INFO::init_base_path(base_path);
        l10n_data::reload_catalogues();
        std::filesystem::remove_all(directory);
    }
    korean_catalogue(const korean_catalogue&) = delete;
    auto operator=(const korean_catalogue&) -> korean_catalogue& = delete;
};

/// What the Korean catalogue says of a source string, without entering a scope.
inline auto in_korean(const std::string& msgid) -> std::string {
    return to_translation(msgid).snapshot().translated(localization::load_locale(korean));
}

/// A native yes/no screen whose words are translated each time it is read.
struct yes_no_boundary {
    input_context context{"YESNO"};
    game_client::input_context_scope input{context, "YESNO"};
    game_client::interaction_scope interaction{
        context, [] {
            return game_client::interaction_snapshot{
                .kind = game_client::interaction_kind::choices,
                .title = _("Confirm"),
                .allow_cancel = true,
                .choices = {{.id = "choice:yes", .label = _("Yes")},
                            {.id = "choice:no", .label = _("No")}},
            };
        }};
    yes_no_boundary() {
        context.register_action("YES");
        game_client::begin_input_boundary();
    }
};

} // namespace locale_fixture

#endif
