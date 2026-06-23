#pragma once

#include "catalua_bindings_utils.h"
#include "catalua_coord.h"
#include "catalua_luna_doc.h"
#include "coordinates.h"
#include "point.h"
#include "sol/forward.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>

namespace cata::detail::lua_coords
{

using lua_coord_result = std::variant<sol::nil_t, lua_point_coord, lua_tripoint_coord>;

auto lua_project_point_to( const lua_point_coord &coord,
                           const std::string &result_scale ) -> std::optional<lua_point_coord>;
auto lua_project_tripoint_to( const lua_tripoint_coord &coord,
                              const std::string &result_scale ) -> std::optional<lua_tripoint_coord>;
auto lua_project_point_remain_to( const lua_point_coord &coord,
                                  const std::string &result_scale ) ->
std::tuple<std::optional<lua_point_coord>, std::optional<lua_point_coord>>;
auto lua_project_tripoint_remain_to( const lua_tripoint_coord &coord,
                                     const std::string &result_scale ) ->
std::tuple<std::optional<lua_tripoint_coord>, std::optional<lua_point_coord>>;
auto lua_project_remain_to( const sol::object &val,
                            const std::string &result_scale ) -> std::tuple<lua_coord_result, std::optional<lua_point_coord>>;
auto lua_project_combine( const sol::object &coarse, const sol::object &fine ) -> lua_coord_result;
auto lua_project_combine( const lua_point_coord &coarse,
                          const sol::object &fine ) -> lua_coord_result;
auto lua_project_combine( const lua_tripoint_coord &coarse,
                          const sol::object &fine ) -> lua_coord_result;
auto lua_point_coord_rl_dist( const lua_point_coord &lhs,
                              const lua_point_coord &rhs ) -> std::optional<int>;
auto lua_point_coord_trig_dist( const lua_point_coord &lhs,
                                const lua_point_coord &rhs ) -> std::optional<double>;
auto lua_point_coord_square_dist( const lua_point_coord &lhs,
                                  const lua_point_coord &rhs ) -> std::optional<int>;
auto lua_tripoint_coord_rl_dist( const lua_tripoint_coord &lhs,
                                 const lua_tripoint_coord &rhs ) -> std::optional<int>;
auto lua_tripoint_coord_trig_dist( const lua_tripoint_coord &lhs,
                                   const lua_tripoint_coord &rhs ) -> std::optional<double>;
auto lua_tripoint_coord_square_dist( const lua_tripoint_coord &lhs,
                                     const lua_tripoint_coord &rhs ) -> std::optional<int>;

struct project_combine_check {
    coords::origin coarse_origin;
    coords::scale coarse_scale;
    coords::origin fine_origin;
    coords::scale fine_scale;
    bool coarse_is_tripoint;
    bool fine_is_tripoint;
};

auto origin_lua_name( coords::origin origin ) -> std::string_view;
auto scale_lua_name( coords::scale scale ) -> std::string_view;
auto origin_type_name( coords::origin origin ) -> std::string_view;
auto scale_type_name( coords::scale scale ) -> std::string_view;
auto parse_origin( std::string_view name ) -> std::optional<coords::origin>;
auto parse_scale( std::string_view name ) -> std::optional<coords::scale>;
auto is_registered_coord( coords::origin origin, coords::scale scale ) -> bool;
auto has_remainder_origin( coords::scale scale ) -> bool;
auto exact_scale_conversion( coords::scale source, coords::scale result ) -> bool;
auto can_project_to( coords::origin origin, coords::scale source, coords::scale result ) -> bool;
auto can_project_remain( coords::origin origin, coords::scale source,
                         coords::scale result ) -> bool;
auto can_project_combine( const project_combine_check &check ) -> bool;
auto coord_type_name( bool is_tripoint, coords::origin origin, coords::scale scale ) -> std::string;
auto point_to_string( const lua_point_coord &coord ) -> std::string;
auto tripoint_to_string( const lua_tripoint_coord &coord ) -> std::string;
auto make_point_coord( coords::origin origin, coords::scale scale,
                       const point &raw ) -> lua_point_coord;
auto make_tripoint_coord( coords::origin origin, coords::scale scale,
                          const tripoint &raw ) -> lua_tripoint_coord;
auto make_point_coord( const std::string &origin_name, const std::string &scale_name,
                       const point &raw ) -> lua_point_coord;
auto make_tripoint_coord( const std::string &origin_name, const std::string &scale_name,
                          const tripoint &raw ) -> lua_tripoint_coord;
auto project_xy( const point &raw, coords::scale source, coords::scale result ) -> point;
auto project_tripoint_raw( const tripoint &raw, coords::scale source,
                           coords::scale result ) -> tripoint;
auto project_to( const lua_point_coord &coord,
                 coords::scale result ) -> std::optional<lua_point_coord>;
auto project_to( const lua_tripoint_coord &coord,
                 coords::scale result ) -> std::optional<lua_tripoint_coord>;
auto same_coord_kind( const lua_point_coord &lhs, const lua_point_coord &rhs ) -> bool;
auto same_coord_kind( const lua_tripoint_coord &lhs, const lua_tripoint_coord &rhs ) -> bool;

} // namespace cata::detail::lua_coords
