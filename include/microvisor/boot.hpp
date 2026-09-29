#pragma once

#include <cstdint>

extern "C"
{
    // Physical address of the Device Tree Blob (DTB) passed in r2 by the
    // bootloader/QEMU, per the ARM Linux kernel boot protocol
    // (r0=0, r1=machine ID, r2=DTB pointer). Populated by startup.S before
    // any C++ code runs.
    extern void *g_dtb_ptr;
}
