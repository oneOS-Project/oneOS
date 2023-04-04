#if defined(__i386__) || defined(__x86_64__)
#include <platform/x86/vmm/consts.h>
#elif __arm__
#include <platform/arm32/vmm/consts.h>
#elif __aarch64__
#include <platform/arm64/vmm/consts.h>
#elif defined(__riscv) && (__riscv_xlen == 64)
#include <platform/riscv64/vmm/consts.h>
#endif