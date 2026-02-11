#pragma once

#include <spdlog/sinks/base_sink.h>

#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace karma::network::tests {

struct CapturedLogEvent {
    spdlog::level::level_enum level = spdlog::level::off;
    std::string logger_name{};
};

class StructuredLogEventSink final : public spdlog::sinks::base_sink<std::mutex> {
 public:
    size_t CountLevel(spdlog::level::level_enum level) {
        std::lock_guard<std::mutex> lock(this->mutex_);
        size_t count = 0;
        for (const auto& event : events_) {
            if (event.level == level) {
                ++count;
            }
        }
        return count;
    }

    size_t CountLevelForLogger(spdlog::level::level_enum level, std::string_view logger_name) {
        std::lock_guard<std::mutex> lock(this->mutex_);
        size_t count = 0;
        for (const auto& event : events_) {
            if (event.level == level && event.logger_name == logger_name) {
                ++count;
            }
        }
        return count;
    }

 protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        CapturedLogEvent event{};
        event.level = msg.level;
        event.logger_name.assign(msg.logger_name.data(), msg.logger_name.size());
        events_.push_back(std::move(event));
    }

    void flush_() override {}

 private:
    std::vector<CapturedLogEvent> events_{};
};

} // namespace karma::network::tests
