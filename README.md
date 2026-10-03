# microvisor

`microvisor` is an experimental, freestanding ARMv7-A hypervisor prototype. It
uses the ARM virtualization extensions to enter Hyp mode, configure stage-1 and
stage-2 address translation, switch to a guest vCPU, and handle guest exits.
The current demo boots one small guest, emulates a couple of MMIO devices, and
stops the guest through its virtual power device.

This project also serves as a proof of feasibility for using
[reloco](https://github.com/jplcz/reloco) and
[microfmt](https://github.com/jplcz/microfmt) in a freestanding, bare-metal
environment. It is a place to exercise both libraries in a concrete system and
discover real-world use cases for them.

The project targets QEMU's `virt` machine with a Cortex-A15 CPU and
virtualization enabled. It is a development prototype, not a production
hypervisor or a general-purpose virtual machine monitor.

## Features

- ARM Hyp-mode startup, exception vectors, and guest/host world switching.
- LPAE stage-1 and stage-2 translation, guest memory mapping, and page
  allocation.
- vCPU state, virtual timer handling, and trap dispatch.
- MMIO device routing with simple console and power-control devices.
- PL011 logging through QEMU's emulated UART.

## Requirements

- CMake 3.20 or newer.
- An ARM cross-compilation toolchain providing `arm-linux-gnueabi-gcc`,
  `arm-linux-gnueabi-g++`, and `arm-linux-gnueabi-objcopy`.
- QEMU's `qemu-system-arm` to run the demo.

By default, CMake fetches the `microfmt` and `structo` dependencies (which
bring in `reloco` transitively). For offline development or local dependency
changes, set `MICROVISOR_MICROFMT_SOURCE_DIR` and `MICROVISOR_STRUCTO_SOURCE_DIR`
to local checkouts (`./config.sh` does this for sibling `../microfmt`,
`../structo`, and `../reloco` checkouts).

### Overriding cache variables (local checkouts, porting headers)

Preset any `JPLCZ_RELOCO_*`/`JPLCZ_MICROFMT_*` cache variable that
`microfmt`/`reloco` will later declare themselves (for example
`JPLCZ_RELOCO_PORTING_HEADERS`, `JPLCZ_MICROFMT_PORTING_HEADERS`, used here to
stage `reloco_porting/`'s custom allocator/panic/TLS/clock backends and
`default_log_sink.hpp`) with `CACHE <type> "" FORCE`, **never** a plain
`set(VAR value)`:

```cmake
set(JPLCZ_RELOCO_PORTING_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/reloco_porting"
    CACHE PATH "" FORCE)
set(JPLCZ_MICROFMT_PORTING_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/reloco_porting"
    CACHE PATH "" FORCE)
```

CMake's `set(<var> <value> CACHE <type> <docstring>)` (without `FORCE`)
silently discards any plain/normal variable of the same name already in
scope the first time it runs -- even though the cache entry did not
previously exist. This repository's `CMakeLists.txt` originally preset both
variables above with a plain `set()`, before `FetchContent_MakeAvailable`
pulled in `microfmt` (and, transitively, `reloco`); as soon as reloco's own
`CMakeLists.txt` declared `JPLCZ_RELOCO_PORTING_HEADERS` (via an unforced
`set(... CACHE PATH ...)`), the preset value was silently cleared, with no
warning or error. The practical effect: every custom porting backend
(`RELOCO_DEFAULT_ALLOCATOR_CUSTOM`, `RELOCO_KERNEL_PANIC`,
`RELOCO_TLS_MODEL_SINGLE`, `RELOCO_INSTANT_CLOCK_TAG`,
`MICROFMT_DEFAULT_LOG_SINK_BACKEND_CUSTOM`) was silently **not** applied --
reloco/microfmt fell back to their hosted-build defaults instead, with the
build still succeeding. Using `CACHE PATH "" FORCE` (as this project does
now) fixes this; see reloco's and microfmt's own READMEs ("Local checkout and
overriding cache variables") for the general rule.

### How the microfmt and structo dependencies are resolved

`CMakeLists.txt` resolves `jplcz_microfmt` and `jplcz_structo` in a fixed
order, each step only taken if the previous one did not already settle the
question (repeated independently for each dependency, under its own
`MICROVISOR_MICROFMT_*`/`MICROVISOR_STRUCTO_*` variable names):

1. **Detect an existing target.** `if(NOT TARGET jplcz_microfmt::microfmt)` /
   `if(NOT TARGET jplcz_structo::structo)` guard each block: if the target
   already exists (for example, `jplcz_structo`'s own
   `add_subdirectory`/`FetchContent_MakeAvailable(jplcz_reloco)` already
   satisfied `jplcz_reloco::reloco` before microvisor's own `jplcz_reloco`
   step would otherwise run), the existing target is reused and every step
   below is skipped.
2. **A local checkout, named by variable.** `MICROVISOR_MICROFMT_SOURCE_DIR` /
   `MICROVISOR_STRUCTO_SOURCE_DIR` (each a `CACHE PATH`, settable with `-D` or
   `set(... FORCE)`, or equivalently the identically-named environment
   variable when the cache variable is left unset) points
   `FetchContent_Declare`'s `SOURCE_DIR` at a local working copy, bypassing
   Git entirely -- this is what `config.sh` sets for the sibling `../microfmt`
   and `../structo` checkouts.
3. **A user-selected Git remote.** If no local checkout was named,
   `MICROVISOR_MICROFMT_GIT_REPOSITORY`/`_GIT_TAG` and
   `MICROVISOR_STRUCTO_GIT_REPOSITORY`/`_GIT_TAG` (also `CACHE STRING`
   variables) let a consumer point `FetchContent_Declare` at their own fork,
   mirror, or pinned tag/commit instead of upstream.
4. **The official repository, by default.** If neither of the above was set,
   these `GIT_REPOSITORY`/`GIT_TAG` variables default to
   `https://github.com/jplcz/microfmt.git`/`https://github.com/jplcz/structo.git`
   and `master`, so a plain `cmake -B build` with no extra configuration
   still works out of the box.

`microfmt`'s and `structo`'s own `CMakeLists.txt` resolve their transitive
`jplcz_reloco` dependency the same way; see their own READMEs ("How the
jplcz_reloco dependency is resolved").

## Build

Configure and build a Debug image:

```sh
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabi.cmake \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The build produces `build/microvisor.elf` and the raw boot image
`build/microvisor.bin`.

To configure both Debug and size-optimized builds using local dependency
checkouts in the sibling directories `../microfmt` and `../reloco`, run:

```sh
./config.sh
```

## Run

Build and launch the Debug image on QEMU:

```sh
./start.sh
```

The script runs `cmake --build build` and starts
`qemu-system-arm -M virt,virtualization=on -cpu cortex-a15 -m 256 -nographic`.
Additional arguments are passed through to QEMU, for example:

```sh
./start.sh -d
```

The guest in `src/main.cpp` is a minimal demonstration, not a guest OS. It
executes a few ARM instructions, accesses the virtual power device, and halts;
hypervisor and guest-exit messages are printed through the emulated PL011
console.
