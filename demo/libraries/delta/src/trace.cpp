#include "delta/sdk.hpp"

#include "ktrace/trace.hpp"

namespace {

bool g_delta_trace_initialized = false;

} // namespace

namespace delta {

void InitializeTraceLogging() {
    if (!g_delta_trace_initialized) {
        ktrace::RegisterChannel("physics", ktrace::ResolveColor("MediumOrchid1"));
        ktrace::RegisterChannel("metrics", ktrace::ResolveColor("LightSkyBlue1"));
        g_delta_trace_initialized = true;
    }
}

void TestTraceLoggingChannels() {
    InitializeTraceLogging();
    KTRACE("physics", "delta trace test on channel 'physics'");
    KTRACE("metrics", "delta trace test on channel 'metrics'");
}

} // namespace delta
