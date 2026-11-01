// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dos_drive.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ef2000 {
namespace {
namespace fs = std::filesystem;

constexpr size_t MAX_FILE_SIZE = 0x4000000;

bool within(const fs::path& child, const fs::path& parent) {
    const fs::path relative = child.lexically_relative(parent);
    return !relative.empty() && relative != "." && *relative.begin() != "..";
}

fs::path canonical_root(const fs::path& root) {
    return root.empty() ? fs::path{} : fs::weakly_canonical(fs::absolute(root));
}

fs::path walk(const fs::path& root, const std::vector<std::string>& parts,
              bool overlay) {
    fs::path path = root;
    for (const std::string& part : parts) {
        std::error_code error;
        fs::path next = path / part;
        for (const auto& entry : fs::directory_iterator(path, error)) {
            std::string name = entry.path().filename().string();
            for (char& c : name) c = std::toupper(static_cast<unsigned char>(c));
            if (name == part) {
                if (overlay && fs::is_symlink(entry.symlink_status())) throw unsigned(3);
                next = entry.path();
                break;
            }
        }
        if (!within(fs::weakly_canonical(next), root)) throw unsigned(3);
        path = next;
    }
    return path;
}

std::vector<uint8_t> read_bytes(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw unsigned(5);
    return {(std::istreambuf_iterator<char>(stream)), {}};
}

std::string virtual_configuration(std::string config, bool joystick, bool digital_sound,
                                  bool cd_music) {
    // The guest sees hardware settings appropriate to this backend without
    // changing the original EF2000.CFG file on disk.
    std::map<std::string, std::string> changes = {
        {"MUSIC", cd_music ? "2,127" : "0,127"},
        {"VRHELMET", "0"}, {"CONTROL", joystick ? "1" : "0"}};
    if (!digital_sound) {
        changes["SOUNDFX"] = "0,127";
        changes["SPEECH"] = "0";
    }
    const size_t factor = config.find("JOY_FACTOR=");
    if (joystick && factor != std::string::npos &&
        (factor == 0 || config[factor - 1] == '\n')) {
        const size_t start = factor + sizeof("JOY_FACTOR=") - 1;
        const size_t end = config.find_first_of("\r\n", start);
        if (config.substr(start, end - start) == "0") changes["JOY_FACTOR"] = "100";
    }
    for (const auto& [key, value] : changes) {
        const std::string prefix = key + "=";
        const size_t found = config.find(prefix);
        if (found != std::string::npos && (found == 0 || config[found - 1] == '\n')) {
            const size_t end = config.find_first_of("\r\n", found);
            config.replace(found, end == std::string::npos ? end : end - found,
                           prefix + value);
        }
    }
    return config;
}
} // namespace

DosDrive::DosDrive(fs::path game_root, fs::path cd_root, fs::path overlay_root)
    : game_root_(canonical_root(game_root)), cd_root_(canonical_root(cd_root)),
      overlay_root_(canonical_root(overlay_root)) {
    if (!overlay_root_.empty()) {
        for (const fs::path& root : {game_root_, cd_root_}) {
            if (!root.empty() && (overlay_root_ == root || within(overlay_root_, root) ||
                                  within(root, overlay_root_)))
                throw std::runtime_error("writable DOS overlay must be separate from original assets");
        }
    }
}

DosDrive::Paths DosDrive::resolve(const std::string& filename) const {
    if (filename.empty() || filename.size() >= 260) throw unsigned(3);
    std::string name = filename;
    for (char& c : name) {
        if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126)
            throw unsigned(3);
        if (c == '\\') c = '/';
        else c = std::toupper(static_cast<unsigned char>(c));
    }
    if (name.size() >= 2 && name[1] == ':') {
        if (name[0] != 'C') throw unsigned(15);
        name.erase(0, 2);
    }
    bool absolute = !name.empty() && name[0] == '/';
    while (!name.empty() && name[0] == '/') name.erase(0, 1);
    if (absolute && name.rfind("EF2000/", 0) == 0) {
        name.erase(0, 7);
        absolute = false;
    }
    std::vector<std::string> parts;
    size_t start = 0;
    while (start < name.size()) {
        const size_t slash = name.find('/', start);
        const std::string part = name.substr(start, slash == std::string::npos ?
                                                    slash : slash - start);
        if (part.empty() || part == ".." || part.find(':') != std::string::npos)
            throw unsigned(3);
        if (part != ".") parts.push_back(part);
        if (slash == std::string::npos) break;
        start = slash + 1;
        if (start == name.size()) throw unsigned(3);
    }
    if (parts.empty()) throw unsigned(3);
    const fs::path& asset_root = absolute ? cd_root_ : game_root_;
    if (asset_root.empty()) throw unsigned(2);
    Paths paths;
    paths.original = walk(asset_root, parts, false);
    paths.original_exists = fs::is_regular_file(paths.original);
    if (!overlay_root_.empty()) {
        paths.overlay = walk(absolute ? overlay_root_ : overlay_root_ / "EF2000",
                             parts, true);
        paths.overlay_exists = fs::is_regular_file(paths.overlay);
    }
    return paths;
}

uint16_t DosDrive::insert(File file) {
    for (uint16_t handle = 5; handle < 64; ++handle)
        if (!files_.count(handle)) {
            files_.emplace(handle, std::move(file));
            return handle;
        }
    throw unsigned(4);
}

uint16_t DosDrive::open(const std::string& name, unsigned access, bool joystick,
                        bool digital_sound, bool cd_music) {
    if (access > 2 || (access != 0 && overlay_root_.empty())) throw unsigned(5);
    const Paths paths = resolve(name);
    const bool overlay = paths.overlay_exists;
    if (!overlay && !paths.original_exists) throw unsigned(2);
    const fs::path& source = overlay ? paths.overlay : paths.original;
    auto bytes = read_bytes(source);
    std::string filename = source.filename().string();
    for (char& c : filename) c = std::toupper(static_cast<unsigned char>(c));
    if (access == 0 && filename == "EF2000.CFG") {
        const std::string configured = virtual_configuration(
            {bytes.begin(), bytes.end()}, joystick, digital_sound, cd_music);
        bytes.assign(configured.begin(), configured.end());
    } else if (access == 0 && cd_music && filename == "MDI.INI") {
        // Miles still loads a music driver when MUSIC=2 (CD audio), even
        // though it should never access FM/MPU-401 hardware in that mode.
        // NULL.MDI is supplied by the original game installation.
        constexpr char null_mdi[] = "DEVICE None\r\nDRIVER NULL.MDI\r\nIO_ADDR -1\r\n"
                                    "IRQ -1\r\nDMA_8_BIT -1\r\nDMA_16_BIT -1\r\n";
        bytes.assign(null_mdi, null_mdi + sizeof(null_mdi) - 1);
    }
    return insert({std::move(bytes), 0, access, access ? paths.overlay : fs::path{}});
}

uint16_t DosDrive::create(const std::string& name) {
    if (overlay_root_.empty()) throw unsigned(5);
    const Paths paths = resolve(name);
    save(paths.overlay, {});
    return insert({{}, 0, 2, paths.overlay});
}

DosDrive::File& DosDrive::get(uint16_t handle) {
    auto found = files_.find(handle);
    if (found == files_.end()) throw unsigned(6);
    return found->second;
}

bool DosDrive::is_open(uint16_t handle) const { return files_.count(handle) != 0; }

std::vector<uint8_t> DosDrive::read(uint16_t handle, size_t size) {
    File& file = get(handle);
    if (file.access == 1) throw unsigned(5);
    const size_t begin = std::min<uint64_t>(file.pos, file.bytes.size());
    const size_t length = std::min(size, file.bytes.size() - begin);
    file.pos += length;
    return {file.bytes.begin() + begin, file.bytes.begin() + begin + length};
}

size_t DosDrive::write(uint16_t handle, const std::vector<uint8_t>& bytes) {
    File& file = get(handle);
    if (file.access == 0 || file.overlay_path.empty() ||
        bytes.size() > MAX_FILE_SIZE || file.pos > MAX_FILE_SIZE - bytes.size())
        throw unsigned(5);
    if (file.pos > file.bytes.size()) file.bytes.resize(file.pos, 0);
    const size_t end = file.pos + bytes.size();
    if (end > file.bytes.size()) file.bytes.resize(end);
    std::copy(bytes.begin(), bytes.end(), file.bytes.begin() + file.pos);
    file.pos = end;
    return bytes.size();
}

uint64_t DosDrive::seek(uint16_t handle, int32_t offset, unsigned origin) {
    File& file = get(handle);
    if (origin > 2) throw unsigned(1);
    const int64_t base = origin == 0 ? 0 :
        (origin == 1 ? int64_t(file.pos) : int64_t(file.bytes.size()));
    const int64_t result = base + offset;
    if (result < 0 || result > std::numeric_limits<uint32_t>::max()) throw unsigned(1);
    return file.pos = result;
}

void DosDrive::save(const fs::path& path, const std::vector<uint8_t>& bytes) {
    if (overlay_root_.empty() || !within(fs::weakly_canonical(path), overlay_root_))
        throw unsigned(5);
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    const fs::path parent = fs::weakly_canonical(path.parent_path());
    if (error || (parent != overlay_root_ && !within(parent, overlay_root_)))
        throw unsigned(5);
    // Always replace via a new file: a hard link in the overlay must not
    // accidentally write through to its original asset inode.
    fs::path temporary;
    do {
        temporary = path;
        temporary += ".ef2000-tmp-" + std::to_string(++temporary_counter_);
    } while (fs::exists(temporary));
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw unsigned(5);
        if (!bytes.empty()) output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        output.close();
        if (!output) {
            fs::remove(temporary, error);
            throw unsigned(5);
        }
    }
    fs::rename(temporary, path, error);
    if (error) {
        fs::remove(temporary, error);
        throw unsigned(5);
    }
}

void DosDrive::flush(uint16_t handle) {
    File& file = get(handle);
    if (file.access && !file.overlay_path.empty()) save(file.overlay_path, file.bytes);
}

void DosDrive::close(uint16_t handle) {
    flush(handle);
    files_.erase(handle);
}

} // namespace ef2000
