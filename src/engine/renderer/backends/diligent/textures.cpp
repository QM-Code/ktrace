#include "internal.hpp"

#include <cstdint>
#include <vector>

namespace karma::renderer_backend {

Diligent::RefCntAutoPtr<Diligent::ITextureView> CreateDiligentWhiteTexture(
    Diligent::IRenderDevice* device) {
    Diligent::RefCntAutoPtr<Diligent::ITextureView> view;
    if (!device) {
        return view;
    }
    const uint32_t white = 0xffffffffu;
    Diligent::TextureDesc desc{};
    desc.Name = "white_tex";
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = 1;
    desc.Height = 1;
    desc.MipLevels = 1;
    desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    Diligent::TextureData texData{};
    Diligent::TextureSubResData subres{};
    subres.pData = &white;
    subres.Stride = sizeof(uint32_t);
    texData.pSubResources = &subres;
    texData.NumSubresources = 1;
    Diligent::RefCntAutoPtr<Diligent::ITexture> tex;
    device->CreateTexture(desc, &texData, &tex);
    if (tex) {
        view = tex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    }
    return view;
}

Diligent::RefCntAutoPtr<Diligent::ITextureView> CreateDiligentTextureView(
    Diligent::IRenderDevice* device,
    const renderer::MeshData::TextureData& tex) {
    Diligent::RefCntAutoPtr<Diligent::ITextureView> view;
    if (!device || tex.width <= 0 || tex.height <= 0) {
        return view;
    }
    const std::vector<detail::Rgba8MipLevel> mip_chain = detail::BuildRgba8MipChain(tex);
    if (mip_chain.empty()) {
        return view;
    }
    const detail::Rgba8MipLevel& base_level = mip_chain.front();
    if (base_level.width <= 0 || base_level.height <= 0 || base_level.pixels.empty()) {
        return view;
    }
    Diligent::TextureDesc desc{};
    desc.Name = "albedo_tex";
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = static_cast<uint32_t>(base_level.width);
    desc.Height = static_cast<uint32_t>(base_level.height);
    desc.MipLevels = static_cast<uint32_t>(mip_chain.size());
    desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    Diligent::TextureData texData{};
    std::vector<Diligent::TextureSubResData> subresources(mip_chain.size());
    for (std::size_t mip = 0; mip < mip_chain.size(); ++mip) {
        const detail::Rgba8MipLevel& level = mip_chain[mip];
        if (level.width <= 0 || level.height <= 0 || level.pixels.empty()) {
            return view;
        }
        Diligent::TextureSubResData& subres = subresources[mip];
        subres.pData = level.pixels.data();
        subres.Stride = static_cast<uint32_t>(level.width * 4);
    }
    texData.pSubResources = subresources.data();
    texData.NumSubresources = static_cast<uint32_t>(subresources.size());
    Diligent::RefCntAutoPtr<Diligent::ITexture> outTex;
    device->CreateTexture(desc, &texData, &outTex);
    if (outTex) {
        view = outTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    }
    return view;
}

} // namespace karma::renderer_backend
