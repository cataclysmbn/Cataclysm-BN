#pragma once
#ifndef CATA_TESTS_TILE_HELPERS_H
#    define CATA_TESTS_TILE_HELPERS_H

#    include <memory>

/// RAII fixture: real cata_tiles contexts on a hidden-window software SDL renderer,
/// with the tiles-mode globals (tilecontext, overmap_tilecontext, use_tiles,
/// use_tiles_overmap) switched over for the test's scope and restored on destruction.
/// alias_overmap_context mirrors the stock TILES == OVERMAP_TILES shared-context setup.
class tile_context_fixture {
public:
    explicit tile_context_fixture(bool alias_overmap_context);
    ~tile_context_fixture();
    tile_context_fixture(const tile_context_fixture&) = delete;
    auto operator=(const tile_context_fixture&) -> tile_context_fixture& = delete; // *NOPAD*

    /// false when this environment cannot set up a headless tile context; WARN and bail out
    auto valid() const -> bool { return valid_; }

private:
    struct impl;
    std::unique_ptr<impl> pimpl_;
    bool valid_ = false;
};

#endif // CATA_TESTS_TILE_HELPERS_H
