cmake_minimum_required(VERSION 3.20)

# The toolchain file must be provided during configuration:
# cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabi.cmake
project(microvisor LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JPLCZ_RELOCO_PORTING_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/reloco_porting" CACHE PATH "" FORCE)
set(JPLCZ_MICROFMT_PORTING_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/microfmt_porting" CACHE PATH "" FORCE)

if(NOT TARGET jplcz_microfmt::microfmt)
    set(MICROVISOR_MICROFMT_SOURCE_DIR "" CACHE PATH
        "Path to a local jplcz_microfmt checkout to use instead of fetching it")

    if(NOT MICROVISOR_MICROFMT_SOURCE_DIR AND DEFINED ENV{MICROVISOR_MICROFMT_SOURCE_DIR})
        set(MICROVISOR_MICROFMT_SOURCE_DIR "$ENV{MICROVISOR_MICROFMT_SOURCE_DIR}" CACHE PATH
            "Path to a local jplcz_microfmt checkout to use instead of fetching it" FORCE)
    endif()

    set(MICROVISOR_MICROFMT_GIT_REPOSITORY "https://github.com/jplcz/microfmt.git" CACHE STRING
        "Git repository to fetch jplcz_microfmt from when MICROVISOR_MICROFMT_SOURCE_DIR is unset")
    set(MICROVISOR_MICROFMT_GIT_TAG "master" CACHE STRING
        "Git tag or commit to fetch jplcz_microfmt from when MICROVISOR_MICROFMT_SOURCE_DIR is unset")

    include(FetchContent)

    if(MICROVISOR_MICROFMT_SOURCE_DIR)
        FetchContent_Declare(
            jplcz_microfmt
            SOURCE_DIR "${MICROVISOR_MICROFMT_SOURCE_DIR}"
        )
    else()
        FetchContent_Declare(
            jplcz_microfmt
            GIT_REPOSITORY "${MICROVISOR_MICROFMT_GIT_REPOSITORY}"
            GIT_TAG "${MICROVISOR_MICROFMT_GIT_TAG}"
        )
    endif()

    # Disable microfmt's tests, strict warnings, and installation logic since
    # we are statically consuming it for a bare-metal executable.
    set(JPLCZ_MICROFMT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(JPLCZ_MICROFMT_ENABLE_STRICT_WARNINGS OFF CACHE BOOL "" FORCE)
    set(JPLCZ_MICROFMT_INSTALL OFF CACHE BOOL "" FORCE)

    # This will download microfmt, execute its CMakeLists.txt, which in turn
    # executes the snippet you provided to pull reloco.
    FetchContent_MakeAvailable(jplcz_microfmt)
endif()

if(NOT TARGET jplcz_structo::structo)
    set(MICROVISOR_STRUCTO_SOURCE_DIR "" CACHE PATH
        "Path to a local jplcz_structo checkout to use instead of fetching it")

    if(NOT MICROVISOR_STRUCTO_SOURCE_DIR AND DEFINED ENV{MICROVISOR_STRUCTO_SOURCE_DIR})
        set(MICROVISOR_STRUCTO_SOURCE_DIR "$ENV{MICROVISOR_STRUCTO_SOURCE_DIR}" CACHE PATH
            "Path to a local jplcz_structo checkout to use instead of fetching it" FORCE)
    endif()

    set(MICROVISOR_STRUCTO_GIT_REPOSITORY "https://github.com/jplcz/structo.git" CACHE STRING
        "Git repository to fetch jplcz_structo from when MICROVISOR_STRUCTO_SOURCE_DIR is unset")
    set(MICROVISOR_STRUCTO_GIT_TAG "master" CACHE STRING
        "Git tag or commit to fetch jplcz_structo from when MICROVISOR_STRUCTO_SOURCE_DIR is unset")

    include(FetchContent)

    if(MICROVISOR_STRUCTO_SOURCE_DIR)
        FetchContent_Declare(
            jplcz_structo
            SOURCE_DIR "${MICROVISOR_STRUCTO_SOURCE_DIR}"
        )
    else()
        FetchContent_Declare(
            jplcz_structo
            GIT_REPOSITORY "${MICROVISOR_STRUCTO_GIT_REPOSITORY}"
            GIT_TAG "${MICROVISOR_STRUCTO_GIT_TAG}"
        )
    endif()

    # jplcz_structo is header-only and depends on jplcz_reloco itself; since
    # jplcz_microfmt (above) already brought jplcz_reloco::reloco into scope,
    # structo's own "if(NOT TARGET jplcz_reloco::reloco)" guard will reuse it
    # as-is rather than fetching/declaring a second copy. Disable structo's
    # own development targets (tests, header checks, strict warnings) since
    # we are statically consuming it for a bare-metal executable.
    set(JPLCZ_STRUCTO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(JPLCZ_STRUCTO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(JPLCZ_STRUCTO_BUILD_HEADER_CHECKS OFF CACHE BOOL "" FORCE)
    set(JPLCZ_STRUCTO_ENABLE_STRICT_WARNINGS OFF CACHE BOOL "" FORCE)
    set(JPLCZ_STRUCTO_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(jplcz_structo)
endif()

# Define the freestanding, zero-overhead environment
set(BARE_METAL_FLAGS
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
    -fno-stack-protector
    -fno-pic
    -Wall -Wextra -Werror
)

add_subdirectory(minilibc)
add_subdirectory(guest_example)

add_executable(microvisor.elf src/startup.S src/main.cpp src/exceptions.S src/mmu.S src/world_switch.S
    src/timer.S src/sysregs.S src/banked_regs.S)

target_compile_options(microvisor.elf PRIVATE ${BARE_METAL_FLAGS})

# Include local directories
target_include_directories(microvisor.elf PRIVATE include)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")

set_target_properties(microvisor.elf PROPERTIES LINK_DEPENDS "${LINKER_SCRIPT}")

target_link_options(microvisor.elf PRIVATE
    -nostdlib
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=microvisor.map
)

target_link_libraries(microvisor.elf PRIVATE
    jplcz_microfmt::microfmt
    jplcz_reloco::reloco
    jplcz_structo::structo
    minilibc
)

# Embed the guest_example payload (see guest_example/CMakeLists.txt) as a
# plain externally-generated object file directly into microvisor.elf.
set_source_files_properties(${GUEST_EXAMPLE_BLOB_OBJECT} PROPERTIES
    EXTERNAL_OBJECT TRUE
    GENERATED TRUE
)
target_sources(microvisor.elf PRIVATE ${GUEST_EXAMPLE_BLOB_OBJECT})
add_dependencies(microvisor.elf guest_example_blob)

add_custom_command(TARGET microvisor.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary microvisor.elf microvisor.bin
    COMMENT "Generating raw binary: microvisor.bin"
)