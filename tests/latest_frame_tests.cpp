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

    // A burst must leave only the newest frame, including timing metadata.
    for (std::uint64_t sequence = 3; sequence < 1003; ++sequence) {
        hcv::Frame frame;
        frame.sequence = sequence;
        frame.conversionMs = 2.5;
        frames.publish(std::move(frame));
    }
    assert(frames.replaced() == 1000);
    newest = frames.take();
    assert(newest && newest->sequence == 1002);
    assert(newest->conversionMs == 2.5);
    assert(!frames.take());
    frames.publish({640, 480, 1003, {}});
    assert(frames.replaced() == 1000);
}
