#pragma once

extern void do_panic(const char *expr, const char *file, int line, const char *mesage);

#define RELOCO_KERNEL_PANIC(expression, file, line, message) do_panic(expression, file, line, message)

#define RELOCO_DEFAULT_ALLOCATOR_CUSTOM

#define RELOCO_KERNEL

#define RELOCO_TLS_MODEL_SINGLE

struct cntpct_clock_tag;

#define RELOCO_INSTANT_CLOCK_TAG cntpct_clock_tag
