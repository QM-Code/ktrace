#include "delta/sdk.hpp"

#include "ktrace.hpp"

namespace {

bool g_delta_trace_initialized = false;

} // namespace

namespace delta {

void SystemStartup() {
    if (!g_delta_trace_initialized) {
        ktrace::RegisterChannel("physics", ktrace::Color("MediumOrchid1"));
        ktrace::RegisterChannel("metrics", ktrace::Color("LightSkyBlue1"));
        g_delta_trace_initialized = true;
    }
}

void TestTraceLoggingChannels() {
    SystemStartup();
    KTRACE("physics", "delta trace test on channel 'physics'");
    KTRACE("metrics", "delta trace test on channel 'metrics'");
}

} // namespace delta
