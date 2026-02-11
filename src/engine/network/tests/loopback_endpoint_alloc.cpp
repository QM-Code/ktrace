#include "network/tests/loopback_endpoint_alloc.hpp"

#include <chrono>
#include <thread>

namespace karma::network::tests {

bool BindLoopbackEndpointDeterministic(uint16_t first_port,
                                       uint16_t last_port,
                                       uint32_t passes,
                                       uint32_t retry_sleep_ms,
                                       const std::function<bool(uint16_t)>& try_bind) {
    if (!try_bind || first_port >= last_port || passes == 0) {
        return false;
    }

    for (uint32_t pass = 0; pass < passes; ++pass) {
        for (uint16_t port = first_port; port < last_port; ++port) {
            if (try_bind(port)) {
                return true;
            }
        }
        if (pass + 1 < passes && retry_sleep_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_sleep_ms));
        }
    }
    return false;
}

} // namespace karma::network::tests
