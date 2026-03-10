#include <beta/sdk.hpp>

#include <ktrace.hpp>

namespace ktrace::demo::beta {

ktrace::TraceLogger GetTraceLogger() {
    static ktrace::TraceLogger logger("beta");
    static const bool initialized = []() {
        logger.addChannel("io", ktrace::Color("MediumSpringGreen"));
        logger.addChannel("scheduler", ktrace::Color("Orange3"));
        return true;
    }();
    (void)initialized;
    return logger;
}

void TestTraceLoggingChannels() {
    const ktrace::TraceLogger trace = GetTraceLogger();
    trace.trace("io", "beta trace test on channel 'io'");
    trace.trace("scheduler", "beta trace test on channel 'scheduler'");
}

} // namespace ktrace::demo::beta
