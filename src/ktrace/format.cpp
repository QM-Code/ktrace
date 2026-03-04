#include <ktrace.hpp>

#include "../ktrace.hpp"

#include <chrono>
#include <cstdio>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace {

const char* resolveCategoryColorCode(std::string_view trace_namespace, std::string_view category) {
    ktrace::detail::initializeColorSupport();

    auto& state = ktrace::detail::getTraceState();
    if (!state.color_enabled) {
        return "";
    }

    if (const auto registered = ktrace::detail::resolveChannelColor(trace_namespace, category)) {
        return ktrace::detail::ansiColorCode(*registered);
    }
    return "";
}

void appendCompactTimestamp(std::string& out) {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto since_epoch = now.time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(since_epoch - seconds).count();
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

} // namespace

namespace ktrace::detail {

void initializeColorSupport() {
    auto& state = getTraceState();
    std::call_once(state.color_init_once, [&state]() {
#ifndef _WIN32
        state.color_enabled = (isatty(fileno(stdout)) != 0);
#else
        state.color_enabled = false;
#endif
    });
}

std::string buildTraceMessagePrefix(std::string_view trace_namespace,
                                    std::string_view category,
                                    std::string_view source_file,
                                    int source_line,
                                    std::string_view function_name) {
    const std::string trace_namespace_label(trace_namespace);
    const char* color = resolveCategoryColorCode(trace_namespace, category);
    const bool has_color = color && color[0] != '\0';

    auto& state = getTraceState();
    const bool filenames_enabled = state.filenames_enabled.load(std::memory_order_relaxed);
    const bool line_numbers_enabled = state.line_numbers_enabled.load(std::memory_order_relaxed);
    const bool function_names_enabled = state.function_names_enabled.load(std::memory_order_relaxed);

    std::string out;
    out.reserve(category.size() + source_file.size() + function_name.size() + trace_namespace_label.size() + 24);
    if (!trace_namespace_label.empty()) {
        if (state.color_enabled) {
            out.append("\x1b[38;5;250m");
        }
        out.push_back('[');
        out.append(trace_namespace_label);
        out.push_back(']');
        if (state.color_enabled) {
            out.append("\x1b[0m");
        }
        out.push_back(' ');
    }

    if (state.timestamps_enabled.load(std::memory_order_relaxed)) {
        if (state.color_enabled) {
            out.append("\x1b[38;5;245m");
        }
        appendCompactTimestamp(out);
        if (state.color_enabled) {
            out.append("\x1b[0m");
        }
        out.push_back(' ');
    }

    out.push_back('[');
    if (has_color) {
        out.append(color);
    }
    out.append(category);
    if (has_color) {
        out.append("\x1b[0m");
    }
    out.push_back(']');

    if (filenames_enabled) {
        const std::string_view source_label = formatSourceLabel(source_file);
        out.push_back(' ');
        if (state.color_enabled) {
            out.append("\x1b[38;5;245m");
        }
        out.push_back('[');
        out.append(source_label.begin(), source_label.end());
        if (line_numbers_enabled && source_line > 0) {
            out.push_back(':');
            out.append(std::to_string(source_line));
        }
        if (function_names_enabled && !function_name.empty()) {
            out.push_back(':');
            out.append(function_name.begin(), function_name.end());
        }
        out.push_back(']');
        if (state.color_enabled) {
            out.append("\x1b[0m");
        }
    }
    return out;
}

} // namespace ktrace::detail
