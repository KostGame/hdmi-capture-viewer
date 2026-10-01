#include "resize_gate.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

int main() {
    using hcv::ResizeGate;
    using Clock = ResizeGate::Clock;
    const Clock::time_point t{std::chrono::seconds(100)};
    ResizeGate gate;
    assert(!gate.ready(t, true));
    gate.request(0, 1080);
    assert(!gate.pending());
    gate.request(800, 600);
    assert(gate.ready(t, true));
    assert(gate.width() == 800 && gate.height() == 600);
    gate.attempted(t);
    gate.request(801, 601);
    gate.request(1200, 900);
    assert(gate.width() == 1200 && gate.height() == 900);
    assert(!gate.ready(t + std::chrono::milliseconds(16), true));
    assert(!gate.ready(t + std::chrono::milliseconds(32), true));
    assert(gate.ready(t + std::chrono::milliseconds(33), true));
    gate.completed(t + std::chrono::milliseconds(33));
    assert(!gate.pending());
    gate.request(1280, 720);
    // Mouse released: always apply the exact final dimensions immediately.
    assert(gate.ready(t + std::chrono::milliseconds(34), false));
    gate.attempted(t + std::chrono::milliseconds(34)); // A failed resize is retried, but throttled.
    assert(gate.pending());
    assert(!gate.ready(t + std::chrono::milliseconds(35), true));
    assert(gate.ready(t + std::chrono::milliseconds(67), true));
    gate.completed(t + std::chrono::milliseconds(67));
    assert(!gate.ready(t + std::chrono::seconds(2), false));
    std::cout << "RESIZE_GATE_TESTS_PASS\n";
}
