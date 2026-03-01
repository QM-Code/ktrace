#include "alpha/sdk.hpp"

#include "ktrace/trace.hpp"

namespace {

bool g_alpha_trace_initialized = false;

} // namespace

namespace alpha {

void InitializeTraceLogging() {
    if (!g_alpha_trace_initialized) {
        ktrace::RegisterChannel("net", ktrace::ResolveColor("DeepSkyBlue1"));
        ktrace::RegisterChannel("cache", ktrace::ResolveColor("Gold3"));
        g_alpha_trace_initialized = true;
    }
}

void TestTraceLoggingChannels() {
    InitializeTraceLogging();
    KTRACE("net", "alpha trace test on channel 'net'");
    KTRACE("cache", "alpha trace test on channel 'cache'");
}

} // namespace alpha
