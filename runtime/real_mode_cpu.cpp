// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "real_mode_cpu.hpp"

#include <unicorn/x86.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <sstream>
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
double seconds_now() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

RealModeCpu::RealModeCpu(SoundMode mode) : sound_mode_(mode) {
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
        if (mode == SoundMode::Dma)
            check(uc_hook_add(cpu_, &hook, UC_HOOK_BLOCK,
                              reinterpret_cast<void*>(audio_block), this, 1, 0),
                  "install real-mode audio scheduling hook");
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
    if (device->sound_mode_ == SoundMode::Dma && SoundBlaster16::handles(port)) {
        if (size != 1) {
            device->fail("unsupported SB16 DMA IN width " + std::to_string(size));
            return 0;
        }
        try {
            device->sb16_dma_.set_time(seconds_now());
            return device->sb16_dma_.read(port);
        } catch (const std::exception& error) {
            device->fail(error.what());
            return 0;
        }
    }
    // Until an actual SB16 model is connected, the absent card has floating
    // read lines. Other I/O is deliberately diagnosed, not silently ignored.
    if (size == 1 && port >= 0x220 && port <= 0x22f) {
        if (device->sound_mode_ == SoundMode::Off) return 0xff;
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
    if (device->sound_mode_ == SoundMode::Dma && SoundBlaster16::handles(port)) {
        if (size != 1) return device->fail("unsupported SB16 DMA OUT width " + std::to_string(size));
        try {
            device->sb16_dma_.set_time(seconds_now());
            device->sb16_dma_.write(port, value & 255);
        } catch (const std::exception& error) { device->fail(error.what()); }
        return;
    }
    if (size == 1 && port >= 0x220 && port <= 0x22f) {
        if (device->sound_mode_ == SoundMode::Off) return;
        try { device->sb16_probe_.write(port, value & 255); }
        catch (const std::exception& error) { device->fail(error.what()); }
        return;
    }
    device->fail("unsupported real-mode OUT port " +
        std::to_string(port) + " width " + std::to_string(size));
}

void RealModeCpu::audio_block(uc_engine*, uint64_t, uint32_t, void* user) {
    auto* device = static_cast<RealModeCpu*>(user);
    if (!device->error_.empty()) return;
    try {
        device->sb16_dma_.advance(seconds_now(), [&](uint32_t address, size_t length) {
            std::vector<uint8_t> bytes(length);
            device->check(uc_mem_read(device->cpu_, address, bytes.data(), bytes.size()),
                          "read original guest audio DMA memory");
            return bytes;
        });
        const auto irq = device->sb16_dma_.pic().next_irq();
        if (irq) {
            uint32_t flags{};
            device->check(uc_reg_read(device->cpu_, UC_X86_REG_EFLAGS, &flags),
                          "read real-mode audio interrupt flags");
            if (flags & 0x200) {
                device->pending_audio_irq_ = irq;
                device->check(uc_emu_stop(device->cpu_), "pause real-mode CPU for audio IRQ");
            }
        }
    } catch (const std::exception& error) {
        device->fail(std::string("audio hardware: ") + error.what());
    }
}

void RealModeCpu::inject_audio_irq(unsigned irq) {
    if (irq >= 8) throw std::runtime_error("unsupported secondary audio IRQ");
    const unsigned vector = irq + 8;
    uint16_t address[2]{};
    check(uc_mem_read(cpu_, vector * 4, address, sizeof(address)), "read audio IRQ vector");
    if (!address[1]) throw std::runtime_error("uninstalled audio IRQ " + std::to_string(irq));
    uint16_t ip{}, cs{}, ss{}, sp{};
    uint32_t flags{};
    check(uc_reg_read(cpu_, UC_X86_REG_IP, &ip), "read audio return IP");
    check(uc_reg_read(cpu_, UC_X86_REG_CS, &cs), "read audio return CS");
    check(uc_reg_read(cpu_, UC_X86_REG_SS, &ss), "read audio stack SS");
    check(uc_reg_read(cpu_, UC_X86_REG_SP, &sp), "read audio stack SP");
    check(uc_reg_read(cpu_, UC_X86_REG_EFLAGS, &flags), "read audio return FLAGS");
    sp = uint16_t(sp - 6);
    const uint32_t frame_address = (uint32_t(ss) << 4) + sp;
    if (frame_address + 6 > MEMORY_SIZE)
        throw std::runtime_error("audio IRQ stack outside DOS RAM");
    const uint16_t frame[] = {ip, cs, uint16_t(flags)};
    check(uc_mem_write(cpu_, frame_address, frame, sizeof(frame)), "push audio IRQ frame");
    check(uc_reg_write(cpu_, UC_X86_REG_SP, &sp), "set audio handler SP");
    const uint32_t handler_flags = flags & ~0x300u;
    check(uc_reg_write(cpu_, UC_X86_REG_EFLAGS, &handler_flags), "mask audio handler IF/TF");
    check(uc_reg_write(cpu_, UC_X86_REG_CS, &address[1]), "enter audio handler CS");
    check(uc_reg_write(cpu_, UC_X86_REG_IP, &address[0]), "enter audio handler IP");
    sb16_dma_.pic().begin(irq);
}

RealResult RealModeCpu::execute(uint32_t entry, const std::array<uint32_t, 256>& vectors,
                                 const std::vector<uint8_t>& memory,
                                 const RealRegisters& regs, uint64_t instruction_limit) {
    const uint32_t address = ((entry >> 16) << 4) + (entry & 0xffff);
    if (address < 0x10000 || address >= 0x90000 || memory.size() != MEMORY_SIZE ||
        instruction_limit == 0)
        throw std::runtime_error("invalid real-mode driver entry, RAM size or instruction limit");
    error_.clear();
    pending_audio_irq_.reset();
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

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    for (unsigned tries = 0; tries < 64; ++tries) {
        if (pending_audio_irq_) {
            inject_audio_irq(*pending_audio_irq_);
            pending_audio_irq_.reset();
        }
        uint16_t current_cs{}, current_ip{};
        check(uc_reg_read(cpu_, UC_X86_REG_CS, &current_cs), "read real-mode CS");
        check(uc_reg_read(cpu_, UC_X86_REG_IP, &current_ip), "read real-mode IP");
        const uint32_t current = (uint32_t(current_cs) << 4) + current_ip;
        if (current_cs == 0xf000 && current_ip == 0) break;
        const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) break;
        check(uc_emu_start(cpu_, current, RETURN_ADDRESS, remaining, instruction_limit),
              "execute original real-mode driver");
        if (!error_.empty()) break;
        if (!pending_audio_irq_) break;
    }
    if (!error_.empty()) throw std::runtime_error(error_);
    uint16_t result_cs{}, result_ip{};
    check(uc_reg_read(cpu_, UC_X86_REG_CS, &result_cs), "read driver CS");
    check(uc_reg_read(cpu_, UC_X86_REG_IP, &result_ip), "read driver IP");
    if (result_cs != 0xf000 || result_ip != 0) {
        uint16_t ss{}, sp{};
        uint32_t ax{}, flags{};
        check(uc_reg_read(cpu_, UC_X86_REG_SS, &ss), "read stalled driver SS");
        check(uc_reg_read(cpu_, UC_X86_REG_SP, &sp), "read stalled driver SP");
        check(uc_reg_read(cpu_, UC_X86_REG_EAX, &ax), "read stalled driver AX");
        check(uc_reg_read(cpu_, UC_X86_REG_EFLAGS, &flags), "read stalled driver FLAGS");
        std::ostringstream detail;
        detail << "real-mode driver instruction/time limit reached at " << std::hex
               << std::setfill('0') << std::setw(4) << result_cs << ':'
               << std::setw(4) << result_ip << " SS:SP=" << ss << ':' << sp
               << " AX=" << ax << " FLAGS=" << flags;
        if (sound_mode_ == SoundMode::Dma)
            detail << " PIC-mask=" << unsigned(sb16_dma_.pic().read(0x21))
                   << " PIC-IRR=" << unsigned(sb16_dma_.pic().read(0x20))
                   << " next-audio-deadline=" << std::dec << sb16_dma_.next_deadline();
        throw std::runtime_error(detail.str());
    }

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
    if (sound_mode_ == SoundMode::Dma)
        result.audio_blocks = sb16_dma_.take_completed();
    return result;
}

double RealModeCpu::next_audio_deadline() const {
    return sound_mode_ == SoundMode::Dma ? sb16_dma_.next_deadline() : 0;
}

bool RealModeCpu::audio_service_due(double now) const {
    if (sound_mode_ != SoundMode::Dma) return false;
    const double deadline = sb16_dma_.next_deadline();
    return (deadline && now >= deadline) || sb16_dma_.pic().next_irq().has_value();
}

AudioServiceResult RealModeCpu::service_audio(
        double now, const std::array<uint32_t, 256>& vectors,
        const std::vector<uint8_t>& memory) {
    AudioServiceResult result;
    if (!audio_service_due(now)) return result;
    if (memory.size() != MEMORY_SIZE)
        throw std::runtime_error("invalid DOS memory size for background audio");
    sb16_dma_.advance(now, [&](uint32_t address, size_t length) {
        if (uint64_t(address) + length > memory.size())
            throw std::runtime_error("background audio DMA outside DOS RAM");
        return std::vector<uint8_t>(memory.begin() + address,
                                    memory.begin() + address + length);
    });
    result.audio_blocks = sb16_dma_.take_completed();
    const auto irq = sb16_dma_.pic().next_irq();
    if (irq) {
        const unsigned vector = *irq + 8;
        // DOS drivers can rewrite the physical IVT directly. X-32's vector
        // registry only sees requests issued through its 2505h service.
        // Prefer the actual guest IVT and fall back to registered vectors.
        auto live_vectors = vectors;
        for (unsigned i = 0; i < live_vectors.size(); ++i) {
            const size_t offset = i * 4;
            const uint32_t direct = uint32_t(memory[offset]) |
                                    (uint32_t(memory[offset + 1]) << 8) |
                                    (uint32_t(memory[offset + 2]) << 16) |
                                    (uint32_t(memory[offset + 3]) << 24);
            if (direct) live_vectors[i] = direct;
        }
        const uint32_t target = live_vectors[vector];
        const uint32_t address = ((target >> 16) << 4) + (target & 0xffff);
        if (!target || address < 0x10000 || address >= 0x90000)
            throw std::runtime_error("uninstalled background audio IRQ vector " +
                                     std::to_string(vector));
        sb16_dma_.pic().begin(*irq);
        // The protected-mode CPU has no real-mode CS:IP/SS:SP to interrupt.
        // Run just the installed DOS handler with a synthetic, IF-cleared
        // hardware frame and keep its conventional-memory side effects.
        RealRegisters regs;
        regs.flags = 0x2;
        auto handled = execute(target, live_vectors, memory, regs);
        result.memory = std::move(handled.memory);
        result.audio_blocks.insert(result.audio_blocks.end(),
                                   std::make_move_iterator(handled.audio_blocks.begin()),
                                   std::make_move_iterator(handled.audio_blocks.end()));
    }
    return result;
}

} // namespace ef2000
