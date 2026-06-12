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
    std::string gpu_backend;
    bool gpu_backend_override_set = false;
    tristate texture_streaming { tristate::auto_select };
};

state_t s_state;

auto tristate_from_legacy_int( const int value ) -> tristate
{
    switch( static_cast<tristate>( value ) ) {
        case tristate::enable:
            return tristate::enable;
        case tristate::disable:
            return tristate::disable;
        default:
        case tristate::auto_select:
            return tristate::auto_select;
    }
}

} // namespace

auto load() -> void
{
    read_from_file_json(
        PATH_INFO::preload(),
    [&]( JsonIn & jsin ) {
        const auto jObj = jsin.get_object();

        s_state.accel = compute_accel_from_string(
                            jObj.get_string( "compute_acceleration", "auto" ) //
                        );

        if( !s_state.gpu_backend_override_set ) {
            s_state.gpu_backend = jObj.get_string( "gpu_backend", "" );
        }

        if( jObj.has_string( "texture_streaming" ) ) {
            s_state.texture_streaming = tristate_from_string(
                                            jObj.get_string( "texture_streaming" ) );
        } else {
            s_state.texture_streaming = tristate_from_legacy_int(
                                            jObj.get_int( "texture_streaming",
                                                    static_cast<int>( tristate::auto_select ) ) );
        }
    },
    true );
}

auto save() -> void
{
    write_to_file( PATH_INFO::preload(), [&]( std::ostream & fout ) {
        JsonOut jout( fout, true );
        jout.start_object();
        jout.member( "compute_acceleration",
                     std::string{ compute_accel_to_string( s_state.accel ) } );
        if( !s_state.gpu_backend.empty() ) {
            jout.member( "gpu_backend", s_state.gpu_backend );
        }
        jout.member( "texture_streaming",
                     std::string{ tristate_to_string( s_state.texture_streaming ) } );
        jout.end_object();
    }, "preload config" );
}

auto get_compute_accel() -> compute_accel                         { return s_state.accel; }
auto set_compute_accel( compute_accel val ) -> void               { s_state.accel = val; }

auto get_gpu_backend_override() -> std::string_view               { return s_state.gpu_backend; }
auto set_gpu_backend_override( std::string_view s ) -> void
{
    s_state.gpu_backend = s;
    s_state.gpu_backend_override_set = true;
}

auto get_texture_streaming() -> tristate                         { return s_state.texture_streaming; }
auto set_texture_streaming( tristate val ) -> void               { s_state.texture_streaming = val; }

auto compute_accel_from_string( std::string_view s ) -> compute_accel
{
    if( s == "off" || s == "cpu" || s == "cpu_compute" ) { return compute_accel::cpu; }
    if( s == "force" || s == "gpu" || s == "hardware" ) { return compute_accel::gpu; }
    if( s == "software" || s == "gpu_software" ) { return compute_accel::gpu_software; }
    return compute_accel::auto_select;
}

auto compute_accel_to_string( compute_accel val ) -> std::string_view
{
    switch( val ) {
        case compute_accel::cpu:
            return "cpu";
        case compute_accel::gpu:
            return "gpu";
        case compute_accel::gpu_software:
            return "gpu_software";
        default:
            return "auto";
    }
}

auto tristate_from_string( std::string_view s ) -> tristate
{
    if( s == "off" || s == "disable" ) { return tristate::disable; }
    if( s == "on" || s == "enable" ) { return tristate::enable; }
    return tristate::auto_select;
}

auto  tristate_to_string( tristate val ) -> std::string_view
{
    switch( val ) {
        case tristate::enable:
            return "enable";
        case tristate::disable:
            return "disable";
        default:
        case tristate::auto_select:
            return "auto";
    }
}

} // namespace preload_config
