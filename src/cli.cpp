#include "trace.hpp"

#include <cstddef>
#include <iostream>
#include <sstream>
#include <stdexcept>
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

bool hasUsableValueToken(const std::size_t index, const std::vector<std::string_view>& argv) {
    if (index + 1 >= argv.size()) {
        return false;
    }
    const std::string_view value = argv[index + 1];
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
}

void printTraceHelp(const std::string& root) {
    std::cout
        << "Trace logging options:\n"
        << "  " << root << " <selectors>        Enable selectors\n"
        << "  " << root << "-examples           Show selector examples\n"
        << "  " << root << "-namespaces         Show initialized trace namespaces\n"
        << "  " << root << "-filenames          Include source filename in trace output\n"
        << "  " << root << "-line-numbers       Include source line number (requires filenames)\n"
        << "  " << root << "-function-names     Include function name (requires filenames)\n"
        << "  " << root << "-timestamps         Include timestamps in trace output\n"
        << "  " << root << "-help               Show this help\n\n";
}

void printTraceExamples(const std::string& root) {
    std::cout
        << "Trace selector examples:\n"
        << "  " << root << " demo.cli\n"
        << "  " << root << " demo.cli.trace\n"
        << "  " << root << " demo.cli,demo.cli.trace\n"
        << "  " << root << " demo.renderer.*\n"
        << "  " << root << " demo.renderer.*.*\n"
        << "  " << root << " demo.{net,io}\n"
        << "  " << root << " demo.{net,io}.packet\n"
        << "  " << root << " '{lib1,lib2}.net'\n"
        << "  " << root << " '*.*'\n"
        << "  " << root << " '*.*.*' " << root << "-filenames\n"
        << "  " << root << " <namespace>.<channel>[.<sub>[.<sub>]]\n\n";
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

std::vector<std::string_view> collectArgv(const int argc, char** argv) {
    std::vector<std::string_view> out;
    if (argc <= 0 || argv == nullptr) {
        return out;
    }
    out.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == nullptr) {
            continue;
        }
        out.emplace_back(argv[i]);
    }
    return out;
}

void processCliArgs(const std::vector<std::string_view>& argv, std::string_view trace_root) {
    if (argv.empty()) {
        return;
    }

    const std::string root = normalizeTraceRoot(trace_root);
    const std::string root_help = root + "-help";
    const std::string root_examples = root + "-examples";
    const std::string root_namespaces = root + "-namespaces";
    const std::string root_filenames = root + "-filenames";
    const std::string root_line_numbers = root + "-line-numbers";
    const std::string root_function_names = root + "-function-names";
    const std::string root_timestamps = root + "-timestamps";

    ktrace::OutputOptions output_options{};
    bool saw_output_option = false;

    for (std::size_t i = 0; i < argv.size(); ++i) {
        const std::string arg = ktrace::detail::trimWhitespace(std::string(argv[i]));
        if (arg.empty() || !startsWith(arg, root)) {
            continue;
        }

        if (arg == root_help) {
            printTraceHelp(root);
            continue;
        }

        if (arg == root_examples) {
            printTraceExamples(root);
            continue;
        }

        if (arg == root_namespaces) {
            printTraceNamespaces();
            continue;
        }

        if (arg == root) {
            if (!hasUsableValueToken(i, argv)) {
                printTraceHelp(root);
                continue;
            }
            enableSelectorListOrThrow(root, argv[++i]);
            continue;
        }

        if (arg == root_filenames) {
            output_options.filenames = true;
            saw_output_option = true;
            continue;
        }
        if (arg == root_line_numbers) {
            output_options.line_numbers = true;
            saw_output_option = true;
            continue;
        }
        if (arg == root_function_names) {
            output_options.function_names = true;
            saw_output_option = true;
            continue;
        }
        if (arg == root_timestamps) {
            output_options.timestamps = true;
            saw_output_option = true;
            continue;
        }

        throw std::invalid_argument("unknown trace option '" + arg + "'");
    }

    if (saw_output_option) {
        ktrace::SetOutputOptions(output_options);
    }
}

} // namespace

namespace ktrace {

void ProcessCLI(int argc, char** argv, std::string_view trace_root) {
    processCliArgs(collectArgv(argc, argv), trace_root);
}

} // namespace ktrace
