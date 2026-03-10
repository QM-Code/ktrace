#include <ktrace.hpp>

#include <cstdio>
#include <stdexcept>
#include <string>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace {

[[noreturn]] void Fail(const std::string& message) {
    throw std::runtime_error(message);
}

void ExpectEqual(const std::string& actual, const std::string& expected) {
    if (actual != expected) {
        Fail("expected '" + expected + "', got '" + actual + "'");
    }
}

void ExpectContains(const std::string& haystack, const std::string& needle) {
    if (haystack.find(needle) == std::string::npos) {
        Fail("expected output to contain '" + needle + "', got:\n" + haystack);
    }
}

void ExpectTrue(const bool condition, const std::string& message) {
    if (!condition) {
        Fail(message);
    }
}

class StdoutCapture {
public:
    StdoutCapture() {
#ifndef _WIN32
        captured_ = std::tmpfile();
        if (!captured_) {
            Fail("failed to create capture file");
        }

        std::fflush(stdout);
        original_stdout_fd_ = ::dup(fileno(stdout));
        if (original_stdout_fd_ < 0) {
            std::fclose(captured_);
            Fail("failed to duplicate stdout");
        }

        if (::dup2(fileno(captured_), fileno(stdout)) < 0) {
            ::close(original_stdout_fd_);
            std::fclose(captured_);
            Fail("failed to redirect stdout");
        }
#else
        Fail("stdout capture is not implemented on Windows");
#endif
    }

    ~StdoutCapture() {
#ifndef _WIN32
        restore();
#endif
    }

    std::string finish() {
#ifndef _WIN32
        restore();

        std::fflush(captured_);
        if (std::fseek(captured_, 0, SEEK_SET) != 0) {
            std::fclose(captured_);
            captured_ = nullptr;
            Fail("failed to rewind capture file");
        }

        std::string text;
        char buffer[256];
        while (true) {
            const std::size_t count = std::fread(buffer, 1, sizeof(buffer), captured_);
            if (count > 0) {
                text.append(buffer, count);
            }
            if (count < sizeof(buffer)) {
                break;
            }
        }

        std::fclose(captured_);
        captured_ = nullptr;
        return text;
#else
        return {};
#endif
    }

private:
#ifndef _WIN32
    void restore() {
        if (original_stdout_fd_ >= 0) {
            std::fflush(stdout);
            ::dup2(original_stdout_fd_, fileno(stdout));
            ::close(original_stdout_fd_);
            original_stdout_fd_ = -1;
        }
    }

    FILE* captured_ = nullptr;
    int original_stdout_fd_ = -1;
#endif
};

void ExpectInvalidFormat(std::string_view format_text) {
    bool threw = false;
    try {
        (void)ktrace::detail::FormatMessage(format_text, 7);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    ExpectTrue(threw, "expected invalid_argument for format '" + std::string(format_text) + "'");
}

void VerifyFormatMessage() {
    ExpectEqual(ktrace::detail::FormatMessage("value {} {}", 7, "done"), "value 7 done");
    ExpectEqual(ktrace::detail::FormatMessage("escaped {{}}"), "escaped {}");
    ExpectEqual(ktrace::detail::FormatMessage("bool {}", true), "bool true");

    ExpectInvalidFormat("value {} {}");
    ExpectInvalidFormat("value");
    ExpectInvalidFormat("{");
    ExpectInvalidFormat("}");
    ExpectInvalidFormat("{:x}");
}

void VerifyPublicLoggingOutput() {
    StdoutCapture capture;
    ktrace::Logger logger;
    ktrace::TraceLogger trace("tests");
    logger.addTraceLogger(trace);

    trace.warn("escaped {{}} {}", 7);

    const std::string text = capture.finish();
    ExpectContains(text, "escaped {} 7");
}

void VerifyTraceLoggerOutput() {
    StdoutCapture capture;
    ktrace::Logger logger;

    ktrace::TraceLogger tracer("tests");
    tracer.addChannel("trace");
    logger.addTraceLogger(tracer);
    logger.enableChannel("tests.trace");

    tracer.trace("trace", "member {} {{ok}}", 42);

    const std::string text = capture.finish();
    ExpectContains(text, "member 42 {ok}");
}

} // namespace

int main() {
    VerifyFormatMessage();
    VerifyPublicLoggingOutput();
    VerifyTraceLoggerOutput();
    return 0;
}
