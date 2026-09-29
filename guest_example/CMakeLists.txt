# SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
# SPDX-License-Identifier: BSD-2-Clause

# Builds the example Guest VM payload as its own freestanding C++23
# executable -- reusing reloco/microfmt and the same bare-metal compiler
# flags as microvisor.elf itself -- then embeds the resulting flat binary
# back into the hypervisor as a linkable object, so src/main.cpp no longer
# needs a hand-encoded array of raw ARM instruction words.

add_executable(guest_example.elf startup.S main.cpp)

target_compile_options(guest_example.elf PRIVATE ${BARE_METAL_FLAGS})

set(GUEST_EXAMPLE_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")

set_target_properties(guest_example.elf PROPERTIES LINK_DEPENDS "${GUEST_EXAMPLE_LINKER_SCRIPT}")

target_link_options(guest_example.elf PRIVATE
    -nostdlib
    -T${GUEST_EXAMPLE_LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=guest_example.map
)

target_link_libraries(guest_example.elf PRIVATE
    jplcz_microfmt::microfmt
    jplcz_reloco::reloco
    minilibc
)

# Flatten the ELF into the raw image that actually gets loaded into Guest RAM.
set(GUEST_EXAMPLE_BIN "${CMAKE_CURRENT_BINARY_DIR}/guest_example.bin")
add_custom_command(
    OUTPUT "${GUEST_EXAMPLE_BIN}"
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:guest_example.elf> "${GUEST_EXAMPLE_BIN}"
    DEPENDS guest_example.elf
    COMMENT "Generating raw guest binary: guest_example.bin"
    VERBATIM
)

# Wrap that flat binary as a linkable object exposing the standard objcopy
# `_binary_<name>_{start,end,size}` symbols (deterministic because objcopy
# derives them from the input file's basename, and we invoke it from this
# fixed working directory), so microvisor.elf can embed it directly.
set(GUEST_EXAMPLE_BLOB_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/guest_example_blob.o")
add_custom_command(
    OUTPUT "${GUEST_EXAMPLE_BLOB_OBJECT}"
    COMMAND ${CMAKE_OBJCOPY} -I binary -O elf32-littlearm -B arm guest_example.bin guest_example_blob.o
    DEPENDS "${GUEST_EXAMPLE_BIN}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Embedding guest_example.bin as guest_example_blob.o (_binary_guest_example_bin_{start,end,size})"
    VERBATIM
)

add_custom_target(guest_example_blob ALL DEPENDS "${GUEST_EXAMPLE_BLOB_OBJECT}")

# Exposed to the top-level CMakeLists.txt so it can add this externally
# generated object file directly as a source of microvisor.elf.
set(GUEST_EXAMPLE_BLOB_OBJECT "${GUEST_EXAMPLE_BLOB_OBJECT}" PARENT_SCOPE)
