#include <ktrace.hpp>

#include "../ktrace.hpp"

namespace ktrace::detail {

bool ShouldTraceActive(std::string_view trace_namespace, std::string_view channel) {
    if (Logger* logger = GetActiveLogger()) {
        return logger->shouldTrace(trace_namespace, channel);
    }
    return false;
}

void TraceActive(std::string_view trace_namespace,
                 std::string_view channel,
                 std::string_view source_file,
                 int source_line,
                 std::string_view function_name,
                 std::string_view message) {
    if (Logger* logger = GetActiveLogger()) {
        logger->trace(trace_namespace, channel, source_file, source_line, function_name, message);
    }
}

void LogActive(std::string_view trace_namespace,
               LogSeverity severity,
               std::string_view source_file,
               int source_line,
               std::string_view function_name,
               std::string_view message) {
    if (Logger* logger = GetActiveLogger()) {
        logger->log(severity, trace_namespace, source_file, source_line, function_name, message);
    }
}

} // namespace ktrace::detail
