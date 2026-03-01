#include "trace.hpp"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool startsWith(const std::string_view value, const std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::string normalizeTraceRoot(std::string_view trace_root) {
    std::string root = ktrace::detail::trimWhitespace(std::string(trace_root));
    if (root.empty()) {
        throw std::invalid_argument("trace root must not be empty");
    }
    if (startsWith(root, "--")) {
        if (root.size() == 2) {
            throw std::invalid_argument("trace root '--' is invalid");
        }
        return root;
    }
    if (root[0] == '-') {
        throw std::invalid_argument("trace root '" + root + "' must begin with '--' or no dashes");
    }
    return std::string("--") + root;
}

bool hasUsableValueToken(const int index, const int argc, char** argv) {
    if (index + 1 >= argc || argv == nullptr) {
        return false;
    }
    const char* raw = argv[index + 1];
    if (raw == nullptr) {
        return false;
    }
    const std::string_view value(raw);
    if (value.empty()) {
        return false;
    }
    return value[0] != '-';
}

std::string normalizeSelectorList(std::string_view raw_value) {
    std::stringstream ss{std::string(raw_value)};
    std::string token;
    std::vector<std::string> tokens;

    while (std::getline(ss, token, ',')) {
        const std::string trimmed = ktrace::detail::trimWhitespace(token);
        if (!trimmed.empty()) {
            tokens.push_back(trimmed);
        }
    }

    std::ostringstream out;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << tokens[i];
    }
    return out.str();
}

void enableSelectorListOrThrow(const std::string_view option, const std::string_view raw_value) {
    const std::string selectors = normalizeSelectorList(raw_value);
    if (selectors.empty()) {
        throw std::invalid_argument("option '" + std::string(option) + "' requires one or more selectors");
    }
    ktrace::EnableChannels(selectors);
    KTRACE("api.cli", "option '{}' enabled selectors '{}'", option, selectors);
}

void printTraceHelp(const std::string& root) {
    std::cout
        << "\nTrace logging options:\n"
        << "  " << root << " <selectors>        Enable selectors\n"
        << "  " << root << "-examples           Show selector examples\n"
        << "  " << root << "-namespaces         Show initialized trace namespaces\n"
        << "  " << root << "-colors             Show available trace colors\n"
        << "  " << root << "-filenames          Include source filename in trace output\n"
        << "  " << root << "-line-numbers       Include source line number (requires filenames)\n"
        << "  " << root << "-function-names     Include function name (requires filenames)\n"
        << "  " << root << "-timestamps         Include timestamps in trace output\n"
        << "  " << root << "-help               Show this help\n\n";
}

void printTraceExamples(const std::string& root) {
    std::cout
        << "\nGeneral trace selector pattern:\n"
        << "  " << root << " <namespace>.<channel>[.<subchannel>[.<subchannel>]]\n\n"
        << "Trace selector examples:\n"
        << "  " << root << " '*.*'            Select all <namespace>.<channel> channels\n"
        << "  " << root << " '*.*.*'          Select all channels up to 3 levels\n"
        << "  " << root << " 'alpha.*'        Select all top-level channels in alpha\n"
        << "  " << root << " 'alpha.*.*'      Select all channels in alpha (up to 3 levels)\n"
        << "  " << root << " '*.net'          Select 'net' across all namespaces\n"
        << "  " << root << " '*.scheduler.tick' Select 'scheduler.tick' across all namespaces\n"
        << "  " << root << " '*.net.*'        Select subchannels under 'net' across namespaces\n"
        << "  " << root << " '*.{net,io}'     Select 'net' and 'io' across all namespaces\n"
        << "  " << root << " '{alpha,beta}.*' Select all top-level channels in alpha and beta\n"
        << "  " << root << " alpha.net\n"
        << "  " << root << " beta.scheduler.tick\n"
        << "  " << root << " alpha.net,beta.io\n"
        << "  " << root << " delta.physics.*\n"
        << "  " << root << " delta.physics.*.*\n"
        << "  " << root << " alpha.{net,cache}\n"
        << "  " << root << " beta.{io,scheduler}.packet\n"
        << "  " << root << " '{alpha,beta}.net'\n\n";
}

void printTraceNamespaces() {
    const std::vector<std::string> namespaces = ktrace::GetNamespaces();
    if (namespaces.empty()) {
        std::cout << "No trace namespaces defined.\n\n";
        return;
    }

    std::cout << "Available trace namespaces:\n";
    for (const std::string& trace_namespace : namespaces) {
        if (trace_namespace.empty()) {
            continue;
        }
        std::cout << "  " << trace_namespace << "\n";
    }
    std::cout << "\n";
}

void printTraceColors() {
    const auto& names = ktrace::detail::colorNames();
    std::cout << "Available trace colors:\n";
    for (const std::string_view color_name : names) {
        if (color_name.empty()) {
            continue;
        }
        std::cout << "  " << color_name << "\n";
    }
    std::cout << "\n";
}

void processCliArgs(int& argc, char** argv, std::string_view trace_root) {
    if (argc <= 0 || argv == nullptr) {
        return;
    }

    ktrace::detail::ensureInternalTraceChannelsRegistered();
    const std::string root = normalizeTraceRoot(trace_root);
    const std::string root_help = root + "-help";
    const std::string root_examples = root + "-examples";
    const std::string root_namespaces = root + "-namespaces";
    const std::string root_colors = root + "-colors";
    const std::string root_filenames = root + "-filenames";
    const std::string root_line_numbers = root + "-line-numbers";
    const std::string root_function_names = root + "-function-names";
    const std::string root_timestamps = root + "-timestamps";

    ktrace::OutputOptions output_options{};
    bool saw_output_option = false;
    std::vector<bool> consumed(static_cast<std::size_t>(argc), false);
    KTRACE("api",
           "processing CLI options (enable api.cli for details): {} arg(s), root '{}'",
           argc,
           root);

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr) {
            continue;
        }
        const std::string arg = ktrace::detail::trimWhitespace(std::string(argv[i]));
        if (arg.empty() || !startsWith(arg, root)) {
            continue;
        }

        if (arg == root_help) {
            consumed[static_cast<std::size_t>(i)] = true;
            printTraceHelp(root);
            KTRACE("api.cli", "handled '{}'", root_help);
            continue;
        }

        if (arg == root_examples) {
            consumed[static_cast<std::size_t>(i)] = true;
            printTraceExamples(root);
            KTRACE("api.cli", "handled '{}'", root_examples);
            continue;
        }

        if (arg == root_namespaces) {
            consumed[static_cast<std::size_t>(i)] = true;
            printTraceNamespaces();
            KTRACE("api.cli", "handled '{}'", root_namespaces);
            continue;
        }

        if (arg == root_colors) {
            consumed[static_cast<std::size_t>(i)] = true;
            printTraceColors();
            KTRACE("api.cli", "handled '{}'", root_colors);
            continue;
        }

        if (arg == root) {
            consumed[static_cast<std::size_t>(i)] = true;
            if (!hasUsableValueToken(i, argc, argv)) {
                printTraceHelp(root);
                KTRACE("api", "cli: '{}' with no value, showed help", root);
                continue;
            }
            consumed[static_cast<std::size_t>(i + 1)] = true;
            try {
                enableSelectorListOrThrow(root, argv[++i]);
            } catch (const std::exception& ex) {
                spdlog::error("\nTrace option error: {}", ex.what());
                printTraceExamples(root);
            }
            continue;
        }

        if (arg == root_filenames) {
            consumed[static_cast<std::size_t>(i)] = true;
            output_options.filenames = true;
            saw_output_option = true;
            KTRACE("api.cli", "enabled '{}' output option", root_filenames);
            continue;
        }
        if (arg == root_line_numbers) {
            consumed[static_cast<std::size_t>(i)] = true;
            output_options.line_numbers = true;
            saw_output_option = true;
            KTRACE("api.cli", "enabled '{}' output option", root_line_numbers);
            continue;
        }
        if (arg == root_function_names) {
            consumed[static_cast<std::size_t>(i)] = true;
            output_options.function_names = true;
            saw_output_option = true;
            KTRACE("api.cli", "enabled '{}' output option", root_function_names);
            continue;
        }
        if (arg == root_timestamps) {
            consumed[static_cast<std::size_t>(i)] = true;
            output_options.timestamps = true;
            saw_output_option = true;
            KTRACE("api.cli", "enabled '{}' output option", root_timestamps);
            continue;
        }

        consumed[static_cast<std::size_t>(i)] = true;
        KTRACE("api", "cli: consumed unknown trace option '{}'", arg);
        continue;
    }

    if (saw_output_option) {
        ktrace::SetOutputOptions(output_options);
    }

    int write_index = 1;
    for (int read_index = 1; read_index < argc; ++read_index) {
        if (!consumed[static_cast<std::size_t>(read_index)]) {
            argv[write_index++] = argv[read_index];
        }
    }
    if (write_index < argc) {
        argv[write_index] = nullptr;
    }
    argc = write_index;
}

} // namespace

namespace ktrace {

void ProcessCLI(int& argc, char** argv, std::string_view trace_root) {
    try {
        processCliArgs(argc, argv, trace_root);
    } catch (const std::exception& ex) {
        std::string root = "--trace";
        try {
            root = normalizeTraceRoot(trace_root);
        } catch (...) {
        }
        spdlog::error("\nTrace option error: {}", ex.what());
        printTraceExamples(root);
    }
}

} // namespace ktrace
