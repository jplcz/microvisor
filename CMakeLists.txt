cmake_minimum_required(VERSION 3.20)

# The toolchain file must be provided during configuration:
# cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabi.cmake
project(microvisor LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-DRELOCO_CONFIG=1  -DMICROFMT_CONFIG=1)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(JPLCZ_RELOCO_PORTING_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/reloco_porting")
set(JPLCZ_MICROFMT_PORTING_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/microfmt_porting")

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

add_executable(microvisor.elf src/startup.S src/main.cpp src/exceptions.S src/mmu.S src/world_switch.S
    src/timer.S)

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
    minilibc
)

add_custom_command(TARGET microvisor.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary microvisor.elf microvisor.bin
    COMMENT "Generating raw binary: microvisor.bin"
)