#!/bin/sh
cmake --build build
qemu-system-arm \
    -M virt,virtualization=on \
    -cpu cortex-a15 \
    -m 256 \
    -nographic \
    -kernel build/microvisor.bin \
    $@
