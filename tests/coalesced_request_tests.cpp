#include "coalesced_request.hpp"
#include <cassert>
#include <iostream>

int main() {
    hcv::CoalescedRequest request;
    assert(request.try_queue());
    assert(!request.try_queue());
    assert(!request.try_queue());
    request.handled();
    assert(request.try_queue());
    request.cancel();
    assert(request.try_queue());
    std::cout << "COALESCED_REQUEST_TESTS_PASS\n";
}
