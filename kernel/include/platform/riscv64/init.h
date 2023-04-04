/*
 * Copyright (C) 2020-2023 The opuntiaOS Project Authors.
 *  + Contributed by Nikita Melekhin <nimelehin@gmail.com>
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _KERNEL_PLATFORM_RISCV64_INIT_H
#define _KERNEL_PLATFORM_RISCV64_INIT_H

#include <libkern/types.h>

void platform_init_boot_cpu();
void platform_setup_boot_cpu();
void platform_setup_secondary_cpu();

#endif /* _KERNEL_PLATFORM_RISCV64_INIT_H */