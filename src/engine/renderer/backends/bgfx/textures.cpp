#include "internal.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace karma::renderer_backend {

bgfx::TextureHandle createWhiteTexture() {
    const uint32_t white = 0xffffffff;
    const bgfx::Memory* mem = bgfx::copy(&white, sizeof(white));
    return bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0, mem);
}

namespace {

constexpr uint64_t kBgfxMaterialTextureSamplerFlags =
    // In this bgfx toolchain, wrap + linear mip are the default when U/V/W and MIP bits are unset.
    BGFX_SAMPLER_MIN_ANISOTROPIC |
    BGFX_SAMPLER_MAG_ANISOTROPIC;

} // namespace

bgfx::TextureHandle createTextureFromData(const renderer::MeshData::TextureData& tex) {
    const std::vector<detail::Rgba8MipLevel> mip_chain = detail::BuildRgba8MipChain(tex);
    if (mip_chain.empty()) {
        return BGFX_INVALID_HANDLE;
    }
    const detail::Rgba8MipLevel& base_level = mip_chain.front();
    if (base_level.width <= 0 || base_level.height <= 0 || base_level.pixels.empty()) {
        return BGFX_INVALID_HANDLE;
    }
    const bool has_mips = mip_chain.size() > 1u;
    bgfx::TextureHandle texture = bgfx::createTexture2D(
        static_cast<uint16_t>(base_level.width),
        static_cast<uint16_t>(base_level.height),
        has_mips,
        1,
        bgfx::TextureFormat::RGBA8,
        kBgfxMaterialTextureSamplerFlags,
        nullptr);
    if (!bgfx::isValid(texture)) {
        return BGFX_INVALID_HANDLE;
    }

    for (std::size_t mip = 0; mip < mip_chain.size(); ++mip) {
        const detail::Rgba8MipLevel& level = mip_chain[mip];
        if (level.width <= 0 || level.height <= 0 || level.pixels.empty()) {
            bgfx::destroy(texture);
            return BGFX_INVALID_HANDLE;
        }
        bgfx::updateTexture2D(
            texture,
            0,
            static_cast<uint8_t>(mip),
            0,
            0,
            static_cast<uint16_t>(level.width),
            static_cast<uint16_t>(level.height),
            bgfx::copy(level.pixels.data(), static_cast<uint32_t>(level.pixels.size())));
    }
    return texture;
}

} // namespace karma::renderer_backend
