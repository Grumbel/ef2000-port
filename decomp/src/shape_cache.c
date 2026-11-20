/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

#define SHAPE_CACHE_SLOTS   1000
#define SHAPE_NAME_TABLE    0xfa0u      /* 1000 name pointers */
#define SHAPE_SENTINEL_NAME 0x0019055cu
#define SHAPE_DEFAULT_FILE  0x00190564u /* "\3\a.tmp" */
#define SHAPE_EXTENSION     0x00190570u /* "3" */
#define EMPTY_STRING        0x00190574u
#define SHAPE_ARCHIVE       0x001603f4u
#define SHAPE_DIR           ((ef_addr32)(uintptr_t)g_shape_dir)
#define SHAPE_DRIVE         ((ef_addr32)(uintptr_t)g_shape_drive)

/* Load the shape data block for a shape file: from the open archive when
   it holds NAME, otherwise from the shape directory or the current
   directory through the file hooks. Returns the new block id. */
static uint32_t read_shape_block(ef_addr32 path, ef_addr32 name, uint32_t *block)
{
    uint32_t id;
    uint32_t size;

    if (refresh_cached_chunk_ea3ec(SHAPE_ARCHIVE, name)) {
        id = take_pool_block_b3db0(read_link_ea414());
        *block = lookup_id_b3f58((int16_t)id);
        decode_chunk_ea420(*block);
        return id;
    }

    make_path_e80d0(path, SHAPE_DRIVE, SHAPE_DIR, name, SHAPE_EXTENSION);
    size = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), path);
    if (size == 0) {
        make_path_e80d0(path, EMPTY_STRING, EMPTY_STRING, name, SHAPE_EXTENSION);
        size = guest_callptr_eax_ret_x(EF_U32(0x00132ab4u), path);
        if (size == 0)
            guest_callptr2_x(EF_U32(0x00132a74u), 0x00190578u, path); /* "Couldn't find %s" */
    }
    id = take_pool_block_b3db0(size);
    *block = lookup_id_b3f58((int16_t)id);
    guest_callptr_eaxedx_ret_x(EF_U32(0x00132ab8u), path, *block);
    return id;
}

/* 0xde864, 620 bytes. Return the shape id for the shape file NAME,
   loading it on first use. Names are lower-cased and looked up by hash
   in the shape tree; a new shape is read, normalized by 0xc4db4 and
   entered under the next slot with a copy of its name and its hash. */
uint32_t load_shape_de864(ef_addr32 name)
{
    uint8_t dir_buf[0x84];
    uint8_t path_buf[0x50];
    uint8_t base_buf[0x0c];
    uint8_t ext_buf[0x08];
    uint8_t drive_buf[0x04];
    ef_addr32 dir = (ef_addr32)(uintptr_t)dir_buf;
    ef_addr32 path = (ef_addr32)(uintptr_t)path_buf;
    ef_addr32 base = (ef_addr32)(uintptr_t)base_buf;
    ef_addr32 ext = (ef_addr32)(uintptr_t)ext_buf;
    ef_addr32 drive = (ef_addr32)(uintptr_t)drive_buf;
    ef_addr32 copy;
    uint32_t id;
    uint32_t block;
    int16_t found;
    int32_t slot;

    if (g_shape_count == 0) {
        g_shape_names = arena_take_e29e8(SHAPE_NAME_TABLE);
        EF_U16(0x004270f2u) = 0;
        EF_U16(0x004270f0u) = 0;
        g_shape_count = 1;
        g_shape_hashes[0] = 0;
        EF_U32(g_shape_names) = SHAPE_SENTINEL_NAME;
    }
    if (g_shape_dir_ready == 0) {
        g_shape_dir[0] = 0;
        g_shape_drive[0] = 0;
        build_default_path_9c980(path);
        strcat_9ca57(path, SHAPE_DEFAULT_FILE);
        split_path_e7fc0(path, SHAPE_DRIVE, SHAPE_DIR, base, ext);
        g_shape_dir_ready = 1;
    }

    copy_string_9d749(path, name);
    split_path_e7fc0(path, drive, dir, base, ext);
    lowercase_string(base);
    found = (int16_t)tree_find_or_insert_de7d0(base);
    if (found >= 0)
        return g_shape_ids[found];

    id = read_shape_block(path, base, &block);
    renderer_normalize_scene_c4db4(block);
    g_shape_ids[g_shape_count] = (uint16_t)id;

    copy = arena_take_e29e8(strlen_ds(base) + 1u);
    copy_string_9d749(copy, base);
    slot = g_shape_count;
    EF_U32(g_shape_names + 4u * (uint32_t)slot) = copy;
    g_shape_hashes[slot] = name_hash_ebfd0(copy);
    g_shape_count++;
    if (g_shape_count == SHAPE_CACHE_SLOTS)
        guest_callptr1_x(EF_U32(0x00132a74u), 0x0019058cu); /* "Out of space for shapes" */
    return id;
}
