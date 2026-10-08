#pragma once

namespace hcv {

enum class ChromeMode { Normal, BorderlessWindow, Fullscreen };
enum class ChromeState { NormalPinned, BorderlessAutoHide, FullscreenAutoHide };

inline int reveal_strip_px(int dpi) noexcept {
    return dpi > 0 ? (4 * dpi + 95) / 96 : 4;
}

inline bool cursor_in_reveal_strip(
    int cursorX, int cursorY,
    int left, int top, int right, int bottom,
    int revealPx) noexcept {
    if (revealPx <= 0 || right <= left || bottom <= top) return false;
    return cursorX >= left && cursorX < right &&
        cursorY >= top && cursorY <= top + revealPx &&
        cursorY < bottom;
}

inline ChromeState chrome_state(ChromeMode mode) noexcept {
    switch (mode) {
    case ChromeMode::Normal: return ChromeState::NormalPinned;
    case ChromeMode::BorderlessWindow: return ChromeState::BorderlessAutoHide;
    case ChromeMode::Fullscreen: return ChromeState::FullscreenAutoHide;
    }
    return ChromeState::NormalPinned;
}

// No screen-edge geometry or inferred Snap state. Only real Windows move/size
// transitions pin the native caption, and only after it was revealed.
inline bool caption_should_pin_after_window_move(
    bool captionRevealed, bool movedOrSized) noexcept {
    return captionRevealed && movedOrSized;
}

} // namespace hcv
