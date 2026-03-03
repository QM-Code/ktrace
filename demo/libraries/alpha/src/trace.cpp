#include "alpha/sdk.hpp"

#include "ktrace.hpp"

namespace {

bool g_alpha_trace_initialized = false;

} // namespace

namespace alpha {

void Init() {
    if (!g_alpha_trace_initialized) {
        ktrace::RegisterChannel("net", ktrace::Color("DeepSkyBlue1"));
        ktrace::RegisterChannel("cache", ktrace::Color("Gold3"));
        g_alpha_trace_initialized = true;
    }
}

void TestTraceLoggingChannels() {
    Init();
    KTRACE("net", "alpha trace test on channel 'net'");
    KTRACE("cache", "alpha trace test on channel 'cache'");
}

} // namespace alpha
