#pragma once

#include <ktrace.hpp>

namespace ktrace::demo::alpha {

ktrace::TraceLogger GetTraceLogger();
void TestTraceLoggingChannels();
void TestStandardLoggingChannels();
} // namespace ktrace::demo::alpha
