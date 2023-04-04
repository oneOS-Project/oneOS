#ifndef _KERNEL_LIBKERN_TYPES_H
#define _KERNEL_LIBKERN_TYPES_H

#include <libkern/_types/_va_list.h>
#include <libkern/bits/types.h>

#ifndef __stdints_defined
#define __stdints_defined
typedef __int8_t int8_t;
typedef __int16_t int16_t;
typedef __int32_t int32_t;
typedef __int64_t int64_t;
typedef __uint8_t uint8_t;
typedef __uint16_t uint16_t;
typedef __uint32_t uint32_t;
typedef __uint64_t uint64_t;
#endif // __stdints_defined

#ifndef __dev_t_defined
#define __dev_t_defined
typedef __dev_t dev_t;
#endif // __dev_t_defined

#ifndef __uid_t_defined
#define __uid_t_defined
typedef __uid_t uid_t;
#endif // __uid_t_defined

#ifndef __gid_t_defined
#define __gid_t_defined
typedef __gid_t gid_t;
#endif // __gid_t_defined

#ifndef __ino_t_defined
#define __ino_t_defined
typedef __ino_t ino_t;
#endif // __ino_t_defined

#ifndef __ino64_t_defined
#define __ino64_t_defined
typedef __ino64_t ino64_t;
#endif // __ino64_t_defined

#ifndef __mode_t_defined
#define __mode_t_defined
typedef __mode_t mode_t;
#endif // __mode_t_defined

#ifndef __nlink_t_defined
#define __nlink_t_defined
typedef __nlink_t nlink_t;
#endif // __nlink_t_defined

#ifndef __off_t_defined
#define __off_t_defined
typedef __off_t off_t;
#endif // __off_t_defined

#ifndef __off64_t_defined
#define __off64_t_defined
typedef __off64_t off64_t;
#endif // __off64_t_defined

#ifndef __pid_t_defined
#define __pid_t_defined
typedef __pid_t pid_t;
#endif // __pid_t_defined

#ifndef __fsid_t_defined
#define __fsid_t_defined
typedef __fsid_t fsid_t;
#endif // __fsid_t_defined

#ifndef __time_t_defined
#define __time_t_defined
typedef __time_t time_t;
#endif // __time_t_defined

#ifdef __i386__
typedef int32_t intptr_t;
typedef uint32_t uintptr_t;
typedef off_t off_t;
#if defined(__clang__)
typedef unsigned int size_t;
typedef int ssize_t;
#elif defined(__GNUC__) || defined(__GNUG__)
typedef unsigned long size_t;
typedef long ssize_t;
#endif
#define BITS32
#elif __x86_64__
typedef uint64_t size_t;
typedef int64_t ssize_t;
typedef int64_t intptr_t;
typedef uint64_t uintptr_t;
typedef off64_t off_t;
#define BITS64
#elif __arm__
typedef unsigned int size_t;
typedef int ssize_t;
typedef int32_t intptr_t;
typedef uint32_t uintptr_t;
typedef off_t off_t;
#define BITS32
#elif __aarch64__
typedef uint64_t size_t;
typedef int64_t ssize_t;
typedef int64_t intptr_t;
typedef uint64_t uintptr_t;
typedef off64_t off_t;
#define BITS64
#elif defined(__riscv) && (__riscv_xlen == 64)
typedef uint64_t size_t;
typedef int64_t ssize_t;
typedef int64_t intptr_t;
typedef uint64_t uintptr_t;
typedef off64_t off_t;
#define BITS64
#endif

#define __user

#define bool _Bool
#define true (1)
#define false (0)

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define NULL ((void*)0)

#define MINORBITS 20
#define MINORMASK ((1U << MINORBITS) - 1)

#define MAJOR(dev) ((unsigned int)((dev) >> MINORBITS))
#define MINOR(dev) ((unsigned int)((dev)&MINORMASK))
#define MKDEV(ma, mi) (((ma) << MINORBITS) | (mi))

#endif // _KERNEL_LIBKERN_TYPES_H