set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Specify the cross-compiler
set(CMAKE_C_COMPILER arm-linux-gnueabi-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabi-g++)
set(CMAKE_ASM_COMPILER arm-linux-gnueabi-gcc)
set(CMAKE_OBJCOPY arm-linux-gnueabi-objcopy)

# Target ARMv7-A with Virtualization Extensions (Cortex-A15 is a safe default)
set(ARCH_FLAGS "-mcpu=cortex-a15 -marm -mfloat-abi=soft -mno-unaligned-access")

set(CMAKE_C_FLAGS_INIT   "${ARCH_FLAGS} -mthumb")
set(CMAKE_CXX_FLAGS_INIT "${ARCH_FLAGS} -mthumb")
set(CMAKE_ASM_FLAGS_INIT "${ARCH_FLAGS}")

# Prevent CMake from trying to link a full C library during its compiler test
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
