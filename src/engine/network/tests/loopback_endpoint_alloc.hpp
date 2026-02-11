#pragma once

#include <functional>
#include <cstdint>

namespace karma::network::tests {

bool BindLoopbackEndpointDeterministic(uint16_t first_port,
                                       uint16_t last_port,
                                       uint32_t passes,
                                       uint32_t retry_sleep_ms,
                                       const std::function<bool(uint16_t)>& try_bind);

} // namespace karma::network::tests
