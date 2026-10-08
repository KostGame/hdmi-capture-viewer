#pragma once

#include <cstdint>

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

// Conservative placement guard, NOT an official Windows Snap-state query.
// GetWindowRect may include invisible resize borders. A freely placed window
// close to two work-area edges may be treated as pinned; keeping its caption
// is safer than changing geometry unexpectedly.
struct WorkRect { int left, top, right, bottom; };

inline bool close_edge(int a, int b, int tolerance) noexcept {
    const auto distance = static_cast<std::int64_t>(a) - b;
    return distance >= -static_cast<std::int64_t>(tolerance) &&
        distance <= static_cast<std::int64_t>(tolerance);
}

inline bool snap_like_placement(
    WorkRect window, WorkRect work, int tolerance) noexcept {
    if (tolerance < 0) return false;
    const auto width = static_cast<std::int64_t>(window.right) - window.left;
    const auto height = static_cast<std::int64_t>(window.bottom) - window.top;
    const auto workWidth = static_cast<std::int64_t>(work.right) - work.left;
    const auto workHeight = static_cast<std::int64_t>(work.bottom) - work.top;
    if (width <= 0 || height <= 0 || workWidth <= 0 || workHeight <= 0)
        return false;
    // Maximize is guarded separately with IsZoomed.
    if (width + 2LL*tolerance >= workWidth &&
        height + 2LL*tolerance >= workHeight) return false;
    const bool alignedX = close_edge(window.left, work.left, tolerance) ||
        close_edge(window.right, work.right, tolerance);
    const bool alignedY = close_edge(window.top, work.top, tolerance) ||
        close_edge(window.bottom, work.bottom, tolerance);
    return alignedX && alignedY;
}

inline bool keep_revealed_caption(
    bool alreadyRevealed, bool snappedOrMaximized,
    bool movingOrResizing, bool systemMenuActive) noexcept {
    return alreadyRevealed &&
        (snappedOrMaximized || movingOrResizing || systemMenuActive);
}

} // namespace hcv
