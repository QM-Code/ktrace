#include "alpha/sdk.hpp"
#include "beta/sdk.hpp"
#include "delta/sdk.hpp"
#include "ktrace/trace.hpp"

int main() {
    ktrace::RegisterChannel("app", ktrace::ResolveColor("BrightCyan"));
    ktrace::RegisterChannel("orchestrator", ktrace::ResolveColor("BrightYellow"));
    ktrace::EnableChannel("executable.app");
    ktrace::EnableChannel("executable.orchestrator");

    KTRACE("app", "executable initialized local trace channels");

    alpha::InitializeTraceLogging();
    beta::InitializeTraceLogging();
    delta::InitializeTraceLogging();

	/*
    ktrace::EnableChannel("alpha.net");
    ktrace::EnableChannel("alpha.cache");
    ktrace::EnableChannel("beta.io");
    ktrace::EnableChannel("beta.scheduler");
    ktrace::EnableChannel("delta.physics");
    ktrace::EnableChannel("delta.metrics");
	*/
	ktrace::SetOutputOptions({
	    .filenames = true,
	    .line_numbers = true,
	    .function_names = true,
	    .timestamps = true,
	});
    ktrace::EnableChannels("*.*");
	
    alpha::TestTraceLoggingChannels();
    beta::TestTraceLoggingChannels();
    delta::TestTraceLoggingChannels();

//    KTRACE("orchestrator", "executable completed imported SDK trace checks");
    return 0;
}
