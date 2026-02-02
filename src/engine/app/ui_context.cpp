#include "karma/app/ui_context.h"

namespace karma::app {

UITextureHandle UIContext::registerExternalTexture(const graphics::TextureHandle &texture) {
    if (!texture.valid()) {
        return 0;
    }
    for (const auto &entry : textures_) {
        const auto &existing = entry.second;
        if (existing.id == texture.id &&
            existing.width == texture.width &&
            existing.height == texture.height &&
            existing.format == texture.format) {
            return entry.first;
        }
    }
    const UITextureHandle handle = next_texture_handle_++;
    textures_[handle] = texture;
    return handle;
}

bool UIContext::resolveExternalTexture(UITextureHandle handle, graphics::TextureHandle &out) const {
    auto it = textures_.find(handle);
    if (it == textures_.end()) {
        return false;
    }
    out = it->second;
    return out.valid();
}

} // namespace karma::app
