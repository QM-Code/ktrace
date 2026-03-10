#include <ktrace.hpp>

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

[[noreturn]] void Fail(const std::string& message) {
    std::cerr << "ktrace_channel_semantics_test failed: " << message << "\n";
    throw std::runtime_error(message);
}

void ExpectTrue(const bool condition, const std::string& message) {
    if (!condition) {
        Fail(message);
    }
}

void ExpectFalse(const bool condition, const std::string& message) {
    if (condition) {
        Fail(message);
    }
}

void AddTestChannels(ktrace::Logger& logger) {
    ktrace::TraceLogger tracer("tests");
    tracer.addChannel("net");
    tracer.addChannel("cache");
    tracer.addChannel("store");
    tracer.addChannel("store.requests");
    logger.addTraceLogger(tracer);
}

void VerifyExplicitOnOffSemantics() {
    ktrace::Logger logger;
    AddTestChannels(logger);

    logger.enableChannels("tests.*");
    ExpectTrue(logger.shouldTraceChannel("tests.net"), "tests.net should be enabled by tests.*");
    ExpectTrue(logger.shouldTraceChannel("tests.cache"), "tests.cache should be enabled by tests.*");

    logger.disableChannels("tests.*");
    ExpectFalse(logger.shouldTraceChannel("tests.net"), "tests.net should be disabled by tests.*");
    ExpectFalse(logger.shouldTraceChannel("tests.cache"), "tests.cache should be disabled by tests.*");

    logger.enableChannel("tests.net");
    ExpectTrue(logger.shouldTraceChannel("tests.net"),
               "explicit enable should turn tests.net back on after broad disable");
    ExpectFalse(logger.shouldTraceChannel("tests.cache"),
                "tests.cache should stay off after explicit enable of tests.net");

    logger.disableChannel("tests.net");
    ExpectFalse(logger.shouldTraceChannel("tests.net"),
                "explicit disable should turn tests.net back off");
}

void VerifyRegisteredChannelSemantics() {
    ktrace::Logger logger;
    AddTestChannels(logger);

    logger.enableChannels("*.*.*");
    ExpectTrue(logger.shouldTraceChannel("tests.store.requests"),
               "tests.store.requests should trace when explicitly registered and enabled");
    ExpectTrue(logger.shouldTraceChannel("tests.net"),
               "tests.net should trace when *.*.* enables channels up to depth 2");
    ExpectFalse(logger.shouldTraceChannel("tests.bad name"),
                "invalid runtime channel names should not trace");

    logger.enableChannel("tests.missing.child");
    ExpectFalse(logger.shouldTraceChannel("tests.missing.child"),
                "enableChannel should ignore unregistered exact channels");

    logger.enableChannels("tests.missing.child");
    ExpectFalse(logger.shouldTraceChannel("tests.missing.child"),
                "enableChannels should ignore unresolved exact selectors");
}

void VerifyTraceLoggerMergeSemantics() {
    ktrace::Logger logger;

    ktrace::TraceLogger first("tests");
    first.addChannel("net");
    logger.addTraceLogger(first);

    ktrace::TraceLogger duplicate("tests");
    duplicate.addChannel("net");
    logger.addTraceLogger(duplicate);

    ktrace::TraceLogger explicit_color("tests");
    explicit_color.addChannel("net", ktrace::Color("Gold3"));
    logger.addTraceLogger(explicit_color);

    ktrace::TraceLogger conflicting_color("tests");
    conflicting_color.addChannel("net", ktrace::Color("Orange3"));

    bool threw = false;
    try {
        logger.addTraceLogger(conflicting_color);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    ExpectTrue(threw, "conflicting explicit channel colors should be rejected");
}

} // namespace

int main() {
    VerifyExplicitOnOffSemantics();
    VerifyRegisteredChannelSemantics();
    VerifyTraceLoggerMergeSemantics();
    return 0;
}
