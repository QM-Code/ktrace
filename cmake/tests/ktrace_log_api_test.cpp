#include <ktrace.hpp>

#include <spdlog/logger.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

[[noreturn]] void Fail(const std::string& message) {
    throw std::runtime_error(message);
}

void ExpectContains(const std::string& haystack, const std::string& needle) {
    if (haystack.find(needle) == std::string::npos) {
        Fail("expected output to contain '" + needle + "', got:\n" + haystack);
    }
}

} // namespace

int main() {
    std::ostringstream output;
    auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(output);
    auto logger = std::make_shared<spdlog::logger>("ktrace_log_api_test", sink);
    logger->set_level(spdlog::level::trace);
    logger->set_pattern("%v");
    spdlog::set_default_logger(logger);

    ktrace::SetOutputOptions({
        .filenames = true,
        .line_numbers = true,
        .function_names = false,
        .timestamps = false,
    });

    const int info_line = __LINE__ + 1;
    ktrace::log::Info("info message");
    const int warn_line = __LINE__ + 1;
    ktrace::log::Warn("warn value {}", 7);
    const int error_line = __LINE__ + 1;
    ktrace::log::Error(std::string("error message"));

    const std::string text = output.str();
    ExpectContains(text, "[tests] [info] ");
    ExpectContains(text, "[tests] [warning] ");
    ExpectContains(text, "[tests] [error] ");
    ExpectContains(text, "info message");
    ExpectContains(text, "warn value 7");
    ExpectContains(text, "error message");
    ExpectContains(text, "ktrace_log_api_test:" + std::to_string(info_line));
    ExpectContains(text, "ktrace_log_api_test:" + std::to_string(warn_line));
    ExpectContains(text, "ktrace_log_api_test:" + std::to_string(error_line));

    return 0;
}
