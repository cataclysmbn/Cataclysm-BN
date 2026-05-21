#include "preload_config.h"

#include "fstream_utils.h"
#include "json.h"
#include "path_info.h"

#include <string>
#include <string_view>

namespace preload_config
{

namespace
{

struct state_t {
    compute_accel accel{ compute_accel::auto_select };
    std::string   gpu_backend;
};

state_t s_state;

} // namespace

auto load() -> void
{
    read_from_file_json( PATH_INFO::preload(), [&]( JsonIn &jsin ) {
        auto jobj = jsin.get_object();
        s_state.accel = compute_accel_from_string(
                            jobj.get_string( "compute_acceleration", "auto" ) );
    }, true );
}

auto save() -> void
{
    write_to_file( PATH_INFO::preload(), [&]( std::ostream &fout ) {
        JsonOut jout( fout, true );
        jout.start_object();
        jout.member( "compute_acceleration",
                     std::string{ compute_accel_to_string( s_state.accel ) } );
        jout.end_object();
    }, "preload config" );
}

auto get_compute_accel() -> compute_accel                         { return s_state.accel; }
auto set_compute_accel( compute_accel val ) -> void               { s_state.accel = val; }

auto get_gpu_backend_override() -> std::string_view               { return s_state.gpu_backend; }
auto set_gpu_backend_override( std::string_view s ) -> void       { s_state.gpu_backend = s; }

auto compute_accel_from_string( std::string_view s ) -> compute_accel
{
    if( s == "off"   ) { return compute_accel::off;   }
    if( s == "force" ) { return compute_accel::force; }
    return compute_accel::auto_select;
}

auto compute_accel_to_string( compute_accel val ) -> std::string_view
{
    switch( val ) {
        case compute_accel::off:   return "off";
        case compute_accel::force: return "force";
        default:                   return "auto";
    }
}

} // namespace preload_config
