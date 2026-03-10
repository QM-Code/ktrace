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

void ExpectContains(const std::string& haystack, const std::string& needle) {
    if (haystack.find(needle) == std::string::npos) {
        Fail("expected output to contain '" + needle + "', got:\n" + haystack);
    }
}

void ExpectNotContains(const std::string& haystack, const std::string& needle) {
    if (haystack.find(needle) != std::string::npos) {
        Fail("expected output to not contain '" + needle + "', got:\n" + haystack);
    }
}

void ExpectStartsWith(const std::string& haystack, const std::string& needle) {
    if (haystack.rfind(needle, 0) != 0) {
        Fail("expected output to start with '" + needle + "', got:\n" + haystack);
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

} // namespace

int main() {
    StdoutCapture capture;
    ktrace::Logger logger;
    ktrace::TraceLogger trace("tests");
    logger.addTraceLogger(trace);
    logger.setOutputOptions({
        .filenames = true,
        .line_numbers = true,
        .function_names = false,
        .timestamps = false,
    });

    const int info_line = __LINE__ + 1;
    trace.info("info message");
    const int warn_line = __LINE__ + 1;
    trace.warn("warn value {}", 7);
    const int error_line = __LINE__ + 1;
    trace.error(std::string("error message"));

    const std::string text = capture.finish();
    ExpectStartsWith(text, "[tests] [info] ");
    ExpectContains(text, "\n[tests] [warning] ");
    ExpectContains(text, "\n[tests] [error] ");
    ExpectContains(text, "info message");
    ExpectContains(text, "warn value 7");
    ExpectContains(text, "error message");
    ExpectContains(text, "ktrace_log_api_test:" + std::to_string(info_line));
    ExpectContains(text, "ktrace_log_api_test:" + std::to_string(warn_line));
    ExpectContains(text, "ktrace_log_api_test:" + std::to_string(error_line));
    ExpectNotContains(text, "[info] [tests] [info]");
    ExpectNotContains(text, "[warning] [tests] [warning]");
    ExpectNotContains(text, "[error] [tests] [error]");

    return 0;
}
