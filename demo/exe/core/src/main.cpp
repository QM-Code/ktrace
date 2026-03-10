#include <alpha/sdk.hpp>
#include <ktrace.hpp>

int main(int argc, char** argv) {
    ktrace::Logger logger;

    ktrace::TraceLogger tracer;
    tracer.addChannel("app", ktrace::Color("BrightCyan"));
    tracer.addChannel("startup", ktrace::Color("BrightYellow"));

    ktrace::TraceLogger alphaTracer = ktrace::demo::alpha::GetTraceLogger();

    logger.addTraceLogger(tracer);
    logger.addTraceLogger(alphaTracer);
    logger.activate();

    logger.enableChannel(".app");
    KTRACE("app", "core initialized local trace channels");

    kcli::Parser parser;
    parser.addInlineParser(ktrace::GetInlineParser());

    parser.parseOrExit(argc, argv);

    KTRACE("app", "cli processing enabled, use --trace for options");

    KTRACE("startup", "testing imported tracing, use --trace '*.*' to view imported channels");
    ktrace::demo::alpha::TestTraceLoggingChannels();

    return 0;
}
