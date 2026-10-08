#pragma once

#include <expected>
#include <string>

struct sqlite3;

enum class sqlite_prefix_operation { exists, erase };

/// Executes a literal file-prefix query without taking ownership of the connection.
/// Returns SQLITE_ROW/SQLITE_DONE or the database error; always finalizes its statement.
auto query_sqlite_file_prefix( sqlite3 *db, const std::string &prefix,
                               sqlite_prefix_operation operation ) -> std::expected<int, std::string>;
