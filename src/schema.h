#pragma once
#ifndef CATA_SRC_SCHEMA_H
#define CATA_SRC_SCHEMA_H

/**
 * @file schema.h
 * @brief Declarative macros for unified JSON/Lua data loading.
 *
 * This header provides macros to define field schemas once and use them
 * with both JsonObject and LuaTableWrapper through the DataReader concept.
 *
 * Usage example:
 * @code
 * // In .cpp file:
 * #include "schema.h"
 *
 * SCHEMA_FIELDS_BEGIN(my_type)
 *     SCHEMA_FIELD_MANDATORY("id", id)
 *     SCHEMA_FIELD_OPTIONAL("name", name, "default")
 *     SCHEMA_FIELD_OPTIONAL_NOVAL("items", items)
 *     SCHEMA_FIELD_OPTIONAL_READER("flags", flags, auto_flags_reader<flag_id>{})
 * SCHEMA_FIELDS_END()
 *
 * // Explicit instantiations
 * template void my_type::load_fields<JsonObject>(const JsonObject&, bool);
 * template void my_type::load_fields<LuaTableWrapper>(const LuaTableWrapper&, bool);
 * @endcode
 *
 * The struct must declare:
 * @code
 * template<typename Reader>
 * requires DataReader<Reader>
 * void load_fields(const Reader& reader, bool was_loaded);
 * @endcode
 */

#include "data_reader.h"
#include "generic_factory.h"

/**
 * Begin a schema field definition block for a type.
 * Creates a template function that works with any DataReader (JSON or Lua).
 *
 * @param TypeName The type being loaded (e.g., mutation_branch)
 *
 * NOTE: The opening brace '{' must be placed AFTER this macro, and the closing
 * brace '}' must be placed explicitly (do NOT use SCHEMA_FIELDS_END).
 * This is required for proper brace matching in MSVC and better error messages.
 *
 * Example:
 * @code
 * SCHEMA_FIELDS_BEGIN(my_type)
 * {
 *     SCHEMA_FIELD_MANDATORY("id", id)
 *     SCHEMA_FIELD_OPTIONAL("name", name, "default")
 * }
 * @endcode
 */
#define SCHEMA_FIELDS_BEGIN(TypeName) \
    template<typename Reader> \
    requires DataReader<Reader> \
    void TypeName::load_fields(const Reader& reader, bool was_loaded)

/**
 * @deprecated Use explicit closing brace '}' instead for better MSVC compatibility.
 */
#define SCHEMA_FIELDS_END()

/**
 * Define a mandatory field (required, error if missing on fresh load).
 *
 * @param json_key The JSON/Lua key name (string literal)
 * @param member The C++ member variable to load into
 */
#define SCHEMA_FIELD_MANDATORY(json_key, member) \
    mandatory(reader, was_loaded, json_key, member);

/**
 * Define a mandatory field with a custom reader.
 *
 * @param json_key The JSON/Lua key name (string literal)
 * @param member The C++ member variable to load into
 * @param reader_instance A reader object (e.g., trait_reader{})
 */
#define SCHEMA_FIELD_MANDATORY_READER(json_key, member, reader_instance) \
    mandatory(reader, was_loaded, json_key, member, reader_instance);

/**
 * Define an optional field with a default value.
 *
 * @param json_key The JSON/Lua key name (string literal)
 * @param member The C++ member variable to load into
 * @param default_val Default value if not present
 */
#define SCHEMA_FIELD_OPTIONAL(json_key, member, default_val) \
    optional(reader, was_loaded, json_key, member, default_val);

/**
 * Define an optional field with no explicit default (uses type's default constructor).
 *
 * @param json_key The JSON/Lua key name (string literal)
 * @param member The C++ member variable to load into
 */
#define SCHEMA_FIELD_OPTIONAL_NOVAL(json_key, member) \
    optional(reader, was_loaded, json_key, member);

/**
 * Define an optional field with a custom reader.
 *
 * The reader must implement get_from_string(const std::string&) for DataReader
 * (Lua) support. Standard readers like auto_flags_reader, string_reader, and
 * string_id_reader all support this.
 *
 * @param json_key The JSON/Lua key name (string literal)
 * @param member The C++ member variable to load into
 * @param reader_instance A reader object (e.g., auto_flags_reader<flag_id>{})
 */
#define SCHEMA_FIELD_OPTIONAL_READER(json_key, member, reader_instance) \
    optional(reader, was_loaded, json_key, member, reader_instance);

/**
 * Define an optional field with a custom reader and explicit default.
 *
 * @param json_key The JSON/Lua key name (string literal)
 * @param member The C++ member variable to load into
 * @param reader_instance A reader object
 * @param default_val Default value if not present
 */
#define SCHEMA_FIELD_OPTIONAL_READER_DEFAULT(json_key, member, reader_instance, default_val) \
    optional(reader, was_loaded, json_key, member, reader_instance, default_val);

/**
 * Define a custom field loader for complex nested structures.
 * The code block has access to 'reader' and 'was_loaded'.
 *
 * @param json_key The JSON/Lua key to check for
 * @param loader_code Code block to execute if the key exists
 *
 * Example:
 * @code
 * SCHEMA_FIELD_CUSTOM("spawn_item", reader.has_object, {
 *     auto si = reader.get_object("spawn_item");
 *     optional(si, was_loaded, "type", spawn_item);
 *     optional(si, was_loaded, "message", spawn_message);
 * })
 * @endcode
 */
#define SCHEMA_FIELD_CUSTOM(json_key, has_method, loader_code) \
    if (reader.has_method(json_key)) loader_code

/**
 * Define a custom field loader that checks has_member.
 *
 * @param json_key The JSON/Lua key to check for
 * @param loader_code Code block to execute if the key exists
 */
#define SCHEMA_FIELD_CUSTOM_MEMBER(json_key, loader_code) \
    if (reader.has_member(json_key)) loader_code

/**
 * Define a custom field loader that checks has_object.
 *
 * @param json_key The JSON/Lua key to check for
 * @param loader_code Code block to execute if the key exists
 */
#define SCHEMA_FIELD_CUSTOM_OBJECT(json_key, loader_code) \
    if (reader.has_object(json_key)) loader_code

/**
 * Define a custom field loader that checks has_array.
 *
 * @param json_key The JSON/Lua key to check for
 * @param loader_code Code block to execute if the key exists
 */
#define SCHEMA_FIELD_CUSTOM_ARRAY(json_key, loader_code) \
    if (reader.has_array(json_key)) loader_code

#endif // CATA_SRC_SCHEMA_H
