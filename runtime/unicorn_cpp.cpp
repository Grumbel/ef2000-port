// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Portable C++ Unicorn/X-32 runner for Linux and Windows. Unsupported guest
// calls stop explicitly; the proprietary executable is only read at runtime
// and is never linked into the program.
#include "dos_drive.hpp"
#include "bios_disk.hpp"
#include "gamepad_profile.hpp"
#include "host_shortcuts.hpp"
#include "menu_font.hpp"
#include "nine_slice.hpp"
#include "overlay_skin.hpp"
#include "overlay_windows.hpp"
#include "cd_audio.hpp"
#include "pcm_audio.hpp"
#include "real_mode_cpu.hpp"
#include "wav_capture.hpp"
#include <unicorn/unicorn.h>
#include <unicorn/x86.h>
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <openssl/sha.h>
#endif
#ifdef EF_UNICORN_SDL3
#include <SDL3/SDL.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <deque>
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
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
constexpr uint32_t COPY_ENTRY = 0xe9a24;
constexpr uint32_t VGA_COPY_BASE = DOS_BASE + 0xa0000;
constexpr uint32_t VGA_COPY_END = VGA_COPY_BASE + 0x10000;
constexpr char SOURCE_SHA256[] = "f21a72d82d768f8990f14a3af9e98caa06c0c08457ab1898ca8d2b50badd7afd";

uint16_t le16(const std::vector<uint8_t>& data, size_t pos) {
    if (pos + 2 > data.size()) throw std::runtime_error("truncated executable header");
    return uint16_t(data[pos] | (uint16_t(data[pos + 1]) << 8));
}
uint32_t le32(const std::vector<uint8_t>& data, size_t pos) {
    return uint32_t(le16(data, pos)) | (uint32_t(le16(data, pos + 2)) << 16);
}
struct MouseEvent {
    uint64_t tick{};
    int x{}, y{};
    unsigned buttons{};
};
struct KeyEvent {
    int scan{}, key{};
    bool down{}, repeat{};
};
struct TimedKeyEvent {
    uint64_t tick{};
    KeyEvent key;
};
struct JoystickEvent {
    uint64_t tick{};
    std::array<int, 4> axes{};
    unsigned count{}, buttons{};
};
struct JoystickSnapshot {
    unsigned count{}, buttons{};
    std::array<int, 4> axes{};
};
struct JoystickAxisBinding {
    unsigned index{};
    int direction{1};
};
enum class ControlSelection { Auto, Keyboard, Joystick };
enum class ExecutionTiming { Auto, Instruction, Realtime };
struct PerformanceOptions {
    unsigned block_clock_check = 256;
    unsigned vga_clock_check = 32;
    uint32_t tcg_buffer_mib{}; // 0 keeps Unicorn's current default.
    bool profile{};
    bool hot_blocks{};
    bool profile_copy{};
    bool profile_copy_callers{};
    bool fast_copy{};
    bool fast_copy_vga{};
    bool clock_loop_benchmark{};
};
struct PerformanceStats {
    uint64_t block_clock_checks{}, port_ins{}, port_outs{};
    uint64_t vga_reads{}, vga_clock_checks{}, audio_reads{}, audio_writes{};
    std::chrono::steady_clock::duration guest_time{}, audio_time{}, cd_time{}, display_time{};
};
struct PendingCopy {
    uint32_t stack{}, return_ip{}, source{}, destination{}, bytes{};
};
struct CopyCallerStats {
    uint64_t calls{}, bytes{}, ge1024{}, ge4096{}, vga_calls{}, vga_bytes{};
    uint64_t accelerated_calls{}, accelerated_bytes{};
    uint32_t max_bytes{};
};
struct WindowOptions {
    bool enabled{}, fullscreen{}, smooth_scaling{};
    bool profile_video_frames{};
    bool show_fps{true}, gamepad_flight_profile{true};
    std::string overlay_frame_bmp;
    std::string overlay_font_bmp;
    std::string overlay_symbols_bmp;
    std::string overlay_skin_dir;
    int overlay_frame_border{12};
    int width = 1280, height = 960;
    std::string aspect_ratio = "4:3";
    int joystick_index{}; // -1 disables the physical controller.
    bool trace_joystick{};
    std::vector<JoystickAxisBinding> joystick_axes{{0, 1}, {1, 1}, {2, 1}, {3, 1}};
};

std::vector<JoystickAxisBinding> parse_joystick_axes(const std::string& value) {
    std::vector<JoystickAxisBinding> mappings;
    size_t first = 0;
    while (first < value.size()) {
        const size_t end = value.find(',', first);
        const auto token = value.substr(first, end == std::string::npos ?
                                       std::string::npos : end - first);
        if (token.empty()) throw std::runtime_error("empty joystick axis mapping");
        const bool reverse = token[0] == '-';
        const std::string index = reverse ? token.substr(1) : token;
        if (index.empty()) throw std::runtime_error("invalid joystick axis mapping");
        size_t parsed{};
        const auto axis = std::stoul(index, &parsed);
        if (parsed != index.size() || axis > 31)
            throw std::runtime_error("invalid joystick axis number (0..31)");
        mappings.push_back({unsigned(axis), reverse ? -1 : 1});
        if (mappings.size() > 4) throw std::runtime_error("too many joystick axes");
        if (end == std::string::npos) break;
        first = end + 1;
    }
    if ((mappings.size() != 2 && mappings.size() != 4) ||
        (!value.empty() && value.back() == ','))
        throw std::runtime_error("--joystick-axes expects two or four comma-separated axes");
    return mappings;
}

unsigned parse_clock_check(const std::string& value, const char* option) {
    size_t parsed{};
    const auto number = std::stoul(value, &parsed);
    if (parsed != value.size() || number < 1 || number > 4096 ||
        (number & (number - 1)) != 0)
        throw std::runtime_error(std::string(option) + " expects a power of two (1..4096)");
    return unsigned(number);
}

class StartupWindow {
public:
    explicit StartupWindow(WindowOptions options) : options_(std::move(options)) {
        if (!options_.enabled) return;
#ifdef EF_UNICORN_SDL3
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
            throw std::runtime_error(std::string("SDL_Init: ") + SDL_GetError());
        window_ = SDL_CreateWindow("EF2000 / Unicorn C++", options_.width, options_.height,
                                   SDL_WINDOW_RESIZABLE |
                                   (options_.fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
        if (!window_) throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) throw std::runtime_error(std::string("SDL_CreateRenderer: ") + SDL_GetError());
        if (!SDL_SetRenderLogicalPresentation(renderer_, 640, 480,
                options_.aspect_ratio == "stretch" ? SDL_LOGICAL_PRESENTATION_STRETCH :
                                                      SDL_LOGICAL_PRESENTATION_LETTERBOX))
            throw std::runtime_error(std::string("SDL_SetRenderLogicalPresentation: ") + SDL_GetError());
        if (!options_.overlay_skin_dir.empty()) {
            auto load_skin = [&](const char* name) {
                const auto filename = (fs::path(options_.overlay_skin_dir) / name).string();
                SDL_Surface* surface = SDL_LoadBMP(filename.c_str());
                if (!surface)
                    throw std::runtime_error("cannot load overlay skin BMP: " + filename +
                                             ": " + SDL_GetError());
                if (surface->w != 640 || surface->h != 400) {
                    SDL_DestroySurface(surface);
                    throw std::runtime_error("overlay skin BMP must be 640x400: " + filename);
                }
                SDL_Texture* loaded = SDL_CreateTextureFromSurface(renderer_, surface);
                SDL_DestroySurface(surface);
                if (!loaded || !SDL_SetTextureScaleMode(loaded, SDL_SCALEMODE_NEAREST))
                    throw std::runtime_error("cannot initialize overlay skin texture: " + filename);
                return loaded;
            };
            overlay_title_texture_ = load_skin("skin-title.bmp");
            overlay_panel_texture_ = load_skin("skin-panel.bmp");
            // The skin extractor also provides the original font specimen.
            // An explicitly selected --overlay-font-bmp always takes priority.
            const auto font = fs::path(options_.overlay_skin_dir) / "menu-font.bmp";
            if (options_.overlay_font_bmp.empty() && fs::is_regular_file(font))
                options_.overlay_font_bmp = font.string();
            const auto symbols = fs::path(options_.overlay_skin_dir) / "menu-symbols.bmp";
            if (options_.overlay_symbols_bmp.empty() && fs::is_regular_file(symbols))
                options_.overlay_symbols_bmp = symbols.string();
            std::cerr << "overlay: original screen-coordinate title and panel textures loaded\n";
        }
        if (!options_.overlay_frame_bmp.empty()) {
            SDL_Surface* surface = SDL_LoadBMP(options_.overlay_frame_bmp.c_str());
            if (!surface)
                throw std::runtime_error("cannot load overlay frame BMP: " + options_.overlay_frame_bmp +
                                         ": " + SDL_GetError());
            overlay_frame_width_ = surface->w;
            overlay_frame_height_ = surface->h;
            const int border = options_.overlay_frame_border;
            if (overlay_frame_width_ <= border * 2 || overlay_frame_height_ <= border * 2) {
                SDL_DestroySurface(surface);
                throw std::runtime_error("overlay frame BMP must be larger than twice its border");
            }
            overlay_frame_texture_ = SDL_CreateTextureFromSurface(renderer_, surface);
            SDL_DestroySurface(surface);
            if (!overlay_frame_texture_ ||
                !SDL_SetTextureScaleMode(overlay_frame_texture_, SDL_SCALEMODE_NEAREST))
                throw std::runtime_error(std::string("SDL3 overlay frame texture: ") + SDL_GetError());
            std::cerr << "overlay: loaded 9-slice frame " << options_.overlay_frame_bmp
                      << " (" << overlay_frame_width_ << "x" << overlay_frame_height_
                      << ", border=" << border << ")\n";
        }
        if (!options_.overlay_font_bmp.empty()) {
            fs::path widths_path = options_.overlay_font_bmp;
            widths_path.replace_extension(".widths");
            std::ifstream widths_stream(widths_path);
            if (!widths_stream)
                throw std::runtime_error("cannot read overlay font widths: " + widths_path.string());
            for (int& width : overlay_font_widths_) {
                if (!(widths_stream >> width) || width < -1 || width > ef2000::menu_font_cell_width)
                    throw std::runtime_error("invalid overlay font width table: " + widths_path.string());
            }
            std::string trailing;
            if (widths_stream >> trailing)
                throw std::runtime_error("overlay font width table has extra data: " + widths_path.string());
            SDL_Surface* surface = SDL_LoadBMP(options_.overlay_font_bmp.c_str());
            if (!surface)
                throw std::runtime_error("cannot load overlay font BMP: " + options_.overlay_font_bmp +
                                         ": " + SDL_GetError());
            if (surface->w != ef2000::menu_font_atlas_width ||
                surface->h != ef2000::menu_font_atlas_height) {
                SDL_DestroySurface(surface);
                throw std::runtime_error("overlay font atlas must be 128x72 pixels");
            }
            if (!SDL_SetSurfaceColorKey(surface, true, SDL_MapSurfaceRGB(surface, 255, 0, 255))) {
                SDL_DestroySurface(surface);
                throw std::runtime_error(std::string("overlay font color key: ") + SDL_GetError());
            }
            overlay_font_texture_ = SDL_CreateTextureFromSurface(renderer_, surface);
            SDL_DestroySurface(surface);
            if (!overlay_font_texture_ ||
                !SDL_SetTextureScaleMode(overlay_font_texture_, SDL_SCALEMODE_NEAREST) ||
                !SDL_SetTextureBlendMode(overlay_font_texture_, SDL_BLENDMODE_BLEND))
                throw std::runtime_error(std::string("SDL3 overlay font texture: ") + SDL_GetError());
            std::cerr << "overlay: loaded original menu glyph atlas "
                      << options_.overlay_font_bmp << "\n";
        }
        if (!options_.overlay_symbols_bmp.empty()) {
            SDL_Surface* surface = SDL_LoadBMP(options_.overlay_symbols_bmp.c_str());
            if (!surface)
                throw std::runtime_error("cannot load overlay symbols BMP: " +
                                         options_.overlay_symbols_bmp + ": " + SDL_GetError());
            if (surface->w != 112 || surface->h != 32) {
                SDL_DestroySurface(surface);
                throw std::runtime_error("overlay symbol atlas must be 112x32 pixels");
            }
            if (!SDL_SetSurfaceColorKey(surface, true, SDL_MapSurfaceRGB(surface, 255, 0, 255))) {
                SDL_DestroySurface(surface);
                throw std::runtime_error(std::string("overlay symbol color key: ") + SDL_GetError());
            }
            overlay_symbols_texture_ = SDL_CreateTextureFromSurface(renderer_, surface);
            SDL_DestroySurface(surface);
            if (!overlay_symbols_texture_ ||
                !SDL_SetTextureScaleMode(overlay_symbols_texture_, SDL_SCALEMODE_NEAREST) ||
                !SDL_SetTextureBlendMode(overlay_symbols_texture_, SDL_BLENDMODE_BLEND))
                throw std::runtime_error(std::string("SDL3 overlay symbol texture: ") + SDL_GetError());
            std::cerr << "overlay: loaded original specimen symbols "
                      << options_.overlay_symbols_bmp << "\n";
        }
        probe_joystick(true);
#else
        throw std::runtime_error("this binary was built without SDL3; remove --windowed");
#endif
    }
    ~StartupWindow() {
#ifdef EF_UNICORN_SDL3
        if (audio_stream_) SDL_DestroyAudioStream(audio_stream_);
        if (cd_audio_stream_) SDL_DestroyAudioStream(cd_audio_stream_);
        if (audio_device_) SDL_CloseAudioDevice(audio_device_);
        if (audio_initialized_) SDL_QuitSubSystem(SDL_INIT_AUDIO);
        close_joystick();
        if (overlay_frame_texture_) SDL_DestroyTexture(overlay_frame_texture_);
        if (overlay_font_texture_) SDL_DestroyTexture(overlay_font_texture_);
        if (overlay_symbols_texture_) SDL_DestroyTexture(overlay_symbols_texture_);
        if (overlay_title_texture_) SDL_DestroyTexture(overlay_title_texture_);
        if (overlay_panel_texture_) SDL_DestroyTexture(overlay_panel_texture_);
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
    bool should_present() const {
        return std::chrono::steady_clock::now() - last_present_ >=
               std::chrono::milliseconds(33);
    }
    bool should_poll() const {
        // The Unicorn loop can execute hundreds of 50K-instruction chunks
        // per second. SDL_PollEvent/joystick updates need only ~120 Hz.
        return std::chrono::steady_clock::now() - last_poll_ >=
               std::chrono::milliseconds(8) || should_present();
    }
    uint64_t input_polls() const { return input_polls_; }
    uint64_t frames_presented() const { return frames_presented_; }
    uint64_t image_frames() const { return image_frames_; }
    uint64_t changed_image_frames() const { return changed_image_frames_; }
    double image_change_hz() const {
        if (image_frames_ < 2 || last_image_time_ <= first_image_time_) return 0;
        return double(changed_image_frames_ - 1) /
               std::chrono::duration<double>(last_image_time_ - first_image_time_).count();
    }
    void play_audio(const ef2000::AudioBlock& block, bool cd = false) {
#ifdef EF_UNICORN_SDL3
        if (!enabled() || audio_disabled_) return;
        const auto pcm = (cd ? cd_pcm_converter_ : pcm_converter_).push(block);
        if (pcm.interleaved.empty()) return;
        const SDL_AudioSpec input{SDL_AUDIO_F32, 2, int(pcm.rate)};
        if (!audio_initialized_) {
            if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
                std::cerr << "SDL3 audio unavailable: " << SDL_GetError() << '\n';
                audio_disabled_ = true;
                return;
            }
            audio_initialized_ = true;
            audio_device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
            if (!audio_device_ || !SDL_ResumeAudioDevice(audio_device_)) {
                std::cerr << "SDL3 sound device unavailable: " << SDL_GetError() << '\n';
                if (audio_device_) SDL_CloseAudioDevice(audio_device_);
                audio_device_ = 0;
                audio_disabled_ = true;
                return;
            }
        }
        SDL_AudioStream*& stream = cd ? cd_audio_stream_ : audio_stream_;
        unsigned& rate = cd ? cd_audio_input_rate_ : audio_input_rate_;
        if (!stream) {
            stream = SDL_CreateAudioStream(&input, nullptr);
            if (!stream || !SDL_BindAudioStream(audio_device_, stream))
                throw std::runtime_error(std::string("SDL3 mixed audio stream: ") + SDL_GetError());
            rate = pcm.rate;
        }
        // Changing an SDL audio stream's input format on *every* DMA block
        // needlessly resets conversion state. Only do it when the guest's
        // programmed sample rate actually changes.
        if (rate != pcm.rate) {
            if (!SDL_SetAudioStreamFormat(stream, &input, nullptr))
                throw std::runtime_error(std::string("SDL3 input audio format: ") + SDL_GetError());
            rate = pcm.rate;
        }
        const int backlog = SDL_GetAudioStreamQueued(stream);
        if (backlog < 0) throw std::runtime_error(std::string("SDL3 audio queue: ") + SDL_GetError());
        if (backlog > int(pcm.rate) * 2 * int(sizeof(float)) * 2 &&
            !SDL_ClearAudioStream(stream))
            throw std::runtime_error(std::string("SDL3 audio overflow: ") + SDL_GetError());
        if (pcm.interleaved.size() > size_t(INT_MAX / sizeof(float)))
            throw std::runtime_error("audio block too large for SDL3");
        if (!SDL_PutAudioStreamData(stream, pcm.interleaved.data(),
                                    int(pcm.interleaved.size() * sizeof(float))))
            throw std::runtime_error(std::string("SDL3 audio playback: ") + SDL_GetError());
#else
        (void)block; (void)cd;
#endif
    }
    void clear_cd_audio() {
#ifdef EF_UNICORN_SDL3
        cd_pcm_converter_.reset();
        if (cd_audio_stream_ && !SDL_ClearAudioStream(cd_audio_stream_))
            throw std::runtime_error(std::string("SDL3 CD stream reset: ") + SDL_GetError());
#endif
    }
    std::vector<MouseEvent> take_mouse_events() {
        return std::exchange(pending_mouse_, {});
    }
    std::vector<KeyEvent> take_key_events() {
        return std::exchange(pending_keys_, {});
    }
    JoystickSnapshot joystick_state() {
        JoystickSnapshot value;
#ifdef EF_UNICORN_SDL3
        probe_joystick();
        if (gamepad_ || joystick_) {
            SDL_UpdateJoysticks();
            const bool focused = (SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS) != 0;
            static constexpr SDL_GamepadAxis gamepad_axes[] = {
                SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY,
                SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY};
            static constexpr SDL_GamepadButton gamepad_buttons[] = {
                SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
                SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH};
            if (gamepad_ && options_.gamepad_flight_profile) {
                const bool back = focused && SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_BACK);
                if (back && !last_help_button_) toggle_overlay();
                last_help_button_ = back;
                ef2000::GamepadFlightInput input;
                if (focused && !overlay_.visible()) {
                    input.left_x = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTX);
                    input.left_y = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTY);
                    input.right_x = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTX);
                    input.right_y = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTY);
                    for (size_t i = 0; i < 4; ++i)
                        input.face[i] = SDL_GetGamepadButton(gamepad_, gamepad_buttons[i]);
                    input.left_shoulder = SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
                    input.right_shoulder = SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
                }
                const auto now = std::chrono::steady_clock::now();
                const double dt = last_flight_sample_.time_since_epoch().count() ?
                    std::chrono::duration<double>(now - last_flight_sample_).count() : 0;
                last_flight_sample_ = now;
                auto mapped = gamepad_flight_.sample(input, dt);
                value.axes = mapped.axes;
                value.count = 4;
                value.buttons = mapped.buttons;
                for (const auto& change : mapped.keys) {
                    const int ascii = change.sdl_scan == 10 ? 'g' :
                        change.sdl_scan == 5 ? 'b' : change.sdl_scan == 8 ? 'e' :
                        change.sdl_scan == 21 ? 'r' : change.sdl_scan == 6 ? 'c' :
                        change.sdl_scan == 40 ? '\r' : change.sdl_scan == 42 ? '\b' : 0;
                    pending_keys_.push_back({change.sdl_scan, ascii, change.down, false});
                }
            } else {
                const unsigned raw_axes = joystick_ ?
                    unsigned(std::max(0, SDL_GetNumJoystickAxes(joystick_))) : 4u;
                for (const auto& binding : options_.joystick_axes) {
                    if (binding.index >= raw_axes ||
                        (gamepad_ && binding.index >= 4)) break;
                    int axis = 0;
                    if (focused && !overlay_.visible())
                        axis = gamepad_ ? SDL_GetGamepadAxis(gamepad_, gamepad_axes[binding.index]) :
                                          SDL_GetJoystickAxis(joystick_, binding.index);
                    value.axes[value.count++] = std::clamp(axis * binding.direction, -32768, 32767);
                }
                if (focused && !overlay_.visible()) {
                    const unsigned raw_buttons = joystick_ ?
                        unsigned(std::max(0, SDL_GetNumJoystickButtons(joystick_))) : 4u;
                    for (unsigned i = 0; i < 4; ++i) {
                        const bool pressed = gamepad_ ? SDL_GetGamepadButton(gamepad_, gamepad_buttons[i]) :
                            (i < raw_buttons && SDL_GetJoystickButton(joystick_, i));
                        if (pressed) value.buttons |= 1u << i;
                    }
                }
            }
        } else if (options_.gamepad_flight_profile) {
            // Disconnect must release every shoulder-generated keyboard key.
            for (const auto& change : gamepad_flight_.sample({}, 0).keys)
                pending_keys_.push_back({change.sdl_scan, 0, change.down, false});
            last_flight_sample_ = {};
            last_help_button_ = false;
        }
        const auto now = std::chrono::steady_clock::now();
        if (options_.trace_joystick && now >= next_joystick_trace_ &&
            (value.count != last_joystick_sample_.count ||
             value.buttons != last_joystick_sample_.buttons ||
             value.axes != last_joystick_sample_.axes)) {
            std::cerr << "input: joystick axes=";
            for (unsigned i = 0; i < value.count; ++i)
                std::cerr << (i ? "," : "") << value.axes[i];
            std::cerr << " buttons=0x" << std::hex << value.buttons << std::dec << '\n';
            last_joystick_sample_ = value;
            next_joystick_trace_ = now + std::chrono::milliseconds(250);
        }
#endif
        return value;
    }
    bool refresh(uint64_t instructions, uint64_t interrupts, const std::string& status,
                 const std::vector<uint8_t>* frame = nullptr, int width = 0, int height = 0) {
#ifdef EF_UNICORN_SDL3
        if (!enabled()) return true;
        last_poll_ = std::chrono::steady_clock::now();
        ++input_polls_;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                return false;
            if (event.type == SDL_EVENT_JOYSTICK_ADDED ||
                event.type == SDL_EVENT_JOYSTICK_REMOVED)
                next_joystick_probe_ = {};
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                const bool down = event.type == SDL_EVENT_KEY_DOWN;
                const int scan = int(event.key.scancode);
                const auto shortcut = host_shortcuts_.key(scan, (event.key.mod & SDL_KMOD_ALT) != 0,
                                                          down, bool(event.key.repeat));
                if (shortcut != ef2000::HostShortcuts::Action::Guest) {
                    if (shortcut == ef2000::HostShortcuts::Action::ToggleFullscreen)
                        toggle_fullscreen();
                    else if (shortcut == ef2000::HostShortcuts::Action::ToggleOverlay)
                        toggle_overlay();
                    continue;
                }
                if (overlay_.visible()) {
                    if (down && !event.key.repeat) {
                        if (scan == SDL_SCANCODE_ESCAPE) toggle_overlay();
                        else if (scan == SDL_SCANCODE_TAB) overlay_.focus_next();
                        else if (scan == SDL_SCANCODE_H)
                            overlay_.open_panel(ef2000::OverlayPanel::Kind::Controls);
                        else if (scan == SDL_SCANCODE_D)
                            overlay_.open_panel(ef2000::OverlayPanel::Kind::Diagnostics);
                        else if (scan == SDL_SCANCODE_DOWN || scan == SDL_SCANCODE_PAGEDOWN)
                            overlay_.wheel(scan == SDL_SCANCODE_DOWN ? -1 : -8,
                                           overlay_max_scroll());
                        else if (scan == SDL_SCANCODE_UP || scan == SDL_SCANCODE_PAGEUP)
                            overlay_.wheel(scan == SDL_SCANCODE_UP ? 1 : 8,
                                           overlay_max_scroll());
                    }
                    continue;
                }
                if (down) forwarded_keys_.insert(scan);
                else if (!forwarded_keys_.erase(scan)) continue;
                pending_keys_.push_back({scan, int(event.key.key), down, bool(event.key.repeat)});
            }
            if (event.type == SDL_EVENT_MOUSE_WHEEL && overlay_.visible()) {
                const float amount = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ?
                    -event.wheel.y : event.wheel.y;
                const int ticks = amount > 0 ? std::max(1, int(std::lround(amount))) :
                                  amount < 0 ? std::min(-1, int(std::lround(amount))) : 0;
                overlay_.wheel(ticks * 3, overlay_max_scroll());
                continue;
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION ||
                event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                // Translate through SDL's logical-presentation viewport, then
                // map 4:3 window coordinates into the original guest mode.
                if (!SDL_ConvertEventToRenderCoordinates(renderer_, &event)) continue;
                const bool motion = event.type == SDL_EVENT_MOUSE_MOTION;
                const float x = motion ? event.motion.x : event.button.x;
                const float y = motion ? event.motion.y : event.button.y;
                if (overlay_.visible()) {
                    if (motion) overlay_.motion(x, y);
                    else overlay_.button(x, y, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN,
                                         event.button.button == SDL_BUTTON_LEFT);
                    continue;
                }
                const int guest_width = width ? width : 640;
                const int guest_height = height ? height : 400;
                const SDL_FRect target = viewport(guest_width, guest_height);
                const int mouse_x = std::clamp(int((x - target.x) * guest_width / target.w),
                                               0, guest_width - 1);
                const int mouse_y = std::clamp(int((y - target.y) * guest_height / target.h),
                                               0, guest_height - 1);
                last_guest_mouse_x_ = mouse_x;
                last_guest_mouse_y_ = mouse_y;
                if (!motion) {
                    const unsigned bit = event.button.button == SDL_BUTTON_LEFT ? 1u :
                                         event.button.button == SDL_BUTTON_RIGHT ? 2u :
                                         event.button.button == SDL_BUTTON_MIDDLE ? 4u : 0u;
                    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) mouse_buttons_ |= bit;
                    else mouse_buttons_ &= ~bit;
                }
                pending_mouse_.push_back({0, mouse_x, mouse_y, mouse_buttons_});
            }
        }
        const auto now = std::chrono::steady_clock::now();
        if (now - last_present_ < std::chrono::milliseconds(33)) return true;
        if (!frame && width > 0) return true;
        last_present_ = now;
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
            if (options_.profile_video_frames || options_.show_fps || overlay_.visible()) {
                // A present is not necessarily a new game frame. Fingerprint
                // the actual RGBA pixels, including palette changes, to
                // distinguish repeated images from visible updates. The HUD
                // enables hashing as well as --profile-video-frames.
                uint64_t hash = UINT64_C(14695981039346656037);
                for (uint8_t byte : *frame) {
                    hash ^= byte;
                    hash *= UINT64_C(1099511628211);
                }
                const bool changed = !image_frames_ || hash != last_image_hash_ ||
                                     width != last_image_width_ || height != last_image_height_;
                if (changed) ++changed_image_frames_;
                last_image_hash_ = hash;
                last_image_width_ = width;
                last_image_height_ = height;
                const auto sampled = std::chrono::steady_clock::now();
                if (changed && (options_.show_fps || overlay_.visible()))
                    changed_frame_times_.push_back(sampled);
                if (!image_frames_) first_image_time_ = sampled;
                last_image_time_ = sampled;
                ++image_frames_;
            }
            if (!texture_ || width != texture_width_ || height != texture_height_) {
                if (texture_) SDL_DestroyTexture(texture_);
                texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING, width, height);
                if (!texture_) throw std::runtime_error(std::string("SDL3 texture: ") + SDL_GetError());
                texture_width_ = width;
                texture_height_ = height;
                if (!SDL_SetTextureScaleMode(texture_, options_.smooth_scaling ?
                        SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST))
                    throw std::runtime_error(std::string("SDL3 texture scale: ") + SDL_GetError());
            }
            const SDL_FRect destination = viewport(width, height);
            if (!SDL_UpdateTexture(texture_, nullptr, frame->data(), width * 4) ||
                !SDL_RenderTexture(renderer_, texture_, nullptr, &destination))
                throw std::runtime_error(std::string("SDL3 video present failed: ") + SDL_GetError());
        } else {
            if (!SDL_SetRenderDrawColor(renderer_, 230, 237, 245, 255) ||
                !SDL_RenderDebugText(renderer_, 30, 30, "EF2000 - NATIVE C++ / UNICORN") ||
                !SDL_RenderDebugText(renderer_, 30, 55, "WAITING FOR NONBLACK GAME VIDEO") ||
                !SDL_RenderDebugText(renderer_, 30, 95, status.c_str()))
                throw std::runtime_error(std::string("SDL3 text draw failed: ") + SDL_GetError());
            const std::string progress =
                (status.find("REAL-TIME") != std::string::npos ? "PIT TICKS: " :
                 "GUEST INSTRUCTION BUDGET: ") + std::to_string(instructions);
            const std::string events = "DOS/X32 INTERRUPTS: " + std::to_string(interrupts);
            if (!SDL_RenderDebugText(renderer_, 30, 125, progress.c_str()) ||
                !SDL_RenderDebugText(renderer_, 30, 145, events.c_str()))
                throw std::runtime_error(std::string("SDL3 text draw failed: ") + SDL_GetError());
        }
        if (options_.show_fps || overlay_.visible()) {
            const auto sampled = std::chrono::steady_clock::now();
            present_times_.push_back(sampled);
            const auto oldest = sampled - std::chrono::seconds(1);
            while (!present_times_.empty() && present_times_.front() < oldest)
                present_times_.pop_front();
            while (!changed_frame_times_.empty() && changed_frame_times_.front() < oldest)
                changed_frame_times_.pop_front();
            if (options_.show_fps) {
                const SDL_FRect background{5, 5, 240, 20};
                const std::string label = "SDL FPS " + std::to_string(present_times_.size()) +
                    "   CHANGED FPS " + std::to_string(changed_frame_times_.size());
                if (!SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255) ||
                    !SDL_RenderFillRect(renderer_, &background) ||
                    !SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255) ||
                    !SDL_RenderDebugText(renderer_, 10, 11, label.c_str()))
                    throw std::runtime_error(std::string("SDL3 FPS HUD: ") + SDL_GetError());
            }
            if (overlay_.visible()) draw_overlay(instructions, interrupts, status, width, height);
        }
        if (!SDL_RenderPresent(renderer_))
            throw std::runtime_error(std::string("SDL3 present failed: ") + SDL_GetError());
        ++frames_presented_;
        return true;
#else
        (void)instructions; (void)interrupts; (void)status; (void)frame; (void)width; (void)height;
        return true;
#endif
    }
private:
    WindowOptions options_;
    ef2000::PcmConverter pcm_converter_;
    ef2000::PcmConverter cd_pcm_converter_;
    unsigned mouse_buttons_{};
    std::vector<MouseEvent> pending_mouse_;
    std::vector<KeyEvent> pending_keys_;
    ef2000::GamepadFlightProfile gamepad_flight_{};
    ef2000::HostShortcuts host_shortcuts_{};
    ef2000::OverlayWindows overlay_{};
    bool last_help_button_{};
    std::set<int> forwarded_keys_;
    int last_guest_mouse_x_{}, last_guest_mouse_y_{};
    std::chrono::steady_clock::time_point last_flight_sample_{};
    std::deque<std::chrono::steady_clock::time_point> present_times_, changed_frame_times_;
    std::chrono::steady_clock::time_point last_present_{};
    std::chrono::steady_clock::time_point last_poll_{};
    uint64_t input_polls_{}, frames_presented_{};
    uint64_t image_frames_{}, changed_image_frames_{}, last_image_hash_{};
    int last_image_width_{}, last_image_height_{};
    std::chrono::steady_clock::time_point first_image_time_{}, last_image_time_{};
#ifdef EF_UNICORN_SDL3
    static const std::vector<std::string>& controls_help_lines() {
        static const std::vector<std::string> lines{
            "Host controls",
            "F12 / Gamepad Back  Toggle overlay",
            "F11 / Alt+Enter    Fullscreen",
            "F10                Padlock (close help)",
            "Esc                Close overlay",
            "Tab                Next window",
            "H / D              Open help / stats",
            "Up/Down, PgUp/PgDn Scroll help",
            "",
            "Gamepad flight (experimental)",
            "Left stick         Roll / pitch",
            "Right X            Rudder axis 4",
            "Right Y            Latched throttle",
            "A / B / X / Y      Gameport buttons",
            "LB + A/B/X/Y       Gear/brake/ECM/radar",
            "RB + A/B/X/Y       Target/air/AG/view",
            "--gamepad-raw      Original mapping",
            "",
            "EF2000 keyboard - main flight",
            "P                  Pause",
            "A                  Autopilot",
            "L                  Auto level",
            "G                  Landing gear",
            "B                  Air brake",
            "W                  Wheel brake",
            "E                  ECM",
            "R                  Radar",
            "T                  TIALD",
            "I                  ILS",
            "D                  DASS",
            "J                  JTIDS",
            "U                  HUD brightness",
            "V                  Night vision",
            "M                  Map",
            "Q                  Wingman commands",
            "1-9 / 0            Wingman menu / back",
            "Y / N              Yes / no",
            "C                  Next target",
            "Enter              Next air weapon",
            "Backspace          Next ground weapon",
            "Space              Fire weapon",
            "- / =              Throttle down / up",
            ", / .              Rudder left / right",
            "[ / ]              Engine left / right",
            "`                  Setup menu",
            "",
            "Views",
            "F1                 HUD views",
            "F2                 Wide cockpit",
            "F3                 Rear view",
            "F4                 Player external",
            "F5                 Wingman external",
            "F6                 Fly-by",
            "F7                 Target view",
            "F8                 Weapon view",
            "F9                 Browse aircraft",
            "F10                Padlock views",
            "",
            "Other useful shortcuts",
            "Shift+Esc          Eject",
            "Shift+Q            Quit to DOS",
            "Shift+W            Next waypoint",
            "Alt+W              Previous waypoint",
            "Alt+R              Video mode",
            "Alt+D              Graphics detail",
            "Insert / Delete    Chaff / flares",
            "KP / / *           AB off / on",
            "KP - / +           Throttle down / up",
            "KP 1/2/3           MFD 1/2/3",
            "KP 0               Briefing",
            "",
            "Based on original keyboard chart",
            "Control details may vary by version"
        };
        return lines;
    }
    int overlay_max_scroll() const {
        if (overlay_.focused_panel() != 0) return 0;
        const auto& panel = overlay_.panels()[0];
        const int rows = std::max(1, int((panel.h - ef2000::OverlayWindows::title_height - 35) /
                                         ef2000::OverlayWindows::line_height));
        return std::max(0, int(controls_help_lines().size()) - rows);
    }
    void toggle_overlay() {
        const bool opening = !overlay_.visible();
        overlay_.toggle();
        if (!opening) return;
        // Host-owned windows must not leave a guest key or mouse button
        // latched when they take focus. Gamepad chords release on resampling.
        for (const int scan : forwarded_keys_)
            pending_keys_.push_back({scan, 0, false, false});
        forwarded_keys_.clear();
        if (mouse_buttons_) {
            mouse_buttons_ = 0;
            pending_mouse_.push_back({0, last_guest_mouse_x_, last_guest_mouse_y_, 0});
        }
    }
    void toggle_fullscreen() {
        const bool fullscreen = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;
        if (!SDL_SetWindowFullscreen(window_, !fullscreen))
            throw std::runtime_error(std::string("SDL3 fullscreen: ") + SDL_GetError());
    }
    void overlay_fill(float x, float y, float w, float h,
                      uint8_t red, uint8_t green, uint8_t blue) {
        const SDL_FRect area{x, y, w, h};
        if (!SDL_SetRenderDrawColor(renderer_, red, green, blue, 255) ||
            !SDL_RenderFillRect(renderer_, &area))
            throw std::runtime_error(std::string("SDL3 overlay panel: ") + SDL_GetError());
    }
    void overlay_text(float x, float y, std::string value, int max_pixels,
                      uint8_t red = 230, uint8_t green = 232, uint8_t blue = 222) {
        if (max_pixels <= 0) return;
        if (overlay_font_texture_) {
            value.resize(ef2000::menu_font_fitting_chars(overlay_font_widths_,
                                                        value, max_pixels));
            const auto pass = [&](float dx, float dy, uint8_t r, uint8_t g, uint8_t b) {
                if (!SDL_SetTextureColorMod(overlay_font_texture_, r, g, b))
                    throw std::runtime_error(std::string("SDL3 overlay font tint: ") + SDL_GetError());
                float offset = 0;
                for (unsigned char code : value) {
                    const auto glyph = ef2000::menu_font_glyph(overlay_font_widths_, code);
                    if (glyph.available && glyph.width > 0) {
                        const SDL_FRect source{float(glyph.x), float(glyph.y),
                                               float(glyph.width), float(glyph.height)};
                        const SDL_FRect target{x + offset + dx, y + dy,
                            float(glyph.width), float(glyph.height) * ef2000::menu_font_pixel_aspect};
                        if (!SDL_RenderTexture(renderer_, overlay_font_texture_, &source, &target))
                            throw std::runtime_error(std::string("SDL3 menu font render: ") + SDL_GetError());
                    } else if (!glyph.available) {
                        const char fallback[]{char(code), '\0'};
                        if (!SDL_SetRenderDrawColor(renderer_, r, g, b, 255) ||
                            !SDL_RenderDebugText(renderer_, x + offset + dx, y + dy, fallback))
                            throw std::runtime_error(std::string("SDL3 menu font fallback: ") + SDL_GetError());
                    }
                    offset += float(glyph.advance);
                }
            };
            // The DOS text blitter offsets black shadow pixels behind the
            // white glyphs; the original font sheet stores just the mask.
            pass(1.0f, ef2000::menu_font_pixel_aspect, 0, 0, 0);
            pass(0, 0, red, green, blue);
            return;
        }
        if (value.size() > size_t(max_pixels / 8)) value.resize(size_t(max_pixels / 8));
        if (!SDL_SetRenderDrawColor(renderer_, red, green, blue, 255) ||
            !SDL_RenderDebugText(renderer_, x, y, value.c_str()))
            throw std::runtime_error(std::string("SDL3 overlay text: ") + SDL_GetError());
    }
    void overlay_skin_fill(SDL_Texture* texture, float x, float y, float w, float h,
                           uint8_t red, uint8_t green, uint8_t blue) {
        overlay_fill(x, y, w, h, red, green, blue);
        if (!texture) return;
        const auto patch = ef2000::overlay_skin_patch(x, y, w, h);
        if (patch.target.w <= 0 || patch.target.h <= 0) return;
        const SDL_FRect source{patch.source.x, patch.source.y, patch.source.w, patch.source.h};
        const SDL_FRect target{patch.target.x, patch.target.y, patch.target.w, patch.target.h};
        if (!SDL_RenderTexture(renderer_, texture, &source, &target))
            throw std::runtime_error(std::string("SDL3 overlay skin draw: ") + SDL_GetError());
    }
    void overlay_chrome_button(float x, float y, int kind, bool toggled) {
        // Original-style shallow silver bevel and black shadow; glyphs are
        // drawn as one-pixel strokes rather than font letters.
        overlay_fill(x + 1, y + 1, 15, 17, 18, 18, 23);
        overlay_fill(x, y, 14, 16, 172, 174, 191);
        overlay_fill(x + 1, y + 1, 12, 14, 100, 102, 121);
        overlay_fill(x + 2, y + 2, 10, 12, 138, 141, 160);
        overlay_fill(x + 12, y + 1, 1, 14, 49, 52, 64);
        overlay_fill(x + 1, y + 14, 12, 1, 49, 52, 64);
        if (overlay_symbols_texture_ && (kind == 1 || kind == 2)) {
            // The specimen's window and X drawings cross the nominal 16px
            // grid: the window motif starts at x=76, not at cell x=80.
            // Clip both to their original glyph bounds so adjacent shapes
            // are never rendered in the titlebar buttons.
            const SDL_FRect source{kind == 1 ? 76.0f : 64.0f, 16,
                                   kind == 1 ? 16.0f : 12.0f, 14};
            // The original maximize motif is compact within the bevel; the
            // X is almost full height. Keep each symbol's own proportions.
            const SDL_FRect destination = kind == 1 ?
                SDL_FRect{x + 3, y + 3, 9, 10} :
                SDL_FRect{x + 1, y + 1, 12, 14};
            if (!SDL_RenderTexture(renderer_, overlay_symbols_texture_, &source, &destination))
                throw std::runtime_error(std::string("SDL3 original button symbol: ") + SDL_GetError());
            return;
        }
        const uint8_t fg = 19;
        if (kind == 0) {
            // TACTCOM depicts the leftmost control as a recessed *window*,
            // not the modern underscore/minimize glyph. Keep its behavior.
            overlay_fill(x + 3, y + 3, 9, 11, 31, 33, 40);
            overlay_fill(x + 4, y + 4, 7, 9, 97, 100, 116);
            overlay_fill(x + 4, y + 3, 7, 1, 185, 189, 204);
            overlay_fill(x + 11, y + 3, 1, 10, 185, 189, 204);
            overlay_fill(x + 4, y + 13, 8, 1, 47, 49, 59);
        } else if (kind == 1) {
            if (toggled) {
                overlay_fill(x + 5, y + 3, 6, 1, fg, fg, fg);
                overlay_fill(x + 5, y + 3, 1, 6, fg, fg, fg);
                overlay_fill(x + 3, y + 5, 6, 1, fg, fg, fg);
                overlay_fill(x + 3, y + 5, 1, 6, fg, fg, fg);
            } else {
                overlay_fill(x + 3, y + 3, 8, 1, fg, fg, fg);
                overlay_fill(x + 3, y + 3, 1, 8, fg, fg, fg);
                overlay_fill(x + 10, y + 3, 1, 8, fg, fg, fg);
                overlay_fill(x + 3, y + 10, 8, 1, fg, fg, fg);
            }
        } else {
            for (int step = 0; step < 8; ++step) {
                overlay_fill(x + 3 + step, y + 3 + step, 1, 1, fg, fg, fg);
                overlay_fill(x + 10 - step, y + 3 + step, 1, 1, fg, fg, fg);
            }
        }
    }
    void overlay_frame(const ef2000::OverlayPanel& panel) {
        if (!overlay_frame_texture_) {
            // TACTCOM's narrow chrome: one dark left/bottom edge, silver top
            // and right hairlines, and an interior texture beginning directly
            // beneath the full-width title. Avoid nesting several thick
            // inner frames, which obscures the original panel pixels.
            overlay_fill(panel.x, panel.y, panel.w, panel.h, 40, 40, 52);
            if (panel.h > ef2000::OverlayWindows::title_height + 1)
                overlay_skin_fill(overlay_panel_texture_, panel.x + 1,
                                  panel.y + ef2000::OverlayWindows::title_height,
                                  panel.w - 2,
                                  panel.h - ef2000::OverlayWindows::title_height - 1,
                                  115, 115, 115);
            overlay_fill(panel.x, panel.y, panel.w, 1, 159, 159, 175);
            overlay_fill(panel.x + panel.w - 1, panel.y, 1, panel.h, 159, 159, 175);
            return;
        }
        const auto patches = ef2000::nine_slice(float(overlay_frame_width_),
            float(overlay_frame_height_), float(options_.overlay_frame_border),
            {panel.x, panel.y, panel.w, panel.h});
        for (const auto& patch : patches) {
            if (patch.target.w <= 0 || patch.target.h <= 0) continue;
            const SDL_FRect source{patch.source.x, patch.source.y, patch.source.w, patch.source.h};
            const SDL_FRect destination{patch.target.x, patch.target.y,
                                        patch.target.w, patch.target.h};
            if (!SDL_RenderTexture(renderer_, overlay_frame_texture_, &source, &destination))
                throw std::runtime_error(std::string("SDL3 overlay frame draw: ") + SDL_GetError());
        }
        // Keep help readable across arbitrary user-cropped frame artworks.
        const float border = float(options_.overlay_frame_border);
        if (panel.h > ef2000::OverlayWindows::title_height + border + 2)
            overlay_fill(panel.x + border, panel.y + ef2000::OverlayWindows::title_height + 1,
                         panel.w - 2 * border,
                         panel.h - ef2000::OverlayWindows::title_height - border - 2,
                         91, 91, 91);
    }
    void draw_overlay(uint64_t instructions, uint64_t interrupts, const std::string& status,
                      int width, int height) {
        const auto sampled = std::chrono::steady_clock::now();
        const auto oldest = sampled - std::chrono::seconds(1);
        while (!present_times_.empty() && present_times_.front() < oldest)
            present_times_.pop_front();
        while (!changed_frame_times_.empty() && changed_frame_times_.front() < oldest)
            changed_frame_times_.pop_front();
        for (const int index : overlay_.order()) {
            const auto& panel = overlay_.panels()[index];
            if (!panel.open) continue;
            const bool focused = overlay_.focused_panel() == index;
            // Original windows cast their solid offset shadow to the LEFT,
            // visible as a three-pixel black strip outside the left frame.
            overlay_fill(panel.x - 4, panel.y + 4, panel.w, panel.h, 0, 0, 0);
            overlay_frame(panel);
            const std::string title = index == 0 ? "Flight Controls" : "System Monitor";
            const float title_width = overlay_font_texture_ ?
                float(ef2000::menu_font_text_width(overlay_font_widths_, title)) :
                float(title.size() * 8);
            const float tab_width = std::min(panel.w - 67, title_width + 20);
            if (overlay_title_texture_) {
                // TACTCOM's title texture runs across the full window width;
                // only the older options menu has stepped short title tabs.
                overlay_skin_fill(overlay_title_texture_, panel.x + 1, panel.y + 1,
                                  panel.w - 2, ef2000::OverlayWindows::title_height - 3,
                                  45, 59, 35);
                overlay_fill(panel.x + 1, panel.y, panel.w - 2, 1, 159, 159, 175);
                overlay_fill(panel.x + 1,
                             panel.y + ef2000::OverlayWindows::title_height - 2,
                             panel.w - 2, 2, 40, 40, 52);
                overlay_text(panel.x + 13, panel.y + 6, title,
                             int(panel.w - 68), 255, 255, 255);
            } else {
                // The old options menu instead has a stepped short title tab.
                overlay_skin_fill(overlay_title_texture_, panel.x + 5, panel.y + 5,
                                  tab_width - 9, 17, focused ? 62 : 46,
                                  focused ? 69 : 51, focused ? 42 : 31);
                overlay_skin_fill(overlay_title_texture_, panel.x + 5 + tab_width - 9,
                                  panel.y + 8, 9, 14, focused ? 62 : 46,
                                  focused ? 69 : 51, focused ? 42 : 31);
                overlay_fill(panel.x + 5, panel.y + 4, tab_width - 12, 1, 173, 185, 159);
                overlay_fill(panel.x + 5 + tab_width - 12, panel.y + 5, 5, 1, 173, 185, 159);
                overlay_fill(panel.x + 5 + tab_width - 7, panel.y + 6, 4, 1, 173, 185, 159);
                overlay_text(panel.x + 10, panel.y + 9, title,
                             int(tab_width - 10), 255, 255, 255);
            }
            // Match the original three square titlebar controls: minimize,
            // maximize/restore, and close, all with actionable hit targets.
            overlay_chrome_button(panel.x + panel.w - 47, panel.y + 4, 0, panel.minimized);
            overlay_chrome_button(panel.x + panel.w - 32, panel.y + 4, 1, panel.maximized);
            overlay_chrome_button(panel.x + panel.w - 17, panel.y + 4, 2, false);
            if (panel.minimized) continue;
            overlay_fill(panel.x + panel.w - 13, panel.y + panel.h - 7, 8, 1, 159, 159, 175);
            overlay_fill(panel.x + panel.w - 7, panel.y + panel.h - 13, 1, 8, 159, 159, 175);

            const std::vector<std::string> diagnostics{
                "Video / Emulation",
                "SDL FPS: " + std::to_string(present_times_.size()),
                "Changed: " + std::to_string(changed_frame_times_.size()),
                "Video: " + std::to_string(width) + "x" + std::to_string(height),
                "Guest: " + std::to_string(instructions),
                "IRQ: " + std::to_string(interrupts),
                "",
                "Profile: " + std::to_string(gamepad_flight_.throttle_percent()) + "% throttle",
                "Runs while overlay open",
                status
            };
            const auto& lines = index == 0 ? controls_help_lines() : diagnostics;
            const int rows = std::max(1, int((panel.h - ef2000::OverlayWindows::title_height - 35) /
                                             ef2000::OverlayWindows::line_height));
            overlay_.clamp_scroll(index, std::max(0, int(lines.size()) - rows));
            const int scroll = overlay_.panels()[index].scroll;
            const float content_inset = overlay_frame_texture_ ?
                std::max(10.0f, float(options_.overlay_frame_border) + 2) : 10.0f;
            const int max_pixels = std::max(1, int(panel.w - content_inset - 10));
            for (int line = scroll; line < int(lines.size()) && line < scroll + rows; ++line) {
                const std::string& value = lines[size_t(line)];
                const bool heading = value == "Host controls" || value == "Gamepad flight (experimental)" ||
                    value == "EF2000 keyboard - main flight" || value == "Views" ||
                    value == "Other useful shortcuts" || value == "Video / Emulation";
                overlay_text(panel.x + content_inset,
                             panel.y + 31 + float(line - scroll) * ef2000::OverlayWindows::line_height,
                             value, max_pixels,
                             heading ? 218 : 242, heading ? 226 : 242, heading ? 204 : 242);
            }
            overlay_text(panel.x + content_inset, panel.y + panel.h - 15,
                         index == 0 ? "Scroll: wheel / Up-Down" : "Live guest display",
                         max_pixels, 210, 218, 204);
        }
        overlay_fill(0, 458, 640, 22, 39, 43, 23);
        overlay_text(8, 465, "F12 Close  F11 / Alt+Enter Fullscreen  Tab Focus  H Help  D Stats",
                     624, 255, 255, 255);
    }
    SDL_AudioStream* audio_stream_{};
    SDL_AudioStream* cd_audio_stream_{};
    SDL_AudioDeviceID audio_device_{};
    bool audio_initialized_{}, audio_disabled_{};
    unsigned audio_input_rate_{}, cd_audio_input_rate_{};
    SDL_FRect viewport(int width, int height) const {
        if (options_.aspect_ratio != "native" || width <= 0 || height <= 0)
            return {0, 0, 640, 480};
        const float scale = std::min(640.0f / width, 480.0f / height);
        const float w = width * scale, h = height * scale;
        return {(640 - w) / 2, (480 - h) / 2, w, h};
    }
    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    SDL_Texture* texture_{};
    SDL_Texture* overlay_frame_texture_{};
    SDL_Texture* overlay_font_texture_{};
    SDL_Texture* overlay_symbols_texture_{};
    SDL_Texture* overlay_title_texture_{};
    SDL_Texture* overlay_panel_texture_{};
    ef2000::MenuFontWidths overlay_font_widths_{};
    int overlay_frame_width_{}, overlay_frame_height_{};
    SDL_Joystick* joystick_{};
    SDL_Gamepad* gamepad_{};
    JoystickSnapshot last_joystick_sample_{};
    std::chrono::steady_clock::time_point next_joystick_probe_{}, next_joystick_trace_{};
    int texture_width_{}, texture_height_{};
    void close_joystick() {
        if (gamepad_) SDL_CloseGamepad(gamepad_);
        if (joystick_) SDL_CloseJoystick(joystick_);
        gamepad_ = nullptr;
        joystick_ = nullptr;
    }
    void probe_joystick(bool startup = false) {
        if (options_.joystick_index < 0) {
            if (startup) std::cerr << "input: physical joystick disabled\n";
            return;
        }
        if ((gamepad_ && !SDL_GamepadConnected(gamepad_)) ||
            (joystick_ && !SDL_JoystickConnected(joystick_))) {
            close_joystick();
            std::cerr << "input: joystick disconnected\n";
        }
        if (gamepad_ || joystick_) return;
        const auto now = std::chrono::steady_clock::now();
        if (now < next_joystick_probe_) return;
        next_joystick_probe_ = now + std::chrono::seconds(1);
        int count{};
        SDL_JoystickID* ids = SDL_GetJoysticks(&count);
        if (ids && options_.joystick_index < count) {
            const auto id = ids[options_.joystick_index];
            if (SDL_IsGamepad(id)) gamepad_ = SDL_OpenGamepad(id);
            if (!gamepad_) joystick_ = SDL_OpenJoystick(id);
            if (gamepad_ || joystick_) {
                const char* name = gamepad_ ? SDL_GetGamepadName(gamepad_) :
                                              SDL_GetJoystickName(joystick_);
                std::cerr << "input: SDL3 " << (gamepad_ ? "gamepad " : "joystick ")
                          << options_.joystick_index << ": " << (name ? name : "unknown")
                          << "; mapped DOS gameport axes=" << (gamepad_ && options_.gamepad_flight_profile ?
                                                                  4 : options_.joystick_axes.size())
                          << (gamepad_ && options_.gamepad_flight_profile ?
                              " (latched throttle + LB/RB layers, Back/F12 overlay)" : " (raw)")
                          << " (use --trace-joystick for input samples)\n";
            } else {
                std::cerr << "input: joystick " << options_.joystick_index
                          << " could not be opened: " << SDL_GetError() << '\n';
            }
        } else if (startup) {
            std::cerr << "input: joystick " << options_.joystick_index
                      << " not found (SDL3 sees " << count << "); no physical input\n";
        }
        if (ids) SDL_free(ids);
    }
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
#ifdef _WIN32
    // Windows has SHA-256 in CryptoAPI; no OpenSSL DLL is needed in the
    // distributable, and the original DOS executable remains untouched.
    HCRYPTPROV provider{};
    HCRYPTHASH hash{};
    if (!CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT))
        throw std::runtime_error("Windows SHA-256 provider unavailable");
    std::array<uint8_t, 32> digest{};
    DWORD length = DWORD(digest.size());
    const bool hashed = CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) &&
                        CryptHashData(hash, data.data(), DWORD(data.size()), 0) &&
                        CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &length, 0) &&
                        length == digest.size();
    if (hash) CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    if (!hashed) throw std::runtime_error("Windows SHA-256 computation failed");
#else
    std::array<uint8_t, 32> digest{};
    SHA256(data.data(), data.size(), digest.data());
#endif
    char hex[65]{};
    for (size_t i = 0; i < digest.size(); ++i)
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
// Unicorn 2.1.3's batch API takes int* (newer headers may accept const int*).
// Keep the stable register ID list writable to compile against both variants.
int REG_IDS[] = {UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
    UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS,
    UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS};
static_assert(sizeof(Registers) == sizeof(REG_IDS) / sizeof(int) * sizeof(uint32_t));

class Machine {
public:
    explicit Machine(const std::vector<uint8_t>& image, fs::path game_dir, fs::path cd_dir,
                     fs::path overlay_dir,
                     ef2000::SoundMode sound_mode = ef2000::SoundMode::Off,
                     bool trace_interrupts = false,
                     std::vector<MouseEvent> mouse_events = {},
                     std::vector<TimedKeyEvent> key_events = {},
                     std::vector<JoystickEvent> joystick_events = {},
                     ControlSelection controls = ControlSelection::Auto,
                     fs::path cd_cue = {})
        : image_size_(image.size()), trace_interrupts_(trace_interrupts),
          mouse_events_(std::move(mouse_events)), key_events_(std::move(key_events)),
          joystick_events_(std::move(joystick_events)), controls_(controls),
          dos_drive_(std::move(game_dir), std::move(cd_dir), std::move(overlay_dir)),
          digital_sound_enabled_(sound_mode != ef2000::SoundMode::Off), real_cpu_(sound_mode) {
        if (!cd_cue.empty()) cd_audio_ = std::make_unique<ef2000::CdAudio>(cd_cue);
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
        if (!joystick_events_.empty()) set_joystick(joystick_events_.front().count,
                                                   {0, 0, 0, 0}, 0);
        uc_hook hook{};
        check(uc_hook_add(cpu_, &hook, UC_HOOK_INTR, (void*)on_interrupt, this, 1, 0),
              "install interrupt hook");
        check(uc_hook_add(cpu_, &hook, UC_HOOK_INSN, (void*)on_port_in, this,
                          1, 0, UC_X86_INS_IN), "install port IN hook");
        check(uc_hook_add(cpu_, &hook, UC_HOOK_INSN, (void*)on_port_out, this,
                          1, 0, UC_X86_INS_OUT), "install port OUT hook");
        check(uc_hook_add(cpu_, &hook, UC_HOOK_MEM_READ_UNMAPPED |
                          UC_HOOK_MEM_WRITE_UNMAPPED | UC_HOOK_MEM_FETCH_UNMAPPED,
                          (void*)on_invalid_memory, this, 1, 0),
              "install invalid-memory diagnostic hook");
    }
    Machine(const Machine&) = delete;
    Machine& operator=(const Machine&) = delete;
    ~Machine() { if (cpu_) uc_close(cpu_); }

    void enable_archive_lookup_trace() {
        check(uc_hook_add(cpu_, &archive_lookup_hook_, UC_HOOK_CODE,
                          (void*)on_archive_lookup, this, 0xeb520, 0xeb520),
              "install archive-lookup trace hook");
    }

    int run(uint64_t limit, WindowOptions window_options, uint64_t timer_interval,
            const std::string& framebuffer, const std::string& wav_directory,
            bool realtime = false, double wall_seconds = 0,
            PerformanceOptions performance = {}) {
        performance_ = performance;
        if (performance_.tcg_buffer_mib)
            check(uc_ctl_set_tcg_buffer_size(cpu_, performance_.tcg_buffer_mib << 20),
                  "set Unicorn translated-code cache");
        const bool trace_joystick = window_options.trace_joystick;
        const bool profile_video_frames = window_options.profile_video_frames;
        if (!wav_directory.empty())
            wav_capture_ = std::make_unique<ef2000::WavCapture>(wav_directory);
        if (!wav_directory.empty() && cd_audio_)
            cd_wav_capture_ = std::make_unique<ef2000::WavCapture>(
                fs::path(wav_directory) / "cd-audio");
        StartupWindow window(std::move(window_options));
        active_window_ = &window;
        const auto joystick = window.joystick_state();
        if (joystick.count) set_joystick(joystick.count, joystick.axes, joystick.buttons);
        else if (controls_ == ControlSelection::Joystick && joystick_events_.empty())
            set_joystick(2, {}, 0);
        std::cout << "C++ X-32: original payload " << image_size_ << " bytes; "
                  << "instruction budget " << (limit ? std::to_string(limit) : "unlimited")
                  << "; timing " << (realtime ? "real-time PIT" : "instruction-counted") << '\n';
        if (!refresh(window, 0, "INITIALIZING X-32 GUEST")) return 0;
        perf_stats_ = {}; // Exclude the initial SDL window setup from timed playback.
        const auto start = std::chrono::steady_clock::now();
        // Bounded diagnostic mode retains instruction-based ticks and exact
        // replay positions. With an unlimited interactive budget, avoid the
        // Unicorn per-instruction counting hook altogether: its timeout-only
        // emulation runs translated blocks much faster and PIT ticks come
        // from the host monotonic clock.
        uint64_t completed = 0;
        uint64_t slices = 0;
        bool running = true;
        constexpr double PIT_HZ = 1193182.0;
        auto pit_period = [&] {
            return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(pit_divisor_ / PIT_HZ));
        };
        auto next_pit = start + pit_period();
        if (realtime)
            check(uc_hook_add(cpu_, &slice_hook_, UC_HOOK_BLOCK, (void*)on_realtime_block,
                              this, 1, 0), "install real-time block-boundary hook");
        while (running && (!limit || completed < limit) && status_ == "instruction_limit") {
            uint32_t ip{};
            check(uc_reg_read(cpu_, UC_X86_REG_EIP, &ip), "read instruction pointer");
            // Keep intermittent 3DAh reads fresh across emulation slices in
            // both instruction-timed diagnostics and real-time mode.
            vga_reads_until_check_ = 1;
            const auto guest_started = performance_.profile ? std::chrono::steady_clock::now() :
                                       std::chrono::steady_clock::time_point{};
            if (realtime) {
                // Stop at a translated basic-block boundary every ~10 ms.
                // Unlike Unicorn's instruction-counted emulation this only
                // hooks blocks, not every guest instruction. Avoid the
                // timeout-based stop, which can leave EIP inside an opcode.
                slice_deadline_ = std::chrono::steady_clock::now() +
                                  std::chrono::milliseconds(10);
                const uc_err result = uc_emu_start(cpu_, ip, 0, 0, 0);
                if (result != UC_ERR_OK) {
                    const auto regs = read_regs();
                    std::ostringstream message;
                    message << "real-time guest execution: " << uc_strerror(result)
                            << " ip=0x" << std::hex << regs.ip
                            << " sp=0x" << regs.sp << std::dec
                            << " ticks=" << ticks_ << " irqs=" << irq0_
                            << " ax=0x" << std::hex << regs.ax
                            << " bx=0x" << regs.bx << " cx=0x" << regs.cx
                            << " dx=0x" << regs.dx << " si=0x" << regs.si
                            << " di=0x" << regs.di << " ds=0x" << regs.ds
                            << " es=0x" << regs.es << std::dec
                            << " irq_active=" << irq_in_service_
                            << " fault=" << invalid_memory_;
                    throw std::runtime_error(message.str());
                }
            } else {
                constexpr uint64_t quantum = 1000;
                uint64_t amount = std::min<uint64_t>(quantum, limit ? limit - completed : quantum);
                amount = std::min(amount, timer_interval - completed % timer_interval);
                check(uc_emu_start(cpu_, ip, 0, 0, amount), "instruction-counted guest execution");
                completed += amount;
            }
            if (pending_copy_) finish_fast_copy();
            if (performance_.profile)
                perf_stats_.guest_time += std::chrono::steady_clock::now() - guest_started;
            if (status_ != "instruction_limit") break;
            ++slices;
            if (eoi_hook_ && !irq_in_service_) {
                check(uc_hook_del(cpu_, eoi_hook_), "remove IRQ return hook");
                eoi_hook_ = 0;
            }
            if (realtime) {
                // Audio DMA and Red Book playback need periodic service even
                // when the guest has programmed a slow BIOS/PIT divisor.
                service_sound();
                if (status_ == "instruction_limit") service_cd();
                const auto now = std::chrono::steady_clock::now();
                unsigned catchup = 0;
                while (now >= next_pit && catchup++ < 4 && status_ == "instruction_limit") {
                    deliver_tick(ip_at_cpu());
                    next_pit += pit_period();
                }
                if (now >= next_pit) next_pit = now + pit_period();
            } else if (completed % timer_interval == 0) deliver_tick(ip_at_cpu());
            if (status_ != "instruction_limit") break;
            if (irq_in_service_ && !eoi_hook_) {
                check(uc_hook_add(cpu_, &eoi_hook_, UC_HOOK_CODE, (void*)on_irq_resume,
                                  this, irq_resume_ip_, irq_resume_ip_), "hook IRQ return");
                check(uc_ctl_remove_cache(cpu_, irq_resume_ip_, irq_resume_ip_ + 1),
                      "invalidate IRQ return translation");
            }
            if (window.enabled() && (realtime || completed % 50000 == 0))
                running = refresh(window, realtime ? ticks_ : completed,
                                  realtime ? "RUNNING GUEST (REAL-TIME)" : "RUNNING GUEST STARTUP");
            if (realtime && wall_seconds > 0 &&
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()
                    >= wall_seconds) {
                status_ = "wall_time_limit";
                break;
            }
        }
        if (!framebuffer.empty()) save_framebuffer(framebuffer);
        if (wav_capture_) wav_capture_->close();
        if (cd_wav_capture_) cd_wav_capture_->close();
        const double seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        const Registers regs = read_regs();
        std::cout << "status=" << status_ << " interrupts=" << interrupts_
                  << " ticks=" << ticks_ << " irq0=" << irq0_
                  << " ip=0x" << std::hex << regs.ip << " ax=0x" << regs.ax
                  << std::dec << "\n";
        if (audio_blocks_)
            std::cout << "audio: modeled_dma_blocks=" << audio_blocks_
                      << " captured_pcm_bytes=" << audio_bytes_
                      << " (SDL3 playback when a sound device is available)\n";
        if (wav_capture_)
            std::cout << "wav_capture: segments=" << wav_capture_->segments()
                      << " pcm_bytes=" << wav_capture_->bytes() << '\n';
        if (cd_audio_)
            std::cout << "cd_audio: tracks=" << cd_audio_->tracks()
                      << " requests=" << cd_audio_->commands()
                      << " pcm_blocks=" << cd_blocks_
                      << " pcm_bytes=" << cd_audio_->played_bytes() << '\n';
        if (cd_wav_capture_)
            std::cout << "cd_wav_capture: segments=" << cd_wav_capture_->segments()
                      << " pcm_bytes=" << cd_wav_capture_->bytes() << '\n';
        if (trace_joystick)
            std::cout << "gameport: axes=" << joystick_count_
                      << " reads=" << gameport_reads_
                      << " writes=" << gameport_writes_ << '\n';
        std::cout << "performance: guest_ips="
                  << (!realtime && completed ? std::to_string(uint64_t(completed /
                        std::max(seconds, 0.000001))) : "n/a")
                  << " wall_seconds=" << std::fixed << std::setprecision(2) << seconds
                  << std::defaultfloat << " emu_slices=" << slices
                  << " pit_divisor=" << pit_divisor_;
        if (window.enabled())
            std::cout << " sdl_polls=" << window.input_polls()
                      << " rendered_frames=" << window.frames_presented();
        std::cout << '\n';
        if (profile_video_frames) {
            std::cout << "video_frames: image_presentations=" << window.image_frames()
                      << " changed_images=" << window.changed_image_frames()
                      << " changed_hz=" << std::fixed << std::setprecision(2)
                      << window.image_change_hz() << std::defaultfloat
                      << " (sampled visible changes; not simulation FPS)\n";
        }
        if (performance_.clock_loop_benchmark)
            std::cout << "benchmark: clock_loop_iterations=" << regs.dx << '\n';
        if (performance_.profile) {
            const auto milliseconds = [](std::chrono::steady_clock::duration time) {
                return std::chrono::duration<double, std::milli>(time).count();
            };
            std::cout << "profile: blocks=" << realtime_blocks_
                      << " block_clock_checks=" << perf_stats_.block_clock_checks
                      << " port_in=" << perf_stats_.port_ins
                      << " port_out=" << perf_stats_.port_outs
                      << " vga_reads=" << perf_stats_.vga_reads
                      << " vga_clock_checks=" << perf_stats_.vga_clock_checks << '\n';
            std::cout << "profile_ms: guest=" << std::fixed << std::setprecision(2)
                      << milliseconds(perf_stats_.guest_time)
                      << " audio=" << milliseconds(perf_stats_.audio_time)
                      << " cd=" << milliseconds(perf_stats_.cd_time)
                      << " display=" << milliseconds(perf_stats_.display_time)
                      << std::defaultfloat << '\n';
            std::cout << "profile_audio_bytes: read=" << perf_stats_.audio_reads
                      << " write=" << perf_stats_.audio_writes << '\n';
        }
        if (performance_.hot_blocks) {
            std::vector<std::pair<uint64_t, uint64_t>> hottest(hot_block_samples_.begin(),
                                                               hot_block_samples_.end());
            std::sort(hottest.begin(), hottest.end(), [](const auto& lhs, const auto& rhs) {
                if (lhs.second != rhs.second) return lhs.second > rhs.second;
                return lhs.first < rhs.first;
            });
            std::cout << "hot_blocks: samples=" << hot_block_total_
                      << " unique=" << hottest.size() << '\n';
            for (size_t i = 0; i < std::min<size_t>(hottest.size(), 16); ++i)
                std::cout << "hot_block: ip=0x" << std::hex << hottest[i].first << std::dec
                          << " samples=" << hottest[i].second << '\n';
        }
        if (performance_.profile_copy) {
            std::cout << "copy_calls: count=" << copy_calls_
                      << " bytes=" << copy_bytes_ << " max=" << copy_max_ << '\n';
            for (size_t i = 0; i < copy_size_buckets_.size(); ++i)
                std::cout << "copy_bucket: range=" << i << " calls=" << copy_size_buckets_[i]
                          << '\n';
        }
        if (performance_.profile_copy_callers) {
            std::vector<std::pair<uint32_t, CopyCallerStats>> callers(copy_callers_.begin(),
                                                                       copy_callers_.end());
            std::sort(callers.begin(), callers.end(), [](const auto& lhs, const auto& rhs) {
                if (lhs.second.bytes != rhs.second.bytes)
                    return lhs.second.bytes > rhs.second.bytes;
                if (lhs.second.calls != rhs.second.calls)
                    return lhs.second.calls > rhs.second.calls;
                return lhs.first < rhs.first;
            });
            constexpr size_t max_reported = 64;
            std::cout << "copy_callers: unique=" << callers.size()
                      << " shown=" << std::min(callers.size(), max_reported) << '\n';
            for (size_t i = 0; i < std::min(callers.size(), max_reported); ++i) {
                const auto& [return_ip, stats] = callers[i];
                std::cout << "copy_caller: ret=0x" << std::hex << return_ip << std::dec
                          << " calls=" << stats.calls << " bytes=" << stats.bytes
                          << " max=" << stats.max_bytes
                          << " ge1024=" << stats.ge1024
                          << " ge4096=" << stats.ge4096
                          << " vga_calls=" << stats.vga_calls
                          << " vga_bytes=" << stats.vga_bytes
                          << " accelerated=" << stats.accelerated_calls
                          << " accelerated_bytes=" << stats.accelerated_bytes << '\n';
            }
        }
        if (performance_.fast_copy)
            std::cout << "fast_copy: calls=" << fast_copy_calls_
                      << " bytes=" << fast_copy_bytes_
                      << " fallback_calls=" << fast_copy_skipped_ << '\n';
        if (performance_.fast_copy_vga)
            std::cout << "fast_copy_vga: calls=" << fast_copy_vga_calls_
                      << " bytes=" << fast_copy_vga_bytes_ << '\n';
        if (!detail_.empty()) std::cout << "detail=" << detail_ << '\n';
        std::cout.flush();
        if (window.enabled()) {
            if (!running) return 0;
            while (refresh(window, completed, "GUEST STOPPED: " + status_ + " / CLOSE WINDOW TO EXIT")) {
#ifdef EF_UNICORN_SDL3
                SDL_Delay(25);
#endif
            }
        }
        return (status_ == "instruction_limit" || status_ == "wall_time_limit" ||
                status_ == "guest_exit") ? 0 : 20;
    }

private:
    std::string invalid_memory_;
    uc_hook slice_hook_{};
    uc_hook archive_lookup_hook_{};
    std::set<std::pair<std::string, std::string>> archive_lookups_seen_;
    std::chrono::steady_clock::time_point slice_deadline_{};
    PerformanceOptions performance_{};
    PerformanceStats perf_stats_{};
    uint64_t realtime_blocks_{};
    uint64_t hot_block_total_{};
    std::unordered_map<uint64_t, uint64_t> hot_block_samples_;
    uint64_t copy_calls_{}, copy_bytes_{};
    uint32_t copy_max_{};
    std::array<uint64_t, 8> copy_size_buckets_{};
    std::unordered_map<uint32_t, CopyCallerStats> copy_callers_;
    std::optional<PendingCopy> pending_copy_;
    uint64_t fast_copy_calls_{}, fast_copy_bytes_{}, fast_copy_skipped_{};
    uint64_t fast_copy_vga_calls_{}, fast_copy_vga_bytes_{};
    std::vector<uint8_t> fast_copy_buffer_;
    unsigned vga_reads_until_check_{1};
    uint32_t vga_status_{};
    StartupWindow* active_window_{};
    std::unique_ptr<ef2000::WavCapture> wav_capture_;
    std::unique_ptr<ef2000::WavCapture> cd_wav_capture_;
    uc_engine* cpu_{};
    size_t image_size_;
    uint64_t interrupts_{};
    uint64_t audio_blocks_{}, audio_bytes_{};
    uint64_t cd_blocks_{};
    uint64_t gameport_reads_{}, gameport_writes_{};
    uint64_t ticks_{}, irq0_{}, irq1_{};
    uint32_t pit_divisor_ = 65536, pit_low_{}, bios_accumulator_{};
    bool pit_high_next_{}, irq_in_service_{}, eoi_seen_{};
    bool keyboard_ack_{};
    uint8_t shift_flags_{};
    std::set<int> keys_pressed_;
    std::deque<uint8_t> scan_queue_;
    std::deque<uint16_t> bios_keys_;
    uint32_t vga_dac_cursor_{};
    uint32_t irq_return_sp_{}, irq_resume_ip_{};
    uc_hook eoi_hook_{};
    std::string status_ = "instruction_limit";
    std::string detail_;
    bool trace_interrupts_{};
    bool reported_missing_bios_disk_{};
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
    uint16_t video_bank_{};
    uint32_t video_pitch_ = 640;
    uint16_t display_x_{}, display_y_{};
    std::array<uint8_t, 0x100000> vram_{};
    int mouse_x_ = 320, mouse_y_ = 100;
    int mouse_min_x_ = 0, mouse_max_x_ = 639, mouse_min_y_ = 0, mouse_max_y_ = 199;
    int mouse_motion_x_{}, mouse_motion_y_{};
    unsigned mouse_buttons_{};
    std::vector<MouseEvent> mouse_events_;
    size_t mouse_event_index_{};
    std::vector<TimedKeyEvent> key_events_;
    size_t key_event_index_{};
    std::vector<JoystickEvent> joystick_events_;
    size_t joystick_event_index_{};
    unsigned joystick_count_{}, joystick_buttons_{};
    ControlSelection controls_;
    std::array<int, 4> joystick_axes_{};
    std::array<uint16_t, 4> joystick_remaining_{};
    ef2000::DosDrive dos_drive_;
    bool digital_sound_enabled_{};
    ef2000::RealModeCpu real_cpu_;
    std::unique_ptr<ef2000::CdAudio> cd_audio_;

    struct VideoFrame { uint16_t width{}, height{}; std::vector<uint8_t> rgba; };
    static std::pair<uint16_t, uint16_t> vbe_dimensions(uint16_t mode) {
        switch (mode) {
            case 0x100: return {640, 400};
            case 0x101: return {640, 480};
            case 0x103: return {800, 600};
            case 0x105: return {1024, 768};
            default: return {0, 0};
        }
    }
    void capture_bank() {
        if (!vbe_dimensions(video_mode_).first) return;
        check(uc_mem_read(cpu_, DOS_BASE + 0xa0000, vram_.data() + video_bank_ * 65536,
                          65536), "capture banked VGA memory");
    }
    VideoFrame video_frame() {
        VideoFrame frame;
        if (video_mode_ == 0x13) {
            frame.width = 320;
            frame.height = 200;
        } else {
            const auto [width, height] = vbe_dimensions(video_mode_);
            if (!width) return frame;
            frame.width = width;
            frame.height = height;
        }
        std::array<uint8_t, 768> palette{};
        check(uc_mem_read(cpu_, DOS_BASE + 0xf1000, palette.data(), palette.size()),
              "read VGA DAC palette");
        std::vector<uint8_t> indices(frame.width * frame.height);
        if (video_mode_ == 0x13) {
            check(uc_mem_read(cpu_, DOS_BASE + 0xa0000, indices.data(), indices.size()),
                  "read VGA framebuffer");
        } else {
            capture_bank();
            const uint32_t start = display_y_ * video_pitch_ + display_x_;
            if (uint64_t(start) + uint64_t(frame.height - 1) * video_pitch_ +
                    frame.width > vram_.size())
                throw std::runtime_error("VBE display start exceeds banked VRAM");
            for (unsigned row = 0; row < frame.height; ++row)
                std::copy_n(vram_.data() + start + row * video_pitch_, frame.width,
                            indices.data() + row * frame.width);
        }
        // Fill the texture buffer directly: push_back() four times per pixel
        // performs needless vector-capacity checks in the 30 Hz video path.
        frame.rgba.resize(indices.size() * 4);
        uint8_t* output = frame.rgba.data();
        for (uint8_t index : indices) {
            const size_t pos = size_t(index) * 3;
            *output++ = uint8_t(palette[pos] << 2);
            *output++ = uint8_t(palette[pos + 1] << 2);
            *output++ = uint8_t(palette[pos + 2] << 2);
            *output++ = 255;
        }
        return frame;
    }
    bool refresh(StartupWindow& window, uint64_t completed, const std::string& status) {
        if (!window.enabled()) return true;
        if (!window.should_poll()) return true;
        const auto started = performance_.profile ? std::chrono::steady_clock::now() :
                             std::chrono::steady_clock::time_point{};
        // Poll input often, but only copy the banked VGA framebuffer and
        // build RGBA pixels when the next display refresh is actually due.
        VideoFrame frame;
        if (video_mode_ == 0x13) {
            frame.width = 320;
            frame.height = 200;
        } else {
            const auto [width, height] = vbe_dimensions(video_mode_);
            frame.width = width;
            frame.height = height;
        }
        if (window.should_present()) frame = video_frame();
        const bool running = window.refresh(completed, interrupts_, status,
                                            frame.rgba.empty() ? nullptr : &frame.rgba,
                                            frame.width, frame.height);
        for (const MouseEvent& event : window.take_mouse_events())
            set_mouse(event.x, event.y, event.buttons);
        const auto joystick = window.joystick_state();
        // The SDL gamepad profile also synthesizes keyboard transitions.
        // Sample it before draining the same frame's keyboard queue.
        for (const KeyEvent& event : window.take_key_events())
            key_event(event.scan, event.key, event.down, event.repeat);
        // Also clear a disconnected controller; otherwise the old axes and
        // buttons remain latched forever. Explicit scripts take precedence
        // when no physical device is attached.
        if (joystick.count) set_joystick(joystick.count, joystick.axes, joystick.buttons);
        else if (joystick_events_.empty())
            set_joystick(controls_ == ControlSelection::Joystick ? 2u : 0u, {}, 0);
        sync_scan_mailbox();
        if (performance_.profile)
            perf_stats_.display_time += std::chrono::steady_clock::now() - started;
        return running;
    }
    void save_framebuffer(const std::string& path) {
        const auto frame = video_frame();
        if (frame.rgba.empty()) return;
        if (trace_interrupts_) {
            const auto colors = std::count_if(frame.rgba.begin(), frame.rgba.end(),
                [](uint8_t channel) { return channel != 0 && channel != 255; });
            const auto filled = std::count_if(vram_.begin(), vram_.end(),
                [](uint8_t index) { return index != 0; });
            std::cerr << "cpp-video: mode=" << std::hex << video_mode_ << std::dec
                      << " bank=" << video_bank_ << " pitch=" << video_pitch_
                      << " nonblack_channels=" << colors << " nonzero_vram=" << filled << '\n';
        }
        std::ofstream output(path, std::ios::binary);
        if (!output) throw std::runtime_error("cannot create framebuffer file: " + path);
        output << "P6\n" << frame.width << " " << frame.height << "\n255\n";
        for (size_t i = 0; i < frame.rgba.size(); i += 4)
            output.write(reinterpret_cast<const char*>(frame.rgba.data() + i), 3);
        if (!output) throw std::runtime_error("failed to write PPM framebuffer");
        std::cout << "framebuffer=" << path << " (" << frame.width << "x" << frame.height << ")\n";
    }

    std::string dos_name(uint32_t address) {
        if (address <= 0x260 || address >= IMAGE_SIZE - 260) throw unsigned(3);
        std::array<char, 260> text{};
        check(uc_mem_read(cpu_, address, text.data(), text.size()), "read DOS filename");
        const auto nul = std::find(text.begin(), text.end(), '\0');
        if (nul == text.end()) throw unsigned(3);
        return {text.begin(), nul};
    }
    void dos_files(uint16_t ax, const Registers& before, Registers& after) {
        const uint8_t ah = ax >> 8;
        const uint16_t handle = before.bx & 0xffff;
        try {
            if (ah == 0x3d) {
                const std::string name = dos_name(before.dx);
                if (trace_interrupts_) std::cerr << "cpp-file: open " << name << '\n';
                const bool joystick_controls = controls_ == ControlSelection::Joystick ||
                    (controls_ == ControlSelection::Auto && joystick_count_ != 0);
                after.ax = dos_drive_.open(name, ax & 3, joystick_controls,
                                           digital_sound_enabled_, bool(cd_audio_));
                after.dx = 0;
            } else if (ah == 0x3c) {
                after.ax = dos_drive_.create(dos_name(before.dx));
            } else if (ah == 0x3e) {
                dos_drive_.close(handle);
                after.ax = 0x3e01;
            } else if (ah == 0x3f) {
                const auto address = before.dx, count = before.cx;
                if (address <= 0x260 || address >= IMAGE_SIZE || count > IMAGE_SIZE - address)
                    throw unsigned(5);
                const auto bytes = dos_drive_.read(handle, count);
                if (!bytes.empty()) write(address, bytes.data(), bytes.size());
                after.cx = after.dx = 0;
                after.ax = bytes.size();
            } else if (ah == 0x40) {
                const auto address = before.dx, count = before.cx;
                if (address <= 0x260 || address >= IMAGE_SIZE || count > IMAGE_SIZE - address)
                    throw unsigned(5);
                std::vector<uint8_t> bytes(count);
                if (count) check(uc_mem_read(cpu_, address, bytes.data(), count), "read DOS write buffer");
                after.ax = dos_drive_.write(handle, bytes);
            } else if (ah == 0x42) {
                const int32_t offset = int32_t(((before.cx & 0xffff) << 16) | (before.dx & 0xffff));
                const uint64_t position = dos_drive_.seek(handle, offset, ax & 255);
                after.ax = position & 0xffff;
                after.dx = (after.dx & 0xffff0000) | ((position >> 16) & 0xffff);
            } else if (ah == 0x68) {
                if (handle > 2) dos_drive_.flush(handle);
            } else if (ax == 0x4400) {
                if (handle > 2 && !dos_drive_.is_open(handle)) throw unsigned(6);
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
        mouse_motion_x_ = mouse_motion_y_ = 0;
    }
    void set_mouse(int x, int y, unsigned buttons) {
        const int dx = x - mouse_x_, dy = y - mouse_y_;
        mouse_motion_x_ += dx;
        mouse_motion_y_ += dy;
        mouse_x_ = std::clamp(mouse_x_ + dx, mouse_min_x_, mouse_max_x_);
        mouse_y_ = std::clamp(mouse_y_ + dy, mouse_min_y_, mouse_max_y_);
        mouse_buttons_ = buttons & 7;
    }
    void scripted_mouse() {
        while (mouse_event_index_ < mouse_events_.size() &&
               mouse_events_[mouse_event_index_].tick <= ticks_) {
            const MouseEvent& event = mouse_events_[mouse_event_index_++];
            set_mouse(event.x, event.y, event.buttons);
            if (trace_interrupts_)
                std::cerr << "cpp-mouse: tick=" << ticks_ << " x=" << mouse_x_
                          << " y=" << mouse_y_ << " buttons=" << mouse_buttons_ << '\n';
        }
    }
    void scripted_keyboard() {
        while (key_event_index_ < key_events_.size() &&
               key_events_[key_event_index_].tick <= ticks_) {
            const auto& event = key_events_[key_event_index_++];
            key_event(event.key.scan, event.key.key, event.key.down, false);
        }
        sync_scan_mailbox();
    }
    void set_joystick(unsigned count, const std::array<int, 4>& axes, unsigned buttons) {
        joystick_count_ = std::min(count, 4u);
        joystick_axes_ = axes;
        joystick_buttons_ = buttons & 15;
        sync_joystick_mailbox();
    }
    void scripted_joystick() {
        while (joystick_event_index_ < joystick_events_.size() &&
               joystick_events_[joystick_event_index_].tick <= ticks_) {
            const JoystickEvent& event = joystick_events_[joystick_event_index_++];
            set_joystick(event.count, event.axes, event.buttons);
            if (trace_interrupts_)
                std::cerr << "cpp-joystick: tick=" << ticks_ << " x=" << event.axes[0]
                          << " y=" << event.axes[1] << " count=" << event.count
                          << " buttons=" << event.buttons << '\n';
        }
    }
    void sync_joystick_mailbox() {
        write_value<uint8_t>(DOS_BASE + 0xf1600, (1u << joystick_count_) - 1);
        write_value<uint8_t>(DOS_BASE + 0xf1601, joystick_buttons_);
        for (unsigned i = 0; i < 4; ++i) {
            const uint32_t normalized = uint32_t(std::clamp(joystick_axes_[i], -32768, 32767) + 32768);
            const uint16_t discharge = i < joystick_count_ ? 1 + normalized * 126 / 65535 : 0;
            write_value<uint16_t>(DOS_BASE + 0xf1602 + i * 2, discharge);
        }
    }
    void charge_joystick() {
        ++gameport_writes_;
        for (unsigned i = 0; i < 4; ++i) {
            const uint32_t normalized = uint32_t(std::clamp(joystick_axes_[i], -32768, 32767) + 32768);
            joystick_remaining_[i] = i < joystick_count_ ? 1 + normalized * 126 / 65535 : 0;
        }
    }
    uint32_t discharge_joystick() {
        ++gameport_reads_;
        const unsigned mask = (1u << joystick_count_) - 1;
        uint32_t value = ((~joystick_buttons_ & 15) << 4) | (~mask & 15);
        for (unsigned i = 0; i < 4; ++i) {
            if (i < joystick_count_ && joystick_remaining_[i]) {
                value |= 1u << i;
                --joystick_remaining_[i];
            }
        }
        return value;
    }
    static int pc_scan(int scan, bool& extended) {
        static constexpr uint8_t letters[] = {0x1e, 0x30, 0x2e, 0x20, 0x12,
            0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31,
            0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11,
            0x2d, 0x15, 0x2c};
        static constexpr uint8_t signs[] = {0x1c, 1, 0x0e, 0x0f, 0x39,
            0x0c, 0x0d, 0x1a, 0x1b, 0x2b, 0x2b, 0x27,
            0x28, 0x29, 0x33, 0x34, 0x35, 0x3a};
        static constexpr uint8_t extended_keys[] = {0x52, 0x47, 0x49, 0x53,
            0x4f, 0x51, 0x4d, 0x4b, 0x50, 0x48};
        if (scan >= 4 && scan <= 29) return letters[scan - 4];
        if (scan >= 30 && scan <= 39) return scan - 28;
        if (scan >= 40 && scan <= 57) return signs[scan - 40];
        if (scan >= 58 && scan <= 67) return 0x3b + scan - 58;
        if (scan >= 73 && scan <= 82) { extended = true; return extended_keys[scan - 73]; }
        if (scan >= 89 && scan <= 99) {
            static constexpr uint8_t keypad[] = {0x4f, 0x50, 0x51, 0x4b,
                0x4c, 0x4d, 0x47, 0x48, 0x49, 0x52, 0x53};
            return keypad[scan - 89];
        }
        switch (scan) {
            case 68: return 0x57; case 69: return 0x58;
            case 71: return 0x46; case 83: return 0x45;
            case 85: return 0x37; case 86: return 0x4a; case 87: return 0x4e;
            case 100: return 0x56;
            case 224: return 0x1d; case 225: return 0x2a;
            case 226: return 0x38; case 229: return 0x36;
            case 84: extended = true; return 0x35;
            case 88: extended = true; return 0x1c;
            case 228: extended = true; return 0x1d;
            case 230: extended = true; return 0x38;
            default: return -1;
        }
    }
    void key_event(int scan, int key, bool down, bool repeat) {
        if (scan == 70) { // Print Screen's synthetic shift prefix.
            const uint8_t bytes[] = {0xe0, uint8_t(down ? 0x2a : 0xb7),
                                     0xe0, uint8_t(down ? 0x37 : 0xaa)};
            for (uint8_t byte : bytes) scan_queue_.push_back(byte);
            return;
        }
        if (scan == 72) { // Pause has no release sequence.
            if (down) for (uint8_t byte : {0xe1, 0x1d, 0x45, 0xe1, 0x9d, 0xc5})
                scan_queue_.push_back(byte);
            return;
        }
        bool extended = false;
        const int code = pc_scan(scan, extended);
        if (code < 0) {
            if (!down) keys_pressed_.erase(scan);
            return;
        }
        if (down) keys_pressed_.insert(scan);
        else if (!keys_pressed_.erase(scan)) return;
        if (extended) scan_queue_.push_back(0xe0);
        scan_queue_.push_back(uint8_t(code | (down ? 0 : 0x80)));
        if (down && !repeat) {
            if (scan == 57) shift_flags_ ^= 0x40;
            if (scan == 83) shift_flags_ ^= 0x20;
            if (scan == 71) shift_flags_ ^= 0x10;
        }
        const auto pressed = [this](int code) { return keys_pressed_.count(code) != 0; };
        shift_flags_ = (shift_flags_ & 0xf0) |
            (pressed(229) ? 1 : 0) | (pressed(225) ? 2 : 0) |
            ((pressed(224) || pressed(228)) ? 4 : 0) |
            ((pressed(226) || pressed(230)) ? 8 : 0);
        if (down && !extended && scan != 224 && scan != 225 && scan != 226 &&
            scan != 229 && scan != 57 && scan != 71 && scan != 83 && bios_keys_.size() < 64) {
            int ascii = key >= 0 && key < 256 ? key : 0;
            if (ascii >= 'a' && ascii <= 'z') {
                if (shift_flags_ & 4) ascii -= 96;
                else if (bool(shift_flags_ & 3) != bool(shift_flags_ & 0x40)) ascii -= 32;
            }
            if (shift_flags_ & 8) ascii = 0;
            bios_keys_.push_back(uint16_t((code << 8) | ascii));
        }
    }
    bool keyboard_pending() {
        if (read_value<uint32_t>(DOS_BASE + 0xf1400) != 0x31504e49) return false;
        const uint16_t head = read_value<uint16_t>(DOS_BASE + 0xf1404);
        const uint16_t tail = read_value<uint16_t>(DOS_BASE + 0xf1406);
        return head < 256 && tail < 256 && head != tail;
    }
    void sync_scan_mailbox() {
        if (scan_queue_.empty()) return;
        uint16_t head = 0, tail = 0;
        if (read_value<uint32_t>(DOS_BASE + 0xf1400) == 0x31504e49) {
            head = read_value<uint16_t>(DOS_BASE + 0xf1404) & 255;
            tail = read_value<uint16_t>(DOS_BASE + 0xf1406) & 255;
        }
        while (!scan_queue_.empty() && ((tail + 1) & 255) != head) {
            write_value<uint8_t>(DOS_BASE + 0xf1408 + tail, scan_queue_.front());
            scan_queue_.pop_front();
            tail = (tail + 1) & 255;
        }
        write_value<uint32_t>(DOS_BASE + 0xf1400, 0x31504e49);
        write_value<uint16_t>(DOS_BASE + 0xf1404, head);
        write_value<uint16_t>(DOS_BASE + 0xf1406, tail);
    }
    bool vbe(Registers& values) {
        const uint16_t command = values.ax & 0xff;
        if (trace_interrupts_) {
            std::cerr << "cpp-vbe: function=" << command << " bx=" << std::hex << values.bx
                      << " cx=" << values.cx << " dx=" << values.dx << " es=" << values.es
                      << " di=" << values.di << std::dec << '\n';
        }
        values.ax = low16(values.ax, 0x004f);
        const uint32_t address = ((values.es & 0xffff) << 4) + (values.di & 0xffff);
        if (command == 0) {
            if (address > 0x100000 - 512) return fail("VBE info buffer outside DOS RAM"), false;
            std::array<uint8_t, 512> info{};
            std::memcpy(info.data(), "VESA", 4);
            auto write16 = [](uint8_t* target, uint16_t value) {
                target[0] = value & 255; target[1] = value >> 8;
            };
            auto write32 = [&](uint8_t* target, uint32_t value) {
                write16(target, value & 0xffff); write16(target + 2, value >> 16);
            };
            write16(info.data() + 4, 0x0102);
            write32(info.data() + 6, 0xf0000200);
            write32(info.data() + 14, 0xf0000300);
            write16(info.data() + 18, 16);
            write(DOS_BASE + address, info.data(), info.size());
            static constexpr char oem[] = "EF2000 virtual VBE BIOS";
            write(DOS_BASE + 0xf0200, oem, sizeof(oem));
            uint16_t modes[] = {0x100, 0x101, 0x103, 0x105, 0xffff};
            write(DOS_BASE + 0xf0300, modes, sizeof(modes));
        } else if (command == 1) {
            const auto [width, height] = vbe_dimensions(values.cx & 0x3fff);
            if (!width) { values.ax = low16(values.ax, 0x014f); return true; }
            if (address > 0x100000 - 256) return fail("VBE mode-info buffer outside DOS RAM"), false;
            std::array<uint8_t, 256> info{};
            auto set16 = [&](size_t pos, uint16_t value) {
                info[pos] = value & 255; info[pos + 1] = value >> 8;
            };
            set16(0, 0x001b);
            info[2] = 7;
            set16(4, 64);
            set16(6, 64);
            set16(8, 0xa000);
            set16(16, width);
            set16(18, width);
            set16(20, height);
            const uint8_t attributes[] = {8, 16, 1, 8, 1, 4, 64,
                                          uint8_t(0x100000 / (width * height) - 1)};
            std::copy(std::begin(attributes), std::end(attributes), info.begin() + 22);
            write(DOS_BASE + address, info.data(), info.size());
        } else if (command == 2) {
            const uint16_t mode = values.bx & 0x3fff;
            const auto [width, height] = vbe_dimensions(mode);
            if (!width || (values.bx & 0x4000)) {
                values.ax = low16(values.ax, 0x014f);
            } else {
                video_mode_ = mode;
                video_bank_ = 0;
                video_pitch_ = width;
                display_x_ = display_y_ = 0;
                reset_mouse(width, height);
                if (!(values.bx & 0x8000)) vram_.fill(0);
                write(DOS_BASE + 0xa0000, vram_.data(), 65536);
            }
        } else if (command == 3) {
            values.bx = low16(values.bx, video_mode_);
        } else if (command == 5 && (values.bx & 255) == 0) {
            const uint16_t operation = (values.bx >> 8) & 255;
            if (operation == 1) values.dx = low16(values.dx, video_bank_);
            else if (operation == 0 && (values.dx & 0xffff) < 16) {
                capture_bank();
                video_bank_ = values.dx & 0xffff;
                write(DOS_BASE + 0xa0000, vram_.data() + video_bank_ * 65536, 65536);
            } else values.ax = low16(values.ax, 0x014f);
        } else if (command == 6 && vbe_dimensions(video_mode_).first &&
                   (values.bx & 255) <= 1) {
            const auto [width, height] = vbe_dimensions(video_mode_);
            const uint32_t pitch = (values.bx & 255) == 0 ?
                ((values.cx & 0xffff) + 7) & ~7u : video_pitch_;
            if (pitch < width || pitch * height > vram_.size())
                values.ax = low16(values.ax, 0x014f);
            else {
                video_pitch_ = pitch;
                values.bx = low16(values.bx, pitch);
                values.cx = low16(values.cx, pitch);
                values.dx = low16(values.dx, vram_.size() / pitch);
            }
        } else if (command == 7 && vbe_dimensions(video_mode_).first) {
            const auto [width, height] = vbe_dimensions(video_mode_);
            const uint16_t x = values.cx & 0xffff, y = values.dx & 0xffff;
            const uint16_t operation = values.bx & 0xffff;
            if (operation == 1) {
                values.cx = low16(values.cx, display_x_);
                values.dx = low16(values.dx, display_y_);
            } else if ((operation == 0 || operation == 0x80) &&
                       x + width <= video_pitch_ &&
                       (uint64_t(y) + height) * video_pitch_ <= vram_.size()) {
                display_x_ = x; display_y_ = y;
            } else values.ax = low16(values.ax, 0x014f);
        } else values.ax = low16(values.ax, 0x014f);
        return true;
    }
    bool bios(uint16_t vector, Registers& values) {
        const uint16_t ax = values.ax & 0xffff;
        if (vector == 0x13) {
            // There is no emulated sector device behind the DOS file drive.
            // BIOS callers must observe a standard failure, not a fabricated
            // successful read/geometry response or access to host disks.
            if (!reported_missing_bios_disk_) {
                std::cerr << "bios-disk: INT 13h AH=0x" << std::hex << (ax >> 8)
                          << " DL=0x" << (values.dx & 0xff) << std::dec
                          << " (no BIOS sector device; returning CF=1 AH=01)\n";
                reported_missing_bios_disk_ = true;
            }
            const auto result = ef2000::absent_bios_disk(values.ax, values.flags);
            values.ax = result.ax;
            values.flags = result.flags;
        } else if (vector == 0x33) {
            if (ax == 0) {
                values.ax = low16(values.ax, 0xffff);
                values.bx = low16(values.bx, 2);
                const auto [vbe_width, vbe_height] = vbe_dimensions(video_mode_);
                reset_mouse(vbe_width ? vbe_width : (video_mode_ == 0x13 ? 320 : 640),
                            vbe_height ? vbe_height : 200);
            } else if (ax == 3) {
                values.bx = low16(values.bx, mouse_buttons_);
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
                values.cx = low16(values.cx, mouse_motion_x_);
                values.dx = low16(values.dx, mouse_motion_y_);
                mouse_motion_x_ = mouse_motion_y_ = 0;
            } else if (ax != 1 && ax != 2 && ax < 0x100) {
                fail("unsupported BIOS mouse function " + std::to_string(ax));
                return false;
            }
        } else if (vector == 0x10) {
            const uint8_t ah = ax >> 8;
            if (ah == 0x4f) {
                if (!vbe(values)) return false;
            } else if (ah == 0xf) {
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
            if (ah == 1 || ah == 0x11) {
                if (bios_keys_.empty()) values.flags |= 0x40;
                else {
                    values.ax = low16(values.ax, bios_keys_.front());
                    values.flags &= ~0x40u;
                }
            } else if (ah == 2) values.ax = (values.ax & 0xffffff00) | shift_flags_;
            else if ((ah == 0 || ah == 0x10) && !bios_keys_.empty()) {
                values.ax = low16(values.ax, bios_keys_.front());
                bios_keys_.pop_front();
            }
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
        values.ds = read_value<uint16_t>(frame + 2);
        values.es = read_value<uint16_t>(frame + 4);
        values.ax = read_value<uint32_t>(frame + 10);
        values.dx = read_value<uint32_t>(frame + 14);
        const bool builtin = vector == 0x10 || vector == 0x16 ||
                             vector == 0x33 || vector == 0x2f;
        if (cd_audio_ && vector == 0x2f && (values.ax & 0xff00) == 0x1500) {
            std::vector<uint8_t> conventional(0x100000);
            check(uc_mem_read(cpu_, DOS_BASE, conventional.data(), conventional.size()),
                  "read real-mode DOS RAM for MSCDEX");
            ef2000::RealRegisters real{
                values.ax, values.bx, values.cx, values.dx, values.si, values.di,
                uint16_t(values.ds), uint16_t(values.es),
                read_value<uint16_t>(frame + 6), read_value<uint16_t>(frame + 8),
                values.flags};
            try {
                const unsigned prior_generation = cd_audio_->generation();
                if (trace_interrupts_)
                    std::cerr << "cpp-cd: ax=" << std::hex << real.ax
                              << " es:bx=" << real.es << ':' << real.bx
                              << " cx=" << real.cx << " dx=" << real.dx
                              << std::dec << '\n';
                const double now = std::chrono::duration<double>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                cd_audio_->interrupt(real, conventional, now);
                write(DOS_BASE, conventional.data(), conventional.size());
                if (cd_audio_->generation() != prior_generation && active_window_)
                    active_window_->clear_cd_audio();
                values.ax = real.ax; values.bx = real.bx; values.cx = real.cx;
                values.dx = real.dx; values.si = real.si; values.di = real.di;
                values.ds = real.ds; values.es = real.es; values.flags = real.flags;
                write_value<uint16_t>(frame + 6, real.fs);
                write_value<uint16_t>(frame + 8, real.gs);
            } catch (const std::exception& error) {
                fail(std::string("MSCDEX CD driver: ") + error.what());
                return false;
            }
        } else if (!builtin && real_vectors_[vector]) {
            std::vector<uint8_t> conventional(0x100000);
            check(uc_mem_read(cpu_, DOS_BASE, conventional.data(), conventional.size()),
                  "read real-mode DOS RAM");
            const ef2000::RealRegisters registers{
                values.ax, values.bx, values.cx, values.dx, values.si, values.di,
                uint16_t(values.ds), uint16_t(values.es),
                read_value<uint16_t>(frame + 6), read_value<uint16_t>(frame + 8),
                values.flags};
            try {
                const auto result = real_cpu_.execute(real_vectors_[vector], real_vectors_,
                                                       conventional, registers);
                record_audio(result.audio_blocks);
                write(DOS_BASE, result.memory.data(), result.memory.size());
                // The original 16-bit driver installs hardware handlers by
                // writing the physical interrupt vector table, bypassing
                // protected-mode X-32 service 2505h. Synchronize only
                // modified entries: untouched 2505h registrations need to
                // survive even when no physical IVT entry was initialized.
                for (size_t i = 0; i < real_vectors_.size(); ++i) {
                    const size_t offset = i * sizeof(uint32_t);
                    if (std::memcmp(conventional.data() + offset,
                                    result.memory.data() + offset, sizeof(uint32_t)))
                        std::memcpy(&real_vectors_[i], result.memory.data() + offset,
                                    sizeof(uint32_t));
                }
                const auto& real = result.registers;
                values.ax = real.ax; values.bx = real.bx; values.cx = real.cx;
                values.dx = real.dx; values.si = real.si; values.di = real.di;
                values.ds = real.ds; values.es = real.es; values.flags = real.flags;
                write_value<uint16_t>(frame + 6, real.fs);
                write_value<uint16_t>(frame + 8, real.gs);
            } catch (const std::exception& error) {
                fail("original 16-bit driver INT " + std::to_string(vector) + ": " + error.what());
                return false;
            }
        } else if (!bios(vector, values)) return false;
        // The 18-byte X-32 request holds the four segment registers, EAX,
        // EDX. Remaining general registers return through protected registers.
        write_value<uint16_t>(frame + 2, values.ds & 0xffff);
        write_value<uint16_t>(frame + 4, values.es & 0xffff);
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
        std::array<void*, std::size(REG_IDS)> addresses{};
        for (size_t i = 0; i < std::size(REG_IDS); ++i)
            addresses[i] = &values[i];
        check(uc_reg_read_batch(cpu_, REG_IDS, addresses.data(), int(addresses.size())),
              "read x86 registers");
        return out;
    }
    void write_regs(const Registers& before, const Registers& after) {
        const uint32_t* old = &before.ax;
        const uint32_t* updated = &after.ax;
        for (size_t i = 0; i < std::size(REG_IDS); ++i)
            if (old[i] != updated[i]) reg(REG_IDS[i], updated[i]);
    }
    void initialize_memory() {
        static constexpr char command[] = "\x04 gui\r";
        static constexpr char environment[] = "DID=.\0quick_flag=1\0\0\x01\0C:\\EF2000\\_EF2000.EXE\0";
        write(PSP_BASE + 0x80, command, sizeof(command) - 1);
        write(ENV_BASE, environment, sizeof(environment) - 1);
        // The original SB16.DIG delay routine reads 0040:0063 and polls
        // base+6 (VGA input-status 3DAh) for vertical retrace transitions.
        write_value<uint16_t>(DOS_BASE + 0x463, 0x3d4);
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
    void record_audio(const std::vector<ef2000::AudioBlock>& blocks) {
        audio_blocks_ += blocks.size();
        for (const auto& block : blocks) {
            audio_bytes_ += block.pcm.size();
            if (wav_capture_) wav_capture_->push(block);
            if (active_window_) active_window_->play_audio(block);
        }
    }
    void service_sound() {
        const double now = std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (!real_cpu_.audio_service_due(now)) return;
        const auto started = performance_.profile ? std::chrono::steady_clock::now() :
                             std::chrono::steady_clock::time_point{};
        try {
            std::vector<uint8_t> memory(0x100000);
            check(uc_mem_read(cpu_, DOS_BASE, memory.data(), memory.size()),
                  "read DOS memory for background audio");
            if (performance_.profile) perf_stats_.audio_reads += memory.size();
            const auto result = real_cpu_.service_audio(now, real_vectors_, memory);
            if (!result.memory.empty()) {
                write(DOS_BASE, result.memory.data(), result.memory.size());
                if (performance_.profile) perf_stats_.audio_writes += result.memory.size();
            }
            record_audio(result.audio_blocks);
        } catch (const std::exception& error) {
            fail(std::string("background SB16 DMA/IRQ: ") + error.what());
        }
        if (performance_.profile)
            perf_stats_.audio_time += std::chrono::steady_clock::now() - started;
    }
    void service_cd() {
        if (!cd_audio_) return;
        const auto started = performance_.profile ? std::chrono::steady_clock::now() :
                             std::chrono::steady_clock::time_point{};
        try {
            const double now = std::chrono::duration<double>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            if (auto block = cd_audio_->pcm(now)) {
                ++cd_blocks_;
                if (cd_wav_capture_) cd_wav_capture_->push(*block);
                if (active_window_) active_window_->play_audio(*block, true);
            }
        } catch (const std::exception& error) {
            fail(std::string("background CD audio: ") + error.what());
        }
        if (performance_.profile)
            perf_stats_.cd_time += std::chrono::steady_clock::now() - started;
    }
    void deliver_tick(uint32_t next_ip) {
        ++ticks_;
        scripted_mouse();
        scripted_keyboard();
        scripted_joystick();
        service_sound();
        if (status_ == "instruction_limit") service_cd();
        if (status_ != "instruction_limit") return;
        bios_accumulator_ += pit_divisor_;
        if (bios_accumulator_ >= 65536) {
            uint32_t previous{};
            check(uc_mem_read(cpu_, DOS_BASE + 0x46c, &previous, sizeof(previous)), "read BIOS ticks");
            previous += bios_accumulator_ / 65536;
            bios_accumulator_ %= 65536;
            write_value<uint32_t>(DOS_BASE + 0x46c, previous);
        }
        const Registers regs = read_regs();
        const int vector = keyboard_pending() && protected_vectors_[9].first != IMAGE_SIZE + 0x100 ? 9 : 8;
        const auto [handler, selector] = protected_vectors_[vector];
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
        if (vector == 9) ++irq1_;
        else ++irq0_;
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
    static void on_archive_lookup(uc_engine*, uint64_t, uint32_t, void* user) {
        static_cast<Machine*>(user)->trace_archive_lookup();
    }
    void trace_archive_lookup() {
        // The original guest routine at 0xeb520 receives the ASCII pathname
        // in EAX and the archive descriptor in EDX (Watcom register ABI).
        uint32_t pathname{}, archive{}, first{}, second{};
        if (uc_reg_read(cpu_, UC_X86_REG_EAX, &pathname) != UC_ERR_OK ||
            uc_reg_read(cpu_, UC_X86_REG_EDX, &archive) != UC_ERR_OK)
            return;
        const bool first_valid = uc_mem_read(cpu_, 0x40d1f0, &first, sizeof(first)) == UC_ERR_OK;
        const bool second_valid = uc_mem_read(cpu_, 0x40d1f4, &second, sizeof(second)) == UC_ERR_OK;
        const std::string source = first_valid && first && archive == first ? "DID1ST.DAT" :
                                   second_valid && second && archive == second ? "DID.DAT" :
                                   "unknown";
        std::string filename;
        bool terminated = false;
        for (uint32_t index = 0; index < 255; ++index) {
            uint8_t value{};
            if (pathname > UINT32_MAX - index ||
                uc_mem_read(cpu_, pathname + index, &value, 1) != UC_ERR_OK)
                return;
            if (!value) { terminated = true; break; }
            if (value == '\\') {
                filename += "\\\\";
            } else if (value == '"') {
                filename += "\\\"";
            } else if (value < 0x20 || value >= 0x7f) {
                char escaped[5]{};
                std::snprintf(escaped, sizeof(escaped), "\\x%02x", value);
                filename += escaped;
            } else {
                filename += char(value);
            }
        }
        if (!terminated) filename += "...";
        if (archive_lookups_seen_.emplace(source, filename).second)
            std::cerr << "archive-lookup: archive=" << source
                      << " path=\"" << filename << "\"\n";
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
                   (before.ax >> 8 & 255) == 0x40 ||
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
            if (ax != 0x3504) {
                write_selector(index);
                if (ax == 0x3503 || ax == 0x3505) {
                    // A guest may already have loaded DS with this selector
                    // before asking X-32 to set its base/limit (Miles does
                    // this for the BIOS 0040:0063 CRT status port). Updating
                    // an LDT entry alone leaves x86's hidden segment cache
                    // pointing at the old address. Refresh active data
                    // selectors to reproduce the service's expected effect.
                    for (const int reg_id : {UC_X86_REG_DS, UC_X86_REG_ES,
                                             UC_X86_REG_FS, UC_X86_REG_GS}) {
                        uint32_t current{};
                        check(uc_reg_read(cpu_, reg_id, &current), "read X-32 segment register");
                        if (current == (before.bx & 0xffff))
                            check(uc_reg_write(cpu_, reg_id, &current),
                                  "refresh X-32 segment cache");
                    }
                }
            }
        } else { fail("unhandled X-32 selector service"); return false; }
        after.flags &= ~1u;
        return true;
    }
    void write_selector(unsigned index) {
        const Selector& selector = selectors_[index];
        std::array<uint8_t, 8> bytes{};
        if (selector.active) {
            const bool large = selector.limit > 0xfffff;
            // X-32's 3503h base is a *guest physical* address. Our protected
            // game image occupies host linear address zero, while the original
            // first MiB (IVT, BIOS data area, DOS drivers) lives at DOS_BASE.
            // Keep the unmodified base for 3504h queries and redirect only
            // the underlying LDT descriptor into the conventional-memory map.
            const uint32_t linear = selector.base < 0x100000 ?
                DOS_BASE + selector.base : selector.base;
            bytes = segment(linear, large ? selector.limit >> 12 : selector.limit,
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
    void copy_entry(uc_engine* cpu) {
        uint32_t stack{};
        uint32_t ds{};
        if (uc_reg_read(cpu, UC_X86_REG_ESP, &stack) != UC_ERR_OK ||
            uint64_t(stack) + 16 > IMAGE_SIZE)
            return;
        std::array<uint32_t, 4> args{};
        if (uc_mem_read(cpu, stack, args.data(), sizeof(args)) != UC_ERR_OK) return;
        const auto [return_ip, bytes, source, destination] = args;
        if (performance_.profile_copy) {
            ++copy_calls_;
            copy_bytes_ += bytes;
            copy_max_ = std::max(copy_max_, bytes);
            const size_t bucket = bytes < 32 ? 0 : bytes < 64 ? 1 : bytes < 128 ? 2 :
                bytes < 256 ? 3 : bytes < 512 ? 4 : bytes < 1024 ? 5 :
                bytes < 4096 ? 6 : 7;
            ++copy_size_buckets_[bucket];
            if (performance_.profile_copy_callers) {
                auto& stats = copy_callers_[return_ip];
                ++stats.calls;
                stats.bytes += bytes;
                stats.max_bytes = std::max(stats.max_bytes, bytes);
                if (bytes >= 1024) ++stats.ge1024;
                if (bytes >= 4096) ++stats.ge4096;
                if (destination >= VGA_COPY_BASE &&
                    uint64_t(destination) + bytes <= VGA_COPY_END) {
                    ++stats.vga_calls;
                    stats.vga_bytes += bytes;
                }
            }
        }
        if (!performance_.fast_copy) return;
        // This exact routine is only present in the SHA-pinned original image.
        // A conservative threshold leaves short or overlapping copies and
        // writes into the original code section to Unicorn's normal executor.
        const uint64_t src_end = uint64_t(source) + bytes;
        const uint64_t dst_end = uint64_t(destination) + bytes;
        const bool guest_data = destination >= image_size_ && dst_end <= IMAGE_SIZE;
        // This region is an ordinary mapped RAM bank. The VBE model captures
        // it via uc_mem_read; there are no MMIO write hooks to preserve.
        // Restrict the experimental extension to the exact 64 KiB VGA window.
        const bool vga_bank = performance_.fast_copy_vga &&
            destination >= VGA_COPY_BASE && dst_end <= VGA_COPY_END;
        if (bytes < 1024 || bytes > 1024 * 1024 || source < 0x1000 ||
            src_end > IMAGE_SIZE || (!guest_data && !vga_bank) ||
            return_ip < ENTRY || return_ip >= image_size_ ||
            (source < dst_end && destination < src_end) ||
            uc_reg_read(cpu, UC_X86_REG_DS, &ds) != UC_ERR_OK || ds != 7) {
            ++fast_copy_skipped_;
            return;
        }
        pending_copy_ = PendingCopy{stack, return_ip, source, destination, bytes};
        if (uc_emu_stop(cpu) != UC_ERR_OK) {
            pending_copy_.reset();
            fail("could not stop at fast-copy entry");
        }
    }
    void finish_fast_copy() {
        const PendingCopy copy = *pending_copy_;
        pending_copy_.reset();
        const uint32_t ip = ip_at_cpu();
        if (ip != COPY_ENTRY)
            throw std::runtime_error("fast-copy entry did not stop at instruction boundary");
        fast_copy_buffer_.resize(copy.bytes);
        check(uc_mem_read(cpu_, copy.source, fast_copy_buffer_.data(), copy.bytes),
              "fast-copy guest source");
        check(uc_mem_write(cpu_, copy.destination, fast_copy_buffer_.data(), copy.bytes),
              "fast-copy guest destination");
        // The original function returns with RET 0Ch and EAX/EDX pointing
        // just past their buffers. EBX, ESI, EDI and EBP are callee-saved.
        reg(UC_X86_REG_EAX, copy.destination + copy.bytes);
        reg(UC_X86_REG_EDX, copy.source + copy.bytes);
        reg(UC_X86_REG_ESP, copy.stack + 16);
        reg(UC_X86_REG_EIP, copy.return_ip);
        ++fast_copy_calls_;
        fast_copy_bytes_ += copy.bytes;
        if (performance_.profile_copy_callers) {
            auto& stats = copy_callers_[copy.return_ip];
            ++stats.accelerated_calls;
            stats.accelerated_bytes += copy.bytes;
        }
        if (copy.destination >= VGA_COPY_BASE &&
            uint64_t(copy.destination) + copy.bytes <= VGA_COPY_END) {
            ++fast_copy_vga_calls_;
            fast_copy_vga_bytes_ += copy.bytes;
        }
    }
    static void on_realtime_block(uc_engine* cpu, uint64_t address, uint32_t, void* user) {
        auto* machine = static_cast<Machine*>(user);
        if ((machine->performance_.profile_copy || machine->performance_.fast_copy) &&
            address == COPY_ENTRY)
            machine->copy_entry(cpu);
        // A clock syscall on *every* translated block is expensive on both
        // Linux and Windows. The callback is still needed for safe block-
        // boundary stops; sample the deadline once per N blocks instead.
        ++machine->realtime_blocks_;
        if (machine->performance_.hot_blocks && (machine->realtime_blocks_ & 4095) == 0) {
            ++machine->hot_block_samples_[address];
            ++machine->hot_block_total_;
        }
        if ((machine->realtime_blocks_ & (machine->performance_.block_clock_check - 1)) == 0) {
            if (machine->performance_.profile) ++machine->perf_stats_.block_clock_checks;
            if (std::chrono::steady_clock::now() >= machine->slice_deadline_)
                uc_emu_stop(cpu);
        }
    }
    static bool on_invalid_memory(uc_engine*, uc_mem_type type, uint64_t address,
                                  int size, int64_t, void* user) {
        auto* machine = static_cast<Machine*>(user);
        std::ostringstream message;
        message << "type=" << type << " address=0x" << std::hex << address
                << std::dec << " size=" << size;
        machine->invalid_memory_ = message.str();
        return false;
    }
    uint32_t port_in(uint32_t port) {
        if (performance_.profile) ++perf_stats_.port_ins;
        if (port == 0x3da) {
            if (performance_.profile) ++perf_stats_.vga_reads;
            // The original VGA/SB16 code can poll vertical retrace in tight
            // loops. Avoid the monotonic-clock query and modulo on most INs.
            // Always sample the first read in each emulation slice.
            if (--vga_reads_until_check_ == 0) {
                if (performance_.profile) ++perf_stats_.vga_clock_checks;
                vga_reads_until_check_ = performance_.vga_clock_check;
                const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                vga_status_ = ns % 16666667 >= 15000000 ? 8 : 0;
            }
            return vga_status_;
        }
        if (port == 0x64) return keyboard_ack_ || keyboard_pending() ? 1 : 0;
        if (port == 0x60) {
            if (keyboard_ack_) { keyboard_ack_ = false; return 0xfa; }
            if (keyboard_pending()) {
                const uint16_t head = read_value<uint16_t>(DOS_BASE + 0xf1404);
                const uint8_t byte = read_value<uint8_t>(DOS_BASE + 0xf1408 + head);
                write_value<uint16_t>(DOS_BASE + 0xf1404, (head + 1) & 255);
                return byte;
            }
            return 0;
        }
        if (port == 0x201) return discharge_joystick();
        if (port == 0x61) return 0;
        const Registers regs = read_regs();
        std::array<uint8_t, 12> instruction{};
        const uc_err fetched = uc_mem_read(cpu_, regs.ip, instruction.data(), instruction.size());
        char message[256];
        std::snprintf(message, sizeof(message),
                      "unsupported port IN %04x at ip=%08x sp=%08x dx=%08x ax=%08x bytes=%02x%02x%02x%02x (%s)",
                      port, regs.ip, regs.sp, regs.dx, regs.ax,
                      instruction[0], instruction[1], instruction[2], instruction[3],
                      uc_strerror(fetched));
        fail(message);
        return 0;
    }
    void port_out(uint32_t port, uint32_t value) {
        if (performance_.profile) ++perf_stats_.port_outs;
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
        if (port == 0x201) { charge_joystick(); return; }
        if (port == 0x61) return;
        char message[80];
        std::snprintf(message, sizeof(message), "unsupported port OUT %04x", port);
        fail(message);
    }
};

void usage(const char* program) {
    std::cout << "Usage: " << program << " _EF2000.EXE [--instruction-limit N]"
              << " [--game-dir DIR] [--cd-dir DIR] [--cd-cue FILE] [--overlay-dir DIR] [--windowed]"
              << " [--timer-instructions N] (instruction timing only)\n"
              << "       [--sound off|sb16-probe|sb16-dma] (SDL3 audio if available)\n"
              << "       [--timing auto|instruction|realtime] (auto uses real-time for unlimited windowed runs)\n"
              << "       [--wall-seconds N] (bounded real-time/headless diagnostic)\n"
              << "       [--block-clock-check N] [--vga-clock-check N] (powers of two, 1..4096)\n"
              << "       [--tcg-cache-mib N] (32..1024 MiB; default Unicorn cache size)\n"
              << "       [--profile] (print callback counts and host time by subsystem)\n"
              << "       [--profile-hot-blocks] (sample and list 16 hottest guest blocks)\n"
              << "       [--profile-copy] (count calls to the original fast-copy routine)\n"
              << "       [--profile-copy-callers] (rank copy callers by bytes and count)\n"
              << "       [--profile-video-frames] (count changed displayed images, requires SDL3 window)\n"
              << "       [--no-fps] (hide the SDL presentation and changed-frame HUD)\n"
              << "       [--overlay-frame-bmp FILE] [--overlay-frame-border N] (1..32)\n"
              << "       [--overlay-font-bmp FILE] (extracted menu-font.bmp and .widths)\n"
              << "       [--overlay-symbols-bmp FILE] (extracted menu-symbols.bmp)\n"
              << "       [--overlay-skin-dir DIR] (extracted original title/panel textures)\n"
              << "       [--fast-copy] (experimental host acceleration of large guest copies)\n"
              << "       [--fast-copy-vga] (also accelerate copies into the banked VGA RAM window)\n"
              << "       [--fullscreen] [--geometry WIDTHxHEIGHT]"
              << " [--aspect-ratio 4:3|native|stretch] [--smooth-scaling]\n"
              << "       --instruction-limit 0 means unlimited (window default)\n"
              << "       [--mouse-event TICK:X:Y:BUTTONS] (repeat for scripted GUI clicks)\n"
              << "       [--key-event TICK:SDL_SCAN:ASCII:DOWN] (repeat for guest keys)\n"
              << "       [--joystick-event TICK:X:Y[:Z:W]:BUTTONS] (2/4 axes)\n"
              << "       [--joystick auto|off|INDEX] [--joystick-axes 0,1[,2,3]]\n"
              << "       [--gamepad-raw] (disable latched throttle and LB/RB keyboard layers)\n"
              << "       [F12 or gamepad Back: movable controls/diagnostics overlay]\n"
              << "       [F11 or Alt+Enter: toggle fullscreen; F10 remains a guest key]\n"
              << "       [--controls auto|keyboard|joystick] (virtual EF2000.CFG)\n"
              << "       [--trace-joystick] (selected SDL device and sampled input)\n"
              << "       [--trace-interrupts] (stderr DOS/X-32 call transcript)\n"
              << "       [--trace-archive-lookups] (unique DID pathname requests on stderr)\n"
              << "       [--framebuffer picture.ppm] (capture actual guest VGA/VBE mode)\n"
              << "       [--wav-capture DIR] (lossless SB16 and/or CD PCM WAV segments)\n"
              << "       --cd-cue FILE enables MSCDEX Red Book music from read-only CUE/BIN\n"
              << "       --cd-dir DIR provides DOS disc files only; it does not enable music\n"
              << "       --overlay-dir writes only to a separate copy-on-write DOS drive\n"
              << "       Windowed default: $XDG_STATE_HOME/ef2000/unicorn/drive\n"
              << "       Windows default: %LOCALAPPDATA%\\ef2000\\state\\unicorn\\drive\n"
              << "Experimental native C++ Unicorn X-32 game frontend.\n"
              << "Stops explicitly at unsupported DOS/BIOS interfaces.\n"
              << "       " << program << " --self-test | --self-test-bda | --self-test-clock | --self-test-retrace | --self-test-fast-copy | --self-test-archive-lookup\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        std::string path;
        std::string game_dir, cd_dir, cd_cue, overlay_dir;
        uint64_t limit = 30000000;
        bool explicit_limit = false;
        uint64_t timer_interval = 250000;
        ExecutionTiming timing = ExecutionTiming::Auto;
        double wall_seconds = 0;
        PerformanceOptions performance;
        WindowOptions window;
        ef2000::SoundMode sound_mode = ef2000::SoundMode::Off;
        ControlSelection controls = ControlSelection::Auto;
        bool self_test = false, self_test_bda = false, self_test_clock = false;
        bool self_test_retrace = false, self_test_fast_copy = false;
        bool self_test_archive_lookup = false;
        bool trace_interrupts = false, trace_archive_lookups = false;
        std::vector<MouseEvent> mouse_events;
        std::vector<TimedKeyEvent> key_events;
        std::vector<JoystickEvent> joystick_events;
        std::string framebuffer, wav_directory;
        for (int i = 1; i < argc; ++i) {
            if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h")) {
                usage(argv[0]); return 0;
            }
            if (!std::strcmp(argv[i], "--instruction-limit") && i + 1 < argc) {
                size_t end{};
                const std::string value = argv[++i];
                limit = std::stoull(value, &end);
                if (end != value.size()) throw std::runtime_error("invalid instruction limit");
                explicit_limit = true;
            } else if (!std::strcmp(argv[i], "--timer-instructions") && i + 1 < argc) {
                size_t end{};
                const std::string value = argv[++i];
                timer_interval = std::stoull(value, &end);
                if (end != value.size() || !timer_interval)
                    throw std::runtime_error("invalid timer interval");
            } else if (!std::strcmp(argv[i], "--timing") && i + 1 < argc) {
                const std::string choice = argv[++i];
                if (choice == "auto") timing = ExecutionTiming::Auto;
                else if (choice == "instruction") timing = ExecutionTiming::Instruction;
                else if (choice == "realtime") timing = ExecutionTiming::Realtime;
                else throw std::runtime_error("--timing expects auto, instruction or realtime");
            } else if (!std::strcmp(argv[i], "--wall-seconds") && i + 1 < argc) {
                size_t end{};
                const std::string value = argv[++i];
                wall_seconds = std::stod(value, &end);
                if (end != value.size() || !std::isfinite(wall_seconds) || wall_seconds <= 0)
                    throw std::runtime_error("--wall-seconds expects a positive finite number");
            } else if (!std::strcmp(argv[i], "--block-clock-check") && i + 1 < argc) {
                performance.block_clock_check = parse_clock_check(argv[++i], "--block-clock-check");
            } else if (!std::strcmp(argv[i], "--vga-clock-check") && i + 1 < argc) {
                performance.vga_clock_check = parse_clock_check(argv[++i], "--vga-clock-check");
            } else if (!std::strcmp(argv[i], "--tcg-cache-mib") && i + 1 < argc) {
                const auto size = parse_clock_check(argv[++i], "--tcg-cache-mib");
                if (size < 32 || size > 1024)
                    throw std::runtime_error("--tcg-cache-mib expects a power of two (32..1024)");
                performance.tcg_buffer_mib = size;
            } else if (!std::strcmp(argv[i], "--profile")) {
                performance.profile = true;
            } else if (!std::strcmp(argv[i], "--profile-hot-blocks")) {
                performance.profile = performance.hot_blocks = true;
            } else if (!std::strcmp(argv[i], "--profile-copy")) {
                performance.profile = performance.profile_copy = true;
            } else if (!std::strcmp(argv[i], "--profile-copy-callers")) {
                performance.profile = performance.profile_copy = performance.profile_copy_callers = true;
            } else if (!std::strcmp(argv[i], "--profile-video-frames")) {
                window.profile_video_frames = true;
            } else if (!std::strcmp(argv[i], "--no-fps")) {
                window.show_fps = false;
            } else if (!std::strcmp(argv[i], "--overlay-frame-bmp") && i + 1 < argc) {
                window.overlay_frame_bmp = argv[++i];
            } else if (!std::strcmp(argv[i], "--overlay-font-bmp") && i + 1 < argc) {
                window.overlay_font_bmp = argv[++i];
            } else if (!std::strcmp(argv[i], "--overlay-symbols-bmp") && i + 1 < argc) {
                window.overlay_symbols_bmp = argv[++i];
            } else if (!std::strcmp(argv[i], "--overlay-skin-dir") && i + 1 < argc) {
                window.overlay_skin_dir = argv[++i];
            } else if (!std::strcmp(argv[i], "--overlay-frame-border") && i + 1 < argc) {
                const std::string value = argv[++i];
                size_t parsed{};
                const auto number = std::stoul(value, &parsed);
                if (parsed != value.size() || number < 1 || number > 32)
                    throw std::runtime_error("--overlay-frame-border expects 1..32 pixels");
                window.overlay_frame_border = int(number);
            } else if (!std::strcmp(argv[i], "--fast-copy")) {
                performance.fast_copy = true;
            } else if (!std::strcmp(argv[i], "--fast-copy-vga")) {
                performance.fast_copy = performance.fast_copy_vga = true;
            } else if (!std::strcmp(argv[i], "--game-dir") && i + 1 < argc) {
                game_dir = argv[++i];
            } else if (!std::strcmp(argv[i], "--cd-dir") && i + 1 < argc) {
                cd_dir = argv[++i];
            } else if (!std::strcmp(argv[i], "--cd-cue") && i + 1 < argc) {
                cd_cue = argv[++i];
            } else if (!std::strcmp(argv[i], "--overlay-dir") && i + 1 < argc) {
                overlay_dir = argv[++i];
            } else if (!std::strcmp(argv[i], "--sound") && i + 1 < argc) {
                const std::string mode = argv[++i];
                if (mode == "sb16-probe") sound_mode = ef2000::SoundMode::Probe;
                else if (mode == "sb16-dma") sound_mode = ef2000::SoundMode::Dma;
                else if (mode == "off") sound_mode = ef2000::SoundMode::Off;
                else throw std::runtime_error("invalid --sound (off, sb16-probe, sb16-dma)");
            } else if (!std::strcmp(argv[i], "--windowed")) {
                window.enabled = true;
            } else if (!std::strcmp(argv[i], "--fullscreen")) {
                window.enabled = window.fullscreen = true;
            } else if (!std::strcmp(argv[i], "--geometry") && i + 1 < argc) {
                int width{}, height{};
                char trailing{};
                if (std::sscanf(argv[++i], "%dx%d%c", &width, &height, &trailing) != 2 ||
                    width <= 0 || height <= 0 || width > 16384 || height > 16384)
                    throw std::runtime_error("invalid --geometry WIDTHxHEIGHT");
                window.width = width;
                window.height = height;
            } else if (!std::strcmp(argv[i], "--aspect-ratio") && i + 1 < argc) {
                window.aspect_ratio = argv[++i];
                if (window.aspect_ratio != "4:3" && window.aspect_ratio != "native" &&
                    window.aspect_ratio != "stretch")
                    throw std::runtime_error("invalid --aspect-ratio (4:3, native, stretch)");
            } else if (!std::strcmp(argv[i], "--smooth-scaling")) {
                window.smooth_scaling = true;
            } else if (!std::strcmp(argv[i], "--joystick") && i + 1 < argc) {
                const std::string selection = argv[++i];
                if (selection == "off") window.joystick_index = -1;
                else if (selection == "auto") window.joystick_index = 0;
                else {
                    size_t parsed{};
                    const auto index = std::stoul(selection, &parsed);
                    if (parsed != selection.size() || index > 31)
                        throw std::runtime_error("--joystick expects auto, off or index 0..31");
                    window.joystick_index = int(index);
                }
            } else if (!std::strcmp(argv[i], "--controls") && i + 1 < argc) {
                const std::string choice = argv[++i];
                if (choice == "auto") controls = ControlSelection::Auto;
                else if (choice == "keyboard") controls = ControlSelection::Keyboard;
                else if (choice == "joystick") controls = ControlSelection::Joystick;
                else throw std::runtime_error("--controls expects auto, keyboard or joystick");
            } else if (!std::strcmp(argv[i], "--joystick-axes") && i + 1 < argc) {
                window.joystick_axes = parse_joystick_axes(argv[++i]);
                window.gamepad_flight_profile = false;
            } else if (!std::strncmp(argv[i], "--joystick-axes=", 16)) {
                window.joystick_axes = parse_joystick_axes(argv[i] + 16);
                window.gamepad_flight_profile = false;
            } else if (!std::strcmp(argv[i], "--gamepad-raw")) {
                window.gamepad_flight_profile = false;
            } else if (!std::strcmp(argv[i], "--trace-joystick")) {
                window.trace_joystick = true;
            } else if (!std::strcmp(argv[i], "--self-test")) {
                self_test = true;
            } else if (!std::strcmp(argv[i], "--self-test-bda")) {
                self_test_bda = true;
            } else if (!std::strcmp(argv[i], "--self-test-clock")) {
                self_test_clock = true;
            } else if (!std::strcmp(argv[i], "--self-test-retrace")) {
                self_test_retrace = true;
            } else if (!std::strcmp(argv[i], "--self-test-fast-copy")) {
                self_test_fast_copy = true;
            } else if (!std::strcmp(argv[i], "--self-test-archive-lookup")) {
                self_test_archive_lookup = true;
            } else if (!std::strcmp(argv[i], "--framebuffer") && i + 1 < argc) {
                framebuffer = argv[++i];
            } else if (!std::strcmp(argv[i], "--wav-capture") && i + 1 < argc) {
                wav_directory = argv[++i];
            } else if (!std::strcmp(argv[i], "--trace-interrupts")) {
                trace_interrupts = true;
            } else if (!std::strcmp(argv[i], "--trace-archive-lookups")) {
                trace_archive_lookups = true;
            } else if (!std::strcmp(argv[i], "--mouse-event") && i + 1 < argc) {
                MouseEvent event;
                unsigned long long tick{};
                char trailing{};
                if (std::sscanf(argv[++i], "%llu:%d:%d:%u%c", &tick, &event.x,
                                &event.y, &event.buttons, &trailing) != 4 ||
                    event.x < 0 || event.x >= 640 || event.y < 0 || event.y >= 480 ||
                    event.buttons >= 8)
                    throw std::runtime_error("invalid --mouse-event TICK:X:Y:BUTTONS");
                event.tick = tick;
                mouse_events.push_back(event);
            } else if (!std::strcmp(argv[i], "--key-event") && i + 1 < argc) {
                TimedKeyEvent event;
                unsigned long long tick{};
                int down{};
                char trailing{};
                if (std::sscanf(argv[++i], "%llu:%d:%d:%d%c", &tick,
                                &event.key.scan, &event.key.key, &down, &trailing) != 4 ||
                    event.key.scan < 0 || event.key.scan > 255 ||
                    event.key.key < 0 || event.key.key > 0x10ffff ||
                    (down != 0 && down != 1))
                    throw std::runtime_error("invalid --key-event TICK:SDL_SCAN:ASCII:DOWN");
                event.tick = tick;
                event.key.down = down;
                key_events.push_back(event);
            } else if (!std::strcmp(argv[i], "--joystick-event") && i + 1 < argc) {
                const std::string value = argv[++i];
                std::vector<int64_t> fields;
                size_t begin = 0;
                while (begin <= value.size()) {
                    const size_t end = value.find(':', begin);
                    const std::string field = value.substr(begin, end == std::string::npos ?
                        std::string::npos : end - begin);
                    size_t parsed{};
                    const int64_t number = std::stoll(field, &parsed);
                    if (parsed != field.size()) throw std::runtime_error("invalid joystick event number");
                    fields.push_back(number);
                    if (end == std::string::npos) break;
                    begin = end + 1;
                }
                if ((fields.size() != 4 && fields.size() != 6) || fields[0] < 0 ||
                    fields.back() < 0 || fields.back() > 15)
                    throw std::runtime_error("invalid --joystick-event TICK:X:Y[:Z:W]:BUTTONS");
                JoystickEvent event;
                event.tick = fields[0];
                event.count = fields.size() - 2;
                event.buttons = fields.back();
                if (!joystick_events.empty() && joystick_events.front().count != event.count)
                    throw std::runtime_error("mixed joystick axis counts");
                for (unsigned axis = 0; axis < event.count; ++axis) {
                    if (fields[axis + 1] < -32768 || fields[axis + 1] > 32767)
                        throw std::runtime_error("joystick axis outside signed 16-bit range");
                    event.axes[axis] = fields[axis + 1];
                }
                joystick_events.push_back(event);
            } else if (argv[i][0] != '-' && path.empty()) path = argv[i];
            else throw std::runtime_error("unrecognized option: " + std::string(argv[i]));
        }
        if (window.profile_video_frames && !window.enabled)
            throw std::runtime_error("--profile-video-frames requires --windowed or --fullscreen");
        if (!window.overlay_frame_bmp.empty() && !window.enabled)
            throw std::runtime_error("--overlay-frame-bmp requires --windowed or --fullscreen");
        if (!window.overlay_font_bmp.empty() && !window.enabled)
            throw std::runtime_error("--overlay-font-bmp requires --windowed or --fullscreen");
        if (!window.overlay_symbols_bmp.empty() && !window.enabled)
            throw std::runtime_error("--overlay-symbols-bmp requires --windowed or --fullscreen");
        if (!window.overlay_skin_dir.empty() && !window.enabled)
            throw std::runtime_error("--overlay-skin-dir requires --windowed or --fullscreen");
        if (self_test || self_test_bda || self_test_clock || self_test_retrace ||
            self_test_fast_copy || self_test_archive_lookup) {
            if (int(self_test) + int(self_test_bda) + int(self_test_clock) +
                int(self_test_retrace) + int(self_test_fast_copy) +
                int(self_test_archive_lookup) != 1)
                throw std::runtime_error("choose one --self-test mode");
            if (!path.empty()) throw std::runtime_error("self-test does not take an executable");
            if (!wav_directory.empty()) throw std::runtime_error("self-test does not capture audio");
            if (!cd_cue.empty()) throw std::runtime_error("self-test does not take a CD image");
            std::vector<uint8_t> image(self_test_fast_copy ? COPY_ENTRY + 5 :
                                       self_test_archive_lookup ? 0xeb521 : 0x1000);
            // DOS query, ISA gameport charge/read, then DOS exit. The same
            // synthetic payload works without any proprietary game files.
            constexpr uint8_t gameport_guest[] = {
                0xb8, 0x0b, 0x35, 0x00, 0x00, 0xcd, 0x21,
                0xba, 0x01, 0x02, 0x00, 0x00,
                0xb0, 0x00, 0xee, 0xec,
                0xb8, 0x00, 0x4c, 0x00, 0x00, 0xcd, 0x21};
            // Guest selects an X-32 descriptor *before* calling 3503/3505
            // to change its base/limit, then reads physical 0040:0063. This
            // catches both a wrong DOS-memory map and a stale x86 segment
            // cache; either failure deliberately traps via INT 82h.
            constexpr uint8_t bda_guest[] = {
                0xb8, 0x01, 0x35, 0x00, 0x00, 0xcd, 0x21,
                0x66, 0x8e, 0xdb,
                0xb9, 0x00, 0x04, 0x00, 0x00,
                0xb8, 0x03, 0x35, 0x00, 0x00, 0xcd, 0x21,
                0xb9, 0xff, 0xff, 0xff, 0xff,
                0xb8, 0x05, 0x35, 0x00, 0x00, 0xcd, 0x21,
                0xb8, 0x04, 0x35, 0x00, 0x00, 0xcd, 0x21,
                0x81, 0xf9, 0x00, 0x04, 0x00, 0x00, 0x74, 0x02, 0xcd, 0x82,
                0x66, 0xbb, 0x63, 0x00,
                0x0f, 0xb7, 0x03,
                0x3d, 0xd4, 0x03, 0x00, 0x00, 0x74, 0x02, 0xcd, 0x82,
                0x66, 0xbb, 0x07, 0x00, 0x66, 0x8e, 0xdb,
                0xb8, 0x00, 0x4c, 0x00, 0x00, 0xcd, 0x21};
            // Repeat a multi-byte instruction and count completed loop
            // iterations for an asset-free execution-speed benchmark.
            constexpr uint8_t clock_guest[] = {
                0xb8, 0x78, 0x56, 0x34, 0x12,  // mov eax,0x12345678
                0xba, 0x00, 0x00, 0x00, 0x00,  // mov edx,0
                0x42,                          // inc edx
                0xeb, 0xfd};                   // jmp back to inc
            // Wait for the end of a retrace pulse and then the next pulse.
            // This detects a stale 3DAh value that would hang game/driver
            // loops when port reads are sampled instead of clocked individually.
            constexpr uint8_t retrace_guest[] = {
                0xba, 0xda, 0x03, 0x00, 0x00,  // mov edx,0x3da
                0xec, 0xa8, 0x08, 0x75, 0xfb, // wait until bit 3 is clear
                0xec, 0xa8, 0x08, 0x74, 0xfb, // wait until bit 3 is set
                0xb8, 0x00, 0x4c, 0x00, 0x00, 0xcd, 0x21}; // DOS exit
            constexpr uint8_t archive_guest[] = {
                0xb8, 0x00, 0x20, 0x00, 0x00, // mov eax,0x2000: test filename
                0xba, 0x00, 0x30, 0x00, 0x00, // mov edx,0x3000: archive handle
                0xe8, 0x00, 0x00, 0x00, 0x00, // call 0xeb520: test tracepoint
                0xb8, 0x00, 0x4c, 0x00, 0x00, 0xcd, 0x21};
            // The 32-bit guest calls a deliberately invalid replacement
            // function at the original memcpy entry address. Passing this
            // test proves the fast path stopped *before* executing the body,
            // returned through RET 0Ch, and copied both ends of the buffer.
            std::vector<uint8_t> copy_guest = {
                0xbe, 0x78, 0x56, 0x34, 0x12,       // mov esi,0x12345678
                0xbf, 0x68, 0x24, 0x57, 0x13,       // mov edi,0x13572468
                0xbd, 0x01, 0xef, 0xcd, 0xab,       // mov ebp,0xabcdef01
                0x89, 0xe3,                         // mov ebx,esp (callee preserves EBX)
                0x68, 0x00, 0x00, 0x20, 0x00,       // push 0x200000 (destination)
                0x68, 0x00, 0x20, 0x00, 0x00,       // push 0x2000 (source)
                0x68, 0x00, 0x04, 0x00, 0x00,       // push 1024
                0xe8, 0x00, 0x00, 0x00, 0x00,       // call 0xe9a24
                0x3d, 0x00, 0x04, 0x20, 0x00,       // cmp eax,0x200400
                0x74, 0x02, 0xcd, 0x82,            // je success (else INT 82h)
                0x81, 0xfa, 0x00, 0x24, 0x00, 0x00, // cmp edx,0x2400
                0x74, 0x02, 0xcd, 0x82,
                0x39, 0xe3,                         // cmp ebx,esp
                0x74, 0x02, 0xcd, 0x82,
                0x81, 0xfe, 0x78, 0x56, 0x34, 0x12, // cmp esi,0x12345678
                0x74, 0x02, 0xcd, 0x82,
                0x81, 0xff, 0x68, 0x24, 0x57, 0x13, // cmp edi,0x13572468
                0x74, 0x02, 0xcd, 0x82,
                0x81, 0xfd, 0x01, 0xef, 0xcd, 0xab, // cmp ebp,0xabcdef01
                0x74, 0x02, 0xcd, 0x82,
                0xa1, 0x00, 0x00, 0x20, 0x00,       // mov eax,[0x200000]
                0x3d, 0x12, 0x34, 0x56, 0x78,
                0x74, 0x02, 0xcd, 0x82,
                0xa1, 0xfc, 0x03, 0x20, 0x00,       // mov eax,[0x2003fc]
                0x3d, 0xdf, 0x9b, 0x57, 0x13,
                0x74, 0x02, 0xcd, 0x82,
                0xb8, 0x00, 0x4c, 0x00, 0x00, 0xcd, 0x21};
            constexpr uint32_t copy_call_offset = 32;
            put32(copy_guest, copy_call_offset + 1,
                  COPY_ENTRY - (ENTRY + copy_call_offset + 5));
            if (performance.fast_copy_vga) {
                // Retarget the same guest ABI and first/last-byte assertions
                // to the 64 KiB banked VGA RAM window rather than guest data.
                put32(copy_guest, 18, VGA_COPY_BASE);
                put32(copy_guest, 38, VGA_COPY_BASE + 1024);
                put32(copy_guest, 93, VGA_COPY_BASE);
                put32(copy_guest, 107, VGA_COPY_BASE + 1020);
            }
            if (self_test_bda)
                std::copy(std::begin(bda_guest), std::end(bda_guest), image.begin() + ENTRY);
            else if (self_test_clock)
                std::copy(std::begin(clock_guest), std::end(clock_guest), image.begin() + ENTRY);
            else if (self_test_retrace)
                std::copy(std::begin(retrace_guest), std::end(retrace_guest), image.begin() + ENTRY);
            else if (self_test_fast_copy) {
                std::fill(image.begin() + 0x2000, image.begin() + 0x2400, 0x5a);
                put32(image, 0x2000, 0x78563412);
                put32(image, 0x23fc, 0x13579bdf);
                image[COPY_ENTRY] = 0xcd; // INT 82h traps if the host fast path fails.
                image[COPY_ENTRY + 1] = 0x82;
                std::copy(copy_guest.begin(), copy_guest.end(), image.begin() + ENTRY);
                performance.fast_copy = true;
            } else if (self_test_archive_lookup) {
                std::copy(std::begin(archive_guest), std::end(archive_guest), image.begin() + ENTRY);
                put32(image, ENTRY + 11, 0xeb520 - (ENTRY + 15));
                image[0xeb520] = 0xc3; // test routine returns without touching registers
                constexpr char pathname[] = "test.bbm";
                std::copy(std::begin(pathname), std::end(pathname), image.begin() + 0x2000);
            }
            else
                std::copy(std::begin(gameport_guest), std::end(gameport_guest), image.begin() + ENTRY);
            Machine machine(image, {}, {}, {}, ef2000::SoundMode::Off, false,
                            {}, {}, std::move(joystick_events), controls);
            if (self_test_archive_lookup) machine.enable_archive_lookup_trace();
            const bool realtime_test = self_test_clock || self_test_retrace || self_test_fast_copy;
            performance.clock_loop_benchmark = self_test_clock;
            return machine.run(realtime_test ? 0 : 10000, window, 250000, "", "",
                               realtime_test,
                               realtime_test ? (wall_seconds > 0 ? wall_seconds : 0.25) : 0,
                               performance);
        }
        if (path.empty()) { usage(argv[0]); return 2; }
        if (!wav_directory.empty() && sound_mode != ef2000::SoundMode::Dma && cd_cue.empty())
            throw std::runtime_error("--wav-capture requires --sound sb16-dma or --cd-cue");
        if ((window.enabled || wall_seconds > 0 || timing == ExecutionTiming::Realtime) &&
            !explicit_limit) limit = 0;
        const bool realtime = timing == ExecutionTiming::Realtime ||
            (timing == ExecutionTiming::Auto && !limit &&
             (window.enabled || wall_seconds > 0));
        if (realtime && limit)
            throw std::runtime_error("real-time timing requires --instruction-limit 0");
        if (performance.fast_copy && !realtime)
            throw std::runtime_error("--fast-copy requires real-time timing");
        if ((performance.hot_blocks || performance.profile_copy) && !realtime)
            throw std::runtime_error("guest block profiling requires real-time timing");
        if (wall_seconds > 0 && !realtime)
            throw std::runtime_error("--wall-seconds requires real-time timing");
        if (window.enabled && overlay_dir.empty()) {
#ifdef _WIN32
            if (const char* local_state = std::getenv("LOCALAPPDATA"); local_state && *local_state)
                overlay_dir = (fs::path(local_state) / "ef2000/state/unicorn/drive").string();
            else throw std::runtime_error("set --overlay-dir or LOCALAPPDATA for writable DOS files");
#else
            if (const char* xdg_state = std::getenv("XDG_STATE_HOME"); xdg_state && *xdg_state)
                overlay_dir = (fs::path(xdg_state) / "ef2000/unicorn/drive").string();
            else if (const char* home = std::getenv("HOME"); home && *home)
                overlay_dir = (fs::path(home) / ".local/state/ef2000/unicorn/drive").string();
            else throw std::runtime_error("set --overlay-dir or XDG_STATE_HOME for writable DOS files");
#endif
        }
        std::stable_sort(mouse_events.begin(), mouse_events.end(),
                         [](const MouseEvent& a, const MouseEvent& b) { return a.tick < b.tick; });
        std::stable_sort(key_events.begin(), key_events.end(),
                         [](const TimedKeyEvent& a, const TimedKeyEvent& b) { return a.tick < b.tick; });
        std::stable_sort(joystick_events.begin(), joystick_events.end(),
                         [](const JoystickEvent& a, const JoystickEvent& b) { return a.tick < b.tick; });
        const auto image = payload_from_executable(path);
        if (game_dir.empty()) game_dir = fs::path(path).parent_path().string();
        if (cd_dir.empty()) cd_dir = game_dir;
        Machine machine(image, game_dir, cd_dir, overlay_dir, sound_mode, trace_interrupts,
                        std::move(mouse_events), std::move(key_events), std::move(joystick_events),
                        controls, cd_cue);
        if (trace_archive_lookups) machine.enable_archive_lookup_trace();
        return machine.run(limit, window, timer_interval, framebuffer, wav_directory,
                           realtime, wall_seconds, performance);
    } catch (const std::exception& error) {
        std::cerr << "ef2000-unicorn-cpp: " << error.what() << '\n';
        return 1;
    }
}
