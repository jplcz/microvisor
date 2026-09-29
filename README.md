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

By default, CMake fetches the `microfmt` dependency (which brings in `reloco`).
For offline development or local dependency changes, set
`MICROVISOR_MICROFMT_SOURCE_DIR` to a local `microfmt` checkout.

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
