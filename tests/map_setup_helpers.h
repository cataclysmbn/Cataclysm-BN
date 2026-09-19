#pragma once
#ifndef CATA_TESTS_MAP_SETUP_HELPERS_H
#    define CATA_TESTS_MAP_SETUP_HELPERS_H

#    include "catch/catch.hpp"
#    include "coordinates.h"

#    include <functional>
#    include <optional>
#    include <string>
#    include <unordered_map>
#    include <vector>

namespace map_helpers {
struct canvas_legend {
private:
    std::unordered_map<char32_t, std::string> data;

public:
    canvas_legend(std::unordered_map<char32_t, std::string>&& data): data(data) {}
    ~canvas_legend() = default;

    static constexpr char32_t key_invalid = U'?';

    auto entry(char32_t c) const -> const std::string&; // *NOPAD*
    auto key_for(const std::string& s) const -> char32_t;
};

struct canvas {
private:
    tripoint size_cache;
    std::vector<std::vector<std::u32string>> data;

    auto calc_size() const -> tripoint;

public:
    canvas() = default;
    canvas(const tripoint& size);
    canvas(std::vector<std::u32string>&& level) {
        data.emplace_back(std::move(level));
        size_cache = calc_size();
        assert_size(size());
    }
    // Moved out to a builder method to prevent compilers (and users) from
    // getting confused by all these curly braces.
    static inline auto make_multilevel(std::vector<std::vector<std::u32string>>&& data) -> canvas {
        canvas c;
        c.data = std::move(data);
        c.size_cache = c.calc_size();
        c.assert_size(c.size());
        return c;
    }
    canvas(const canvas&) = default;
    canvas(canvas&&) = default;
    ~canvas() = default;

    auto operator==(const canvas& rhs) const -> bool { return data == rhs.data; }

    inline auto size() const -> const tripoint& { return size_cache; } // *NOPAD*

    void assert_size(const tripoint& sz) const;
    auto to_string() const -> std::string;

    inline auto in_bounds(const tripoint& p) const -> bool {
        return p.x >= 0 && p.y >= 0 && p.z >= 0 && p.x < size().x && p.y < size().y
            && p.z < size().z;
    }

    inline void set(const tripoint& p, char32_t val) {
        assert(in_bounds(p));
        data[p.z][p.y][p.x] = val;
    }
    inline auto get(const tripoint& p) const -> char32_t {
        assert(in_bounds(p));
        return data[p.z][p.y][p.x];
    }

    auto replace(char32_t what, char32_t with) -> std::vector<tripoint>;
    auto replace_unique(char32_t what, char32_t with) -> tripoint;
    auto replace_opt(char32_t what, char32_t with) -> std::optional<tripoint>;

    auto rotated(int turns) const -> canvas;
};

struct canvas_adapter {
private:
    const canvas_legend* l = nullptr;
    std::function<std::string(const tripoint&)> getter;
    std::function<void(const tripoint&, const std::string&)> setter;

public:
    canvas_adapter() = default;
    canvas_adapter(const canvas_legend& l) { with_legend(l); };
    ~canvas_adapter() = default;

    inline auto with_legend(const canvas_legend& l) -> canvas_adapter& { // *NOPAD*
        this->l = &l;
        return *this;
    }
    inline auto with_getter(std::function<std::string(const tripoint&)> f)
        -> canvas_adapter& { // *NOPAD*
        getter = f;
        return *this;
    }
    inline auto with_setter(std::function<void(const tripoint&, const std::string&)> f)
        -> canvas_adapter& { // *NOPAD*
        setter = f;
        return *this;
    }

    void set_all(const canvas& c);

    auto extract_to_canvas(const tripoint& sz) -> canvas;

    void check_matches_expected(const canvas& expected, bool require);
};
} // namespace map_helpers

namespace Catch {
template <> struct StringMaker<map_helpers::canvas> {
    static auto convert(const map_helpers::canvas& c) -> std::string { return c.to_string(); }
};
} // namespace Catch

#endif // CATA_TESTS_MAP_SETUP_HELPERS_H
