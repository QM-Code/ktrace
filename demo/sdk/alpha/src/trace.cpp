#include <alpha/sdk.hpp>

#include <ktrace.hpp>

namespace ktrace::demo::alpha {

ktrace::TraceLogger GetTraceLogger() {
    ktrace::TraceLogger logger;
    logger.addChannel("net", ktrace::Color("DeepSkyBlue1"));
    logger.addChannel("net.alpha");
    logger.addChannel("net.beta");
    logger.addChannel("net.gamma");
    logger.addChannel("net.gamma.deep");
    logger.addChannel("cache", ktrace::Color("Gold3"));
    logger.addChannel("cache.gamma", ktrace::Color("Gold3"));
    logger.addChannel("cache.delta");
    logger.addChannel("cache.special", ktrace::Color("Red"));
    return logger;
}

void TestTraceLoggingChannels() {
    KTRACE("net", "testing...");
    KTRACE("net.alpha", "testing...");
    KTRACE("net.beta", "testing...");
    KTRACE("net.gamma", "testing...");
    KTRACE("net.gamma.deep", "testing...");
    KTRACE("cache", "testing...");
    KTRACE("cache.gamma", "testing...");
    KTRACE("cache.delta", "testing...");
    KTRACE("cache.special", "testing...");
}

void TestStandardLoggingChannels() {
    ktrace::Info("testing...");
    ktrace::Warn("testing...");
    ktrace::Error("testing...");
}

} // namespace ktrace::demo::alpha
