#include <gamma/sdk.hpp>

#include <ktrace.hpp>

namespace {

bool g_gamma_trace_initialized = false;

} // namespace

namespace ktrace::demo::gamma {

void SystemStartup() {
    if (!g_gamma_trace_initialized) {
        ktrace::RegisterChannel("physics", ktrace::Color("MediumOrchid1"));
        ktrace::RegisterChannel("metrics", ktrace::Color("LightSkyBlue1"));
        g_gamma_trace_initialized = true;
    }
}

void TestTraceLoggingChannels() {
    SystemStartup();
    KTRACE("physics", "gamma trace test on channel 'physics'");
    KTRACE("metrics", "gamma trace test on channel 'metrics'");
}

} // namespace ktrace::demo::gamma
