#ifndef _KERNEL_LIBKERN_VERSION_H
#define _KERNEL_LIBKERN_VERSION_H

#define OSTYPE "opuntiaOS"
#define OSRELEASE "1.0.0-dev"
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_REVISION 0
#define VERSION_VARIANT "0"

#ifdef __i386__
#define MACHINE "x86"
#elif __x86_64__
#define MACHINE "x86-64"
#elif __arm__
#define MACHINE "arm"
#elif __aarch64__
#define MACHINE "arm64"
#elif defined(__riscv) && (__riscv_xlen == 64)
#define MACHINE "riscv64"
#endif

#endif // _KERNEL_LIBKERN_VERSION_H