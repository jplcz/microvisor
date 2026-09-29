/* The guest entry point defined in startup.S */
ENTRY(_start)

/*
 * This must match the 2 MB (order-9) Host RAM block that src/main.cpp
 * carves out of the buddy allocator and Stage-2 maps to Guest IPA
 * 0x40000000 -- see the `guest_ram_order`/`gpa_ram` in bootstrap code.
 */
MEMORY
{
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 2M
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : ALIGN(4096)
    {
        /* KEEP + placing .text.boot first mirrors the hypervisor's own
         * linker.ld: the raw .bin must start with the entry point. */
        KEEP(*(.text.boot))
        *(.text .text.*)
    } > RAM

    .rodata : ALIGN(4096)
    {
        *(.rodata .rodata.*)
    } > RAM

    /* C++ Global Constructors, same convention as the hypervisor's own
     * linker.ld -- lets guest-side global objects initialize. */
    .init_array : ALIGN(4)
    {
        __init_array_start = .;
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    } > RAM

    .data : ALIGN(4096)
    {
        *(.data .data.*)
    } > RAM

    .bss (NOLOAD) : ALIGN(4096)
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /* Small boot stack for the guest payload. */
    . = ALIGN(8);
    . = . + 0x1000; /* 4 KB */
    . = ALIGN(4096);
    __stack_top = .;

    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.gnu_debuglink)
        *(.interp)
        *(.comment)
    }
}
