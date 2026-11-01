// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "real_mode_cpu.hpp"

#include <unicorn/x86.h>

#include <array>
#include <cstring>
#include <stdexcept>
#include <string>

namespace ef2000 {
namespace {
constexpr uint32_t MEMORY_SIZE = 0x100000;
constexpr uint32_t RETURN_ADDRESS = 0xf0000;
constexpr uint32_t RETURN_STACK = 0x9fefa;
constexpr std::array<int, 11> REGISTER_IDS = {
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_DS, UC_X86_REG_ES,
    UC_X86_REG_FS, UC_X86_REG_GS, UC_X86_REG_EFLAGS};
}

RealModeCpu::RealModeCpu(bool sb16_probe) : sb16_probe_enabled_(sb16_probe) {
    check(uc_open(UC_ARCH_X86, UC_MODE_16, &cpu_), "open 16-bit Unicorn CPU");
    try {
        check(uc_mem_map(cpu_, 0, MEMORY_SIZE, UC_PROT_ALL), "map conventional DOS memory");
        uc_hook hook{};
        check(uc_hook_add(cpu_, &hook, UC_HOOK_INSN,
                          reinterpret_cast<void*>(port_in), this, 1, 0, UC_X86_INS_IN),
              "install real-mode IN hook");
        check(uc_hook_add(cpu_, &hook, UC_HOOK_INSN,
                          reinterpret_cast<void*>(port_out), this, 1, 0, UC_X86_INS_OUT),
              "install real-mode OUT hook");
        check(uc_hook_add(cpu_, &hook, UC_HOOK_INTR,
                          reinterpret_cast<void*>(interrupt), this, 1, 0),
              "install nested real-mode interrupt hook");
    } catch (...) {
        uc_close(cpu_);
        cpu_ = nullptr;
        throw;
    }
}

RealModeCpu::~RealModeCpu() { if (cpu_) uc_close(cpu_); }

void RealModeCpu::check(uc_err error, const char* operation) {
    if (error != UC_ERR_OK)
        throw std::runtime_error(std::string(operation) + ": " + uc_strerror(error));
}

void RealModeCpu::fail(const std::string& message) {
    error_ = message;
    uc_emu_stop(cpu_);
}

void RealModeCpu::interrupt(uc_engine*, uint32_t vector, void* user) {
    static_cast<RealModeCpu*>(user)->fail("unsupported nested real-mode INT " +
                                           std::to_string(vector));
}

uint32_t RealModeCpu::port_in(uc_engine*, uint32_t port, int size, void* user) {
    auto* device = static_cast<RealModeCpu*>(user);
    // Until an actual SB16 model is connected, the absent card has floating
    // read lines. Other I/O is deliberately diagnosed, not silently ignored.
    if (size == 1 && port >= 0x220 && port <= 0x22f) {
        if (!device->sb16_probe_enabled_) return 0xff;
        try { return device->sb16_probe_.read(port); }
        catch (const std::exception& error) {
            device->fail(error.what());
            return 0;
        }
    }
    device->fail("unsupported real-mode IN port " + std::to_string(port) +
                 " width " + std::to_string(size));
    return 0;
}

void RealModeCpu::port_out(uc_engine*, uint32_t port, int size,
                           uint32_t value, void* user) {
    auto* device = static_cast<RealModeCpu*>(user);
    if (size == 1 && port >= 0x220 && port <= 0x22f) {
        if (!device->sb16_probe_enabled_) return;
        try { device->sb16_probe_.write(port, value & 255); }
        catch (const std::exception& error) { device->fail(error.what()); }
        return;
    }
    device->fail("unsupported real-mode OUT port " +
        std::to_string(port) + " width " + std::to_string(size));
}

RealResult RealModeCpu::execute(uint32_t entry, const std::array<uint32_t, 256>& vectors,
                                 const std::vector<uint8_t>& memory,
                                 const RealRegisters& regs, uint64_t instruction_limit) {
    const uint32_t address = ((entry >> 16) << 4) + (entry & 0xffff);
    if (address < 0x10000 || address >= 0x90000 || memory.size() != MEMORY_SIZE ||
        instruction_limit == 0)
        throw std::runtime_error("invalid real-mode driver entry, RAM size or instruction limit");
    error_.clear();
    check(uc_mem_write(cpu_, 0, memory.data(), memory.size()), "load DOS memory");
    check(uc_mem_write(cpu_, 0, vectors.data(), vectors.size() * sizeof(uint32_t)),
          "load real-mode interrupt vector table");
    check(uc_ctl_remove_cache(cpu_, 0, MEMORY_SIZE), "invalidate translated real-mode driver");
    const uint8_t halt = 0xf4;
    check(uc_mem_write(cpu_, RETURN_ADDRESS, &halt, 1), "install driver return stub");

    const uint32_t values[] = {regs.ax, regs.bx, regs.cx, regs.dx, regs.si, regs.di,
                               regs.ds, regs.es, regs.fs, regs.gs, regs.flags};
    for (size_t i = 0; i < REGISTER_IDS.size(); ++i)
        check(uc_reg_write(cpu_, REGISTER_IDS[i], &values[i]), "set driver register");
    const uint16_t stack[] = {0, 0xf000, uint16_t(regs.flags)};
    check(uc_mem_write(cpu_, RETURN_STACK, stack, sizeof(stack)), "install far interrupt return");
    const uint16_t ss = 0x9000, sp = 0xfefa, cs = entry >> 16, ip = entry & 0xffff;
    check(uc_reg_write(cpu_, UC_X86_REG_SS, &ss), "set driver SS");
    check(uc_reg_write(cpu_, UC_X86_REG_SP, &sp), "set driver SP");
    check(uc_reg_write(cpu_, UC_X86_REG_CS, &cs), "set driver CS");
    check(uc_reg_write(cpu_, UC_X86_REG_IP, &ip), "set driver IP");

    const uc_err execution = uc_emu_start(cpu_, address, RETURN_ADDRESS,
                                          500000, instruction_limit);
    if (execution != UC_ERR_OK) check(execution, "execute original real-mode driver");
    if (!error_.empty()) throw std::runtime_error(error_);
    uint16_t result_cs{}, result_ip{};
    check(uc_reg_read(cpu_, UC_X86_REG_CS, &result_cs), "read driver CS");
    check(uc_reg_read(cpu_, UC_X86_REG_IP, &result_ip), "read driver IP");
    if (result_cs != 0xf000 || result_ip != 0)
        throw std::runtime_error("real-mode driver instruction/time limit reached");

    RealResult result;
    result.memory.resize(MEMORY_SIZE);
    check(uc_mem_read(cpu_, 0, result.memory.data(), MEMORY_SIZE), "read driver memory");
    // The synthetic IRET stack and HLT destination are host implementation
    // details, not guest writes. Keep their original DOS bytes intact.
    std::memcpy(result.memory.data() + RETURN_STACK,
                memory.data() + RETURN_STACK, sizeof(stack));
    result.memory[RETURN_ADDRESS] = memory[RETURN_ADDRESS];
    uint32_t output[REGISTER_IDS.size()]{};
    for (size_t i = 0; i < REGISTER_IDS.size(); ++i)
        check(uc_reg_read(cpu_, REGISTER_IDS[i], &output[i]), "read driver register");
    result.registers = {output[0], output[1], output[2], output[3], output[4], output[5],
                        uint16_t(output[6]), uint16_t(output[7]), uint16_t(output[8]),
                        uint16_t(output[9]), output[10]};
    return result;
}

} // namespace ef2000
