#pragma once

#include "ui/backends/driver.hpp"

namespace karma::ui::backend {

std::unique_ptr<BackendDriver> CreateSoftwareBackend();
std::unique_ptr<BackendDriver> CreateImGuiBackend();
std::unique_ptr<BackendDriver> CreateRmlUiBackend();

} // namespace karma::ui::backend
