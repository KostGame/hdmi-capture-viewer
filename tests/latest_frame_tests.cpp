#include "../src/latest_frame.hpp"

#include <cassert>

int main() {
    std::size_t packedBytes = 0;
    assert(hcv::packed_yuy2_size(1920, 1080, packedBytes));
    assert(packedBytes == 1920u * 1080u * 2u);
    assert(!hcv::packed_yuy2_size(1919, 1080, packedBytes));
    assert(!hcv::packed_yuy2_size(0, 1080, packedBytes));

    hcv::LatestFrame frames;
    assert(!frames.take());
    hcv::Frame oldFrame; oldFrame.width = 640; oldFrame.height = 480; oldFrame.sequence = 1; oldFrame.pixels = {1};
    frames.publish(std::move(oldFrame));
    hcv::Frame packedFrame; packedFrame.width = 1920; packedFrame.height = 1080; packedFrame.sequence = 2;
    packedFrame.format = hcv::PixelFormat::Yuy2; packedFrame.pixels.resize(packedBytes);
    frames.publish(std::move(packedFrame));
    assert(frames.replaced() == 1);
    auto newest = frames.take();
    assert(newest && newest->sequence == 2 && newest->width == 1920 && newest->format == hcv::PixelFormat::Yuy2);
    assert(!frames.take());
    assert(frames.replaced() == 1);

    // A burst must leave only the newest frame, including timing metadata.
    for (std::uint64_t sequence = 3; sequence < 1003; ++sequence) {
        hcv::Frame frame;
        frame.sequence = sequence;
        frame.captureCopyMs = 2.5;
        frames.publish(std::move(frame));
    }
    assert(frames.replaced() == 1000);
    newest = frames.take();
    assert(newest && newest->sequence == 1002);
    assert(newest->captureCopyMs == 2.5);
    assert(!frames.take());
    hcv::Frame lastFrame; lastFrame.width = 640; lastFrame.height = 480; lastFrame.sequence = 1003;
    frames.publish(std::move(lastFrame));
    assert(frames.replaced() == 1000);
}
