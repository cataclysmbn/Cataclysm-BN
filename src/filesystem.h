#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/**
 * Create directory if it does not exist.
 * @return true if directory exists or was successfully created.
 */
auto assure_dir_exist( const fs::path &path ) -> bool;
/**
 * Check if directory exists.
 * @return false if directory does not exist or if unable to check.
 */
auto dir_exist( const fs::path &path ) -> bool;
/**
 * Check if file exists.
 * @return false if file does not exist or if unable to check.
 */
auto file_exist( const fs::path &path ) -> bool;
/**
 * Remove a file. Does not remove directories.
 * @return true on success.
 */
auto remove_file( const fs::path &path ) -> bool;
/**
 * Remove an empty directory.
 * @return true on success, false on failure (e.g. directory is not empty).
 */
auto remove_directory( const fs::path &path ) -> bool;
/**
 * Remove a directory and all its children.
 * @return true on success or if the directory did not exist,
 *         false on failure to remove (e.g. no permissions, directory is being used).
 */
auto remove_tree( const fs::path &path ) -> bool;
/**
 * Rename a file, overwriting the target. Does not overwrite directories.
 * @return true on success, false on failure.
 */
auto rename_file( const fs::path &old_path, const fs::path &new_path ) -> bool;
/**
 * Check if can write to the given directory (write permission, disk space).
 * @return false if cannot write or if unable to check.
 */
auto can_write_to_dir( const fs::path &dir_path ) -> bool;
/**
 * Copy file, overwriting the target. Does not overwrite directories.
 * @return true on success, false on failure.
 */
auto copy_file( const fs::path &source_path, const fs::path &dest_path ) -> bool;
/** Get process id string. Used for temporary file paths. */
auto get_pid_string() -> std::string;

/**
 * Read entire file to string.
 * @return empty string on failure.
 */
auto read_entire_file( const fs::path &path ) -> std::string;

/** Force 'path' to be a normalized directory */
auto as_norm_dir( const fs::path &path ) -> fs::path;

namespace cata_files
{
auto eol() -> const char *;
} // namespace cata_files

//--------------------------------------------------------------------------------------------------
/**
 * Returns a vector of files or directories matching pattern at @p root_path.
 *
 * Searches through the directory tree breadth-first. Directories are searched in lexical
 * order. Matching files within in each directory are also ordered lexically.
 *
 * @param pattern The sub-string or path-shaped file name to match.
 * @param root_path The path relative to the current working directory to search; empty means ".".
 * @param recursive_search Whether to recursively search sub directories.
 * @param match_extension If true, match pattern at the end of file names. Otherwise, match anywhere
 *                        in the file name.
 */
auto get_files_from_path( const fs::path &pattern,
const fs::path &root_path = {}, bool recursive_search = false,
bool match_extension = false ) -> std::vector<fs::path>;

//--------------------------------------------------------------------------------------------------
/**
 * Returns a vector of directories which contain files matching any of @p patterns.
 *
 * @param patterns A vector or patterns to match.
 * @see get_files_from_path
 */
auto get_directories_with( const std::vector<fs::path> &patterns,
const fs::path &root_path = {}, bool recursive_search = false ) -> std::vector<fs::path>;

auto get_directories_with( const fs::path &pattern,
const fs::path &root_path = {}, bool recursive_search = false ) -> std::vector<fs::path>;

/**
 *  Replace invalid characters in a string with a default character; can be used to ensure that a file name is compliant with most file systems.
 *  @param file_name Name of the file to check.
 *  @return A string with all invalid characters replaced with the replacement character, if any change was made.
 *  @note  The default replacement character is space (0x20) and the invalid characters are "\\/:?\"<>|".
 */
auto ensure_valid_file_name( const std::string &file_name ) -> std::string;

