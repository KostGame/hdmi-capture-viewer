#pragma once

#include <algorithm>
#include <cstdint>

namespace hcv {

struct IntSize {
    int width{};
    int height{};
};

struct IntRect {
    int left{}, top{}, right{}, bottom{};
};

struct Insets {
    int left{}, top{}, right{}, bottom{};
};

inline Insets clipped_insets(IntRect outer, IntRect visibleBounds) {
    return {
        std::max(0, visibleBounds.left - outer.left),
        std::max(0, visibleBounds.top - outer.top),
        std::max(0, outer.right - visibleBounds.right),
        std::max(0, outer.bottom - visibleBounds.bottom)
    };
}

// Return the largest rectangle with the video's aspect ratio that fits inside
// the current client rectangle. This only shrinks one dimension, so a snapped
// window never grows outside its assigned screen/FancyZones region.
inline IntSize fit_inside_aspect(int clientWidth, int clientHeight, int videoWidth, int videoHeight) {
    if (clientWidth <= 0 || clientHeight <= 0 || videoWidth <= 0 || videoHeight <= 0)
        return {clientWidth, clientHeight};

    const std::int64_t lhs = static_cast<std::int64_t>(clientWidth) * videoHeight;
    const std::int64_t rhs = static_cast<std::int64_t>(clientHeight) * videoWidth;
    if (lhs == rhs) return {clientWidth, clientHeight};

    if (lhs > rhs) { // client is too wide: keep height, trim width.
        const auto rounded = (static_cast<std::int64_t>(clientHeight) * videoWidth + videoHeight / 2) / videoHeight;
        return {std::max(1, static_cast<int>(rounded)), clientHeight};
    }

    const auto rounded = (static_cast<std::int64_t>(clientWidth) * videoHeight + videoWidth / 2) / videoWidth;
    return {clientWidth, std::max(1, static_cast<int>(rounded))};
}

} // namespace hcv
