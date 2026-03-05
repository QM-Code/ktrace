#include <ktrace.hpp>

#include "../ktrace.hpp"

#include <kcli.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

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

void processCliArgs(int& argc,
                    char** argv,
                    std::string_view trace_root,
                    std::string_view local_namespace) {
    if (argc <= 0 || argv == nullptr) {
        return;
    }

    ktrace::detail::ensureInternalTraceChannelsRegistered();
    ktrace::OutputOptions output_options{};
    bool saw_output_option = false;

    kcli::Parser cli;
    cli.Initialize(argc, argv, trace_root);
    const std::string root = std::string("--") + cli.GetRoot();
    const std::string root_examples = root + "-examples";
    const std::string root_namespaces = root + "-namespaces";
    const std::string root_channels = root + "-channels";
    const std::string root_colors = root + "-colors";
    const std::string root_filenames = root + "-filenames";
    const std::string root_line_numbers = root + "-line-numbers";
    const std::string root_function_names = root + "-function-names";
    const std::string root_timestamps = root + "-timestamps";

    KTRACE("api",
           "processing CLI options (enable api.cli for details): {} arg(s), root '{}'",
           argc,
           root);

    cli.SetRootValueHandler(
        [&](const kcli::HandlerContext&, std::string_view value) {
            try {
                ktrace::EnableChannels(value, local_namespace);
                KTRACE("api.cli", "option '{}' enabled selectors '{}'", root, value);
            } catch (const std::exception& ex) {
                spdlog::error("\nTrace option error: {}", ex.what());
                printTraceExamples(root);
            }
        },
        "<selector>",
        "Enable trace channel(s) (may pass more than once)");

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

    (void)cli.Process();

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
        spdlog::error("\nTrace option error: {}", ex.what());
        printTraceExamples("--trace");
    }
}

} // namespace ktrace
