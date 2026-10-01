#include "../src/latest_frame.hpp"

#include <cassert>

int main() {
    hcv::LatestFrame frames;
    assert(!frames.take());
    frames.publish({640, 480, 1, {1}});
    frames.publish({1920, 1080, 2, {2}});
    assert(frames.replaced() == 1);
    auto newest = frames.take();
    assert(newest && newest->sequence == 2 && newest->width == 1920);
    assert(!frames.take());
    assert(frames.replaced() == 1);
}
