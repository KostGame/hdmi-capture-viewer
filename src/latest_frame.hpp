#pragma once

#include <cstdint>
#include <chrono>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace hcv {

struct Frame {
    int width{};
    int height{};
    std::uint64_t sequence{};
    std::vector<std::uint8_t> bgra;
    std::chrono::steady_clock::time_point capturedAt{std::chrono::steady_clock::now()};
};

// A one-slot mailbox: producers replace stale frames and consumers take the
// newest complete frame. The queue never grows with capture rate.
class LatestFrame {
public:
    void publish(Frame frame) {
        std::lock_guard lock(mutex_);
        if (pending_) ++replaced_;
        pending_ = std::move(frame);
    }

    std::optional<Frame> take() {
        std::lock_guard lock(mutex_);
        auto result = std::move(pending_);
        pending_.reset();
        return result;
    }

    std::uint64_t replaced() const {
        std::lock_guard lock(mutex_);
        return replaced_;
    }

private:
    mutable std::mutex mutex_;
    std::optional<Frame> pending_;
    std::uint64_t replaced_{};
};

} // namespace hcv
