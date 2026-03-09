#include <gamma/sdk.hpp>

#include <ktrace.hpp>

namespace ktrace::demo::gamma {

ktrace::TraceLogger GetTraceLogger() {
    ktrace::TraceLogger logger;
    logger.addChannel("physics", ktrace::Color("MediumOrchid1"));
    logger.addChannel("metrics", ktrace::Color("LightSkyBlue1"));
    return logger;
}

void TestTraceLoggingChannels() {
    KTRACE("physics", "gamma trace test on channel 'physics'");
    KTRACE("metrics", "gamma trace test on channel 'metrics'");
}

} // namespace ktrace::demo::gamma
