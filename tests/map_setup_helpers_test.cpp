#include "catacharset.h"
#include "catch/catch.hpp"
#include "map_setup_helpers.h"
#include "state_helpers.h"
#include "string_formatter.h"

static void cata_assert(bool expr) {
    assert(expr);
    (void)expr;
}

namespace map_helpers {
const std::string& canvas_legend::entry(char32_t c) const {
    auto it = data.find(c);
    cata_assert(it != data.end());
    return it->second;
}

char32_t canvas_legend::key_for(const std::string& s) const {
    for (const auto& it : data) {
        if (it.second == s) { return it.first; }
    }
    return key_invalid;
}

canvas::canvas(const tripoint& size) {
    size_cache = size;
    data.resize(
        size.z,
        std::vector<std::u32string>(size.y, std::u32string(size.x, canvas_legend::key_invalid)));
}

tripoint canvas::calc_size() const {
    if (data.empty() || data[0].empty()) {
        return tripoint_zero;
    } else {
        return tripoint(static_cast<int>(data[0][0].size()), static_cast<int>(data[0].size()),
                        static_cast<int>(data.size()));
    }
}

void canvas::assert_size(const tripoint& sz) const {
    cata_assert(static_cast<int>(data.size()) == sz.z);
    for (const auto& level : data) {
        cata_assert(static_cast<int>(level.size()) == sz.y);
        for (const auto& line : level) { cata_assert(static_cast<int>(line.size()) == sz.x); }
    }
}

std::string canvas::to_string() const {
    std::string res;
    res += "\n";
    for (size_t i = 0; i < data.size(); i++) {
        res += string_format("lev %d\n", i);
        for (const auto& line : data[i]) {
            res += utf32_to_utf8(line);
            res += "\n";
        }
        res += "\n";
    }
    return res;
}

std::vector<tripoint> canvas::replace(char32_t what, char32_t with) {
    std::vector<tripoint> ret;
    tripoint p;
    for (p.z = 0; p.z < size().z; p.z++) {
        for (p.y = 0; p.y < size().y; p.y++) {
            for (p.x = 0; p.x < size().x; p.x++) {
                if (get(p) == what) {
                    set(p, with);
                    ret.push_back(p);
                }
            }
        }
    }
    return ret;
}

tripoint canvas::replace_unique(char32_t what, char32_t with) {
    std::vector<tripoint> candidates = replace(what, with);
    cata_assert(candidates.size() == 1);
    return candidates.front();
}

std::optional<tripoint> canvas::replace_opt(char32_t what, char32_t with) {
    std::vector<tripoint> candidates = replace(what, with);
    cata_assert(candidates.size() <= 1);
    if (candidates.empty()) {
        return std::nullopt;
    } else {
        return candidates.front();
    }
}

canvas canvas::rotated(int turns) const {
    const tripoint new_size = size_cache.rotate_2d(turns).abs();
    canvas ret(new_size);
    tripoint p;
    for (p.z = 0; p.z < size().z; p.z++) {
        for (p.y = 0; p.y < size().y; p.y++) {
            for (p.x = 0; p.x < size().x; p.x++) {
                tripoint new_p = p.rotate_2d(turns, size_cache.xy());
                ret.set(new_p, get(p));
            }
        }
    }
    return ret;
}

void canvas_adapter::set_all(const canvas& c) {
    cata_assert(l);
    cata_assert(!!setter);
    tripoint p;
    for (p.z = 0; p.z < c.size().z; p.z++) {
        for (p.y = 0; p.y < c.size().y; p.y++) {
            for (p.x = 0; p.x < c.size().x; p.x++) { setter(p, l->entry(c.get(p))); }
        }
    }
}

canvas canvas_adapter::extract_to_canvas(const tripoint& sz) {
    cata_assert(l);
    cata_assert(!!getter);
    canvas ret(sz);
    tripoint p;
    for (p.z = 0; p.z < sz.z; p.z++) {
        for (p.y = 0; p.y < sz.y; p.y++) {
            for (p.x = 0; p.x < sz.x; p.x++) { ret.set(p, l->key_for(getter(p))); }
        }
    }
    return ret;
}

void canvas_adapter::check_matches_expected(const canvas& expected, bool require) {
    struct mismatch_entry {
        tripoint p;
        std::string exp;
        std::string got;
    };

    cata_assert(l);
    cata_assert(!!getter);

    std::vector<mismatch_entry> fails;
    tripoint p;
    for (p.z = 0; p.z < expected.size().z; p.z++) {
        for (p.y = 0; p.y < expected.size().y; p.y++) {
            for (p.x = 0; p.x < expected.size().x; p.x++) {
                const std::string& exp = l->entry(expected.get(p));
                std::string got = getter(p);
                if (exp != got) { fails.push_back({p, exp, std::move(got)}); }
            }
        }
    }

    if (!fails.empty()) {
        std::string fails_string = "\n";
        for (const mismatch_entry& e : fails) {
            fails_string += string_format("%s exp:%s got:%s\n", e.p.to_string(), e.exp, e.got);
        }

        canvas canvas_got = extract_to_canvas(expected.size());

        CAPTURE(expected.to_string());
        CAPTURE(canvas_got.to_string());
        CAPTURE(fails.size());
        CAPTURE(fails_string);

        if (require) {
            FAIL();
        } else {
            FAIL_CHECK();
        }
    } else {
        SUCCEED();
    }
}

} // namespace map_helpers

TEST_CASE("map_test_setup_canvas_rotation", "[utility]") {
    clear_all_state();
    SECTION("even_sides_with_2_levels") {
        map_helpers::canvas canvas = map_helpers::canvas::make_multilevel({{
            {
                U"..",
                U"ab",
                U"cd",
                U"..",
            },
            {
                U"..",
                U"ef",
                U"gh",
                U"..",
            },
        }});

        REQUIRE(canvas.rotated(0) == canvas);
        {
            std::vector<std::u32string> lev1 = {
                U".ca.",
                U".db.",
            };
            std::vector<std::u32string> lev2 = {
                U".ge.",
                U".hf.",
            };
            map_helpers::canvas exp = map_helpers::canvas::make_multilevel({{
                lev1,
                lev2,
            }});
            REQUIRE(canvas.rotated(1) == exp);
        }
        {
            map_helpers::canvas exp = map_helpers::canvas::make_multilevel({{
                {
                    U"..",
                    U"dc",
                    U"ba",
                    U"..",
                },
                {
                    U"..",
                    U"hg",
                    U"fe",
                    U"..",
                },
            }});
            REQUIRE(canvas.rotated(2) == exp);
        }
        {
            std::vector<std::u32string> lev1 = {
                U".bd.",
                U".ac.",
            };
            std::vector<std::u32string> lev2 = {
                U".fh.",
                U".eg.",
            };
            map_helpers::canvas exp = map_helpers::canvas::make_multilevel({{lev1, lev2}});
            REQUIRE(canvas.rotated(3) == exp);
        }
        REQUIRE(canvas.rotated(4) == canvas);
    }
    SECTION("not_even_sides") {
        map_helpers::canvas canvas = {{
            U"...",
            U"abc",
            U"...",
            U"def",
            U"...",
        }};

        REQUIRE(canvas.rotated(0) == canvas);
        {
            map_helpers::canvas exp = {{
                U".d.a.",
                U".e.b.",
                U".f.c.",
            }};
            REQUIRE(canvas.rotated(1) == exp);
        }
        {
            map_helpers::canvas exp = {{U"...", U"fed", U"...", U"cba", U"..."}};
            REQUIRE(canvas.rotated(2) == exp);
        }
        {
            map_helpers::canvas exp = {{
                U".c.f.",
                U".b.e.",
                U".a.d.",
            }};
            REQUIRE(canvas.rotated(3) == exp);
        }
        REQUIRE(canvas.rotated(4) == canvas);
    }
}
