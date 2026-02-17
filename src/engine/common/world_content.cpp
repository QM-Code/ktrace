#include "karma/common/world_content.hpp"

#include "karma/common/content/catalog.hpp"

namespace world {

WorldContent LoadWorldContent(const std::vector<karma::data::ConfigLayerSpec>& baseSpecs,
                              const std::optional<karma::json::Value>& worldConfig,
                              const std::filesystem::path& worldDir,
                              const std::string& fallbackName,
                              const std::string& logContext) {
    return karma::content::LoadWorldContent(baseSpecs,
                                            worldConfig,
                                            worldDir,
                                            fallbackName,
                                            logContext);
}

} // namespace world
