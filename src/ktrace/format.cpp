#include <ktrace.hpp>

#include "../ktrace.hpp"

#include <chrono>
#include <cstdio>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace {

std::once_flag g_color_init_once;
bool g_color_enabled = false;

const char* resolveChannelColorCode(const ktrace::detail::LoggerData& logger_data,
                                    std::string_view trace_namespace,
                                    std::string_view channel) {
    ktrace::detail::initializeColorSupport();
    if (!g_color_enabled) {
        return "";
    }

    if (const auto registered =
            ktrace::detail::resolveChannelColor(logger_data, trace_namespace, channel)) {
        return ktrace::detail::ansiColorCode(*registered);
    }
    return "";
}

const char* resolveSeverityColorCode(ktrace::detail::LogSeverity severity) {
    ktrace::detail::initializeColorSupport();
    if (!g_color_enabled) {
        return "";
    }

    switch (severity) {
        case ktrace::detail::LogSeverity::info:
            return "\x1b[36m";
        case ktrace::detail::LogSeverity::warning:
            return "\x1b[33m";
        case ktrace::detail::LogSeverity::error:
            return "\x1b[31m";
    }
    return "";
}

std::string_view severityLabel(ktrace::detail::LogSeverity severity) {
    switch (severity) {
        case ktrace::detail::LogSeverity::info:
            return "info";
        case ktrace::detail::LogSeverity::warning:
            return "warning";
        case ktrace::detail::LogSeverity::error:
            return "error";
    }
    return "info";
}

void appendCompactTimestamp(std::string& out) {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto since_epoch = now.time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
    auto micros =
        std::chrono::duration_cast<std::chrono::microseconds>(since_epoch - seconds).count();
    auto sec_count = seconds.count();
    if (micros < 0) {
        --sec_count;
        micros += 1000000;
    }

    char compact_buffer[48];
    std::snprintf(compact_buffer,
                  sizeof(compact_buffer),
                  "[%lld.%06lld]",
                  static_cast<long long>(sec_count),
                  static_cast<long long>(micros));
    out.append(compact_buffer);
}

std::string_view formatSourceLabel(std::string_view source_path) {
    const auto trim_after_marker = [&](std::string_view marker) {
        const std::size_t pos = source_path.rfind(marker);
        if (pos == std::string_view::npos) {
            return false;
        }
        source_path.remove_prefix(pos + marker.size());
        return true;
    };

    const auto trim_leading_prefix = [&](std::string_view prefix) {
        if (source_path.rfind(prefix, 0) != 0) {
            return false;
        }
        source_path.remove_prefix(prefix.size());
        return true;
    };

    if (!trim_after_marker("/src/") &&
        !trim_after_marker("\\src\\") &&
        !trim_after_marker("/include/") &&
        !trim_after_marker("\\include\\")) {
        if (!trim_leading_prefix("src/") &&
            !trim_leading_prefix("src\\") &&
            !trim_leading_prefix("include/")) {
            trim_leading_prefix("include\\");
        }
    }

    const std::size_t slash = source_path.find_last_of("/\\");
    const std::size_t dot = source_path.find_last_of('.');
    if (dot != std::string_view::npos && (slash == std::string_view::npos || dot > slash)) {
        source_path = source_path.substr(0, dot);
    }
    return source_path;
}

std::string_view formatFunctionLabel(std::string_view function_name) {
    const std::size_t paren = function_name.find('(');
    if (paren != std::string_view::npos) {
        function_name = function_name.substr(0, paren);
    }

    const std::size_t scope = function_name.rfind("::");
    if (scope != std::string_view::npos) {
        function_name.remove_prefix(scope + 2);
    }

    const std::size_t space = function_name.find_last_of(' ');
    if (space != std::string_view::npos) {
        function_name.remove_prefix(space + 1);
    }

    return function_name;
}

} // namespace

namespace ktrace::detail {

void initializeColorSupport() {
    std::call_once(g_color_init_once, []() {
#ifndef _WIN32
        g_color_enabled = (isatty(fileno(stdout)) != 0);
#else
        g_color_enabled = false;
#endif
    });
}

std::string buildTraceMessagePrefix(const LoggerData& logger_data,
                                    std::string_view trace_namespace,
                                    std::string_view channel,
                                    std::string_view source_file,
                                    int source_line,
                                    std::string_view function_name) {
    const std::string trace_namespace_label(trace_namespace);
    const char* color = resolveChannelColorCode(logger_data, trace_namespace, channel);
    const bool has_color = color && color[0] != '\0';

    const bool filenames_enabled = logger_data.filenames_enabled.load(std::memory_order_relaxed);
    const bool line_numbers_enabled =
        logger_data.line_numbers_enabled.load(std::memory_order_relaxed);
    const bool function_names_enabled =
        logger_data.function_names_enabled.load(std::memory_order_relaxed);
    const bool timestamps_enabled =
        logger_data.timestamps_enabled.load(std::memory_order_relaxed);

    std::string out;
    out.reserve(channel.size() + source_file.size() + function_name.size() +
                trace_namespace_label.size() + 24);
    if (!trace_namespace_label.empty()) {
        if (g_color_enabled) {
            out.append("\x1b[38;5;250m");
        }
        out.push_back('[');
        out.append(trace_namespace_label);
        out.push_back(']');
        if (g_color_enabled) {
            out.append("\x1b[0m");
        }
        out.push_back(' ');
    }

    if (timestamps_enabled) {
        if (g_color_enabled) {
            out.append("\x1b[38;5;245m");
        }
        appendCompactTimestamp(out);
        if (g_color_enabled) {
            out.append("\x1b[0m");
        }
        out.push_back(' ');
    }

    out.push_back('[');
    if (has_color) {
        out.append(color);
    }
    out.append(channel);
    if (has_color) {
        out.append("\x1b[0m");
    }
    out.push_back(']');

    if (filenames_enabled) {
        const std::string_view source_label = formatSourceLabel(source_file);
        out.push_back(' ');
        if (g_color_enabled) {
            out.append("\x1b[38;5;245m");
        }
        out.push_back('[');
        out.append(source_label.begin(), source_label.end());
        if (line_numbers_enabled && source_line > 0) {
            out.push_back(':');
            out.append(std::to_string(source_line));
        }
        const std::string_view function_label = formatFunctionLabel(function_name);
        if (function_names_enabled && !function_label.empty()) {
            out.push_back(':');
            out.append(function_label.begin(), function_label.end());
        }
        out.push_back(']');
        if (g_color_enabled) {
            out.append("\x1b[0m");
        }
    }
    return out;
}

std::string buildLogMessagePrefix(const LoggerData& logger_data,
                                  std::string_view trace_namespace,
                                  LogSeverity severity,
                                  std::string_view source_file,
                                  int source_line,
                                  std::string_view function_name) {
    const std::string trace_namespace_label(trace_namespace);
    const std::string_view severity_text = severityLabel(severity);
    const char* color = resolveSeverityColorCode(severity);
    const bool has_color = color && color[0] != '\0';

    const bool filenames_enabled = logger_data.filenames_enabled.load(std::memory_order_relaxed);
    const bool line_numbers_enabled =
        logger_data.line_numbers_enabled.load(std::memory_order_relaxed);
    const bool function_names_enabled =
        logger_data.function_names_enabled.load(std::memory_order_relaxed);
    const bool timestamps_enabled =
        logger_data.timestamps_enabled.load(std::memory_order_relaxed);

    std::string out;
    out.reserve(trace_namespace_label.size() + severity_text.size() + source_file.size() +
                function_name.size() + 24);
    if (!trace_namespace_label.empty()) {
        if (g_color_enabled) {
            out.append("\x1b[38;5;250m");
        }
        out.push_back('[');
        out.append(trace_namespace_label);
        out.push_back(']');
        if (g_color_enabled) {
            out.append("\x1b[0m");
        }
        out.push_back(' ');
    }

    if (timestamps_enabled) {
        if (g_color_enabled) {
            out.append("\x1b[38;5;245m");
        }
        appendCompactTimestamp(out);
        if (g_color_enabled) {
            out.append("\x1b[0m");
        }
        out.push_back(' ');
    }

    out.push_back('[');
    if (has_color) {
        out.append(color);
    }
    out.append(severity_text.begin(), severity_text.end());
    if (has_color) {
        out.append("\x1b[0m");
    }
    out.push_back(']');

    if (filenames_enabled) {
        const std::string_view source_label = formatSourceLabel(source_file);
        out.push_back(' ');
        if (g_color_enabled) {
            out.append("\x1b[38;5;245m");
        }
        out.push_back('[');
        out.append(source_label.begin(), source_label.end());
        if (line_numbers_enabled && source_line > 0) {
            out.push_back(':');
            out.append(std::to_string(source_line));
        }
        const std::string_view function_label = formatFunctionLabel(function_name);
        if (function_names_enabled && !function_label.empty()) {
            out.push_back(':');
            out.append(function_label.begin(), function_label.end());
        }
        out.push_back(']');
        if (g_color_enabled) {
            out.append("\x1b[0m");
        }
    }
    return out;
}

} // namespace ktrace::detail
