#pragma once

#include "client_animation.h"
#include "presentation_sink.h"

#include <memory>

/// Clears the presentation sink and animation service the display registered, and puts them back
/// when it goes out of scope, so a test that installs its own leaves the next test the display it
/// found.
struct scoped_presentation {
    std::unique_ptr<presentation::sink> sink = presentation::set_sink(nullptr);
    std::unique_ptr<game_client::animation_service> animation = game_client::set_animation(nullptr);
    scoped_presentation() = default;
    scoped_presentation(const scoped_presentation&) = delete;
    auto operator=(const scoped_presentation&) -> scoped_presentation& = delete;
    ~scoped_presentation() {
        presentation::set_sink(std::move(sink));
        game_client::set_animation(std::move(animation));
    }
};
