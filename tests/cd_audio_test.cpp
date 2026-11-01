// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../runtime/cd_audio.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
constexpr unsigned SECTOR_SIZE = 2352;
constexpr size_t PACKET = 0x20000, BUFFER = 0x21000;

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
void put16(std::vector<uint8_t>& memory, size_t offset, unsigned value) {
    memory[offset] = value & 255;
    memory[offset + 1] = value >> 8;
}
void put32(std::vector<uint8_t>& memory, size_t offset, uint32_t value) {
    put16(memory, offset, value);
    put16(memory, offset + 2, value >> 16);
}
unsigned word(const std::vector<uint8_t>& memory, size_t offset) {
    return memory[offset] | (unsigned(memory[offset + 1]) << 8);
}

struct Fixture {
    fs::path dir;
    Fixture() {
        for (unsigned i = 0; i != 1000; ++i) {
            dir = fs::temp_directory_path() /
                ("ef2000-cd-test-" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()) +
                 "-" + std::to_string(i));
            if (fs::create_directory(dir)) break;
        }
        require(fs::is_directory(dir), "cannot create fixture directory");
        std::ofstream cue(dir / "disc.cue");
        cue << "FILE \"disc.bin\" BINARY\n"
               "  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n"
               "  TRACK 02 AUDIO\n    INDEX 01 00:00:20\n";
        cue.close();
        std::ofstream image(dir / "disc.bin", std::ios::binary);
        for (unsigned sector = 0; sector != 120; ++sector) {
            std::vector<uint8_t> data(SECTOR_SIZE);
            if (sector == 16) {
                data[16] = 1;
                for (unsigned i = 0; i != 5; ++i) data[17 + i] = "CD001"[i];
            }
            if (sector >= 20) {
                for (unsigned i = 0; i < SECTOR_SIZE; ++i) data[i] = uint8_t(i ^ sector);
            }
            image.write(reinterpret_cast<const char*>(data.data()), data.size());
        }
        require(bool(image), "cannot write CD fixture");
    }
    ~Fixture() {
        std::error_code ignored;
        fs::remove_all(dir, ignored);
    }
};

ef2000::RealRegisters request(unsigned ax) {
    ef2000::RealRegisters regs{};
    regs.ax = ax;
    regs.cx = 3;
    regs.es = PACKET >> 4;
    regs.bx = PACKET & 15;
    regs.flags = 1;
    return regs;
}
void packet(std::vector<uint8_t>& memory, unsigned command) {
    std::fill(memory.begin() + PACKET, memory.begin() + PACKET + 26, 0);
    memory[PACKET + 2] = command;
    put16(memory, PACKET + 14, BUFFER & 15);
    put16(memory, PACKET + 16, BUFFER >> 4);
    put16(memory, PACKET + 18, 16);
}
unsigned ioctl(ef2000::CdAudio& cd, std::vector<uint8_t>& memory, unsigned sub) {
    packet(memory, 3);
    memory[BUFFER] = sub;
    if (sub == 11) memory[BUFFER + 1] = 2;
    auto regs = request(0x1510);
    cd.interrupt(regs, memory, 100);
    require(!(regs.flags & 1), "MSCDEX IOCTL carry flag");
    return word(memory, PACKET + 3);
}
} // namespace

int main() {
    try {
        Fixture fixture;
        ef2000::CdAudio cd(fixture.dir / "disc.cue");
        std::vector<uint8_t> memory(0x100000);
        require(cd.tracks() == 2 && cd.sectors() == 120, "CUE track discovery");

        auto regs = request(0x1500);
        cd.interrupt(regs, memory, 100);
        require(regs.bx == 1 && regs.cx == 3 && !(regs.flags & 1), "MSCDEX drive discovery");
        regs = request(0x150c);
        cd.interrupt(regs, memory, 100);
        require(regs.bx == 0x020a, "MSCDEX version");
        regs = request(0x150b);
        cd.interrupt(regs, memory, 100);
        require(regs.ax == 1 && regs.bx == 0xadad, "MSCDEX CD drive validation");
        regs = request(0x1505);
        regs.dx = 0;
        cd.interrupt(regs, memory, 100);
        require(regs.ax == 1 && std::string(memory.begin() + PACKET + 1,
                                            memory.begin() + PACKET + 6) == "CD001",
                "MSCDEX raw volume descriptor");

        require(ioctl(cd, memory, 10) == 0x100, "disc information IOCTL status");
        require(memory[BUFFER] == 10 && memory[BUFFER + 1] == 1 &&
                memory[BUFFER + 2] == 2 && memory[BUFFER + 3] == 45 &&
                memory[BUFFER + 4] == 3 && memory[BUFFER + 5] == 0,
                "disc information MSF lead-out");
        require(ioctl(cd, memory, 11) == 0x100, "track information IOCTL status");
        require(memory[BUFFER + 1] == 2 && memory[BUFFER + 2] == 20 &&
                memory[BUFFER + 6] == 0, "audio track MSF");
        require(ioctl(cd, memory, 6) == 0x100 &&
                memory[BUFFER + 1] == 0x90, "MSCDEX device capabilities");
        require(ioctl(cd, memory, 9) == 0x100 && memory[BUFFER + 1] == 1,
                "MSCDEX media status");

        packet(memory, 0x84);
        put32(memory, PACKET + 14, 0);
        put32(memory, PACKET + 18, 10);
        regs = request(0x1510);
        cd.interrupt(regs, memory, 100);
        require(word(memory, PACKET + 3) == 0x8108, "data track must not play as audio");
        require(!cd.pcm(100.5), "invalid play must not produce audio");

        packet(memory, 0x84);
        put32(memory, PACKET + 14, 20);
        put32(memory, PACKET + 18, 30);
        regs = request(0x1510);
        cd.interrupt(regs, memory, 100);
        require(word(memory, PACKET + 3) == 0x300 && cd.generation() == 1,
                "CD play status and generation");
        auto early = cd.pcm(100 + 1.0 / 75.0 + 0.0001);
        require(early && early->pcm.size() == SECTOR_SIZE && early->rate == 44100 &&
                early->bits == 16 && early->channels == 2 && early->signed_pcm,
                "first sector of signed stereo 44.1 kHz Red Book PCM");
        require(early->pcm[0] == 20 && early->pcm[1] == 21,
                "CD PCM sector bytes must be unchanged");
        auto rest = cd.pcm(101);
        require(rest && rest->pcm.size() == 29 * SECTOR_SIZE,
                "elapsed CD audio must be capped to play range");
        require(!cd.pcm(102), "completed play must not duplicate sectors");

        packet(memory, 0x85);
        regs = request(0x1510);
        cd.interrupt(regs, memory, 102);
        require(cd.generation() == 2 && word(memory, PACKET + 3) == 0x100,
                "stop clears playing status");
        require(!cd.pcm(103), "stop must not emit stale PCM");

        packet(memory, 12);
        memory[BUFFER] = 2;
        regs = request(0x1510);
        cd.interrupt(regs, memory, 104);
        require(cd.generation() == 3 && word(memory, PACKET + 3) == 0x100,
                "reset clears playback");
        require(cd.played_bytes() == 30 * SECTOR_SIZE && cd.commands() == 8,
                "CD command and PCM diagnostics");

        std::ofstream bad(fixture.dir / "bad.cue");
        bad << "FILE \"disc.bin\" BINARY\n TRACK 01 MODE2/2352\n"
               " INDEX 01 00:00:00\n";
        bad.close();
        bool unsupported_rejected = false;
        try { ef2000::CdAudio invalid(fixture.dir / "bad.cue"); }
        catch (const std::runtime_error&) { unsupported_rejected = true; }
        require(unsupported_rejected, "unsupported CUE track must be rejected");

        std::cout << "cd-audio: MSCDEX requests and read-only Red Book PCM passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "cd-audio test: " << error.what() << '\n';
        return 1;
    }
}
