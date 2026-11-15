#pragma once

#include "options.h"
#include "utils/time_config_generation.h"

#include <string>

/// Option value that is read again only after `calendar::option_generation()` changes.
/// Use one `thread_local` instance per call site on paths that run every turn.
template <typename T> struct cached_option {
    std::string name;
    int generation = -1;
    T value{};

    auto get() -> T {
        const auto current = calendar::option_generation();
        if (generation != current) {
            value = get_option<T>(name);
            generation = current;
        }
        return value;
    }
};
