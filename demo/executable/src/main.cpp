#include <alpha/sdk.hpp>
#include <beta/sdk.hpp>
#include <gamma/sdk.hpp>
#include <ktrace.hpp>

int main(int argc, char** argv) {

	// Register channels
    ktrace::RegisterChannel("app", ktrace::Color("BrightCyan"));
    ktrace::RegisterChannel("orchestrator", ktrace::Color("BrightYellow"));
    ktrace::RegisterChannel("deep");
    ktrace::RegisterChannel("deep.branch");
    ktrace::RegisterChannel("deep.branch.leaf", ktrace::Color("LightSalmon1"));

	// Enable and test a channel
    ktrace::EnableChannel(".app");
    KTRACE("app", "executable initialized local trace channels");

	// Enabling external library tracing
	ktrace::Initialize();
    ktrace::demo::alpha::Init();
    ktrace::demo::beta::InitializeTraceLogging();
    ktrace::demo::gamma::SystemStartup();

	// Process the CLI
	// Must happen after enabling external library tracing
	ktrace::ProcessCLI(argc,argv,"--trace");
    KTRACE("app", "cli processing enabled, use --trace for options");

	// Test external trace logging.
    KTRACE("app", "testing external tracing, use --trace '*.*' to view top-level channels");
    KTRACE("deep.branch.leaf", "executable trace test on channel 'deep.branch.leaf'");
    ktrace::demo::alpha::TestTraceLoggingChannels();
    ktrace::demo::beta::TestTraceLoggingChannels();
    ktrace::demo::gamma::TestTraceLoggingChannels();

	// Random shutdown note from internal logging.
    KTRACE("orchestrator", "executable completed imported SDK trace checks");

    return 0;
}
