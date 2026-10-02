#pragma once

namespace hcv {

enum class ChromeMode { Normal, BorderlessWindow, Fullscreen };

struct OuterRect {
    int left{};
    int top{};
    int right{};
    int bottom{};
};

inline OuterRect same_outer_rect_after_chrome_change(OuterRect rect) noexcept {
    return rect;
}

inline bool frameless(ChromeMode mode) noexcept { return mode != ChromeMode::Normal; }

} // namespace hcv
