#include "chrome_mode.hpp"
#include <cassert>
#include <iostream>

int main() {
    using hcv::ChromeMode;
    static_assert(ChromeMode::Normal != ChromeMode::BorderlessWindow);
    static_assert(ChromeMode::BorderlessWindow != ChromeMode::Fullscreen);
    assert(!hcv::frameless(ChromeMode::Normal));
    assert(hcv::frameless(ChromeMode::BorderlessWindow));
    assert(hcv::frameless(ChromeMode::Fullscreen));
    const hcv::OuterRect before{73, 41, 1301, 829};
    const auto after = hcv::same_outer_rect_after_chrome_change(before);
    assert(after.left == before.left && after.top == before.top);
    assert(after.right == before.right && after.bottom == before.bottom);
    std::cout << "CHROME_MODE_TESTS_PASS\n";
}
