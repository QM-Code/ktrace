#include <alpha/sdk.hpp>
#include <beta/sdk.hpp>
#include <gamma/sdk.hpp>
#include <ktrace.hpp>

int main(int argc, char** argv) {
    ktrace::Logger logger;

    ktrace::TraceLogger tracer("omega");
    tracer.addChannel("app", ktrace::Color("BrightCyan"));
    tracer.addChannel("orchestrator", ktrace::Color("BrightYellow"));
    tracer.addChannel("deep");
    tracer.addChannel("deep.branch");
    tracer.addChannel("deep.branch.leaf", ktrace::Color("LightSalmon1"));

    logger.addTraceLogger(tracer);

    const ktrace::TraceLogger alphaTracer = ktrace::demo::alpha::GetTraceLogger();
    const ktrace::TraceLogger betaTracer = ktrace::demo::beta::GetTraceLogger();
    const ktrace::TraceLogger gammaTracer = ktrace::demo::gamma::GetTraceLogger();

    logger.addTraceLogger(alphaTracer);
    logger.addTraceLogger(betaTracer);
    logger.addTraceLogger(gammaTracer);

    // Enable and test a channel.
    logger.enableChannel(tracer, ".app");
    tracer.trace("app", "omega initialized local trace channels");
    logger.disableChannel(tracer, ".app");

    // Build and run the CLI parser after all trace channels are registered.
    kcli::Parser parser;
    parser.addInlineParser(logger.makeInlineParser(tracer));

    parser.parseOrExit(argc, argv);

    tracer.trace("app", "cli processing enabled, use --trace for options");

    // Exercise imported SDK trace logging.
    tracer.trace("app", "testing external tracing, use --trace '*.*' to view top-level channels");
    tracer.trace("deep.branch.leaf", "omega trace test on channel 'deep.branch.leaf'");
    ktrace::demo::alpha::TestTraceLoggingChannels();
    ktrace::demo::beta::TestTraceLoggingChannels();
    ktrace::demo::gamma::TestTraceLoggingChannels();
    ktrace::demo::alpha::TestStandardLoggingChannels();
    //ktrace::demo::beta::TestStandardLoggingChannels();
    //ktrace::demo::gamma::TestStandardLoggingChannels();

    tracer.trace("orchestrator", "omega completed imported SDK trace checks");

    tracer.info("testing...");
    tracer.warn("testing...");
    tracer.error("testing...");

    return 0;
}
