#pragma once

#include <string>
#include <string_view>

namespace ktrace {

void EnableTraceChannels(const std::string& list);
void RegisterTraceNamespace(std::string_view trace_namespace);
const char* GetTraceNamespacesHelp();
const char* GetDefaultTraceChannelsHelp();
const char* GetTraceChannelsHelpForNamespace(std::string_view trace_namespace);
bool ShouldTraceChannel(std::string_view trace_namespace, const std::string& name);
#ifndef KTRACE_NAMESPACE
#error "KTRACE_NAMESPACE must be defined for trace logging."
#endif
inline bool ShouldTraceChannel(const std::string& name) {
    return ShouldTraceChannel(KTRACE_NAMESPACE, name);
}
void ConfigureLogPatterns(bool timestamp_logging);
void ConfigureOutput(bool filenames,
                     bool line_numbers,
                     bool function_names);

} // namespace ktrace
