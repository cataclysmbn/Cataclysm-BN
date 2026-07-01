#pragma once

#include "coordinates.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace save_tools {

enum class blob_compression_mode {
    info,
    compress,
    decompress,
};

struct blob_compression_stats {
    std::size_t databases = 0;
    std::size_t rows = 0;
    std::size_t compressed_rows = 0;
    std::size_t uncompressed_rows = 0;
    std::size_t unknown_compression_rows = 0;
    std::size_t changed_rows = 0;
    std::uint64_t stored_bytes = 0;
    std::uint64_t raw_bytes = 0;
};

struct save_prune_bubble {
    point_abs_omt center;
    int radius = 0;
};

struct save_prune_preview {
    std::size_t databases = 0;
    std::size_t kept_rows = 0;
    std::size_t pruned_rows = 0;
    std::size_t ignored_rows = 0;
    std::vector<std::string> vehicles;
    std::vector<std::string> npcs;
};

struct save_prune_result {
    save_prune_preview preview;
    std::filesystem::path backup_path;
    unsigned int old_seed = 0;
    unsigned int new_seed = 0;
};

auto rewrite_world_blobs(const std::filesystem::path& world_path, blob_compression_mode mode)
    -> blob_compression_stats;
auto preview_prune_world_outside_bubble(
    const std::filesystem::path& world_path, const save_prune_bubble& bubble) -> save_prune_preview;
auto prune_world_outside_bubble(
    const std::filesystem::path& world_path, const save_prune_bubble& bubble) -> save_prune_result;
auto make_world_backup(const std::filesystem::path& world_path) -> std::filesystem::path;

} // namespace save_tools
