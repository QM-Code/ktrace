#include <ktrace.hpp>

#include "../ktrace.hpp"

#include <kcli.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool startsWith(const std::string_view value, const std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::string normalizeTraceRootName(std::string_view trace_root) {
    std::string root = ktrace::detail::trimWhitespace(std::string(trace_root));
    if (root.empty()) {
        throw std::invalid_argument("trace root must not be empty");
    }
    if (startsWith(root, "--")) {
        if (root.size() == 2) {
            throw std::invalid_argument("trace root '--' is invalid");
        }
        root.erase(0, 2);
        return root;
    }
    if (root[0] == '-') {
        throw std::invalid_argument("trace root '" + root + "' must begin with '--' or no dashes");
    }
    return root;
}

std::string buildTraceRootOption(std::string_view trace_root) {
    return std::string("--") + normalizeTraceRootName(trace_root);
}

bool hasUsableValueToken(const int index,
                         const int argc,
                         char** argv,
                         const std::vector<bool>* consumed = nullptr) {
    if (index + 1 >= argc || argv == nullptr) {
        return false;
    }
    if (consumed != nullptr &&
        static_cast<std::size_t>(index + 1) < consumed->size() &&
        (*consumed)[static_cast<std::size_t>(index + 1)]) {
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

void enableSelectorListOrThrow(const std::string_view option,
                               const std::string_view raw_value,
                               const std::string_view local_namespace) {
    const std::string selectors = ktrace::detail::trimWhitespace(std::string(raw_value));
    if (selectors.empty()) {
        throw std::invalid_argument("option '" + std::string(option) + "' requires one or more selectors");
    }
    ktrace::EnableChannels(selectors, local_namespace);
    KTRACE("api.cli", "option '{}' enabled selectors '{}'", option, selectors);
}

void printTraceHelp(const std::string& root) {
    std::cout
        << "\nTrace logging options:\n"
        << "  " << root << " <selector>         Enable trace channel(s) (may pass more than once)\n"
        << "  " << root << "-examples           Show selector examples\n"
        << "  " << root << "-namespaces         Show initialized trace namespaces\n"
        << "  " << root << "-channels           Show initialized trace channels\n"
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
        << "  " << root << " '.abc'           Select local 'abc' in current namespace\n"
        << "  " << root << " '.abc.xyz'       Select local nested channel in current namespace\n"
        << "  " << root << " 'otherapp.channel' Select explicit namespace channel\n"
        << "  " << root << " '*.*'            Select all <namespace>.<channel> channels\n"
        << "  " << root << " '*.*.*'          Select all channels up to 2 levels\n"
        << "  " << root << " '*.*.*.*'        Select all channels up to 3 levels\n"
        << "  " << root << " 'alpha.*'        Select all top-level channels in alpha\n"
        << "  " << root << " 'alpha.*.*'      Select all channels in alpha (up to 2 levels)\n"
        << "  " << root << " 'alpha.*.*.*'    Select all channels in alpha (up to 3 levels)\n"
        << "  " << root << " '*.net'          Select 'net' across all namespaces\n"
        << "  " << root << " '*.scheduler.tick' Select 'scheduler.tick' across all namespaces\n"
        << "  " << root << " '*.net.*'        Select subchannels under 'net' across namespaces\n"
        << "  " << root << " '*.{net,io}'     Select 'net' and 'io' across all namespaces\n"
        << "  " << root << " '{alpha,beta}.*' Select all top-level channels in alpha and beta\n"
        << "  " << root << " alpha.net\n"
        << "  " << root << " beta.scheduler.tick\n"
        << "  " << root << " alpha.net,beta.io\n"
        << "  " << root << " gamma.physics.*\n"
        << "  " << root << " gamma.physics.*.*\n"
        << "  " << root << " alpha.{net,cache}\n"
        << "  " << root << " beta.{io,scheduler}.packet\n"
        << "  " << root << " '{alpha,beta}.net'\n\n";
}

void printTraceNamespaces() {
    std::vector<std::string> namespaces = ktrace::GetNamespaces();
    if (namespaces.empty()) {
        std::cout << "No trace namespaces defined.\n\n";
        return;
    }

    std::sort(namespaces.begin(), namespaces.end());
    std::cout << "\nAvailable trace namespaces:\n";
    for (const std::string& trace_namespace : namespaces) {
        if (trace_namespace.empty()) {
            continue;
        }
        std::cout << "  " << trace_namespace << "\n";
    }
    std::cout << "\n";
}

void printTraceChannels() {
    std::vector<std::string> namespaces = ktrace::GetNamespaces();
    std::sort(namespaces.begin(), namespaces.end());

    bool printed_any = false;
    for (const std::string& trace_namespace : namespaces) {
        if (trace_namespace.empty()) {
            continue;
        }
        std::vector<std::string> channels = ktrace::GetChannels(trace_namespace);
        std::sort(channels.begin(), channels.end());
        for (const std::string& channel : channels) {
            if (channel.empty()) {
                continue;
            }
            if (!printed_any) {
                std::cout << "\nAvailable trace channels:\n";
                printed_any = true;
            }
            std::cout << "  " << trace_namespace << "." << channel << "\n";
        }
    }

    if (!printed_any) {
        std::cout << "No trace channels defined.\n\n";
        return;
    }
    std::cout << "\n";
}

void printTraceColors() {
    const auto& names = ktrace::detail::colorNames();
    std::cout << "\nAvailable trace colors:\n";
    for (const std::string_view color_name : names) {
        if (color_name.empty()) {
            continue;
        }
        std::cout << "  " << color_name << "\n";
    }
    std::cout << "\n";
}

void compactArgv(int& argc, char** argv, const std::vector<bool>& consumed) {
    if (argc <= 0 || argv == nullptr) {
        return;
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

void processCliArgs(int& argc,
                    char** argv,
                    std::string_view trace_root,
                    std::string_view local_namespace) {
    if (argc <= 0 || argv == nullptr) {
        return;
    }

    ktrace::detail::ensureInternalTraceChannelsRegistered();
    const std::string root_name = normalizeTraceRootName(trace_root);
    const std::string root = std::string("--") + root_name;
    const std::string root_help = root + "-help";
    const std::string root_examples = root + "-examples";
    const std::string root_namespaces = root + "-namespaces";
    const std::string root_channels = root + "-channels";
    const std::string root_colors = root + "-colors";
    const std::string root_filenames = root + "-filenames";
    const std::string root_line_numbers = root + "-line-numbers";
    const std::string root_function_names = root + "-function-names";
    const std::string root_timestamps = root + "-timestamps";

    ktrace::OutputOptions output_options{};
    bool saw_output_option = false;

    KTRACE("api",
           "processing CLI options (enable api.cli for details): {} arg(s), root '{}'",
           argc,
           root);

    std::vector<bool> consumed(static_cast<std::size_t>(argc), false);

    // Preserve legacy root behavior for selectors: --trace <selector>
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr) {
            continue;
        }

        const std::string arg = ktrace::detail::trimWhitespace(std::string(argv[i]));
        if (arg != root) {
            continue;
        }

        consumed[static_cast<std::size_t>(i)] = true;
        if (!hasUsableValueToken(i, argc, argv, &consumed)) {
            printTraceHelp(root);
            KTRACE("api", "cli: '{}' with no value, showed help", root);
            continue;
        }

        consumed[static_cast<std::size_t>(i + 1)] = true;
        try {
            enableSelectorListOrThrow(root, argv[++i], local_namespace);
        } catch (const std::exception& ex) {
            spdlog::error("\nTrace option error: {}", ex.what());
            printTraceExamples(root);
        }
    }

    compactArgv(argc, argv, consumed);

    kcli::Parser cli;
    cli.Initialize(argc, argv, root_name);

    cli.Implement("help",
                  [&](const kcli::HandlerContext&) {
                      printTraceHelp(root);
                      KTRACE("api.cli", "handled '{}'", root_help);
                  },
                  "Show trace CLI help.");

    cli.Implement("examples",
                  [&](const kcli::HandlerContext&) {
                      printTraceExamples(root);
                      KTRACE("api.cli", "handled '{}'", root_examples);
                  },
                  "Show selector examples.");

    cli.Implement("namespaces",
                  [&](const kcli::HandlerContext&) {
                      printTraceNamespaces();
                      KTRACE("api.cli", "handled '{}'", root_namespaces);
                  },
                  "Show initialized trace namespaces.");

    cli.Implement("channels",
                  [&](const kcli::HandlerContext&) {
                      printTraceChannels();
                      KTRACE("api.cli", "handled '{}'", root_channels);
                  },
                  "Show initialized trace channels.");

    cli.Implement("colors",
                  [&](const kcli::HandlerContext&) {
                      printTraceColors();
                      KTRACE("api.cli", "handled '{}'", root_colors);
                  },
                  "Show available trace colors.");

    cli.Implement("filenames",
                  [&](const kcli::HandlerContext&) {
                      output_options.filenames = true;
                      saw_output_option = true;
                      KTRACE("api.cli", "enabled '{}' output option", root_filenames);
                  },
                  "Include source filename in trace output.");

    cli.Implement("line-numbers",
                  [&](const kcli::HandlerContext&) {
                      output_options.line_numbers = true;
                      saw_output_option = true;
                      KTRACE("api.cli", "enabled '{}' output option", root_line_numbers);
                  },
                  "Include source line number in trace output.");

    cli.Implement("function-names",
                  [&](const kcli::HandlerContext&) {
                      output_options.function_names = true;
                      saw_output_option = true;
                      KTRACE("api.cli", "enabled '{}' output option", root_function_names);
                  },
                  "Include function name in trace output.");

    cli.Implement("timestamps",
                  [&](const kcli::HandlerContext&) {
                      output_options.timestamps = true;
                      saw_output_option = true;
                      KTRACE("api.cli", "enabled '{}' output option", root_timestamps);
                  },
                  "Include timestamps in trace output.");

    std::vector<std::string> unknown_options;
    cli.SetUnknownOptionHandler(
        [&](std::string_view option) {
            unknown_options.emplace_back(option);
        });

    const kcli::ProcessResult result = cli.Process();

    if (!unknown_options.empty()) {
        for (const std::string& unknown : unknown_options) {
            spdlog::error("\nTrace option error: unknown trace option '{}'", unknown);
            printTraceHelp(root);
        }
    } else if (!result) {
        spdlog::error("\nTrace option error: {}", result.error_message);
        printTraceHelp(root);
    }

    if (saw_output_option) {
        ktrace::SetOutputOptions(output_options);
    }
}

} // namespace

namespace ktrace {

void ProcessCLI(int& argc,
                char** argv,
                std::string_view trace_root,
                std::string_view local_namespace) {
    try {
        processCliArgs(argc, argv, trace_root, local_namespace);
    } catch (const std::exception& ex) {
        std::string root = "--trace";
        try {
            root = buildTraceRootOption(trace_root);
        } catch (...) {
        }
        spdlog::error("\nTrace option error: {}", ex.what());
        printTraceExamples(root);
    }
}

} // namespace ktrace
