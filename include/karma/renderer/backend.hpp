#pragma once

#include "karma/renderer/types.hpp"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace karma::platform { class Window; }

namespace karma::renderer_backend {

enum class BackendKind {
    Auto,
    Bgfx,
    Diligent
};

const char* BackendKindName(BackendKind kind);
std::optional<BackendKind> ParseBackendKind(std::string_view name);
std::vector<BackendKind> CompiledBackends();

class Backend {
 public:
    virtual ~Backend() = default;

    virtual void beginFrame(int width, int height, float dt) = 0;
    virtual void endFrame() = 0;

    virtual renderer::MeshId createMesh(const renderer::MeshData& mesh) = 0;
    virtual void destroyMesh(renderer::MeshId mesh) = 0;

    virtual renderer::MaterialId createMaterial(const renderer::MaterialDesc& material) = 0;
    virtual void destroyMaterial(renderer::MaterialId material) = 0;

    virtual void submit(const renderer::DrawItem& item) = 0;
    virtual void submitDebugLine(const renderer::DebugLineItem& line) = 0;
    virtual void renderFrame() = 0;
    virtual void renderLayer(renderer::LayerId layer) = 0;

    virtual void setCamera(const renderer::CameraData& camera) = 0;
    virtual void setDirectionalLight(const renderer::DirectionalLightData& light) = 0;
    virtual void setLights(const std::vector<renderer::LightData>& lights) = 0;
    virtual void setEnvironmentLighting(const renderer::EnvironmentLightingData& environment) = 0;
    virtual bool isValid() const = 0;
};

std::unique_ptr<Backend> CreateBackend(karma::platform::Window& window,
                                       BackendKind preferred = BackendKind::Auto,
                                       BackendKind* out_selected = nullptr);

} // namespace karma::renderer_backend
