#pragma once

#include <memory>

class iuse_actor;

/// Item action for photo albums, configured through their item variables.
auto make_photo_album_actor() -> std::unique_ptr<iuse_actor>;
