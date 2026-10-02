#pragma once

#include <atomic>

namespace hcv {

// One outstanding posted wakeup at most. The owner clears this when it begins
// handling the wakeup, allowing one later request while rendering is underway.
class CoalescedRequest {
public:
    bool try_queue() noexcept {
        bool expected = false;
        return queued_.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
    }

    void handled() noexcept { queued_.store(false, std::memory_order_release); }
    void cancel() noexcept { handled(); }

private:
    std::atomic<bool> queued_{false};
};

} // namespace hcv
