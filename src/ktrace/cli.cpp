#include <ktrace.hpp>

#include "../ktrace.hpp"

#include <kcli.hpp>

#include <algorithm>
#include <iostream>
#include <source_location>
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

    std::string root_name(trace_root);
    if (root_name.rfind("--", 0) == 0) {
        root_name.erase(0, 2);
    }
    const std::string root = std::string("--") + root_name;

    KTRACE("api",
           "processing CLI options (enable api.cli for details): {} arg(s), root '{}'",
           argc,
           root);

    kcli::Initialize(argc,
                     argv,
                     {.root = trace_root, .failure_mode = kcli::FailureMode::Throw});

    kcli::SetRootValueHandler([&](const kcli::HandlerContext&, std::string_view value) {
        try {
            ktrace::EnableChannels(value, local_namespace);
            KTRACE("api.cli", "option '{}' enabled selectors '{}'", root, value);
        } catch (const std::exception& ex) {
            const std::source_location location = std::source_location::current();
            ktrace::detail::LogChecked(
                local_namespace,
                ktrace::detail::LogSeverity::error,
                location.file_name(),
                static_cast<int>(location.line()),
                location.function_name(),
                fmt::format("Trace option error: {}", ex.what()));
            printTraceExamples(root);
        }
    });

    kcli::SetHandler("-examples",
                     [&](const kcli::HandlerContext& context) {
                         printTraceExamples(root);
                         KTRACE("api.cli", "handled '{}'", context.option);
                     },
                     "Show selector examples.");

    kcli::SetHandler("-namespaces",
                     [&](const kcli::HandlerContext& context) {
                         printTraceNamespaces();
                         KTRACE("api.cli", "handled '{}'", context.option);
                     },
                     "Show initialized trace namespaces.");

    kcli::SetHandler("-channels",
                     [&](const kcli::HandlerContext& context) {
                         printTraceChannels();
                         KTRACE("api.cli", "handled '{}'", context.option);
                     },
                     "Show initialized trace channels.");

    kcli::SetHandler("-colors",
                     [&](const kcli::HandlerContext& context) {
                         printTraceColors();
                         KTRACE("api.cli", "handled '{}'", context.option);
                     },
                     "Show available trace colors.");

    kcli::SetHandler("-files",
                     [&](const kcli::HandlerContext& context) {
                         output_options.filenames = true;
                         output_options.line_numbers = true;
                         saw_output_option = true;
                         KTRACE("api.cli", "enabled '{}' output option", context.option);
                     },
                     "Include source file and line in trace output.");

    kcli::SetHandler("-functions",
                     [&](const kcli::HandlerContext& context) {
                         output_options.function_names = true;
                         saw_output_option = true;
                         KTRACE("api.cli", "enabled '{}' output option", context.option);
                     },
                     "Include function names in trace output (requires --trace-files).");

    kcli::SetHandler("-timestamps",
                     [&](const kcli::HandlerContext& context) {
                         output_options.timestamps = true;
                         saw_output_option = true;
                         KTRACE("api.cli", "enabled '{}' output option", context.option);
                     },
                     "Include timestamps in trace output.");

    (void)kcli::Process();

    if (saw_output_option) {
        ktrace::SetOutputOptions(output_options);
    }

    if (output_options.function_names && !output_options.filenames) {
        const std::source_location location = std::source_location::current();
        ktrace::detail::LogChecked(
            local_namespace,
            ktrace::detail::LogSeverity::warning,
            location.file_name(),
            static_cast<int>(location.line()),
            location.function_name(),
            fmt::format("--{}-functions requires --{}-files to be operational",
                        root_name,
                        root_name));
    }
}

} // namespace

namespace ktrace {

void ProcessCLI(int& argc,
                char** argv,
                std::string_view trace_root,
                std::string_view local_namespace) {
    processCliArgs(argc, argv, trace_root, local_namespace);
}

} // namespace ktrace
