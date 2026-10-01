#pragma once

#include <chrono>

namespace hcv {

// Coalesce a burst of WM_SIZE requests; constrain expensive DXGI resizes
// during the Win32 interactive move/size loop. The UI thread owns this gate.
class ResizeGate {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto interval = std::chrono::milliseconds(33);

    void request(int width, int height) {
        if (width <= 0 || height <= 0) return;
        width_ = width;
        height_ = height;
        pending_ = true;
    }

    bool ready(Clock::time_point now, bool interactive) const {
        if (!pending_) return false;
        return !interactive || !lastAttempt_.time_since_epoch().count() ||
            now - lastAttempt_ >= interval;
    }

    int width() const { return width_; }
    int height() const { return height_; }
    bool pending() const { return pending_; }
    void attempted(Clock::time_point now) { lastAttempt_ = now; }
    void completed(Clock::time_point now) { attempted(now); pending_ = false; }

private:
    int width_{};
    int height_{};
    bool pending_{};
    Clock::time_point lastAttempt_{};
};

} // namespace hcv
