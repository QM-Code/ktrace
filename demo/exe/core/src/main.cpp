#include <alpha/sdk.hpp>
#include <ktrace.hpp>

int main(int argc, char** argv) {
    ktrace::Logger logger;

    ktrace::TraceLogger tracer("core");
    tracer.addChannel("app", ktrace::Color("BrightCyan"));
    tracer.addChannel("startup", ktrace::Color("BrightYellow"));

    const ktrace::TraceLogger alphaTracer = ktrace::demo::alpha::GetTraceLogger();

    logger.addTraceLogger(tracer);
    logger.addTraceLogger(alphaTracer);

    logger.enableChannel(tracer, ".app");
    tracer.trace("app", "core initialized local trace channels");

    kcli::Parser parser;
    parser.addInlineParser(logger.makeInlineParser(tracer));

    parser.parseOrExit(argc, argv);

    tracer.trace("app", "cli processing enabled, use --trace for options");

    tracer.trace("startup", "testing imported tracing, use --trace '*.*' to view imported channels");
    ktrace::demo::alpha::TestTraceLoggingChannels();

    return 0;
}
