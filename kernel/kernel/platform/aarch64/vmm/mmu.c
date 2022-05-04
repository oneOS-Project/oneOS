/*
 * Copyright (C) 2020-2022 The opuntiaOS Project Authors.
 *  + Contributed by Nikita Melekhin <nimelehin@gmail.com>
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <libkern/log.h>
#include <libkern/types.h>
#include <mem/vmm.h>

#define SET_FLAGS(mmu_flags, mf, arch_flags, af) \
    if (TEST_FLAG(mmu_flags, mf)) {              \
        arch_flags |= af;                        \
    }

#define SET_OP(mmu_flags, mf, op)   \
    if (TEST_FLAG(mmu_flags, mf)) { \
        op;                         \
    }

#define SET_OP_NEG(mmu_flags, mf, op) \
    if (!TEST_FLAG(mmu_flags, mf)) {  \
        op;                           \
    }

static inline void clear_arch_flags(ptable_entity_t* entity, ptable_lv_t lv)
{
    // TODO(aarch64): This is set for 4kb pages.
    const int frame_offset = 12;

    switch (lv) {
    case PTABLE_LV0:
    case PTABLE_LV1:
    case PTABLE_LV2:
    case PTABLE_LV3:
        *entity &= ~((1ull << (frame_offset)) - 1);
        *entity &= ((1ull << (48)) - 1);
        return;
    }
}

ptable_entity_t vm_mmu_to_arch_flags(mmu_flags_t mmu_flags, ptable_lv_t lv)
{
    ptable_entity_t arch_flags = 0;
    vm_ptable_entity_set_default_flags(&arch_flags, lv);

    switch (lv) {
    case PTABLE_LV0:
        SET_OP(mmu_flags, MMU_FLAG_PERM_READ, arch_flags |= 0b11);

        // MAIR has the following settings: 0x04ff, so if uncached setting index 1.
        SET_OP(mmu_flags, MMU_FLAG_UNCACHED, arch_flags |= (0b001 << 2));

        SET_OP(mmu_flags, MMU_FLAG_NONPRIV, arch_flags |= (0b01 << 6));
        SET_OP_NEG(mmu_flags, MMU_FLAG_PERM_WRITE, arch_flags |= (0b10 << 6));
        return arch_flags;

    case PTABLE_LV1:
    case PTABLE_LV2:
    case PTABLE_LV3:
        SET_OP(mmu_flags, MMU_FLAG_PERM_READ, arch_flags |= 0x3);
    }

    return arch_flags;
}

mmu_flags_t vm_arch_to_mmu_flags(ptable_entity_t* entity, ptable_lv_t lv)
{
    ptable_entity_t arch_flags = *entity;
    mmu_flags_t mmu_flags = 0;

    switch (lv) {
    case PTABLE_LV0:
        if ((arch_flags & 0b11) == 0b11) {
            mmu_flags |= MMU_FLAG_PERM_READ;
        }

        if (((arch_flags >> 2) & 0b111) == 0b001) {
            mmu_flags |= MMU_FLAG_UNCACHED;
        }

        if (((arch_flags >> 6) & 0b11) == 0b00) {
            mmu_flags |= MMU_FLAG_PERM_WRITE;
        } else if (((arch_flags >> 6) & 0b11) == 0b01) {
            mmu_flags |= MMU_FLAG_NONPRIV | MMU_FLAG_PERM_WRITE;
        } else if (((arch_flags >> 6) & 0b11) == 0b11) {
            mmu_flags |= MMU_FLAG_NONPRIV;
        }

        return mmu_flags;

    case PTABLE_LV1:
    case PTABLE_LV2:
    case PTABLE_LV3:
        mmu_flags |= MMU_FLAG_PERM_READ | MMU_FLAG_PERM_WRITE | MMU_FLAG_NONPRIV;
        return mmu_flags;
    }

    return mmu_flags;
}

mmu_pf_info_flags_t vm_arch_parse_pf_info(arch_pf_info_t info)
{
    mmu_pf_info_flags_t res = 0;

    // // pl0 bit is set at aarch32/interrupt_handlers.c
    // if (((info >> 31) & 0x1) == 0) {
    //     res |= MMU_PF_INFO_ON_NONPRIV_ACCESS;
    // }
    // if (((info >> 11) & 0x1) == 0x1) {
    //     res |= MMU_PF_INFO_ON_WRITE;
    // }
    // if ((info & 0b1101) == 0b0101) {
    //     res |= MMU_PF_INFO_ON_NOT_PRESENT;
    // }
    // if ((info & 0b1111) == 0b1111) {
    //     res |= MMU_PF_INFO_SECURITY_VIOLATION;
    // }
    return res;
}

ptable_state_t vm_ptable_entity_state(ptable_entity_t* entity, ptable_lv_t lv)
{
    ptable_entity_t arch_flags = *entity;

    switch (lv) {
    case PTABLE_LV0:
    case PTABLE_LV1:
    case PTABLE_LV2:
    case PTABLE_LV3:
        if ((arch_flags & 0b11) == 0b11) {
            return PTABLE_ENTITY_PRESENT;
        } else {
            return PTABLE_ENTITY_INVALID;
        }
    }

    return PTABLE_ENTITY_INVALID;
}

void vm_ptable_entity_set_default_flags(ptable_entity_t* entity, ptable_lv_t lv)
{
    switch (lv) {
    case PTABLE_LV0:
        *entity = 0x700;
        return;

    case PTABLE_LV1:
    case PTABLE_LV2:
    case PTABLE_LV3:
        *entity = 0x0;
        return;
    }
}

void vm_ptable_entity_allocated(ptable_entity_t* entity, ptable_lv_t lv)
{
    return;
}

void vm_ptable_entity_invalidate(ptable_entity_t* entity, ptable_lv_t lv)
{
    *entity = 0;
}

void vm_ptable_entity_set_mmu_flags(ptable_entity_t* entity, ptable_lv_t lv, mmu_flags_t mmu_flags)
{
    mmu_flags_t old_mmu_flags = vm_arch_to_mmu_flags(entity, lv);
    old_mmu_flags |= mmu_flags;
    ptable_entity_t arch_flags = vm_mmu_to_arch_flags(old_mmu_flags, lv);
    clear_arch_flags(entity, lv);
    *entity |= arch_flags;
}

void vm_ptable_entity_rm_mmu_flags(ptable_entity_t* entity, ptable_lv_t lv, mmu_flags_t mmu_flags)
{
    mmu_flags_t old_mmu_flags = vm_arch_to_mmu_flags(entity, lv);
    old_mmu_flags &= ~mmu_flags;
    ptable_entity_t arch_flags = vm_mmu_to_arch_flags(old_mmu_flags, lv);
    clear_arch_flags(entity, lv);
    *entity |= arch_flags;
}

void vm_ptable_entity_set_frame(ptable_entity_t* entity, ptable_lv_t lv, uintptr_t frame)
{
    // TODO(aarch64): This is set for 4kb pages.
    const int frame_offset = 12;

    switch (lv) {
    case PTABLE_LV0:
        *entity &= ((1 << (frame_offset)) - 1);
        frame >>= frame_offset;
        *entity |= (frame << frame_offset);
        return;

    case PTABLE_LV1:
    case PTABLE_LV2:
    case PTABLE_LV3:
        *entity &= ((1 << (frame_offset)) - 1);
        frame >>= frame_offset;
        *entity |= (frame << frame_offset);
        return;
    }
}

uintptr_t vm_ptable_entity_get_frame(ptable_entity_t* entity, ptable_lv_t lv)
{
    // TODO(aarch64): This is set for 4kb pages.
    const int frame_offset = 12;

    switch (lv) {
    case PTABLE_LV0:
    case PTABLE_LV1:
    case PTABLE_LV2:
    case PTABLE_LV3:
        return ((*entity >> frame_offset) << frame_offset);
    }

    return 0;
}

bool vm_ptable_entity_is_present(ptable_entity_t* entity, ptable_lv_t lv)
{
    if (!entity) {
        return false;
    }
    return vm_ptable_entity_state(entity, lv) == PTABLE_ENTITY_PRESENT;
}

bool vm_ptable_entity_is_only_allocated(ptable_entity_t* entity, ptable_lv_t lv)
{
    // TODO(aarch64): Mark this somehow.
    // This is for 32bit systems only.
    return false;
}
