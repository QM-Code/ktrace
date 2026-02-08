#pragma once

#include "karma/audio/backend.hpp"

namespace karma::audio_backend {

std::unique_ptr<Backend> CreateSdl3AudioBackend();
std::unique_ptr<Backend> CreateMiniaudioBackend();

} // namespace karma::audio_backend

