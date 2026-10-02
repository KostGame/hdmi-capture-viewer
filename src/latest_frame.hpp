#pragma once

#include <cstdint>
#include <chrono>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace hcv {

enum class PixelFormat { Bgra32, Yuy2 };

inline bool packed_yuy2_size(int width, int height, std::size_t& bytes) {
    if (width <= 0 || height <= 0 || (width & 1) != 0) return false;
    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    if (w > static_cast<std::size_t>(-1) / 2 || w * 2 > static_cast<std::size_t>(-1) / h) return false;
    bytes = w * 2 * h;
    return true;
}

struct Frame {
    int width{};
    int height{};
    std::uint64_t sequence{};
    PixelFormat format{PixelFormat::Bgra32};
    std::vector<std::uint8_t> pixels;
    std::chrono::steady_clock::time_point capturedAt{std::chrono::steady_clock::now()};
    double captureCopyMs{};
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
