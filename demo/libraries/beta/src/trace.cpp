#include "beta/sdk.hpp"

#include "ktrace/trace.hpp"

namespace {

bool g_beta_trace_initialized = false;

} // namespace

namespace beta {

void InitializeTraceLogging() {
    if (!g_beta_trace_initialized) {
        ktrace::RegisterChannel("io", ktrace::ResolveColor("MediumSpringGreen"));
        ktrace::RegisterChannel("scheduler", ktrace::ResolveColor("Orange3"));
        g_beta_trace_initialized = true;
    }
}

void TestTraceLoggingChannels() {
    InitializeTraceLogging();
    KTRACE("io", "beta trace test on channel 'io'");
    KTRACE("scheduler", "beta trace test on channel 'scheduler'");
}

} // namespace beta
