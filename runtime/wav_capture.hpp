// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#pragma once

#include "sound_blaster.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace ef2000 {

// Optional lossless capture of the original guest DMA stream. Every distinct
// rate/channel/sample-format/generation gets its own ordinary PCM WAV file.
// Destination files are never overwritten.
class WavCapture {
public:
    explicit WavCapture(std::filesystem::path directory);
    ~WavCapture();
    WavCapture(const WavCapture&) = delete;
    WavCapture& operator=(const WavCapture&) = delete;

    void push(const AudioBlock& block);
    void close();
    unsigned segments() const { return segments_; }
    uint64_t bytes() const { return bytes_; }

private:
    void begin(const AudioBlock& block);
    void open_file();
    static void put16(std::ostream& out, uint16_t value);
    static void put32(std::ostream& out, uint32_t value);

    std::filesystem::path directory_;
    std::ofstream file_;
    std::vector<uint8_t> carry_;
    uint32_t file_bytes_{};
    uint64_t bytes_{};
    unsigned segments_{}, bits_{}, channels_{}, rate_{}, generation_{};
    bool signed_pcm_{}, active_{};
};

} // namespace ef2000
