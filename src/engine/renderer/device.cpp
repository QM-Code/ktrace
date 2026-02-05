#include "karma/renderer/device.hpp"

#include "karma/common/logging.hpp"
#include "geometry/mesh_loader.hpp"

namespace karma::renderer {

GraphicsDevice::GraphicsDevice(karma::platform::Window& window)
    : backend_(renderer_backend::CreateBackend(window)) {
    if (backend_ && !backend_->isValid()) {
        spdlog::error("GraphicsDevice: backend failed to initialize");
        backend_.reset();
    }
}

GraphicsDevice::~GraphicsDevice() = default;

void GraphicsDevice::beginFrame(int width, int height, float dt) {
    if (backend_) {
        backend_->beginFrame(width, height, dt);
    }
}

void GraphicsDevice::endFrame() {
    if (backend_) {
        backend_->endFrame();
    }
}

MeshId GraphicsDevice::createMesh(const MeshData& mesh) {
    return backend_ ? backend_->createMesh(mesh) : kInvalidMesh;
}

MeshId GraphicsDevice::createMeshFromFile(const std::filesystem::path& path) {
    MeshData mesh{};
    if (!karma::geometry::LoadMesh(path, mesh)) {
        spdlog::error("Failed to load mesh {}", path.string());
        return kInvalidMesh;
    }
    return createMesh(mesh);
}

void GraphicsDevice::destroyMesh(MeshId mesh) {
    if (backend_) {
        backend_->destroyMesh(mesh);
    }
}

MaterialId GraphicsDevice::createMaterial(const MaterialDesc& material) {
    return backend_ ? backend_->createMaterial(material) : kInvalidMaterial;
}

void GraphicsDevice::destroyMaterial(MaterialId material) {
    if (backend_) {
        backend_->destroyMaterial(material);
    }
}

void GraphicsDevice::submit(const DrawItem& item) {
    if (backend_) {
        backend_->submit(item);
    }
}

void GraphicsDevice::renderFrame() {
    if (backend_) {
        backend_->renderFrame();
    }
}

void GraphicsDevice::setCamera(const CameraData& camera) {
    if (backend_) {
        backend_->setCamera(camera);
    }
}

bool GraphicsDevice::isValid() const {
    return backend_ != nullptr;
}

} // namespace karma::renderer
