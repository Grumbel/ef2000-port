// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Native C++ Unicorn/X-32 runner. This is deliberately a bounded diagnostic
// during the Python-to-C++ transition; unsupported guest calls stop explicitly.
// The proprietary executable is read at runtime and is never linked in.
#include <unicorn/unicorn.h>
#include <unicorn/x86.h>
#include <openssl/sha.h>
#ifdef EF_UNICORN_SDL3
#include <SDL3/SDL.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace fs = std::filesystem;
constexpr uint32_t IMAGE_SIZE = 0x4000000;
constexpr uint32_t INITIAL_SP = IMAGE_SIZE - 0x1000;
constexpr uint32_t ENTRY = 0x86;
constexpr uint32_t DOS_BASE = 0x05000000;
constexpr uint32_t PSP_BASE = 0x05200000;
constexpr uint32_t ENV_BASE = PSP_BASE + 0x1000;
constexpr uint32_t EXTENDER_BASE = PSP_BASE + 0x2000;
constexpr uint32_t TABLE_BASE = IMAGE_SIZE + 0x1000;
constexpr char SOURCE_SHA256[] = "f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd";

uint16_t le16(const std::vector<uint8_t>& data, size_t pos) {
    if (pos + 2 > data.size()) throw std::runtime_error("truncated executable header");
    return uint16_t(data[pos] | (uint16_t(data[pos + 1]) << 8));
}
uint32_t le32(const std::vector<uint8_t>& data, size_t pos) {
    return uint32_t(le16(data, pos)) | (uint32_t(le16(data, pos + 2)) << 16);
}

class StartupWindow {
public:
    explicit StartupWindow(bool enabled) {
        if (!enabled) return;
#ifdef EF_UNICORN_SDL3
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(std::string("SDL_Init: ") + SDL_GetError());
        window_ = SDL_CreateWindow("EF2000 / Unicorn C++ (early development)", 1280, 960,
                                   SDL_WINDOW_RESIZABLE);
        if (!window_) throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) throw std::runtime_error(std::string("SDL_CreateRenderer: ") + SDL_GetError());
        if (!SDL_SetRenderLogicalPresentation(renderer_, 640, 480,
                                              SDL_LOGICAL_PRESENTATION_LETTERBOX))
            throw std::runtime_error(std::string("SDL_SetRenderLogicalPresentation: ") + SDL_GetError());
#else
        throw std::runtime_error("this binary was built without SDL3; remove --windowed");
#endif
    }
    ~StartupWindow() {
#ifdef EF_UNICORN_SDL3
        if (texture_) SDL_DestroyTexture(texture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        if (window_) SDL_Quit();
#endif
    }
    bool enabled() const {
#ifdef EF_UNICORN_SDL3
        return window_ != nullptr;
#else
        return false;
#endif
    }
    bool refresh(uint64_t instructions, uint64_t interrupts, const std::string& status,
                 const std::vector<uint8_t>* frame = nullptr, int width = 0, int height = 0) {
#ifdef EF_UNICORN_SDL3
        if (!enabled()) return true;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                return false;
        }
        if (!SDL_SetRenderDrawColor(renderer_, 10, 18, 30, 255) || !SDL_RenderClear(renderer_))
            throw std::runtime_error(std::string("SDL3 clear failed: ") + SDL_GetError());
        bool has_image = false;
        if (frame) {
            for (size_t i = 0; i + 3 < frame->size(); i += 4) {
                if ((*frame)[i] || (*frame)[i + 1] || (*frame)[i + 2]) {
                    has_image = true;
                    break;
                }
            }
        }
        if (has_image) {
            if (!texture_ || width != texture_width_ || height != texture_height_) {
                if (texture_) SDL_DestroyTexture(texture_);
                texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING, width, height);
                if (!texture_) throw std::runtime_error(std::string("SDL3 texture: ") + SDL_GetError());
                texture_width_ = width;
                texture_height_ = height;
                SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
            }
            const SDL_FRect destination{0, 0, 640, 480};
            if (!SDL_UpdateTexture(texture_, nullptr, frame->data(), width * 4) ||
                !SDL_RenderTexture(renderer_, texture_, nullptr, &destination))
                throw std::runtime_error(std::string("SDL3 video present failed: ") + SDL_GetError());
        } else {
            if (!SDL_SetRenderDrawColor(renderer_, 230, 237, 245, 255) ||
                !SDL_RenderDebugText(renderer_, 30, 30, "EF2000 - NATIVE C++ / UNICORN") ||
                !SDL_RenderDebugText(renderer_, 30, 55, "WAITING FOR NONBLACK GAME VIDEO") ||
                !SDL_RenderDebugText(renderer_, 30, 95, status.c_str()))
                throw std::runtime_error(std::string("SDL3 text draw failed: ") + SDL_GetError());
            const std::string progress = "GUEST INSTRUCTION BUDGET: " + std::to_string(instructions);
            const std::string events = "DOS/X32 INTERRUPTS: " + std::to_string(interrupts);
            if (!SDL_RenderDebugText(renderer_, 30, 125, progress.c_str()) ||
                !SDL_RenderDebugText(renderer_, 30, 145, events.c_str()))
                throw std::runtime_error(std::string("SDL3 text draw failed: ") + SDL_GetError());
        }
        if (!SDL_RenderPresent(renderer_))
            throw std::runtime_error(std::string("SDL3 present failed: ") + SDL_GetError());
        return true;
#else
        (void)instructions; (void)interrupts; (void)status; (void)frame; (void)width; (void)height;
        return true;
#endif
    }
private:
#ifdef EF_UNICORN_SDL3
    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    SDL_Texture* texture_{};
    int texture_width_{}, texture_height_{};
#endif
};
void put16(std::vector<uint8_t>& data, size_t pos, uint16_t value) {
    data.at(pos) = value & 255;
    data.at(pos + 1) = value >> 8;
}
void put32(std::vector<uint8_t>& data, size_t pos, uint32_t value) {
    put16(data, pos, value & 0xffff);
    put16(data, pos + 2, value >> 16);
}

std::vector<uint8_t> payload_from_executable(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open original executable: " + path);
    const std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
    if (data.size() < 0x80 || le16(data, 0) != 0x5a4d ||
        std::memcmp(data.data() + 0x28, "B23X", 4) != 0)
        throw std::runtime_error("not a supported EF2000 X-32 executable");
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), digest);
    char hex[SHA256_DIGEST_LENGTH * 2 + 1]{};
    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        std::snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    if (std::strcmp(hex, SOURCE_SHA256))
        throw std::runtime_error("unsupported EF2000 executable SHA-256");
    const uint32_t first_stage = uint32_t(le16(data, 8)) * 16;
    const uint32_t stage2 = le16(data, first_stage);
    const uint32_t stage2_size = le16(data, first_stage + 2);
    const uint32_t payload_start = stage2 + stage2_size;
    const uint32_t stage2_data = uint32_t(le16(data, first_stage + 0x12)) * 16;
    if (first_stage != 0x30 || stage2 >= payload_start ||
        payload_start > data.size() || stage2_data + 12 > stage2_size ||
        uint64_t(stage2) + stage2_data + 12 > data.size() ||
        le32(data, stage2 + stage2_data + 4) != data.size() - payload_start)
        throw std::runtime_error("invalid X-32 payload layout");
    return {data.begin() + payload_start, data.end()};
}

std::array<uint8_t, 8> segment(uint32_t base, uint32_t limit, uint8_t access,
                               uint8_t flags = 0xc) {
    return {uint8_t(limit), uint8_t(limit >> 8), uint8_t(base), uint8_t(base >> 8),
            uint8_t(base >> 16), access, uint8_t((limit >> 16 & 15) | (flags << 4)),
            uint8_t(base >> 24)};
}

struct Registers {
    uint32_t ax{}, bx{}, cx{}, dx{}, si{}, di{}, bp{}, sp{}, ip{}, flags{};
    uint32_t ds{}, es{}, ss{};
};
constexpr int REG_IDS[] = {UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
    UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS,
    UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS};
static_assert(sizeof(Registers) == sizeof(REG_IDS) / sizeof(int) * sizeof(uint32_t));

class Machine {
public:
    explicit Machine(const std::vector<uint8_t>& image, fs::path game_dir, fs::path cd_dir,
                     bool trace_interrupts = false)
        : image_size_(image.size()), trace_interrupts_(trace_interrupts),
          game_dir_(std::move(game_dir)), cd_dir_(std::move(cd_dir)) {
        if (image.size() < ENTRY + 2 || image.size() > IMAGE_SIZE)
            throw std::runtime_error("invalid initialized image length");
        check(uc_open(UC_ARCH_X86, UC_MODE_32, &cpu_), "uc_open");
        check(uc_mem_map(cpu_, 0, IMAGE_SIZE, UC_PROT_ALL), "map guest image");
        check(uc_mem_map(cpu_, DOS_BASE, 0x100000, UC_PROT_ALL), "map DOS RAM");
        check(uc_mem_map(cpu_, PSP_BASE, 0x3000, UC_PROT_ALL), "map PSP");
        check(uc_mem_map(cpu_, IMAGE_SIZE, 0x2000, UC_PROT_ALL), "map tables");
        write(0, image.data(), image.size());
        initialize_memory();
        initialize_segments();
        uc_hook hook{};
        check(uc_hook_add(cpu_, &hook, UC_HOOK_INTR, (void*)on_interrupt, this, 1, 0),
              "install interrupt hook");
        check(uc_hook_add(cpu_, &hook, UC_HOOK_INSN, (void*)on_port_in, this,
                          1, 0, UC_X86_INS_IN), "install port IN hook");
        check(uc_hook_add(cpu_, &hook, UC_HOOK_INSN, (void*)on_port_out, this,
                          1, 0, UC_X86_INS_OUT), "install port OUT hook");
    }
    Machine(const Machine&) = delete;
    Machine& operator=(const Machine&) = delete;
    ~Machine() { if (cpu_) uc_close(cpu_); }

    int run(uint64_t limit, bool windowed, uint64_t timer_interval,
            const std::string& framebuffer) {
        StartupWindow window(windowed);
        std::cout << "C++ X-32: original payload " << image_size_ << " bytes; "
                  << "instruction budget " << limit << '\n';
        if (!refresh(window, 0, "INITIALIZING X-32 GUEST")) return 0;
        // Batch execution leaves the CPU in native Unicorn's C core for most
        // instructions while still delivering deterministic PIT ticks. A
        // 1,000-instruction quantum bounds the EOI-to-return observation gap.
        uint64_t completed = 0;
        bool running = true;
        while (running && completed < limit && status_ == "instruction_limit") {
            uint64_t amount = std::min<uint64_t>(1000, limit - completed);
            amount = std::min(amount, timer_interval - completed % timer_interval);
            uint32_t ip{};
            check(uc_reg_read(cpu_, UC_X86_REG_EIP, &ip), "read instruction pointer");
            check(uc_emu_start(cpu_, ip, 0, 0, amount), "guest execution");
            if (status_ != "instruction_limit") break;
            completed += amount;
            if (eoi_hook_ && !irq_in_service_) {
                check(uc_hook_del(cpu_, eoi_hook_), "remove IRQ return hook");
                eoi_hook_ = 0;
            }
            if (completed % timer_interval == 0) deliver_tick(ip_at_cpu());
            if (irq_in_service_ && !eoi_hook_) {
                check(uc_hook_add(cpu_, &eoi_hook_, UC_HOOK_CODE, (void*)on_irq_resume,
                                  this, irq_resume_ip_, irq_resume_ip_), "hook IRQ return");
                check(uc_ctl_remove_cache(cpu_, irq_resume_ip_, irq_resume_ip_ + 1),
                      "invalidate IRQ return translation");
            }
            if (window.enabled() && completed % 50000 == 0)
                running = refresh(window, completed, "RUNNING GUEST STARTUP");
        }
        if (!framebuffer.empty()) save_framebuffer(framebuffer);
        if (window.enabled()) {
            if (!running) return 0;
            while (refresh(window, completed, "GUEST STOPPED: " + status_ + " / CLOSE WINDOW TO EXIT")) {
#ifdef EF_UNICORN_SDL3
                SDL_Delay(25);
#endif
            }
        }
        const Registers regs = read_regs();
        std::cout << "status=" << status_ << " interrupts=" << interrupts_
                  << " ticks=" << ticks_ << " irq0=" << irq0_
                  << " ip=0x" << std::hex << regs.ip << " ax=0x" << regs.ax
                  << std::dec << "\n";
        if (!detail_.empty()) std::cout << "detail=" << detail_ << '\n';
        return (status_ == "instruction_limit" || status_ == "guest_exit") ? 0 : 20;
    }

private:
    uc_engine* cpu_{};
    size_t image_size_;
    uint64_t interrupts_{};
    uint64_t ticks_{}, irq0_{};
    uint32_t pit_divisor_ = 65536, pit_low_{}, bios_accumulator_{};
    bool pit_high_next_{}, irq_in_service_{}, eoi_seen_{};
    bool keyboard_ack_{};
    uint32_t vga_dac_cursor_{};
    uint32_t irq_return_sp_{}, irq_resume_ip_{};
    uc_hook eoi_hook_{};
    std::string status_ = "instruction_limit";
    std::string detail_;
    bool trace_interrupts_{};
    uint32_t heap_end_ = 0x4df000;
    uint32_t stack_end_ = (0x3fff000 - 0x126a) & 0xfffff000;
    uint32_t free_bytes_ = stack_end_ - 0x4e1000;
    uint32_t conventional_end_ = 0x10000;
    struct Selector { bool active{}; uint32_t base{}, limit{0xffffffff}; };
    std::array<Selector, 16> selectors_{};
    std::array<std::pair<uint32_t, uint32_t>, 256> protected_vectors_{};
    std::array<std::pair<uint32_t, uint32_t>, 17> exception_vectors_{};
    std::array<uint32_t, 256> real_vectors_{};
    uint16_t video_mode_ = 3;
    int mouse_x_ = 320, mouse_y_ = 100;
    int mouse_min_x_ = 0, mouse_max_x_ = 639, mouse_min_y_ = 0, mouse_max_y_ = 199;
    fs::path game_dir_, cd_dir_;
    struct OpenFile { std::vector<uint8_t> bytes; uint64_t pos{}; };
    std::map<uint16_t, OpenFile> files_;

    struct VideoFrame { uint16_t width{}, height{}; std::vector<uint8_t> rgba; };
    VideoFrame video_frame() {
        VideoFrame frame;
        if (video_mode_ != 0x13) return frame;
        frame.width = 320;
        frame.height = 200;
        std::array<uint8_t, 768> palette{};
        check(uc_mem_read(cpu_, DOS_BASE + 0xf1000, palette.data(), palette.size()),
              "read VGA DAC palette");
        std::vector<uint8_t> indices(frame.width * frame.height);
        check(uc_mem_read(cpu_, DOS_BASE + 0xa0000, indices.data(), indices.size()),
              "read VGA framebuffer");
        frame.rgba.reserve(indices.size() * 4);
        for (uint8_t index : indices) {
            const size_t pos = size_t(index) * 3;
            frame.rgba.push_back(palette[pos] << 2);
            frame.rgba.push_back(palette[pos + 1] << 2);
            frame.rgba.push_back(palette[pos + 2] << 2);
            frame.rgba.push_back(255);
        }
        return frame;
    }
    bool refresh(StartupWindow& window, uint64_t completed, const std::string& status) {
        if (!window.enabled()) return true;
        const auto frame = video_frame();
        return window.refresh(completed, interrupts_, status, &frame.rgba,
                              frame.width, frame.height);
    }
    void save_framebuffer(const std::string& path) {
        const auto frame = video_frame();
        if (frame.rgba.empty()) return;
        std::ofstream output(path, std::ios::binary);
        if (!output) throw std::runtime_error("cannot create framebuffer file: " + path);
        output << "P6\n" << frame.width << " " << frame.height << "\n255\n";
        for (size_t i = 0; i < frame.rgba.size(); i += 4)
            output.write(reinterpret_cast<const char*>(frame.rgba.data() + i), 3);
        if (!output) throw std::runtime_error("failed to write PPM framebuffer");
        std::cout << "framebuffer=" << path << " (" << frame.width << "x" << frame.height << ")\n";
    }

    // DOS names are case-insensitive; only files within the chosen asset root
    // may be opened. Never let original-game requests write to this tree.
    fs::path resolve_dos_path(std::string name) {
        if (name.empty() || name.size() >= 260) throw unsigned(3);
        for (const unsigned char c : name) if (c < 32 || c > 126) throw unsigned(3);
        std::replace(name.begin(), name.end(), '\\', '/');
        if (name.size() >= 2 && name[1] == ':') {
            if (std::toupper(static_cast<unsigned char>(name[0])) != 'C') throw unsigned(15);
            name.erase(0, 2);
        }
        bool absolute = !name.empty() && name[0] == '/';
        while (!name.empty() && name[0] == '/') name.erase(0, 1);
        for (char& c : name) c = std::toupper(static_cast<unsigned char>(c));
        if (absolute && name.rfind("EF2000/", 0) == 0) {
            name.erase(0, 7);
            absolute = false;
        }
        fs::path path = absolute ? cd_dir_ : game_dir_;
        if (path.empty()) throw unsigned(2);
        size_t pos = 0;
        while (pos < name.size()) {
            const size_t slash = name.find('/', pos);
            const std::string part = name.substr(pos, slash == std::string::npos ? slash : slash - pos);
            if (part.empty() || part == ".." || part.find(':') != std::string::npos) throw unsigned(3);
            if (part != ".") {
                bool found = false;
                std::error_code error;
                for (const auto& candidate : fs::directory_iterator(path, error)) {
                    auto filename = candidate.path().filename().string();
                    for (char& c : filename) c = std::toupper(static_cast<unsigned char>(c));
                    if (filename == part) { path = candidate.path(); found = true; break; }
                }
                if (!found) throw unsigned(2);
            }
            if (slash == std::string::npos) break;
            pos = slash + 1;
        }
        const fs::path root = fs::weakly_canonical(absolute ? cd_dir_ : game_dir_);
        const fs::path target = fs::weakly_canonical(path);
        auto relative = target.lexically_relative(root);
        if (relative.empty() || relative == "." || *relative.begin() == "..") throw unsigned(3);
        if (!fs::is_regular_file(target)) throw unsigned(3);
        return target;
    }
    std::string dos_name(uint32_t address) {
        if (address <= 0x260 || address >= IMAGE_SIZE - 260) throw unsigned(3);
        std::array<char, 260> text{};
        check(uc_mem_read(cpu_, address, text.data(), text.size()), "read DOS filename");
        const auto nul = std::find(text.begin(), text.end(), '\0');
        if (nul == text.end()) throw unsigned(3);
        return {text.begin(), nul};
    }
    uint16_t open_file(const std::string& dos_path) {
        const fs::path path = resolve_dos_path(dos_path);
        std::ifstream stream(path, std::ios::binary);
        if (!stream) throw unsigned(5);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(stream)), {});
        // Keep sound off until the original DOS hardware mixer is ported.
        if (path.filename() == "EF2000.CFG" || path.filename() == "ef2000.cfg") {
            std::string config(data.begin(), data.end());
            const std::map<std::string, std::string> overrides = {
                {"MUSIC", "0,127"}, {"SOUNDFX", "0,127"}, {"SPEECH", "0"},
                {"VRHELMET", "0"}};
            for (const auto& [key, value] : overrides) {
                const std::string prefix = key + "=";
                const size_t found = config.find(prefix);
                if (found != std::string::npos && (found == 0 || config[found-1] == '\n')) {
                    const size_t end = config.find_first_of("\r\n", found);
                    config.replace(found, end == std::string::npos ? end : end - found,
                                   prefix + value);
                }
            }
            data.assign(config.begin(), config.end());
        }
        for (uint16_t handle = 5; handle < 64; ++handle)
            if (!files_.count(handle)) { files_.emplace(handle, OpenFile{std::move(data), 0}); return handle; }
        throw unsigned(4);
    }
    void dos_files(uint16_t ax, const Registers& before, Registers& after) {
        const uint8_t ah = ax >> 8;
        const uint16_t handle = before.bx & 0xffff;
        try {
            if (ah == 0x3d) {
                if ((ax & 3) != 0) throw unsigned(5);
                const std::string name = dos_name(before.dx);
                if (trace_interrupts_) std::cerr << "cpp-file: open " << name << '\n';
                after.ax = open_file(name);
                after.dx = 0;
            } else if (ah == 0x3c) {
                throw unsigned(5); // No overlay: reject writes to original assets.
            } else if (ah == 0x3e) {
                if (!files_.erase(handle)) throw unsigned(6);
                after.ax = 0x3e01;
            } else if (ah == 0x3f) {
                auto file = files_.find(handle);
                if (file == files_.end()) throw unsigned(6);
                const auto address = before.dx, count = before.cx;
                if (address <= 0x260 || address >= IMAGE_SIZE || count > IMAGE_SIZE - address)
                    throw unsigned(5);
                const auto& data = file->second.bytes;
                const size_t begin = std::min<uint64_t>(file->second.pos, data.size());
                const size_t length = std::min<size_t>(count, data.size() - begin);
                if (length) write(address, data.data() + begin, length);
                file->second.pos += length;
                after.cx = after.dx = 0;
                after.ax = length;
            } else if (ah == 0x42) {
                auto file = files_.find(handle);
                if (file == files_.end()) throw unsigned(6);
                const int32_t offset = int32_t(((before.cx & 0xffff) << 16) | (before.dx & 0xffff));
                const uint8_t whence = ax & 255;
                if (whence > 2) throw unsigned(1);
                const int64_t base = whence == 0 ? 0 : (whence == 1 ? int64_t(file->second.pos) :
                    int64_t(file->second.bytes.size()));
                if (base + offset < 0) throw unsigned(1);
                file->second.pos = base + offset;
                after.ax = file->second.pos & 0xffff;
                after.dx = (after.dx & 0xffff0000) | ((file->second.pos >> 16) & 0xffff);
            } else if (ah == 0x68) {
                if (handle > 2 && !files_.count(handle)) throw unsigned(6);
            } else if (ax == 0x4400) {
                if (handle > 2 && !files_.count(handle)) throw unsigned(6);
                after.dx = (after.dx & 0xffff0000) | (handle <= 2 ? 0x80d3 : 2);
                after.ax = handle <= 2 ? 0x80d3 : 2;
            } else throw unsigned(1);
            after.flags &= ~1u;
        } catch (unsigned error) {
            after.ax = error;
            after.flags |= 1u;
        }
    }
    template <class T> T read_value(uint64_t address) {
        T value{};
        check(uc_mem_read(cpu_, address, &value, sizeof(value)), "read guest value");
        return value;
    }
    static uint32_t low16(uint32_t old, uint32_t value) {
        return (old & 0xffff0000) | (value & 0xffff);
    }
    void reset_mouse(int width, int height) {
        mouse_min_x_ = mouse_min_y_ = 0;
        mouse_max_x_ = width - 1;
        mouse_max_y_ = height - 1;
        mouse_x_ = width / 2;
        mouse_y_ = height / 2;
    }
    bool bios(uint16_t vector, Registers& values) {
        const uint16_t ax = values.ax & 0xffff;
        if (vector == 0x33) {
            if (ax == 0) {
                values.ax = low16(values.ax, 0xffff);
                values.bx = low16(values.bx, 2);
                reset_mouse(video_mode_ == 0x13 ? 320 : 640,
                            video_mode_ == 0x13 ? 200 : 200);
            } else if (ax == 3) {
                values.bx = low16(values.bx, 0);
                values.cx = low16(values.cx, mouse_x_);
                values.dx = low16(values.dx, mouse_y_);
            } else if (ax == 4) {
                mouse_x_ = values.cx & 0xffff;
                mouse_y_ = values.dx & 0xffff;
            } else if (ax == 7) {
                mouse_min_x_ = values.cx & 0xffff;
                mouse_max_x_ = values.dx & 0xffff;
            } else if (ax == 8) {
                mouse_min_y_ = values.cx & 0xffff;
                mouse_max_y_ = values.dx & 0xffff;
            } else if (ax == 0xb) {
                values.cx = low16(values.cx, 0);
                values.dx = low16(values.dx, 0);
            } else if (ax != 1 && ax != 2 && ax < 0x100) {
                fail("unsupported BIOS mouse function " + std::to_string(ax));
                return false;
            }
        } else if (vector == 0x10) {
            const uint8_t ah = ax >> 8;
            if (ah == 0xf) {
                values.ax = low16(values.ax, ((video_mode_ == 0x13 ? 40 : 80) << 8) | video_mode_);
                values.bx &= ~0xff00u;
            } else if (ah == 0 && ((ax & 0x7f) == 3 || (ax & 0x7f) == 0x13)) {
                video_mode_ = ax & 0x7f;
                reset_mouse(video_mode_ == 0x13 ? 320 : 640, 200);
                write_value<uint8_t>(DOS_BASE + 0x449, video_mode_);
                write_value<uint16_t>(DOS_BASE + 0x44a, video_mode_ == 0x13 ? 40 : 80);
                if (!(ax & 0x80)) {
                    std::array<uint8_t, 4096> zeros{};
                    for (uint32_t i = 0; i < 65536; i += zeros.size())
                        write(DOS_BASE + 0xa0000 + i, zeros.data(), zeros.size());
                }
            } else {
                char message[90];
                std::snprintf(message, sizeof(message), "unsupported BIOS video AX=%04x", ax);
                fail(message);
                return false;
            }
        } else if (vector == 0x16) {
            const uint8_t ah = ax >> 8;
            if (ah == 1 || ah == 0x11) values.flags |= 0x40;
            else if (ah == 2) values.ax &= 0xffffff00;
            else {
                fail("unsupported/blocking BIOS keyboard function " + std::to_string(ah));
                return false;
            }
        } else if (vector == 0x2f && ax == 0x1500) {
            values.bx = 0;
        } else {
            fail("unsupported real-mode BIOS INT " + std::to_string(vector));
            return false;
        }
        return true;
    }
    bool dos_realmode(const Registers& before, Registers& after) {
        if (before.ds != 7 || before.dx <= 0x260 || before.dx > IMAGE_SIZE - 18) {
            fail("invalid X-32 real-mode call frame");
            return false;
        }
        const uint32_t frame = before.dx;
        const uint16_t vector = read_value<uint16_t>(frame);
        if (vector >= 256) { fail("invalid real-mode interrupt vector"); return false; }
        Registers values = before;
        values.ax = read_value<uint32_t>(frame + 10);
        values.dx = read_value<uint32_t>(frame + 14);
        if (!bios(vector, values)) return false;
        // The 18-byte X-32 request holds the four segment registers, EAX,
        // EDX. Remaining general registers return through protected registers.
        write_value<uint32_t>(frame + 14, values.dx);
        after.ax = values.ax;
        after.bx = values.bx;
        after.cx = values.cx;
        after.dx = values.dx;
        after.si = values.si;
        after.di = values.di;
        after.flags = (before.flags & ~0x8d5u) | (values.flags & 0x8d5u);
        return true;
    }

    void check(uc_err err, const char* what) {
        if (err != UC_ERR_OK) throw std::runtime_error(std::string(what) + ": " + uc_strerror(err));
    }
    void write(uint64_t address, const void* data, size_t size) {
        check(uc_mem_write(cpu_, address, data, size), "write guest RAM");
    }
    template<class T> void write_value(uint64_t address, T value) { write(address, &value, sizeof(value)); }
    void reg(int id, uint32_t value) { check(uc_reg_write(cpu_, id, &value), "write x86 register"); }
    Registers read_regs() {
        Registers out;
        uint32_t* values = &out.ax;
        for (size_t i = 0; i < std::size(REG_IDS); ++i)
            check(uc_reg_read(cpu_, REG_IDS[i], &values[i]), "read x86 register");
        return out;
    }
    void write_regs(const Registers& before, const Registers& after) {
        const uint32_t* old = &before.ax;
        const uint32_t* updated = &after.ax;
        for (size_t i = 0; i < std::size(REG_IDS); ++i)
            if (old[i] != updated[i]) reg(REG_IDS[i], updated[i]);
    }
    void initialize_memory() {
        static constexpr char command[] = "\x04gui\r";
        static constexpr char environment[] = "DID=.\0quick_flag=1\0\0\x01\0C:\\EF2000\\_EF2000.EXE\0";
        write(PSP_BASE + 0x80, command, sizeof(command) - 1);
        write(ENV_BASE, environment, sizeof(environment) - 1);
        write_value<uint16_t>(0x120cc4, 0x1f);
        write_value<uint32_t>(0x120cc6, DOS_BASE);
        write_value<uint16_t>(0x132a24, 0x27);
        // This is the loader's initialized BSS pattern; guest startup clears it.
        std::array<uint8_t, 4096> poison;
        poison.fill(0xa5);
        for (uint32_t cursor = 0x195fb0; cursor < 0x4de2f0; cursor += poison.size())
            write(cursor, poison.data(), std::min<size_t>(poison.size(), 0x4de2f0 - cursor));
        for (auto& vector : protected_vectors_) vector = {IMAGE_SIZE + 0x100, 0x2f};
        for (auto& vector : exception_vectors_) vector = {IMAGE_SIZE + 0x100, 0x2f};
    }
    void initialize_segments() {
        const uint8_t trampoline[] = {0x50, 0xb0, 0x20, 0xe6, 0x20, 0xfb, 0x58, 0xcf};
        write(IMAGE_SIZE + 0x100, trampoline, sizeof(trampoline));
        std::array<uint8_t, 0x30> gdt{};
        auto fill = [](auto& table, size_t offset, auto bytes) {
            std::copy(bytes.begin(), bytes.end(), table.begin() + offset);
        };
        fill(gdt, 8, segment(TABLE_BASE + 0x100, 0xff, 0x82, 0));
        fill(gdt, 16, segment(0, 0xffff, 0x9a));
        fill(gdt, 24, segment(0, 0xffff, 0x92));
        write(TABLE_BASE, gdt.data(), gdt.size());
        std::array<uint8_t, 0x100> ldt{};
        constexpr uint32_t bases[] = {0, PSP_BASE, ENV_BASE, DOS_BASE, EXTENDER_BASE, 0};
        for (size_t i = 0; i < 6; ++i) {
            if (!i) fill(ldt, i * 8, segment(0, 0x7f, 0xf6, 0x4));
            else fill(ldt, i * 8, segment(bases[i], 0xffff, i == 5 ? 0xfa : 0xf2));
        }
        write(TABLE_BASE + 0x100, ldt.data(), ldt.size());
        uc_x86_mmr gdtr{0, TABLE_BASE, uint32_t(gdt.size() - 1), 0};
        uc_x86_mmr ldtr{8, TABLE_BASE + 0x100, uint32_t(ldt.size() - 1), 0};
        check(uc_reg_write(cpu_, UC_X86_REG_GDTR, &gdtr), "set GDTR");
        check(uc_reg_write(cpu_, UC_X86_REG_LDTR, &ldtr), "set LDTR");
        for (auto id : {UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS,
                        UC_X86_REG_FS, UC_X86_REG_GS}) reg(id, 0x18);
        reg(UC_X86_REG_CS, 0x10);
        const uint8_t iret = 0xcf;
        write(TABLE_BASE + 0x200, &iret, 1);
        std::vector<uint8_t> code = {0x66, 0xb8, 0x07, 0x00, 0x8e, 0xd8,
            0x8e, 0xc0, 0x66, 0xb8, 0x0f, 0x00, 0x8e, 0xe0, 0x66, 0xb8,
            0x17, 0x00, 0x8e, 0xe8, 0xe9, 0, 0, 0, 0};
        put32(code, code.size() - 4, ENTRY - (TABLE_BASE + 0x300 + code.size()));
        write(TABLE_BASE + 0x300, code.data(), code.size());
        uint32_t frame[] = {TABLE_BASE + 0x300, 0x2f, 0x3202, INITIAL_SP, 7};
        write(INITIAL_SP - sizeof(frame), frame, sizeof(frame));
        reg(UC_X86_REG_ESP, INITIAL_SP - sizeof(frame));
        reg(UC_X86_REG_EIP, TABLE_BASE + 0x200);
        reg(UC_X86_REG_EFLAGS, 0x202);
    }
    void fail(std::string message) {
        status_ = "unsupported_interface";
        detail_ = std::move(message);
        uc_emu_stop(cpu_);
    }
    uint32_t ip_at_cpu() {
        uint32_t ip{};
        check(uc_reg_read(cpu_, UC_X86_REG_EIP, &ip), "read guest EIP");
        return ip;
    }
    void deliver_tick(uint32_t next_ip) {
        ++ticks_;
        bios_accumulator_ += pit_divisor_;
        if (bios_accumulator_ >= 65536) {
            uint32_t previous{};
            check(uc_mem_read(cpu_, DOS_BASE + 0x46c, &previous, sizeof(previous)), "read BIOS ticks");
            previous += bios_accumulator_ / 65536;
            bios_accumulator_ %= 65536;
            write_value<uint32_t>(DOS_BASE + 0x46c, previous);
        }
        const Registers regs = read_regs();
        const auto [handler, selector] = protected_vectors_[8];
        if (irq_in_service_ || handler >= IMAGE_SIZE ||
            (selector != 0x2c && selector != 0x2f) ||
            (regs.ss != 7 || (regs.flags & 0x200) == 0 || regs.sp < 0x1000)) return;
        uint32_t cs{};
        check(uc_reg_read(cpu_, UC_X86_REG_CS, &cs), "read CS");
        if (cs != 0x2c && cs != 0x2f) return;
        uint32_t frame[] = {next_ip, cs, regs.flags};
        write(regs.sp - 12, frame, sizeof(frame));
        reg(UC_X86_REG_ESP, regs.sp - 12);
        reg(UC_X86_REG_EIP, handler);
        reg(UC_X86_REG_EFLAGS, regs.flags & ~0x300u);
        irq_in_service_ = true;
        eoi_seen_ = false;
        irq_return_sp_ = regs.sp;
        irq_resume_ip_ = next_ip;
        ++irq0_;
    }
    static void on_irq_resume(uc_engine*, uint64_t, uint32_t, void* user) {
        auto* machine = static_cast<Machine*>(user);
        if (machine->irq_in_service_ && machine->eoi_seen_) {
            uint32_t esp{};
            if (uc_reg_read(machine->cpu_, UC_X86_REG_ESP, &esp) == UC_ERR_OK &&
                esp == machine->irq_return_sp_) {
                machine->irq_in_service_ = false;
                machine->eoi_seen_ = false;
            }
        }
    }
    static void on_interrupt(uc_engine*, uint32_t vector, void* user) {
        static_cast<Machine*>(user)->interrupt(vector);
    }
    void interrupt(uint32_t vector) {
        ++interrupts_;
        const Registers before = read_regs();
        Registers after = before;
        const uint32_t ip = before.ip - 2;
        const uint16_t ax = before.ax & 0xffff;
        if (trace_interrupts_) {
            std::cerr << "cpp-int: n=" << interrupts_ << " ip=" << std::hex << ip
                      << " vec=" << vector << " ax=" << before.ax << " bx=" << before.bx
                      << " cx=" << before.cx << " dx=" << before.dx << std::dec << '\n';
        }
        if (vector != 0x21) {
            if (!bios(vector, after)) return;
        } else if (ip == 0x93 && ax == 0x3505 && (before.bx & 0xffff) == 7 && before.cx == 0x260) {
            after.flags = (after.flags | 0x40) & ~1u;
            auto desc = segment(0, 0x260, 0xf6, 0x4);
            write(TABLE_BASE + 0x100, desc.data(), desc.size());
        } else if (ip == 0xa0 && ax == 0x3005) {
            after.ax = 5;
            after.bx &= ~255u;
            after.cx &= 0xffff0000;
        } else if (ip == 0x19a && ax == 0x3509) {
            if (before.bx < before.cx) return fail("stack reserve below zero");
            const uint32_t boundary = (before.bx - before.cx) & 0xfffff000;
            if (boundary < 0x4e1000 || boundary > stack_end_ || stack_end_ - boundary > free_bytes_)
                return fail("stack reserve exhausted");
            free_bytes_ -= stack_end_ - boundary;
            stack_end_ = boundary;
            const uint32_t old = boundary - free_bytes_;
            after.dx = old - 1;
            after.flags = dec_flags(old, after.dx, before.flags);
        } else if (ip == 0x245 && ax == 0x3508) {
            const uint32_t rounded = (before.cx + 0xfff) & 0xfffff000;
            if (rounded >= IMAGE_SIZE || (rounded > heap_end_ && rounded - heap_end_ > free_bytes_))
                return fail("heap growth exhausted");
            if (rounded > heap_end_) { free_bytes_ -= rounded - heap_end_; heap_end_ = rounded; }
            const uint32_t floor = stack_end_ - free_bytes_;
            after.ax = std::max(floor, 0x4e1000u);
            after.cx = heap_end_;
            after.flags = sub_flags(floor, 0x4e1000, floor - 0x4e1000, before.flags);
        } else if (ax == 0x350b) {
            after.ax = std::min(IMAGE_SIZE - heap_end_, free_bytes_);
            after.flags &= ~1u;
        } else if (ax == 0x3501 || ax == 0x3502 || ax == 0x3503 || ax == 0x3504 || ax == 0x3505) {
            if (!selector_service(ax, before, after)) return;
        } else if (ax == 0x2502 || ax == 0x2503 || ax == 0x2504 || ax == 0x2505 ||
                   ax == 0x2506 || ax == 0x250c || ax == 0x2532 || ax == 0x2533) {
            uint32_t vec = before.cx & 255;
            if (ax == 0x2502) {
                after.bx = protected_vectors_[vec].first;
                after.es = protected_vectors_[vec].second;
            } else if (ax == 0x2503) after.bx = real_vectors_[vec];
            else if (ax == 0x2504) protected_vectors_[vec] = {before.dx, before.ds};
            else if (ax == 0x2505) real_vectors_[vec] = before.bx;
            else if (ax == 0x2506) {
                protected_vectors_[vec] = {before.dx, before.ds};
                real_vectors_[vec] = 0xf0000000 | (vec << 2);
            } else if (ax == 0x2532 && vec < 16) {
                const auto [target, selector] = vec < 8 ? protected_vectors_[vec] : exception_vectors_[vec];
                after.bx = target;
                after.es = selector;
            } else if (ax == 0x2533 && vec <= 16 && before.ds == exception_vectors_[0].second) {
                exception_vectors_[vec] = {before.dx, before.ds};
                if (vec < 8) protected_vectors_[vec] = {before.dx, before.ds};
            } else if (ax == 0x250c) {
                after.ax = (after.ax & 0xffff0000) | 0x7008;
                after.bx &= 0xffff0000;
            } else {
                return fail("unsupported X-32 vector request");
            }
            after.flags &= ~1u;
        } else if ((before.ax >> 8 & 255) == 0x48) {
            const uint32_t size = (before.bx & 0xffff) << 4;
            if (!size) return fail("DOS zero paragraph allocation");
            if (size > 0x90000 - conventional_end_) {
                after.ax = 8; after.bx = (0x90000 - conventional_end_) >> 4;
                after.flags |= 1;
            } else {
                after.ax = conventional_end_ >> 4;
                after.bx = DOS_BASE + conventional_end_;
                conventional_end_ += size;
                after.flags &= ~1u;
            }
        } else if (ax == 0x252b && (before.bx & 0xffff) == 0x501) {
            after.ax = 0x252b;
            after.flags &= ~1u;
        } else if (ax == 0x252b && (before.bx & 0xffff) == 0x601) {
            after.ax = 0x252b;
            after.flags &= ~1u;
        } else if (ax == 0x252b) {
            after.ax = 0xa5a5a5a5;
            after.flags |= 1;
        } else if (ax == 0x2511) {
            if (!dos_realmode(before, after)) return;
        } else if ((before.ax >> 8 & 255) == 0x2c || (before.ax >> 8 & 255) == 0x2a) {
            const auto instant = std::chrono::system_clock::now();
            const std::time_t seconds = std::chrono::system_clock::to_time_t(instant);
            std::tm utc{};
#ifdef _WIN32
            gmtime_s(&utc, &seconds);
#else
            gmtime_r(&seconds, &utc);
#endif
            if ((before.ax >> 8 & 255) == 0x2c) {
                const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                    instant.time_since_epoch()).count() % 1000;
                after.ax = 0x2c00;
                after.cx = (after.cx & 0xffff0000) | (utc.tm_hour << 8) | utc.tm_min;
                after.dx = (after.dx & 0xffff0000) | (utc.tm_sec << 8) | (millis / 10);
            } else {
                after.ax = (after.ax & 0xffffff00) | utc.tm_wday;
                after.cx = (after.cx & 0xffff0000) | (utc.tm_year + 1900);
                after.dx = (after.dx & 0xffff0000) | ((utc.tm_mon + 1) << 8) | utc.tm_mday;
            }
        } else if ((before.ax >> 8 & 255) == 0x40 && (before.bx & 0xffff) <= 2) {
            const uint32_t size = before.cx;
            if (before.dx <= 0x260 || before.dx >= IMAGE_SIZE || size > IMAGE_SIZE - before.dx || size > 65536)
                return fail("invalid DOS console write buffer");
            std::vector<char> output(size);
            check(uc_mem_read(cpu_, before.dx, output.data(), size), "read console");
            std::cout.write(output.data(), output.size());
            after.ax = size;
            after.flags &= ~1u;
        } else if ((before.ax >> 8 & 255) == 0x3c || (before.ax >> 8 & 255) == 0x3d ||
                   (before.ax >> 8 & 255) == 0x3e || (before.ax >> 8 & 255) == 0x3f ||
                   (before.ax >> 8 & 255) == 0x42 || (before.ax >> 8 & 255) == 0x68 ||
                   ax == 0x4400) {
            dos_files(ax, before, after);
        } else if ((before.ax >> 8 & 255) == 0x4c) {
            status_ = "guest_exit";
            uc_emu_stop(cpu_);
        } else {
            char message[128];
            std::snprintf(message, sizeof(message), "unimplemented INT 21h AX=%04x at IP=%08x", ax, ip);
            fail(message);
            return;
        }
        write_regs(before, after);
    }
    static bool parity(uint32_t value) { return __builtin_popcount(value & 255) % 2 == 0; }
    static uint32_t dec_flags(uint32_t before, uint32_t result, uint32_t flags) {
        flags &= ~0x8d5u;
        if (result & 0x80000000) flags |= 0x80;
        if (!result) flags |= 0x40;
        if ((before ^ result ^ 1) & 0x10) flags |= 0x10;
        if (before == 0x80000000) flags |= 0x800;
        if (parity(result)) flags |= 4;
        return flags;
    }
    static uint32_t sub_flags(uint32_t left, uint32_t right, uint32_t result, uint32_t flags) {
        flags &= ~0x8d5u;
        if (result & 0x80000000) flags |= 0x80;
        if (!result) flags |= 0x40;
        if ((left ^ right ^ result) & 0x10) flags |= 0x10;
        if ((left ^ right) & (left ^ result) & 0x80000000) flags |= 0x800;
        if (parity(result)) flags |= 4;
        return flags;
    }
    bool selector_service(uint16_t ax, const Registers& before, Registers& after) {
        if (ax == 0x3501) {
            auto found = std::find_if(selectors_.begin(), selectors_.end(),
                                      [](const Selector& entry) { return !entry.active; });
            if (found == selectors_.end()) { after.bx = 0; after.flags |= 1; return true; }
            found->active = true;
            found->base = 0;
            found->limit = 0xffffffff;
            after.bx = ((found - selectors_.begin() + 6) << 3) | 7;
            write_selector(found - selectors_.begin());
        } else if (ax == 0x3504 &&
                   (before.bx & 0xffff) == 7) after.cx = 0;
        else if (ax == 0x3504 && (before.bx & 0xffff) == 0x2f) after.cx = 0;
        else if ((before.bx & 7) == 7 && (before.bx >> 3) >= 6 &&
                 (before.bx >> 3) < 22) {
            const unsigned index = (before.bx >> 3) - 6;
            Selector& selector = selectors_[index];
            if (!selector.active) { fail("X-32 selector not allocated"); return false; }
            if (ax == 0x3502) selector.active = false;
            else if (ax == 0x3503) selector.base = before.cx;
            else if (ax == 0x3504) after.cx = selector.base;
            else if (ax == 0x3505) selector.limit = before.cx | (before.cx > 0xfffff ? 0xfff : 0);
            if (ax != 0x3504) write_selector(index);
        } else { fail("unhandled X-32 selector service"); return false; }
        after.flags &= ~1u;
        return true;
    }
    void write_selector(unsigned index) {
        const Selector& selector = selectors_[index];
        std::array<uint8_t, 8> bytes{};
        if (selector.active) {
            const bool large = selector.limit > 0xfffff;
            bytes = segment(selector.base, large ? selector.limit >> 12 : selector.limit,
                            0xf2, large ? 0xc : 0x4);
        }
        write(TABLE_BASE + 0x100 + (index + 6) * 8, bytes.data(), bytes.size());
    }
    static uint32_t on_port_in(uc_engine*, uint32_t port, int, void* user) {
        return static_cast<Machine*>(user)->port_in(port);
    }
    static void on_port_out(uc_engine*, uint32_t port, int, uint32_t value, void* user) {
        static_cast<Machine*>(user)->port_out(port, value);
    }
    uint32_t port_in(uint32_t port) {
        if (port == 0x3da) {
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            return ns % 16666667 >= 15000000 ? 8 : 0;
        }
        if (port == 0x64) return keyboard_ack_ ? 1 : 0;
        if (port == 0x60) {
            if (keyboard_ack_) { keyboard_ack_ = false; return 0xfa; }
            return 0;
        }
        if (port == 0x201 || port == 0x61) return 0;
        char message[80];
        std::snprintf(message, sizeof(message), "unsupported port IN %04x", port);
        fail(message);
        return 0;
    }
    void port_out(uint32_t port, uint32_t value) {
        if (port == 0x20 && (value & 255) == 0x20) { eoi_seen_ = true; return; }
        if (port == 0x43 && ((value & 255) == 0x36 || (value & 255) == 0x34)) {
            pit_high_next_ = false; return;
        }
        if (port == 0x40) {
            if (!pit_high_next_) pit_low_ = value & 255;
            else {
                const uint32_t divisor = pit_low_ | ((value & 255) << 8);
                pit_divisor_ = divisor ? divisor : 65536;
            }
            pit_high_next_ = !pit_high_next_;
            return;
        }
        if (port == 0x60) { keyboard_ack_ = true; return; }
        if (port == 0x3c8) { vga_dac_cursor_ = (value & 255) * 3; return; }
        if (port == 0x3c9) {
            write_value<uint8_t>(DOS_BASE + 0xf1000 + vga_dac_cursor_, value & 63);
            vga_dac_cursor_ = (vga_dac_cursor_ + 1) % 768;
            return;
        }
        if (port == 0x61 || port == 0x201) return;
        char message[80];
        std::snprintf(message, sizeof(message), "unsupported port OUT %04x", port);
        fail(message);
    }
};

void usage(const char* program) {
    std::cout << "Usage: " << program << " _EF2000.EXE [--instruction-limit N]"
              << " [--game-dir DIR] [--cd-dir DIR] [--windowed]"
              << " [--timer-instructions N]\n"
              << "       [--trace-interrupts] (stderr DOS/X-32 call transcript)\n"
              << "       [--framebuffer picture.ppm] (320x200 VGA mode 13h)\n"
              << "Experimental native C++ Unicorn X-32 startup probe.\n"
              << "Stops explicitly at unsupported DOS/BIOS interfaces.\n"
              << "       " << program << " --self-test\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        std::string path;
        std::string game_dir, cd_dir;
        uint64_t limit = 30000000;
        uint64_t timer_interval = 250000;
        bool windowed = false;
        bool self_test = false;
        bool trace_interrupts = false;
        std::string framebuffer;
        for (int i = 1; i < argc; ++i) {
            if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h")) {
                usage(argv[0]); return 0;
            }
            if (!std::strcmp(argv[i], "--instruction-limit") && i + 1 < argc) {
                size_t end{};
                const std::string value = argv[++i];
                limit = std::stoull(value, &end);
                if (end != value.size() || !limit) throw std::runtime_error("invalid instruction limit");
            } else if (!std::strcmp(argv[i], "--timer-instructions") && i + 1 < argc) {
                size_t end{};
                const std::string value = argv[++i];
                timer_interval = std::stoull(value, &end);
                if (end != value.size() || !timer_interval)
                    throw std::runtime_error("invalid timer interval");
            } else if (!std::strcmp(argv[i], "--game-dir") && i + 1 < argc) {
                game_dir = argv[++i];
            } else if (!std::strcmp(argv[i], "--cd-dir") && i + 1 < argc) {
                cd_dir = argv[++i];
            } else if (!std::strcmp(argv[i], "--windowed")) {
                windowed = true;
            } else if (!std::strcmp(argv[i], "--self-test")) {
                self_test = true;
            } else if (!std::strcmp(argv[i], "--framebuffer") && i + 1 < argc) {
                framebuffer = argv[++i];
            } else if (!std::strcmp(argv[i], "--trace-interrupts")) {
                trace_interrupts = true;
            } else if (argv[i][0] != '-' && path.empty()) path = argv[i];
            else throw std::runtime_error("unrecognized option: " + std::string(argv[i]));
        }
        if (self_test) {
            if (!path.empty()) throw std::runtime_error("self-test does not take an executable");
            std::vector<uint8_t> image(0x1000);
            constexpr uint8_t guest[] = {0xb8, 0x0b, 0x35, 0x00, 0x00, 0xcd, 0x21,
                                          0xb8, 0x00, 0x4c, 0x00, 0x00, 0xcd, 0x21};
            std::copy(std::begin(guest), std::end(guest), image.begin() + ENTRY);
            Machine machine(image, {}, {});
            return machine.run(10000, false, 250000, "");
        }
        if (path.empty()) { usage(argv[0]); return 2; }
        const auto image = payload_from_executable(path);
        if (game_dir.empty()) game_dir = fs::path(path).parent_path().string();
        if (cd_dir.empty()) cd_dir = game_dir;
        Machine machine(image, game_dir, cd_dir, trace_interrupts);
        return machine.run(limit, windowed, timer_interval, framebuffer);
    } catch (const std::exception& error) {
        std::cerr << "ef2000-unicorn-cpp: " << error.what() << '\n';
        return 1;
    }
}
