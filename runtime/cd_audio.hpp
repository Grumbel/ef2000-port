// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#pragma once

#include "real_mode_cpu.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace ef2000 {

// Read-only mixed-mode single-BINARY CUE image and the original game's
// MSCDEX INT 2Fh AX=15xx interface. Time is passed in by the caller so
// command and audio behavior can be tested without sleeping or SDL.
class CdAudio {
public:
    explicit CdAudio(const std::filesystem::path& cue);

    void interrupt(RealRegisters& regs, std::vector<uint8_t>& memory, double now);
    std::optional<AudioBlock> pcm(double now);
    unsigned generation() const { return generation_; }
    unsigned tracks() const { return unsigned(tracks_.size()); }
    uint32_t sectors() const { return sectors_; }
    uint64_t commands() const { return commands_; }
    uint64_t played_bytes() const { return played_bytes_; }

private:
    struct Track {
        unsigned number{};
        bool audio{};
        uint32_t sector{};
    };
    static void check_memory(const std::vector<uint8_t>& memory, size_t offset, size_t size);
    static uint16_t word(const std::vector<uint8_t>& memory, size_t offset);
    static uint32_t dword(const std::vector<uint8_t>& memory, size_t offset);
    static void put_word(std::vector<uint8_t>& memory, size_t offset, uint16_t value);
    static void put_msf(std::vector<uint8_t>& memory, size_t offset, uint32_t sector);
    uint32_t position(double now) const;
    bool playing(double now) const;

    std::ifstream image_;
    std::vector<Track> tracks_;
    uint32_t sectors_{}, start_{}, end_{}, cursor_{};
    double started_{};
    bool started_valid_{};
    unsigned generation_{};
    uint64_t commands_{}, played_bytes_{};
};

} // namespace ef2000
