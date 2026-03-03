#include "beta/sdk.hpp"

#include "ktrace.hpp"

namespace {

bool g_beta_trace_initialized = false;

} // namespace

namespace ktrace::demo::beta {

void InitializeTraceLogging() {
    if (!g_beta_trace_initialized) {
        ktrace::RegisterChannel("io", ktrace::Color("MediumSpringGreen"));
        ktrace::RegisterChannel("scheduler", ktrace::Color("Orange3"));
        g_beta_trace_initialized = true;
    }
}

void TestTraceLoggingChannels() {
    InitializeTraceLogging();
    KTRACE("io", "beta trace test on channel 'io'");
    KTRACE("scheduler", "beta trace test on channel 'scheduler'");
}

} // namespace ktrace::demo::beta
