// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../runtime/dos_drive.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;

void require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}

template <class Function> void expect_error(unsigned code, Function action) {
    try { action(); }
    catch (unsigned actual) {
        require(actual == code, "unexpected DOS error code");
        return;
    }
    throw std::runtime_error("expected DOS error was not raised");
}

void write_file(const fs::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary);
    require(bool(output), "cannot create test fixture");
    output.write(text.data(), text.size());
    require(bool(output), "cannot write test fixture");
}

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "cannot read test fixture");
    return {(std::istreambuf_iterator<char>(input)), {}};
}

std::string bytes(const std::vector<uint8_t>& data) {
    return {data.begin(), data.end()};
}

std::vector<uint8_t> bytes(const std::string& text) {
    return {text.begin(), text.end()};
}

struct TemporaryDirectory {
    fs::path path;
    TemporaryDirectory() {
        for (unsigned attempt = 0; attempt < 1000; ++attempt) {
            path = fs::temp_directory_path() /
                ("ef2000-dos-drive-test-" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()) +
                 "-" + std::to_string(attempt));
            if (fs::create_directory(path)) return;
        }
        throw std::runtime_error("cannot allocate disposable test directory");
    }
    ~TemporaryDirectory() {
        if (!path.empty()) {
            std::error_code error;
            fs::remove_all(path, error);
        }
    }
};
} // namespace

int main() {
    const char* stage = "setup";
    try {
        TemporaryDirectory temp;
        const fs::path game = temp.path / "game";
        const fs::path cd = temp.path / "cd";
        const fs::path state = temp.path / "state";
        fs::create_directories(game);
        fs::create_directories(cd);
        write_file(game / "CONFIG.DAT", "abcdefgh");
        write_file(game / "EF2000.CFG", "CONTROL=1\r\nJOY_FACTOR=0\r\nMUSIC=1,127\r\n"
                                       "SOUNDFX=1,127\r\nSPEECH=1\r\n");
        write_file(cd / "CHECK.CD", "disc");

        stage = "read-only files";
        ef2000::DosDrive readonly(game, cd, {});
        auto handle = readonly.open("config.dat", 0, false);
        require(bytes(readonly.read(handle, 8)) == "abcdefgh", "original case-insensitive read");
        expect_error(5, [&] { readonly.write(handle, bytes("X")); });
        readonly.close(handle);
        expect_error(5, [&] { readonly.open("CONFIG.DAT", 2, false); });
        expect_error(5, [&] { readonly.create("NEW.DAT"); });
        handle = readonly.open("\\CHECK.CD", 0, false);
        require(bytes(readonly.read(handle, 4)) == "disc", "separate CD root");
        readonly.close(handle);
        expect_error(15, [&] { readonly.open("D:\\CHECK.CD", 0, false); });
        expect_error(3, [&] { readonly.open("..\\CONFIG.DAT", 0, false); });

        stage = "overlay original copy";
        ef2000::DosDrive drive(game, cd, state);
        handle = drive.open("CONFIG.DAT", 2, false);
        require(bytes(drive.read(handle, 3)) == "abc", "read before copy-on-write");
        require(drive.write(handle, bytes("XY")) == 2, "write to virtual copy");
        drive.flush(handle);
        drive.close(handle);
        require(read_file(game / "CONFIG.DAT") == "abcdefgh", "original asset changed");
        require(read_file(state / "EF2000/CONFIG.DAT") == "abcXYfgh", "overlay copy-on-write");
        handle = drive.open("config.dat", 0, false);
        require(bytes(drive.read(handle, 8)) == "abcXYfgh", "overlay takes precedence");
        expect_error(5, [&] { drive.write(handle, bytes("X")); });
        drive.close(handle);

        stage = "absolute DOS drive create";
        handle = drive.create("C:\\TEST.$$$");
        require(drive.write(handle, bytes("hi")) == 2, "created file write");
        require(drive.seek(handle, 4, 0) == 4, "absolute DOS seek");
        require(drive.write(handle, bytes("!")) == 1, "sparse write");
        drive.close(handle);
        require(read_file(state / "TEST.$$$") == std::string("hi\0\0!", 5),
                "absolute drive overlay and sparse gap");
        handle = drive.open("C:\\TEST.$$$", 1, false);
        expect_error(5, [&] { drive.read(handle, 1); });
        require(drive.seek(handle, -1, 2) == 4, "relative end seek");
        expect_error(1, [&] { drive.seek(handle, -100, 0); });
        expect_error(1, [&] { drive.seek(handle, 0, 3); });
        drive.close(handle);

        stage = "nested create";
        handle = drive.create("subdir\\NEW.DAT");
        drive.write(handle, bytes("nested"));
        drive.close(handle);
        require(read_file(state / "EF2000/SUBDIR/NEW.DAT") == "nested", "nested create");
        handle = drive.open("SuBdIr\\new.dat", 0, false);
        require(bytes(drive.read(handle, 6)) == "nested", "case-insensitive overlay lookup");
        drive.close(handle);
        expect_error(3, [&] { drive.create("..\\escape"); });
        expect_error(3, [&] { drive.create("subdir/../escape"); });
        std::error_code symlink_error;
        fs::create_directory_symlink(game, state / "EF2000/LINK_OUT", symlink_error);
        if (!symlink_error)
            expect_error(3, [&] { drive.create("LINK_OUT/ESCAPE.DAT"); });

        stage = "virtual configuration";
        handle = drive.open("EF2000.CFG", 0, false);
        const std::string keyboard_cfg = bytes(drive.read(handle, 256));
        drive.close(handle);
        require(keyboard_cfg.find("CONTROL=0\r\n") != std::string::npos,
                "no joystick must select keyboard control");
        require(keyboard_cfg.find("MUSIC=0,127\r\n") != std::string::npos,
                "original sound must remain disabled");
        handle = drive.open("EF2000.CFG", 0, true);
        const std::string joystick_cfg = bytes(drive.read(handle, 256));
        drive.close(handle);
        require(joystick_cfg.find("CONTROL=1\r\n") != std::string::npos &&
                joystick_cfg.find("JOY_FACTOR=100\r\n") != std::string::npos,
                "attached joystick virtual calibration");
        require(read_file(game / "EF2000.CFG").find("JOY_FACTOR=0") != std::string::npos,
                "game configuration was modified");
        handle = drive.open("EF2000.CFG", 0, false, true);
        const std::string probe_cfg = bytes(drive.read(handle, 256));
        drive.close(handle);
        require(probe_cfg.find("SOUNDFX=1,127\r\n") != std::string::npos &&
                probe_cfg.find("SPEECH=1\r\n") != std::string::npos &&
                probe_cfg.find("MUSIC=0,127\r\n") != std::string::npos,
                "SB16 probe must enable digital effects without FM music");

        // Replacing an overlay hard link must not modify the original inode.
        stage = "hard link protection";
        fs::create_hard_link(game / "CONFIG.DAT", state / "EF2000/HARDLINK.DAT");
        handle = drive.open("HARDLINK.DAT", 2, false);
        drive.write(handle, bytes("X"));
        drive.close(handle);
        require(read_file(game / "CONFIG.DAT") == "abcdefgh", "overlay hard link escaped");
        require(read_file(state / "EF2000/HARDLINK.DAT") == "Xbcdefgh", "hard link replacement");

        stage = "overlap rejection";
        try {
            ef2000::DosDrive invalid(game, cd, game / "save");
            throw std::runtime_error("overlay inside original game root was accepted");
        } catch (const std::runtime_error& failure) {
            require(std::string(failure.what()).find("separate") != std::string::npos,
                    "overlap rejection error");
        }
        std::cout << "dos-drive: original assets unchanged; overlay read/write tests passed\n";
        return 0;
    } catch (unsigned error) {
        std::cerr << "dos-drive self-test: DOS error " << error << " at " << stage << '\n';
        return 1;
    } catch (const std::exception& failure) {
        std::cerr << "dos-drive self-test: " << failure.what() << " at " << stage << '\n';
        return 1;
    }
}
