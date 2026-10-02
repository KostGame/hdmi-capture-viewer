#include "resize_gate.hpp"
#include <cassert>
#include <iostream>

int main() {
    using hcv::ResizeGate;
    using Clock = ResizeGate::Clock;
    const Clock::time_point t{};
    ResizeGate gate;
    assert(!gate.ready(t, true));
    gate.request(0, 1080);
    assert(!gate.pending());
    gate.request(800, 600);
    assert(!gate.ready(t, true)); // No ResizeBuffers during the native modal loop.
    assert(gate.ready(t, false));
    assert(gate.width() == 800 && gate.height() == 600);
    gate.request(801, 601);
    gate.request(1200, 900);
    assert(gate.width() == 1200 && gate.height() == 900);
    assert(!gate.ready(t, true));
    assert(gate.ready(t, false));
    gate.completed(t);
    assert(!gate.pending());
    gate.request(1280, 720);
    // Mouse released: always apply the exact final dimensions immediately.
    assert(gate.ready(t, false));
    assert(gate.pending());
    assert(!gate.ready(t, true));
    assert(gate.ready(t, false));
    gate.completed(t);
    assert(!gate.ready(t, false));
    std::cout << "RESIZE_GATE_TESTS_PASS\n";
}
