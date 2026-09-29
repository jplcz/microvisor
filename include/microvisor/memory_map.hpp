#pragma once
#include <reloco/region_set.hpp>

namespace microvisor
{

    struct memory_map
    {
        // The absolute physical bounds of the hardware RAM
        reloco::region_set<16, uintptr_t> total;

        // The safe, usable RAM after subtracting hypervisor footprint and boot ROM
        reloco::region_set<16, uintptr_t> free;
    };

} // namespace microvisor
