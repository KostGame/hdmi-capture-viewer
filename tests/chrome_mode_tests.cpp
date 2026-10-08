#include "chrome_mode.hpp"
#include <cassert>
#include <iostream>

int main() {
    using hcv::ChromeMode;
    using hcv::ChromeState;
    using hcv::WorkRect;

    assert(hcv::reveal_strip_px(96) == 4);
    assert(hcv::reveal_strip_px(120) == 5);
    assert(hcv::reveal_strip_px(144) == 6);
    assert(hcv::reveal_strip_px(192) == 8);
    assert(hcv::cursor_in_reveal_strip(500, 0, 0, 0, 1920, 1080, 4));
    assert(hcv::cursor_in_reveal_strip(500, 4, 0, 0, 1920, 1080, 4));
    assert(!hcv::cursor_in_reveal_strip(500, 5, 0, 0, 1920, 1080, 4));
    assert(!hcv::cursor_in_reveal_strip(-1, 0, 0, 0, 1920, 1080, 4));
    assert(!hcv::cursor_in_reveal_strip(1920, 0, 0, 0, 1920, 1080, 4));
    assert(!hcv::cursor_in_reveal_strip(500, 0, 0, 0, 0, 1080, 4));
    assert(hcv::chrome_state(ChromeMode::Normal) == ChromeState::NormalPinned);
    assert(hcv::chrome_state(ChromeMode::BorderlessWindow) == ChromeState::BorderlessAutoHide);
    assert(hcv::chrome_state(ChromeMode::Fullscreen) == ChromeState::FullscreenAutoHide);

    constexpr WorkRect work{0, 0, 3840, 2080};
    // Left/right halves, all four quarters, including invisible 8px edges.
    assert(hcv::snap_like_placement({-8, -8, 1920, 2088}, work, 24));
    assert(hcv::snap_like_placement({1920, -8, 3848, 2088}, work, 24));
    assert(hcv::snap_like_placement({-8, -8, 1920, 1040}, work, 24));
    assert(hcv::snap_like_placement({1920, -8, 3848, 1040}, work, 24));
    assert(hcv::snap_like_placement({-8, 1040, 1920, 2088}, work, 24));
    assert(hcv::snap_like_placement({1920, 1040, 3848, 2088}, work, 24));
    // A free-floating or full-screen window must not be snap-locked.
    assert(!hcv::snap_like_placement({200, 120, 1800, 1200}, work, 24));
    assert(!hcv::snap_like_placement({0, 0, 3840, 2080}, work, 24));
    assert(!hcv::snap_like_placement({0, 0, 100, 100}, work, -1));
    assert(!hcv::snap_like_placement({0, 0, 0, 100}, work, 24));
    // Multi-monitor negative work coordinates.
    constexpr WorkRect leftMonitor{-1920, 0, 0, 1040};
    assert(hcv::snap_like_placement({-1928, -8, -960, 1048}, leftMonitor, 24));
    assert(!hcv::snap_like_placement({-1800, 100, -700, 800}, leftMonitor, 24));
    // Conservative false positive: a floating window manually aligned to
    // work-area corner also keeps its caption. No claim of OS Snap certainty.
    assert(hcv::snap_like_placement({0, 0, 1000, 800}, work, 24));

    assert(hcv::keep_revealed_caption(true, true, false, false));
    assert(hcv::keep_revealed_caption(true, false, true, false));
    assert(hcv::keep_revealed_caption(true, false, false, true));
    assert(!hcv::keep_revealed_caption(true, false, false, false));
    assert(!hcv::keep_revealed_caption(false, true, true, true));

    std::cout << "NATIVE_CAPTION_SNAP_GUARD_TESTS_PASS\n";
}
