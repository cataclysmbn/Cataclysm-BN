#pragma once

#include "compute_backend.h"

namespace cata_compute::slang_cpu
{

auto init() -> void;
auto shutdown() -> void;
auto status() -> backend_status;

#if defined( CATA_SDL )
auto resident_lighting_ready_for_visibility( resident_lighting_ready_params const &p ) -> bool;
auto resident_lighting_ready_for_sight_pairs( resident_sight_pair_inputs_params const &p )
-> bool;
auto prepare_lighting_transparency_output( prepare_lighting_transparency_output_params const &p )
-> resident_transparency_output;
auto mark_lighting_transparency_level_updated( int zlev ) -> void;
auto lighting_transparency_level_is_valid( int zlev ) -> bool;
auto invalidate_lighting_transparency_levels( std::vector<int> const &levels ) -> void;
auto shift_lighting_resident_inputs( shift_lighting_residency_params const &p ) -> bool;
auto dispatch_transparency( dispatch_transparency_params const &p ) -> bool;
auto begin_lighting( lighting_params const &p ) -> lighting_work;
auto finish_lighting( lighting_work const &work ) -> bool;
auto run_lighting( lighting_params const &p ) -> bool;
auto begin_visibility( visibility_params const &p ) -> visibility_work;
auto finish_visibility( visibility_work const &work ) -> bool;
auto run_visibility( visibility_params const &p ) -> bool;
auto begin_sight_pairs( begin_sight_pairs_params const &p ) -> sight_pairs_work;
auto finish_sight_pairs( sight_pairs_work const &work, std::vector<uint32_t> &results )
-> bool;
auto run_sight_pairs( run_sight_pairs_params const &p ) -> bool;
#endif

} // namespace cata_compute::slang_cpu
