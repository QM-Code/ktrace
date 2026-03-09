#include <alpha/sdk.hpp>
#include <ktrace.hpp>

#include <iostream>

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

    kcli::PrimaryParser parser;
    parser.addInlineParser(ktrace::GetInlineParser());

    try {
        parser.parse(argc, argv);
    } catch (const kcli::CliError& ex) {
        std::cerr << "CLI error: " << ex.what() << "\n";
        return 2;
    }

    KTRACE("app", "cli processing enabled, use --trace for options");

    KTRACE("startup", "testing imported tracing, use --trace '*.*' to view imported channels");
    ktrace::demo::alpha::TestTraceLoggingChannels();

    return 0;
}
