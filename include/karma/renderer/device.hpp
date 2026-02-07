#pragma once

#include <filesystem>

#include "karma/renderer/backend.hpp"
#include "karma/renderer/types.hpp"

namespace karma::renderer {

class GraphicsDevice {
 public:
    explicit GraphicsDevice(karma::platform::Window& window,
                            renderer_backend::BackendKind preferred_backend = renderer_backend::BackendKind::Auto);
    ~GraphicsDevice();

    void beginFrame(int width, int height, float dt);
    void endFrame();

    MeshId createMesh(const MeshData& mesh);
    MeshId createMeshFromFile(const std::filesystem::path& path);
    void destroyMesh(MeshId mesh);

    MaterialId createMaterial(const MaterialDesc& material);
    void destroyMaterial(MaterialId material);

    void submit(const DrawItem& item);
    void renderFrame();
    void renderLayer(LayerId layer);
    void setCamera(const CameraData& camera);
    void setDirectionalLight(const DirectionalLightData& light);
    bool isValid() const;
    renderer_backend::BackendKind backendKind() const { return backend_kind_; }
    const char* backendName() const { return renderer_backend::BackendKindName(backend_kind_); }

 private:
    std::unique_ptr<renderer_backend::Backend> backend_;
    renderer_backend::BackendKind backend_kind_ = renderer_backend::BackendKind::Auto;
};

} // namespace karma::renderer
