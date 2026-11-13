#pragma once

/// Parts of loading the tileset that loading a world runs ahead of the tileset step proper.
/// Only available in builds with tiles; they do nothing while tiles are not in use.
namespace tileset_loading
{

/// Starts decoding the images of the tileset in use on worker threads, unless that tileset
/// is loaded already. The tileset picks the decoded images up when it loads.
auto decode_images_ahead() -> void;
/// Drops the images decoded ahead that no tileset picked up.
auto discard_images_decoded_ahead() -> void;
/// Loads the tileset for the active world. load_tileset() then finds it loaded.
auto load_for_active_world() -> void;

} // namespace tileset_loading
