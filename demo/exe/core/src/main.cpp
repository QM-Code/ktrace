#include <alpha/sdk.hpp>
#include <ktrace.hpp>

int main(int argc, char** argv) {
    ktrace::RegisterChannel("app", ktrace::Color("BrightCyan"));
    ktrace::RegisterChannel("startup", ktrace::Color("BrightYellow"));

    ktrace::EnableChannel(".app");
    KTRACE("app", "core initialized local trace channels");

    ktrace::Initialize();
    ktrace::demo::alpha::Init();

    ktrace::ProcessCLI(argc, argv, "trace");
    KTRACE("app", "cli processing enabled, use --trace for options");

    KTRACE("startup", "testing imported tracing, use --trace '*.*' to view imported channels");
    ktrace::demo::alpha::TestTraceLoggingChannels();

    return 0;
}
