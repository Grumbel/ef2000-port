// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace ef2000 {

// Host-only 640x480 logical-presentation window manager. No SDL or guest
// dependencies: input and geometry can be tested without original game files.
struct OverlayPanel {
    enum class Kind { Controls, Diagnostics };
    Kind kind{};
    float x{}, y{}, w{}, h{};
    int scroll{};
    bool open{true};
    bool minimized{}, maximized{};
};

class OverlayWindows {
public:
    static constexpr float canvas_width = 640, canvas_height = 480;
    static constexpr float title_height = 24, resize_size = 16, line_height = 16;

    bool visible() const { return visible_; }
    const std::array<OverlayPanel, 2>& panels() const { return panels_; }
    const std::array<int, 2>& order() const { return order_; }
    int focused_panel() const { return focused_; }

    void toggle() {
        visible_ = !visible_;
        if (visible_ && !panels_[0].open && !panels_[1].open) {
            panels_[0].open = true;
            panels_[1].open = true;
        }
        grab_ = Grab::None;
    }

    // Overlay input is modal while visible: even a click outside a host
    // panel must not leak through to the DOS menu/cockpit underneath.
    bool button(float x, float y, bool down, bool left) {
        if (!visible_) return false;
        if (!left) return true;
        if (!down) {
            grab_ = Grab::None;
            return true;
        }
        grab_ = Grab::None;
        for (int z = int(order_.size()) - 1; z >= 0; --z) {
            const int index = order_[z];
            auto& panel = panels_[index];
            if (!panel.open || !contains(panel, x, y)) continue;
            focus(index);
            if (y >= panel.y + 4 && y < panel.y + title_height - 2) {
                if (x >= panel.x + panel.w - 18) {
                    panel.open = false;
                    grab_ = Grab::None;
                    if (panels_[1 - index].open) focus(1 - index);
                    return true;
                }
                if (x >= panel.x + panel.w - 33) {
                    maximize(index);
                    return true;
                }
                if (x >= panel.x + panel.w - 48) {
                    minimize(index);
                    return true;
                }
            }
            start_x_ = x;
            start_y_ = y;
            initial_ = panel;
            if (!panel.maximized && !panel.minimized &&
                x >= panel.x + panel.w - resize_size &&
                y >= panel.y + panel.h - resize_size)
                grab_ = Grab::Resize;
            else if (!panel.maximized && y < panel.y + title_height)
                grab_ = Grab::Move;
            return true;
        }
        return true;
    }

    bool motion(float x, float y) {
        if (!visible_) return false;
        if (grab_ == Grab::None) return true;
        auto& p = panels_[focused_];
        if (!p.open) return true;
        const float dx = x - start_x_, dy = y - start_y_;
        if (grab_ == Grab::Move) {
            // Keep the titlebar reachable even if the cursor leaves the view.
            p.x = std::clamp(initial_.x + dx, -initial_.w + 90, canvas_width - 90);
            p.y = std::clamp(initial_.y + dy, 0.0f, canvas_height - title_height);
        } else {
            const float min_width = p.kind == OverlayPanel::Kind::Controls ? 220 : 165;
            p.w = std::clamp(initial_.w + dx, min_width,
                             std::max(min_width, canvas_width - p.x));
            p.h = std::clamp(initial_.h + dy, 120.0f,
                             std::max(120.0f, canvas_height - p.y));
        }
        return true;
    }

    void wheel(int lines, int max_scroll) {
        if (!visible_ || !panels_[focused_].open) return;
        auto& panel = panels_[focused_];
        if (panel.minimized) return;
        panel.scroll = std::clamp(panel.scroll - lines, 0, std::max(0, max_scroll));
    }

    void focus_next() {
        if (!visible_) return;
        for (int i = 0; i < int(panels_.size()); ++i) {
            const int next = (focused_ + i + 1) % int(panels_.size());
            if (panels_[next].open) { focus(next); return; }
        }
    }

    void open_panel(OverlayPanel::Kind kind) {
        const int index = kind == OverlayPanel::Kind::Controls ? 0 : 1;
        panels_[index].open = true;
        if (panels_[index].minimized) minimize(index);
        focus(index);
    }

    void clamp_scroll(int index, int max_scroll) {
        panels_[index].scroll = std::clamp(panels_[index].scroll, 0, std::max(0, max_scroll));
    }

private:
    enum class Grab { None, Move, Resize };
    bool visible_{};
    std::array<OverlayPanel, 2> panels_{ {
        {OverlayPanel::Kind::Controls, 28, 45, 385, 360, 0, true},
        {OverlayPanel::Kind::Diagnostics, 420, 70, 200, 190, 0, true}
    } };
    std::array<int, 2> order_{{0, 1}}; // back-to-front
    int focused_{1};
    Grab grab_{Grab::None};
    float start_x_{}, start_y_{};
    OverlayPanel initial_{};
    std::array<OverlayPanel, 2> maximize_restore_{};
    std::array<float, 2> minimize_restore_height_{};

    void minimize(int index) {
        auto& panel = panels_[index];
        if (panel.maximized) panel = maximize_restore_[index];
        if (panel.minimized) {
            panel.h = minimize_restore_height_[index];
            panel.minimized = false;
        } else {
            minimize_restore_height_[index] = panel.h;
            panel.h = title_height;
            panel.minimized = true;
        }
    }
    void maximize(int index) {
        auto& panel = panels_[index];
        if (panel.maximized) {
            panel = maximize_restore_[index];
            return;
        }
        maximize_restore_[index] = panel;
        panel.x = 0;
        panel.y = 0;
        panel.w = canvas_width;
        panel.h = canvas_height - title_height;
        panel.minimized = false;
        panel.maximized = true;
    }

    static bool contains(const OverlayPanel& p, float x, float y) {
        return x >= p.x && x < p.x + p.w && y >= p.y && y < p.y + p.h;
    }
    void focus(int index) {
        focused_ = index;
        if (order_.back() != index) std::swap(order_[0], order_[1]);
    }
};

} // namespace ef2000
