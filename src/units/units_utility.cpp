#include "units_utility.h"

#include "options.h"

auto velocity_units(const units_type vel_units) -> const char* {
    if (get_option<std::string>("USE_METRIC_SPEEDS") == "mph") {
        return _("mph");
    } else if (get_option<std::string>("USE_METRIC_SPEEDS") == "t/t") {
        //~ vehicle speed tiles per turn
        return _("t/t");
    } else {
        switch (vel_units) {
            case VU_VEHICLE:
                return _("km/h");
            case VU_WIND:
                return _("m/s");
        }
    }
    return "error: unknown units!";
}

auto normalize(units::angle a, units::angle mod) -> units::angle {
    a = units::fmod(a, mod);
    if (a < 0_degrees) { a += mod; }
    return a;
}

auto weight_units() -> const char* {
    return get_option<std::string>("USE_METRIC_WEIGHTS") == "lbs" ? _("lbs") : _("kg");
}

auto volume_units_abbr() -> const char* {
    const std::string vol_units = get_option<std::string>("VOLUME_UNITS");
    if (vol_units == "c") {
        return pgettext("Volume unit", "c");
    } else if (vol_units == "l") {
        return pgettext("Volume unit", "L");
    } else {
        return pgettext("Volume unit", "qt");
    }
}

auto volume_units_long() -> const char* {
    const std::string vol_units = get_option<std::string>("VOLUME_UNITS");
    if (vol_units == "c") {
        return _("cup");
    } else if (vol_units == "l") {
        return _("liter");
    } else {
        return _("quart");
    }
}

auto convert_velocity(int velocity, const units_type vel_units) -> double {
    const std::string type = get_option<std::string>("USE_METRIC_SPEEDS");
    if (vel_units == VU_VEHICLE) {
        const double meters_per_second = static_cast<double>(velocity) / 100.0;
        if (type == "mph") { return meters_per_second * 2.23694; }
        if (type == "t/t") { return meters_per_second / 1.78816; }
        return meters_per_second * 3.6;
    }

    double ret = static_cast<double>(velocity) / 100;
    if (type == "km/h") {
        ret *= 0.447f;
    } else if (type == "t/t") {
        ret /= 4;
    }

    return ret;
}

auto convert_weight(const units::mass& weight) -> double {
    double ret = to_gram(weight);
    if (get_option<std::string>("USE_METRIC_WEIGHTS") == "kg") {
        ret /= 1000;
    } else {
        ret /= 453.6;
    }
    return ret;
}

auto convert_volume(int volume) -> double { return convert_volume(volume, nullptr); }

auto convert_volume(int volume, int* out_scale) -> double {
    double ret = volume;
    int scale = 0;
    const std::string vol_units = get_option<std::string>("VOLUME_UNITS");
    if (vol_units == "c") {
        ret *= 0.004;
        scale = 1;
    } else if (vol_units == "l") {
        ret *= 0.001;
        scale = 2;
    } else {
        ret *= 0.00105669;
        scale = 2;
    }
    if (out_scale != nullptr) { *out_scale = scale; }
    return ret;
}
