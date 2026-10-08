#include "chrome_mode.hpp"
#include <cassert>
#include <iostream>

int main() {
    using hcv::ChromeMode;
    using hcv::ChromeState;
    assert(hcv::reveal_strip_px(96) == 4);
    assert(hcv::reveal_strip_px(120) == 5);
    assert(hcv::reveal_strip_px(144) == 6);
    assert(hcv::reveal_strip_px(192) == 8);
    assert(hcv::cursor_in_reveal_strip(500, 0, 0, 0, 1920, 1080, 4));
    assert(hcv::cursor_in_reveal_strip(500, 4, 0, 0, 1920, 1080, 4));
    assert(!hcv::cursor_in_reveal_strip(500, 5, 0, 0, 1920, 1080, 4));
    assert(!hcv::cursor_in_reveal_strip(-1, 0, 0, 0, 1920, 1080, 4));
    assert(hcv::chrome_state(ChromeMode::Normal) == ChromeState::NormalPinned);
    assert(hcv::chrome_state(ChromeMode::BorderlessWindow) == ChromeState::BorderlessAutoHide);
    assert(hcv::chrome_state(ChromeMode::Fullscreen) == ChromeState::FullscreenAutoHide);

    // A style-only SWP_FRAMECHANGED must NOT pin the caption.
    assert(!hcv::caption_should_pin_after_window_move(true, false));
    // Real drag/resize or Win+Arrow/Snap with a visible caption must pin.
    assert(hcv::caption_should_pin_after_window_move(true, true));
    // No automatic revelation or pinning from moving a hidden-caption window.
    assert(!hcv::caption_should_pin_after_window_move(false, true));
    assert(!hcv::caption_should_pin_after_window_move(false, false));
    std::cout << "NATIVE_CAPTION_EVENT_LATCH_TESTS_PASS\n";
}
