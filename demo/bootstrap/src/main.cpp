#include <ktrace.hpp>

#include <iostream>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    ktrace::Logger logger;
    ktrace::TraceLogger tracer;
    tracer.addChannel("bootstrap", ktrace::Color("BrightGreen"));
    logger.addTraceLogger(tracer);
    logger.activate();
    logger.enableChannel(".bootstrap");
    KTRACE("bootstrap", "ktrace bootstrap compile/link check");

    std::cout << "Bootstrap succeeded.\n";
    return 0;
}
