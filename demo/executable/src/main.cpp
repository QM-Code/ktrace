#include "alpha/sdk.hpp"
#include "beta/sdk.hpp"
#include "delta/sdk.hpp"
#include "ktrace/trace.hpp"

int main(int argc, char** argv) {

	// Register channels
    ktrace::RegisterChannel("app", ktrace::Color("BrightCyan"));
    ktrace::RegisterChannel("orchestrator", ktrace::Color("BrightYellow"));

	// Enable and test a channel
    ktrace::EnableChannel(".app");
    KTRACE("app", "executable initialized local trace channels");

	// Enabling external library tracing
	ktrace::EnableInternalTrace();
    alpha::Init();
    beta::InitializeTraceLogging();
    delta::SystemStartup();

	// Process the CLI
	// Must happen after enabling external library tracing
	ktrace::ProcessCLI(argc,argv,"--trace");
    KTRACE("app", "cli processing enabled, use --trace for options");

	// Test external trace logging.
    KTRACE("app", "testing external tracing, use --trace '*.*' to view top-level channels");
    alpha::TestTraceLoggingChannels();
    beta::TestTraceLoggingChannels();
    delta::TestTraceLoggingChannels();

	// Random shutdown note from internal logging.
    KTRACE("orchestrator", "executable completed imported SDK trace checks");

    return 0;
}
