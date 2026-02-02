#include "karma_extras/ui/imgui/texture_registry.hpp"

#include <unordered_map>

namespace ui {
namespace {
std::unordered_map<uint64_t, graphics::TextureHandle> kTextureRegistry;
} // namespace

void RegisterImGuiTexture(const graphics::TextureHandle& texture) {
    if (!texture.valid()) {
        return;
    }
    kTextureRegistry[texture.id] = texture;
}

bool LookupImGuiTexture(uint64_t id, graphics::TextureHandle& out) {
    auto it = kTextureRegistry.find(id);
    if (it == kTextureRegistry.end()) {
        return false;
    }
    out = it->second;
    return out.valid();
}

void ClearImGuiTextureRegistry() {
    kTextureRegistry.clear();
}

} // namespace ui

