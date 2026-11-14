// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#pragma once

#include "call_capture.hpp"
#include <unicorn/unicorn.h>
#include <unicorn/x86.h>
#include <cstddef>
#include <limits>

namespace ef2000 {
inline void capture_x87_context(uc_engine* cpu, CapturedCall& capture) {
    auto read = [cpu](int id, void* out) {
        const auto error = uc_reg_read(cpu, id, out);
        if (error != UC_ERR_OK) throw std::runtime_error(std::string("capture register: ")+uc_strerror(error));
    };
    const int extras[] = {UC_X86_REG_FS, UC_X86_REG_GS, UC_X86_REG_CS};
    for (size_t i = 0; i < 3; ++i) read(extras[i], &capture.extra_registers[i]);
    const int environment[] = {UC_X86_REG_FPCW, UC_X86_REG_FPSW, UC_X86_REG_FPTAG,
        UC_X86_REG_FIP, UC_X86_REG_FCS, UC_X86_REG_FDP, UC_X86_REG_FDS, UC_X86_REG_FOP};
    for (size_t i = 0; i < 8; ++i) {
        uint64_t value{}; // Some register APIs write narrower values, others 64 bits.
        read(environment[i], &value);
        if (value > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("capture x87 environment exceeds 32-bit guest width");
        capture.x87[i] = static_cast<uint32_t>(value);
    }
    struct RawFloat { uint64_t mantissa{}; uint16_t exponent{}; };
    static_assert(offsetof(RawFloat, exponent) == 8);
    for (int i = 0; i < 8; ++i) {
        RawFloat value;
        read(UC_X86_REG_FP0+i, &value);
        capture.x87[8+3*i] = static_cast<uint32_t>(value.mantissa);
        capture.x87[8+3*i+1] = static_cast<uint32_t>(value.mantissa>>32);
        capture.x87[8+3*i+2] = value.exponent;
    }
    uc_x86_mmr tables[2]{};
    read(UC_X86_REG_GDTR, &tables[0]);
    read(UC_X86_REG_LDTR, &tables[1]);
    for (size_t i = 0; i < 2; ++i) {
        const auto& t = tables[i];
        const uint32_t words[] = {t.selector, static_cast<uint32_t>(t.base),
            static_cast<uint32_t>(t.base>>32), t.limit, t.flags};
        for (size_t j = 0; j < 5; ++j) capture.table_registers[5*i+j] = words[j];
        const size_t length = size_t(std::min<uint64_t>(uint64_t(t.limit)+1, 65536));
        auto& bytes = capture.table_bytes[i];
        bytes.resize(length);
        if (t.base > UINT64_MAX-(length-1) || uc_mem_read(cpu, t.base, bytes.data(), length) != UC_ERR_OK)
            bytes.clear();
    }
    const uint32_t selectors[] = {capture.registers[11], capture.extra_registers[2],
        capture.registers[12], capture.registers[10], capture.extra_registers[0], capture.extra_registers[1]};
    capture.descriptor_valid_mask = 0;
    capture.segment_descriptors.fill(0);
    for (size_t i = 0; i < 6; ++i) {
        const auto& table = tables[(selectors[i] & 4u) ? 1 : 0];
        const uint64_t offset = selectors[i] & 0xfff8u;
        if (offset+7u > table.limit || table.base > UINT64_MAX-offset) continue;
        std::array<uint8_t, 8> raw{};
        if (uc_mem_read(cpu, table.base+offset, raw.data(), raw.size()) != UC_ERR_OK) continue;
        for (size_t byte = 0; byte < raw.size(); ++byte)
            capture.segment_descriptors[2*i+byte/4] |= uint32_t(raw[byte]) << (8*(byte%4));
        capture.descriptor_valid_mask |= 1u<<i;
    }
    capture.producer_version = uc_version(nullptr, nullptr);
    capture.has_x87 = true;
}
} // namespace ef2000
