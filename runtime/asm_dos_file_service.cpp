// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#include "asm_dos_file_service.hpp"
#include "dos_drive.hpp"

extern "C" unsigned ef_asm_dos_file_service(ef_lift_state *guest, uint32_t ip,
                                           unsigned vector, void *context)
{
    if (!guest || !context || vector != 0x21u) return 0x122u;
    const uint16_t ax = guest->r[0] & 0xffffu;
    const unsigned ah = ax >> 8;
    if (!((ip == 0xfa973u && ah == 0x42u) ||
          (ip == 0x1103b7u && ah == 0x42u) ||
          (ip == 0x1103e0u && ah == 0x40u) ||
          (ip == 0x1101c7u && ax == 0x4400u))) return 0x122u;

    auto& drive = *static_cast<ef2000::DosDrive*>(context);
    const uint16_t handle = guest->r[3] & 0xffffu;
    try {
        if (ah == 0x42u) {
            const int32_t offset = int32_t(((guest->r[1] & 0xffffu) << 16) |
                                         (guest->r[2] & 0xffffu));
            const uint64_t position = drive.seek(handle, offset, ax & 255u);
            guest->r[0] = position & 0xffffu;
            guest->r[2] = (guest->r[2] & 0xffff0000u) | ((position >> 16) & 0xffffu);
        } else if (ah == 0x40u) {
            const uint32_t address = guest->r[2], count = guest->r[1];
            if (!guest->memory || address <= 0x260u || address >= guest->memory_size ||
                count > guest->memory_size - address) throw unsigned(5);
            const std::vector<uint8_t> bytes(guest->memory + address,
                                            guest->memory + address + count);
            guest->r[0] = drive.write(handle, bytes);
        } else {
            if (handle > 2 && !drive.is_open(handle)) throw unsigned(6);
            const uint32_t device = handle <= 2 ? 0x80d3u : 2u;
            guest->r[2] = (guest->r[2] & 0xffff0000u) | device;
            guest->r[0] = device;
        }
        guest->flags &= ~1u;
    } catch (unsigned error) {
        guest->r[0] = error;
        guest->flags |= 1u;
    }
    return 0;
}
