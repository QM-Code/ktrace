#include <alpha/sdk.hpp>

#include <ktrace.hpp>

namespace ktrace::demo::alpha {

ktrace::TraceLogger GetTraceLogger() {
    ktrace::TraceLogger logger;
    logger.addChannel("net", ktrace::Color("DeepSkyBlue1"));
    logger.addChannel("cache", ktrace::Color("Gold3"));
    return logger;
}

void TestTraceLoggingChannels() {
    KTRACE("net", "alpha trace test on channel 'net'");
    KTRACE("cache", "alpha trace test on channel 'cache'");
}

} // namespace ktrace::demo::alpha
