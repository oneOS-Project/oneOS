/*
 * Copyright (C) 2020-2022 The opuntiaOS Project Authors.
 *  + Contributed by Nikita Melekhin <nimelehin@gmail.com>
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fs/vfs.h>
#include <libkern/bits/errno.h>
#include <libkern/libkern.h>
#include <libkern/lock.h>
#include <libkern/log.h>
#include <mem/kmalloc.h>
#include <tasking/tasking.h>
#include <time/time_manager.h>

#define MAX_BLOCK_LEN 1024

#define DENTRY_FSDATA(dentry) ((ext2_fsdata_t*)dentry->vfsdev->fsdata)
#define VFSDEV_FSDATA(vfsdev) ((ext2_fsdata_t*)vfsdev->fsdata)

#define DENTRY_FSLOCK(dentry) dentry->vfsdev->fslock
#define VFSDEV_FSLOCK(vfsdev) vfsdev->fslock

#define VFSDEV_SUPERBLOCK(vfsdev) (VFSDEV_FSDATA(vfsdev)->sb)
#define VFSDEV_GROUP_COUNT(vfsdev) (VFSDEV_FSDATA(vfsdev)->gt->count)
#define VFSDEV_GROUP_TABLE(vfsdev, id) (VFSDEV_FSDATA(vfsdev)->gt->table[id])

#define BLOCK_LEN(sb) (1024 << (sb->log_block_size))
#define TO_EXT_BLOCKS_CNT(sb, x) (x / (2 << (sb->log_block_size)))
#define NORM_FILENAME(x) (x + ((4 - (x & 0b11)) & 0b11))

static superblock_t* _ext2_superblocks[MAX_DEVICES_COUNT];
static ext2_groups_info_t _ext2_group_table_info[MAX_DEVICES_COUNT];
static spinlock_t _ext2_lock;

driver_desc_t _ext2_driver_info();

/* DRIVE RELATED FUNCTIONS */
static void _ext2_read_from_dev(vfs_device_t* vfsdev, uint8_t* buf, uint32_t start, uint32_t len);
static void _ext2_write_to_dev(vfs_device_t* vfsdev, uint8_t* buf, uint32_t start, uint32_t len);
static void _ext2_user_read_from_dev(vfs_device_t* vfsdev, void __user* buf, uint32_t start, uint32_t len);
static void _ext2_user_write_to_dev(vfs_device_t* vfsdev, void __user* buf, uint32_t start, uint32_t len);
static uint32_t _ext2_get_disk_size(vfs_device_t* vfsdev);

/* UTILS */
static inline bool _ext2_bitmap_get(uint8_t* bitmap, uint32_t index);
static inline void _ext2_bitmap_set_bit(uint8_t* bitmap, uint32_t index);
static inline void _ext2_bitmap_unset_bit(uint8_t* bitmap, uint32_t index);

/* GROUPS FUNCTIONS */
static inline uint32_t _ext2_get_group_len(superblock_t* sb);
static inline int _ext2_get_groups_cnt(vfs_device_t* vfsdev, superblock_t* sb);

/* BLOCK FUNCTIONS */
static uint32_t _ext2_get_block_offset(superblock_t* sb, uint32_t block_index);

static uint32_t _ext2_get_block_of_inode_lev0(dentry_t* dentry, uint32_t cur_block, uint32_t inode_block_index);
static uint32_t _ext2_get_block_of_inode_lev1(dentry_t* dentry, uint32_t cur_block, uint32_t inode_block_index);
static uint32_t _ext2_get_block_of_inode_lev2(dentry_t* dentry, uint32_t cur_block, uint32_t inode_block_index);
static uint32_t _ext2_get_block_of_inode(dentry_t* dentry, uint32_t inode_block_index);

static int _ext2_set_block_of_inode_lev0(dentry_t* dentry, uint32_t cur_block, uint32_t inode_block_index, uint32_t val);
static int _ext2_set_block_of_inode_lev1(dentry_t* dentry, uint32_t cur_block, uint32_t inode_block_index, uint32_t val);
static int _ext2_set_block_of_inode_lev2(dentry_t* dentry, uint32_t cur_block, uint32_t inode_block_index, uint32_t val);
static int _ext2_set_block_of_inode(dentry_t* dentry, uint32_t inode_block_index, uint32_t val);

static int _ext2_find_free_block_index(vfs_device_t* vfsdev, uint32_t* block_index, uint32_t group_index);
static int _ext2_allocate_block_index(vfs_device_t* vfsdev, uint32_t* block_index, uint32_t pref_group);
static int _ext2_free_block_index(vfs_device_t* vfsdev, uint32_t block_index);

static int _ext2_allocate_block_for_inode(dentry_t* dentry, uint32_t pref_group, uint32_t* block_index);

/* INODE FUNCTIONS */
int ext2_read_inode(dentry_t* dentry);
int ext2_write_inode(dentry_t* dentry);
int ext2_free_inode(dentry_t* dentry);

static int _ext2_find_free_inode_index(vfs_device_t* vfsdev, uint32_t* inode_index, uint32_t group_index);
static int _ext2_allocate_inode_index(vfs_device_t* vfsdev, uint32_t* inode_index, uint32_t pref_group);
static int _ext2_free_inode_index(vfs_device_t* vfsdev, uint32_t inode_index);

/* DIR FUNCTIONS */
static int _ext2_lookup_block(vfs_device_t* vfsdev, uint32_t block_index, const char* name, uint32_t len, uint32_t* found_inode_index);
static int _ext2_get_dir_entries_count_in_block(vfs_device_t* vfsdev, uint32_t block_index);
static bool _ext2_is_dir_empty(dentry_t* dir);
static int _ext2_add_first_entry_to_dir_block(vfs_device_t* vfsdev, uint32_t block_index, dentry_t* child_dentry, const char* filename, uint32_t len);
static int _ext2_add_to_dir_block(vfs_device_t* vfsdev, uint32_t block_index, dentry_t* child_dentry, const char* filename, uint32_t len);
static int _ext2_rm_from_dir_block(vfs_device_t* vfsdev, uint32_t block_index, dentry_t* child_dentry);
static int _ext2_getdents_block(vfs_device_t* vfsdev, uint32_t block_index, void __user* buf, uint32_t len, uint32_t inner_offset, off_t* scanned_bytes);

static int _ext2_add_child(dentry_t* dir, dentry_t* child_dentry, const char* name, int len);
static int _ext2_rm_child(dentry_t* dir, dentry_t* child_dentry);
static int _ext2_setup_dir(dentry_t* dir, dentry_t* parent_dir, mode_t mode, uid_t uid, gid_t gid);

/* FILE FUNCTIONS */
static int _ext2_setup_file(dentry_t* file, mode_t mode, uid_t uid, gid_t gid);

/* API FUNTIONS */
int ext2_recognize_drive(vfs_device_t* vfsdev);
int ext2_prepare_fs(vfs_device_t* vfsdev);
int ext2_save_state(vfs_device_t* vfsdev);

int ext2_read(file_t* file, void __user* buf, size_t start, size_t len);
int ext2_write(file_t* file, void __user* buf, size_t start, size_t len);
int ext2_truncate(file_t* file, size_t len);
int ext2_lookup(const path_t* path, const char* name, size_t len, path_t* result);
int ext2_mkdir(const path_t* path, const char* name, size_t len, mode_t mode, uid_t uid, gid_t gid);
int ext2_getdirent(dentry_t* dir, off_t* offset, dirent_t* res);
int ext2_create(const path_t* path, const char* name, size_t len, mode_t mode, uid_t uid, gid_t gid);
int ext2_rm(const path_t* path);

/**
 * DRIVE RELATED FUNCTIONS
 */

static void _ext2_read_from_dev(vfs_device_t* vfsdev, uint8_t* buf, uint32_t start, uint32_t len)
{
    void (*read)(device_t* d, uint32_t s, uint8_t* r) = devman_function_handler(vfsdev->dev, DRIVER_STORAGE_READ);
    int already_read = 0;
    uint32_t sector = start / 512;
    uint32_t start_offset = start % 512;
    uint8_t tmp_buf[512];

    while (len) {
        read(vfsdev->dev, sector, tmp_buf);

        size_t to_read = min(512 - start_offset, len);
        memcpy(&buf[already_read], &tmp_buf[start_offset], to_read);

        len -= to_read;
        already_read += to_read;
        sector++;
        start_offset = 0;
    }
}

static void _ext2_write_to_dev(vfs_device_t* vfsdev, uint8_t* buf, uint32_t start, uint32_t len)
{
    void (*read)(device_t* d, uint32_t s, uint8_t* r) = devman_function_handler(vfsdev->dev, DRIVER_STORAGE_READ);
    void (*write)(device_t* d, uint32_t s, uint8_t* r, uint32_t siz) = devman_function_handler(vfsdev->dev, DRIVER_STORAGE_WRITE);
    int already_written = 0;
    uint32_t sector = start / 512;
    uint32_t start_offset = start % 512;
    uint8_t tmp_buf[512];
    while (len != 0) {
        if (start_offset != 0 || len < 512) {
            read(vfsdev->dev, sector, tmp_buf);
        }

        size_t to_write = min(512 - start_offset, len);
        memcpy(&tmp_buf[start_offset], &buf[already_written], to_write);
        write(vfsdev->dev, sector, tmp_buf, 512);
        len -= to_write;
        already_written += to_write;
        sector++;
        start_offset = 0;
    }
}

static void _ext2_umem_copy_to_user(vfs_device_t* vfsdev, void __user* dest, const void* src, size_t len)
{
    umem_copy_to_user(dest, src, len);
}

static void _ext2_umem_copy_from_user(vfs_device_t* vfsdev, void* dest, const void __user* src, size_t len)
{
    umem_copy_from_user(dest, src, len);
}

static void _ext2_user_read_from_dev(vfs_device_t* vfsdev, void __user* buf, uint32_t start, uint32_t len)
{
    void (*read)(device_t* d, uint32_t s, uint8_t* r) = devman_function_handler(vfsdev->dev, DRIVER_STORAGE_READ);
    int already_read = 0;
    uint32_t sector = start / 512;
    uint32_t start_offset = start % 512;
    uint8_t tmp_buf[512];

    while (len) {
        read(vfsdev->dev, sector, tmp_buf);

        size_t to_read = min(512 - start_offset, len);
        _ext2_umem_copy_to_user(vfsdev, buf + already_read, &tmp_buf[start_offset], to_read);

        len -= to_read;
        already_read += to_read;
        sector++;
        start_offset = 0;
    }
}

static void _ext2_user_write_to_dev(vfs_device_t* vfsdev, void __user* buf, uint32_t start, uint32_t len)
{
    void (*read)(device_t* d, uint32_t s, uint8_t* r) = devman_function_handler(vfsdev->dev, DRIVER_STORAGE_READ);
    void (*write)(device_t* d, uint32_t s, uint8_t* r, uint32_t siz) = devman_function_handler(vfsdev->dev, DRIVER_STORAGE_WRITE);
    int already_written = 0;
    uint32_t sector = start / 512;
    uint32_t start_offset = start % 512;
    uint8_t tmp_buf[512];
    while (len != 0) {
        if (start_offset != 0 || len < 512) {
            read(vfsdev->dev, sector, tmp_buf);
        }

        size_t to_write = min(512 - start_offset, len);
        _ext2_umem_copy_from_user(vfsdev, &tmp_buf[start_offset], buf + already_written, to_write);

        write(vfsdev->dev, sector, tmp_buf, 512);
        len -= to_write;
        already_written += to_write;
        sector++;
        start_offset = 0;
    }
}

static uint32_t _ext2_get_disk_size(vfs_device_t* vfsdev)
{
    uint32_t (*get_size)(device_t* d) = devman_function_handler(vfsdev->dev, DRIVER_STORAGE_CAPACITY);
    return get_size(vfsdev->dev);
}

/**
 * UTILS
 */

static inline bool _ext2_bitmap_get(uint8_t* bitmap, uint32_t index)
{
    return (bitmap[index / 8] >> (index % 8)) & 1;
}

static inline void _ext2_bitmap_set_bit(uint8_t* bitmap, uint32_t index)
{
    bitmap[index / 8] |= (uint8_t)(1 << (index % 8));
}

static inline void _ext2_bitmap_unset_bit(uint8_t* bitmap, uint32_t index)
{
    bitmap[index / 8] &= ~(1 << (index % 8));
}

/**
 * GROUPS FUNCTIONS
 */

static inline uint32_t _ext2_get_group_len(superblock_t* sb)
{
    return BLOCK_LEN(sb) * BLOCK_LEN(sb) * 8;
}

static inline int _ext2_get_groups_cnt(vfs_device_t* vfsdev, superblock_t* sb)
{
    uint32_t sz = _ext2_get_disk_size(vfsdev) - SUPERBLOCK_START;
    uint32_t ans = sz / _ext2_get_group_len(sb);
    /* TODO: Work with the last smaller group. */
    // if (sz % get_group_len()) {
    //     ans++;
    // }
    return ans;
}

/**
 * BLOCK FUNCTIONS
 */

static uint32_t _ext2_get_block_offset(superblock_t* sb, uint32_t block_index)
{
    return SUPERBLOCK_START + (block_index - 1) * BLOCK_LEN(sb);
}

static uint32_t _ext2_get_block_of_inode_lev0(dentry_t* dentry, uint32_t cur_block, uint32_t inode_block_index)
{
    uint32_t offset = inode_block_index;
    uint32_t res;
    _ext2_read_from_dev(dentry->vfsdev, (uint8_t*)&res, _ext2_get_block_offset(DENTRY_FSDATA(dentry)->sb, cur_block) + offset * 4, 4);
    return res;
}

static uint32_t _ext2_get_block_of_inode_lev1(dentry_t* dentry, uint32_t cur_block, uint32_t inode_block_index)
{
    uint32_t lev_contain = BLOCK_LEN(DENTRY_FSDATA(dentry)->sb) / 4;
    uint32_t offset = inode_block_index / lev_contain;
    uint32_t offset_inner = inode_block_index % lev_contain;
    uint32_t res;
    _ext2_read_from_dev(dentry->vfsdev, (uint8_t*)&res, _ext2_get_block_offset(DENTRY_FSDATA(dentry)->sb, cur_block) + offset * 4, 4);
    return res ? _ext2_get_block_of_inode_lev0(dentry, res, offset_inner) : 0;
}

static uint32_t _ext2_get_block_of_inode_lev2(dentry_t* dentry, uint32_t cur_block, uint32_t inode_block_index)
{
    uint32_t block_len = BLOCK_LEN(DENTRY_FSDATA(dentry)->sb) / 4;
    uint32_t lev_contain = block_len * block_len;
    uint32_t offset = inode_block_index / lev_contain;
    uint32_t offset_inner = inode_block_index % lev_contain;
    uint32_t res;
    _ext2_read_from_dev(dentry->vfsdev, (uint8_t*)&res, _ext2_get_block_offset(DENTRY_FSDATA(dentry)->sb, cur_block) + offset * 4, 4);
    return res ? _ext2_get_block_of_inode_lev1(dentry, res, offset_inner) : 0;
}

/* FIXME: think of more effecient way */
static uint32_t _ext2_get_block_of_inode(dentry_t* dentry, uint32_t inode_block_index)
{
    uint32_t block_len = BLOCK_LEN(DENTRY_FSDATA(dentry)->sb) / 4;
    if (inode_block_index < 12) {
        return dentry->inode->block[inode_block_index];
    }
    if (inode_block_index < 12 + block_len) { // single indirect
        return _ext2_get_block_of_inode_lev0(dentry, dentry->inode->block[12], inode_block_index - 12);
    }
    if (inode_block_index < 12 + block_len + block_len * block_len) { // double indirect
        return _ext2_get_block_of_inode_lev1(dentry, dentry->inode->block[13], inode_block_index - 12 - block_len);
    } // triple indirect
    return _ext2_get_block_of_inode_lev2(dentry, dentry->inode->block[14], inode_block_index - (12 + block_len + block_len * block_len));
}

static int _ext2_set_block_of_inode_lev0(dentry_t* dentry, uint32_t cur_block, uint32_t inode_block_index, uint32_t val)
{
    uint32_t offset = inode_block_index;
    _ext2_write_to_dev(dentry->vfsdev, (uint8_t*)&val, _ext2_get_block_offset(DENTRY_FSDATA(dentry)->sb, cur_block) + offset * 4, 4);
    return 0;
}

static int _ext2_set_block_of_inode_lev1(dentry_t* dentry, uint32_t cur_block, uint32_t inode_block_index, uint32_t val)
{
    uint32_t lev_contain = BLOCK_LEN(DENTRY_FSDATA(dentry)->sb) / 4;
    uint32_t offset = inode_block_index / lev_contain;
    uint32_t offset_inner = inode_block_index % lev_contain;
    uint32_t res;
    _ext2_read_from_dev(dentry->vfsdev, (uint8_t*)&res, _ext2_get_block_offset(DENTRY_FSDATA(dentry)->sb, cur_block) + offset * 4, 4);
    if (!res) {
        int err = _ext2_allocate_block_index(dentry->vfsdev, &res, 0);
        if (err) {
            return err;
        }
        _ext2_write_to_dev(dentry->vfsdev, (uint8_t*)&res, _ext2_get_block_offset(DENTRY_FSDATA(dentry)->sb, cur_block) + offset * 4, 4);
    }

    return res ? _ext2_set_block_of_inode_lev0(dentry, res, offset_inner, val) : -1;
}

static int _ext2_set_block_of_inode_lev2(dentry_t* dentry, uint32_t cur_block, uint32_t inode_block_index, uint32_t val)
{
    uint32_t block_len = BLOCK_LEN(DENTRY_FSDATA(dentry)->sb) / 4;
    uint32_t lev_contain = block_len * block_len;
    uint32_t offset = inode_block_index / lev_contain;
    uint32_t offset_inner = inode_block_index % lev_contain;
    uint32_t res;
    _ext2_read_from_dev(dentry->vfsdev, (uint8_t*)&res, _ext2_get_block_offset(DENTRY_FSDATA(dentry)->sb, cur_block) + offset * 4, 4);
    if (!res) {
        int err = _ext2_allocate_block_index(dentry->vfsdev, &res, 0);
        if (err) {
            return err;
        }
        _ext2_write_to_dev(dentry->vfsdev, (uint8_t*)&res, _ext2_get_block_offset(DENTRY_FSDATA(dentry)->sb, cur_block) + offset * 4, 4);
    }

    return res ? _ext2_set_block_of_inode_lev1(dentry, res, offset_inner, val) : -1;
}

/**
 * @note Both dentry and fslock should be acquired.
 */
int _ext2_set_block_of_inode(dentry_t* dentry, uint32_t inode_block_index, uint32_t val)
{
    uint32_t block_len = BLOCK_LEN(DENTRY_FSDATA(dentry)->sb) / 4;
    if (inode_block_index < 12) {
        dentry->inode->block[inode_block_index] = val;
        dentry_set_flag_locked(dentry, DENTRY_DIRTY);
        return 0;
    }
    if (inode_block_index < 12 + block_len) { // single indirect
        if (!dentry->inode->block[12]) {
            int err = _ext2_allocate_block_index(dentry->vfsdev, &dentry->inode->block[12], 0);
            if (err) {
                return err;
            }
        }
        return _ext2_set_block_of_inode_lev0(dentry, dentry->inode->block[12], inode_block_index - 12, val);
    }
    if (inode_block_index < 12 + block_len + block_len * block_len) { // double indirect
        if (!dentry->inode->block[13]) {
            int err = _ext2_allocate_block_index(dentry->vfsdev, &dentry->inode->block[13], 0);
            if (err) {
                return err;
            }
        }
        return _ext2_set_block_of_inode_lev1(dentry, dentry->inode->block[13], inode_block_index - 12 - block_len, val);
    }

    if (!dentry->inode->block[14]) {
        int err = _ext2_allocate_block_index(dentry->vfsdev, &dentry->inode->block[14], 0);
        if (err) {
            return err;
        }
    }
    return _ext2_set_block_of_inode_lev2(dentry, dentry->inode->block[14], inode_block_index - (12 + block_len + block_len * block_len), val);
}

/**
 * @note Both dentry and fslock should be acquired.
 */
static int _ext2_find_free_block_index(vfs_device_t* vfsdev, uint32_t* block_index, uint32_t group_index)
{
    uint8_t block_bitmap[MAX_BLOCK_LEN];
    ext2_fsdata_t* fsdata = VFSDEV_FSDATA(vfsdev);

    uint32_t blockoff = _ext2_get_block_offset(fsdata->sb, fsdata->gt->table[group_index].block_bitmap);
    uint32_t blocklen = BLOCK_LEN(fsdata->sb);
    _ext2_read_from_dev(vfsdev, block_bitmap, blockoff, blocklen);

    for (uint32_t off = 0; off < 8 * blocklen; off++) {
        if (!_ext2_bitmap_get(block_bitmap, off)) {
            *block_index = fsdata->sb->blocks_per_group * group_index + off + 1;
            _ext2_bitmap_set_bit(block_bitmap, off);
            _ext2_write_to_dev(vfsdev, block_bitmap, blockoff, blocklen);
            return 0;
        }
    }
    return -ENOSPC;
}

/**
 * @note Both dentry and fslock should be acquired.
 */
static int _ext2_allocate_block_index(vfs_device_t* vfsdev, uint32_t* block_index, uint32_t pref_group)
{
    uint32_t groups_cnt = VFSDEV_GROUP_COUNT(vfsdev);
    for (int i = 0; i < groups_cnt; i++) {
        uint32_t group_id = (pref_group + i) % groups_cnt;
        if (VFSDEV_GROUP_TABLE(vfsdev, group_id).free_blocks_count) {
            if (_ext2_find_free_block_index(vfsdev, block_index, group_id) == 0) {
                VFSDEV_GROUP_TABLE(vfsdev, group_id).free_blocks_count--;
                return 0;
            }
        }
    }
    return -ENOSPC;
}

/**
 * @brief Frees block on device.
 *
 * @note Dentry lock should be acquired calling this function.
 */
static int _ext2_free_block_index(vfs_device_t* vfsdev, uint32_t block_index)
{
    spinlock_acquire(&VFSDEV_FSLOCK(vfsdev));
    ext2_fsdata_t* fsdata = VFSDEV_FSDATA(vfsdev);

    block_index--;
    uint32_t block_len = BLOCK_LEN(fsdata->sb);
    uint32_t blockes_per_group = fsdata->sb->blocks_per_group;
    uint32_t group_index = block_index / blockes_per_group;
    uint32_t off = block_index % block_len;
    uint32_t block_off = _ext2_get_block_offset(fsdata->sb, fsdata->gt->table[group_index].block_bitmap);

    uint8_t block_bitmap[MAX_BLOCK_LEN];
    _ext2_read_from_dev(vfsdev, block_bitmap, block_off, block_len);

    _ext2_bitmap_unset_bit(block_bitmap, off);
    _ext2_write_to_dev(vfsdev, block_bitmap, block_off, block_len);
    VFSDEV_GROUP_TABLE(vfsdev, group_index).free_blocks_count++;

    spinlock_release(&VFSDEV_FSLOCK(vfsdev));
    return 0;
}

/**
 * @brief Allocated block on device.
 *
 * @note Dentry lock should be acquired calling this function.
 */
static int _ext2_allocate_block_for_inode(dentry_t* dentry, uint32_t pref_group, uint32_t* block_index)
{
    spinlock_acquire(&DENTRY_FSLOCK(dentry));

    if (_ext2_allocate_block_index(dentry->vfsdev, block_index, pref_group) == 0) {
        uint32_t blocks_per_inode = TO_EXT_BLOCKS_CNT(DENTRY_FSDATA(dentry)->sb, dentry->inode->blocks);
        if (_ext2_set_block_of_inode(dentry, blocks_per_inode, *block_index) == 0) {
            dentry->inode->blocks += BLOCK_LEN(DENTRY_FSDATA(dentry)->sb) / 512;
            dentry_set_flag_locked(dentry, DENTRY_DIRTY);
            spinlock_release(&DENTRY_FSLOCK(dentry));
            return 0;
        }
    }

    spinlock_release(&DENTRY_FSLOCK(dentry));
    return -ENOSPC;
}

/**
 * INODE FUNCTIONS
 */

int ext2_read_inode(dentry_t* dentry)
{
    uint32_t inodes_per_group = DENTRY_FSDATA(dentry)->sb->inodes_per_group;
    uint32_t holder_group = (dentry->inode_indx - 1) / inodes_per_group;
    uint32_t pos_inside_group = (dentry->inode_indx - 1) % inodes_per_group;
    uint32_t inode_start = _ext2_get_block_offset(DENTRY_FSDATA(dentry)->sb, DENTRY_FSDATA(dentry)->gt->table[holder_group].inode_table) + (pos_inside_group * INODE_LEN);
    _ext2_read_from_dev(dentry->vfsdev, (uint8_t*)dentry->inode, inode_start, INODE_LEN);
    return 0;
}

int ext2_write_inode(dentry_t* dentry)
{
    uint32_t inodes_per_group = DENTRY_FSDATA(dentry)->sb->inodes_per_group;
    uint32_t holder_group = (dentry->inode_indx - 1) / inodes_per_group;
    uint32_t pos_inside_group = (dentry->inode_indx - 1) % inodes_per_group;
    uint32_t inode_start = _ext2_get_block_offset(DENTRY_FSDATA(dentry)->sb, DENTRY_FSDATA(dentry)->gt->table[holder_group].inode_table) + (pos_inside_group * INODE_LEN);
    _ext2_write_to_dev(dentry->vfsdev, (uint8_t*)dentry->inode, inode_start, INODE_LEN);
    return 0;
}

/**
 * @note Both dentry and fslock should be acquired.
 */
static int _ext2_find_free_inode_index(vfs_device_t* vfsdev, uint32_t* inode_index, uint32_t group_index)
{
    uint8_t inode_bitmap[MAX_BLOCK_LEN];
    ext2_fsdata_t* fsdata = VFSDEV_FSDATA(vfsdev);

    uint32_t blockoff = _ext2_get_block_offset(fsdata->sb, fsdata->gt->table[group_index].inode_bitmap);
    uint32_t blocklen = BLOCK_LEN(fsdata->sb);
    _ext2_read_from_dev(vfsdev, inode_bitmap, blockoff, blocklen);

    for (uint32_t off = 0; off < 8 * blocklen; off++) {
        if (!_ext2_bitmap_get(inode_bitmap, off)) {
            *inode_index = fsdata->sb->inodes_per_group * group_index + off + 1;
            _ext2_bitmap_set_bit(inode_bitmap, off);
            _ext2_write_to_dev(vfsdev, inode_bitmap, blockoff, blocklen);
            return 0;
        }
    }
    return -ENOSPC;
}

/**
 * @note Dentry lock should be acquired calling this function.
 */
static int _ext2_allocate_inode_index(vfs_device_t* vfsdev, uint32_t* inode_index, uint32_t pref_group)
{
    spinlock_acquire(&VFSDEV_FSLOCK(vfsdev));

    uint32_t groups_cnt = VFSDEV_GROUP_COUNT(vfsdev);
    for (int i = 0; i < groups_cnt; i++) {
        uint32_t group_id = (pref_group + i) % groups_cnt;
        if (VFSDEV_FSDATA(vfsdev)->gt->table[group_id].free_inodes_count) {
            if (_ext2_find_free_inode_index(vfsdev, inode_index, group_id) == 0) {
                spinlock_release(&VFSDEV_FSLOCK(vfsdev));
                return 0;
            }
        }
    }

    spinlock_release(&VFSDEV_FSLOCK(vfsdev));
    return -ENOSPC;
}

/**
 * @note Dentry lock should be acquired calling this function.
 */
static int _ext2_free_inode_index(vfs_device_t* vfsdev, uint32_t inode_index)
{
    spinlock_acquire(&VFSDEV_FSLOCK(vfsdev));
    ext2_fsdata_t* fsdata = VFSDEV_FSDATA(vfsdev);

    inode_index--;
    uint32_t block_len = BLOCK_LEN(fsdata->sb);
    uint32_t inodes_per_group = fsdata->sb->inodes_per_group;
    uint32_t group_index = inode_index / inodes_per_group;
    uint32_t off = inode_index % inodes_per_group;
    uint32_t block_off = _ext2_get_block_offset(fsdata->sb, fsdata->gt->table[group_index].inode_bitmap);

    uint8_t inode_bitmap[MAX_BLOCK_LEN];
    _ext2_read_from_dev(vfsdev, inode_bitmap, block_off, block_len);

    _ext2_bitmap_unset_bit(inode_bitmap, off);
    _ext2_write_to_dev(vfsdev, inode_bitmap, block_off, block_len);
    VFSDEV_GROUP_TABLE(vfsdev, group_index).free_inodes_count++;

    spinlock_release(&VFSDEV_FSLOCK(vfsdev));
    return 0;
}

/**
 * @note Dentry lock should be acquired calling this function.
 */
int ext2_free_inode(dentry_t* dentry)
{
    ASSERT(dentry->d_count == 0 && dentry->inode->links_count == 0);
    uint32_t block_per_dir = TO_EXT_BLOCKS_CNT(DENTRY_FSDATA(dentry)->sb, dentry->inode->blocks);

    /* freeing all data blocks */
    for (int block_index = 0; block_index < block_per_dir; block_index++) {
        uint32_t data_block_index = _ext2_get_block_of_inode(dentry, block_index);
        _ext2_free_block_index(dentry->vfsdev, data_block_index);
    }

    // Zeroing up the whole inode before freeing it to be safe of potential
    // security problems.
    memset(dentry->inode, 0, sizeof(inode_t));
    ext2_write_inode(dentry);

    _ext2_free_inode_index(dentry->vfsdev, dentry->inode_indx);
    return 0;
}

/**
 * DIR FUNCTIONS
 */

/**
 * @note Dentry lock should be acquired calling this function.
 * @todo Only link version is supported.
 */
static int _ext2_lookup_block(vfs_device_t* vfsdev, uint32_t block_index, const char* name, uint32_t len, uint32_t* found_inode_index)
{
    ext2_fsdata_t* fsdata = VFSDEV_FSDATA(vfsdev);
    uint32_t blocklen = BLOCK_LEN(fsdata->sb);

    if (block_index == 0) {
        return -EINVAL;
    }

    uint8_t tmp_buf[MAX_BLOCK_LEN];
    _ext2_read_from_dev(vfsdev, tmp_buf, _ext2_get_block_offset(fsdata->sb, block_index), blocklen);
    dir_entry_t* start_of_entry = (dir_entry_t*)tmp_buf;
    for (;;) {
        if (start_of_entry->inode == 0) {
            return -EFAULT;
        }

        if (start_of_entry->name_len == len) {
            bool is_name_same = true;
            for (int i = 0; i < start_of_entry->name_len; i++) {
                is_name_same &= (name[i] == *((char*)start_of_entry + 8 + i));
            }

            if (is_name_same) {
                *found_inode_index = start_of_entry->inode;
                return 0;
            }
        }

        start_of_entry = (dir_entry_t*)((uintptr_t)start_of_entry + start_of_entry->rec_len);
        if ((uintptr_t)start_of_entry >= (uintptr_t)tmp_buf + blocklen) {
            return -EFAULT;
        }
    }
    return -EFAULT;
}

static int _ext2_get_dir_entries_count_in_block(vfs_device_t* vfsdev, uint32_t block_index)
{
    ASSERT(block_index != 0);
    uint8_t tmp_buf[MAX_BLOCK_LEN];
    ext2_fsdata_t* fsdata = VFSDEV_FSDATA(vfsdev);

    const uint32_t block_len = BLOCK_LEN(fsdata->sb);
    uint32_t internal_offset = 0;
    int result = 0;

    _ext2_read_from_dev(vfsdev, tmp_buf, _ext2_get_block_offset(fsdata->sb, block_index), block_len);
    for (;;) {
        dir_entry_t* start_of_entry = (dir_entry_t*)((uintptr_t)tmp_buf + internal_offset);
        internal_offset += start_of_entry->rec_len;

        if (start_of_entry->inode != 0) {
            result++;
        }

        if (internal_offset >= block_len) {
            return result;
        }
    }
    return result;
}

static bool _ext2_is_dir_empty(dentry_t* dir)
{
    const uint32_t block_len = BLOCK_LEN(DENTRY_FSDATA(dir)->sb);
    uint32_t end_block_index = TO_EXT_BLOCKS_CNT(DENTRY_FSDATA(dir)->sb, dir->inode->blocks);
    int result = 0;

    for (uint32_t block_index = 0; block_index < end_block_index; block_index++) {
        uint32_t data_block_index = _ext2_get_block_of_inode(dir, block_index);
        result += _ext2_get_dir_entries_count_in_block(dir->vfsdev, data_block_index);

        // At least 3 elemetns are required (including . and .. which should be ignored).
        if (result > 2) {
            return false;
        }
    }

    return true;
}

/**
 * _ext2_getdents_block puts dents into @buf from block with index @block_index.
 * @block_index: index block to put from
 * @buf: result buffer
 * @len: len of the buffer @buf
 * @inner_offset: offset of block, shows the start from where to start
 * @scanned_bytes: keeps the result value how many bytes were scanned in this block.
 * return: returns an error (if return < 0) or count of bytes written to @buf.
 */
static int _ext2_getdents_block(vfs_device_t* vfsdev, uint32_t block_index, void __user* buf, uint32_t len, uint32_t inner_offset, off_t* scanned_bytes)
{
    ext2_fsdata_t* fsdata = VFSDEV_FSDATA(vfsdev);

    if (block_index == 0) {
        return -EINVAL;
    }
    const uint32_t block_len = BLOCK_LEN(fsdata->sb);
    int already_read = 0;

    uint8_t tmp_buf[MAX_BLOCK_LEN];
    _ext2_read_from_dev(vfsdev, tmp_buf, _ext2_get_block_offset(fsdata->sb, block_index), block_len);
    for (;;) {
        dir_entry_t* start_of_entry = (dir_entry_t*)((uintptr_t)tmp_buf + inner_offset);
        uint32_t record_name_len = NORM_FILENAME(start_of_entry->name_len);
        uint32_t real_rec_len = 8 + record_name_len + 1;

        if (real_rec_len > len) {
            /*  There is no space to put this element in the buffer.
                If we havn't written anything, that means we can't fit
                the elem in the buf, so, return an error */
            if (already_read == 0) {
                return -EINVAL;
            }
            return already_read;
        }

        inner_offset += start_of_entry->rec_len;
        *scanned_bytes += start_of_entry->rec_len;
        if (start_of_entry->inode != 0) {
            // Change it here, to have the correct copied data.
            start_of_entry->rec_len = real_rec_len;

            _ext2_umem_copy_to_user(vfsdev, buf + already_read, (void*)start_of_entry, real_rec_len);
            char __user* cbuf = (char __user*)buf;
            umem_put_user('\0', &cbuf[already_read + real_rec_len - 1]);

            already_read += real_rec_len;
            len -= real_rec_len;
        }

        if (inner_offset >= block_len) {
            return already_read;
        }
    }
    return already_read;
}

/**
 * @note Both dentry and fslock should be acquired.
 */
static int _ext2_add_first_entry_to_dir_block(vfs_device_t* vfsdev, uint32_t block_index, dentry_t* child_dentry, const char* filename, uint32_t len)
{
    ext2_fsdata_t* fsdata = VFSDEV_FSDATA(vfsdev);

    if (block_index == 0) {
        return -EINVAL;
    }

    uint32_t blockoff = _ext2_get_block_offset(fsdata->sb, block_index);
    uint32_t record_name_len = NORM_FILENAME(len);
    uint32_t min_rec_len = 8 + record_name_len;
    dir_entry_t new_entry;

    uint8_t tmp_buf[DIR_ENTRY_LEN];
    _ext2_read_from_dev(vfsdev, tmp_buf, blockoff, DIR_ENTRY_LEN);
    dir_entry_t* start_of_entry = (dir_entry_t*)tmp_buf;
    new_entry.inode = child_dentry->inode_indx;
    new_entry.rec_len = BLOCK_LEN(fsdata->sb);
    new_entry.name_len = len;
    memcpy((void*)start_of_entry, (void*)&new_entry, 8);
    memcpy((void*)((uintptr_t)start_of_entry + 8), (void*)filename, len);
    memset((void*)((uintptr_t)start_of_entry + 8 + len), 0, record_name_len - len);
    _ext2_write_to_dev(vfsdev, tmp_buf, blockoff, DIR_ENTRY_LEN);
    return 0;
}

/**
 * @note Both dentry and fslock should be acquired.
 */
static int _ext2_add_to_dir_block(vfs_device_t* vfsdev, uint32_t block_index, dentry_t* child_dentry, const char* filename, uint32_t len)
{
    ext2_fsdata_t* fsdata = VFSDEV_FSDATA(vfsdev);

    if (block_index == 0) {
        return -EINVAL;
    }

    uint32_t blockoff = _ext2_get_block_offset(fsdata->sb, block_index);
    uint32_t blocklen = BLOCK_LEN(fsdata->sb);

    uint32_t record_name_len = NORM_FILENAME(len);
    uint32_t min_rec_len = 8 + record_name_len;
    dir_entry_t new_entry;

    uint8_t tmp_buf[MAX_BLOCK_LEN];
    _ext2_read_from_dev(vfsdev, tmp_buf, blockoff, blocklen);
    dir_entry_t* start_of_entry = (dir_entry_t*)tmp_buf;
    dir_entry_t* start_of_new_entry;

    if (start_of_entry->inode == 0) {
        kpanic("Ext2: can't add as first entry with help of that function.");
    }

    for (;;) {
        uint32_t cur_filename_len = NORM_FILENAME(start_of_entry->name_len);
        uint32_t cur_rec_len = 8 + cur_filename_len;

        // We have enough place to put both records
        if (start_of_entry->rec_len >= cur_rec_len + min_rec_len) {
            new_entry.inode = child_dentry->inode_indx;
            new_entry.rec_len = start_of_entry->rec_len - cur_rec_len;
            new_entry.name_len = len;
            start_of_new_entry = (dir_entry_t*)((uintptr_t)start_of_entry + cur_rec_len);
            start_of_entry->rec_len = cur_rec_len;
            goto update_res;
        }

        start_of_entry = (dir_entry_t*)((uintptr_t)start_of_entry + start_of_entry->rec_len);
        if ((uintptr_t)start_of_entry >= (uintptr_t)tmp_buf + blocklen) {
            return -EFAULT;
        }
    }

update_res:
    memcpy((void*)start_of_new_entry, (void*)&new_entry, 8);
    memcpy((void*)((uintptr_t)start_of_new_entry + 8), (void*)filename, len);
    memset((void*)((uintptr_t)start_of_new_entry + 8 + len), 0, record_name_len - len);
    _ext2_write_to_dev(vfsdev, tmp_buf, blockoff, blocklen);
    return 0;
}

static int _ext2_rm_from_dir_block(vfs_device_t* vfsdev, uint32_t block_index, dentry_t* child_dentry)
{
    ext2_fsdata_t* fsdata = VFSDEV_FSDATA(vfsdev);

    if (block_index == 0) {
        return -EINVAL;
    }

    uint32_t blockoff = _ext2_get_block_offset(fsdata->sb, block_index);
    uint32_t blocklen = BLOCK_LEN(fsdata->sb);

    uint8_t tmp_buf[MAX_BLOCK_LEN];
    _ext2_read_from_dev(vfsdev, tmp_buf, blockoff, blocklen);
    dir_entry_t* start_of_entry = (dir_entry_t*)tmp_buf;
    dir_entry_t* prev_entry = (dir_entry_t*)0;

    for (;;) {
        if (start_of_entry->inode == child_dentry->inode_indx) {
            // TODO: Fix and remove the following if.
            if (!prev_entry) {
                kpanic("Ext2: can't delete first entry!");
            }

            start_of_entry->inode = 0;
            prev_entry->rec_len += start_of_entry->rec_len;

            _ext2_write_to_dev(vfsdev, tmp_buf, blockoff, blocklen);
            return 0;
        }

        prev_entry = start_of_entry;
        start_of_entry = (dir_entry_t*)((uintptr_t)start_of_entry + start_of_entry->rec_len);
        if ((uintptr_t)start_of_entry >= (uintptr_t)tmp_buf + blocklen) {
            return -EFAULT;
        }
    }
}

/**
 * @note Both dentries should be acquired.
 */
static int _ext2_add_child(dentry_t* dir, dentry_t* child_dentry, const char* name, int len)
{
    uint32_t block_index;
    uint32_t blocks_per_dir = TO_EXT_BLOCKS_CNT(DENTRY_FSDATA(dir)->sb, dir->inode->blocks);

    for (int i = 0; i < blocks_per_dir; i++) {
        if ((block_index = _ext2_get_block_of_inode(dir, i))) {
            if (_ext2_add_to_dir_block(dir->vfsdev, block_index, child_dentry, name, len) == 0) {
                goto updated_inode;
            }
        }
    }

    // FIXME: group
    uint32_t new_block_index;
    if (_ext2_allocate_block_for_inode(dir, 0, &new_block_index) == 0) {
        if (_ext2_add_first_entry_to_dir_block(dir->vfsdev, new_block_index, child_dentry, name, len) == 0) {
            goto updated_inode;
        }
    }

    return -EFAULT;

updated_inode:
    child_dentry->inode->links_count++;
    dentry_set_flag_locked(child_dentry, DENTRY_DIRTY);
    return 0;
}

/**
 * @note Both dentries should be acquired.
 */
static int _ext2_rm_child(dentry_t* dir, dentry_t* child_dentry)
{
    uint32_t block_index;
    uint32_t blocks_per_dir = TO_EXT_BLOCKS_CNT(DENTRY_FSDATA(dir)->sb, dir->inode->blocks);

    for (int i = 0; i < blocks_per_dir; i++) {
        if ((block_index = _ext2_get_block_of_inode(dir, i))) {
            if (_ext2_rm_from_dir_block(dir->vfsdev, block_index, child_dentry) == 0) {
                child_dentry->inode->links_count--;
                dentry_set_flag_locked(child_dentry, DENTRY_DIRTY);
                return 0;
            }
        }
    }

    return -ENOENT;
}

/**
 * @note Both dentries should be acquired.
 */
static int _ext2_setup_dir(dentry_t* dir, dentry_t* parent_dir, mode_t mode, uid_t uid, gid_t gid)
{
    dir->inode->mode = mode;
    dir->inode->uid = uid;
    dir->inode->gid = gid;
    dir->inode->links_count = 0;
    dir->inode->blocks = 0;
    dentry_set_flag_locked(dir, DENTRY_DIRTY);
    if (_ext2_add_child(dir, dir, ".", 1) < 0) {
        return -EFAULT;
    }
    if (_ext2_add_child(dir, parent_dir, "..", 2) < 0) {
        return -EFAULT;
    }
    return 0;
}

/**
 * FILE FUNCTIONS
 */

/**
 * @note Both dentries should be acquired.
 */
static int _ext2_setup_file(dentry_t* file, mode_t mode, uid_t uid, gid_t gid)
{
    file->inode->mode = mode;
    file->inode->uid = uid;
    file->inode->gid = gid;
    file->inode->links_count = 0;
    file->inode->blocks = 0;
    file->inode->size = 0;
    dentry_set_flag_locked(file, DENTRY_DIRTY);
    return 0;
}

/**
 * API FUNCTIONS
 */

bool ext2_can_read(file_t* file, size_t start)
{
    return true;
}

bool ext2_can_write(file_t* file, size_t start)
{
    return true;
}

int ext2_read(file_t* file, void __user* buf, size_t start, size_t len)
{
    dentry_t* dentry = file_dentry_assert(file);

    spinlock_acquire(&dentry->lock);
    const uint32_t block_len = BLOCK_LEN(DENTRY_FSDATA(dentry)->sb);
    uint32_t blocks_allocated = TO_EXT_BLOCKS_CNT(DENTRY_FSDATA(dentry)->sb, dentry->inode->blocks);
    uint32_t start_block_index = start / block_len;
    uint32_t end_block_index = min((start + len - 1) / block_len, blocks_allocated - 1);

    if (start >= dentry->inode->size) {
        spinlock_release(&dentry->lock);
        return 0;
    }

    uint32_t have_to_read = min(len, dentry->inode->size - start);
    uint32_t read_offset = start % block_len;
    uint32_t already_read = 0;

    for (uint32_t virt_block_index = start_block_index; virt_block_index <= end_block_index; virt_block_index++) {
        uint32_t data_block_index = _ext2_get_block_of_inode(dentry, virt_block_index);
        uint32_t read_from_block = min(have_to_read, block_len - read_offset);
        _ext2_user_read_from_dev(dentry->vfsdev, buf + already_read, _ext2_get_block_offset(DENTRY_FSDATA(dentry)->sb, data_block_index) + read_offset, read_from_block);
        have_to_read -= read_from_block;
        already_read += read_from_block;
        read_offset = 0;
    }

    spinlock_release(&dentry->lock);
    return already_read;
}

int ext2_write(file_t* file, void __user* buf, size_t start, size_t len)
{
    dentry_t* dentry = file_dentry_assert(file);

    spinlock_acquire(&dentry->lock);
    const uint32_t block_len = BLOCK_LEN(DENTRY_FSDATA(dentry)->sb);
    uint32_t start_block_index = start / block_len;
    uint32_t end_block_index = (start + len) / block_len;
    uint32_t write_offset = start % block_len;
    uint32_t to_write = len;
    uint32_t already_written = 0;
    uint32_t blocks_allocated = TO_EXT_BLOCKS_CNT(DENTRY_FSDATA(dentry)->sb, dentry->inode->blocks);

    for (uint32_t data_block_index, virt_block_index = start_block_index; virt_block_index <= end_block_index; virt_block_index++) {
        uint32_t write_to_block = min(to_write, block_len - write_offset);

        if (blocks_allocated <= virt_block_index) {
            int err = _ext2_allocate_block_for_inode(dentry, 0, &data_block_index);
            if (err) {
                return err;
            }
        } else {
            data_block_index = _ext2_get_block_of_inode(dentry, virt_block_index);
        }

        _ext2_user_write_to_dev(dentry->vfsdev, buf + already_written, _ext2_get_block_offset(DENTRY_FSDATA(dentry)->sb, data_block_index) + write_offset, write_to_block);
        to_write -= write_to_block;
        already_written += write_to_block;
        write_offset = 0;
    }

    if (dentry->inode->size < start + len) {
        dentry->inode->size = start + len;
    }
    dentry->inode->mtime = (uint32_t)timeman_seconds_since_epoch();
    dentry_set_flag_locked(dentry, DENTRY_DIRTY);

    spinlock_release(&dentry->lock);
    return already_written;
}

int ext2_truncate(file_t* file, size_t len)
{
    dentry_t* dentry = file_dentry_assert(file);

    spinlock_acquire(&dentry->lock);
    if (dentry->inode->size <= len) {
        spinlock_release(&dentry->lock);
        return 0;
    }

    const uint32_t block_len = BLOCK_LEN(DENTRY_FSDATA(dentry)->sb);
    const uint32_t last_written_byte = len - 1;
    uint32_t last_written_block_index = last_written_byte / block_len;
    uint32_t start_block_index = last_written_block_index + 1;
    uint32_t blocks_allocated = TO_EXT_BLOCKS_CNT(DENTRY_FSDATA(dentry)->sb, dentry->inode->blocks);

    for (uint32_t block_index, virt_block_index = start_block_index; virt_block_index < blocks_allocated; virt_block_index++) {
        block_index = _ext2_get_block_of_inode(dentry, virt_block_index);
        _ext2_free_block_index(dentry->vfsdev, block_index);
    }

    dentry->inode->size = len;
    dentry->inode->mtime = (uint32_t)timeman_seconds_since_epoch();
    dentry_set_flag_locked(dentry, DENTRY_DIRTY);
    spinlock_release(&dentry->lock);
    return 0;
}

int ext2_lookup(const path_t* path, const char* name, size_t len, path_t* result)
{
    dentry_t* dir = path->dentry;
    spinlock_acquire(&dir->lock);

    uint32_t block_per_dir = TO_EXT_BLOCKS_CNT(DENTRY_FSDATA(dir)->sb, dir->inode->blocks);
    for (int block_index = 0; block_index < block_per_dir; block_index++) {
        uint32_t data_block_index = _ext2_get_block_of_inode(dir, block_index);
        uint32_t res_inode_indx = 0;
        if (_ext2_lookup_block(dir->vfsdev, data_block_index, name, len, &res_inode_indx) == 0) {
            result->dentry = dentry_get(dir->dev_indx, res_inode_indx);
            spinlock_release(&dir->lock);
            return 0;
        }
    }

    spinlock_release(&dir->lock);
    return -ENOENT;
}

int ext2_mkdir(const path_t* path, const char* name, size_t len, mode_t mode, uid_t uid, gid_t gid)
{
    dentry_t* dir = path->dentry;
    spinlock_acquire(&dir->lock);

    uint32_t new_dir_inode_indx = 0;
    if (_ext2_allocate_inode_index(dir->vfsdev, &new_dir_inode_indx, 0) < 0) {
        spinlock_release(&dir->lock);
        return -ENOSPC;
    }

    dentry_t* new_dir = dentry_get(dir->dev_indx, new_dir_inode_indx);
    spinlock_acquire(&new_dir->lock);
    if (_ext2_setup_dir(new_dir, dir, mode, uid, gid) < 0) {
        spinlock_release(&new_dir->lock);
        dentry_put(new_dir);
        spinlock_release(&dir->lock);
        return -EFAULT;
    }

    if (_ext2_add_child(dir, new_dir, name, len) < 0) {
        spinlock_release(&new_dir->lock);
        dentry_put(new_dir);
        spinlock_release(&dir->lock);
        return -EFAULT;
    }

    spinlock_release(&new_dir->lock);
    dentry_put(new_dir);
    spinlock_release(&dir->lock);
    return 0;
}

int ext2_rmdir(const path_t* path)
{
    dentry_t* dir = path->dentry;
    dentry_t* parent_dir = dentry_get_parent(dir);
    spinlock_acquire(&dir->lock);

    if (!parent_dir) {
        spinlock_release(&dir->lock);
        return -EPERM;
    }

    if (_ext2_is_dir_empty(dir)) {
        spinlock_acquire(&parent_dir->lock);
        if (_ext2_rm_child(parent_dir, dir) < 0) {
            spinlock_release(&parent_dir->lock);
            spinlock_release(&dir->lock);
            return -EFAULT;
        }
        parent_dir->inode->links_count--;
        dir->inode->links_count--;
        spinlock_release(&parent_dir->lock);
        spinlock_release(&dir->lock);
        return 0;
    }

    spinlock_release(&dir->lock);
    return -ENOTEMPTY;
}

int ext2_getdents(dentry_t* dentry, void __user* buf, off_t* offset, size_t len)
{
    spinlock_acquire(&dentry->lock);
    const uint32_t block_len = BLOCK_LEN(DENTRY_FSDATA(dentry)->sb);
    uint32_t start_block_index = *offset / block_len;
    uint32_t end_block_index = TO_EXT_BLOCKS_CNT(DENTRY_FSDATA(dentry)->sb, dentry->inode->blocks);
    uint32_t read_offset = *offset % block_len;
    uint32_t already_read = 0;

    for (uint32_t block_index = start_block_index; block_index < end_block_index; block_index++) {
        uint32_t data_block_index = _ext2_get_block_of_inode(dentry, block_index);
        uint32_t read_from_block = min(len, block_len - read_offset);
        int act_read = _ext2_getdents_block(dentry->vfsdev, data_block_index, buf + already_read, read_from_block, read_offset, offset);
        if (act_read < 0) {
            spinlock_release(&dentry->lock);
            if (already_read == 0) {
                return act_read;
            }
            return already_read;
        }
        len -= act_read;
        already_read += act_read;
        read_offset = 0;
    }

    spinlock_release(&dentry->lock);
    return already_read;
}

int ext2_create(const path_t* path, const char* name, size_t len, mode_t mode, uid_t uid, gid_t gid)
{
    dentry_t* dir = path->dentry;
    spinlock_acquire(&dir->lock);
    uint32_t new_file_inode_indx = 0;
    if (_ext2_allocate_inode_index(dir->vfsdev, &new_file_inode_indx, 0) < 0) {
        spinlock_release(&dir->lock);
        return -ENOSPC;
    }

    dentry_t* new_file = dentry_get(dir->dev_indx, new_file_inode_indx);
    spinlock_acquire(&new_file->lock);

    if (_ext2_setup_file(new_file, mode, uid, gid) < 0) {
        spinlock_release(&new_file->lock);
        dentry_put(new_file);
        spinlock_release(&dir->lock);
        return -EFAULT;
    }

    if (_ext2_add_child(dir, new_file, name, len) < 0) {
        spinlock_release(&new_file->lock);
        dentry_put(new_file);
        spinlock_release(&dir->lock);
        return -EFAULT;
    }

    spinlock_release(&new_file->lock);
    dentry_put(new_file);
    spinlock_release(&dir->lock);
    return 0;
}

int ext2_rm(const path_t* path)
{
    dentry_t* dentry = path->dentry;
    dentry_t* parent_dir = dentry_get_parent(dentry);
    spinlock_acquire(&dentry->lock);

    if (!parent_dir) {
        spinlock_release(&dentry->lock);
        return -EPERM;
    }

    spinlock_acquire(&parent_dir->lock);
    if (_ext2_rm_child(parent_dir, dentry) < 0) {
        spinlock_release(&parent_dir->lock);
        spinlock_release(&dentry->lock);
        return -EFAULT;
    }

    spinlock_release(&parent_dir->lock);
    spinlock_release(&dentry->lock);
    return 0;
}

int ext2_fchmod(file_t* file, mode_t mode)
{
    dentry_t* dentry = file_dentry_assert(file);

    proc_t* current_p = RUNNING_THREAD->process;
    if (dentry->inode->uid != current_p->euid && !proc_is_su(current_p)) {
        return -EPERM;
    }

    spinlock_acquire(&dentry->lock);
    dentry_set_flag_locked(dentry, DENTRY_DIRTY);
    dentry->inode->mode = (dentry->inode->mode & ~(uint32_t)07777) | (mode & (uint32_t)07777);
    spinlock_release(&dentry->lock);
    return 0;
}

int ext2_fstat(file_t* file, stat_t* stat)
{
    dentry_t* dentry = file_dentry_assert(file);
    spinlock_acquire(&dentry->lock);

    stat->st_dev = MKDEV(0, dentry->dev_indx);
    stat->st_ino = dentry->inode_indx;
    stat->st_mode = dentry->inode->mode;
    stat->st_size = dentry->inode->size;
    stat->st_uid = dentry->inode->uid;
    stat->st_gid = dentry->inode->gid;
    stat->st_blksize = DENTRY_FSDATA(dentry)->blksize;
    stat->st_nlink = dentry->inode->links_count;
    stat->st_blocks = dentry->inode->blocks;
    stat->st_atim.tv_sec = dentry->inode->atime;
    stat->st_atim.tv_nsec = 0;
    stat->st_mtim.tv_sec = dentry->inode->mtime;
    stat->st_mtim.tv_nsec = 0;
    stat->st_ctim.tv_sec = dentry->inode->ctime;
    stat->st_ctim.tv_nsec = 0;

    spinlock_release(&dentry->lock);
    return 0;
}

int ext2_recognize_drive(vfs_device_t* vfsdev)
{
    spinlock_acquire(&VFSDEV_FSLOCK(vfsdev));
    superblock_t* superblock = (superblock_t*)kmalloc(SUPERBLOCK_LEN);
    _ext2_read_from_dev(vfsdev, (uint8_t*)superblock, SUPERBLOCK_START, SUPERBLOCK_LEN);

    if (superblock->magic != 0xEF53) {
        kfree(superblock);
        spinlock_release(&VFSDEV_FSLOCK(vfsdev));
        return -EINVAL;
    }
    if (superblock->rev_level != 0) {
        kfree(superblock);
        spinlock_release(&VFSDEV_FSLOCK(vfsdev));
        return -EINVAL;
    }

    kfree(superblock);
    spinlock_release(&VFSDEV_FSLOCK(vfsdev));
    return 0;
}

int ext2_prepare_fs(vfs_device_t* vfsdev)
{
    spinlock_acquire(&VFSDEV_FSLOCK(vfsdev));
    superblock_t* superblock = (superblock_t*)kmalloc(SUPERBLOCK_LEN);
    _ext2_read_from_dev(vfsdev, (uint8_t*)superblock, SUPERBLOCK_START, SUPERBLOCK_LEN);
    _ext2_superblocks[vfsdev->dev->id] = superblock;

    uint32_t groups_cnt = _ext2_get_groups_cnt(vfsdev, superblock);
    uint32_t group_table_len = groups_cnt * GROUP_LEN;
    group_desc_t* group_table = (group_desc_t*)kmalloc(group_table_len);
    _ext2_read_from_dev(vfsdev, (uint8_t*)group_table, _ext2_get_block_offset(superblock, 2), group_table_len);

    _ext2_group_table_info[vfsdev->dev->id].count = groups_cnt;
    _ext2_group_table_info[vfsdev->dev->id].table = group_table;

    ext2_fsdata_t* fsdata = kmalloc(sizeof(ext2_fsdata_t));
    fsdata->sb = superblock;
    fsdata->gt = &_ext2_group_table_info[vfsdev->dev->id];
    fsdata->blksize = BLOCK_LEN(superblock);

    vfsdev->fsdata = fsdata;
    spinlock_release(&VFSDEV_FSLOCK(vfsdev));
    return 0;
}

int ext2_save_state(vfs_device_t* vfsdev)
{
    spinlock_acquire(&VFSDEV_FSLOCK(vfsdev));
    if (!_ext2_superblocks[vfsdev->dev->id]) {
        spinlock_release(&VFSDEV_FSLOCK(vfsdev));
        return -1;
    }

    superblock_t* superblock = _ext2_superblocks[vfsdev->dev->id];

    uint32_t group_table_len = _ext2_group_table_info[vfsdev->dev->id].count * GROUP_LEN;
    group_desc_t* group_table = _ext2_group_table_info[vfsdev->dev->id].table;
    _ext2_write_to_dev(vfsdev, (uint8_t*)group_table, _ext2_get_block_offset(superblock, 2), group_table_len);
    kfree(group_table);

    _ext2_write_to_dev(vfsdev, (uint8_t*)superblock, SUPERBLOCK_START, SUPERBLOCK_LEN);
    kfree(superblock);
    kfree(vfsdev->fsdata);
    spinlock_release(&VFSDEV_FSLOCK(vfsdev));
    return 0;
}

/**
 * INIT FUNCTIONS
 */

driver_desc_t _ext2_driver_info()
{
    driver_desc_t fs_desc = { 0 };
    fs_desc.type = DRIVER_FILE_SYSTEM;
    fs_desc.functions[DRIVER_FILE_SYSTEM_RECOGNIZE] = ext2_recognize_drive;
    fs_desc.functions[DRIVER_FILE_SYSTEM_PREPARE_FS] = ext2_prepare_fs;
    fs_desc.functions[DRIVER_FILE_SYSTEM_CAN_READ] = ext2_can_read;
    fs_desc.functions[DRIVER_FILE_SYSTEM_CAN_WRITE] = ext2_can_write;
    fs_desc.functions[DRIVER_FILE_SYSTEM_READ] = ext2_read;
    fs_desc.functions[DRIVER_FILE_SYSTEM_WRITE] = ext2_write;
    fs_desc.functions[DRIVER_FILE_SYSTEM_OPEN] = NULL; /* No custom open, vfs will use its code */
    fs_desc.functions[DRIVER_FILE_SYSTEM_TRUNCATE] = ext2_truncate;
    fs_desc.functions[DRIVER_FILE_SYSTEM_MKDIR] = ext2_mkdir;
    fs_desc.functions[DRIVER_FILE_SYSTEM_RMDIR] = ext2_rmdir;
    fs_desc.functions[DRIVER_FILE_SYSTEM_EJECT_DEVICE] = ext2_save_state;

    fs_desc.functions[DRIVER_FILE_SYSTEM_READ_INODE] = ext2_read_inode;
    fs_desc.functions[DRIVER_FILE_SYSTEM_WRITE_INODE] = ext2_write_inode;
    fs_desc.functions[DRIVER_FILE_SYSTEM_FREE_INODE] = ext2_free_inode;
    fs_desc.functions[DRIVER_FILE_SYSTEM_LOOKUP] = ext2_lookup;
    fs_desc.functions[DRIVER_FILE_SYSTEM_GETDENTS] = ext2_getdents;
    fs_desc.functions[DRIVER_FILE_SYSTEM_CREATE] = ext2_create;
    fs_desc.functions[DRIVER_FILE_SYSTEM_UNLINK] = ext2_rm;

    fs_desc.functions[DRIVER_FILE_SYSTEM_FSTAT] = ext2_fstat;
    fs_desc.functions[DRIVER_FILE_SYSTEM_FCHMOD] = ext2_fchmod;
    fs_desc.functions[DRIVER_FILE_SYSTEM_IOCTL] = NULL;
    fs_desc.functions[DRIVER_FILE_SYSTEM_MMAP] = NULL;

    return fs_desc;
}

void ext2_install()
{
    devman_register_driver(_ext2_driver_info(), "ext2");
}
devman_register_driver_installation(ext2_install);