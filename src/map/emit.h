#pragma once

#include "field_type.h"
#include "type_id.h"

#include <map>
#include <string>

class JsonObject;

class emit {
public:
    emit();

    auto id() const -> const emit_id& { return id_; }

    /** When null @ref field is always fd_null */
    auto is_null() const -> bool;

    /** When valid @ref field is never fd_null */
    auto is_valid() const -> bool;

    /** Type of field to emit @see emit::is_valid */
    auto field() const -> field_type_id { return field_; }

    /** Intensity of output fields, range [1..maximum_intensity] */
    auto intensity() const -> int { return intensity_; }

    /** Units of field to generate per turn subject to @ref chance */
    auto qty() const -> int { return qty_; }

    /** Chance to emit each turn, range [1..100] */
    auto chance() const -> int { return chance_; }

    /** Load emission data from JSON definition */
    static void load_emit(const JsonObject& jo);

    /** Get all currently loaded emission data */
    static auto all() -> const std::map<emit_id, emit>&;

    /** Check consistency of all loaded emission data */
    static void finalize();

    /** Check consistency of all loaded emission data */
    static void check_consistency();

    /** Clear all loaded emission data (invalidating any pointers) */
    static void reset();

private:
    emit_id id_;
    field_type_id field_ = fd_null;
    int intensity_ = 1;
    int qty_ = 1;
    int chance_ = 100;

    /** used during JSON loading only */
    std::string field_name;

public:
    LUA_TYPE_OPS(emit, id_);
};
