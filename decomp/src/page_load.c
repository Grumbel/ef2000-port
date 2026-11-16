/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

typedef struct {
    uint8_t name[80];
    uint8_t path[80];
    uint8_t mode;
    uint8_t unused_mode[3];
    uint8_t reused;
    uint8_t unused_reused[3];
} ef_page_load_frame;
EF_STATIC_ASSERT(page_load_path, offsetof(ef_page_load_frame, path) == 80);
EF_STATIC_ASSERT(page_load_mode, offsetof(ef_page_load_frame, mode) == 160);
EF_STATIC_ASSERT(page_load_reused, offsetof(ef_page_load_frame, reused) == 164);
EF_STATIC_ASSERT(page_load_frame_size, sizeof(ef_page_load_frame) == 168);

static uint8_t page_record_mode(ef_addr32 record)
{
    return (uint8_t)((EF_U8(record + 10u) >> 5) & 3u);
}

/* 0x9de08,440 bytes. EAX filename, low EDX mode, AX page ID;
 * preserves EBX/ECX/ESI/EDI/EBP, plain RET. Callers 3cb1b and 7a93f
 * store only AX; 9ddff is the zero-mode wrapper. */
uint16_t load_page_9de08(ef_addr32 filename, uint32_t mode)
{
    ef_page_load_frame storage;
    ef_addr32 frame = (ef_addr32)(uintptr_t)&storage;
    ef_addr32 resource, record;
    uint32_t resource_id, page_id, size, remaining;
    uint16_t next;
    uint8_t flags, count, header, reused;

    EF_U8(frame + 160u) = (uint8_t)mode;
    (void)append_base_name_9dc88(frame, frame + 80u, filename);
    resource_id = tree_find_or_insert_9dbf0(frame);
    if ((int16_t)resource_id < 0)
        resource_id = register_resource_9dcf8(frame + 80u, frame);
    resource = EF_U32(0x0040d200u + ((uint32_t)(int32_t)(int16_t)resource_id << 2));
    count = EF_U8(resource + 1u);
    header = EF_U8(resource);
    /* Every allocation and fill receives only the computed low word. */
    size = (uint16_t)((uint32_t)count * 29u + header + 11u);
    EF_U8(frame + 164u) = 0u;
    if ((EF_U8(resource + 4u) & 1u) == 0u) {
        page_id = take_pool_block_b3db0(size);
    } else {
        page_id = EF_U16(resource + 6u);
        if ((uint16_t)page_id == 0xffffu) {
            page_id = take_pool_block_b3db0(size);
            EF_U16(resource + 6u) = (uint16_t)page_id;
        } else {
            record = lookup_id_b3f58((int16_t)page_id);
            EF_U8(frame + 164u) = 1u;
            if (page_record_mode(record) != EF_U8(frame + 160u)) {
                EF_U8(frame + 164u) = 0u;
                for (;;) {
                    if (page_record_mode(record) == EF_U8(frame + 160u))
                        break;
                    next = EF_U16(record + 8u);
                    if (next == 0xffffu)
                        break;
                    page_id = next;
                    record = lookup_id_b3f58((int16_t)page_id);
                }
                if (page_record_mode(record) == EF_U8(frame + 160u)) {
                    EF_U8(frame + 164u) = 1u;
                    if ((uint16_t)page_id == 0xffffu)
                        guest_callptr1_x(EF_U32(0x00132a74u), 0x0018b2ccu);
                } else {
                    page_id = take_pool_block_b3db0(size);
                    EF_U16(record + 8u) = (uint16_t)page_id;
                }
            }
        }
    }
    reused = EF_U8(frame + 164u);
    record = lookup_id_b3f58((int16_t)page_id);
    if (reused == 0u) {
        fill_bytes_b3fb0(record, 0u, size);
        mode = EF_U8(frame + 160u);
        flags = EF_U8(record + 10u);
        EF_U16(record + 8u) = 0xffffu;
        flags &= 0x9fu;
        EF_U8(record + 10u) = flags;
        EF_U16(record) = (uint16_t)resource_id;
        EF_U8(record + 10u) = (uint8_t)(flags | ((mode & 3u) << 5));
    }
    if (EF_U8(frame + 164u) == 0u) {
        run_page_text_9e508((int16_t)page_id, 0u);
        EF_U8(record + 10u) &= 0x7fu;
    }
    remaining = EF_U32(0x00417160u) - 1u;
    EF_U32(0x00417160u) = remaining;
    if (remaining == 0u)
        load_record_a1ec8();
    return (uint16_t)page_id;
}
