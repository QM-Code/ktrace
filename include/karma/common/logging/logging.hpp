#pragma once

#include <string>
#include <spdlog/spdlog.h>

namespace karma::common::logging {

void EnableTraceChannels(const std::string& list);
const char* GetDefaultTraceChannelsHelp();
spdlog::logger* GetTraceLogger(const std::string& name);
bool ShouldTraceChannel(const std::string& name);
void ConfigureLogPatterns(bool timestamp_logging);
const char* TraceCategoryColor(const std::string& category);

#define KARMA_TRACE(cat, fmt, ...)                                     \
    do {                                                               \
        if (karma::common::logging::ShouldTraceChannel(cat)) {                 \
            if (auto* logger = karma::common::logging::GetTraceLogger(cat)) {  \
                const char* _c = karma::common::logging::TraceCategoryColor(cat); \
                const char* _r = (_c && _c[0]) ? "\x1b[0m" : "";        \
                logger->trace("[{}{}{}] " fmt,                         \
                              _c,                                      \
                              cat,                                     \
                              _r,                                      \
                              ##__VA_ARGS__);                          \
            }                                                          \
        }                                                              \
    } while (0)

#define KARMA_TRACE_CHANGED(cat, key_expr, fmt, ...)                        \
    do {                                                                    \
        if (karma::common::logging::ShouldTraceChannel(cat)) {                      \
            static std::string last_key;                                     \
            std::string next_key = (key_expr);                              \
            if (next_key != last_key) {                                     \
                last_key = std::move(next_key);                             \
                if (auto* logger = karma::common::logging::GetTraceLogger(cat)) {   \
                    const char* _c = karma::common::logging::TraceCategoryColor(cat); \
                    const char* _r = (_c && _c[0]) ? "\x1b[0m" : "";         \
                    logger->trace("[{}{}{}] " fmt,                          \
                                  _c,                                        \
                                  cat,                                      \
                                  _r,                                        \
                                  ##__VA_ARGS__);                           \
                }                                                           \
            }                                                               \
        }                                                                   \
    } while (0)

} // namespace karma::common::logging
