#pragma once

#include "ui/backends/rmlui/adapter.hpp"

#if defined(KARMA_HAS_RMLUI)

#include <RmlUi/Core.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace karma::ui::rmlui {

struct Geometry {
    std::vector<Rml::Vertex> vertices;
    std::vector<int> indices;
};

struct Texture {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
};

struct DrawCall {
    Rml::CompiledGeometryHandle geometry = 0;
    Rml::Vector2f translation{0.0f, 0.0f};
    Rml::TextureHandle texture = 0;
    bool scissor_enabled = false;
    Rml::Rectanglei scissor = Rml::Rectanglei::FromSize({0, 0});
};

int MapModifiers(const platform::Modifiers& mods);
Rml::Input::KeyIdentifier MapKey(platform::Key key);
int MapMouseButton(platform::MouseButton button);

class SystemInterface final : public Rml::SystemInterface {
 public:
    double GetElapsedTime() override;
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;

 private:
    std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
};

class CpuRenderInterface final : public Rml::RenderInterface {
 public:
    bool beginFrame(int width, int height);
    void rasterize();
    const renderer::MeshData::TextureData& frameTexture() const;
    size_t drawCallCount() const;

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry,
                        Rml::Vector2f translation,
                        Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
                                       Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;
    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;

 private:
    static void sampleTexture(const Texture* texture, float u, float v, float& r, float& g, float& b, float& a);
    void rasterizeTriangle(const Rml::Vertex& va,
                           const Rml::Vertex& vb,
                           const Rml::Vertex& vc,
                           const Rml::Vector2f& translation,
                           const Texture* texture,
                           int clip_min_x,
                           int clip_min_y,
                           int clip_max_x,
                           int clip_max_y);

    int frame_width_ = 0;
    int frame_height_ = 0;
    bool scissor_enabled_ = false;
    Rml::Rectanglei scissor_ = Rml::Rectanglei::FromSize({0, 0});
    size_t draw_call_count_ = 0;
    Rml::CompiledGeometryHandle next_geometry_handle_ = 1;
    Rml::TextureHandle next_texture_handle_ = 1;
    std::unordered_map<Rml::CompiledGeometryHandle, Geometry> geometries_{};
    std::unordered_map<Rml::TextureHandle, Texture> textures_{};
    std::unordered_set<std::string> missing_texture_sources_{};
    std::vector<DrawCall> draw_calls_{};
    renderer::MeshData::TextureData frame_texture_{};
};

std::string EscapeText(const std::string& text);
std::string BuildBaseDocument();
std::string BuildPanelsMarkup(const std::vector<UiDrawContext::TextPanel>& text_panels);

} // namespace karma::ui::rmlui

#endif // defined(KARMA_HAS_RMLUI)
