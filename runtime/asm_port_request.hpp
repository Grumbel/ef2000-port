// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#pragma once
#include "ef_lift_state.h"

struct EfAsmPortRequest { uint16_t port; unsigned width; bool output; };
// Decode the opt-in scalar port boundary contract without mutating guest state.
inline bool ef_asm_decode_port_request(const ef_lift_state* guest, uint32_t ip,
                                       unsigned request, EfAsmPortRequest& decoded)
{
    if (!guest || !guest->memory || (request & ~0xfffu) != 0x1000u) return false;
    const unsigned length = (request >> 8) & 15u, opcode = request & 255u;
    if (opcode != 0xe4u && opcode != 0xe5u && opcode != 0xe6u && opcode != 0xe7u &&
        opcode != 0xecu && opcode != 0xedu && opcode != 0xeeu && opcode != 0xefu)
        return false;
    if (!length || ip >= guest->memory_size || length > guest->memory_size - ip) return false;
    const uint8_t* bytes = guest->memory + ip;
    const bool word = bytes[0] == 0x66u, immediate = opcode < 0xecu;
    if (length != unsigned(word) + (immediate ? 2u : 1u) ||
        (word && !(opcode & 1u)) || bytes[unsigned(word)] != opcode ||
        guest->pc != ip + length) return false;
    decoded = {uint16_t(immediate ? bytes[length - 1] : guest->r[2] & 0xffffu),
               (opcode & 1u) ? (word ? 16u : 32u) : 8u, bool(opcode & 2u)};
    return true;
}
