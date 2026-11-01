// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace ef2000 {

// Implements the DOS file bridge's copy-on-write policy. DOS failures throw
// their numeric error code as unsigned; the X-32 bridge converts that to CF/AX.
class DosDrive {
public:
    DosDrive(std::filesystem::path game_root, std::filesystem::path cd_root,
             std::filesystem::path overlay_root);

    uint16_t open(const std::string& name, unsigned access, bool joystick,
                  bool digital_sound = false, bool cd_music = false);
    uint16_t create(const std::string& name);
    void close(uint16_t handle);
    std::vector<uint8_t> read(uint16_t handle, size_t size);
    size_t write(uint16_t handle, const std::vector<uint8_t>& bytes);
    uint64_t seek(uint16_t handle, int32_t offset, unsigned origin);
    void flush(uint16_t handle);
    bool is_open(uint16_t handle) const;

private:
    struct File {
        std::vector<uint8_t> bytes;
        uint64_t pos{};
        unsigned access{};
        std::filesystem::path overlay_path;
    };
    struct Paths {
        std::filesystem::path original, overlay;
        bool original_exists{}, overlay_exists{};
    };

    Paths resolve(const std::string& name) const;
    uint16_t insert(File file);
    void save(const std::filesystem::path& path, const std::vector<uint8_t>& bytes);
    File& get(uint16_t handle);

    std::filesystem::path game_root_, cd_root_, overlay_root_;
    std::map<uint16_t, File> files_;
    uint64_t temporary_counter_{};
};

} // namespace ef2000
