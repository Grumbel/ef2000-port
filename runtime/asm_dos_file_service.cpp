// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#include "asm_dos_file_service.hpp"
#include "dos_drive.hpp"
#include <algorithm>

static unsigned serve(ef_lift_state *guest, uint32_t ip, unsigned vector,
                      EfAsmDosFileContext *context)
{
    if (!guest || !context || !context->drive || vector != 0x21u) return 0x122u;
    const uint16_t ax = guest->r[0] & 0xffffu;
    const unsigned ah = ax >> 8;
    // intdos loads the request from its caller's register block. Admit only
    // file services implemented below, never arbitrary extender requests.
    const bool intdos_file = ip == 0x2f0u &&
        (ah == 0x3cu || ah == 0x3du || ah == 0x3eu || ah == 0x3fu ||
         ah == 0x40u || ah == 0x42u || ah == 0x68u || ax == 0x4400u);
    if (!(intdos_file || (ip == 0xfa973u && ah == 0x42u) ||
          (ip == 0x1103b7u && ah == 0x42u) ||
          ((ip == 0x1103e0u || ip == 0xfa541u) && ah == 0x40u) ||
          ((ip == 0xfa9a8u || ip == 0xfa518u || ip == 0xfa553u ||
            ip == 0xfa5c4u) && ah == 0x3eu) ||
          (ip == 0x10f0efu && ah == 0x3fu) ||
          ((ip == 0xfa4dbu || ip == 0xfa5dfu) && ah == 0x3du) ||
          (ip == 0xfa5b0u && ah == 0x3cu) ||
          (ip == 0x1101c7u && ax == 0x4400u))) return 0x122u;

    auto& drive = *context->drive;
    const uint16_t handle = guest->r[3] & 0xffffu;
    try {
        if (ah == 0x3du || ah == 0x3cu) {
            const uint32_t address = guest->r[2];
            // Match the hybrid bridge's complete 260-byte filename window.
            if (!guest->memory || guest->memory_size < 260u || address <= 0x260u ||
                address >= guest->memory_size - 260u) throw unsigned(3);
            const auto first = guest->memory + address;
            const auto end = std::find(first, first + 260, uint8_t(0));
            if (end == first + 260) throw unsigned(3);
            const std::string name(first, end);
            if (ah == 0x3du) {
                guest->r[0] = drive.open(name, ax & 3u, context->joystick,
                                         context->digital_sound, context->cd_music);
                guest->r[2] = 0;
            } else guest->r[0] = drive.create(name);
        } else if (ah == 0x42u) {
            const int32_t offset = int32_t(((guest->r[1] & 0xffffu) << 16) |
                                         (guest->r[2] & 0xffffu));
            const uint64_t position = drive.seek(handle, offset, ax & 255u);
            guest->r[0] = position & 0xffffu;
            guest->r[2] = (guest->r[2] & 0xffff0000u) | ((position >> 16) & 0xffffu);
        } else if (ah == 0x3fu) {
            const uint32_t address = guest->r[2], count = guest->r[1];
            if (!guest->memory || address <= 0x260u || address >= guest->memory_size ||
                count > guest->memory_size - address) throw unsigned(5);
            const auto bytes = drive.read(handle, count);
            std::copy(bytes.begin(), bytes.end(), guest->memory + address);
            guest->r[0] = bytes.size();
            guest->r[1] = guest->r[2] = 0;
        } else if (ah == 0x40u) {
            const uint32_t address = guest->r[2], count = guest->r[1];
            if (!guest->memory || address <= 0x260u || address >= guest->memory_size ||
                count > guest->memory_size - address) throw unsigned(5);
            const std::vector<uint8_t> bytes(guest->memory + address,
                                            guest->memory + address + count);
            guest->r[0] = drive.write(handle, bytes);
        } else if (ah == 0x68u) {
            // Match the hybrid bridge: console handles succeed, successful
            // flush preserves the complete EAX request, and the file stays open.
            if (handle > 2) drive.flush(handle);
        } else if (ah == 0x3eu) {
            drive.close(handle);
            guest->r[0] = 0x3e01u;
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

extern "C" unsigned ef_asm_dos_file_service(ef_lift_state *guest, uint32_t ip,
                                           unsigned vector, void *context)
{
    EfAsmDosFileContext configured{static_cast<ef2000::DosDrive*>(context)};
    return serve(guest, ip, vector, &configured);
}

extern "C" unsigned ef_asm_dos_file_service_configured(ef_lift_state *guest,
    uint32_t ip, unsigned vector, void *context)
{
    return serve(guest, ip, vector, static_cast<EfAsmDosFileContext*>(context));
}
