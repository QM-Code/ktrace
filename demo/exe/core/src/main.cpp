#include <alpha/sdk.hpp>
#include <ktrace.hpp>

#include <iostream>

int main(int argc, char** argv) {
    ktrace::RegisterChannel("app", ktrace::Color("BrightCyan"));
    ktrace::RegisterChannel("startup", ktrace::Color("BrightYellow"));

    ktrace::EnableChannel(".app");
    KTRACE("app", "core initialized local trace channels");

    ktrace::Initialize();
    ktrace::demo::alpha::Init();

    kcli::PrimaryParser parser;
    parser.addInlineParser(ktrace::GetInlineParser("trace"));

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
