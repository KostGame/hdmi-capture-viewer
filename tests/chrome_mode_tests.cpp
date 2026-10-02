#include "chrome_mode.hpp"
#include <cassert>
#include <iostream>

int main() {
    using hcv::ChromeMode;
    using hcv::ChromeState;
    hcv::OverlayVisibilityPolicy overlay;

    assert(hcv::chrome_state(ChromeMode::Normal) == ChromeState::NormalPinned);
    assert(hcv::chrome_state(ChromeMode::BorderlessWindow) == ChromeState::BorderlessAutoHide);
    assert(hcv::chrome_state(ChromeMode::Fullscreen) == ChromeState::FullscreenAutoHide);

    overlay.set_mode(ChromeMode::Normal, 0);
    overlay.timer(10000);
    assert(overlay.visible()); // Normal chrome stays pinned.

    overlay.set_mode(ChromeMode::BorderlessWindow, 10000);
    assert(!overlay.visible() && overlay.auto_hide());
    overlay.reveal(10100); // Parent top reveal zone.
    overlay.timer(11299);
    assert(overlay.visible());
    overlay.timer(11300);
    assert(!overlay.visible());

    overlay.reveal(12000);
    overlay.pointer_over_overlay(true, 12050);
    overlay.timer(20000);
    assert(overlay.visible()); // Hovering the child keeps chrome visible.
    overlay.pointer_over_overlay(false, 20000);
    overlay.timer(21199);
    assert(overlay.visible());
    overlay.timer(21200);
    assert(!overlay.visible());

    overlay.set_mode(ChromeMode::Fullscreen, 22000);
    assert(!overlay.visible() && overlay.auto_hide());
    overlay.reveal(22001);
    overlay.timer(23201);
    assert(!overlay.visible());
    std::cout << "CHROME_MODE_TESTS_PASS\n";
}
