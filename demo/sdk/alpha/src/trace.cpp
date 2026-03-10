#include <alpha/sdk.hpp>

#include <ktrace.hpp>

namespace ktrace::demo::alpha {

ktrace::TraceLogger GetTraceLogger() {
    static ktrace::TraceLogger logger("alpha");
    static const bool initialized = []() {
        logger.addChannel("net", ktrace::Color("DeepSkyBlue1"));
        logger.addChannel("net.alpha");
        logger.addChannel("net.beta");
        logger.addChannel("net.gamma");
        logger.addChannel("net.gamma.deep");
        logger.addChannel("cache", ktrace::Color("Gold3"));
        logger.addChannel("cache.gamma", ktrace::Color("Gold3"));
        logger.addChannel("cache.delta");
        logger.addChannel("cache.special", ktrace::Color("Red"));
        return true;
    }();
    (void)initialized;
    return logger;
}

void TestTraceLoggingChannels() {
    const ktrace::TraceLogger trace = GetTraceLogger();
    trace.trace("net", "testing...");
    trace.trace("net.alpha", "testing...");
    trace.trace("net.beta", "testing...");
    trace.trace("net.gamma", "testing...");
    trace.trace("net.gamma.deep", "testing...");
    trace.trace("cache", "testing...");
    trace.trace("cache.gamma", "testing...");
    trace.trace("cache.delta", "testing...");
    trace.trace("cache.special", "testing...");
}

void TestStandardLoggingChannels() {
    const ktrace::TraceLogger trace = GetTraceLogger();
    trace.info("testing...");
    trace.warn("testing...");
    trace.error("testing...");
}

} // namespace ktrace::demo::alpha
