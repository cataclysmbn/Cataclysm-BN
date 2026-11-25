#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "catch/catch.hpp"
#include "json.h"
#include "json_export.h"
#include "map/map.h"
#include "map_helpers.h"
#include "output.h"
#include "state_helpers.h"
#include "tileray.h"
#include "units_utility.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_display.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_variant.h"

#include <sstream>

/// Validate the data parser and CDDA's eight-way symbols without any tileset-specific settings.
TEST_CASE("vehicle_variant_symbols", "[vehicle_variant]") {
    auto stream = std::istringstream(
        R"({"id":"directions","label":"Directions","symbols":"01234567","symbols_broken":"#"})");
    JsonIn input(stream);
    auto variant = vpart_variant{};
    variant.load(input.get_object());
    for (auto i = 0; i < 8; ++i) {
        CHECK(variant.get_symbol(i * 45_degrees, false) == static_cast<char32_t>('0' + i));
        CHECK(variant.get_symbol(i * 45_degrees, true) == U'#');
    }
    CHECK(variant.get_symbol(-45_degrees, false) == U'7');
    CHECK(variant.get_symbol(360_degrees, false) == U'0');
    CHECK(variant.get_symbol(30_degrees, false) == U'0');
    CHECK(variant.get_symbol(60_degrees, false) == U'2');
    for (const auto* symbols : {"", "ab", "界"}) {
        auto bad = std::istringstream(
            std::string(R"({"symbols":")") + symbols + R"(","symbols_broken":"#"})");
        JsonIn invalid(bad);
        REQUIRE_THROWS(variant.load(invalid.get_object()));
    }
}

/// A real prototype must retain appearance through cloning, vector moves, saves and export.
TEST_CASE("vehicle_variant_roundtrip", "[vehicle_variant]") {
    clear_all_state();
    clear_map();
    const auto id = vpart_id("test_variant_frame");
    REQUIRE(id.is_valid());
    REQUIRE(id->variants.size() == 6);
    CHECK(id->variant_default == "vertical");
    CHECK(id->variants.at("black_cross").get_label() == "Black Cross");
    CHECK(vpart_id("test_variant_inherited")->variants.contains("black_cross"));
    auto* veh = get_map().add_vehicle(
        vproto_id("test_variant_vehicle"), tripoint_bub_ms(60, 60, 0), 0_degrees, 100, 0);
    REQUIRE(veh);
    REQUIRE(veh->part_count() == 4);
    CHECK(veh->cpart(0).variant == "cross");
    CHECK(veh->cpart(1).variant == "black_vertical");
    CHECK(veh->cpart(2).variant == "vertical");
    CHECK(veh->cpart(3).variant.empty());
    CHECK(veh->cpart(0).info().get_id() == id);
    CHECK(veh->get_display_of_tile(veh->cpart(0).mount).get_tileset_id()
          == "vp_test_variant_frame#cross");
    for (auto i = 4; i < 24; ++i) {
        REQUIRE(
            veh->install_part(tripoint_mnt_veh(i, 0, 0), vpart_id("frame_vertical"), true) >= 0);
    }
    CHECK(veh->cpart(1).variant == "black_vertical");
    REQUIRE(veh->part(0).set_variant("literal"));
    CHECK(veh->part_sym(0, true, 0_degrees) == 'h');
    veh->set_hp(veh->part(0), 0);
    CHECK(veh->part_sym(0, true, 90_degrees) == 'u');
    CHECK(veh->get_display_of_tile(veh->cpart(0).mount).is_broken);
    REQUIRE_FALSE(veh->part(0).set_variant("unknown"));
    CHECK(veh->cpart(0).variant == "vertical");
    std::ostringstream saved;
    {
        JsonOut json(saved);
        veh->serialize(json);
    }
    auto loaded = vehicle();
    std::istringstream encoded(saved.str());
    JsonIn parser(encoded);
    loaded.deserialize(parser);
    CHECK(loaded.cpart(1).variant == "black_vertical");
    CHECK(loaded.cpart(1).info().get_id() == id);
    auto part_json = std::ostringstream{};
    {
        JsonOut json(part_json);
        veh->cpart(1).serialize(json);
    }
    auto stale = part_json.str();
    const auto at = stale.find("black_vertical");
    REQUIRE(at != std::string::npos);
    stale.replace(at, std::string("black_vertical").size(), "deleted_variant");
    auto recovered = vehicle_part{};
    std::istringstream bad_variant(stale);
    JsonIn bad_input(bad_variant);
    recovered.deserialize(bad_input);
    CHECK(recovered.variant == "vertical");
    CHECK(recovered.info().get_id() == id);
    auto old_json = std::ostringstream{};
    {
        JsonOut json(old_json);
        veh->cpart(3).serialize(json);
    }
    std::istringstream old_save(old_json.str());
    JsonIn old_input(old_save);
    recovered.deserialize(old_input);
    CHECK(recovered.info().get_id() == vpart_id("frame_vertical"));
    CHECK(recovered.variant.empty());
    std::ostringstream exported;
    {
        JsonOut json(exported);
        json_export::vehicle(json, *veh);
    }
    CHECK(exported.str().find("test_variant_frame#black_vertical") != std::string::npos);
    CHECK(exported.str().find("frame_vertical#") == std::string::npos);
    auto* blueprint = get_map().add_vehicle(
        vproto_id("test_variant_blueprint"), tripoint_bub_ms(65, 65, 0), 0_degrees, 100, 0);
    REQUIRE(blueprint);
    REQUIRE(blueprint->part_count() == 2);
    CHECK(blueprint->cpart(0).variant == "cross");
    CHECK(blueprint->cpart(1).variant == "black_vertical");
}

/// An omitted variant uses the explicit default; an empty variant remains a distinct saved choice.
TEST_CASE("vehicle_variant_default_and_empty", "[vehicle_variant]") {
    clear_all_state();
    clear_map();
    auto* veh = get_map().add_vehicle(
        vproto_id("test_variant_default_choices"), tripoint_bub_ms(60, 60, 0), 0_degrees, 100, 0);
    REQUIRE(veh);
    REQUIRE(veh->part_count() == 3);
    CHECK(veh->cpart(0).variant == "cross");
    CHECK(veh->cpart(1).variant.empty());
    CHECK(veh->cpart(2).variant == "cross");
    auto exported = std::ostringstream{};
    {
        auto json = JsonOut(exported);
        json_export::vehicle(json, *veh);
    }
    auto export_input = std::istringstream(exported.str());
    auto export_parser = JsonIn(export_input);
    const auto prototype = export_parser.get_object();
    prototype.allow_omitted_members();
    auto references = std::vector<std::string>{};
    for (const JsonObject row : prototype.get_array("parts")) {
        row.allow_omitted_members();
        references.push_back(row.get_string("part"));
    }
    REQUIRE(references.size() == 3);
    CHECK(references[0] == "test_variant_explicit_default");
    CHECK(references[1] == "test_variant_explicit_default#");
    CHECK(references[2] == "test_variant_explicit_default");
    for (auto i = 0; i < 2; ++i) {
        auto saved = std::ostringstream{};
        {
            auto json = JsonOut(saved);
            veh->cpart(i).serialize(json);
        }
        auto recovered = vehicle_part{};
        auto input = std::istringstream(saved.str());
        auto parser = JsonIn(input);
        recovered.deserialize(parser);
        CHECK(recovered.variant == veh->cpart(i).variant);
        if (i == 0) {
            auto legacy = saved.str();
            const auto at = legacy.find("\"variant\"");
            REQUIRE(at != std::string::npos);
            legacy.erase(at, legacy.find(',', at) - at + 1);
            auto old_input = std::istringstream(legacy);
            auto old_parser = JsonIn(old_input);
            recovered.deserialize(old_parser);
            CHECK(recovered.variant == "cross");
        }
    }
}

/// Old symbols/shapes must keep their prior glyphs at all 24 supported vehicle headings.
TEST_CASE("vehicle_variant_legacy_symbols", "[vehicle_variant]") {
    clear_all_state();
    clear_map();
    auto* veh = get_map().add_vehicle(
        vproto_id("test_variant_legacy_grid"), tripoint_bub_ms(60, 60, 0), 0_degrees, 100, 0);
    REQUIRE(veh);
    REQUIRE(veh->part_count() > 0);
    for (auto i = 0; i < veh->part_count(); ++i) {
        const auto& p = veh->cpart(i);
        REQUIRE(p.variant.empty());
        for (const auto roof : {false, true}) {
            auto owner = roof ? veh->roof_at_part(i) : -1;
            if (owner < 0 || veh->cpart(owner).removed) { owner = veh->part_displayed_at(p.mount); }
            REQUIRE(owner >= 0);
            const auto& old = veh->cpart(owner);
            const auto display = veh->get_display_of_tile(p.mount, roof);
            CHECK(display.id
                  == (old.proxy_part_id.is_null() ? old.info().get_id() : old.proxy_part_id));
            CHECK(display.is_open == (old.open && old.info().has_flag(VPFLAG_OPENABLE)));
            CHECK(display.is_broken == old.is_broken());
            CHECK(display.direction == normalize(veh->face.dir() + old.direction));
        }
        for (auto degrees = 0; degrees < 360; degrees += 15) {
            const auto original =
                p.info().has_flag(VPFLAG_OPENABLE) && p.open ? '\''
                : p.is_broken()                              ? p.info().sym_broken
                : p.proxy_sym
                    ? p.proxy_sym
                    : p.info().sym;
            CHECK(veh->part_sym(i, true, degrees * 1_degrees)
                  == special_symbol(tileray(degrees * 1_degrees).dir_symbol(original)));
        }
    }
}

#if defined(TILES)
#    include "cata_tiles.h"
#    include "sdltiles.h"
#    include "tile_helpers.h"

namespace {
class variant_memory_probe: public cata_tiles {
public:
    using cata_tiles::get_vpart_memory_at;
};
} // namespace

/// Atlas lookup preserves variants through inheritance, while preferring each part's own base.
TEST_CASE("vehicle_variant_sprite_fallback", "[vehicle_variant]") {
    const auto fixture = tile_context_fixture(true);
    REQUIRE(fixture.valid());
    auto* pack = const_cast<tileset*>(tilecontext->current_tileset());
    for (const auto* image :
         {"vp_test_variant_frame_cross", "vp_test_variant_frame_black", "vp_test_variant_frame",
          "vp_frame_vertical_cross", "vp_frame_vertical_vertical_cross", "vp_frame_vertical"}) {
        pack->create_tile_type(image, tile_type{});
    }
    const auto expect = [&](const std::string& id, const std::string& expected) {
        auto found = tilecontext->find_tile_looks_like(id, C_VEHICLE_PART);
        REQUIRE(found);
        CHECK(found->id() == expected);
    };
    expect("vp_test_variant_frame#cross_left_unknown", "vp_test_variant_frame_cross");
    expect("vp_test_variant_frame#black_vertical_left", "vp_test_variant_frame_black");
    expect("vp_test_variant_frame#missing", "vp_test_variant_frame");
    expect("vp_test_variant_inherited#cross", "vp_test_variant_frame_cross");
    expect("vp_test_variant_inherited#black_vertical_left", "vp_test_variant_frame_black");
    // The parent's base wins before a more distant ancestor's matching variant.
    expect("vp_test_variant_inherited#vertical_cross", "vp_test_variant_frame");
    CHECK_FALSE(
        tilecontext->find_tile_looks_like("vp_test_variant_inherited#cross", C_VEHICLE_PART, 1));
    CHECK(tilecontext->find_tile_looks_like("vp_test_variant_inherited#cross", C_VEHICLE_PART, 2));
    expect("vp_test_variant_frame#", "vp_test_variant_frame");
    CHECK_FALSE(
        tilecontext->find_tile_looks_like("vp_test_variant_nonexistent#cross", C_VEHICLE_PART));
    CHECK_FALSE(tilecontext->find_tile_looks_like("vp_test_variant_cycle#cross", C_VEHICLE_PART));
    pack->create_tile_type("vp_door_left", tile_type{});
    pack->create_tile_type("vp_wheel_bicycle_rear", tile_type{});
    pack->create_tile_type("vp_halfboard_ne", tile_type{});
    expect("vp_hddoor#left", "vp_door_left");
    expect("vp_wheel_bicycle_or#rear", "vp_wheel_bicycle_rear");
    expect("vp_xlhalfboard#ne", "vp_halfboard_ne");
    pack->create_tile_type("f_washer", tile_type{});
    pack->create_tile_type("t_wall_glass", tile_type{});
    expect("vp_washing_machine#missing", "f_washer");
    expect("vp_windshield#missing", "t_wall_glass");
    pack->create_tile_type("f_washer_missing", tile_type{});
    expect("vp_washing_machine#missing", "f_washer_missing");
    pack->create_tile_type("f_crate_o_var_cosmetic", tile_type{});
    expect("vp_box#cosmetic", "f_crate_o_var_cosmetic");
    pack->create_tile_type("f_crate_o_prefixed", tile_type{});
    expect("vp_box#_prefixed", "f_crate_o_prefixed");
    CHECK_FALSE(tilecontext->find_tile_looks_like("vp_box#cosmetic", C_VEHICLE_PART, 1));
    for (const auto state : {0, int(broken)}) {
        const auto glyph = tilecontext->tile_type_search(
            {"vp_test_variant_heavy#horizontal", C_VEHICLE_PART, "", state, 0});
        REQUIRE(glyph);
        INFO("heavy glyph state=" << state << " found=" << glyph->found_id);
        REQUIRE(glyph->found_id.starts_with("ASCII_"));
        CHECK(glyph->found_id[6] == (state == 0 ? '=' : 'H'));
    }
    // Both an empty variant ID and an ASCII symbol resembling a legacy line code must remain
    // literal.
    for (const auto* id : {"vp_test_variant_empty", "vp_test_variant_ascii#literal"}) {
        for (auto angle = 0; angle < 360; angle += 15) {
            for (const auto state : {0, int(open_), int(broken)}) {
                const auto glyph = tilecontext->tile_type_search(
                    {id, C_VEHICLE_PART, "", state, angle});
                REQUIRE(glyph);
                REQUIRE(glyph->found_id.size() >= 7);
                CHECK(glyph->found_id.starts_with("ASCII_"));
                CHECK(
                    glyph->found_id[6]
                    == (state == int(open_) ? '\''
                        : state == int(broken)
                            ? 'u'
                            : 'h'));
            }
        }
    }
    auto& you = get_avatar();
    const auto enabled = you.should_show_map_memory();
    if (!enabled) { you.toggle_map_memory(); }
    const auto restore = on_out_of_scope([&]() {
        if (!enabled) { you.toggle_map_memory(); }
    });
    const auto position = tripoint_bub_ms(61, 61, 0);
    const auto absolute = bub_to_abs(position);
    you.prepare_map_memory_region(absolute - point(12, 12), absolute + point(12, 12));
    you.memorize_tile(absolute, "vp_test_variant_frame#cross_left", open_, 270);
    REQUIRE(you.get_memorized_tile(absolute).tile == "vp_test_variant_frame#cross_left");
    const auto memory = variant_memory_probe::get_vpart_memory_at(position);
    REQUIRE(memory);
    CHECK(memory->tile == "vp_test_variant_frame#cross_left");
    CHECK(memory->subtile == open_);
    CHECK(memory->rotation == 270);
    expect(memory->tile, "vp_test_variant_frame_cross");
}
#endif

/// Imported appearances keep BN part properties and legacy shapes, and load on four vehicles.
TEST_CASE("vehicle_variant_imported_data", "[vehicle_variant]") {
    for (const auto* name : {"board", "halfboard"}) {
        const auto& part = vpart_id(name).obj();
        const auto& old = vpart_id(std::string(name) + "_horizontal").obj();
        REQUIRE(part.variants.size() == 28);
        CHECK(part.item == old.item);
        CHECK(part.durability == old.durability);
        CHECK(part.location == old.location);
        CHECK(part.get_flags() == old.get_flags());
    }
    for (const auto* name : {"door", "windshield"}) {
        CHECK(vpart_id(name)->variant_default.empty());
    }
    for (const auto* name :
         {"door_front_left", "door_rear_right", "windshield_nw", "windshield_horizontal_front"}) {
        REQUIRE(vpart_id(name).is_valid());
        CHECK(vpart_id(name)->variants.empty());
        CHECK(vpart_id(name)->variant_default.empty());
    }
    for (const auto* id : {"car", "beetle", "lux_rv", "schoolbus"}) {
        clear_all_state();
        clear_map();
        auto* veh =
            get_map().add_vehicle(vproto_id(id), tripoint_bub_ms(60, 60, 0), 0_degrees, 100, 0);
        REQUIRE(veh);
        auto imported = 0;
        for (auto i = 0; i < veh->part_count(); ++i) {
            const auto& p = veh->cpart(i);
            if (p.variant.empty()) { continue; }
            REQUIRE(p.info().variants.contains(p.variant));
            ++imported;
        }
        CHECK(imported > 0);
    }
}
