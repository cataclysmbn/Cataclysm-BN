#pragma once
#ifndef CATA_TESTS_STRINGMAKER_H
#    define CATA_TESTS_STRINGMAKER_H

#    include "cata_variant.h"
#    include "catch/catch.hpp"
#    include "coordinates.h"
#    include "cuboid_rectangle.h"
#    include "dialogue.h"
#    include "distribution_grid.h"
#    include "item.h"
#    include "units_angle.h"

#    include <optional>
#    include <utility>

// StringMaker specializations for Cata types for reporting via Catch2 macros

namespace Catch {

template <typename T1, typename T2> struct StringMaker<std::pair<T1, T2>> {
    static auto convert(const std::pair<T1, T2>& p) -> std::string {
        return string_format(
            "{ %s, %s }", StringMaker<T1>::convert(p.first), StringMaker<T2>::convert(p.second));
    }
};

template <typename T> struct StringMaker<string_id<T>> {
    static auto convert(const string_id<T>& i) -> std::string {
        return string_format("string_id( \"%s\" )", i.str());
    }
};

template <typename T> struct StringMaker<std::optional<T>> {
    static auto convert(const std::optional<T>& i) -> std::string {
        if (i.has_value()) {
            const T& val = *i;
            return string_format("optional( %s )", StringMaker<T>::convert(val));
        } else {
            return "optional()";
        }
    }
};

template <> struct StringMaker<std::nullopt_t> {
    static auto convert(const std::nullopt_t&) -> std::string { return "optional()"; }
};

template <> struct StringMaker<item> {
    static auto convert(const item& i) -> std::string {
        return string_format("item( itype_id( \"%s\" ) )", i.typeId().str());
    }
};

template <> struct StringMaker<point> {
    static auto convert(const point& p) -> std::string {
        return string_format("point( %d, %d )", p.x, p.y);
    }
};

template <> struct StringMaker<tripoint> {
    static auto convert(const tripoint& p) -> std::string {
        return string_format("tripoint( %d, %d, %d )", p.x, p.y, p.z);
    }
};

template <typename Point> struct StringMaker<rectangle<Point>> {
    static auto convert(const rectangle<Point>& r) -> std::string {
        return string_format("[%s-%s]", r.p_min.to_string(), r.p_max.to_string());
    }
};

template <typename Tripoint> struct StringMaker<cuboid<Tripoint>> {
    static auto convert(const cuboid<Tripoint>& b) -> std::string {
        return string_format("[%s-%s]", b.p_min.to_string(), b.p_max.to_string());
    }
};

template <> struct StringMaker<cata_variant> {
    static auto convert(const cata_variant& v) -> std::string {
        return string_format(
            "cata_variant<%s>(\"%s\")", io::enum_to_string(v.type()), v.get_string());
    }
};

template <> struct StringMaker<time_duration> {
    static auto convert(const time_duration& d) -> std::string {
        return string_format("time_duration( %d ) [%s]", to_turns<int>(d), to_string(d));
    }
};

template <> struct StringMaker<units::angle> {
    static auto convert(const units::angle& a) -> std::string {
        return string_format("angle( %fdeg | %frad )", units::to_degrees(a), units::to_radians(a));
    }
};

template <> struct StringMaker<talk_response> {
    static auto convert(const talk_response& r) -> std::string {
        return string_format("talk_response( text=\"%s\" )", r.text);
    }
};

template <> struct StringMaker<grid_furn_transform_queue> {
    static auto convert(const grid_furn_transform_queue& q) -> std::string {
        return string_format("grid_furn_transform_queue(\n%s)", q.to_string());
    }
};

} // namespace Catch

#endif // CATA_TESTS_STRINGMAKER_H
