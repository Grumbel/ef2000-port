// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "cd_audio.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ef2000 {
namespace {
constexpr size_t SECTOR_BYTES = 2352;
constexpr unsigned FRAMES_PER_SECOND = 75;

uint32_t parse_sector(const std::smatch& parts) {
    const auto minutes = std::stoul(parts[1].str());
    const auto seconds = std::stoul(parts[2].str());
    const auto frames = std::stoul(parts[3].str());
    if (seconds >= 60 || frames >= FRAMES_PER_SECOND ||
        minutes > (std::numeric_limits<uint32_t>::max() / 4500))
        throw std::runtime_error("invalid CUE INDEX position");
    const uint64_t position = (uint64_t(minutes) * 60 + seconds) * FRAMES_PER_SECOND + frames;
    if (position > std::numeric_limits<uint32_t>::max())
        throw std::runtime_error("CUE INDEX position overflow");
    return uint32_t(position);
}
}

CdAudio::CdAudio(const std::filesystem::path& cue) {
    std::ifstream source(cue);
    if (!source) throw std::runtime_error("cannot read CD CUE: " + cue.string());
    std::string text((std::istreambuf_iterator<char>(source)), {});
    const std::regex file_pattern(R"re(^\s*FILE\s+"([^"]+)"\s+BINARY\s*$)re",
                                  std::regex_constants::icase);
    const std::regex track_pattern(R"re(^\s*TRACK\s+(\d+)\s+(AUDIO|MODE1/2352)\s*$)re",
                                   std::regex_constants::icase);
    const std::regex index_pattern(R"re(^\s*INDEX\s+0?1\s+(\d+):(\d+):(\d+)\s*$)re",
                                   std::regex_constants::icase);
    const std::regex unsupported_pattern(R"re(^\s*(FILE|TRACK|INDEX\s+0?1)\b)re",
                                         std::regex_constants::icase);
    std::filesystem::path image_path;
    std::istringstream lines(text);
    std::string line;
    unsigned file_count = 0;
    std::vector<bool> indices;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::smatch parts;
        if (std::regex_match(line, parts, file_pattern)) {
            ++file_count;
            image_path = cue.parent_path() / parts[1].str();
        } else if (std::regex_match(line, parts, track_pattern)) {
            const auto number = std::stoul(parts[1].str());
            if (number != tracks_.size() + 1)
                throw std::runtime_error("CUE tracks must be consecutive starting at 1");
            std::string kind = parts[2].str();
            std::transform(kind.begin(), kind.end(), kind.begin(),
                           [](unsigned char ch) { return char(std::toupper(ch)); });
            tracks_.push_back({unsigned(number), kind == "AUDIO", 0});
            indices.push_back(false);
        } else if (std::regex_match(line, parts, index_pattern)) {
            if (tracks_.empty() || indices.back())
                throw std::runtime_error("unexpected CUE INDEX 01");
            tracks_.back().sector = parse_sector(parts);
            indices.back() = true;
        } else if (std::regex_search(line, unsupported_pattern)) {
            throw std::runtime_error("unsupported CD CUE declaration: " + line);
        }
    }
    if (file_count != 1 || tracks_.empty() ||
        std::find(indices.begin(), indices.end(), false) != indices.end())
        throw std::runtime_error("CD audio requires one BINARY image with indexed tracks");
    image_.open(image_path, std::ios::binary | std::ios::ate);
    if (!image_) throw std::runtime_error("cannot read CD image: " + image_path.string());
    const auto bytes = image_.tellg();
    if (bytes <= 0 || uint64_t(bytes) % SECTOR_BYTES ||
        uint64_t(bytes) / SECTOR_BYTES > std::numeric_limits<uint32_t>::max())
        throw std::runtime_error("CD image must contain complete 2352-byte sectors");
    sectors_ = uint32_t(uint64_t(bytes) / SECTOR_BYTES);
    for (size_t i = 0; i < tracks_.size(); ++i)
        if (tracks_[i].sector >= sectors_ ||
            (i && tracks_[i - 1].sector >= tracks_[i].sector))
            throw std::runtime_error("CUE track positions outside image or not increasing");
    image_.seekg(0);
}

void CdAudio::check_memory(const std::vector<uint8_t>& memory, size_t offset, size_t size) {
    if (offset > memory.size() || size > memory.size() - offset)
        throw std::runtime_error("MSCDEX request outside DOS conventional memory");
}
uint16_t CdAudio::word(const std::vector<uint8_t>& memory, size_t offset) {
    check_memory(memory, offset, 2);
    return uint16_t(memory[offset] | (unsigned(memory[offset + 1]) << 8));
}
uint32_t CdAudio::dword(const std::vector<uint8_t>& memory, size_t offset) {
    return uint32_t(word(memory, offset)) | (uint32_t(word(memory, offset + 2)) << 16);
}
void CdAudio::put_word(std::vector<uint8_t>& memory, size_t offset, uint16_t value) {
    check_memory(memory, offset, 2);
    memory[offset] = value & 255;
    memory[offset + 1] = value >> 8;
}
void CdAudio::put_msf(std::vector<uint8_t>& memory, size_t offset, uint32_t sector) {
    check_memory(memory, offset, 4);
    const uint64_t frame = uint64_t(sector) + 150;
    memory[offset] = frame % 75;
    memory[offset + 1] = frame / 75 % 60;
    memory[offset + 2] = frame / 4500;
    memory[offset + 3] = 0;
}

uint32_t CdAudio::position(double now) const {
    if (!started_valid_) return cursor_;
    const auto elapsed = std::max(0.0, now - started_);
    const auto progressed = uint64_t(std::min(elapsed * 75.0, double(end_ - start_)));
    return std::min(end_, uint32_t(start_ + progressed));
}
bool CdAudio::playing(double now) const {
    return started_valid_ && position(now) < end_;
}

void CdAudio::interrupt(RealRegisters& regs, std::vector<uint8_t>& memory, double now) {
    const auto ax = regs.ax & 0xffff;
    if (ax == 0x1500) {
        regs.bx = 1;
        regs.cx = 3; // D:
    } else if (ax == 0x150c) {
        regs.bx = 0x020a;
    } else if (ax == 0x150b) {
        regs.ax = (regs.cx & 0xffff) == 3 ? 1 : 0;
        regs.bx = 0xadad;
    } else if (ax == 0x1505 && (regs.cx & 0xffff) == 3) {
        const auto address = size_t(regs.es) * 16 + (regs.bx & 0xffff);
        check_memory(memory, address, 2048);
        const auto sector = 16 + (regs.dx & 0xffff);
        if (sector >= sectors_) {
            regs.ax = 0xff;
        } else {
            image_.clear();
            image_.seekg(uint64_t(sector) * SECTOR_BYTES + 16);
            image_.read(reinterpret_cast<char*>(memory.data() + address), 2048);
            if (!image_) throw std::runtime_error("CD volume descriptor read failed");
            regs.ax = std::string(reinterpret_cast<const char*>(memory.data() + address + 1), 5)
                == "CD001" ? 1 : 0xff;
        }
    } else if (ax == 0x1510 && (regs.cx & 0xffff) == 3) {
        ++commands_;
        const auto address = size_t(regs.es) * 16 + (regs.bx & 0xffff);
        check_memory(memory, address, 22);
        const uint8_t command = memory[address + 2];
        uint16_t status = 0x100;
        if (command == 3 || command == 12) {
            const size_t data = size_t(word(memory, address + 16)) * 16 +
                                word(memory, address + 14);
            const size_t size = word(memory, address + 18);
            check_memory(memory, data, size);
            const int sub = size ? memory[data] : -1;
            std::vector<uint8_t> result;
            if (command == 3) {
                if (sub == 10) {
                    result = {10, 1, uint8_t(tracks_.size()), 0, 0, 0, 0};
                    put_msf(result, 3, sectors_);
                } else if (sub == 11 && size >= 2 &&
                           memory[data + 1] > 0 && memory[data + 1] <= tracks_.size()) {
                    const auto& track = tracks_[memory[data + 1] - 1];
                    result = {11, uint8_t(track.number), 0, 0, 0, 0,
                              uint8_t(track.audio ? 0 : 0x40)};
                    put_msf(result, 2, track.sector);
                } else if (sub == 6) {
                    result = {6, 0x90, 0x02, 0, 0};
                } else if (sub == 9) {
                    result = {9, 1};
                } else if (sub == 12) {
                    result = std::vector<uint8_t>(11, 0);
                    result[0] = 12;
                }
                if (result.empty() || result.size() > size) status = 0x8103;
                else std::copy(result.begin(), result.end(), memory.begin() + data);
            } else if (sub == 2) {
                started_valid_ = false;
                ++generation_;
            } else status = 0x8103;
        } else if (command == 0x84) {
            const auto mode = memory[address + 13];
            uint32_t start = dword(memory, address + 14);
            const auto length = dword(memory, address + 18);
            if (mode == 1)
                start = ((start >> 16) & 255) * 4500 +
                        ((start >> 8) & 255) * 75 + (start & 255) - 150;
            const uint64_t end = uint64_t(start) + length;
            const auto track = std::find_if(tracks_.rbegin(), tracks_.rend(),
                [start](const Track& item) { return item.sector <= start; });
            if (mode > 1 || track == tracks_.rend() || !track->audio ||
                !length || end > sectors_) status = 0x8108;
            else {
                start_ = cursor_ = start;
                end_ = uint32_t(end);
                started_ = now;
                started_valid_ = true;
                ++generation_;
            }
        } else if (command == 0x85) {
            cursor_ = position(now);
            started_valid_ = false;
            ++generation_;
        } else status = 0x8103;
        if (playing(now)) status |= 0x200;
        put_word(memory, address + 3, status);
    } else {
        regs.ax = 15;
        regs.flags |= 1;
        return;
    }
    regs.flags &= ~1u;
}

std::optional<AudioBlock> CdAudio::pcm(double now) {
    const auto end = position(now);
    if (end <= cursor_) return {};
    const size_t bytes = size_t(end - cursor_) * SECTOR_BYTES;
    AudioBlock block;
    block.bits = 16;
    block.channels = 2;
    block.signed_pcm = true;
    block.rate = 44100;
    block.generation = generation_;
    block.gains = {1.0, 1.0};
    block.pcm.resize(bytes);
    image_.clear();
    image_.seekg(uint64_t(cursor_) * SECTOR_BYTES);
    image_.read(reinterpret_cast<char*>(block.pcm.data()), bytes);
    if (!image_) throw std::runtime_error("CD audio PCM read failed");
    cursor_ = end;
    played_bytes_ += bytes;
    return block;
}

} // namespace ef2000
