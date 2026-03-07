#include <ktrace.hpp>

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

[[noreturn]] void Fail(const std::string& message) {
    std::cerr << "ktrace_channel_semantics_test failed: " << message << "\n";
    throw std::runtime_error(message);
}

void ExpectTrue(const bool condition, const std::string& message) {
    if (!condition) {
        Fail(message);
    }
}

void ExpectFalse(const bool condition, const std::string& message) {
    if (condition) {
        Fail(message);
    }
}

void RegisterTestChannels() {
    static bool registered = false;
    if (registered) {
        return;
    }

    ktrace::Initialize();
    ktrace::RegisterChannel("net");
    ktrace::RegisterChannel("cache");

    ktrace::detail::RegisterChannelBridge("foreignns", "store", ktrace::kDefaultColor);
    ktrace::detail::RegisterChannelBridge("foreignns", "store.requests", ktrace::kDefaultColor);

    registered = true;
}

void VerifyExplicitOnOffSemantics() {
    ktrace::ClearEnabledChannels();

    ktrace::EnableChannels("tests.*");
    ExpectTrue(ktrace::ShouldTraceChannel("tests.net"), "tests.net should be enabled by tests.*");
    ExpectTrue(ktrace::ShouldTraceChannel("tests.cache"), "tests.cache should be enabled by tests.*");

    ktrace::DisableChannels("tests.*");
    ExpectFalse(ktrace::ShouldTraceChannel("tests.net"), "tests.net should be disabled by tests.*");
    ExpectFalse(ktrace::ShouldTraceChannel("tests.cache"), "tests.cache should be disabled by tests.*");

    ktrace::EnableChannel("tests.net");
    ExpectTrue(ktrace::ShouldTraceChannel("tests.net"),
               "explicit enable should turn tests.net back on after broad disable");
    ExpectFalse(ktrace::ShouldTraceChannel("tests.cache"),
                "tests.cache should stay off after explicit enable of tests.net");

    ktrace::DisableChannel("tests.net");
    ExpectFalse(ktrace::ShouldTraceChannel("tests.net"),
                "explicit disable should turn tests.net back off");
}

void VerifyRegisteredNamespaceSemantics() {
    ktrace::ClearEnabledChannels();

    ktrace::EnableChannels("*.*.*");
    ExpectTrue(ktrace::detail::ShouldTraceBridge("foreignns", "store.requests"),
               "foreignns.store.requests should trace when explicitly registered and enabled");
    ExpectFalse(ktrace::detail::ShouldTraceBridge("tests", "store.requests"),
                "tests.store.requests should not trace when only foreignns.store.requests is registered");
    ExpectFalse(ktrace::detail::ShouldTraceBridge("tests", "bad name"),
                "invalid runtime channel names should not trace");

    ktrace::EnableChannel("tests.store.requests");
    ExpectFalse(ktrace::ShouldTraceChannel("tests.store.requests"),
                "EnableChannel should ignore unregistered exact channels");

    ktrace::EnableChannels("tests.store.requests");
    ExpectFalse(ktrace::ShouldTraceChannel("tests.store.requests"),
                "EnableChannels should ignore unresolved exact selectors");
}

} // namespace

int main() {
    RegisterTestChannels();
    VerifyExplicitOnOffSemantics();
    VerifyRegisteredNamespaceSemantics();
    return 0;
}
