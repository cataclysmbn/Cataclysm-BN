#pragma once

class JsonObject;

auto load_weather_generator_compat( const JsonObject &jo ) -> void;
auto load_map_extra_collection_compat( const JsonObject &jo ) -> void;
auto load_region_settings_map_extras_compat( const JsonObject &jo ) -> void;
auto load_region_terrain_furniture_compat( const JsonObject &jo ) -> void;
auto load_region_settings_terrain_furniture_compat( const JsonObject &jo ) -> void;
auto load_region_settings_forest_mapgen_compat( const JsonObject &jo ) -> void;
