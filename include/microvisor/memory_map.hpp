#pragma once
#include <structo/region_set.hpp>

namespace microvisor
{

    struct memory_map
    {
        // The absolute physical bounds of the hardware RAM
        structo::region_set<16, uintptr_t> total;

        // The safe, usable RAM after subtracting hypervisor footprint and boot ROM
        structo::region_set<16, uintptr_t> free;
    };

} // namespace microvisor
