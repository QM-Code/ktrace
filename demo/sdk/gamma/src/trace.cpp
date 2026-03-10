#include <gamma/sdk.hpp>

#include <ktrace.hpp>

namespace ktrace::demo::gamma {

ktrace::TraceLogger GetTraceLogger() {
    static ktrace::TraceLogger logger("gamma");
    static const bool initialized = []() {
        logger.addChannel("physics", ktrace::Color("MediumOrchid1"));
        logger.addChannel("metrics", ktrace::Color("LightSkyBlue1"));
        return true;
    }();
    (void)initialized;
    return logger;
}

void TestTraceLoggingChannels() {
    const ktrace::TraceLogger trace = GetTraceLogger();
    trace.trace("physics", "gamma trace test on channel 'physics'");
    trace.trace("metrics", "gamma trace test on channel 'metrics'");
}

} // namespace ktrace::demo::gamma
