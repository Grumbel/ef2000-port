// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "../runtime/wav_capture.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
uint32_t le32(const std::vector<uint8_t>& data, size_t offset) {
    return unsigned(data.at(offset)) | (unsigned(data.at(offset + 1)) << 8) |
           (unsigned(data.at(offset + 2)) << 16) | (unsigned(data.at(offset + 3)) << 24);
}
std::vector<uint8_t> bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "missing WAV segment");
    return {(std::istreambuf_iterator<char>(input)), {}};
}
}

int main() {
    namespace fs = std::filesystem;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path output = fs::temp_directory_path() /
                            ("ef2000-wav-capture-test-" + std::to_string(stamp));
    try {
        ef2000::AudioBlock block;
        block.bits = 8;
        block.channels = 1;
        block.signed_pcm = true;
        block.rate = 22050;
        block.pcm = {0x80, 0, 0x7f};
        {
            ef2000::WavCapture capture(output);
            capture.push(block);
            block.bits = 16;
            block.channels = 2;
            block.signed_pcm = false;
            block.rate = 44100;
            block.pcm = {0, 0, 0xff}; // Incomplete 16-bit stereo DMA frame.
            capture.push(block);
            block.pcm = {0xff, 0, 0x80, 0xff, 0x7f};
            capture.push(block);
            require(capture.segments() == 2 && capture.bytes() == 11,
                    "original PCM WAV segment accounting");
            capture.close();
        }
        const auto first = bytes(output / "audio-000.wav");
        require(first.size() == 47 && le32(first, 4) == 39 &&
                le32(first, 40) == 3 && le32(first, 24) == 22050,
                "8-bit WAV RIFF size, data length and rate");
        require(std::vector<uint8_t>(first.begin() + 44, first.end()) ==
                    std::vector<uint8_t>({0, 128, 255}),
                "signed 8-bit DMA converts to unsigned WAV samples");
        const auto second = bytes(output / "audio-001.wav");
        require(second.size() == 52 && le32(second, 4) == 44 &&
                le32(second, 40) == 8 && le32(second, 24) == 44100,
                "16-bit WAV RIFF size, data length and rate");
        require(std::vector<uint8_t>(second.begin() + 44, second.end()) ==
                    std::vector<uint8_t>({0, 0x80, 0xff, 0x7f, 0, 0, 0xff, 0xff}),
                "unsigned 16-bit stereo DMA converts to signed WAV with split frames");
        ef2000::WavCapture duplicate(output);
        try {
            duplicate.push(block);
            throw std::runtime_error("WAV capture unexpectedly overwrote an existing segment");
        } catch (const std::runtime_error& error) {
            require(std::string(error.what()).find("refusing to overwrite") != std::string::npos,
                    "WAV capture must refuse existing filenames");
        }
        // A format transition after a partial stereo frame must not leave
        // an empty, misleading WAV file behind.
        const fs::path small = output / "partial";
        ef2000::WavCapture partial(small);
        block.generation = 7;
        block.pcm = {0x12, 0x34};
        partial.push(block);
        block.bits = 8;
        block.channels = 1;
        block.signed_pcm = false;
        block.pcm = {0x80};
        partial.push(block);
        partial.close();
        require(partial.segments() == 1 && partial.bytes() == 1 &&
                bytes(small / "audio-000.wav").size() == 45,
                "incomplete old-format PCM must not emit a WAV segment");
        fs::remove(small / "audio-000.wav");
        fs::remove(small);
        fs::remove(output / "audio-000.wav");
        fs::remove(output / "audio-001.wav");
        fs::remove(output);
        std::cout << "wav-capture: format conversions, split frames and no overwrite passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "wav-capture test: " << error.what() << '\n';
        return 1;
    }
}
