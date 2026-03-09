#include <alpha/sdk.hpp>
#include <beta/sdk.hpp>
#include <gamma/sdk.hpp>
#include <ktrace.hpp>

#include <iostream>

int main(int argc, char** argv) {
    // Register channels.
    ktrace::RegisterChannel("app", ktrace::Color("BrightCyan"));
    ktrace::RegisterChannel("orchestrator", ktrace::Color("BrightYellow"));
    ktrace::RegisterChannel("deep");
    ktrace::RegisterChannel("deep.branch");
    ktrace::RegisterChannel("deep.branch.leaf", ktrace::Color("LightSalmon1"));

    // Enable and test a channel.
    ktrace::EnableChannel(".app");
    KTRACE("app", "omega initialized local trace channels");

    // Enable external library tracing.
    ktrace::Initialize();
    ktrace::demo::alpha::Init();
    ktrace::demo::beta::InitializeTraceLogging();
    ktrace::demo::gamma::SystemStartup();

    // Build and run the CLI parser after all trace channels are registered.
    kcli::PrimaryParser parser;
    parser.addInlineParser(ktrace::GetInlineParser("trace"));

    try {
        parser.parse(argc, argv);
    } catch (const kcli::CliError& ex) {
        std::cerr << "CLI error: " << ex.what() << "\n";
        return 2;
    }

    KTRACE("app", "cli processing enabled, use --trace for options");

    // Exercise imported SDK trace logging.
    KTRACE("app", "testing external tracing, use --trace '*.*' to view top-level channels");
    KTRACE("deep.branch.leaf", "omega trace test on channel 'deep.branch.leaf'");
    ktrace::demo::alpha::TestTraceLoggingChannels();
    ktrace::demo::beta::TestTraceLoggingChannels();
    ktrace::demo::gamma::TestTraceLoggingChannels();

    KTRACE("orchestrator", "omega completed imported SDK trace checks");

    return 0;
}
