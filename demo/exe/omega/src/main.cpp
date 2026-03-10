#include <alpha/sdk.hpp>
#include <beta/sdk.hpp>
#include <gamma/sdk.hpp>
#include <ktrace.hpp>

int main(int argc, char** argv) {
    ktrace::Logger logger;

    ktrace::TraceLogger tracer;
    tracer.addChannel("app", ktrace::Color("BrightCyan"));
    tracer.addChannel("orchestrator", ktrace::Color("BrightYellow"));
    tracer.addChannel("deep");
    tracer.addChannel("deep.branch");
    tracer.addChannel("deep.branch.leaf", ktrace::Color("LightSalmon1"));

    logger.addTraceLogger(tracer);

    ktrace::TraceLogger alphaTracer = ktrace::demo::alpha::GetTraceLogger();
    ktrace::TraceLogger betaTracer = ktrace::demo::beta::GetTraceLogger();
    ktrace::TraceLogger gammaTracer = ktrace::demo::gamma::GetTraceLogger();

    logger.addTraceLogger(alphaTracer);
    logger.addTraceLogger(betaTracer);
    logger.addTraceLogger(gammaTracer);

    logger.activate();

    // Enable and test a channel.
    logger.enableChannel(".app");
    KTRACE("app", "omega initialized local trace channels");
    logger.disableChannel(".app");

    // Build and run the CLI parser after all trace channels are registered.
    kcli::Parser parser;
    parser.addInlineParser(ktrace::GetInlineParser());

    parser.parseOrExit(argc, argv);

    KTRACE("app", "cli processing enabled, use --trace for options");

    // Exercise imported SDK trace logging.
    KTRACE("app", "testing external tracing, use --trace '*.*' to view top-level channels");
    KTRACE("deep.branch.leaf", "omega trace test on channel 'deep.branch.leaf'");
    ktrace::demo::alpha::TestTraceLoggingChannels();
    ktrace::demo::beta::TestTraceLoggingChannels();
    ktrace::demo::gamma::TestTraceLoggingChannels();
    ktrace::demo::alpha::TestStandardLoggingChannels();
    //ktrace::demo::beta::TestStandardLoggingChannels();
    //ktrace::demo::gamma::TestStandardLoggingChannels();

    KTRACE("orchestrator", "omega completed imported SDK trace checks");

    ktrace::Info("testing...");
    ktrace::Warn("testing...");
    ktrace::Error("testing...");

    return 0;
}
