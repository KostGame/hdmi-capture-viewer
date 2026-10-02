#pragma once

#include <chrono>

namespace hcv {

// Coalesce WM_SIZE requests. The UI thread applies the latest size after
// Windows exits its interactive move/size loop.
class ResizeGate {
public:
    using Clock = std::chrono::steady_clock;
    void request(int width, int height) {
        if (width <= 0 || height <= 0) return;
        width_ = width;
        height_ = height;
        pending_ = true;
    }

    bool ready(Clock::time_point, bool interactive) const {
        if (!pending_) return false;
        return !interactive;
    }

    int width() const { return width_; }
    int height() const { return height_; }
    bool pending() const { return pending_; }
    void completed(Clock::time_point) { pending_ = false; }

private:
    int width_{};
    int height_{};
    bool pending_{};
};

} // namespace hcv
