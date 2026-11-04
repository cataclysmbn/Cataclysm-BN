#pragma once

#include "calendar.h"
#include "type_id.h"

struct itype;

/// Inputs for detached presentation-only item construction. No gameplay spawn actors run.
struct item_preview_request {
    const itype *type;
    time_point turn = calendar::turn;
    int charges = -1;
};

struct item_ammo_request {
    itype_id ammo;
    int charges = -1;
    bool preview = false;
};
