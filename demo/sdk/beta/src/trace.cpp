#include <beta/sdk.hpp>

#include <ktrace.hpp>

namespace ktrace::demo::beta {

ktrace::TraceLogger GetTraceLogger() {
    ktrace::TraceLogger logger;
    logger.addChannel("io", ktrace::Color("MediumSpringGreen"));
    logger.addChannel("scheduler", ktrace::Color("Orange3"));
    return logger;
}

void TestTraceLoggingChannels() {
    KTRACE("io", "beta trace test on channel 'io'");
    KTRACE("scheduler", "beta trace test on channel 'scheduler'");
}

} // namespace ktrace::demo::beta
