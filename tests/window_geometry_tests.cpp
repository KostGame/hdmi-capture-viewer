#include "window_geometry.hpp"
#include <cassert>
#include <iostream>

int main() {
    using hcv::fit_inside_aspect;
    assert((fit_inside_aspect(1920, 1080, 1920, 1080).width == 1920));
    auto wide = fit_inside_aspect(1920, 900, 1920, 1080);
    assert(wide.width == 1600 && wide.height == 900);
    auto tall = fit_inside_aspect(1200, 900, 1920, 1080);
    assert(tall.width == 1200 && tall.height == 675);
    auto fourThree = fit_inside_aspect(1024, 768, 1920, 1080);
    assert(fourThree.width == 1024 && fourThree.height == 576);
    auto invalid = fit_inside_aspect(800, 600, 0, 0);
    assert(invalid.width == 800 && invalid.height == 600);
    std::cout << "WINDOW_GEOMETRY_TESTS_PASS\n";
}
