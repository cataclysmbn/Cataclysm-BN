#pragma once

#include "coordinates.h"
#include "visitable.h"

#include <iosfwd>
#include <vector>

class vehicle;

class vehicle_cursor: public location_visitable<vehicle_cursor> {
public:
    vehicle_cursor(vehicle& veh, std::ptrdiff_t part, bool ignore_vpart = false)
        : veh(veh),
          part(part),
          ignore_vpart(ignore_vpart) {}
    vehicle& veh;
    std::ptrdiff_t part;
    bool ignore_vpart;
};

class vehicle_selector: public location_visitable<vehicle_selector> {
    friend location_visitable<vehicle_selector>;

public:
    using value_type = vehicle_cursor;
    using size_type = std::vector<value_type>::size_type;
    using iterator = std::vector<value_type>::iterator;
    using const_iterator = std::vector<value_type>::const_iterator;
    using reference = std::vector<value_type>::reference;
    using const_reference = std::vector<value_type>::const_reference;

    /**
     *  Constructs vehicle_selector used for querying items located on vehicle tiles
     *  @param pos map position at which to start each query which may or may not contain vehicle
     *  @param radius number of adjacent tiles to include (searching from pos outwards)
     *  @param accessible whether found items must be accessible from pos to be considered
     *  @param visibility_only accessibility based on line of sight, not walkability
     */
    vehicle_selector(
        const tripoint_bub_ms& pos, int radius = 0, bool accessible = true,
        bool visibility_only = false);

    /**
     *  Constructs vehicle_selector used for querying items located on vehicle tiles
     *  @param pos map position at which to start each query which may or may not contain vehicle
     *  @param radius number of adjacent tiles to include (searching from pos outwards)
     *  @param accessible whether found items must be accessible from pos to be considered
     *  @param ignore don't include this vehicle as part of the selection
     */
    vehicle_selector(
        const tripoint_bub_ms& pos, int radius, bool accessible, const vehicle& ignore);

    // similar to item_location you are not supposed to store this class between turns
    vehicle_selector(const vehicle_selector& that) = delete;
    auto operator=(const vehicle_selector&) -> vehicle_selector& = delete;
    vehicle_selector(vehicle_selector&&) = default;

    auto size() const -> size_type { return data.size(); }
    auto begin() -> iterator { return data.begin(); }
    auto end() -> iterator { return data.end(); }
    auto begin() const -> const_iterator { return data.cbegin(); }
    auto end() const -> const_iterator { return data.cend(); }
    auto cbegin() const -> const_iterator { return data.cbegin(); }
    auto cend() const -> const_iterator { return data.cend(); }
    auto front() -> reference { return data.front(); }
    auto front() const -> const_reference { return data.front(); }
    auto back() -> reference { return data.back(); }
    auto back() const -> const_reference { return data.back(); }

private:
    std::vector<value_type> data;
};
