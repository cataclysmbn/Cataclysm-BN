#include "vpart_variant.h"

#include "catacharset.h"
#include "json.h"
#include "string_formatter.h"
#include "translations.h"
#include "units_utility.h"

#include <string_view>

auto vpart_variant::load(const JsonObject& data) -> void {
    id = data.get_string("id", "");
    label = data.get_string("label", "");
    if (id.find('#') != std::string::npos) { data.throw_error("reserved '#' in variant id", "id"); }
    const auto read = [&](const char* member, auto& destination) {
        const auto glyphs = utf8_display_split(data.get_string(member));
        if (glyphs.size() != 1 && glyphs.size() != destination.size()) {
            data.throw_error("expected one or eight single-cell symbols", member);
        }
        for (auto i = std::size_t{}; i < destination.size(); ++i) {
            const auto& glyph = glyphs[glyphs.size() == 1 ? 0 : i];
            const auto symbol = UTF8_getch(glyph);
            if (utf8_width(glyph) != 1 || utf32_to_utf8(symbol) != glyph) {
                data.throw_error("symbol must occupy one console cell", member);
            }
            destination[i] = symbol;
        }
    };
    read("symbols", symbols);
    read("symbols_broken", symbols_broken);
}

auto vpart_variant::get_label() const -> std::string {
    const auto shape = to_translation("vpart_variants", label).translated();
    if (base_label.empty()) { return shape; }
    const auto base = to_translation("vpart_variants", base_label).translated();
    return label.empty()
             ? base
             : string_format(pgettext("vpart_variants", "%1$s %2$s"), base, shape);
}

auto vpart_variant::get_symbol(const units::angle direction, const bool broken) const -> char32_t {
    /// CDDA gives cardinal directions five 15-degree notches and diagonals one.
    constexpr auto octants = std::string_view("001222223444445666667000");
    const auto notch = static_cast<int>(normalize(direction - 15_degrees / 2.0) / 15_degrees);
    const auto index = octants[notch] - '0';
    return (broken ? symbols_broken : symbols)[index];
}
