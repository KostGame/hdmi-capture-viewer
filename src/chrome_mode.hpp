#pragma once

#include <cstdint>

namespace hcv {

enum class ChromeMode { Normal, BorderlessWindow, Fullscreen };
enum class ChromeState { NormalPinned, BorderlessAutoHide, FullscreenAutoHide };

inline ChromeState chrome_state(ChromeMode mode) noexcept {
    switch (mode) {
    case ChromeMode::Normal: return ChromeState::NormalPinned;
    case ChromeMode::BorderlessWindow: return ChromeState::BorderlessAutoHide;
    case ChromeMode::Fullscreen: return ChromeState::FullscreenAutoHide;
    }
    return ChromeState::NormalPinned;
}

// Clock-driven policy keeps reveal and hide behavior testable without Win32.
class OverlayVisibilityPolicy {
public:
    static constexpr std::uint64_t HideDelayMs = 1200;

    void set_mode(ChromeMode mode, std::uint64_t nowMs) noexcept {
        mode_ = mode;
        visible_ = mode == ChromeMode::Normal;
        lastActivityMs_ = nowMs;
        if (mode == ChromeMode::Normal) pointerOverOverlay_ = false;
    }

    void reveal(std::uint64_t nowMs) noexcept {
        visible_ = true;
        lastActivityMs_ = nowMs;
    }

    void pointer_over_overlay(bool over, std::uint64_t nowMs) noexcept {
        pointerOverOverlay_ = over;
        if (over) reveal(nowMs);
        else lastActivityMs_ = nowMs;
    }

    void timer(std::uint64_t nowMs) noexcept {
        if (mode_ != ChromeMode::Normal && visible_ && !pointerOverOverlay_ &&
            nowMs >= lastActivityMs_ && nowMs - lastActivityMs_ >= HideDelayMs) {
            visible_ = false;
        }
    }

    bool visible() const noexcept { return visible_; }
    bool auto_hide() const noexcept { return mode_ != ChromeMode::Normal; }

private:
    ChromeMode mode_{ChromeMode::Normal};
    bool visible_{true};
    bool pointerOverOverlay_{};
    std::uint64_t lastActivityMs_{};
};

} // namespace hcv
