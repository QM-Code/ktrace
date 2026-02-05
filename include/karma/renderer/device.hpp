#pragma once

#include <filesystem>

#include "karma/renderer/backend.hpp"
#include "karma/renderer/types.hpp"

namespace karma::renderer {

class GraphicsDevice {
 public:
    explicit GraphicsDevice(karma::platform::Window& window);
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

 private:
    std::unique_ptr<renderer_backend::Backend> backend_;
};

} // namespace karma::renderer
